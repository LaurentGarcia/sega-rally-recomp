#include "native_textured_model.h"
#include <algorithm>
#include <bit>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>
namespace ps2::native {
struct TexturedModel::Storage {
    std::vector<WorldPoint> vertices;std::vector<WorldFace> faces;
    std::vector<Material> materials;std::vector<Texture> textures;std::vector<std::uint8_t> indices;
};
namespace {
constexpr std::size_t header_bytes=64,max_indices=4*1024*1024;
constexpr std::size_t max_file=header_bytes+65536*12+32768*68+4096*76+1024*16+max_indices;
// Serialized loader-owned identities occupy the low half; prepared models
// reserve the high half. External owners still obey Display invalidation.
constexpr std::uint64_t generation_limit=std::uint64_t(1)<<63;
std::uint64_t next_generation=1;
std::uint32_t u32(const std::uint8_t*p){return std::uint32_t(p[0])|std::uint32_t(p[1])<<8|std::uint32_t(p[2])<<16|std::uint32_t(p[3])<<24;}
float f32(const std::uint8_t*p){return std::bit_cast<float>(u32(p));}
bool scalar(float x){const auto b=std::bit_cast<std::uint32_t>(x)&0x7fffffffu;return !b||(b>=0x00800000u&&b<=0x47800000u);}
bool side(unsigned n){return n>=8&&n<=256&&(n&(n-1))==0;}
std::uint32_t crc32(std::span<const std::uint8_t>b){std::uint32_t c=~0u;for(auto v:b){c^=v;for(unsigned i=0;i<8;++i)c=(c>>1)^(0xedb88320u&std::uint32_t(-std::int32_t(c&1)));}return ~c;}
bool fail(TexturedModelError e,TexturedModelError&out){out=e;return false;}
}
TexturedModel::TexturedModel()noexcept=default;TexturedModel::~TexturedModel()=default;
bool TexturedModel::valid()const noexcept{return bool(storage_);}
WorldModel TexturedModel::model()const noexcept{return storage_?WorldModel{storage_->vertices,storage_->faces,{},storage_->materials}:WorldModel{};}
std::size_t TexturedModel::persistent_bytes()const noexcept{return storage_?sizeof(Storage)+storage_->vertices.capacity()*sizeof(WorldPoint)+storage_->faces.capacity()*sizeof(WorldFace)+storage_->materials.capacity()*sizeof(Material)+storage_->textures.capacity()*sizeof(Texture)+storage_->indices.capacity():0;}
bool decode_textured_model(std::span<const std::uint8_t>data,TexturedModel&output,TexturedModelError&error)noexcept{
    if(data.size()<header_bytes||data.size()>max_file)return fail(TexturedModelError::format,error);
    const auto*h=data.data();const auto version=u32(h+8);
    if((version!=1&&version!=2)||std::memcmp(h,version==1?"RLYTEXM1":"RLYTEXM2",8))return fail(TexturedModelError::format,error);
    const unsigned face_stride=version==1?40:68;
    const auto nv=u32(h+16),nf=u32(h+20),nm=u32(h+24),nt=u32(h+28),nb=u32(h+32);
    if(u32(h+12)!=64||u32(h+44)!=12||u32(h+48)!=face_stride||u32(h+52)!=76||u32(h+56)!=16||u32(h+60))return fail(TexturedModelError::format,error);
    if(nv<3||nv>65536||!nf||nf>32768||!nm||nm>4096||nt>1024||nb>max_indices)return fail(TexturedModelError::limit,error);
    const std::size_t payload=std::size_t(nv)*12+std::size_t(nf)*face_stride+std::size_t(nm)*76+std::size_t(nt)*16+nb;
    if(data.size()!=64+payload||u32(h+40)!=payload)return fail(TexturedModelError::format,error);
    if(crc32(data.subspan(64))!=u32(h+36))return fail(TexturedModelError::checksum,error);
    try{
        auto s=std::make_unique<TexturedModel::Storage>();s->vertices.resize(nv);s->faces.resize(nf);s->materials.resize(nm);s->textures.resize(nt);
        const auto*p=h+64;
        for(auto&v:s->vertices){v={f32(p),f32(p+4),f32(p+8)};p+=12;if(!scalar(v.x)||!scalar(v.y)||!scalar(v.z))return fail(TexturedModelError::geometry,error);}
        for(auto&f:s->faces){for(unsigned i=0;i<3;++i)f.indices[i]=u32(p+4*i);
            for(unsigned i=0;i<3;++i){f.uv[i]={f32(p+12+8*i),f32(p+16+8*i)};if(!scalar(f.uv[i].u)||!scalar(f.uv[i].v))return fail(TexturedModelError::geometry,error);}
            f.material_index=u32(p+36);
            if(version==2){const auto enabled=u32(p+64);if(enabled>1)return fail(TexturedModelError::geometry,error);
                f.facing={{f32(p+40),f32(p+44),f32(p+48)},{f32(p+52),f32(p+56),f32(p+60)},enabled!=0};
                for(unsigned i=0;i<6;++i)if(!scalar(f32(p+40+4*i)))return fail(TexturedModelError::geometry,error);}
            p+=face_stride;
            if(f.material_index>=nm||f.indices[0]>=nv||f.indices[1]>=nv||f.indices[2]>=nv||f.indices[0]==f.indices[1]||f.indices[0]==f.indices[2]||f.indices[1]==f.indices[2])return fail(TexturedModelError::geometry,error);}
        std::vector<std::uint32_t> texture_index(nm);
        for(unsigned j=0;j<nm;++j){auto&m=s->materials[j];texture_index[j]=u32(p);m.color=u32(p+4);const auto flags=u32(p+8);
            if((flags&~3u)||(m.color>>24)!=255||(texture_index[j]!=~0u&&texture_index[j]>=nt))return fail(TexturedModelError::material,error);
            m.cutout=flags&1;m.checker=flags&2;
            for(unsigned i=0;i<16;++i){m.palette[i]=u32(p+12+4*i);const auto a=m.palette[i]>>24;if(a!=0&&a!=255)return fail(TexturedModelError::material,error);}
            p+=76;}
        const auto*descriptors=p;p+=std::size_t(nt)*16;
        s->indices.assign(p,p+nb);std::uint32_t used=0;
        if(next_generation>generation_limit-nt)return fail(TexturedModelError::limit,error);
        for(unsigned i=0;i<nt;++i){const auto*d=descriptors+16*i;const auto w=u32(d),hgt=u32(d+4),offset=u32(d+8),n=u32(d+12);
            if(!side(w)||!side(hgt)||offset!=used||n!=w*hgt/2||n>nb-used)return fail(TexturedModelError::material,error);
            s->textures[i]={s->indices.data()+offset,n,std::uint16_t(w),std::uint16_t(hgt),next_generation+i};used+=n;}
        if(used!=nb)return fail(TexturedModelError::material,error);
        for(unsigned i=0;i<nm;++i)if(texture_index[i]!=~0u)s->materials[i].texture=&s->textures[texture_index[i]];
        next_generation+=nt;output.storage_.swap(s);error=TexturedModelError::none;return true;
    }catch(const std::bad_alloc&){return fail(TexturedModelError::allocation,error);}catch(const std::length_error&){return fail(TexturedModelError::allocation,error);}
}
bool load_textured_model(const char*path,TexturedModel&output,TexturedModelError&error)noexcept{
    if(!path||!*path)return fail(TexturedModelError::io,error);
    FILE*f=std::fopen(path,"rb");if(!f)return fail(TexturedModelError::io,error);
    try{
        if(std::fseek(f,0,SEEK_END)){std::fclose(f);return fail(TexturedModelError::io,error);}
        const auto n=std::ftell(f);
        if(n<0||std::size_t(n)<64||std::size_t(n)>max_file||std::fseek(f,0,SEEK_SET)){std::fclose(f);return fail(TexturedModelError::format,error);}
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(n));const bool ok=std::fread(bytes.data(),1,bytes.size(),f)==bytes.size();
        const bool closed=std::fclose(f)==0;f=nullptr;if(!ok||!closed)return fail(TexturedModelError::io,error);
        return decode_textured_model(bytes,output,error);
    }catch(const std::bad_alloc&){if(f)std::fclose(f);return fail(TexturedModelError::allocation,error);}catch(const std::length_error&){if(f)std::fclose(f);return fail(TexturedModelError::allocation,error);}
}
}
