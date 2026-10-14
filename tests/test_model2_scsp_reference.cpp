// Differential oracle: compile the UNMODIFIED pinned public scsp.cpp,
// scspdsp.cpp and diserial.cpp behind minimal host services, separately from
// the native implementation. No algorithm is copied from the native chip.
// Pin dddd73680656e355bb2b5beecab1167c9f07bf81; hashes are in model2_scsp.cpp.
// Prepare an ignored include directory with those three .cpp and .h pairs,
// plus empty emu.h, dirom.h, logmacro.h; add that directory with -I.
// This verifies the chip algorithm, not a complete MAME machine scheduler.
#include "runtime/model2_scsp.h"
#include "runtime/model2_scsp_dsp.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <span>
#include <tuple>
#include <vector>

using u8=uint8_t; using s8=int8_t; using u16=uint16_t; using s16=int16_t;
using u32=uint32_t; using s32=int32_t; using u64=uint64_t; using s64=int64_t;
using offs_t=uint32_t;
template<class T> constexpr unsigned BIT(T value,unsigned bit) { return (uint32_t(value)>>bit)&1; }
namespace util { template<class T> constexpr s32 sext(T value,unsigned bits) {
    return s32(uint32_t(value)<<(32-bits))>>(32-bits);
} }
constexpr int CLEAR_LINE=0, ASSERT_LINE=1, HOLD_LINE=2, ENDIANNESS_BIG=1;
#define ATTR_COLD
#define TIMER_CALLBACK_MEMBER(name) void name(int param)
#define FUNC(name) &name
#define NAME(name) name
#define DEFINE_DEVICE_TYPE(name,...) constexpr int name=0;
#define DECLARE_DEVICE_TYPE(...)
#define LOGMASKED(...) ((void)0)
#define popmessage(...) ((void)0)
#define COMBINE_DATA(p) (*(p)=(*(p)&~mem_mask)|(data&mem_mask))

// Independent attosecond scheduler: the source's timers retain their actual
// sub-nanosecond deadlines. Logs round UP only when compared with native time.
struct attotime {
    __int128 value=0;
    static const attotime never;
    static attotime from_hz(u32 rate) { return {1'000'000'000'000'000'000ll/rate}; }
    static attotime from_ticks(u64 ticks,u32 rate) { return {__int128(ticks)*(1'000'000'000'000'000'000ll/rate)}; }
    bool is_never() const { return value<0; }
    attotime operator/(int n)const { return is_never()?never:attotime{value/n}; }
    attotime operator*(int n)const { return is_never()?never:attotime{value*n}; }
};
const attotime attotime::never{-1};
struct oracle_scheduler;
struct emu_timer {
    oracle_scheduler &scheduler;
    std::function<void(int)> callback;
    u64 revision=0;
    void adjust(attotime delay,int param=0,attotime period=attotime::never);
};
struct oracle_scheduler {
    __int128 now=0;
    u64 sequence=0;
    struct Event { emu_timer *timer;u64 revision;int param;attotime period; };
    std::map<std::pair<__int128,u64>,Event> queue;
    std::vector<std::unique_ptr<emu_timer>> timers;
    emu_timer *timer_alloc(std::function<void(int)> callback) {
        timers.push_back(std::make_unique<emu_timer>(emu_timer{*this,std::move(callback)}));
        return timers.back().get();
    }
    void advance(u64 ns) {
        const auto target=__int128(ns)*1'000'000'000;
        assert(target>=now);
        while(!queue.empty() && queue.begin()->first.first<=target) {
            const auto [key,event]=*queue.begin(); queue.erase(queue.begin());
            if(event.revision!=event.timer->revision) continue;
            now=key.first;
            event.timer->callback(event.param);
            if(event.revision==event.timer->revision && !event.period.is_never())
                event.timer->adjust(event.period,event.param,event.period);
        }
        now=target;
    }
    u64 ns()const { return u64((now+999'999'999)/1'000'000'000); }
};
void emu_timer::adjust(attotime delay,int param,attotime period) {
    ++revision;
    if(!delay.is_never()) scheduler.queue.emplace(std::pair{scheduler.now+delay.value,++scheduler.sequence},
        oracle_scheduler::Event{this,revision,param,period});
}
template<class C> auto timer_expired_delegate(void(C::*method)(int),C *object) {
    return [=](int param) { (object->*method)(param); };
}
struct address_space {
    std::span<u8> bytes;
    u8 read_byte(u32 address)const { address&=0xfffff;return address<0x80000?bytes[address]:0; }
    u16 read_word(u32 address)const {address&=0xffffe;return u16(read_byte(address)<<8|read_byte(address+1));}
    void write_word(u32 address,u16 value) {address&=0xffffe;if(address<0x80000){bytes[address]=value>>8;bytes[address+1]=value;}}
};
struct running_machine {
    oracle_scheduler scheduler_;
    address_space memory;
    u32 rng=0x9d14abd7;
    oracle_scheduler &scheduler(){return scheduler_;}
    const char *describe_context()const{return "oracle";}
    u32 rand(){rng=1664525*rng+1013904223;return (rng>>16)|(rng<<16);}
};
struct machine_config { running_machine &machine; };
struct device_t {
    using feature_type=int; struct feature {static constexpr int SOUND=1;};
    running_machine &machine_;u32 clock_;
    device_t(const machine_config& c,int,const char*,device_t*,u32 clock):machine_(c.machine),clock_(clock){}
    virtual ~device_t()=default;
    running_machine &machine(){return machine_;}
    u32 clock()const{return clock_;}
    virtual void device_start(){} virtual void device_reset(){} virtual void device_post_load(){} virtual void device_clock_changed(){}
    template<class...T>void save_item(T&&...){}
    template<class...T>void logerror(T&&...){}
    template<class C>emu_timer*timer_alloc(void(C::*method)(int),C*object){return machine_.scheduler().timer_alloc(timer_expired_delegate(method,object));}
};
struct device_interface {
    device_t &device_;
    device_interface(device_t&device,const char*):device_(device){}
    virtual ~device_interface()=default;
    device_t &device(){return device_;}
    virtual void interface_pre_start(){}virtual void interface_post_start(){}
};
template<class Signature> struct shared_callback {
    std::shared_ptr<std::function<Signature>> value=std::make_shared<std::function<Signature>>();
    template<class F>shared_callback&operator=(F f){*value=std::move(f);return *this;}
    explicit operator bool()const{return bool(*value);}
    template<class...T>void operator()(T...args){(*value)(args...);}
};
struct devcb_write8 {
    shared_callback<void(unsigned,int)> callback;
    devcb_write8(device_t&){}
    devcb_write8&bind(){return *this;}
    bool isunset()const{return false;}
    void operator()(unsigned line,int state){if(callback)callback(line,state);}
};
struct devcb_write_line {
    shared_callback<void(int)> callback;
    devcb_write_line(device_t&){}
    devcb_write_line&bind(){return *this;}
    void operator()(int state){if(callback)callback(state);}
};
struct sound_stream {
    running_machine &machine;
    std::function<void(sound_stream&)> callback;
    std::vector<snd::Model2Scsp::Sample> output;
    u64 frames=0;unsigned rate;float gain[2]={1,1};snd::Model2Scsp::Sample current;
    int samples()const{return 1;}
    float get(int,int)const{return 0;}
    void put_int_clamp(int channel,int,s32 value,s32 maximum) {
        float sample=float(double(std::clamp(value,-maximum,maximum-1))/maximum);
        (channel?current.right:current.left)=sample;
    }
    void set_sample_rate(unsigned value){rate=value;}
    void update() {
        if(!machine.scheduler_.now)return;
        const auto end=u64(machine.scheduler_.now*rate/1'000'000'000'000'000'000ll)+1;
        while(frames<end){callback(*this);current.left*=gain[0];current.right*=gain[1];output.push_back(current);++frames;}
    }
};
struct device_sound_interface {
    device_t&device;std::unique_ptr<sound_stream> stream;
    device_sound_interface(const machine_config&,device_t&d):device(d){}
    virtual void sound_stream_update(sound_stream&)=0;
    sound_stream*stream_alloc(int,int,unsigned rate) {
        stream=std::make_unique<sound_stream>(sound_stream{device.machine(),[this](auto&s){sound_stream_update(s);},{},0,rate,{1,1},{}});
        return stream.get();
    }
    void set_output_gain(unsigned channel,float gain){stream->gain[channel]=gain;}
};
template<int,int,int,int>struct device_rom_interface {
    device_t&device;
    device_rom_interface(const machine_config&,device_t&d):device(d){}
    virtual void rom_bank_pre_change(){}
    address_space&space(){return device.machine().memory;}
    u8 read_byte(u32 a){return space().read_byte(a);}u16 read_word(u32 a){return space().read_word(a);}
};

// The oracle sources are supplied unchanged from the pinned public checkout.
#include "diserial.cpp"
#include "scspdsp.cpp"
#include "scsp.cpp"

struct Oracle:scsp_device {
    explicit Oracle(const machine_config &c):scsp_device(c,"oracle",nullptr){}
    void start(){interface_pre_start();device_start();device_reset();}
    void reset(){device_reset();}
    void output_irq(std::function<void(unsigned,int)> cb){m_irq_cb_access(cb);}
    void m_irq_cb_access(std::function<void(unsigned,int)> cb) {
        // bind() returns by value upstream, but each copied adapter shares its
        // callback storage in MAME. The shim installs below through its binder.
        irq_cb().callback=std::move(cb);
    }
};

static unsigned checks=0;
static void check(bool value,const char*why){++checks;if(!value){std::cerr<<"SCSP reference mismatch: "<<why<<'\n';std::abort();}}
static void dsp_differential() {
    snd::Model2Scsp::ReferenceRandom rng;
    std::vector<u8> a_ram(0x80000),b_ram(0x80000);
    snd::scsp_detail::ScspRam a_space{a_ram};address_space b_space{b_ram};
    snd::scsp_detail::Model2ScspDsp a;SCSPDSP b;
    for(unsigned program=0;program<256;++program) {
        a.Init();b.Init();a.space=&a_space;b.space=&b_space;
        for(unsigned i=0;i<a_ram.size();++i)a_ram[i]=b_ram[i]=u8(rng());
        a.RBP=b.RBP=rng()&63;a.RBL=b.RBL=8192u<<(rng()&3);
        for(unsigned i=0;i<64;++i)a.COEF[i]=b.COEF[i]=s16(rng());
        for(unsigned i=0;i<32;++i){a.MADRS[i]=b.MADRS[i]=u16(rng());a.MEMS[i]=b.MEMS[i]=s32(rng()&0xffffff)-0x800000;}
        for(unsigned i=0;i<128;++i){
            a.TEMP[i]=b.TEMP[i]=s32(rng()&0xffffff)-0x800000;
            for(unsigned word=0;word<4;++word)a.MPRO[i*4+word]=b.MPRO[i*4+word]=u16(rng());
            const u16 input=u16(rng()%50);
            a.MPRO[i*4+1]=b.MPRO[i*4+1]=u16((a.MPRO[i*4+1]&~0xfc0)|(input<<6));
        }
        a.Start();b.Start();check(a.LastStep==b.LastStep,"DSP program extent");
        for(unsigned frame=0;frame<16;++frame) {
            for(unsigned input=0;input<16;++input){const s32 sample=s32(rng()&0x7ffff)-0x40000;a.SetSample(sample,input,7);b.SetSample(sample,input,7);}
            for(unsigned input=0;input<2;++input)a.EXTS[input]=b.EXTS[input]=s16(rng());
            a.Step();b.Step();
            check(std::equal(std::begin(a.TEMP),std::end(a.TEMP),std::begin(b.TEMP)),"DSP TEMP");
            check(std::equal(std::begin(a.MEMS),std::end(a.MEMS),std::begin(b.MEMS)),"DSP MEMS");
            check(std::equal(std::begin(a.MIXS),std::end(a.MIXS),std::begin(b.MIXS)),"DSP MIXS");
            check(std::equal(std::begin(a.EFREG),std::end(a.EFREG),std::begin(b.EFREG)),"DSP EFREG");
            check(a.DEC==b.DEC,"DSP DEC");check(a_ram==b_ram,"DSP RAM");
        }
    }
}
struct Pair {
    std::vector<u8> native_ram=std::vector<u8>(0x80000),reference_ram=std::vector<u8>(0x80000);
    rt::Model2AClock clock;
    snd::Model2Scsp::ReferenceRandom random;
    snd::Model2Scsp native{clock,native_ram,[this]{return random();},100000};
    running_machine machine{{},{reference_ram}};
    machine_config config{machine};
    // Upstream leaves some cold monitor/DMA fields unspecified. Zero backing
    // storage before construction makes these explicit for both instances.
    alignas(Oracle) std::array<std::byte,sizeof(Oracle)> storage{};
    Oracle*reference=new(storage.data())Oracle(config);
    using Irq=std::tuple<u64,unsigned,int>;
    std::vector<Irq> native_irq,reference_irq;
    std::vector<std::pair<u64,int>> native_tx,reference_tx;
    Pair(){reference->start();
        native.on_irq=[this](unsigned level,auto state){native_irq.emplace_back(clock.now_ns(),level,int(state));};
        reference->output_irq([this](unsigned level,int state){reference_irq.emplace_back(machine.scheduler_.ns(),level,state);});
        native.on_midi_tx=[this](bool state){native_tx.emplace_back(clock.now_ns(),state);};
        reference->midi_out_cb().callback=[this](int state){reference_tx.emplace_back(machine.scheduler_.ns(),state);};
    }
    ~Pair(){reference->~Oracle();}
    void advance(u64 ns){clock.advance(ns);machine.scheduler_.advance(ns);reference->stream->update();}
    void write(u16 offset,u16 data,u16 mask=0xffff){native.write16(offset,data,mask);reference->write(offset/2,data,mask);}
    void read(u16 offset){check(native.read16(offset,0xffff)==reference->read(offset/2),"register read");}
    void rx(bool value){native.midi_in(value);reference->midi_in(value);}
    void reset(){native.reset();reference->reset();}
    void compare(){
        check(native_ram==reference_ram,"RAM");check(random.state==machine.rng,"random state");
        check(native_irq==reference_irq,"IRQ transitions");check(native_tx==reference_tx,"MIDI TX transitions");
        check(native.take_samples()==reference->stream->output,"sample buffers");reference->stream->output.clear();
    }
};

int main(){
    {Pair p;for(unsigned offset=0;offset<0x1000;offset+=2)p.read(offset);p.advance(10'000'000);p.compare();}
    {Pair p;
        for(unsigned offset=0;offset<0x1000;offset+=2){
            if(offset>=0x400 && offset<0x600)continue; // exercise control separately
            const u16 value=u16(offset*3811+0x7142);
            // Keep voice KEYONEX and DSP Start off in register-only coverage.
            if(offset<0x400 && offset%32==0)p.write(offset,value&~0x1800);
            else if(offset!=0xbf0)p.write(offset,value);
            p.read(offset);p.write(offset,u16(~value),0xff00);p.read(offset);
        }
        p.compare();
    }
    {Pair p;
        for(unsigned i=0;i<64;++i)p.native_ram[0x100+i]=p.reference_ram[0x100+i]=u8(i*113);
        p.write(0x424,0x10);p.write(0x41e,0x10);
        for(const auto mode:{0x1000,0x3000,0x7000,0x5000}) {
            p.write(0x412,0x100);p.write(0x414,0x700);p.write(0x416,u16(mode|64));
            for(unsigned i=0;i<64;i+=2)p.read(u16(0x700+i));p.compare();
        }
    }
    {Pair p;p.write(0x424,0x0048);p.write(0x426,0x0088);p.write(0x428,0x0080);p.write(0x41e,0x01f8);
        for(unsigned i=0;i<3;++i)p.write(u16(0x418+i*2),u16((i<<8)|250));
        for(u64 ns=1;ns<1'000'000;ns+=113){p.advance(ns);p.read(0x420);}
        p.write(0x422,0x1ff);p.read(0x420);p.compare();
    }
    {Pair p;
        for(unsigned byte=0;byte<256;++byte){const u64 start=byte*400'000ull;
            p.advance(start);p.rx(1);p.rx(0);
            for(unsigned bit=0;bit<8;++bit){p.advance(start+32'000*(bit+1));p.rx((byte>>bit)&1);}
            p.advance(start+288'000);p.rx(1);p.advance(start+304'000);p.read(0x404);
            p.write(0x406,u16(byte));
        }
        p.advance(103'000'000);p.compare();
    }
    {Pair p;
        p.rx(1);p.rx(0);p.advance(32'000);p.reset();p.rx(1);p.rx(0);
        for(unsigned bit=0;bit<8;++bit){p.advance(63'000+32'000*bit);p.rx((0x96>>bit)&1);}
        p.advance(319'000);p.rx(1);p.advance(320'000);p.read(0x404);p.compare();
    }
    // Synthetic multi-voice PCM8/16, noise, loop modes, envelopes, all LFO
    // waveforms, pitch/pan, modulation ring, DSP routing and key-off.
    {Pair p;
        for(unsigned i=0;i<0x80000;++i)p.native_ram[i]=p.reference_ram[i]=u8(i*73+(i>>8));
        p.write(0x400,0x000f);
        // Active DSP program: coefficient multiplication plus alternating
        // delay-RAM read/write, TEMP/MEMS, address/fraction and effect output.
        for(unsigned i=0;i<64;++i)p.write(u16(0x700+2*i),u16(0x7fff-i*317));
        for(unsigned i=0;i<32;++i)p.write(u16(0x780+2*i),u16(i*77));
        for(unsigned i=0;i<128;++i){
            p.write(u16(0x800+i*8),u16((i<<8)|0x80|((i+1)&127)));
            p.write(u16(0x802+i*8),u16(0xa000|((0x20+i%16)<<6)|0x20|(i%32)));
            p.write(u16(0x804+i*8),u16(0x30c8|((i%16)<<8)|((i%3)<<4)|((i&1)?0x4000:0)));
            p.write(u16(0x806+i*8),u16(((i%64)<<9)|((i%32)<<2)|(i&1)));
        }
        p.write(0xbf0,0x7e00); // reference Start register is 0xbf0
        for(unsigned slot=0;slot<32;++slot){const u16 base=slot*32;
            p.write(base+2,u16(slot*2048));p.write(base+4,4);p.write(base+6,64);
            p.write(base+8,u16(0x002f|((slot%32)<<6)));p.write(base+10,u16(0x2000|(slot&31)));
            p.write(base+12,u16(slot*7));p.write(base+14,u16(((slot%4)<<12)|((slot*2)<<6)|slot));
            p.write(base+16,u16((slot%3)<<11|(slot*31&0x3ff)));
            p.write(base+18,u16(((slot%4)<<8)|((slot%4)<<3)|0x1877));
            p.write(base+20,u16((slot%16)<<3|7));p.write(base+22,u16(0xe0e0|((slot%32)<<8)|(slot%32)));
            p.write(base,u16(0x0800|((slot%4)<<5)|((slot%2)<<4)|((slot==31)?0x80:0)));
        }
        p.write(0,0x1820);
        for(u64 ns=1;ns<100'000'000;ns+=22'677){p.advance(ns);p.read(0x408);}
        for(unsigned slot=0;slot<32;++slot)p.write(u16(slot*32),0);
        p.write(0,0x1000);p.advance(120'000'000);p.compare();
    }
    dsp_differential();
    std::cout<<"SCSP pinned-source differential: "<<checks<<" checks passed\n";
}
