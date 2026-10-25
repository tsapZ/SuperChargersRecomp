"""German (or English) text and voices for this game's Spanish / Italian Xbox 360 disc, from
another disc of the game the owner owns: the European English / German Xbox 360 disc, or the PS3
disc.

This disc keeps its text in .lng files (IGZ version 9 "igLocalizedStringData") inside its .pak
archives, one per language ("..._es.lng" and "..._it.lng": 1146 Spanish ones in 362 archives),
and its voices in 347 voice archives ("<bank>_es.pak"). The other disc has the same archives with
a twin of every Spanish file in its languages. The tool appends those files to this disc's
archives, stored uncompressed, and points the Spanish entries at them; nothing else changes.

--xbox360: the European English / German disc ("Skylanders - SuperChargers (Europe) (En,De)",
checked 2026-10-21). The same paths ("/xenon/"), its voices the game's own XMA banks. Its
default.xex is a different build and doesn't run with this port (the files do).
--ps3 (taken from ImaginatorsRecomp): the PS3 archives' paths say "/ps3/", the IGZ header's
platform word goes from 6 (PS3) to 5 (Xbox 360), the voices are MP3 (FSB5 codec 11; this game's
FMOD has the MP3 codec, "FMOD MPEG Codec" in the executable's strings).

Two texts have no twin by name on a PS3 disc, as in Imaginators (an Xbox 360 disc has both):
- The licence agreement: softwareLicense.pak has licenseAgreement_<language>.igz for eleven
  languages on this disc too; the Spanish entry is pointed at the one in the target language.
- The 360's own platform strings (xenonPlatformStrings_es.lng in permanent_xenon.pak, 195 texts:
  "Pulsa START" and the Xbox Live messages): a copy is made by key, from the PS3 disc's
  ps3PlatformStrings / pcPlatformStrings where their Spanish matches the 360's, else (German only)
  from XENON_GERMAN below (written for Imaginators' keys; the run names any text that stays
  Spanish). The .lng writer reproduces this disc's file byte for byte when nothing changes.

The original state can always be restored: before an archive is changed, its length and its
original table of contents entries are saved next to it in rom/archives/german_text_backup/.

Usage (from the repository folder, with the 360 disc extracted to rom/ as for playing):
  python tools/german_text.py --xbox360 "D:\\path\\to\\the En-De disc.iso" --voices
  python tools/german_text.py --xbox360 "D:\\path\\to\\the extracted En-De disc"
  python tools/german_text.py --ps3 "D:\\path\\to\\the PS3 disc.iso" [--voices]
  python tools/german_text.py --xbox360 ... --movies      (the story movies too, ~5 GB)
  python tools/german_text.py ... --language en           (English instead of German)
  python tools/german_text.py --restore
An Xbox 360 .iso is unpacked whole with extract-xiso (installer/extract-xiso.exe) into a temporary
folder, a PS3 .iso partly with 7-Zip; the folder is deleted afterwards.

Only the Python standard library is used.
"""

import argparse
import filecmp
import json
import lzma
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

BLOCK = 0x8000
STORED = 0xFFFFFFFF
SPANISH = "_es.lng"
SPANISH_VOICES = "_es.pak"
BACKUP_DIR = "german_text_backup"
MOVIES_DIR = "movies"  # in BACKUP_DIR: this disc's movies that --movies replaced
PLATFORM_PS3 = 6
PLATFORM_XENON = 5
XENON_STRINGS = "xenonplatformstrings_es.lng"

# Per target language: the files' suffix, the voices' folder, a cutscene voice's ending (English
# ones have none: "APA1_Intro_gen.wav"), the name in messages.
LANGUAGES = {"de": ("_de", "/german/", "_de.wav", "German"),
             "en": ("_en", "/english/", ".wav", "English")}
# What the run takes its files from: "ps3" (the PS3 disc, renamed paths, platform word 6 -> 5) or
# "xenon" (another Xbox 360 disc of the game, e.g. a European English / German one: the same
# paths and the 360's own voice banks). Set by main().
SOURCE = {"platform": "ps3", "language": "de"}


def lang():
    return LANGUAGES[SOURCE["language"]]


def from_ps3():
    return SOURCE["platform"] == "ps3"

# The 360 platform strings whose Spanish text differs from both PS3 disc files (they name Xbox
# Live, the gamertag or the 360's START button), in German in the PS3 disc's wording.
XENON_GERMAN = {
    "CGlobalPlatformStrings::_cableUnplugged":
        "Du musst bei Xbox Live angemeldet sein, um diese Funktion zu nutzen.",
    "CGlobalPlatformStrings::_friendScoresUnavailable":
        "Der Imaginators-Dienst ist nicht verfügbar. Bitte versuche es später noch einmal.",
    "CGlobalPlatformStrings::_gamertagString": "GAMERTAG",
    "CGlobalPlatformStrings::_onlineMultiplayerRestrictedMessage":
        "Du musst mit einem Xbox Live-Profil angemeldet sein, das die nötigen Berechtigungen zum "
        "Spielen hat.",
    "CGlobalPlatformStrings::_quitToHubConfirmation":
        "Zum O.R.T. zurückkehren? Fortschritte seit dem letzten Speichern gehen verloren.",
    "CGlobalPlatformStrings::_serverUnavailable":
        "Der Imaginators-Dienst ist nicht verfügbar. Bitte versuche es später noch einmal.",
    "CGlobalPlatformStrings::_signinToViewLeaderboard":
        "Melde dich bei Xbox Live an, um auf Online-Features zuzugreifen.",
    "CGlobalPlatformStrings::_titleScreenPressStartString": "Drücke START",
    "CGlobalPlatformStrings::_titleScreenTapToStartString": "Zum Starten antippen",
}


class Archive:
    """An Alchemy igArchive version 11 (big-endian), as far as this tool needs it."""

    def __init__(self, path):
        self.path = path
        with open(path, "rb") as f:
            head = f.read(0x38)
            if head[:4] != b"\x1aAGI" or struct.unpack_from(">I", head, 4)[0] != 11:
                raise ValueError("not an IGA version 11 archive (big-endian)")
            self.count, self.sector = struct.unpack_from(">2I", head, 0x0C)
            n_large, n_medium, n_small = struct.unpack_from(">3I", head, 0x1C)
            names_off = struct.unpack_from(">Q", head, 0x28)[0]
            names_size = struct.unpack_from(">I", head, 0x30)[0]
            self.entries_at = 0x38 + 4 * self.count
            toc = f.read(4 * self.count + 16 * self.count + 4 * n_large + 2 * n_medium + n_small)
            f.seek(names_off)
            names = f.read(names_size)
        self.files = []
        for i in range(self.count):
            ordoff, length, block = struct.unpack_from(">QII", toc, 4 * self.count + 16 * i)
            at = struct.unpack_from(">I", names, 4 * i)[0]
            name = names[at:names.index(b"\0", at)].decode("latin-1")
            self.files.append((name, ordoff, length, block))
        t = 4 * self.count + 16 * self.count
        self.large = struct.unpack_from(">%dI" % n_large, toc, t)
        t += 4 * n_large
        self.medium = struct.unpack_from(">%dH" % n_medium, toc, t)
        t += 2 * n_medium
        self.small = toc[t:t + n_small]

    def _entry(self, index, length):
        if 0x7F * self.sector < length:
            if 0x7FFF * self.sector < length:
                v = self.large[index]
                return v >> 31, v & 0x7FFFFFFF
            v = self.medium[index]
            return v >> 15, v & 0x7FFF
        v = self.small[index]
        return v >> 7, v & 0x7F

    def read(self, i):
        name, ordoff, length, block = self.files[i]
        offset = ordoff & 0xFFFFFFFFFF
        with open(self.path, "rb") as f:
            if block == STORED:
                f.seek(offset)
                return f.read(length)
            comp, first = block >> 28, block & 0x0FFFFFFF
            out = bytearray()
            for b in range((length + BLOCK - 1) // BLOCK):
                compressed, sector = self._entry(first + b, length)
                want = min(BLOCK, length - b * BLOCK)
                f.seek(offset + sector * self.sector)
                if not compressed:
                    out += f.read(want)
                    continue
                packed = struct.unpack("<H", f.read(2))[0]
                if comp == 2:  # LZMA: 5 property bytes, then the raw stream
                    props = f.read(5)
                    lc, lp, pb = props[0] % 9, (props[0] // 9) % 5, props[0] // 45
                    filters = [{"id": lzma.FILTER_LZMA1, "dict_size": struct.unpack("<I", props[1:5])[0],
                                "lc": lc, "lp": lp, "pb": pb}]
                    dec = lzma.LZMADecompressor(format=lzma.FORMAT_RAW, filters=filters)
                    out += dec.decompress(f.read(packed), max_length=want)
                elif comp == 1:  # raw deflate
                    out += zlib.decompressobj(-15).decompress(f.read(packed))[:want]
                else:
                    raise ValueError("unknown compression %d in %s" % (comp, name))
            return bytes(out)

    def find(self, lower_name):
        """The index of the entry with this name (lower case), or None."""
        for i, f in enumerate(self.files):
            if f[0].lower() == lower_name:
                return i
        return None


def igz_sections(data):
    """An IGZ file's sections as (descriptor position, offset, length); section 0 holds the fixups,
    section n + 1 is memory pool n, which serialized pointers address as pool << 27 | offset."""
    out = []
    at = 0x14
    while True:
        offset, length = struct.unpack_from(">2I", data, at + 4)
        if offset == 0:
            return out
        out.append((at, offset, length))
        at += 16


def lng_parse(data):
    """An IGZ version 9 .lng file's texts: {position of the field pointing at it: (key, text)} for
    every field RSTR lists, in RSTR's order. The fields of the igLocalizedStringData objects have
    their key; another one (the object list's name) has None. An object is +0 its type (a TMET
    index), +8 the value (a pointer to UTF-8 text) and +0x10 the key's handle, 0x80000000 | n:
    EXNM entry n + 1 (entry 0 is the file's own name), whose second word is the key's TSTR index.
    RVTB (the objects) and RSTR (the text fields) are lists of positions, packed as deltas."""
    sections = igz_sections(data)
    fixups = {}
    at = sections[0][1]
    for _ in range(struct.unpack_from(">I", data, 0x10)[0]):
        count, length, start = struct.unpack_from(">3I", data, at + 4)
        fixups[data[at:at + 4][::-1].decode("latin-1")] = (count, at + start, at + length)
        at += length

    def strings(tag):
        count, at, end = fixups[tag]
        out = []
        while len(out) < count and at < end:
            stop = data.index(b"\0", at)
            out.append(data[at:stop].decode("utf-8"))
            at = stop + 1
            if at < end and data[at] == 0 and (at - fixups[tag][1]) % 2:  # even distances
                at += 1
        return out

    def pointer(value):
        pool, offset = value >> 27, value & 0x07FFFFFF
        return sections[pool + 1][1] + offset if pool + 1 < len(sections) else None

    def positions(tag):
        count, at, end = fixups[tag]
        nibbles = [n for byte in data[at:end] for n in (byte & 0xF, byte >> 4)]
        out, position, i = [], 0, 0
        for _ in range(count):
            value, shift = 0, 0
            while True:  # 3 bits per nibble, bit 3 set while more follow
                value |= (nibbles[i] & 7) << shift
                shift += 3
                i += 1
                if not nibbles[i - 1] & 8:
                    break
            position += value * 4
            out.append(pointer(position))
        return out

    def text_at(field):
        at = pointer(struct.unpack_from(">I", data, field)[0])
        return data[at:data.index(b"\0", at)].decode("utf-8")

    types, names = strings("TMET"), strings("TSTR")
    count, at, end = fixups["EXNM"]
    exnm = [struct.unpack_from(">2I", data, at + 8 * i) for i in range(count)]
    keys = {}
    for obj in positions("RVTB"):
        kind = struct.unpack_from(">I", data, obj)[0]
        if kind < len(types) and types[kind] == "igLocalizedStringData":
            handle = struct.unpack_from(">I", data, obj + 0x10)[0] & 0x7FFFFFFF
            keys[obj + 8] = names[exnm[handle + 1][1]]
    return {field: (keys.get(field), text_at(field)) for field in positions("RSTR")}


def lng_texts(data):
    """Key -> text of a .lng file."""
    return {key: text for key, text in lng_parse(data).values() if key is not None}


def rewrite_lng(data, texts):
    """A copy of a .lng file with each key's text from texts where it has one. The texts are the
    last section's whole content (zero-terminated, each at an even offset, a text used twice stored
    once, in RSTR's order: as the game's own files are, which this reproduces byte for byte when
    nothing changes), so it is written anew, and the pointers and the section's length set to
    match."""
    sections = igz_sections(data)
    descriptor, pool_at, pool_length = sections[-1]
    if pool_at + pool_length != len(data):
        raise ValueError("the texts are not the file's last part")
    pool_index = len(sections) - 2
    out = bytearray(data[:pool_at])
    pool = bytearray()
    placed = {}
    for field, (key, text) in lng_parse(data).items():
        text = texts.get(key, text) if key is not None else text
        if text not in placed:
            if len(pool) % 2:
                pool += b"\0"
            placed[text] = len(pool)
            pool += text.encode("utf-8") + b"\0"
        struct.pack_into(">I", out, field, pool_index << 27 | placed[text])
    struct.pack_into(">I", out, descriptor + 8, len(pool))
    return bytes(out + pool)


def find_7zip():
    for candidate in ("7z", "7z.exe", r"C:\Program Files\7-Zip\7z.exe", r"C:\Program Files (x86)\7-Zip\7z.exe"):
        path = shutil.which(candidate) or (candidate if os.path.isfile(candidate) else None)
        if path:
            return path
    return None


def spanish_archives(archives_dir):
    """The archives with Spanish text files in them."""
    out = []
    for name in sorted(os.listdir(archives_dir)):
        if not name.lower().endswith(".pak"):
            continue
        try:
            archive = Archive(os.path.join(archives_dir, name))
        except (ValueError, OSError, struct.error):
            continue
        if any(f[0].lower().endswith(SPANISH) for f in archive.files):
            out.append(name)
    return out


def voice_archives(archives_dir):
    """The Spanish voice archives (one per voice bank, "<bank>_es.pak")."""
    return sorted(n for n in os.listdir(archives_dir) if n.lower().endswith(SPANISH_VOICES))


def german_name(x360_name):
    """The source archive's name of a 360 file's twin in the target language, lower case."""
    lower = x360_name.lower()
    if from_ps3():
        lower = lower.replace("/xenon/", "/ps3/")
    return lower[: -len(SPANISH)] + lang()[0] + ".lng"


def without_encoding(name):
    """A voice's name without the build's encoding settings after ".wav", which differ between the
    discs (".wav,0,32000,kDefaultFormat,40,1,0,0" on the 360, ".wav,0,48000,..." on the PS3)."""
    at = name.find(".wav,")
    return name[: at + 4] if at >= 0 else name


def german_voice_name(x360_name):
    """The source archive's name of a 360 voice's twin, lower case (German from the PS3):
    ".../xenon/.../voice/spanish/.../vo_x_1.wav,..." -> ".../ps3/.../voice/german/...",
    ".../cutscene/APA1_Intro_gen_es.wav,..." -> ".../APA1_Intro_gen_de.wav,..."."""
    lower = x360_name.lower()
    if from_ps3():
        lower = lower.replace("/xenon/", "/ps3/")
    return lower.replace("/spanish/", lang()[1]).replace("_es.wav", lang()[2])


def ps3_voice_archive_name(x360_archive):
    """The source disc's voice archive in the target language for a 360 Spanish one."""
    return x360_archive.lower()[: -len(SPANISH_VOICES)] + lang()[0] + ".pak"


def ps3_archive_name(x360_archive):
    """The source disc's archive for a 360 one (the two Xbox 360 ones have PS3 twins by name)."""
    return x360_archive.lower().replace("xenon", "ps3") if from_ps3() else x360_archive.lower()


def patch(archives_dir, name, ps3_path, backup_dir, voices=False):
    """Points the Spanish entries of one 360 archive at the PS3 archive's German twins."""
    path = os.path.join(archives_dir, name)
    backup = os.path.join(backup_dir, name + ".json")
    if os.path.exists(backup):
        return 0, "already done"
    x = Archive(path)
    p = Archive(ps3_path)
    german = {without_encoding(f[0].lower()): i for i, f in enumerate(p.files)}
    changes = []
    for i, (fname, ordoff, length, block) in enumerate(x.files):
        if not voices and not fname.lower().endswith(SPANISH):
            continue
        j = german.get(without_encoding(german_voice_name(fname) if voices else german_name(fname)))
        if j is None:
            continue
        data = bytearray(p.read(j))
        if data[:4] == b"IGZ\x01" and struct.unpack_from(">I", data, 0x0C)[0] == PLATFORM_PS3:
            struct.pack_into(">I", data, 0x0C, PLATFORM_XENON)
        changes.append((i, bytes(data)))
    if not changes:
        return 0, "no German twins"
    write_changes(path, x, backup, changes)
    return len(changes), "ok"


def write_changes(path, x, backup, changes):
    """Points entries of the archive x (at path) elsewhere: (entry index, data) appends the data,
    stored, and points the entry at it; (entry index, other index) points it at another entry's
    data in the same archive. The way back is written first, so an interrupted run can still be
    undone."""
    original_size = os.path.getsize(path)
    record = {"size": original_size,
              "entries": [[x.entries_at + 16 * i, "%016x%08x%08x" % x.files[i][1:]] for i, _ in changes]}
    with open(backup, "w", encoding="utf-8") as b:
        json.dump(record, b)
    with open(path, "r+b") as f:
        end = original_size
        for i, new in changes:
            ordinal = x.files[i][1] >> 40 << 40
            f.seek(x.entries_at + 16 * i)
            if isinstance(new, int):
                _, ordoff, length, block = x.files[new]
                f.write(struct.pack(">QII", ordinal | (ordoff & 0xFFFFFFFFFF), length, block))
                continue
            end = (end + x.sector - 1) // x.sector * x.sector
            f.write(struct.pack(">QII", ordinal | end, len(new), STORED))
            f.seek(end)
            f.write(new)
            end += len(new)


def disc_german(archives_dir, backup_dir):
    """Files this disc has in German itself, next to the Spanish one in the same archive (the
    licence agreement: softwareLicense.pak, licenseAgreement_de.igz): the Spanish entry is pointed
    at the German file. Returns how many."""
    total = 0
    for name in sorted(os.listdir(archives_dir)):
        backup = os.path.join(backup_dir, name + ".disc.json")
        lower = name.lower()
        if not lower.endswith(".pak") or lower.endswith(SPANISH_VOICES) or os.path.exists(backup):
            continue
        try:
            x = Archive(os.path.join(archives_dir, name))
        except (ValueError, OSError, struct.error):
            continue
        changes = []
        for i, f in enumerate(x.files):
            root, ext = os.path.splitext(f[0].lower())
            j = x.find(root[:-3] + lang()[0] + ext) if root.endswith("_es") and ext != ".lng" else None
            if j is not None:
                changes.append((i, j))
        if changes:
            write_changes(os.path.join(archives_dir, name), x, backup, changes)
            total += len(changes)
    return total


def platform_strings(archives_dir, ps3_path, backup_dir):
    """A German xenonPlatformStrings_es.lng (permanent_xenon.pak), made from the PS3 disc's
    permanent_ps3.pak as the module's description says. Returns the texts that stay Spanish."""
    name = "permanent_xenon.pak"
    path = os.path.join(archives_dir, name)
    backup = os.path.join(backup_dir, name + ".platform.json")
    if os.path.exists(backup) or not os.path.exists(path):
        return []
    x = Archive(path)
    p = Archive(ps3_path)
    i = next((k for k, f in enumerate(x.files) if f[0].lower().endswith("/" + XENON_STRINGS)), None)
    if i is None:
        return []
    xenon = x.read(i)
    spanish = lng_texts(xenon)

    def ps3_texts(file_name):
        j = next((k for k, f in enumerate(p.files) if f[0].lower().endswith("/" + file_name)), None)
        return lng_texts(p.read(j)) if j is not None else {}

    suffix = lang()[0]
    sources = [(ps3_texts("ps3platformstrings_es.lng"), ps3_texts("ps3platformstrings%s.lng" % suffix)),
               (ps3_texts("pcplatformstrings_es.lng"), ps3_texts("pcplatformstrings%s.lng" % suffix))]
    german, missing = {}, []
    for key, text in spanish.items():
        for their_spanish, their_german in sources:
            if their_spanish.get(key) == text and key in their_german:
                german[key] = their_german[key]
                break
        else:
            if key in XENON_GERMAN and SOURCE["language"] == "de":
                german[key] = XENON_GERMAN[key]
            else:
                missing.append(key)
    write_changes(path, x, backup, [(i, rewrite_lng(xenon, german))])
    return missing


def movies(archives_dir, source_dir, backup_dir):
    """The other disc's movies where they differ from this disc's: the Bink movies carry their
    spoken track in the disc's languages (the same track numbers on both discs; the story movies
    differ, the logo and vehicle movies are the same file). This disc's file goes to
    <backup>/movies first, so --restore can put it back."""
    store = os.path.join(backup_dir, MOVIES_DIR)
    os.makedirs(store, exist_ok=True)
    changed = same = 0
    for name in sorted(os.listdir(archives_dir)):
        if not name.lower().endswith(".bk2"):
            continue
        source = os.path.join(source_dir, name)
        target = os.path.join(archives_dir, name)
        if not os.path.isfile(source) or os.path.exists(os.path.join(store, name)):
            continue
        if filecmp.cmp(source, target, shallow=False):
            same += 1
            continue
        os.replace(target, os.path.join(store, name))
        shutil.copyfile(source, target)
        changed += 1
    if not os.listdir(store):
        os.rmdir(store)
    return changed, same


def restore(archives_dir):
    backup_dir = os.path.join(archives_dir, BACKUP_DIR)
    if not os.path.isdir(backup_dir):
        print("Nothing to restore.")
        return
    store = os.path.join(backup_dir, MOVIES_DIR)
    if os.path.isdir(store):
        for name in os.listdir(store):
            os.replace(os.path.join(store, name), os.path.join(archives_dir, name))
        os.rmdir(store)
        print("The disc's own movies are back.")
    steps = {}
    for record_name in sorted(os.listdir(backup_dir)):
        if not record_name.endswith(".json"):
            continue
        archive = record_name[:-5]
        if not archive.lower().endswith(".pak"):  # "<archive>.<step>.json"
            archive = archive.rsplit(".", 1)[0]
        with open(os.path.join(backup_dir, record_name), encoding="utf-8") as b:
            steps.setdefault(archive, []).append((json.load(b), record_name))
    restored = 0
    for archive, records in steps.items():
        # The later steps first (their records start from a longer file), so that an entry two
        # steps changed ends up as the disc has it.
        records.sort(key=lambda r: -r[0]["size"])
        with open(os.path.join(archives_dir, archive), "r+b") as f:
            for record, _ in records:
                for at, entry in record["entries"]:
                    f.seek(at)
                    f.write(bytes.fromhex(entry))
            f.truncate(min(record["size"] for record, _ in records))
        for _, record_name in records:
            os.remove(os.path.join(backup_dir, record_name))
        restored += 1
    os.rmdir(backup_dir)
    print("Restored %d archives to the disc's state." % restored)


def find_extract_xiso():
    """extract-xiso (on the PATH, or the copy in this repository's installer folder)."""
    here = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "installer", "extract-xiso.exe")
    for candidate in ("extract-xiso", "extract-xiso.exe", here):
        path = shutil.which(candidate) or (candidate if os.path.isfile(candidate) else None)
        if path:
            return path
    return None


def main():
    parser = argparse.ArgumentParser(
        description="German (or English) text and voices for this game's Spanish / Italian 360 disc, "
                    "from another disc of the game you own.")
    parser.add_argument("--rom", default="rom", help="the extracted 360 disc (default: rom)")
    parser.add_argument("--ps3", help="the PS3 disc: its .iso, or its extracted PS3_GAME\\USRDIR folder")
    parser.add_argument("--xbox360", help="another Xbox 360 disc with the language (e.g. the European "
                                          "English / German one): its .iso, or its extracted folder")
    parser.add_argument("--language", choices=sorted(LANGUAGES), default="de",
                        help="the language to put in: de (German, the default) or en (English)")
    parser.add_argument("--restore", action="store_true", help="put the 360 disc's archives back as they were")
    parser.add_argument("--voices", action="store_true",
                        help="also the voices (from an Xbox 360 disc they are the game's own; from the PS3 "
                             "disc experimental: MP3 banks, which this game's FMOD has a decoder for)")
    parser.add_argument("--movies", action="store_true",
                        help="also the movies, from another Xbox 360 disc: their spoken track is in that "
                             "disc's languages (else the story movies stay Spanish / Italian)")
    args = parser.parse_args()
    archives_dir = os.path.join(args.rom, "archives")
    if not os.path.isdir(archives_dir):
        sys.exit("No %s folder: extract the 360 disc first (see README)." % archives_dir)
    if args.restore:
        restore(archives_dir)
        return
    if bool(args.ps3) == bool(args.xbox360):
        sys.exit("Give the other disc with --ps3 or with --xbox360 (an .iso or its extracted folder).")
    SOURCE["platform"] = "ps3" if args.ps3 else "xenon"
    SOURCE["language"] = args.language
    source = args.ps3 or args.xbox360
    language = lang()[3]
    names = spanish_archives(archives_dir)
    print("%d archives with Spanish text." % len(names))
    voices = voice_archives(archives_dir) if args.voices else []
    if voices:
        print("%d Spanish voice archives." % len(voices))
    backup_dir = os.path.join(archives_dir, BACKUP_DIR)
    os.makedirs(backup_dir, exist_ok=True)
    # A second run (say with --voices after text only) leaves the archives that are done alone.
    done_before = sum(os.path.exists(os.path.join(backup_dir, n + ".json")) for n in names + voices)
    names = [n for n in names if not os.path.exists(os.path.join(backup_dir, n + ".json"))]
    voices = [n for n in voices if not os.path.exists(os.path.join(backup_dir, n + ".json"))]
    if done_before:
        print("%d archives are done from an earlier run already." % done_before)
    wanted = [ps3_archive_name(n) for n in names] + [ps3_voice_archive_name(n) for n in voices]
    if from_ps3() and not os.path.exists(os.path.join(backup_dir, "permanent_xenon.pak.platform.json")):
        wanted.append("permanent_ps3.pak")
    temp = None
    try:
        if os.path.isfile(source) and from_ps3():
            seven = find_7zip()
            if not seven:
                sys.exit("7-Zip is needed to read the PS3 .iso (or extract its PS3_GAME\\USRDIR yourself).")
            temp = tempfile.mkdtemp(prefix="german_text_")
            listing = os.path.join(temp, "list.txt")
            with open(listing, "w", encoding="utf-8") as l:
                for name in sorted(set(wanted)):
                    l.write("PS3_GAME\\USRDIR\\archives\\%s\n" % name)
            print("Unpacking the PS3 archives with 7-Zip ...")
            # 7-Zip reports this disc's UDF image with a header warning (exit code 2) but reads it.
            subprocess.run([seven, "e", source, "-i@" + listing, "-o" + temp, "-y"],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            source_dir = temp
        elif os.path.isfile(source):
            xiso = find_extract_xiso()
            if not xiso:
                sys.exit("extract-xiso is needed to read the Xbox 360 .iso (or extract it yourself).")
            temp = tempfile.mkdtemp(prefix="german_text_")
            print("Unpacking the Xbox 360 disc with extract-xiso (about 8 GB, a few minutes) ...")
            subprocess.run([xiso, "-d", temp, source], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            source_dir = os.path.join(temp, "archives")
        else:
            source_dir = os.path.join(source, "archives") if os.path.isdir(os.path.join(source, "archives")) else source
        source_files = {n.lower(): os.path.join(source_dir, n) for n in os.listdir(source_dir)}
        total = done = 0
        for k, name in enumerate(names, 1):
            source_path = source_files.get(ps3_archive_name(name))
            if not source_path:
                print("  %s: not on the other disc, stays Spanish" % name)
                continue
            n, how = patch(archives_dir, name, source_path, backup_dir)
            total += n
            done += 1 if n else 0
            if k % 50 == 0 or k == len(names):
                print("  %d / %d archives, %d text files %s" % (k, len(names), total, language))
        print("Done: %d text files in %d archives are %s now." % (total, done, language))
        n = disc_german(archives_dir, backup_dir)
        if n:
            print("The licence agreement: %d file(s) pointed at the disc's own %s one." % (n, language))
        if "permanent_ps3.pak" in source_files:
            missing = platform_strings(archives_dir, source_files["permanent_ps3.pak"], backup_dir)
            print("The Xbox 360's own texts (title screen, save slots, Xbox Live) are %s now%s." %
                  (language, "; Spanish still: " + ", ".join(k.split("::")[-1] for k in missing) if missing else ""))
        total = done = 0
        for k, name in enumerate(voices, 1):
            source_path = source_files.get(ps3_voice_archive_name(name))
            if not source_path:
                continue
            n, how = patch(archives_dir, name, source_path, backup_dir, voices=True)
            total += n
            done += 1 if n else 0
            if k % 100 == 0 or k == len(voices):
                print("  %d / %d voice archives, %d voices %s" % (k, len(voices), total, language))
        if voices:
            print("Done: %d voices in %d archives are %s now." % (total, done, language))
        if args.movies and not from_ps3():
            changed, same = movies(archives_dir, source_dir, backup_dir)
            print("Movies: %d taken from the other disc, %d the same on both." % (changed, same))
        print("Undo everything with --restore.")
    finally:
        if temp:
            shutil.rmtree(temp, ignore_errors=True)


if __name__ == "__main__":
    main()
