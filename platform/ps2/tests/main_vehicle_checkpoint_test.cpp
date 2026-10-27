#include "main_vehicle_checkpoint.h"
#include "main_vehicle_checkpoint_fields.h"
#include "main_vehicle_update_adapter.h"
#include <bit>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <set>
#include <type_traits>
namespace {
using namespace ps2::native;
unsigned checks=0;
bool check(bool condition){++checks;return condition;}
std::uint32_t word(const MainVehicleCheckpointBytes& b,unsigned at){return b[at]|std::uint32_t(b[at+1])<<8|std::uint32_t(b[at+2])<<16|std::uint32_t(b[at+3])<<24;}
bool same(const MainVehicleCheckpoint& a,const MainVehicleCheckpoint& b){
    MainVehicleCheckpointBytes first,second;MainVehicleCheckpointError error;
    return encode_main_vehicle_checkpoint(a,first,error)&&encode_main_vehicle_checkpoint(b,second,error)&&first==second;
}
bool tests(){
    MainVehicleCheckpointError error;MainVehicleCheckpoint source;
    // The independently maintained original-boundary adapter enumerates every
    // persistent state field. Require storage coverage, independent of order.
    std::set<const void*> mapped,serialized;
    vehicle_update_test::state_words(source.state,0,0,[&](std::uint32_t,auto& value){mapped.insert(&value);});
    unsigned ordinal=0;
    visit_main_vehicle_checkpoint_values(source.setup,source.input,source.state,[&](auto& value){if(ordinal++>=19)serialized.insert(&value);});
    if(!check(mapped==serialized)||!check(mapped.size()==161))return false;
    source.course_selector=7;source.asset_payload_crc32=0x12345678;
    constexpr std::uint32_t patterns[]{0,0x80000000,1,0x807fffff,0x00800000,0x7f800000,0xff800000,0x7fc12345,0x7f7fffff};
    for(unsigned variant=0;variant<9;++variant){
        unsigned index=0;
        visit_main_vehicle_checkpoint_values(source.setup,source.input,source.state,[&](auto& value){
            using T=std::remove_reference_t<decltype(value)>;++index;
            if constexpr(std::is_same_v<T,float>)value=std::bit_cast<float>(patterns[(index+variant)%9]);
            else if constexpr(std::is_same_v<T,bool>)value=(index+variant)%2;
            else if constexpr(std::is_signed_v<T>)value=T((index+variant)%2?-int(index):int(index));
            else value=T(index*19+variant);
        });
        MainVehicleCheckpointBytes encoded;encoded.fill(0xad);
        if(!check(index==180)||!check(encode_main_vehicle_checkpoint(source,encoded,error))||!check(error==MainVehicleCheckpointError::none))return false;
        if(!check(encoded.size()==752)||!check(std::memcmp(encoded.data(),"RLYCAR1\0",8)==0)||!check(word(encoded,8)==1)||
           !check(word(encoded,12)==32)||!check(word(encoded,16)==180)||!check(word(encoded,24)==7)||!check(word(encoded,28)==0x12345678))return false;
        // Independent fixed positions in the version1 contract, including
        // first/last scalar and the two booleans after seven integer/five byte inputs.
        if(!check(word(encoded,32)==std::bit_cast<std::uint32_t>(source.setup.divisors[0]))||
           !check(word(encoded,32+4*17)==unsigned(source.input.manual_selection))||
           !check(word(encoded,32+4*18)==unsigned(source.input.surrounding_car_update))||
           !check(word(encoded,748)==source.state.diagnostic_marker[2]))return false;
        MainVehicleCheckpoint output;
        if(!check(decode_main_vehicle_checkpoint(encoded,output,error))||!check(same(source,output)))return false;
        const auto sentinel=output;
        for(unsigned offset: {0u,7u,8u,12u,16u,20u,32u,751u}){
            auto damaged=encoded;damaged[offset]^=0x40;
            if(!check(!decode_main_vehicle_checkpoint(damaged,output,error))||!check(same(output,sentinel)))return false;
        }
        auto unchanged=encoded;
        if(!check(!encode_main_vehicle_checkpoint(source,std::span(encoded).first(751),error))||!check(encoded==unchanged)||
           !check(!decode_main_vehicle_checkpoint(std::span(encoded).first(751),output,error))||!check(same(output,sentinel)))return false;
        if(!check(!load_main_vehicle_checkpoint_file(nullptr,output,error))||!check(same(output,sentinel)))return false;
    }
    return true;
}
}
int main(int argc,char** argv){
    if(argc==3){
        MainVehicleCheckpoint checkpoint;checkpoint.course_selector=42;checkpoint.state.power.random_state=0x719a;
        const auto previous=checkpoint;MainVehicleCheckpointError error;
        const bool ok=load_main_vehicle_checkpoint_file(argv[1],checkpoint,error);
        if(std::string_view(argv[2])=="valid"){
            if(!ok||checkpoint.course_selector!=3||checkpoint.asset_payload_crc32!=0x23456789||
               std::bit_cast<std::uint32_t>(checkpoint.setup.divisors[0])!=0x80000000||
               !checkpoint.input.manual_selection||checkpoint.input.surrounding_car_update||
               checkpoint.state.diagnostic_marker[2]!=0xabcdef12)return 1;
            std::puts("MAIN_VEHICLE_CHECKPOINT FILE PASS");return 0;
        }
        if(ok||!same(checkpoint,previous))return 1;
        std::printf("MAIN_VEHICLE_CHECKPOINT REJECT %u\n",unsigned(error));return 0;
    }
    if(argc!=1||!tests())return 1;
    std::printf("MAIN_VEHICLE_CHECKPOINT PASS checks=%u bytes=%zu words=%zu\n",checks,main_vehicle_checkpoint_bytes,main_vehicle_checkpoint_word_count);
    return 0;
}
