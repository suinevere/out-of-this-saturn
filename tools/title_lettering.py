import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, "saturn", "src", "menu_art.cxx")

COLS = (4, 3, 3, 3, 4)
ROWS = (4, 3, 3, 3, 4)
GAP = 3
SPACE = 6

ART_W, ART_H = 200, 18

GLYPHS = {
    "A": ["o###o", "#...#", "#####", "#...#", "#...#"],
    "C": ["#####", "#....", "#....", "#....", "#####"],
    "D": ["####o", "#...#", "#...#", "#...#", "####o"],
    "E": ["#####", "#....", "####.", "#....", "#####"],
    "G": ["#####", "#....", "#.###", "#...#", "#####"],
    "I": ["#", "#", "#", "#", "#"],
    "L": ["#....", "#....", "#....", "#....", "#####"],
    "M": ["#####", "#.#.#", "#.#.#", "#...#", "#...#"],
    "N": ["####o", "#...#", "#...#", "#...#", "#...#"],
    "O": ["#####", "#...#", "#...#", "#...#", "#####"],
    "P": ["#####", "#...#", "#####", "#....", "#...."],
    "R": ["####o", "#...#", "####o", "#....", "#...."],
    "S": ["#####", "#....", "#####", "....#", "#####"],
    "T": ["#####", "..#..", "..#..", "..#..", "..#.."],
    "U": ["#...#", "#...#", "#...#", "#...#", "#####"],
}

ROUND = 3

GLYPH_COLS = {"T": (4, 3, 4, 3, 4)}

DIAGONALS = {"R": (10, 4)}

STRINGS = {
    "s_startGameBits": "START GAME",
    "s_loadGameBits": "LOAD GAME",
    "s_continueGameBits": "CONTINUE",
    "s_optionsBits": "OPTIONS",
}

def glyph_mask(ch):
    cells = GLYPHS[ch]
    widths = GLYPH_COLS.get(ch, COLS) if len(cells[0]) == 5 else (COLS[0],)
    w, h = sum(widths), sum(ROWS)
    mask = [[False] * w for _ in range(h)]
    xs = [sum(widths[:c]) for c in range(len(widths))]
    ys = [sum(ROWS[:r]) for r in range(len(ROWS))]

    def cell(r, c):
        return 0 <= r < len(ROWS) and 0 <= c < len(widths) and cells[r][c] != "."

    for r in range(len(ROWS)):
        for c in range(len(widths)):
            if cell(r, c):
                for yy in range(ys[r], ys[r] + ROWS[r]):
                    for xx in range(xs[c], xs[c] + widths[c]):
                        mask[yy][xx] = True
    for r in range(len(ROWS)):
        for c in range(len(widths)):
            if cells[r][c] != "o":
                continue
            for dr in (-1, 1):
                for dc in (-1, 1):
                    if cell(r + dr, c) or cell(r, c + dc):
                        continue
                    cx = xs[c] if dc < 0 else xs[c] + widths[c] - 1
                    cy = ys[r] if dr < 0 else ys[r] + ROWS[r] - 1
                    for k in range(ROUND):
                        for m in range(ROUND - k):
                            mask[cy - dr * k][cx - dc * m] = False
                    if cell(r - dr, c - dc):
                        continue
                    iy = ys[r - dr] if dr < 0 else ys[r - dr] + ROWS[r - dr] - 1
                    ix = xs[c - dc] if dc < 0 else xs[c - dc] + widths[c - dc] - 1
                    for k in range(ROUND):
                        for m in range(ROUND - k):
                            mask[iy - dr * k][ix - dc * m] = True

    if ch in DIAGONALS:
        top, left = DIAGONALS[ch]
        for yy in range(top, h):
            for xx in range(left + yy - top, min(w, left + yy - top + 2 * COLS[-1])):
                mask[yy][xx] = True

    def filled(xx, yy):
        return 0 <= xx < w and 0 <= yy < h and mask[yy][xx]

    corners = [(xx, yy) for yy in range(h) for xx in range(w) if mask[yy][xx]
               and ((not filled(xx - 1, yy) and not filled(xx, yy - 1))
                    or (not filled(xx + 1, yy) and not filled(xx, yy - 1))
                    or (not filled(xx - 1, yy) and not filled(xx, yy + 1))
                    or (not filled(xx + 1, yy) and not filled(xx, yy + 1)))]
    for xx, yy in corners:
        mask[yy][xx] = False
    return mask

def render(text):
    masks = [None if ch == " " else glyph_mask(ch) for ch in text]
    total = sum(SPACE if m is None else len(m[0]) for m in masks) + GAP * (len(masks) - 1)
    if total + 1 > ART_W:
        raise SystemExit("%s is %d pixels, wider than %d" % (text, total + 1, ART_W))
    px = [[0] * ART_W for _ in range(ART_H)]
    x = (ART_W - total - 1) // 2
    for m in masks:
        if m is None:
            x += SPACE + GAP
            continue
        for yy, row in enumerate(m):
            for xx, on in enumerate(row):
                if on:
                    px[yy][x + xx] = 2
        x += len(m[0]) + GAP
    face = [(xx, yy) for yy in range(ART_H) for xx in range(ART_W) if px[yy][xx] == 2]
    for xx, yy in face:
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                nx, ny = xx + dx, yy + dy
                if not (0 <= nx < ART_W and 0 <= ny < ART_H) or px[ny][nx] == 0:
                    px[yy][xx] = 3
    for xx, yy in face:
        nx, ny = xx + 1, yy + 1
        if nx < ART_W and ny < ART_H and px[ny][nx] == 0:
            px[ny][nx] = 1
    return px

def pack(px):
    out = bytearray()
    for row in px:
        for x in range(0, ART_W, 4):
            out.append((row[x] << 6) | (row[x + 1] << 4) | (row[x + 2] << 2) | row[x + 3])
    return bytes(out)

def c_bytes(data):
    return "".join("\t" + " ".join("0x%02X," % b for b in data[i:i + 12]) + "\n"
                   for i in range(0, len(data), 12))

if __name__ == "__main__":
    src = open(ART, newline="").read()
    nl = "\r\n" if "\r\n" in src else "\n"
    src = src.replace("\r\n", "\n")
    for name, text in STRINGS.items():
        data = pack(render(text))
        src, n = re.subn(r"(static const uint8_t %s\[)\d+(\] = \{\n).*?(\};)" % name,
                         lambda m: m.group(1) + str(len(data)) + m.group(2) + c_bytes(data) + m.group(3),
                         src, count=1, flags=re.S)
        if n != 1:
            raise SystemExit("%s not found in %s" % (name, ART))
        print("%-20s %s" % (name, text))
    open(ART, "w", newline="").write(src.replace("\n", nl))
    sys.exit(0)
