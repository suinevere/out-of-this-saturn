import os
import sys
import struct
import importlib.util

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))

MEMENTRY_SIZE = 20
INTRO_CODE_RES = 0x18

def _load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod

def restore(data_dir):
    mlpath = os.path.join(data_dir, "memlist.bin")
    blob = bytearray(open(mlpath, "rb").read())
    F = ">BBHHBBIHHHH"

    def get(i):
        o = i * MEMENTRY_SIZE
        return list(struct.unpack(F, bytes(blob[o:o + MEMENTRY_SIZE])))

    prev, cur, nxt = get(INTRO_CODE_RES - 1), get(INTRO_CODE_RES), get(INTRO_CODE_RES + 1)
    if not (prev[5] == nxt[5]):
        print("neighbours are not in one bank; cannot reconstruct")
        return 1

    offset = prev[6] + prev[8]
    packed = nxt[6] - offset
    bank = prev[5]
    data = open(os.path.join(data_dir, "bank%02x" % bank), "rb").read()
    size = struct.unpack(">I", data[offset + packed - 4:offset + packed])[0]

    print("entry 0x%02X: %d/%d/%d -> %d/%d/%d"
          % (INTRO_CODE_RES, cur[6], cur[8], cur[10], offset, packed, size))
    cur[5], cur[6], cur[8], cur[10] = bank, offset, packed, size
    o = INTRO_CODE_RES * MEMENTRY_SIZE
    blob[o:o + MEMENTRY_SIZE] = struct.pack(F, *cur)
    open(mlpath, "wb").write(bytes(blob))
    return 0

def main():
    if len(sys.argv) >= 3 and sys.argv[1] == "--restore":
        return restore(sys.argv[2])

    if len(sys.argv) < 3:
        print("usage: awswapintro.py <HOTA Track 01.bin> <cd/data dir>")
        print("       awswapintro.py --restore <cd/data dir>")
        return 1

    track, data_dir = sys.argv[1], sys.argv[2]
    awd = _load(os.path.join(ROOT, "tools", "awdisasm.py"), "awdisasm")
    cues = _load(os.path.join(ROOT, "tools", "awcues.py"), "awcues")

    mac = bytes(cues.read_iso_files(track)["INTRO7.MAC"])
    print("INTRO7.MAC: %d bytes" % len(mac))
    if len(mac) > 0xFFFF:
        print("too large for a 16-bit size field")
        return 1

    mlpath = os.path.join(data_dir, "memlist.bin")
    blob = bytearray(open(mlpath, "rb").read())
    off = INTRO_CODE_RES * MEMENTRY_SIZE
    (state, typ, buf, unk4, rank, bank, offset,
     unkC, packed, unk10, size) = struct.unpack(">BBHHBBIHHHH",
                                                bytes(blob[off:off + MEMENTRY_SIZE]))
    print("entry 0x%02X: bank %02x offset %d packed %d size %d"
          % (INTRO_CODE_RES, bank, offset, packed, size))

    bankpath = os.path.join(data_dir, "bank%02x" % bank)
    data = bytearray(open(bankpath, "rb").read())
    new_offset = len(data)
    data += mac
    open(bankpath, "wb").write(bytes(data))
    print("appended to %s at offset %d (file now %d bytes)"
          % (os.path.basename(bankpath), new_offset, len(data)))

    blob[off:off + MEMENTRY_SIZE] = struct.pack(
        ">BBHHBBIHHHH", state, typ, buf, unk4, rank, bank,
        new_offset, unkC, len(mac), unk10, len(mac))
    open(mlpath, "wb").write(bytes(blob))
    print("memlist entry repointed: offset %d packed %d size %d (unpacked)"
          % (new_offset, len(mac), len(mac)))

    check, ok = awd.load_resource(data_dir, awd.read_memlist(data_dir)[INTRO_CODE_RES])
    print("readback: %d bytes, matches INTRO7.MAC: %s"
          % (len(check), bytes(check) == mac))
    return 0

if __name__ == "__main__":
    sys.exit(main())
