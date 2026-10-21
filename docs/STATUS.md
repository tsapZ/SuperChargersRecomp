# Status

Updated at the end of every session. Only record what a build actually did.

## Current state (2026-10-21)

| Area | State |
|---|---|
| Repository | scaffolded from GiantsRecomp: build, app, portal (emulated, USB, hybrid), portal / player / settings menus, achievements, input scripts and test tools; none of its game hooks or file readers |
| Game data | disc extracted into `rom/` (2906 files), `default.xex` hash pinned |
| SDK | our fork `tsapZ/rexglue-sdk`, branch `skylanders`, shared with the other games |
| Codegen | works with 28 `config/default.toml` entries: function starts the analyzer put one instruction late |
| Boot | ✅ logo movies, the title screen ("PULSA START"), "¡Elige tu estilo de juego!" (difficulty), the mode books (Aventura, Carreras), the save slots, then the long opening movie (Start pauses it, doesn't skip it); no crash in 5 minutes |
| Languages | this disc has **Spanish and Italian only** (347 language files each, `*_es.pak` / `*_it.pak`), so the game runs in Spanish with language "auto" on a German Windows |

## 2026-10-21: session log

- **Own repository**, like the other games: each playable on its own, later through the hub
  (Skylanders: Core of Light).
- **Scaffold:** GiantsRecomp's tracked files, renamed; its Giants-specific parts left out.
- **Disc:** Vicarious Visions' layout like Swap Force and Imaginators: `archives\*.pak` (IGA version 11, big-endian), `build.xml` (2015, changelist 2785777). A European pressing with Spanish and Italian.
- **First boot:** "Call to invalid or unregistered function at guest address 0x8252E1F8" 3 s in; the analyzer had that start inside the 12-byte sub_8252E1F0. Registered in `config/default.toml`, the game boots through the logos and the title into the opening movie.
- **27 more late starts:** the same shape searched in the whole code (a dump of guest memory with the input script's `save`): right after a `blr` an instruction that is no known start, followed by one that is. Registered too, so they don't stop the game when it first calls them. Next: the first level and the portal, then the opening movie skip.
- **Logo movie skip** (from TrapTeamRecomp): a midasm hook at 0x831C7CC8 right after BinkOpen reads the 44-byte header into r1+112; the Activision (226 frames) and Vicarious Visions (270) bumpers are cut to one frame, the opening movie FMV01.bk2 (3905) with skip_opening_movie. Checked: the title at 8 s.
