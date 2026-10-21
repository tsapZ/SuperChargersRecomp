#pragma once

// The hats for the portal menu: names, stats and pictures from the game's files (hats.cpp).

#include <filesystem>
#include <memory>
#include <string>

#include "portal_rex/portal_menu.h"

namespace superchargersrecomp {

// Reads the hats from the disc's archives in the background and gives them to the portal menu
// (SetHatInfoProvider). `rom_dir` is the game's folder (rom/).
void InstallHats(const std::filesystem::path& rom_dir);

// A hat's picture for the menu's art provider ("hat:<number>"), nullptr if it has none or the
// hats aren't read yet.
std::shared_ptr<const skylanders::FigureImage> HatPicture(const std::string& key);

}  // namespace superchargersrecomp
