// First-run assets only: no recompiler, subprocess, network, or save-file access.
#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace app::segarally {
struct ImportedImage {
    std::string name;
    uint64_t size;
    std::string sha256;
};
struct RomImportResult {
    std::filesystem::path directory;
    std::string archive_sha256;
    unsigned verified_files = 0;
    std::vector<ImportedImage> images;
};
using RomImportProgress = std::function<void(std::string_view)>;

// Accepts any ZIP filename, but only the exact Revision B file bytes pinned in
// config/segarally_{roms,tgp,video,sound}.json. Stored/deflated ZIP only. Every
// required file and alias is checked by size, CRC32 and SHA1 before disk output;
// each assembled image must also match its fixed SHA256. Throws on failure.
// new_directory must not exist. Its parent must exist and contain no symlinks.
// A same-directory staging tree is flushed and committed with no-replace rename.
// Failures leave the destination absent, preserve existing data, and remove only
// this call's staging directory. Saves must live outside new_directory.
RomImportResult import_rom_zip(const std::filesystem::path &zip,
    const std::filesystem::path &new_directory, RomImportProgress progress = {});

// Rechecks the exact fixed manifest and all eight compiled-in image identities.
// No caller-supplied hashes or generation-provenance claims are trusted. Returns
// the verified assets directory; throws if incomplete, changed, linked or extra.
RomImportResult validate_cached_assets(const std::filesystem::path &directory);
} // namespace app::segarally
