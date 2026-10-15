// Test-only persistent MAME CPU on the authenticated real UART transcript.
// Frozen native cuts stay byte-exact. Devices and instruction-start MMIO are
// shared models; reference bus-phase stamps are retained separately.
#include "model2a_sound_reference.h"
#define main frozen_native_uart_main
#include "test_segarally_sound_uart_capture.cpp"
#undef main

namespace {
struct BusRow {uint32_t address;uint16_t value,mask;bool write;uint64_t ns,phase;};
struct EventTap final:snd::AddressSpace {
 snd::AddressSpace *target;snd::Cpu68k &cpu;rt::Model2AClock &clock;std::vector<BusRow> rows;
 EventTap(snd::AddressSpace *t,snd::Cpu68k &c,rt::Model2AClock &k):target(t),cpu(c),clock(k){}
 void record(uint32_t a,uint16_t v,uint16_t mask,bool write){require(rows.size()<128,"native per-event transfer bound");rows.push_back({a,uint16_t(v&mask),mask,write,clock.now_ns(),cpu.cycles});}
 uint16_t read16(uint32_t a,uint16_t mask)override{const auto v=target->read16(a,mask);if(cpu.timing_active())record(a,v,mask,false);return v;}
 void write16(uint32_t a,uint16_t v,uint16_t mask)override{require(cpu.timing_active(),"native modeled write clock");record(a,v,mask,true);target->write16(a,v,mask);}
};
struct Record {char kind=0;uint64_t ns=0,seq=0;bool write=false;uint32_t aligned=0,lanes=0,mask=0,address=0,width=0,value=0;std::string state;};
std::vector<Record> transcript(const std::filesystem::path &path,const std::string &hash){
 std::ifstream file(path,std::ios::binary);require(bool(file),"open exact authentic tape");std::string bytes;std::array<char,65536> buffer{};
 while(file){file.read(buffer.data(),buffer.size());const auto count=file.gcount();require(bytes.size()+size_t(count)<=16*1024*1024,"actual tape read bound");bytes.append(buffer.data(),size_t(count));}
 require(file.eof()&&!file.bad()&&rt::sha256_hex({reinterpret_cast<const uint8_t*>(bytes.data()),bytes.size()})==hash,"actual immutable tape bytes authenticated");
 std::istringstream input(bytes);std::string line;require(bool(std::getline(input,line))&&line=="segarally-uart-native-v1 "+std::string(uart_inputs::assets_report_sha256),"exact native tape header");
 std::vector<Record> result;uint64_t last=0,sequence=0;bool complete=false;
 while(std::getline(input,line)){
  require(!complete&&line.size()<4096&&result.size()<9000,"bounded rows before completion");std::istringstream row(line);std::string tag;require(bool(row>>tag),"nonempty tape row");
  if(tag=="END"){
   uint64_t lines,uart,ns;require(bool(row>>lines>>uart>>ns),"complete footer");row>>std::ws;
   require(row.eof()&&lines==result.size()&&uart==sequence&&uart==7&&ns==last&&ns==1444941000,"exact authentic tape terminal");complete=true;continue;
  }
  Record record;record.kind=tag.size()==1?tag[0]:0;
  if(record.kind=='A'){
   uint64_t n,c,pc,write,a,width,value,aligned,lanes,mask;
   require(bool(row>>record.seq>>record.ns>>n>>c>>pc>>write>>a>>width>>value>>aligned>>lanes>>mask),"all UART columns");row>>std::ws;
   require(row.eof()&&record.seq==++sequence&&sequence<=7&&n<=20000000&&c<=max_ns/40&&record.ns==c*40&&write<=1&&(width==1||width==2||width==4)&&a>=0x1c80000&&a<=0x1c80003&&a+width<=0x1c80004,"authentic UART range/order");
   const auto bits=(uint64_t(1)<<(width*8))-1;
   require(value<=bits&&aligned==(a&~3u)&&lanes==(value<<((a&3)*8))&&mask==(bits<<((a&3)*8)),"unaltered original UART lanes");
   record.write=write;record.address=uint32_t(a);record.width=uint32_t(width);record.value=uint32_t(value);record.aligned=uint32_t(aligned);record.lanes=uint32_t(lanes);record.mask=uint32_t(mask);
  }else{
   require(record.kind=='C'&&bool(row>>record.ns),"state cut row");std::getline(row,record.state);require(record.state.starts_with(' '),"state cut separator");record.state.erase(0,1);
   std::istringstream state(record.state);std::array<uint64_t,42> values{};for(auto &v:values)require(bool(state>>v),"full cut architecture");
   std::string ram,data,audio;uint64_t pcm;require(bool(state>>ram>>data>>pcm>>audio),"full cut digests");state>>std::ws;
   const auto hex=[](const std::string &s){return s.size()==64&&s.find_first_not_of("0123456789abcdef")==std::string::npos;};
   require(state.eof()&&values[4]<=7&&values[5]<=1&&values[28]<=1260947&&values[34]==0&&pcm<=88200&&!(pcm&1)&&hex(ram)&&hex(data)&&hex(audio),"bounded original cut witness");
  }
  require(record.ns>=last&&record.ns<=max_ns,"finite ordered input time");last=record.ns;result.push_back(record);
 }
 require(input.eof()&&complete,"strict completed tape before execution");
 uint64_t cuts=0;for(size_t i=0;i<result.size();++i){if(result[i].kind=='C')++cuts;else require(i+1<result.size()&&result[i+1].kind=='C'&&result[i+1].ns==result[i].ns,"UART followed by same-time cut");}require(cuts==163,"all163 original cuts");return result;
}
void phase(rt::Sha256 &hash,const BusRow &r){for(auto value:{uint64_t(r.address),uint64_t(r.value),uint64_t(r.mask),uint64_t(r.write),r.ns,r.phase})number(hash,value);}
}

int main(int argc,char **argv){try{
 require(argc==5,"staged assets, authentic tape, expected SHA and exclusive state path required");
 require(!std::filesystem::exists(argv[4]),"exclusive result state path");const auto records=transcript(argv[2],argv[3]);
 const auto program=load(std::filesystem::path(argv[1])/"sound_program.bin",0x80000),samples=load(std::filesystem::path(argv[1])/"sound_samples.bin",0x800000);
 rt::Model2AClock nc,rc;Link nl(nc),rl(rc);SoundTap sound;active_sound=&sound;sound.now=[&]{return nc.now_ns();};snd::Model2Scsp::ReferenceRandom nr,rr;
 snd::Model2ASound native(nc,{program,samples},[&]{return nr();},44100);
 sound_reference::OracleBoard reference(rc,{program,samples},[&]{return rr();},44100);
 nl.connect(native);rl.receiver=[&](bool v){reference.midi_in(v);};reference.on_midi_tx=[&](bool v){rl.chip.write_rxd(v);};reference.midi_in(rl.chip.txd_level());rl.chip.write_rxd(true);
 // Reset has already run on both actual boards. Attach before their first
 // opcode. The frozen SoundTap retains its actual board pointer and hashing.
 auto &cpu=const_cast<snd::Cpu68k&>(native.cpu());sound.attach(cpu);EventTap events(sound.target,cpu,nc);sound.target=&events;
 uint64_t boundaries=0,data_rows=0,prefetch_rows=0,iack_rows=0,cut_count=0,pcm_frames=0;bool midi=false;
 rt::Sha256 native_phases,reference_phases;std::vector<std::pair<BusRow,BusRow>> phase_examples;unsigned first_mmio=0;
 const auto compare=[&](bool reset){
  const auto n=native.stats();const auto r=reference.stats();const auto &a=native.cpu();const auto &b=reference.cpu();
  if(n.instructions!=r.instructions||n.cycles!=r.cycles||n.pc!=r.pc||n.irq_entries!=r.irq_entries)
   std::fprintf(stderr,"first boundary mismatch ns=%llu native N%llu/C%llu/PC%06x/IRQ%llu ref N%llu/C%llu/PC%06x/IRQ%llu\n",(unsigned long long)nc.now_ns(),(unsigned long long)n.instructions,(unsigned long long)n.cycles,n.pc,(unsigned long long)n.irq_entries,(unsigned long long)r.instructions,(unsigned long long)r.cycles,r.pc,(unsigned long long)r.irq_entries);
  require(n.status==snd::Model2ASound::Status::Running&&n.instructions==r.instructions&&n.cycles==r.cycles&&n.pc==r.pc&&n.previous_dispatch==r.previous_dispatch&&n.irq_entries==r.irq_entries,"independent CPU instruction/cycle/dispatch/IRQ equality");
  require(b.m_sp==15||b.m_sp==16,"real active stack bank");
  for(unsigned i=0;i<8;++i)require(a.d[i]==b.m_da[i]&&a.a[i]==b.m_da[i==7?b.m_sp:i+8],"all independent D/A registers");
  require(a.other_sp==b.m_da[b.m_sp==16?15:16]&&a.sr()==b.m_sr,"full SR and inactive stack; no flag mask");
  require(a.irq_line==reference.irq_line()&&a.nmi_pending==reference.nmi_pending(),"current IPL/NMI latch");
  require(std::equal(sound.board->sound_ram().begin(),sound.board->sound_ram().end(),reference.bus().sound_ram().begin()),"full independent512KiB RAM");
  require(nc.snapshot()==rc.snapshot()&&nr.state==rr.state,"queue ordering and independent RNG state");
  require(nl.chip.snapshot()==rl.chip.snapshot()&&nl.changes==rl.changes,"physical UART snapshots/TX changes");
  require(n.sample_frames==reference.sample_count()&&n.dropped_samples==0&&reference.dropped_samples()==0,"sample progression/no drops");
  std::vector<BusRow> expected;
  for(const auto &e:reference.accesses()){
   if(e.kind=='p'){
    ++prefetch_rows;require(!e.write&&e.mask==0xffff,"actual plain reference prefetch");
    const uint32_t offset=(reset&&e.address<16)?e.address:e.address-0x600000;
    require((reset&&e.address<16)||(e.address>=0x600000&&e.address<0x680000),"reference immutable prefetch address");
    require(offset+1<program.size()&&e.data==uint16_t(program[offset]<<8|program[offset+1]),"independent actual reference prefetch value");
   }else if(e.kind=='i'){require(!e.write&&e.mask==0xffff&&e.data>=25&&e.data<=31,"actual independent autovector");++iack_rows;}
   else expected.push_back({e.address,e.data,e.mask,e.write,e.board_ns,e.phase_cycle});
  }
  if(!reset){
   require(events.rows.size()==expected.size(),"per-event modeled data transfer count");
   for(size_t i=0;i<expected.size();++i){const auto &x=events.rows[i],&y=expected[i];
    const bool same=x.address==y.address&&x.value==y.value&&x.mask==y.mask&&x.write==y.write&&x.ns==y.ns;
    if(!same)std::fprintf(stderr,"first transfer mismatch native %06x/%04x/%04x/%d ns%llu phase%llu ref %06x/%04x/%04x/%d ns%llu phase%llu\n",x.address,x.value,x.mask,int(x.write),(unsigned long long)x.ns,(unsigned long long)x.phase,y.address,y.value,y.mask,int(y.write),(unsigned long long)y.ns,(unsigned long long)y.phase);
    require(same,"ordered modeled data at shared instruction-start time");phase(native_phases,x);phase(reference_phases,y);++data_rows;
    if((x.address>=0x100000&&x.address<0x101000&&first_mmio++<8)||x.address==0x100404){require(phase_examples.size()<32,"bounded phase evidence");phase_examples.emplace_back(x,y);}
    if(x.address==0x100404&&!x.write&&uint8_t(x.value)==0xff&&x.ns==1443240416)midi=true;
   }
  }
  events.rows.clear();reference.drain_accesses();++boundaries;
  require(boundaries<=4000000&&n.instructions<=1260947&&nc.now_ns()<=max_ns,"finite actual replay checkpoint");
 };
 compare(true);
 const auto advance=[&](uint64_t target){
  require(target>=nc.now_ns()&&target<=max_ns,"external target time");
  while(nc.next_deadline()&&*nc.next_deadline()<=target){require(nc.next_deadline()==rc.next_deadline(),"next independent deadline");const auto next=*nc.next_deadline();nc.advance(next);rc.advance(next);compare(false);}
  require(nc.next_deadline()==rc.next_deadline(),"paired remainder deadline");nc.advance(target);rc.advance(target);compare(false);
 };
 std::vector<std::pair<uint64_t,uint8_t>> writes;
 for(const auto &row:records){advance(row.ns);
  if(row.kind=='A'){
   if(row.write){nl.board.write(row.aligned,row.lanes,row.mask);rl.board.write(row.aligned,row.lanes,row.mask);if((row.address&3)==0)writes.emplace_back(row.ns,uint8_t(row.value));}
   else{uint32_t n=0,r=0;nl.board.read(row.aligned,row.mask,n);rl.board.read(row.aligned,row.mask,r);require((n>>((row.address&3)*8))==row.value&&n==r,"actual captured UART read result");}
   compare(false);
  }else{
   const auto n=native.stats();auto np=native.take_samples();auto rp=reference.take_samples();require(np.size()==rp.size(),"once-per-cut PCM drain count");std::vector<float> pcm;pcm.reserve(np.size()*2);
   for(size_t i=0;i<np.size();++i){require(std::isfinite(np[i].left)&&std::isfinite(np[i].right)&&std::bit_cast<uint32_t>(np[i].left)==std::bit_cast<uint32_t>(rp[i].left)&&std::bit_cast<uint32_t>(np[i].right)==std::bit_cast<uint32_t>(rp[i].right),"independent raw finite PCM bits at original cut");pcm.push_back(np[i].left);pcm.push_back(np[i].right);}pcm_frames+=np.size();
   const auto actual=cut(sound,{n.instructions,n.cycles,n.irq_entries,n.sample_frames,n.dropped_samples,n.pc,n.previous_dispatch,nl.chip.mode(),nl.chip.command(),nl.changes},pcm);
   if(actual!=row.state)std::fprintf(stderr,"original native cut mismatch cut=%llu ns=%llu\nexpected: %s\nactual:   %s\n",(unsigned long long)cut_count,(unsigned long long)row.ns,row.state.c_str(),actual.c_str());
   require(actual==row.state,"complete original native cut unchanged");++cut_count;
  }
 }
 const auto n=native.stats();require(cut_count==163&&n.instructions==1260947&&n.cycles==16312806&&n.irq_entries==660&&iack_rows==660&&midi&&nl.chip.tx_empty()&&rl.chip.tx_empty(),"exact authenticated independent terminal/MIDI");genuine_consumption(sound,writes);
 FILE *out=std::fopen(argv[4],"wx");require(out!=nullptr,"exclusive result state");
 std::fprintf(out,"{\"status\":\"PASS\",\"instructions\":%llu,\"cycles\":%llu,\"IRQ_entries\":%llu,\"board_ns\":%llu,\"original_cuts\":%llu,\"paired_observations\":%llu,\"modeled_data_rows\":%llu,\"reference_prefetch_rows\":%llu,\"PCM_frames\":%llu,\"native_phase_digest\":\"%s\",\"reference_phase_digest\":\"%s\",\"phase_examples\":[",(unsigned long long)n.instructions,(unsigned long long)n.cycles,(unsigned long long)n.irq_entries,(unsigned long long)nc.now_ns(),(unsigned long long)cut_count,(unsigned long long)boundaries,(unsigned long long)data_rows,(unsigned long long)prefetch_rows,(unsigned long long)pcm_frames,digest(native_phases).c_str(),digest(reference_phases).c_str());
 for(size_t i=0;i<phase_examples.size();++i){const auto &[x,y]=phase_examples[i];std::fprintf(out,"%s{\"address\":%u,\"data\":%u,\"mask\":%u,\"write\":%s,\"board_ns\":%llu,\"native_boundary_cycle\":%llu,\"reference_microcode_phase_cycle\":%llu}",i?",":"",x.address,x.value,x.mask,x.write?"true":"false",(unsigned long long)x.ns,(unsigned long long)x.phase,(unsigned long long)y.phase);}
 std::fputs("],\"scope\":\"Independent MAME CPU/shared devices/instruction-start MMIO; no bus-phase/title-music/sound-on-recording claim\"}\n",out);require(std::fclose(out)==0,"closed result state");
 std::printf("PASS independent authentic UART sound CPU instructions=%llu cycles=%llu IRQ=%llu cuts=%llu PCMframes=%llu observations=%llu\n",(unsigned long long)n.instructions,(unsigned long long)n.cycles,(unsigned long long)n.irq_entries,(unsigned long long)cut_count,(unsigned long long)pcm_frames,(unsigned long long)boundaries);return 0;
 }catch(const std::exception &e){std::fprintf(stderr,"independent authentic UART FAIL: %s\n",e.what());return 1;}}
