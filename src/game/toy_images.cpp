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
// 88x88 is all the disc has of most figures (their HUD portraits, ui/HealthHud_2015/Portraits,
// are the same size; the bios' full-screen pictures their toy data names, <Name>_FullScreen_
// materials, are not on the Xbox 360 disc). Some have a sharper copy: the collection tracker's
// CollectibleTrackerIcons.pak holds 128x128 ones (the same pictures) of 20 of this game's own characters
// (counted 2026-10-21), "textures/ColorMap,textures@!ui!Collections!ToyInventoryIcons!<path>`tga,
// 111.igz" for a material's "GuiStandard_diffuse,...!ToyInventoryIcons!<path>`tga,101", and those
// are taken instead. The menu shows an icon about twice as large as 88 (more at 4K), where the
// GPU's bilinear filter makes it soft. So the first time the menu asks for an icon it is enlarged
// 2x with a Lanczos-3 filter (sharper edges, on premultiplied alpha so the round edge stays clean)
// and kept that way: 120 to 260 KB per icon on screen instead of 70 MB for all of them at start.
// A variant without an entry of its own (most chase variants) shows its toy's default icon.
//
// A vehicle's terrain (SetFigureTraitProvider): CVehicleToyData +0x30, 1 land, 2 sky, 3 sea (the
// game's TemplateLandVehicle / TemplateAirVehicle / TemplateSeaVehicle toy data have 1 / 2 / 3;
// read 2026-10-21 from all 36 vehicles' files). Its symbol is the collection screen's,
// "textures/GuiStandard_diffuse,textures@!ui!Characters_Collection!Elements_Collection!
// CharactersCollection_Vehicle<Land|Flight|Swim>`tga,101.igz" in AP_MenuSelection_Handling.pak
// (88x88, gold rim, stored upside down like the others), and its name the game's word for it
// ("Landfahrzeug", "Seefahrzeug", "Himmelsfahrzeug" in systemData's German text). The owner asked
// for such symbols after Swap Force's SWAP skills (2026-10-21).
//
// The German names of the magic items and traps (SetFigureNameProvider): next to a toy data file
// the game keeps its texts, ToyData/<Name>_ToyData_<language>.lng (game_data.h's ReadText), the
// toy's "CToyData::_toyName" by its toy object's name and each variant's
// "CVariantIdentifier::_toyName" by the variant's. Read 2026-10-23 from all 125 of them: the
// German disc translates only the magic items' and traps' ("Kampfhammer", "Gezeiten-Tiki",
// "Goldene Drachenfeuer-Kanone"); the Skylanders, vehicles and their variants keep their
// English names there ("Hot Streak", "Legendary Bash"), which the catalog already has. So only a
// toy with a German name of its own gets names, from the _de file (the En/De disc) or else the _es
// one (the German text on the German disc, as toy_upgrades.cpp's). The variant numbers of these
// files match the catalog's (all 64 traps, the Golden Dragonfire Cannon, the Legendary Hand of
// Fate).
// A catalog variant without an entry of its own is the plain toy when the catalog calls it what it
// calls the toy's lowest variant (the Dragonfire Cannon is 0x1206), and keeps what the catalog adds
// to that ("Sunscraper Spire (LightCore)" -> "Sonnenkratzer (LightCore)"); the others keep the
// catalog's more specific name ("Platinum Treasure Chest").

#include "toy_images.h"

#include "controller_picture.h"
#include "game_data.h"
#include "hats.h"
#include "toy_upgrades.h"

#include <algorithm>
#include <cctype>
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

#include "alchemy/iga_archive.h"
#include "alchemy/igz_file.h"
#include "portal/figure_catalog.h"
#include "portal_rex/menu_draw.h"
#include "portal_rex/portal_menu.h"

namespace superchargersrecomp {

// The games' shared Alchemy readers (skylanders-portal src/alchemy).
namespace alchemy = skylanders::alchemy;
namespace {

using alchemy::IgaArchive;
using alchemy::IgzFile;
using Clock = std::chrono::steady_clock;
using skylanders::FigureImage;

constexpr char kToyDataArchive[] = "permanent.pak";
constexpr char kMaterialArchive[] = "ToyCollectionMaterials.pak";
constexpr std::string_view kToyDataDir = "/ToyData/";
constexpr std::string_view kToyDataSuffix = "_ToyData.igz";
constexpr std::string_view kToyTextSuffixes[] = {"_ToyData_de.lng", "_ToyData_es.lng"};
constexpr char kToyNameField[] = "CToyData::_toyName";
constexpr char kVariantNameField[] = "CVariantIdentifier::_toyName";
constexpr std::string_view kMaterialDir = "/materialInstances/ToyCollection/";
constexpr std::string_view kTextureDir = "/textures/";
constexpr std::string_view kCollectionPrefix = "Collection_";
constexpr char kImageNamespace[] = "image";
constexpr char kTrackerArchive[] = "CollectibleTrackerIcons.pak";
constexpr std::string_view kIconPath = "textures@!ui!Collections!ToyInventoryIcons!";
constexpr std::string_view kSharpSuffix = "`tga,111.igz";

// The menu's symbols (SetArtImageProvider): the HUD's element badges,
// "textures/GuiStandard_diffuse,textures@!ui!HealthHud_2015!Portraits!Elements!<Element>_HUD`tga,
// 101.igz" in permanent.pak (88x88 DXT5, all ten elements, the same medallions as the collection
// screen's), and the ability shop's coin, "...textures@!ui!AbilityShop_2015!coin_icon`tga,101.igz"
// in juicedomain_permanent.pak (44x44). Stored upside down like the icons (seen 2026-10-21: the
// flame and the leaves pointing down), enlarged like them when first asked for.
constexpr char kElementArchive[] = "permanent.pak";
constexpr std::string_view kElementTexture = "textures@!ui!HealthHud_2015!Portraits!Elements!";
constexpr std::string_view kElementSuffix = "_HUD`tga,101.igz";
constexpr char kCoinArchive[] = "juicedomain_permanent.pak";
constexpr std::string_view kCoinTexture = "textures@!ui!AbilityShop_2015!coin_icon`tga,101.igz";
// The menu's element names (portal_menu.h's "element:<name>" keys).
constexpr std::string_view kElements[] = {"magic", "earth", "water", "fire",  "tech",
                                          "undead", "life", "air",  "light", "dark"};

constexpr uint32_t kToyIdField = 0x08;
constexpr uint32_t kToyMaterialField = 0x20;
constexpr uint32_t kDecoField = 0x08;
constexpr uint32_t kYearField = 0x0C;
constexpr uint32_t kFlagsField = 0x10;
constexpr uint32_t kVariantMaterialField = 0x1C;
constexpr uint32_t kVehicleTerrainField = 0x30;  // CVehicleToyData
constexpr char kTerrainArchive[] = "AP_MenuSelection_Handling.pak";
constexpr std::string_view kTerrainTexture =
    "textures@!ui!Characters_Collection!Elements_Collection!CharactersCollection_Vehicle";

constexpr uint16_t kSeries2 = 0x800;
constexpr uint16_t kAlternateDeco = 0x400;
constexpr uint16_t kLightCore = 0x200;
constexpr uint16_t kDecoIdMask = 0x1FF;

struct Toy {
  std::string material;                                   // the default collection material
  std::unordered_map<uint16_t, std::string> variants;     // catalog variant -> material
  std::string character;                                  // "ChopChop" (its toy data file's name)
  uint32_t terrain = 0;                                   // vehicles: 1 land, 2 sky, 3 sea
  std::string name_de;                                    // its German name, if translated
  std::unordered_map<uint16_t, std::string> variant_names_de;  // every variant's, maybe empty
};

std::mutex g_mutex;
std::unordered_map<uint16_t, Toy> g_toys;                                    // by toy id
std::unordered_map<std::string, std::shared_ptr<const FigureImage>> g_icons;  // by material
std::unordered_map<uint32_t, std::string> g_names_de;  // by id << 16 | catalog variant
bool g_ready = false;

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
constexpr int kEnlargeBelow = 256;
constexpr int kEnlargeFactor = 2;

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

// The first picture of the archive's texture file `index`, upright (stored upside down); nullptr
// if it can't be read.
std::shared_ptr<FigureImage> ReadPicture(const IgaArchive& archive, size_t index) {
  auto pixels = archive.Read(index);
  if (!pixels) return nullptr;
  auto tex = IgzFile::Parse(std::move(*pixels));
  auto rgba = tex ? tex->DecodeFirstImage() : std::nullopt;
  if (!rgba) return nullptr;
  auto image = std::make_shared<FigureImage>();
  image->width = rgba->width;
  image->height = rgba->height;
  image->rgba = std::move(rgba->pixels);
  FlipVertically(*image);
  return image;
}

// The German text next to a toy data file (`name` ends with kToyDataSuffix), empty if none.
GameText ToyText(const IgaArchive& archive, const std::unordered_map<std::string, size_t>& files,
                 const std::string& name) {
  const std::string base = name.substr(0, name.size() - kToyDataSuffix.size());
  for (std::string_view suffix : kToyTextSuffixes) {
    auto it = files.find(base + std::string(suffix));
    if (it == files.end()) continue;
    auto data = archive.Read(it->second);
    if (auto lng = data ? IgzFile::Parse(std::move(*data)) : std::nullopt) return ReadText(*lng);
  }
  return {};
}

void IndexToyData(const IgaArchive& archive, ToyMap& toys) {
  std::unordered_map<std::string, size_t> files;
  for (size_t i = 0; i < archive.size(); ++i) files.emplace(archive.name(i), i);
  for (size_t i = 0; i < archive.size(); ++i) {
    const std::string& name = archive.name(i);
    if (name.find(kToyDataDir) == std::string::npos || !EndsWith(name, kToyDataSuffix)) continue;
    auto data = archive.Read(i);
    if (!data) continue;
    auto igz = IgzFile::Parse(std::move(*data));
    if (!igz) continue;
    const GameText text = ToyText(archive, files, name);
    const auto object_names = ObjectNames(*igz);
    auto object_name = [&](size_t object) {
      auto it = object_names.find(object);
      return it == object_names.end() ? std::string() : it->second;
    };
    // The file's variants belong to its toys; a file with two (a Skylander and its Legendary)
    // gives both the same list, and the catalog's numbers pick the right one.
    std::unordered_map<uint16_t, std::string> variants, variant_names;
    for (const auto& [object, type] : igz->Objects("CVariantIdentifier")) {
      const uint16_t number = VariantNumber(*igz, object);
      variant_names.emplace(number, text.Get(object_name(object), kVariantNameField));
      std::string material = CollectionMaterial(*igz, igz->Field(object, kVariantMaterialField));
      if (!material.empty()) variants.emplace(number, std::move(material));
    }
    for (const auto& [object, type] : igz->Objects("ToyData")) {
      const uint32_t id = igz->Field(object, kToyIdField);
      std::string material = CollectionMaterial(*igz, igz->Field(object, kToyMaterialField));
      std::string name_de = text.Get(object_name(object), kToyNameField);
      // A toy without a collection icon can still have a name (the UFO Hat).
      if (id > 0xFFFF || (material.empty() && name_de.empty())) continue;
      Toy& toy = toys[uint16_t(id)];
      toy.name_de = std::move(name_de);
      toy.variant_names_de = variant_names;
      if (material.empty()) continue;
      toy.material = std::move(material);
      const size_t file = name.rfind('/') + 1;
      toy.character = name.substr(file, name.size() - file - kToyDataSuffix.size());
      if (type == "CVehicleToyData") toy.terrain = igz->Field(object, kVehicleTerrainField);
      for (const auto& [variant, m] : variants) toy.variants.emplace(variant, m);
    }
  }
}

// The German names by catalog figure (see the top of this file), for the toys that have one.
std::unordered_map<uint32_t, std::string> GermanNames(const ToyMap& toys) {
  using skylanders::portal::AllSkylanders;
  std::unordered_map<uint16_t, const skylanders::portal::SkylanderInfo*> lowest;
  for (const auto& sky : AllSkylanders()) {
    auto& low = lowest[sky.id];
    if (!low || sky.variant < low->variant) low = &sky;
  }
  std::unordered_map<uint32_t, std::string> names;
  for (const auto& sky : AllSkylanders()) {
    auto toy = toys.find(sky.id);
    if (toy == toys.end() || toy->second.name_de.empty()) continue;
    const uint32_t key = uint32_t(sky.id) << 16 | sky.variant;
    if (auto v = toy->second.variant_names_de.find(sky.variant); v != toy->second.variant_names_de.end()) {
      if (!v->second.empty()) names.emplace(key, v->second);
    } else if (sky.name.starts_with(lowest[sky.id]->name)) {
      names.emplace(key, toy->second.name_de + std::string(sky.name.substr(lowest[sky.id]->name.size())));
    }
  }
  return names;
}

// The tracker's sharper icons by their path under ToyInventoryIcons ("2015Characters!AirLock").
using SharpMap = std::unordered_map<std::string, size_t>;
SharpMap IndexSharpIcons(const IgaArchive& archive) {
  SharpMap sharp;
  for (size_t i = 0; i < archive.size(); ++i) {
    const std::string& name = archive.name(i);
    const size_t at = name.find(kIconPath);
    if (at == std::string::npos || !EndsWith(name, kSharpSuffix)) continue;
    sharp.emplace(name.substr(at + kIconPath.size(), name.size() - at - kIconPath.size() - kSharpSuffix.size()),
                  i);
  }
  return sharp;
}

// The sharper copy of a material's texture ("GuiStandard_diffuse,textures@!ui!Collections!
// ToyInventoryIcons!2015Characters!AirLock`tga,101"), or nullptr.
std::shared_ptr<FigureImage> SharpIcon(const IgaArchive* tracker, const SharpMap& sharp,
                                       const std::string& texture) {
  const size_t at = texture.find(kIconPath);
  const size_t end = texture.rfind('`');
  if (!tracker || at == std::string::npos || end == std::string::npos || end < at) return nullptr;
  auto it = sharp.find(texture.substr(at + kIconPath.size(), end - at - kIconPath.size()));
  return it == sharp.end() ? nullptr : ReadPicture(*tracker, it->second);
}

void IndexIcons(const IgaArchive& archive, IconMap& icons, const IgaArchive* tracker,
                const SharpMap& sharp) {
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
      auto image = SharpIcon(tracker, sharp, ext->first);
      if (!image) image = ReadPicture(archive, texture->second);
      if (image) icons[material] = std::move(image);
      break;
    }
  }
}

// The menu's symbols in `archive`: the element badges (kElementTexture + <Element> +
// kElementSuffix) as "element:<element>" and the coin (kCoinTexture) as "ui:gold". Returns how
// many it read.
int IndexArt(const IgaArchive& archive, IconMap& icons) {
  int count = 0;
  for (size_t i = 0; i < archive.size(); ++i) {
    const std::string& name = archive.name(i);
    std::string key;
    if (EndsWith(name, kCoinTexture)) {
      key = "ui:gold";
    } else if (const size_t at = name.find(kTerrainTexture); at != std::string::npos) {
      const std::string kind = name.substr(at + kTerrainTexture.size());
      if (kind.rfind("Land`", 0) == 0) key = "vehicle:land";
      else if (kind.rfind("Flight`", 0) == 0) key = "vehicle:sky";
      else if (kind.rfind("Swim`", 0) == 0) key = "vehicle:sea";
      else continue;
    } else if (const size_t at = name.find(kElementTexture);
               at != std::string::npos && EndsWith(name, kElementSuffix)) {
      std::string element = name.substr(at + kElementTexture.size(),
                                        name.size() - at - kElementTexture.size() - kElementSuffix.size());
      std::transform(element.begin(), element.end(), element.begin(),
                     [](unsigned char c) { return char(std::tolower(c)); });
      if (std::find(std::begin(kElements), std::end(kElements), element) == std::end(kElements)) continue;
      key = "element:" + element;
    } else {
      continue;
    }
    if (icons.contains(key)) continue;
    if (auto image = ReadPicture(archive, i)) {
      icons[key] = std::move(image);
      ++count;
    }
  }
  return count;
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
  auto tracker = IgaArchive::Open(rom / "archives" / kTrackerArchive);
  const SharpMap sharp = tracker ? IndexSharpIcons(*tracker) : SharpMap{};
  IndexIcons(*materials, icons, tracker ? &*tracker : nullptr, sharp);
  int symbols = 0;
  for (const char* name : {kElementArchive, kCoinArchive, kTerrainArchive}) {
    if (auto art = IgaArchive::Open(rom / "archives" / name)) symbols += IndexArt(*art, icons);
  }
  if (auto controls = IgaArchive::Open(rom / "archives" / "Controller_SchemeXenon.pak")) {
    if (auto picture = ReadControllerPicture(*controls)) {
      icons[kControllerArtKey] = std::move(picture);
      ++symbols;
    }
  }
  size_t variants = 0;
  for (const auto& [id, toy] : toys) variants += toy.variants.size();
  auto names_de = GermanNames(toys);
  const size_t toy_count = toys.size(), icon_count = icons.size(), name_count = names_de.size();
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_toys = std::move(toys);
    g_icons = std::move(icons);
    g_names_de = std::move(names_de);
    g_ready = true;
  }
  REXLOG_INFO("Toy images: {} toys with {} variants, {} icons and {} symbols, {} German names, in {} ms",
              toy_count, variants, icon_count - symbols, symbols, name_count,
              std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count());
}

// The first time an icon is asked for, it is enlarged (see the top of this file). g_mutex held.
std::shared_ptr<const FigureImage> Icon(const std::string& material) {
  auto it = g_icons.find(material);
  if (it == g_icons.end()) return nullptr;
  if (it->second->width < kEnlargeBelow) it->second = Enlarge(*it->second, kEnlargeFactor);
  return it->second;
}

// A vehicle's terrain as the menu's trait, or nothing for the other toys.
std::optional<skylanders::FigureTrait> Terrain(uint16_t id) {
  uint32_t terrain = 0;
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_ready) return std::nullopt;
    auto it = g_toys.find(id);
    if (it == g_toys.end()) return std::nullopt;
    terrain = it->second.terrain;
  }
  const bool german = skylanders::menu_draw::GermanText();
  skylanders::FigureTrait trait;
  switch (terrain) {
    case 1: trait = {"vehicle:land", german ? "Landfahrzeug" : "Land Vehicle"}; break;
    case 2: trait = {"vehicle:sky", german ? "Himmelsfahrzeug" : "Sky Vehicle"}; break;
    case 3: trait = {"vehicle:sea", german ? "Seefahrzeug" : "Sea Vehicle"}; break;
    default: return std::nullopt;
  }
  return trait;
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

std::optional<std::string> GermanName(uint16_t id, uint16_t variant) {
  if (!skylanders::menu_draw::GermanText()) return std::nullopt;
  std::lock_guard<std::mutex> lock(g_mutex);
  auto it = g_names_de.find(uint32_t(id) << 16 | variant);
  if (!g_ready || it == g_names_de.end()) return std::nullopt;
  return it->second;
}

}  // namespace

std::string ToyCharacter(uint16_t id) {
  std::lock_guard<std::mutex> lock(g_mutex);
  auto it = g_toys.find(id);
  return it == g_toys.end() ? std::string() : it->second.character;
}

void InstallToyImages(const std::filesystem::path& rom_dir) {
  std::thread(Load, rom_dir).detach();
  skylanders::SetFigureImageProvider(
      [](uint16_t id, uint16_t variant) { return Lookup(id, variant); });
  skylanders::SetFigureTraitProvider([](uint16_t id, uint16_t) { return Terrain(id); });
  skylanders::SetFigureNameProvider(GermanName);
  skylanders::SetArtImageProvider([](const std::string& key) -> std::shared_ptr<const FigureImage> {
    if (auto hat = HatPicture(key)) return hat;  // "hat:<number>" (hats.cpp)
    if (key.starts_with("upgrade:")) return UpgradeImage(key);  // toy_upgrades.cpp
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_ready ? Icon(key) : nullptr;
  });
}

}  // namespace superchargersrecomp
