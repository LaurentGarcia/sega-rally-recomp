// Synthetic main/sound programs, real Machine devices, no game ROM or GPU.
#include "tools/srally_boot/recording_bundle.h"
#include "runtime/sha256_digest.h"
#include "runtime/gen_support.h"
#include "runtime/snd_gen_support.h"
#include "runtime/model2a_clock.h"
#include "runtime/tgp.h"
#include <chrono>
#include <fstream>
#include <cstdio>

namespace {
namespace rec=srally::recording;namespace hi=srally::host_input;
using Machine=rt::Model2AMachine;unsigned checks=0;uint64_t generated_calls=0;
constexpr Machine::Options legacy_flags{true,true,false,true,true,true,true,true,true,true};
static_assert(legacy_flags.sound&&legacy_flags.instruction_boundary_hook&&legacy_flags.sound_buffer_frames==44100,
              "new options must preserve the original ten-boolean aggregate contract");
constexpr uint32_t FRAME_END=(rt::Model2AScreen::FRAME_NS+rt::Model2AScreen::VBLANK_NS)/40;
void require(bool b,const char*s){++checks;if(!b)throw std::runtime_error(s);}
template<class F>void rejects(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}require(bad,"invalid recording accepted");}
uint32_t code_word(uint32_t pc){return 0x60000000u|pc;}
Machine::Images images(bool sound,unsigned fault=0){
    Machine::Images i;i.program.resize(0x600);auto put=[&](uint32_t a,uint32_t w){for(unsigned b=0;b<4;++b)i.program[a+b]=uint8_t(w>>(8*b));};
    put(12,fault==2?0x580:fault?0x540:0x420);for(uint32_t pc:{0x420u,0x424u,0x428u,0x540u,0x580u})put(pc,code_word(pc));
    i.tgp_tables.resize(0x40000);i.copro_data.resize(0x800000);i.polygons.resize(4);i.textures.resize(2);
    if(sound){i.sound_program.resize(0x80000);i.sound_samples.resize(0x800000);i.sound_program[2]=0x50;i.sound_program[5]=0x60;i.sound_program[6]=1;}return i;
}
Machine::Options options(bool sound){Machine::Options o;o.comm=false;o.external_video=true;o.sound=sound;o.instruction_boundary_hook=sound;return o;}
void seeds(Machine&m){auto e=m.save_eeprom(),b=m.save_backup();e[7]=0x23;b[10]=0x42;m.load_eeprom(e);m.load_backup(b);}
void write(const std::filesystem::path&p,std::span<const uint8_t>b){std::ofstream o(p,std::ios::binary|std::ios::trunc);o.write(reinterpret_cast<const char*>(b.data()),std::streamsize(b.size()));if(!o)throw std::runtime_error("fixture write failed");}
void write(const std::filesystem::path&p,const std::string&s){write(p,{reinterpret_cast<const uint8_t*>(s.data()),s.size()});}
struct Fixture{
    std::filesystem::path root,data,bundle;std::filesystem::path exe;
    Fixture(std::filesystem::path e):exe(std::move(e)){
        root=std::filesystem::temp_directory_path()/("segarally-bundle-unit-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        require(std::filesystem::create_directory(root),"unique fixture creation failed");data=root/"data";bundle=root/"capture";std::filesystem::create_directory(data);
    }
    ~Fixture(){std::error_code error;std::filesystem::remove_all(root,error);}
    void stage(const Machine::Images&i){
        const char*names[]={"program","main_data","tgp_tables","copro_data","polygons","textures","sound_program","sound_samples"};
        const std::vector<uint8_t>*v[]={&i.program,&i.main_data,&i.tgp_tables,&i.copro_data,&i.polygons,&i.textures,&i.sound_program,&i.sound_samples};
        for(size_t n=0;n<8;++n)write(data/(std::string(names[n])+".bin"),*v[n]);write(data/"assets.json","{\"scope\":\"synthetic identity_only\"}\n");
    }
    void unchanged_cpu_reject(const std::filesystem::path&p){
        auto original=rec::read_bounded(p,256*1024*1024),bad=original;require(!bad.empty(),"corruption fixture empty");bad[0]^=1;write(p,bad);
        const auto before=generated_calls;rejects([&]{rec::replay_bundle(bundle,data,exe);});require(generated_calls==before,"invalid bundle executed generated code before preflight");write(p,original);
    }
};
void capture(Fixture&f,bool sound,unsigned fault=0){
    auto i=images(sound,fault);f.stage(i);const auto o=options(sound);const auto assets=rec::read_bounded(f.data/"assets.json",1024*1024);
    const auto identity=rec::identify_startup(i,o,assets,f.exe);Machine m(i,o);seeds(m);rec::Recorder writer(f.bundle,identity,m);
    rejects([&]{rec::Recorder overwrite(f.bundle,identity,m);});
    uint64_t nonzero=0;
    for(uint64_t n=0;n<(fault?1u:3u);++n){
        hi::Resume r;r.index=n;r.budget=1;r.before=hi::Witness::of(m.stats());r.inputs.analog[0]=uint8_t(0x70+n);r.inputs.in0=n==2?0xfe:0xff;m.inputs()=r.inputs;
        const auto result=m.run_until_frame(r.budget);const auto pcm=m.take_audio();for(float sample:pcm)nonzero+=sample!=0;
        r.after=hi::Witness::of(m.stats());r.reason=result.reason;writer.append(r,result,pcm);
        if(!fault&&n==1)require(r.before.instructions==r.after.instructions&&r.reason==Machine::Reason::Frame&&r.after.frames==1,"same-count frame boundary not captured");
    }
    if(sound&&!fault)require(nonzero>0,"coupled SCSP test PCM was entirely silent");
    if(!fault)require(m.save_backup()[0]==0x44,"fixture did not mutate final backup");
    writer.finish(m,fault==2?rec::TerminalKind::Fault:fault?rec::TerminalKind::Frontier:rec::TerminalKind::Quit);
    const auto restored=rec::preflight_bundle(f.bundle,f.data,f.exe).options();
    require(restored.instruction_boundary_hook==o.instruction_boundary_hook&&restored.sound_buffer_frames==Machine::kAudioRate,
        "v1 option decoding changed the boundary hook or default audio capacity");
    const auto summary=rec::replay_bundle(f.bundle,f.data,f.exe);
    require(summary.records==(fault?1u:3u)&&summary.frontier==bool(fault)&&summary.terminal==hi::Witness::of(m.stats())&&
        summary.kind==(fault==2?rec::TerminalKind::Fault:fault?rec::TerminalKind::Frontier:rec::TerminalKind::Quit),"same-Machine recording replay summary differs");
}
void unsupported_capacity(const std::filesystem::path&exe){
    for(bool sound:{false,true}){
        const auto i=images(sound);auto o=options(sound);
        const std::string assets="{\"scope\":\"synthetic identity_only\"}\n";
        for(size_t capacity:{size_t(0),size_t(1),size_t(1024),size_t(Machine::kAudioRate+1)}){
            o.sound_buffer_frames=capacity;
            rejects([&]{rec::identify_startup(i,o,{reinterpret_cast<const uint8_t*>(assets.data()),assets.size()},exe);});
        }
    }
}
void external_provider(const std::filesystem::path&exe){
    struct SoundProvider final:snd::SoundImageSource{
        mutable unsigned accesses=0;
        uint32_t size(snd::SoundImage)const override{++accesses;return 0;}
        uint16_t read16_be(snd::SoundImage,uint32_t)override{++accesses;return 0;}
    }sound_provider;
#ifdef M2_PAGED_ROM
    struct RomProvider final:rt::RomSource{
        mutable unsigned accesses=0;
        uint32_t size(rt::RomRegion)const override{++accesses;return 0;}
        const uint8_t *page(rt::RomRegion,uint32_t)override{++accesses;return nullptr;}
    }rom_provider;
#endif
    const std::string report="{\"scope\":\"synthetic identity_only\"}\n";
    const std::span<const uint8_t> assets{reinterpret_cast<const uint8_t*>(report.data()),report.size()};
    for(bool sound:{false,true}){
        auto i=images(sound);i.sound_rom=&sound_provider;
        rejects([&]{rec::identify_startup(i,options(sound),assets,exe);});
        require(sound_provider.accesses==0,"v1 capture must reject unrepresented sound providers without reading them");
#ifdef M2_PAGED_ROM
        i.sound_rom=nullptr;i.rom=&rom_provider;
        rejects([&]{rec::identify_startup(i,options(sound),assets,exe);});
        require(rom_provider.accesses==0,"v1 capture must reject unrepresented ROM providers without reading them");
#endif
    }
}
void integrity(Fixture&f,bool sound){
    for(const char*name:{"complete.txt","manifest.txt","resumes.txt","observations.txt","eeprom.initial.bin","backup.initial.bin","eeprom.final.bin","backup.final.bin","assets.json"})f.unchanged_cpu_reject(f.bundle/name);
    for(const char*name:{"assets.json","program.bin","main_data.bin","tgp_tables.bin","copro_data.bin","polygons.bin","textures.bin"}){
        if(std::filesystem::file_size(f.data/name))f.unchanged_cpu_reject(f.data/name);
    }
    if(sound)f.unchanged_cpu_reject(f.data/"sound_samples.bin");
    const auto other=f.root/"other-executable";write(other,"different executable");const auto before=generated_calls;
    rejects([&]{rec::replay_bundle(f.bundle,f.data,other);});require(generated_calls==before,"executable mismatch reached generated CPU");
    const auto extra=f.bundle/"unexpected";write(extra,"extra");rejects([&]{rec::replay_bundle(f.bundle,f.data,f.exe);});std::filesystem::remove(extra);
    auto marker=rec::read_bounded(f.bundle/"complete.txt",8192);std::filesystem::remove(f.bundle/"complete.txt");
    rejects([&]{rec::replay_bundle(f.bundle,f.data,f.exe);});require(generated_calls==before,"missing completion reached generated CPU");write(f.bundle/"complete.txt",marker);
    auto seal=std::string(marker.begin(),marker.end());const auto at=seal.find("terminal quit ");require(at!=std::string::npos,"terminal fixture missing");seal.replace(at,14,"terminal fault ");write(f.bundle/"complete.txt",seal);
    rejects([&]{rec::replay_bundle(f.bundle,f.data,f.exe);});require(generated_calls==before,"false fault terminal reached generated CPU");write(f.bundle/"complete.txt",marker);
    seal=std::string(marker.begin(),marker.end());seal.replace(at,14,"terminal frame_limit ");write(f.bundle/"complete.txt",seal);
    rejects([&]{rec::replay_bundle(f.bundle,f.data,f.exe);});require(generated_calls==before,"budget return accepted as frame limit");write(f.bundle/"complete.txt",marker);
}
void reseal_observations(Fixture&f){
    const auto blob=rec::read_bounded(f.bundle/"observations.txt",64*1024*1024);auto seal=rec::read_bounded(f.bundle/"complete.txt",8192);
    std::string text(seal.begin(),seal.end());const auto at=text.find("observations "),stop=text.find('\n',at);
    require(at!=std::string::npos&&stop!=std::string::npos,"observation seal fixture missing");
    text.replace(at,stop-at,"observations "+std::to_string(blob.size())+" "+rt::sha256_hex(blob));write(f.bundle/"complete.txt",text);
}
void observation_truth(Fixture&f,bool frontier){
    const auto original=rec::read_bounded(f.bundle/"observations.txt",64*1024*1024),seal=rec::read_bounded(f.bundle/"complete.txt",8192);
    std::string modified(original.begin(),original.end());
    if(frontier){const auto at=modified.find(" 2 1 ");require(at!=std::string::npos,"fault width fixture missing");modified.replace(at,5," 4 1 ");}
    else{
        const auto row=modified.find("\nresume 1 ")+1;require(row>0,"PCM row missing");std::istringstream fields(modified.substr(row));
        std::string tag,index,count,hash;fields>>tag>>index>>count>>hash;const auto at=modified.find(hash,row);require(hash.size()==64&&at!=std::string::npos,"PCM hash fixture missing");modified[at]=hash[0]=='a'?'b':'a';
    }
    write(f.bundle/"observations.txt",modified);reseal_observations(f);const auto before=generated_calls;
    rejects([&]{rec::replay_bundle(f.bundle,f.data,f.exe);});require(generated_calls>before,"replay trusted resealed PCM/fault evidence without executing comparison");
    write(f.bundle/"observations.txt",original);write(f.bundle/"complete.txt",seal);
}
void incomplete(const std::filesystem::path&exe){
    Fixture f(exe);auto i=images(false);f.stage(i);Machine m(i,options(false));seeds(m);
    {auto identity=rec::identify_startup(i,options(false),rec::read_bounded(f.data/"assets.json",1024*1024),exe);rec::Recorder writer(f.bundle,identity,m);}
    require(!std::filesystem::exists(f.bundle/"complete.txt"),"destructor completed an abandoned recording");
    const auto before=generated_calls;rejects([&]{rec::replay_bundle(f.bundle,f.data,exe);});require(generated_calls==before,"abandoned bundle reached generated CPU");
    Fixture zero(exe);zero.stage(i);Machine idle(i,options(false));seeds(idle);
    auto identity=rec::identify_startup(i,options(false),rec::read_bounded(zero.data/"assets.json",1024*1024),exe);
    {rec::Recorder writer(zero.bundle,identity,idle);writer.finish(idle,rec::TerminalKind::Quit);}
    require(rec::replay_bundle(zero.bundle,zero.data,exe).records==0,"valid zero-row quit did not replay");
    require(generated_calls==before,"zero-row quit executed generated CPU");
    const auto footer=rec::read_bounded(zero.bundle/"complete.txt",8192);
    for(const char *kind:{"frame_limit","instruction_limit"}) {
        std::string forged(footer.begin(),footer.end());const auto at=forged.find("terminal quit ");
        require(at!=std::string::npos,"zero-row terminal fixture missing");forged.replace(at,14,std::string("terminal ")+kind+" ");write(zero.bundle/"complete.txt",forged);
        rejects([&]{rec::replay_bundle(zero.bundle,zero.data,exe);});require(generated_calls==before,"zero rows accepted as a limit terminal");
    }
}
void invalid_finish(const std::filesystem::path&exe,bool fault){
    Fixture f(exe);auto i=images(false,fault?1:0);f.stage(i);Machine m(i,options(false));seeds(m);
    auto identity=rec::identify_startup(i,options(false),rec::read_bounded(f.data/"assets.json",1024*1024),exe);rec::Recorder writer(f.bundle,identity,m);
    hi::Resume r;r.budget=1;r.before=hi::Witness::of(m.stats());const auto result=m.run_until_frame(1);r.after=hi::Witness::of(m.stats());r.reason=result.reason;writer.append(r,result,{});
    if(fault){r.index=1;r.before=r.after;rejects([&]{writer.append(r,result,{});});rejects([&]{writer.finish(m,rec::TerminalKind::Frontier);});}
    else{write(f.bundle/"eeprom.final.bin","already exists");rejects([&]{writer.finish(m,rec::TerminalKind::InstructionLimit);});}
    require(!std::filesystem::exists(f.bundle/"complete.txt"),"failed append/finish left a complete recording");
}
} // namespace
namespace gen{
bool instruction_boundary_hook_compatible(){return true;}bool has_code(uint32_t pc){return pc==0x420||pc==0x424||pc==0x428||pc==0x540||pc==0x580;}uint64_t native_instructions(){return 5;}
void run(Env&e){++generated_calls;while(!e.ls.boundary_yielded()&&has_code(e.c.m_IP)&&!e.ls.boundary()){
    const auto pc=e.c.m_IP;if(e.c.bus->read_dword(pc)!=code_word(pc))throw rt::Fatal("synthetic live guard mismatch");e.c.consume_reference_cycles(pc==0x420?FRAME_END:25000);
    if(pc==0x540)e.c.bus->write_word(0xdead0000,9);
    if(pc==0x580)throw rt::Fatal("synthetic runtime fault with spaces\nand newline");
    if(pc==0x424){e.c.m_r[16]=(e.c.bus->read_dword(0x01c0001c)>>16)&255;e.c.m_r[17]=(e.c.bus->read_dword(0x01c00000)>>16)&255;e.c.bus->write_dword(0x01d00000,0x11223344);}
    e.c.m_IP=pc+4;++e.ls.count;
}}
}
namespace sndgen{
bool has_code(uint32_t pc){return pc==0x600100||pc==0x600102;}uint64_t native_instructions(){return 2;}
void run(Env&e){if(e.s.boundary())return;auto&c=e.c;c.begin_instruction();if(c.pc==0x600100){
    for(unsigned n=0;n<8;++n)c.w16(0x100+n*2,0x1000);c.w16(0x100400,15);c.w16(0x100002,0x100);c.w16(0x100004,0);c.w16(0x100006,4);
    c.w16(0x10000c,0x100);c.w16(0x100016,0xff00);c.w16(0x100000,0x1820);c.pc=0x600102;
}c.account_prefetch(c.pc+2);c.finish_instruction(40);++e.s.count;}
}
namespace rt::tgpgen{extern const uint32_t program_crc32=0,program_words=0;void run(Tgp&,uint64_t){throw TgpFatal("synthetic capture must not execute TGP");}}
int main(int argc,char**argv){try{
    require(argc>0,"missing executable identity");const auto exe=std::filesystem::canonical(argv[0]);
    unsupported_capacity(exe);
    external_provider(exe);
    for(bool sound:{false,true}){Fixture f(exe);capture(f,sound);integrity(f,sound);observation_truth(f,false);}
    {Fixture f(exe);capture(f,true,1);observation_truth(f,true);}{Fixture f(exe);capture(f,true,2);}incomplete(exe);invalid_finish(exe,false);invalid_finish(exe,true);
    std::printf("Recording bundle same-Machine/PCM/seeds/preflight/fault: %u checks passed\n",checks);return 0;
}catch(const std::exception&e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
