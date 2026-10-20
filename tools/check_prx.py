#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Static checks for the built plugin: import NIDs and size budgets.

Imports: on firmware 6.xx most kernel libraries have randomized NIDs. ARK-4 translates the
pre-6.x NIDs that PSPSDK stubs use, but only those listed in its resolver table
(tools/nids/ark4_660.tsv). A library absent from that table was not randomized, so its NIDs
work as-is. An import from a randomized library that is not in the table must be listed in
tools/nids/allowlist.tsv, otherwise the check fails: such a call would hit `syscall 0x15`.

Usage:
  check_prx.py --elf pocketwatch.elf --prx pocketwatch.prx \
      --nid-table tools/nids/ark4_660.tsv --allowlist tools/nids/allowlist.tsv \
      --max-prx-size 65536 --max-mem 131072
"""
import argparse
import os
import struct
import sys

SHT_SYMTAB = 2
SHT_NOBITS = 8
SHF_ALLOC = 0x2


class Elf:
    """Just enough ELF32 little-endian parsing for PSP modules."""

    def __init__(self, path):
        with open(path, "rb") as f:
            self.data = f.read()
        if self.data[:4] != b"\x7fELF" or self.data[4] != 1 or self.data[5] != 1:
            raise ValueError("%s: not a 32-bit little-endian ELF" % path)
        shoff, = struct.unpack_from("<I", self.data, 0x20)
        shentsize, shnum, shstrndx = struct.unpack_from("<HHH", self.data, 0x2E)
        self.sections = []
        for i in range(shnum):
            fields = struct.unpack_from("<10I", self.data, shoff + i * shentsize)
            self.sections.append(dict(zip(
                ("name", "type", "flags", "addr", "offset", "size", "link", "info", "align", "entsize"),
                fields)))
        strtab = self.sections[shstrndx]
        for s in self.sections:
            s["name"] = self._cstr(strtab["offset"] + s["name"])

    def _cstr(self, off):
        end = self.data.index(b"\0", off)
        return self.data[off:end].decode("ascii", "replace")

    def section(self, name):
        return next((s for s in self.sections if s["name"] == name), None)

    def addr_to_offset(self, addr):
        for s in self.sections:
            if s["flags"] & SHF_ALLOC and s["type"] != SHT_NOBITS and s["addr"] <= addr < s["addr"] + s["size"]:
                return s["offset"] + addr - s["addr"]
        raise ValueError("address 0x%08X is not in any loaded section" % addr)

    def u32(self, addr):
        return struct.unpack_from("<I", self.data, self.addr_to_offset(addr))[0]

    def cstr_at(self, addr):
        return self._cstr(self.addr_to_offset(addr))

    def symbols_by_addr(self):
        symtab = next((s for s in self.sections if s["type"] == SHT_SYMTAB), None)
        if symtab is None:
            return {}
        strtab = self.sections[symtab["link"]]
        names = {}
        for off in range(symtab["offset"], symtab["offset"] + symtab["size"], 16):
            st_name, st_value, _size, _info, _other, _shndx = struct.unpack_from("<IIIBBH", self.data, off)
            if st_name:
                names.setdefault(st_value, self._cstr(strtab["offset"] + st_name))
        return names

    def alloc_size(self):
        return sum(s["size"] for s in self.sections if s["flags"] & SHF_ALLOC)


def imports(elf):
    """Yield (library, nid, function name) for every imported function."""
    stub = elf.section(".lib.stub")
    if stub is None:
        return
    names = elf.symbols_by_addr()
    pos, end = stub["addr"], stub["addr"] + stub["size"]
    while pos < end:
        name_ptr = elf.u32(pos)
        len_words = elf.data[elf.addr_to_offset(pos + 8)]
        func_count = struct.unpack_from("<H", elf.data, elf.addr_to_offset(pos + 10))[0]
        nids_ptr, stubs_ptr = elf.u32(pos + 12), elf.u32(pos + 16)
        if len_words == 0:
            break
        lib = elf.cstr_at(name_ptr)
        for i in range(func_count):
            yield lib, elf.u32(nids_ptr + i * 4), names.get(stubs_ptr + i * 8, "?")
        pos += len_words * 4


def load_tsv(path, key_cols):
    rows = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            cols = line.split("\t")
            rows[tuple(cols[:key_cols])] = cols
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--elf", required=True)
    ap.add_argument("--prx", required=True)
    ap.add_argument("--nid-table", required=True)
    ap.add_argument("--allowlist", required=True)
    ap.add_argument("--max-prx-size", type=int, required=True, help="bytes")
    ap.add_argument("--max-mem", type=int, required=True, help="bytes of loaded sections (code+data+bss)")
    args = ap.parse_args()

    elf = Elf(args.elf)
    table = load_tsv(args.nid_table, 2)
    randomized = {lib for lib, _ in table}
    allow = load_tsv(args.allowlist, 2)

    failures = 0
    owner = {}  # a stub's library: two tables claiming one stub is a broken fixup (2026-10-20)
    print("Imports:")
    for lib, nid, name in imports(elf):
        key = (lib, "0x%08X" % nid)
        if name != "?" and owner.setdefault(name, lib) != lib:
            print("FAIL: %s is in the tables of %s and %s (an IMPORT_START of a library PSPSDK"
                  " already has?)" % (name, owner[name], lib))
            failures += 1
        if key in table:
            status = "ark-mapped -> %s" % table[key][2]
        elif lib not in randomized:
            status = "native (library not randomized)"
        elif key in allow:
            status = "allowlist: %s" % allow[key][3]
        else:
            status = "UNVERIFIED: randomized library, NID unknown to ARK-4 and not allowlisted"
            failures += 1
        print("  %-28s 0x%08X  %-30s %s" % (lib, nid, name, status))

    prx_size = os.path.getsize(args.prx)
    mem = elf.alloc_size()
    print("\nSize: PRX %d bytes (budget %d), loaded sections %d bytes (budget %d)"
          % (prx_size, args.max_prx_size, mem, args.max_mem))
    if prx_size > args.max_prx_size:
        print("FAIL: PRX exceeds its size budget")
        failures += 1
    if mem > args.max_mem:
        print("FAIL: loaded sections exceed the kernel memory budget")
        failures += 1

    if failures:
        print("\n%d check(s) failed" % failures)
        return 1
    print("\nAll checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
