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
| Languages | this disc has **Spanish and Italian only** (347 language files each, `*_es.pak` / `*_it.pak`); German text and voices from the owner's English / German 360 disc with tools/german_text.py (`--xbox360`, applied to this machine's rom/) |

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
- **Native 3D picture, experimental** (src/hooks/scene_size_hook.cpp, `scene_size`, Graphics page,
  off by default): the 360 draws the 3D scene at 1040x620 (mainScene in standard_renderer.igz in
  LooseFiles.pak) and stretches it to 720p. SwapForceRecomp's hook, the same render code: the target
  size function (0x826E8718) and the three EDRAM places (planning heap, tiling decision, depth base)
  found in a dump of this game's code by their instructions. With it on the scene is drawn at
  1264x704 with the SDK's 4096-tile EDRAM (D3D12). Checked by script up to the portal tunnel with a
  figure: the target goes 1040x620 -> 1264x704, the scene draws right, the edges about 12 % sharper
  (mean gradient of the same frame), no crash.
- **Ultrawide** (src/hooks/ultrawide_hook.cpp, `ultrawide_fov`, on): a window wider than 16:9
  (`resolution`) shows more to the sides instead of black bars, as in SwapForceRecomp. The
  perspective builder (sub_8250E610) found by Swap Force's instructions in a dump of this game's
  code; the scene camera's call with a probe, a hook at each of the builder's callers logging its
  arguments in play: 0x82B0BCA0 runs every frame with 16:9 and the camera's horizontal FOV. The hook
  widens the aspect and the FOV there, keeping the vertical FOV; the app turns the letterbox off.
  Checked by script: the portal tunnel in a 2560x1080 window: the vehicle's proportions as in 16:9
  (width / height 1.42 in both), more of the tunnel at both sides; the HUD stretched.
- **German from the owner's English / German Xbox 360 disc** (tools/german_text.py `--xbox360`,
  `--language de|en`): that disc ("Skylanders - SuperChargers (Europe) (En,De)") has the same
  2748 archives, with a German and an English twin of every Spanish file, but its default.xex is a
  different build (the same size, other function addresses, e.g. 0x82698078 there for 0x82698108
  here, checked by running codegen on it), so this port stays on the Spanish / Italian xex and
  takes only the files: 1146 text files in 362 archives, the licence, and 10833 voices in 341
  voice archives, the game's own XMA banks (no conversion as with a PS3 disc), in 9 s. The 360
  platform strings come as a twin too ("Drücke Start"). On copies the tool was applied and undone
  (`--restore`, identical md5). Checked by script: the menus, "Sofort-Skylander verwenden" /
  "Level neu starten" at the portal and "Halte RT zum Beschleunigen!" in the tunnel, no error.
- **The portal menu's pictures the right way up and sharper** (src/game/toy_images.cpp): the
  collection icons are stored upside down, which the menu showed (the owner saw it, 2026-10-21; the
  code's comment had called them upright). They are flipped when read. The disc has only 88x88 of
  each figure (the HUD portraits, ui/HealthHud_2015/Portraits, are the same size), shown about twice
  as large, so the first time the menu asks for an icon it is enlarged 2x with a Lanczos-3 filter
  (on premultiplied alpha) and kept. Checked by script: Double Trouble and the magic items upright
  and crisper.
- **60 FPS and more** (src/hooks/frame_rate_hook.cpp, `present_interval`, F4 > Bildrate, off by
  default): the game swaps with presentation interval 2 like Swap Force (30.0 FPS with
  frame_rate_limit 60, 72.0 with 144); Swap Force's hook on the D3D library's swap (0x828777D8,
  found by its instructions) sets 1. With it, 144.2 FPS with frame_rate_limit 144 in play, and the
  game keeps its speed: the magic moment's states around a figure's arrival last as long as at 30
  FPS (the figure's intro 7.34 against 7.45 s, the portal sequence 5.88 against 5.94 s). The magic
  moment watch logs its state changes at info level now (a few lines per figure change), which is
  how the speed was timed.
- **The menus in the game's own fonts** (src/game/game_fonts.cpp, skylanders-portal b804d9c,
  SwapForceRecomp #54): fonts/ProximaNova-Title.igz (permanent.pak, 133 glyphs, small caps, cells
  146 px) and fonts/ProximaNova-Body.igz (essentialui.pak, 170 glyphs with ä ö ü ß, cells 34 px)
  are igBitmapFont files; their igCharMetrics give each glyph's cell and their atlas pages are
  16-bit Xbox 360 tiled, the first byte the coverage. The portal library draws the menus' titles
  and text with them (Segoe UI fills in the few glyphs they lack, e.g. the title font's ä and ü),
  scaled so their capitals are as tall as Segoe's. Read in 24 ms at start. Checked by script: the
  portal menu's title, names, tabs and hints in the game's letters.
- **The game's element badges and coin in the portal menu** (src/game/toy_images.cpp): the
  menu drew its own; the HUD's element badges (permanent.pak, ui/HealthHud_2015/Portraits/
  Elements, 88x88, all ten elements, the collection screen's medallions) and the ability shop's
  coin (juicedomain_permanent.pak) are handed to it now, stored upside down like the icons.
- **Sharper icons where the disc has them:** 88x88 is all it has of most figures (the bios'
  full-screen pictures the toy data names are not on the Xbox 360 disc), but the collection
  tracker (CollectibleTrackerIcons.pak) has 128x128 copies of 20 of this game's own characters;
  those are taken instead. Every icon is now enlarged 2x (also the 128 ones).
- **The later games' characters in their element groups:** the portal library knew the elements
  of the first three games only, so this game's tab (and Trap Team's) listed Splat, Spitfire, the
  vehicles and the others under none. Its table now has them, read from the games' own files
  (here: the toy data's +0x0C). Checked by script: the SuperChargers tab shows Splat, Soda Skimmer
  and Splatter Splasher under Magic, Smash Hit, Shark Tank and Thump Truck under Earth.
- **The HUD at the window's edges with ultrawide** (`ultrawide_hud`, on; F4 > Grafik > "HUD bei
  Ultrawide"): the owner found the stretched HUD squashed. The UI (Juice) lays itself out on its
  contexts' expected screen width, as in Swap Force (whose hook was found first); the hook sets it
  to the window's width at the UI's height where the sizes are read for drawing, so the HUD and the
  menus keep their proportions at the window's edges. Checked by script in a 2560x1080 window:
  the portal prompt, a level's HUD (the gauge and the portrait at the left edge, the hint
  banner) and the pause menu, nothing left uncovered.
- **Movies on ultrawide** (2026-10-21): the logo movies and the opening movie are 16:9 Bink
  movies; while one plays the presenter shows it with black bars at the sides instead of
  stretched (present_use_actual_aspect off and present_letterbox on for that time, not saved).
  Start: BinkOpen's header hook; end: a hook at the start of BinkClose (sub_831C9F38, called only
  from the game's movie close). Checked by script in a 2560x1080 window with skip_intro off: the
  logos 16:9 in the middle, the screen after them fills the window again.
