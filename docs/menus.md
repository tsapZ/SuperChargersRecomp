# Menus and keys

Everything the port adds on top of the game, reachable with a controller or the keyboard. The
menus come from the `feature/portal-menu` branch of the skylanders-portal fork
(`thirdparty/skylanders-portal/src/portal_rex/`), the same in all six ports; they share one look
and hide all input from the game while they are open. They speak the language the game runs in.

| What | Controller | Keyboard | Setting |
|---|---|---|---|
| Portal menu: put Skylanders, vehicles and items on the portal | Guide (Xbox), Back + Start, hold Back | F6 | `portal_menu_open_buttons`, `bind_portal_menu` |
| Take the item off the portal | Back + Y | F5 | `portal_item_off_buttons`, `bind_portal_item_off` |
| Player menu: which controller is which player | L3 + R3, or F4 > Portal > "Spieler-Menü öffnen" | F9 | `player_menu_buttons`, `bind_player_menu` |
| Settings menu (also over the portal or player menu, which then closes as with B) | Back + RB | F4 | `settings_menu_buttons`, `bind_settings_menu` |
| Achievements | F4 > Spiel > Erfolge | F7 | `bind_achievements` |
| Quit or restart the game (both ask first, "Nein" chosen) | F4 > Spiel > "Spiel beenden" / "Spiel neu starten" | Alt+F4 | |
| All settings (developer list of the SDK) | from the settings menu, last page | F10 | `bind_settings` |
| Portal developer tools (put any figure on) | | F8 | `bind_portal_overlay` |
| Debug overlay (FPS) | | F3 | `bind_debug_overlay` |

Button entries are comma-separated: a button (`guide`, `back`, `start`, `a`, `b`, `x`, `y`, `lb`,
`rb`, `ls`, `rs`, `up`, `down`, `left`, `right`), a chord joined with `+`, or `hold_<button>`.
System buttons that start an entry (Back, Start, Guide, the stick clicks) are kept from the game
for a moment, so Back doesn't also open the game's own menu; gameplay buttons never are. The
settings menu's own buttons have no "off" choice, so a controller can always get back to it.

## Portal menu

Opens by itself when the game asks a player for a Skylander (see [portal.md](portal.md)), or with
the buttons above. It lists the figures of Spyro's Adventure, Giants, Swap Force, Trap Team and SuperChargers by game and element, with this game's own
pictures, read at run time from the owner's disc:

- **Pictures:** the round icons of the game's collection screen for every toy and variant, the
  sharper copies of the collection tracker where it has them (`src/game/toy_images.cpp`); the
  element badges, the game's coin, and for a vehicle its terrain with the collection screen's
  symbol ("Landfahrzeug", "Himmelsfahrzeug", "Seefahrzeug").
- **Names:** the magic items and traps as the German game calls them ("Kampfhammer",
  "Sonnenkratzer (LightCore)"), from each toy's own text file next to its toy data
  (`src/game/toy_images.cpp`); the Skylanders and vehicles keep their names, as in the game.
- **Details (X):** the upgrades with the game's names, descriptions, prices and pictures, the
  bought ones marked (`src/game/toy_upgrades.cpp`); the hat with the game's name, picture and
  bonuses (`src/game/hats.cpp`).

The full description of the menu (its layout, the details view, the options on Start) is in
SwapForceRecomp's [menus.md](https://github.com/tsapZ/SwapForceRecomp/blob/main/docs/menus.md).

## Portal check

Opens by itself on the game's "too many" screens (`CMagicMoment` state 17, an error): it lists
the figures on the emulated portal, A marks one to take off, Continue takes them off. See
[portal.md](portal.md#portal-check).

## Settings menu

`portal_rex/settings_menu.cpp` draws the menu; `src/settings_pages.cpp` describes its pages:

- **Graphics:** preset (Performance: scale 1 + FSR; Quality: scale 2 + CAS; Best: scale 3 + CAS),
  internal resolution (`resolution_scale`), the native 3D picture (`scene_size`, experimental),
  sharpening (`present_effect`) and its strength (`present_cas_additional_sharpness`), smooth
  gradients (`present_dither`, on), texture filtering (`anisotropic_override`), the game's own
  edge smoothing kept sharp (`fxaa_follow_resolution_scale`), extra edge smoothing
  (`swap_post_effect`), glow (`bloom_strength`), motion blur (`motion_blur_strength`),
  fullscreen, window size.
- **Frame rate:** 60 FPS and more (`present_interval`), frame rate limit (`frame_rate_limit`),
  VSync, render path, graphics card, graphics interface (D3D12 or Vulkan).
- **Game:** language (automatic, English, German, Spanish, Italian: what the two European discs
  have), skip the logo movies, skip the opening movie, the figure intro ("Kurz":
  `figure_intro_seconds`), achievements, quit and restart the game.
- **Portal:** virtual / both / USB / none (`portal_mode`), the portal menu opening by itself, only my
  figures, later games' figures, the player menu.
- **Controls:** the menu buttons, each with a few choices that don't clash, keyboard and mouse,
  which player the keyboard is; the info panel shows the buttons on the game's own controller
  picture.
- **Advanced:** opens the SDK's list of every setting (F10, keyboard and mouse).

LB / RB or LT / RT switch the page, up / down pick a setting, left / right (or A) change it, B or
Start close. **Y saves** into `superchargersrecomp.toml` (only the changed lines). Settings that only apply at
start-up say so and get a gold dot when changed. For test runs `--config_file=<path>` makes the app
use another settings file.
