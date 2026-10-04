#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Spike S2: does the XMB Saved Data Utility list a save folder without a valid hash?

Creates two test folders in <SAVEDATA dir> (e.g. D:/PSP/SAVEDATA):
  PWTEST00A  SAVEDATA_PARAMS mode 0x00, empty SAVEDATA_FILE_LIST
  PWTEST00B  SAVEDATA_PARAMS mode 0x00, STATE.BIN listed with a zero hash
Both get ICON0.PNG (144x80), PIC1.PNG (480x272) and a 1 KB STATE.BIN.

Then check on the PSP: Game -> Saved Data Utility. Note for each folder whether it is listed,
whether icon/title/detail/background show, and whether it is marked as corrupted.

  s2_savedata_test.py <SAVEDATA dir>            create the folders
  s2_savedata_test.py <SAVEDATA dir> --remove   delete them again
"""
import math
import os
import shutil
import struct
import sys
import zlib

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import sfo  # noqa: E402

FOLDERS = ("PWTEST00A", "PWTEST00B")


def png(width, height, pixel):
    """Encode an RGB PNG; pixel(x, y) -> (r, g, b)."""
    rows = bytearray()
    for y in range(height):
        rows.append(0)  # filter: none
        for x in range(width):
            rows.extend(pixel(x, y))

    def chunk(tag, body):
        return struct.pack(">I", len(body)) + tag + body + struct.pack(">I", zlib.crc32(tag + body))

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(bytes(rows), 9)) + chunk(b"IEND", b"")


def watch_face(width, height, accent):
    """Dark radial gradient with a ring, so the test entries are easy to spot."""
    cx, cy = width / 2, height / 2
    radius = min(width, height) * 0.36

    def pixel(x, y):
        d = math.hypot(x - cx, y - cy)
        base = max(0.0, 1.0 - d / max(width, height))
        r, g, b = (int(20 + 60 * base), int(24 + 70 * base), int(40 + 110 * base))
        if abs(d - radius) < max(1.5, radius * 0.06):
            r, g, b = accent
        return (r, g, b)

    return pixel


def make_folder(root, name, list_state_bin):
    path = os.path.join(root, name)
    os.makedirs(path, exist_ok=True)
    file_list = sfo.file_list_entry("STATE.BIN") if list_state_bin else b""
    variant = "B: STATE.BIN in file list" if list_state_bin else "A: empty file list"
    entries = sfo.savedata_entries(
        title="Pocketwatch test",
        savedata_title="Save State test " + name[-1],
        detail="Spike S2, variant %s.\nSAVEDATA_PARAMS mode 0x00 (no hash)." % variant,
        directory=name,
        file_list=file_list + b"\0" * (3168 - len(file_list)),
    )
    accent = (230, 200, 120) if list_state_bin else (120, 200, 230)
    files = {
        "PARAM.SFO": sfo.build(entries),
        "ICON0.PNG": png(144, 80, watch_face(144, 80, accent)),
        "PIC1.PNG": png(480, 272, watch_face(480, 272, accent)),
        "STATE.BIN": bytes(1024),
    }
    for fname, data in files.items():
        with open(os.path.join(path, fname), "wb") as f:
            f.write(data)
    print("created %s (%s)" % (path, ", ".join("%s %d B" % (k, len(v)) for k, v in files.items())))


def main(argv):
    if len(argv) not in (2, 3) or (len(argv) == 3 and argv[2] != "--remove"):
        sys.exit(__doc__)
    root = argv[1]
    if not os.path.isdir(root):
        sys.exit("not a directory: %s" % root)
    if len(argv) == 3:
        for name in FOLDERS:
            path = os.path.join(root, name)
            if os.path.isdir(path):
                shutil.rmtree(path)
                print("removed %s" % path)
        return
    make_folder(root, FOLDERS[0], list_state_bin=False)
    make_folder(root, FOLDERS[1], list_state_bin=True)


if __name__ == "__main__":
    main(sys.argv)
