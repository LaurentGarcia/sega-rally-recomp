// ROM-free complete capture through the SAME dual-board consumer used for
// actual recordings. No alternate board provider or instruction interpreter.
#define M2_SRALLY_RECORDING_REFERENCE
#define M2_SRALLY_RECORDING_CONTRACT_HEADER "tests/segarally_recording_fixture_contract.h"
#define main recording_reference_main
#include "test_segarally_boot_parity.cpp"
#undef main
#include <chrono>

namespace srally::recording_contract { recording_reference::BuildBinding fixture; }
namespace {
namespace rec=srally::recording;
using M=rec::Machine;
unsigned checks=0;uint64_t native_calls=0;
constexpr std::array<uint32_t,4> addresses{0x420,0x428,0x430,0x438};
void check(bool c,const char *s){++checks;if(!c)throw rt::Fatal(s);}
M::Images images(){
 M::Images i;i.program.resize(0x600);auto word=[&](uint32_t a,uint32_t w){for(unsigned n=0;n<4;++n)i.program[a+n]=uint8_t(w>>(8*n));};
 word(12,0x420);word(0x420,0x8c803000);word(0x424,0x12345678);
 word(0x428,0x90803000);word(0x42c,0x01d00000);word(0x430,0x90803000);word(0x434,0x01c0001c);word(0x438,0x8c800005);
 i.tgp_tables.resize(0x40000);i.copro_data.resize(0x800000);i.polygons.resize(4);i.textures.resize(2);return i;
}
void write(const std::filesystem::path &p,std::span<const uint8_t> b){std::ofstream o(p,std::ios::binary|std::ios::trunc);o.write(reinterpret_cast<const char*>(b.data()),std::streamsize(b.size()));if(!o)throw rt::Fatal("fixture write failed");}
void write(const std::filesystem::path &p,const std::string &s){write(p,{reinterpret_cast<const uint8_t*>(s.data()),s.size()});}
std::string read(const std::filesystem::path &p){const auto b=rec::read_bounded(p,64*1024*1024);return {b.begin(),b.end()};}
std::string hash(const std::string &s){return rt::sha256_hex({reinterpret_cast<const uint8_t*>(s.data()),s.size()});}
void replace(std::string &s,const std::string &a,const std::string &b){const auto at=s.find(a);check(at!=std::string::npos,"fixture replacement missing");s.replace(at,a.size(),b);}
void reseal(const std::filesystem::path &bundle){
 auto seal=read(bundle/"complete.txt");
 for(const auto &[tag,name]:std::array<std::pair<const char*,const char*>,3>{{{"manifest","manifest.txt"},{"transcript","resumes.txt"},{"observations","observations.txt"}}}){
  const auto value=read(bundle/name);const auto start=seal.find(std::string(tag)+' '),end=seal.find('\n',start);
  check(start!=std::string::npos&&end!=std::string::npos,"fixture seal field missing");seal.replace(start,end-start,std::string(tag)+' '+std::to_string(value.size())+' '+hash(value));
 }
 write(bundle/"complete.txt",seal);
}
int run(const std::filesystem::path &bundle,const std::filesystem::path &data,const std::filesystem::path &exe){
 std::vector<std::string> words{"reference-fixture","--recording",bundle.string(),data.string(),exe.string()};std::vector<char*> args;for(auto &s:words)args.push_back(s.data());return recording_reference_main(int(args.size()),args.data());
}
}
namespace gen {
bool instruction_boundary_hook_compatible(){return true;}
bool has_code(uint32_t pc){return std::find(addresses.begin(),addresses.end(),pc)!=addresses.end();}
uint64_t native_instructions(){return addresses.size();}
void run(Env&e){++native_calls;while(!e.ls.boundary_yielded()&&has_code(e.c.m_IP)&&!e.ls.boundary()){
 const auto pc=e.c.m_IP;if(pc==0x420){e.c.consume_reference_cycles(1);e.c.m_r[16]=0x12345678;e.c.m_IP=0x428;}
 else if(pc==0x428||pc==0x430){e.c.consume_reference_cycles(4);e.c.m_r[16]=e.c.bus->read_dword(pc==0x428?0x01d00000:0x01c0001c);e.c.m_IP+=8;}
 else{e.c.consume_reference_cycles(1);e.c.m_r[16]=5;e.c.m_IP+=4;}++e.ls.count;
}}
}
namespace rt::tgpgen {extern const uint32_t program_crc32=0,program_words=0;void run(Tgp&,uint64_t){throw TgpFatal("fixture must not execute TGP");}}
int main(int argc,char **argv){try{
 check(argc>0,"missing fixture executable");const auto exe=std::filesystem::canonical(argv[0]);
 const auto root=std::filesystem::temp_directory_path()/("segarally-reference-bundle-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code ec;std::filesystem::remove_all(p,ec);}}cleanup{root};
 check(std::filesystem::create_directory(root),"exclusive fixture directory failed");const auto data=root/"data",bundle=root/"bundle";std::filesystem::create_directory(data);
 const auto i=images();const std::array<const std::vector<uint8_t>*,6> vectors{&i.program,&i.main_data,&i.tgp_tables,&i.copro_data,&i.polygons,&i.textures};
 const std::array<const char*,6> names{"program","main_data","tgp_tables","copro_data","polygons","textures"};for(size_t n=0;n<6;++n)write(data/(std::string(names[n])+".bin"),*vectors[n]);
 write(data/"sound_program.bin",std::string{});write(data/"sound_samples.bin",std::string{});
 write(data/"assets.json","{\"scope\":\"synthetic identity_only\"}\n");const auto assets=rec::read_bounded(data/"assets.json",1024*1024);M::Options opts;opts.external_video=true;opts.instruction_boundary_hook=true;
 M m(i,opts);auto ee=m.save_eeprom(),backup=m.save_backup();ee[3]=0x17;backup[0]=0x21;backup[1]=0x43;backup[2]=0x65;backup[3]=0x87;m.load_eeprom(ee);m.load_backup(backup);
 rec::Recorder recorder(bundle,rec::identify_startup(i,opts,assets,exe),m);
 for(unsigned n=0;n<4;++n){srally::host_input::Resume row;row.index=n;row.budget=1;row.before=srally::host_input::Witness::of(m.stats());row.inputs.in0=uint8_t(0xf0+n);row.inputs.analog[0]=uint8_t(0x90+n);m.inputs()=row.inputs;
  const auto result=m.run_until_frame(1);row.after=srally::host_input::Witness::of(m.stats());row.reason=result.reason;recorder.append(row,result,m.take_audio());}
 recorder.finish(m,rec::TerminalKind::Quit);
 const auto exe_hash=rt::sha256_file(exe,1024ull*1024*1024).hex,assets_hash=rt::sha256_hex(assets);
 srally::recording_contract::fixture={exe_hash,assets_hash,addresses,{},100};
 check(run(bundle,data,exe)==0,"complete independent synthetic recording failed");
 const auto frozen=rec::preflight_bundle(bundle,data,exe);const auto original=read(bundle/"resumes.txt");write(bundle/"resumes.txt","changed after preflight\n");
 unsigned rows=0;frozen.each([&](const auto&,const auto&,uint64_t,const auto&){++rows;});check(rows==4&&frozen.initial_backup()==backup,"preflight did not preserve actual immutable rows/seeds");
 bool refused=false;try{frozen.verify_stream_files_unchanged();}catch(const std::exception&){refused=true;}check(refused,"preflight disk edit diagnostic missing");write(bundle/"resumes.txt",original);
 const auto calls=native_calls;const std::string bad_hash(64,'0');
 srally::recording_contract::fixture.executable_sha256=bad_hash;check(run(bundle,data,exe)!=0&&native_calls==calls,"unbound host reached native guest");srally::recording_contract::fixture.executable_sha256=exe_hash;
 srally::recording_contract::fixture.assets_sha256=bad_hash;check(run(bundle,data,exe)!=0&&native_calls==calls,"unbound assets reached native guest");srally::recording_contract::fixture.assets_sha256=assets_hash;
 const std::array<uint32_t,4> unknown{0x420,0x428,0x430,0x43c};srally::recording_contract::fixture.instruction_addresses=unknown;check(run(bundle,data,exe)!=0&&native_calls==calls,"unbound inventory reached native guest");srally::recording_contract::fixture.instruction_addresses=addresses;
 srally::recording_contract::fixture.maximum_instructions=3;check(run(bundle,data,exe)!=0&&native_calls==calls,"bounded rejection executed native guest");srally::recording_contract::fixture.maximum_instructions=100;
 const auto manifest=read(bundle/"manifest.txt"),observations=read(bundle/"observations.txt"),seal=read(bundle/"complete.txt");
 auto sound=manifest;replace(sound,"options 1 1 1 1 1 1 1 1 0 1","options 1 1 1 1 1 1 1 1 1 1");write(bundle/"manifest.txt",sound);
 auto transcript=original;replace(transcript,"context "+hash(manifest)+" 0 1","context "+hash(sound)+" 1 1");write(bundle/"resumes.txt",transcript);reseal(bundle);
 check(run(bundle,data,exe)!=0&&native_calls==calls,"sound-on bundle rejection executed native guest");write(bundle/"manifest.txt",manifest);write(bundle/"resumes.txt",original);write(bundle/"complete.txt",seal);
 auto fault=original;const auto last=fault.rfind(" budget\n");check(last!=std::string::npos,"last Budget row missing");fault.replace(last,8," device\n");write(bundle/"resumes.txt",fault);
 auto fault_observation=observations;const auto last_obs=fault_observation.rfind(" budget ");check(last_obs!=std::string::npos,"last observation Budget missing");fault_observation.replace(last_obs,8," device ");write(bundle/"observations.txt",fault_observation);
 auto fault_seal=seal;replace(fault_seal,"terminal quit ","terminal frontier ");write(bundle/"complete.txt",fault_seal);reseal(bundle);
 check(run(bundle,data,exe)!=0&&native_calls==calls,"faulting bundle rejection executed native guest");
 std::printf("Independent complete sound-off bundle: %u checks passed\n",checks);return 0;
}catch(const std::exception &e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
