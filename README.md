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
| Portal | ✅ emulated portal; the portal menu opens by itself and shows the game's collection icons, hats and upgrade trees |
| Settings | ✅ F4 menu like the other games': native 3D picture, the game's FXAA at the internal resolution, bloom and motion blur strength, ultrawide with the HUD at the edges, the controller picture, the shorter Skylander intro |

## Which disc

Two European Xbox 360 discs run (the same build; [docs/game/discs.md](docs/game/discs.md)):

| Disc | Languages |
|---|---|
| Skylanders - SuperChargers (Europe) **(En,De)** | English and German, text and voices: the disc to use |
| Skylanders - SuperChargers (Europe) (Es,It) | Spanish and Italian; German or English can be added from the En/De disc (below) |

Other regions (USA and others) are refused at start: their executable differs, and the port's
addresses are made for this one.

### German (or English) on the Spanish / Italian disc

If you own the Es/It disc and also the En/De disc (or the PS3 disc), `tools/german_text.py` puts
the other disc's German text and voices into your extracted Es/It disc (`rom/`); the game then runs
in German throughout. With the En/De disc the voices are the game's own Xbox 360 banks.

    python tools/german_text.py --xbox360 "D:\path\to\the En-De disc.iso" --voices
    python tools/german_text.py --ps3 "D:\path\to\the PS3 disc.iso" [--voices]
    python tools/german_text.py --restore        (back to the disc's Spanish)

`--language en` puts in English instead. It needs Python 3, and extract-xiso (in `installer/`) for
an Xbox 360 .iso or 7-Zip for a PS3 .iso; an already extracted disc folder works as well. The title
screen, the licence agreement and the Xbox 360's own messages change too. With the En/De disc
itself none of this is needed: extract it into `rom/` and play.

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
