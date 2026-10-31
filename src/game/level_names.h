#pragma once

// The levels' names as the game shows them, for the Discord status (level_names.cpp).

#include <filesystem>

namespace superchargersrecomp {

// Reads the levels' names from the disc's archives in the background and gives them to the Discord
// status (skylanders-portal portal_rex/discord_presence.h). `rom_dir` is the game's folder (rom/).
void InstallLevelNames(const std::filesystem::path& rom_dir);

}  // namespace superchargersrecomp
