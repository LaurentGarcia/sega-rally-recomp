// Bounded numerical replay of privately supplied rendering inputs. No game
// data is embedded. The native game update is not linked into this fixture.
#include "vu1_transform.h"
#include "vu0_pack.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>
#ifdef _EE
#include <dma.h>
#include <dma_registers.h>
#endif
namespace {
using namespace ps2;
static_assert(std::endian::native==std::endian::little);
bool read(std::FILE*f,void*p,std::size_t bytes){return std::fread(p,1,bytes,f)==bytes;}
bool write(std::FILE*f,const void*p,std::size_t bytes){return std::fwrite(p,1,bytes,f)==bytes;}
// Diagnostic output contains privately supplied geometry, never fixture source.
// Consumers must require the completion footer and the separately sealed PASS.
struct Results {
    std::FILE* file=nullptr;
    std::array<char,16384> buffer{};
    ~Results(){if(file)std::fclose(file);}
    bool open(const char*path,std::uint32_t records,std::uint32_t poses){
        if(!path)return true;
        file=std::fopen(path,"wb");if(!file)return false;
        if(std::setvbuf(file,buffer.data(),_IOFBF,buffer.size()))return false;
        return write(file,"RLYVUO1\0",8)&&write(file,&records,4)&&write(file,&poses,4);
    }
    bool record(std::uint32_t frame,std::span<const TransformVector4> values){
        const auto count=static_cast<std::uint32_t>(values.size());
        return !file||(write(file,&frame,4)&&write(file,&count,4)&&write(file,values.data(),values.size_bytes()));
    }
    bool finish(std::uint32_t records){
        if(!file)return true;
        const bool wrote=write(file,"COMPLETE",8)&&write(file,&records,4)&&std::fflush(file)==0;
        auto*closing=file;file=nullptr;
        return std::fclose(closing)==0&&wrote;
    }
};
std::array<float,4> lanes(const TransformVector4&v){return std::bit_cast<std::array<float,4>>(v);}
#ifndef _EE
void host_float(const TransformMatrix&a,const TransformMatrix&b,std::span<const TransformVector4>in,std::span<TransformVector4>out){
    for(std::size_t i=0;i<in.size();++i){const auto p=lanes(in[i]);std::array<float,4>w{},c{};
        for(unsigned r=0;r<4;++r){w[r]=a[r]*p[0];for(unsigned j=1;j<4;++j)w[r]+=a[j*4+r]*p[j];}
        for(unsigned r=0;r<4;++r){c[r]=b[r]*w[0];for(unsigned j=1;j<4;++j)c[r]+=b[j*4+r]*w[j];}
        out[i]=std::bit_cast<TransformVector4>(c);}
}
#endif
int run(const char*path,const char*result_path){
    if(result_path&&std::strcmp(path,result_path)==0)return 15;
    auto*f=std::fopen(path,"rb");if(!f){std::printf("VU1 CORPUS FAIL open %s\n",path);return 1;}
    struct Close{std::FILE*f;~Close(){std::fclose(f);}}close{f};
    char magic[8];std::uint32_t records=0,poses=0;
    if(!read(f,magic,8)||std::memcmp(magic,"RLYVUC1\0",8)||!read(f,&records,4)||!read(f,&poses,4)||
       !records||records>100000||!poses||poses>601)return 2;
    Vu1Transform backend;
#ifdef _EE
    dma_channel_initialize(DMA_CHANNEL_VIF1,nullptr,0);
    const auto dma_before=*DMA_REG_CTRL;*DMA_REG_CTRL=dma_before|1u;asm volatile("sync.l":::"memory");
    std::printf("VU1 CORPUS DMA_SETUP before=%08lx after=%08lx\n",(unsigned long)dma_before,(unsigned long)*DMA_REG_CTRL);
    if(*DMA_REG_CTRL!=(dma_before|1u))return 3;
    if(!backend.initialize()){std::printf("VU1 CORPUS FAIL %s\n",backend.error());return 3;}
#endif
    Results results;if(!results.open(result_path,records,poses))return 15;
    std::uint64_t vertices=0,accepted=0,near=0,budget=0,unsupported=0,native_equal=0;
    double maximum_clip=0,maximum_pixels=0,maximum_depth=0,maximum_ratio=0;
    std::uint32_t prior_frame=0,seen_poses=0;
    alignas(16) std::array<TransformVector4,96>input{},reference{},actual{};
#ifdef _EE
    std::array<TransformVector4,96>guarded{};
#endif
#ifndef _EE
    std::array<TransformVector4,96>recalculated{};
#endif
    for(std::uint32_t record=0;record<records;++record){
        std::uint32_t frame,count;TransformMatrix a,b;
        if(!read(f,&frame,4)||!read(f,&count,4)||!count||count>96||frame<prior_frame||
           !read(f,a.data(),64)||!read(f,b.data(),64)||!read(f,input.data(),count*16)||!read(f,reference.data(),count*16))return 4;
        if(frame!=prior_frame){++seen_poses;prior_frame=frame;}
        const std::span<const TransformVector4>source(input.data(),count);
        TransformErrorBound error;
        if(!bound_vu1_transform(a,b,source,error)){
            unsupported+=count;
            // A complete replay must contain one result for every input batch.
            if(result_path)return 16;
            continue;
        }
#ifndef _EE
        if(!transform_reference(a,b,source,{recalculated.data(),count}))return 5;
        // Host-generated reference floats can differ by one final rounding bit
        // from EE software-double narrowing. Both must lie inside the declared
        // bound; the stored host reference remains the independent comparison.
        for(unsigned i=0;i<count;++i){const auto r=lanes(reference[i]),c=lanes(recalculated[i]);
            for(unsigned lane=0;lane<4;++lane)if(!std::isfinite(r[lane])||
                std::abs(double(r[lane])-c[lane])>error.absolute[lane])return 6;}
#else
        // Stored expected clip coordinates are sealed by the host capture
        // report. Re-running software-double transforms here is redundant;
        // compare the actual VU output directly to those expected words below.
        for(unsigned i=0;i<count;++i)for(float x:lanes(reference[i])){
            const auto m=std::bit_cast<std::uint32_t>(x)&0x7fffffffu;
            if(m && (m<0x00800000u||m>0x47800000u))return 6;}
#endif
        PreparedProjectionGuard plan;if(!prepare_vu1_projection_guard(error,{},plan))return 6;
#ifdef _EE
        if(!backend.transform_native(a,b,source,{actual.data(),count})){
            std::printf("VU1 CORPUS FAIL record=%lu frame=%lu %s\n",(unsigned long)record,(unsigned long)frame,backend.error());return 7;}
        if(!backend.transform(a,b,source,{guarded.data(),count},error)||std::memcmp(actual.data(),guarded.data(),count*16)){
            std::printf("VU1 CORPUS FAIL raw guarded mismatch record=%lu\n",(unsigned long)record);return 14;}
        native_equal+=count;
#else
        host_float(a,b,source,{actual.data(),count});
#endif
        for(unsigned i=0;i<count;++i){++vertices;const auto got=lanes(actual[i]),want=lanes(reference[i]);
            for(unsigned lane=0;lane<4;++lane){const double delta=std::abs(double(got[lane])-want[lane]);
                maximum_clip=std::max(maximum_clip,delta);maximum_ratio=std::max(maximum_ratio,delta/error.absolute[lane]);
                if(delta>error.absolute[lane]){
                    std::printf("VU1 CORPUS FAIL enclosure record=%lu vertex=%u lane=%u got=%08lx want=%08lx error=%.9g bound=%.9g\n",
                        (unsigned long)record,i,lane,(unsigned long)std::bit_cast<std::uint32_t>(got[lane]),
                        (unsigned long)std::bit_cast<std::uint32_t>(want[lane]),delta,error.absolute[lane]);return 8;}}
            TransformProjectionError estimate;const auto guard=guard_prepared_vu1_projection(actual[i],plan,&estimate);
            const auto original_guard=guard_vu1_projection(actual[i],error,{});
            if(guard!=original_guard){std::printf("VU1 CORPUS FAIL prepared acceptance record=%lu vertex=%u\n",(unsigned long)record,i);return 13;}
            if(guard==TransformGuard::accepted){++accepted;
                const auto&v=actual[i];const auto&r=reference[i];
                const double pixels=std::max(248*std::abs(double(v.x)/v.w-double(r.x)/r.w),192*std::abs(double(v.y)/v.w-double(r.y)/r.w));
                const double depth=std::abs(double(v.z)/v.w-double(r.z)/r.w);
                maximum_pixels=std::max(maximum_pixels,pixels);maximum_depth=std::max(maximum_depth,depth);
                if(pixels>estimate.pixels||depth>estimate.depth)return 9;
            }else if(guard==TransformGuard::near_clip_plane)++near;
            else if(guard==TransformGuard::projection_budget)++budget;
            else return 10;
        }
        GsCoordinates xy{{2048.125f,2047.9375f,0,4095.9375f}};GsFixedCoordinates fixed{};
        quantize_gs_xy_vu0(&xy,&fixed,1);
        if(fixed.xy[0]!=32770||fixed.xy[1]!=32767||fixed.xy[2]!=0||fixed.xy[3]!=65535)return 11;
        if(!results.record(frame,{actual.data(),count}))return 15;
        if(record%1000==0)std::printf("VU1 CORPUS PROGRESS record=%lu/%lu frame=%lu\n",(unsigned long)record,(unsigned long)records,(unsigned long)frame);
    }
    if(std::fgetc(f)!=EOF||std::ferror(f)||seen_poses!=poses)return 12;
    if(!results.finish(records))return 15;
    std::printf("VU1 CORPUS PASS records=%lu poses=%lu vertices=%llu accepted=%llu near=%llu budget=%llu unsupported=%llu max_clip=%.9g max_pixels=%.9g max_depth=%.9g bound_ratio=%.9g hardware_batches=%llu native_equal_vertices=%llu\n",
        (unsigned long)records,(unsigned long)poses,(unsigned long long)vertices,(unsigned long long)accepted,
        (unsigned long long)near,(unsigned long long)budget,(unsigned long long)unsupported,maximum_clip,maximum_pixels,maximum_depth,
        maximum_ratio,(unsigned long long)backend.stats().batches,(unsigned long long)native_equal);
    return 0;
}
}
int main(int argc,char**argv){
#ifdef _EE
    const char*default_results="host:vu1-results.bin";
#else
    const char*default_results=nullptr;
#endif
    return run(argc>1?argv[1]:"host:vu1-corpus.bin",argc>2?argv[2]:default_results);
}
