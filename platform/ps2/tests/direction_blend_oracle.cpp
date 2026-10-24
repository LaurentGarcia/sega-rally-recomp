// Private original-program comparison; never linked into a native game.
#include "direction_blend.h"
#include "direction_blend_cases.h"
#include "refcore/i960_ref.h"
#include <algorithm>
#include <bit>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

namespace {
using direction_blend_cases::Bits;
constexpr std::uint32_t relocation=0x59f000,entry=relocation+0x3110c,end=relocation+0x31158;
std::vector<std::uint8_t> load(const char *name) {
    std::ifstream stream(name,std::ios::binary);
    if (!stream) throw rt::Fatal("cannot read private oracle input");
    return {std::istreambuf_iterator<char>(stream),{}};
}
std::uint32_t word(const std::vector<std::uint8_t> &data,std::size_t offset) {
    std::uint32_t result=0;
    for (unsigned i=0;i<4;++i) result|=std::uint32_t(data.at(offset+i))<<(8*i);
    return result;
}
struct Memory : rt::Bus {
    const std::vector<std::uint8_t> &program;
    std::uint32_t object=0,output=0,target_first=0;
    std::array<std::uint32_t,4> result{};
    unsigned reads=0,writes=0;
    explicit Memory(const std::vector<std::uint8_t> &image):program(image) {}
    std::uint32_t fetch(std::uint32_t address) override {
        if ((address&3) || address<entry || address>=end)
            throw rt::Fatal("direction instruction escaped verified range");
        return word(program,address-relocation);
    }
    std::uint32_t read_dword(std::uint32_t address) override {
        if (address!=object+0x9c) throw rt::Fatal("unexpected direction data read");
        ++reads; return target_first;
    }
    void write_dword(std::uint32_t address,std::uint32_t value) override {
        if (address<output || address>output+12 || (address&3))
            throw rt::Fatal("unexpected direction data write");
        result[(address-output)/4]=value; ++writes;
    }
    std::uint8_t read_byte(std::uint32_t) override { throw rt::Fatal("unexpected byte read"); }
    std::uint16_t read_word(std::uint32_t) override { throw rt::Fatal("unexpected halfword read"); }
    void write_byte(std::uint32_t,std::uint8_t) override { throw rt::Fatal("unexpected byte write"); }
    void write_word(std::uint32_t,std::uint16_t) override { throw rt::Fatal("unexpected halfword write"); }
    std::uint16_t flags(std::uint32_t) override { return rt::Cpu::BURST; }
};
}
int main(int argc,char **argv) {
    if (argc<2 || argc>3) return 2;
    std::uint64_t cases=0,instructions=0,reads=0,writes=0,live=0;
    std::uint64_t fingerprint=14695981039346656037ull;
    unsigned minimum=32,maximum=0;
    std::vector<Bits> visited;
    try {
        const auto program=load(argv[1]);
        if (program.size()!=2097152) return 2;
        Memory memory(program);
        rt::I960Ref reference(&memory);
        const auto test=[&](Bits input,const std::array<std::uint32_t,2> *captured) {
            const ps2::native::PlanarPair base{std::bit_cast<float>(input[0]),std::bit_cast<float>(input[1])};
            const ps2::native::PlanarPair target{std::bit_cast<float>(input[2]),std::bit_cast<float>(input[3])};
            ps2::native::PlanarPair native{};
            if (!ps2::native::blend_planar_direction(base,target,native))
                throw rt::Fatal("original case outside bounded native contract");
            const std::array<std::uint32_t,2> expected{std::bit_cast<std::uint32_t>(native.first),
                                                       std::bit_cast<std::uint32_t>(native.third)};
            std::fill(std::begin(reference.m_r),std::end(reference.m_r),std::uint32_t(cases)^0x9e37b1a5u);
            std::fill(std::begin(reference.m_fp),std::end(reference.m_fp),-13.5);
            memory.object=0x200000+(std::uint32_t(cases)&255u)*1024u;
            memory.output=0x300000+(std::uint32_t(cases)&255u)*64u;
            memory.target_first=input[2]; memory.reads=memory.writes=0;
            memory.result.fill(0xdead9876u);
            reference.m_r[4]=input[0]; reference.m_r[23]=input[1]; reference.m_r[18]=input[3];
            reference.m_r[16]=memory.object; reference.m_r[17]=memory.output;
            const auto middle=reference.m_r[5],padding=reference.m_r[7];
            reference.m_AC=0x3f001000; reference.m_IP=entry;
            unsigned steps=0;
            while (reference.m_IP!=end) {
                if (++steps>24) throw rt::Fatal("direction instruction bound exceeded");
                reference.execute_one();
            }
            if (memory.result[0]!=expected[0] || memory.result[2]!=expected[1] ||
                memory.result[1]!=middle || memory.result[3]!=padding || memory.reads!=1 || memory.writes!=4 ||
                (captured && *captured!=expected)) {
                std::fprintf(stderr,"DIRECTION ORACLE MISMATCH case=%llu input=%08x,%08x,%08x,%08x native=%08x,%08x original=%08x,%08x live=%u\n",
                    (unsigned long long)cases,input[0],input[1],input[2],input[3],expected[0],expected[1],
                    memory.result[0],memory.result[2],captured?1u:0u);
                return false;
            }
            for (auto value:input) fingerprint=(fingerprint^value)*1099511628211ull;
            for (auto value:expected) fingerprint=(fingerprint^value)*1099511628211ull;
            minimum=std::min(minimum,steps); maximum=std::max(maximum,steps);
            ++cases; instructions+=steps; reads+=memory.reads; writes+=memory.writes;
            visited.push_back(input);
            return true;
        };
        if (!direction_blend_cases::probe([&](Bits input) { return test(input,nullptr); }) ||
            !direction_blend_cases::extended([&](Bits input) { return test(input,nullptr); })) return 1;
        if (argc==3) {
            const auto records=load(argv[2]);
            const std::array<std::uint8_t,8> magic{'R','L','Y','D','B','L','1',0};
            if (records.size()<12 || !std::equal(magic.begin(),magic.end(),records.begin())) return 2;
            const auto count=word(records,8);
            if (count>100000 || records.size()!=12+std::size_t(count)*32) return 2;
            for (unsigned index=0;index<count;++index) {
                const auto start=12+std::size_t(index)*32;
                Bits input{}; for (unsigned k=0;k<4;++k) input[k]=word(records,start+k*4);
                const std::array<std::uint32_t,2> expected{word(records,start+16),word(records,start+20)};
                if (!test(input,&expected)) return 1;
                ++live;
            }
        }
    } catch (const std::exception &error) {
        std::fprintf(stderr,"direction oracle failed: %s\n",error.what()); return 1;
    }
    std::sort(visited.begin(),visited.end());
    const auto unique=std::unique(visited.begin(),visited.end())-visited.begin();
    std::printf("NATIVE_DIRECTION_ORACLE PASS cases=%llu unique_quadruples=%llu live_records=%llu instructions=%llu reads=%llu writes=%llu steps=%u..%u hash=%016llx\n",
        (unsigned long long)cases,(unsigned long long)unique,(unsigned long long)live,
        (unsigned long long)instructions,(unsigned long long)reads,(unsigned long long)writes,
        minimum,maximum,(unsigned long long)fingerprint);
    return 0;
}
