// Strength of the game's bloom and motion blur, as in Swap Force (SwapForceRecomp's
// post_effects_hook.cpp; Cemu's Wii U packs offer the same).
//
// SuperChargers keeps its bloom in the level's environment settings, one object per zone with
// some 200 reflection fields (sun, sky, clouds, shadows, ambient, bloom, fog, vignette, colour
// grading ...; names at 0x820D3D50, offsets at 0x820D4420): _bloomLuminanceCutoff at +0x3AC and
// _bloomScale, how much glow is added, at +0x3B0. sub_82BD0C30 blends the zones into the object
// at +2480 of its `this` each time it runs, every field from the sources afresh. The hook at its
// last instruction before the exit (config/default.toml, 0x82BD389C) scales the blended
// _bloomScale; the early exit (nothing to blend) skips it, so the scale never compounds.
//
// Found 2026-10-22: the field tables from the reflection strings (_bloomScale), their offsets,
// then the loads of +0x3B0 in the generated code; the blend function writes it three times from
// the zones (a first lerp from two sources, then two lerps towards further ones). Checked on the
// title screen (scale 1, cutoff 5 there, the same each call, so nothing compounds): at 0 the glow
// around the logo is gone, at 3 it is much stronger.
//
// Motion blur, as Swap Force's option: SuperChargers' environment class lost its motion blur
// exposure in development (its upgrade tag "mbukowski-2015-06-16-RemoveMotionBlurExposure"), so
// the strength comes from the motion blur material itself. CMotionBlurMaterial (vtable
// 0x82284F48) keeps ig_motion_blur_exposure_params at +96, as loaded with the material (0.125,
// -0.125 for the one the levels draw with; the tile size at +112, the largest blur, 7 px, at
// +128). sub_8360C7E8 hands the three to the shader each time the material is bound (found from
// the parameter names, 0x82288D88: their handles are looked up by name once, sub_836148F0, and only
// this function sets them). A write watch saw no writes to +96 once a level runs, so the material
// keeps the value it was loaded with; the hook at the setter's start scales it in place, from the
// game's value: a value other than the one it wrote last is the game's (a material loaded later, or
// a new one at the same address), so nothing compounds. Checked 2026-10-22 on the tunnel ride with
// the throttle held (the runs differ a little in timing): the gear bits visibly streaked at 2 and
// at 4; in one set of runs the picture's mean gradient was 1.53 / 1.51 at 0, 1.45 / 1.39 at the
// game's 1 and 1.42 / 1.32 at 3.

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <unordered_map>

#include <rex/cvar.h>
#include <rex/memory/utils.h>
#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>

REXCVAR_DEFINE_DOUBLE(bloom_strength, 1.0, "Graphics",
                      "Glow around bright areas: 0 = off, 1 = the game's, up to 3");
// Off by default, as in Swap Force (the owner's choice there, 2026-10-19): the blur is tuned for
// 30 FPS and mostly smears at higher frame rates.
REXCVAR_DEFINE_DOUBLE(motion_blur_strength, 0.0, "Graphics",
                      "Motion blur: 0 = off (default), 1 = the game's, up to 2");

namespace {

constexpr uint32_t kBlended = 2480;     // the blended environment object in sub_82BD0C30's this
constexpr uint32_t kBloomScale = 0x3B0;  // its _bloomScale

}  // namespace

// 0x82BD389C: r31 = sub_82BD0C30's this; the blended object's fields are written.
void BloomStrengthHook(PPCRegister& r31) {
  const double scale = std::clamp(REXCVAR_GET(bloom_strength), 0.0, 3.0);
  if (scale == 1.0) return;
  uint8_t* base = rex::system::kernel_state()->memory()->virtual_membase();
  const uint32_t blended = rex::memory::load_and_swap<uint32_t>(base + r31.u32 + kBlended);
  if (!blended) return;
  uint8_t* p = base + blended + kBloomScale;
  rex::memory::store_and_swap<float>(p, float(rex::memory::load_and_swap<float>(p) * scale));
}

// 0x82BD439C: the join after sub_82BD38A8 blended one zone's _bloomScale into the result
// (r3 = its this, r4 = the zone). sub_82BD38A8 is the other blend: it moves each field of the result
// towards a zone's own value, result = result + (zone - result) * w, for the fields the zone sets
// (_blendAll at +111 or the field's state, +53 for _bloomScale), w at +2520 of this.
// Started from the scaled result, that leaves the zone's share unscaled; adding
// w * (scale - 1) * zone gives result + (scale * zone - result) * w, the scaled blend, whichever
// of the two functions ran before.
void BloomStrengthZoneHook(PPCRegister& r3, PPCRegister& r4) {
  const double scale = std::clamp(REXCVAR_GET(bloom_strength), 0.0, 3.0);
  if (scale == 1.0 || !r3.u32 || !r4.u32) return;
  uint8_t* base = rex::system::kernel_state()->memory()->virtual_membase();
  if (!base[r4.u32 + 111] && !base[r4.u32 + 53]) return;
  const uint32_t result = rex::memory::load_and_swap<uint32_t>(base + r3.u32 + 2480);
  if (!result) return;
  const double w = rex::memory::load_and_swap<float>(base + r3.u32 + 2520);
  const double zone = rex::memory::load_and_swap<float>(base + r4.u32 + 0x3B0);
  uint8_t* p = base + result + 0x3B0;
  const double now = rex::memory::load_and_swap<float>(p);
  rex::memory::store_and_swap<float>(p, float(now + w * (scale - 1.0) * zone));
}

// 0x8360C7E8, the start of the motion blur material's parameter setter: r3 = the material. Its
// exposure parameter (+96, +100) becomes the game's value times motion_blur_strength.
void MotionBlurStrengthHook(PPCRegister& r3) {
  if (!r3.u32) return;
  struct Seen {
    float game[2];
    float written[2];
  };
  static std::mutex mutex;
  static std::unordered_map<uint32_t, Seen> seen;  // by material
  const float scale = float(std::clamp(REXCVAR_GET(motion_blur_strength), 0.0, 2.0));
  uint8_t* p = rex::system::kernel_state()->memory()->virtual_membase() + r3.u32 + 96;
  std::lock_guard lock(mutex);
  auto [it, fresh] = seen.try_emplace(r3.u32);
  Seen& s = it->second;
  for (int i = 0; i < 2; ++i) {
    const float now = rex::memory::load_and_swap<float>(p + 4 * i);
    if (fresh || now != s.written[i]) s.game[i] = now;
    s.written[i] = s.game[i] * scale;
    rex::memory::store_and_swap<float>(p + 4 * i, s.written[i]);
  }
}
