// Offline contact query and projected-test oracle; no firmware/data embedded.
#include "runtime/tgp.h"
#include <array>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
#include "contact_search.h"
#include "contact_search_cases.h"

namespace {
std::vector<std::uint8_t> read(const char *path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw rt::TgpFatal("cannot open count-probe input");
    return {std::istreambuf_iterator<char>(input), {}};
}
std::uint32_t word(const std::vector<std::uint8_t> &bytes, std::size_t offset) {
    return bytes.at(offset) | (std::uint32_t(bytes.at(offset + 1)) << 8) |
           (std::uint32_t(bytes.at(offset + 2)) << 16) | (std::uint32_t(bytes.at(offset + 3)) << 24);
}
struct Bus final : rt::TgpBus {
    const std::vector<std::uint8_t> &copro;
    std::array<std::uint32_t, 0x8000> buffer{};
    std::vector<std::uint32_t> input, output, reads;
    std::size_t consumed = 0;
    explicit Bus(const std::vector<std::uint8_t> &data) : copro(data) {}
    bool fifo_pop(std::uint32_t &value) override {
        if (consumed == input.size()) return false;
        value = input[consumed++]; return true;
    }
    void fifo_push(std::uint32_t value) override { output.push_back(value); }
    std::uint32_t mem_r(std::uint32_t address) override {
        reads.push_back(address);
        if (address & 0x800000) return word(copro, std::size_t(address & 0x1fffff) * 4);
        if (address & 0x400000) return buffer[address & 0x7fff];
        throw rt::TgpFatal("unexpected unmapped count-probe read");
    }
    void mem_w(std::uint32_t address, std::uint32_t value) override {
        if (!(address & 0x400000) || (address & 0x800000))
            throw rt::TgpFatal("unexpected count-probe write");
        buffer[address & 0x7fff] = value;
    }
};
void run(rt::Tgp &tgp, Bus &bus, std::uint32_t command) {
    bus.input.push_back(command);
    const auto bound = tgp.count + 10000;
    rt::tgpgen::run(tgp, bound);
    if (tgp.count >= bound || bus.consumed != bus.input.size())
        throw rt::TgpFatal("count-probe instruction/input bound failed");
}
} // namespace


#include "contact_projection.h"
struct Candidate { std::array<ps2::native::ContactPointXZ,4> vertices; ps2::native::ContactPointXZ point; unsigned count=0; };
Candidate candidate;
unsigned candidates=0,hits=0;
std::uint32_t final_cross=0;
std::uint64_t edge_hash=14695981039346656037ull;
void observe(rt::Tgp &t,std::uint16_t pc){
    if(pc==0x5da){
        candidate.point={rt::Tgp::u2f(t.data[0x45]),rt::Tgp::u2f(t.data[0x47])};
        for(unsigned i=0;i<4;++i)candidate.vertices[i]={rt::Tgp::u2f(t.data[0x256+3*i]),rt::Tgp::u2f(t.data[0x258+3*i])};
        candidate.count=0;
    }
    if(pc==0x5f5)candidate.count=t.d?4:3;
    if(pc==0x60e||pc==0x626)final_cross=t.b;
    if(pc==0x630||pc==0x632){
        if(candidate.count!=3 && candidate.count!=4)throw rt::TgpFatal("missing candidate boundary");
        ps2::native::ContactProjection result;
        if(!ps2::native::contact_projection(std::span(candidate.vertices.data(),candidate.count),candidate.point,result))
            throw rt::TgpFatal("actual projected candidate outside native domain");
        const std::array<std::uint32_t,4> expected{t.data[0x40],t.data[0x41],candidate.count==4?t.data[0x42]:final_cross,candidate.count==4?final_cross:0};
        for(unsigned i=0;i<4;++i){
            if(expected[i]!=rt::Tgp::f2u(result.edge_cross[i]))throw rt::TgpFatal("actual candidate determinant mismatch");
            edge_hash=(edge_hash^expected[i])*1099511628211ull;
        }
        if(result.accepted!=(pc==0x630))throw rt::TgpFatal("actual candidate acceptance mismatch");
        edge_hash=(edge_hash^result.accepted)*1099511628211ull;++candidates;hits+=result.accepted;
        candidate.count=0;
    }
}
int main(int argc,char **argv){
    if(argc!=7)return 2;
    try{
        const auto program=read(argv[1]),firmware=read(argv[2]),tables=read(argv[3]),copro=read(argv[4]),inputs=read(argv[5]);
        if(program.size()!=2097152||firmware.size()!=6660||tables.size()!=262144||copro.size()!=8388608||inputs.empty()||inputs.size()%92||inputs.size()>4096*92)
            throw rt::TgpFatal("invalid private query input sizes");
        std::array<std::uint32_t,0x10000> decoded{};
        for(unsigned i=0;i<decoded.size();++i)decoded[i]=word(tables,4*i);
        std::ofstream output(argv[6],std::ios::binary);if(!output)throw rt::TgpFatal("cannot open private query output");
        std::uint64_t query_steps=0,reply_steps=0,result_hash=14695981039346656037ull;
        const unsigned queries=inputs.size()/92;
        for(unsigned row=0;row<queries;++row){
            Bus bus(copro);rt::Tgp tgp;tgp.bus=&bus;tgp.tables=decoded.data();
            for(unsigned i=0;i<firmware.size()/4;++i)tgp.prog[i]=word(firmware,4*i);
            tgp.reset();run(tgp,bus,0x04000000);
            if(bus.output!=std::vector<std::uint32_t>{8})throw rt::TgpFatal("unexpected firmware startup");
            const auto selector=word(inputs,row*92),index=word(inputs,row*92+4),slot=word(inputs,row*92+8);
            if(selector>=8||slot!=0)throw rt::TgpFatal("query selector/slot outside inspected domain");
            bus.buffer[0x6000]=word(program,0x3dac0+12*selector);run(tgp,bus,0x03800707);
            const auto count=bus.output.back();bus.output.clear();
            if(!count||count>65535||index>=count||index>32767)throw rt::TgpFatal("query index out of native course range");
            const auto descriptor=word(program,0x3dac4+12*selector);
            std::vector<std::vector<ps2::native::ContactPolygon>> polygons(count);
            std::vector<ps2::native::ContactSegment> segments(count);
            for(unsigned s=0;s<count;++s){
                const auto record=(std::size_t(descriptor)+4*s)*4;
                const auto size=word(copro,record),vertices=word(copro,record+4),attributes=word(copro,record+8);
                if(!size||size>255||(vertices&3)||(attributes&3))throw rt::TgpFatal("invalid typed asset record");
                polygons[s].resize(size);
                for(unsigned p=0;p<size;++p){
                    auto &polygon=polygons[s][p];
                    for(unsigned i=0;i<4;++i){
                        const auto offset=std::size_t(vertices)+64*p+12*i;
                        polygon.vertices[i]={rt::Tgp::u2f(word(copro,offset)),rt::Tgp::u2f(word(copro,offset+4)),rt::Tgp::u2f(word(copro,offset+8))};
                        polygon.coefficients[i]=rt::Tgp::u2f(word(copro,std::size_t(vertices)+64*p+48+4*i));
                    }
                    polygon.attributes=word(copro,std::size_t(attributes)+4*p);
                }
                segments[s].polygons=polygons[s];
            }
            const ps2::native::ContactVertex point{rt::Tgp::u2f(word(inputs,row*92+12)),rt::Tgp::u2f(word(inputs,row*92+16)),rt::Tgp::u2f(word(inputs,row*92+20))};
            ps2::native::ContactSearchResult native;
            if(!ps2::native::search_course_contact(segments,std::int16_t(index),point,native))
                throw rt::TgpFatal("native full query rejected captured input");
            for(unsigned i=0;i<3;++i)bus.buffer[0x6001+i]=word(inputs,row*92+12+i*4);
            bus.buffer[0x6004]=word(program,0x3dac4+12*selector);bus.buffer[0x6005]=count*4;
            bus.buffer[0x6006]=(index?index-1:count-1)*4;bus.buffer[0x6007]=index*4;
            bus.buffer[0x6008]=(index+1==count?0:index+1)*4;bus.buffer[0x6009]=slot*2;
            tgp.hook=observe;const auto before=tgp.count;run(tgp,bus,0x29005252);query_steps+=tgp.count-before;
            if(!bus.output.empty())throw rt::TgpFatal("search unexpectedly emitted FIFO result");
            if(native.matched){
                const auto record=(std::size_t(descriptor)+4*native.segment)*4;
                const auto vertex_pointer=0xff800000u|(word(copro,record+4)/4+native.polygon*16);
                const auto attribute_pointer=0xff800000u|(word(copro,record+8)/4+native.polygon);
                if(tgp.data[0x68]!=attribute_pointer||tgp.data[0x69]!=vertex_pointer)
                    throw rt::TgpFatal("native selected polygon differs from original pointers");
            }else if(tgp.data[0x68]||tgp.data[0x69])throw rt::TgpFatal("native miss differs from original pointers");
            bus.input.push_back(0x29805353);bus.input.push_back(slot*2);const auto reply_before=tgp.count;
            const auto limit=tgp.count+10000;rt::tgpgen::run(tgp,limit);reply_steps+=tgp.count-reply_before;
            if(tgp.count>=limit||bus.consumed!=bus.input.size()||bus.output.size()!=17)
                throw rt::TgpFatal("reply did not complete with 17 words");
            const auto native_payload=contact_search_cases::payload(native.data);
            for(unsigned i=0;i<17;++i){
                const auto value=bus.output[i];
                if(value!=native_payload[i])throw rt::TgpFatal("native complete payload differs from original reply");
                if(value!=word(inputs,row*92+24+4*i)){
                    std::fprintf(stderr,"QUERY REPLY MISMATCH row=%u word=%u expected=%08x actual=%08x\n",row,i,word(inputs,row*92+24+4*i),value);
                    return 1;
                }
                for(unsigned j=0;j<4;++j)output.put(char(value>>(8*j)));
                result_hash=(result_hash^value)*1099511628211ull;
            }
        }
        if(!output)throw rt::TgpFatal("cannot finish private query output");
        std::printf("CONTACT_QUERY_ORACLE PASS queries=%u native_searches=%u reply_words=%u candidates=%u accepted=%u search_instructions=%llu reply_instructions=%llu result_hash=%016llx edge_hash=%016llx\n",queries,queries,queries*17,candidates,hits,(unsigned long long)query_steps,(unsigned long long)reply_steps,(unsigned long long)result_hash,(unsigned long long)edge_hash);
    }catch(const std::exception&e){std::fprintf(stderr,"contact query oracle failed: %s\n",e.what());return 1;}
}
