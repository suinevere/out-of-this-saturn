import os
import re
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PNG = os.path.join(ROOT, "tools", "assets", "png", "ootw.png")
ART = os.path.join(ROOT, "saturn", "src", "menus", "menu_art.cxx")

PAGE_W, PAGE_H = 320, 200
LOGO_BOX = (8, 20, 304, 90)

FREE_SLOTS = (1, 2, 3, 11)
FIXED_SLOTS = {0: (0, 0, 0), 4: (0, 4, 4), 5: (2, 6, 6), 6: (4, 9, 9), 15: (15, 15, 15)}

DIM_ROW = (8, 9, 10)
LIT_ROW = (12, 13, 14)

ALPHA_FLOOR = 24

GAIN = 0.7

def load_page():
    im = Image.open(PNG).convert("RGBA")
    im = im.crop(im.split()[3].point(lambda a: 255 if a >= ALPHA_FLOOR else 0).getbbox())
    back = Image.new("RGBA", im.size, (0, 0, 0, 255))
    im = Image.alpha_composite(back, im).convert("RGB").point(lambda v: int(v * GAIN))
    x0, y0, bw, bh = LOGO_BOX
    scale = min(bw / im.width, bh / im.height)
    im = im.resize((round(im.width * scale), round(im.height * scale)), Image.LANCZOS)
    page = Image.new("RGB", (PAGE_W, PAGE_H), (0, 0, 0))
    page.paste(im, (x0 + (bw - im.width) // 2, y0 + (bh - im.height) // 2))
    return page

def choose_palette(page):
    lit = [p for p in page.get_flattened_data() if max(p) > 24]
    sample = Image.new("RGB", (len(lit), 1))
    sample.putdata(lit)
    q = sample.quantize(colors=len(FREE_SLOTS), method=Image.MEDIANCUT)
    flat = q.getpalette()[:len(FREE_SLOTS) * 3]
    pal = [None] * 16
    for slot, rgb in FIXED_SLOTS.items():
        pal[slot] = rgb
    chosen = sorted(tuple((flat[n * 3 + c] + 8) // 17 for c in range(3))
                    for n in range(len(FREE_SLOTS)))
    chosen.sort(key=sum)
    for slot, rgb in zip(FREE_SLOTS, chosen):
        pal[slot] = rgb
    mid = tuple((a + b) // 2 for a, b in zip(chosen[1], chosen[2]))
    shine = tuple((c + 15) // 2 for c in chosen[3])
    for slot, rgb in zip(DIM_ROW, (chosen[0], chosen[1], mid)):
        pal[slot] = rgb
    for slot, rgb in zip(LIT_ROW, (chosen[2], chosen[3], shine)):
        pal[slot] = rgb
    return pal

def map_pixels(page, pal):
    usable = [(s, tuple(c * 17 for c in rgb)) for s, rgb in enumerate(pal)
              if rgb is not None and s not in DIM_ROW + LIT_ROW]
    cache = {}
    out = []
    for p in page.get_flattened_data():
        if p not in cache:
            cache[p] = min(usable, key=lambda e: sum((a - b) ** 2 for a, b in zip(p, e[1])))[0]
        out.append(cache[p])
    return out

def c_bytes(data, per_line):
    lines = []
    for i in range(0, len(data), per_line):
        lines.append("\t" + " ".join("0x%02X," % b for b in data[i:i + per_line]))
    return "\n".join(lines) + "\n"

def rewrite(pixels, pal):
    src = open(ART, newline="").read()
    nl = "\r\n" if "\r\n" in src else "\n"
    src = src.replace("\r\n", "\n")
    page = bytes((pixels[i] << 4) | pixels[i + 1] for i in range(0, len(pixels), 2))
    src = re.sub(r"(const uint8_t MENU_ART_TITLE_BACKDROP\[32000\] = \{\n).*?(\};)",
                 lambda m: m.group(1) + c_bytes(page, 12) + m.group(2), src, count=1, flags=re.S)
    src = patch_palette(src, "MENU_ART_TITLE_PALETTE", FREE_SLOTS + DIM_ROW + LIT_ROW, pal)
    src = patch_palette(src, "MENU_ART_PALETTE", DIM_ROW + LIT_ROW, pal)
    open(ART, "w", newline="").write(src.replace("\n", nl))

def patch_palette(src, name, slots, pal):
    m = re.search(r"const uint8_t %s\[32\] = \{\n(.*?)\};" % name, src, re.S)
    entries = re.findall(r"\t0x([0-9A-F]{2}), 0x([0-9A-F]{2}),", m.group(1))
    for slot in slots:
        r, g, b = pal[slot]
        entries[slot] = ("%02X" % r, "%02X" % ((g << 4) | b))
    body = "".join("\t0x%s, 0x%s,\n" % e for e in entries)
    return src[:m.start(1)] + body + src[m.end(1):]

if __name__ == "__main__":
    page = load_page()
    pal = choose_palette(page)
    rewrite(map_pixels(page, pal), pal)
    print("title palette:", {s: pal[s] for s in FREE_SLOTS + DIM_ROW + LIT_ROW})
    sys.exit(0)
