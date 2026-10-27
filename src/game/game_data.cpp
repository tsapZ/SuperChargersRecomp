#include "game_data.h"

#include <algorithm>
#include <cctype>

namespace superchargersrecomp {

// The games' shared Alchemy readers (skylanders-portal src/alchemy).
namespace alchemy = skylanders::alchemy;
namespace {

using alchemy::IgaArchive;
using alchemy::IgzFile;

constexpr char kFieldNamespace[] = "metafield";
constexpr uint32_t kListCountField = 0x08;
constexpr uint32_t kListDataField = 0x14;
constexpr uint32_t kMaxListItems = 4096;
constexpr uint32_t kTextField = 0x08;  // igLocalizedStringData
constexpr uint32_t kTextObjectField = 0x0C;
constexpr uint32_t kTextFieldField = 0x10;
constexpr uint32_t kTableSizeField = 0x08;
constexpr uint32_t kTableValuesField = 0x0C;
constexpr uint32_t kTableKeysField = 0x14;
constexpr uint32_t kTableTextField = 0x28;  // igLocalizedHashTable: its field
constexpr uint32_t kEmptyKey = 0xFAFAFAFA;
constexpr uint32_t kHandleIndex = 0x7FFFFFFF;
constexpr uint32_t kMaxTableEntries = 4096;

std::string Lower(std::string_view s) {
  std::string out(s);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return char(std::tolower(c)); });
  return out;
}

}  // namespace

std::string GameText::Get(const std::string& object, std::string_view field) const {
  auto it = fields.find(object + "|" + std::string(field));
  return it == fields.end() ? std::string() : it->second;
}

bool EndsWith(const std::string& s, std::string_view suffix) {
  return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

uint32_t Fnv1a(std::string_view s) {
  uint32_t hash = 0x811C9DC5u;
  for (unsigned char c : s) {
    hash ^= c;
    hash *= 0x01000193u;
  }
  return hash;
}

std::optional<size_t> Target(const IgzFile& igz, uint32_t pointer) {
  return pointer ? igz.Pointer(pointer) : std::nullopt;
}

std::optional<IgzFile> ReadFile(const IgaArchive& archive, std::string_view suffix) {
  const std::string want = Lower(suffix);
  for (size_t i = 0; i < archive.size(); ++i) {
    if (!EndsWith(Lower(archive.name(i)), want)) continue;
    auto data = archive.Read(i);
    return data ? IgzFile::Parse(std::move(*data)) : std::nullopt;
  }
  return std::nullopt;
}

void ForEachEntry(const IgzFile& igz, size_t table, const std::function<void(uint32_t, uint32_t)>& f) {
  const uint32_t capacity = (igz.Field(table, kTableSizeField) & 0x00FFFFFF) / 4;
  const auto values = Target(igz, igz.Field(table, kTableValuesField));
  const auto keys = Target(igz, igz.Field(table, kTableKeysField));
  if (!values || !keys || capacity > kMaxTableEntries) return;
  for (uint32_t k = 0; k < capacity; ++k) {
    const uint32_t key = igz.Field(*keys, 4 * k), value = igz.Field(*values, 4 * k);
    if (key != kEmptyKey && value) f(key, value);
  }
}

std::vector<size_t> ListItems(const IgzFile& igz, size_t list) {
  std::vector<size_t> out;
  const uint32_t count = std::min(igz.Field(list, kListCountField), kMaxListItems);
  const auto items = Target(igz, igz.Field(list, kListDataField));
  if (!items) return out;
  for (uint32_t k = 0; k < count; ++k) {
    if (const auto item = Target(igz, igz.Field(*items, 4 * k))) out.push_back(*item);
  }
  return out;
}

GameText ReadText(const IgzFile& lng) {
  GameText text;
  std::vector<std::string> objects, fields;
  for (uint32_t k = 0;; ++k) {
    const auto name = lng.ExternalName(0x80000000u | k);
    if (!name) break;
    (name->first == kFieldNamespace ? fields : objects).push_back(name->second);
  }
  for (const auto& [entry, type] : lng.Objects("igLocalizedStringData")) {
    const uint32_t object = lng.Field(entry, kTextObjectField) & kHandleIndex;
    const uint32_t field = lng.Field(entry, kTextFieldField) & kHandleIndex;
    if (object < objects.size() && field < fields.size()) {
      text.fields[objects[object] + "|" + fields[field]] = lng.StringAt(lng.Field(entry, kTextField));
    }
  }
  for (const auto& [table, type] : lng.Objects("igLocalizedHashTable")) {
    const uint32_t field = lng.Field(table, kTableTextField) & kHandleIndex;
    if (field >= fields.size()) continue;
    auto& entries = text.tables[fields[field]];
    ForEachEntry(lng, table, [&](uint32_t key, uint32_t value) { entries[key] = lng.StringAt(value); });
  }
  return text;
}

std::unordered_map<size_t, std::string> ObjectNames(const IgzFile& igz) {
  std::unordered_map<size_t, std::string> names;
  const auto roots = igz.Objects("igObjectList");
  const auto lists = igz.Objects("igNameList");
  if (roots.empty() || lists.empty()) return names;
  const size_t root = roots.front().first, list = lists.back().first;
  const uint32_t count = std::min(igz.Field(root, kListCountField), igz.Field(list, kListCountField));
  const auto objects = Target(igz, igz.Field(root, kListDataField));
  const auto entries = Target(igz, igz.Field(list, kListDataField));
  if (!objects || !entries) return names;
  for (uint32_t k = 0; k < count; ++k) {
    if (const auto at = Target(igz, igz.Field(*objects, 4 * k))) {
      names[*at] = igz.StringAt(igz.Field(*entries, 8 * k));
    }
  }
  return names;
}

std::unordered_map<size_t, std::string> ObjectTypes(const IgzFile& igz) {
  std::unordered_map<size_t, std::string> types;
  for (auto& [object, type] : igz.Objects("")) types.emplace(object, std::move(type));
  return types;
}

void FlipVertically(skylanders::FigureImage& image) {
  const size_t row = size_t(image.width) * 4;
  for (int y = 0; y < image.height / 2; ++y) {
    std::swap_ranges(image.rgba.begin() + y * row, image.rgba.begin() + (y + 1) * row,
                     image.rgba.begin() + (image.height - 1 - y) * row);
  }
}

std::string Humanize(std::string_view name) {
  std::string out;
  for (size_t i = 0; i < name.size(); ++i) {
    const char c = name[i];
    if (c == '_') {
      if (!out.empty() && out.back() != ' ') out.push_back(' ');
      continue;
    }
    const bool upper = std::isupper(static_cast<unsigned char>(c)) != 0;
    if (upper && i > 0 && std::islower(static_cast<unsigned char>(name[i - 1])) && !out.empty() &&
        out.back() != ' ') {
      out.push_back(' ');
    }
    out.push_back(c);
  }
  return out;
}

}  // namespace superchargersrecomp
