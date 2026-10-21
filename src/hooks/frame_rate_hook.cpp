// Frame rate unlock: overrides the presentation interval of the game's D3D swaps.
//
// SwapForceRecomp's hook (its src/hooks/frame_rate_hook.cpp and docs/high-framerate.md have how it
// was found there): the game paces itself through the D3D library's swap queue. sub_828777D8
// queues a swap and takes a swap word in r3 whose bits 8-11 are the presentation interval
// (vblanks per frame); the vblank handler only flips to the frame once the vblank counter reaches
// "last swap + interval". This game uses interval 2, i.e. 30 FPS at 60 Hz: measured 2026-10-21,
// 30.0 FPS in play with frame_rate_limit 60 and 72.0 with 144 (the emulated vblank's rate / 2).
// The midasm hook at 0x828777D8 (config/default.toml) rewrites the interval before the function
// reads it. The function is the D3D library's own code, found in a dump of this game's code by
// Swap Force's instructions (38 of its first 40 the same).
//
// Off by default: whether all of the game keeps its speed at more frames is checked per game (see
// docs/STATUS.md).

#include <atomic>
#include <cstdint>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc/context.h>

REXCVAR_DEFINE_INT32(present_interval, 0, "Graphics",
                     "Override the game's presentation interval (vblanks per frame): 0 = game "
                     "default (2, 30 FPS), 1 = 60 FPS and more (up to frame_rate_limit).");

void SwapIntervalHook(PPCRegister& r3) {
  static std::atomic<bool> logged{false};
  if (!logged.exchange(true)) {
    REXLOG_INFO("Swap: game presentation interval {} (swap word {:08X})", (r3.u32 >> 8) & 0xF, r3.u32);
  }
  const int32_t interval = REXCVAR_GET(present_interval);
  if (interval > 0 && interval <= 15) {
    r3.u32 = (r3.u32 & ~0xF00u) | (static_cast<uint32_t>(interval) << 8);
  }
}
