// The hats for the portal menu, all from the owner's game files (rom/archives), as Swap Force's
// and the earlier games' menus show them: the hat's name in the game's language, its picture and
// what it adds to the wearer's stats. Read 2026-10-22 (the owner saw "Hut Nr. 12" in the menu):
//
// - permanent.pak LooseData/CollectibleTrackers.igz has one CHatCollectibleTracker per hat (266),
//   named after the hat ("Hat_CowboyHat", the file's igNameList, in the order of its root
//   igObjectList). Its number is the same as a figure stores and as Swap Force's
//   kTfbSpyroTag_Hat_* values (Cowboy Hat 22, Viking Helmet 12, Pan Hat 33); its stats are a
//   CCharacterAttributeList of CCharacterAttribute (+0x08 the attribute, +0x0C the amount as a
//   float); its icon a handle to a material. Found by SuperChargers' Cowboy Hat: icon handle at +0x0C, attribute list at +0x80, hat 22 at +0x84.
// - The names are only in the language files next to it (CollectibleTrackers_<language>.lng: an
//   igLocalizedStringData per text, +0x08 the text, +0x0C the object and +0x10 the field, each an
//   index into the file's external names, objects and "metafield" fields counted separately;
//   "CHatCollectibleTracker::_hatName"). The German menu takes the _de file (the European
//   English / German disc), else the _es one, which holds the German text on the Spanish /
//   Italian disc after tools/german_text.py; the English menu the _en file, and where the disc has
//   none (the Es/It disc) makes a name from the hat's ("Hat_CowboyHat" -> "Cowboy Hat").
// - The attributes' names: JuiceDomain_FrontEnd.pak LooseData/systemData.igz, its
//   CCharacterAttributeStringHashTable (attribute -> English name: 3 "Armor", 0x24 "Elemental
//   Power"; the numbers differ between the games), translated by systemData_es.lng's
//   igLocalizedHashTable for "CSystemData::_characterAttributeNames", whose keys are the FNV-1a
//   hashes of the English names ("Armor" -> "Rüstung").
// - Pictures: CollectibleTrackerIcons.pak holds the hat icons (hatIcon_<Name>_material(s).igz, 266 of them, made
//   from textures/...!ui!Hats!...), stored upside down like the toys' icons.

#include "hats.h"

#include <algorithm>
#include <bit>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <rex/logging.h>

#include "alchemy/iga_archive.h"
#include "alchemy/igz_file.h"
#include "game/game_data.h"
#include "portal_rex/menu_draw.h"

namespace superchargersrecomp {

// The games' shared Alchemy readers (skylanders-portal src/alchemy).
namespace alchemy = skylanders::alchemy;
namespace {

using alchemy::IgaArchive;
using alchemy::IgzFile;
using skylanders::FigureImage;
using skylanders::HatInfo;

constexpr char kTrackerArchive[] = "permanent.pak";
constexpr std::string_view kTrackerFile = "/LooseData/CollectibleTrackers.igz";
constexpr std::string_view kTrackerText = "/LooseData/CollectibleTrackers_de.lng";
constexpr std::string_view kTrackerTextEs = "/LooseData/CollectibleTrackers_es.lng";
constexpr std::string_view kTrackerTextEn = "/LooseData/CollectibleTrackers_en.lng";
constexpr char kSystemArchive[] = "JuiceDomain_FrontEnd.pak";
constexpr std::string_view kSystemFile = "/LooseData/systemData.igz";
constexpr std::string_view kSystemText = "/LooseData/systemData_de.lng";
constexpr std::string_view kSystemTextEs = "/LooseData/systemData_es.lng";
constexpr char kIconArchive[] = "CollectibleTrackerIcons.pak";
constexpr std::string_view kMaterialDir = "/materialInstances/";
constexpr std::string_view kTextureDir = "/textures/";
constexpr char kImageNamespace[] = "image";
constexpr char kHatNameField[] = "CHatCollectibleTracker::_hatName";
constexpr char kAttributeNamesField[] = "CSystemData::_characterAttributeNames";
constexpr std::string_view kHatPrefix = "Hat_";
constexpr std::string_view kPictureKey = "hat:";

constexpr uint32_t kHatIconField = 0x0C;   // CHatCollectibleTracker: its icon material
constexpr uint32_t kHatStatsField = 0x80;  // CHatCollectibleTracker: its CCharacterAttributeList
constexpr uint32_t kHatIdField = 0x84;     // CHatCollectibleTracker: the hat's number
constexpr uint32_t kAttributeField = 0x08;      // CCharacterAttribute
constexpr uint32_t kAttributeValueField = 0x0C;
constexpr uint32_t kListCountField = 0x08;      // igObjectList and the like
constexpr uint32_t kListDataField = 0x14;
constexpr uint32_t kMaxListEntries = 64;

struct Hat {
  std::string object;                                // "Hat_CowboyHat"
  std::vector<std::pair<uint32_t, float>> stats;     // attribute, amount
  std::shared_ptr<const FigureImage> picture;
};

std::mutex g_mutex;
bool g_ready = false;
std::unordered_map<uint16_t, Hat> g_hats;
std::unordered_map<uint32_t, std::string> g_attribute_names;   // English, by attribute
GameText g_hat_text;                                           // the German texts (_de or _es)
GameText g_hat_text_en;                                        // the English ones (_en), if any
std::unordered_map<uint32_t, std::string> g_attribute_german;  // by the English name's hash
std::unordered_map<uint32_t, std::shared_ptr<const HatInfo>> g_infos;  // hat << 1 | German

// A material's picture: the texture its "image" external names, in the same archive, upright.
std::shared_ptr<const FigureImage> MaterialPicture(const IgaArchive& archive,
                                                   const std::unordered_map<std::string, size_t>& files,
                                                   const std::string& material) {
  auto file = files.find(std::string(kMaterialDir) + material + ".igz");
  if (file == files.end()) return nullptr;
  auto data = archive.Read(file->second);
  auto igz = data ? IgzFile::Parse(std::move(*data)) : std::nullopt;
  if (!igz) return nullptr;
  for (uint32_t index = 0; index < 64; ++index) {
    const auto ext = igz->ExternalName(0x80000000u | index);
    if (!ext) break;
    if (ext->second != kImageNamespace) continue;
    auto texture = files.find(std::string(kTextureDir) + ext->first + ".igz");
    if (texture == files.end()) return nullptr;
    auto pixels = archive.Read(texture->second);
    auto tex = pixels ? IgzFile::Parse(std::move(*pixels)) : std::nullopt;
    auto rgba = tex ? tex->DecodeFirstImage() : std::nullopt;
    if (!rgba) return nullptr;
    auto image = std::make_shared<FigureImage>();
    image->width = rgba->width;
    image->height = rgba->height;
    image->rgba = std::move(rgba->pixels);
    FlipVertically(*image);
    return image;
  }
  return nullptr;
}

// "Hat_CowboyHat" -> "Cowboy Hat", for the English menu where the disc has no English text.
std::string HatName(std::string_view object) {
  if (object.starts_with(kHatPrefix)) object.remove_prefix(kHatPrefix.size());
  return Humanize(object);
}

void Load(std::filesystem::path rom) {
  const auto trackers = IgaArchive::Open(rom / "archives" / kTrackerArchive);
  const auto system = IgaArchive::Open(rom / "archives" / kSystemArchive);
  auto igz = trackers ? ReadFile(*trackers, kTrackerFile) : std::nullopt;
  if (!igz) {
    REXLOG_WARN("Hats: {} has no {}; no hat names", kTrackerArchive, kTrackerFile);
    return;
  }
  const auto names = ObjectNames(*igz);
  const auto icons = IgaArchive::Open(rom / "archives" / kIconArchive);
  std::unordered_map<std::string, size_t> icon_files;
  if (icons) {
    for (size_t i = 0; i < icons->size(); ++i) {
      const std::string& name = icons->name(i);
      for (std::string_view dir : {kMaterialDir, kTextureDir}) {
        if (const size_t at = name.find(dir); at != std::string::npos) icon_files.emplace(name.substr(at), i);
      }
    }
  }
  std::unordered_map<uint16_t, Hat> hats;
  size_t pictures = 0;
  for (const auto& [object, type] : igz->Objects("CHatCollectibleTracker")) {
    const uint32_t id = igz->Field(object, kHatIdField);
    auto name = names.find(object);
    if (id == 0 || id > 0xFFFF || name == names.end()) continue;
    Hat hat;
    hat.object = name->second;
    if (const auto list = Target(*igz, igz->Field(object, kHatStatsField))) {
      const uint32_t count = std::min(igz->Field(*list, kListCountField), kMaxListEntries);
      if (const auto items = Target(*igz, igz->Field(*list, kListDataField))) {
        for (uint32_t k = 0; k < count; ++k) {
          const auto attribute = Target(*igz, igz->Field(*items, 4 * k));
          if (!attribute) continue;
          hat.stats.emplace_back(igz->Field(*attribute, kAttributeField),
                                 std::bit_cast<float>(igz->Field(*attribute, kAttributeValueField)));
        }
      }
    }
    if (const uint32_t handle = igz->Field(object, kHatIconField); icons && (handle & 0x80000000u)) {
      if (const auto material = igz->ExternalName(handle)) {
        hat.picture = MaterialPicture(*icons, icon_files, material->first);
        if (hat.picture) ++pictures;
      }
    }
    hats[uint16_t(id)] = std::move(hat);
  }
  GameText hat_text;
  if (auto lng = ReadFile(*trackers, kTrackerText)) {
    hat_text = ReadText(*lng);
  } else if (auto lng_es = ReadFile(*trackers, kTrackerTextEs)) {
    hat_text = ReadText(*lng_es);
  }
  GameText hat_text_en;
  if (auto lng_en = ReadFile(*trackers, kTrackerTextEn)) hat_text_en = ReadText(*lng_en);
  std::unordered_map<uint32_t, std::string> attribute_names, attribute_german;
  if (system) {
    if (auto sys = ReadFile(*system, kSystemFile)) {
      const auto tables = sys->Objects("CCharacterAttributeStringHashTable");
      if (!tables.empty()) {
        ForEachEntry(*sys, tables.front().first,
                     [&](uint32_t attribute, uint32_t text) { attribute_names[attribute] = sys->StringAt(text); });
      }
    }
    auto lng = ReadFile(*system, kSystemText);
    if (!lng) lng = ReadFile(*system, kSystemTextEs);
    if (lng) {
      GameText text = ReadText(*lng);
      attribute_german = std::move(text.tables[kAttributeNamesField]);
    }
  }
  const size_t hat_count = hats.size(), name_count = hat_text.fields.size(),
               english_count = hat_text_en.fields.size(), attribute_count = attribute_names.size();
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_hats = std::move(hats);
    g_hat_text = std::move(hat_text);
    g_hat_text_en = std::move(hat_text_en);
    g_attribute_names = std::move(attribute_names);
    g_attribute_german = std::move(attribute_german);
    g_infos.clear();
    g_ready = true;
  }
  REXLOG_INFO("Hats: {} hats ({} with a picture), {} texts, {} English texts, {} attribute names",
              hat_count, pictures, name_count, english_count, attribute_count);
}

std::shared_ptr<const HatInfo> InfoFor(uint16_t number) {
  const bool german = skylanders::menu_draw::GermanText();
  std::lock_guard<std::mutex> lock(g_mutex);
  if (!g_ready) return nullptr;
  const uint32_t key = (uint32_t(number) << 1) | (german ? 1u : 0u);
  if (auto it = g_infos.find(key); it != g_infos.end()) return it->second;
  auto hat = g_hats.find(number);
  if (hat == g_hats.end()) return nullptr;
  auto info = std::make_shared<HatInfo>();
  const GameText& texts = german ? g_hat_text : g_hat_text_en;
  auto text = texts.fields.find(hat->second.object + "|" + kHatNameField);
  if (text != texts.fields.end()) info->name = text->second;
  if (info->name.empty()) info->name = HatName(hat->second.object);
  if (hat->second.picture) info->image = std::string(kPictureKey) + std::to_string(number);
  for (const auto& [attribute, amount] : hat->second.stats) {
    skylanders::HatStat stat;
    auto english = g_attribute_names.find(attribute);
    if (english != g_attribute_names.end()) {
      stat.name = english->second;
      if (german) {
        auto translated = g_attribute_german.find(Fnv1a(english->second));
        if (translated != g_attribute_german.end()) stat.name = translated->second;
      }
    }
    stat.value = amount;
    info->stats.push_back(std::move(stat));
  }
  g_infos[key] = info;
  return info;
}

}  // namespace

void InstallHats(const std::filesystem::path& rom_dir) {
  std::thread(Load, rom_dir).detach();
  skylanders::SetHatInfoProvider([](uint16_t hat) { return InfoFor(hat); });
}

std::shared_ptr<const FigureImage> HatPicture(const std::string& key) {
  if (!key.starts_with(kPictureKey)) return nullptr;
  const uint16_t number = uint16_t(std::strtoul(key.c_str() + kPictureKey.size(), nullptr, 10));
  std::lock_guard<std::mutex> lock(g_mutex);
  auto hat = g_hats.find(number);
  return hat == g_hats.end() ? nullptr : hat->second.picture;
}

}  // namespace superchargersrecomp
