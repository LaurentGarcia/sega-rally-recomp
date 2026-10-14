// Small streaming wrapper around the pinned public-domain 7-Zip SHA-256.
// Upstream C/Sha256.c, Sha256Opt.c and CpuArch.c are linked unchanged.
#pragma once
#include <Sha256.h>
#include <array>
#include <cstdint>
#include <filesystem>
#include <istream>
#include <span>
#include <string>

namespace rt {
using Sha256Digest = std::array<uint8_t, SHA256_DIGEST_SIZE>;

class Sha256 {
public:
    // SHA-256's bit-length field is 64 bits; reject byte-count wrap/truncation.
    static constexpr uint64_t maximum_bytes = UINT64_MAX / 8;
    Sha256();
    void update(std::span<const uint8_t> bytes);
    Sha256Digest finish();
    uint64_t bytes() const { return bytes_; }
private:
    CSha256 state_{};
    uint64_t bytes_ = 0;
    bool finished_ = false;
};

std::string sha256_digest_hex(const Sha256Digest &digest);
// Hash the exact bytes actually loaded/snapshotted for a manifest; reopening
// the original seed path may observe different bytes and is not equivalent.
std::string sha256_hex(std::span<const uint8_t> bytes);

struct Sha256Result {
    uint64_t bytes = 0;
    std::string hex;
};
// Both helpers use a fixed 64-KiB buffer and require an explicit input bound.
// Oversize, missing/non-regular files and read failures throw; none return a
// partial hash. The stream helper consumes from the current stream position.
Sha256Result sha256_stream(std::istream &input, uint64_t maximum_bytes);
Sha256Result sha256_file(const std::filesystem::path &path, uint64_t maximum_bytes);
} // namespace rt
