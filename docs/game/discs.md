# Supported discs

The port runs every Xbox 360 disc whose `default.xex` has one of the SHA-256 hashes in
`default.xex.sha256` (one per line; the build, the game's own check at start, the installer's
check tool and the Core of Light launcher all read that list).

| Disc | Languages | `default.xex` SHA-256 | Notes |
|---|---|---|---|
| Skylanders - SuperChargers (Europe) (En,De) | English, German | `02ff6672d460c9aaa54e69d584af67a7138c6c93cde5e04b0da4284207be7c61` | the disc to use: German and English text and voices are the game's own |
| Skylanders - SuperChargers (Europe) (Es,It) | Spanish, Italian | `02846647633e93c627e74b92d3ad63a90e1504546958fc8d81e28ac65de1f9bc` | the disc the port was first made with; German or English from the En/De disc with `tools/german_text.py` |

## Why both run

The two discs carry the same build (changelist 2785777 in `version.xml`, the same size of
`default.xex`). Their executables were compared instruction by instruction (codegen's
disassembly of each, numbers left in): 4,810,671 instructions at the same addresses, every one the
same; the hashes differ only outside the code. So every hook address in `config/default.toml`
holds for both, and nothing is rebased.

What differs is the data: the En/De disc has a `_de` and an `_en` twin of every language file
(347 each) where the Es/It disc has `_es` and `_it`, and its opening movie
(`archives/FMV01.bk2`) is another file (3905 frames instead of the Es/It one's). The code that
reads the disc's text takes the `_de` files first and falls back to `_es` (the German text on an
Es/It disc after `tools/german_text.py`), and the intro skip knows both movies.

Checked on the En/De disc by script (copied user data, the software portal): the hash check
passes, the opening movie is skipped when asked, the menus and the first level in German ("Halte
RT zum Beschleunigen!" in the tunnel), the hats in the portal menu with their German names
(1847 texts from `CollectibleTrackers_de.lng`).

## Other discs

Any other region (USA, other European pressings, the PS3, Wii U or Xbox One discs) has a
different `default.xex` and is refused at start with its hash in the message. If such a disc has
the same build, adding its hash here (after a comparison as above) is all it takes; a different
build needs its own address map.
