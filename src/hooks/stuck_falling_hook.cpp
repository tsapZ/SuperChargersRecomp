// The game's "stuck while falling" check at any frame rate, as in Swap Force (SwapForceRecomp
// src/hooks/stuck_falling_hook.cpp, the same check there).
//
// CPlayerJumpComponent's update, sub_82F96310, calls sub_82F949B8 every frame: a falling player
// (state +0x1C == 3, not in hang time +0xDC) who moved less than CPlayerSystemData's
// _stuckFallingDistanceThreshold (+0x54) times the component's +0x74 since the last frame runs the
// component's stuck timer (_stuckFallingTimer, a timer object at +0x98), and once it ran longer
// than _stuckFallingTime (+0x58) the game puts the player back (sub_82F91540 with 5). Moving more
// stops the timer. The threshold is a distance per frame, so it was tuned for 30 FPS: this game
// runs at 60 by default and faster with frame_rate_limit, where more of a fall counts as stuck and
// a long drop can end with the player put back, as Swap Force showed at 240 FPS.
//
// Here the threshold is scaled by the game's own frame time (30 * dt, at most 1), so it means the
// same speed as at 30 FPS at any frame rate. dt is the update context's +0x14: sub_82F96310 keeps
// the context (its third argument) in r26, which sub_82F949B8 doesn't touch, and passes it to the
// component's other per-frame functions, which read their dt there (sub_82F62260: lfs f23,20(r30)).
//
// Found 2026-10-23: the field names and offsets from the game's reflection tables
// (CPlayerSystemData's names at 0x82089008, offsets at 0x820890B8; CPlayerJumpComponent's at
// 0x820889B0 / 0x82088B10, each followed by the fields' types and then their u16 offsets), and
// sub_82F949B8 as the one function reading the system data's +0x54 and +0x58; it has the shape of
// Swap Force's sub_82A4A828.
// Measured 2026-10-23 with log_stuck_falling in the first level after the rift ride, the player
// launched with the component's bounce pad fields (+0x38 on, +0x44 up speed, +0x48 / +0x4C gravity
// 1): without the fix, at the default 60 FPS and at about 130, launches at 5000 and 8000 counted as
// stuck for over 3 s and ended with the player put back (normal jumps 0.42 s); with it at about
// 110-130 FPS never (normal jumps 0.23-0.29 s, a launch at 5000 0.27 s).

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/memory/utils.h>
#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>

REXCVAR_DEFINE_BOOL(fix_stuck_falling, true, "Game",
                    "Above 30 FPS, keep the game from taking a normal fall for being stuck (its "
                    "check counts the distance per frame, tuned for 30 FPS)");
REXCVAR_DEFINE_BOOL(log_stuck_falling, false, "Game",
                    "Log how long each fall counted as stuck (for testing fix_stuck_falling)");

namespace {

using Clock = std::chrono::steady_clock;

float LoadFloat(uint32_t guest_address) {
  return rex::memory::load_and_swap<float>(
      rex::system::kernel_state()->memory()->virtual_membase() + guest_address);
}

// log_stuck_falling: per component, the current run of checks that counted as stuck.
struct Run {
  uint32_t component = 0;
  Clock::time_point since{};
  Clock::time_point last{};
  bool stuck = false;
};

std::mutex g_runs_mutex;
std::array<Run, 8> g_runs;

Run& RunFor(uint32_t component) {
  for (auto& r : g_runs) {
    if (r.component == component) return r;
  }
  for (auto& r : g_runs) {
    if (!r.stuck) {
      r = {component};
      return r;
    }
  }
  g_runs[0] = {component};
  return g_runs[0];
}

void EndRun(Run& r) {
  if (!r.stuck) return;
  r.stuck = false;
  const double seconds = std::chrono::duration<double>(r.last - r.since).count();
  if (seconds >= 0.05) {
    REXLOG_INFO("Stuck falling: counted as stuck for {:.2f} s (component {:08X})", seconds,
                r.component);
  }
}

}  // namespace

// Before 0x82F94A40 (fmuls f11,f13,f12): f12 = _stuckFallingDistanceThreshold, f13 = the
// component's factor, f0 = the squared distance since the last frame, r26 = the caller's update
// context (dt at +0x14), r30 = CPlayerSystemData, r31 = the component.
void StuckFallingThresholdHook(PPCRegister& f0, PPCRegister& f12, PPCRegister& f13,
                               PPCRegister& r26, PPCRegister& r30, PPCRegister& r31) {
  const float dt = LoadFloat(r26.u32 + 0x14);
  const double scale = dt > 0.0f && dt < 1.0f ? std::min(1.0, double(dt) * 30.0) : 1.0;
  static std::atomic<bool> logged{false};
  if (!logged.exchange(true)) {
    REXLOG_INFO("Stuck falling: threshold {} per frame, factor {}, time {} s, dt {:.4f}, frame "
                "scale {:.3f} (component {:08X})", f12.f64, f13.f64, LoadFloat(r30.u32 + 0x58), dt,
                scale, r31.u32);
  }
  if (REXCVAR_GET(fix_stuck_falling)) f12.f64 = double(float(f12.f64 * scale));
  if (REXCVAR_GET(log_stuck_falling)) {
    std::lock_guard lock(g_runs_mutex);
    Run& run = RunFor(r31.u32);
    const auto now = Clock::now();
    // A gap means the checks in between were not falls (the game skips this part then).
    if (run.stuck && now - run.last > std::chrono::milliseconds(100)) EndRun(run);
    const double limit = f12.f64 * f13.f64;
    if (f0.f64 < limit * limit) {
      if (!run.stuck) {
        run.stuck = true;
        run.since = now;
      }
      run.last = now;
    } else {
      EndRun(run);
    }
  }
}

// Before 0x82F94A78: the stuck timer ran past _stuckFallingTime (f31; f1 = the timer's time) and
// the game puts the player back.
void StuckFallingRespawnHook(PPCRegister& f1, PPCRegister& f31) {
  REXLOG_INFO("Stuck falling: player put back after {:.2f} s (limit {:.2f} s)", f1.f64, f31.f64);
}
