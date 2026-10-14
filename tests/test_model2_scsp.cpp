// Synthetic hardware tests; no game program/sample data.
#include "runtime/model2_scsp.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <tuple>

namespace {
unsigned checks=0;
#define CHECK(x) do { ++checks; if(!(x)) throw rt::Fatal("failed: " #x); } while(0)
using Chip=snd::Model2Scsp;
struct Fixture {
    rt::Model2AClock time;
    std::vector<uint8_t> ram=std::vector<uint8_t>(0x80000);
    Chip chip{time,ram,Chip::ReferenceRandom{}};
    std::vector<std::tuple<uint64_t,unsigned,Chip::LineState>> irqs;
    Fixture() { chip.on_irq=[this](unsigned line,Chip::LineState state) {irqs.emplace_back(time.now_ns(),line,state);}; }
    void write(uint16_t a,uint16_t d,uint16_t m=0xffff) { chip.write16(a,d,m); }
    uint16_t read(uint16_t a,uint16_t m=0xffff) { return chip.read16(a,m); }
};
void phase_and_lifetime() {
    rt::Model2AClock clock;
    std::vector<uint8_t> ram(0x80000);
    {
        Chip chip(clock,ram,Chip::ReferenceRandom{},4);
        CHECK(chip.sample_count()==0);
        chip.advance(0); CHECK(chip.sample_count()==0);
        chip.advance(1); CHECK(chip.sample_count()==1);
        chip.advance(22675); CHECK(chip.sample_count()==1);
        chip.advance(22676); CHECK(chip.sample_count()==2);
        chip.advance(10'000'000); CHECK(chip.sample_count()==442);
        CHECK(chip.dropped_samples()==438);
        const auto samples=chip.take_samples(); CHECK(samples.size()==4);
        CHECK(std::all_of(samples.begin(),samples.end(),[](auto s){return s.left==0 && s.right==0;}));
        CHECK(chip.take_samples().empty());
        chip.reset(); chip.advance(10'022'675); CHECK(chip.sample_count()==442);
        chip.advance(10'022'676); CHECK(chip.sample_count()==443);
    }
    CHECK(clock.snapshot().pending.empty());
    const auto maximum=std::numeric_limits<uint64_t>::max();
    rt::Model2AClock ending(maximum-1);
    {
        Chip chip(ending,ram,Chip::ReferenceRandom{},0);
        bool overflow=false;
        try {ending.advance(maximum);}catch(const rt::Fatal&){overflow=true;}
        CHECK(overflow && chip.sample_count()==1 && chip.dropped_samples()==1);
    }
    CHECK(ending.snapshot().pending.empty());
    bool rejected=false;
    try {Chip chip(ending,ram,Chip::ReferenceRandom{});}catch(const rt::Fatal&){rejected=true;}
    CHECK(rejected && ending.snapshot().pending.empty());
}
void registers_and_dma() {
    Fixture f;
    f.write(0x702,0x1234); f.write(0x702,0xab00,0xff00); f.write(0x702,0x00cd,0x00ff);
    CHECK(f.read(0x702)==0xabcd);
    f.write(0x780,0x4567); CHECK(f.read(0x7c0)==0x4567);
    f.write(0x7c2,0x89ab); CHECK(f.read(0x782)==0x89ab);
    f.write(0x432,0xffff); CHECK(f.read(0x432)==0);
    f.write(0x424,0x10); f.write(0x41e,0x10); // DMA -> level1, enabled
    f.ram[0x100]=0x13; f.ram[0x101]=0x57;
    f.write(0x412,0x100); f.write(0x414,0x704); f.write(0x416,0x1002);
    CHECK(f.read(0x704)==0x1357 && (f.read(0x416)&0x1000)==0);
    CHECK((f.irqs.back()==std::tuple{uint64_t(0),1u,Chip::LineState::Hold}));
    f.write(0x412,0x102); f.write(0x414,0x704); f.write(0x416,0x3002);
    CHECK(f.ram[0x102]==0x13 && f.ram[0x103]==0x57);
    f.write(0x412,0x102); f.write(0x414,0x704); f.write(0x416,0x7002);
    CHECK(f.ram[0x102]==0 && f.ram[0x103]==0);
    f.write(0x412,0x100); f.write(0x414,0x704); f.write(0x416,0x5002);
    CHECK(f.read(0x704)==0);
    bool rejected=false; try { f.read(1); } catch(const rt::Fatal&) {rejected=true;} CHECK(rejected);
}
void timers() {
    Fixture f;
    f.write(0x424,0x40); f.write(0x41e,0x40); f.write(0x418,0xfe);
    f.time.advance(22675); CHECK(f.irqs.empty());
    f.time.advance(22676); CHECK((f.irqs.back()==std::tuple{uint64_t(22676),1u,Chip::LineState::Assert}));
    CHECK((f.read(0x420)&0x40)!=0 && (f.read(0x418)&0xff)==0xff);
    f.write(0x422,0x40);
    CHECK(std::get<2>(f.irqs.back())==Chip::LineState::Clear);
    CHECK((f.read(0x420)&0x40)!=0); // expired timer re-latches after IRQ update
    f.write(0x418,0xfe);
    f.write(0x418,0xff); // reference does not cancel the previous timer
    f.irqs.clear(); f.time.advance(45351); CHECK(f.irqs.empty());
    f.time.advance(45352); CHECK(std::get<2>(f.irqs.back())==Chip::LineState::Assert);
    f.write(0x418,0xfe); f.chip.reset(); f.irqs.clear();
    f.time.advance(68028); CHECK(!f.irqs.empty()); // SCSP soft reset preserves timers
    for(unsigned timer=0;timer<3;++timer)for(unsigned prescaler=0;prescaler<8;++prescaler)
        for(unsigned low:{0u,1u,127u,254u}) {
            Fixture test;
            const unsigned irq=timer?0x80:0x40;
            test.write(0x424,uint16_t(irq));test.write(0x41e,uint16_t(0x40u<<timer));
            test.write(uint16_t(0x418+timer*2),uint16_t((prescaler<<8)|low));
            const uint64_t frequency=(Chip::CLOCK_HZ/(1u<<prescaler))/(255-low);
            const uint64_t deadline=(512*(1'000'000'000'000'000'000ull/frequency)+999'999'999)/1'000'000'000;
            test.time.advance(deadline-1);CHECK(test.irqs.empty());
            test.time.advance(deadline);CHECK((test.read(0x420)&(0x40<<timer))!=0);
            CHECK((test.irqs.back()==std::tuple{deadline,1u,Chip::LineState::Assert}));
        }
}
void receive(Fixture &f,uint8_t byte) {
    const auto start=f.time.now_ns()+1000;
    f.chip.midi_in(true); f.time.advance(start); f.chip.midi_in(false);
    for(unsigned bit=0;bit<8;++bit) {f.time.advance(start+(bit+1)*32'000); f.chip.midi_in((byte>>bit)&1);}
    f.time.advance(start+288'000); f.chip.midi_in(true);
    f.time.advance(start+303'999); CHECK((f.read(0x420)&8)==0);
    f.time.advance(start+304'000); CHECK((f.read(0x420)&8)!=0);
}
void serial() {
    Fixture f;
    f.write(0x424,8); f.write(0x41e,8);
    for(unsigned byte=0;byte<256;++byte) {
        receive(f,uint8_t(byte));
        CHECK((f.read(0x404)&0xff)==byte);
        CHECK((f.read(0x420)&8)==0);
    }
    receive(f,0x5a); f.write(0x404,0xff00,0xff00); // write's read-before-combine consumes RX
    CHECK((f.read(0x420)&8)==0);
    Fixture tx;
    std::vector<std::pair<uint64_t,bool>> pins;
    tx.chip.on_midi_tx=[&](bool b){pins.emplace_back(tx.time.now_ns(),b);};
    tx.write(0x406,0x96); tx.write(0x406,0x69);
    tx.time.advance(15'999); CHECK(pins.empty());
    tx.time.advance(624'000); CHECK(pins.size()==20);
    for(unsigned byte=0;byte<2;++byte) for(unsigned bit=0;bit<10;++bit) {
        const auto [time,value]=pins[byte*10+bit];
        const auto data=byte?0x69:0x96;
        CHECK(time==16'000+uint64_t(byte)*320'000+bit*32'000);
        CHECK(value==(bit==0?false:bit==9?true:bool((data>>(bit-1))&1)));
    }
    // Reset while RX half-clock is1: phase is retained, so first sample is
    // 32us after the next start rather than the normal48us.
    Fixture reset;
    reset.chip.midi_in(true); reset.chip.midi_in(false); reset.time.advance(32'000);
    reset.chip.reset(); reset.chip.midi_in(true); reset.chip.midi_in(false);
    for(unsigned bit=0;bit<8;++bit) {
        reset.time.advance(32'000+(bit+1)*32'000-1); reset.chip.midi_in((0xa5>>bit)&1);
    }
    reset.time.advance(32'000+9*32'000-1); reset.chip.midi_in(true);
    reset.time.advance(32'000+9*32'000); CHECK((reset.read(0x404)&0xff)==0xa5);
}
void pcm() {
    Fixture f;
    for(unsigned i=0;i<8;++i) {f.ram[0x100+i*2]=0x10; f.ram[0x101+i*2]=0;}
    f.write(0x400,0xf); f.write(0x002,0x100); f.write(0x004,0); f.write(0x006,4);
    f.write(0x00c,0x100); // SDIR: bypass EG and TL
    f.write(0x016,0xe000); // direct stereo, maximum send
    f.write(0x000,0x1820); // keyon+execute, normal loop
    CHECK((f.read(0)&0x1000)==0);
    f.time.advance(1'000'000);
    auto samples=f.chip.take_samples(); CHECK(samples.size()==45);
    for(auto sample:samples) CHECK(sample.left==0.125f && sample.right==0.125f);
    f.write(0x400,0); f.time.advance(2'000'000);
    samples=f.chip.take_samples(); CHECK(!samples.empty());
    for(auto sample:samples) CHECK(sample.left==0 && sample.right==0);
}
}
int main() {
    try {
        phase_and_lifetime(); registers_and_dma(); timers(); serial(); pcm();
        std::printf("SCSP: %u register/DMA/timer/serial/sample checks passed\n",checks); return 0;
    } catch(const std::exception&e) {std::fprintf(stderr,"%s\n",e.what());return 1;}
}
