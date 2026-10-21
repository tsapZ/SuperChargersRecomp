// The game's controller picture for the settings menu's controls page, as Swap Force's
// (SwapForceRecomp src/game/game_art.cpp, CleanController).
//
// Controller_SchemeXenon.pak (the game's Controls screen on the Xbox 360) has
// "textures/GuiStandard_diffuse,textures@!ui!Controllers!Xbox_Controller`tga,101.igz": 1024x724
// DXT5, stored upside down like the other UI pictures, the controller in the middle with callout
// lines from its buttons to the screen's labels and a second column of face buttons at the right.
// SuperChargers and Imaginators have the same drawing at the same place (their opaque pixels
// overlap 97 %, measured 2026-10-22), only the lines differ: teal lines, about (107, 162, 140).
// To clean it: the line pixels (IsCallout below) and two pixels around them are painted over
// from their neighbours, layer by layer from the outside in; then only the opaque part joined to
// the middle is kept (the label column's face buttons go) and the picture is cut to kCrop. The
// buttons' places were measured on the picture (the colours of the face buttons, the dark blue of
// the sticks, the d-pad and the guide button) and are given in the full picture's pixels.

#include "controller_picture.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "game/alchemy/igz_file.h"

namespace superchargersrecomp {

const char kControllerArtKey[] = "controller:xbox360";

namespace {

using alchemy::IgaArchive;
using alchemy::IgzFile;
using skylanders::FigureImage;

constexpr std::string_view kTexture =
    "/textures/GuiStandard_diffuse,textures@!ui!Controllers!Xbox_Controller`tga,101.igz";
constexpr int kWidth = 1024, kHeight = 724;
// The controller's box in the picture, with a few pixels to spare.
constexpr int kCropX = 304, kCropY = 234, kCropW = 350, kCropH = 284;
constexpr int kLineGrow = 2;      // pixels painted over around each line pixel
constexpr int kFillPasses = 64;   // enough for the widest line

// This disc's lines are teal, about (107, 162, 140), 3 to 4 pixels wide.
bool IsCallout(const uint8_t* p, bool /*near_x_button*/) {
  const int r = p[0], g = p[1], b = p[2];
  return p[3] > 20 && std::abs(r - 107) < 40 && std::abs(g - 162) < 40 && std::abs(b - 140) < 40 &&
         g - r > 25;
}

// Grows `mask` by one pixel (4-neighbourhood) `n` times.
void Grow(std::vector<uint8_t>& mask, int w, int h, int n) {
  std::vector<uint8_t> next(mask.size());
  for (int k = 0; k < n; ++k) {
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        const size_t i = size_t(y) * w + x;
        next[i] = mask[i] || (x > 0 && mask[i - 1]) || (x + 1 < w && mask[i + 1]) || (y > 0 && mask[i - w]) ||
                  (y + 1 < h && mask[i + w]);
      }
    }
    mask.swap(next);
  }
}

std::shared_ptr<FigureImage> Clean(const FigureImage& in) {
  const int w = in.width, h = in.height;
  if (w != kWidth || h != kHeight || in.rgba.size() < size_t(w) * h * 4) return nullptr;
  // The X button's blue, so its letter's edges don't count as lines.
  std::vector<uint8_t> x_button(size_t(w) * h);
  for (size_t i = 0; i < x_button.size(); ++i) {
    const uint8_t* p = &in.rgba[i * 4];
    x_button[i] = p[3] > 200 && p[0] < 80 && p[2] - p[0] > 150;
  }
  Grow(x_button, w, h, 7);
  std::vector<uint8_t> unknown(size_t(w) * h);
  for (size_t i = 0; i < unknown.size(); ++i) unknown[i] = IsCallout(&in.rgba[i * 4], x_button[i] != 0);
  Grow(unknown, w, h, kLineGrow);
  // Paint over: every pass fills the unknown pixels next to known ones with their average.
  std::vector<float> px(in.rgba.begin(), in.rgba.end());
  for (int pass = 0; pass < kFillPasses; ++pass) {
    std::vector<std::pair<size_t, std::array<float, 4>>> filled;
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        const size_t i = size_t(y) * w + x;
        if (!unknown[i]) continue;
        std::array<float, 4> sum{};
        int count = 0;
        for (int dy = -1; dy <= 1; ++dy) {
          for (int dx = -1; dx <= 1; ++dx) {
            const int nx = x + dx, ny = y + dy;
            if ((dx || dy) && nx >= 0 && ny >= 0 && nx < w && ny < h && !unknown[size_t(ny) * w + nx]) {
              for (int c = 0; c < 4; ++c) sum[c] += px[(size_t(ny) * w + nx) * 4 + c];
              ++count;
            }
          }
        }
        if (count) {
          for (float& v : sum) v /= float(count);
          filled.emplace_back(i, sum);
        }
      }
    }
    if (filled.empty()) break;
    for (const auto& [i, value] : filled) {
      std::copy(value.begin(), value.end(), &px[i * 4]);
      unknown[i] = 0;
    }
  }
  // Keep the opaque part joined to the middle (the guide button), with two pixels of its edge.
  std::vector<uint8_t> opaque(size_t(w) * h), keep(size_t(w) * h);
  for (size_t i = 0; i < opaque.size(); ++i) opaque[i] = px[i * 4 + 3] > 100;
  std::vector<std::pair<int, int>> stack = {{478, 379}};
  while (!stack.empty()) {
    const auto [x, y] = stack.back();
    stack.pop_back();
    if (x < 0 || y < 0 || x >= w || y >= h) continue;
    const size_t i = size_t(y) * w + x;
    if (keep[i] || !opaque[i]) continue;
    keep[i] = 1;
    stack.insert(stack.end(), {{x + 1, y}, {x - 1, y}, {x, y + 1}, {x, y - 1}});
  }
  Grow(keep, w, h, 2);
  auto out = std::make_shared<FigureImage>();
  out->width = kCropW;
  out->height = kCropH;
  out->rgba.resize(size_t(kCropW) * kCropH * 4);
  for (int y = 0; y < kCropH; ++y) {
    for (int x = 0; x < kCropW; ++x) {
      const size_t i = size_t(kCropY + y) * w + kCropX + x;
      uint8_t* o = &out->rgba[(size_t(y) * kCropW + x) * 4];
      for (int c = 0; c < 4; ++c) o[c] = uint8_t(std::clamp(std::lround(px[i * 4 + c]), 0L, 255L));
      if (!keep[i]) o[3] = 0;
    }
  }
  return out;
}

}  // namespace

std::shared_ptr<FigureImage> ReadControllerPicture(const IgaArchive& archive) {
  for (size_t i = 0; i < archive.size(); ++i) {
    const std::string& name = archive.name(i);
    if (name.size() < kTexture.size() || name.compare(name.size() - kTexture.size(), kTexture.size(), kTexture) != 0) {
      continue;
    }
    auto data = archive.Read(i);
    auto igz = data ? IgzFile::Parse(std::move(*data)) : std::nullopt;
    auto rgba = igz ? igz->DecodeFirstImage() : std::nullopt;
    if (!rgba) return nullptr;
    FigureImage image;
    image.width = rgba->width;
    image.height = rgba->height;
    image.rgba = std::move(rgba->pixels);
    // Stored upside down.
    const size_t row = size_t(image.width) * 4;
    for (int y = 0; y < image.height / 2; ++y) {
      std::swap_ranges(image.rgba.begin() + y * row, image.rgba.begin() + (y + 1) * row,
                       image.rgba.begin() + (image.height - 1 - y) * row);
    }
    return Clean(image);
  }
  return nullptr;
}

skylanders::ControllerPicture ControllerPictureLayout() {
  // In the full picture's pixels (see the top).
  static const skylanders::ControllerSpot kSpots[] = {
      {"guide", 478, 379, 22}, {"back", 435, 380, 9},  {"start", 520, 380, 9},  {"y", 595, 343, 12},
      {"x", 570, 384, 12},     {"b", 620, 377, 12},    {"a", 595, 417, 12},     {"ls", 362, 386, 25},
      {"rs", 532, 456, 25},    {"up", 432, 436, 9},    {"down", 432, 474, 9},   {"left", 413, 455, 9},
      {"right", 451, 455, 9},  {"lb", 385, 258, 15},   {"rb", 565, 258, 15},
  };
  skylanders::ControllerPicture picture;
  picture.art_key = kControllerArtKey;
  picture.width = kCropW;
  picture.height = kCropH;
  for (skylanders::ControllerSpot spot : kSpots) {
    spot.x -= kCropX;
    spot.y -= kCropY;
    picture.spots.push_back(spot);
  }
  return picture;
}

}  // namespace superchargersrecomp
