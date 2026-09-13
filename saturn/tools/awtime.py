import os
import sys
import importlib.util

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))

_spec = importlib.util.spec_from_file_location(
    "awdisasm", os.path.join(ROOT, "tools", "awdisasm.py"))
awd = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(awd)

PART_CODE = [0x15, 0x18, 0x1B, 0x1E, 0x21, 0x24, 0x27, 0x2A, 0x7E, 0x7E]

NUM_THREADS = 64
INACTIVE = 0xFFFF
NO_SETVEC = 0xFFFF
FRAME_CAP = 500000

def s16(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v

class VM(object):

    def __init__(self, code):
        self.c = code
        self.v = [0] * 256
        self.v[0x54] = 0x81
        self.v[0x3C] = 0x1234
        self.v[0xBC] = 0x10
        self.v[0xC6] = 0x80
        self.v[0xF2] = 4000
        self.v[0xDC] = 33
        self.v[0xE4] = 0x14
        self.pc_off = [INACTIVE] * NUM_THREADS
        self.req_pc = [NO_SETVEC] * NUM_THREADS
        self.act = [0] * NUM_THREADS
        self.req_act = [0] * NUM_THREADS
        self.pc_off[0] = 0
        self.pc = 0
        self.stack = []
        self.goto_next = False
        self.slices = 0
        self.frames = 0
        self.music = []
        self.sfx = []
        self.pal = []
        self.done = None

    def b(self):
        x = self.c[self.pc]
        self.pc += 1
        return x

    def w(self):
        x = (self.c[self.pc] << 8) | self.c[self.pc + 1]
        self.pc += 2
        return x

    def exec_thread(self):
        while not self.goto_next:
            if self.pc >= len(self.c):
                self.goto_next = True
                self.done = "ran off the end"
                return
            op = self.b()

            if op & 0x80:
                self.b(); self.b(); self.b()
                continue
            if op & 0x40:
                self.w()
                self.b()
                if not (op & 0x20) and not (op & 0x10):
                    self.b()
                self.b()
                if not (op & 8) and not (op & 4):
                    self.b()
                self.b()
                if (not (op & 2) and not (op & 1)) or ((op & 2) and (op & 1)):
                    self.pc -= 1
                continue

            if op == 0x00:
                i = self.b(); self.v[i] = s16(self.w())
            elif op == 0x01:
                d = self.b(); s = self.b(); self.v[d] = self.v[s]
            elif op == 0x02:
                d = self.b(); s = self.b(); self.v[d] = s16(self.v[d] + self.v[s])
            elif op == 0x03:
                i = self.b(); self.v[i] = s16(self.v[i] + s16(self.w()))
            elif op == 0x04:
                off = self.w(); self.stack.append(self.pc); self.pc = off
            elif op == 0x05:
                if not self.stack:
                    self.goto_next = True
                    self.done = "stack underflow at 0x%04X" % (self.pc - 1)
                    return
                self.pc = self.stack.pop()
            elif op == 0x06:
                self.goto_next = True
            elif op == 0x07:
                self.pc = self.w()
            elif op == 0x08:
                t = self.b(); self.req_pc[t] = self.w()
            elif op == 0x09:
                i = self.b(); self.v[i] = s16(self.v[i] - 1)
                if self.v[i] != 0:
                    self.pc = self.w()
                else:
                    self.w()
            elif op == 0x0A:
                sub = self.b(); bb = self.v[self.b()]
                if sub & 0x80:
                    a = self.v[self.b()]
                elif sub & 0x40:
                    a = s16(self.w())
                else:
                    a = self.b()
                k = sub & 7
                e = ((bb == a) if k == 0 else (bb != a) if k == 1 else
                     (bb > a) if k == 2 else (bb >= a) if k == 3 else
                     (bb < a) if k == 4 else (bb <= a) if k == 5 else False)
                if e:
                    self.pc = self.w()
                else:
                    self.w()
            elif op == 0x0B:
                self.pal.append((self.slices, self.w() >> 8))
            elif op == 0x0C:
                t = self.b(); i = self.b() & (NUM_THREADS - 1)
                n = (i - t) & 0xFF
                if n >= 0x80:
                    n -= 0x100
                if n < 0:
                    continue
                n += 1
                a = self.b()
                for k in range(n):
                    if t + k >= NUM_THREADS:
                        break
                    if a == 2:
                        self.req_pc[t + k] = 0xFFFE
                    elif a < 2:
                        self.req_act[t + k] = a
            elif op == 0x0D:
                self.b()
            elif op == 0x0E:
                self.b(); self.b()
            elif op == 0x0F:
                self.b(); self.b()
            elif op == 0x10:
                self.b()
                self.slices += self.v[0xFF]
                self.frames += 1
                self.v[0xF7] = 0
            elif op == 0x11:
                self.pc = INACTIVE
                self.goto_next = True
                return
            elif op == 0x12:
                self.w(); self.b(); self.b(); self.b()
            elif op == 0x13:
                d = self.b(); s = self.b(); self.v[d] = s16(self.v[d] - self.v[s])
            elif op == 0x14:
                i = self.b(); self.v[i] = s16((self.v[i] & 0xFFFF) & self.w())
            elif op == 0x15:
                i = self.b(); self.v[i] = s16((self.v[i] & 0xFFFF) | self.w())
            elif op == 0x16:
                i = self.b()
                self.v[i] = s16(((self.v[i] & 0xFFFF) << (self.w() & 15)) & 0xFFFF)
            elif op == 0x17:
                i = self.b(); self.v[i] = s16((self.v[i] & 0xFFFF) >> (self.w() & 15))
            elif op == 0x18:
                r = self.w(); fq = self.b(); vol = self.b(); ch = self.b()
                self.sfx.append((self.slices, r, fq, vol, ch))
            elif op == 0x19:
                r = self.w()
                if r >= 16000:
                    self.done = "part switch to 0x%X" % r
                    self.goto_next = True
                    return
            elif op == 0x1A:
                self.music.append(
                    (self.frames, self.slices, self.w(), self.w(), self.b()))
            else:
                self.goto_next = True
                self.done = "bad opcode 0x%02X at 0x%04X" % (op, self.pc - 1)
                return

    def check_requests(self):
        for t in range(NUM_THREADS):
            self.act[t] = self.req_act[t]
            n = self.req_pc[t]
            if n != NO_SETVEC:
                self.pc_off[t] = INACTIVE if n == 0xFFFE else n
                self.req_pc[t] = NO_SETVEC

    def host_frame(self):
        for t in range(NUM_THREADS):
            if self.act[t]:
                continue
            n = self.pc_off[t]
            if n == INACTIVE:
                continue
            self.pc = n
            self.stack = []
            self.goto_next = False
            self.exec_thread()
            if self.done:
                return
            self.pc_off[t] = self.pc if self.pc <= 0xFFFF else INACTIVE

def run_part(data_dir, part_index):
    memlist = awd.read_memlist(data_dir)
    code, crc_ok = awd.load_resource(data_dir, memlist[PART_CODE[part_index]])
    if not crc_ok:
        print("warning: bytecode resource failed its checksum")
    vm = VM(code)
    for _ in range(FRAME_CAP):
        vm.check_requests()
        if vm.done:
            break
        vm.host_frame()
        if vm.done:
            break
    return vm

def main():
    data_dir = sys.argv[1] if len(sys.argv) > 1 else \
        os.path.join(ROOT, "saturn", "cd", "data")
    part = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    track_ms = int(sys.argv[3]) if len(sys.argv) > 3 else 156053

    vm = run_part(data_dir, part)
    print("part index %d: stopped on %s" % (part, vm.done))
    print("  displayed frames : %d" % vm.frames)
    print("  total slices     : %d" % vm.slices)
    for frames, slices, res, delay, pos in vm.music:
        print("  playMusic frame %-5d slice %-6d res=0x%X delay=%d pos=%d"
              % (frames, slices, res, delay, pos))
    if vm.slices:
        print("  at 20 ms a slice : %.2f s" % (vm.slices * 0.020))
        print("  track            : %.2f s" % (track_ms / 1000.0))
        print("  slice must be    : %d/%d ms = %.4f"
              % (track_ms, vm.slices, float(track_ms) / vm.slices))

if __name__ == "__main__":
    main()
