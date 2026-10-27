#include "driving_assets.h"
#include <bit>
#include <cstdio>
#include <cstring>
#include <new>
#include <stdexcept>
#include <vector>

namespace ps2::native {
struct DrivingAssets::Storage {
    std::vector<ContactPointXZ> vertices,directions;
    std::vector<CourseSegmentCorners> quads;
    std::vector<ContactPolygon> polygons;
    std::vector<ContactSegment> segments;
    std::vector<ContactNormalPair> normals;
    std::array<VehicleParameterRecord,10> parameters{};
    std::array<AffinePoint3,4> local{};
    std::array<MainBalanceAssets,2> balance{};
    std::array<std::array<float,82>,2> power{};
    DrivingMotionAssets motion{};
    std::vector<DrivingSoundRow> sounds;
    std::uint32_t selector=0,crc=0;
};
namespace {
struct Header {std::uint32_t selector,vertices,segments,polygons,crc,payload,sounds;};
std::uint32_t word(const std::uint8_t* p){return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);}
float scalar(const std::uint8_t* p){return std::bit_cast<float>(word(p));}
bool finite_bound(std::uint32_t bits){return (bits&0x7fffffffu)<=0x49800000u;}
bool fail(DrivingAssetError reason,DrivingAssetError& error){error=reason;return false;}
std::uint32_t crc32(std::span<const std::uint8_t> b){std::uint32_t crc=0xffffffffu;for(auto v:b){crc^=v;for(unsigned i=0;i<8;++i)crc=(crc>>1)^(0xedb88320u&(0u-(crc&1)));}return crc^0xffffffffu;}
bool header(std::span<const std::uint8_t> b,Header& h,DrivingAssetError& e){
    if(b.size()<driving_header_bytes)return fail(DrivingAssetError::wrong_size,e);
    if(std::memcmp(b.data(),"RLYDRV3",8))return fail(DrivingAssetError::invalid_header,e);
    if(word(b.data()+8)!=3)return fail(DrivingAssetError::unsupported_version,e);
    if(word(b.data()+12)!=driving_header_bytes||word(b.data()+32)!=8192||word(b.data()+36)!=10||word(b.data()+40)!=4||word(b.data()+52))return fail(DrivingAssetError::invalid_header,e);
    for(unsigned i=60;i<driving_header_bytes;++i)if(b[i])return fail(DrivingAssetError::invalid_header,e);
    h={word(b.data()+16),word(b.data()+20),word(b.data()+24),word(b.data()+28),word(b.data()+44),word(b.data()+48),word(b.data()+56)};
    if(h.selector>=8||!h.vertices||!h.segments||!h.polygons)return fail(DrivingAssetError::invalid_header,e);
    if(h.vertices>driving_vertex_limit||h.segments>driving_segment_limit||h.polygons>driving_polygon_limit||!h.sounds||h.sounds>driving_sound_row_limit)return fail(DrivingAssetError::limit_exceeded,e);
    const std::size_t expected=std::size_t(h.vertices)*8+std::size_t(h.segments)*24+std::size_t(h.polygons)*68+8192*8+10*52+4*12+501*4+std::size_t(h.sounds)*3;
    if(h.payload!=expected)return fail(DrivingAssetError::invalid_header,e);
    return true;
}
}
DrivingAssets::DrivingAssets() noexcept=default;
DrivingAssets::~DrivingAssets()=default;
DrivingAssets::DrivingAssets(DrivingAssets&&) noexcept=default;
DrivingAssets& DrivingAssets::operator=(DrivingAssets&&) noexcept=default;
bool DrivingAssets::loaded()const noexcept{return bool(storage_);}
std::uint32_t DrivingAssets::selector()const noexcept{return storage_?storage_->selector:0;}
std::uint32_t DrivingAssets::payload_crc32()const noexcept{return storage_?storage_->crc:0;}
std::size_t DrivingAssets::storage_bytes()const noexcept{return storage_?sizeof(Storage)+(storage_->vertices.capacity()+storage_->directions.capacity())*sizeof(ContactPointXZ)+storage_->quads.capacity()*sizeof(CourseSegmentCorners)+storage_->polygons.capacity()*sizeof(ContactPolygon)+storage_->segments.capacity()*sizeof(ContactSegment)+storage_->normals.capacity()*sizeof(ContactNormalPair)+storage_->sounds.capacity()*sizeof(DrivingSoundRow):0;}
std::span<const ContactPointXZ> DrivingAssets::selection_vertices()const noexcept{return storage_?std::span<const ContactPointXZ>(storage_->vertices):std::span<const ContactPointXZ>{};}
std::span<const CourseSegmentCorners> DrivingAssets::selection_segments()const noexcept{return storage_?std::span<const CourseSegmentCorners>(storage_->quads):std::span<const CourseSegmentCorners>{};}
std::span<const ContactPointXZ> DrivingAssets::course_directions()const noexcept{return storage_?std::span<const ContactPointXZ>(storage_->directions):std::span<const ContactPointXZ>{};}
std::span<const ContactSegment> DrivingAssets::course()const noexcept{return storage_?std::span<const ContactSegment>(storage_->segments):std::span<const ContactSegment>{};}
std::span<const ContactNormalPair> DrivingAssets::normal_table()const noexcept{return storage_?std::span<const ContactNormalPair>(storage_->normals):std::span<const ContactNormalPair>{};}
const std::array<VehicleParameterRecord,10>* DrivingAssets::parameters()const noexcept{return storage_?&storage_->parameters:nullptr;}
const std::array<AffinePoint3,4>* DrivingAssets::local_points()const noexcept{return storage_?&storage_->local:nullptr;}
const MainBalanceAssets* DrivingAssets::balance_assets(unsigned variant)const noexcept{return storage_&&variant<2?&storage_->balance[variant]:nullptr;}
const DrivingMotionAssets* DrivingAssets::motion_assets()const noexcept{return storage_?&storage_->motion:nullptr;}
std::span<const DrivingSoundRow> DrivingAssets::sound_commands()const noexcept{return storage_?std::span<const DrivingSoundRow>(storage_->sounds):std::span<const DrivingSoundRow>{};}
const char* driving_asset_error_message(DrivingAssetError e)noexcept{
    switch(e){case DrivingAssetError::none:return "NONE";case DrivingAssetError::invalid_path:return "EMPTY DRIVING ASSET PATH";
    case DrivingAssetError::open_failed:return "DRIVING ASSET OPEN FAILED";case DrivingAssetError::read_failed:return "DRIVING ASSET READ FAILED";
    case DrivingAssetError::invalid_header:return "INVALID DRIVING ASSET HEADER";case DrivingAssetError::unsupported_version:return "UNSUPPORTED DRIVING ASSET VERSION";
    case DrivingAssetError::limit_exceeded:return "DRIVING ASSET COUNT LIMIT";case DrivingAssetError::wrong_size:return "DRIVING ASSET SIZE MISMATCH";
    case DrivingAssetError::checksum_mismatch:return "DRIVING ASSET CHECKSUM MISMATCH";case DrivingAssetError::invalid_vertex:return "INVALID DRIVING VERTEX";
    case DrivingAssetError::invalid_indices:return "INVALID SEGMENT CORNERS";case DrivingAssetError::invalid_segments:return "INVALID COURSE RANGES";
    case DrivingAssetError::invalid_polygon:return "INVALID CONTACT POLYGON XZ";case DrivingAssetError::invalid_normals:return "INVALID CONTACT NORMAL TABLE";
    case DrivingAssetError::invalid_local_points:return "INVALID LOCAL CONTACT POINTS";
    case DrivingAssetError::invalid_vehicle_assets:return "INVALID VEHICLE ASSET VALUE";
    case DrivingAssetError::invalid_course_direction:return "INVALID COURSE DIRECTION";default:return "DRIVING ASSET ALLOCATION FAILED";}
}
bool decode_driving_assets(std::span<const std::uint8_t> b,DrivingAssets& output,DrivingAssetError& error)noexcept{
    Header h{};if(!header(b,h,error))return false;
    if(b.size()!=driving_header_bytes+h.payload)return fail(DrivingAssetError::wrong_size,error);
    if(crc32(b.subspan(driving_header_bytes))!=h.crc)return fail(DrivingAssetError::checksum_mismatch,error);
    try{
        auto data=std::make_unique<DrivingAssets::Storage>();
        data->vertices.resize(h.vertices);data->quads.resize(h.segments);data->polygons.resize(h.polygons);
        data->segments.resize(h.segments);data->normals.resize(8192);data->sounds.resize(h.sounds);data->directions.resize(h.segments);data->selector=h.selector;data->crc=h.crc;
        std::size_t offset=driving_header_bytes;
        for(auto& v:data->vertices){if(!finite_bound(word(b.data()+offset))||!finite_bound(word(b.data()+offset+4)))return fail(DrivingAssetError::invalid_vertex,error);v={scalar(b.data()+offset),scalar(b.data()+offset+4)};offset+=8;}
        for(auto& q:data->quads){for(auto& index:q){index=std::uint16_t(b[offset]|(unsigned(b[offset+1])<<8));offset+=2;if(index>=h.vertices)return fail(DrivingAssetError::invalid_indices,error);}}
        std::uint32_t next=0;
        for(auto& segment:data->segments){const auto first=word(b.data()+offset),count=word(b.data()+offset+4);offset+=8;
            if(first!=next||!count||count>255||count>h.polygons-next)return fail(DrivingAssetError::invalid_segments,error);
            segment.polygons={data->polygons.data()+first,count};next+=count;}
        if(next!=h.polygons)return fail(DrivingAssetError::invalid_segments,error);
        for(auto& polygon:data->polygons){for(auto& v:polygon.vertices){if(!finite_bound(word(b.data()+offset))||!finite_bound(word(b.data()+offset+8)))return fail(DrivingAssetError::invalid_polygon,error);
                v={scalar(b.data()+offset),scalar(b.data()+offset+4),scalar(b.data()+offset+8)};offset+=12;}
            for(auto& coefficient:polygon.coefficients){coefficient=scalar(b.data()+offset);offset+=4;}polygon.attributes=word(b.data()+offset);offset+=4;}
        for(auto& n:data->normals){const auto slope=word(b.data()+offset),intercept=word(b.data()+offset+4);
            if(slope<0x3e000000u||slope>0x3f800000u||intercept<0x3f800000u||intercept>0x40800000u)return fail(DrivingAssetError::invalid_normals,error);
            n={std::bit_cast<float>(slope),std::bit_cast<float>(intercept)};offset+=8;}
        for(auto& record:data->parameters)for(auto& value:record.words){value=word(b.data()+offset);offset+=4;}
        for(auto& p:data->local){for(unsigned k=0;k<3;++k)if(!finite_bound(word(b.data()+offset+4*k)))return fail(DrivingAssetError::invalid_local_points,error);
            p={scalar(b.data()+offset),scalar(b.data()+offset+4),scalar(b.data()+offset+8)};offset+=12;}
        // Decode finite value assets without depending on host floating-point
        // classification (the EE treats some exceptional values differently).
        bool valid=true;const auto next_float=[&](){const auto raw=word(b.data()+offset);offset+=4;
            valid&=(raw&0x7fffffffu)<0x7f800000u;return std::bit_cast<float>(raw);};
        auto& a=data->balance[0];a.parameters=data->parameters;
        for(auto& x:a.limit_mix)x=next_float();for(auto& x:a.balance_shape)x=next_float();a.lateral_deadzone=next_float();
        for(auto& x:a.cabinet_center)x=next_float();for(auto& x:a.cabinet_low)x=next_float();for(auto& x:a.cabinet_high)x=next_float();
        a.cabinet_delta_gain=next_float();a.cabinet_force_gain=next_float();a.ratio_scale=next_float();
        for(auto& x:a.gear_ratios)x=next_float();
        a.inertia_base=next_float();a.inertia_ratio_scale=next_float();a.friction=next_float();a.drive_scale=next_float();
        a.brake_scale=next_float();a.feedback_rate_scale=next_float();a.slip_boost=next_float();
        for(auto& row:a.sound_parameters)for(auto& x:row){x=word(b.data()+offset);offset+=4;}
        for(auto& x:a.occasional_commands){x=word(b.data()+offset);offset+=4;}
        for(auto& curve:data->power)for(auto& x:curve)x=next_float();
        data->balance[1]=a;for(unsigned i=0;i<2;++i)data->balance[i].power_curve=data->power[i];
        for(auto& list:data->motion.commands)for(auto& x:list){x=word(b.data()+offset);offset+=4;}
        data->motion.scalar=next_float();
        for(auto& p:data->motion.local_points){for(unsigned k=0;k<3;++k)if(!finite_bound(word(b.data()+offset+4*k)))return fail(DrivingAssetError::invalid_local_points,error);
            p={next_float(),next_float(),next_float()};}
        if(!valid)return fail(DrivingAssetError::invalid_vehicle_assets,error);
        for(auto& row:data->sounds)for(auto& byte:row.bytes)byte=b[offset++];
        for(auto& direction:data->directions){
            if(!finite_bound(word(b.data()+offset))||!finite_bound(word(b.data()+offset+4)))return fail(DrivingAssetError::invalid_course_direction,error);
            direction={scalar(b.data()+offset),scalar(b.data()+offset+4)};offset+=8;
        }
        if(offset!=b.size())return fail(DrivingAssetError::wrong_size,error);
        output.storage_.swap(data);error=DrivingAssetError::none;return true;
    }catch(const std::bad_alloc&){return fail(DrivingAssetError::allocation_failed,error);}
    catch(const std::length_error&){return fail(DrivingAssetError::allocation_failed,error);}
}
bool load_driving_assets_file(const char* path,DrivingAssets& output,DrivingAssetError& error)noexcept{
    if(!path||!*path)return fail(DrivingAssetError::invalid_path,error);
    struct File{std::FILE* f;~File(){if(f)std::fclose(f);}} file{std::fopen(path,"rb")};
    if(!file.f)return fail(DrivingAssetError::open_failed,error);
    std::array<std::uint8_t,driving_header_bytes> prefix{};
    if(std::fread(prefix.data(),1,prefix.size(),file.f)!=prefix.size())return fail(DrivingAssetError::read_failed,error);
    Header h{};if(!header(prefix,h,error))return false;
    try{std::vector<std::uint8_t> bytes(driving_header_bytes+h.payload);std::memcpy(bytes.data(),prefix.data(),prefix.size());
        if(std::fread(bytes.data()+driving_header_bytes,1,h.payload,file.f)!=h.payload)return fail(DrivingAssetError::read_failed,error);
        if(std::fgetc(file.f)!=EOF)return fail(DrivingAssetError::wrong_size,error);
        if(std::ferror(file.f))return fail(DrivingAssetError::read_failed,error);
        return decode_driving_assets(bytes,output,error);
    }catch(const std::bad_alloc&){return fail(DrivingAssetError::allocation_failed,error);}
    catch(const std::length_error&){return fail(DrivingAssetError::allocation_failed,error);}
}
} // namespace ps2::native
