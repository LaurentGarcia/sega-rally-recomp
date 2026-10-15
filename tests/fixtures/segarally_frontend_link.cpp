// Original, ROM-free link symbols for compiling the real frontend in CI.
// These contain no guest instructions and cannot run a game.
#include "runtime/gen_support.h"
#include "runtime/tgp.h"
#include "runtime/snd_gen_support.h"
#include <stdexcept>
namespace gen {
bool has_code(uint32_t) { return false; }
void run(Env &) { throw std::runtime_error("ROM-free frontend compile fixture is not a game"); }
uint64_t native_instructions() { return 0; }
#ifdef M2_INSTRUCTION_BOUNDARY_HOOK
bool instruction_boundary_hook_compatible() { return false; }
#endif
}
namespace rt::tgpgen {
extern const uint32_t program_crc32 = 0;
extern const uint32_t program_words = 0;
void run(Tgp &, uint64_t) { throw std::runtime_error("No TGP program in compile fixture"); }
}
namespace sndgen {
bool has_code(uint32_t) { return false; }
void run(Env &) { throw std::runtime_error("No sound program in compile fixture"); }
uint64_t native_instructions() { return 0; }
}
