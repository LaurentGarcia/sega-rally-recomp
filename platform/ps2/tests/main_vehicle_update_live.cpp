#include "main_vehicle_update_adapter.h"
#include "main_vehicle_checkpoint.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <type_traits>
#include <vector>
namespace {
using namespace ps2::native;
struct Reader {
 std::vector<std::uint8_t> bytes;std::size_t cursor=0;
 explicit Reader(const char* path){std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("Cannot open private vehicle records");if(f.tellg()<0||f.tellg()>16*1024*1024)throw std::runtime_error("Vehicle record limit");f.seekg(0);bytes={std::istreambuf_iterator<char>(f),{}};if(bytes.size()>16*1024*1024)throw std::runtime_error("Vehicle record limit");}
 std::uint32_t word(){if(bytes.size()-cursor<4)throw std::runtime_error("Truncated vehicle record");const auto* p=bytes.data()+cursor;cursor+=4;return p[0]|std::uint32_t(p[1])<<8|std::uint32_t(p[2])<<16|std::uint32_t(p[3])<<24;}
};
using Memory=std::map<std::uint32_t,std::uint32_t>;
std::uint32_t read(const Memory& memory,std::uint32_t address,unsigned size=4){
 auto found=memory.find(address&~3u);if(found==memory.end()||size>4||(address&3)+size>4){std::fprintf(stderr,"missing word=%08x width=%u\n",address,size);throw std::runtime_error("Missing canonical expected word");}
 const auto shifted=found->second>>(8*(address&3));return size==4?shifted:shifted&((1u<<(8*size))-1);
}
template<class T>std::uint32_t bits(T value){if constexpr(sizeof(T)==1)return std::uint8_t(value);else if constexpr(sizeof(T)==2)return std::bit_cast<std::uint16_t>(value);else return std::bit_cast<std::uint32_t>(value);}
template<class T>T value(std::uint32_t word){if constexpr(sizeof(T)==1)return T(word);else if constexpr(sizeof(T)==2)return std::bit_cast<T>(std::uint16_t(word));else return std::bit_cast<T>(word);}
void hash_word(std::uint64_t& h,std::uint32_t w){for(unsigned i=0;i<4;++i){h^=(w>>(8*i))&255;h*=1099511628211ull;}}
struct Clock {std::uint64_t tick=~std::uint64_t{0}-51,calls=0;
 static std::uint64_t read(void* context) noexcept {auto& c=*static_cast<Clock*>(context);++c.calls;c.tick+=17;return c.tick;}};
template<class T>auto representation(const T& value){std::array<unsigned char,sizeof(T)> bytes;std::memcpy(bytes.data(),&value,sizeof value);return bytes;}
void profile_rejections(const DrivingAssets& assets,const MainVehicleSetup& setup,const MainVehicleInputs& input,
                        const MainVehicleState& initial){
 MainVehicleState state=initial;MainVehicleOutput output{};Clock clock;
 MainVehicleProfile profile{Clock::read,&clock,1,2,3,4,5,6};
 const auto state_before=representation(state);
 const auto output_before=representation(output);
 const auto profile_before=representation(profile);
 const auto unchanged=[&]{return representation(state)==state_before&&representation(output)==output_before&&representation(profile)==profile_before;};
 DrivingAssets missing;
 if(update_main_vehicle(missing,setup,input,state,output,&profile)||clock.calls||!unchanged())throw std::runtime_error("Unloaded profile rejection mutated publication");
 auto unsupported=input;unsupported.surrounding_car_update=true;
 if(update_main_vehicle(assets,setup,unsupported,state,output,&profile)||!unchanged())throw std::runtime_error("Late profile rejection mutated publication");
}
}
int main(int argc,char** argv){try{
 if(argc!=4)return 2;DrivingAssets assets;DrivingAssetError asset_error;
 if(!load_driving_assets_file(argv[1],assets,asset_error))throw std::runtime_error(driving_asset_error_message(asset_error));
 Reader records(argv[2]);constexpr std::array<std::uint8_t,8> magic{'V','E','H','S','T','E','P','1'};
 if(records.bytes.size()<16||!std::equal(magic.begin(),magic.end(),records.bytes.begin()))throw std::runtime_error("Invalid vehicle magic");
 records.cursor=8;const auto count=records.word(),seed_size=records.word();if(!count||count>1201||seed_size!=0x140000||records.bytes.size()-records.cursor<seed_size)throw std::runtime_error("Invalid vehicle seed/count");
 Memory initial;for(std::uint32_t a=0x200000;a<0x240000;a+=4)initial.emplace(a,records.word());for(std::uint32_t a=0x500000;a<0x600000;a+=4)initial.emplace(a,records.word());
 MainVehicleState state,unprofiled_state;MainVehicleSetup setup;Clock profile_clock;
 MainVehicleCheckpoint first_checkpoint{},driving_checkpoint{};bool driving_checkpoint_ready=false;std::uint32_t first_object=0,first_fp=0,first_pose=0,previous_frame=0;
 std::uint64_t hash=14695981039346656037ull;unsigned fields=0,poses=0,commands=0;
 for(unsigned row=0;row<count;++row){
  const auto frame=records.word(),fp=records.word(),object=records.word(),pose=records.word(),queue_cursor=records.word();
  std::array<std::uint32_t,12> input_words{};for(auto& w:input_words)w=records.word();
  std::array<std::uint32_t,3> expected_direction{};for(auto& w:expected_direction)w=records.word();
  if((row&&frame!=previous_frame+1)||queue_cursor>=128)throw std::runtime_error("Invalid consecutive vehicle identity");previous_frame=frame;
  if(row==0){first_object=object;first_fp=fp;first_pose=pose;
   vehicle_update_test::state_words(state,object,fp,[&](std::uint32_t a,auto& field){using T=std::remove_reference_t<decltype(field)>;field=value<T>(read(initial,a,sizeof(T)));});
   unprofiled_state=state;
   setup=vehicle_update_test::setup([&](std::uint32_t a){return read(initial,a);},object);
   if(read(initial,0x214354)!=assets.selector())throw std::runtime_error("Initial course identity differs");
   // The sole original state seed is discarded before the first native tick.
   initial.clear();
  }else if(object!=first_object||fp!=first_fp||pose!=first_pose)throw std::runtime_error("Canonical storage identity changed");
  const auto block_count=records.word();if(!block_count||block_count>1024)throw std::runtime_error("Invalid expected block count");Memory expected;
  for(unsigned block=0;block<block_count;++block){const auto a=records.word(),n=records.word();if(a&3||!n||n>4096||std::uint64_t(a)+4ull*n>0x100000000ull)throw std::runtime_error("Invalid expected block");
   for(unsigned i=0;i<n;++i)if(!expected.emplace(a+4*i,records.word()).second)throw std::runtime_error("Overlapping expected words");}
  const auto queued=records.word();if(queued>216)throw std::runtime_error("Expected queue limit");std::vector<std::array<std::uint32_t,2>> expected_queue;
  for(unsigned i=0;i<queued;++i){const auto a=records.word(),b=records.word();if(a<0x20b100||a>=0x20b180||b>255)throw std::runtime_error("Invalid queue byte");expected_queue.push_back({a,b});}
  const auto input=vehicle_update_test::inputs(input_words);MainVehicleOutput output{},unprofiled_output{};
  const auto direction_index=state.segment<0?0u:unsigned(state.segment);
  if(direction_index>=assets.course_directions().size())throw std::runtime_error("Prior native segment lacks course guidance");
  const auto direction=assets.course_directions()[direction_index];
  if(expected_direction!=std::array<std::uint32_t,3>{bits(direction.x),0,bits(direction.z)})throw std::runtime_error("Native prior segment guidance differs from original surrounding lookup");
  if(row==0)first_checkpoint={assets.selector(),assets.payload_crc32(),setup,input,state};
  if(!driving_checkpoint_ready&&frame>=2400&&input.operating_mode==1&&input.screen==7){driving_checkpoint={assets.selector(),assets.payload_crc32(),setup,input,state};driving_checkpoint_ready=true;}
  if(row==0)profile_rejections(assets,setup,input,state);
  MainVehicleProfile profile{Clock::read,&profile_clock};
  if(!update_main_vehicle(assets,setup,input,state,output,&profile)){std::fprintf(stderr,"vehicle rejected row=%u frame=%u input_frame=%u\n",row,frame,input.frame);throw std::runtime_error("Complete native callback rejected original sequence");}
  if(!update_main_vehicle(assets,setup,input,unprofiled_state,unprofiled_output)||
     representation(state)!=representation(unprofiled_state)||representation(output)!=representation(unprofiled_output))throw std::runtime_error("Profiling changed state or output bytes");
  if(profile.clock!=Clock::read||profile.context!=&profile_clock||!profile.entry_ticks||!profile.contact_ticks||!profile.front_ticks||
     !profile.balance_ticks||!profile.motion_ticks||profile.total_ticks<profile.entry_ticks+profile.contact_ticks+profile.front_ticks+profile.balance_ticks+profile.motion_ticks)
      throw std::runtime_error("Successful profile timings missing or inconsistent");
  if(row==0){
   auto no_clock_state=first_checkpoint.state;MainVehicleOutput no_clock_output{};
   MainVehicleProfile disabled{nullptr,&profile_clock,1,2,3,4,5,6};const auto calls_before=profile_clock.calls;
   if(!update_main_vehicle(assets,setup,input,no_clock_state,no_clock_output,&disabled)||profile_clock.calls!=calls_before||
      disabled.clock||disabled.context!=&profile_clock||disabled.entry_ticks||disabled.contact_ticks||disabled.front_ticks||
      disabled.balance_ticks||disabled.motion_ticks||disabled.total_ticks||representation(no_clock_state)!=representation(state)||
      representation(no_clock_output)!=representation(output))throw std::runtime_error("Null clock changed update or profile contract");
  }
  vehicle_update_test::state_words(state,object,fp,[&](std::uint32_t a,auto field){const auto actual=bits(field),wanted=read(expected,a,sizeof(field));
   if(actual!=wanted){std::fprintf(stderr,"vehicle mismatch row=%u frame=%u address=%08x width=%zu wanted=%08x actual=%08x\n",row,frame,a,sizeof(field),wanted,actual);throw std::runtime_error("Persistent native state differs");}hash_word(hash,actual);++fields;});
  std::array<std::uint32_t,22> pose_words{};unsigned at=0;
  for(auto w:output.pose.words.position)pose_words[at++]=w;for(auto w:output.pose.words.motion)pose_words[at++]=w;
  for(auto w:output.pose.words.orientation)pose_words[at++]=w;for(auto w:output.pose.words.attributes)pose_words[at++]=w;
  pose_words[at++]=output.pose.flags;pose_words[at++]=output.pose.progress;
  for(unsigned i=0;i<pose_words.size();++i){const auto wanted=read(expected,pose+4*i);if(pose_words[i]!=wanted){std::fprintf(stderr,"vehicle pose mismatch row=%u frame=%u index=%u wanted=%08x actual=%08x\n",row,frame,i,wanted,pose_words[i]);throw std::runtime_error("Published dynamic pose differs");}hash_word(hash,pose_words[i]);++poses;}
  if(std::bit_cast<std::uint32_t>(output.selected_segment)!=read(expected,0x2140c8)||std::bit_cast<std::uint32_t>(output.progression)!=read(expected,0x2140c4)||bits(output.speed_metric)!=read(expected,0x20b0b4))throw std::runtime_error("Published selection or speed differs");
  std::vector<std::array<std::uint32_t,2>> actual_queue;auto cursor=queue_cursor;
  if(output.command_count>output.commands.size())throw std::runtime_error("Native command bound");
  for(unsigned i=0;i<output.command_count;++i){const auto& request=output.commands[i];if(request.code>=assets.sound_commands().size())throw std::runtime_error("Unknown native sound request");auto bytes=assets.sound_commands()[request.code].bytes;
   if(request.has_parameter&&bytes[2]==255)bytes[2]=request.parameter&255;
   for(auto byte:bytes){actual_queue.push_back({0x20b100+cursor,byte});cursor=(cursor+1)&127;hash_word(hash,byte);}}
  if(actual_queue!=expected_queue){std::fprintf(stderr,"vehicle sound mismatch row=%u frame=%u wanted=%zu actual=%zu\n",row,frame,expected_queue.size(),actual_queue.size());throw std::runtime_error("Ordered sound requests differ");}commands+=output.command_count;
 }
 if(records.cursor!=records.bytes.size())throw std::runtime_error("Extra vehicle record bytes");
 if(!driving_checkpoint_ready)throw std::runtime_error("No active developer checkpoint reached");
 const auto export_checkpoint=[&](const MainVehicleCheckpoint& checkpoint,const char* name){
  MainVehicleCheckpointBytes bytes;MainVehicleCheckpointError error;
  if(!encode_main_vehicle_checkpoint(checkpoint,bytes,error))throw std::runtime_error(main_vehicle_checkpoint_error_message(error));
  const auto path=std::string(argv[3])+"/"+name;std::ifstream exists(path,std::ios::binary);if(exists)throw std::runtime_error("Checkpoint output already exists");
  std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());if(!out)throw std::runtime_error("Cannot write validated native checkpoint");
 };
 export_checkpoint(first_checkpoint,"initial-car.bin");export_checkpoint(driving_checkpoint,"driving-car.bin");
 std::printf("MAIN_VEHICLE_UPDATE_LIVE PASS steps=%u state_words=%u pose_words=%u sound_requests=%u seeds=1 hash=%016llx\n",count,fields,poses,commands,(unsigned long long)hash);
}catch(const std::exception& e){std::fprintf(stderr,"vehicle comparison failed: %s\n",e.what());return 1;}return 0;}
