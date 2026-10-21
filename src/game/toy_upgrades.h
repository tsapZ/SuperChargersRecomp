#pragma once

// Every Skylander's upgrades for the portal menu's details view, from the game's character data
// (see toy_upgrades.cpp).

#include <filesystem>
#include <memory>
#include <string>

#include "portal_rex/portal_menu.h"

namespace superchargersrecomp {

// Registers the portal menu's upgrade provider; a character's upgrades are read from `rom_dir`
// the first time the menu asks for them.
void InstallToyUpgrades(const std::filesystem::path& rom_dir);

// The picture for an "upgrade:..." art key of the upgrades above; nullptr while it is still
// loading (it is read on first request) or if there is none. Called on the UI thread.
std::shared_ptr<const skylanders::FigureImage> UpgradeImage(const std::string& key);

}  // namespace superchargersrecomp
