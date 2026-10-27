// Original data exercises ownership, malformed input and real typed consumers.
#include "driving_assets.h"
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <utility>
#include <vector>
namespace { long fail_after=-1; }
void* operator new(std::size_t n){if(fail_after==0)throw std::bad_alloc();if(fail_after>0)--fail_after;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
namespace {
using namespace ps2::native;using Bytes=std::vector<std::uint8_t>;unsigned checks=0;
void require(bool value){++checks;if(!value){std::fprintf(stderr,"DRIVING_ASSETS FAIL check=%u\n",checks);std::exit(1);}}
void set(Bytes& b,std::size_t p,std::uint32_t x){for(unsigned i=0;i<4;++i)b[p+i]=std::uint8_t(x>>(8*i));}
void append(Bytes& b,std::uint32_t x){const auto p=b.size();b.resize(p+4);set(b,p,x);}
std::uint32_t crc(const Bytes& b){std::uint32_t c=~0u;for(std::size_t i=96;i<b.size();++i){c^=b[i];for(unsigned k=0;k<8;++k)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}return ~c;}
void seal(Bytes& b){set(b,44,crc(b));}
std::uint64_t typed_hash(const DrivingAssets& a){std::uint64_t h=14695981039346656037ull;
    const auto word=[&](std::uint32_t w,unsigned n=4){for(unsigned i=0;i<n;++i){h^=std::uint8_t(w>>(8*i));h*=1099511628211ull;}};
    const auto value=[&](float x){word(std::bit_cast<std::uint32_t>(x));};
    for(auto v:a.selection_vertices()){value(v.x);value(v.z);}
    for(auto q:a.selection_segments())for(auto index:q)word(index,2);
    unsigned first=0;for(auto s:a.course()){word(first);word(std::uint32_t(s.polygons.size()));first+=s.polygons.size();}
    for(auto s:a.course())for(const auto& p:s.polygons){for(auto v:p.vertices){value(v.x);value(v.y);value(v.z);}for(auto c:p.coefficients)value(c);word(p.attributes);}
    for(auto p:a.normal_table()){value(p.slope);value(p.intercept);}
    for(auto r:*a.parameters())for(auto w:r.words)word(w);
    for(auto p:*a.local_points()){value(p.x);value(p.y);value(p.z);}
    const auto& balance=*a.balance_assets(0);
    for(auto x:balance.limit_mix)value(x);for(auto x:balance.balance_shape)value(x);value(balance.lateral_deadzone);
    for(auto x:balance.cabinet_center)value(x);for(auto x:balance.cabinet_low)value(x);for(auto x:balance.cabinet_high)value(x);
    value(balance.cabinet_delta_gain);value(balance.cabinet_force_gain);value(balance.ratio_scale);
    for(auto x:balance.gear_ratios)value(x);
    for(auto x:{balance.inertia_base,balance.inertia_ratio_scale,balance.friction,balance.drive_scale,balance.brake_scale,balance.feedback_rate_scale,balance.slip_boost})value(x);
    for(auto r:balance.sound_parameters)for(auto x:r)word(x);for(auto x:balance.occasional_commands)word(x);
    for(unsigned i=0;i<2;++i)for(auto x:a.balance_assets(i)->power_curve)value(x);
    for(auto list:a.motion_assets()->commands)for(auto x:list)word(x);value(a.motion_assets()->scalar);
    for(auto p:a.motion_assets()->local_points){value(p.x);value(p.y);value(p.z);}
    for(auto r:a.sound_commands())for(auto x:r.bytes)word(x,1);
    for(auto direction:a.course_directions()){value(direction.x);value(direction.z);}return h;
}
Bytes fixture(){Bytes b(96);std::memcpy(b.data(),"RLYDRV3",8);
    set(b,8,3);set(b,12,96);set(b,16,3);set(b,20,4);set(b,24,1);set(b,28,1);
    set(b,32,8192);set(b,36,10);set(b,40,4);
    set(b,56,2);
    const std::array<std::array<std::uint32_t,2>,4> xz{{{0xbf800000u,0xbf800000u},{0x3f800000u,0xbf800000u},{0x3f800000u,0x3f800000u},{0xbf800000u,0x3f800000u}}};
    for(auto p:xz)for(auto w:p)append(b,w);
    append(b,0x00010000);append(b,0x00030002);append(b,0);append(b,1);
    for(auto p:xz){append(b,p[0]);append(b,0x7fc12345);append(b,p[1]);}
    for(auto w:{0xff800001u,0x80000000u,0x7f800000u,0x01234567u,0xabcdef80u})append(b,w);
    for(unsigned i=0;i<8192;++i){append(b,0x3f000000);append(b,0x40000000);}
    for(unsigned i=0;i<130;++i)append(b,0x7f800001u+i);
    for(unsigned i=0;i<12;++i)append(b,i==0?0x80000000:0);
    for(unsigned i=0;i<260;++i)append(b,i<212?0x3f000000u+i:0x7f800000u+i);
    for(unsigned i=0;i<164;++i)append(b,0x40000000u+i);
    for(unsigned i=0;i<64;++i)append(b,0x7f800100u+i);append(b,0x3f000400u);
    for(unsigned i=0;i<12;++i)append(b,0x3f000500u+i);
    b.insert(b.end(),{0,127,255,255,128,0});
    append(b,0x80000000);append(b,0x3f800000);
    set(b,48,std::uint32_t(b.size()-96));seal(b);return b;
}
void run(){auto bytes=fixture();DrivingAssets assets;DrivingAssetError error{};
    require(!assets.loaded()&&!assets.parameters()&&!assets.local_points()&&assets.course().empty()&&!assets.balance_assets(0)&&!assets.motion_assets()&&assets.sound_commands().empty());
    require(decode_driving_assets(bytes,assets,error)&&error==DrivingAssetError::none);
    require(assets.selector()==3&&assets.course().size()==1&&assets.selection_vertices().size()==4&&assets.normal_table().size()==8192);
    const auto* original=assets.course().data();const auto* polygons=original[0].polygons.data();const auto original_crc=assets.payload_crc32();
    require(std::bit_cast<std::uint32_t>(polygons[0].vertices[0].y)==0x7fc12345);
    require(std::bit_cast<std::uint32_t>(polygons[0].coefficients[0])==0xff800001);
    require(assets.parameters()->at(9).words[12]==0x7f800001u+129);
    require(std::bit_cast<std::uint32_t>(assets.local_points()->at(0).x)==0x80000000);
    require(assets.balance_assets(0)&&assets.balance_assets(1)&&!assets.balance_assets(2)&&assets.balance_assets(0)->power_curve.size()==82);
    require(assets.balance_assets(0)->power_curve.data()!=assets.balance_assets(1)->power_curve.data());
    require(assets.balance_assets(1)->parameters[0].words==assets.parameters()->at(0).words);
    require(std::bit_cast<std::uint32_t>(assets.balance_assets(1)->power_curve[81])==0x400000a3);
    require(assets.sound_commands().size()==2&&assets.sound_commands()[0].bytes[2]==255);
    require(assets.course_directions().size()==1&&std::bit_cast<std::uint32_t>(assets.course_directions()[0].x)==0x80000000&&assets.course_directions()[0].z==1);
    SegmentSelection selection;require(select_course_segment(assets.selection_vertices(),assets.selection_segments(),{0,0},0,selection)&&selection.selected==0);
    ContactSearchResult contact;require(search_course_contact(assets.course(),0,{0,0,0},contact)&&contact.matched&&contact.data.attributes==0xabcdef80);
    const auto tuning=select_vehicle_parameters(1u<<9,*assets.parameters());require(tuning.index==9&&tuning.record.words==assets.parameters()->at(9).words);
    auto rejected=[&](Bytes bad,DrivingAssetError expected,bool repair=true){if(repair&&bad.size()>=96)seal(bad);require(!decode_driving_assets(bad,assets,error)&&error==expected);
        require(assets.course().data()==original&&assets.course()[0].polygons.data()==polygons&&assets.payload_crc32()==original_crc);};
    rejected(Bytes(95),DrivingAssetError::wrong_size,false);
    auto bad=bytes;bad[0]^=1;rejected(bad,DrivingAssetError::invalid_header);
    bad=bytes;set(bad,8,4);rejected(bad,DrivingAssetError::unsupported_version);
    for(unsigned offset:{12u,16u,20u,24u,28u,32u,36u,40u,48u,52u,56u,60u,92u}){bad=bytes;set(bad,offset,offset==16?8:0xffffffffu);rejected(bad,(offset==20||offset==24||offset==28||offset==56)?DrivingAssetError::limit_exceeded:DrivingAssetError::invalid_header);}
    for(unsigned offset:{20u,24u,28u}){bad=bytes;set(bad,offset,0);rejected(bad,DrivingAssetError::invalid_header);}
    bad=bytes;bad.pop_back();rejected(bad,DrivingAssetError::wrong_size,false);
    bad=bytes;bad.push_back(0);rejected(bad,DrivingAssetError::wrong_size,false);
    bad=bytes;bad.back()^=1;rejected(bad,DrivingAssetError::checksum_mismatch,false);
    for(auto bits:{0x7fc00000u,0x7f800000u,0x49800001u}){bad=bytes;set(bad,96,bits);rejected(bad,DrivingAssetError::invalid_vertex);}
    bad=bytes;bad[128]=4;rejected(bad,DrivingAssetError::invalid_indices);
    for(auto count:{0u,2u,256u}){bad=bytes;set(bad,140,count);rejected(bad,DrivingAssetError::invalid_segments);}
    bad=bytes;set(bad,136,1);rejected(bad,DrivingAssetError::invalid_segments);
    bad=bytes;set(bad,144,0x7fc00000);rejected(bad,DrivingAssetError::invalid_polygon);
    for(auto bits:{0x3dffffffu,0x3f800001u,0xbf000000u,0x7fc00000u}){bad=bytes;set(bad,212,bits);rejected(bad,DrivingAssetError::invalid_normals);}
    for(auto bits:{0x3f7fffffu,0x40800001u}){bad=bytes;set(bad,216,bits);rejected(bad,DrivingAssetError::invalid_normals);}
    bad=bytes;set(bad,66268,0x7f800000);rejected(bad,DrivingAssetError::invalid_local_points);
    for(unsigned offset:{66316u,66316u+1040,66316u+1696+256}){bad=bytes;set(bad,offset,0x7f800001);rejected(bad,DrivingAssetError::invalid_vehicle_assets);}
    bad=bytes;set(bad,66316+1696+260,0x7f800000);rejected(bad,DrivingAssetError::invalid_local_points);
    for(auto raw:{0x7f800000u,0x7fc12345u,0x49800001u}){bad=bytes;set(bad,bad.size()-8,raw);rejected(bad,DrivingAssetError::invalid_course_direction);}
    for(long allocation=0;allocation<8;++allocation){fail_after=allocation;const bool ok=decode_driving_assets(bytes,assets,error);fail_after=-1;
        require(!ok&&error==DrivingAssetError::allocation_failed&&assets.course().data()==original&&assets.payload_crc32()==original_crc);}
    DrivingAssets moved=std::move(assets);require(!assets.loaded()&&moved.course().data()==original&&moved.course()[0].polygons.data()==polygons);
    require(std::bit_cast<std::uint32_t>(moved.balance_assets(1)->power_curve[81])==0x400000a3);
    assets=std::move(moved);require(!moved.loaded()&&assets.course().data()==original);
    require(!load_driving_assets_file(nullptr,assets,error)&&error==DrivingAssetError::invalid_path);
    require(!load_driving_assets_file("",assets,error)&&error==DrivingAssetError::invalid_path);
    bad=bytes;set(bad,16,4);seal(bad);require(decode_driving_assets(bad,assets,error)&&assets.selector()==4&&assets.course().data()!=original);
    std::printf("DRIVING_ASSETS PASS checks=%u storage_bytes=%zu\n",checks,assets.storage_bytes());
}
}
int main(int argc,char** argv){run();if(argc==2){ps2::native::DrivingAssets data;ps2::native::DrivingAssetError error;
    if(!ps2::native::load_driving_assets_file(argv[1],data,error)){std::fprintf(stderr,"LOAD FAIL %s\n",ps2::native::driving_asset_error_message(error));return 1;}
    std::size_t polygons=0;for(auto s:data.course())polygons+=s.polygons.size();
    std::printf("DRIVING_ASSETS FILE PASS selector=%u vertices=%zu segments=%zu polygons=%zu crc=%08x storage_bytes=%zu payload_hash=%016llx\n",unsigned(data.selector()),data.selection_vertices().size(),data.course().size(),polygons,unsigned(data.payload_crc32()),data.storage_bytes(),(unsigned long long)typed_hash(data));
}return 0;}
