// The game's own anti-aliasing at the real resolution.
//
// SwapForceRecomp's hook (its src/hooks/fxaa_scale_hook.cpp has how it was found there): the game
// runs NVIDIA FXAA 3.11 (its console / Xbox 360 variant) over the 3D scene every frame, with
// sample offsets that sub_83610558 turns the frame size (f1 = width, f2 = height, f3 = 0.5) into:
// fxaaQualityRcpFrame (1/w, 1/h), fxaaConsoleRcpFrameOpt (+-0.5/w, +-0.5/h), Opt2 and the 360 Opt2
// (8/w, 8/h, -4/w, -4/h). The game passes its own scene size. With resolution_scale the scene is
// drawn that many times larger, so the shader sampled that many host pixels apart: softer edges
// and only part of the smoothing (the owner found the picture "sehr unscharf", 2026-10-21).
// Multiplying the size by the draw resolution scale puts the samples one pixel apart again, as on
// the console.
//
// Found 2026-10-21 in a dump of this game's code by Swap Force's instructions: all 59 of the
// function's instructions the same. The function only writes the attr (four vectors at r3 + 16..64),
// so scaling its two inputs changes nothing else.

#include <algorithm>
#include <atomic>
#include <cstdint>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc/context.h>

REXCVAR_DEFINE_BOOL(fxaa_follow_resolution_scale, true, "Graphics",
                    "Let the game's own FXAA work at the internal resolution: its sample offsets "
                    "assume the console's 1040x620, so without this it blurs at resolution_scale "
                    "2 and up");

namespace {

// The draw resolution scale the GPU code uses (TextureCache::GetConfigDrawResolutionScale, which
// lives in the GPU plugin): resolution_scale for both axes unless draw_resolution_scale_x/y are set.
uint32_t DrawScale(const char* axis_cvar) {
  const auto shared = uint32_t(std::max(1, rex::cvar::Query<int32_t>("resolution_scale")));
  const bool use_shared = rex::cvar::HasNonDefaultValue("resolution_scale") &&
                          !rex::cvar::HasNonDefaultValue(axis_cvar);
  const uint32_t scale =
      use_shared ? shared : uint32_t(std::max(1, rex::cvar::Query<int32_t>(axis_cvar)));
  return std::min<uint32_t>(scale, 7);
}

}  // namespace

void FxaaFrameSizeHook(PPCRegister& f1, PPCRegister& f2) {
  if (!REXCVAR_GET(fxaa_follow_resolution_scale)) return;
  static const uint32_t scale_x = DrawScale("draw_resolution_scale_x");
  static const uint32_t scale_y = DrawScale("draw_resolution_scale_y");
  static std::atomic<bool> logged{false};
  if (!logged.exchange(true)) {
    REXLOG_INFO("FXAA: frame {}x{} scaled by {}x{}", f1.f64, f2.f64, scale_x, scale_y);
  }
  f1.f64 *= scale_x;
  f2.f64 *= scale_y;
}
