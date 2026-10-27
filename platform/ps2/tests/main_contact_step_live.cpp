// Offline native composition versus recorded original main-car boundaries.
// Private course/normal/local-point data are supplied by the caller.
#include "main_contact_step_cases.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace {
using namespace ps2::native;
namespace cases=main_contact_step_cases;
using Bytes=std::vector<std::uint8_t>;
Bytes read(const char* path) {
    std::ifstream stream(path,std::ios::binary);
    if(!stream)throw std::runtime_error("cannot open private input");
    return {std::istreambuf_iterator<char>(stream),{}};
}
std::uint32_t word(const Bytes& bytes,std::size_t offset) {
    return bytes.at(offset)|(std::uint32_t(bytes.at(offset+1))<<8)|
        (std::uint32_t(bytes.at(offset+2))<<16)|(std::uint32_t(bytes.at(offset+3))<<24);
}
struct Course {
    std::vector<std::vector<ContactPolygon>> polygons;
    std::vector<ContactSegment> segments;
    Course(const Bytes& program,const Bytes& copro,std::uint32_t selector) {
        if(selector>=8)throw std::runtime_error("course selector outside recovered domain");
        const auto count=word(copro,std::size_t(word(program,0x3dac0+12*selector))*4);
        const auto descriptor=word(program,0x3dac4+12*selector);
        if(!count||count>65535)throw std::runtime_error("invalid native course count");
        polygons.resize(count);segments.resize(count);
        for(unsigned s=0;s<count;++s) {
            const auto record=(std::size_t(descriptor)+4*s)*4;
            const auto count_here=word(copro,record),vertices=word(copro,record+4),attributes=word(copro,record+8);
            if(!count_here||count_here>255||(vertices&3)||(attributes&3))throw std::runtime_error("invalid native polygon record");
            polygons[s].resize(count_here);
            for(unsigned p=0;p<count_here;++p) {
                auto& polygon=polygons[s][p];
                for(unsigned j=0;j<4;++j) {
                    const auto start=std::size_t(vertices)+64*p+12*j;
                    polygon.vertices[j]={cases::value(word(copro,start)),cases::value(word(copro,start+4)),cases::value(word(copro,start+8))};
                    polygon.coefficients[j]=cases::value(word(copro,std::size_t(vertices)+64*p+48+4*j));
                }
                polygon.attributes=word(copro,std::size_t(attributes)+4*p);
            }
            segments[s].polygons=polygons[s];
        }
    }
};
}

int main(int argc,char** argv) {try {
    if(argc!=5)return 2;
    const auto program=read(argv[1]),tables=read(argv[2]),copro=read(argv[3]),records=read(argv[4]);
    constexpr std::array<std::uint8_t,8> magic{'M','A','I','N','S','T','P','1'};
    if(program.size()!=2097152||tables.size()!=262144||copro.size()!=8388608||records.size()<12||
       !std::equal(magic.begin(),magic.end(),records.begin()))throw std::runtime_error("invalid private asset or record size");
    const auto count=word(records,8);
    if(!count||count>32||records.size()!=12ull+440ull*count)throw std::runtime_error("invalid main contact-step record count");
    std::array<ContactNormalPair,8192> normals{};
    for(unsigned i=0;i<normals.size();++i)normals[i]={cases::value(word(tables,4*(0xc000+2*i))&0x7fffffffu),cases::value(word(tables,4*(0xc001+2*i)))};
    std::uint64_t hash=14695981039346656037ull;unsigned active=0;
    for(unsigned row=0;row<count;++row) {
        const auto raw=[&](unsigned column){return word(records,12+440*row+4*column);};
        const auto value=[&](unsigned column){return cases::value(raw(column));};
        Course course(program,copro,raw(104));MainContactStepInput input;
        if(raw(105)>32767||raw(105)>=course.segments.size()||raw(109)>1)throw std::runtime_error("main contact-step inputs exceed recovered domain");
        input.course=course.segments;input.normal_table=normals;input.segment=std::int16_t(raw(105));
        input.response_parameter=value(106);input.gain_parameter=value(107);input.scalar_divisor=value(108);input.feedback_enabled=raw(109)!=0;
        for(unsigned i=0;i<4;++i)input.local_points[i]={value(92+3*i),value(93+3*i),value(94+3*i)};
        cases::Words before{},expected{};
        for(unsigned i=0;i<46;++i){before[i]=raw(i);expected[i]=raw(46+i);}
        auto state=cases::state(before);
        if(!resolve_main_contact_step(input,state)) {
            std::fprintf(stderr,"main contact step rejected row=%u\n",row);
            throw std::runtime_error("native composed step rejected captured input");
        }
        const auto actual=cases::words(state);
        for(unsigned i=0;i<actual.size();++i)if(actual[i]!=expected[i]) {
            std::fprintf(stderr,"main contact step mismatch row=%u field=%u expected=%08x actual=%08x\n",row,i,expected[i],actual[i]);
            throw std::runtime_error("native composed state differs from original boundaries");
        }
        cases::hash(hash,actual);active+=(cases::bits(state.selected_scalar)&0x7fffffffu)!=0;
    }
    std::printf("MAIN_CONTACT_STEP_LIVE PASS steps=%u contacts=%u active_steps=%u state_words=%u hash=%016llx\n",count,count*4,active,count*46,(unsigned long long)hash);
}catch(const std::exception& error){std::fprintf(stderr,"main contact step comparison failed: %s\n",error.what());return 1;}return 0;}
