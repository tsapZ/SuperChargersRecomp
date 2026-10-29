// Where the player is, for problem reports: the level and player 1's position (where_hook.cpp).

#pragma once

#include <cstdint>

#include <rex/ppc/context.h>

namespace superchargersrecomp {

// Names the level after the level archive the game opened last (skylanders-portal dev/where.h).
void InstallWhere();

}  // namespace superchargersrecomp

#if defined(SKYLANDERS_DEVKIT)
// The developer kit's per-player work (a private test build only), from where_hook.cpp's hook.
void DevKitPlayerCheck(PPCContext& ctx, uint8_t* base);
#endif
