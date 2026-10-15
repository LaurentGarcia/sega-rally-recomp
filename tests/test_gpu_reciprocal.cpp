// ROM-free manual Metal arithmetic regression. Requires separately compiled
// test_gpu_reciprocal.hlsl entries; never opens a game or audio device.
// Explicit invocation: test_gpu_reciprocal PREPARED_DIRECTORY NEW_OUTPUT_DIRECTORY.
// Run under a 30-second process-group watchdog. Two256x256rawuint draws/readbacks.
// Sampled Metal coverage only; no assertion about other backends or scene cost.
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>
#include "app/gpu_device.h"
#include <SDL3/SDL.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

using Row=std::array<uint32_t,4>;
constexpr uint32_t width=256,height=256,count=width*height,byte_count=count*sizeof(Row);
void require(bool yes,const char*why){if(!yes)throw std::runtime_error(std::string(why)+": "+SDL_GetError());}
std::vector<uint8_t> bytes(const std::filesystem::path&p){std::ifstream f(p,std::ios::binary);require(bool(f),"input open");return {std::istreambuf_iterator<char>(f),{}};}
void save(const std::filesystem::path&p,const std::vector<Row>&rows){std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(rows.data()),std::streamsize(rows.size()*sizeof(Row)));require(bool(f),"readback save");}
uint32_t rational_q(uint32_t m){const uint64_t n=uint64_t(1)<<47;auto q=n/m;const auto r=n%m;if(2*r>m || (2*r==m&&(q&1)))++q;return uint32_t(q);}
uint32_t rational_bits(uint32_t bits){const uint32_t mag=bits&0x7fffffff;uint32_t q=rational_q(0x800000|(mag&0x7fffff)),exponent=253-(mag>>23);if(q==0x1000000){q>>=1;++exponent;}return(bits&0x80000000)|(exponent<<23)|(q&0x7fffff);}

// Public binary32 patterns only. No captured scene values or game inputs.
std::vector<Row> operands() {
 std::vector<Row> rows;rows.reserve(count);
 const auto add=[&](uint32_t bits,uint32_t mode=0,uint32_t seed=0) {
  require(rows.size()<count,"operand bound");
  rows.push_back({bits,seed,mode,0xa5000000u|uint32_t(rows.size())});
 };
 for(uint32_t exponent=1;exponent<255;++exponent)
  for(int delta:{-1,0,1}) for(uint32_t sign:{0u,0x80000000u})
   add(uint32_t(int64_t(exponent<<23)+delta)|sign);
 for(unsigned bit=0;bit<23;++bit)
  for(int delta:{-1,0,1}) for(uint32_t sign:{0u,0x80000000u})
   add(uint32_t(int64_t(uint32_t(1)<<bit)+delta)|sign);
 for(uint32_t value:{0u,1u,0x7fffffu,0x800000u,0x7e800000u,0x7e800001u,
       0x7f7ffffeu,0x7f7fffffu,0x7f800000u,0x7f800001u,0x7fbfffffu,
       0x7fc00000u,0x7fc00001u,0x7ffffffeu,0x7fffffffu})
  for(uint32_t sign:{0u,0x80000000u}) add(value|sign);
 uint32_t random=0x1264a731;
 while(rows.size()<count/2) {
  random=1664525u*random+1013904223u;
  add((random&0x80000000u)|((1u+(random>>16)%252u)<<23)|(random&0x7fffffu));
 }
 constexpr std::array<uint32_t,9> invalid{0,0x7fc00000,0x7f800001,0x7f800000,
    0xbf000000,0x3f000000,0x3f800000,0x3effffff,0x3f800001};
 for(uint32_t index=0;index<2048;++index) {
  const uint32_t m=0x800001u+uint32_t(uint64_t(index)*0x7ffffe/2047);
  const uint32_t seed=0x3f000000u+rational_q(m)-0x800000u;
  for(int delta=-3;delta<=3;++delta) add(m,1,uint32_t(int64_t(seed)+delta));
  for(uint32_t value:invalid) add(m,1,value);
 }
 require(rows.size()==count,"complete public operand inventory");return rows;
}
struct Fixture {
 SDL_GPUDevice*device=nullptr;SDL_GPUTexture*target=nullptr;SDL_GPUBuffer*input=nullptr;
 SDL_GPUGraphicsPipeline *candidate=nullptr,*baseline=nullptr;unsigned draws=0,readbacks=0;
 ~Fixture(){if(device){SDL_WaitForGPUIdle(device);if(candidate)SDL_ReleaseGPUGraphicsPipeline(device,candidate);if(baseline)SDL_ReleaseGPUGraphicsPipeline(device,baseline);if(input)SDL_ReleaseGPUBuffer(device,input);if(target)SDL_ReleaseGPUTexture(device,target);SDL_DestroyGPUDevice(device);}SDL_Quit();}
 void finish(SDL_GPUCommandBuffer*command){auto*fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence,"submit");require(SDL_WaitForGPUFences(device,true,&fence,1),"fence wait");SDL_ReleaseGPUFence(device,fence);}
 SDL_GPUShader*shader(const std::filesystem::path&p,SDL_GPUShaderStage stage){auto code=bytes(p);code.push_back(0);SDL_GPUShaderCreateInfo info{};info.code=code.data();info.code_size=code.size();info.entrypoint="main0";info.format=SDL_GPU_SHADERFORMAT_MSL;info.stage=stage;info.num_storage_buffers=stage==SDL_GPU_SHADERSTAGE_FRAGMENT?1:0;auto*s=SDL_CreateGPUShader(device,&info);require(s,"MSL shader");return s;}
 SDL_GPUGraphicsPipeline*pipeline(SDL_GPUShader*vs,SDL_GPUShader*ps){SDL_GPUColorTargetDescription color{};color.format=SDL_GPU_TEXTUREFORMAT_R32G32B32A32_UINT;SDL_GPUGraphicsPipelineCreateInfo info{};info.vertex_shader=vs;info.fragment_shader=ps;info.primitive_type=SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;info.rasterizer_state.cull_mode=SDL_GPU_CULLMODE_NONE;info.target_info.color_target_descriptions=&color;info.target_info.num_color_targets=1;auto*p=SDL_CreateGPUGraphicsPipeline(device,&info);require(p,"uint pipeline");return p;}
 void init(const std::filesystem::path&root,const std::vector<Row>&rows){
  SDL_SetMainReady();SDL_SetHint(SDL_HINT_GPU_DRIVER,"metal");SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP,"1");require(SDL_Init(SDL_INIT_VIDEO),"SDL init");device=app::create_gpu_device();require(device,"device");require(std::string(SDL_GetGPUDeviceDriver(device))=="metal","Metal selected");
  require(SDL_GPUTextureSupportsFormat(device,SDL_GPU_TEXTUREFORMAT_R32G32B32A32_UINT,SDL_GPU_TEXTURETYPE_2D,SDL_GPU_TEXTUREUSAGE_COLOR_TARGET),"raw uint render target support");
  SDL_GPUTextureCreateInfo ti{};ti.type=SDL_GPU_TEXTURETYPE_2D;ti.format=SDL_GPU_TEXTUREFORMAT_R32G32B32A32_UINT;ti.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;ti.width=width;ti.height=height;ti.layer_count_or_depth=ti.num_levels=1;target=SDL_CreateGPUTexture(device,&ti);require(target,"uint target");
  auto*vs=shader(root/"shaders/vs_arithmetic.metal",SDL_GPU_SHADERSTAGE_VERTEX);auto*ps=shader(root/"shaders/ps_arithmetic.metal",SDL_GPU_SHADERSTAGE_FRAGMENT);candidate=pipeline(vs,ps);SDL_ReleaseGPUShader(device,ps);ps=shader(root/"shaders/ps_baseline.metal",SDL_GPU_SHADERSTAGE_FRAGMENT);baseline=pipeline(vs,ps);SDL_ReleaseGPUShader(device,ps);SDL_ReleaseGPUShader(device,vs);
  SDL_GPUBufferCreateInfo bi{};bi.usage=SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;bi.size=byte_count;input=SDL_CreateGPUBuffer(device,&bi);require(input,"operand buffer");SDL_GPUTransferBufferCreateInfo upload_info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,byte_count,0};auto*upload=SDL_CreateGPUTransferBuffer(device,&upload_info);require(upload,"upload transfer");auto*data=SDL_MapGPUTransferBuffer(device,upload,false);require(data,"upload map");std::memcpy(data,rows.data(),byte_count);SDL_UnmapGPUTransferBuffer(device,upload);
  auto*command=SDL_AcquireGPUCommandBuffer(device);require(command,"upload command");auto*pass=SDL_BeginGPUCopyPass(command);require(pass,"upload pass");SDL_GPUTransferBufferLocation from{upload,0};SDL_GPUBufferRegion to{input,0,byte_count};SDL_UploadToGPUBuffer(pass,&from,&to,false);SDL_EndGPUCopyPass(pass);finish(command);SDL_ReleaseGPUTransferBuffer(device,upload);
 }
 std::vector<Row>draw(SDL_GPUGraphicsPipeline*pipeline){
  require(++draws<=2,"draw cap");auto*command=SDL_AcquireGPUCommandBuffer(device);require(command,"draw command");SDL_GPUColorTargetInfo color{};color.texture=target;color.load_op=SDL_GPU_LOADOP_DONT_CARE;color.store_op=SDL_GPU_STOREOP_STORE;auto*pass=SDL_BeginGPURenderPass(command,&color,1,nullptr);require(pass,"render pass");SDL_GPUViewport viewport{0,0,float(width),float(height),0,1};SDL_Rect scissor{0,0,int(width),int(height)};SDL_SetGPUViewport(pass,&viewport);SDL_SetGPUScissor(pass,&scissor);SDL_BindGPUGraphicsPipeline(pass,pipeline);SDL_BindGPUFragmentStorageBuffers(pass,0,&input,1);SDL_DrawGPUPrimitives(pass,3,1,0,0);SDL_EndGPURenderPass(pass);finish(command);
  require(++readbacks<=2,"readback cap");SDL_GPUTransferBufferCreateInfo di{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,byte_count,0};auto*download=SDL_CreateGPUTransferBuffer(device,&di);require(download,"download buffer");command=SDL_AcquireGPUCommandBuffer(device);require(command,"download command");auto*copy=SDL_BeginGPUCopyPass(command);require(copy,"download pass");SDL_GPUTextureRegion from{};from.texture=target;from.w=width;from.h=height;from.d=1;SDL_GPUTextureTransferInfo to{};to.transfer_buffer=download;to.pixels_per_row=width;to.rows_per_layer=height;SDL_DownloadFromGPUTexture(copy,&from,&to);SDL_EndGPUCopyPass(copy);finish(command);auto*data=static_cast<const Row*>(SDL_MapGPUTransferBuffer(device,download,false));require(data,"download map");std::vector<Row>result(data,data+count);SDL_UnmapGPUTransferBuffer(device,download);SDL_ReleaseGPUTransferBuffer(device,download);return result;
 }
};
int main(int argc,char**argv){try{
 require(argc==3,"prepared fixture directory and fresh output directory");const std::filesystem::path root=argv[1],out=argv[2];require(std::filesystem::create_directory(out),"exclusive output directory");const auto input=operands();save(out/"operands.rgba32uint",input);
 Fixture f;f.init(root,input);auto actual=f.draw(f.candidate);save(out/"candidate.rgba32uint",actual);auto legacy=f.draw(f.baseline);save(out/"baseline.rgba32uint",legacy);
 unsigned normals=0,excluded=0,seeds=0,legacy_differences=0,max_legacy_ulp=0;
 for(uint32_t i=0;i<count;++i){const auto &row=input[i],&a=actual[i],&b=legacy[i];
  auto equal=[&](uint32_t got,uint32_t expected,const char*label){if(got!=expected){std::fprintf(stderr,"row %u mode %u input %08x seed %08x %s: got %08x expected %08x\n",i,row[2],row[0],row[1],label,got,expected);throw std::runtime_error(label);}};
  equal(a[2],row[0],"candidate input echo");equal(b[2],row[0],"baseline input echo");equal(a[3],row[3],"candidate coverage tag");equal(b[3],row[3],"baseline coverage tag");
  if(row[2]==1){++seeds;const auto q=rational_q(row[0]);equal(a[0],q,"certified seeded reciprocal");equal(a[1],q,"integer fallback reciprocal");}
  else {equal(a[1],b[0],"legacy expression versus separate baseline");equal(b[1],b[0],"baseline channels");const auto magnitude=row[0]&0x7fffffffu;
   if(magnitude>=0x00800000u && magnitude<=0x7e800000u){++normals;const auto expected=rational_bits(row[0]);equal(a[0],expected,"normal RNE reciprocal");if(b[0]!=expected){++legacy_differences;const auto delta=b[0]>expected?b[0]-expected:expected-b[0];if(delta>max_legacy_ulp)max_legacy_ulp=delta;}}
   else {++excluded;equal(a[0],b[0],"excluded class preserves baseline bits");}
  }
 }
 require(f.draws==2&&f.readbacks==2&&normals+excluded==count/2&&seeds==count/2,"final case counts");std::ofstream report(out/"result.json");report<<"{\"status\":\"PASS\",\"rows\":"<<count<<",\"normal_rows\":"<<normals<<",\"excluded_rows\":"<<excluded<<",\"seed_rows\":"<<seeds<<",\"legacy_normal_differences\":"<<legacy_differences<<",\"maximum_legacy_ulp\":"<<max_legacy_ulp<<",\"draws\":2,\"readbacks\":2,\"format\":\"RGBA32_UINT\",\"scope\":\"ROM-free sampled Metal arithmetic only; no scene, performance or other-backend claim\"}\n";require(bool(report),"report write");std::puts("Metal reciprocal arithmetic PASS: 65536 public rows, two raw uint draws/readbacks");return 0;
}catch(const std::exception&e){std::fprintf(stderr,"Metal reciprocal arithmetic FAIL: %s\n",e.what());return 1;}}
