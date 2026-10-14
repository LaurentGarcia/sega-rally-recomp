#include "runtime/sha256_digest.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {
unsigned checks = 0;
void require(bool value, const char *message) { ++checks; if (!value) throw std::runtime_error(message); }
template<class F> void rejects(F operation) {
    bool rejected = false; try { operation(); } catch (const std::exception &) { rejected = true; }
    require(rejected, "invalid SHA-256 operation returned a digest");
}
std::span<const uint8_t> bytes(const std::string &text) {
    return {reinterpret_cast<const uint8_t *>(text.data()), text.size()};
}
void known(const std::string &input, const std::string &expected) {
    const auto actual = rt::sha256_hex(bytes(input));
    if (actual != expected) throw std::runtime_error("known-answer hash differs at " + std::to_string(input.size()) +
        " bytes: " + actual + " expected " + expected);
    require(actual == expected, "known-answer byte-span hash differs");
    for (size_t chunk : {1u, 7u, 55u, 56u, 63u, 64u, 65u, 99u, 128u, 1024u}) {
        rt::Sha256 state; state.update({});
        for (size_t at = 0; at < input.size(); at += chunk)
            state.update(bytes(input).subspan(at, std::min(chunk, input.size() - at)));
        require(state.bytes() == input.size() && rt::sha256_digest_hex(state.finish()) == expected,
            "chunking or padding boundary changed known-answer hash");
        rejects([&] { state.finish(); }); rejects([&] { state.update({}); });
    }
    std::istringstream stream(input); const auto result = rt::sha256_stream(stream, input.size());
    require(result.bytes == input.size() && result.hex == expected, "known-answer stream hash differs");
    std::istringstream throwing(input); throwing.exceptions(std::ios::failbit | std::ios::badbit);
    require(rt::sha256_stream(throwing, input.size()).hex == expected, "throw-on-EOF stream lost final bytes");
}
struct Fixture {
    std::filesystem::path directory, path;
    Fixture() {
        directory = std::filesystem::temp_directory_path() / ("segarally-sha256-unit-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        if (!std::filesystem::create_directory(directory)) throw std::runtime_error("cannot create unique fixture directory");
        path = directory / "input.bin";
    }
    ~Fixture() { std::error_code error; std::filesystem::remove(path, error); std::filesystem::remove(directory, error); }
    void write(const std::string &text) {
        std::ofstream out(path, std::ios::binary); out.write(text.data(), std::streamsize(text.size()));
        if (!out) throw std::runtime_error("cannot write SHA-256 fixture");
    }
};
void bounded_io() {
    const std::string abc_hash = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    Fixture file; file.write("abc");
    const auto result = rt::sha256_file(file.path, 3);
    require(result.bytes == 3 && result.hex == abc_hash, "bounded file hash differs from loaded bytes");
    rejects([&] { rt::sha256_file(file.path, 2); });
    rejects([&] { rt::sha256_file(file.path, 0); });
    rejects([&] { rt::sha256_file(file.directory / "missing", 10); });
    rejects([&] { rt::sha256_file(file.directory, 10); });
    file.write(""); require(rt::sha256_file(file.path, 0).bytes == 0, "empty file rejected at zero bound");
    const std::string large(1'000'000, 'a'); file.write(large);
    const std::string expected = "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0";
    require(rt::sha256_file(file.path, large.size()).hex == expected, "multi-buffer file known answer differs");
    rejects([&] { rt::sha256_file(file.path, large.size() - 1); });
    std::istringstream failed("abc"); failed.setstate(std::ios::badbit);
    rejects([&] { rt::sha256_stream(failed, 3); });
    std::istringstream oversized("abc"); rejects([&] { rt::sha256_stream(oversized, 2); });
    std::istringstream invalid_bound; rejects([&] { rt::sha256_stream(invalid_bound, UINT64_MAX); });
    // Hash the seed snapshot actually loaded, rather than reopening its path.
    const std::string startup = "abc"; file.write("changed after startup load");
    require(rt::sha256_hex(bytes(startup)) == abc_hash && rt::sha256_file(file.path, 100).hex != abc_hash,
        "snapshot hash unexpectedly followed subsequent file mutation");
    std::istringstream positioned("prefixabc"); positioned.seekg(6);
    require(rt::sha256_stream(positioned, 3).hex == abc_hash, "stream hash ignored current position");
}
} // namespace
int main() {
    try {
        known("", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
        known("abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
        known("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
        known("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
            "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
            "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1");
        std::string binary; for (unsigned n = 0; n < 256; ++n) binary += char(n);
        known(binary, "40aff2e9d2d8922e47afd4648e6967497158785fbd1da870e7110266bf944880");
        const std::array<uint8_t, 32> zeros{};
        require(rt::sha256_hex(zeros) == "66687aadf862bd776c8fc18b8e9f8e20089714856ee233b3902a591d0d5f2925",
            "32-byte input was formatted as a digest instead of hashed");
        bounded_io(); std::printf("Pinned SHA-256 span/stream/file/chunking: %u checks passed\n", checks); return 0;
    } catch (const std::exception &e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
}
