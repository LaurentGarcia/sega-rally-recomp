#include "driving_capture.h"
#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace ps2::offline {
namespace {
constexpr std::uint32_t relocation = 0x59f000;
constexpr std::size_t byte_limit = 16 * 1024 * 1024;
struct Call {
    std::uint64_t id = 0, instructions = 0, frame = 0;
    std::uint32_t object = 0, descriptor = 0, return_pc = 0, caller_fp = 0, entry_pc = 0, branch_selector = 0;
    bool detailed = false;
};
struct Capture {
    std::filesystem::path path;
    std::ofstream events, cadence, selectors;
    unsigned first = 0, last = 0, detailed = 0, events_count = 0, completed = 0;
    std::uint64_t calls = 0, last_count = ~std::uint64_t{}, selected_frame = ~std::uint64_t{};
    unsigned frame_samples = 0;
    std::size_t bytes = 0;
    std::uint64_t observed = 0, rom_entries = 0, ram_entries = 0;
    unsigned primary_calls = 0, alternate_calls = 0, selector_count = 0;
    std::uint32_t query_return = 0, query_caller_fp = 0;
    // Original and relocated forms of the bounded car-code analysis region.
    std::array<std::uint64_t, 2*0x8000> code_counts{};
    Call call;
};
std::unique_ptr<Capture> active;
std::uint32_t original_pc(std::uint32_t pc) {
    return pc >= relocation && pc < 0x600000 ? pc-relocation : pc;
}
bool ram(std::uint32_t address, std::size_t bytes) {
    const auto end = std::uint64_t(address) + bytes;
    return !(address & 3u) && ((address >= 0x200000 && end <= 0x240000) ||
                             (address >= 0x500000 && end <= 0x600000));
}
std::vector<std::uint32_t> words(const rt::Cpu &cpu, std::uint32_t address, unsigned count) {
    if (count > 256 || !ram(address, std::size_t(count)*4))
        throw std::runtime_error("driving observation requested non-RAM or unbounded memory");
    std::vector<std::uint32_t> result;
    result.reserve(count);
    for (unsigned i=0; i<count; ++i) result.push_back(cpu.bus->read_dword(address+4*i));
    return result;
}
template<class Values> void integers(std::ostream &out, const Values &values) {
    out << '['; bool first=true;
    for (auto value:values) { if (!first) out << ','; first=false; out << value; }
    out << ']';
}
void write(Capture &capture, std::ofstream &stream, const std::string &text) {
    if (capture.bytes + text.size() + 1 > byte_limit)
        throw std::runtime_error("driving capture byte limit exceeded");
    stream << text << '\n';
    if (!stream) throw std::runtime_error("cannot write driving capture");
    capture.bytes += text.size()+1;
}
void cadence(Capture &capture, const rt::Cpu &cpu, const char *event,
             std::uint64_t instructions, std::uint64_t frame, std::uint64_t ns) {
    std::ostringstream out;
    out << "{\"event\":\"" << event << "\",\"call_id\":" << capture.call.id
        << ",\"entry_pc\":" << capture.call.entry_pc
        << ",\"branch_selector\":" << capture.call.branch_selector
        << ",\"object\":" << capture.call.object << ",\"frame\":" << frame
        << ",\"instructions\":" << instructions << ",\"cycles\":" << cpu.reference_cycles
        << ",\"board_ns\":" << ns << ",\"pc\":" << cpu.m_IP << '}';
    write(capture,capture.cadence,out.str());
}
void snapshot(Capture &capture, const rt::Cpu &cpu, const char *event,
              std::uint64_t instructions, std::uint64_t frame, std::uint64_t ns) {
    if (!capture.call.detailed) return;
    if (++capture.events_count > 4096) throw std::runtime_error("driving capture event limit exceeded");
    std::ostringstream out;
    out << "{\"schema\":1,\"event\":\"" << event << "\",\"call_id\":" << capture.call.id
        << ",\"entry_pc\":" << capture.call.entry_pc
        << ",\"branch_selector\":" << capture.call.branch_selector
        << ",\"object\":" << capture.call.object << ",\"frame\":" << frame
        << ",\"instructions\":" << instructions << ",\"cycles\":" << cpu.reference_cycles
        << ",\"board_ns\":" << ns << ",\"pc\":" << cpu.m_IP << ",\"ac\":" << cpu.m_AC
        << ",\"registers\":";
    integers(out,cpu.m_r);
    std::array<std::uint64_t,4> fp{};
    for (unsigned i=0;i<4;++i) fp[i]=std::bit_cast<std::uint64_t>(cpu.m_fp[i]);
    out << ",\"fp_bits\":"; integers(out,fp);
    out << ",\"object_words\":"; integers(out,words(cpu,capture.call.object,256));
    out << ",\"blocks\":[";
    bool first=true;
    const auto block=[&](const char *name,std::uint32_t address,unsigned count) {
        if (!ram(address,std::size_t(count)*4)) return;
        if (!first) out << ','; first=false;
        out << "{\"name\":\"" << name << "\",\"address\":" << address << ",\"words\":";
        integers(out,words(cpu,address,count)); out << '}';
    };
    block("globals",0x214100,192);
    block("velocity_globals",0x20ca00,64);
    block("query_stage_global",0x20a280,16);
    block("query_descriptors",0x5dcac0,24);
    block("tuning",0x5cabe0,8);
    block("descriptor_prefix",capture.call.descriptor,32);
    const auto index=cpu.bus->read_dword(capture.call.object+0xe4);
    const auto selected=std::uint64_t(capture.call.descriptor)+16*std::uint64_t(index);
    if (selected<=0xffffffffu) block("selected_descriptor",std::uint32_t(selected),4);
    const auto pc=original_pc(cpu.m_IP);
    if (pc==0x2abc0) block("query_point",cpu.m_r[18],3);
    if (pc==0x3110c || pc==0x31154 || pc==0x31158) block("blend_output",cpu.m_r[17],4);
    const auto staging=cpu.bus->read_dword(0x20a290);
    block("query_staging",staging,32);
    out << "]}";
    write(capture,capture.events,out.str());
}
} // namespace

void begin_driving_capture(const char *directory,unsigned first,unsigned last) {
    if (active || !directory || !*directory || first>last || last>10000 || last-first>1200)
        throw std::runtime_error("invalid bounded driving capture interval");
    namespace fs=std::filesystem;
    const auto root=fs::canonical(__FILE__).parent_path().parent_path().parent_path().parent_path();
    if (fs::is_symlink(root/"build")) throw std::runtime_error("private build root is a symlink");
    const auto build=fs::canonical(root/"build");
    const auto path=fs::weakly_canonical(fs::absolute(directory));
    auto b=build.begin(),p=path.begin();
    for (;b!=build.end()&&p!=path.end()&&*b==*p;++b,++p) {}
    if (b!=build.end()||p==path.end()||fs::exists(path))
        throw std::runtime_error("driving capture requires a new directory below ignored build/");
    fs::create_directories(path);
    auto capture=std::make_unique<Capture>();
    capture->path=path;capture->first=first;capture->last=last;
    capture->events.open(path/"events.jsonl.tmp");capture->cadence.open(path/"cadence.jsonl.tmp");
    capture->selectors.open(path/"selectors.jsonl.tmp");
    if (!capture->events||!capture->cadence||!capture->selectors) throw std::runtime_error("cannot open driving capture");
    active=std::move(capture);
}

void observe_driving_instruction(const rt::Cpu &cpu,std::uint64_t instructions,
                                 std::uint64_t frame,std::uint64_t ns) {
    if (!active) return;
    auto &capture=*active;
    if (capture.last_count==instructions) return;
    capture.last_count=instructions;
    ++capture.observed;
    if (cpu.m_IP==0x2cce0) ++capture.rom_entries;
    if (cpu.m_IP==relocation+0x2cce0) ++capture.ram_entries;
    if (capture.query_return && cpu.m_IP==capture.query_return && cpu.m_r[31]==capture.query_caller_fp) {
        snapshot(capture,cpu,"query_post",instructions,frame,ns);
        capture.query_return=0;
    }
    if (capture.call.id && cpu.m_IP==capture.call.return_pc && cpu.m_r[31]==capture.call.caller_fp) {
        if (capture.query_return) throw std::runtime_error("integrator returned with an unpaired query");
        cadence(capture,cpu,"integrator_return",instructions,frame,ns);
        snapshot(capture,cpu,"integrator_return",instructions,frame,ns);
        ++capture.completed;capture.call={};
    }
    const auto pc=original_pc(cpu.m_IP);
    if (frame>=capture.first && frame<=capture.last && pc>=0x20000 && pc<0x40000 && !(pc&3))
        ++capture.code_counts[(cpu.m_IP==pc ? 0 : 0x8000)+(pc-0x20000)/4];
    if (pc==0x2bfa4 && frame>=capture.first && frame<=capture.last) {
        if (++capture.selector_count>16384) throw std::runtime_error("selector observation limit exceeded");
        const auto object=cpu.m_r[5];
        if (!ram(object,0xec)) throw std::runtime_error("selector object is outside plain RAM");
        std::ostringstream out;
        out << "{\"frame\":" << frame << ",\"instructions\":" << instructions
            << ",\"pc\":" << cpu.m_IP << ",\"object\":" << object
            << ",\"selector\":" << cpu.bus->read_dword(object+0xe8) << '}';
        write(capture,capture.selectors,out.str());
    }
    if ((pc==0x2cce0 || pc==0x2e650) && frame>=capture.first && frame<=capture.last) {
        if (capture.call.id) throw std::runtime_error("nested or unpaired live integrator call");
        if (++capture.calls>16384) throw std::runtime_error("driving cadence call limit exceeded");
        if (capture.selected_frame!=frame) { capture.selected_frame=frame;capture.frame_samples=0; }
        const bool selected=frame%60==0 || frame==2300 || frame==2500 || frame==2501 ||
                            frame==2600 || frame==2601 || frame==2700 || frame==2701;
        const bool detailed=selected && capture.frame_samples<4 && capture.detailed<128;
        if (detailed) { ++capture.frame_samples;++capture.detailed; }
        if (!ram(cpu.m_r[16],0xec)) throw std::runtime_error("integrator observation requested non-RAM object");
        const auto selector=cpu.bus->read_dword(cpu.m_r[16]+0xe8);
        if (pc==0x2cce0) ++capture.primary_calls; else ++capture.alternate_calls;
        capture.call={capture.calls,instructions,frame,cpu.m_r[16],cpu.m_r[17],cpu.m_r[2],cpu.m_r[0]&~0x3fu,cpu.m_IP,selector,detailed};
        cadence(capture,cpu,"integrator_entry",instructions,frame,ns);
        snapshot(capture,cpu,"integrator_entry",instructions,frame,ns);
        return;
    }
    if (!capture.call.id) return;
    if (instructions-capture.call.instructions>2000000)
        throw std::runtime_error("live integrator did not return within bounded instruction count");
    const char *event=nullptr;
    switch(pc) {
    case 0x30e00:event="velocity_entry";break;
    case 0x3110c:event="blend_pre";break;
    case 0x31154:event="blend_store";break;
    case 0x31158:event="blend_post";break;
    case 0x2abc0:
        if (capture.query_return) throw std::runtime_error("nested or unpaired table query");
        capture.query_return=cpu.m_r[2];capture.query_caller_fp=cpu.m_r[0]&~0x3fu;
        if (!capture.query_return) throw std::runtime_error("query has no return address");
        event="query_pre";break;
    case 0x2ac08:event="query_count";break;
    case 0x2e560:event="orientation_pre";break;
    case 0x2e638:event="orientation_post";break;
    case 0x2e648:event="integrator_before_return";break;
    default:break;
    }
    if (event) {
        if ((pc==0x2e560 || pc==0x2e638) && cpu.m_r[15]!=capture.call.object)
            throw std::runtime_error("orientation object differs from its observed integrator caller");
        snapshot(capture,cpu,event,instructions,frame,ns);
    }
}

void end_driving_capture() {
    if (!active) throw std::runtime_error("driving capture is not active");
    auto &capture=*active;
    std::ofstream diagnostic(capture.path/"observation.json");
    diagnostic << "{\"observed_instructions\":" << capture.observed
               << ",\"rom_integrator_entries\":" << capture.rom_entries
               << ",\"relocated_integrator_entries\":" << capture.ram_entries
               << ",\"bounded_calls\":" << capture.calls
               << ",\"completed_calls\":" << capture.completed << "}\n";
    diagnostic.close();
    if (!diagnostic) throw std::runtime_error("cannot write capture observation counts");
    std::ofstream coverage(capture.path/"code-coverage.jsonl");
    for (std::size_t i=0;i<capture.code_counts.size();++i) if (capture.code_counts[i])
        coverage << "{\"pc\":" << (0x20000+4*(i%0x8000)+(i>=0x8000 ? relocation : 0))
                 << ",\"count\":" << capture.code_counts[i] << "}\n";
    coverage.close();
    if (!coverage) throw std::runtime_error("cannot write bounded code execution coverage");
    if (capture.call.id || capture.query_return || !capture.calls || !capture.detailed || capture.completed!=capture.calls)
        throw std::runtime_error("driving capture has no complete bounded caller evidence");
    capture.events.close();capture.cadence.close();capture.selectors.close();
    if (!capture.events||!capture.cadence||!capture.selectors) throw std::runtime_error("cannot finish driving capture streams");
    std::ofstream summary(capture.path/"capture.json.tmp");
    summary << "{\"schema\":1,\"complete\":true,\"first_frame\":" << capture.first
        << ",\"last_frame\":" << capture.last << ",\"calls\":" << capture.calls
        << ",\"completed_calls\":" << capture.completed << ",\"detailed_calls\":" << capture.detailed
        << ",\"primary_calls\":" << capture.primary_calls << ",\"alternate_calls\":" << capture.alternate_calls
        << ",\"selector_observations\":" << capture.selector_count
        << ",\"events\":" << capture.events_count << ",\"bytes\":" << capture.bytes
        << ",\"sampling\":\"up to four calls at each selected input/60-frame checkpoint; maximum128 detailed calls\"}\n";
    summary.close();if (!summary) throw std::runtime_error("cannot finish driving capture marker");
    std::filesystem::rename(capture.path/"events.jsonl.tmp",capture.path/"events.jsonl");
    std::filesystem::rename(capture.path/"cadence.jsonl.tmp",capture.path/"cadence.jsonl");
    std::filesystem::rename(capture.path/"selectors.jsonl.tmp",capture.path/"selectors.jsonl");
    std::filesystem::rename(capture.path/"capture.json.tmp",capture.path/"capture.json");
    active.reset();
}
} // namespace ps2::offline
