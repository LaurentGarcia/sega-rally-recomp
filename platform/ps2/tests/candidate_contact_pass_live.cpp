// Offline native composition versus recorded original main-car boundaries.
// Private course/normal/local-point data are supplied by the caller.
#include "candidate_contact_pass.h"
#include "contact_point_preparation.h"
#include "contact_pass_response_cases.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace {
using namespace ps2::native;
namespace cases=contact_pass_response_cases;
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
    constexpr std::array<std::uint8_t,8> magic{'C','A','N','D','P','A','S','1'};
    if(program.size()!=2097152||tables.size()!=262144||copro.size()!=8388608||records.size()<12||
       !std::equal(magic.begin(),magic.end(),records.begin()))throw std::runtime_error("invalid private asset or record size");
    const auto count=word(records,8);
    if(!count||count>32||records.size()!=12ull+396ull*count)throw std::runtime_error("invalid candidate pass record count");
    std::array<ContactNormalPair,8192> normals{};
    for(unsigned i=0;i<normals.size();++i)normals[i]={cases::value(word(tables,4*(0xc000+2*i))&0x7fffffffu),cases::value(word(tables,4*(0xc001+2*i)))};
    std::uint64_t hash=14695981039346656037ull;unsigned active=0;
    for(unsigned row=0;row<count;++row) {
        const auto raw=[&](unsigned column){return word(records,12+396*row+4*column);};
        const auto value=[&](unsigned column){return cases::value(raw(column));};
        Course course(program,copro,raw(77));CandidateContactPassInput input;
        if(raw(78)>32767||raw(78)>=course.segments.size()||raw(38)>1)throw std::runtime_error("candidate pass inputs exceed recovered domain");
        input.course=course.segments;input.normal_table=normals;input.segment=std::int16_t(raw(78));
        input.response_parameter=value(32);input.gain_parameter=value(33);input.scalar_divisor=value(34);
        input.bases={value(35),value(36),value(37)};input.feedback_enabled=raw(38)!=0;
        CandidateContactPassState state;
        for(unsigned i=0;i<3;++i) {
            state.response.car.motion[i]=value(39+i);state.response.car.candidate_position[i]=value(42+i);
            state.response.car.candidate_orientation[i]=value(45+i);
        }
        state.response.car.feedback={std::bit_cast<std::int32_t>(raw(48)),std::bit_cast<std::int32_t>(raw(49))};
        state.response.contact_mask=raw(50);state.response.selected_scalar=value(51);
        for(unsigned i=0;i<4;++i) {
            input.local_points[i]={value(65+3*i),value(66+3*i),value(67+3*i)};
            state.contacts[i]={{value(79+5*i),value(80+5*i),value(81+5*i)},value(82+5*i),raw(83+5*i)};
        }
        // Check the composition's preparation boundary as well as its outputs.
        ContactPointPreparation prepared;
        if(!prepare_contact_points({value(45),value(46),value(47)},{value(42),value(43),value(44)},input.local_points,prepared))
            throw std::runtime_error("native candidate preparation rejected captured input");
        for(unsigned i=0;i<4;++i) {
            const auto& p=prepared.offsets[i];const std::array<std::uint32_t,3> bits{cases::bits(p.x),cases::bits(p.y),cases::bits(p.z)};
            for(unsigned j=0;j<3;++j)if(bits[j]!=raw(20+3*i+j))throw std::runtime_error("native offset differs from captured preparation");
        }
        if(!resolve_candidate_contact_pass(input,state)) {
            std::fprintf(stderr,"candidate pass rejected row=%u\n",row);
            throw std::runtime_error("native complete pass rejected captured input");
        }
        const auto actual=cases::words(state.response);
        for(unsigned i=0;i<actual.size();++i)if(actual[i]!=raw(52+i)) {
            std::fprintf(stderr,"candidate state mismatch row=%u field=%u expected=%08x actual=%08x\n",row,i,raw(52+i),actual[i]);
            throw std::runtime_error("native complete state differs from original capture");
        }
        cases::hash(hash,actual);active+=(actual[12]&0x7fffffffu)!=0;
        for(unsigned i=0;i<4;++i) {
            const auto& c=state.contacts[i];const std::array<std::uint32_t,5> bits{cases::bits(c.direction[0]),cases::bits(c.direction[1]),cases::bits(c.direction[2]),cases::bits(c.gap),c.status};
            for(unsigned j=0;j<bits.size();++j) {
                if(bits[j]!=raw(5*i+j)) {
                    std::fprintf(stderr,"candidate contact mismatch row=%u slot=%u field=%u expected=%08x actual=%08x\n",row,i,j,raw(5*i+j),bits[j]);
                    throw std::runtime_error("native contact differs from original capture");
                }
                hash=(hash^bits[j])*1099511628211ull;
            }
        }
    }
    std::printf("CANDIDATE_CONTACT_PASS_LIVE PASS passes=%u contacts=%u active_passes=%u state_words=%u hash=%016llx\n",count,count*4,active,count*33,(unsigned long long)hash);
}catch(const std::exception& error){std::fprintf(stderr,"candidate pass comparison failed: %s\n",error.what());return 1;}return 0;}
