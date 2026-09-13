import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import awdisasm

SECTOR_BYTES = 2352
USER_OFFSET = 16
USER_BYTES = 2048

PAL_BYTES = 32
PAL_COUNT = 32

LOAD_ADDR = 0x6000

DISPATCH = 0x450E

PALETTE_DELTA = -0x1000

CHAPTERS = [
    ("INTRO7", 1, 0x17, "introduction"),
    ("EAU3", 2, 0x1A, "water"),
    ("PRI3", 3, 0x1D, "prison"),
    ("CITE1", 4, 0x20, "city"),
    ("LUXE2", 6, 0x26, "luxe"),
    ("ARENE2", 5, 0x23, "arene"),
    ("FINAL3", 7, 0x29, "final"),
    ("CODE", 8, 0x7D, "password"),
]

ALL_PALETTE_RES = [
    (0x14, "protection"), (0x17, "introduction"), (0x1A, "water"),
    (0x1D, "prison"), (0x20, "city"), (0x23, "arene"),
    (0x26, "luxe"), (0x29, "final"), (0x7D, "password"),
]

HIT_ERR = 0.10

TRIVIAL_COLOURS = 3

def sector_user(track, lba):
    track.seek(lba * SECTOR_BYTES + USER_OFFSET)
    return track.read(USER_BYTES)

def read_extent(track, lba, size):
    out = bytearray()
    for i in range((size + USER_BYTES - 1) // USER_BYTES):
        out += sector_user(track, lba + i)
    return bytes(out[:size])

def read_root(path):
    track = open(path, "rb")
    pvd = sector_user(track, 16)
    if pvd[1:6] != b"CD001":
        raise SystemExit("%s is not a MODE1/2352 ISO9660 track" % path)
    root = pvd[156:156 + 34]
    data = read_extent(track, struct.unpack("<I", root[2:6])[0],
                       struct.unpack("<I", root[10:14])[0])
    out = {}
    pos = 0
    while pos < len(data):
        length = data[pos]
        if length == 0:
            pos = (pos // USER_BYTES + 1) * USER_BYTES
            continue
        rec = data[pos:pos + length]
        name = rec[33:33 + rec[32]].decode("ascii", "replace").split(";")[0]
        out[name] = (struct.unpack("<I", rec[2:6])[0],
                     struct.unpack("<I", rec[10:14])[0])
        pos += length
    return track, out

def walk_loader(blob):
    out = []
    off = DISPATCH
    while struct.unpack_from(">H", blob, off)[0] == 0x0C00:
        number = struct.unpack_from(">H", blob, off + 2)[0]
        branch = struct.unpack_from(">H", blob, off + 4)[0]
        if (branch & 0xFF00) != 0x6600 or (branch & 0xFF) == 0:
            break
        end = off + 6 + (branch & 0xFF)
        lea = struct.unpack_from(">H", blob, off + 6)[0]
        block = (struct.unpack_from(">I", blob, off + 8)[0] - LOAD_ADDR
                 if (lea & 0xF1FF) == 0x41F9 else None)
        reads = []
        pos = off + 6
        while pos < end - 10:
            if (struct.unpack_from(">H", blob, pos)[0] == 0x203C and
                    struct.unpack_from(">H", blob, pos + 6)[0] == 0x223C):
                reads.append((struct.unpack_from(">I", blob, pos + 2)[0],
                              struct.unpack_from(">I", blob, pos + 8)[0]))
                pos += 12
            else:
                pos += 2
        out.append((number, block, reads))
        off = end
    return out

def md_palette(blob, off):
    out = []
    for i in range(16):
        word = struct.unpack_from(">H", blob, off + i * 2)[0]
        if word & 0xF111:
            return None
        out.append((((word >> 1) & 7) / 7.0,
                    ((word >> 5) & 7) / 7.0,
                    ((word >> 9) & 7) / 7.0))
    return out

def aw_palette(buf, palnum):
    out = []
    for i in range(16):
        word = struct.unpack_from(">H", buf, palnum * PAL_BYTES + i * 2)[0]
        out.append((((word >> 8) & 0xF) / 15.0,
                    ((word >> 4) & 0xF) / 15.0,
                    (word & 0xF) / 15.0))
    return out

def palette_error(want, got):
    total = 0.0
    for (r1, g1, b1), (r2, g2, b2) in zip(want, got):
        total += (r1 - r2) ** 2 + (g1 - g2) ** 2 + (b1 - b2) ** 2
    return (total / 48.0) ** 0.5

def score(blob, base, buf):
    hits = 0
    readable = 0
    for p in range(PAL_COUNT):
        raw = buf[p * PAL_BYTES:(p + 1) * PAL_BYTES]
        if not any(raw):
            continue
        if len(set(struct.unpack(">16H", raw))) <= TRIVIAL_COLOURS:
            continue
        got = md_palette(blob, base + p * PAL_BYTES)
        if got is None:
            continue
        readable += 1
        if palette_error(aw_palette(buf, p), got) < HIT_ERR:
            hits += 1
    return hits, readable

def resolve(blob, data_dir, entries, names):
    arms = walk_loader(blob)
    out = []
    for (number, block, reads), (chapter, _part, _res, label) in zip(arms, CHAPTERS):
        if reads:
            first = [n for n, (lba, _s) in names.items() if lba == reads[0][0]]
            if first and not first[0].startswith(chapter):
                raise SystemExit("chapter %d reads %s, expected %s"
                                 % (number, first[0], chapter))
        base = block + PALETTE_DELTA
        ranked = []
        for res_id, res_name in ALL_PALETTE_RES:
            buf, _ok = awdisasm.load_resource(data_dir, entries[res_id])
            hits, readable = score(blob, base, buf)
            ranked.append((hits, res_name, readable))
        ranked.sort(reverse=True)
        best = ranked[0]
        unique = sum(1 for h, _n, _r in ranked if h == best[0]) == 1
        out.append((chapter, label, base, best[0], best[2], best[1],
                    unique and best[0] > 0))
    return out

def report(blob, data_dir, entries, names):
    print("palette base = loader block %s0x%X"
          % ("-" if PALETTE_DELTA < 0 else "+", abs(PALETTE_DELTA)))
    print()
    print("%-8s %-11s %-14s %-9s %s"
          % ("chapter", "palettes", "this game's", "readable", "scores as"))
    for chapter, label, base, hits, readable, best, unique in \
            resolve(blob, data_dir, entries, names):
        if readable == 0:
            print("%-8s 0x%06X    %-14s %-9s no palette data here"
                  % (chapter, base, label, "0/32"))
            continue
        if unique and best == label:
            verdict = "%s %d  <== confirmed" % (best, hits)
        elif unique:
            verdict = "%s %d  <== MISMATCH" % (best, hits)
        else:
            verdict = "nothing unique"
        print("%-8s 0x%06X    %-14s %2d/32     %s"
              % (chapter, base, label, readable, verdict))

def loader(blob, names):
    bylba = {}
    for name, (lba, size) in names.items():
        bylba[lba] = (name, (size + USER_BYTES - 1) // USER_BYTES)
    for number, block, reads in walk_loader(blob):
        shown = []
        for lba, count in reads:
            name, want = bylba.get(lba, ("?", 0))
            shown.append("%s %d/%d%s"
                         % (name, lba, count, "" if want == count else " (!%d)" % want))
        print("  chapter %d  A4 0x%06X  palettes 0x%06X  %s"
              % (number, block, block + PALETTE_DELTA, "  ".join(shown)))

def dump(blob, data_dir, entries, names, want):
    rows = dict((r[0], r) for r in resolve(blob, data_dir, entries, names))
    if want not in rows:
        raise SystemExit("no chapter %s; try one of %s"
                         % (want, ", ".join(c for c, _p, _r, _l in CHAPTERS)))
    chapter, label, base, _hits, readable, _best, _unique = rows[want]
    if readable == 0:
        raise SystemExit("%s has no palette data at 0x%06X" % (chapter, base))
    res_id = dict((c, r) for c, _p, r, _l in CHAPTERS)[chapter]
    buf, _ok = awdisasm.load_resource(data_dir, entries[res_id])
    print("%s, %s, table 0x%06X ($%06X)"
          % (chapter, label, base, base + LOAD_ADDR))
    for p in range(PAL_COUNT):
        got = md_palette(blob, base + p * PAL_BYTES)
        if got is None:
            print("  pal %2d  --" % p)
            continue
        print("  pal %2d  err=%.4f  %s"
              % (p, palette_error(aw_palette(buf, p), got),
                 " ".join("#%02X%02X%02X"
                          % (int(r * 255), int(g * 255), int(b * 255))
                          for r, g, b in got)))

def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: awpal.py <track01.bin> <data dir> "
                         "<list|loader|chapter>")
    track, names = read_root(sys.argv[1])
    lba, size = names["MAKE2S.BIN"]
    blob = read_extent(track, lba, size)
    entries = awdisasm.read_memlist(sys.argv[2])
    if sys.argv[3] == "list":
        report(blob, sys.argv[2], entries, names)
    elif sys.argv[3] == "loader":
        loader(blob, names)
    else:
        dump(blob, sys.argv[2], entries, names, sys.argv[3].upper())

if __name__ == "__main__":
    main()
