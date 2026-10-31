// The levels' names as the game shows them, for the Discord status. Read 2026-10-31:
//
// - zoneinfos.pak has a <level>_zoneinfo.igz per level (academy, intro1, nat1 ... final2, the
//   adventure packs apa1 ... aps4) with its CZoneInfo, and the language files next to it
//   (<level>_zoneinfo_<language>.lng): "CZoneInfo|CZoneInfo::_displayName" is the level's name
//   ("Bremsenlichtung" for nat1), "CZoneInfo::_storyModePresenceText" the Xbox Live status the
//   game itself had ("Schrumpft auf der Bremsenlichtung!").
// - German from the _de files (the European English / German disc), else the _es ones (German on
//   the Spanish / Italian disc after tools/german_text.py), as for the hats (hats.cpp); English
//   from the _en files. The Spanish / Italian disc has no English text: the English status then
//   shows no level.
// - The level as where_hook.cpp names it: its archive ("Nat1", "Academy", "APA1"), the same name
//   as the zone info's in any case.

#include "game/level_names.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <rex/logging.h>

#include "alchemy/iga_archive.h"
#include "alchemy/igz_file.h"
#include "game/game_data.h"
#include "portal_rex/discord_presence.h"
#include "portal_rex/menu_draw.h"

namespace superchargersrecomp {
namespace {

constexpr char kArchive[] = "zoneinfos.pak";
constexpr std::string_view kZoneInfo = "_zoneinfo";

using Table = std::unordered_map<std::string, std::string>;

std::string Lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

}  // namespace

void InstallLevelNames(const std::filesystem::path& rom_dir) {
  std::thread([rom_dir] {
    const auto archive = alchemy::IgaArchive::Open(rom_dir / "archives" / kArchive);
    if (!archive) {
      REXLOG_WARN("Level names: no {}; the Discord status shows no level", kArchive);
      return;
    }
    // Every zone info's name by language ("de", "es", "en"), the level in lower case.
    std::unordered_map<std::string, Table> by_language;
    for (size_t i = 0; i < archive->size(); ++i) {
      const std::string name = Lower(archive->name(i));
      // ".../nat1_zoneinfo_de.lng"
      if (!EndsWith(name, ".lng") || name.size() < 7) continue;
      const std::string language = name.substr(name.size() - 6, 2);
      const size_t base = name.find_last_of('/') + 1;
      const size_t zone = name.rfind(kZoneInfo);
      if (zone == std::string::npos || zone < base || zone + kZoneInfo.size() + 7 != name.size()) continue;
      auto data = archive->Read(i);
      auto lng = data ? alchemy::IgzFile::Parse(std::move(*data)) : std::nullopt;
      if (!lng) continue;
      std::string shown = ReadText(*lng).Get("CZoneInfo", "CZoneInfo::_displayName");
      if (!shown.empty()) by_language[language][name.substr(base, zone - base)] = std::move(shown);
    }
    auto german = std::make_shared<Table>(by_language.contains("de") ? by_language["de"] : by_language["es"]);
    auto english = std::make_shared<Table>(by_language["en"]);
    // Which levels have a name, so a log shows the ones the status leaves out.
    std::vector<std::string> known;
    for (const auto& [level, shown] : german->empty() ? *english : *german) known.push_back(level);
    std::sort(known.begin(), known.end());
    std::string list;
    for (const std::string& level : known) list += (list.empty() ? "" : " ") + level;
    REXLOG_INFO("Level names: {} German, {} English ({})", german->size(), english->size(), list);
    skylanders::SetDiscordLevelNames([german, english](const std::string& level) {
      const Table& table = skylanders::menu_draw::GermanText() ? *german : *english;
      const auto it = table.find(Lower(level));
      return it == table.end() ? std::string() : it->second;
    });
  }).detach();
}

}  // namespace superchargersrecomp
