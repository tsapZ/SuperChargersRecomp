#pragma once

// The game's controller picture for the settings menu's controls page (controller_picture.cpp).

#include <memory>

#include "game/alchemy/iga_archive.h"
#include "portal_rex/portal_menu.h"
#include "portal_rex/settings_menu.h"

namespace superchargersrecomp {

// The art key the picture is served under ("controller:xbox360").
extern const char kControllerArtKey[];

// Reads the Controls screen's picture from Controller_SchemeXenon.pak and cleans it (see the .cpp);
// nullptr if the archive or the picture isn't there.
std::shared_ptr<skylanders::FigureImage> ReadControllerPicture(const alchemy::IgaArchive& archive);

// The picture's size and the buttons on it, for skylanders::SetControllerPicture.
skylanders::ControllerPicture ControllerPictureLayout();

}  // namespace superchargersrecomp
