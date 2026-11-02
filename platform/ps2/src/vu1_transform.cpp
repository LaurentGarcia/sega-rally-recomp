#include "vu1_transform.h"
#include "vu1_world.h"
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
#include <gif_tags.h>
#include <gs_gp.h>
#include <gs_privileged.h>
extern "C" const unsigned char native_vu1_transform_begin[],native_vu1_transform_end[];
extern "C" const unsigned char native_vu1_project_begin[],native_vu1_project_end[];
extern "C" const unsigned char native_vu1_world_begin[],native_vu1_world_end[];
extern "C" const unsigned char native_vu1_world_emit[],native_vu1_world_emit_bank1[];
#endif

namespace ps2 {
namespace {
static_assert(sizeof(TransformVector4)==16&&sizeof(TransformProjectedVertex)==16);
const void* owner=nullptr;
bool quarantined=false;
#ifdef _EE
std::uint64_t world_ticket=0; // Serialized by the process-wide exclusive lease.
#endif
#ifdef _EE
constexpr std::uint32_t running=0x100,limit=300000000;
constexpr unsigned projection_entry=256; // Micro-instruction units, not bytes.
constexpr unsigned world_entry=512;
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
bool overlapping(const void*a,const void*b,std::size_t bytes){
    const auto x=reinterpret_cast<std::uintptr_t>(a),y=reinterpret_cast<std::uintptr_t>(b);
    return x<=y?y-x<bytes:x-y<bytes;
}
bool aligned(const void*p){return (reinterpret_cast<std::uintptr_t>(p)&15u)==0;}
bool ranges_overlap(const void*a,std::size_t an,const void*b,std::size_t bn){
    if(!an||!bn)return false;
    const auto x=reinterpret_cast<std::uintptr_t>(a),y=reinterpret_cast<std::uintptr_t>(b);
    return x<=y?y-x<an:x-y<bn;
}
#ifdef _EE
bool usable_projection(const TransformVector4&c,const TransformProjectedVertex&p){
    const auto normal=[](float f){const auto m=std::bit_cast<std::uint32_t>(f)&0x7fffffffu;
        return !m||(m>=0x00800000u&&m<0x7f800000u);};
    return c.w>=0x1p-10f&&c.x>=-c.w&&c.x<=c.w&&c.y>=-c.w&&c.y<=c.w&&c.z>=0&&c.z<=c.w&&
        normal(p.x)&&normal(p.y)&&normal(p.depth)&&normal(p.q)&&
        p.x>=0&&p.x<=496&&p.y>=0&&p.y<=384&&p.depth>=0&&p.depth<=1&&p.q>=0x1p-16f&&p.q<=1024;
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
bool valid_native_vu1_world_input(const Vu1WorldBatch& batch) noexcept {
    const auto n=batch.corners.size();
    if(n>vu1_world_corner_capacity||n%3||batch.states.size()!=n/3||
       (n&&(!batch.corners.data()||!batch.states.data()||!aligned(batch.corners.data())))||
       (batch.output_height!=448&&batch.output_height!=512)||
       (batch.completion!=Vu1WorldCompletion::gs_finished&&batch.completion!=Vu1WorldCompletion::path1_retired)||
       !valid_native_vu1_transform_input(batch.object_to_world,batch.world_to_clip,{}))return false;
    const auto value=[](float f){const auto m=std::bit_cast<std::uint32_t>(f)&0x7fffffffu;
        return !m||(m>=0x00800000u&&m<=0x47800000u);};
    const auto scale=[](float f){const auto bits=std::bit_cast<std::uint32_t>(f);
        return bits==0x3f800000u||(bits>=0x3b800000u&&bits<=0x3e000000u&&(bits&0x7fffffu)==0);};
    for(const auto&c:batch.corners)
        if(!value(c.position.x)||!value(c.position.y)||!value(c.position.z)||
           std::bit_cast<std::uint32_t>(c.position.w)!=0x3f800000u||
           !value(c.u)||!value(c.v)||!scale(c.inv_width)||!scale(c.inv_height))return false;
    for(const auto&s:batch.states)
        if((s.prim!=3u&&s.prim!=19u)||(s.rgba>>24)>128u)return false;
    return true;
}
bool valid_native_vu1_world_geometry(const Vu1WorldGeometry& geometry) noexcept {
    const auto count=geometry.corners.size();
    if(!count||count>vu1_world_corner_capacity||count%3||!geometry.corners.data()||
       !aligned(geometry.corners.data())||(geometry.output_height!=448&&geometry.output_height!=512)||
       !valid_native_vu1_transform_input(geometry.object_to_world,geometry.world_to_clip,{}))return false;
    const auto value=[](float f){const auto m=std::bit_cast<std::uint32_t>(f)&0x7fffffffu;
        return !m||(m>=0x00800000u&&m<=0x47800000u);};
    const auto scale=[](float f){const auto bits=std::bit_cast<std::uint32_t>(f);
        return bits==0x3f800000u||(bits>=0x3b800000u&&bits<=0x3e000000u&&(bits&0x7fffffu)==0);};
    for(const auto&c:geometry.corners)
        if(!value(c.position.x)||!value(c.position.y)||!value(c.position.z)||
           std::bit_cast<std::uint32_t>(c.position.w)!=0x3f800000u||
           !value(c.u)||!value(c.v)||!scale(c.inv_width)||!scale(c.inv_height))return false;
    return true;
}
struct Vu1Transform::Storage {
    struct World {
        using Quad=std::array<std::uint32_t,4>;
        alignas(64) std::array<std::uint32_t,2048> packet{};
        alignas(16) std::array<Quad,16> header{};
        alignas(16) std::array<Quad,vu1_world_triangle_capacity*15+2> output{};
        alignas(16) std::array<TransformVector4,vu1_world_corner_capacity> clips{};
        unsigned used=0,bank=0;
        struct Bank {
            std::uint64_t ticket=0;
            Quad echo{};
            unsigned count=0,phase=0; // free, preparing, ready, emitting
        };
        std::array<Bank,2> banks{};
        struct Pending {
            std::uint64_t ticket=0;
            unsigned bank=0,kind=0; //1 preparation,2 emission
            std::uint32_t started=0;
            std::uint16_t mark=0;
            Vu1WorldCompletion completion=Vu1WorldCompletion::path1_retired;
        } pending;
        std::uint32_t submit_started=0;
        bool ready=false;
        Vu1WorldStats counters{};
        void word(std::uint32_t v){packet[used++]=v;}
        void words(const void*p,std::size_t bytes){std::memcpy(packet.data()+used,p,bytes);used+=unsigned(bytes/4);}
    };
    std::unique_ptr<World> world;
    alignas(64) std::array<std::uint32_t,1024> packet{};
    alignas(16) std::array<TransformVector4,vu1_transform_capacity> result{};
    alignas(16) std::array<TransformProjectedVertex,vu1_transform_capacity> projected{};
    unsigned used=0;std::uint16_t sequence=0;
    bool ready=false,retain=false,projection_ready=false;
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
    bool world_submit(bool path1){
        auto&w=*world;++sequence;
        w.word(command(path1?0x11:0x10)); // FLUSH retires VU and PATH1/2.
        w.word(command(0x07,0,sequence));
        while(w.used%4)w.word(0);
        for(unsigned i=0;i<4;++i)w.word(0);
        w.submit_started=ticks();
        if(w.pending.ticket){w.pending.started=w.submit_started;w.pending.mark=sequence;}
        if(dma_channel_send_normal(DMA_CHANNEL_VIF1,w.packet.data(),int(w.used/4),0,0)<0)
            return retire("VU1 WORLD DMA SUBMISSION FAILED");
        w.counters.dma_qwords+=w.used/4;
        return true;
    }
    bool world_transfer(bool path1){
        if(!world_submit(path1))return false;
        const auto start=world->submit_started;
        while(VIF1_MARK!=sequence||!idle())
            if(std::uint32_t(ticks()-start)>=limit)return retire("VU1 WORLD COMPLETION TIMEOUT");
        asm volatile("sync":::"memory");return true;
    }
    bool world_idle(const void* expected){
        if(owner!=expected||quarantined||!configured()||!idle()){
            world->banks={};ready=false;quarantined=true;failure="VU1 WORLD OWNERSHIP LOST";return false;
        }
        if((*R_EE_D2_CHCR&running)||*R_EE_D2_QWC){
            world->banks={};ready=false;quarantined=true;failure="VU1 WORLD PATH3 OWNERSHIP LOST";
            return false; // Never stop a foreign DMA.
        }
        const auto started=ticks();
        while(*R_EE_GIF_STAT&0x1f001febu)
            if(std::uint32_t(ticks()-started)>=limit){
                world->banks={};ready=false;quarantined=true;failure="VU1 WORLD GIF IDLE TIMEOUT";return false;
            }
        asm volatile("sync":::"memory");return true;
    }
    bool world_clips(unsigned base,unsigned count,std::span<TransformVector4> output){
        const volatile std::uint32_t*memory=reinterpret_cast<const volatile std::uint32_t*>(0x1100c000u+base*16);
        for(unsigned i=0;i<count;++i){std::array<std::uint32_t,4> values;
            for(unsigned c=0;c<4;++c){values[c]=memory[(112+i)*4+c];const auto m=values[c]&0x7fffffffu;
                if(m&&(m<0x00800000u||m>0x47800000u)){failure="VU1 WORLD CLIP OUTSIDE DOMAIN";return false;}}
            world->clips[i]=std::bit_cast<TransformVector4>(values);
        }
        std::copy_n(world->clips.begin(),count,output.begin());return true;
    }
    bool retire(const char*message){
        if(world){world->banks={};world->pending={};}
        ready=false;failure=message;quarantined=true;
        retain=!stop_owned();return false;
    }
#endif
};
void Vu1Transform::invalidate_world() noexcept {
    if(storage_&&storage_->world&&!storage_->world->pending.ticket&&
       (storage_->world->banks[0].phase||storage_->world->banks[1].phase))storage_->world->banks={};
}
bool Vu1Transform::world_pending() noexcept {
    if(storage_&&storage_->world&&storage_->world->pending.ticket){
        storage_->failure="VU1 WORLD OPERATION PENDING";return true;
    }
    return false;
}
Vu1Transform::Vu1Transform() noexcept=default;
Vu1Transform::~Vu1Transform(){
    if(storage_&&storage_->world&&storage_->world->pending.ticket){
        Vu1WorldSubmission submission;submission.owner_=this;submission.ticket_=storage_->world->pending.ticket;
        if(!drain_world(submission)&&storage_->world->pending.ticket){
#ifdef _EE
            storage_->retire("VU1 WORLD DESTRUCTION DRAIN FAILED");
#endif
        }
    }
    if(owner==this && !quarantined)owner=nullptr;
    if(storage_&&storage_->retain)storage_.release();
}
bool Vu1Transform::initialize() noexcept {
    if(world_pending())return false;
    if(storage_&&storage_->ready)return true;
    invalidate_world();
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
    if(world_pending())return false;
    invalidate_world();
    if(!storage_||!storage_->ready)return false;
    auto&s=*storage_;TransformErrorBound candidate;
    if(output.size()<input.size()||(!input.empty()&&!output.data())||
       !bound_vu1_transform(a,b,input,candidate)){s.failure="VU1 TRANSFORM INPUT UNSUPPORTED";return false;}
    if(!transform_validated(a,b,input,output))return false;
    bound=candidate;return true;
}
bool Vu1Transform::transform_native(const TransformMatrix&a,const TransformMatrix&b,
                                   std::span<const TransformVector4>input,std::span<TransformVector4>output) noexcept {
    if(world_pending())return false;
    invalidate_world();
    if(!storage_||!storage_->ready)return false;
    if(output.size()<input.size()||(!input.empty()&&!output.data())||!valid_native_vu1_transform_input(a,b,input)){
        storage_->failure="VU1 NATIVE INPUT UNSUPPORTED";return false;}
    return transform_validated(a,b,input,output);
}
bool Vu1Transform::transform_project_native(const TransformMatrix&a,const TransformMatrix&b,
                                           std::span<const TransformVector4>input,std::span<TransformVector4>output,
                                           std::span<TransformProjectedVertex>projected) noexcept {
    if(world_pending())return false;
    invalidate_world();
    if(!storage_||!storage_->ready)return false;
    const auto count=input.size();
    if(count>vu1_transform_capacity||output.size()<count||projected.size()<count||
       (count&&(!input.data()||!output.data()||!projected.data()||
          !aligned(input.data())||!aligned(output.data())||!aligned(projected.data())||
          overlapping(input.data(),projected.data(),count*sizeof(TransformVector4))||
          overlapping(output.data(),projected.data(),count*sizeof(TransformVector4))))||
       !valid_native_vu1_transform_input(a,b,input)){
        storage_->failure="VU1 PROJECTED INPUT UNSUPPORTED";return false;
    }
    return transform_validated(a,b,input,output,projected.first(count));
}
bool Vu1Transform::initialize_world() noexcept {
    if(world_pending())return false;
    if(!storage_||!storage_->ready)return false;
    auto&s=*storage_;
    if(s.world&&s.world->ready)return true;
    invalidate_world();
    if(!s.world)s.world.reset(new(std::nothrow)Storage::World);
    if(!s.world){s.failure="VU1 WORLD ALLOCATION FAILED";return false;}
    auto&w=*s.world;w.counters.persistent_bytes=sizeof(Storage::World);
    s.counters.persistent_bytes=sizeof(Storage)+sizeof(Storage::World);
#ifdef _EE
    if(owner!=this||quarantined||!configured()||!idle()){
        s.ready=false;quarantined=true;s.failure="VU1 WORLD OWNERSHIP LOST";return false;
    }
    const auto bytes=std::size_t(reinterpret_cast<std::uintptr_t>(native_vu1_world_end)-
                                reinterpret_cast<std::uintptr_t>(native_vu1_world_begin));
    if(!bytes||bytes%8||bytes>8192||world_entry+bytes/8>2048){
        s.failure="VU1 WORLD PROGRAM SIZE INVALID";return false;
    }
    for(const auto*entry:{native_vu1_world_emit,native_vu1_world_emit_bank1}){
        const auto offset=reinterpret_cast<std::uintptr_t>(entry)-reinterpret_cast<std::uintptr_t>(native_vu1_world_begin);
        if(offset%8||offset>=bytes){s.failure="VU1 WORLD EMIT ENTRY INVALID";return false;}
    }
    // MPG is bounded to256 instructions per command; address units are8 bytes.
    for(std::size_t offset=0;offset<bytes;){
        const auto chunk=std::min<std::size_t>(2048,bytes-offset);w.used=0;
        w.word(command(0x10));
        while(w.used%2!=1)w.word(0);
        w.word(command(0x4a,unsigned(chunk/8),world_entry+unsigned(offset/8)));
        w.words(native_vu1_world_begin+offset,chunk);
        if(!s.world_transfer(false))return false;
        offset+=chunk;
    }
    w.ready=true;s.failure="NONE";return true;
#else
    s.failure="VU1 HARDWARE UNAVAILABLE";return false;
#endif
}
Vu1WorldResult Vu1Transform::draw_world(const Vu1WorldBatch& batch,std::span<TransformVector4> clip) noexcept {
    if(world_pending())return Vu1WorldResult::rejected;
    invalidate_world();
    if(quarantined)return Vu1WorldResult::failed;
    if(!storage_||!storage_->ready||!storage_->world||!storage_->world->ready)return Vu1WorldResult::rejected;
    auto&s=*storage_;auto&w=*s.world;const auto count=batch.corners.size();
    const auto reject=[&](const char*why){s.failure=why;++w.counters.rejected_batches;return Vu1WorldResult::rejected;};
    if(!valid_native_vu1_world_input(batch)||clip.size()<count||
       (count&&(!clip.data()||!aligned(clip.data())||
        ranges_overlap(clip.data(),count*sizeof(TransformVector4),batch.corners.data(),batch.corners.size_bytes())||
        ranges_overlap(clip.data(),count*sizeof(TransformVector4),batch.states.data(),batch.states.size_bytes()))))
        return reject("VU1 WORLD INPUT UNSUPPORTED");
    if(!count){s.failure="NONE";return Vu1WorldResult::drawn;}
#ifdef _EE
    if(owner!=this||quarantined||!configured()||!idle()){
        s.ready=false;quarantined=true;s.failure="VU1 WORLD OWNERSHIP LOST";return Vu1WorldResult::failed;
    }
    // The Display has retired its PATH3 upload/state transfer. Never stop a
    // foreign channel. GIF's drain/activity telemetry can lag its completed DMA.
    if((*R_EE_D2_CHCR&running)||*R_EE_D2_QWC){
        s.ready=false;quarantined=true;s.failure="VU1 WORLD PATH3 OWNERSHIP LOST";
        return Vu1WorldResult::failed; // No stop of the foreign DMA and no scalar retry.
    }
    const auto gif_start=ticks();
    while(*R_EE_GIF_STAT&0x1f001febu)
        if(std::uint32_t(ticks()-gif_start)>=limit){
            s.ready=false;quarantined=true;s.failure="VU1 WORLD GIF IDLE TIMEOUT";return Vu1WorldResult::failed;
        }
    static_assert(sizeof(Vu1WorldCorner)==32);
    const auto bits=[](float f){return std::bit_cast<std::uint32_t>(f);};
    const bool deferred=batch.completion==Vu1WorldCompletion::path1_retired;
    std::memcpy(w.header.data(),batch.object_to_world.data(),sizeof(TransformMatrix));
    std::memcpy(w.header.data()+4,batch.world_to_clip.data(),sizeof(TransformMatrix));
    w.header[8]={std::uint32_t(count),std::uint32_t(count/3),batch.sequence,0};
    w.header[9]={bits(248),bits(192),0,bits(0x1p-10f)};
    w.header[10]={bits(640.f/496.f),bits(float(batch.output_height)/384.f),bits(2048),bits(65536)};
    w.header[11]={bits(496),bits(384),bits(1),bits(65536)};
    w.header[12]={};w.header[13]={};
    w.header[14]={bits(65536),bits(.5f),0,bits(0x1p-17f)};
    w.header[15]={bits(0x1p-16f),bits(1024),bits(65536),bits(65536)};
    const auto tag=[&](std::size_t index,std::uint64_t low,std::uint64_t high){
        w.output[index]={std::uint32_t(low),std::uint32_t(low>>32),std::uint32_t(high),std::uint32_t(high>>32)};
    };
    for(std::size_t t=0;t<count/3;++t){const auto&state=batch.states[t];const auto base=t*15;
        tag(base,GIF_SET_TAG(4,0,0,0,GIF_FLG_PACKED,1),GIF_REG_AD);
        tag(base+1,state.test,GS_REG_TEST);tag(base+2,state.tex0,GS_REG_TEX0);
        tag(base+3,state.clamp,GS_REG_CLAMP);tag(base+4,state.prim,GS_REG_PRIM);
        tag(base+5,GIF_SET_TAG(3,deferred&&t+1==count/3,0,0,GIF_FLG_PACKED,3),
            GIF_REG_ST|(std::uint64_t(GIF_REG_RGBAQ)<<4)|(std::uint64_t(GIF_REG_XYZ2)<<8));
    }
    const auto output_count=count/3*15+2;
    tag(output_count-2,GIF_SET_TAG(1,1,0,0,GIF_FLG_PACKED,1),GIF_REG_AD);
    tag(output_count-1,0,GS_REG_FINISH);
    const unsigned base=w.bank*512;w.used=0;
    w.word(command(0x6c,16,base));w.words(w.header.data(),sizeof(w.header));
    w.word(command(0x6c,unsigned(count*2),base+16));w.words(batch.corners.data(),batch.corners.size_bytes());
    // Publish only immutable headers/state and color. The microprogram writes
    // every ST/XYZ slot after whole-batch admission, so sending their old CPU
    // contents would waste six qwords per triangle. VIF V4-8 zero-extends GS
    // RGBA bytes; CL=3/WL=1 skips over the ST/XYZ slots between color entries.
    // Every tag is still supplied per draw: the optional projection operation
    // may have overwritten bank0 output memory between world submissions.
    for(std::size_t t=0;t<count/3;++t){const unsigned address=base+256+unsigned(t*15);
        w.word(command(0x6c,6,address));w.words(w.output.data()+t*15,6*16);
        w.word(command(0x01,0,0x0103));
        w.word(command(0x6e,3,(address+7)|0x4000)); // Unsigned V4-8, absolute address.
        for(unsigned k=0;k<3;++k)w.word(batch.states[t].rgba);
        w.word(command(0x01,0,0x0101));
    }
    if(!deferred){
        w.word(command(0x6c,2,base+256+unsigned(output_count)-2));
        w.words(w.output.data()+output_count-2,2*16);
    }
    w.word(command(0x14,0,world_entry+w.bank*3));
    if(!deferred)*GS_REG_CSR=2; // Queued mode emits no event and clears none.
    if(!s.world_transfer(true))return Vu1WorldResult::failed;
    // EE-mapped VU data reads require VU/VIF/GIF to be stopped. FLUSH/MARK
    // proves execution and PATH1 transfer; observe the remaining GIF activity
    // tail too before reading status or permitting reuse of either data bank.
    const auto retired=ticks();
    while(*R_EE_GIF_STAT&0x1f001febu)
        if(std::uint32_t(ticks()-retired)>=limit){s.retire("VU1 WORLD PATH1 IDLE TIMEOUT");return Vu1WorldResult::failed;}
    const volatile std::uint32_t*memory=reinterpret_cast<const volatile std::uint32_t*>(0x1100c000u+base*16);
    std::array<std::uint32_t,4> status,echo;
    for(unsigned i=0;i<4;++i){status[i]=memory[12*4+i];echo[i]=memory[13*4+i];}
    if(echo!=w.header[8]||status[1]!=count||(status[0]!=1&&status[0]!=2)||
       (status[0]==2&&(status[2]!=count/3||status[3]!=0))||
       (status[0]==1&&(status[2]!=0||status[3]<1||status[3]>3))){
        s.retire("VU1 WORLD STATUS INVALID");return Vu1WorldResult::failed;
    }
    ++w.counters.batches;w.counters.corners+=count;w.bank^=1;
    if(status[0]==2){
        if(!deferred){const auto started=ticks();
            while(!(*GS_REG_CSR&2))
                if(std::uint32_t(ticks()-started)>=limit){s.retire("VU1 WORLD GS COMPLETION TIMEOUT");return Vu1WorldResult::failed;}}
        ++w.counters.drawn_batches;s.failure="NONE";return Vu1WorldResult::drawn;
    }
    // No packet was emitted. Validate the entire retained clip prefix before
    // publishing any of it; numerical rejection asks Display to transform on EE.
    for(std::size_t i=0;i<count;++i){std::array<std::uint32_t,4> values;
        for(unsigned c=0;c<4;++c){values[c]=memory[(112+i)*4+c];const auto m=values[c]&0x7fffffffu;
            if(m&&(m<0x00800000u||m>0x47800000u))return reject("VU1 WORLD CLIP OUTSIDE DOMAIN");}
        w.clips[i]=std::bit_cast<TransformVector4>(values);
    }
    std::copy_n(w.clips.begin(),count,clip.begin());++w.counters.fallback_batches;s.failure="NONE";
    return Vu1WorldResult::fallback;
#else
    return reject("VU1 HARDWARE UNAVAILABLE");
#endif
}
Vu1WorldSubmitResult Vu1Transform::submit_prepare_world(const Vu1WorldGeometry& geometry,
                                                       Vu1WorldSubmission& submission) noexcept {
    if(world_pending())return Vu1WorldSubmitResult::rejected;
    if(quarantined)return Vu1WorldSubmitResult::failed;
    if(!storage_||!storage_->ready||!storage_->world||!storage_->world->ready)return Vu1WorldSubmitResult::rejected;
    auto&s=*storage_;auto&w=*s.world;
    const auto reject=[&](const char*why){s.failure=why;++w.counters.rejected_batches;return Vu1WorldSubmitResult::rejected;};
    if(!valid_native_vu1_world_geometry(geometry)||
       ranges_overlap(&submission,sizeof(submission),geometry.corners.data(),geometry.corners.size_bytes()))
        return reject("VU1 WORLD PREPARE INPUT UNSUPPORTED");
    const unsigned bank=w.banks[w.bank].phase?w.bank^1:w.bank;
    if(w.banks[bank].phase)return reject("VU1 WORLD BANKS OCCUPIED");
#ifdef _EE
    const auto count=geometry.corners.size();
    if(world_ticket==UINT64_MAX)return reject("VU1 WORLD TICKET EXHAUSTED");
    if(!s.world_idle(this))return Vu1WorldSubmitResult::failed;
    const auto bits=[](float f){return std::bit_cast<std::uint32_t>(f);};
    std::memcpy(w.header.data(),geometry.object_to_world.data(),sizeof(TransformMatrix));
    std::memcpy(w.header.data()+4,geometry.world_to_clip.data(),sizeof(TransformMatrix));
    w.header[8]={std::uint32_t(count),std::uint32_t(count/3),geometry.sequence,1};
    w.header[9]={bits(248),bits(192),0,bits(0x1p-10f)};
    w.header[10]={bits(640.f/496.f),bits(float(geometry.output_height)/384.f),bits(2048),bits(65536)};
    w.header[11]={bits(496),bits(384),bits(1),bits(65536)};
    w.header[12]={};w.header[13]={};
    w.header[14]={bits(65536),bits(.5f),0,bits(0x1p-17f)};
    w.header[15]={bits(0x1p-16f),bits(1024),bits(65536),bits(65536)};
    const unsigned base=bank*512;w.used=0;
    w.word(command(0x6c,16,base));w.words(w.header.data(),sizeof(w.header));
    w.word(command(0x6c,unsigned(count*2),base+16));w.words(geometry.corners.data(),geometry.corners.size_bytes());
    w.word(command(0x14,0,world_entry+bank*3));
    // q8.w selects no-emission preparation. FLUSHE is sufficient for this
    // program; full GIF retirement is still required before mapped VU reads.
    auto& retained=w.banks[bank];retained={++world_ticket,w.header[8],unsigned(count),1};
    w.pending={retained.ticket,bank,1,0,0,Vu1WorldCompletion::path1_retired};
    if(!s.world_submit(false))return Vu1WorldSubmitResult::failed;
    w.bank=bank^1;submission.owner_=this;submission.ticket_=w.pending.ticket;s.failure="NONE";
    return Vu1WorldSubmitResult::submitted;
#else
    return reject("VU1 HARDWARE UNAVAILABLE");
#endif
}
Vu1WorldPollResult Vu1Transform::poll_world(const Vu1WorldSubmission& submission) noexcept {
    if(quarantined)return Vu1WorldPollResult::failed;
    if(!storage_||!storage_->ready||!storage_->world||!storage_->world->ready)return Vu1WorldPollResult::rejected;
    auto&s=*storage_;auto&w=*s.world;
    if(submission.owner_!=this||!submission.ticket_||submission.ticket_!=w.pending.ticket){
        s.failure="VU1 WORLD SUBMISSION STALE";return Vu1WorldPollResult::rejected;
    }
#ifdef _EE
    // Only D1 was submitted by this owner. Never stop foreign D2, even when
    // its unexpected activity forces the pending operation into quarantine.
    if(owner!=this||!configured()||(*R_EE_D2_CHCR&running)||*R_EE_D2_QWC){
        s.retire("VU1 WORLD PENDING OWNERSHIP LOST");return Vu1WorldPollResult::failed;
    }
    const bool finished=VIF1_MARK==w.pending.mark&&idle()&&!(*R_EE_GIF_STAT&0x1f001febu)&&
        (w.pending.kind!=2||w.pending.completion!=Vu1WorldCompletion::gs_finished||(*GS_REG_CSR&2));
    if(finished){asm volatile("sync":::"memory");s.failure="NONE";return Vu1WorldPollResult::complete;}
    if(std::uint32_t(ticks()-w.pending.started)>=limit){
        s.retire("VU1 WORLD ASYNC COMPLETION TIMEOUT");return Vu1WorldPollResult::failed;
    }
    s.failure="NONE";return Vu1WorldPollResult::pending;
#else
    s.failure="VU1 HARDWARE UNAVAILABLE";return Vu1WorldPollResult::rejected;
#endif
}
Vu1WorldPrepareResult Vu1Transform::finish_prepare_world(const Vu1WorldSubmission& submission,
                                                        Vu1WorldToken& token,std::span<TransformVector4> clip) noexcept {
    if(quarantined)return Vu1WorldPrepareResult::failed;
    if(!storage_||!storage_->ready||!storage_->world||!storage_->world->ready)return Vu1WorldPrepareResult::rejected;
    auto&s=*storage_;auto&w=*s.world;
    const auto reject=[&](const char*why){s.failure=why;++w.counters.rejected_batches;return Vu1WorldPrepareResult::rejected;};
    if(submission.owner_!=this||!submission.ticket_||submission.ticket_!=w.pending.ticket||w.pending.kind!=1)
        return reject("VU1 WORLD PREPARE SUBMISSION STALE");
    auto& retained=w.banks[w.pending.bank];const auto count=retained.count;
    if(clip.size()<count||!clip.data()||!aligned(clip.data())||
       ranges_overlap(clip.data(),count*sizeof(TransformVector4),&token,sizeof(token))||
       ranges_overlap(clip.data(),count*sizeof(TransformVector4),&submission,sizeof(submission))||
       ranges_overlap(&token,sizeof(token),&submission,sizeof(submission)))
        return reject("VU1 WORLD PREPARE OUTPUT UNSUPPORTED");
#ifdef _EE
    Vu1WorldPollResult polled;
    do{polled=poll_world(submission);}while(polled==Vu1WorldPollResult::pending);
    if(polled==Vu1WorldPollResult::failed)return Vu1WorldPrepareResult::failed;
    if(polled!=Vu1WorldPollResult::complete)return reject("VU1 WORLD PREPARE SUBMISSION LOST");
    const unsigned base=w.pending.bank*512;
    const volatile std::uint32_t*memory=reinterpret_cast<const volatile std::uint32_t*>(0x1100c000u+base*16);
    std::array<std::uint32_t,4> status,echo;
    for(unsigned i=0;i<4;++i){status[i]=memory[12*4+i];echo[i]=memory[13*4+i];}
    if(echo!=retained.echo||status[1]!=count||status[2]||
       (status[0]!=1&&status[0]!=3)||(status[0]==3&&status[3])||
       (status[0]==1&&(status[3]<1||status[3]>3))){
        s.retire("VU1 WORLD PREPARE STATUS INVALID");return Vu1WorldPrepareResult::failed;
    }
    w.pending={};++w.counters.batches;++w.counters.prepared_batches;w.counters.corners+=count;
    if(status[0]==1){
        retained={};
        if(!s.world_clips(base,count,clip))return reject(s.failure);
        ++w.counters.fallback_batches;s.failure="NONE";return Vu1WorldPrepareResult::fallback;
    }
    retained.phase=2;token.owner_=this;token.ticket_=retained.ticket;++w.counters.ready_batches;s.failure="NONE";
    return Vu1WorldPrepareResult::ready;
#else
    return reject("VU1 HARDWARE UNAVAILABLE");
#endif
}
Vu1WorldSubmitResult Vu1Transform::submit_emit_world(const Vu1WorldToken& token,std::span<const Vu1WorldState> states,
                                                    Vu1WorldCompletion completion,Vu1WorldSubmission& submission) noexcept {
    if(world_pending())return Vu1WorldSubmitResult::rejected;
    if(quarantined)return Vu1WorldSubmitResult::failed;
    if(!storage_||!storage_->ready||!storage_->world||!storage_->world->ready)return Vu1WorldSubmitResult::rejected;
    auto&s=*storage_;auto&w=*s.world;
    const auto reject=[&](const char*why){s.failure=why;++w.counters.rejected_batches;return Vu1WorldSubmitResult::rejected;};
    unsigned bank=0;while(bank<2&&(w.banks[bank].phase!=2||w.banks[bank].ticket!=token.ticket_))++bank;
    if(token.owner_!=this||!token.ticket_||bank==2)return reject("VU1 WORLD TOKEN STALE");
    auto& retained=w.banks[bank];
    if(states.size()!=retained.count/3||!states.data()||
       (completion!=Vu1WorldCompletion::gs_finished&&completion!=Vu1WorldCompletion::path1_retired)||
       ranges_overlap(&submission,sizeof(submission),states.data(),states.size_bytes())||
       ranges_overlap(&submission,sizeof(submission),&token,sizeof(token)))
        return reject("VU1 WORLD EMIT INPUT UNSUPPORTED");
    for(const auto&state:states)
        if((state.prim!=3&&state.prim!=19)||(state.rgba>>24)>128)return reject("VU1 WORLD STATE UNSUPPORTED");
#ifdef _EE
    if(world_ticket==UINT64_MAX)return reject("VU1 WORLD TICKET EXHAUSTED");
    if(!s.world_idle(this))return Vu1WorldSubmitResult::failed;
    const unsigned base=bank*512;
    const bool deferred=completion==Vu1WorldCompletion::path1_retired;
    const auto tag=[&](std::size_t index,std::uint64_t low,std::uint64_t high){
        w.output[index]={std::uint32_t(low),std::uint32_t(low>>32),std::uint32_t(high),std::uint32_t(high>>32)};
    };
    for(std::size_t t=0;t<states.size();++t){const auto&state=states[t];const auto p=t*15;
        tag(p,GIF_SET_TAG(4,0,0,0,GIF_FLG_PACKED,1),GIF_REG_AD);
        tag(p+1,state.test,GS_REG_TEST);tag(p+2,state.tex0,GS_REG_TEX0);
        tag(p+3,state.clamp,GS_REG_CLAMP);tag(p+4,state.prim,GS_REG_PRIM);
        tag(p+5,GIF_SET_TAG(3,deferred&&t+1==states.size(),0,0,GIF_FLG_PACKED,3),
            GIF_REG_ST|(std::uint64_t(GIF_REG_RGBAQ)<<4)|(std::uint64_t(GIF_REG_XYZ2)<<8));
    }
    const auto tail=states.size()*15;
    tag(tail,GIF_SET_TAG(1,1,0,0,GIF_FLG_PACKED,1),GIF_REG_AD);tag(tail+1,0,GS_REG_FINISH);
    retained.ticket=0;retained.phase=3;w.used=0;
    w.pending={++world_ticket,bank,2,0,0,completion};
    for(std::size_t t=0;t<states.size();++t){const unsigned address=base+256+unsigned(t*15);
        w.word(command(0x6c,6,address));w.words(w.output.data()+t*15,6*16);
        w.word(command(0x01,0,0x0103));w.word(command(0x6e,3,(address+7)|0x4000));
        for(unsigned k=0;k<3;++k)w.word(states[t].rgba);
        w.word(command(0x01,0,0x0101));
    }
    if(!deferred){w.word(command(0x6c,2,base+256+unsigned(tail)));w.words(w.output.data()+tail,32);}
    const auto*entry=base?native_vu1_world_emit_bank1:native_vu1_world_emit;
    const auto offset=std::size_t(reinterpret_cast<std::uintptr_t>(entry)-reinterpret_cast<std::uintptr_t>(native_vu1_world_begin));
    w.word(command(0x14,0,world_entry+unsigned(offset/8)));
    if(!deferred)*GS_REG_CSR=2;
    if(!s.world_submit(true))return Vu1WorldSubmitResult::failed;
    submission.owner_=this;submission.ticket_=w.pending.ticket;s.failure="NONE";
    return Vu1WorldSubmitResult::submitted;
#else
    return reject("VU1 HARDWARE UNAVAILABLE");
#endif
}
Vu1WorldResult Vu1Transform::finish_emit_world(const Vu1WorldSubmission& submission) noexcept {
    if(quarantined)return Vu1WorldResult::failed;
    if(!storage_||!storage_->ready||!storage_->world||!storage_->world->ready)return Vu1WorldResult::rejected;
    auto&s=*storage_;auto&w=*s.world;
    const auto reject=[&](const char*why){s.failure=why;++w.counters.rejected_batches;return Vu1WorldResult::rejected;};
    if(submission.owner_!=this||!submission.ticket_||submission.ticket_!=w.pending.ticket||w.pending.kind!=2)
        return reject("VU1 WORLD EMIT SUBMISSION STALE");
#ifdef _EE
    Vu1WorldPollResult polled;
    do{polled=poll_world(submission);}while(polled==Vu1WorldPollResult::pending);
    if(polled==Vu1WorldPollResult::failed)return Vu1WorldResult::failed;
    if(polled!=Vu1WorldPollResult::complete)return reject("VU1 WORLD EMIT SUBMISSION LOST");
    auto& retained=w.banks[w.pending.bank];const unsigned base=w.pending.bank*512,count=retained.count;
    const volatile std::uint32_t*memory=reinterpret_cast<const volatile std::uint32_t*>(0x1100c000u+base*16);
    std::array<std::uint32_t,4> status,echo;
    for(unsigned i=0;i<4;++i){status[i]=memory[12*4+i];echo[i]=memory[13*4+i];}
    if(echo!=retained.echo||status!=std::array<std::uint32_t,4>{2,count,count/3,0}){
        s.retire("VU1 WORLD EMIT STATUS INVALID");return Vu1WorldResult::failed;
    }
    retained={};w.pending={};++w.counters.drawn_batches;s.failure="NONE";return Vu1WorldResult::drawn;
#else
    return reject("VU1 HARDWARE UNAVAILABLE");
#endif
}
Vu1WorldResult Vu1Transform::cancel_world(const Vu1WorldToken& token,std::span<TransformVector4> clip) noexcept {
    if(world_pending())return Vu1WorldResult::rejected;
    if(quarantined)return Vu1WorldResult::failed;
    if(!storage_||!storage_->ready||!storage_->world||!storage_->world->ready)return Vu1WorldResult::rejected;
    auto&s=*storage_;auto&w=*s.world;
    const auto reject=[&](const char*why){s.failure=why;++w.counters.rejected_batches;return Vu1WorldResult::rejected;};
    unsigned bank=0;while(bank<2&&(w.banks[bank].phase!=2||w.banks[bank].ticket!=token.ticket_))++bank;
    if(token.owner_!=this||!token.ticket_||bank==2)return reject("VU1 WORLD TOKEN STALE");
    auto& retained=w.banks[bank];
    if(clip.size()<retained.count||!clip.data()||!aligned(clip.data())||
       ranges_overlap(clip.data(),retained.count*sizeof(TransformVector4),&token,sizeof(token)))
        return reject("VU1 WORLD CANCEL OUTPUT UNSUPPORTED");
#ifdef _EE
    if(!s.world_idle(this))return Vu1WorldResult::failed;
    // Ready proved these clips valid. Revalidate before atomic publication;
    // corruption is a fatal ownership failure, not a retryable numerical case.
    if(!s.world_clips(bank*512,retained.count,clip)){
        s.retire("VU1 WORLD PREPARED CLIP CORRUPTED");return Vu1WorldResult::failed;
    }
    retained={};++w.counters.cancelled_batches;++w.counters.fallback_batches;s.failure="NONE";
    return Vu1WorldResult::fallback;
#else
    return reject("VU1 HARDWARE UNAVAILABLE");
#endif
}
Vu1WorldPrepareResult Vu1Transform::prepare_world(const Vu1WorldGeometry& geometry,Vu1WorldToken& token,
                                                 std::span<TransformVector4> clip) noexcept {
    if(world_pending())return Vu1WorldPrepareResult::rejected;
    invalidate_world();
    if(quarantined)return Vu1WorldPrepareResult::failed;
    if(!storage_||!storage_->ready||!storage_->world||!storage_->world->ready)return Vu1WorldPrepareResult::rejected;
    const auto count=geometry.corners.size();
    if(count>vu1_world_corner_capacity||clip.size()<count||!clip.data()||!aligned(clip.data())||
       ranges_overlap(clip.data(),count*sizeof(TransformVector4),geometry.corners.data(),geometry.corners.size_bytes())||
       ranges_overlap(clip.data(),count*sizeof(TransformVector4),&token,sizeof(token))||
       ranges_overlap(&token,sizeof(token),geometry.corners.data(),geometry.corners.size_bytes())){
        storage_->failure="VU1 WORLD PREPARE INPUT UNSUPPORTED";++storage_->world->counters.rejected_batches;
        return Vu1WorldPrepareResult::rejected;
    }
    Vu1WorldSubmission submission;
    const auto result=submit_prepare_world(geometry,submission);
    if(result==Vu1WorldSubmitResult::failed)return Vu1WorldPrepareResult::failed;
    if(result!=Vu1WorldSubmitResult::submitted)return Vu1WorldPrepareResult::rejected;
    return finish_prepare_world(submission,token,clip);
}
Vu1WorldResult Vu1Transform::emit_world(const Vu1WorldToken& token,std::span<const Vu1WorldState> states,
                                      Vu1WorldCompletion completion) noexcept {
    Vu1WorldSubmission submission;
    const auto result=submit_emit_world(token,states,completion,submission);
    if(result==Vu1WorldSubmitResult::failed)return Vu1WorldResult::failed;
    if(result!=Vu1WorldSubmitResult::submitted)return Vu1WorldResult::rejected;
    return finish_emit_world(submission);
}
bool Vu1Transform::drain_world(const Vu1WorldSubmission& submission) noexcept {
    if(quarantined)return false;
    if(!storage_||!storage_->ready||!storage_->world||!storage_->world->ready)return false;
    auto&w=*storage_->world;
    if(submission.owner_!=this||!submission.ticket_||submission.ticket_!=w.pending.ticket){
        storage_->failure="VU1 WORLD SUBMISSION STALE";return false;
    }
    if(w.pending.kind==2)return finish_emit_world(submission)==Vu1WorldResult::drawn;
    Vu1WorldToken token;std::array<TransformVector4,vu1_world_corner_capacity> clips;
    const auto result=finish_prepare_world(submission,token,clips);
    if(result==Vu1WorldPrepareResult::ready)return cancel_world(token,clips)==Vu1WorldResult::fallback;
    // Our output arguments are valid: rejected here is a completed numerical
    // rejection, not a retained live job. No caller output has been published.
    return result!=Vu1WorldPrepareResult::failed&&!w.pending.ticket;
}
Vu1WorldStats Vu1Transform::world_stats() const noexcept {
    return storage_&&storage_->world?storage_->world->counters:Vu1WorldStats{};
}
bool Vu1Transform::transform_validated(const TransformMatrix&a,const TransformMatrix&b,
                                      std::span<const TransformVector4>input,std::span<TransformVector4>output,
                                      std::span<TransformProjectedVertex>projected) noexcept {
    auto&s=*storage_;
    if(input.empty()){s.failure="NONE";return true;}
#ifdef _EE
    if(!configured()||!idle()){
        // Nothing was submitted by this call: never stop another owner's DMA.
        s.ready=false;quarantined=true;s.failure="VU1 OWNERSHIP OR CONFIGURATION LOST";return false;
    }
    const bool project=!projected.empty();
    if(project&&!s.projection_ready){
        const auto bytes=std::size_t(reinterpret_cast<std::uintptr_t>(native_vu1_project_end)-
                                    reinterpret_cast<std::uintptr_t>(native_vu1_project_begin));
        // MPG's zero count means256 instructions. Keep a single bounded MPG
        // and leave the original entry0 program untouched.
        if(!bytes||bytes%8||bytes>2048||projection_entry+bytes/8>2048){
            s.failure="VU1 PROJECTION PROGRAM SIZE INVALID";return false;
        }
        s.used=0;s.word(command(0x10));
        s.word(command(0x4a,unsigned(bytes/8),projection_entry));
        s.words(native_vu1_project_begin,bytes);
        if(!s.transfer())return false;
        s.projection_ready=true;
    }
    s.used=0;
    s.word(command(0x6c,8,0));s.words(a.data(),sizeof(a));s.words(b.data(),sizeof(b));
    s.word(command(0x6c,1,8));s.word(std::uint32_t(input.size()));s.word(0);s.word(0);s.word(0);
    if(project){
        s.word(command(0x6c,1,9)); // x/y scale, zero and minimum positive w.
        s.word(0x43780000u);s.word(0x43400000u);s.word(0);s.word(0x3a800000u);
    }
    s.word(command(0x6c,unsigned(input.size()),10));s.words(input.data(),input.size_bytes());
    s.word(command(0x14,0,project?projection_entry:0));
    if(!s.transfer())return false;
    const volatile std::uint32_t*memory=reinterpret_cast<const volatile std::uint32_t*>(0x1100c000u+106*16);
    for(std::size_t i=0;i<input.size();++i){
        std::array<std::uint32_t,4> bits;
        for(unsigned c=0;c<4;++c){bits[c]=memory[4*i+c];const auto magnitude=bits[c]&0x7fffffffu;
            if(magnitude && (magnitude<0x00800000u||magnitude>0x47800000u)){
                s.failure="VU1 OUTPUT OUTSIDE BOUNDED DOMAIN";return false;}}
        s.result[i]=std::bit_cast<TransformVector4>(bits);
    }
    if(project){
        memory=reinterpret_cast<const volatile std::uint32_t*>(0x1100c000u+202*16);
        for(std::size_t i=0;i<input.size();++i){
            std::array<std::uint32_t,4>bits;
            for(unsigned c=0;c<4;++c)bits[c]=memory[4*i+c];
            const auto p=std::bit_cast<TransformProjectedVertex>(bits);
            // An inadmissible projection falls back without losing its valid
            // clip vector. No projection value is published before all clip
            // outputs have passed the existing bounded-domain checks.
            s.projected[i]=usable_projection(s.result[i],p)?p:TransformProjectedVertex{};
        }
    }
    std::copy_n(s.result.begin(),input.size(),output.begin());
    if(project)std::copy_n(s.projected.begin(),input.size(),projected.begin());
    ++s.counters.batches;s.counters.vertices+=input.size();s.failure="NONE";return true;
#else
    (void)a;(void)b;(void)output;(void)projected;return false;
#endif
}
bool Vu1Transform::available()const noexcept{return storage_&&storage_->ready;}
const char*Vu1Transform::error()const noexcept{return storage_?storage_->failure:"VU1 NOT INITIALIZED";}
Vu1TransformStats Vu1Transform::stats()const noexcept{return storage_?storage_->counters:Vu1TransformStats{};}
} // namespace ps2
