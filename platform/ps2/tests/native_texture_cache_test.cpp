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
void last_hit_boundaries(){
 Cache cache;auto original=key(500);const auto first=cache.acquire(original);REQUIRE(!first.hit);
 for(unsigned i=0;i<8;++i){const auto age=cache.entry(first.slot).age;const auto hit=cache.acquire(original);
  REQUIRE(hit.hit&&hit.slot==first.slot&&hit.evictions==0);REQUIRE(cache.entry(first.slot).age==age+1);}
 // A recently used identity cannot bypass byte-count or pointer validation.
 const auto before=digest(cache);auto short_buffer=original;--short_buffer.bytes;
 REQUIRE(cache.acquire(short_buffer).slot==96);REQUIRE(digest(cache)==before);
 auto missing=original;missing.indices=nullptr;REQUIRE(cache.acquire(missing).slot==96);REQUIRE(digest(cache)==before);
 // Extra declared bytes do not change an otherwise identical texture key.
 auto extra=original;++extra.bytes;REQUIRE(cache.acquire(extra).hit);
 REQUIRE(cache.entry(first.slot).key.bytes==original.bytes);
 auto moved=original;moved.indices=pixels.data()+1;REQUIRE(!cache.acquire(moved).hit);
 auto generation=moved;++generation.generation;REQUIRE(!cache.acquire(generation).hit);
 auto dimensions=generation;dimensions.width=64;REQUIRE(!cache.acquire(dimensions).hit);
 cache.clear();REQUIRE(cache.used_pages()==0);REQUIRE(!cache.acquire(dimensions).hit);
 // Repeated last-slot hits must update LRU, not merely return the same slot.
 cache.clear();for(unsigned i=1;i<=96;++i)REQUIRE(!cache.acquire(key(i)).hit);
 for(unsigned i=0;i<8;++i)REQUIRE(cache.acquire(key(1)).hit);
 const auto replacement=cache.acquire(key(97));REQUIRE(!replacement.hit&&replacement.evictions==1&&replacement.slot==1);
 REQUIRE(cache.entry(0).key.generation==1&&cache.entry(1).key.generation==97);
 REQUIRE(cache.acquire(key(1)).hit);const auto next=cache.acquire(key(98));REQUIRE(!next.hit&&next.evictions==1&&next.slot==2);
 invariant(cache);
#ifdef RALLY_TEST_CACHE_AGE
 // Only the temporary test header exposes the counter. No shipping API or
 // counter-width change is needed to exercise its2^64 boundary.
 cache.clear();const auto a=cache.acquire(key(7));const auto b=cache.acquire(key(8));REQUIRE(cache.acquire(key(7)).hit);
 cache.age_=std::numeric_limits<std::uint64_t>::max();const auto saved=digest(cache);
 short_buffer=key(7);--short_buffer.bytes;REQUIRE(cache.acquire(short_buffer).slot==96);
 REQUIRE(cache.age_==std::numeric_limits<std::uint64_t>::max()&&digest(cache)==saved);
 REQUIRE(cache.acquire(key(7)).hit);REQUIRE(cache.entry(a.slot).age==3&&cache.entry(b.slot).age==1&&cache.age_==3);
 cache.age_=std::numeric_limits<std::uint64_t>::max();const auto added=cache.acquire(key(9));
 REQUIRE(!added.hit&&cache.entry(added.slot).age==3&&cache.entry(a.slot).age==2&&cache.entry(b.slot).age==1);
 invariant(cache);std::puts("TEXTURE_CACHE AGE_WRAP PASS");
#endif
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
int main(){last_hit_boundaries();cases();}
