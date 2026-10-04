#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Read and write PSP PARAM.SFO files.

  sfo.py dump <PARAM.SFO>     print every key with its format, length and value

As a module: parse(data) -> list of entries, build(entries) -> bytes, savedata_entries(...)
builds the 8-key layout that real PSP saves use (verified against saves on a PSP-3000).
This is also the reference the plugin's C writer is tested against.
"""
import struct
import sys

MAGIC = b"\0PSF"
VERSION = 0x00000101

FMT_UTF8_RAW = 0x0004  # not NUL-terminated (binary blobs)
FMT_UTF8 = 0x0204      # NUL-terminated string
FMT_INT32 = 0x0404


def parse(data):
    """Return [(key, fmt, max_len, value_bytes)] in file order."""
    magic, version, key_start, data_start, count = struct.unpack_from("<4sIIII", data, 0)
    if magic != MAGIC:
        raise ValueError("not a PARAM.SFO")
    entries = []
    for i in range(count):
        key_off, fmt, length, max_len, data_off = struct.unpack_from("<HHIII", data, 20 + i * 16)
        key_end = data.index(b"\0", key_start + key_off)
        key = data[key_start + key_off:key_end].decode("ascii")
        value = data[data_start + data_off:data_start + data_off + length]
        entries.append((key, fmt, max_len, value))
    return entries


def build(entries):
    """entries: [(key, fmt, max_len, value_bytes)]; keys are sorted as the PSP expects."""
    entries = sorted(entries, key=lambda e: e[0])
    keys = b""
    key_offsets = []
    for key, _, _, _ in entries:
        key_offsets.append(len(keys))
        keys += key.encode("ascii") + b"\0"
    key_start = 20 + 16 * len(entries)
    keys += b"\0" * ((-(key_start + len(keys))) % 4)  # data table is 4-byte aligned
    data_start = key_start + len(keys)

    index, blob = b"", b""
    for (key, fmt, max_len, value), key_off in zip(entries, key_offsets):
        if len(value) > max_len:
            raise ValueError("%s: %d bytes > max %d" % (key, len(value), max_len))
        index += struct.pack("<HHIII", key_off, fmt, len(value), max_len, len(blob))
        blob += value + b"\0" * (max_len - len(value))
    header = struct.pack("<4sIIII", MAGIC, VERSION, key_start, data_start, len(entries))
    return header + index + keys + blob


def utf8z(text):
    return text.encode("utf-8") + b"\0"


def savedata_entries(title, savedata_title, detail, directory, params=b"\0" * 128,
                     file_list=b"\0" * 3168, parental_level=1):
    """The 8 keys found in every save on the test PSP, with the same max lengths."""
    return [
        ("CATEGORY", FMT_UTF8, 4, utf8z("MS")),
        ("PARENTAL_LEVEL", FMT_INT32, 4, struct.pack("<I", parental_level)),
        ("SAVEDATA_DETAIL", FMT_UTF8, 1024, utf8z(detail)),
        ("SAVEDATA_DIRECTORY", FMT_UTF8, 64, utf8z(directory)),
        ("SAVEDATA_FILE_LIST", FMT_UTF8_RAW, 3168, file_list),
        ("SAVEDATA_PARAMS", FMT_UTF8_RAW, 128, params),
        ("SAVEDATA_TITLE", FMT_UTF8, 128, utf8z(savedata_title)),
        ("TITLE", FMT_UTF8, 128, utf8z(title)),
    ]


def file_list_entry(name, digest=b"\0" * 16):
    """One SAVEDATA_FILE_LIST record: 13-byte name, 16-byte hash, 3 bytes padding."""
    raw = name.encode("ascii")
    if len(raw) > 12:
        raise ValueError("file name too long for SAVEDATA_FILE_LIST: %s" % name)
    return raw + b"\0" * (13 - len(raw)) + digest + b"\0" * 3


def _show(fmt, value):
    if fmt == FMT_INT32:
        return str(struct.unpack("<I", value)[0])
    if fmt == FMT_UTF8:
        return repr(value.rstrip(b"\0").decode("utf-8", "replace"))
    nonzero = sum(1 for b in value if b)
    return "%d bytes, %d non-zero, first 16: %s" % (len(value), nonzero, value[:16].hex())


def main(argv):
    if len(argv) != 3 or argv[1] != "dump":
        sys.exit(__doc__)
    with open(argv[2], "rb") as f:
        data = f.read()
    print("%s: %d bytes" % (argv[2], len(data)))
    for key, fmt, max_len, value in parse(data):
        print("  %-20s fmt=0x%04X len=%4d max=%4d  %s" % (key, fmt, len(value), max_len, _show(fmt, value)))


if __name__ == "__main__":
    main(sys.argv)
