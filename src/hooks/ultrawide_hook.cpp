// Ultrawide: a wider field of view for the game's 3D camera when the window is wider than 16:9.
//
// SwapForceRecomp's hook (its src/hooks/ultrawide_hook.cpp has how the camera was found there):
// every frame the renderer sets up the scene camera, stores the display's aspect ratio (16:9) and
// the field of view, and builds the projection with the perspective builder (r3 = matrix, f1 =
// horizontal FOV, f2 = vertical FOV, f3 = aspect, f4 = near, f5 = far; radians; the horizontal FOV
// when f1 >= 0). The guest frame stays 16:9; with present_letterbox off (the app turns it off while
// this is active, unless set by hand) the presenter stretches it across the wider window, so the
// camera has to see that much more to the sides for the picture to keep its proportions ("Hor+"):
// the hook at the call widens the aspect and, if the call gives a horizontal FOV, that one too,
// keeping the vertical FOV. The HUD is drawn into the same 16:9 frame and would be stretched.
//
// The HUD (ultrawide_hud, on): the UI (Juice) runs in contexts with Swap Force's
// fields plus one (SwapForceRecomp src/hooks/ultrawide_hook.cpp has how it was found there):
// _renderer, _vfxManager, _viewerWidthMain, _viewerHeightMain, _settingWidthMain,
// _settingHeightMain, _screenWidthSub, _screenHeightSub, _expectedScreenWidth,
// _expectedScreenHeight (the name list at 0x823A2730's), in the objects +0x08 .. +0x2C, all sizes 1280 x 720
// (floats; seen 2026-10-21 in a snapshot of SuperChargers' three contexts at the portal prompt).
// Juice lays its UI out on the expected width, so the hook sets it (+0x28, the height at +0x2C)
// to the window's width at the UI's height where the context's sizes are read for drawing
// (sub_827CBB80, the main screen's branch at 0x827CBBC4, r11 = the context), and puts the game's own
// back when ultrawide is off. Checked by script in a 2560x1080 window (2026-10-21): the portal prompt, a level's HUD (the gauge and portrait at the left edge, the hint banner) and the pause menu keep
// their proportions, nothing left uncovered (Swap Force's pause menu dimming leaves a strip there,
// so it is off by default in that game).
//
// Found 2026-10-21: the builder sub_8250E610 in a dump of this game's code by Swap Force's
// (42 of its first 48 instructions the same); the call with a probe, a hook at each of the
// builder's callers logging its arguments in play: 0x82B0BCA0 is the one that runs every
// frame with the display's 16:9 and a horizontal FOV that follows the camera (the vertical one
// passed as -1 degree, unset). The other callers draw other views (the vertical FOV -1, or a
// 5 degree FOV).
//
// Movies: the logos and the opening movie are 16:9 Bink movies drawn into the 16:9 frame, which
// the presenter would stretch across the window like the 3D picture. While one plays the
// presenter keeps black bars at the sides instead (present_use_actual_aspect off and
// present_letterbox on just for that time, not saved). BinkOpen's header hook
// (intro_skip_hook.cpp) marks the start, the start of BinkClose (sub_831C9F38, called only from the
// game's movie close sub_826FB958, which then clears the movie's handle) the end.

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

#include "ultrawide_hook.h"

#include <rex/cvar.h>
#include <rex/graphics/video_mode_util.h>
#include <rex/logging.h>
#include <rex/memory.h>
#include <rex/memory/utils.h>
#include <rex/ppc/context.h>
#include <rex/ui/flags.h>
#include <rex/system/kernel_state.h>

REXCVAR_DECLARE(std::string, resolution);

REXCVAR_DEFINE_BOOL(ultrawide_fov, true, "Graphics",
                    "On a window wider than 16:9 (resolution), show more to the sides instead of "
                    "black bars: a wider field of view and the picture stretched across the "
                    "window");
REXCVAR_DEFINE_BOOL(ultrawide_hud, true, "Graphics",
                    "With ultrawide_fov on a wider window, the HUD and the menus at the window's "
                    "edges in their own proportions instead of stretched");

namespace {

// True while a movie plays with the presenter's bars turned on for it; the aspect setting it
// changed for that, to put back.
std::atomic<bool> g_movie_bars{false};
std::atomic<bool> g_movie_actual_aspect{true};

constexpr double kGameAspect = 16.0 / 9.0;

// The window's aspect ratio from `resolution`, or 0 if it isn't set.
double WindowAspect() {
  int32_t width = 0, height = 0;
  if (!rex::graphics::video_mode_util::TryParseResolutionPreset(REXCVAR_GET(resolution), width,
                                                                height) ||
      width <= 0 || height <= 0) {
    return 0;
  }
  return double(width) / double(height);
}

}  // namespace

namespace superchargersrecomp {

// Whether the picture should fill a wider window (the presenter then must not letterbox).
bool UltrawideActive() {
  return REXCVAR_GET(ultrawide_fov) && WindowAspect() > kGameAspect * 1.01;
}


void MovieOpened(uint32_t width, uint32_t height) {
  // Only when the picture fills the wider window (the app turned the bars off for ultrawide).
  if (!UltrawideActive() || REXCVAR_GET(present_letterbox) || g_movie_bars.exchange(true)) return;
  // The presenter takes the frame's aspect from the window's size (present_use_actual_aspect),
  // so the 16:9 frame counts as wide as the window: off for the movie, the console's 16:9 video
  // mode then gives the aspect and present_letterbox the bars.
  g_movie_actual_aspect = rex::cvar::Query<bool>("present_use_actual_aspect");
  rex::cvar::SetFlagByName("present_use_actual_aspect", "false");
  rex::cvar::SetFlagByName("present_letterbox", "true");
  REXLOG_INFO("Ultrawide: movie {}x{} with bars at the sides", width, height);
}

void MovieClosed() {
  if (!g_movie_bars.exchange(false)) return;
  rex::cvar::SetFlagByName("present_letterbox", "false");
  rex::cvar::SetFlagByName("present_use_actual_aspect", g_movie_actual_aspect ? "true" : "false");
}

}  // namespace superchargersrecomp

// At 0x82B0BCA0 (bl sub_8250E610): f1 = horizontal FOV (radians; negative when the
// vertical one is given), f2 = vertical FOV, f3 = aspect.
void UltrawideFovHook(PPCRegister& f1, PPCRegister& f2, PPCRegister& f3) {
  static std::atomic<bool> logged{false};
  if (!logged.exchange(true)) {
    REXLOG_INFO("Ultrawide: camera FOV h {} v {}, aspect {}", f1.f64, f2.f64, f3.f64);
  }
  if (!superchargersrecomp::UltrawideActive() || f3.f64 <= 0) return;
  const double wide = WindowAspect();
  if (wide <= f3.f64) return;
  const double scale = wide / f3.f64;
  if (f1.f64 >= 0) {
    // Same vertical FOV: tan(h'/2) = tan(h/2) * scale.
    f1.f64 = double(float(2.0 * std::atan(std::tan(f1.f64 * 0.5) * scale)));
  }
  f3.f64 = double(float(wide));
}

// At 0x827CBBC4 (lfs f0,16(r11) in sub_827CBB80): r11 = a Juice context, its expected screen size at
// +0x28 / +0x2C.
void UltrawideHudHook(PPCRegister& r11) {
  if (!r11.u32) return;
  uint8_t* context = rex::system::kernel_state()->memory()->virtual_membase() + r11.u32;
  constexpr uint32_t kWidth = 0x28, kHeight = 0x2C;  // _expectedScreenWidth / Height
  auto load = [](const uint8_t* p) {
    const uint32_t bits = rex::memory::load_and_swap<uint32_t>(p);
    float v;
    std::memcpy(&v, &bits, 4);
    return v;
  };
  const float width = load(context + kWidth), height = load(context + kHeight);
  if (height <= 0) return;
  // The width written here, so that any other value counts as the game's own.
  static std::atomic<uint32_t> written_bits{0};
  uint32_t width_bits;
  std::memcpy(&width_bits, &width, 4);
  const bool ours = width_bits == written_bits.load();
  const bool wide = REXCVAR_GET(ultrawide_hud) && superchargersrecomp::UltrawideActive();
  float want = width;
  if (wide && !ours) {
    want = float(double(height) * WindowAspect());
  } else if (!wide && ours) {
    want = float(double(height) * 16.0 / 9.0);  // the game's own: 16:9 at its height
  }
  if (want == width) return;
  uint32_t bits;
  std::memcpy(&bits, &want, 4);
  rex::memory::store_and_swap<uint32_t>(context + kWidth, bits);
  if (wide) {
    written_bits = bits;
    static std::atomic<bool> logged{false};
    if (!logged.exchange(true)) REXLOG_INFO("Ultrawide: UI {} x {} -> {} wide", width, height, want);
  }
}

// At the start of BinkClose (sub_831C9F38): the movie is over (or skipped).
void BinkCloseHook() {
  superchargersrecomp::MovieClosed();
}
