#include "rally_body_scene.h"
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <vector>
namespace {long failure=-1;unsigned allocations=0;}
void* operator new(std::size_t n){++allocations;if(failure==0)throw std::bad_alloc();if(failure>0)--failure;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
namespace {
using namespace ps2::native;
unsigned checks=0;
void require(bool ok){++checks;if(!ok){std::fprintf(stderr,"RALLY_BODY FAIL check=%u\n",checks);std::exit(1);}}
WorldMatrix identity(){return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
void word(std::vector<std::uint8_t>& bytes,unsigned offset,std::uint32_t value){for(unsigned i=0;i<4;++i)bytes[offset+i]=std::uint8_t(value>>(8*i));}
void crc(std::vector<std::uint8_t>& bytes){std::uint32_t value=~0u;for(unsigned i=64;i<bytes.size();++i){value^=bytes[i];for(unsigned k=0;k<8;++k)value=(value>>1)^(0xedb88320u&(0u-(value&1u)));}word(bytes,28,~value);}
std::vector<std::uint8_t> model(){
    std::vector<std::uint8_t> data(116);const char magic[]="RLYBODY";
    for(unsigned i=0;i<8;++i)data[i]=magic[i];
    for(auto pair:{std::pair{8u,1u},{12u,64u},{16u,3u},{20u,1u},{24u,52u},{32u,12u},{36u,16u}})word(data,pair.first,pair.second);
    const std::array<float,9> vertices{-.25f,-.25f,.5f,.25f,-.25f,.5f,0,.25f,.5f};
    for(unsigned i=0;i<9;++i)word(data,64+4*i,std::bit_cast<std::uint32_t>(vertices[i]));
    word(data,100,0);word(data,104,1);word(data,108,2);word(data,112,0xffb3c4d5);crc(data);return data;
}
void run(){
    auto data=model();RallyBodyScene scene;RallyBodyError error;
    const auto bridge=identity();WorldView view{identity()};RallyBodyPose pose{};
    require(!scene.update(pose,view,error)&&error==RallyBodyError::not_loaded);
    require(scene.decode(data,bridge,error));require(scene.frame().triangles.empty());
    const auto before_allocations=allocations;
    require(scene.update(pose,view,error));require(allocations==before_allocations);
    require(scene.frame().triangles.size()==1&&scene.frame().depth_test);
    const auto first=scene.frame().triangles[0];pose.position.x=.25f;
    require(scene.update(pose,view,error));require(allocations==before_allocations);
    for(unsigned i=0;i<3;++i)require(std::fabs(scene.frame().triangles[0].vertices[i].x-first.vertices[i].x-62)<.0001f);
    // A fixed collision quad remains in the same world coordinates while the
    // separately supplied body pose changes. No camera recentering occurs.
    ContactPolygon polygon;polygon.attributes=4;polygon.vertices={ContactVertex{-.5f,-.5f,.75f},ContactVertex{.5f,-.5f,.75f},ContactVertex{.5f,.5f,.75f},ContactVertex{-.5f,.5f,.75f}};
    const ContactSegment segment{{&polygon,1}};
    require(scene.attach_course({&segment,1},error));require(scene.frame().triangles.empty());
    require(scene.update(pose,view,error));require(scene.frame().triangles.size()==3&&scene.stats().instances==2);
    const auto fixed=scene.frame().triangles[0];const auto body=scene.frame().triangles[2];
    const auto current_allocations=allocations;pose.position.x=0;
    require(scene.update(pose,view,error));require(allocations==current_allocations);
    for(unsigned i=0;i<3;++i){
        require(scene.frame().triangles[0].vertices[i].x==fixed.vertices[i].x);
        require(std::fabs(scene.frame().triangles[2].vertices[i].x-body.vertices[i].x+62)<.0001f);
    }
    auto* retained=scene.frame().triangles.data();
    const auto preserved=[&](){require(scene.frame().triangles.data()==retained&&scene.frame().triangles.size()==3);};
    auto invalid=bridge;invalid[3]=1;require(!scene.decode(data,invalid,error));preserved();
    for(auto change:{std::pair{8u,2u},{16u,65537u},{20u,16385u},{40u,1u},{64u,0x7fc12345u},{100u,3u},{112u,0x7fb3c4d5u}}){
        auto bad=data;word(bad,change.first,change.second);crc(bad);require(!scene.decode(bad,bridge,error));preserved();
    }
    auto bad=data;bad.back()^=1;require(!scene.decode(bad,bridge,error));preserved();
    for(long i=0;i<9;++i){failure=i;const auto ok=scene.decode(data,bridge,error);failure=-1;require(!ok&&error==RallyBodyError::allocation_failed);preserved();}
    pose.orientation.x=std::bit_cast<float>(0x7fc12345u);require(!scene.update(pose,view,error));preserved();pose.orientation.x=0;
    auto bad_view=view;bad_view.world_to_clip[0]=std::bit_cast<float>(1u);require(!scene.update(pose,bad_view,error));preserved();
    polygon.vertices[0].x=std::bit_cast<float>(0x7fc12345u);require(!scene.attach_course({&segment,1},error));preserved();polygon.vertices[0].x=-.5f;
    for(long i=0;i<8;++i){failure=i;const auto ok=scene.attach_course({&segment,1},error);failure=-1;require(!ok&&error==RallyBodyError::allocation_failed);preserved();}
    // A triangle's unused fourth corner is arbitrary payload, not geometry.
    polygon.attributes=0;polygon.vertices[3].x=std::bit_cast<float>(0x7fc12345u);
    require(scene.attach_course({&segment,1},error));require(scene.update(pose,view,error));
    require(scene.stats().vertices==6&&scene.stats().source_faces==2&&scene.frame().triangles.size()==2);
    polygon.attributes=4;require(!scene.attach_course({&segment,1},error)&&error==RallyBodyError::invalid_geometry);
    require(scene.stats().vertices==6&&scene.frame().triangles.size()==2);
    require(scene.attach_course({},error));require(scene.update(pose,view,error));require(scene.stats().instances==1);
    require(!scene.load(nullptr,bridge,error)&&error==RallyBodyError::invalid_file);
    require(scene.persistent_bytes()>116);
    std::printf("RALLY_BODY PASS checks=%u fixed_translation_pixels=62 course_stationary=1 no_update_allocations=1\n",checks);
}
}
int main(int argc,char** argv){run();if(argc==2){RallyBodyScene body;RallyBodyError error;auto view=identity();view[0]=.25f;view[5]=.25f;view[10]=.125f;view[14]=.5f;
    require(body.load(argv[1],identity(),error));require(body.update({},WorldView{view},error));
    const auto before=body.frame().triangles[0].vertices[0].x;const auto allocated=allocations;
    require(body.update({{.25f,0,0},{}},WorldView{view},error));require(allocations==allocated);
    const auto delta=body.frame().triangles[0].vertices[0].x-before;
    require(std::fabs(delta-15.5f)<.0001f);
    std::printf("RALLY_BODY PRIVATE vertices=%u faces=%u triangles=%u persistent_bytes=%zu translation_pixels=%.1f\n",body.stats().vertices,body.stats().source_faces,body.stats().output_triangles,body.persistent_bytes(),double(delta));
}return 0;}
