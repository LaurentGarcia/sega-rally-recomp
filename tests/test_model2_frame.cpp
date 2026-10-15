// ROM-free packet invariants and deterministic migration fingerprint.
#include "app/gpu/model2_frame.h"
#include "runtime/video.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace {
void require(bool value) { if (!value) throw std::runtime_error("Model2 packet invariant"); }
uint64_t digest=1469598103934665603ull;
void bytes(const void *data, size_t size) {
    const auto *p=static_cast<const unsigned char *>(data);
    for (size_t i=0;i<size;++i) digest=(digest^p[i])*1099511628211ull;
}
}
int main() { try {
    std::vector<uint8_t> tiles(0x10000), chars(0x80000), palette(0x4000), translation(0xc000), luma(0x20000,255);
    std::vector<uint32_t> texture(0x80000);
    palette[0x2000]=255;palette[0x2001]=127;
    for(size_t i=0;i<translation.size();i+=2) translation[i]=uint8_t(i/2);
    rt::VideoMem memory{palette.data(),translation.data(),luma.data(),texture.data(),texture.data(),0};
    rt::Video video(tiles.data(),chars.data());video.xhout_w(uint16_t(-84));video.xvout_w(uint16_t(-130));
    app::Model2Frame packet;
    unsigned cases=0;
    for(bool exact:{false,true}) for(int scale:{1,2,4}) for(int count=3;count<=8;++count)
    for(bool reverse:{false,true}) for(bool clipped:{false,true}) {
        rt::GeoPoly polygon;polygon.num_vertices=uint8_t(count);polygon.z=100;polygon.luma=252;polygon.center[1]=384;
        polygon.viewport[0]=clipped?17:0;polygon.viewport[1]=clipped?184:0;
        polygon.viewport[2]=clipped?199:495;polygon.viewport[3]=clipped?367:384;
        const std::array<std::array<float,2>,8> points{{{10.5f,30.5f},{30.5f,10.5f},{60.5f,10.5f},{80.5f,30.5f},
            {80.5f,60.5f},{60.5f,80.5f},{30.5f,80.5f},{10.5f,60.5f}}};
        for(int i=0;i<count;++i) {const auto &p=points[size_t(reverse?count-1-i:i)];polygon.v[i]={p[0],-p[1],{1.f,8.f,8.f}};}
        auto behind=polygon;behind.z=200;
        const std::vector<rt::GeoPoly> polygons{behind,polygon};
        video.set_external_3d(true,true);video.frame_start();video.screen_update(polygons,0,memory);
        packet.set_polygon_scanlines(exact);packet.build(video,496,384,scale);
        require(!packet.vertices().empty());require(!packet.batches().empty());
        if(!exact) require(packet.edges().empty());
        for(const auto &b:packet.batches()) {
            require(b.first+b.count<=packet.vertices().size());require(b.count%3==0);
            require(b.clip.x>=0 && b.clip.y>=0 && b.clip.x+b.clip.w<=496 && b.clip.y+b.clip.h<=384);
        }
        for(const auto &v:packet.vertices()) require(std::isfinite(v.x) && std::isfinite(v.y) && v.depth>=0 && v.depth<1);
        bytes(packet.vertices().data(),packet.vertices().size()*sizeof(app::Model2Frame::PolyVertex));
        bytes(packet.edges().data(),packet.edges().size()*sizeof(app::Model2Frame::PolyEdge));
        bytes(packet.batches().data(),packet.batches().size()*sizeof(app::Model2Frame::Batch));
        ++cases;
    }
    const std::vector<rt::GeoPoly> empty;
    video.frame_start();video.screen_update(empty,0,memory);packet.build(video,496,384,1);
    require(packet.vertices().empty() && packet.edges().empty() && packet.batches().empty());
    // Recorded against the pre-extraction projection body at 9c969c1 with
    // contraction disabled. Includes full vertex/edge/order/scissor bytes.
    require(digest==0xf4d290e762fc3523ull);
    std::printf("Model2 frame: %u cases, packet fingerprint=%016llx; no SDL or GPU\n",cases,static_cast<unsigned long long>(digest));
    return 0;
} catch(const std::exception &e) { std::fprintf(stderr,"%s\n",e.what());return 1; } }
