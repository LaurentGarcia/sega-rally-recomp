// Prove bounded native 68000 dispatch candidates using the shared decoder.
// This analyzes supplied program bytes; it contains no game instructions.
#include "m68k/reach.h"
#include <charconv>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
uint32_t number(std::string text) {
    if(text.starts_with("0x"))text.erase(0,2);
    uint32_t value=0;const auto result=std::from_chars(text.data(),text.data()+text.size(),value,16);
    if(result.ec!=std::errc{} || result.ptr!=text.data()+text.size())throw std::runtime_error("invalid hexadecimal address");
    return value;
}
void require(bool valid,const char *message){if(!valid)throw std::runtime_error(message);}
bool interior(uint32_t address,uint32_t start,uint32_t end){return address>start && address<=end;}
}
int main(int argc,char **argv) {
    if(argc!=5){std::fprintf(stderr,"usage: srally_sound_tables PROGRAM.bin START JMP SEEDS.txt\n");return 2;}
    try {
        std::ifstream file(argv[1],std::ios::binary);
        require(bool(file),"cannot read sound program");
        const std::vector<uint8_t> image{std::istreambuf_iterator<char>(file),{}};
        require(image.size()==0x80000,"sound proof requires exactly 512 KiB program");
        const auto read=[&](uint32_t address)->std::optional<uint16_t>{
            if(address>=0x600000 && address<0x67ffff)address-=0x600000;
            else if(address>=15)return std::nullopt;
            if(address&1)return std::nullopt;
            return uint16_t(image[address]<<8|image[address+1]);
        };
        const uint32_t start=number(argv[2]),jump=number(argv[3]);
        require(start>=0x600000 && start<jump && jump<0x67fffe,"proof body outside program");
        auto seeds=m68k::vector_seeds(read,0,4);
        std::ifstream seed_file(argv[4]);require(bool(seed_file),"cannot read sound seeds");
        for(std::string row;std::getline(seed_file,row);)if(!row.empty() && row[0]!='#')seeds.push_back(number(row));
        const auto reach=m68k::reach(seeds,read);
        std::vector<m68k::Insn> body;
        uint32_t cursor=start;
        for(unsigned i=0;i<4;++i){m68k::Insn in;require(m68k::decode(cursor,read,in),"unmapped proof body");
            require(reach.insns.contains(cursor),"proof body is not statically reachable from audited seeds");
            body.push_back(in);cursor+=in.length;}
        require(body[3].addr==jump,"proof body must be four consecutive instructions");
        using m68k::Mode;using m68k::Op;
        const auto &mask=body[0],&shift=body[1],&lea=body[2],&branch=body[3];
        require(mask.op==Op::Andi && (mask.size==2 || mask.size==4) && mask.src.mode==Mode::Imm &&
                (mask.src.imm==0xf0 || mask.src.imm==0x0f) && mask.dst.mode==Mode::Dn,"proof requires a word/long nibble mask");
        require(shift.op==(mask.src.imm==0xf0?Op::Lsr:Op::Lsl) && shift.size<=mask.size && shift.size>0 && shift.src.mode==Mode::Imm &&
                shift.src.imm==2 && shift.dst.mode==Mode::Dn && shift.dst.reg==mask.dst.reg,
                "proof requires same-register logical shift by two with direction matching masked nibble");
        require(lea.op==Op::Lea && lea.src.mode==Mode::PcIndex && !lea.src.xlong && lea.src.xreg==mask.dst.reg &&
                lea.dst.mode==Mode::An,"proof requires a matching PC-relative word index");
        require(branch.op==Op::Jmp && branch.src.mode==Mode::Ind && branch.src.reg==lea.dst.reg,
                "proof requires an indirect jump through the proven address register");
        std::set<uint32_t> entries(seeds.begin(),seeds.end());
        for(const auto &[pc,in]:reach.insns) {
            if(m68k::is_branch(in))entries.insert(in.target);
            if((in.op==Op::Jmp || in.op==Op::Jsr) && !m68k::is_indirect(in))entries.insert(in.src.addr);
            if((pc<start || pc>jump) && !m68k::ends_block(in))entries.insert(pc+in.length);
        }
        for(auto entry:entries)require(!interior(entry,start,jump),"known control-flow entry bypasses the mask");
        const uint32_t base=lea.src.addr;
        require(!(base&1) && base>=cursor && base<=0x680000-16*4,"bounded table is outside program or overlaps proof body");
        std::vector<uint32_t> targets;
        for(unsigned index=0;index<16;++index){
            m68k::Insn in;require(m68k::decode(base+4*index,read,in),"unmapped bounded entry");
            require(in.op==Op::Jmp && in.length==4 && in.src.mode==Mode::PcDisp,
                    "each bounded slot must be one four-byte PC-relative jump");
            require(in.src.addr>=0x600000 && in.src.addr<0x67fffe && !(in.src.addr&1),"bounded jump target outside aligned program");
            require(!interior(in.src.addr,start,jump),"bounded target bypasses proof mask");
            targets.push_back(in.src.addr);
        }
        std::printf("{\"start\":%u,\"jump\":%u,\"body_size\":%u,\"table_base\":%u,\"entries\":16,\"stride\":4,\"candidates\":[",
                    start,jump,cursor-start,base);
        for(unsigned i=0;i<16;++i)std::printf("%s{\"address\":%u,\"direct_target\":%u}",i?",":"",base+4*i,targets[i]);
        std::puts("]}");return 0;
    } catch(const std::exception &error){std::fprintf(stderr,"sound table proof: %s\n",error.what());return 2;}
}
