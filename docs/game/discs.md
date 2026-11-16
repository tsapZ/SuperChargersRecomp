# Supported discs

The port runs every Xbox 360 disc whose `default.xex` has one of the SHA-256 hashes in
`default.xex.sha256` (one per line), or the code pin in `default.xex.code`; the build, the game's
own check at start, the installer's check tool and the Core of Light launcher all read both.

| Disc | Languages | `default.xex` SHA-256 | Notes |
|---|---|---|---|
| Skylanders - SuperChargers (Europe) (En,De) | English, German | `02ff6672d460c9aaa54e69d584af67a7138c6c93cde5e04b0da4284207be7c61` | the disc to use: German and English text and voices are the game's own |
| Skylanders - SuperChargers (Europe) (Es,It) | Spanish, Italian | `02846647633e93c627e74b92d3ad63a90e1504546958fc8d81e28ac65de1f9bc` | the disc the port was first made with; German or English from the En/De disc with `tools/german_text.py` |
| Skylanders - SuperChargers (Europe) (Fr,Nl) | French, Dutch | `c6b1d7a32195ace94f31a49d750b06b01c817ed70ce3a41f3d977d3d80241f10` | XEX version `0.0.0.4`, `default.xex.code` pin `82450000:d379ae251fb9336a3ea4aadd46bfb6f0a51d5bef2cbc26b704e3c43c95234f7f`; French and Dutch text and voices (Dutch via `XLanguage::kDutch` / `user_language = 16`) |

## Why all three run

The En/De and Es/It discs carry the same build (changelist 2785777 in `version.xml`, the same size
of `default.xex`). Their executables were compared instruction by instruction (codegen's
disassembly of each, numbers left in): 4,810,671 instructions at the same addresses, every one the
same; the hashes differ only outside the code. So every hook address in `config/default.toml`
holds for both, and nothing is rebased.

The Fr/Nl disc is XEX version `0.0.0.4` from the same changelist `2785777` (`24,985,600` bytes).
Because `skylanders::xex::CodePin` hashes every XEX2 page descriptor from `0x82450000` through the
end of the page table, its code pin (`82450000:d379ae25...`) differs from the En/De and Es/It one
(`82450000:3140330d...`). Recompiling `(Fr,Nl)`'s `default.xex` (`generated/default/`, 639 files)
confirmed all 28 `[functions]` splits and all 26 `[[midasm_hook]]` instruction sites in
`config/default.toml`, `setjmp`/`longjmp`, and the guest functions and globals read by `src/hooks/`
(`where_hook`, `figure_intro_hook`, `god_mode_hook`, `ultrawide_hook`, `post_effects_hook`,
`scene_size_hook`, `frame_rate_hook`, `fxaa_scale_hook`, `attribute_cheats_hook`),
`src/game/magic_moment_watch.cpp` (`0x83764ED0`, `0x83771990`), and `src/localization.cpp`
(`sub_83567248`, `sub_824D7E10`) sit at the same addresses with matching PPC instructions, and that
the `Bumper_ATVI.bk2`, `Bumper_VVisions.bk2`, and `FMV01.bk2` headers match `intro_skip_hook.cpp`.

What differs is the data: each disc carries `_de`/`_en`, `_es`/`_it`, or `_fr`/`_nl` twins of the
localized `.lng` and voice archives (`FMV01.bk2` also differs in frame count between En/De and
Es/It; the Fr/Nl header matches `3905` frames). When running `(Fr,Nl)` without importing German or
English strings via `tools/add_language.py`, the Discord level list and portal-menu hat/upgrade
labels fall back to catalog/internal names because only `_de`/`_es`/`_en` text files are parsed.

Checked on the En/De disc by script (copied user data, the software portal): the hash check
passes, the opening movie is skipped when asked, the menus and the first level in German ("Halte
RT zum Beschleunigen!" in the tunnel), the hats in the portal menu with their German names
(1847 texts from `CollectibleTrackers_de.lng`).

Checked on the Fr/Nl disc by scripted headless Vulkan runs (`--portal_mode=software`) in both
French (`--language=french`) and Dutch (`--language=dutch`): startup XEX verification passes, logo
bumpers and `FMV01.bk2` skip (`--skip_opening_movie=true` and Y prompt), the Rift tunnel (`Intro`)
plays through to the Flynn/Cali/Hugo transition, and `Intro1` ("Skylands Enchaînés — Chapitre 1" /
"Skylands in Ketenen — Hoofdstuk 1", `Where: intro1`) loads with on-foot movement, attacks, coins,
jumping, save/figure backups, `MagicMomentWatch`, `FigureIntro`, `UltrawideFOV`, FXAA, cheats,
Achievements, Discord status, and the F4/F6/F7 menus at ~60 guest FPS. Further levels (`Academy`
and `Nat1` onward) were not played through in this check.

## Other discs

Any other Xbox 360 pressing whose code pin matches one in `default.xex.code` runs as well, without
its hash here. The pin is where the code loads and the SHA-256 of the XEX header's page table from
there on; the table holds a SHA-1 of every run of loaded pages (En/De and Es/It share one pin,
and Fr/Nl has its own), so it needs no decryption. A different build (USA and other regions, most
likely) is refused at start with its title, region and hash in the message, and needs its own
address map. The PS3, Wii U and Xbox One discs aren't Xbox 360 executables at all.
