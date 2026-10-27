#pragma once

// Readers for the game's data files that more than one part needs (hats.cpp, toy_upgrades.cpp):
// the language files, object names, lists and hash tables of IGZ version 9 (alchemy/igz_file.h).

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "alchemy/iga_archive.h"
#include "alchemy/igz_file.h"
#include "portal_rex/portal_menu.h"

namespace superchargersrecomp {

// The games' shared Alchemy readers (skylanders-portal src/alchemy).
namespace alchemy = skylanders::alchemy;

// The game's text in one language (a .lng file): each object's localized fields
// ("<object>|<field>"), and the localized hash tables by field, which translate an English text
// by its FNV-1a hash. A .lng has an igLocalizedStringData per text: +0x08 the text, +0x0C the
// object and +0x10 the field, each an index into the file's external names, objects and
// "metafield" fields counted separately (worked out on CollectibleTrackers_es.lng, 2026-10-22).
struct GameText {
  std::unordered_map<std::string, std::string> fields;
  std::unordered_map<std::string, std::unordered_map<uint32_t, std::string>> tables;

  // The text of an object's field, empty if there is none.
  std::string Get(const std::string& object, std::string_view field) const;
};
GameText ReadText(const alchemy::IgzFile& lng);

bool EndsWith(const std::string& s, std::string_view suffix);
uint32_t Fnv1a(std::string_view s);
// A serialized pointer as an offset, nullopt for 0 or one outside the pools.
std::optional<size_t> Target(const alchemy::IgzFile& igz, uint32_t pointer);
// The first file of the archive whose name ends with `suffix` (any case), parsed.
std::optional<alchemy::IgzFile> ReadFile(const alchemy::IgaArchive& archive, std::string_view suffix);
// Calls f(key, value) for every filled slot of a hash table object (+0x08 the values' size,
// +0x0C the values, +0x14 the keys; empty slots have the key 0xFAFAFAFA).
void ForEachEntry(const alchemy::IgzFile& igz, size_t table,
                  const std::function<void(uint32_t key, uint32_t value)>& f);
// The items of a list object (igObjectList and the like: +0x08 the count, +0x14 the items).
std::vector<size_t> ListItems(const alchemy::IgzFile& igz, size_t list);
// The objects' names: the igNameList's entries (string, hash) in the root igObjectList's order.
std::unordered_map<size_t, std::string> ObjectNames(const alchemy::IgzFile& igz);
// Every object's class name by its offset.
std::unordered_map<size_t, std::string> ObjectTypes(const alchemy::IgzFile& igz);
void FlipVertically(skylanders::FigureImage& image);
// "CowboyHat" -> "Cowboy Hat", "Penguin_Drill" -> "Penguin Drill": a name for the English menu
// where the disc has no English text.
std::string Humanize(std::string_view name);

}  // namespace superchargersrecomp
