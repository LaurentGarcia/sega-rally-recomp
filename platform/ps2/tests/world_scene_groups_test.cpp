#include "world_scene.h"
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <vector>

namespace {unsigned allocations=0;}
void* operator new(std::size_t n){++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
namespace {
using namespace ps2::native;
unsigned checks=0,frames=0;std::uint64_t culled=0,transformed=0,reference_transformed=0;
void require(bool value){++checks;if(!value){std::fprintf(stderr,"WORLD_GROUPS FAIL check=%u frame=%u\n",checks,frames);std::exit(1);}}
WorldMatrix identity(){return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
bool same(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
void compare(Frame a,Frame b){
    require(a.clear_color==b.clear_color&&a.depth_test==b.depth_test&&a.triangles.size()==b.triangles.size());
    for(std::size_t i=0;i<a.triangles.size();++i){const auto& x=a.triangles[i];const auto& y=b.triangles[i];
        require(x.material->color==y.material->color&&x.material->palette==y.material->palette&&
            x.material->texture==y.material->texture&&x.material->cutout==y.material->cutout);
        for(unsigned j=0;j<3;++j){const auto p=x.vertices[j],q=y.vertices[j];
            require(same(p.x,q.x)&&same(p.y,q.y)&&same(p.u,q.u)&&same(p.v,q.v)&&same(p.q,q.q)&&same(p.depth,q.depth));
        }
    }
}
void run(){
    std::vector<WorldPoint> points;std::vector<WorldFace> faces;std::vector<WorldFaceGroup> groups;
    for(unsigned n=0;n<96;++n){const float x=float(int(n%12)-6)*4,y=float(int(n/12)-4)*4;
        const auto v=std::uint32_t(points.size()),f=std::uint32_t(faces.size());
        const float z=float(n%5)*.125f;
        points.insert(points.end(),{{x-.25f,y-.25f,z},{x+.25f,y-.25f,z+.125f},{x+.25f,y+.25f,z+.25f},{x-.25f,y+.25f,z+.125f}});
        faces.push_back({{v,v+1,v+2},0xff000000u+n});faces.push_back({{v,v+2,v+3},0xff002000u+n});groups.push_back({f,2});
    }
    WorldScene ordinary,grouped;WorldSceneError error;
    require(ordinary.configure(faces.size(),1536,error,false)&&grouped.configure(faces.size(),1536,error));
    WorldInstance plain{{points,faces},identity()},optimized{{points,faces,groups},identity()};WorldView view{identity()};
    const auto compare_update=[&](){
        ++frames;const auto count=allocations;WorldSceneError a,b;
        const bool first=ordinary.update({&plain,1},view,a),second=grouped.update({&optimized,1},view,b);
        require(first==second&&a==b);require(allocations==count);compare(ordinary.frame(),grouped.frame());
        if(first){const auto stats=grouped.stats();require(stats.source_faces==ordinary.stats().source_faces&&stats.groups==groups.size());
            culled+=stats.culled_groups;transformed+=stats.transformed_vertices;reference_transformed+=ordinary.stats().transformed_vertices;}
    };
    compare_update();require(grouped.stats().culled_groups>48&&grouped.stats().transformed_vertices<288);
    // Fixed random sequence varies affine transforms, perspective, all clipping
    // planes, scale, signed zero and exactly-on-plane cases. The ordinary path
    // supplies the complete ordered vertex/material reference each frame.
    std::uint32_t random=0x64ab1329u;
    const auto next=[&](){random^=random<<13;random^=random>>17;random^=random<<5;return random;};
    for(unsigned i=0;i<2048;++i){
        plain.object_to_world=identity();
        const float sine=float(int(next()%17)-8)*.125f,cosine=float(int(next()%17)-8)*.125f;
        plain.object_to_world[0]=cosine;plain.object_to_world[1]=sine;plain.object_to_world[4]=-sine;plain.object_to_world[5]=cosine;
        plain.object_to_world[12]=float(int(next()%65)-32)*.25f;plain.object_to_world[13]=float(int(next()%65)-32)*.25f;
        plain.object_to_world[14]=float(int(next()%17)-8)*.125f;optimized.object_to_world=plain.object_to_world;
        view.world_to_clip=identity();view.world_to_clip[0]=float((next()%8)+1)*.125f;view.world_to_clip[5]=float((next()%8)+1)*.125f;
        view.world_to_clip[8]=float(int(next()%5)-2)*.125f;view.world_to_clip[9]=float(int(next()%5)-2)*.125f;
        if(i%3==0){view.world_to_clip[11]=1;view.world_to_clip[15]=float(i%5)*.25f;}
        if(i%31==0)view.world_to_clip[14]=-0.0f;
        compare_update();
    }
    // A group outside one plane still must not hide a subnormal or over-limit
    // transformed component in another. Both paths reject identically.
    plain.object_to_world=optimized.object_to_world=identity();view.world_to_clip=identity();view.world_to_clip[12]=128;
    view.world_to_clip[5]=0x1p-126f;compare_update();
    view.world_to_clip=identity();view.world_to_clip[0]=65536;compare_update();
    view.world_to_clip=identity();view.world_to_clip[15]=clip_min_w;compare_update();
    const auto retained=grouped.frame().triangles.data();const auto retained_count=grouped.frame().triangles.size();
    auto malformed=groups;malformed[1].first_face=1;optimized.model.groups=malformed;
    require(!grouped.update({&optimized,1},view,error)&&error==WorldSceneError::invalid_model);
    require(grouped.frame().triangles.data()==retained&&grouped.frame().triangles.size()==retained_count);
    malformed=groups;malformed[1].face_count=0;require(!grouped.update({&optimized,1},view,error));
    malformed=groups;malformed.back().face_count=3;require(!grouped.update({&optimized,1},view,error));
    optimized.model.groups=std::span(groups).first(groups.size()-1);require(!grouped.update({&optimized,1},view,error));
    require(culled>100000&&transformed<reference_transformed/2);
    std::printf("WORLD_GROUPS PASS frames=%u checks=%u culled_groups=%llu transformed_vertices=%llu reference_vertices=%llu no_update_allocations=1\n",
        frames,checks,static_cast<unsigned long long>(culled),static_cast<unsigned long long>(transformed),static_cast<unsigned long long>(reference_transformed));
}
}
int main(){run();}
