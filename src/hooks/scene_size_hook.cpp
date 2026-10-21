// Draws the 3D scene at another size than the Xbox 360's 1040x620 (experimental).
//
// The same render pipeline and the same code as SwapForceRecomp's (its scene_size_hook.cpp has the
// whole story of how this was found): the pipeline is data (renderer/standard_renderer.igz
// in LooseFiles.pak), every render target is an igRenderTarget, and only the platforms' copies of
// "mainScene" have a size of their own: 1040x620 for the 360 (also 1920x1080 and 1024x576 for the
// other consoles); the other targets follow it through their parent and their width / height
// scale. The igRenderTarget's own _width / _height are at +0x20 / +0x24 as in Swap Force (read
// from standard_renderer.igz); the computed size moved by 4, to +0x5C / +0x60.
//
// The four places, found 2026-10-21 in a dump of this game's code (the input script's `save`) by
// the instructions of Swap Force's, each the same instruction sequence around it:
// - sub_826E8718 computes a target's size (r3 = the igRenderTarget): the only function reading
//   +0x20 and +0x24 with the scales +0x28 / +0x2C nearby. The hook at its start puts scene_size
//   into a target that says 1040x620, so the whole pipeline follows.
// - 0x826EDC64 follows lis r5,0x20: r5 = 0x200000 is the frame's EDRAM planning heap, 2048 tiles of
//   1024 bytes. With the SDK's 4096-tile EDRAM (edram_tile_count) it gets that many tiles.
// - 0x82600D20 is cmpwi cr6,r5,0x800: a render pass's tile count against the 360's 2048, above
//   which the game cuts the screen into predicated-tiling strips the SDK doesn't emulate well
//   (black scene). A pass that fits into the larger EDRAM goes untiled.
// - 0x82601910 follows subfic r9,r11,0x800, the fallback depth surface's EDRAM base (2048 minus
//   its tiles); with the larger EDRAM it goes to the top of that.
//
// A scene_size larger than the game's turns edram_tile_count 4096 on by itself
// (superchargersrecomp_app.h). As in Swap Force, a configured 1280x720 draws 1264x704: at exactly
// the screen's size Swap Force crashed in about half of its save loads, presumably because the
// game shares textures of one size between its scene and screen targets.

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <string>

#include <rex/cvar.h>
#include <rex/graphics/edram_layout.h>
#include <rex/logging.h>
#include <rex/memory.h>
#include <rex/memory/utils.h>
#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>

REXCVAR_DEFINE_STRING(scene_size, "", "Graphics",
                      "EXPERIMENTAL: draw the 3D scene at this size instead of the Xbox 360's "
                      "1040x620, e.g. 1280x720. Needs the larger emulated EDRAM, which it turns on "
                      "(edram_tile_count 4096, D3D12 and resolution_scale up to 5 only; otherwise "
                      "the game's size stays). Empty: the game's size")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);

namespace {

constexpr uint32_t kWidthOffset = 0x20;
constexpr uint32_t kHeightOffset = 0x24;
constexpr int32_t kGameWidth = 1040;
constexpr int32_t kGameHeight = 620;
// The game's screen (back buffer and HUD) size, which the scene must not match exactly.
constexpr int32_t kScreenWidth = 1280;
constexpr int32_t kScreenHeight = 720;
constexpr int32_t kScreenSizeStep = 16;

struct Size {
  int32_t width = 0, height = 0;
};

Size ConfiguredSize() {
  Size size;
  const std::string text = REXCVAR_GET(scene_size);
  unsigned w = 0, h = 0;
  if (!text.empty() && std::sscanf(text.c_str(), "%ux%u", &w, &h) == 2 && w >= 320 && h >= 240 &&
      w <= 7680 && h <= 4320) {
    size = {int32_t(w), int32_t(h)};
    if (size.width == kScreenWidth && size.height == kScreenHeight) {
      size = {kScreenWidth - kScreenSizeStep, kScreenHeight - kScreenSizeStep};
      REXLOG_INFO("Scene size: {}x{} is the screen's size, which crashed Swap Force while "
                  "loading; drawing {}x{} instead",
                  kScreenWidth, kScreenHeight, size.width, size.height);
    }
  } else if (!text.empty()) {
    REXLOG_WARN("Scene size: '{}' is not WIDTHxHEIGHT; keeping the game's size", text);
  }
  return size;
}

}  // namespace

// At 0x826EDC64 (li r4,0x400): r5 = 0x200000 is the size of the frame's EDRAM planning heap,
// 2048 tiles of 1024 bytes, passed to its init call two instructions on. With the larger EDRAM it
// gets that many tiles.
void EdramHeapSizeHook(PPCRegister& r5) {
  const uint32_t tiles = rex::graphics::ActiveEdramLayout().tile_count;
  if (tiles <= 2048 || r5.u32 != 2048 * 1024) return;
  r5.u64 = uint64_t(tiles) * 1024;
  static std::atomic<bool> logged{false};
  if (!logged.exchange(true)) REXLOG_INFO("Scene size: EDRAM planning heap of {} tiles", tiles);
}

// At 0x82600D20 (cmpwi cr6,r5,0x800): r5 is the pass's tile count. Between 2048 and the larger
// EDRAM it becomes 2048, so the comparison takes the "fits" path (r5 isn't read on that path; the
// strip size it stores is then the whole surface).
void EdramTilingHook(PPCRegister& r5) {
  const uint32_t tiles = rex::graphics::ActiveEdramLayout().tile_count;
  if (tiles <= 2048 || r5.u32 <= 2048 || r5.u32 > tiles) return;
  static std::atomic<int> logged{0};
  if (logged.fetch_add(1) < 4) {
    REXLOG_INFO("Scene size: a pass of {} tiles fits into the {}-tile EDRAM, no tiling", r5.u32,
                tiles);
  }
  r5.u64 = 2048;
}

// At 0x82601910 (cmplwi cr6,r10,0xe10): r9 = 2048 - r11 is the depth surface's EDRAM base, r11 its
// tile count; stw r9,0x80(r23) stores it next. With a 4096-tile EDRAM in use the depth goes to the
// top of that instead. Only r9 changes, and only when the EDRAM is the larger one.
void EdramDepthBaseHook(PPCRegister& r9, PPCRegister& r11) {
  const uint32_t tiles = rex::graphics::ActiveEdramLayout().tile_count;
  if (tiles <= 2048 || r11.u32 > tiles) return;
  r9.u64 = tiles - r11.u32;
  static std::atomic<int> logged{0};
  if (logged.fetch_add(1) < 4) {
    REXLOG_INFO("Scene size: depth surface of {} tiles at EDRAM tile {} of {}", r11.u32, r9.u32,
                tiles);
  }
}

void SceneSizeHook(PPCRegister& r3) {
  static const Size size = ConfiguredSize();
  if (!size.width) return;
  // Without the larger EDRAM (Vulkan, or a resolution scale area above 25) a larger scene doesn't
  // fit and would stay black; then the game keeps its own size.
  if (size.width * size.height > kGameWidth * kGameHeight &&
      !rex::graphics::ActiveEdramLayout().is_extended()) {
    static std::atomic<bool> warned{false};
    if (!warned.exchange(true)) {
      REXLOG_WARN("Scene size: {}x{} needs the 4096-tile EDRAM (D3D12, resolution scale up to 5); "
                  "keeping 1040x620",
                  size.width, size.height);
    }
    return;
  }
  uint8_t* target = rex::system::kernel_state()->memory()->virtual_membase() + r3.u32;
  if (rex::memory::load_and_swap<int32_t>(target + kWidthOffset) != kGameWidth ||
      rex::memory::load_and_swap<int32_t>(target + kHeightOffset) != kGameHeight) {
    return;
  }
  rex::memory::store_and_swap<int32_t>(target + kWidthOffset, size.width);
  rex::memory::store_and_swap<int32_t>(target + kHeightOffset, size.height);
  static std::atomic<int> logged{0};
  if (logged.fetch_add(1) < 4) {
    REXLOG_INFO("Scene size: render target {:08X} 1040x620 -> {}x{}", r3.u32, size.width,
                size.height);
  }
}
