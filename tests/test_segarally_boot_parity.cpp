// Test oracle only: native reset execution versus the inherited i960 core.
// ROM images and generated native objects are supplied from ignored build/.
#include "refcore/i960_ref.h"
#include "runtime/gen_support.h"
#include "runtime/model2a_bus.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <tuple>
#include <vector>
#ifdef M2_SRALLY_TGP
#include "runtime/m2_tgp_board.h"
#include <memory>
struct NativeTgpModel : rt::TgpBoardModel {
    rt::TgpBoard& board;
    explicit NativeTgpModel(rt::TgpBoard& b):TgpBoardModel(b),board(b) {}
    Check read(uint32_t address,uint32_t mask,uint32_t& value) override {
        if(address>=0x00900000 && address<=0x0097ffff) board.sync();
        return TgpBoardModel::read(address,mask,value);
    }
};
#endif

struct ObservedBus : rt::Bus {
    rt::Model2ABus board;
    std::vector<std::tuple<uint32_t,unsigned,uint32_t>> writes;
    ObservedBus(const std::vector<uint8_t>& p,const std::vector<uint8_t>& d):board(p,d) {}
    uint32_t fetch(uint32_t a) override { return board.fetch(a); }
    uint8_t read_byte(uint32_t a) override { return board.read_byte(a); }
    uint16_t read_word(uint32_t a) override { return board.read_word(a); }
    uint32_t read_dword(uint32_t a) override { return board.read_dword(a); }
    uint16_t flags(uint32_t a) override { return board.flags(a); }
    void write_byte(uint32_t a,uint8_t v) override { board.write_byte(a,v);writes.emplace_back(a,1,v); }
    void write_word(uint32_t a,uint16_t v) override { board.write_word(a,v);writes.emplace_back(a,2,v); }
    void write_dword(uint32_t a,uint32_t v) override { board.write_dword(a,v);writes.emplace_back(a,4,v); }
};

static std::vector<uint8_t> load(const char* path) {
    std::ifstream file(path,std::ios::binary);
    if(!file) throw rt::Fatal("cannot read input image");
    return {std::istreambuf_iterator<char>(file),{}};
}
static bool same(const rt::Cpu& a,const rt::Cpu& b) {
    // PIP is the interpreter's last decoded instruction, a diagnostic field
    // that generated code does not update. It is not architectural state.
    return !std::memcmp(a.m_r,b.m_r,sizeof(a.m_r)) &&
        !std::memcmp(a.m_fp,b.m_fp,sizeof(a.m_fp)) &&
        !std::memcmp(a.m_rcache,b.m_rcache,sizeof(a.m_rcache)) &&
        !std::memcmp(a.m_rcache_frame_addr,b.m_rcache_frame_addr,sizeof(a.m_rcache_frame_addr)) &&
        !std::memcmp(a.m_irq_line_state,b.m_irq_line_state,sizeof(a.m_irq_line_state)) &&
        a.m_rcache_pos==b.m_rcache_pos && a.m_IP==b.m_IP && a.m_AC==b.m_AC &&
        a.m_PC==b.m_PC && a.m_SAT==b.m_SAT && a.m_PRCB==b.m_PRCB && a.m_ICR==b.m_ICR &&
        a.m_immediate_irq==b.m_immediate_irq && a.m_immediate_vector==b.m_immediate_vector &&
        a.m_immediate_pri==b.m_immediate_pri;
}
int main(int argc,char** argv) {
    if(argc!=4 && argc!=6) return 2;
    try {
        auto p=load(argv[1]),d=load(argv[2]);
        ObservedBus nb(p,d),rb(p,d);
#ifdef M2_SRALLY_TGP
        std::unique_ptr<rt::TgpBoard> nt,rtgp;
        std::unique_ptr<NativeTgpModel> nm,rm;
        if(argc==6) {
            auto tables=load(argv[4]),data=load(argv[5]);
            nt=std::make_unique<rt::TgpBoard>(tables,data);
            rtgp=std::make_unique<rt::TgpBoard>(tables,data);
            nm=std::make_unique<NativeTgpModel>(*nt);rm=std::make_unique<NativeTgpModel>(*rtgp);
            nb.board.set_model(nm.get());rb.board.set_model(rm.get());
        }
#else
        if(argc==6) throw rt::Fatal("TGP support not linked into test");
#endif
        rt::Cpu native(&nb);rt::I960Ref ref(&rb);
        native.reset();ref.reset();
        if(!same(native,ref) || nb.writes!=rb.writes) throw rt::Fatal("reset parity mismatch");
        rt::Lockstep ls(native);gen::Env env{native,ls};
        const uint64_t limit=std::stoull(argv[3]);
        uint64_t writes=0;
        while(ls.count<limit) {
            const auto pc=native.m_IP;
            if(!gen::has_code(pc)) {
                std::printf("PASS native/reference parity instructions=%llu writes=%llu frontier=missing_code pc=%08x\n",
                    (unsigned long long)ls.count,(unsigned long long)writes,pc);
                return 0;
            }
            nb.writes.clear();rb.writes.clear();
            ls.end_count=ls.count+1;
            try {
                while(!ls.finished()) gen::run(env);
            } catch(const rt::Model2AAccess& access) {
                try { ref.execute_one(); }
                catch(const rt::Model2AAccess& other) {
                    // The interpreter advances IP before executing; generated
                    // code leaves IP at this instruction until it completes.
                    // Neither engine completed the failing instruction.
                    ref.m_IP=native.m_IP;
                    if(access.address==other.address && access.width==other.width &&
                       access.write==other.write && access.value==other.value &&
                       nb.writes==rb.writes && same(native,ref)) {
                        std::printf("PASS native/reference parity instructions=%llu writes=%llu frontier=device pc=%08x address=%08x\n",
                            (unsigned long long)ls.count,(unsigned long long)writes,pc,access.address);
                        return 0;
                    }
                    std::fprintf(stderr,"Frontier mismatch count=%llu pc=%08x native=%08x/%u/%d/%08x reference=%08x/%u/%d/%08x state=%d writes=%d\n",
                        (unsigned long long)ls.count,pc,access.address,access.width,access.write,access.value,
                        other.address,other.width,other.write,other.value,same(native,ref),nb.writes==rb.writes);
                }
                throw rt::Fatal("device frontier parity mismatch");
            }
            ref.execute_one();
            if(!same(native,ref) || nb.writes!=rb.writes) {
                std::fprintf(stderr,"Parity mismatch count=%llu pc=%08x native_ip=%08x reference_ip=%08x\n",
                    (unsigned long long)ls.count,pc,native.m_IP,ref.m_IP);
                for(unsigned i=0;i<32;i++) if(native.m_r[i]!=ref.m_r[i])
                    std::fprintf(stderr,"r%u native=%08x reference=%08x\n",i,native.m_r[i],ref.m_r[i]);
                return 1;
            }
            writes+=nb.writes.size();
        }
        std::printf("PASS native/reference parity instructions=%llu writes=%llu frontier=instruction_limit pc=%08x\n",
            (unsigned long long)ls.count,(unsigned long long)writes,native.m_IP);
        return 0;
    } catch(const std::exception& e) { std::fprintf(stderr,"boot parity: %s\n",e.what());return 1; }
}
