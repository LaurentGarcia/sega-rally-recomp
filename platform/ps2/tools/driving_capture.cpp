#include "driving_capture.h"
#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace ps2::offline {
namespace {
constexpr std::uint32_t relocation = 0x59f000;
constexpr std::size_t byte_limit = 16 * 1024 * 1024;
struct ReturnFrame {
    std::uint32_t pc=0,frame_pointer=0;
    int cache_index=-1;
    std::array<std::uint32_t,16> words{};
};
struct Call {
    std::uint64_t id = 0, instructions = 0, frame = 0;
    std::uint32_t object = 0, descriptor = 0, return_pc = 0, caller_fp = 0, entry_pc = 0, branch_selector = 0;
    bool detailed = false;
    bool orientation_seen = false;
};
struct Orientation {
    std::uint32_t frame_pointer = 0, candidate = 0;
    unsigned phase = 0;
};
struct Contact {
    std::uint32_t return_pc = 0, caller_fp = 0, frame_pointer = 0, point = 0, output = 0;
    bool reply_seen = false;
    std::uint32_t last_ret_pc = 0, last_ret_pfp = 0, last_ret_rip = 0, last_ret_fp = 0;
    ReturnFrame returning;
};
struct Steering {
    std::uint32_t frame_pointer = 0, return_pc = 0, input = 0;
    bool detailed = false;
};
struct SteeringFilter {
    std::uint32_t frame_pointer=0;
    ReturnFrame returning;
    bool detailed=false;
};
struct MainPass {
    std::uint64_t id=0,instructions=0;
    std::uint32_t object=0,frame_pointer=0,position=0,orientation=0;
    ReturnFrame returning,query_return;
    Contact contact;
    std::uint32_t query_frame=0,query_point=0;
    unsigned queries=0,query_completions=0,contacts=0,contact_completions=0,responses=0,crosses=0;
    unsigned metrics=0,steps=0,normalizations=0,aggregate_phase=0;
    bool detailed=false,prepared=false,query_count=false,cross_pending=false;
    bool metric_pending=false,step_pending=false,normalization_pending=false,normalization_seen=false;
    bool response_pending=false;
};
struct VehicleHelper {
    ReturnFrame returning;
    std::uint32_t frame_pointer=0,vector=0,triple=0,direction=0,output=0,slot=0;
    unsigned phase=0;
};
struct Vehicle {
    std::uint64_t id=0,instructions=0;
    std::uint32_t object=0,frame_pointer=0;
    ReturnFrame returning;
    VehicleHelper slot,balance;
    unsigned prefix_phase=0,slots=0;
    bool detailed=false,exit_seen=false;
};
struct Capture {
    std::filesystem::path path;
    std::ofstream events, cadence, selectors, callbacks, steering, steering_filters,main_events,main_cadence;
    std::ofstream vehicle_events,vehicle_cadence;
    unsigned first = 0, last = 0, detailed = 0, events_count = 0, completed = 0;
    std::uint64_t calls = 0, last_count = ~std::uint64_t{}, selected_frame = ~std::uint64_t{};
    unsigned frame_samples = 0;
    std::size_t bytes = 0;
    std::uint64_t observed = 0, rom_entries = 0, ram_entries = 0;
    unsigned primary_calls = 0, alternate_calls = 0, selector_count = 0;
    unsigned callback_count = 0;
    std::uint32_t query_return = 0, query_caller_fp = 0;
    unsigned orientation_entries = 0, orientation_completions = 0;
    unsigned contact_entries = 0, contact_completions = 0;
    unsigned steering_entries = 0, steering_completions = 0, steering_samples = 0;
    std::uint64_t steering_selected_frame = ~std::uint64_t{};
    unsigned filter_entries=0,filter_completions=0,filter_samples=0;
    std::uint64_t filter_selected_frame=~std::uint64_t{};
    Orientation orientation;
    Contact contact;
    Steering steering_call;
    ReturnFrame outer_return,query_return_frame;
    SteeringFilter filter;
    MainPass main;
    unsigned main_calls=0,main_completions=0,main_samples=0,main_event_count=0;
    unsigned main_queries=0,main_query_completions=0,main_contacts=0,main_contact_completions=0;
    unsigned main_cross_pairs=0,main_sampled_cross_pairs=0;
    unsigned main_metric_entries=0,main_metric_completions=0,main_metric_sampled_pairs=0;
    unsigned main_step_entries=0,main_step_completions=0,main_step_sampled_pairs=0;
    unsigned main_aggregate_entries=0,main_aggregate_completions=0,main_aggregate_samples=0;
    unsigned main_normalization_entries=0,main_normalization_completions=0,main_normalization_sampled_pairs=0;
    unsigned main_response_entries=0,main_response_completions=0,main_response_sampled_pairs=0;
    std::uint64_t main_selected_frame=~std::uint64_t{};
    Vehicle vehicle;
    unsigned vehicle_calls=0,vehicle_completions=0,vehicle_samples=0,vehicle_event_count=0;
    unsigned vehicle_prefix_entries=0,vehicle_prefix_completions=0,vehicle_prefix_samples=0;
    unsigned vehicle_slot_entries=0,vehicle_slot_completions=0,vehicle_slot_samples=0;
    unsigned vehicle_balance_entries=0,vehicle_balance_completions=0,vehicle_balance_samples=0;
    unsigned vehicle_balance_calls=0,vehicle_balance_returns=0;
    unsigned vehicle_selection_entries=0,vehicle_selection_completions=0,vehicle_selection_samples=0;
    std::uint64_t vehicle_selected_frame=~std::uint64_t{};
    // Original and relocated forms of the bounded car-code analysis region.
    std::array<std::uint64_t, 2*0x8000> code_counts{};
    Call call;
};
std::unique_ptr<Capture> active;
std::uint32_t original_pc(std::uint32_t pc) {
    return pc >= relocation && pc < 0x600000 ? pc-relocation : pc;
}
bool ram(std::uint32_t address, std::size_t bytes) {
    const auto end = std::uint64_t(address) + bytes;
    return !(address & 3u) && ((address >= 0x200000 && end <= 0x240000) ||
                             (address >= 0x500000 && end <= 0x600000));
}
std::vector<std::uint32_t> words(const rt::Cpu &cpu, std::uint32_t address, unsigned count) {
    if (count > 256 || !ram(address, std::size_t(count)*4))
        throw std::runtime_error("driving observation requested non-RAM or unbounded memory");
    std::vector<std::uint32_t> result;
    result.reserve(count);
    for (unsigned i=0; i<count; ++i) result.push_back(cpu.bus->read_dword(address+4*i));
    return result;
}
template<class Values> void integers(std::ostream &out, const Values &values) {
    out << '['; bool first=true;
    for (auto value:values) { if (!first) out << ','; first=false; out << value; }
    out << ']';
}
ReturnFrame caller_return(const rt::Cpu &cpu) {
    ReturnFrame result;
    result.frame_pointer=cpu.m_r[0]&~0x3fu;
    if (!ram(result.frame_pointer,64)) throw std::runtime_error("saved caller frame is outside plain RAM");
    const auto index=std::int64_t(cpu.m_rcache_pos)-1;
    if (index>=0 && index<rt::Cpu::I960_RCACHE_SIZE) {
        result.cache_index=int(index);
        if (cpu.m_rcache_frame_addr[index]!=result.frame_pointer)
            throw std::runtime_error("saved caller register-cache frame mismatch");
        for (unsigned i=0;i<16;++i) result.words[i]=cpu.m_rcache[index][i];
    } else {
        const auto saved=words(cpu,result.frame_pointer,16);
        for (unsigned i=0;i<16;++i) result.words[i]=saved[i];
    }
    result.pc=result.words[2];
    if (!result.pc || result.pc&3u) throw std::runtime_error("saved caller has no aligned return address");
    return result;
}
void return_frame(std::ostream &out,const ReturnFrame &frame) {
    out << "{\"pc\":" << frame.pc << ",\"frame_pointer\":" << frame.frame_pointer
        << ",\"cache_index\":" << frame.cache_index << ",\"source\":\""
        << (frame.cache_index<0 ? "ordinary_ram" : "register_cache") << "\",\"words\":";
    integers(out,frame.words);out << '}';
}
void write(Capture &capture, std::ofstream &stream, const std::string &text) {
    if (capture.bytes + text.size() + 1 > byte_limit)
        throw std::runtime_error("driving capture byte limit exceeded");
    stream << text << '\n';
    if (!stream) throw std::runtime_error("cannot write driving capture");
    capture.bytes += text.size()+1;
}
bool same_return(const ReturnFrame &left,const ReturnFrame &right) {
    // Cache residency may change across interrupts; the saved caller identity may not.
    return left.pc==right.pc && left.frame_pointer==right.frame_pointer && left.words==right.words;
}
void vehicle_cadence(Capture &capture,const rt::Cpu &cpu,const char *event,
                     std::uint64_t instructions,std::uint64_t frame,std::uint64_t ns) {
    const auto &vehicle=capture.vehicle;
    std::ostringstream out;
    out << "{\"event\":\"" << event << "\",\"call_id\":" << vehicle.id
        << ",\"object\":" << vehicle.object << ",\"frame\":" << frame
        << ",\"instructions\":" << instructions << ",\"cycles\":" << cpu.reference_cycles
        << ",\"board_ns\":" << ns << ",\"pc\":" << cpu.m_IP
        << ",\"slot_pairs\":" << vehicle.slots << '}';
    write(capture,capture.vehicle_cadence,out.str());
}
void vehicle_snapshot(Capture &capture,const rt::Cpu &cpu,const char *event,
                      std::uint64_t instructions,std::uint64_t frame,std::uint64_t ns) {
    const auto &vehicle=capture.vehicle;
    if (!vehicle.detailed) return;
    if (++capture.vehicle_event_count>1024) throw std::runtime_error("main vehicle event limit exceeded");
    const std::string_view kind(event);
    const auto *helper=kind.starts_with("slot_") ? &vehicle.slot :
                       (kind.starts_with("balance_") || kind.starts_with("selection_")) ? &vehicle.balance : nullptr;
    std::ostringstream out;
    out << "{\"schema\":1,\"event\":\"" << event << "\",\"call_id\":" << vehicle.id
        << ",\"object\":" << vehicle.object << ",\"frame\":" << frame
        << ",\"instructions\":" << instructions << ",\"cycles\":" << cpu.reference_cycles
        << ",\"board_ns\":" << ns << ",\"pc\":" << cpu.m_IP << ",\"ac\":" << cpu.m_AC
        << ",\"vehicle_frame_pointer\":" << vehicle.frame_pointer << ",\"registers\":";
    integers(out,cpu.m_r);
    out << ",\"return_frame\":";return_frame(out,helper ? helper->returning : vehicle.returning);
    out << ",\"parent_return_frame\":";return_frame(out,vehicle.returning);
    if (helper) out << ",\"helper_frame_pointer\":" << helper->frame_pointer;
    if (kind.starts_with("slot_")) out << ",\"slot_index\":" << vehicle.slot.slot;
    std::array<std::uint64_t,4> fp{};
    for (unsigned i=0;i<4;++i) fp[i]=std::bit_cast<std::uint64_t>(cpu.m_fp[i]);
    out << ",\"fp_bits\":";integers(out,fp);
    out << ",\"object_words\":";integers(out,words(cpu,vehicle.object,64));
    out << ",\"blocks\":[";
    bool first=true;
    const auto block=[&](const char *name,std::uint32_t address,unsigned count) {
        if (!first) out << ',';first=false;
        out << "{\"name\":\"" << name << "\",\"address\":" << address << ",\"words\":";
        integers(out,words(cpu,address,count));out << '}';
    };
    block("main_object_pointer",0x2140cc,1);
    if (kind.starts_with("prefix_")) block("prefix_global",0x213974,1);
    if (kind.starts_with("prefix_")) {
        block("candidate_position",vehicle.frame_pointer+0x130,3);
        block("candidate_orientation",vehicle.frame_pointer+0x140,3);
    }
    if (kind.starts_with("slot_")) {
        block("slot_vector",vehicle.slot.vector,3);
        block("slot_triple",vehicle.slot.triple,3);
        block("slot_direction",vehicle.slot.direction,3);
        block("slot_output",vehicle.slot.output,3);
        block("slot_result",0x2138d0+36*vehicle.slot.slot,9);
        block("slot_globals",0x213880,16);
        block("slot_mask",0x214208,1);
        block("slot_parameters",0x2141d0,5);
        block("slot_control",0x214124,1);
    }
    if (kind.starts_with("balance_")) {
        block("balance_vector",vehicle.balance.vector,3);
        block("balance_output",vehicle.balance.output,3);
        block("balance_pairs",vehicle.object+0x64,4);
        block("balance_prior",0x21425c,1);
    }
    if (kind.starts_with("selection_")) {
        block("selection_flags",0x214208,1);
        block("selected_parameters",0x2141c0,13);
    }
    out << "]}";write(capture,capture.vehicle_events,out.str());
}
void observe_vehicle(Capture &capture,const rt::Cpu &cpu,std::uint64_t instructions,
                     std::uint64_t frame,std::uint64_t ns) {
    auto &vehicle=capture.vehicle;
    const auto pc=original_pc(cpu.m_IP);
    if (pc==0x34f40 && frame>=capture.first && frame<=capture.last) {
        if (vehicle.id) throw std::runtime_error("nested main vehicle callback");
        if (++capture.vehicle_calls>16384) throw std::runtime_error("main vehicle call limit exceeded");
        if (!ram(cpu.m_r[16],0x100) || !ram(cpu.m_r[31],0x1f4))
            throw std::runtime_error("main vehicle arguments require bounded ordinary RAM");
        const auto returning=caller_return(cpu);
        if (original_pc(returning.pc)!=0x2abac) throw std::runtime_error("main vehicle dispatcher return mismatch");
        vehicle={};vehicle.id=capture.vehicle_calls;vehicle.instructions=instructions;
        vehicle.object=cpu.m_r[16];vehicle.frame_pointer=cpu.m_r[31];vehicle.returning=returning;
        const bool selected=frame%60==0 || frame==2300 || frame==2500 || frame==2501 ||
                            frame==2600 || frame==2601 || frame==2700 || frame==2701 || frame==2752 ||
                            (frame>=2571 && frame<=2579);
        vehicle.detailed=selected && capture.vehicle_selected_frame!=frame && capture.vehicle_samples<32;
        if (vehicle.detailed) { ++capture.vehicle_samples;capture.vehicle_selected_frame=frame; }
        vehicle_cadence(capture,cpu,"vehicle_entry",instructions,frame,ns);
        vehicle_snapshot(capture,cpu,"vehicle_entry",instructions,frame,ns);return;
    }
    if (!vehicle.id) return;
    if (instructions-vehicle.instructions>2000000) throw std::runtime_error("main vehicle exceeded bounded instruction count");
    if (vehicle.slot.phase && cpu.m_IP==vehicle.slot.returning.pc) {
        if (vehicle.slot.phase!=2 || cpu.m_r[31]!=vehicle.frame_pointer)
            throw std::runtime_error("main vehicle slot return identity or completion mismatch");
        vehicle_snapshot(capture,cpu,"slot_post",instructions,frame,ns);
        ++vehicle.slots;++capture.vehicle_slot_completions;
        if (vehicle.detailed) ++capture.vehicle_slot_samples;
        vehicle.slot={};
    }
    if (vehicle.balance.phase && vehicle.balance.phase<7 && cpu.m_IP==vehicle.balance.returning.pc) {
        if (vehicle.balance.phase!=6 || cpu.m_r[31]!=vehicle.frame_pointer)
            throw std::runtime_error("main vehicle balance return identity or completion mismatch");
        vehicle_snapshot(capture,cpu,"balance_helper_post",instructions,frame,ns);
        vehicle.balance.phase=7;++capture.vehicle_balance_returns;
    }
    if (cpu.m_IP==vehicle.returning.pc) {
        if (cpu.m_r[31]!=vehicle.returning.frame_pointer || !vehicle.exit_seen || vehicle.prefix_phase!=2 ||
            vehicle.slots!=4 || vehicle.slot.phase || vehicle.balance.phase!=7)
            throw std::runtime_error("main vehicle return has incomplete or mismatched boundaries");
        vehicle_snapshot(capture,cpu,"vehicle_return",instructions,frame,ns);
        vehicle_cadence(capture,cpu,"vehicle_return",instructions,frame,ns);
        ++capture.vehicle_completions;vehicle={};return;
    }
    switch(pc) {
    case 0x34ff0:
    case 0x35218:
        if (cpu.m_r[31]!=vehicle.frame_pointer || cpu.m_r[pc==0x34ff0 ? 28 : 16]!=vehicle.object ||
            (cpu.m_AC&0xc0000000u) || !same_return(caller_return(cpu),vehicle.returning) ||
            vehicle.prefix_phase!=(pc==0x34ff0 ? 0u : 1u))
            throw std::runtime_error("main vehicle prefix identity or sequence mismatch");
        ++vehicle.prefix_phase;
        vehicle_snapshot(capture,cpu,pc==0x34ff0 ? "prefix_pre" : "prefix_post",instructions,frame,ns);
        if (pc==0x34ff0) ++capture.vehicle_prefix_entries;
        else { ++capture.vehicle_prefix_completions;if (vehicle.detailed) ++capture.vehicle_prefix_samples; }
        break;
    case 0x37200: {
        if (vehicle.slot.phase || vehicle.slots>=4 || cpu.m_r[22]!=vehicle.slots || cpu.m_r[16]!=vehicle.object ||
            cpu.m_r[30] || !ram(cpu.m_r[31],0x80) || !ram(cpu.m_r[17],12) || !ram(cpu.m_r[18],12) ||
            !ram(cpu.m_r[19],12) || !ram(cpu.m_r[21],12))
            throw std::runtime_error("main vehicle slot arguments or sequence mismatch");
        constexpr std::array<std::uint32_t,4> returns{0x35b88,0x35bb4,0x35bf0,0x35c28};
        const auto returning=caller_return(cpu);
        if (returning.frame_pointer!=vehicle.frame_pointer || original_pc(returning.pc)!=returns[vehicle.slots])
            throw std::runtime_error("main vehicle slot caller mismatch");
        vehicle.slot={returning,cpu.m_r[31],cpu.m_r[17],cpu.m_r[18],cpu.m_r[19],cpu.m_r[21],vehicle.slots,1};
        ++capture.vehicle_slot_entries;vehicle_snapshot(capture,cpu,"slot_pre",instructions,frame,ns);break;
    }
    case 0x37360:
    case 0x37674:
        if (vehicle.slot.phase!=1 || cpu.m_r[31]!=vehicle.slot.frame_pointer ||
            !same_return(caller_return(cpu),vehicle.slot.returning))
            throw std::runtime_error("main vehicle slot exit identity mismatch");
        vehicle.slot.phase=2;break;
    case 0x37870: {
        if (vehicle.balance.phase || cpu.m_r[16]!=vehicle.object || !ram(cpu.m_r[17],12) ||
            !ram(cpu.m_r[18],12) || !ram(cpu.m_r[31],0x80))
            throw std::runtime_error("main vehicle balance arguments or sequence mismatch");
        const auto returning=caller_return(cpu);
        if (returning.frame_pointer!=vehicle.frame_pointer || original_pc(returning.pc)!=0x36760)
            throw std::runtime_error("main vehicle balance caller mismatch");
        vehicle.balance={returning,cpu.m_r[31],cpu.m_r[17],0,0,cpu.m_r[18],0,1};
        ++capture.vehicle_balance_calls;vehicle_snapshot(capture,cpu,"balance_helper_pre",instructions,frame,ns);break;
    }
    case 0x3789c:
    case 0x378f4:
        if (vehicle.balance.phase!=(pc==0x3789c ? 1u : 2u) || cpu.m_r[31]!=vehicle.balance.frame_pointer ||
            cpu.m_r[9]!=vehicle.object || !same_return(caller_return(cpu),vehicle.balance.returning))
            throw std::runtime_error("main vehicle selection identity or sequence mismatch");
        ++vehicle.balance.phase;
        vehicle_snapshot(capture,cpu,pc==0x3789c ? "selection_pre" : "selection_post",instructions,frame,ns);
        if (pc==0x3789c) ++capture.vehicle_selection_entries;
        else { ++capture.vehicle_selection_completions;if (vehicle.detailed) ++capture.vehicle_selection_samples; }
        break;
    case 0x37900:
    case 0x379d0:
        if (vehicle.balance.phase!=(pc==0x37900 ? 3u : 4u) || cpu.m_r[31]!=vehicle.balance.frame_pointer ||
            cpu.m_r[9]!=vehicle.object || !same_return(caller_return(cpu),vehicle.balance.returning))
            throw std::runtime_error("main vehicle balance identity or sequence mismatch");
        ++vehicle.balance.phase;
        vehicle_snapshot(capture,cpu,pc==0x37900 ? "balance_pre" : "balance_post",instructions,frame,ns);
        if (pc==0x37900) ++capture.vehicle_balance_entries;
        else { ++capture.vehicle_balance_completions;if (vehicle.detailed) ++capture.vehicle_balance_samples; }
        break;
    case 0x38670:
        if (vehicle.balance.phase!=5 || cpu.m_r[31]!=vehicle.balance.frame_pointer ||
            !same_return(caller_return(cpu),vehicle.balance.returning))
            throw std::runtime_error("main vehicle balance exit identity mismatch");
        vehicle.balance.phase=6;break;
    case 0x371f0:
        if (vehicle.exit_seen || cpu.m_r[31]!=vehicle.frame_pointer ||
            !same_return(caller_return(cpu),vehicle.returning))
            throw std::runtime_error("main vehicle exit identity mismatch");
        vehicle.exit_seen=true;break;
    default:break;
    }
}
void cadence(Capture &capture, const rt::Cpu &cpu, const char *event,
             std::uint64_t instructions, std::uint64_t frame, std::uint64_t ns) {
    std::ostringstream out;
    out << "{\"event\":\"" << event << "\",\"call_id\":" << capture.call.id
        << ",\"entry_pc\":" << capture.call.entry_pc
        << ",\"branch_selector\":" << capture.call.branch_selector
        << ",\"object\":" << capture.call.object << ",\"frame\":" << frame
        << ",\"instructions\":" << instructions << ",\"cycles\":" << cpu.reference_cycles
        << ",\"board_ns\":" << ns << ",\"pc\":" << cpu.m_IP << '}';
    write(capture,capture.cadence,out.str());
}
void steering_snapshot(Capture &capture, const rt::Cpu &cpu, const char *event,
                       std::uint64_t instructions, std::uint64_t frame, std::uint64_t ns) {
    if (!capture.steering_call.detailed) return;
    std::ostringstream out;
    out << "{\"schema\":1,\"event\":\"" << event << "\",\"pair_id\":" << capture.steering_entries
        << ",\"frame\":" << frame << ",\"instructions\":" << instructions
        << ",\"cycles\":" << cpu.reference_cycles << ",\"board_ns\":" << ns
        << ",\"pc\":" << cpu.m_IP << ",\"ac\":" << cpu.m_AC
        << ",\"input_address\":2105424,\"input_byte\":" << capture.steering_call.input
        << ",\"return_pc\":" << capture.steering_call.return_pc
        << ",\"frame_pointer\":" << capture.steering_call.frame_pointer << ",\"registers\":";
    integers(out,cpu.m_r);out << '}';
    write(capture,capture.steering,out.str());
}
void filter_snapshot(Capture &capture,const rt::Cpu &cpu,const char *event,
                     std::uint64_t instructions,std::uint64_t frame,std::uint64_t ns) {
    if (!capture.filter.detailed) return;
    std::ostringstream out;
    out << "{\"schema\":1,\"event\":\"" << event << "\",\"pair_id\":" << capture.filter_entries
        << ",\"frame\":" << frame << ",\"instructions\":" << instructions
        << ",\"cycles\":" << cpu.reference_cycles << ",\"board_ns\":" << ns
        << ",\"pc\":" << cpu.m_IP << ",\"ac\":" << cpu.m_AC
        << ",\"frame_pointer\":" << capture.filter.frame_pointer << ",\"return_frame\":";
    return_frame(out,capture.filter.returning);
    out << ",\"registers\":";integers(out,cpu.m_r);
    out << ",\"blocks\":[{\"name\":\"steering_history\",\"address\":" << 0x2142c0
        << ",\"words\":[" << cpu.bus->read_dword(0x2142c0)
        << "]},{\"name\":\"steering_input\",\"address\":" << 0x202050
        << ",\"words\":[" << cpu.bus->read_dword(0x202050) << "]}]}";
    write(capture,capture.steering_filters,out.str());
}
void main_cadence(Capture &capture,const rt::Cpu &cpu,const char *event,
                  std::uint64_t instructions,std::uint64_t frame,std::uint64_t ns) {
    const auto &main=capture.main;
    std::ostringstream out;
    out << "{\"event\":\"" << event << "\",\"call_id\":" << main.id
        << ",\"object\":" << main.object << ",\"frame\":" << frame
        << ",\"instructions\":" << instructions << ",\"cycles\":" << cpu.reference_cycles
        << ",\"board_ns\":" << ns << ",\"pc\":" << cpu.m_IP
        << ",\"cross_pairs\":" << main.crosses << '}';
    write(capture,capture.main_cadence,out.str());
}
void main_snapshot(Capture &capture,const rt::Cpu &cpu,const char *event,
                   std::uint64_t instructions,std::uint64_t frame,std::uint64_t ns) {
    const auto &main=capture.main;
    if (!main.detailed) return;
    if (++capture.main_event_count>4096) throw std::runtime_error("main contact event limit exceeded");
    const std::string_view kind(event);
    std::ostringstream out;
    out << "{\"schema\":1,\"event\":\"" << event << "\",\"call_id\":" << main.id
        << ",\"object\":" << main.object << ",\"frame\":" << frame
        << ",\"instructions\":" << instructions << ",\"cycles\":" << cpu.reference_cycles
        << ",\"board_ns\":" << ns << ",\"pc\":" << cpu.m_IP << ",\"ac\":" << cpu.m_AC
        << ",\"main_frame_pointer\":" << main.frame_pointer << ",\"registers\":";
    integers(out,cpu.m_r);
    out << ",\"return_frame\":";
    return_frame(out,kind.starts_with("query_") ? main.query_return :
                     kind.starts_with("contact_") ? main.contact.returning : main.returning);
    out << ",\"parent_return_frame\":";return_frame(out,main.returning);
    if (kind.starts_with("query_")) out << ",\"query_index\":" << main.queries-1;
    if (kind.starts_with("contact_")) out << ",\"contact_index\":" << main.contacts-1;
    if (kind=="response_pre" || kind=="response_post" || kind.starts_with("normalization_"))
        out << ",\"contact_index\":" << main.responses;
    if (kind.starts_with("cross_")) out << ",\"cross_index\":" << main.crosses;
    if (kind=="cross_post" || kind=="metric_post") out << ",\"metric_index\":" << main.metrics;
    if (kind=="metric_post" || kind=="step_post") out << ",\"step_index\":" << main.steps;
    std::array<std::uint64_t,4> fp{};
    for (unsigned i=0;i<4;++i) fp[i]=std::bit_cast<std::uint64_t>(cpu.m_fp[i]);
    out << ",\"fp_bits\":";integers(out,fp);
    out << ",\"object_words\":";integers(out,words(cpu,main.object,256));
    out << ",\"blocks\":[";
    bool first=true;
    const auto block=[&](const char *name,std::uint32_t address,unsigned count) {
        if (!first) out << ',';first=false;
        out << "{\"name\":\"" << name << "\",\"address\":" << address << ",\"words\":";
        integers(out,words(cpu,address,count));out << '}';
    };
    block("candidate_position",main.position,3);
    block("candidate_orientation",main.orientation,3);
    block("query_stage_global",0x20a280,16);
    block("query_descriptors",0x5dcac0,24);
    block("course_selector",0x214354,1);
    block("object_pointer_globals",0x2140c0,16);
    block("result_slots",0x2138d0,36);
    block("feedback",0x213960,3);
    block("feedback_enable",0x2142c8,1);
    if (kind=="main_entry" || kind=="main_prequery")
        block("local_contact_points",0x5d2e30,12);
    if (main.prepared) {
        block("transformed_offsets",main.frame_pointer+0x70,12);
        block("query_points",main.frame_pointer+0x40,12);
        block("response_vectors",main.frame_pointer+0xa0,12);
    }
    if (kind.starts_with("query_")) block("query_point",main.query_point,3);
    if (kind.starts_with("contact_")) {
        block("contact_point",main.contact.point,3);
        block("contact_output",main.contact.output,8);
        if (kind=="contact_reply") {
            block("contact_reply_prefix",main.contact.frame_pointer+0x60,12);
            block("contact_reply_tail",main.contact.frame_pointer+0x50,4);
        }
    }
    if (kind.starts_with("cross_")) {
        block("cross_left",main.frame_pointer+0x70+12*main.crosses,3);
        block("cross_right",main.frame_pointer+0xa0+12*main.crosses,3);
        block("cross_result",main.frame_pointer+0xd0,3);
    }
    if (kind=="cross_post" || kind=="metric_post" || kind=="step_post")
        block("metric_cross",main.frame_pointer+0xd0,3);
    if (kind.starts_with("normalization_"))
        block("normalization_vector",main.frame_pointer+0xd0,3);
    out << "]}";write(capture,capture.main_events,out.str());
}
void observe_main(Capture &capture,const rt::Cpu &cpu,std::uint64_t instructions,
                  std::uint64_t frame,std::uint64_t ns) {
    auto &main=capture.main;
    const auto pc=original_pc(cpu.m_IP);
    if (pc==0x34380 && frame>=capture.first && frame<=capture.last) {
        if (main.id) throw std::runtime_error("nested main contact pass");
        if (++capture.main_calls>16384) throw std::runtime_error("main contact call limit exceeded");
        if (!ram(cpu.m_r[16],1024) || !ram(cpu.m_r[17],12) || !ram(cpu.m_r[18],12) ||
            !ram(cpu.m_r[31],0x114))
            throw std::runtime_error("main contact arguments require bounded ordinary RAM");
        const auto returning=caller_return(cpu);
        if (original_pc(returning.pc)!=0x3521c) throw std::runtime_error("main contact caller return mismatch");
        main={};main.id=capture.main_calls;main.instructions=instructions;
        main.object=cpu.m_r[16];main.position=cpu.m_r[17];main.orientation=cpu.m_r[18];
        main.frame_pointer=cpu.m_r[31];main.returning=returning;
        // Capture008 cadence located the aggregate path in these nine replay frames.
        // Keep the observed window explicit rather than infer physical contact states.
        const bool selected=frame%60==0 || frame==2300 || frame==2500 || frame==2501 ||
                            frame==2600 || frame==2601 || frame==2700 || frame==2701 || frame==2752 ||
                            (frame>=2571 && frame<=2579);
        main.detailed=selected && capture.main_selected_frame!=frame && capture.main_samples<32;
        if (main.detailed) { ++capture.main_samples;capture.main_selected_frame=frame; }
        main_cadence(capture,cpu,"main_entry",instructions,frame,ns);
        main_snapshot(capture,cpu,"main_entry",instructions,frame,ns);
        return;
    }
    if (!main.id) return;
    if (instructions-main.instructions>2000000) throw std::runtime_error("main contact exceeded bounded instruction count");
    if (main.contact.return_pc && cpu.m_IP==main.contact.return_pc) {
        if (cpu.m_r[31]!=main.contact.caller_fp || !main.contact.reply_seen)
            throw std::runtime_error("main contact result return frame or reply mismatch");
        main_snapshot(capture,cpu,"contact_post",instructions,frame,ns);
        ++main.contact_completions;++capture.main_contact_completions;main.contact={};
    }
    if (main.query_return.pc && cpu.m_IP==main.query_return.pc) {
        if (cpu.m_r[31]!=main.query_return.frame_pointer || !main.query_count)
            throw std::runtime_error("main query return frame or count mismatch");
        main_snapshot(capture,cpu,"query_post",instructions,frame,ns);
        ++main.query_completions;++capture.main_query_completions;main.query_return={};
    }
    if (cpu.m_IP==main.returning.pc) {
        if (cpu.m_r[31]!=main.returning.frame_pointer || !main.prepared || main.query_return.pc ||
            main.contact.return_pc || main.cross_pending || main.queries!=4 || main.query_completions!=4 ||
            main.contacts!=4 || main.contact_completions!=4 || main.responses!=4 || (main.crosses!=0 && main.crosses!=4) ||
            main.metric_pending || main.step_pending || main.normalization_pending || main.response_pending ||
            main.metrics!=main.crosses || main.steps!=main.crosses || main.aggregate_phase!=(main.crosses ? 2u : 0u))
            throw std::runtime_error("main contact return has incomplete or mismatched boundaries");
        main_snapshot(capture,cpu,"main_return",instructions,frame,ns);
        main_cadence(capture,cpu,"main_return",instructions,frame,ns);
        ++capture.main_completions;main={};return;
    }
    switch(pc) {
    case 0x346d8:
        if (main.prepared || cpu.m_r[31]!=main.frame_pointer || cpu.m_r[13]!=main.object)
            throw std::runtime_error("main prequery frame or sequence mismatch");
        main.prepared=true;main_snapshot(capture,cpu,"main_prequery",instructions,frame,ns);break;
    case 0x2abc0: {
        if (!main.prepared || main.query_return.pc || main.contact.return_pc || main.queries>=4 ||
            main.queries!=main.contacts || cpu.m_r[18]!=main.frame_pointer+0x40+12*main.queries || cpu.m_r[19]!=0)
            throw std::runtime_error("main query sequence or point mismatch");
        const auto returning=caller_return(cpu);
        if (returning.frame_pointer!=main.frame_pointer ||
            original_pc(returning.pc)!=(main.queries ? 0x34728u : 0x346dcu))
            throw std::runtime_error("main query saved caller mismatch");
        main.query_return=returning;main.query_frame=cpu.m_r[31];main.query_point=cpu.m_r[18];main.query_count=false;
        ++main.queries;++capture.main_queries;main_snapshot(capture,cpu,"query_pre",instructions,frame,ns);break;
    }
    case 0x2ac08:
        if (!main.query_return.pc || main.query_count || cpu.m_r[31]!=main.query_frame)
            throw std::runtime_error("main query count frame or sequence mismatch");
        main.query_count=true;main_snapshot(capture,cpu,"query_count",instructions,frame,ns);break;
    case 0x2acc0: {
        if (main.contact.return_pc || main.query_return.pc || main.contacts>=4 ||
            main.query_completions!=main.contacts+1 || main.responses!=main.contacts ||
            cpu.m_r[16]!=main.frame_pointer+0x40+12*main.contacts ||
            cpu.m_r[17]!=0x2138d0+36*main.contacts || !ram(cpu.m_r[31],0x90))
            throw std::runtime_error("main contact result sequence or argument mismatch");
        const auto returning=caller_return(cpu);
        if (returning.frame_pointer!=main.frame_pointer || original_pc(returning.pc)!=0x34710)
            throw std::runtime_error("main contact result saved caller mismatch");
        main.contact={};main.contact.returning=returning;main.contact.return_pc=returning.pc;
        main.contact.caller_fp=returning.frame_pointer;main.contact.frame_pointer=cpu.m_r[31];
        main.contact.point=cpu.m_r[16];main.contact.output=cpu.m_r[17];
        ++main.contacts;++capture.main_contacts;main_snapshot(capture,cpu,"contact_pre",instructions,frame,ns);break;
    }
    case 0x2adac:
        if (!main.contact.return_pc || main.contact.reply_seen || cpu.m_r[31]!=main.contact.frame_pointer)
            throw std::runtime_error("main contact reply frame or sequence mismatch");
        main.contact.reply_seen=true;main_snapshot(capture,cpu,"contact_reply",instructions,frame,ns);break;
    case 0x3472c: {
        const auto index=main.responses;
        if (!main.prepared || main.response_pending || index>=4 ||
            main.contact.return_pc || main.query_return.pc || main.contact_completions!=index+1 ||
            main.query_completions!=(index<3 ? index+2 : 4) ||
            cpu.m_r[31]!=main.frame_pointer || cpu.m_r[13]!=main.object ||
            cpu.m_r[3]!=0x2138ecu+36*index || cpu.m_r[9]!=0x2138d0u+36*index ||
            cpu.m_r[10]!=0x2138e8u+36*index || cpu.m_r[5]!=index ||
            cpu.m_r[6]!=12*index || cpu.m_r[15]!=36*index ||
            cpu.m_r[7]!=main.frame_pointer+0xd0 || cpu.m_r[14]!=main.frame_pointer+0xe0 ||
            cpu.m_r[25]!=main.frame_pointer+0xd8 || cpu.m_r[30]!=0 || cpu.m_r[20]>255)
            throw std::runtime_error("main response input frame, pointer or sequence mismatch");
        const auto returning=caller_return(cpu);
        if (returning.pc!=main.returning.pc || returning.frame_pointer!=main.returning.frame_pointer)
            throw std::runtime_error("main response input saved parent mismatch");
        main_snapshot(capture,cpu,"response_pre",instructions,frame,ns);
        main.response_pending=true;++capture.main_response_entries;
        break;
    }
    case 0x34a30:
        if (cpu.m_r[31]!=main.frame_pointer || cpu.m_r[13]!=main.object || main.responses>=4 ||
            cpu.m_r[5]!=main.responses || main.contact_completions!=main.responses+1 ||
            !main.response_pending || main.normalization_pending)
            throw std::runtime_error("main response frame or sequence mismatch");
        if (const auto returning=caller_return(cpu); returning.pc!=main.returning.pc ||
            returning.frame_pointer!=main.returning.frame_pointer)
            throw std::runtime_error("main response output saved parent mismatch");
        main_snapshot(capture,cpu,"response_post",instructions,frame,ns);
        main.response_pending=false;++main.responses;main.normalization_seen=false;
        ++capture.main_response_completions;
        if (main.detailed) ++capture.main_response_sampled_pairs;
        break;
    case 0x348ec:
    case 0x34968:
        if (cpu.m_r[31]!=main.frame_pointer || cpu.m_r[13]!=main.object || main.responses>=4 ||
            cpu.m_r[5]!=main.responses || main.contact_completions!=main.responses+1 || !main.response_pending ||
            cpu.m_r[7]!=main.frame_pointer+0xd0 || cpu.m_r[25]!=main.frame_pointer+0xd8 ||
            (pc==0x348ec ? main.normalization_seen : !main.normalization_pending))
            throw std::runtime_error("main normalization frame, pointer or sequence mismatch");
        if (const auto returning=caller_return(cpu); returning.pc!=main.returning.pc ||
            returning.frame_pointer!=main.returning.frame_pointer)
            throw std::runtime_error("main normalization saved parent mismatch");
        main_snapshot(capture,cpu,pc==0x348ec ? "normalization_pre" : "normalization_post",instructions,frame,ns);
        main.normalization_pending=pc==0x348ec;
        if (pc==0x348ec) { main.normalization_seen=true;++capture.main_normalization_entries; }
        else {
            ++main.normalizations;++capture.main_normalization_completions;
            if (main.detailed) ++capture.main_normalization_sampled_pairs;
        }
        break;
    case 0x34a88:
    case 0x34b04:
        if (cpu.m_r[31]!=main.frame_pointer || cpu.m_r[13]!=main.object || main.responses!=4 ||
            main.crosses>=4 || cpu.m_r[23]!=12*main.crosses || main.metrics!=main.crosses ||
            main.steps!=main.crosses || main.metric_pending || main.step_pending || main.aggregate_phase ||
            (pc==0x34a88 ? (main.cross_pending || cpu.m_r[21]!=main.frame_pointer+0x40+12*main.crosses) : !main.cross_pending))
            throw std::runtime_error("main cross frame, operand or sequence mismatch");
        if (pc==0x34b04 && cpu.m_r[30]!=0)
            throw std::runtime_error("main metric requires the observed zero scratch operand");
        if (const auto returning=caller_return(cpu); returning.pc!=main.returning.pc ||
            returning.frame_pointer!=main.returning.frame_pointer)
            throw std::runtime_error("main cross/metric saved parent mismatch");
        main_snapshot(capture,cpu,pc==0x34a88 ? "cross_pre" : "cross_post",instructions,frame,ns);
        main.cross_pending=pc==0x34a88;
        if (pc==0x34b04) {
            ++main.crosses;++capture.main_cross_pairs;
            main.metric_pending=true;++capture.main_metric_entries;
            if (main.detailed) ++capture.main_sampled_cross_pairs;
        }
        break;
    case 0x34b5c:
    case 0x34b84: {
        const auto index=pc==0x34b5c ? main.metrics : main.steps;
        if (cpu.m_r[31]!=main.frame_pointer || cpu.m_r[13]!=main.object || index>=4 ||
            cpu.m_r[23]!=12*index || cpu.m_r[30]!=0 ||
            (pc==0x34b5c ? (!main.metric_pending || main.step_pending || main.crosses!=index+1 || main.steps!=index) :
                            (!main.step_pending || main.metric_pending || main.metrics!=index+1)))
            throw std::runtime_error("main metric/step frame, operand or sequence mismatch");
        const auto returning=caller_return(cpu);
        if (returning.pc!=main.returning.pc || returning.frame_pointer!=main.returning.frame_pointer)
            throw std::runtime_error("main metric/step saved parent mismatch");
        main_snapshot(capture,cpu,pc==0x34b5c ? "metric_post" : "step_post",instructions,frame,ns);
        if (pc==0x34b5c) {
            main.metric_pending=false;main.step_pending=true;++main.metrics;
            ++capture.main_metric_completions;++capture.main_step_entries;
            if (main.detailed) ++capture.main_metric_sampled_pairs;
        } else {
            main.step_pending=false;++main.steps;++capture.main_step_completions;
            if (main.detailed) ++capture.main_step_sampled_pairs;
        }
        break;
    }
    case 0x34b90:
        if (main.aggregate_phase || main.steps!=4 || main.metrics!=4 || main.crosses!=4 ||
            main.metric_pending || main.step_pending || cpu.m_r[31]!=main.frame_pointer || cpu.m_r[13]!=main.object)
            throw std::runtime_error("main aggregate input frame or sequence mismatch");
        if (const auto returning=caller_return(cpu); returning.pc!=main.returning.pc ||
            returning.frame_pointer!=main.returning.frame_pointer)
            throw std::runtime_error("main aggregate input saved parent mismatch");
        main.aggregate_phase=1;++capture.main_aggregate_entries;
        main_snapshot(capture,cpu,"aggregate_pre",instructions,frame,ns);break;
    case 0x34cc0:
        // The inactive aggregate path also reaches this PC. It has no paired input.
        if (!main.aggregate_phase) {
            if (main.crosses) throw std::runtime_error("main aggregate output lacks its paired input");
            break;
        }
        if (main.aggregate_phase!=1 || cpu.m_r[31]!=main.frame_pointer || cpu.m_r[13]!=main.object)
            throw std::runtime_error("main aggregate output frame or sequence mismatch");
        if (const auto returning=caller_return(cpu); returning.pc!=main.returning.pc ||
            returning.frame_pointer!=main.returning.frame_pointer)
            throw std::runtime_error("main aggregate output saved parent mismatch");
        main_snapshot(capture,cpu,"aggregate_post",instructions,frame,ns);
        main.aggregate_phase=2;++capture.main_aggregate_completions;
        if (main.detailed) ++capture.main_aggregate_samples;
        break;
    default:break;
    }
}
[[noreturn]] void contact_failure(Capture &capture,const rt::Cpu &cpu,const char *reason,
                                  std::uint64_t instructions,std::uint64_t frame) {
    const auto &contact=capture.contact;
    std::ofstream report(capture.path/"contact-failure.json");
    report << "{\"complete\":false,\"reason\":\"" << reason << "\",\"frame\":" << frame
           << ",\"instructions\":" << instructions << ",\"call_id\":" << capture.call.id
           << ",\"object\":" << capture.call.object << ",\"pc\":" << cpu.m_IP
           << ",\"fp\":" << cpu.m_r[31] << ",\"pfp\":" << cpu.m_r[0] << ",\"rip\":" << cpu.m_r[2]
           << ",\"saved_return\":" << contact.return_pc << ",\"saved_caller_fp\":" << contact.caller_fp
           << ",\"saved_helper_fp\":" << contact.frame_pointer << ",\"reply_seen\":" << (contact.reply_seen ? "true" : "false")
           << ",\"last_ret_pc\":" << contact.last_ret_pc << ",\"last_ret_pfp\":" << contact.last_ret_pfp
           << ",\"last_ret_rip\":" << contact.last_ret_rip << ",\"last_ret_fp\":" << contact.last_ret_fp << "}\n";
    report.close();
    if (!report) throw std::runtime_error("cannot save contact pairing failure");
    throw std::runtime_error(reason);
}
void snapshot(Capture &capture, const rt::Cpu &cpu, const char *event,
              std::uint64_t instructions, std::uint64_t frame, std::uint64_t ns) {
    if (!capture.call.detailed) return;
    if (++capture.events_count > 4096) throw std::runtime_error("driving capture event limit exceeded");
    std::ostringstream out;
    out << "{\"schema\":1,\"event\":\"" << event << "\",\"call_id\":" << capture.call.id
        << ",\"entry_pc\":" << capture.call.entry_pc
        << ",\"branch_selector\":" << capture.call.branch_selector
        << ",\"object\":" << capture.call.object << ",\"frame\":" << frame
        << ",\"instructions\":" << instructions << ",\"cycles\":" << cpu.reference_cycles
        << ",\"board_ns\":" << ns << ",\"pc\":" << cpu.m_IP << ",\"ac\":" << cpu.m_AC
        << ",\"registers\":";
    integers(out,cpu.m_r);
    out << ",\"return_frame\":";
    const std::string_view kind(event);
    return_frame(out,kind.starts_with("contact_") ? capture.contact.returning :
                     kind.starts_with("query_") ? capture.query_return_frame : capture.outer_return);
    std::array<std::uint64_t,4> fp{};
    for (unsigned i=0;i<4;++i) fp[i]=std::bit_cast<std::uint64_t>(cpu.m_fp[i]);
    out << ",\"fp_bits\":"; integers(out,fp);
    out << ",\"object_words\":"; integers(out,words(cpu,capture.call.object,256));
    out << ",\"blocks\":[";
    bool first=true;
    const auto block=[&](const char *name,std::uint32_t address,unsigned count) {
        if (!ram(address,std::size_t(count)*4)) return;
        if (!first) out << ','; first=false;
        out << "{\"name\":\"" << name << "\",\"address\":" << address << ",\"words\":";
        integers(out,words(cpu,address,count)); out << '}';
    };
    block("globals",0x214100,192);
    block("velocity_globals",0x20ca00,64);
    block("query_stage_global",0x20a280,16);
    block("query_descriptors",0x5dcac0,24);
    block("tuning",0x5cabe0,8);
    if (std::string_view(event)=="integrator_entry") {
        block("object_pointer_globals",0x2140c0,16);
        block("baseline_object",0x500000,64);
    }
    block("descriptor_prefix",capture.call.descriptor,32);
    const auto index=cpu.bus->read_dword(capture.call.object+0xe4);
    const auto selected=std::uint64_t(capture.call.descriptor)+16*std::uint64_t(index);
    if (selected<=0xffffffffu) block("selected_descriptor",std::uint32_t(selected),4);
    const auto pc=original_pc(cpu.m_IP);
    if (pc==0x2abc0) block("query_point",cpu.m_r[18],3);
    if (pc==0x3110c || pc==0x31154 || pc==0x31158) block("blend_output",cpu.m_r[17],4);
    if (std::string_view(event).starts_with("alternate_orientation_"))
        block("orientation_candidate",capture.orientation.candidate,4);
    if (std::string_view(event).starts_with("contact_")) {
        block("contact_point",capture.contact.point,3);
        block("contact_output",capture.contact.output,8);
        if (pc==0x2adac) {
            block("contact_reply_prefix",capture.contact.frame_pointer+0x60,12);
            block("contact_reply_tail",capture.contact.frame_pointer+0x50,4);
        }
    }
    const auto staging=cpu.bus->read_dword(0x20a290);
    block("query_staging",staging,32);
    out << "]}";
    write(capture,capture.events,out.str());
}
} // namespace

void begin_driving_capture(const char *directory,unsigned first,unsigned last) {
    if (active || !directory || !*directory || first>last || last>10000 || last-first>1200)
        throw std::runtime_error("invalid bounded driving capture interval");
    namespace fs=std::filesystem;
    const auto root=fs::canonical(__FILE__).parent_path().parent_path().parent_path().parent_path();
    if (fs::is_symlink(root/"build")) throw std::runtime_error("private build root is a symlink");
    const auto build=fs::canonical(root/"build");
    const auto path=fs::weakly_canonical(fs::absolute(directory));
    auto b=build.begin(),p=path.begin();
    for (;b!=build.end()&&p!=path.end()&&*b==*p;++b,++p) {}
    if (b!=build.end()||p==path.end()||fs::exists(path))
        throw std::runtime_error("driving capture requires a new directory below ignored build/");
    fs::create_directories(path);
    auto capture=std::make_unique<Capture>();
    capture->path=path;capture->first=first;capture->last=last;
    capture->events.open(path/"events.jsonl.tmp");capture->cadence.open(path/"cadence.jsonl.tmp");
    capture->selectors.open(path/"selectors.jsonl.tmp");
    capture->callbacks.open(path/"callbacks.jsonl.tmp");
    capture->steering.open(path/"steering.jsonl.tmp");
    capture->steering_filters.open(path/"steering-filter.jsonl.tmp");
    capture->main_events.open(path/"main-contact.jsonl.tmp");
    capture->main_cadence.open(path/"main-contact-cadence.jsonl.tmp");
    capture->vehicle_events.open(path/"main-vehicle.jsonl.tmp");
    capture->vehicle_cadence.open(path/"main-vehicle-cadence.jsonl.tmp");
    if (!capture->events||!capture->cadence||!capture->selectors||!capture->callbacks||!capture->steering||!capture->steering_filters||
        !capture->main_events||!capture->main_cadence||!capture->vehicle_events||!capture->vehicle_cadence)
        throw std::runtime_error("cannot open driving capture");
    active=std::move(capture);
}

void observe_driving_instruction(const rt::Cpu &cpu,std::uint64_t instructions,
                                 std::uint64_t frame,std::uint64_t ns) {
    if (!active) return;
    auto &capture=*active;
    if (capture.last_count==instructions) return;
    capture.last_count=instructions;
    ++capture.observed;
    if (cpu.m_IP==0x2cce0) ++capture.rom_entries;
    if (cpu.m_IP==relocation+0x2cce0) ++capture.ram_entries;
    const auto pc=original_pc(cpu.m_IP);
    observe_vehicle(capture,cpu,instructions,frame,ns);
    observe_main(capture,cpu,instructions,frame,ns);
    if (capture.contact.return_pc && (pc==0x2ae68 || pc==0x2aeb4 || pc==0x2aec0)) {
        capture.contact.last_ret_pc=cpu.m_IP;capture.contact.last_ret_pfp=cpu.m_r[0];
        capture.contact.last_ret_rip=cpu.m_r[2];capture.contact.last_ret_fp=cpu.m_r[31];
    }
    if (capture.contact.return_pc && cpu.m_IP==capture.contact.return_pc && cpu.m_r[31]!=capture.contact.caller_fp)
        contact_failure(capture,cpu,"contact helper reached return address in a different frame",instructions,frame);
    if (capture.contact.return_pc && cpu.m_IP==capture.contact.return_pc && cpu.m_r[31]==capture.contact.caller_fp) {
        if (!capture.contact.reply_seen) throw std::runtime_error("contact helper returned without its reply observation");
        snapshot(capture,cpu,"contact_post",instructions,frame,ns);
        ++capture.contact_completions;capture.contact={};
    }
    if (capture.query_return && cpu.m_IP==capture.query_return && cpu.m_r[31]==capture.query_caller_fp) {
        snapshot(capture,cpu,"query_post",instructions,frame,ns);
        capture.query_return=0;
    }
    if (capture.call.id && cpu.m_IP==capture.call.return_pc && cpu.m_r[31]==capture.call.caller_fp) {
        if (capture.query_return) throw std::runtime_error("integrator returned with an unpaired query");
        if (capture.orientation.phase) throw std::runtime_error("integrator returned with incomplete alternate orientation");
        if (capture.contact.return_pc)
            contact_failure(capture,cpu,"integrator returned with an unpaired contact helper",instructions,frame);
        cadence(capture,cpu,"integrator_return",instructions,frame,ns);
        snapshot(capture,cpu,"integrator_return",instructions,frame,ns);
        ++capture.completed;capture.call={};
    }
    if (pc==0x39a90 && frame>=capture.first && frame<=capture.last) {
        if (capture.filter.returning.pc) throw std::runtime_error("nested steering filter");
        if (++capture.filter_entries>16384) throw std::runtime_error("steering filter call limit exceeded");
        const bool detailed=(frame%20==0 || frame==2750) && capture.filter_selected_frame!=frame && capture.filter_samples<32;
        capture.filter={cpu.m_r[31],caller_return(cpu),detailed};
        if (detailed) { ++capture.filter_samples;capture.filter_selected_frame=frame; }
        filter_snapshot(capture,cpu,"steering_filter_pre",instructions,frame,ns);
    }
    if (pc==0x39ae8 && capture.filter.returning.pc) {
        const auto returning=caller_return(cpu);
        if (cpu.m_r[31]!=capture.filter.frame_pointer || returning.pc!=capture.filter.returning.pc ||
            returning.frame_pointer!=capture.filter.returning.frame_pointer)
            throw std::runtime_error("steering filter return or frame mismatch");
        filter_snapshot(capture,cpu,"steering_filter_post",instructions,frame,ns);
        ++capture.filter_completions;capture.filter={};
    }
    if (pc==0x395d8 && frame>=capture.first && frame<=capture.last) {
        if (capture.steering_call.return_pc || !cpu.m_r[30])
            throw std::runtime_error("nested steering leaf or missing return identity");
        if (++capture.steering_entries>16384) throw std::runtime_error("steering call limit exceeded");
        const bool detailed=(frame%20==0 || frame==2750) && capture.steering_selected_frame!=frame && capture.steering_samples<32;
        capture.steering_call={cpu.m_r[31],cpu.m_r[30],0,detailed};
        if (detailed) {
            ++capture.steering_samples;capture.steering_selected_frame=frame;
            capture.steering_call.input=cpu.bus->read_dword(0x202050)&255u;
        }
        steering_snapshot(capture,cpu,"steering_pre",instructions,frame,ns);
    }
    if (pc==0x39664 && capture.steering_call.return_pc) {
        if (cpu.m_r[31]!=capture.steering_call.frame_pointer || cpu.m_r[17]!=capture.steering_call.return_pc)
            throw std::runtime_error("steering leaf return or frame mismatch");
        steering_snapshot(capture,cpu,"steering_post",instructions,frame,ns);
        ++capture.steering_completions;capture.steering_call={};
    }
    if (frame>=capture.first && frame<=capture.last && pc>=0x20000 && pc<0x40000 && !(pc&3))
        ++capture.code_counts[(cpu.m_IP==pc ? 0 : 0x8000)+(pc-0x20000)/4];
    if (pc==0x2aba8 && frame>=capture.first && frame<=capture.last) {
        if (++capture.callback_count>16384) throw std::runtime_error("callback observation limit exceeded");
        std::ostringstream out;
        out << "{\"frame\":" << frame << ",\"instructions\":" << instructions
            << ",\"pc\":" << cpu.m_IP << ",\"object\":" << cpu.m_r[16]
            << ",\"callback\":" << cpu.m_r[20] << '}';
        write(capture,capture.callbacks,out.str());
    }
    if (pc==0x2bfa4 && frame>=capture.first && frame<=capture.last) {
        if (++capture.selector_count>16384) throw std::runtime_error("selector observation limit exceeded");
        const auto object=cpu.m_r[5];
        if (!ram(object,0xec)) throw std::runtime_error("selector object is outside plain RAM");
        std::ostringstream out;
        out << "{\"frame\":" << frame << ",\"instructions\":" << instructions
            << ",\"pc\":" << cpu.m_IP << ",\"object\":" << object
            << ",\"selector\":" << cpu.bus->read_dword(object+0xe8) << '}';
        write(capture,capture.selectors,out.str());
    }
    if ((pc==0x2cce0 || pc==0x2e650) && frame>=capture.first && frame<=capture.last) {
        if (capture.call.id) throw std::runtime_error("nested or unpaired live integrator call");
        if (++capture.calls>16384) throw std::runtime_error("driving cadence call limit exceeded");
        if (capture.selected_frame!=frame) { capture.selected_frame=frame;capture.frame_samples=0; }
        const bool selected=frame%60==0 || frame==2300 || frame==2500 || frame==2501 ||
                            frame==2600 || frame==2601 || frame==2700 || frame==2701 || frame==2752;
        const bool detailed=selected && capture.frame_samples<4 && capture.detailed<128;
        if (detailed) { ++capture.frame_samples;++capture.detailed; }
        if (!ram(cpu.m_r[16],0xec)) throw std::runtime_error("integrator observation requested non-RAM object");
        const auto selector=cpu.bus->read_dword(cpu.m_r[16]+0xe8);
        if (pc==0x2cce0) ++capture.primary_calls; else ++capture.alternate_calls;
        capture.outer_return=caller_return(cpu);
        capture.call={capture.calls,instructions,frame,cpu.m_r[16],cpu.m_r[17],capture.outer_return.pc,
                      capture.outer_return.frame_pointer,cpu.m_IP,selector,detailed};
        cadence(capture,cpu,"integrator_entry",instructions,frame,ns);
        snapshot(capture,cpu,"integrator_entry",instructions,frame,ns);
        return;
    }
    if (!capture.call.id) return;
    if (instructions-capture.call.instructions>2000000)
        throw std::runtime_error("live integrator did not return within bounded instruction count");
    const char *event=nullptr;
    switch(pc) {
    case 0x30e00:event="velocity_entry";break;
    case 0x3110c:event="blend_pre";break;
    case 0x31154:event="blend_store";break;
    case 0x31158:event="blend_post";break;
    case 0x2abc0:
        if (capture.query_return) throw std::runtime_error("nested or unpaired table query");
        capture.query_return_frame=caller_return(cpu);
        capture.query_return=capture.query_return_frame.pc;capture.query_caller_fp=capture.query_return_frame.frame_pointer;
        event="query_pre";break;
    case 0x2ac08:event="query_count";break;
    case 0x2acc0: {
        if (capture.contact.return_pc) throw std::runtime_error("nested or unpaired contact helper");
        if (!ram(cpu.m_r[16],12) || !ram(cpu.m_r[17],32) ||
            !ram(cpu.m_r[31],0x90))
            throw std::runtime_error("contact observation requires bounded ordinary RAM");
        const auto returning=caller_return(cpu);
        capture.contact={};capture.contact.returning=returning;
        capture.contact.return_pc=returning.pc;capture.contact.caller_fp=returning.frame_pointer;
        capture.contact.frame_pointer=cpu.m_r[31];capture.contact.point=cpu.m_r[16];capture.contact.output=cpu.m_r[17];
        ++capture.contact_entries;event="contact_pre";break;
    }
    case 0x2adac:
        if (!capture.contact.return_pc || capture.contact.reply_seen || cpu.m_r[31]!=capture.contact.frame_pointer)
            throw std::runtime_error("contact reply has no matching helper frame");
        capture.contact.reply_seen=true;event="contact_reply";break;
    case 0x2ecb4: {
        if (original_pc(capture.call.entry_pc)!=0x2e650 || capture.orientation.phase || capture.call.orientation_seen)
            throw std::runtime_error("alternate orientation has no unique alternate caller");
        const auto candidate=std::uint64_t(cpu.m_r[31])+0x90;
        if (candidate>0xffffffffu || !ram(std::uint32_t(candidate),16) ||
            cpu.m_r[5]!=candidate || cpu.m_r[12]!=capture.call.object)
            throw std::runtime_error("alternate orientation input frame or object mismatch");
        capture.orientation={cpu.m_r[31],std::uint32_t(candidate),1};
        capture.call.orientation_seen=true;++capture.orientation_entries;
        event="alternate_orientation_pre";break;
    }
    case 0x2ece0:
    case 0x2ee00:
        if (capture.orientation.phase!=(pc==0x2ece0 ? 1u : 2u) ||
            cpu.m_r[31]!=capture.orientation.frame_pointer || cpu.m_r[5]!=capture.orientation.candidate ||
            cpu.m_r[12]!=capture.call.object)
            throw std::runtime_error("alternate orientation sequence or identity mismatch");
        capture.orientation.phase=2;
        event=pc==0x2ece0 ? "alternate_orientation_folded" : "alternate_orientation_post";
        break;
    case 0x2e560:event="orientation_pre";break;
    case 0x2e638:event="orientation_post";break;
    case 0x2e648:event="integrator_before_return";break;
    default:break;
    }
    if (event) {
        if ((pc==0x2e560 || pc==0x2e638) && cpu.m_r[15]!=capture.call.object)
            throw std::runtime_error("orientation object differs from its observed integrator caller");
        snapshot(capture,cpu,event,instructions,frame,ns);
        if (pc==0x2ee00) { ++capture.orientation_completions;capture.orientation={}; }
    }
}

void end_driving_capture() {
    if (!active) throw std::runtime_error("driving capture is not active");
    auto &capture=*active;
    std::ofstream diagnostic(capture.path/"observation.json");
    diagnostic << "{\"observed_instructions\":" << capture.observed
               << ",\"rom_integrator_entries\":" << capture.rom_entries
               << ",\"relocated_integrator_entries\":" << capture.ram_entries
               << ",\"bounded_calls\":" << capture.calls
               << ",\"completed_calls\":" << capture.completed << "}\n";
    diagnostic.close();
    if (!diagnostic) throw std::runtime_error("cannot write capture observation counts");
    std::ofstream coverage(capture.path/"code-coverage.jsonl");
    for (std::size_t i=0;i<capture.code_counts.size();++i) if (capture.code_counts[i])
        coverage << "{\"pc\":" << (0x20000+4*(i%0x8000)+(i>=0x8000 ? relocation : 0))
                 << ",\"count\":" << capture.code_counts[i] << "}\n";
    coverage.close();
    if (!coverage) throw std::runtime_error("cannot write bounded code execution coverage");
    if (capture.call.id || capture.query_return || capture.contact.return_pc || capture.orientation.phase || capture.steering_call.return_pc || capture.filter.returning.pc || capture.main.id || capture.vehicle.id ||
        !capture.calls || !capture.detailed || capture.completed!=capture.calls ||
        capture.orientation_entries!=capture.orientation_completions || capture.contact_entries!=capture.contact_completions ||
        capture.steering_entries!=capture.steering_completions || capture.filter_entries!=capture.filter_completions ||
        capture.main_calls!=capture.main_completions || capture.main_queries!=capture.main_query_completions ||
        capture.main_contacts!=capture.main_contact_completions ||
        capture.main_metric_entries!=capture.main_metric_completions ||
        capture.main_step_entries!=capture.main_step_completions ||
        capture.main_aggregate_entries!=capture.main_aggregate_completions ||
        capture.main_normalization_entries!=capture.main_normalization_completions ||
        capture.main_response_entries!=capture.main_response_completions ||
        capture.vehicle_calls!=capture.vehicle_completions ||
        capture.vehicle_prefix_entries!=capture.vehicle_prefix_completions ||
        capture.vehicle_slot_entries!=capture.vehicle_slot_completions ||
        capture.vehicle_balance_entries!=capture.vehicle_balance_completions ||
        capture.vehicle_selection_entries!=capture.vehicle_selection_completions ||
        capture.vehicle_balance_calls!=capture.vehicle_balance_returns)
        throw std::runtime_error("driving capture has no complete bounded caller evidence");
    capture.events.close();capture.cadence.close();capture.selectors.close();capture.callbacks.close();capture.steering.close();
    capture.steering_filters.close();
    capture.main_events.close();capture.main_cadence.close();
    capture.vehicle_events.close();capture.vehicle_cadence.close();
    if (!capture.events||!capture.cadence||!capture.selectors||!capture.callbacks||!capture.steering||!capture.steering_filters||
        !capture.main_events||!capture.main_cadence||!capture.vehicle_events||!capture.vehicle_cadence)
        throw std::runtime_error("cannot finish driving capture streams");
    std::ofstream summary(capture.path/"capture.json.tmp");
    summary << "{\"schema\":1,\"complete\":true,\"first_frame\":" << capture.first
        << ",\"last_frame\":" << capture.last << ",\"calls\":" << capture.calls
        << ",\"completed_calls\":" << capture.completed << ",\"detailed_calls\":" << capture.detailed
        << ",\"primary_calls\":" << capture.primary_calls << ",\"alternate_calls\":" << capture.alternate_calls
        << ",\"selector_observations\":" << capture.selector_count
        << ",\"callback_observations\":" << capture.callback_count
        << ",\"alternate_orientation_entries\":" << capture.orientation_entries
        << ",\"alternate_orientation_completions\":" << capture.orientation_completions
        << ",\"contact_entries\":" << capture.contact_entries
        << ",\"contact_completions\":" << capture.contact_completions
        << ",\"steering_entries\":" << capture.steering_entries
        << ",\"steering_completions\":" << capture.steering_completions
        << ",\"steering_samples\":" << capture.steering_samples
        << ",\"steering_filter_entries\":" << capture.filter_entries
        << ",\"steering_filter_completions\":" << capture.filter_completions
        << ",\"steering_filter_samples\":" << capture.filter_samples
        << ",\"main_calls\":" << capture.main_calls << ",\"main_completions\":" << capture.main_completions
        << ",\"main_samples\":" << capture.main_samples << ",\"main_events\":" << capture.main_event_count
        << ",\"main_queries\":" << capture.main_queries << ",\"main_query_completions\":" << capture.main_query_completions
        << ",\"main_contacts\":" << capture.main_contacts << ",\"main_contact_completions\":" << capture.main_contact_completions
        << ",\"main_cross_pairs\":" << capture.main_cross_pairs
        << ",\"main_sampled_cross_pairs\":" << capture.main_sampled_cross_pairs
        << ",\"main_metric_entries\":" << capture.main_metric_entries
        << ",\"main_metric_completions\":" << capture.main_metric_completions
        << ",\"main_metric_sampled_pairs\":" << capture.main_metric_sampled_pairs
        << ",\"main_step_entries\":" << capture.main_step_entries
        << ",\"main_step_completions\":" << capture.main_step_completions
        << ",\"main_step_sampled_pairs\":" << capture.main_step_sampled_pairs
        << ",\"main_aggregate_entries\":" << capture.main_aggregate_entries
        << ",\"main_aggregate_completions\":" << capture.main_aggregate_completions
        << ",\"main_aggregate_samples\":" << capture.main_aggregate_samples
        << ",\"main_normalization_entries\":" << capture.main_normalization_entries
        << ",\"main_normalization_completions\":" << capture.main_normalization_completions
        << ",\"main_normalization_sampled_pairs\":" << capture.main_normalization_sampled_pairs
        << ",\"main_response_entries\":" << capture.main_response_entries
        << ",\"main_response_completions\":" << capture.main_response_completions
        << ",\"main_response_sampled_pairs\":" << capture.main_response_sampled_pairs
        << ",\"vehicle_calls\":" << capture.vehicle_calls << ",\"vehicle_completions\":" << capture.vehicle_completions
        << ",\"vehicle_samples\":" << capture.vehicle_samples << ",\"vehicle_events\":" << capture.vehicle_event_count
        << ",\"vehicle_prefix_entries\":" << capture.vehicle_prefix_entries
        << ",\"vehicle_prefix_completions\":" << capture.vehicle_prefix_completions
        << ",\"vehicle_prefix_samples\":" << capture.vehicle_prefix_samples
        << ",\"vehicle_slot_entries\":" << capture.vehicle_slot_entries
        << ",\"vehicle_slot_completions\":" << capture.vehicle_slot_completions
        << ",\"vehicle_slot_samples\":" << capture.vehicle_slot_samples
        << ",\"vehicle_balance_entries\":" << capture.vehicle_balance_entries
        << ",\"vehicle_balance_completions\":" << capture.vehicle_balance_completions
        << ",\"vehicle_balance_samples\":" << capture.vehicle_balance_samples
        << ",\"vehicle_balance_calls\":" << capture.vehicle_balance_calls
        << ",\"vehicle_balance_returns\":" << capture.vehicle_balance_returns
        << ",\"vehicle_selection_entries\":" << capture.vehicle_selection_entries
        << ",\"vehicle_selection_completions\":" << capture.vehicle_selection_completions
        << ",\"vehicle_selection_samples\":" << capture.vehicle_selection_samples
        << ",\"events\":" << capture.events_count << ",\"bytes\":" << capture.bytes
        << ",\"sampling\":\"up to four calls at each selected input/60-frame checkpoint; maximum128 detailed calls\"}\n";
    summary.close();if (!summary) throw std::runtime_error("cannot finish driving capture marker");
    std::filesystem::rename(capture.path/"events.jsonl.tmp",capture.path/"events.jsonl");
    std::filesystem::rename(capture.path/"cadence.jsonl.tmp",capture.path/"cadence.jsonl");
    std::filesystem::rename(capture.path/"selectors.jsonl.tmp",capture.path/"selectors.jsonl");
    std::filesystem::rename(capture.path/"callbacks.jsonl.tmp",capture.path/"callbacks.jsonl");
    std::filesystem::rename(capture.path/"steering.jsonl.tmp",capture.path/"steering.jsonl");
    std::filesystem::rename(capture.path/"steering-filter.jsonl.tmp",capture.path/"steering-filter.jsonl");
    std::filesystem::rename(capture.path/"main-contact.jsonl.tmp",capture.path/"main-contact.jsonl");
    std::filesystem::rename(capture.path/"main-contact-cadence.jsonl.tmp",capture.path/"main-contact-cadence.jsonl");
    std::filesystem::rename(capture.path/"main-vehicle.jsonl.tmp",capture.path/"main-vehicle.jsonl");
    std::filesystem::rename(capture.path/"main-vehicle-cadence.jsonl.tmp",capture.path/"main-vehicle-cadence.jsonl");
    std::filesystem::rename(capture.path/"capture.json.tmp",capture.path/"capture.json");
    active.reset();
}
} // namespace ps2::offline
