// Pictures of the toys for the portal menu: the round icons of the game's collection screen.
//
// The chain, all inside the owner's game files (rom/archives), nothing of it in the repository:
//   1. permanent.pak holds one toy data file per toy, ToyData/<Name>_ToyData.igz (IGZ version 9,
//      alchemy/igz_file.h). Its toy objects (CFullCharacterToyData, CVehicleToyData,
//      CBattlePieceToyData, ... every class ending in "ToyData") have the toy id at +0x08, the
//      same as the portal catalog's (Chop Chop 30, Legendary Chop Chop 430, Warnado 2), and at
//      +0x20 a handle to the toy's collection material ("Collection_ChopChop_Normal"). Its
//      CVariantIdentifier objects are the variants: +0x08 deco id, +0x0C year (0 is Spyro's
//      Adventure), +0x10 four flag bytes (LightCore, alternate deco, Series 2, -), +0x14 the
//      variant's name ("Eon's Elite"), +0x1C a handle to its collection material
//      ("Collection_ChopChop_Elite"). A handle is 0x80000000 | an index into the file's external
//      names (EXNM). Together they make the catalog's variant number:
//        variant = year << 12 | Series 2 << 11 | alternate deco << 10 | LightCore << 9 | deco id
//      Read 2026-10-21 from Chop Chop's, Warnado's, the Earth Tank's, Count Moneybone's vehicle's
//      and the Dragonfire Cannon's files: Eon's Elite Chop Chop is year 3, Series 2, deco 16 as in
//      the catalog; LightCore Warnado year 2, LightCore, deco 6.
//   2. ToyCollectionMaterials.pak holds the material instances,
//      materialInstances/ToyCollection/<Material>.igz; the external name in the "image" namespace
//      is the texture ("GuiStandard_diffuse,textures@!ui!Collections!...!Collections_ChopChop`tga,
//      101"), stored in the same archive as textures/<that name>.igz. Its igImage2 is the icon,
//      88x88 DXT5, stored upside down (as Trap Team's portraits; first shown upside down in the
//      menu, seen by the owner 2026-10-21), so it is flipped when it is read.
// 88x88 is all the disc has of a figure (its HUD portraits, ui/HealthHud_2015/Portraits, are the
// same size), and the menu shows it about twice as large (more at 4K), where the GPU's bilinear
// filter makes it soft. So the first time the menu asks for an icon it is enlarged 2x with a
// Lanczos-3 filter (sharper edges, on premultiplied alpha so the round edge stays clean) and kept
// that way: about 120 KB per icon on screen instead of 70 MB for all of them at start.
// A variant without an entry of its own (most chase variants) shows its toy's default icon.

#include "toy_images.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include <rex/logging.h>

#include "game/alchemy/iga_archive.h"
#include "game/alchemy/igz_file.h"
#include "portal_rex/portal_menu.h"

namespace superchargersrecomp {
namespace {

using alchemy::IgaArchive;
using alchemy::IgzFile;
using Clock = std::chrono::steady_clock;
using skylanders::FigureImage;

constexpr char kToyDataArchive[] = "permanent.pak";
constexpr char kMaterialArchive[] = "ToyCollectionMaterials.pak";
constexpr std::string_view kToyDataDir = "/ToyData/";
constexpr std::string_view kToyDataSuffix = "_ToyData.igz";
constexpr std::string_view kMaterialDir = "/materialInstances/ToyCollection/";
constexpr std::string_view kTextureDir = "/textures/";
constexpr std::string_view kCollectionPrefix = "Collection_";
constexpr char kImageNamespace[] = "image";

constexpr uint32_t kToyIdField = 0x08;
constexpr uint32_t kToyMaterialField = 0x20;
constexpr uint32_t kDecoField = 0x08;
constexpr uint32_t kYearField = 0x0C;
constexpr uint32_t kFlagsField = 0x10;
constexpr uint32_t kVariantMaterialField = 0x1C;

constexpr uint16_t kSeries2 = 0x800;
constexpr uint16_t kAlternateDeco = 0x400;
constexpr uint16_t kLightCore = 0x200;
constexpr uint16_t kDecoIdMask = 0x1FF;

struct Toy {
  std::string material;                                   // the default collection material
  std::unordered_map<uint16_t, std::string> variants;     // catalog variant -> material
};

std::mutex g_mutex;
std::unordered_map<uint16_t, Toy> g_toys;                                    // by toy id
std::unordered_map<std::string, std::shared_ptr<const FigureImage>> g_icons;  // by material
bool g_ready = false;

bool EndsWith(const std::string& s, std::string_view suffix) {
  return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// The collection material a handle names, or empty.
std::string CollectionMaterial(const IgzFile& igz, uint32_t handle) {
  const auto name = igz.ExternalName(handle);
  if (!name || !name->first.starts_with(kCollectionPrefix)) return {};
  return name->first;
}

uint16_t VariantNumber(const IgzFile& igz, size_t object) {
  const uint32_t flags = igz.Field(object, kFlagsField);
  uint16_t variant = uint16_t((igz.Field(object, kYearField) & 0xF) << 12) |
                     uint16_t(igz.Field(object, kDecoField) & kDecoIdMask);
  if (flags & 0xFF000000) variant |= kLightCore;
  if (flags & 0x00FF0000) variant |= kAlternateDeco;
  if (flags & 0x0000FF00) variant |= kSeries2;
  return variant;
}

// Icons smaller than this are enlarged by kEnlargeFactor when the menu first asks for them.
constexpr int kEnlargeBelow = 128;
constexpr int kEnlargeFactor = 2;

void FlipVertically(FigureImage& image) {
  const size_t row = size_t(image.width) * 4;
  for (int y = 0; y < image.height / 2; ++y) {
    std::swap_ranges(image.rgba.begin() + y * row, image.rgba.begin() + (y + 1) * row,
                     image.rgba.begin() + (image.height - 1 - y) * row);
  }
}

double Lanczos3(double x) {
  constexpr double kPi = 3.14159265358979323846;
  x = std::abs(x);
  if (x < 1e-8) return 1.0;
  if (x >= 3.0) return 0.0;
  const double px = kPi * x;
  return 3.0 * std::sin(px) * std::sin(px / 3.0) / (px * px);
}

// One direction of a separable resample: `count` lines of `in_size` RGBA float pixels, `stride`
// apart in floats (4 for a row, 4 * width for a column), to `out_size` pixels each.
void ResampleLines(const std::vector<float>& in, std::vector<float>& out, int lines, int in_size,
                   int out_size, size_t in_line, size_t in_step, size_t out_line, size_t out_step) {
  const double scale = double(in_size) / out_size;
  for (int o = 0; o < out_size; ++o) {
    const double center = (o + 0.5) * scale - 0.5;
    const int first = int(std::floor(center)) - 2;
    double weights[6], total = 0;
    for (int k = 0; k < 6; ++k) total += weights[k] = Lanczos3(center - (first + k));
    for (int line = 0; line < lines; ++line) {
      float sum[4] = {0, 0, 0, 0};
      for (int k = 0; k < 6; ++k) {
        const int i = std::clamp(first + k, 0, in_size - 1);
        const float* px = &in[line * in_line + i * in_step];
        for (int c = 0; c < 4; ++c) sum[c] += float(weights[k] / total) * px[c];
      }
      float* to = &out[line * out_line + o * out_step];
      for (int c = 0; c < 4; ++c) to[c] = sum[c];
    }
  }
}

std::shared_ptr<const FigureImage> Enlarge(const FigureImage& image, int factor) {
  const int w = image.width, h = image.height, ow = w * factor, oh = h * factor;
  std::vector<float> pre(size_t(w) * h * 4);
  for (size_t i = 0; i < size_t(w) * h; ++i) {
    const float a = image.rgba[4 * i + 3] / 255.0f;
    for (int c = 0; c < 3; ++c) pre[4 * i + c] = image.rgba[4 * i + c] / 255.0f * a;
    pre[4 * i + 3] = a;
  }
  std::vector<float> wide(size_t(ow) * h * 4), tall(size_t(ow) * oh * 4);
  ResampleLines(pre, wide, h, w, ow, size_t(w) * 4, 4, size_t(ow) * 4, 4);       // rows
  ResampleLines(wide, tall, ow, h, oh, 4, size_t(ow) * 4, 4, size_t(ow) * 4);    // columns
  auto out = std::make_shared<FigureImage>();
  out->width = ow;
  out->height = oh;
  out->rgba.resize(tall.size());
  for (size_t i = 0; i < size_t(ow) * oh; ++i) {
    const float a = std::clamp(tall[4 * i + 3], 0.0f, 1.0f);
    for (int c = 0; c < 3; ++c) {
      const float v = a > 1e-4f ? tall[4 * i + c] / a : 0.0f;
      out->rgba[4 * i + c] = uint8_t(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f));
    }
    out->rgba[4 * i + 3] = uint8_t(std::lround(a * 255.0f));
  }
  return out;
}

using ToyMap = std::unordered_map<uint16_t, Toy>;
using IconMap = std::unordered_map<std::string, std::shared_ptr<const FigureImage>>;

void IndexToyData(const IgaArchive& archive, ToyMap& toys) {
  for (size_t i = 0; i < archive.size(); ++i) {
    const std::string& name = archive.name(i);
    if (name.find(kToyDataDir) == std::string::npos || !EndsWith(name, kToyDataSuffix)) continue;
    auto data = archive.Read(i);
    if (!data) continue;
    auto igz = IgzFile::Parse(std::move(*data));
    if (!igz) continue;
    // The file's variants belong to its toys; a file with two (a Skylander and its Legendary)
    // gives both the same list, and the catalog's numbers pick the right one.
    std::unordered_map<uint16_t, std::string> variants;
    for (const auto& [object, type] : igz->Objects("CVariantIdentifier")) {
      std::string material = CollectionMaterial(*igz, igz->Field(object, kVariantMaterialField));
      if (!material.empty()) variants.emplace(VariantNumber(*igz, object), std::move(material));
    }
    for (const auto& [object, type] : igz->Objects("ToyData")) {
      const uint32_t id = igz->Field(object, kToyIdField);
      std::string material = CollectionMaterial(*igz, igz->Field(object, kToyMaterialField));
      if (id > 0xFFFF || material.empty()) continue;
      Toy& toy = toys[uint16_t(id)];
      toy.material = std::move(material);
      for (const auto& [variant, m] : variants) toy.variants.emplace(variant, m);
    }
  }
}

void IndexIcons(const IgaArchive& archive, IconMap& icons) {
  // The textures by file name, then each material's.
  std::unordered_map<std::string, size_t> textures;
  for (size_t i = 0; i < archive.size(); ++i) {
    const std::string& name = archive.name(i);
    const size_t at = name.find(kTextureDir);
    if (at != std::string::npos && EndsWith(name, ".igz")) {
      textures.emplace(name.substr(at + kTextureDir.size(), name.size() - at - kTextureDir.size() - 4),
                       i);
    }
  }
  for (size_t i = 0; i < archive.size(); ++i) {
    const std::string& name = archive.name(i);
    const size_t at = name.find(kMaterialDir);
    if (at == std::string::npos || !EndsWith(name, ".igz")) continue;
    const std::string material =
        name.substr(at + kMaterialDir.size(), name.size() - at - kMaterialDir.size() - 4);
    auto data = archive.Read(i);
    if (!data) continue;
    auto igz = IgzFile::Parse(std::move(*data));
    if (!igz) continue;
    for (uint32_t index = 0; index < 64; ++index) {
      const auto ext = igz->ExternalName(0x80000000u | index);
      if (!ext) break;
      if (ext->second != kImageNamespace) continue;
      auto texture = textures.find(ext->first);
      if (texture == textures.end()) break;
      auto pixels = archive.Read(texture->second);
      if (!pixels) break;
      auto tex = IgzFile::Parse(std::move(*pixels));
      auto rgba = tex ? tex->DecodeFirstImage() : std::nullopt;
      if (!rgba) break;
      auto image = std::make_shared<FigureImage>();
      image->width = rgba->width;
      image->height = rgba->height;
      image->rgba = std::move(rgba->pixels);
      FlipVertically(*image);  // stored upside down
      icons[material] = std::move(image);
      break;
    }
  }
}

void Load(std::filesystem::path rom) {
  const auto start = Clock::now();
  auto toydata = IgaArchive::Open(rom / "archives" / kToyDataArchive);
  auto materials = IgaArchive::Open(rom / "archives" / kMaterialArchive);
  if (!toydata || !materials) {
    REXLOG_WARN("Toy images: {} or {} not readable", kToyDataArchive, kMaterialArchive);
    return;
  }
  ToyMap toys;
  IconMap icons;
  IndexToyData(*toydata, toys);
  IndexIcons(*materials, icons);
  size_t variants = 0;
  for (const auto& [id, toy] : toys) variants += toy.variants.size();
  const size_t toy_count = toys.size(), icon_count = icons.size();
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_toys = std::move(toys);
    g_icons = std::move(icons);
    g_ready = true;
  }
  REXLOG_INFO("Toy images: {} toys with {} variants, {} icons, in {} ms", toy_count, variants,
              icon_count,
              std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count());
}

// The first time an icon is asked for, it is enlarged (see the top of this file). g_mutex held.
std::shared_ptr<const FigureImage> Icon(const std::string& material) {
  auto it = g_icons.find(material);
  if (it == g_icons.end()) return nullptr;
  if (it->second->width < kEnlargeBelow) it->second = Enlarge(*it->second, kEnlargeFactor);
  return it->second;
}

std::shared_ptr<const FigureImage> Lookup(uint16_t id, uint16_t variant) {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (!g_ready) return nullptr;
  auto it = g_toys.find(id);
  if (it == g_toys.end()) return nullptr;
  const Toy& toy = it->second;
  if (auto v = toy.variants.find(variant); v != toy.variants.end()) {
    if (auto icon = Icon(v->second)) return icon;
  }
  return Icon(toy.material);
}

}  // namespace

void InstallToyImages(const std::filesystem::path& rom_dir) {
  std::thread(Load, rom_dir).detach();
  skylanders::SetFigureImageProvider(
      [](uint16_t id, uint16_t variant) { return Lookup(id, variant); });
}

}  // namespace superchargersrecomp
