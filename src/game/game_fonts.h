#pragma once

// The game's own fonts for the portal menus (src/game/game_fonts.cpp).

#include <filesystem>

#include "portal_rex/game_font.h"

namespace superchargersrecomp {

// The game's title and body fonts from its files (rom/archives); a font that can't be read is
// left empty, so the menus keep the Windows font for it.
skylanders::GameFonts LoadGameFonts(const std::filesystem::path& rom_dir);

}  // namespace superchargersrecomp
