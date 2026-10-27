// Offline package integration against sealed native contact-step boundaries.
#include "driving_assets.h"
#include "main_contact_step_cases.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>
namespace {
using namespace ps2::native;namespace cases=main_contact_step_cases;
std::uint32_t word(const std::vector<std::uint8_t>& b,std::size_t p){return b.at(p)|(std::uint32_t(b.at(p+1))<<8)|(std::uint32_t(b.at(p+2))<<16)|(std::uint32_t(b.at(p+3))<<24);}
}
int main(int argc,char** argv){try{
    if(argc!=3)return 2;
    DrivingAssets assets;DrivingAssetError error;
    if(!load_driving_assets_file(argv[1],assets,error))throw std::runtime_error(driving_asset_error_message(error));
    std::ifstream stream(argv[2],std::ios::binary);if(!stream)throw std::runtime_error("cannot open private records");
    std::vector<std::uint8_t> rows{std::istreambuf_iterator<char>(stream),{}};
    constexpr std::array<std::uint8_t,8> magic{'M','A','I','N','S','T','P','1'};
    if(rows.size()<12||!std::equal(magic.begin(),magic.end(),rows.begin()))throw std::runtime_error("invalid record header");
    const auto count=word(rows,8);
    if(!count||count>32||rows.size()!=12ull+440ull*count)throw std::runtime_error("invalid record count");
    std::uint64_t hash=14695981039346656037ull;unsigned active=0;
    const auto* course_pointer=assets.course().data();const auto* normal_pointer=assets.normal_table().data();
    for(unsigned row=0;row<count;++row){const auto raw=[&](unsigned column){return word(rows,12+440*row+4*column);};
        if(raw(104)!=assets.selector()||raw(105)>32767||raw(105)>=assets.course().size()||raw(109)>1)throw std::runtime_error("asset/step selection mismatch");
        MainContactStepInput input;input.course=assets.course();input.normal_table=assets.normal_table();input.local_points=*assets.local_points();
        input.segment=std::int16_t(raw(105));input.feedback_enabled=raw(109)!=0;
        for(unsigned i=0;i<4;++i){const auto& p=input.local_points[i];if(cases::bits(p.x)!=raw(92+3*i)||cases::bits(p.y)!=raw(93+3*i)||cases::bits(p.z)!=raw(94+3*i))throw std::runtime_error("local points differ from sealed input");}
        // These are explicit vehicle-state inputs. The package's thirteen-word
        // selection records belong to another helper; they are not aliases for
        // the object's contact parameters and do not initialize those fields.
        input.response_parameter=cases::value(raw(106));input.gain_parameter=cases::value(raw(107));input.scalar_divisor=cases::value(raw(108));
        cases::Words before{},expected{};for(unsigned i=0;i<46;++i){before[i]=raw(i);expected[i]=raw(46+i);}
        auto state=cases::state(before);if(!resolve_main_contact_step(input,state))throw std::runtime_error("loaded assets rejected by composed operation");
        const auto actual=cases::words(state);
        if(actual!=expected)throw std::runtime_error("loaded-asset output differs from original boundary");
        if(course_pointer!=assets.course().data()||normal_pointer!=assets.normal_table().data())throw std::runtime_error("asset views changed during query");
        cases::hash(hash,actual);active+=(cases::bits(state.selected_scalar)&0x7fffffffu)!=0;
    }
    std::printf("DRIVING_ASSETS_LIVE PASS steps=%u contacts=%u active_steps=%u state_words=%u hash=%016llx\n",unsigned(count),unsigned(count*4),active,unsigned(count*46),(unsigned long long)hash);
}catch(const std::exception& error){std::fprintf(stderr,"driving asset comparison failed: %s\n",error.what());return 1;}return 0;}
