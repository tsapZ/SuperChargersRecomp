"""German text for this game's Spanish / Italian Xbox 360 disc, from the owner's own PS3 disc.

Taken from ImaginatorsRecomp's tools/german_text.py, where it works (2026-10-21): this disc has
the same layout, its text in .lng files (IGZ version 9 "igLocalizedStringData") inside its .pak
archives, one per language ("..._es.lng" and "..._it.lng": 1146 Spanish ones in 362 archives),
and 347 Spanish voice archives ("<bank>_es.pak"). The tool appends the PS3 archives' German files
to the 360 archives, stored uncompressed, and points the Spanish entries at them; the IGZ header's
platform word goes from 6 (PS3) to 5 (Xbox 360). With --voices it does the same for the voice
banks: the PS3's are MP3 (FSB5 codec 11), and this game's FMOD has the MP3 codec compiled in too
("FMOD MPEG Codec" in the executable's strings). Not tried here yet: the owner's SuperChargers PS3
disc isn't dumped yet.

The original state can always be restored: before an archive is changed, its length and its
original table of contents entries are saved next to it in rom/archives/german_text_backup/.

Usage (from the repository folder, with the 360 disc extracted to rom/ as for playing):
  python tools/german_text.py --ps3 "D:\\path\\to\\the PS3 disc.iso" [--voices]
  python tools/german_text.py --ps3 "D:\\path\\to\\PS3_GAME\\USRDIR"    (an extracted PS3 disc)
  python tools/german_text.py --restore
An ISO is read with 7-Zip (7z.exe on the PATH or in "C:\\Program Files\\7-Zip"); the PS3
archives it needs are unpacked into a temporary folder and deleted afterwards.

Only the Python standard library is used.
"""

import argparse
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
PLATFORM_PS3 = 6
PLATFORM_XENON = 5


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
    """The PS3 archive's name of a 360 file's German twin, lower case."""
    lower = x360_name.lower().replace("/xenon/", "/ps3/")
    return lower[: -len(SPANISH)] + "_de.lng"


def without_encoding(name):
    """A voice's name without the build's encoding settings after ".wav", which differ between the
    discs (".wav,0,32000,kDefaultFormat,40,1,0,0" on the 360, ".wav,0,48000,..." on the PS3)."""
    at = name.find(".wav,")
    return name[: at + 4] if at >= 0 else name


def german_voice_name(x360_name):
    """The PS3 archive's name of a 360 voice's German twin, lower case:
    ".../xenon/.../voice/spanish/.../vo_x_1.wav,..." -> ".../ps3/.../voice/german/...",
    ".../cutscene/APA1_Intro_gen_es.wav,..." -> ".../APA1_Intro_gen_de.wav,..."."""
    return (x360_name.lower().replace("/xenon/", "/ps3/").replace("/spanish/", "/german/")
            .replace("_es.wav", "_de.wav"))


def ps3_voice_archive_name(x360_archive):
    """The PS3 disc's German voice archive for a 360 Spanish one."""
    return x360_archive.lower()[: -len(SPANISH_VOICES)] + "_de.pak"


def ps3_archive_name(x360_archive):
    """The PS3 disc's archive for a 360 one (the two Xbox 360 ones have PS3 twins by name)."""
    return x360_archive.lower().replace("xenon", "ps3")


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
        changes.append((i, ordoff, length, block, bytes(data)))
    if not changes:
        return 0, "no German twins"
    # The way back is written first, so an interrupted run can still be undone.
    original_size = os.path.getsize(path)
    record = {"size": original_size,
              "entries": [[x.entries_at + 16 * i, "%016x%08x%08x" % (ordoff, length, block)]
                          for i, ordoff, length, block, _ in changes]}
    with open(backup, "w", encoding="utf-8") as b:
        json.dump(record, b)
    with open(path, "r+b") as f:
        end = original_size
        for i, ordoff, length, block, data in changes:
            end = (end + x.sector - 1) // x.sector * x.sector
            f.seek(end)
            f.write(data)
            f.seek(x.entries_at + 16 * i)
            f.write(struct.pack(">QII", (ordoff >> 40 << 40) | end, len(data), STORED))
            end += len(data)
    return len(changes), "ok"


def restore(archives_dir):
    backup_dir = os.path.join(archives_dir, BACKUP_DIR)
    if not os.path.isdir(backup_dir):
        print("Nothing to restore.")
        return
    restored = 0
    for record_name in sorted(os.listdir(backup_dir)):
        if not record_name.endswith(".json"):
            continue
        path = os.path.join(archives_dir, record_name[:-5])
        with open(os.path.join(backup_dir, record_name), encoding="utf-8") as b:
            record = json.load(b)
        with open(path, "r+b") as f:
            for at, entry in record["entries"]:
                f.seek(at)
                f.write(bytes.fromhex(entry))
            f.truncate(record["size"])
        os.remove(os.path.join(backup_dir, record_name))
        restored += 1
    os.rmdir(backup_dir)
    print("Restored %d archives to the disc's state." % restored)


def main():
    parser = argparse.ArgumentParser(description="German text from the PS3 disc for this game's 360 disc.")
    parser.add_argument("--rom", default="rom", help="the extracted 360 disc (default: rom)")
    parser.add_argument("--ps3", help="the PS3 disc: its .iso, or its extracted PS3_GAME\\USRDIR folder")
    parser.add_argument("--restore", action="store_true", help="put the 360 disc's archives back as they were")
    parser.add_argument("--voices", action="store_true",
                        help="also the German voices (experimental: the PS3's MP3 banks, which this game's "
                             "FMOD has a decoder for)")
    args = parser.parse_args()
    archives_dir = os.path.join(args.rom, "archives")
    if not os.path.isdir(archives_dir):
        sys.exit("No %s folder: extract the 360 disc first (see README)." % archives_dir)
    if args.restore:
        restore(archives_dir)
        return
    if not args.ps3:
        sys.exit("Give the PS3 disc with --ps3 (an .iso or its PS3_GAME\\USRDIR folder).")
    names = spanish_archives(archives_dir)
    print("%d archives with Spanish text." % len(names))
    voices = voice_archives(archives_dir) if args.voices else []
    if voices:
        print("%d Spanish voice archives." % len(voices))
    backup_dir = os.path.join(archives_dir, BACKUP_DIR)
    os.makedirs(backup_dir, exist_ok=True)
    temp = None
    try:
        if os.path.isfile(args.ps3):
            seven = find_7zip()
            if not seven:
                sys.exit("7-Zip is needed to read the PS3 .iso (or extract its PS3_GAME\\USRDIR yourself).")
            temp = tempfile.mkdtemp(prefix="german_text_")
            listing = os.path.join(temp, "list.txt")
            with open(listing, "w", encoding="utf-8") as l:
                for name in names:
                    l.write("PS3_GAME\\USRDIR\\archives\\%s\n" % ps3_archive_name(name))
                for name in voices:
                    l.write("PS3_GAME\\USRDIR\\archives\\%s\n" % ps3_voice_archive_name(name))
            print("Unpacking the PS3 archives with 7-Zip ...")
            # 7-Zip reports this disc's UDF image with a header warning (exit code 2) but reads it.
            subprocess.run([seven, "e", args.ps3, "-i@" + listing, "-o" + temp, "-y"],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            ps3_dir = temp
        else:
            ps3_dir = os.path.join(args.ps3, "archives") if os.path.isdir(os.path.join(args.ps3, "archives")) else args.ps3
        ps3_files = {n.lower(): os.path.join(ps3_dir, n) for n in os.listdir(ps3_dir)}
        total = done = 0
        for k, name in enumerate(names, 1):
            ps3_path = ps3_files.get(ps3_archive_name(name))
            if not ps3_path:
                print("  %s: not on the PS3 disc, stays Spanish" % name)
                continue
            n, how = patch(archives_dir, name, ps3_path, backup_dir)
            total += n
            done += 1 if n else 0
            if k % 50 == 0 or k == len(names):
                print("  %d / %d archives, %d text files German" % (k, len(names), total))
        print("Done: %d text files in %d archives are German now." % (total, done))
        total = done = 0
        for k, name in enumerate(voices, 1):
            ps3_path = ps3_files.get(ps3_voice_archive_name(name))
            if not ps3_path:
                continue
            n, how = patch(archives_dir, name, ps3_path, backup_dir, voices=True)
            total += n
            done += 1 if n else 0
            if k % 100 == 0 or k == len(voices):
                print("  %d / %d voice archives, %d voices German" % (k, len(voices), total))
        if voices:
            print("Done: %d voices in %d archives are German now." % (total, done))
        print("Undo everything with --restore.")
    finally:
        if temp:
            shutil.rmtree(temp, ignore_errors=True)


if __name__ == "__main__":
    main()
