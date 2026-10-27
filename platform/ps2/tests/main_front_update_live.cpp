#include "main_front_update_adapter.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace {
using namespace ps2::native;
struct Reader {
    std::vector<std::uint8_t> bytes;
    std::size_t cursor=0;
    explicit Reader(const char* path){
        std::ifstream stream(path,std::ios::binary);
        if(!stream)throw std::runtime_error("Cannot open private records");
        bytes={std::istreambuf_iterator<char>(stream),{}};
        if(bytes.size()>16*1024*1024)throw std::runtime_error("Private record limit");
    }
    std::uint32_t word(){
        if(bytes.size()-cursor<4)throw std::runtime_error("Truncated private record");
        const auto* p=bytes.data()+cursor;cursor+=4;
        return p[0]|std::uint32_t(p[1])<<8|std::uint32_t(p[2])<<16|std::uint32_t(p[3])<<24;
    }
};
using Memory=std::map<std::uint32_t,std::uint32_t>;
std::uint32_t read(const Memory& memory,std::uint32_t address,unsigned size=4){
    const auto found=memory.find(address&~3u);
    if(found==memory.end()||(address&(size-1))){std::fprintf(stderr,"missing word=%08x\n",address);throw std::runtime_error("Missing private state word");}
    return size<4?(found->second>>(8*(address&3)))&((1u<<(8*size))-1):found->second;
}
template<class Value>std::uint32_t bits(Value value){
    if constexpr(sizeof(Value)<4)return value;
    else return std::bit_cast<std::uint32_t>(value);
}
void hash_word(std::uint64_t& hash,std::uint32_t value){for(unsigned i=0;i<4;++i){hash^=(value>>(8*i))&255;hash*=1099511628211ull;}}
}
int main(int argc,char** argv){try{
    if(argc!=3)return 2;
    DrivingAssets assets;DrivingAssetError error;
    if(!load_driving_assets_file(argv[1],assets,error))throw std::runtime_error(driving_asset_error_message(error));
    Reader records(argv[2]);
    constexpr std::array<std::uint8_t,8> magic{'F','R','O','N','T','S','T','1'};
    if(records.bytes.size()<12||!std::equal(magic.begin(),magic.end(),records.bytes.begin()))throw std::runtime_error("Invalid front record magic");
    records.cursor=8;const auto count=records.word();if(!count||count>1201)throw std::runtime_error("Invalid front count");
    std::uint64_t hash=14695981039346656037ull;unsigned fields=0,commands=0,previous_frame=0;
    for(unsigned row=0;row<count;++row){
        const auto frame=records.word(),fp=records.word(),object=records.word(),blocks=records.word();
        if((row&&frame!=previous_frame+1)||!blocks||blocks>16384)throw std::runtime_error("Invalid front row");
        previous_frame=frame;Memory before,after;
        for(unsigned block=0;block<blocks;++block){
            const auto address=records.word(),words=records.word();
            if(address&3||!words||words>1024||std::uint64_t(address)+4ull*words>0x100000000ull)throw std::runtime_error("Invalid front block");
            for(auto* memory:{&before,&after})for(unsigned i=0;i<words;++i)
                if(!memory->emplace(address+4*i,records.word()).second)throw std::runtime_error("Overlapping front block");
        }
        const auto selected=records.word();
        MainFrontState state{};
        front_update_test::state_words(state,object,fp,[&](std::uint32_t address,auto& value){
            using T=std::remove_reference_t<decltype(value)>;
            if constexpr(sizeof(T)<4)value=T(read(before,address,sizeof(T)));
            else value=std::bit_cast<T>(read(before,address));
        });
        state.contact.selected_scalar=std::bit_cast<float>(selected);
        const auto input=front_update_test::inputs([&](std::uint32_t address){return read(before,address);},object,assets);
        if(read(before,0x214354)!=assets.selector())throw std::runtime_error("Front course asset differs");
        MainBalanceEffects effects{};MainFrontOutput output;
        if(!update_main_front(input,state,output,effects)){std::fprintf(stderr,"front rejected row=%u frame=%u scene=%u scalar=%08x\n",row,frame,input.scene_metric_scale,selected);throw std::runtime_error("Front rejected original input");}
        front_update_test::state_words(state,object,fp,[&](std::uint32_t address,auto value){
            const auto wanted=read(after,address,sizeof(value)),actual=bits(value);
            if(wanted!=actual){std::fprintf(stderr,"front mismatch row=%u frame=%u field=%08x wanted=%08x actual=%08x\n",row,frame,address,wanted,actual);throw std::runtime_error("Front output differs");}
            hash_word(hash,actual);++fields;
        });
        front_update_test::output_words(output,fp,[&](std::uint32_t address,auto value){
            const auto wanted=read(after,address),actual=bits(value);
            if(wanted!=actual){std::fprintf(stderr,"front output mismatch row=%u frame=%u field=%08x wanted=%08x actual=%08x\n",row,frame,address,wanted,actual);throw std::runtime_error("Front derived output differs");}
            hash_word(hash,actual);++fields;
        });
        commands+=effects.count;
    }
    if(records.cursor!=records.bytes.size())throw std::runtime_error("Extra front record bytes");
    std::printf("MAIN_FRONT_UPDATE_LIVE PASS steps=%u state_words=%u sound_requests=%u hash=%016llx\n",count,fields,commands,(unsigned long long)hash);
}catch(const std::exception& e){std::fprintf(stderr,"front comparison failed: %s\n",e.what());return 1;}return 0;}
