// Strength of the game's bloom, as in Swap Force (SwapForceRecomp's post_effects_hook.cpp; Cemu's
// Wii U packs offer the same).
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
// Swap Force's motion blur option has no counterpart here yet: SuperChargers' environment class
// lost its motion blur exposure in development (its upgrade tag
// "mbukowski-2015-06-16-RemoveMotionBlurExposure"), and _enableMotionBlur is a water material's.

#include <algorithm>
#include <cstdint>

#include <rex/cvar.h>
#include <rex/memory/utils.h>
#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>

REXCVAR_DEFINE_DOUBLE(bloom_strength, 1.0, "Graphics",
                      "Glow around bright areas: 0 = off, 1 = the game's, up to 3");

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
