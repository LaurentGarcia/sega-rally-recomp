// Compiler-side continuation proof using the inherited i960 decoder.
// No ROM content is embedded; output contains addresses and span lengths only.
#include "i960/decode.h"
#include <fstream>
#include <iostream>
#include <set>
#include <vector>
#include <stdexcept>
#include <functional>
#include <optional>
#include <limits>
#include <array>

static uint32_t number(const std::string &text) {
    size_t used=0; const auto value=std::stoull(text,&used,16);
    if(used!=text.size() || value>std::numeric_limits<uint32_t>::max())
        throw std::runtime_error("address exceeds 32 bit range");
    return uint32_t(value);
}

// Only these decoded operations may cross an object reaching definition or a
// returning leaf. Footprints follow the shared executor: wide memory loads
// round their register base down, whereas emul writes dst and dst+1 directly.
static std::optional<uint32_t> written_registers(const i960::Insn &in) {
    using namespace i960;
    if (!in.valid() || in.quirks || in.op->flow!=Flow::None) return {};
    if (in.fmt==Format::Mem && in.ea!=EaMode::Reserved) {
        switch(in.op->code) {
        case 0x82: case 0x92: case 0x9a: case 0xb2: return 0;
        case 0x80: case 0x8c: case 0x90: return uint32_t(1)<<in.src1;
        case 0x98: return uint32_t(3)<<(in.src1&0x1e);
        case 0xb0: return uint32_t(15)<<(in.src1&0x1c);
        default: return {};
        }
    }
    if (in.fmt!=Format::Reg) return {};
    // chkbit has no destination: its otherwise unused m3 bit is ignored by
    // the executor (and is set in the returning leaf used by this ROM).
    if(in.op->code==0x5ae) return 0;
    if(in.m3) return {};
    switch(in.op->code) {
    case 0x5cc: case 0x581: case 0x58c: case 0x590: case 0x592:
    case 0x5b0: return uint32_t(1)<<in.dst;
    case 0x670:
        if(in.dst<31) return uint32_t(3)<<in.dst;
        return {};
    default: return {};
    }
}

struct LeafProof { uint32_t start, size, clobbers; };
struct LeafCall { uint32_t call; LeafProof leaf; };

struct CallbackProof { uint32_t table, count, size, fallback=0; };
static std::optional<CallbackProof> compare_proof(uint32_t start,
    const std::function<i960::Insn(uint32_t)> &decode) {
    using namespace i960;
    auto bound=decode(start);
    if(!bound.valid() || bound.op->code!=0x5a0 || bound.m1 || !bound.m2 || bound.src2>31) return {};
    const auto index=bound.src1;
    uint32_t p=start+4;
    for(unsigned n=0;n<16;++n) {
        auto in=decode(p); if(!in.valid()) return {};
        // st/stq preserve both unsigned comparison flags and index register.
        // Restrict the audited spill idiom to fixed offsets from frame pointer.
        if(in.fmt==Format::Mem && (in.op->code==0x92 || in.op->code==0xb2) &&
            in.ea==EaMode::AbaseOffset && in.src2==31 &&
            (in.op->code!=0xb2 || in.src1<=28)) { p+=in.length; continue; }
        if(in.op->code!=0x11 || !in.has_target) return {};
        auto load=decode(p+4), branch=decode(p+12);
        if(!load.valid() || load.op->code!=0x90 || load.ea!=EaMode::IndexDisp ||
            load.index!=index || load.scale!=2 || load.length!=8 || !branch.valid() ||
            branch.op->code!=0x84 || branch.ea!=EaMode::Abase || branch.src2!=load.src1) return {};
        return CallbackProof{load.disp,uint32_t(bound.src2)+1,p+16-start,in.target};
    }
    return {};
}
static std::optional<CallbackProof> callback_proof(uint32_t start,
    const std::function<i960::Insn(uint32_t)> &decode) {
    using namespace i960;
    auto mask=decode(start), load=decode(start+4), pointer=decode(start+12);
    if (!mask.valid() || mask.op->code!=0x581 || !mask.m1 || mask.m2 || mask.m3 ||
        mask.src1!=31 || mask.src2!=20 || mask.dst!=20 || !load.valid() ||
        load.op->code!=0x90 || load.ea!=EaMode::IndexDisp || load.index!=20 || load.scale!=2 ||
        load.src1!=20 || load.length!=8 || !pointer.valid() || pointer.op->code!=0x590 ||
        pointer.m1 || !pointer.m2 || pointer.m3 || pointer.src1!=16 || pointer.src2!=4 || pointer.dst!=21)
        return {};
    bool saved=false, reloaded=false, compared=false, skipped=false;
    uint32_t p=start+16;
    for (unsigned n=0;n<64;++n) {
        auto in=decode(p); if (!in.valid() || in.quirks) return {};
        if (in.op->code==0x86 && in.ea==EaMode::Abase && in.src2==21 && skipped)
            return CallbackProof{load.disp,32,p+4-start};
        if (skipped) return {}; // null branch must immediately precede callx
        if (in.op->code==0x12 && compared && in.target==p+8) { skipped=true; p+=4; continue; }
        if (in.fmt==Format::Reg && in.op->code==0x5a1 && reloaded && !compared &&
            !in.m1 && in.m2 && in.src1==21 && in.src2==0) { compared=true;p+=4;continue; }
        if (in.fmt!=Format::Mem || in.op->flow!=Flow::None || in.ea==EaMode::Reserved) return {};
        if (in.op->code==0x92 && in.ea==EaMode::Abase && in.src2==21 && in.src1==20 && !saved) {
            saved=true;p+=4;continue;
        }
        if (in.op->code==0x90 && in.ea==EaMode::Abase && in.src2==21 && in.src1==21 && saved && !reloaded) {
            reloaded=true;p+=4;continue;
        }
        if (in.op->code==0x92 || in.op->code==0x9a) {
            // Other object stores cannot overlap the saved pointer at +4.
            // Include both words of stl, and reject every alternate base/EA.
            const uint32_t width=in.op->code==0x9a?8:4;
            if (in.ea!=EaMode::AbaseOffset || in.src2!=16 || (width==8 && in.src1>30) ||
                (in.offset<8 && in.offset+width>4)) return {};
        } else if (in.op->code==0x90 || in.op->code==0x98) {
            const unsigned width=in.op->code==0x98?2:1;
            const unsigned base=in.src1 & (width==2?0x1e:0x1f);
            // Descriptor reads use r4; never overwrite object base/pointer,
            // or the table value before it has been saved.
            if ((in.ea!=EaMode::AbaseOffset && in.ea!=EaMode::Abase) || in.src2!=4 || in.src1+width>32) return {};
            for(unsigned r=base;r<base+width;++r)
                if(r==16 || r==21 || (!saved && r==20)) return {};
        } else return {};
        p+=in.length;
    }
    return {};
}

static std::set<uint32_t> addresses(const char *path) {
    std::ifstream f(path); if (!f) throw std::runtime_error("address file unavailable");
    std::set<uint32_t> result; std::string word;
    while (f >> word) result.insert(number(word));
    return result;
}
int main(int argc, char **argv) {
    try {
        const bool updates=argc==10 && std::string(argv[1])=="--updates";
        const bool comparison=argc==7 && std::string(argv[1])=="--compare";
        const bool masked=argc==7 && std::string(argv[1])=="--masked";
        const bool table_mode=masked || comparison;
        if (!table_mode && !updates && argc != 8) throw std::runtime_error("program trusted reachable entries source guest size required");
        std::ifstream f(argv[table_mode||updates?2:1], std::ios::binary); if (!f) throw std::runtime_error("program unavailable");
        const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(f), {}};
        auto trusted = table_mode ? std::set<uint32_t>{} : addresses(argv[updates?3:2]);
        auto reached = table_mode ? std::set<uint32_t>{} : addresses(argv[updates?4:3]);
        const uint32_t source = number(argv[updates?6:table_mode?4:5]);
        const uint32_t guest = number(argv[updates?7:table_mode?5:6]);
        const uint32_t size = number(argv[updates?8:table_mode?6:7]);
        if ((source | guest | size) & 3 || !size || uint64_t(source)+size > bytes.size() ||
            guest < 0x500000 || uint64_t(guest)+size > 0x600000)
            throw std::runtime_error("invalid verified copy span");
        auto word = [&](uint32_t p) { return uint32_t(bytes[p]) | uint32_t(bytes[p+1])<<8 |
            uint32_t(bytes[p+2])<<16 | uint32_t(bytes[p+3])<<24; };
        auto decode = [&](uint32_t p) {
            if (p < source || uint64_t(p)+4 > uint64_t(source)+size)
                return i960::Insn{};
            auto in = i960::decode(p, word(p), uint64_t(p)+8 <= uint64_t(source)+size ? word(p+4) : 0);
            if ((!table_mode && !reached.contains(p)) || !in.valid() || in.quirks || uint64_t(p)+in.length > uint64_t(source)+size)
                return i960::Insn{};
            return in;
        };
        if (table_mode) {
            const auto start=number(argv[3]);
            const auto result=comparison?compare_proof(start,decode):callback_proof(start,decode);
            if (!result) throw std::runtime_error("table operand/dataflow proof failed");
            std::cout<<"{\"table\":"<<result->table<<",\"count\":"<<result->count<<",\"size\":"<<result->size;
            if(comparison) {
                if(result->fallback<source || uint64_t(result->fallback)>=uint64_t(source)+size)
                    throw std::runtime_error("unsigned rejection target exceeds verified copy");
                std::cout<<",\"fallback\":"<<guest+result->fallback-source;
            }
            std::cout<<"}\n";
            return 0;
        }
        // Any known seed or direct transfer into the interior invalidates the
        // straight-line reaching definition. Unknown indirect entries are not
        // resolved by this pass and do not receive an exclusivity claim.
        auto entries = addresses(argv[updates?5:4]);
        for (auto p : reached) {
            if(uint64_t(p)+4>bytes.size()) continue;
            // Incoming direct edges can originate outside this copied span.
            auto in=i960::decode(p,word(p),uint64_t(p)+8<=bytes.size()?word(p+4):0);
            if (in.valid() && !in.quirks && uint64_t(p)+in.length<=bytes.size() && in.has_target) {
                auto target = in.target;
                if (target >= guest && uint64_t(target) < uint64_t(guest)+size)
                    target = source + target - guest;
                entries.insert(target);
            }
        }
        auto interior_entry=[&](uint32_t start,uint32_t end) {
            auto found=entries.upper_bound(start);
            return found!=entries.end() && *found<end;
        };
        auto returning_leaf=[&](uint32_t start)->std::optional<LeafProof> {
            // BAL shares the caller's registers; there is no callee-save ABI
            // assumption. Prove a straight-line return to its incoming g14.
            uint32_t links=uint32_t(1)<<30, clobbers=0, p=start;
            for(unsigned n=0;n<64;++n) {
                auto in=decode(p);
                if(!in.valid() || interior_entry(start,p+in.length)) return {};
                if(in.op->code==0x84 && in.ea==i960::EaMode::Abase &&
                    (links & (uint32_t(1)<<in.src2)))
                    return LeafProof{start,p+in.length-start,clobbers};
                const auto writes=written_registers(in);
                if(!writes) return {};
                const bool copied_link=in.op->code==0x5cc && !in.m1 &&
                    (links & (uint32_t(1)<<in.src1));
                links &= ~*writes;
                if(copied_link) links |= uint32_t(1)<<in.dst;
                clobbers |= *writes;
                p+=in.length;
            }
            return {};
        };
        if(updates) {
            using namespace i960;
            const auto dispatcher=number(argv[9]);
            auto load=decode(dispatcher), guard=decode(dispatcher+4);
            auto move=decode(dispatcher+8), call=decode(dispatcher+12);
            if(!load.valid() || load.op->code!=0x90 || load.ea!=EaMode::AbaseOffset ||
                load.src2!=4 || load.offset!=4 || load.src1!=20 || !guard.valid() ||
                guard.op->code!=0x3a || !guard.m1 || guard.src1!=0 || guard.src2!=20 ||
                guard.target!=dispatcher+16 || !move.valid() || move.op->code!=0x5cc ||
                move.m1 || move.m3 || move.src1!=4 || move.dst!=16 || !call.valid() ||
                call.op->code!=0x86 || call.ea!=EaMode::Abase || call.src2!=20 ||
                interior_entry(dispatcher,dispatcher+16))
                throw std::runtime_error("object update dispatcher operand/side-entry proof failed");
            bool comma=false;std::cout<<'[';
            for(auto start:trusted) {
                // A trusted constructor/update receives the object in g0.
                // Track saved aliases explicitly, including across proven
                // returning leaves. Unknown control flow remains a barrier.
                std::array<std::optional<std::pair<uint32_t,uint32_t>>,32> definitions{};
                uint32_t objects=uint32_t(1)<<16;
                std::vector<LeafCall> calls;
                auto kill=[&](uint32_t writes) {
                    objects &= ~writes;
                    for(unsigned r=0;r<32;++r)
                        if(writes & (uint32_t(1)<<r)) definitions[r]={};
                };
                uint32_t p=start;
                for(unsigned n=0;n<64;++n) {
                    auto in=decode(p);
                    if(!in.valid() || interior_entry(start,p+in.length)) break;
                    if(in.op->code==0x0b && in.has_target) {
                        auto target=in.target;
                        if(target>=guest && uint64_t(target)<uint64_t(guest)+size)
                            target=source+target-guest;
                        if(target&3) break;
                        const auto leaf=returning_leaf(target);
                        if(!leaf) break;
                        kill(leaf->clobbers | (uint32_t(1)<<30));
                        calls.push_back({p,*leaf});
                    } else {
                        const auto writes=written_registers(in);
                        if(!writes) break;
                        if(in.fmt==Format::Mem) {
                            if(in.op->code==0x92 && in.ea==EaMode::AbaseOffset &&
                                (objects & (uint32_t(1)<<in.src2)) && in.offset==4 && definitions[in.src1]) {
                                auto [definition,target]=*definitions[in.src1];
                                if(comma) std::cout<<',';comma=true;
                                std::cout<<"{\"root\":"<<start<<",\"definition\":"<<definition<<
                                    ",\"store\":"<<p<<",\"target\":"<<target<<",\"register\":"<<unsigned(in.src1)<<
                                    ",\"object_base_register\":"<<unsigned(in.src2)<<",\"leaf_calls\":[";
                                for(size_t i=0;i<calls.size();++i) {
                                    const auto &c=calls[i];if(i) std::cout<<',';
                                    std::cout<<"{\"call\":"<<c.call<<",\"source\":"<<c.leaf.start<<
                                        ",\"size\":"<<c.leaf.size<<",\"clobbered_registers\":[";
                                    bool separator=false;
                                    for(unsigned r=0;r<32;++r) if(c.leaf.clobbers & (uint32_t(1)<<r)) {
                                        if(separator) std::cout<<','; separator=true; std::cout<<r;
                                    }
                                    std::cout<<"]}";
                                }
                                std::cout<<"]}";
                            }
                            kill(*writes);
                            if(in.op->code==0x8c && in.ea==EaMode::Disp && !(in.disp&3) &&
                                in.disp>=guest && uint64_t(in.disp)<uint64_t(guest)+size)
                                definitions[in.src1]=std::pair{p,in.disp};
                        } else {
                            const auto constant=definitions[in.src1];
                            const bool object=objects & (uint32_t(1)<<in.src1);
                            kill(*writes);
                            if(in.op->code==0x5cc && !in.m1) {
                                definitions[in.dst]=constant;
                                if(object) objects |= uint32_t(1)<<in.dst;
                            }
                        }
                    }
                    p+=in.length;
                }
            }
            std::cout<<"]\n";return 0;
        }
        bool comma=false; std::cout << "[";
        for (auto start : trusted) {
            auto load=decode(start), move=decode(start+8);
            if (!load.valid() || load.op->code != 0x8c || load.ea != i960::EaMode::Disp ||
                load.src1 != 30 || load.length != 8 || !move.valid() || move.op->code != 0x5cc ||
                move.m1 || move.m3 || move.src1 != 30 || move.dst != 17) continue;
            uint32_t p=start+12; bool proven=false;
            for (unsigned n=0; n<64; ++n) {
                auto in=decode(p); if (!in.valid() || entries.contains(p)) break;
                if (in.op->code==0x84 && in.ea==i960::EaMode::Abase && in.src2==17) {
                    // Restrict to the compiler's adjacent continuation idiom.
                    proven=load.disp==guest+p+4-source; p+=4; break;
                }
                if (in.op->flow != i960::Flow::None) break;
                bool allowed=false;
                if (in.fmt==i960::Format::Reg)
                    allowed=(in.op->code==0x5cc || in.op->code==0x581 || in.op->code==0x58c) && !in.m3 && in.dst!=17;
                else if (in.fmt==i960::Format::Mem && in.ea!=i960::EaMode::Reserved)
                    allowed=(in.op->code==0x82 || in.op->code==0x92 ||
                        ((in.op->code==0x80 || in.op->code==0x90 || in.op->code==0x8c) && in.src1!=17));
                if (!allowed) break;
                p+=in.length;
            }
            // The mov itself is also an interior entry, checked separately.
            if (!proven || interior_entry(start,p)) continue;
            if (comma) std::cout << ','; comma=true;
            std::cout << "{\"start\":"<<start<<",\"target\":"<<p<<",\"size\":"<<p-start<<"}";
        }
        std::cout << "]\n";
    } catch (const std::exception &e) { std::cerr << e.what()<<'\n'; return 1; }
}
