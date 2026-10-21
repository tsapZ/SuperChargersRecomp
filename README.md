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
| Portal | ✅ emulated portal; the portal menu opens by itself and shows the game's collection icons |

## German from your PS3 disc

The Xbox 360 disc this port is made for has Spanish and Italian only. If you also own the PS3
disc of the game, `tools/german_text.py` puts its German text (and with `--voices` its German
voices) into your extracted 360 disc (`rom/`), as in ImaginatorsRecomp:

    python tools/german_text.py --ps3 "D:\path\to\the PS3 disc.iso" [--voices]
    python tools/german_text.py --restore        (back to the disc's Spanish)

It needs 7-Zip for the PS3 .iso and Python 3. The title screen, the licence agreement and the
Xbox 360's own messages become German too. Not tried with this game's PS3 disc yet.

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
