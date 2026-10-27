#include "main_vehicle_checkpoint.h"
#include "main_vehicle_checkpoint_fields.h"
#include <algorithm>
#include <bit>
#include <cstdio>
#include <limits>
#include <type_traits>

namespace ps2::native {
namespace {
constexpr std::array<std::uint8_t,8> magic{'R','L','Y','C','A','R','1',0};
std::uint32_t read(std::span<const std::uint8_t> bytes,std::size_t at) noexcept {
    return bytes[at]|(std::uint32_t(bytes[at+1])<<8)|(std::uint32_t(bytes[at+2])<<16)|(std::uint32_t(bytes[at+3])<<24);
}
void write(std::span<std::uint8_t> bytes,std::size_t at,std::uint32_t value) noexcept {
    for(unsigned i=0;i<4;++i)bytes[at+i]=std::uint8_t(value>>(8*i));
}
std::uint32_t crc32(std::span<const std::uint8_t> bytes) noexcept {
    std::uint32_t crc=0xffffffffu;
    for(auto value:bytes){crc^=value;for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1u)?0xedb88320u:0u);}
    return crc^0xffffffffu;
}
template<class T>std::uint32_t encoded(T value) noexcept {
    if constexpr(std::is_same_v<T,float>)return std::bit_cast<std::uint32_t>(value);
    else return static_cast<std::uint32_t>(value);
}
template<class T>bool decoded(std::uint32_t raw,T& output) noexcept {
    if constexpr(std::is_same_v<T,float>){output=std::bit_cast<float>(raw);return true;}
    else if constexpr(std::is_same_v<T,bool>){if(raw>1)return false;output=raw!=0;return true;}
    else if constexpr(std::is_signed_v<T>){
        const auto value=std::bit_cast<std::int32_t>(raw);
        if(value<std::numeric_limits<T>::min()||value>std::numeric_limits<T>::max())return false;
        output=static_cast<T>(value);return true;
    }else{if(raw>std::numeric_limits<T>::max())return false;output=static_cast<T>(raw);return true;}
}
constexpr std::size_t listed_words() {
    MainVehicleSetup setup;MainVehicleInputs input;MainVehicleState state;std::size_t count=0;
    visit_main_vehicle_checkpoint_values(setup,input,state,[&](const auto&){++count;});return count;
}
static_assert(listed_words()==main_vehicle_checkpoint_word_count);
static_assert(sizeof(float)==4&&std::numeric_limits<float>::radix==2&&std::numeric_limits<float>::digits==24);
}
const char* main_vehicle_checkpoint_error_message(MainVehicleCheckpointError error) noexcept {
    switch(error){
        case MainVehicleCheckpointError::none:return "Checkpoint ready";
        case MainVehicleCheckpointError::invalid_size:return "Checkpoint size differs";
        case MainVehicleCheckpointError::invalid_header:return "Checkpoint version or header differs";
        case MainVehicleCheckpointError::checksum_mismatch:return "Checkpoint checksum differs";
        case MainVehicleCheckpointError::invalid_encoding:return "Checkpoint value encoding differs";
        case MainVehicleCheckpointError::open_failed:return "Cannot open checkpoint";
        case MainVehicleCheckpointError::read_failed:return "Cannot read checkpoint";
    }
    return "Unknown checkpoint error";
}
bool encode_main_vehicle_checkpoint(const MainVehicleCheckpoint& source,std::span<std::uint8_t> bytes,
                                   MainVehicleCheckpointError& error) noexcept {
    if(bytes.size()!=main_vehicle_checkpoint_bytes){error=MainVehicleCheckpointError::invalid_size;return false;}
    MainVehicleCheckpointBytes result{};std::copy(magic.begin(),magic.end(),result.begin());
    write(result,8,1);write(result,12,main_vehicle_checkpoint_header_bytes);write(result,16,main_vehicle_checkpoint_word_count);
    write(result,24,source.course_selector);write(result,28,source.asset_payload_crc32);
    std::size_t cursor=main_vehicle_checkpoint_header_bytes;
    visit_main_vehicle_checkpoint_values(source.setup,source.input,source.state,[&](const auto& value){write(result,cursor,encoded(value));cursor+=4;});
    write(result,20,crc32(std::span<const std::uint8_t>(result).subspan(main_vehicle_checkpoint_header_bytes)));
    std::copy(result.begin(),result.end(),bytes.begin());error=MainVehicleCheckpointError::none;return true;
}
bool decode_main_vehicle_checkpoint(std::span<const std::uint8_t> bytes,MainVehicleCheckpoint& output,
                                   MainVehicleCheckpointError& error) noexcept {
    if(bytes.size()!=main_vehicle_checkpoint_bytes){error=MainVehicleCheckpointError::invalid_size;return false;}
    if(!std::equal(magic.begin(),magic.end(),bytes.begin())||read(bytes,8)!=1||
       read(bytes,12)!=main_vehicle_checkpoint_header_bytes||read(bytes,16)!=main_vehicle_checkpoint_word_count){
        error=MainVehicleCheckpointError::invalid_header;return false;
    }
    if(read(bytes,20)!=crc32(bytes.subspan(main_vehicle_checkpoint_header_bytes))){error=MainVehicleCheckpointError::checksum_mismatch;return false;}
    MainVehicleCheckpoint result;result.course_selector=read(bytes,24);result.asset_payload_crc32=read(bytes,28);
    bool valid=true;std::size_t cursor=main_vehicle_checkpoint_header_bytes;
    visit_main_vehicle_checkpoint_values(result.setup,result.input,result.state,[&](auto& value){valid&=decoded(read(bytes,cursor),value);cursor+=4;});
    if(!valid){error=MainVehicleCheckpointError::invalid_encoding;return false;}
    output=result;error=MainVehicleCheckpointError::none;return true;
}
bool load_main_vehicle_checkpoint_file(const char* path,MainVehicleCheckpoint& output,
                                      MainVehicleCheckpointError& error) noexcept {
    if(!path||!*path){error=MainVehicleCheckpointError::open_failed;return false;}
    auto* file=std::fopen(path,"rb");if(!file){error=MainVehicleCheckpointError::open_failed;return false;}
    MainVehicleCheckpointBytes bytes;
    const auto count=std::fread(bytes.data(),1,bytes.size(),file);
    const int extra=std::fgetc(file);const bool failed=std::ferror(file)!=0;const int closed=std::fclose(file);
    if(failed||closed!=0){error=MainVehicleCheckpointError::read_failed;return false;}
    if(count!=bytes.size()||extra!=EOF){error=MainVehicleCheckpointError::invalid_size;return false;}
    return decode_main_vehicle_checkpoint(bytes,output,error);
}
} // namespace ps2::native
