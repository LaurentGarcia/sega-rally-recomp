#include "vu1_transform.h"
#include "vu0_pack.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#ifdef _EE
#include <dma.h>
#include <dma_registers.h>
#endif
namespace {
using namespace ps2;
unsigned checks=0,accepted=0,fallbacks=0,vertices=0;
double maximum_error=0,maximum_pixels=0,maximum_depth=0;
std::uint32_t seed=0x14295ab3;
std::uint32_t next(){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
bool check(bool value,const char*what){++checks;if(!value)std::printf("VU1 ENGINE FAIL %s check=%u\n",what,checks);return value;}
TransformMatrix identity(){TransformMatrix m{};m[0]=m[5]=m[10]=m[15]=1;return m;}
std::array<float,4> lanes(const TransformVector4&v){return std::bit_cast<std::array<float,4>>(v);}
// Host diagnostic only: independently ordered binary32 multiply/add with
// contraction disabled. It tests the enclosure; it is not a VU emulator.
#ifndef _EE
void host_float(const TransformMatrix&a,const TransformMatrix&b,std::span<const TransformVector4>in,
                std::span<TransformVector4>out){
    for(std::size_t i=0;i<in.size();++i){const auto p=lanes(in[i]);std::array<float,4>w{},c{};
        for(unsigned r=0;r<4;++r){w[r]=a[r]*p[0];for(unsigned j=1;j<4;++j)w[r]=w[r]+a[j*4+r]*p[j];}
        for(unsigned r=0;r<4;++r){c[r]=b[r]*w[0];for(unsigned j=1;j<4;++j)c[r]=c[r]+b[j*4+r]*w[j];}
        out[i]=std::bit_cast<TransformVector4>(c);}
}
#endif
#ifdef _EE
bool projection_contract(Vu1Transform&backend){
    static_assert(sizeof(TransformVector4)==16 && sizeof(TransformProjectedVertex)==16);
    const auto id=identity();
    std::array<TransformVector4,97> input{},clip{},plain{};
    std::array<TransformProjectedVertex,97> projected{};
    const TransformVector4 cs{17,18,19,20};
    const TransformProjectedVertex ps{21,22,23,24};
    const auto reset=[&](){clip.fill(cs);projected.fill(ps);};
    const auto unchanged=[&](){for(unsigned i=0;i<97;++i){
        if(std::memcmp(&clip[i],&cs,16)||std::memcmp(&projected[i],&ps,16))return false;}
        return true;};
    auto before=backend.stats().dma_qwords;reset();
    if(!check(backend.transform_project_native(id,id,{},{},{})&&backend.stats().dma_qwords==before,
              "empty projected batch performs no transfer")||!check(unchanged(),"empty projected unchanged"))return false;
    unsigned inside_count=0,outside_count=0;
    double max_pixel=0,max_depth=0,max_q=0;
    // Each matrix emits an authored clip vector independently of the input.
    // Both sides of every plane, exact corners and reciprocal extremes are
    // covered. In particular zero w must never admit the saturated VU DIV.
    std::array<TransformVector4,101> cases{};unsigned n=0;
    cases[n++]={0,0,0,0};cases[n++]={0,0,0,-1};
    for(float w:std::array<float,5>{0x1p-10f,.5f,1,7,65536}){
        cases[n++]={0,0,w*.5f,w};
        for(unsigned axis=0;axis<3;++axis)for(unsigned side=0;side<2;++side){
            TransformVector4 v{0,0,w*.5f,w};
            auto values=lanes(v);values[axis]=axis==2?(side?w:0):(side?w:-w);
            cases[n++]=std::bit_cast<TransformVector4>(values);
            const auto edge=std::bit_cast<std::uint32_t>(values[axis]);
            values[axis]=std::bit_cast<float>(edge+1u);
            if(axis==2&&!side)values[axis]=-0x1p-20f;
            cases[n++]=std::bit_cast<TransformVector4>(values);
        }
        cases[n++]={-w,-w,0,w};cases[n++]={w,w,w,w};
    }
    cases[n++]={0,0,0,std::bit_cast<float>(std::uint32_t(0x3a7fffff))};
    cases[n++]={0,0,0,std::bit_cast<float>(std::uint32_t(0x3a800001))};
    while(n<cases.size()){const float w=float(n-70);cases[n++]={w*.25f,-w*.375f,w*.625f,w};}
    for(const auto c:cases){
        auto b=TransformMatrix{};b[12]=c.x;b[13]=c.y;b[14]=c.z;b[15]=c.w;
        input[0]={0,0,0,1};reset();
        // 65536 outward-neighbor tests exceed the API input bound and must
        // reject both output spans before submitting anything.
        if(!valid_native_vu1_transform_input(id,b,{input.data(),1})){
            before=backend.stats().dma_qwords;
            if(!check(!backend.transform_project_native(id,b,{input.data(),1},clip,projected)&&
                      unchanged()&&backend.stats().dma_qwords==before,"projected bounded matrix rejection"))return false;
            continue;
        }
        if(!check(backend.transform_native(id,b,{input.data(),1},plain),"projection baseline")||
           !check(backend.transform_project_native(id,b,{input.data(),1},clip,projected),backend.error())||
           !check(std::memcmp(plain.data(),clip.data(),16)==0,"projected clip preserves original kernel"))return false;
        const auto v=clip[0];const auto p=projected[0];
        const bool inside=v.w>=0x1p-10f&&v.x>=-v.w&&v.x<=v.w&&v.y>=-v.w&&v.y<=v.w&&v.z>=0&&v.z<=v.w;
        if(!inside){++outside_count;const TransformProjectedVertex zero{};
            if(!check(std::memcmp(&p,&zero,16)==0,"outside all-zero projection including zero w"))return false;
        }else{++inside_count;
            const double q=1.0/double(v.w);
            const double pixel=std::max(std::abs(double(p.x)-(1+double(v.x)*q)*248),
                                        std::abs(double(p.y)-(1-double(v.y)*q)*192));
            const double depth=std::abs(double(p.depth)-(1-double(v.z)*q));
            const double qe=std::abs(double(p.q)-q)/q;
            max_pixel=std::max(max_pixel,pixel);max_depth=std::max(max_depth,depth);max_q=std::max(max_q,qe);
            if(!check(p.q>0&&p.x>=0&&p.x<=496&&p.y>=0&&p.y<=384&&p.depth>=0&&p.depth<=1&&
                      pixel<=1.0/32&&depth<=0x1p-20&&qe<=0x1p-20,"projection boundary numerical budget"))return false;
        }
        if(!check(std::memcmp(&clip[1],&cs,16)==0&&std::memcmp(&projected[1],&ps,16)==0,"projected output prefix only"))return false;
    }
    for(unsigned count:std::array<unsigned,4>{0,1,95,96}){
        for(unsigned i=0;i<count;++i)input[i]={float(i%9)/8-.5f,float(i%7)/8-.25f,.5f,1};
        reset();
        if(!check(backend.transform_project_native(id,id,{input.data(),count},clip,projected),"projected count boundary")||
           !check(std::memcmp(&clip[count],&cs,16)==0&&std::memcmp(&projected[count],&ps,16)==0,"projected count preserves tail"))return false;
        for(unsigned i=0;i<count;++i)if(!check(std::memcmp(&clip[i],&input[i],16)==0&&projected[i].q==1,"projected count output"))return false;
    }
    reset();input[0]={.25f,-.5f,.75f,1};before=backend.stats().dma_qwords;
    alignas(16) std::array<unsigned char,32> misaligned{};
    if(!check(!backend.transform_project_native(id,id,{input.data(),1},{clip.data(),0},projected)&&unchanged(),"short clip rejection")||
       !check(!backend.transform_project_native(id,id,{input.data(),1},clip,{projected.data(),0})&&unchanged(),"short projected rejection")||
       !check(!backend.transform_project_native(id,id,input,clip,projected)&&unchanged(),"oversized projected rejection")||
       !check(!backend.transform_project_native(id,id,{input.data(),1},clip,
             {reinterpret_cast<TransformProjectedVertex*>(input.data()),1})&&input[0].x==.25f&&unchanged(),"input projected overlap rejection")||
       !check(!backend.transform_project_native(id,id,{input.data(),1},clip,
             {reinterpret_cast<TransformProjectedVertex*>(clip.data()),1})&&unchanged(),"clip projected overlap rejection")||
       !check(!backend.transform_project_native(id,id,{input.data(),1},clip,
             {reinterpret_cast<TransformProjectedVertex*>(misaligned.data()+4),1})&&unchanged(),"projected alignment rejection")||
       !check(backend.stats().dma_qwords==before,"invalid projected requests perform no DMA"))return false;
    for(std::uint32_t bits:std::array<std::uint32_t,5>{1,0x7f800000,0x7fc00000,0x47800001,0xff800000}){
        input[0].x=std::bit_cast<float>(bits);
        if(!check(!backend.transform_project_native(id,id,{input.data(),1},clip,projected)&&unchanged()&&
                  backend.stats().dma_qwords==before,"invalid projected encoding atomic before DMA"))return false;
    }
    input[0]={0,0,.5f,1};input[1]={2,0,.5f,1};auto large=id;large[0]=65536;
    if(!check(!backend.transform_project_native(large,id,{input.data(),2},clip,projected)&&unchanged()&&backend.available(),
              "late clip failure preserves both outputs and lease"))return false;
    input[0]={.25f,-.5f,.75f,1};
    if(!check(backend.transform_project_native(id,id,{input.data(),1},{input.data(),1},projected)&&input[0].z==.75f&&
              projected[0].x==310&&projected[0].y==288&&projected[0].depth==.25f,"projected in-place clip after rejection")||
       !check(backend.transform_native(id,id,{input.data(),1},clip)&&clip[0].z==.75f,"entry zero survives lazy projection upload"))return false;
    std::printf("VU1 PROJECTED PASS cases=%u inside=%u outside=%u max_pixels=%.9g max_depth=%.9g max_q_relative=%.9g persistent=%u\n",
        n,inside_count,outside_count,max_pixel,max_depth,max_q,unsigned(backend.stats().persistent_bytes));
    return true;
}
#endif
bool run(){
    Vu1Transform backend;
#ifdef _EE
    dma_channel_initialize(DMA_CHANNEL_VIF1,nullptr,0);
    const auto dma_before=*DMA_REG_CTRL;*DMA_REG_CTRL=dma_before|1u;asm volatile("sync.l":::"memory");
    std::printf("VU1 ENGINE DMA_SETUP before=%08lx after=%08lx\n",(unsigned long)dma_before,(unsigned long)*DMA_REG_CTRL);
    if(!check(*DMA_REG_CTRL==(dma_before|1u),"global DMA enable"))return false;
    if(!check(backend.initialize(),backend.error())||!check(backend.initialize(),"idempotent initialize"))return false;
    Vu1Transform other;if(!check(!other.initialize(),"second owner rejection"))return false;
#else
    if(!check(!backend.initialize()&&!backend.available(),"host hardware unavailable"))return false;
#endif
    std::array<TransformVector4,96>input{},output{},reference{};
#ifdef _EE
    std::array<TransformVector4,96>native_output{};
#endif
    for(unsigned batch=0;batch<160;++batch){
        auto a=identity(),b=identity();
        const unsigned count=batch<4?std::array<unsigned,4>{0,1,95,96}[batch]:96;
        if(batch%4==0){a[0]=0;a[1]=1;a[4]=-1;a[5]=0;a[12]=370;a[14]=-293;
            b[0]=1.f/7;b[5]=1.f/5.42f;b[10]=1.f/100;b[12]=-370.f/7;b[14]=293.f/100+.5f;}
        else if(batch%4==1){a[0]=.9238795f;a[2]=-.3826834f;a[8]=.3826834f;a[10]=.9238795f;
            a[12]=-1024.125f;a[13]=32.25f;a[14]=511.75f;
            b[0]=.125f;b[5]=.125f;b[10]=.015625f;b[12]=128.015625f;b[13]=-4.03125f;b[14]=-7.49609375f;}
        else if(batch%4==2){b[0]=.5f;b[5]=.5f;b[10]=.25f;b[11]=.0625f;b[14]=.5f;b[15]=1;}
        else{a[0]=2;a[5]=.5f;a[12]=1;b[0]=.125f;b[5]=.25f;b[10]=.0625f;b[14]=.5f;}
        for(unsigned i=0;i<count;++i)input[i]={float(int(next()%8193)-4096)/2048,
            float(int(next()%8193)-4096)/2048,float(int(next()%8193)-4096)/2048,1};
        TransformErrorBound bound;
        if(!check(valid_native_vu1_transform_input(a,b,{input.data(),count}),"native input domain"))return false;
        if(!check(bound_vu1_transform(a,b,{input.data(),count},bound),"input bound")||
           !check(transform_reference(a,b,{input.data(),count},{reference.data(),count}),"reference"))return false;
        PreparedProjectionGuard plan;if(!check(prepare_vu1_projection_guard(bound,{},plan),"prepare guard"))return false;
#ifdef _EE
        if(!check(backend.transform(a,b,{input.data(),count},{output.data(),count},bound),backend.error()))return false;
        if(!check(backend.transform_native(a,b,{input.data(),count},{native_output.data(),count}),"native raw transform")||
           !check(std::memcmp(output.data(),native_output.data(),count*16)==0,"native raw identical"))return false;
#else
        host_float(a,b,{input.data(),count},{output.data(),count});
#endif
        for(unsigned i=0;i<count;++i){++vertices;auto got=lanes(output[i]),want=lanes(reference[i]);
            for(unsigned c=0;c<4;++c){const double error=std::abs(double(got[c])-want[c]);maximum_error=std::max(maximum_error,error);
                if(!check(error<=bound.absolute[c],"arithmetic enclosure"))return false;}
            TransformProjectionError estimate;
            const auto original=guard_vu1_projection(output[i],bound,{},&estimate);
            TransformProjectionError prepared_estimate;const auto prepared=guard_prepared_vu1_projection(output[i],plan,&prepared_estimate);
            if(!check(prepared==original,"prepared authored acceptance"))return false;
            if(original==TransformGuard::accepted){
                ++accepted;
                const double pixels=std::max(248*std::abs(double(output[i].x)/output[i].w-double(reference[i].x)/reference[i].w),
                    192*std::abs(double(output[i].y)/output[i].w-double(reference[i].y)/reference[i].w));
                const double depth=std::abs(double(output[i].z)/output[i].w-double(reference[i].z)/reference[i].w);
                maximum_pixels=std::max(maximum_pixels,pixels);maximum_depth=std::max(maximum_depth,depth);
                if(!check(pixels<=estimate.pixels && depth<=estimate.depth,"projected enclosure"))return false;
                if(!check(pixels<=prepared_estimate.pixels && depth<=prepared_estimate.depth,"prepared projected enclosure"))return false;
            }else ++fallbacks;
        }
        // The real VU0 kernel executes after each production VU1 fence. Its
        // independent integer expected values check cross-unit preservation.
        GsCoordinates xy{{2048.125f,2047.9375f,0.f,4095.9375f}};GsFixedCoordinates packed{};
        quantize_gs_xy_vu0(&xy,&packed,1);
        if(!check(packed.xy[0]==32770 && packed.xy[1]==32767 && packed.xy[2]==0 && packed.xy[3]==65535,"VU0 after VU1"))return false;
    }
    auto a=identity(),b=identity();TransformErrorBound bound{{1,2,3,4}},old=bound;
    input[0]={0,0,.5f,1};output[0]={17,18,19,20};const auto unchanged=output[0];
    input[0].x=std::numeric_limits<float>::denorm_min();
    if(!check(!bound_vu1_transform(a,b,{input.data(),1},bound)&&bound.absolute==old.absolute,"subnormal atomic bound rejection")||
       !check(!transform_reference(a,b,{input.data(),1},{output.data(),1})&&std::memcmp(&unchanged,&output[0],16)==0,"reference atomic rejection"))return false;
#ifdef _EE
    if(!check(!backend.transform(a,b,{input.data(),1},{output.data(),1},bound)&&std::memcmp(&unchanged,&output[0],16)==0,"hardware atomic input rejection"))return false;
#endif
    input[0]={0,0,.5f,1};
    if(!check(bound_vu1_transform(a,b,{input.data(),1},bound),"guard base"))return false;
    if(!check(guard_vu1_projection({1,0,.5f,1},bound,{})==TransformGuard::near_clip_plane,"on plane fallback")||
       !check(guard_vu1_projection({0,0,0,1},bound,{})==TransformGuard::near_clip_plane,"depth plane fallback")||
       !check(guard_vu1_projection({0,0,.5f,1},bound,{0,0})==TransformGuard::projection_budget,"zero budget fallback"))return false;
    for(const auto raw:std::array<std::uint32_t,8>{0x00000001u,0x007fffffu,0x80000001u,0x807fffffu,
            0x7f800000u,0xff800000u,0x7fc00000u,0x47800001u}){
        input[0]={std::bit_cast<float>(raw),0,.5f,1};bound=old;output[0]=unchanged;
        if(!check(!bound_vu1_transform(a,b,{input.data(),1},bound)&&bound.absolute==old.absolute,"invalid encoding bound"))return false;
        if(!check(!valid_native_vu1_transform_input(a,b,{input.data(),1}),"invalid native encoding"))return false;
#ifdef _EE
        if(!check(!backend.transform(a,b,{input.data(),1},{output.data(),1},bound)&&std::memcmp(&unchanged,&output[0],16)==0,"invalid encoding atomic hardware"))return false;
        const auto before=backend.stats().dma_qwords;
        if(!check(!backend.transform_native(a,b,{input.data(),1},{output.data(),1})&&std::memcmp(&unchanged,&output[0],16)==0&&backend.stats().dma_qwords==before,"native invalid before DMA"))return false;
#endif
    }
    // VU flush-to-zero is part of the enclosure, not silently treated as IEEE
    // subnormal arithmetic. The reference clipper itself rejects subnormals.
    input[0]={std::bit_cast<float>(std::uint32_t(0x00800000u)),0,.5f,1};b[0]=.5f;
    if(!check(bound_vu1_transform(a,b,{input.data(),1},bound),"underflow bound"))return false;
#ifdef _EE
    if(!check(backend.transform(a,b,{input.data(),1},{output.data(),1},bound),"underflow hardware"))return false;
    if(!check(std::abs(double(output[0].x)-0x1p-127)<=bound.absolute[0],"underflow enclosed"))return false;
#endif
    b=identity();
    // Native matrices permit large finite intermediate values even when the
    // reference mode declines its65k error enclosure; both stages remain far
    // below VU exponent overflow under the documented input limits.
    auto large=identity(),small=identity();large[0]=65536;small[0]=0x1p-32f;
    input[0]={65536,0,.5f,1};
    if(!check(valid_native_vu1_transform_input(large,small,{input.data(),1}),"large native intermediate domain")||
       !check(!bound_vu1_transform(large,small,{input.data(),1},bound),"reference rejects large enclosure"))return false;
#ifdef _EE
    if(!check(backend.transform_native(large,small,{input.data(),1},{output.data(),1})&&output[0].x==1,"large intermediate executes"))return false;
    input[0]={0,0,.5f,1};input[1]={2,0,.5f,1};output[0]=output[1]=unchanged;
    if(!check(!backend.transform_native(large,b,{input.data(),2},{output.data(),2})&&backend.available()&&
        std::memcmp(&unchanged,&output[0],16)==0&&std::memcmp(&unchanged,&output[1],16)==0,"late native output rejection atomic lease retained"))return false;
    if(!check(backend.transform_native(a,b,{input.data(),1},{output.data(),1})&&output[0].z==.5f,"native recovery after numerical rejection"))return false;
#endif
    // Original expression-order cancellation previously made a decrementing
    // threshold loop unbounded. Preparation is bounded and preserves rejection.
    PreparedProjectionGuard cancellation;
    TransformErrorBound cancel{{std::bit_cast<double>(std::uint64_t(0x3fdfffffffffffffull)),0,0,.5}};
    if(!check(prepare_vu1_projection_guard(cancel,{},cancellation),"cancellation preparation")||
       !check(guard_prepared_vu1_projection({0,0,.25f,1},cancellation)==guard_vu1_projection({0,0,.25f,1},cancel,{}),"cancellation decision"))return false;
    // Boundary/random guards check the implication required for a conservative
    // prepared plan: every accepted point passes the original scalar guard.
    for(unsigned trial=0;trial<512;++trial){
        TransformErrorBound e;for(double&x:e.absolute)x=double(next()%10000)*0x1p-28;
        TransformProjectionBudget budget{double(next()%1024)*0x1p-12,double(next()%1024)*0x1p-24};
        PreparedProjectionGuard plan;if(!check(prepare_vu1_projection_guard(e,budget,plan),"random prepare"))return false;
        for(unsigned j=0;j<64;++j){
            TransformVector4 p{float(int(next()%65537)-32768)/32768,float(int(next()%65537)-32768)/32768,float(next()%32769)/32768,1};
            if(j<8){const auto raw=0x3f800000u-j;p.x=std::bit_cast<float>(raw);}
            if(j==8)p.w=.5f;
            if(j==9)p.z=std::bit_cast<float>(std::uint32_t(1));
            TransformProjectionError before,after;const auto slow=guard_vu1_projection(p,e,budget,&before),fast=guard_prepared_vu1_projection(p,plan,&after);
            if(!check(fast!=TransformGuard::accepted||slow==TransformGuard::accepted,"prepared conservative acceptance"))return false;
            if(fast==TransformGuard::accepted && !check(after.pixels>=before.pixels&&after.depth>=before.depth,"prepared box bound"))return false;
        }
    }
    // Exact dyadic identity case and overlapping input/output publication.
    input[0]={.25f,-.5f,.75f,1};
    if(!check(transform_reference(a,b,{input.data(),1},{input.data(),1})&&input[0].x==.25f,"reference alias"))return false;
#ifdef _EE
    if(!check(backend.transform(a,b,{input.data(),1},{input.data(),1},bound)&&input[0].z==.75f,"hardware alias"))return false;
    if(!check(backend.transform_native(a,b,{input.data(),1},{input.data(),1})&&input[0].z==.75f,"native hardware alias"))return false;
#endif
    #ifdef _EE
    if(!projection_contract(backend))return false;
    #else
    std::array<TransformProjectedVertex,1> unavailable{{{21,22,23,24}}};
    output[0]=unchanged;
    if(!check(!backend.transform_project_native(a,b,{input.data(),1},{output.data(),1},unavailable)&&
        std::memcmp(&output[0],&unchanged,16)==0&&unavailable[0].q==24,"host projected API unavailable preserves outputs"))return false;
    #endif
    std::printf("VU1 ENGINE PASS checks=%u vertices=%u accepted=%u fallback=%u max_clip=%.9g max_pixels=%.9g max_depth=%.9g batches=%llu\n",
        checks,vertices,accepted,fallbacks,maximum_error,maximum_pixels,maximum_depth,
        static_cast<unsigned long long>(backend.stats().batches));return true;
}
}
int main(){return run()?0:1;}
