import difflib
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import awdisasm

SECTOR_BYTES = 2352
USER_BYTES = 2048
USER_OFFSET = 16

STOP_RES = 4000

RES_TO_CUE = {}
for _res in range(4001, 4026):
    if _res == 4019:
        _cue = 16
    elif _res < 4019:
        _cue = _res - 4000 + 1
    else:
        _cue = _res - 4000
    RES_TO_CUE[_res] = (_cue, 0 if _res == 4025 else 1)

CHAPTERS = [
    ("INTRO7.MAC", "GAME_PART2", 0x18),
    ("EAU3.MAC", "GAME_PART3", 0x1B),
    ("PRI3.MAC", "GAME_PART4", 0x1E),
    ("CITE1.MAC", "GAME_PART5", 0x21),
    ("ARENE2.MAC", "GAME_PART6", 0x24),
    ("LUXE2.MAC", "GAME_PART7", 0x27),
    ("FINAL3.MAC", "GAME_PART8", 0x2A),
]

def sector_user(track, lba):
    track.seek(lba * SECTOR_BYTES + USER_OFFSET)
    return track.read(USER_BYTES)

def read_extent(track, lba, size):
    out = bytearray()
    for i in range((size + USER_BYTES - 1) // USER_BYTES):
        out += sector_user(track, lba + i)
    return bytes(out[:size])

def read_iso_files(path):
    track = open(path, "rb")
    pvd = sector_user(track, 16)
    root = pvd[156:156 + 34]
    root_lba = struct.unpack("<I", root[2:6])[0]
    root_size = struct.unpack("<I", root[10:14])[0]
    blob = read_extent(track, root_lba, root_size)

    files = {}
    pos = 0
    while pos < len(blob):
        length = blob[pos]
        if length == 0:
            pos = (pos // USER_BYTES + 1) * USER_BYTES
            if pos >= len(blob):
                break
            continue
        rec = blob[pos:pos + length]
        lba = struct.unpack("<I", rec[2:6])[0]
        size = struct.unpack("<I", rec[10:14])[0]
        flags = rec[25]
        name = rec[33:33 + rec[32]].decode("latin-1").split(";")[0]
        if not (flags & 2) and size > 0:
            files[name] = read_extent(track, lba, size)
        pos += length

    track.close()
    return files

def decode(code):
    out = []
    pos = 0
    total = len(code)

    while pos < total:
        start = pos
        opcode = code[pos]
        pos += 1

        if opcode & 0xC0:
            width = awdisasm.poly_width(code, pos, opcode)
        elif opcode > 0x1A:
            break
        else:
            width = awdisasm.OPCODE_WIDTHS[opcode]
            if width is None:
                width = awdisasm.cond_jmp_width(code, pos)

        if pos + width > total:
            break

        operands = bytes(code[pos:pos + width])

        if opcode in (0x04, 0x07):
            key = "%02X" % opcode
        elif opcode == 0x09:
            key = "%02X:%02X" % (opcode, operands[0])
        elif opcode == 0x0A:
            key = "%02X:%s" % (opcode, operands[:-2].hex())
        else:
            key = "%02X:%s" % (opcode, operands.hex())

        out.append((start, key, opcode))
        pos += width

    return out

def chapter_cues(mac_code, amiga_code):
    mac = decode(mac_code)
    amiga = decode(amiga_code)

    matcher = difflib.SequenceMatcher(None, [i[1] for i in mac],
                                      [i[1] for i in amiga], autojunk=False)
    blocks = matcher.get_matching_blocks()
    aligned = sum(b.size for b in blocks)
    pct = 100.0 * aligned / max(1, len(mac))

    cues = []
    for index, (offset, _key, opcode) in enumerate(mac):
        if opcode != 0x1A:
            continue
        res_num = struct.unpack(">H", mac_code[offset + 1:offset + 3])[0]
        pos = mac_code[offset + 5]
        if res_num != STOP_RES and res_num not in RES_TO_CUE:
            continue
        for block in blocks:
            if block.size and block.a > index:
                cues.append((amiga[block.b][0], res_num, pos))
                break

    cues.sort()
    return cues, pct

def main():
    disc = sys.argv[1]
    data_dir = sys.argv[2]
    out_path = sys.argv[3]

    files = read_iso_files(disc)
    entries = awdisasm.read_memlist(data_dir)

    rows = []

    for mac_name, part, res_id in CHAPTERS:
        if mac_name not in files:
            sys.stderr.write("missing %s on the disc\n" % mac_name)
            continue
        amiga_code, ok = awdisasm.load_resource(data_dir, entries[res_id])
        if not ok:
            sys.stderr.write("resource 0x%02X failed its crc\n" % res_id)
            continue
        cues, _ = chapter_cues(bytearray(files[mac_name]), amiga_code)
        for address, res_num, pos in cues:
            if res_num == STOP_RES:
                rows.append((part, address, 0, 0, pos, res_num))
            else:
                cue, loops = RES_TO_CUE[res_num]
                rows.append((part, address, cue, loops, pos, res_num))

    out = []
    out.append("#ifndef PART_CUES_H")
    out.append("#define PART_CUES_H")
    out.append("")
    out.append("#define PART_CUES_LIST(X) \\")
    for part, address, cue, loops, pos, res_num in rows:
        out.append("\tX(%-11s 0x%04X, %2d, %d, %d) \\"
                   % (part + ",", address, cue, loops, pos))
    out.append("")
    out.append("#define PART_CUES_COUNT %d" % len(rows))
    out.append("")
    out.append("#endif")

    open(out_path, "w", newline="\n").write("\n".join(out) + "\n")
    sys.stderr.write("wrote %s: %d rows\n" % (out_path, len(rows)))

if __name__ == "__main__":
    main()
