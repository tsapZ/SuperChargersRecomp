# Status

Updated at the end of every session. Only record what a build actually did.

## Current state (2026-10-22)

| Area | State |
|---|---|
| Repository | scaffolded from GiantsRecomp: build, app, portal (emulated, USB, hybrid), portal / player / settings menus, achievements, input scripts and test tools; none of its game hooks or file readers |
| Game data | disc extracted into `rom/` (2906 files); two discs run, their `default.xex` hashes pinned (docs/game/discs.md) |
| SDK | our fork `tsapZ/rexglue-sdk`, branch `skylanders`, shared with the other games |
| Codegen | works with 28 `config/default.toml` entries: function starts the analyzer put one instruction late |
| Boot | ✅ logo movies, the title screen ("PULSA START"), "¡Elige tu estilo de juego!" (difficulty), the mode books (Aventura, Carreras), the save slots, then the long opening movie (Start pauses it, doesn't skip it); no crash in 5 minutes |
| Portal menu | ✅ opens with Guide; shows the game's own round collection icons for every Skylander, vehicle, trap and magic item, each figure's variant where the game has one; opens by itself when the game asks for a Skylander, the portal check on the "too many" screens; a vehicle's card shows its terrain (land, sea, sky) with the game's symbol; the details view shows the hat (name, icon, stats) and every Skylander's upgrade tree with the game's pictures and prices |
| Languages | the European **English / German** disc runs as it is (its own German and English); the European Spanish / Italian disc (347 language files each, `*_es.pak` / `*_it.pak`) takes English or German from the En/De disc with tools/add_language.py (called german_text.py before) |

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
  2748 archives, with a German and an English twin of every Spanish file. Its default.xex was
  thought to be a different build here (codegen on it had put function starts elsewhere, e.g.
  0x82698078 for 0x82698108); that was the analyzer without this port's function list, not the
  code -- see the 2026-10-22 entry: the disc now runs as it is. The tool takes only the files: 1146 text files in 362 archives, the licence, and 10833 voices in 341
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
- **The menus in the game's own fonts** (src/game/game_fonts.cpp, skylanders-portal 465b4c1,
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

## 2026-10-22: session log (night)

- **Vehicles' terrain** (efb198f): CVehicleToyData +0x30 (1 land, 2 sky, 3 sea, as the game's three template vehicles say), shown on the card, the preview and the details with the collection screen's symbol and the game's word ("Landfahrzeug", "Seefahrzeug", "Himmelsfahrzeug").
- **Hats in the details view** (src/game/hats.cpp): permanent.pak's CollectibleTrackers.igz (one CHatCollectibleTracker per hat: its number at +0x84, attributes at +0x80, icon material at +0x0C), the names from CollectibleTrackers_es.lng (the German text on the German disc; the English menu makes a name from the hat's object name), the attribute names from systemData.igz translated by systemData_es.lng's hash table (FNV-1a of the English name), the pictures from CollectibleTrackerIcons.pak. Checked on a copy of a Chop Chop with the flower hat: "Blumenhut", its picture, "+2 Rüstung", "+2 Kritische Trefferwahrscheinlichkeit". Log: "266 hats (266 with a picture), 1847 texts, 28 attribute names".
- **Upgrades in the details view** (src/game/toy_upgrades.cpp; shared readers in src/game/game_data.cpp): a character's <Name>_CharacterData.igz (CSkillsUpgradeComponentData), its texts from <Name>_CharacterData_es.lng, the pictures through <Name>_AbilityUpgrades.pak's materials; read the first time the menu asks for a character. Checked on the Chop Chop copy: the whole tree with "Einfache Verbesserungen", "Vampirkrieger", "Gespenstischer Verteidiger", "Seelenstein-Verbesserung", the prices and the descriptions with X/Y/B.
- **Portal library** (83cc22e, 141d32e): German card subtitles in the singular ("Fahrzeug" instead of "Vehicles"); a plain hat symbol where a game has no hat picture.
- **New issues:** #3 bloom and motion blur options, #4 the game's controller picture (it is on the disc: Controller_SchemeXenon.pak, 1024x724).

## 2026-10-22: later the same night

- The settings example and the installer's template set skip_intro twice (an invalid file); fixed, and the example says 60 FPS.
- **Settings file read even with a repeated key** (SDK fork 8761248): a key set twice in a table makes the whole TOML file invalid, and the game then ignored every setting in it without a visible error (the error went to a log that didn't exist yet). The SDK now reads such a file with the repeats left out and logs which key was repeated.
- **`--settings_schema=<file>`** (skylanders-portal 4521ced): the game writes its F4 menu's pages with their values and quits before a window opens; the Core of Light launcher shows them as the game's settings and writes into this game's settings file.
- **Figure pictures for the launcher** (skylanders-portal e17786b): while the game runs, every figure picture it reads from the disc goes into `Documents\Skylanders\Pictures\<game>` once, for the launcher's figure library; later the same night (skylanders-portal 6c3c6a3) also the hats it knows (hats.txt, their pictures) and the upgrade trees of the figures in the figure folder; and
(skylanders-portal f571d26) the game's achievement list with the title's own pictures, for the
launcher's achievements screen.
- **Bloom strength (F4 > Grafik > Leuchten (Bloom)), as in Swap Force:** SuperChargers keeps
  its bloom in the level's environment settings (some 200 reflection fields; `_bloomScale` at
  +0x3B0). A midasm hook at the end of the zone blend (`sub_82BD0C30`, 0x82BD389C) scales the
  blended value, a second one after the per-zone blend (`sub_82BD38A8`) keeps the setting in
  zones with their own bloom (`src/hooks/post_effects_hook.cpp`). Checked on the title screen:
  at 0 the glow around the logo is gone, at 3 it is much stronger.
- **Motion blur (F4 > Grafik > Bewegungsunschärfe), as in Swap Force, off by default:** the
  environment class lost its exposure in development, so the strength comes from the motion blur
  material's `ig_motion_blur_exposure_params` (+96, 0.125 / -0.125 as loaded). A midasm hook at
  the start of its parameter setter (`sub_8360C7E8`, 0x8360C7E8) writes the game's value times
  the setting. Checked on the tunnel ride with the throttle held: at 2 and 4 the gear bits are
  visibly streaked, at 0 the picture is a little sharper than at the game's 1 (#3).
- **Skylander-Vorstellung: Kurz (F4 > Spiel), as in Swap Force (SwapForceRecomp #60):** the
  figure's intro after the placement ends after 1.5 s. The intro's time check (`sub_829D7B78`,
  0x829D7CB0) alone doesn't end it: the magic moment's update in state 6 (`sub_82A135B8`) waits
  for the figure's intro animation (`sub_82E08770`); a write watch on the state (+48) found that.
  The hooks shorten the time and, once the intro ran 1.5 s, answer "over" for the animation
  (0x82A137DC, 0x82A136D8), so the game's own ending runs. Checked in the first level with Chop
  Chop at the prompt: the tunnel ride starts about 3 s after the placement instead of 6 s, camera
  and HUD as usual.
- **The English / German disc runs as it is** (docs/game/discs.md): its default.xex and the
  Spanish / Italian one were disassembled by codegen and compared at every address: 4,810,671
  instructions, all the same (the same build, changelist 2785777; the earlier "other function
  addresses" came from codegen without this port's `[functions]` list). Both hashes are pinned
  (`docs/game/default.xex.sha256`, one per line; the game, the installer's check tool and the
  launcher accept either), the intro skip knows the En/De disc's opening movie (3905 frames), and
  the hats' and upgrade trees' texts come from the `_de` files, else from `_es`. Checked by script
  on the En/De disc with copied user data: hash check passed, the opening movie skipped, menus and
  the first level in German ("Halte RT zum Beschleunigen!"), the hats with German names (1847
  texts).
- **Nothing skipped by default** (the owner's choice, 2026-10-22): `skip_intro` is off, the logo
  movies play as on the console (the option skips them); the settings templates say so too. The
  16x texture filter now comes from skylanders-portal (`ApplyGraphicsDefaults`), so the Core of
  Light launcher shows 16x as the game uses it.

## 2026-10-23: session log (night)

- **Title music from the disc** for the Core of Light launcher (skylanders-portal 6e5c284,
  rexglue-sdk 63e87be): `--export_title_music=<file.wav>` decodes the track the title screen
  streams (found with a run logging the game's file reads) from XMA2 with the SDK's FFmpeg and
  quits before a window opens, in under a second; each normal start leaves `title_music.wav` in
  the game's picture folder once. Checked: INTER_Main_Theme of archives/INTER_Streamed_bank.pak (80.6 s); identical to FFmpeg 8.1's own decode of the
  track (-63 dB).
- **Leaving the game with the pad:** F4 > Spiel ends with "Spiel beenden" and "Spiel neu
  starten" (both ask, "Nein" first; restart not offered when the launcher started the game; the
  new instance waits for the old one), the player menu is a row on the Portal page, the settings
  menu's buttons have no "Aus" any more (it locked a pad out), "Alle Einstellungen" says it is for
  keyboard and mouse (skylanders-portal 5fff7aa).
- **HD texture packs:** rexglue-sdk 37d3e77 dumps the game's BC1-BC3 textures
  (`texture_dump_dir`) and loads `<hash>.dds` replacements of any size (`texture_replace_dir`);
  a pack in `Documents\Skylanders\Textures\SuperChargers` is used by itself (skylanders-portal 94175f0).
  Checked in Swap Force's hub (see SwapForceRecomp docs/texture-packs.md, #62).
- **F4 > Grafik:** "Schärfe-Stärke" (CAS's additional sharpness) and "Farbverläufe glätten
  (Dithering)", dithering on by default.
- **Installer:** its own AppId (all six shared one); `/NOROM=1` installs without the disc for the
  launcher (the settings file is still written); reasons a silent install stops go into its log.
  Silent install from the owner's ISO, start (German title screen), silent uninstall: checked.
- The language list offers the two European discs' languages only (no French).
- **German names of the magic items and traps** in the portal menu (it showed the catalog's
  English ones): from the toys' own text files next to their toy data
  (ToyData/<Name>_ToyData_de.lng, else _es; "Kampfhammer", "Schicksalshand", "Sonnenkratzer
  (LightCore)"). The German disc keeps the Skylanders' and vehicles' English names, so those stay.
  Checked by script in F6 > Magische Objekte (91 names).
- **Long falls fixed (`fix_stuck_falling`, on), as in Swap Force:** the player's jump component
  (sub_82F949B8) counts a fall as stuck when the player moved less than 20 units since the last
  frame, and after 3 s of that puts the player back. 20 per frame was tuned for 30 FPS. Measured in
  the first level after the rift ride, the player launched with the jump component's bounce pad
  fields: without the fix a launch at 5000 and one at 8000 ended with the player put back after
  3 s even at the default 60 FPS (normal jumps counted as stuck 0.42 s), at about 130 FPS too; with
  the fix (threshold times 30 x the frame time) at about 110-130 FPS never, normal jumps 0.23-0.29 s.

## 2026-10-24: 0.1.0-beta.7 (a colleague's evening on SteamOS)

- **Ultrawide by itself in fullscreen** (skylanders-portal bd1b318): the ultrawide camera and the
  presenter take the aspect from `resolution`, so "Fenstergröße: Automatisch" stayed 16:9 on a
  21:9 screen. `ResolutionFromScreenIfAutomatic`, first thing in OnPreSetup, sets it for the start
  to the primary screen's mode (nothing written; not for the launcher's schema).
- **Window sizes** add 2560x1080 and 3440x1440 (21:9), 3840x1600 (24:10) and 5120x1440 (32:9).
- **"Tastatur spielt als"** is greyed and can't be changed while "Tastatur und Maus" is off
  (skylanders-portal 10a00cd, `enabled_by`, also in the schema for Core of Light).

## 2026-10-24: 0.1.0-beta.8

- **Later games' versions in the portal menu's version choice** (skylanders-portal 202b4df): Y on a
  Skylander lists its versions from the later games too (Spyro in Spyro's Adventure: Series 2 and
  3, LightCore, Eon's Elite; the game plays them by their id as its own; the owner's wish). The
  tabs stay this game's own; "Figuren späterer Spiele" now only adds the later games' tabs.
- Ultrawide checked in a 2560x1080 window from the title to the first menus in Spyro's Adventure,
  Giants, Swap Force, SuperChargers and Imaginators (menus centred, nothing stretched, movies with
  bars); Trap Team's window couldn't be captured here (black at 16:9 too), so not by picture.

## 2026-10-24: 0.1.0-beta.9

- **One data folder for every game** (the owner's call; skylanders-portal data_folder.h): the saves,
  achievements and shader cache are in `Documents\Skylanders\SuperChargers` now, next to Figures and
  Pictures, or in the folder picked in Core of Light (`%APPDATA%\Skylanders\data-folder.txt`). The
  first start copies the old `Documents\superchargersrecomp` there (complete or not at all) and leaves a
  note in it; Core of Light asks before the old folder goes. `--user_data_root` works as before.
- **Keyboard keys changeable in F4** (SwapForceRecomp #63): a "Tastatur" page after "Steuerung", a
  row per controller button; A and then the key. Y also saves with a German keyboard now (the
  menu reads keys by their US place, where that key is Z).
- The portal menu's Portal of Power: a band of light round its side, ridges on the rim, rings of
  light running out over the floor (SwapForceRecomp #57).
- Checked: every game boots to its first menus on fresh scratch data with the new data folder
  code (with --user_data_root, so the copy itself was checked in the launcher on a scratch
  Documents).

## 2026-10-25: 0.2.0-beta.1

- **Vehicles next to the Skylander** (#6; skylanders-portal 2278b99): each player's vehicle goes
  in a slot of its own and no longer lifts the player's Skylander.
- **The old sidekicks are Minis here** (Core of Light #32): they go on as a player's Skylander.
- **Bowser, Donkey Kong and their vehicles hidden** (ImaginatorsRecomp #7): the 360 disc doesn't
  have them; `portal_menu_nintendo = true` lists them.
- **One "Bildrate" row as in the other games**: the "60 FPS und mehr" switch is gone.
- Checked: Splat placed, then Soda Skimmer: the vehicle in slot 1, Splat stays.
- **Mouse wheel in the menus** (SwapForceRecomp #65; skylanders-portal ccc7009): F4's rows, the
  portal menu's cards and its variant list scroll a row per notch; the chosen row stays, and the view
  follows it again once the keys or the pad move it.
- **A chosen 4x texture filter stays 4x** (Core of Light #21, a colleague's report and fix;
  skylanders-portal 23be5f2): 4x is the SDK's own default, so it counted as unset and became 16x.

## 2026-10-25 (night): portal requests

- **The vehicle request opens the menu** (#8): CMagicMoment Idle with no player
  (_currentPlayerId -1, reason 13 VehicleRequired) where a Skylander prompt names the player. The
  menu opens on a "Fahrzeuge" tab with a row per terrain ("Himmelsfahrzeug", "Landfahrzeug",
  "Seefahrzeug", the game's names) without lifting the Skylander, and the vehicle goes on at once;
  it waited for the game to stop being busy before, 10 s. Checked on a copy of the owner's save:
  the request at load, Buzz Wing picked, the game on to its vehicle intro 0.5 s later.
- **A Skylander's death opens the menu too**: the Dead state (14) with the player's id.
- **Vehicles with versions ask which one** (#10): Soda Skimmer / Nitro Soda Skimmer.
- **Hats** (#9): the SuperChargers hat slot is the number minus 256 (Enigma's 13 = 269 "Gold
  Rusher-Zahnradkappe", not 268 "Shark Tank-Zylinder"); vehicles show no hat.
- Test runs use a short junction to the scratch data: with the full path the options save passed
  Windows' 260 characters and the game called it damaged (not a problem of the game).

## 2026-10-25: 0.2.0-beta.2

- The setup's version for what came since 0.2.0-beta.1: the vehicle request and a Skylander's death open the portal menu (#8), versions to choose (#10), hats (#9). Also in every game: the portal overview (Back + Y / F5: what is on the portal, taken off one by one without lifting the
  Skylander) and F4 > Controller (button jobs, vibration strength per player; not yet tried with a
  real controller), skylanders-portal f2c4940 / 6e96c16; an A press right after the menu opened by
  itself no longer picks a figure (SwapForceRecomp #72); one bad command-line option no longer drops
  the others (rexglue-sdk 605000d).

## 2026-10-25: afternoon (after 0.2.0-beta.2)

- **German movies** (#9): the game plays spoken track 5 (BinkSetSoundTrack logged with tracks
  0, 1, 2, 3, 5 with German chosen), which on this En/De disc is English; 6 is German.
  `tools/german_text.py --movies` now copies track 6 into 5 (every frame checked first, the original
  kept for `--restore`). Applied to the dev rom: 46 movies, a second run changes none.
- **A Skylander dies** (state 14): the figure is marked tired as the menu opens.
- **The portal overview holds the game** while it is open (rexglue-sdk adf2a90, a frame gate in
  VdSwap: the guest waits before handing over its next frame while the menus keep drawing;
  skylanders-portal ab11cbc). It closes when the portal menu opens and after a vehicle is taken
  off there (the game's next request took the Skylander off with the same A, SuperChargersRecomp).
  Its cards stand just above the portal, and the portal's band of light goes all the way round.
- **Tired Skylanders** (skylanders-portal `PlayerSkylanderTired`): when the game says a player's
  Skylander is out of health, the menu opens and that figure gets a red cross until it has been on
  the portal a while again (a new level). It stays pickable, so a wrong guess can't lock a figure.
- **A hat shows only in a game that knows it** (Enigma's SuperChargers hat showed as "Hut Nr. 269").
- **F4 opens on "Spiel"**, which now has the game's **volume** (0-100 %, SwapForceRecomp #68) and
  the **frame rate display** (the SDK's F3 window, Core of Light #28). **F4 > Tastatur** names each
  key as this PC's keyboard prints it ("Ö / Leertaste", "Umschalt+Pfeil hoch") and has "Alle
  Tasten zurücksetzen" (SwapForceRecomp #66). skylanders-portal 218defd.
- The "who's talking" circle sits higher, above the game's health bar.
- **Settings descriptions broken into lines** (SwapForceRecomp #69): one line on what a setting does,
  the choices on lines of their own, notes ("Kostet ...", "Braucht D3D12") after a blank line;
  the same wording in every game where the setting is the same. The Back + Y row is called
  "Portal-Übersicht" now (it opens the portal overview since skylanders-portal f2c4940).
  skylanders-portal 2a97055.
- **The vehicle the game wants, on its terrain** (the owner, 2026-10-25): the CVehicleManager
  singleton (0x83771990) keeps the wanted terrain (+0x54: 1 land, 2 sky, 3 sea) and element
  (+0x50), set by a gate or the level script. The vehicle request and the "wrong vehicle" error
  screen (_errorType 11 / 10) open the menu on that terrain's row and that element's card; the
  wrong-vehicle screen no longer opens the portal check. Found from the class's field
  registration; not yet seen at a gate.
- **Hints in the pad's own buttons** (Core of Light #29, skylanders-portal 39ab210): after a press on a
  PlayStation pad the menus' hints show its symbols and L1 / R1 / Options, on a Nintendo pad its
  letters in their places and L / R / ZL / ZR / +; Xbox-like pads stay as they were.

## 2026-10-25: 0.2.0-beta.3

- The setup's version for what came since the last one: German movies in the track the game plays (tools/german_text.py --movies, #9) and the vehicle menu on the terrain the game wants. Also in every game: the portal overview holds the game while open, a Skylander out of health gets the menu and a red cross, hats only where the game knows them, F4 opens on "Spiel" with volume and frame rate rows, key names as the keyboard prints them and a reset row, descriptions in short lines, hints in a PlayStation or Nintendo pad's own buttons, and figure changes with an HD pack without the stutter (skylanders-portal 39ab210, rexglue-sdk ce9dbb6).

## 2026-10-26: 0.2.0-beta.4

- The setup's version for what came since the last one: tools/german_text.py is tools/add_language.py now (the old name still works). Also in every game: no crash at start when the window moves to another monitor (rexglue-sdk 07f1106), and the portal menu no longer lifts a Skylander the game is still loading from the portal (skylanders-portal d4ce7b6).

## 2026-10-26: night

- rexglue-sdk 90f3bad: an XMA input counts as used up only once its output is out (the cut-off ends of Trap Team's portal lines, TrapTeamRecomp #15).
- skylanders-portal 8cbf30d: the picture cache also leaves the game's element symbols for the launcher's figure library (SkylandersCoreOfLight #38). Checked: all ten.
