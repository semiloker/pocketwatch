#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Writes RLZ test streams for tests/test_gfx.c (plugin/src/gfx/rlz.c), from our own data.

  python3 tests/fixtures/make_rlz.py tests/fixtures

The encoder is the exact mirror of the decoder: same probability tables and contexts as
RCOMage's rlzpack.c (LGPL-2.1), the PSP's update rule, an LZMA-style range coder with carry. Each
fixture is <name>.rlz (compressed) + <name>.raw (expected output).
"""
import os
import random
import sys

P_LIT = 0
P_DIST_LONG = P_LIT + 8 * 0xFF
P_DIST_SHORT = P_DIST_LONG + 0xF0
P_DISTANCES = P_DIST_SHORT + 0x40
P_LEN_DESC = P_DISTANCES + 32 * 0x20
P_MATCH_LENS = P_LEN_DESC + 0x40
P_TOTAL = P_MATCH_LENS + 0xFF


class Encoder:
    def __init__(self, mode):
        self.mode = mode
        self.p = [((4 - ((mode >> 3) & 3)) << 5) & 0xFF] * P_TOTAL
        self.low = 0
        self.range = 0xFFFFFFFF
        self.cache = 0
        self.cache_size = 1
        self.out = bytearray()

    def shift_low(self):
        if self.low < 0xFF000000 or self.low > 0xFFFFFFFF:
            carry = self.low >> 32
            temp = self.cache
            while True:
                self.out.append((temp + carry) & 0xFF)
                temp = 0xFF
                self.cache_size -= 1
                if self.cache_size == 0:
                    break
            self.cache = (self.low >> 24) & 0xFF
        self.cache_size += 1
        self.low = (self.low & 0x00FFFFFF) << 8

    def normalize(self):
        if (self.range >> 24) == 0:
            self.range = (self.range << 8) & 0xFFFFFFFF
            self.shift_low()

    def bit(self, i, b):
        prob = self.p[i]
        bound = (self.range >> 8) * prob
        prob -= prob >> 3
        if b:
            self.range = bound
            prob += 31
        else:
            self.low += bound
            self.range -= bound
        self.p[i] = prob & 0xFF
        self.normalize()

    def number(self, i, v, k):
        if k >= 3:
            self.bit(i, (v >> k) & 1)
            k -= 1
            if k >= 3:
                self.bit(i, (v >> k) & 1)
                k -= 1
            i += 1
            if k >= 3:
                for j in range(k - 3, -1, -1):
                    self.range >>= 1
                    if not (v >> (3 + j)) & 1:
                        self.low += self.range
                k = 2
                self.normalize()
        while k >= 0:
            self.bit(i, (v >> k) & 1)
            i += 1
            k -= 1

    def finish(self):
        for _ in range(5):
            self.shift_low()
        self.out[0] = self.mode
        return bytes(self.out)


def encode(data, mode, tokens):
    """tokens: ('L',) or ('M', length, distance), consuming data in order."""
    e = Encoder(mode)
    real = mode & 7
    pos = 0
    var_c = 0
    last = 0

    def put_len(v):
        n = 0
        while n < 8:
            flag = 1 if v >= (1 << n) else 0
            e.bit(P_LEN_DESC + n * 8 + var_c, flag)
            if not flag:
                break
            n += 1
        hi = n - 1
        if hi > 0:
            k = hi - 1
            e.number(P_MATCH_LENS + ((var_c & 7) + k * 8) * 4 + ((pos << k) & 3), v, k)
        return hi

    for t in tokens:
        if t[0] == 'L':
            put_len(0)
            b = data[pos]
            ctx = ((((pos & 7) << 8) | last) >> real) & 7
            node = 1
            for s in range(7, -1, -1):
                bb = (b >> s) & 1
                e.bit(P_LIT + ctx * 0xFF + node - 1, bb)
                node = node * 2 + bb
            pos += 1
            last = b
            if var_c > 0:
                var_c -= 1
        else:
            _, length, dist = t
            hi = put_len(length - 1)
            ret = hi
            sig = dist.bit_length() - 1
            bits, tree, val = (3, P_DIST_SHORT, sig) if length < 4 else (5, P_DIST_LONG, sig + 12)
            node = 1
            for s in range(bits - 1, -1, -1):
                bb = (val >> s) & 1
                e.bit(tree + node * 8 + ret, bb)
                node = node * 2 + bb
            if sig > 0:
                e.number(P_DISTANCES + (sig - 1) * 0x20, dist, sig - 1)
            pos += length
            last = data[pos - 1]
            var_c = 7 - (pos & 1)
    put_len(0xFF)
    assert pos == len(data)
    return e.finish()


def parse(data, max_dist):
    """Greedy LZ77: the longest match within max_dist, else a literal."""
    tokens = []
    pos = 0
    while pos < len(data):
        best_len, best_dist = 0, 0
        for dist in range(1, min(pos, max_dist) + 1):
            n = 0
            while n < 255 and pos + n < len(data) and data[pos + n] == data[pos + n - dist]:
                n += 1
            if n > best_len and (n >= 4 or (n >= 2 and dist < 256)):
                best_len, best_dist = n, dist
            if best_len == 255:
                break
        if best_len >= 2:
            tokens.append(('M', best_len, best_dist))
            pos += best_len
        else:
            tokens.append(('L',))
            pos += 1
    return tokens


def fixtures():
    rnd = random.Random(1234)
    text = b'Pocketwatch - Pause. Save. Play. ' * 12 + bytes(rnd.randrange(256) for _ in range(300))
    runs = b'\x00' * 700 + b'\xff' * 3 + b'AB' * 200 + bytes(range(256))
    far_block = bytes(rnd.randrange(256) for _ in range(400))
    far = far_block + bytes(rnd.randrange(256) for _ in range(70000)) + far_block
    return [
        ('rlz_text', 0x05, text, parse(text, 4096)),
        ('rlz_runs', 0x07, runs, parse(runs, 512)),
        ('rlz_far', 0x05, far, [('L',)] * (len(far) - 400) + [('M', 255, 70400), ('M', 145, 70400)]),
    ]


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(__file__)
    for name, mode, data, tokens in fixtures():
        packed = encode(data, mode, tokens)
        with open(os.path.join(out_dir, name + '.rlz'), 'wb') as f:
            f.write(packed)
        with open(os.path.join(out_dir, name + '.raw'), 'wb') as f:
            f.write(data)
        print(name, len(data), '->', len(packed))


if __name__ == '__main__':
    main()
