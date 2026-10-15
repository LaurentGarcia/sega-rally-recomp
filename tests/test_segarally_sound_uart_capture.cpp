// Test-only authentic main-UART capture and native sound recapture. No device
// output or independent CPU claim; all generated instruction bodies unchanged.
#include "runtime/model2a_machine.h"
#include "runtime/model2a_sound.h"
#include "runtime/model2a_uart.h"
#include "runtime/sha256_digest.h"
#include "runtime/snd_gen_support.h"
#include "segarally-sound-reset-inputs.h"
#include "segarally-uart-build-inputs.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace uart_capture_native {using Env=sndgen::Env;}
#define sndgen uart_capture_native
#include SOUND_REFERENCE_NATIVE_SOURCE
#undef sndgen

namespace {
constexpr uint64_t max_ns=2'000'000'000,max_main=20'000'000,max_uart=4096,max_lines=9000;
void require(bool value,const char *message){if(!value)throw rt::Fatal(message);}
std::string digest(rt::Sha256 &hash){const auto result=rt::sha256_digest_hex(hash.finish());hash=rt::Sha256();return result;}
void number(rt::Sha256 &hash,uint64_t value){std::array<uint8_t,8> bytes{};for(unsigned i=0;i<8;++i)bytes[i]=uint8_t(value>>(i*8));hash.update(bytes);}
std::vector<uint8_t> load(const std::filesystem::path &path,size_t size){
 require(std::filesystem::is_regular_file(path)&&std::filesystem::file_size(path)==size,"loaded image size");
 std::ifstream f(path,std::ios::binary);std::vector<uint8_t> result{std::istreambuf_iterator<char>(f),{}};require(result.size()==size,"loaded image read");
 const auto name=path.filename().string();const auto found=std::find_if(uart_inputs::assets.begin(),uart_inputs::assets.end(),[&](const auto &a){return a.file==name;});
 require(found!=uart_inputs::assets.end()&&found->size==size&&rt::sha256_hex(result)==found->sha256,"actual loaded bytes match compiled input binding");return result;
}
struct SoundTap final:snd::AddressSpace {
 std::function<uint64_t()> now;
 snd::Cpu68k *cpu=nullptr;snd::AddressSpace *target=nullptr;
 const snd::Model2ASoundBus *board=nullptr;
 rt::Sha256 transfers;uint64_t data_rows=0,guards=0,midi_reads=0;
 struct Consume{uint64_t ns,cycle;uint8_t byte;int before,after;};
 std::vector<Consume> consumes;
 void attach(snd::Cpu68k &c){
  if(!target){cpu=&c;target=c.bus;board=dynamic_cast<const snd::Model2ASoundBus*>(target);require(board!=nullptr,"actual sound board bus");c.bus=this;}
  require(cpu==&c&&c.bus==this,"sound observer ownership");
 }
 void row(uint32_t a,uint16_t value,uint16_t mask,bool write){
  require(now&&now()<=max_ns,"finite sound event time");
  for(auto v:{uint64_t(a),uint64_t(value&mask),uint64_t(mask),uint64_t(write),now(),cpu->cycles})number(transfers,v);
  ++data_rows;
 }
 uint16_t read16(uint32_t a,uint16_t mask)override{
  const int before=cpu->irq_line;const auto value=target->read16(a,mask);
  if(cpu->timing_active()){
   row(a,value,mask,false);
   if(a==0x100404){++midi_reads;if(before>cpu->irq_line){require(consumes.size()<max_uart,"finite MIDI consume evidence");consumes.push_back({now(),cpu->cycles,uint8_t(value),before,cpu->irq_line});}}
  }else{require(a>=0x600000&&a<0x680000,"live guard ROM range");++guards;}
  return value;
 }
 void write16(uint32_t a,uint16_t value,uint16_t mask)override{
  require(cpu->timing_active(),"modeled write timing active");row(a,value,mask,true);target->write16(a,value,mask);
 }
 std::string architecture()const{
  require(cpu&&board,"sound observation after first real opcode");std::ostringstream out;
  out<<cpu->pc<<' '<<cpu->cycles<<' '<<cpu->sr()<<' '<<cpu->other_sp<<' '<<cpu->irq_line<<' '<<cpu->nmi_pending;
  for(unsigned i=0;i<8;++i)out<<' '<<cpu->d[i]<<' '<<cpu->a[i];
  const auto &s=cpu->cycle_stats;
  out<<' '<<s.data_transfers<<' '<<s.prefetch_transfers<<' '<<s.dummy_reads<<' '<<s.mapped_waits<<' '<<s.iack_transfers<<' '<<s.iack_waits;
  return out.str();
 }
};
SoundTap *active_sound=nullptr;
struct State {
 uint64_t instructions,cycles,irq,samples,drops;uint32_t pc,previous;uint8_t mode,command;uint64_t tx_changes;
};
std::string cut(SoundTap &tap,const State &s,const std::vector<float> &pcm){
 require(s.drops==0,"no sound buffer drops");rt::Sha256 audio;
 for(float value:pcm){require(std::isfinite(value),"finite PCM");const auto bits=std::bit_cast<uint32_t>(value);std::array<uint8_t,4> bytes{};for(unsigned i=0;i<4;++i)bytes[i]=uint8_t(bits>>(i*8));audio.update(bytes);}
 std::ostringstream out;out<<tap.architecture()<<' '<<s.instructions<<' '<<s.cycles<<' '<<s.irq<<' '<<s.pc<<' '<<s.previous<<' '<<s.samples<<' '<<s.drops
  <<' '<<unsigned(s.mode)<<' '<<unsigned(s.command)<<' '<<s.tx_changes<<' '<<tap.data_rows<<' '<<tap.guards<<' '<<tap.midi_reads<<' '<<tap.consumes.size()
  <<' '<<rt::sha256_hex(tap.board->sound_ram())<<' '<<digest(tap.transfers)<<' '<<pcm.size()<<' '<<digest(audio);
 return out.str();
}
struct Writer {
 std::ofstream out;uint64_t lines=0,uart=0,first_data_ns=0,bytes=0;std::vector<std::pair<uint64_t,uint8_t>> written;
 explicit Writer(const std::filesystem::path &path):out(path){require(bool(out),"open exclusive tape");out<<"segarally-uart-native-v1 "<<uart_inputs::assets_report_sha256<<'\n';}
 void line(const std::string &s){bytes+=s.size()+1;require(++lines<=max_lines&&s.size()<4096&&bytes<=16*1024*1024-256,"bounded tape rows/bytes");out<<s<<'\n';require(bool(out),"stream tape write");}
};
struct MainTap final:rt::Bus {
 rt::Model2AMachine &machine;rt::Cpu &cpu;rt::Bus *target;SoundTap &sound;Writer &writer;
 MainTap(rt::Model2AMachine &m,SoundTap &s,Writer &w):machine(m),cpu(const_cast<rt::Cpu&>(m.cpu())),target(cpu.bus),sound(s),writer(w){cpu.bus=this;}
 ~MainTap(){cpu.bus=target;}
 void bound(){require(cpu.reference_cycles<=max_ns/40,"2s main board cap");}
 bool uart(uint32_t a,unsigned width){return a>=0x1c80000&&uint64_t(a)+width<=0x1c80004;}
 void observe(bool write,uint32_t a,unsigned width,uint32_t value){
  if(!uart(a,width))return;
  require(sound.cpu!=nullptr,"actual UART operation follows sound observer attachment");
  require(++writer.uart<=max_uart,"4096 UART access bound");const auto s=machine.stats();
  require(s.board_time_ns==cpu.reference_cycles*40,"UART occurs after complete advance barrier");
  const uint64_t bits=(uint64_t(1)<<(width*8))-1;const auto shift=(a&3)*8;
  std::ostringstream row;row<<"A "<<writer.uart<<' '<<s.board_time_ns<<' '<<s.instructions<<' '<<cpu.reference_cycles<<' '<<cpu.m_IP<<' '<<write<<' '<<a<<' '<<width<<' '<<value<<' '<<(a&~3u)<<' '<<((uint64_t(value)&bits)<<shift)<<' '<<(bits<<shift);writer.line(row.str());
  if(write&&(a&3)==0){writer.written.emplace_back(s.board_time_ns,uint8_t(value));if(!writer.first_data_ns)writer.first_data_ns=s.board_time_ns;}
  checkpoint();
 }
 void checkpoint(){
  const auto s=machine.stats();require(s.instructions<=max_main&&s.board_time_ns<=max_ns,"finite capture cut");
  if(!sound.cpu)return; // Constructor reset is separately authenticated, not bus-intercepted.
  std::ostringstream row;row<<"C "<<s.board_time_ns<<' '<<cut(sound,{s.sound_instructions,s.sound_cycles,s.sound_irqs,s.sound_samples,s.sound_dropped_samples,s.sound_pc,s.sound_previous_dispatch,s.uart_mode,s.uart_command,s.tx_pin_changes},machine.take_audio());writer.line(row.str());
 }
 uint32_t fetch(uint32_t a)override{bound();return target->fetch(a);}
 uint8_t read_byte(uint32_t a)override{bound();auto v=target->read_byte(a);observe(false,a,1,v);return v;}
 uint16_t read_word(uint32_t a)override{bound();auto v=target->read_word(a);observe(false,a,2,v);return v;}
 uint32_t read_dword(uint32_t a)override{bound();auto v=target->read_dword(a);observe(false,a,4,v);return v;}
 void write_byte(uint32_t a,uint8_t v)override{bound();target->write_byte(a,v);observe(true,a,1,v);}
 void write_word(uint32_t a,uint16_t v)override{bound();target->write_word(a,v);observe(true,a,2,v);}
 void write_dword(uint32_t a,uint32_t v)override{bound();target->write_dword(a,v);observe(true,a,4,v);}
 uint16_t flags(uint32_t a)override{bound();return target->flags(a);}
 rt::SceneryState *scenery()override{return target->scenery();}
};
void genuine_consumption(const SoundTap &sound,const std::vector<std::pair<uint64_t,uint8_t>> &writes){
 for(const auto &e:sound.consumes)for(const auto &[ns,byte]:writes)
  if(e.ns>ns&&e.byte==byte&&e.before>e.after){
   std::printf("MIDI evidence actual_main_write_ns=%llu byte=%02x sound_read_ns=%llu boundary_cycle=%llu IRQ=%d_to_%d\n",(unsigned long long)ns,unsigned(byte),(unsigned long long)e.ns,(unsigned long long)e.cycle,e.before,e.after);return;
  }
 throw rt::Fatal("actual UART byte received then consumed through MIDI read/IRQ clear was not observed");
}
rt::Model2AMachine::Images machine_images(const std::filesystem::path &data){
 rt::Model2AMachine::Images i;i.program=load(data/"program.bin",0x200000);i.main_data=load(data/"main_data.bin",0x2400000);
 i.tgp_tables=load(data/"tgp_tables.bin",0x40000);i.copro_data=load(data/"copro_data.bin",0x800000);
 i.polygons=load(data/"polygons.bin",0x2000000);i.textures=load(data/"textures.bin",0x1000000);
 i.sound_program=load(data/"sound_program.bin",0x80000);i.sound_samples=load(data/"sound_samples.bin",0x800000);return i;
}
void capture(const std::filesystem::path &data,const std::filesystem::path &out){
 SoundTap sound;active_sound=&sound;rt::Model2AMachine::Options options;options.sound=true;options.instruction_boundary_hook=true;options.external_video=true;
 rt::Model2AMachine machine(machine_images(data),options);sound.now=[&]{return machine.stats().board_time_ns;};
 const auto &pins=machine.inputs();require(pins.in0==0xff&&pins.in1==0x8f&&pins.in2==0&&pins.switches==0xff&&pins.analog==std::array<uint8_t,8>{0x80,0,0,0xff,0xff,0xff,0xff,0xff},"actual neutral12 cabinet pins");
 machine.load_eeprom(std::vector<uint8_t>(128,0xff));machine.load_backup(std::vector<uint8_t>(16384,0xff));
 const auto save=[&](const char *name,const std::vector<uint8_t> &bytes){std::ofstream f(out/name,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));f.close();require(!f.fail(),"actual seed/save snapshot written");};
 require(machine.save_eeprom()==std::vector<uint8_t>(128,0xff)&&machine.save_backup()==std::vector<uint8_t>(16384,0xff),"actual freshFF startup seeds");
 save("initial.eeprom.bin",machine.save_eeprom());save("initial.backup.bin",machine.save_backup());
 Writer writer(out/"uart.txt");MainTap main(machine,sound,writer);
 for(;;){
  const auto before=machine.stats();require(before.instructions<max_main&&before.board_time_ns<max_ns,"real traffic discovered within cap");
  const auto result=machine.run_for(std::min<uint64_t>(writer.first_data_ns?1000:100000,max_main-before.instructions));
  require(result.reason==rt::Model2AMachine::Reason::Budget,"capture native main stopped before bound");main.checkpoint();
  const auto s=machine.stats();
  std::printf("progress native main_instructions=%llu board_ns=%llu UART_accesses=%llu sound_instructions=%llu\n",(unsigned long long)s.instructions,(unsigned long long)s.board_time_ns,(unsigned long long)writer.uart,(unsigned long long)s.sound_instructions);std::fflush(stdout);
  if(writer.first_data_ns&&s.board_time_ns>=writer.first_data_ns+2'000'000){
   require(s.tx_pin_changes>0,"actual main UART physical transmission");genuine_consumption(sound,writer.written);
   writer.line("END "+std::to_string(writer.lines)+" "+std::to_string(writer.uart)+" "+std::to_string(s.board_time_ns));writer.out.flush();require(bool(writer.out),"complete capture flush");writer.out.close();require(!writer.out.fail(),"complete capture close");break;
  }
 }
 save("final.eeprom.bin",machine.save_eeprom());save("final.backup.bin",machine.save_backup());
 std::puts("PASS authentic UART capture; native recapture still required");
}
struct Link {
 rt::Model2AClock &clock;rt::Model2UART chip;rt::Model2AUART board{chip};bool level=false,pin=true;uint64_t changes=0;
 rt::Model2AClock::Token next;std::function<void(bool)> receiver;
 explicit Link(rt::Model2AClock &c):clock(c){
  rt::Model2UART::Callbacks cb;cb.txd=[this](bool v){if(v!=pin)++changes;pin=v;if(receiver)receiver(v);};chip.set_callbacks(cb);board.reset();schedule();
 }
 ~Link(){clock.cancel(next);}
 void schedule(){next=clock.schedule_after(1000,[this](uint64_t){level=!level;board.clock_edge(level);schedule();});}
 void connect(snd::Model2ASound &sound){receiver=[&sound](bool v){sound.midi_in(v);};sound.on_midi_tx=[this](bool v){chip.write_rxd(v);};sound.midi_in(chip.txd_level());chip.write_rxd(true);}
};
void recapture(const std::filesystem::path &data,const std::filesystem::path &tape,const std::string &expected_hash){
 // Preflight all bounded immutable bytes before any guest opcode. Execution
 // uses these buffers; no reopening transcript rows after validation.
 require(std::filesystem::file_size(tape)<=16*1024*1024,"bounded transcript bytes");std::ifstream file(tape,std::ios::binary);std::string bytes;std::array<char,65536> buffer{};
 while(file){file.read(buffer.data(),buffer.size());const auto count=file.gcount();require(bytes.size()+size_t(count)<=16*1024*1024,"bounded actual transcript read");bytes.append(buffer.data(),size_t(count));}
 require(file.eof()&&!file.bad()&&rt::sha256_hex({reinterpret_cast<const uint8_t*>(bytes.data()),bytes.size()})==expected_hash,"actual immutable transcript bytes match captured identity");
 std::istringstream input(bytes);std::vector<std::string> rows;std::string line;
 while(std::getline(input,line)){require(line.size()<4096&&rows.size()<max_lines+2,"bounded preflight rows");rows.push_back(line);}
 require(input.eof()&&rows.size()>2&&rows.front()=="segarally-uart-native-v1 "+std::string(uart_inputs::assets_report_sha256)&&rows.back().starts_with("END "),"strict complete native transcript");
 uint64_t seq=0,last=0,expected_lines=0,expected_uart=0,terminal=0;
 for(size_t i=1;i+1<rows.size();++i){std::istringstream row(rows[i]);char kind=0;uint64_t ns=0;require(bool(row>>kind),"nonempty transcript row");
  if(kind=='A'){uint64_t index,n,c,pc,write,a,width,value,aligned,lanes,mask;require(bool(row>>index>>ns>>n>>c>>pc>>write>>a>>width>>value>>aligned>>lanes>>mask),"complete UART columns");row>>std::ws;
   require(row.eof()&&index==++seq&&seq<=max_uart&&n<=max_main&&c<=max_ns/40&&ns==c*40&&write<=1&&(width==1||width==2||width==4)&&a>=0x1c80000&&a<=0x1c80003&&a+width<=0x1c80004,"strict actual UART row");
   const auto bits=(uint64_t(1)<<(width*8))-1;require(value<=bits&&aligned==(a&~3u)&&lanes==(value<<((a&3)*8))&&mask==(bits<<((a&3)*8)),"UART width/lane provenance");
   require(i+1<rows.size()&&rows[i+1].starts_with("C "+std::to_string(ns)+" "),"each UART access followed by same-time state cut");
  }else if(kind=='C'){
   require(bool(row>>ns),"state cut time");std::array<uint64_t,42> values{};for(auto &v:values)require(bool(row>>v),"complete architectural cut columns");
   std::string ram,data,audio;uint64_t pcm;require(bool(row>>ram>>data>>pcm>>audio),"complete cut digest columns");row>>std::ws;
   const auto hex=[](const std::string &s){return s.size()==64&&s.find_first_not_of("0123456789abcdef")==std::string::npos;};
   require(row.eof()&&values[0]<=0xffffff&&values[2]<=0xffff&&values[4]<=7&&values[5]<=1&&values[34]==0&&values[35]<=255&&values[36]<=255&&pcm<=88200&&!(pcm&1)&&hex(ram)&&hex(data)&&hex(audio),"strict bounded state cut");
  }
  else throw rt::Fatal("unsupported native transcript row");
  require(!row.bad()&&ns>=last&&ns<=max_ns,"monotonic finite transcript time");last=ns;
 }
 {std::istringstream footer(rows.back());std::string tag;require(bool(footer>>tag>>expected_lines>>expected_uart>>terminal),"complete terminal columns");footer>>std::ws;require(footer.eof()&&expected_lines==rows.size()-2&&expected_uart==seq&&terminal==last,"strict terminal ledger");}
 rt::Model2AClock clock;Link link(clock);SoundTap sound;active_sound=&sound;sound.now=[&]{return clock.now_ns();};snd::Model2Scsp::ReferenceRandom random;
 snd::Model2ASound native(clock,{load(data/"sound_program.bin",0x80000),load(data/"sound_samples.bin",0x800000)},[&]{return random();},44100);link.connect(native);
 std::vector<std::pair<uint64_t,uint8_t>> writes;uint64_t first_data=0;
 for(size_t i=1;i+1<rows.size();++i){std::istringstream row(rows[i]);char kind;uint64_t ns;row>>kind;
  if(kind=='A'){uint64_t index,n,c,pc,write,a,width,value,aligned,lanes,mask;row>>index>>ns>>n>>c>>pc>>write>>a>>width>>value>>aligned>>lanes>>mask;
   clock.advance(ns); // External register access AFTER every <=ns callback.
   if(write){link.board.write(uint32_t(aligned),uint32_t(lanes),uint32_t(mask));if((a&3)==0){writes.emplace_back(ns,uint8_t(value));if(!first_data)first_data=ns;}}
   else{uint32_t actual=0;link.board.read(uint32_t(aligned),uint32_t(mask),actual);require((actual>>((a&3)*8))==value,"native recaptured UART read result");}
  }else{row>>ns;std::string expected;std::getline(row,expected);require(expected.starts_with(' '),"state separator");expected.erase(0,1);clock.advance(ns);
   const auto s=native.stats();const auto samples=native.take_samples();std::vector<float> pcm;pcm.reserve(samples.size()*2);for(auto sample:samples){pcm.push_back(sample.left);pcm.push_back(sample.right);}
   const auto actual=cut(sound,{s.instructions,s.cycles,s.irq_entries,s.sample_frames,s.dropped_samples,s.pc,s.previous_dispatch,link.chip.mode(),link.chip.command(),link.changes},pcm);
   if(actual!=expected){std::fprintf(stderr,"first native recapture mismatch line=%zu board_ns=%llu\nexpected: %s\nactual:   %s\n",i,(unsigned long long)ns,expected.c_str(),actual.c_str());throw rt::Fatal("native recaptured architecture/RAM/IRQ/data/PCM differs");}
  }
 }
 require(first_data&&terminal>=first_data+2000000&&link.chip.tx_empty(),"physical UART frame complete within recorded tail");genuine_consumption(sound,writes);
 std::printf("PASS native UART recapture accesses=%llu board_ns=%llu sound_instructions=%llu MIDI_consumes=%zu\n",(unsigned long long)seq,(unsigned long long)terminal,(unsigned long long)native.stats().instructions,sound.consumes.size());
}
}
namespace sndgen {
bool has_code(uint32_t pc){return uart_capture_native::has_code(pc);}
uint64_t native_instructions(){return uart_capture_native::native_instructions();}
void run(Env &env){require(active_sound!=nullptr,"sound observer bound");active_sound->attach(env.c);uart_capture_native::run(env);}
}
int main(int argc,char **argv){try{
 require(argc==4||argc==5,"mode, exact staged assets and exclusive output/tape required");
 const std::string mode=argv[1];
 if(mode=="capture"){require(argc==4,"capture argument count");const std::filesystem::path out=argv[3];require(std::filesystem::create_directory(out),"exclusive capture directory");capture(argv[2],out);}
 else{require(mode=="recapture"&&argc==5,"recapture requires captured tape SHA");recapture(argv[2],argv[3],argv[4]);}
 return 0;
 }catch(const std::exception &e){std::fprintf(stderr,"UART native gate FAIL: %s\n",e.what());return 1;}}
