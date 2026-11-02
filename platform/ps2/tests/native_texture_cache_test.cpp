#include "native_texture_cache.h"
#include <cstdio>
#include <cstdlib>
#include <new>
namespace {std::size_t allocations=0;}
void*operator new(std::size_t n){++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}
namespace {
using namespace ps2::native;using Cache=detail::TexturePageCache;
unsigned checks=0;
void check(bool okay,unsigned line){++checks;if(!okay){std::fprintf(stderr,"TEXTURE_CACHE FAIL line=%u checks=%u\n",line,checks);std::exit(1);}}
#define REQUIRE(x) check(bool(x),__LINE__)
std::array<std::uint8_t,32768> pixels{};
Texture key(unsigned generation,unsigned w=128,unsigned h=128){return {pixels.data(),std::size_t(w)*h/2,std::uint16_t(w),std::uint16_t(h),generation};}
std::uint64_t digest(const Cache&c){std::uint64_t h=0;for(unsigned i=0;i<Cache::slot_count;++i){const auto&e=c.entry(i);
 for(auto v:{std::uint64_t(e.valid),e.key.generation,e.age,std::uint64_t(e.first_page),std::uint64_t(e.pages),std::uint64_t(e.key.width),std::uint64_t(e.key.height)})h=(h^v)*1099511628211ull;}return h^c.used_pages();}
void invariant(const Cache&c){std::array<bool,96> owned{};unsigned total=0;
 for(unsigned i=0;i<Cache::slot_count;++i){const auto&e=c.entry(i);if(!e.valid)continue;
  REQUIRE(e.pages==1||e.pages==2||e.pages==4);REQUIRE(e.first_page%e.pages==0);REQUIRE(e.first_page+e.pages<=96);
  for(unsigned p=e.first_page;p<e.first_page+e.pages;++p){REQUIRE(!owned[p]);owned[p]=true;++total;}}
 REQUIRE(total==c.used_pages()&&total<=96);
}
void cases(){Cache cache;std::array<std::uint64_t,96> content{};unsigned requests=0;
 const auto request=[&](Texture k){bool known=false;
  for(unsigned i=0;i<96;++i){const auto&e=cache.entry(i);known|=e.valid&&e.key.indices==k.indices&&e.key.generation==k.generation&&e.key.width==k.width&&e.key.height==k.height;}
  const auto before=allocations;const auto r=cache.acquire(k);REQUIRE(allocations==before);REQUIRE(r.slot<96&&r.hit==known);
  const auto&e=cache.entry(r.slot);REQUIRE(e.key.generation==k.generation);REQUIRE(e.pages==Cache::pages_for(k.width,k.height));
  for(unsigned p=e.first_page;p<e.first_page+e.pages;++p){if(r.hit)REQUIRE(content[p]==k.generation);else content[p]=k.generation;}
  invariant(cache);++requests;return r;
 };
 // Ninety-six small images fill exactly the entire pool. A four-page image
 // evicts the oldest four adjacent residents, preserving all other content.
 for(unsigned i=1;i<=96;++i)REQUIRE(!request(key(i)).hit);
 REQUIRE(cache.used_pages()==96);const auto large=request(key(1000,256,256));REQUIRE(large.evictions==4);
 for(unsigned i=5;i<=96;++i)REQUIRE(request(key(i)).hit);
 REQUIRE(request(key(1000,256,256)).hit);
 // Identical pointer with changed generation or dimensions must upload.
 REQUIRE(!request(key(1001,256,256)).hit);REQUIRE(!request(key(1001,128,256)).hit);REQUIRE(request(key(1001,128,256)).hit);
 cache.clear();REQUIRE(cache.used_pages()==0);REQUIRE(!request(key(1001,128,256)).hit);
 // All supported aspect ratios, adversarial churn and sparse LRU holes.
 const std::array<unsigned,6> sides{8,16,32,64,128,256};unsigned generation=2000;
 for(unsigned round=0;round<64;++round)for(unsigned w:sides)for(unsigned h:sides){
  const auto k=key(generation++,w,h);REQUIRE(!request(k).hit);REQUIRE(request(k).hit);}
 const auto preserved=digest(cache);
 for(auto bad:{Texture{},key(9,7,8),key(9,8,512),Texture{pixels.data(),31,8,8,9}}){
  REQUIRE(cache.acquire(bad).slot==96);REQUIRE(digest(cache)==preserved);}
 REQUIRE(Cache::pitch_for(8)==128&&Cache::pitch_for(128)==128&&Cache::pitch_for(256)==256);
 REQUIRE(display_vram_bytes(512)==4161536&&display_vram_bytes(448)==3751936);
 std::printf("TEXTURE_CACHE PASS requests=%u checks=%u page_ownership=1 eviction=1 generation=1 atomic_rejection=1 no_allocations=1\n",requests,checks);
}
}
int main(){cases();}
