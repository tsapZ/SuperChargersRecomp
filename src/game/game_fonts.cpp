// The game's own fonts for the portal menus: the bitmap fonts of its UI (ProximaNova), read from
// the owner's game files at start-up, like the pictures (nothing of them is in the repository).
//
// fonts/ProximaNova-Title.igz (permanent.pak) and fonts/ProximaNova-Body.igz (essentialui.pak) are
// igBitmapFont files (IGZ version 9). The igBitmapFont's +0x0C is the font's size in pixels (146
// and 40). Each igCharMetricsList (its count at +0x08, its igCharMetrics pointers inline from
// +0x18) lists the glyphs of one atlas page, the igImage2 objects in the same order. An
// igCharMetrics (48 bytes): +0x08 u16 code point, +0x0C advance, +0x10 / +0x14 the cell's offset,
// +0x18 / +0x1C its width and height (the Body font's cells are 34 high, the Title font's 146),
// +0x20 / +0x24 its top-left texture coordinates. The pages are Xbox 360 tiled with 16 bits per
// texel, the first byte the coverage (the second is 255): the Title font one 1024x1029 page, the
// Body font a 128x770 one, plus a 32-bit page with <>[]{}~ and a button symbol that is left out
// (those come from the Windows font). Read 2026-10-21 from both files: the decoded pages show every glyph
// upright; the Body font has the German ä ö ü ß, the Title font Ä Ö Ü and ö but no ä or ü (the
// Windows font fills in).

#include "game_fonts.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <string_view>

#include <rex/logging.h>

#include "game/alchemy/iga_archive.h"
#include "game/alchemy/igz_file.h"

namespace superchargersrecomp {
namespace {

using alchemy::IgaArchive;
using alchemy::IgzFile;
using skylanders::GameFont;
using skylanders::GameFontGlyph;
using skylanders::GameFontPage;

constexpr char kTitleArchive[] = "permanent.pak";
constexpr std::string_view kTitleFont = "/fonts/ProximaNova-Title.igz";
constexpr char kBodyArchive[] = "essentialui.pak";
constexpr std::string_view kBodyFont = "/fonts/ProximaNova-Body.igz";

// The atlas pages' format (its name's hash in the file's EXID list), 16 bits per texel.
constexpr uint32_t kCoverageFormat = 0xF8C753BD;
constexpr uint32_t kFontSizeField = 0x0C;
constexpr uint32_t kListCountField = 0x08;
constexpr uint32_t kListItems = 0x18;
constexpr uint32_t kCodepointField = 0x08;
constexpr uint32_t kAdvanceField = 0x0C;
constexpr uint32_t kOffsetXField = 0x10;
constexpr uint32_t kOffsetYField = 0x14;
constexpr uint32_t kWidthField = 0x18;
constexpr uint32_t kHeightField = 0x1C;
constexpr uint32_t kU0Field = 0x20;
constexpr uint32_t kV0Field = 0x24;

uint16_t Be16(const std::vector<uint8_t>& b, size_t at) { return uint16_t(b[at] << 8 | b[at + 1]); }

float F32(const IgzFile& igz, size_t object, uint32_t field) {
  const uint32_t bits = igz.Field(object, field);
  float value;
  std::memcpy(&value, &bits, 4);
  return value;
}

// From a line's top to the baseline: the bottom of the H's ink.
float Ascent(const GameFont& font) {
  for (const GameFontGlyph& g : font.glyphs) {
    if (g.codepoint != 'H' || size_t(g.page) >= font.pages.size()) continue;
    const GameFontPage& page = font.pages[g.page];
    if (page.alpha.empty()) break;
    for (int row = g.height - 1; row >= 0; --row) {
      for (int col = 0; col < g.width; ++col) {
        const int x = g.x + col, y = g.y + row;
        if (x < page.width && y < page.height && page.alpha[size_t(y) * page.width + x] >= 128) {
          return g.y_offset + row + 1;
        }
      }
    }
  }
  return font.size * 0.8f;
}

std::shared_ptr<const GameFont> ReadFont(const std::filesystem::path& archive_path,
                                         std::string_view suffix) {
  auto archive = IgaArchive::Open(archive_path);
  if (!archive) return nullptr;
  for (size_t i = 0; i < archive->size(); ++i) {
    const std::string& name = archive->name(i);
    if (name.size() < suffix.size() || name.compare(name.size() - suffix.size(), suffix.size(), suffix)) {
      continue;
    }
    auto data = archive->Read(i);
    if (!data) return nullptr;
    auto igz = IgzFile::Parse(std::move(*data));
    if (!igz) return nullptr;
    const auto fonts = igz->Objects("igBitmapFont");
    const auto lists = igz->Objects("igCharMetricsList");
    const auto images = igz->RawImages();
    if (fonts.empty()) return nullptr;
    const std::vector<uint8_t>& bytes = igz->Bytes();
    auto font = std::make_shared<GameFont>();
    font->size = F32(*igz, fonts[0].first, kFontSizeField);
    for (size_t page = 0; page < images.size() && page < lists.size(); ++page) {
      const alchemy::RawImage& image = images[page];
      GameFontPage& out = font->pages.emplace_back();
      const uint32_t rows = (uint32_t(image.height) + 31) & ~31u;
      // The 16-bit coverage format only (the Body font's 32-bit symbol page is left out), its
      // first mip level (Imaginators' title page has a second one after it).
      if (image.format_hash != kCoverageFormat || image.width <= 0 || image.height <= 0 ||
          image.size < uint32_t(image.width) * rows * 2) {
        continue;
      }
      out.width = image.width;
      out.height = image.height;
      out.alpha.resize(size_t(out.width) * out.height);
      for (int y = 0; y < out.height; ++y) {
        for (int x = 0; x < out.width; ++x) {
          const size_t at = image.data + alchemy::TiledOffset2D(x, y, uint32_t(out.width), 1);
          out.alpha[size_t(y) * out.width + x] = bytes[at];
        }
      }
      const size_t list = lists[page].first;
      const uint32_t count = igz->Field(list, kListCountField);
      for (uint32_t k = 0; k < count && k < 4096; ++k) {
        const auto g = igz->Pointer(igz->Field(list, kListItems + 4 * k));
        if (!g || *g + 0x30 > bytes.size()) continue;
        GameFontGlyph glyph;
        glyph.codepoint = Be16(bytes, *g + kCodepointField);
        glyph.advance = F32(*igz, *g, kAdvanceField);
        glyph.x_offset = F32(*igz, *g, kOffsetXField);
        glyph.y_offset = F32(*igz, *g, kOffsetYField);
        glyph.page = int(page);
        glyph.x = int(std::lround(F32(*igz, *g, kU0Field) * out.width));
        glyph.y = int(std::lround(F32(*igz, *g, kV0Field) * out.height));
        glyph.width = std::min(int(std::lround(F32(*igz, *g, kWidthField))), out.width - glyph.x);
        glyph.height = std::min(int(std::lround(F32(*igz, *g, kHeightField))), out.height - glyph.y);
        if (glyph.x < 0 || glyph.y < 0) continue;
        font->glyphs.push_back(glyph);
      }
    }
    if (font->glyphs.empty() || font->size <= 0) return nullptr;
    font->ascent = Ascent(*font);
    return font;
  }
  return nullptr;
}

}  // namespace

skylanders::GameFonts LoadGameFonts(const std::filesystem::path& rom_dir) {
  const auto start = std::chrono::steady_clock::now();
  skylanders::GameFonts fonts;
  fonts.title = ReadFont(rom_dir / "archives" / kTitleArchive, kTitleFont);
  fonts.body = ReadFont(rom_dir / "archives" / kBodyArchive, kBodyFont);
  REXLOG_INFO("Game fonts: title {} glyphs, body {} glyphs, in {} ms",
              fonts.title ? fonts.title->glyphs.size() : 0,
              fonts.body ? fonts.body->glyphs.size() : 0,
              std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - start).count());
  return fonts;
}

}  // namespace superchargersrecomp
