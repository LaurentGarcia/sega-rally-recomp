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

static uint32_t number(const std::string &text) {
    size_t used=0; const auto value=std::stoull(text,&used,16);
    if(used!=text.size() || value>std::numeric_limits<uint32_t>::max())
        throw std::runtime_error("address exceeds 32 bit range");
    return uint32_t(value);
}

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
            // Descriptor reads use r4; never overwrite object base/pointer,
            // or the table value before it has been saved.
            if ((in.ea!=EaMode::AbaseOffset && in.ea!=EaMode::Abase) || in.src2!=4 || in.src1+width>32) return {};
            for(unsigned r=in.src1;r<in.src1+width;++r)
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
        const bool comparison=argc==7 && std::string(argv[1])=="--compare";
        const bool masked=argc==7 && std::string(argv[1])=="--masked";
        const bool table_mode=masked || comparison;
        if (!table_mode && argc != 8) throw std::runtime_error("program trusted reachable entries source guest size required");
        std::ifstream f(argv[table_mode?2:1], std::ios::binary); if (!f) throw std::runtime_error("program unavailable");
        const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(f), {}};
        auto trusted = table_mode ? std::set<uint32_t>{} : addresses(argv[2]);
        auto reached = table_mode ? std::set<uint32_t>{} : addresses(argv[3]);
        const uint32_t source = number(argv[table_mode?4:5]);
        const uint32_t guest = number(argv[table_mode?5:6]);
        const uint32_t size = number(argv[table_mode?6:7]);
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
        auto entries = addresses(argv[4]);
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
            if (!proven || entries.contains(start+8)) continue;
            if (comma) std::cout << ','; comma=true;
            std::cout << "{\"start\":"<<start<<",\"target\":"<<p<<",\"size\":"<<p-start<<"}";
        }
        std::cout << "]\n";
    } catch (const std::exception &e) { std::cerr << e.what()<<'\n'; return 1; }
}
