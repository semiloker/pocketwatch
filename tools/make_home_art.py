#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Pocketwatch Home's own XMB art: ICON0.PNG (144x80) and PIC1.PNG (480x272), drawn here so the
project ships no third-party pictures. Plain Python (zlib), 4x supersampled.

    python3 tools/make_home_art.py home/res
"""
import math
import os
import struct
import sys
import zlib

SLATE_TOP = (26, 29, 42)
SLATE_BOT = (8, 9, 14)
ACCENT = (0, 114, 206)


def png(path, w, h, rgba):
    raw = b''.join(b'\0' + bytes(rgba[y * w * 4:(y + 1) * w * 4]) for y in range(h))

    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data))

    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)))
        f.write(chunk(b'IDAT', zlib.compress(raw, 9)))
        f.write(chunk(b'IEND', b''))


def mix(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def render(w, h, shade, ss=4):
    """shade(x, y) -> (r, g, b, a) in [0, 255] at sub-pixel coordinates (pixels)."""
    out = bytearray(w * h * 4)
    for y in range(h):
        for x in range(w):
            acc = [0, 0, 0, 0]
            for sy in range(ss):
                for sx in range(ss):
                    c = shade(x + (sx + 0.5) / ss, y + (sy + 0.5) / ss)
                    for i in range(4):
                        acc[i] += c[i]
            n = ss * ss
            out[(y * w + x) * 4:(y * w + x) * 4 + 4] = bytes(int(v / n) for v in acc)
    return out


def background(x, y, w, h):
    base = mix(SLATE_TOP, SLATE_BOT, y / h)
    d = math.hypot(x - w * 0.78, y + h * 0.15) / (w * 0.55)
    glow = max(0.0, 1.0 - d) ** 2 * 0.35
    return mix(base, ACCENT, glow)


def watch(x, y, cx, cy, r):
    """1 inside the stopwatch's white strokes: ring, crown, two hands."""
    d = math.hypot(x - cx, y - cy)
    t = r * 0.18
    if abs(d - r) < t / 2:
        return 1
    if abs(x - cx) < r * 0.2 and cy - r * 1.42 < y < cy - r * 1.12:  # crown
        return 1

    def seg(ax, ay, bx, by):
        px, py = x - ax, y - ay
        vx, vy = bx - ax, by - ay
        u = max(0.0, min(1.0, (px * vx + py * vy) / (vx * vx + vy * vy)))
        return math.hypot(px - u * vx, py - u * vy) < t / 2

    return seg(cx, cy, cx, cy - r * 0.62) or seg(cx, cy, cx + r * 0.45, cy + r * 0.25)


def icon(x, y):
    w, h = 144, 80
    c = background(x, y, w, h)
    if watch(x, y, 72, 44, 23):
        c = (255, 255, 255)
    return c + (255,)


def pic(x, y):
    return background(x, y, 480, 272) + (255,)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else 'home/res'
    os.makedirs(out, exist_ok=True)
    png(os.path.join(out, 'ICON0.PNG'), 144, 80, render(144, 80, icon))
    png(os.path.join(out, 'PIC1.PNG'), 480, 272, render(480, 272, pic, ss=1))


if __name__ == '__main__':
    main()
