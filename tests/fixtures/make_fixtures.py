#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Regenerate the test fixtures from the Python reference implementations.

  python3 tests/fixtures/make_fixtures.py

The inputs here must match the ones in tests/test_sfo.c.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools"))
import sfo  # noqa: E402


def main():
    file_list = sfo.file_list_entry("STATE.BIN")
    entries = sfo.savedata_entries(
        title="Need for Speed™ Most Wanted 5-1-0",
        savedata_title="Save State 2",
        detail="Slot 2\n2026-10-04 13:22",
        directory="PWULUS1003602",
        file_list=file_list + b"\0" * (3168 - len(file_list)),
        parental_level=1,
    )
    with open(os.path.join(HERE, "savedata_basic.sfo"), "wb") as f:
        f.write(sfo.build(entries))


if __name__ == "__main__":
    main()
