#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Convert ARK-4's NID resolver table into tools/nids/ark4_660.tsv.

Usage: gen_ark4_table.py <ARK-4>/core/systemctrl/src/nid_660_data.c > ark4_660.tsv

Each row: library, NID as used by PSPSDK stubs (pre-6.x), NID on 6.60/6.61, function name.
"""
import re
import sys

LIB_RE = re.compile(r"^\s*static\s+NidResolverEntry\s+(\w+)_nid\[\]")
ROW_RE = re.compile(r"^\s*\{\s*(0x[0-9A-Fa-f]{8})\s*,\s*(0x[0-9A-Fa-f]{8}|UNKNOWNNID)\s*,?\s*\}\s*,?\s*(?://\s*(\S+))?")


def main(path):
    sys.stdout.reconfigure(newline="\n")  # same bytes on Windows and Linux
    lib = None
    print("# library\told_nid\tnew_nid\tname")
    with open(path, encoding="utf-8", errors="replace") as src:
        for line in src:
            m = LIB_RE.match(line)
            if m:
                lib = m.group(1)
                continue
            m = ROW_RE.match(line)
            if m and lib:
                old = "0x%08X" % int(m.group(1), 16)
                new = m.group(2) if m.group(2) == "UNKNOWNNID" else "0x%08X" % int(m.group(2), 16)
                print("%s\t%s\t%s\t%s" % (lib, old, new, m.group(3) or "-"))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
