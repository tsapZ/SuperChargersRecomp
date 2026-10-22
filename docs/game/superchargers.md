# Skylanders: SuperChargers (Xbox 360)

What we know about the game and the dump this project targets, and what still has to be checked.

## The game

- The fifth Skylanders game, 2015, published by Activision, developed by Vicarious Visions, with vehicles. It reads every earlier game's figures and its own: SuperChargers drivers, vehicles and their trophies.

## Target dump

| Property | Value | Source |
|---|---|---|
| Disc content | 2906 files | `extract-xiso`, measured |
| `default.xex` size | 24,985,600 bytes, in the disc root | measured |
| `default.xex` SHA-256 | `02846647633e93c627e74b92d3ad63a90e1504546958fc8d81e28ac65de1f9bc` (Es/It), `02ff6672d460c9aaa54e69d584af67a7138c6c93cde5e04b0da4284207be7c61` (En/De) | measured, in `default.xex.sha256`; both run (discs.md) |

Top level of the disc: `$SystemUpdate`, `VramBudget.igx`, `archives`, `build.xml`, `cachenonce.txt`, `default.xex`, `nxeart`, `smoke_config.xml`, `version.xml`.

## Differences that matter

- **Scaffolded from GiantsRecomp** (app, portal, menus, tooling); none of its game hooks or file
  readers, which are for Giants' executable and its archives (IGA version 8).
