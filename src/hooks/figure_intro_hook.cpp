// A shorter figure intro when a Skylander comes onto the portal (Swap Force #60, option, off by
// default), as Swap Force's "Skylander-Vorstellung: Kurz".
//
// When a figure is placed, the game's magic moment (the object at 0xB5A6A5AC in the scripted runs)
// shows it with its name for about 3-4 s before you can play. Two checks end the intro:
//
// - sub_829D7B78 checks every frame whether the intro's time is up: it takes the intro's length in
//   f30 (the magic moment's tuning at +92, +236, or +260 for the figure type at +360 == 12, unless
//   the figure's own intro data has one at +136), compares the time since the intro began (the
//   counter at +208 times the constant at 0x82002C40) against it at 0x829D7CB0 (fcmpu cr6,f0,f30)
//   and, when it is longer, fires an event on the figure (sub_82E23778) and marks the time done
//   (+617). That alone does not end the intro (the game went on at the same time, Swap Force #60).
// - sub_82A135B8, the magic moment's update while the intro plays (its state at +48 is 6), ends it
//   when sub_82E08770 says the figure's intro animation is over: then sub_82A0EF38 moves on
//   (sub_82A0B2E0 sets the state to 8, the figure goes into the level). It asks at 0x82A137D8 for
//   one player and at 0x82A136D4 for each figure with several players.
//
// With figure_intro_seconds set, the first hook shortens the length the time check compares
// against, and the second one answers "over" for the animation once the intro has run that long,
// so the game's own ending runs from there, camera and HUD included.
//
// Found 2026-10-22: the time check as the function shaped like Swap Force's intro check
// (sub_829BFFF8 there); the end with a write watch on the magic moment's state (+48) during a
// scripted placement, whose writers were sub_82A12CD0 (6, the intro begins) and sub_82A0B2E0 (8)
// from sub_82A0EF38, called by sub_82A135B8 once the animation check is true. Checked in the first
// level with Chop Chop placed at the prompt, a screenshot every second: at 0 the tunnel ride
// starts about 6 s after the placement, with 1.5 s about 3 s after it, the name shown briefly, the
// camera and the HUD as usual.

#include <cstdint>

#include <rex/cvar.h>
#include <rex/memory/utils.h>
#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>

REXCVAR_DEFINE_DOUBLE(figure_intro_seconds, 0.0, "Game",
                      "How long a Skylander's intro may last when it comes onto the portal, in "
                      "seconds (0: as long as the game's, about 3-4 s)");

namespace {

constexpr uint32_t kIntroCounter = 208;        // in the magic moment; -1 before the intro
constexpr uint32_t kCounterStep = 0x82002C40;  // seconds per count, as sub_829D7B78 reads it

}  // namespace

// Before 0x829D7CB0 (fcmpu cr6,f0,f30): f30 is the intro's length. Only f30 changes, and only to a
// shorter length; f30 is saved by the function and not read after the comparison.
void FigureIntroLengthHook(PPCRegister& f30) {
  const double limit = REXCVAR_GET(figure_intro_seconds);
  if (limit > 0.0 && f30.f64 > limit) f30.f64 = limit;
}

// After the call to sub_82E08770 (0x82A137DC and 0x82A136D8, clrlwi r9,r3,24 next): r3 its answer,
// the intro animation over in the low byte; r30 the magic moment. Only a "not over" becomes
// "over", and only once the intro has run figure_intro_seconds.
void FigureIntroEndHook(PPCRegister& r3, PPCRegister& r30) {
  const double limit = REXCVAR_GET(figure_intro_seconds);
  if (limit <= 0.0 || (r3.u32 & 0xFF) || !r30.u32) return;
  uint8_t* base = rex::system::kernel_state()->memory()->virtual_membase();
  const uint32_t counter = rex::memory::load_and_swap<uint32_t>(base + r30.u32 + kIntroCounter);
  if (counter == 0xFFFFFFFF) return;
  const float step = rex::memory::load_and_swap<float>(base + kCounterStep);
  if (double(float(counter) * step) >= limit) r3.u64 = 1;
}
