#include "runtime/sha256_digest.h"
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace rt {
Sha256::Sha256() {
    Sha256_Init(&state_);
    // Per-context public API selection. No process-global Sha256Prepare call,
    // CPU probing, or mutable global initialization is needed by this wrapper.
    if (!Sha256_SetFunction(&state_, SHA256_ALGO_SW))
        throw std::runtime_error("SHA-256 software implementation unavailable");
}
void Sha256::update(std::span<const uint8_t> bytes) {
    if (finished_) throw std::logic_error("SHA-256 update after finish");
    if (bytes.size() > maximum_bytes - bytes_)
        throw std::length_error("SHA-256 input exceeds 64-bit bit-length limit");
    if (!bytes.empty()) Sha256_Update(&state_, bytes.data(), bytes.size());
    bytes_ += bytes.size();
}
Sha256Digest Sha256::finish() {
    if (finished_) throw std::logic_error("SHA-256 finish called twice");
    Sha256Digest result{}; Sha256_Final(&state_, result.data()); finished_ = true; return result;
}
std::string sha256_digest_hex(const Sha256Digest &digest) {
    constexpr char hex[] = "0123456789abcdef";
    std::string result; result.reserve(SHA256_DIGEST_SIZE * 2);
    for (auto byte : digest) { result += hex[byte >> 4]; result += hex[byte & 15]; }
    return result;
}
std::string sha256_hex(std::span<const uint8_t> bytes) {
    Sha256 state; state.update(bytes); return sha256_digest_hex(state.finish());
}
Sha256Result sha256_stream(std::istream &input, uint64_t maximum_bytes) {
    if (maximum_bytes > Sha256::maximum_bytes) throw std::length_error("invalid SHA-256 stream bound");
    Sha256 state; std::array<uint8_t, 64 * 1024> buffer;
    for (;;) {
        const auto remaining = maximum_bytes - state.bytes();
        // Read one byte beyond the bound to distinguish an exactly-sized file
        // from oversize input, without trusting mutable filesystem metadata.
        const auto requested = std::min(uint64_t(buffer.size()), remaining + 1);
        try { input.read(reinterpret_cast<char *>(buffer.data()), std::streamsize(requested)); }
        catch (const std::ios_base::failure &) {
            // Streams that throw on fail/eof still report a valid final short
            // read through gcount. A real I/O failure must never yield a hash.
            if (input.bad() || !input.eof()) throw;
        }
        if (input.bad() || (input.fail() && !input.eof())) throw std::runtime_error("SHA-256 stream read failed");
        const auto count = uint64_t(input.gcount());
        if (count > remaining) throw std::length_error("SHA-256 stream exceeds input bound");
        state.update({buffer.data(), size_t(count)});
        if (input.eof()) return {state.bytes(), sha256_digest_hex(state.finish())};
    }
}
Sha256Result sha256_file(const std::filesystem::path &path, uint64_t maximum_bytes) {
    if (!std::filesystem::is_regular_file(path)) throw std::runtime_error("SHA-256 requires a regular file: " + path.string());
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open SHA-256 input: " + path.string());
    return sha256_stream(input, maximum_bytes);
}
} // namespace rt
