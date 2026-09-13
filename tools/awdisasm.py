import os
import struct
import sys

MEMENTRY_SIZE = 20
STATE_END = 0xFF
RT_BYTECODE = 4

PART_FIRST = 0x3E80
MEMLIST_PARTS = [
    (0x14, 0x15, 0x16, 0x00),
    (0x17, 0x18, 0x19, 0x00),
    (0x1A, 0x1B, 0x1C, 0x11),
    (0x1D, 0x1E, 0x1F, 0x11),
    (0x20, 0x21, 0x22, 0x11),
    (0x23, 0x24, 0x25, 0x00),
    (0x26, 0x27, 0x28, 0x11),
    (0x29, 0x2A, 0x2B, 0x11),
    (0x7D, 0x7E, 0x7F, 0x00),
    (0x7D, 0x7E, 0x7F, 0x00),
]

class Entry(object):

    def __init__(self, raw):
        (self.state, self.type, _buf, _unk4, self.rank, self.bank,
         self.offset, _unkC, self.packed, _unk10, self.size) = struct.unpack(
            ">BBHHBBIHHHH", raw)

def read_memlist(data_dir):
    path = os.path.join(data_dir, "memlist.bin")
    blob = open(path, "rb").read()
    out = []
    pos = 0
    while pos + MEMENTRY_SIZE <= len(blob):
        entry = Entry(blob[pos:pos + MEMENTRY_SIZE])
        if entry.state == STATE_END:
            break
        out.append(entry)
        pos += MEMENTRY_SIZE
    return out

class Unpacker(object):

    def __init__(self, buf, packed_size):
        self.buf = buf
        self.i = packed_size - 4
        self.size = 0
        self.datasize = self.read_be32()
        self.o = self.datasize - 1
        self.crc = self.read_be32()
        self.chk = self.read_be32()
        self.crc ^= self.chk

    def read_be32(self):
        val = struct.unpack(">I", bytes(self.buf[self.i:self.i + 4]))[0]
        self.i -= 4
        return val

    def rcr(self, carry):
        out = self.chk & 1
        self.chk >>= 1
        if carry:
            self.chk |= 0x80000000
        return out

    def next_chunk(self):
        carry = self.rcr(False)
        if self.chk == 0:
            self.chk = self.read_be32()
            self.crc ^= self.chk
            carry = self.rcr(True)
        return carry

    def get_code(self, num_chunks):
        code = 0
        for _ in range(num_chunks):
            code = (code << 1) | (1 if self.next_chunk() else 0)
        return code

    def dec_literal(self, num_chunks, add_count):
        count = self.get_code(num_chunks) + add_count + 1
        self.datasize -= count
        for _ in range(count):
            self.buf[self.o] = self.get_code(8)
            self.o -= 1

    def dec_copy(self, num_chunks):
        back = self.get_code(num_chunks)
        count = self.size + 1
        self.datasize -= count
        for _ in range(count):
            self.buf[self.o] = self.buf[self.o + back]
            self.o -= 1

    def run(self):
        while self.datasize > 0:
            self.size = 0
            if not self.next_chunk():
                self.size = 1
                if not self.next_chunk():
                    self.dec_literal(3, 0)
                else:
                    self.dec_copy(8)
            else:
                code = self.get_code(2)
                if code == 3:
                    self.dec_literal(8, 8)
                elif code < 2:
                    self.size = code + 2
                    self.dec_copy(code + 9)
                else:
                    self.size = self.get_code(8)
                    self.dec_copy(12)
        return self.crc == 0

def load_resource(data_dir, entry):
    path = os.path.join(data_dir, "bank%02x" % entry.bank)
    handle = open(path, "rb")
    handle.seek(entry.offset)
    packed = handle.read(entry.packed)
    handle.close()

    buf = bytearray(max(entry.size, entry.packed))
    buf[0:len(packed)] = packed

    if entry.packed == entry.size:
        return buf[:entry.size], True

    ok = Unpacker(buf, entry.packed).run()
    return buf[:entry.size], ok

OPCODE_WIDTHS = {
    0x00: 3, 0x01: 2, 0x02: 2, 0x03: 3,
    0x04: 2, 0x05: 0, 0x06: 0, 0x07: 2,
    0x08: 3, 0x09: 3, 0x0A: None, 0x0B: 2,
    0x0C: 3, 0x0D: 1, 0x0E: 2, 0x0F: 2,
    0x10: 1, 0x11: 0, 0x12: 5, 0x13: 2,
    0x14: 3, 0x15: 3, 0x16: 3, 0x17: 3,
    0x18: 5, 0x19: 2, 0x1A: 5,
}

OPCODE_NAMES = {0x00: "movConst", 0x18: "playSound", 0x19: "updateMemList",
                0x1A: "playMusic"}

def cond_jmp_width(code, pos):
    sub = code[pos]
    if sub & 0x80:
        return 5
    if sub & 0x40:
        return 6
    return 5

def poly_width(code, pos, opcode):
    if opcode & 0x80:
        return 3

    width = 4
    if not (opcode & 0x20) and not (opcode & 0x10):
        width += 1
    if not (opcode & 8) and not (opcode & 4):
        width += 1
    if ((opcode >> 1) & 1) != (opcode & 1):
        width += 1
    return width

def walk(code, name):
    hits = []
    pos = 0
    total = len(code)

    while pos < total:
        start = pos
        opcode = code[pos]
        pos += 1

        if opcode & 0xC0:
            width = poly_width(code, pos, opcode)
            if width is None or pos + width > total:
                print("  %s: undecodable polygon at 0x%04X" % (name, start))
                break
            pos += width
            continue

        if opcode > 0x1A:
            print("  %s: invalid opcode 0x%02X at 0x%04X" % (name, opcode, start))
            break

        width = OPCODE_WIDTHS[opcode]
        if width is None:
            width = cond_jmp_width(code, pos)

        if pos + width > total:
            print("  %s: operand runs past end at 0x%04X" % (name, start))
            break

        if opcode in OPCODE_NAMES:
            operands = code[pos:pos + width]
            hits.append((start, OPCODE_NAMES[opcode], bytes(operands)))

        pos += width

    return hits

def main():
    data_dir = sys.argv[1] if len(sys.argv) > 1 else "saturn/cd/data"
    entries = read_memlist(data_dir)
    print("memlist: %d entries" % len(entries))

    for index, part in enumerate(MEMLIST_PARTS):
        res_id = part[1]
        entry = entries[res_id]
        code, ok = load_resource(data_dir, entry)
        label = "PART%d (0x%04X) code=0x%02X" % (index + 1, PART_FIRST + index,
                                                 res_id)
        print("")
        print("%s  size=%d crc_ok=%s type=%d" % (label, len(code), ok,
                                                 entry.type))

        for offset, opname, operands in walk(code, label):
            if opname == "playMusic":
                res_num = struct.unpack(">H", operands[0:2])[0]
                delay = struct.unpack(">H", operands[2:4])[0]
                pos_op = operands[4]
                print("  0x%04X playMusic res=0x%04X (%d) delay=%d pos=%d"
                      % (offset, res_num, res_num, delay, pos_op))
            elif opname == "movConst" and operands[0] == 0x00:
                value = struct.unpack(">H", operands[1:3])[0]
                print("  0x%04X checkpoint := %d" % (offset, value))

if __name__ == "__main__":
    main()
