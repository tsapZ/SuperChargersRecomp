// Every Skylander's upgrades for the portal menu's details view, as Swap Force's menu shows them
// (SwapForceRecomp src/game/game_art.cpp: the same CSkillsUpgradeComponentData there in IGX).
// Read 2026-10-22 from King Pen's, Spyro's and Chop Chop's files on both discs (SuperChargers and
// Imaginators store them alike):
//
// - A character's archive, rom/archives/<Name>.pak (<Name> as its toy data file calls it,
//   toy_images.cpp: BladePenguin for King Pen, ChopChop), holds <Name>_CharacterData.igz. Its
//   CSkillsUpgradeComponentData has at +0x14 the starting attacks (a CSkillUpgradeList) and at
//   +0x18 a CSkillsUpgradePathList: the basic upgrades, the two paths, the Soul Gem and, for the
//   earlier games' Skylanders, the Wow Pows. Each CSkillsUpgradePath has its CSkillUpgradeList at
//   +0x10; each CSkillUpgrade +0x10 its price, +0x24 its picture, a handle to a material in
//   <Name>_AbilityUpgrades. A Wow Pow is a CSkillUpgradeWowpow with its year at +0x34 (1 for
//   Giants' Series 2, 2 for Swap Force's Series 3: Spyro's "Fireslam" and "Magical Worm"), so a
//   figure shows its own year's. A handle is the index of a named entry of the file's external
//   names (those whose name word has the top bit set): this file's other entries are type names.
// - The names and descriptions are in <Name>_CharacterData_es.lng next to it (game_data.h:
//   "CSkillUpgrade::_skillName", "CUpgrade::_upgradeDescription", the paths'
//   "igNamedObject::_name" and "CSkillsUpgradePath::_description"; the German text on the German
//   disc). The disc has no English text: the English menu names the upgrades after their objects
//   ("..._0_PenguinDrill" -> "Penguin Drill").
// - Which bit of the figure's upgrade flags an upgrade is, as in Swap Force: the basic upgrades
//   from bit 2, then the two paths sharing the same bits (bit 1 says which path), the Soul Gem
//   after them, the other path's upgrades from bit 11 and the last upgrade, the Wow Pow, bit 10.
//   Imaginators' senseis have their Sky-Chi as the Soul Gem path's second upgrade; it is shown
//   there with bit 10 too.
// - The pictures: <Name>_AbilityUpgrades.pak's materialInstances file has one igGraphicsMaterial
//   per upgrade picture, named like the handle ("EmperorPenguin"), followed by its
//   igGraphicsTexture whose +0x10 is the "image" external, the texture in the same archive
//   ("textures/GuiStandard_diffuse,textures@!ui!AbilityUpgrades!2016!Skylanders!BladePenguin!
//   PNG!EmperorPenguin`tga,101.igz"), stored upside down like the other UI pictures.
// - The texts' button glyphs, as Swap Force's: "~" the first attack (X), "^" the second (Y), "`"
//   the third (B).
// A character is read the first time the menu asks for it, on a worker thread, and its pictures
// when the details view first shows them.

#include "toy_upgrades.h"

#include <algorithm>
#include <cctype>
#include <deque>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <rex/logging.h>

#include "game/alchemy/iga_archive.h"
#include "game/alchemy/igz_file.h"
#include "game/game_data.h"
#include "game/toy_images.h"
#include "portal_rex/menu_draw.h"

namespace superchargersrecomp {
namespace {

using alchemy::IgaArchive;
using alchemy::IgzFile;
using skylanders::FigureImage;
using skylanders::FigureUpgrade;
using skylanders::FigureUpgradePath;
using skylanders::FigureUpgrades;

constexpr std::string_view kKeyPrefix = "upgrade:";
constexpr char kCharacterSuffix[] = "_CharacterData.igz";
constexpr char kTextSuffix[] = "_CharacterData_es.lng";
constexpr char kPicturesSuffix[] = "_AbilityUpgrades.pak";
constexpr std::string_view kMaterialDir = "/materialinstances/";
constexpr char kTextureDir[] = "/textures/";
constexpr char kImageNamespace[] = "image";
constexpr char kNameField[] = "CSkillUpgrade::_skillName";
constexpr char kDescriptionField[] = "CUpgrade::_upgradeDescription";
constexpr char kPathNameField[] = "igNamedObject::_name";
constexpr char kPathDescriptionField[] = "CSkillsUpgradePath::_description";

constexpr uint32_t kInitialField = 0x14;   // CSkillsUpgradeComponentData
constexpr uint32_t kPathsField = 0x18;
constexpr uint32_t kPathSkillsField = 0x10;  // CSkillsUpgradePath
constexpr uint32_t kCostField = 0x10;      // CSkillUpgrade
constexpr uint32_t kPictureField = 0x24;
constexpr uint32_t kWowPowYearField = 0x34;  // CSkillUpgradeWowpow
constexpr uint32_t kTextureImageField = 0x10;  // igGraphicsTexture
constexpr int kYearShift = 12;              // catalog variant: year << 12
constexpr int kFirstUpgradeBit = 2;
constexpr int kLastUpgradeBit = 10;
constexpr int kFirstAlternateBit = 11;

struct Upgrade {
  std::string name;
  std::string description;
  uint32_t cost = 0;
  std::string material;  // its picture
  int wow_pow_year = -1;
};
struct Path {
  std::string name;
  std::string description;
  std::vector<Upgrade> upgrades;
  bool wow_pows = false;
};
struct Character {
  std::vector<Upgrade> start;
  std::vector<Path> paths;  // in the game's order
  std::unordered_map<std::string, std::string> textures;  // material -> texture
  std::filesystem::path pictures;  // <Name>_AbilityUpgrades.pak
};

std::filesystem::path g_rom;
std::mutex g_mutex;
std::unordered_map<std::string, std::shared_ptr<const Character>> g_characters;  // null: none
std::unordered_map<std::string, std::shared_ptr<const FigureUpgrades>> g_built;  // "<name>|<year>"
std::unordered_map<std::string, std::shared_ptr<const FigureImage>> g_images;
std::unordered_set<std::string> g_requested;  // characters and picture keys asked for
std::deque<std::string> g_queue;              // characters (no prefix) and picture keys
bool g_worker = false;

// The game's button glyphs as the button's letter.
std::string ButtonNames(const std::string& s) {
  std::string out;
  for (char c : s) {
    switch (c) {
      case '~': out += 'X'; break;
      case '^': out += 'Y'; break;
      case '`': out += 'B'; break;
      default: out.push_back(c);
    }
  }
  return out;
}

// A text, or empty for the disc's placeholders ("[Beschreibung fehlt]").
std::string Shown(const std::string& s) { return s.empty() || s.front() == '[' ? std::string() : ButtonNames(s); }

Upgrade ReadUpgrade(const IgzFile& igz, size_t object, const std::string& type,
                    const std::unordered_map<size_t, std::string>& names, const GameText& text) {
  Upgrade u;
  auto name = names.find(object);
  const std::string object_name = name == names.end() ? std::string() : name->second;
  u.name = Shown(text.Get(object_name, kNameField));
  if (u.name.empty()) {
    // "..._skills_BasicSkills_0_PenguinDrill" -> "Penguin Drill" (not "0" for an unnamed one).
    const std::string last = object_name.substr(object_name.find_last_of('_') + 1);
    if (!last.empty() && !std::isdigit(static_cast<unsigned char>(last.front()))) u.name = Humanize(last);
  }
  u.description = Shown(text.Get(object_name, kDescriptionField));
  u.cost = igz.Field(object, kCostField);
  if (const auto picture = igz.NamedExternalName(igz.Field(object, kPictureField))) u.material = picture->second;
  if (type == "CSkillUpgradeWowpow") u.wow_pow_year = int(igz.Field(object, kWowPowYearField));
  return u;
}

// The material -> texture names of an ability upgrades archive.
std::unordered_map<std::string, std::string> ReadTextures(const IgaArchive& archive) {
  std::unordered_map<std::string, std::string> textures;
  for (size_t i = 0; i < archive.size(); ++i) {
    std::string name = archive.name(i);
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    if (name.find(kMaterialDir) == std::string::npos || !EndsWith(name, ".igz") || EndsWith(name, "_pkg.igz")) continue;
    auto data = archive.Read(i);
    auto igz = data ? IgzFile::Parse(std::move(*data)) : std::nullopt;
    if (!igz) continue;
    const auto names = ObjectNames(*igz);
    std::string material;
    for (const auto& [object, type] : igz->Objects("")) {
      if (type == "igGraphicsMaterial") {
        auto it = names.find(object);
        material = it == names.end() ? std::string() : it->second;
      } else if (type == "igGraphicsTexture" && !material.empty()) {
        const auto image = igz->ExternalName(igz->Field(object, kTextureImageField));
        if (image && image->second == kImageNamespace) textures.emplace(material, image->first);
        material.clear();
      }
    }
  }
  return textures;
}

std::shared_ptr<const Character> ReadCharacter(const std::string& name, bool german) {
  auto pak = IgaArchive::Open(g_rom / "archives" / (name + ".pak"));
  if (!pak) return nullptr;
  auto igz = ReadFile(*pak, "/" + name + kCharacterSuffix);
  if (!igz) return nullptr;
  const auto components = igz->Objects("CSkillsUpgradeComponentData");
  if (components.empty()) return nullptr;
  GameText text;
  if (german) {
    if (auto lng = ReadFile(*pak, "/" + name + kTextSuffix)) text = ReadText(*lng);
  }
  const auto names = ObjectNames(*igz);
  const auto types = ObjectTypes(*igz);
  auto type_of = [&](size_t object) {
    auto it = types.find(object);
    return it == types.end() ? std::string() : it->second;
  };
  auto character = std::make_shared<Character>();
  const size_t component = components.front().first;
  if (const auto start = Target(*igz, igz->Field(component, kInitialField))) {
    for (size_t object : ListItems(*igz, *start)) {
      character->start.push_back(ReadUpgrade(*igz, object, type_of(object), names, text));
    }
  }
  if (const auto paths = Target(*igz, igz->Field(component, kPathsField))) {
    for (size_t path_object : ListItems(*igz, *paths)) {
      Path path;
      auto path_name = names.find(path_object);
      if (path_name != names.end()) {
        path.name = Shown(text.Get(path_name->second, kPathNameField));
        path.description = Shown(text.Get(path_name->second, kPathDescriptionField));
      }
      if (const auto skills = Target(*igz, igz->Field(path_object, kPathSkillsField))) {
        for (size_t object : ListItems(*igz, *skills)) {
          path.upgrades.push_back(ReadUpgrade(*igz, object, type_of(object), names, text));
          if (path.upgrades.back().wow_pow_year >= 0) path.wow_pows = true;
        }
      }
      character->paths.push_back(std::move(path));
    }
  }
  if (character->paths.empty()) return nullptr;
  character->pictures = g_rom / "archives" / (name + kPicturesSuffix);
  if (auto pictures = IgaArchive::Open(character->pictures)) character->textures = ReadTextures(*pictures);
  return character;
}

std::string PictureKey(const std::string& name, const Character& c, const Upgrade& u) {
  if (u.material.empty() || !c.textures.contains(u.material)) return {};
  return std::string(kKeyPrefix) + name + ":" + u.material;
}

std::shared_ptr<const FigureUpgrades> Build(const std::string& name, const Character& c, int year) {
  auto result = std::make_shared<FigureUpgrades>();
  auto make = [&](const Upgrade& from, int bit) {
    FigureUpgrade u;
    u.name = from.name;
    u.description = from.description;
    u.cost = from.cost;
    u.bit = bit;
    u.image = PictureKey(name, c, from);
    return u;
  };
  if (!c.start.empty()) {
    FigureUpgradePath start;
    start.kind = FigureUpgradePath::Kind::kStart;
    for (const Upgrade& u : c.start) start.upgrades.push_back(make(u, -1));
    result->paths.push_back(std::move(start));
  }
  int basic_count = 0, path_length = 0, paths_seen = 0;
  bool soul_gem_seen = false;
  for (const Path& path : c.paths) {
    FigureUpgradePath out;
    out.name = path.name;
    out.description = path.description;
    if (path.wow_pows) {
      // Only this figure's year's, and only for a later year's figure.
      for (const Upgrade& u : path.upgrades) {
        if (year > 0 && u.wow_pow_year == year) out.upgrades.push_back(make(u, kLastUpgradeBit));
      }
      if (out.upgrades.empty()) continue;
      out.kind = FigureUpgradePath::Kind::kWowPow;
    } else if (paths_seen == 0) {
      out.kind = FigureUpgradePath::Kind::kBasic;
      basic_count = int(path.upgrades.size());
      for (int i = 0; i < basic_count; ++i) out.upgrades.push_back(make(path.upgrades[i], kFirstUpgradeBit + i));
    } else if (paths_seen <= 2) {
      out.kind = FigureUpgradePath::Kind::kPath;
      if (paths_seen == 1) path_length = int(path.upgrades.size());
      for (int j = 0; j < int(path.upgrades.size()); ++j) {
        FigureUpgrade u = make(path.upgrades[j], kFirstUpgradeBit + basic_count + j);
        u.alternate_bit = kFirstAlternateBit + j;
        out.upgrades.push_back(std::move(u));
      }
    } else if (!soul_gem_seen) {
      out.kind = FigureUpgradePath::Kind::kSoulGem;
      soul_gem_seen = true;
      for (size_t k = 0; k < path.upgrades.size(); ++k) {
        out.upgrades.push_back(make(path.upgrades[k], k == 0 ? kFirstUpgradeBit + basic_count + path_length
                                                            : kLastUpgradeBit));
      }
    } else {
      continue;
    }
    if (!path.wow_pows) ++paths_seen;
    result->paths.push_back(std::move(out));
  }
  return result;
}

std::shared_ptr<const FigureImage> ReadPicture(const std::filesystem::path& archive_path, const std::string& texture) {
  auto archive = IgaArchive::Open(archive_path);
  if (!archive) return nullptr;
  auto tex = ReadFile(*archive, std::string(kTextureDir) + texture + ".igz");
  auto rgba = tex ? tex->DecodeFirstImage() : std::nullopt;
  if (!rgba) return nullptr;
  auto image = std::make_shared<FigureImage>();
  image->width = rgba->width;
  image->height = rgba->height;
  image->rgba = std::move(rgba->pixels);
  FlipVertically(*image);
  return image;
}

// Reads what was asked for, one after the other, and ends when nothing is left.
void Worker() {
  const bool german = skylanders::menu_draw::GermanText();
  for (;;) {
    std::string item;
    std::filesystem::path archive;
    std::string texture;
    {
      std::lock_guard<std::mutex> lock(g_mutex);
      if (g_queue.empty()) {
        g_worker = false;
        return;
      }
      item = std::move(g_queue.front());
      g_queue.pop_front();
      if (item.starts_with(kKeyPrefix)) {
        // "upgrade:<name>:<material>"
        const size_t colon = item.find(':', kKeyPrefix.size());
        auto character = colon == std::string::npos
                             ? g_characters.end()
                             : g_characters.find(item.substr(kKeyPrefix.size(), colon - kKeyPrefix.size()));
        if (character == g_characters.end() || !character->second) continue;
        auto it = character->second->textures.find(item.substr(colon + 1));
        if (it == character->second->textures.end()) continue;
        archive = character->second->pictures;
        texture = it->second;
      }
    }
    if (!item.starts_with(kKeyPrefix)) {
      auto character = ReadCharacter(item, german);
      REXLOG_INFO("Toy upgrades: {} {}", item, character ? "read" : "has none");
      std::lock_guard<std::mutex> lock(g_mutex);
      g_characters[item] = std::move(character);
      continue;
    }
    auto image = ReadPicture(archive, texture);
    std::lock_guard<std::mutex> lock(g_mutex);
    g_images[item] = std::move(image);
  }
}

// Queues `item` once; g_mutex held.
void Request(const std::string& item) {
  if (!g_requested.insert(item).second) return;
  g_queue.push_back(item);
  if (!g_worker) {
    g_worker = true;
    std::thread(Worker).detach();
  }
}

std::shared_ptr<const FigureUpgrades> Lookup(uint16_t id, uint16_t variant) {
  const std::string name = ToyCharacter(id);
  if (name.empty()) return nullptr;
  const int year = (variant >> kYearShift) & 0xF;
  std::lock_guard<std::mutex> lock(g_mutex);
  auto character = g_characters.find(name);
  if (character == g_characters.end()) {
    Request(name);
    return nullptr;
  }
  if (!character->second) return nullptr;
  const std::string key = name + "|" + std::to_string(year);
  if (auto it = g_built.find(key); it != g_built.end()) return it->second;
  return g_built[key] = Build(name, *character->second, year);
}

}  // namespace

void InstallToyUpgrades(const std::filesystem::path& rom_dir) {
  g_rom = rom_dir;
  skylanders::SetFigureUpgradesProvider([](uint16_t id, uint16_t variant) { return Lookup(id, variant); });
}

std::shared_ptr<const FigureImage> UpgradeImage(const std::string& key) {
  if (!key.starts_with(kKeyPrefix)) return nullptr;
  std::lock_guard<std::mutex> lock(g_mutex);
  if (auto it = g_images.find(key); it != g_images.end()) return it->second;
  Request(key);
  return nullptr;
}

}  // namespace superchargersrecomp
