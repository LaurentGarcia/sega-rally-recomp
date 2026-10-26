#include "contact_point_preparation_cases.h"
#include <cstdio>
#ifndef _EE
#include <cfenv>
#include <cmath>
#else
#include <kernel.h>
#endif
namespace {
namespace cases=contact_point_cases;
using namespace ps2::native;
#ifndef _EE
float add(float a,float b){volatile float v=a+b;return v;}
float mul(float a,float b){volatile float v=a*b;return v;}
float polynomial(float angle){
    const float pi=std::bit_cast<float>(0x40490fdbu),half=std::bit_cast<float>(0x3fc90fdbu);
    const unsigned n=unsigned(std::floor(mul(std::fabs(angle),std::bit_cast<float>(0x3ea2f983u))));
    float x=add(add(std::fabs(angle),-mul(float(n),pi)),-half),power[6];power[0]=mul(x,x);
    for(unsigned i=1;i<6;++i)power[i]=mul(power[0],power[i-1]);
    constexpr std::array<std::uint32_t,6> coefficient{0x3f000000,0x3d2aaaab,0x3ab60b61,0x37d00d01,0x3493f27e,0x310f76c7};
    float v=mul(power[5],std::bit_cast<float>(coefficient[5]));
    for(int i=4;i>=0;--i){float term=mul(power[i],std::bit_cast<float>(coefficient[i]));v=add(v,(i&1)?term:-term);}
    v=add(v,1.0f);return (n+unsigned(angle<=0))%2?-v:v;
}
ContactPointPreparation reference(const cases::Case&input){
    float m[3][3]{{1,0,0},{0,1,0},{0,0,1}};
    for(unsigned stage=0;stage<3;++stage){
        const unsigned index=stage==0?1:stage==1?0:2,a=stage==1?1:0,b=stage==2?1:2;
        const float angle=std::bit_cast<float>(input[index]),s=polynomial(angle),c=polynomial(add(angle,std::bit_cast<float>(0x3fc90fdbu)));
        for(unsigned row=0;row<3;++row){const float old_a=m[a][row],old_b=m[b][row];
            m[a][row]=add(mul(c,old_a),stage==0?mul(s,old_b):-mul(s,old_b));
            m[b][row]=stage==0?add(mul(c,old_b),-mul(s,old_a)):add(mul(s,old_a),mul(c,old_b));}
    }
    ContactPointPreparation out;for(unsigned i=0;i<3;++i)out.matrix.columns[i]={m[i][0],m[i][1],m[i][2]};
    for(unsigned i=0;i<4;++i){float offset[3],point[3];for(unsigned axis=0;axis<3;++axis){float v=0;for(unsigned column=0;column<3;++column)v=add(v,mul(m[column][axis],std::bit_cast<float>(input[6+3*i+column])));offset[axis]=v;point[axis]=add(v,std::bit_cast<float>(input[3+axis]));}out.offsets[i]={offset[0],offset[1],offset[2]};out.points[i]={point[0],point[1],point[2]};}
    return out;
}
#endif
bool run(bool extended){unsigned count=0;std::uint64_t hash=14695981039346656037ull;
    if(!cases::each([&](const auto&c){ContactPointPreparation r;
        if(!prepare_contact_points(cases::point(c,0),cases::point(c,3),cases::local(c),r))return false;
        const auto actual=cases::words(r);
#ifndef _EE
        const auto expected=cases::words(reference(c));if(actual!=expected){for(unsigned j=0;j<36;++j)if(actual[j]!=expected[j])std::printf("POINT MISMATCH case=%u lane=%u actual=%08lx expected=%08lx\n",count,j,(unsigned long)actual[j],(unsigned long)expected[j]);return false;}
#endif
        ContactPointPreparation alias;alias.matrix.columns[0]=cases::point(c,0);alias.matrix.translation=cases::point(c,3);alias.points=cases::local(c);
        if(!prepare_contact_points(alias.matrix.columns[0],alias.matrix.translation,alias.points,alias)||cases::words(alias)!=actual)return false;
        cases::hash(hash,c,actual);++count;return true;},extended))return false;
    if(count!=(extended?49295u:3279u)||hash!=(extended?0xf05d0d03253121d8ull:0xef519d24a3f602faull))return false;
    std::printf("CONTACT_POINT_%s PASS cases=%u hash=%016llx aliases=3\n",extended?"HOST":"PROBE",count,(unsigned long long)hash);return true;
}
}
int main(){
#ifndef _EE
    if(std::fegetround()!=FE_TONEAREST)return 1;
#endif
    if(!run(false))return 1;
#ifndef _EE
    if(!run(true))return 1;
#endif
    unsigned rejects=0;
    for(unsigned lane=0;lane<18;++lane)for(auto bad:{0x7f800000u,0xff800000u,0x7fc00000u,0xff800001u,0x49800001u,0xc9800001u}){
        auto c=cases::base();c[lane]=bad;ContactPointPreparation out;out.points[3].z=std::bit_cast<float>(0x7fc12345u);const auto before=cases::words(out);
        if(prepare_contact_points(cases::point(c,0),cases::point(c,3),cases::local(c),out)||cases::words(out)!=before)return 1;++rejects;}
    for(unsigned lane=0;lane<3;++lane)for(auto bad:{0x41000001u,0xc1000001u}){auto c=cases::base();c[lane]=bad;ContactPointPreparation out;const auto before=cases::words(out);if(prepare_contact_points(cases::point(c,0),cases::point(c,3),cases::local(c),out)||cases::words(out)!=before)return 1;++rejects;}
    std::printf("CONTACT_POINT_REJECTION PASS cases=%u\n",rejects);std::fflush(stdout);
#ifdef _EE
    SleepThread();
#endif
    return 0;
}
