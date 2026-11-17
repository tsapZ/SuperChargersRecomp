// Skip talks (F4 > Game): B or Y skips every cutscene, also those the game only lets you skip on a replay (the owner's
// wish 2026-11-16, "die labern zu viel", and 2026-11-17 "mach B einfach fest").
//
// Vicarious Visions' developer switch "auto skip cutscenes": sub_8298E780 answers true when byte +74 of the object
// behind the word 0x83741600 is set (or 0x8373EA5C is set and a timer test passes). Found 2026-11-17 from the scripts'
// native AutoSkipCutscenes_Internal (sub_82D2CF38, by scanning the running game for its name 0x8216F158: the
// registered entry is the name followed by the native), the twin of Imaginators' sub_829AEFF8, whose switch ended a
// running scene within 3 s (im_autoskip_test.ps1). Its callers: the native, the scene update sub_82A10478 (true and the
// scene in state 3, +60: the scene is ended) and sub_82E7C950; the update asks about 120 times a second, in the hubs
// too. With skip_talks on, sub_8298E780 also says yes for 100 ms after a new press of B or Y: that ends a playing scene
// and does nothing without one. Tested 2026-11-17 22:56: nat1's opening, which the game alone doesn't let you skip,
// ended at the first B. Talks (the text box lines after it) go another way and are not skipped by this. The presses
// come from the lib's own collector for this (TakeNewSkipPresses), so the movie skip keeps its presses. Nothing is
// written to the game.

#include <chrono>
#include <cstdint>
#include <mutex>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/ppc/context.h>

#include "portal_rex/portal_menu_input.h"

REXCVAR_DEFINE_BOOL(skip_talks, false, "Game",
                    "B or Y skips every cutscene, also those the game only lets you skip on a replay");

REX_EXTERN(__imp__sub_8298E780);  // the auto skip switch (original)

namespace {

constexpr uint16_t kSkipButtons = 0x2000 | 0x8000;  // X_INPUT_GAMEPAD_B | X_INPUT_GAMEPAD_Y
using Clock = std::chrono::steady_clock;

std::mutex g_mutex;
Clock::time_point g_last_ask{};
Clock::time_point g_skip_until{};

}  // namespace

REX_HOOK_RAW(sub_8298E780) {
  __imp__sub_8298E780(ctx, base);
  if (!REXCVAR_GET(skip_talks)) return;
  const Clock::time_point now = Clock::now();
  std::lock_guard lock(g_mutex);
  // Presses from before a run of asks (a load, a menu) are dropped.
  const bool new_run = now - g_last_ask > std::chrono::milliseconds(250);
  g_last_ask = now;
  const uint16_t presses = skylanders::menu_input::TakeNewSkipPresses();
  if (!new_run && (presses & kSkipButtons) && !skylanders::menu_input::MenuOpen()) {
    g_skip_until = now + std::chrono::milliseconds(100);  // the update asks every frame
    REXLOG_INFO("Skip talks: {} skips the scene", (presses & 0x2000) ? "B" : "Y");
  }
  if (now < g_skip_until) ctx.r3.u64 = 1;
}
