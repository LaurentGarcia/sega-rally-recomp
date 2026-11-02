#include "native_textured_model.h"
#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace ps2::native;
namespace {
unsigned checks=0;
void require(bool b){++checks;if(!b){std::fprintf(stderr,"TEXTURED_MODEL FAIL check=%u\n",checks);std::exit(1);}}
void put(std::vector<std::uint8_t>&b,std::size_t at,std::uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=std::uint8_t(v>>(8*i));}
std::uint32_t crc(std::span<const std::uint8_t>b){std::uint32_t c=~0u;for(auto v:b){c^=v;for(unsigned i=0;i<8;++i)c=c&1?(c>>1)^0xedb88320u:c>>1;}return ~c;}
void seal(std::vector<std::uint8_t>&b){put(b,36,crc(std::span(b).subspan(64)));}
std::vector<std::uint8_t> original(){
    std::vector<std::uint8_t>b(64+36+40+76+16+32);std::memcpy(b.data(),"RLYTEXM1",8);
    const std::array<std::uint32_t,14>h{1,64,3,1,1,1,32,0,std::uint32_t(b.size()-64),12,40,76,16,0};
    for(unsigned i=0;i<h.size();++i)put(b,8+4*i,h[i]);
    put(b,64+12,0x3f800000);put(b,64+24+4,0x3f800000);
    const unsigned f=100;put(b,f,0);put(b,f+4,1);put(b,f+8,2);put(b,f+20,0x41000000);put(b,f+32,0x41000000);
    const unsigned m=140;put(b,m,0);put(b,m+4,0xffffffff);put(b,m+8,3);
    for(unsigned i=0;i<16;++i)put(b,m+12+4*i,i==15?0:0xff010203+i);
    const unsigned t=216;put(b,t,8);put(b,t+4,8);put(b,t+8,0);put(b,t+12,32);
    for(unsigned i=0;i<32;++i)b[232+i]=std::uint8_t(i);seal(b);return b;
}
}
int main(int argc,char**argv){
    TexturedModel output;TexturedModelError e{};auto valid=original();require(decode_textured_model(valid,output,e));
    auto view=output.model();require(view.vertices.size()==3&&view.faces.size()==1&&view.materials.size()==1);
    require(view.faces[0].uv[1].u==8&&view.faces[0].uv[2].v==8&&view.faces[0].material_index==0);
    require(view.materials[0].checker&&view.materials[0].cutout&&view.materials[0].texture->bytes==32);
    require(view.materials[0].texture->indices[31]==31&&view.materials[0].palette[15]==0);
    const auto retained=view.vertices.data();const auto generation=view.materials[0].texture->generation;
    const auto reject=[&](std::vector<std::uint8_t>b,bool checksum=true){if(checksum)seal(b);require(!decode_textured_model(b,output,e));require(output.model().vertices.data()==retained&&output.model().materials[0].texture->generation==generation);};
    for(auto [offset,value]:std::array<std::pair<unsigned,std::uint32_t>,15>{{{8,2},{12,60},{16,65537},{20,32769},{24,4097},{28,1025},{32,4194305},{40,1},{44,16},{48,44},{52,80},{56,20},{60,1},{100+4,3},{100+36,1}}}){auto b=valid;put(b,offset,value);reject(b);}
    for(auto bits:std::array<std::uint32_t,6>{1,0x007fffff,0x7fc00000,0x7f800000,0x47800001,0xff800000}){
        auto b=valid;put(b,64,bits);reject(b);b=valid;put(b,100+12,bits);reject(b);}
    for(auto [offset,value]:std::array<std::pair<unsigned,std::uint32_t>,8>{{{100+4,0},{140,1},{144,0x7fffffff},{148,4},{152,0x80000000},{216,7},{224,1},{228,31}}}){auto b=valid;put(b,offset,value);reject(b);}
    auto b=valid;b.back()^=1;reject(b,false);b=valid;b.push_back(0);reject(b,false);b=valid;b.pop_back();reject(b,false);
    require(!load_textured_model("/a/path/which/does/not/exist",output,e));require(output.model().vertices.data()==retained);
    require(decode_textured_model(valid,output,e));require(output.model().materials[0].texture->generation!=generation);
    // V2 retains an authored plane independently of triangle winding. V1
    // remains readable and disables facing on replacement.
    b=valid;b.insert(b.begin()+140,28,0);std::memcpy(b.data(),"RLYTEXM2",8);
    put(b,8,2);put(b,40,std::uint32_t(b.size()-64));put(b,48,68);
    put(b,148,0xbf800000);put(b,152,0x40000000);put(b,156,0x40400000);put(b,160,0x40800000);put(b,164,1);seal(b);
    require(decode_textured_model(b,output,e));view=output.model();
    require(view.faces[0].facing.enabled&&view.faces[0].facing.normal.z==-1&&view.faces[0].facing.point.x==2&&view.faces[0].facing.point.y==3&&view.faces[0].facing.point.z==4);
    require(view.materials[0].texture->generation<(std::uint64_t(1)<<63));
    const auto v2_owner=view.vertices.data();
    for(auto [offset,value]:std::array<std::pair<unsigned,std::uint32_t>,5>{{{140,1},{144,0x7f800000},{160,0x47800001},{164,2},{48,40}}}){
        auto bad=b;put(bad,offset,value);seal(bad);require(!decode_textured_model(bad,output,e));require(output.model().vertices.data()==v2_owner&&output.model().faces[0].facing.enabled);}
    require(decode_textured_model(valid,output,e));require(!output.model().faces[0].facing.enabled);
    if(argc==2){require(load_textured_model(argv[1],output,e));view=output.model();require(!view.vertices.empty()&&!view.faces.empty());
        std::printf("TEXTURED_MODEL FILE vertices=%zu faces=%zu materials=%zu bytes=%zu\n",view.vertices.size(),view.faces.size(),view.materials.size(),output.persistent_bytes());}
    std::printf("TEXTURED_MODEL PASS checks=%u\n",checks);return 0;
}
