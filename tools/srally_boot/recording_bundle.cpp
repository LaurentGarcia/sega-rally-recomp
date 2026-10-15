#include "recording_bundle.h"
#include "runtime/sha256_digest.h"
#include <bit>
#include <fstream>
#include <limits>
#include <sstream>

namespace srally::recording {
namespace {
namespace hi = host_input;
constexpr uint64_t TEXT_LIMIT = 64 * 1024 * 1024, REPORT_LIMIT = 1024 * 1024;
constexpr uint64_t IMAGE_LIMIT = 256 * 1024 * 1024, EXECUTABLE_LIMIT = 1024 * 1024 * 1024;
constexpr const char *MODEL = "model main25MHz sound11289600 scsp22579200 pcm44100-le-f32 instruction_boundary-v1 rng-lcg32-9d14abd7-1664525-1013904223-rotate16";
constexpr const char *NAMES[] = {"program", "main_data", "tgp_tables", "copro_data", "polygons", "textures", "sound_program", "sound_samples"};
[[noreturn]] void fail(const char *s) { throw std::runtime_error(std::string("recording bundle: ") + s); }
struct Digest { uint64_t bytes = 0; std::string hash; bool operator==(const Digest &) const = default; };
Digest digest(std::span<const uint8_t> data) { return {data.size(), rt::sha256_hex(data)}; }
Digest digest_file(const std::filesystem::path &p, uint64_t limit) {
    const auto d = rt::sha256_file(p, limit); return {d.bytes, d.hex};
}
void valid_hash(const std::string &s) {
    if (s.size() != 64 || s.find_first_not_of("0123456789abcdef") != std::string::npos) fail("invalid SHA-256");
}
std::array<bool, 10> bits(const Machine::Options &o) {
    return {o.tgp,o.io,o.comm,o.control,o.video,o.screen,o.uart,o.external_video,o.sound,o.instruction_boundary_hook};
}
Machine::Options options(const std::array<bool, 10> &b) { return {b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],b[8],b[9]}; }
auto image_refs(const Machine::Images &i) {
    return std::array<const std::vector<uint8_t> *, 8>{&i.program,&i.main_data,&i.tgp_tables,&i.copro_data,&i.polygons,&i.textures,&i.sound_program,&i.sound_samples};
}
auto image_refs(Machine::Images &i) {
    return std::array<std::vector<uint8_t> *, 8>{&i.program,&i.main_data,&i.tgp_tables,&i.copro_data,&i.polygons,&i.textures,&i.sound_program,&i.sound_samples};
}
void config(const Machine::Options &o) {
    if (!(o.io && o.screen && o.video && o.tgp && o.control) || (o.sound && !o.uart)) fail("unsupported recording Machine configuration");
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
    if (std::endian::native != std::endian::little) fail("PCM format requires little-endian IEEE binary32");
}
std::string line(std::istream &in, uint64_t &bytes, uint64_t limit = TEXT_LIMIT) {
    std::string s;
    for (;;) {
        const auto c = in.get(); if (c == std::char_traits<char>::eof()) fail("truncated text artifact");
        if (++bytes > limit) fail("text artifact exceeds byte bound");
        if (c == '\n') return s;
        if (s.size() == 2048) fail("text line exceeds bound"); s += char(c);
    }
}
void end(std::istream &in) {
    if (in.peek() != std::char_traits<char>::eof() || in.bad()) fail("data after artifact end");
}
void tag(std::istream &in, const char *expected) {
    std::string actual; if (!(in >> actual) || actual != expected) fail("unexpected text field");
}
Digest read_digest(std::istream &in) {
    Digest d; d.bytes = hi::detail::number(in); if (!(in >> d.hash)) fail("missing digest"); valid_hash(d.hash); hi::detail::exhausted(in); return d;
}
void digest_line(std::ostream &out, const char *name, const Digest &d) { out << name << ' ' << d.bytes << ' ' << d.hash << '\n'; }
void write(const std::filesystem::path &path, std::span<const uint8_t> data) {
    if (std::filesystem::exists(path)) fail("artifact already exists");
    std::ofstream file(path, std::ios::binary); file.write(reinterpret_cast<const char *>(data.data()), std::streamsize(data.size()));
    file.flush(); if (!file) fail("cannot write artifact");
}
std::span<const uint8_t> bytes(const std::string &s) { return {reinterpret_cast<const uint8_t *>(s.data()), s.size()}; }
void write(const std::filesystem::path &path, const std::string &s) { write(path, bytes(s)); }
std::string text(const std::vector<uint8_t> &s) { return {s.begin(), s.end()}; }
struct Manifest {
    Machine::Options opts;
    Digest executable, assets, eeprom, backup;
    std::array<Digest, 8> images;
    Witness initial;
};
std::string serialize(const Manifest &m) {
    std::ostringstream out; out << "segarally-recording-v1\nscope same_machine\nsource identity_only\n" << MODEL << "\noptions";
    for (bool b : bits(m.opts)) out << ' ' << b; out << '\n';
    digest_line(out,"executable",m.executable); digest_line(out,"assets",m.assets);
    for (size_t n = 0; n < 8; ++n) digest_line(out,NAMES[n],m.images[n]);
    digest_line(out,"eeprom_initial",m.eeprom); digest_line(out,"backup_initial",m.backup);
    out << "initial "; hi::detail::witness(out,m.initial); out << "\nend\n"; return out.str();
}
Manifest parse_manifest(const std::string &s) {
    std::istringstream in(s); uint64_t count = 0; Manifest m;
    for (const char *expected : {"segarally-recording-v1","scope same_machine","source identity_only",MODEL})
        if (line(in,count,8192) != expected) fail("unknown manifest/model/scope");
    std::istringstream flags(line(in,count,8192)); tag(flags,"options"); std::array<bool,10> b{};
    for (auto &v : b) { const auto n = hi::detail::number(flags); if (n > 1) fail("unknown Machine option"); v = bool(n); }
    hi::detail::exhausted(flags); m.opts = options(b); config(m.opts);
    auto read = [&](const char *name) { std::istringstream fields(line(in,count,8192)); tag(fields,name); return read_digest(fields); };
    m.executable = read("executable"); m.assets = read("assets");
    uint64_t total = 0;
    for (size_t n = 0; n < 8; ++n) { m.images[n] = read(NAMES[n]); if (m.images[n].bytes > IMAGE_LIMIT) fail("image too large"); total += m.images[n].bytes; }
    if (total > 512 * 1024 * 1024 || m.executable.bytes > EXECUTABLE_LIMIT || !m.executable.bytes ||
        m.assets.bytes > REPORT_LIMIT || !m.assets.bytes) fail("startup identities exceed bounds");
    if (!m.opts.sound && (m.images[6].bytes || m.images[7].bytes)) fail("disabled sound images are not loaded inputs");
    m.eeprom = read("eeprom_initial"); m.backup = read("backup_initial");
    if (m.eeprom.bytes != 128 || m.backup.bytes != 16384) fail("wrong startup seed sizes");
    std::istringstream initial(line(in,count,8192)); tag(initial,"initial"); m.initial = hi::detail::witness(initial); hi::detail::exhausted(initial);
    if (m.initial.instructions || line(in,count,8192) != "end") fail("invalid initial state/end"); end(in);
    if (serialize(m) != s) fail("noncanonical manifest"); return m;
}
hi::Context context(const Manifest &m) { return {rt::sha256_hex(bytes(serialize(m))),m.opts.sound,m.opts.instruction_boundary_hook}; }
hi::ApproveContext approval(hi::Context expected) {
    // Only generated from this module's ACTUAL hashes/validated canonical
    // manifest. This callback does not independently authenticate generation.
    return [=](const hi::Context &c) { if (c != expected) fail("transcript context mismatch"); };
}
std::string hex_message(const std::string &s) {
    if (s.size() > 512) fail("fault message exceeds capture bound"); if (s.empty()) return "-";
    constexpr char h[] = "0123456789abcdef"; std::string out;
    for (unsigned char c : s) { out += h[c >> 4]; out += h[c & 15]; } return out;
}
std::string unhex_message(const std::string &s) {
    if (s == "-") return {}; if (s.empty() || s.size() > 1024 || (s.size() & 1)) fail("invalid fault message");
    auto digit = [](char c) { const auto p = std::string("0123456789abcdef").find(c); if (p == std::string::npos) fail("invalid message hex"); return unsigned(p); };
    std::string out; for (size_t i = 0; i < s.size(); i += 2) out += char(digit(s[i]) * 16 + digit(s[i+1])); return out;
}
struct Observation {
    uint64_t index = 0, samples = 0;
    std::string pcm;
    Machine::RunResult result;
};
std::string observation_line(const Observation &o) {
    std::ostringstream out; out << "resume " << o.index << ' ' << o.samples << ' ' << o.pcm << ' ' << hi::detail::reason(o.result.reason) << ' '
        << std::hex << std::setw(8) << std::setfill('0') << o.result.pc << std::dec << ' ' << o.result.instructions << ' '
        << std::hex << std::setw(8) << o.result.address << ' ' << std::setw(8) << o.result.value << std::dec << ' '
        << o.result.width << ' ' << o.result.write << ' ' << hex_message(o.result.message); return out.str();
}
Observation read_observation(std::istream &in, uint64_t &used, const hi::Resume &r, bool sound) {
    const auto raw = line(in,used); std::istringstream f(raw); tag(f,"resume"); Observation o;
    o.index = hi::detail::number(f); o.samples = hi::detail::number(f); if (!(f >> o.pcm)) fail("missing PCM digest"); valid_hash(o.pcm);
    o.result.reason = hi::detail::reason(f);
    auto word = [&] { const auto n = hi::detail::number(f,16,8); if (n > UINT32_MAX) fail("fault word overflow"); return uint32_t(n); };
    o.result.pc = word(); o.result.instructions = hi::detail::number(f); o.result.address = word(); o.result.value = word();
    const auto width = hi::detail::number(f), writing = hi::detail::number(f);
    if (width > 4 || writing > 1 || (width && width != 1 && width != 2 && width != 4)) fail("invalid fault width/write");
    o.result.width = unsigned(width); o.result.write = bool(writing); std::string message;
    if (!(f >> message)) fail("missing fault message"); o.result.message = unhex_message(message); hi::detail::exhausted(f);
    if (o.index != r.index || o.result.reason != r.reason || o.result.instructions != r.after.instructions || o.result.pc != r.after.pc ||
        o.samples > 2 * Machine::kAudioRate || (o.samples & 1) || (!sound && o.samples)) fail("observation/resume mismatch");
    if (!hi::detail::frontier(o.result.reason) && (o.result.width || o.result.write || o.result.address || o.result.value || !o.result.message.empty())) fail("normal return has fault details");
    if (observation_line(o) != raw) fail("noncanonical observation"); return o;
}
Digest pcm_digest(std::span<const float> pcm) { return digest({reinterpret_cast<const uint8_t *>(pcm.data()),pcm.size_bytes()}); }
void compare_result(const Machine::RunResult &a, const Machine::RunResult &b) {
    if (std::tie(a.reason,a.pc,a.instructions,a.message,a.address,a.value,a.width,a.write) !=
        std::tie(b.reason,b.pc,b.instructions,b.message,b.address,b.value,b.width,b.write)) fail("replayed return/fault details differ");
}
struct Completion {
    Digest manifest, transcript, observations, eeprom, backup;
    uint64_t records = 0;
    Witness terminal;
    TerminalKind kind = TerminalKind::Quit;
};
std::string serialize(const Completion &c) {
    std::ostringstream out; out << "segarally-recording-complete-v1\n";
    digest_line(out,"manifest",c.manifest); digest_line(out,"transcript",c.transcript); digest_line(out,"observations",c.observations);
    digest_line(out,"eeprom_final",c.eeprom); digest_line(out,"backup_final",c.backup);
    out << "records " << c.records << "\nterminal " << terminal_name(c.kind) << ' ';
    hi::detail::witness(out,c.terminal); out << "\nend\n"; return out.str();
}
Completion completion(const std::filesystem::path &dir) {
    const auto raw = text(read_bounded(dir/"complete.txt",8192)); std::istringstream in(raw); uint64_t used = 0; Completion c;
    if (line(in,used,8192) != "segarally-recording-complete-v1") fail("missing/unknown completion");
    auto read = [&](const char *key) { std::istringstream f(line(in,used,8192)); tag(f,key); return read_digest(f); };
    c.manifest=read("manifest");c.transcript=read("transcript");c.observations=read("observations");c.eeprom=read("eeprom_final");c.backup=read("backup_final");
    std::istringstream count(line(in,used,8192));tag(count,"records");c.records=hi::detail::number(count);hi::detail::exhausted(count);
    if (c.records > 250000) fail("record bound exceeded");
    std::istringstream final(line(in,used,8192));tag(final,"terminal");std::string kind;final>>kind;
    bool found=false;for(auto k:{TerminalKind::Quit,TerminalKind::FrameLimit,TerminalKind::InstructionLimit,TerminalKind::Frontier,TerminalKind::Fault})if(kind==terminal_name(k)){c.kind=k;found=true;}
    if(!found)fail("unknown terminal kind");
    c.terminal=hi::detail::witness(final);hi::detail::exhausted(final);if(line(in,used,8192)!="end")fail("missing completion end");end(in);
    if (serialize(c)!=raw || c.eeprom.bytes!=128 || c.backup.bytes!=16384) fail("noncanonical/invalid completion");return c;
}
void observation_end(std::istream &in,uint64_t &used,uint64_t count) {
    if(line(in,used)!="end "+std::to_string(count))fail("observation footer mismatch");end(in);
}
} // namespace

const char *terminal_name(TerminalKind kind) {
    switch(kind){case TerminalKind::Quit:return "quit";case TerminalKind::FrameLimit:return "frame_limit";
    case TerminalKind::InstructionLimit:return "instruction_limit";case TerminalKind::Frontier:return "frontier";case TerminalKind::Fault:return "fault";}
    fail("unknown terminal kind");
}
namespace {
void terminal_reason(TerminalKind kind,Machine::Reason reason,uint64_t instructions) {
    if(kind==TerminalKind::Fault){if(reason!=Machine::Reason::RuntimeError)fail("fault terminal lacks runtime fault");}
    else if(kind==TerminalKind::Frontier){if(reason!=Machine::Reason::MissingCode&&reason!=Machine::Reason::DeviceAccess)fail("frontier terminal lacks frontier");}
    else if(kind==TerminalKind::FrameLimit){if(reason!=Machine::Reason::Frame)fail("frame limit terminal lacks frame return");}
    else if(kind==TerminalKind::InstructionLimit){
        if(!instructions||(reason!=Machine::Reason::Budget&&reason!=Machine::Reason::Frame))fail("instruction limit terminal lacks executed instructions");
    }
    else if(hi::detail::frontier(reason))fail("normal terminal follows a persistent frontier/fault");
}
}

std::vector<uint8_t> read_bounded(const std::filesystem::path &path,uint64_t maximum_bytes) {
    if (maximum_bytes > IMAGE_LIMIT) fail("invalid read byte bound");
    if (!std::filesystem::is_regular_file(path)) fail("missing/nonregular input file");
    std::ifstream in(path,std::ios::binary); if(!in)fail("cannot open input file");std::vector<uint8_t> result;std::array<char,65536> buffer;
    for (;;) {
        const auto requested=std::min(uint64_t(buffer.size()),maximum_bytes-result.size()+1);
        in.read(buffer.data(),std::streamsize(requested));if(in.bad()||(in.fail()&&!in.eof()))fail("input read failure");
        const auto n=size_t(in.gcount());if(n>maximum_bytes-result.size())fail("input exceeds byte bound");
        result.insert(result.end(),reinterpret_cast<const uint8_t *>(buffer.data()),reinterpret_cast<const uint8_t *>(buffer.data())+n);
        if(in.eof())return result;
    }
}
struct StartupIdentity::Data { Manifest manifest; std::vector<uint8_t> assets; };
StartupIdentity identify_startup(const Machine::Images &images,Machine::Options opts,std::span<const uint8_t> assets,const std::filesystem::path &exe) {
    config(opts);if(assets.empty()||assets.size()>REPORT_LIMIT)fail("assets report is missing/oversize");
    auto data=std::make_shared<StartupIdentity::Data>();data->manifest.opts=opts;data->assets.assign(assets.begin(),assets.end());
    data->manifest.assets=digest(assets);data->manifest.executable=digest_file(exe,EXECUTABLE_LIMIT);
    uint64_t total=0;const auto refs=image_refs(images);
    for(size_t n=0;n<8;++n){if(refs[n]->size()>IMAGE_LIMIT)fail("loaded image exceeds bound");total+=refs[n]->size();data->manifest.images[n]=digest(*refs[n]);}
    if(total>512*1024*1024||!data->manifest.executable.bytes)fail("startup identity exceeds bounds");
    if(!opts.sound&&(!images.sound_program.empty()||!images.sound_samples.empty()))fail("disabled sound images must be absent");
    StartupIdentity result;result.data_=std::move(data);return result;
}
struct Recorder::Impl {
    std::filesystem::path directory;
    Manifest manifest;
    std::ofstream resumes,observations;
    std::unique_ptr<hi::Writer> writer;
    uint64_t observation_bytes=0;
    bool failed=false,finished=false;
    Machine::Reason last_reason=Machine::Reason::Budget;
    void observation(const std::string &s){if(s.size()>2048||s.size()+1>TEXT_LIMIT-observation_bytes)fail("observation stream exceeds bound");observations<<s<<'\n';if(!observations)fail("cannot write observations");observation_bytes+=s.size()+1;}
};
Recorder::Recorder(const std::filesystem::path &dir,StartupIdentity identity,const Machine &machine):impl_(std::make_unique<Impl>()) {
    if(!identity.data_)fail("actual loaded startup identity required");auto &p=*impl_;p.directory=dir;p.manifest=identity.data_->manifest;
    const auto eeprom=machine.save_eeprom(),backup=machine.save_backup();p.manifest.initial=Witness::of(machine.stats());
    p.manifest.eeprom=digest(eeprom);p.manifest.backup=digest(backup);
    const auto m=serialize(p.manifest);(void)parse_manifest(m);
    if(!dir.parent_path().empty())std::filesystem::create_directories(dir.parent_path());
    if(!std::filesystem::create_directory(dir))fail("capture directory already exists");
    write(dir/"manifest.txt",m);write(dir/"assets.json",identity.data_->assets);write(dir/"eeprom.initial.bin",eeprom);write(dir/"backup.initial.bin",backup);
    p.resumes.open(dir/"resumes.txt",std::ios::binary);p.observations.open(dir/"observations.txt",std::ios::binary);
    const auto ctx=context(p.manifest);p.writer=std::make_unique<hi::Writer>(p.resumes,ctx,p.manifest.initial,approval(ctx));p.observation("segarally-recording-observations-v1");
}
Recorder::~Recorder()=default;
uint64_t Recorder::records()const{return impl_->writer->records();}
void Recorder::append(const hi::Resume &r,const Machine::RunResult &outcome,std::span<const float> pcm) {
    auto &p=*impl_;try{
        if(p.failed||p.finished)fail("capture failed/finished");
        if(pcm.size()>2*Machine::kAudioRate||(pcm.size()&1)||(!p.manifest.opts.sound&&!pcm.empty()))fail("PCM block exceeds capture configuration/bound");
        Observation o{r.index,pcm.size(),pcm_digest(pcm).hash,outcome};
        const auto raw=observation_line(o);std::istringstream check(raw+'\n');uint64_t used=0;(void)read_observation(check,used,r,p.manifest.opts.sound);
        p.writer->append(r);p.observation(raw);p.last_reason=outcome.reason;
    }catch(...){p.failed=true;throw;}
}
void Recorder::finish(const Machine &machine,TerminalKind kind) {
    auto &p=*impl_;try{
        if(p.failed||p.finished)fail("capture failed/finished");const auto terminal=Witness::of(machine.stats());terminal_reason(kind,p.last_reason,terminal.instructions);
        // Check the terminal witness before writing final state or any footer.
        p.writer->finish(terminal);p.observation("end "+std::to_string(p.writer->records()));p.observations.flush();
        if(!p.observations)fail("cannot finish observations");p.resumes.close();p.observations.close();if(p.resumes.fail()||p.observations.fail())fail("cannot close capture streams");
        const auto eeprom=machine.save_eeprom(),backup=machine.save_backup();if(eeprom.size()!=128||backup.size()!=16384)fail("wrong final seed sizes");
        write(p.directory/"eeprom.final.bin",eeprom);write(p.directory/"backup.final.bin",backup);
        Completion c{digest_file(p.directory/"manifest.txt",8192),digest_file(p.directory/"resumes.txt",TEXT_LIMIT),digest_file(p.directory/"observations.txt",TEXT_LIMIT),digest(eeprom),digest(backup),p.writer->records(),terminal,kind};
        if(std::filesystem::exists(p.directory/"complete.txt"))fail("completion artifact already exists");
        write(p.directory/"complete.tmp",serialize(c));std::filesystem::rename(p.directory/"complete.tmp",p.directory/"complete.txt");p.finished=true;
    }catch(...){p.failed=true;throw;}
}
struct PreflightBundle::Data {
    std::filesystem::path directory;
    Manifest manifest;
    Completion seal;
    Machine::Images images;
    std::vector<uint8_t> assets, initial_eeprom, initial_backup, final_eeprom, final_backup;
    std::string resumes, observations;
};
const Machine::Options &PreflightBundle::options() const { return data_->manifest.opts; }
const Machine::Images &PreflightBundle::images() const { return data_->images; }
const std::vector<uint8_t> &PreflightBundle::assets_report() const { return data_->assets; }
const std::vector<uint8_t> &PreflightBundle::initial_eeprom() const { return data_->initial_eeprom; }
const std::vector<uint8_t> &PreflightBundle::initial_backup() const { return data_->initial_backup; }
const std::vector<uint8_t> &PreflightBundle::final_eeprom() const { return data_->final_eeprom; }
const std::vector<uint8_t> &PreflightBundle::final_backup() const { return data_->final_backup; }
Witness PreflightBundle::initial() const { return data_->manifest.initial; }
hi::Context PreflightBundle::context() const { return recording::context(data_->manifest); }
ReplaySummary PreflightBundle::summary() const {
    const auto &c=data_->seal;return {c.records,c.terminal,c.kind==TerminalKind::Frontier||c.kind==TerminalKind::Fault,c.kind};
}
void PreflightBundle::each(const std::function<void(const hi::Resume &,const Machine::RunResult &,uint64_t,const std::string &)> &visit) const {
    const auto &p=*data_;const auto ctx=context();
    std::istringstream stream(p.resumes),observations(p.observations);
    hi::Reader reader(stream,ctx,p.manifest.initial,approval(ctx));uint64_t used=0;
    if(line(observations,used)!="segarally-recording-observations-v1")fail("unknown observations version");
    Witness last=p.manifest.initial;Machine::Reason last_reason=Machine::Reason::Budget;
    while(const auto r=reader.next()){
        const auto o=read_observation(observations,used,*r,p.manifest.opts.sound);
        if(visit)visit(*r,o.result,o.samples,o.pcm);
        last=r->after;last_reason=r->reason;
    }
    observation_end(observations,used,reader.records());
    if(!reader.complete_transcript()||reader.records()!=p.seal.records||last!=p.seal.terminal)fail("terminal/record count mismatch");
    terminal_reason(p.seal.kind,last_reason,last.instructions);
}
void PreflightBundle::verify_stream_files_unchanged() const {
    const auto &p=*data_;
    if(digest_file(p.directory/"resumes.txt",TEXT_LIMIT)!=p.seal.transcript||
       digest_file(p.directory/"observations.txt",TEXT_LIMIT)!=p.seal.observations)fail("artifact digest/size mismatch");
}
PreflightBundle preflight_bundle(const std::filesystem::path &dir,const std::filesystem::path &data,const std::filesystem::path &exe) {
    const auto seal=completion(dir);
    for(const auto &entry:std::filesystem::directory_iterator(dir)){
        const auto name=entry.path().filename().string();const std::array<const char*,9> allowed{"manifest.txt","assets.json","eeprom.initial.bin","backup.initial.bin","resumes.txt","observations.txt","eeprom.final.bin","backup.final.bin","complete.txt"};
        if(std::find(allowed.begin(),allowed.end(),name)==allowed.end()||entry.symlink_status().type()!=std::filesystem::file_type::regular)fail("unexpected/nonregular bundle artifact");
    }
    auto check=[&](const char *name,const Digest &expected,uint64_t limit){if(digest_file(dir/name,limit)!=expected)fail("artifact digest/size mismatch");};
    check("eeprom.final.bin",seal.eeprom,128);check("backup.final.bin",seal.backup,16384);
    const auto manifest_bytes=read_bounded(dir/"manifest.txt",8192);if(digest(manifest_bytes)!=seal.manifest)fail("actual manifest bytes differ");
    const auto manifest=parse_manifest(text(manifest_bytes));
    // Freeze the exact bounded text bytes that preflight validates. Reopening
    // paths during execution would allow newly edited rows to execute first.
    const auto resume_text=text(read_bounded(dir/"resumes.txt",TEXT_LIMIT)),observation_text=text(read_bounded(dir/"observations.txt",TEXT_LIMIT));
    if(digest(bytes(resume_text))!=seal.transcript||digest(bytes(observation_text))!=seal.observations)fail("actual transcript/observation bytes differ");
    check("assets.json",manifest.assets,REPORT_LIMIT);check("eeprom.initial.bin",manifest.eeprom,128);check("backup.initial.bin",manifest.backup,16384);
    if(digest_file(exe,EXECUTABLE_LIMIT)!=manifest.executable)fail("actual executable differs from captured host");
    const auto assets=read_bounded(data/"assets.json",REPORT_LIMIT);if(digest(assets)!=manifest.assets)fail("actual assets report differs");
    Machine::Images images;auto refs=image_refs(images);
    for(size_t n=0;n<8;++n){if(n<6||manifest.opts.sound)*refs[n]=read_bounded(data/(std::string(NAMES[n])+".bin"),manifest.images[n].bytes);if(digest(*refs[n])!=manifest.images[n])fail("actual loaded image differs");}
    const auto initial_eeprom=read_bounded(dir/"eeprom.initial.bin",128),initial_backup=read_bounded(dir/"backup.initial.bin",16384);
    const auto final_eeprom=read_bounded(dir/"eeprom.final.bin",128),final_backup=read_bounded(dir/"backup.final.bin",16384);
    if(digest(initial_eeprom)!=manifest.eeprom||digest(initial_backup)!=manifest.backup||digest(final_eeprom)!=seal.eeprom||digest(final_backup)!=seal.backup)fail("actual loaded seed bytes differ");
    auto frozen=std::make_shared<PreflightBundle::Data>();frozen->directory=dir;
    frozen->manifest=manifest;frozen->seal=seal;frozen->images=std::move(images);frozen->assets=assets;
    frozen->initial_eeprom=initial_eeprom;frozen->initial_backup=initial_backup;
    frozen->final_eeprom=final_eeprom;frozen->final_backup=final_backup;
    frozen->resumes=resume_text;frozen->observations=observation_text;
    PreflightBundle result(std::move(frozen));result.each({}); // No CPU exists.
    return result;
}
ReplaySummary replay_bundle(const std::filesystem::path &dir,const std::filesystem::path &data,const std::filesystem::path &exe) {
    const auto frozen=preflight_bundle(dir,data,exe);
    Machine machine(frozen.images(),frozen.options());machine.load_eeprom(frozen.initial_eeprom());machine.load_backup(frozen.initial_backup());
    if(Witness::of(machine.stats())!=frozen.initial())fail("actual startup Machine witness differs");
    frozen.each([&](const hi::Resume &r,const Machine::RunResult &expected,uint64_t samples,const std::string &pcm_hash){
        if(Witness::of(machine.stats())!=r.before)fail("replay pre-witness mismatch");machine.inputs()=r.inputs;
        const auto result=machine.run_until_frame(r.budget);const auto pcm=machine.take_audio();
        if(Witness::of(machine.stats())!=r.after)fail("replay post-witness mismatch");compare_result(result,expected);
        if(pcm.size()!=samples||pcm_digest(pcm).hash!=pcm_hash)fail("replay PCM count/raw bits mismatch");
    });
    if(machine.save_eeprom()!=frozen.final_eeprom()||machine.save_backup()!=frozen.final_backup())fail("replay final EEPROM/backup differs");
    frozen.verify_stream_files_unchanged();return frozen.summary();
}
} // namespace srally::recording
