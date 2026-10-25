#include "driving_capture.h"
#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace ps2::offline {
namespace {
constexpr std::uint32_t relocation = 0x59f000;
constexpr std::size_t byte_limit = 16 * 1024 * 1024;
struct ReturnFrame {
    std::uint32_t pc=0,frame_pointer=0;
    int cache_index=-1;
    std::array<std::uint32_t,16> words{};
};
struct Call {
    std::uint64_t id = 0, instructions = 0, frame = 0;
    std::uint32_t object = 0, descriptor = 0, return_pc = 0, caller_fp = 0, entry_pc = 0, branch_selector = 0;
    bool detailed = false;
    bool orientation_seen = false;
};
struct Orientation {
    std::uint32_t frame_pointer = 0, candidate = 0;
    unsigned phase = 0;
};
struct Contact {
    std::uint32_t return_pc = 0, caller_fp = 0, frame_pointer = 0, point = 0, output = 0;
    bool reply_seen = false;
    std::uint32_t last_ret_pc = 0, last_ret_pfp = 0, last_ret_rip = 0, last_ret_fp = 0;
    ReturnFrame returning;
};
struct Steering {
    std::uint32_t frame_pointer = 0, return_pc = 0, input = 0;
    bool detailed = false;
};
struct SteeringFilter {
    std::uint32_t frame_pointer=0;
    ReturnFrame returning;
    bool detailed=false;
};
struct Capture {
    std::filesystem::path path;
    std::ofstream events, cadence, selectors, callbacks, steering, steering_filters;
    unsigned first = 0, last = 0, detailed = 0, events_count = 0, completed = 0;
    std::uint64_t calls = 0, last_count = ~std::uint64_t{}, selected_frame = ~std::uint64_t{};
    unsigned frame_samples = 0;
    std::size_t bytes = 0;
    std::uint64_t observed = 0, rom_entries = 0, ram_entries = 0;
    unsigned primary_calls = 0, alternate_calls = 0, selector_count = 0;
    unsigned callback_count = 0;
    std::uint32_t query_return = 0, query_caller_fp = 0;
    unsigned orientation_entries = 0, orientation_completions = 0;
    unsigned contact_entries = 0, contact_completions = 0;
    unsigned steering_entries = 0, steering_completions = 0, steering_samples = 0;
    std::uint64_t steering_selected_frame = ~std::uint64_t{};
    unsigned filter_entries=0,filter_completions=0,filter_samples=0;
    std::uint64_t filter_selected_frame=~std::uint64_t{};
    Orientation orientation;
    Contact contact;
    Steering steering_call;
    ReturnFrame outer_return,query_return_frame;
    SteeringFilter filter;
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
ReturnFrame caller_return(const rt::Cpu &cpu) {
    ReturnFrame result;
    result.frame_pointer=cpu.m_r[0]&~0x3fu;
    if (!ram(result.frame_pointer,64)) throw std::runtime_error("saved caller frame is outside plain RAM");
    const auto index=std::int64_t(cpu.m_rcache_pos)-1;
    if (index>=0 && index<rt::Cpu::I960_RCACHE_SIZE) {
        result.cache_index=int(index);
        if (cpu.m_rcache_frame_addr[index]!=result.frame_pointer)
            throw std::runtime_error("saved caller register-cache frame mismatch");
        for (unsigned i=0;i<16;++i) result.words[i]=cpu.m_rcache[index][i];
    } else {
        const auto saved=words(cpu,result.frame_pointer,16);
        for (unsigned i=0;i<16;++i) result.words[i]=saved[i];
    }
    result.pc=result.words[2];
    if (!result.pc || result.pc&3u) throw std::runtime_error("saved caller has no aligned return address");
    return result;
}
void return_frame(std::ostream &out,const ReturnFrame &frame) {
    out << "{\"pc\":" << frame.pc << ",\"frame_pointer\":" << frame.frame_pointer
        << ",\"cache_index\":" << frame.cache_index << ",\"source\":\""
        << (frame.cache_index<0 ? "ordinary_ram" : "register_cache") << "\",\"words\":";
    integers(out,frame.words);out << '}';
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
void steering_snapshot(Capture &capture, const rt::Cpu &cpu, const char *event,
                       std::uint64_t instructions, std::uint64_t frame, std::uint64_t ns) {
    if (!capture.steering_call.detailed) return;
    std::ostringstream out;
    out << "{\"schema\":1,\"event\":\"" << event << "\",\"pair_id\":" << capture.steering_entries
        << ",\"frame\":" << frame << ",\"instructions\":" << instructions
        << ",\"cycles\":" << cpu.reference_cycles << ",\"board_ns\":" << ns
        << ",\"pc\":" << cpu.m_IP << ",\"ac\":" << cpu.m_AC
        << ",\"input_address\":2105424,\"input_byte\":" << capture.steering_call.input
        << ",\"return_pc\":" << capture.steering_call.return_pc
        << ",\"frame_pointer\":" << capture.steering_call.frame_pointer << ",\"registers\":";
    integers(out,cpu.m_r);out << '}';
    write(capture,capture.steering,out.str());
}
void filter_snapshot(Capture &capture,const rt::Cpu &cpu,const char *event,
                     std::uint64_t instructions,std::uint64_t frame,std::uint64_t ns) {
    if (!capture.filter.detailed) return;
    std::ostringstream out;
    out << "{\"schema\":1,\"event\":\"" << event << "\",\"pair_id\":" << capture.filter_entries
        << ",\"frame\":" << frame << ",\"instructions\":" << instructions
        << ",\"cycles\":" << cpu.reference_cycles << ",\"board_ns\":" << ns
        << ",\"pc\":" << cpu.m_IP << ",\"ac\":" << cpu.m_AC
        << ",\"frame_pointer\":" << capture.filter.frame_pointer << ",\"return_frame\":";
    return_frame(out,capture.filter.returning);
    out << ",\"registers\":";integers(out,cpu.m_r);
    out << ",\"blocks\":[{\"name\":\"steering_history\",\"address\":" << 0x2142c0
        << ",\"words\":[" << cpu.bus->read_dword(0x2142c0)
        << "]},{\"name\":\"steering_input\",\"address\":" << 0x202050
        << ",\"words\":[" << cpu.bus->read_dword(0x202050) << "]}]}";
    write(capture,capture.steering_filters,out.str());
}
[[noreturn]] void contact_failure(Capture &capture,const rt::Cpu &cpu,const char *reason,
                                  std::uint64_t instructions,std::uint64_t frame) {
    const auto &contact=capture.contact;
    std::ofstream report(capture.path/"contact-failure.json");
    report << "{\"complete\":false,\"reason\":\"" << reason << "\",\"frame\":" << frame
           << ",\"instructions\":" << instructions << ",\"call_id\":" << capture.call.id
           << ",\"object\":" << capture.call.object << ",\"pc\":" << cpu.m_IP
           << ",\"fp\":" << cpu.m_r[31] << ",\"pfp\":" << cpu.m_r[0] << ",\"rip\":" << cpu.m_r[2]
           << ",\"saved_return\":" << contact.return_pc << ",\"saved_caller_fp\":" << contact.caller_fp
           << ",\"saved_helper_fp\":" << contact.frame_pointer << ",\"reply_seen\":" << (contact.reply_seen ? "true" : "false")
           << ",\"last_ret_pc\":" << contact.last_ret_pc << ",\"last_ret_pfp\":" << contact.last_ret_pfp
           << ",\"last_ret_rip\":" << contact.last_ret_rip << ",\"last_ret_fp\":" << contact.last_ret_fp << "}\n";
    report.close();
    if (!report) throw std::runtime_error("cannot save contact pairing failure");
    throw std::runtime_error(reason);
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
    out << ",\"return_frame\":";
    const std::string_view kind(event);
    return_frame(out,kind.starts_with("contact_") ? capture.contact.returning :
                     kind.starts_with("query_") ? capture.query_return_frame : capture.outer_return);
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
    if (std::string_view(event)=="integrator_entry") {
        block("object_pointer_globals",0x2140c0,16);
        block("baseline_object",0x500000,64);
    }
    block("descriptor_prefix",capture.call.descriptor,32);
    const auto index=cpu.bus->read_dword(capture.call.object+0xe4);
    const auto selected=std::uint64_t(capture.call.descriptor)+16*std::uint64_t(index);
    if (selected<=0xffffffffu) block("selected_descriptor",std::uint32_t(selected),4);
    const auto pc=original_pc(cpu.m_IP);
    if (pc==0x2abc0) block("query_point",cpu.m_r[18],3);
    if (pc==0x3110c || pc==0x31154 || pc==0x31158) block("blend_output",cpu.m_r[17],4);
    if (std::string_view(event).starts_with("alternate_orientation_"))
        block("orientation_candidate",capture.orientation.candidate,4);
    if (std::string_view(event).starts_with("contact_")) {
        block("contact_point",capture.contact.point,3);
        block("contact_output",capture.contact.output,8);
        if (pc==0x2adac) {
            block("contact_reply_prefix",capture.contact.frame_pointer+0x60,12);
            block("contact_reply_tail",capture.contact.frame_pointer+0x50,4);
        }
    }
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
    capture->callbacks.open(path/"callbacks.jsonl.tmp");
    capture->steering.open(path/"steering.jsonl.tmp");
    capture->steering_filters.open(path/"steering-filter.jsonl.tmp");
    if (!capture->events||!capture->cadence||!capture->selectors||!capture->callbacks||!capture->steering||!capture->steering_filters)
        throw std::runtime_error("cannot open driving capture");
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
    const auto pc=original_pc(cpu.m_IP);
    if (capture.contact.return_pc && (pc==0x2ae68 || pc==0x2aeb4 || pc==0x2aec0)) {
        capture.contact.last_ret_pc=cpu.m_IP;capture.contact.last_ret_pfp=cpu.m_r[0];
        capture.contact.last_ret_rip=cpu.m_r[2];capture.contact.last_ret_fp=cpu.m_r[31];
    }
    if (capture.contact.return_pc && cpu.m_IP==capture.contact.return_pc && cpu.m_r[31]!=capture.contact.caller_fp)
        contact_failure(capture,cpu,"contact helper reached return address in a different frame",instructions,frame);
    if (capture.contact.return_pc && cpu.m_IP==capture.contact.return_pc && cpu.m_r[31]==capture.contact.caller_fp) {
        if (!capture.contact.reply_seen) throw std::runtime_error("contact helper returned without its reply observation");
        snapshot(capture,cpu,"contact_post",instructions,frame,ns);
        ++capture.contact_completions;capture.contact={};
    }
    if (capture.query_return && cpu.m_IP==capture.query_return && cpu.m_r[31]==capture.query_caller_fp) {
        snapshot(capture,cpu,"query_post",instructions,frame,ns);
        capture.query_return=0;
    }
    if (capture.call.id && cpu.m_IP==capture.call.return_pc && cpu.m_r[31]==capture.call.caller_fp) {
        if (capture.query_return) throw std::runtime_error("integrator returned with an unpaired query");
        if (capture.orientation.phase) throw std::runtime_error("integrator returned with incomplete alternate orientation");
        if (capture.contact.return_pc)
            contact_failure(capture,cpu,"integrator returned with an unpaired contact helper",instructions,frame);
        cadence(capture,cpu,"integrator_return",instructions,frame,ns);
        snapshot(capture,cpu,"integrator_return",instructions,frame,ns);
        ++capture.completed;capture.call={};
    }
    if (pc==0x39a90 && frame>=capture.first && frame<=capture.last) {
        if (capture.filter.returning.pc) throw std::runtime_error("nested steering filter");
        if (++capture.filter_entries>16384) throw std::runtime_error("steering filter call limit exceeded");
        const bool detailed=(frame%20==0 || frame==2750) && capture.filter_selected_frame!=frame && capture.filter_samples<32;
        capture.filter={cpu.m_r[31],caller_return(cpu),detailed};
        if (detailed) { ++capture.filter_samples;capture.filter_selected_frame=frame; }
        filter_snapshot(capture,cpu,"steering_filter_pre",instructions,frame,ns);
    }
    if (pc==0x39ae8 && capture.filter.returning.pc) {
        const auto returning=caller_return(cpu);
        if (cpu.m_r[31]!=capture.filter.frame_pointer || returning.pc!=capture.filter.returning.pc ||
            returning.frame_pointer!=capture.filter.returning.frame_pointer)
            throw std::runtime_error("steering filter return or frame mismatch");
        filter_snapshot(capture,cpu,"steering_filter_post",instructions,frame,ns);
        ++capture.filter_completions;capture.filter={};
    }
    if (pc==0x395d8 && frame>=capture.first && frame<=capture.last) {
        if (capture.steering_call.return_pc || !cpu.m_r[30])
            throw std::runtime_error("nested steering leaf or missing return identity");
        if (++capture.steering_entries>16384) throw std::runtime_error("steering call limit exceeded");
        const bool detailed=(frame%20==0 || frame==2750) && capture.steering_selected_frame!=frame && capture.steering_samples<32;
        capture.steering_call={cpu.m_r[31],cpu.m_r[30],0,detailed};
        if (detailed) {
            ++capture.steering_samples;capture.steering_selected_frame=frame;
            capture.steering_call.input=cpu.bus->read_dword(0x202050)&255u;
        }
        steering_snapshot(capture,cpu,"steering_pre",instructions,frame,ns);
    }
    if (pc==0x39664 && capture.steering_call.return_pc) {
        if (cpu.m_r[31]!=capture.steering_call.frame_pointer || cpu.m_r[17]!=capture.steering_call.return_pc)
            throw std::runtime_error("steering leaf return or frame mismatch");
        steering_snapshot(capture,cpu,"steering_post",instructions,frame,ns);
        ++capture.steering_completions;capture.steering_call={};
    }
    if (frame>=capture.first && frame<=capture.last && pc>=0x20000 && pc<0x40000 && !(pc&3))
        ++capture.code_counts[(cpu.m_IP==pc ? 0 : 0x8000)+(pc-0x20000)/4];
    if (pc==0x2aba8 && frame>=capture.first && frame<=capture.last) {
        if (++capture.callback_count>16384) throw std::runtime_error("callback observation limit exceeded");
        std::ostringstream out;
        out << "{\"frame\":" << frame << ",\"instructions\":" << instructions
            << ",\"pc\":" << cpu.m_IP << ",\"object\":" << cpu.m_r[16]
            << ",\"callback\":" << cpu.m_r[20] << '}';
        write(capture,capture.callbacks,out.str());
    }
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
                            frame==2600 || frame==2601 || frame==2700 || frame==2701 || frame==2752;
        const bool detailed=selected && capture.frame_samples<4 && capture.detailed<128;
        if (detailed) { ++capture.frame_samples;++capture.detailed; }
        if (!ram(cpu.m_r[16],0xec)) throw std::runtime_error("integrator observation requested non-RAM object");
        const auto selector=cpu.bus->read_dword(cpu.m_r[16]+0xe8);
        if (pc==0x2cce0) ++capture.primary_calls; else ++capture.alternate_calls;
        capture.outer_return=caller_return(cpu);
        capture.call={capture.calls,instructions,frame,cpu.m_r[16],cpu.m_r[17],capture.outer_return.pc,
                      capture.outer_return.frame_pointer,cpu.m_IP,selector,detailed};
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
        capture.query_return_frame=caller_return(cpu);
        capture.query_return=capture.query_return_frame.pc;capture.query_caller_fp=capture.query_return_frame.frame_pointer;
        event="query_pre";break;
    case 0x2ac08:event="query_count";break;
    case 0x2acc0: {
        if (capture.contact.return_pc) throw std::runtime_error("nested or unpaired contact helper");
        if (!ram(cpu.m_r[16],12) || !ram(cpu.m_r[17],32) ||
            !ram(cpu.m_r[31],0x90))
            throw std::runtime_error("contact observation requires bounded ordinary RAM");
        const auto returning=caller_return(cpu);
        capture.contact={};capture.contact.returning=returning;
        capture.contact.return_pc=returning.pc;capture.contact.caller_fp=returning.frame_pointer;
        capture.contact.frame_pointer=cpu.m_r[31];capture.contact.point=cpu.m_r[16];capture.contact.output=cpu.m_r[17];
        ++capture.contact_entries;event="contact_pre";break;
    }
    case 0x2adac:
        if (!capture.contact.return_pc || capture.contact.reply_seen || cpu.m_r[31]!=capture.contact.frame_pointer)
            throw std::runtime_error("contact reply has no matching helper frame");
        capture.contact.reply_seen=true;event="contact_reply";break;
    case 0x2ecb4: {
        if (original_pc(capture.call.entry_pc)!=0x2e650 || capture.orientation.phase || capture.call.orientation_seen)
            throw std::runtime_error("alternate orientation has no unique alternate caller");
        const auto candidate=std::uint64_t(cpu.m_r[31])+0x90;
        if (candidate>0xffffffffu || !ram(std::uint32_t(candidate),16) ||
            cpu.m_r[5]!=candidate || cpu.m_r[12]!=capture.call.object)
            throw std::runtime_error("alternate orientation input frame or object mismatch");
        capture.orientation={cpu.m_r[31],std::uint32_t(candidate),1};
        capture.call.orientation_seen=true;++capture.orientation_entries;
        event="alternate_orientation_pre";break;
    }
    case 0x2ece0:
    case 0x2ee00:
        if (capture.orientation.phase!=(pc==0x2ece0 ? 1u : 2u) ||
            cpu.m_r[31]!=capture.orientation.frame_pointer || cpu.m_r[5]!=capture.orientation.candidate ||
            cpu.m_r[12]!=capture.call.object)
            throw std::runtime_error("alternate orientation sequence or identity mismatch");
        capture.orientation.phase=2;
        event=pc==0x2ece0 ? "alternate_orientation_folded" : "alternate_orientation_post";
        break;
    case 0x2e560:event="orientation_pre";break;
    case 0x2e638:event="orientation_post";break;
    case 0x2e648:event="integrator_before_return";break;
    default:break;
    }
    if (event) {
        if ((pc==0x2e560 || pc==0x2e638) && cpu.m_r[15]!=capture.call.object)
            throw std::runtime_error("orientation object differs from its observed integrator caller");
        snapshot(capture,cpu,event,instructions,frame,ns);
        if (pc==0x2ee00) { ++capture.orientation_completions;capture.orientation={}; }
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
    if (capture.call.id || capture.query_return || capture.contact.return_pc || capture.orientation.phase || capture.steering_call.return_pc || capture.filter.returning.pc ||
        !capture.calls || !capture.detailed || capture.completed!=capture.calls ||
        capture.orientation_entries!=capture.orientation_completions || capture.contact_entries!=capture.contact_completions ||
        capture.steering_entries!=capture.steering_completions || capture.filter_entries!=capture.filter_completions)
        throw std::runtime_error("driving capture has no complete bounded caller evidence");
    capture.events.close();capture.cadence.close();capture.selectors.close();capture.callbacks.close();capture.steering.close();
    capture.steering_filters.close();
    if (!capture.events||!capture.cadence||!capture.selectors||!capture.callbacks||!capture.steering||!capture.steering_filters)
        throw std::runtime_error("cannot finish driving capture streams");
    std::ofstream summary(capture.path/"capture.json.tmp");
    summary << "{\"schema\":1,\"complete\":true,\"first_frame\":" << capture.first
        << ",\"last_frame\":" << capture.last << ",\"calls\":" << capture.calls
        << ",\"completed_calls\":" << capture.completed << ",\"detailed_calls\":" << capture.detailed
        << ",\"primary_calls\":" << capture.primary_calls << ",\"alternate_calls\":" << capture.alternate_calls
        << ",\"selector_observations\":" << capture.selector_count
        << ",\"callback_observations\":" << capture.callback_count
        << ",\"alternate_orientation_entries\":" << capture.orientation_entries
        << ",\"alternate_orientation_completions\":" << capture.orientation_completions
        << ",\"contact_entries\":" << capture.contact_entries
        << ",\"contact_completions\":" << capture.contact_completions
        << ",\"steering_entries\":" << capture.steering_entries
        << ",\"steering_completions\":" << capture.steering_completions
        << ",\"steering_samples\":" << capture.steering_samples
        << ",\"steering_filter_entries\":" << capture.filter_entries
        << ",\"steering_filter_completions\":" << capture.filter_completions
        << ",\"steering_filter_samples\":" << capture.filter_samples
        << ",\"events\":" << capture.events_count << ",\"bytes\":" << capture.bytes
        << ",\"sampling\":\"up to four calls at each selected input/60-frame checkpoint; maximum128 detailed calls\"}\n";
    summary.close();if (!summary) throw std::runtime_error("cannot finish driving capture marker");
    std::filesystem::rename(capture.path/"events.jsonl.tmp",capture.path/"events.jsonl");
    std::filesystem::rename(capture.path/"cadence.jsonl.tmp",capture.path/"cadence.jsonl");
    std::filesystem::rename(capture.path/"selectors.jsonl.tmp",capture.path/"selectors.jsonl");
    std::filesystem::rename(capture.path/"callbacks.jsonl.tmp",capture.path/"callbacks.jsonl");
    std::filesystem::rename(capture.path/"steering.jsonl.tmp",capture.path/"steering.jsonl");
    std::filesystem::rename(capture.path/"steering-filter.jsonl.tmp",capture.path/"steering-filter.jsonl");
    std::filesystem::rename(capture.path/"capture.json.tmp",capture.path/"capture.json");
    active.reset();
}
} // namespace ps2::offline
