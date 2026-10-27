// Original entry boundary comparison; private addresses stay in this adapter.
#include "main_update_entry.h"
#include "driving_assets.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <vector>

namespace {
using namespace ps2::native;
struct Reader {
    std::vector<std::uint8_t> bytes;std::size_t cursor=8;
    explicit Reader(const char* path){std::ifstream file(path,std::ios::binary);
        if(!file)throw std::runtime_error("Cannot open entry records");
        bytes={std::istreambuf_iterator<char>(file),{}};
        const std::array<std::uint8_t,8> magic{'E','N','T','S','T','E','P','1'};
        if(bytes.size()<12||bytes.size()>16*1024*1024||!std::equal(magic.begin(),magic.end(),bytes.begin()))throw std::runtime_error("Entry record bounds or magic");}
    std::uint32_t word(){if(bytes.size()-cursor<4)throw std::runtime_error("Truncated entry record");
        const auto* p=bytes.data()+cursor;cursor+=4;return p[0]|std::uint32_t(p[1])<<8|std::uint32_t(p[2])<<16|std::uint32_t(p[3])<<24;}
};
using Words=std::map<std::uint32_t,std::uint32_t>;
std::uint32_t read(const Words& words,std::uint32_t address){const auto item=words.find(address);
    if(item==words.end())throw std::runtime_error("Entry field missing");return item->second;}
template<class Visit>void state_words(MainEntryState& s,std::uint32_t object,Visit visit){
    visit(object+0x10,s.flags);visit(object+0x74,s.steering);visit(0x2142c0,s.steering_history);
    visit(0x2140c8,s.published_segment);visit(0x2140c4,s.published_progression);
}
}
int main(int argc,char** argv){try{
    if(argc!=3)return 2;
    DrivingAssets assets;DrivingAssetError error;if(!load_driving_assets_file(argv[1],assets,error))throw std::runtime_error(driving_asset_error_message(error));
    Reader records(argv[2]);const auto count=records.word();if(!count||count>1201)throw std::runtime_error("Entry record count");
    std::uint64_t hash=14695981039346656037ull;unsigned previous=0;
    const auto hashed=[&](std::uint32_t value){for(unsigned i=0;i<4;++i){hash^=(value>>(8*i))&255;hash*=1099511628211ull;}};
    for(unsigned row=0;row<count;++row){
        const auto frame=records.word(),fp=records.word(),object=records.word(),blocks=records.word();
        if((row&&frame!=previous+1)||fp&63||!blocks||blocks>1024)throw std::runtime_error("Entry row identity");previous=frame;
        Words before,after;
        for(unsigned b=0;b<blocks;++b){const auto address=records.word(),words=records.word();
            if(address&3||!words||words>1024||std::uint64_t(address)+4ull*words>0x100000000ull)throw std::runtime_error("Entry block bounds");
            for(auto* map:{&before,&after})for(unsigned i=0;i<words;++i)if(!map->emplace(address+4*i,records.word()).second)throw std::runtime_error("Entry overlapping block");}
        if(records.word())throw std::runtime_error("Unexpected entry sound commands");
        MainEntryState state;
        state_words(state,object,[&](std::uint32_t a,auto& value){value=std::bit_cast<std::remove_reference_t<decltype(value)>>(read(before,a));});
        const auto index=read(before,object+0x88);
        state.segment=std::bit_cast<std::int16_t>(std::uint16_t(index));state.progression=std::bit_cast<std::int16_t>(std::uint16_t(index>>16));
        MainEntryInput input;input.vertices=assets.selection_vertices();input.segments=assets.selection_segments();
        input.position={std::bit_cast<float>(read(before,object+0x14)),std::bit_cast<float>(read(before,object+0x1c))};
        input.steering_sample=read(before,0x202050)&255;input.flag_sample=read(before,0x2139d8)&255;
        input.screen=std::bit_cast<std::int32_t>(read(before,0x2020a8));input.surrounding_car_update=std::bit_cast<std::int32_t>(read(before,0x20a560))>0;
        if(read(before,0x214354)!=assets.selector())throw std::runtime_error("Entry selector differs from assets");
        if(!begin_main_update(input,state))throw std::runtime_error("Entry rejected captured input");
        state_words(state,object,[&](std::uint32_t a,auto& value){const auto actual=std::bit_cast<std::uint32_t>(value),wanted=read(after,a);
            if(actual!=wanted){std::fprintf(stderr,"entry mismatch frame=%u address=%08x wanted=%08x actual=%08x\n",frame,a,wanted,actual);throw std::runtime_error("Entry state differs");}hashed(actual);});
        const auto actual=std::uint32_t(std::bit_cast<std::uint16_t>(state.segment))|(std::uint32_t(std::bit_cast<std::uint16_t>(state.progression))<<16);
        if(actual!=read(after,object+0x88))throw std::runtime_error("Entry segment or progression differs");hashed(actual);
    }
    if(records.cursor!=records.bytes.size())throw std::runtime_error("Trailing entry bytes");
    std::printf("MAIN_UPDATE_ENTRY_LIVE PASS steps=%u state_words=%u hash=%016llx\n",count,count*6,(unsigned long long)hash);
}catch(const std::exception& error){std::fprintf(stderr,"entry comparison failed: %s\n",error.what());return 1;}return 0;}
