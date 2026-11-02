#pragma once
#include "native_display.h"
#include <array>
#include <limits>

namespace ps2::native::detail {
// GS PSMT4 pages are128x128 pixels/8192 bytes. Reserving complete pages avoids
// relying on contiguous sub-page block footprints for narrow/short textures.
class TexturePageCache {
public:
    static constexpr unsigned page_count=96,page_bytes=8192,slot_count=96;
    struct Entry { Texture key{};std::uint64_t age=0;unsigned first_page=0,pages=0;bool valid=false; };
    struct Result {unsigned slot=slot_count,evictions=0;bool hit=false;};
    static constexpr unsigned pages_for(unsigned width,unsigned height) noexcept {
        return ((width+127)/128)*((height+127)/128);
    }
    static constexpr unsigned pitch_for(unsigned width) noexcept {return width<=128?128:256;}
    Result acquire(const Texture& key) noexcept {
        const auto side=[](unsigned n){return n>=8&&n<=256&&(n&(n-1))==0;};
        if(!key.indices||!side(key.width)||!side(key.height)||key.bytes<std::size_t(key.width)*key.height/2)return {};
        // Rebase ages on the exceptionally rare counter boundary without
        // changing the strict LRU order or identities. Fixed bounded scratch.
        if(age_==std::numeric_limits<std::uint64_t>::max())rebase();
        // The last hit still undergoes the complete validated identity check.
        // Keep its LRU update identical to a hit found by the full scan.
        if(last_<slot_count){auto& e=entries_[last_];
            if(e.valid&&e.key.indices==key.indices&&e.key.generation==key.generation&&
               e.key.width==key.width&&e.key.height==key.height){e.age=++age_;return {last_,0,true};}
        }
        for(unsigned i=0;i<slot_count;++i){auto& e=entries_[i];
            if(e.valid&&e.key.indices==key.indices&&e.key.generation==key.generation&&
               e.key.width==key.width&&e.key.height==key.height){e.age=++age_;last_=i;return {i,0,true};}
        }
        const unsigned pages=pages_for(key.width,key.height);unsigned evictions=0;
        auto first=free_range(pages);
        while(first==page_count){
            unsigned oldest=slot_count;
            for(unsigned i=0;i<slot_count;++i)if(entries_[i].valid&&
                (oldest==slot_count||entries_[i].age<entries_[oldest].age))oldest=i;
            // A valid <=4-page request always fits after at most96 evictions.
            auto& e=entries_[oldest];
            for(unsigned p=e.first_page;p<e.first_page+e.pages;++p)owners_[p]=0;
            used_pages_-=e.pages;e.valid=false;++evictions;first=free_range(pages);
        }
        unsigned slot=0;while(entries_[slot].valid)++slot;
        entries_[slot]={key,++age_,first,pages,true};
        for(unsigned p=first;p<first+pages;++p)owners_[p]=static_cast<std::uint8_t>(slot+1);
        used_pages_+=pages;last_=slot;return {slot,evictions,false};
    }
    const Entry& entry(unsigned slot)const noexcept{return entries_[slot];}
    unsigned used_pages()const noexcept{return used_pages_;}
    void clear() noexcept{entries_={};owners_={};age_=0;used_pages_=0;last_=slot_count;}
private:
    unsigned free_range(unsigned pages)const noexcept {
        for(unsigned first=0;first+pages<=page_count;first+=pages){
            bool available=true;for(unsigned i=0;i<pages;++i)available=available&&!owners_[first+i];
            if(available)return first;
        }return page_count;
    }
    void rebase() noexcept {
        std::array<std::uint64_t,slot_count> old{};
        for(unsigned i=0;i<slot_count;++i)old[i]=entries_[i].age;
        age_=0;
        for(unsigned i=0;i<slot_count;++i)if(entries_[i].valid){
            std::uint64_t rank=1;
            for(unsigned j=0;j<slot_count;++j)if(entries_[j].valid&&old[j]<old[i])++rank;
            entries_[i].age=rank;if(rank>age_)age_=rank;
        }
    }
    std::array<Entry,slot_count> entries_{};
    std::array<std::uint8_t,page_count> owners_{};
    std::uint64_t age_=0;unsigned used_pages_=0,last_=slot_count;
};
}
