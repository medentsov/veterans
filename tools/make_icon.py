#!/usr/bin/env python3
"""Генератор иконки приложения «Ветераны» (чистый stdlib, без зависимостей).

Рисует скруглённый квадрат с белой звездой и сохраняет PNG нужных размеров.
Запуск: python3 tools/make_icon.py
"""

import math
import struct
import zlib
from pathlib import Path

BG = (45, 108, 223)      # #2D6CDF — акцентный синий
FG = (255, 255, 255)     # белая звезда
OUT_DIR = Path(__file__).resolve().parent.parent / "resources" / "icons"


def star_polygon(cx=0.5, cy=0.52, outer=0.29, inner=0.122):
    points = []
    for i in range(10):
        radius = outer if i % 2 == 0 else inner
        angle = math.radians(-90 + i * 36)
        points.append((cx + radius * math.cos(angle),
                       cy + radius * math.sin(angle)))
    return points


STAR = star_polygon()


def in_rounded_rect(x, y, radius=0.22):
    cx = min(max(x, radius), 1.0 - radius)
    cy = min(max(y, radius), 1.0 - radius)
    return (x - cx) ** 2 + (y - cy) ** 2 <= radius ** 2


def in_polygon(x, y, poly):
    inside = False
    j = len(poly) - 1
    for i in range(len(poly)):
        xi, yi = poly[i]
        xj, yj = poly[j]
        if (yi > y) != (yj > y):
            xint = (xj - xi) * (y - yi) / (yj - yi) + xi
            if x < xint:
                inside = not inside
        j = i
    return inside


def render(size, ss):
    grid = size * ss
    rows = []
    for py in range(size):
        row = bytearray()
        for px in range(size):
            hits = 0
            r = g = b = 0
            for sy in range(ss):
                y = (py * ss + sy + 0.5) / grid
                for sx in range(ss):
                    x = (px * ss + sx + 0.5) / grid
                    if not in_rounded_rect(x, y):
                        continue
                    hits += 1
                    if in_polygon(x, y, STAR):
                        r += FG[0]; g += FG[1]; b += FG[2]
                    else:
                        r += BG[0]; g += BG[1]; b += BG[2]
            total = ss * ss
            if hits:
                row += bytes((round(r / hits), round(g / hits), round(b / hits),
                              round(255 * hits / total)))
            else:
                row += b"\x00\x00\x00\x00"
        rows.append(bytes(row))
    return rows


def write_png(path, size, ss):
    rows = render(size, ss)
    raw = b"".join(b"\x00" + row for row in rows)

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw, 9))
           + chunk(b"IEND", b""))
    path.write_bytes(png)
    print(f"{path.name}: {size}x{size}, {len(png)} байт")


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    write_png(OUT_DIR / "veterans-256.png", 256, 4)
    write_png(OUT_DIR / "veterans-64.png", 64, 8)
    write_png(OUT_DIR / "veterans-32.png", 32, 8)


if __name__ == "__main__":
    main()
