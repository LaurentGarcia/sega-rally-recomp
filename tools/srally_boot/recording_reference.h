// Test-only binding for the existing dual-board independent CPU harness.
#pragma once
#include "recording_bundle.h"
#include "reference_resume.h"
#include "runtime/sha256_digest.h"
#include "runtime/gen_support.h"
#include <map>

namespace srally::recording_reference {
struct Relocation { uint32_t guest, program_offset, size; };
struct BuildBinding {
    // Emitted by the wrapper from ACTUAL measured files after audit/build
    // checks, never accepted as an assertion supplied alongside a recording.
    std::string_view executable_sha256, assets_sha256;
    std::span<const uint32_t> instruction_addresses;
    std::span<const Relocation> relocations;
    uint64_t maximum_instructions;
    uint64_t progress_instructions = 0; // Test reporting only; disabled by default.
};
using Profile = std::map<uint32_t,reference_resume::GuardedInstruction>;
inline Profile validate(const recording::PreflightBundle &bundle,
        const std::filesystem::path &host, const BuildBinding &binding) {
    if(bundle.options().sound)throw rt::Fatal("independent recording: sound-on bundle unsupported");
    if(!binding.maximum_instructions||bundle.summary().terminal.instructions>binding.maximum_instructions)
        throw rt::Fatal("independent recording: requested instruction bound exceeded");
    if(rt::sha256_file(host,1024ull*1024*1024).hex!=binding.executable_sha256||
       rt::sha256_hex(bundle.assets_report())!=binding.assets_sha256)
        throw rt::Fatal("independent recording: actual host/assets differ from linked build contract");
    const auto empty=rt::sha256_hex(std::span<const uint8_t>{});
    bundle.each([&](const host_input::Resume &row,const recording::Machine::RunResult &,
                    uint64_t samples,const std::string &pcm){
        if(row.reason==recording::Machine::Reason::DeviceAccess||row.reason==recording::Machine::Reason::RuntimeError)
            throw rt::Fatal("independent recording: faulting bundle unsupported");
        if(samples||pcm!=empty)throw rt::Fatal("independent recording: nonempty sound-off PCM evidence");
    });
    if(binding.instruction_addresses.empty()||binding.instruction_addresses.size()>1000000||
       gen::native_instructions()!=binding.instruction_addresses.size())
        throw rt::Fatal("independent recording: linked instruction inventory differs");
    const auto &program=bundle.images().program;
    Profile profile;uint32_t previous=0;bool first=true;
    for(uint32_t pc:binding.instruction_addresses){
        if((pc&3)||(!first&&pc<=previous)||!gen::has_code(pc))
            throw rt::Fatal("independent recording: invalid linked instruction address");
        first=false;previous=pc;uint64_t offset=pc,available=pc<0x200000?0x200000-pc:0;bool mapped=pc<0x200000;
        if(!mapped)for(const auto &r:binding.relocations){
            if(r.guest<0x5a0000||uint64_t(r.guest)+r.size>0x600000||(r.guest|r.program_offset|r.size)&3)
                throw rt::Fatal("independent recording: unproven reset-copy mapping");
            if(pc>=r.guest&&uint64_t(pc)<uint64_t(r.guest)+r.size){
                if(mapped)throw rt::Fatal("independent recording: overlapping reset-copy mapping");
                offset=uint64_t(r.program_offset)+pc-r.guest;available=uint64_t(r.guest)+r.size-pc;mapped=true;
            }
        }
        if(!mapped||offset+4>program.size())throw rt::Fatal("independent recording: instruction outside verified program");
        auto word=[&](uint64_t at){uint32_t v=0;for(unsigned n=0;n<4;++n)v|=uint32_t(program[at+n])<<(8*n);return v;};
        const auto w0=word(offset),w1=offset+8<=program.size()?word(offset+4):0;
        const auto decoded=i960::decode(pc,w0,w1);
        if(!decoded.executable()||(decoded.length!=4&&decoded.length!=8)||(decoded.length>available)||offset+decoded.length>program.size())
            throw rt::Fatal("independent recording: invalid generated instruction profile");
        profile.emplace(pc,reference_resume::GuardedInstruction{{w0,w1},unsigned(decoded.length/4)});
    }
    return profile;
}
} // namespace srally::recording_reference
