#pragma once

// The game's own pictures of the toys for the portal menu (toy_images.cpp).

#include <filesystem>

namespace superchargersrecomp {

// Reads the toys' collection icons from the disc's archives in the background and gives them to
// the portal menu (SetFigureImageProvider). `rom_dir` is the game's folder (rom/).
void InstallToyImages(const std::filesystem::path& rom_dir);

}  // namespace superchargersrecomp
