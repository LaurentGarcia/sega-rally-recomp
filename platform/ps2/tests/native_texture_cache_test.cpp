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
void resident_touch_differential(){
 Cache ordinary,memo;unsigned requests=0,hits=0,misses=0;
 const auto equal=[](const Cache&a,const Cache&b){
  REQUIRE(a.used_pages()==b.used_pages());
  for(unsigned i=0;i<Cache::slot_count;++i){const auto&x=a.entry(i);const auto&y=b.entry(i);
   REQUIRE(x.valid==y.valid&&x.key.indices==y.key.indices&&x.key.bytes==y.key.bytes&&
           x.key.width==y.key.width&&x.key.height==y.key.height&&x.key.generation==y.key.generation&&
           x.age==y.age&&x.first_page==y.first_page&&x.pages==y.pages);
  }
#ifdef RALLY_TEST_CACHE_AGE
  REQUIRE(a.owners_==b.owners_&&a.age_==b.age_&&a.last_==b.last_&&a.used_pages_==b.used_pages_);
#endif
 };
 const auto request=[&](Texture k,unsigned slot){
  const auto saved=memo;const auto allocated=allocations;
  const bool hit=memo.try_touch_resident(slot,k);
  if(!hit)equal(memo,saved); // Even an age-wrap miss must not rebase.
  const auto expected=ordinary.acquire(k);
  const auto actual=hit?Cache::Result{slot,0,true}:memo.acquire(k);
  REQUIRE(actual.slot==expected.slot&&actual.evictions==expected.evictions&&actual.hit==expected.hit);
  if(hit)REQUIRE(expected.hit&&expected.slot==slot);
  equal(ordinary,memo);REQUIRE(allocated==allocations);
  ++requests;if(hit)++hits;else ++misses;return hit;
 };
 REQUIRE(!request(key(1),0));REQUIRE(request(key(1),0));
 // Wrong resident slot, out-of-range slot and cleared/reused slot must not
 // turn a stale memo into a hit or disturb subsequent fallback LRU order.
 REQUIRE(!request(key(2),0));REQUIRE(!request(key(1),1));
 REQUIRE(!request(key(1),Cache::slot_count));
 REQUIRE(!request(key(1),std::numeric_limits<unsigned>::max()));
 REQUIRE(request(key(2),1));
 auto extra=key(2);++extra.bytes;REQUIRE(request(extra,1));
 auto short_buffer=key(2);--short_buffer.bytes;REQUIRE(!request(short_buffer,1));
 auto absent=key(2);absent.indices=nullptr;REQUIRE(!request(absent,1));
 REQUIRE(!request(key(2,7,8),1));REQUIRE(!request(key(2,8,512),1));
 REQUIRE(!request(key(3),1));REQUIRE(!request(key(2,64,128),1));
 auto moved=key(2);moved.indices=pixels.data()+16;REQUIRE(!request(moved,1));
 REQUIRE(!request(key(2,128,64),1));
 auto high_generation=key(2);high_generation.generation=std::numeric_limits<std::uint64_t>::max();
 REQUIRE(!request(high_generation,1));
 ordinary.clear();memo.clear();REQUIRE(!request(key(2),1));
 REQUIRE(!request(key(9),0));REQUIRE(!request(key(2),1));
 // Fill all pages, repeatedly touch an older key, then require the same
 // four-page eviction and sparse-page reuse decisions as ordinary acquire.
 ordinary.clear();memo.clear();
 for(unsigned i=1;i<=96;++i)REQUIRE(!request(key(i),Cache::slot_count));
 for(unsigned i=0;i<8;++i)REQUIRE(request(key(1),0));
 REQUIRE(!request(key(1000,256,256),0));
 REQUIRE(!request(key(1001,128,256),1));
 // Deterministic mixed hits, stale identities/slots, invalid metadata and
 // new textures cover all page sizes without relying on private game data.
 std::uint32_t random=0xa341316cu;const std::array<unsigned,6>sides{8,16,32,64,128,256};
 for(unsigned i=0;i<8192;++i){
  random=random*1664525u+1013904223u;unsigned slot=random%Cache::slot_count;
  Texture k=ordinary.entry(slot).valid?ordinary.entry(slot).key:key(2000+i,sides[(random>>8)%6],sides[(random>>16)%6]);
  if(i%11==0)k=key(12000+i,sides[(random>>8)%6],sides[(random>>16)%6]);
  if(i%19==0)k.bytes=0;
  if(i%23==0)++k.generation;
  if(i%31==0)k.indices=nullptr;
  if(i%13==0)slot=(slot+1)%Cache::slot_count;
  if(i%7==0)slot=Cache::slot_count;
  request(k,slot);
 }
#ifdef RALLY_TEST_CACHE_AGE
 ordinary.clear();memo.clear();request(key(7),0);request(key(8),1);request(key(7),0);
 ordinary.age_=memo.age_=std::numeric_limits<std::uint64_t>::max();
 const auto boundary=memo;
 REQUIRE(!memo.try_touch_resident(0,Texture{}));equal(memo,boundary);
 REQUIRE(!memo.try_touch_resident(1,key(7)));equal(memo,boundary);
 REQUIRE(!memo.try_touch_resident(Cache::slot_count,key(7)));equal(memo,boundary);
 REQUIRE(request(key(7),0));REQUIRE(memo.age_==3);
 ordinary.age_=memo.age_=std::numeric_limits<std::uint64_t>::max();
 REQUIRE(!request(key(9),0));REQUIRE(memo.age_==3);
 std::puts("TEXTURE_CACHE TOUCH_PRIVATE PASS owners=1 age=1 last=1 wrap=1");
#endif
 invariant(ordinary);invariant(memo);REQUIRE(hits>1000&&misses>1000);
 std::printf("TEXTURE_CACHE TOUCH PASS requests=%u hits=%u misses=%u stale_identity=1 atomic_miss=1 exact_lru=1 no_allocations=1\n",requests,hits,misses);
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
int main(){last_hit_boundaries();resident_touch_differential();cases();}
