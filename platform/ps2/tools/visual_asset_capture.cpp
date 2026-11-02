#include "visual_asset_capture.h"
#include "mesh_capture.h"
#include "runtime/video.h"
#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>

namespace ps2::offline {
namespace {
struct Capture {std::filesystem::path directory;std::ofstream polygons;unsigned frame{},objects{},links{},object_links{};bool in_object=false;};
std::unique_ptr<Capture> active;
template<class T> void list(std::ostream&out,const T& values){out<<'[';bool first=true;for(auto value:values){if(!first)out<<',';first=false;out<<value;}out<<']';}
void bytes(const std::filesystem::path&path,const void*data,std::size_t size){
    if(!data)throw std::runtime_error("visual capture missing video memory");
    std::ofstream out(path,std::ios::binary);out.write(static_cast<const char*>(data),size);out.close();
    if(!out)throw std::runtime_error("visual capture memory write failed");
}
}
void begin_visual_asset_frame(const char*directory,unsigned frame){
    if(active)throw std::runtime_error("overlapping visual asset frames");
    // The existing raw mesh observer enforces a new confined build directory.
    begin_mesh_capture(directory,frame);
    auto next=std::make_unique<Capture>();next->directory=directory;next->frame=frame;
    next->polygons.open(next->directory/"materials.jsonl.tmp");
    if(!next->polygons)throw std::runtime_error("cannot open visual materials");
    active=std::move(next);
}
void visual_asset_object(const rt::Geo::geo_state&state,rt::GeoPtr input,std::uint32_t opcode,
                         std::uint32_t points,std::uint32_t headers,std::uint32_t address,std::uint32_t links){
    if(!active)return;
    auto&c=*active;if(c.in_object)throw std::runtime_error("nested visual geometry object");
    capture_mesh_object(state,input,opcode,points,headers,address,links);
    c.in_object=true;c.object_links=0;
}
void visual_asset_polygon(const rt::Geo::raster_state&state,std::uint32_t attributes,
                          const rt::GeoVertex*vertices,const std::uint16_t*headers,unsigned luma,unsigned corners){
    if(!active||!active->in_object)return;
    auto&c=*active;
    if((corners!=3&&corners!=4)||luma>255||++c.links>131072)throw std::runtime_error("visual polygon limit/shape");
    auto&out=c.polygons;
    out<<"{\"schema\":1,\"frame\":"<<c.frame<<",\"object\":"<<c.objects<<",\"link\":"<<c.object_links++
       <<",\"attributes\":"<<attributes<<",\"corners\":"<<corners<<",\"luma\":"<<luma
       <<",\"reverse\":"<<state.reverse<<",\"window\":"<<unsigned(state.cur_window)<<",\"viewport\":";
    list(out,state.viewport);out<<",\"center\":";list(out,state.center[state.center_sel]);
    out<<",\"header\":";list(out,std::array{headers[0],headers[1],headers[2],headers[3]});
    out<<",\"uv_bits\":[";
    for(unsigned i=0;i<corners;++i){if(i)out<<',';list(out,std::array{std::bit_cast<std::uint32_t>(vertices[i].p[1]),std::bit_cast<std::uint32_t>(vertices[i].p[2])});}
    out<<"],\"view_xyz_bits\":[";
    for(unsigned i=0;i<corners;++i){if(i)out<<',';list(out,std::array{std::bit_cast<std::uint32_t>(vertices[i].x),std::bit_cast<std::uint32_t>(vertices[i].y),std::bit_cast<std::uint32_t>(vertices[i].p[0])});}
    out<<"]}\n";if(!out)throw std::runtime_error("cannot write visual material record");
}
void visual_asset_object_end(){if(!active)return;auto&c=*active;if(!c.in_object)throw std::runtime_error("visual object not open");c.in_object=false;++c.objects;}
void end_visual_asset_frame(rt::Video&video){
    if(!active||active->in_object)throw std::runtime_error("incomplete visual asset frame");
    auto&c=*active;const auto&mem=video.gpu_mem();
    bytes(c.directory/"palette.bin",mem.palram,0x4000);
    bytes(c.directory/"translation.bin",mem.colorxlat,0xc000);
    bytes(c.directory/"luma.bin",mem.lumaram,0x20000);
    bytes(c.directory/"texture0.bin",mem.tex0,0x200000);
    bytes(c.directory/"texture1.bin",mem.tex1,0x200000);
    std::ofstream image(c.directory/"reference.ppm",std::ios::binary);
    image<<"P6\n"<<video.width()<<" 384\n255\n";
    for(auto p:video.screen()){image.put(char(p>>16));image.put(char(p>>8));image.put(char(p));}
    image.close();if(!image)throw std::runtime_error("visual reference image write failed");
    // Same offline recompose/restore sequence as gs_scene_capture.cpp. The
    // disabled replay must still match every subsequent frame checkpoint.
    const bool compact=video.compact_external_3d();
    video.set_compact_external_3d(true);
    video.screen_update(video.gpu_polys(),video.gpu_windows(),mem);
    const auto&background=video.background_layer();
    if(background.size()!=496*384)throw std::runtime_error("visual background extent differs");
    bytes(c.directory/"background.bin",background.data(),background.size()*4);
    video.render_compact_reference();video.set_compact_external_3d(compact);
    c.polygons.close();if(!c.polygons)throw std::runtime_error("visual polygon close failed");
    std::filesystem::rename(c.directory/"materials.jsonl.tmp",c.directory/"materials.jsonl");
    end_mesh_capture();
    std::ofstream out(c.directory/"visual.json");
    out<<"{\"schema\":1,\"complete\":true,\"completed_frame_before_run\":"<<c.frame<<",\"objects\":"<<c.objects
       <<",\"links\":"<<c.links<<",\"texture_generation\":"<<mem.tex_generation<<",\"crtc\":["<<video.crtc_x()<<','<<video.crtc_y()
       <<"],\"render\":["<<video.render_x()<<','<<video.render_y()<<"],\"dynamic_lighting_recovered\":false}\n";
    out.close();if(!out)throw std::runtime_error("visual frame completion failed");active.reset();
}
}
