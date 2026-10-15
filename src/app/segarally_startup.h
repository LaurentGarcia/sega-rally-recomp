#pragma once
#include <filesystem>

namespace app::segarally {
// Existing personal bundles load their own data. Empty player packages import
// a verified ZIP into the user's data folder; never compile or download code.
// An empty return means the player cancelled the native file chooser.
std::filesystem::path prepare_player_data(const std::filesystem::path &bundled,
    const std::filesystem::path &saves, const std::filesystem::path &rom,
    bool interactive);
}
