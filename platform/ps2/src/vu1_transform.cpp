#include "vu1_transform.h"
#include <algorithm>
#include <bit>
#include <cstring>
#include <cstdio>
#include <new>
#ifdef _EE
#include <dma.h>
#include <dma_registers.h>
#include <ee_regs.h>
#include <kernel.h>
#include <vif_registers.h>
extern "C" const unsigned char native_vu1_transform_begin[],native_vu1_transform_end[];
#endif

namespace ps2 {
namespace {
const void* owner=nullptr;
bool quarantined=false;
#ifdef _EE
constexpr std::uint32_t running=0x100,limit=300000000;
std::uint32_t ticks(){std::uint32_t t;asm volatile("mfc0 %0,$9":"=r"(t));return t;}
bool idle(){
    // DBF/MRK are harmless; command/FIFO/error/reverse state is not.
    return !(*R_EE_D1_CHCR&running) && !*R_EE_D1_QWC && !(VIF1_STAT&0x1f803f0fu);
}
bool configured(){
    const u32 c=*DMA_REG_CTRL,p=*DMA_REG_PCR;
    return (c&1u) && !(c&0xccu) && !(*DMA_REG_ENABLER&0x10000u) &&
        (!(p&0x80000000u)||(p&0x20000u)) && !(*DMA_REG_STAT&(1u<<17));
}
bool stop_owned(){
    const int interrupts=DIntr();const u32 hold=*DMA_REG_ENABLER;
    *DMA_REG_ENABLEW=hold|0x10000u;
    const u32 channel=*R_EE_D1_CHCR;const u32 control=*DMA_REG_CTRL;(void)control;
    *R_EE_D1_CHCR=channel&~running;asm volatile("sync.l":::"memory");
    const bool stopped=(*R_EE_D1_CHCR&running)==0;
    *DMA_REG_ENABLEW=hold;if(interrupts)EIntr();return stopped;
}
constexpr std::uint32_t command(unsigned op,unsigned count=0,unsigned immediate=0){
    return (std::uint32_t(op)<<24)|(std::uint32_t(count&255)<<16)|(immediate&65535u);
}
#endif
}
bool valid_native_vu1_transform_input(const TransformMatrix&a,const TransformMatrix&b,
                                     std::span<const TransformVector4>input) noexcept {
    if(input.size()>vu1_transform_capacity||(!input.empty()&&!input.data()))return false;
    const auto valid=[](float f){const auto m=std::bit_cast<std::uint32_t>(f)&0x7fffffffu;
        return !m||(m>=0x00800000u&&m<=0x47800000u);};
    for(float f:a)if(!valid(f))return false;
    for(float f:b)if(!valid(f))return false;
    for(unsigned i:{3u,7u,11u})if(std::bit_cast<std::uint32_t>(a[i])&0x7fffffffu)return false;
    if(std::bit_cast<std::uint32_t>(a[15])!=0x3f800000u)return false;
    for(const auto&p:input){const auto v=std::bit_cast<std::array<float,4>>(p);
        for(float f:v)if(!valid(f))return false;
        if(std::bit_cast<std::uint32_t>(p.w)!=0x3f800000u)return false;}
    return true;
}
struct Vu1Transform::Storage {
    alignas(64) std::array<std::uint32_t,1024> packet{};
    alignas(16) std::array<TransformVector4,vu1_transform_capacity> result{};
    unsigned used=0;std::uint16_t sequence=0;
    bool ready=false,retain=false;
    const char* failure="VU1 NOT INITIALIZED";
    Vu1TransformStats counters{};
#ifdef _EE
    void word(std::uint32_t v){packet[used++]=v;}
    void words(const void*p,std::size_t bytes){std::memcpy(packet.data()+used,p,bytes);used+=unsigned(bytes/4);}
    bool transfer(){
        ++sequence;word(command(0x10));word(command(0x07,0,sequence));
        while(used%4)word(0);
        // Valid VIF NOPs also avoid the documented Play0.72 final-prefetched-
        // qword resume defect. Keep the ENTIRE source alive through DMA end.
        for(unsigned i=0;i<4;++i)word(0);
        // The pinned SDK's normal send publishes the full qwc*16-byte packet
        // through SyncDCache before starting DMA; do not flush that range twice.
        const auto start=ticks();
        if(dma_channel_send_normal(DMA_CHANNEL_VIF1,packet.data(),int(used/4),0,0)<0)
            return retire("VU1 DMA SUBMISSION FAILED");
        counters.dma_qwords+=used/4;
        while(VIF1_MARK!=sequence || (*R_EE_D1_CHCR&running)){
            if(std::uint32_t(ticks()-start)>=limit)return retire("VU1 COMPLETION TIMEOUT");
        }
        // FLUSHE before MARK proves all VU data writes complete. DMA end alone
        // is insufficient. Check FIFO/command/error state before reuse too.
        while(!idle()){
            if(std::uint32_t(ticks()-start)>=limit)return retire("VU1 IDLE TIMEOUT");
        }
        asm volatile("sync":::"memory");return true;
    }
    bool retire(const char*message){
        ready=false;failure=message;quarantined=true;
        retain=!stop_owned();return false;
    }
#endif
};
Vu1Transform::Vu1Transform() noexcept=default;
Vu1Transform::~Vu1Transform(){
    if(owner==this && !quarantined)owner=nullptr;
    if(storage_&&storage_->retain)storage_.release();
}
bool Vu1Transform::initialize() noexcept {
    if(storage_&&storage_->ready)return true;
    if(!storage_)storage_.reset(new(std::nothrow)Storage);
    if(!storage_)return false;
    auto&s=*storage_;s.counters.persistent_bytes=sizeof(Storage);
    if(quarantined){s.failure="VU1 TRANSPORT QUARANTINED";return false;}
    if(owner && owner!=this){s.failure="VU1 ALREADY OWNED";return false;}
#ifdef _EE
    if(!configured()){
        std::printf("VU1 CONFIG D_CTRL=%08lx D_PCR=%08lx D_ENABLER=%08lx D_STAT=%08lx VIF1_STAT=%08lx\n",
            (unsigned long)*DMA_REG_CTRL,(unsigned long)*DMA_REG_PCR,(unsigned long)*DMA_REG_ENABLER,
            (unsigned long)*DMA_REG_STAT,(unsigned long)VIF1_STAT);
        s.failure="VU1 DMA CONFIGURATION UNSUPPORTED";return false;}
    if(!idle()){s.failure="VU1 REQUIRES IDLE EXCLUSIVE VIF1";return false;}
    u32 vpu;asm volatile("cfc2 %0,$29":"=r"(vpu)::"memory");
    if(vpu&0x100u){s.failure="VU1 MICROPROGRAM ALREADY RUNNING";return false;}
    // Ownership is also a caller contract: some emulator versions omit VBS1.
    const auto bytes=std::size_t(reinterpret_cast<std::uintptr_t>(native_vu1_transform_end)-
                                reinterpret_cast<std::uintptr_t>(native_vu1_transform_begin));
    if(!bytes || bytes%8 || bytes>2048){s.failure="VU1 PROGRAM SIZE INVALID";return false;}
    owner=this;s.used=0;
    s.word(command(0x10)); // FLUSHE before publishing a new microprogram.
    s.word(command(0x01,0,0x0101)); // STCYCL: one qword written per input.
    s.word(command(0x05)); // STMOD: no VIF arithmetic.
    while(s.used%2!=1)s.word(0); // MPG payload doubleword alignment.
    s.word(command(0x4a,unsigned(bytes/8)));s.words(native_vu1_transform_begin,bytes);
    if(!s.transfer())return false;
    s.ready=true;s.failure="NONE";return true;
#else
    s.failure="VU1 HARDWARE UNAVAILABLE";return false;
#endif
}
bool Vu1Transform::transform(const TransformMatrix&a,const TransformMatrix&b,
                            std::span<const TransformVector4>input,std::span<TransformVector4>output,
                            TransformErrorBound&bound) noexcept {
    if(!storage_||!storage_->ready)return false;
    auto&s=*storage_;TransformErrorBound candidate;
    if(output.size()<input.size()||(!input.empty()&&!output.data())||
       !bound_vu1_transform(a,b,input,candidate)){s.failure="VU1 TRANSFORM INPUT UNSUPPORTED";return false;}
    if(!transform_validated(a,b,input,output))return false;
    bound=candidate;return true;
}
bool Vu1Transform::transform_native(const TransformMatrix&a,const TransformMatrix&b,
                                   std::span<const TransformVector4>input,std::span<TransformVector4>output) noexcept {
    if(!storage_||!storage_->ready)return false;
    if(output.size()<input.size()||(!input.empty()&&!output.data())||!valid_native_vu1_transform_input(a,b,input)){
        storage_->failure="VU1 NATIVE INPUT UNSUPPORTED";return false;}
    return transform_validated(a,b,input,output);
}
bool Vu1Transform::transform_validated(const TransformMatrix&a,const TransformMatrix&b,
                                      std::span<const TransformVector4>input,std::span<TransformVector4>output) noexcept {
    auto&s=*storage_;
    if(input.empty()){s.failure="NONE";return true;}
#ifdef _EE
    if(!configured()||!idle()){
        // Nothing was submitted by this call: never stop another owner's DMA.
        s.ready=false;quarantined=true;s.failure="VU1 OWNERSHIP OR CONFIGURATION LOST";return false;
    }
    s.used=0;
    s.word(command(0x6c,8,0));s.words(a.data(),sizeof(a));s.words(b.data(),sizeof(b));
    s.word(command(0x6c,1,8));s.word(std::uint32_t(input.size()));s.word(0);s.word(0);s.word(0);
    s.word(command(0x6c,unsigned(input.size()),10));s.words(input.data(),input.size_bytes());
    s.word(command(0x14)); // MSCAL address0; kernel uses absolute data addresses.
    if(!s.transfer())return false;
    const volatile std::uint32_t*memory=reinterpret_cast<const volatile std::uint32_t*>(0x1100c000u+106*16);
    for(std::size_t i=0;i<input.size();++i){
        std::array<std::uint32_t,4> bits;
        for(unsigned c=0;c<4;++c){bits[c]=memory[4*i+c];const auto magnitude=bits[c]&0x7fffffffu;
            if(magnitude && (magnitude<0x00800000u||magnitude>0x47800000u)){
                s.failure="VU1 OUTPUT OUTSIDE BOUNDED DOMAIN";return false;}}
        s.result[i]=std::bit_cast<TransformVector4>(bits);
    }
    std::copy_n(s.result.begin(),input.size(),output.begin());
    ++s.counters.batches;s.counters.vertices+=input.size();s.failure="NONE";return true;
#else
    (void)a;(void)b;(void)output;return false;
#endif
}
bool Vu1Transform::available()const noexcept{return storage_&&storage_->ready;}
const char*Vu1Transform::error()const noexcept{return storage_?storage_->failure:"VU1 NOT INITIALIZED";}
Vu1TransformStats Vu1Transform::stats()const noexcept{return storage_?storage_->counters:Vu1TransformStats{};}
} // namespace ps2
