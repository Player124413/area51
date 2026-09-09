#!/usr/bin/env python3
"""
make_icons.py - regenerates the launcher icons.

Pure standard library (zlib + struct), no PIL needed, so anybody can re-run it:

    python3 tools/make_icons.py

The icon is drawn by hand: a dark panel, the amber warning triangle from the
game's UI, and a cyan scan line.
"""

import os
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
RES = os.path.normpath(os.path.join(HERE, "..", "app", "src", "main", "res"))

DENSITIES = {
    "mipmap-mdpi": 48,
    "mipmap-hdpi": 72,
    "mipmap-xhdpi": 96,
    "mipmap-xxhdpi": 144,
    "mipmap-xxxhdpi": 192,
}

BG = (7, 11, 18)
BORDER = (42, 53, 70)
AMBER = (255, 167, 38)
CYAN = (51, 198, 244)


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def draw(size):
    px = bytearray()
    r = size * 0.22          # corner radius
    cx = cy = size / 2.0
    for y in range(size):
        for x in range(size):
            # ---- rounded square mask
            dx = max(r - x, x - (size - 1 - r), 0.0)
            dy = max(r - y, y - (size - 1 - r), 0.0)
            inside = (dx * dx + dy * dy) <= r * r
            if not inside:
                px += bytes((0, 0, 0, 0))
                continue

            # ---- background with a soft vertical gradient
            t = y / float(size)
            col = lerp((13, 20, 32), BG, t)

            # ---- border
            edge = min(x, y, size - 1 - x, size - 1 - y)
            if edge < max(1, size // 32):
                col = BORDER

            # ---- amber warning triangle (outline)
            tri_h = size * 0.52
            tri_w = size * 0.60
            top = (cx, cy - tri_h * 0.45)
            left = (cx - tri_w / 2, cy + tri_h * 0.55)
            right = (cx + tri_w / 2, cy + tri_h * 0.55)
            thick = max(1.5, size * 0.045)
            if near_segment(x, y, top, left, thick) or \
               near_segment(x, y, left, right, thick) or \
               near_segment(x, y, right, top, thick):
                col = AMBER

            # ---- cyan scan line through the middle
            if abs(y - cy) < max(1.0, size * 0.02) and abs(x - cx) < tri_w * 0.32:
                col = CYAN

            px += bytes((col[0], col[1], col[2], 255))
    return px


def near_segment(px, py, a, b, thick):
    ax, ay = a
    bx, by = b
    vx, vy = bx - ax, by - ay
    length2 = vx * vx + vy * vy
    if length2 == 0:
        return False
    t = ((px - ax) * vx + (py - ay) * vy) / length2
    if t < 0.0:
        t = 0.0
    elif t > 1.0:
        t = 1.0
    dx = px - (ax + vx * t)
    dy = py - (ay + vy * t)
    return (dx * dx + dy * dy) <= thick * thick


def write_png(path, size, pixels, circular=False):
    raw = bytearray()
    for y in range(size):
        raw.append(0)  # filter type 0
        row = y * size * 4
        for x in range(size):
            i = row + x * 4
            a = pixels[i + 3]
            if circular:
                dx = x - (size - 1) / 2.0
                dy = y - (size - 1) / 2.0
                radius = size / 2.0
                if dx * dx + dy * dy > radius * radius:
                    a = 0
            raw += bytes((pixels[i], pixels[i + 1], pixels[i + 2], a))

    def chunk(tag, data):
        out = struct.pack(">I", len(data)) + tag + data
        out += struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
        return out

    ihdr = struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", ihdr)
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")

    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(png)


def main():
    for folder, size in DENSITIES.items():
        pixels = draw(size)
        write_png(os.path.join(RES, folder, "ic_launcher.png"), size, pixels)
        write_png(os.path.join(RES, folder, "ic_launcher_round.png"), size, pixels, circular=True)
        print("wrote %s (%dx%d)" % (folder, size, size))


if __name__ == "__main__":
    main()
