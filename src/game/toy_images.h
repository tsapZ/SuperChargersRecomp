#pragma once

// The game's own pictures of the toys for the portal menu (toy_images.cpp).

#include <cstdint>
#include <filesystem>
#include <string>

namespace superchargersrecomp {

// Reads the toys' collection icons from the disc's archives in the background and gives them to
// the portal menu (SetFigureImageProvider). `rom_dir` is the game's folder (rom/).
void InstallToyImages(const std::filesystem::path& rom_dir);

// The name a toy's files go by (its toy data file's: "BladePenguin" for King Pen,
// "ChopChop"), empty for an unknown toy or while the toy data is being read.
std::string ToyCharacter(uint16_t id);

}  // namespace superchargersrecomp
