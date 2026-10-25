# Portal

The Portal of Power comes from the portal library (`thirdparty/skylanders-portal`, our fork's
branch `feature/portal-menu`), the same code in all six ports. Its protocol, the figure files and
the menus are described in SwapForceRecomp's
[portal.md](https://github.com/tsapZ/SwapForceRecomp/blob/main/docs/portal.md); this page covers
what is particular to SuperChargers.

## Modes

`portal_mode` (F4 > Portal):

| Value | What |
|---|---|
| `software` | the emulated portal, filled from the portal menu (default) |
| `hybrid` | the emulated portal and the real USB portal together |
| `usb` | the real USB portal only |
| `none` | no portal |

The portal menu lists the figures of Spyro's Adventure, Giants, Swap Force, Trap Team and SuperChargers (`portal_menu_games`).

## Figures

`Documents\Skylanders\Figures`, one folder for every game (portal fork `SharedFiguresDir`), with a
subfolder per game. Test runs with a copied `--user_data_root` get their own folder next to the
copy.

## The menu at the game's portal screens

As in Swap Force, the game runs everything around "place a Skylander on the Portal of Power"
through one engine object, the `CMagicMoment` singleton, with more fields and states for the
vehicles. `src/game/magic_moment_watch.cpp` polls it every 30 ms (only reads):

| Field | What |
|---|---|
| global 0x83764ED0 | the pointer to the instance |
| +0x2C `_magicMomentState` | 3 the camera zooms out to the portal, 5 the prompt is up, 17 an error screen |
| +0x168 `_reason` | why: a toy removed, too many character / vehicle / trap toys, a vehicle needed, ... |
| +0x1F8 `_errorType` | the error screen's kind |
| +0x1FC `_currentPlayerId` | whose prompt: 0 player 1, 1 player 2 |

The menu opens for that player when the prompt has been up for a moment, and closes again when the
game stops waiting and nothing was chosen; the error screen opens the portal check.

The vehicle the game wants is in the `CVehicleManager` singleton (global 0x83771990): +0x54
`_requiredVehicleType` 1 land, 2 sky, 3 sea (0 any), +0x50 `_requiredVehicleElement` (an elemental
gate). A gate or a level script sets them. At the vehicle request (the prompt with no player) and at
the "wrong vehicle" error screen (`_errorType` 11, or 10 for the element) the menu opens on the
vehicles, on that terrain's row and that element's first card, instead of the portal check.

## Portal check

Lists everything on the emulated portal, marks what there is too much of, and takes off what the
player picks; B closes it, and while the error stays it comes back after 8 s. The menu itself never
puts a second item or Skylander on for one player, so this is the fail-safe for figures put on some
other way (a real portal in hybrid mode, the developer tools).
