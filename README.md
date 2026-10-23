# SuperChargersRecomp

An unofficial native Windows port of **Skylanders: SuperChargers** (2015, Xbox 360), made with
static recompilation (ReXGlue). Work in progress: the first level plays with a figure on the emulated portal.

> **Unofficial fan project.** Not affiliated with or endorsed by Activision or Vicarious Visions. This
> repository contains **no game data**: you need your own copy of the game.

## Why this exists

I wanted to play the Xbox 360 Skylanders games on my PC with just a controller, no emulator, and
with a nicer picture. That's really all it is: a private fan project. It's public in case someone
else wants the same.

## How it was made

To be upfront about it: almost all of the code here was written by [Claude](https://claude.ai) in
Claude Code, the recompile setup, the fixes, the portal menu, the launcher and the installer, and
most of the docs too. I decided what to build, said what was broken and played the games a lot. If
AI-made projects aren't your thing, that's fair.

It runs on its own; [Core of Light](https://github.com/tsapZ/SkylandersCoreOfLight) is the launcher
for all six ports if you want them in one place.

## Play it

You need:

- Windows 10 or 11 (64-bit) and a graphics card with DirectX 12
- **your own copy of the game**: the Xbox 360 disc listed under [Which disc](#which-disc), as an ISO
  or an extracted folder (nothing of the game comes with this project)
- a controller (Xbox or compatible; keyboard and mouse work too). A real Portal of Power is
  optional: the emulated portal's menu has every figure

Then:

1. Download from the [Releases](https://github.com/tsapZ/SuperChargersRecomp/releases) page: just this game's
   installer, or [Core of Light](https://github.com/tsapZ/SkylandersCoreOfLight/releases), the
   launcher that downloads and sets up all six games.
2. Start it and choose your ISO. It checks that it is the right disc and sets the game up.
3. Play. When the game asks for a Skylander, the portal menu opens by itself: pick one.

## Status

| Area | State |
|---|---|
| Boot, menus | ✅ logo movies (skippable), title screen, difficulty, save slots, the opening movie |
| First level | ✅ the portal tunnel ride and the level with a figure |
| Rest of the game | 🟡 not played through yet |
| Frame rate | ✅ 60 FPS, 144 and more |
| Portal | ✅ emulated; vehicles |
| Portal menu | ✅ opens by itself; the game's collection icons, a vehicle's terrain, hats and upgrade trees |
| Settings | ✅ native 3D picture, the game's FXAA at the internal resolution, bloom and motion blur strength, the shorter Skylander intro, the controller picture |
| Ultrawide | ✅ the 3D view and the HUD at the edges |
| Installer tested on a clean PC | ⬜ planned |

Where each part stands and what comes next: [docs/roadmap.md](docs/roadmap.md); session details and
known bugs: [docs/STATUS.md](docs/STATUS.md). Part of six ports of the Xbox 360 Skylanders games,
started together from [Skylanders: Core of Light](https://github.com/tsapZ/SkylandersCoreOfLight).

## What it adds

- **Native Windows program**: the game's Xbox 360 code recompiled to x86-64 (no emulator of the
  CPU), the GPU through Xenia's D3D12 path.
- **Picture**: internal resolution 1x, 2x or 3x of the console's 720p, sharpened with AMD CAS,
  16x texture filtering, presets *Performance*, *Quality* (2x, recommended) and *Best*. The game's
  own 3D picture at its native size instead of the console's smaller one is an experimental
  option.
- **60 FPS** (what the installer sets); 30 as on the console, or 144, 200 and more.
- **Ultrawide**: a wider window shows more to the sides, with the HUD at the window's edges.
- **The Portal of Power** emulated (figures as `.dump` files), a real USB portal, or both at once.
  The **portal menu** (Guide, Back + Start or F6) is drawn in the game's own fonts with its own
  pictures of every figure, their hats and upgrade trees, per player, for controller and keyboard.
- **A settings menu** in the same style (F4 or Back + RB): graphics, frame rate, language, portal,
  controls with the game's controller picture.
- **Achievements**, shown in the portal menu and in the Core of Light launcher.
- English and German menus (from the English / German disc, see below).

Screenshots are left out of this repository on purpose: they would show the games' artwork,
which belongs to Activision.

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

## Installing

- **Installer (beta):** `just package-installer` builds `installer/Output/SuperChargersRecompSetup-<version>.exe`
  (Inno Setup; the same file is on the [Releases](https://github.com/tsapZ/SuperChargersRecomp/releases) page). It installs into your user folder
  (`%LOCALAPPDATA%\Programs\SuperChargers Recompiled`, no admin rights) and asks for your disc, as an ISO or an
  already extracted folder (it checks the version, then copies the files next to the game), the
  game language, the portal (emulated, real USB or none), the window size and the internal
  resolution. Everything else starts at the settings above (60 FPS, sharpening, 16x filtering) and is changed later in the F4 menu.
- **[Skylanders: Core of Light](https://github.com/tsapZ/SkylandersCoreOfLight)**, the launcher
  for all six ports, finds the installed game, sets up its disc from your ISO, keeps profiles with
  their own saves and figures and starts the game.
- **From source:** build it (below), put the extracted disc into `rom/` ([rom/README.md](rom/README.md))
  and run `just play-release`.

Your figures are `.dump` files in `Documents\Skylanders\Figures`, shared by every game, like real
figures next to the portal. The saves are in `Documents\superchargersrecomp`, the settings file next to the
game's exe.

## Reporting a problem

Open an [issue](https://github.com/tsapZ/SuperChargersRecomp/issues/new/choose) with the **Problem** form and say
what you did and what happened. The most useful thing to add is the report from Core of Light:
**LB** (the launcher's settings) > **Report a problem**, or the question that comes up by itself
after a crash. It writes one text file to the desktop with the latest logs and settings of every
game, with your Windows user name taken out; drag that file into the issue. Without the launcher,
each game keeps its logs in the `logs` folder next to its exe.

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
- **[skylanders-portal](https://github.com/TheBiemGamer/skylanders-portal)** by TheBiemGamer:
  the portal (software and USB), its overlay and the figure catalog, here in a fork with the
  shared portal and settings menus on top.
- Figure save-data decoding builds on public reverse-engineering of the Skylanders NFC format,
  among others **[SkyReader](https://github.com/reedstrm/SkyReader)**.
- **[extract-xiso](https://github.com/XboxDev/extract-xiso)** is included (prebuilt, unmodified)
  for the installer (`installer/extract-xiso.LICENSE.txt`).

Skylanders is a trademark of Activision. This project is not affiliated with or endorsed by
Activision.

## License

The code and documentation in this repository are released under the [MIT license](LICENSE). That
doesn't cover the game or its data, which you need to own and supply yourself, or the third-party
components in `thirdparty/`, which keep their own licenses.
