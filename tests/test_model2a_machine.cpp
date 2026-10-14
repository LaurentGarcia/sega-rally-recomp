// Host lifecycle fixture using synthetic generated entry points, no game ROM.
#include "runtime/model2a_machine.h"
#include "runtime/gen_support.h"
#include <cassert>
#include <cstdio>

namespace { bool device_frontier = false; }
namespace gen {
bool has_code(uint32_t pc) { return pc >= 0x420 && pc < 0x440 && !(pc & 3); }
uint64_t native_instructions() { return 8; }
void run(Env &e) {
    while (has_code(e.c.m_IP) && !e.ls.boundary()) {
        e.c.consume_reference_cycles(2);
        if (device_frontier && e.c.m_IP == 0x428) e.c.bus->write_dword(0xdead0000, 7);
        e.c.m_IP += 4;
        ++e.ls.count;
    }
}
}
namespace {
using Machine = rt::Model2AMachine;
Machine::Images images() {
    Machine::Images result;
    result.program.resize(32);
    result.program[12] = 0x20; result.program[13] = 4;
    return result;
}
Machine::Options options() { return {false, true, false, true, false, false, false, false}; }
}
int main() {
    Machine machine(images(), options());
    assert(machine.cpu().m_IP == 0x420);
    auto eeprom = machine.save_eeprom();
    auto backup = machine.save_backup();
    assert(eeprom.size() == 128 && eeprom.front() == 255);
    assert(backup.size() == 16384 && backup.back() == 255);
    eeprom[3] = 23; backup[10] = 42;
    machine.load_eeprom(eeprom); machine.load_backup(backup);
    assert(machine.save_eeprom() == eeprom && machine.save_backup() == backup);
    bool failed = false;
    try { machine.load_backup({1, 2}); } catch (const rt::Fatal &) { failed = true; }
    assert(failed && machine.save_backup() == backup);
    machine.inputs().analog[0] = 11;
    assert(machine.inputs().analog[0] == 11);
    auto result = machine.run_for(3);
    assert(result.reason == Machine::Reason::Budget && result.instructions == 3 && result.pc == 0x42c);
    assert(machine.stats().reference_cycles == 6);
    result = machine.run_for(2);
    assert(result.reason == Machine::Reason::Budget && result.instructions == 5 && result.pc == 0x434);
    assert(machine.run_for(0).instructions == 5);
    failed = false;
    try { machine.load_eeprom(eeprom); } catch (const rt::Fatal &) { failed = true; }
    assert(failed);
    failed = false;
    try { (void)machine.run_until_frame(1); } catch (const rt::Fatal &) { failed = true; }
    assert(failed);
    result = machine.run_for(100);
    assert(result.reason == Machine::Reason::MissingCode && result.instructions == 8 && result.pc == 0x440);
    assert(machine.run_for(100).instructions == 8 && machine.stats().reference_cycles == 16);
    device_frontier = true;
    Machine stopped(images(), options());
    result = stopped.run_for(100);
    assert(result.reason == Machine::Reason::DeviceAccess && result.instructions == 2 && result.pc == 0x428);
    assert(result.address == 0xdead0000 && result.width == 4 && result.write && result.value == 7);
    assert(stopped.stats().reference_cycles == 6 && stopped.run_for(10).instructions == 2);
    std::puts("Model2A machine resume, persistence and exact frontier checks passed");
}
