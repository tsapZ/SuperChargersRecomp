// Skips the startup logo movies and, optionally, the opening movie of a new game.
//
// BinkOpen (Bink 2) reads the 44-byte file header into r1+112 and then checks its signature (the
// "KB2f".. compares that follow, at loc_831C7CC8 in the generated code: lis r11,0x6632 / ori
// 0x424B); the midasm hook at 0x831C7CC8 (config/default.toml) runs right after the read. When the
// header belongs to one of the movies below, recognised by the file-size and frame-count fields
// that are unique to them, the frame count at +8 is set to 1. Bink then plays one frame and
// reports the movie finished, so the game moves on as if it had played out. The same approach as
// TrapTeamRecomp's (the same r1+112), found here by the same code shape: the call that
// reads 44 bytes (li r6,44) and the signature compare after it.

#include <cstdint>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/memory.h>
#include <rex/memory/utils.h>
#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>

REXCVAR_DEFINE_BOOL(skip_intro, true, "Game",
                    "Skip the Activision and Vicarious Visions logo movies at startup.");
REXCVAR_DEFINE_BOOL(skip_opening_movie, false, "Game",
                    "Skip the opening movie when starting a new game.");

namespace {

struct Movie {
  uint32_t size_field;  // header +4: file size minus 8
  uint32_t frames;      // header +8
  bool opening;         // true: controlled by skip_opening_movie instead of skip_intro
  const char* name;
};

// Header values read from the disc's files (all "KB2i").
constexpr Movie kMovies[] = {
    {0x0065D818, 226, false, "archives/Bumper_ATVI.bk2"},     // Activision logo
    {0x00803C9C, 270, false, "archives/Bumper_VVisions.bk2"}, // Vicarious Visions logo
    {0x06F40788, 3905, true, "archives/FMV01.bk2"},           // new-game opening movie
};

}  // namespace

void BinkHeaderLoaded(PPCRegister& r1) {
  const bool skip_logos = REXCVAR_GET(skip_intro);
  const bool skip_opening = REXCVAR_GET(skip_opening_movie);
  if (!skip_logos && !skip_opening) {
    return;
  }
  uint8_t* base = rex::system::kernel_state()->memory()->virtual_membase();
  uint8_t* header = base + r1.u32 + 112;
  // The file stores these fields little-endian, but Bink's reader has already swapped the header
  // into native (big-endian) words by the time it reaches this point.
  const uint32_t size_field = rex::memory::load_and_swap<uint32_t>(header + 4);
  const uint32_t frames = rex::memory::load_and_swap<uint32_t>(header + 8);
  for (const Movie& movie : kMovies) {
    if (size_field != movie.size_field || frames != movie.frames) {
      continue;
    }
    if (movie.opening ? skip_opening : skip_logos) {
      rex::memory::store_and_swap<uint32_t>(header + 8, 1);
      REXLOG_INFO("Skipping movie {} ({} frames)", movie.name, frames);
    }
    return;
  }
}
