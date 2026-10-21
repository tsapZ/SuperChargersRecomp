# SuperChargersRecomp

An unofficial native Windows port of **Skylanders: SuperChargers** (Xbox 360), made with static
recompilation (ReXGlue). Work in progress: it plays the first level with a figure on the emulated portal.

You need your own copy of the game: nothing from the game is in this repository.

## Status

See [docs/STATUS.md](docs/STATUS.md) and [docs/roadmap.md](docs/roadmap.md).

| Area | State |
|---|---|
| Build | ✅ codegen and build |
| Boot | ✅ menus and the first level |
| Portal | ✅ emulated portal; the portal menu (Guide) shows the game's collection icons |

## Building

The same as the other Skylanders ports: see [docs/development.md](docs/development.md).

## Credits

- **[GiantsRecomp](https://github.com/tsapZ/GiantsRecomp)**, **[SSARecomp](https://github.com/tsapZ/SSARecomp)**
  and **[SwapForceRecomp](https://github.com/tsapZ/SwapForceRecomp)**: this project is scaffolded
  from them; app, portal, menus and tooling come from there.
- **[GiantsRecomp by TheBiemGamer](https://github.com/TheBiemGamer/GiantsRecomp)** brought
  Skylanders Giants to PC first and is the template these ports started from.
- **[ReXGlue](https://github.com/rexglue/rexglue-sdk)**, the static recompilation SDK, here in a
  fork with our fixes on top.
- **[extract-xiso](https://github.com/XboxDev/extract-xiso)** is included (prebuilt, unmodified)
  for the installer (`installer/extract-xiso.LICENSE.txt`).

Skylanders is a trademark of Activision. This project is not affiliated with or endorsed by
Activision.
