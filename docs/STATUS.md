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
| Portal menu | ✅ opens with Guide; shows the game's own round collection icons for every Skylander, vehicle, trap and magic item, each figure's variant where the game has one; opens by itself when the game asks for a Skylander, the portal check on the "too many" screens |
| Languages | this disc has **Spanish and Italian only** (347 language files each, `*_es.pak` / `*_it.pak`), so the game runs in Spanish with language "auto" on a German Windows |

## 2026-10-21: session log

- **Own repository**, like the other games: each playable on its own, later through the hub
  (Skylanders: Core of Light).
- **Scaffold:** GiantsRecomp's tracked files, renamed; its Giants-specific parts left out.
- **Disc:** Vicarious Visions' layout like Swap Force and Imaginators: `archives\*.pak` (IGA version 11, big-endian), `build.xml` (2015, changelist 2785777). A European pressing with Spanish and Italian.
- **First boot:** "Call to invalid or unregistered function at guest address 0x8252E1F8" 3 s in; the analyzer had that start inside the 12-byte sub_8252E1F0. Registered in `config/default.toml`, the game boots through the logos and the title into the opening movie.
- **27 more late starts:** the same shape searched in the whole code (a dump of guest memory with the input script's `save`): right after a `blr` an instruction that is no known start, followed by one that is. Registered too, so they don't stop the game when it first calls them. Next: the first level and the portal, then the opening movie skip.
- **Logo movie skip** (from TrapTeamRecomp): a midasm hook at 0x831C7CC8 right after BinkOpen reads the 44-byte header into r1+112; the Activision (226 frames) and Vicarious Visions (270) bumpers are cut to one frame, the opening movie FMV01.bk2 (3905) with skip_opening_movie. Checked: the title at 8 s.
- **First level** (script, `skip_opening_movie=true`): after the save slot the game asks for a land vehicle on the portal and offers its loaner ("Instant Hot Streak", taken with A), then for a Skylander: Chop Chop (a Spyro's Adventure figure, put on by script) comes in with his intro and drives Hot Streak through the opening vortex ("Mantén RT para acelerar"). Next: the portal menu with this game's vehicles and trophies, the prompt watch.

## 2026-10-21: session log (morning)

- **The game's pictures in the portal menu** (src/game/toy_images.cpp): permanent.pak holds one
  toy data file per toy (ToyData/<Name>_ToyData.igz, IGZ version 9): the toy id at +0x08 of every
  "...ToyData" object, a handle to its collection material at +0x20, and its variants as
  CVariantIdentifier objects (deco +0x08, year +0x0C, flag bytes LightCore / alternate deco /
  Series 2 at +0x10, material handle +0x1C), which give the portal catalog's variant numbers
  exactly (Eon's Elite Chop Chop: year 3, Series 2, deco 16). ToyCollectionMaterials.pak has the
  materials and their textures (88x88 DXT5 icons). The readers are SwapForceRecomp's, taken over
  for IGA version 11 and IGZ version 9 (src/game/alchemy). 240 toys, 261 variants, 469 icons,
  read in 157 ms at start-up. Checked by script in the first level: the Air Skylanders of Spyro's
  Adventure, Swap Force's Magic ones (with swappers), Trap Team's (Eon's Elite Spyro, a Mini),
  SuperChargers' and the magic items show their pictures.
- **The portal menu opens by itself** (src/game/magic_moment_watch.cpp, Swap Force's watch with
  this game's values): the CMagicMoment class is the same as in Swap Force with more fields.
  Its field names and u16 offsets come from the executable (Ghidra project imported this
  morning: names at 0x820B5FF8, offsets right after at 0x820B62C0; state +0x2C, reason +0x168,
  error type +0x1F8, player +0x1FC), the enum names too (eMMS_ states with Idle 5 and Error 17,
  eMMR_ reasons with VehicleRequired 13), and the instance pointer (0x83764ED0) from snapshots of
  .data and the physical heap at the prompt and in play. Checked by script: the menu came up by
  itself at "Coloca un Skylander", A and A put Double Trouble on, and the game went on into the
  portal tunnel with Double Trouble's portrait in the HUD.
