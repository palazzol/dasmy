"""Decode an MSVC two-level switch table (byte index table + jump table).

Usage: python3 -I re/scripts/switch.py original/Dasmx.exe INDEX_VA TARGETS_VA COUNT [BASE]

COUNT is the number of cases (the `cmp ecx,N` bound + 1) and BASE the value
subtracted before the lookup (the negated `add ecx,-BASE`), default 0.
Prints each target address with the case values that reach it.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from pe import PE


def main():
    pe = PE(sys.argv[1])
    idx, tgt, count = (int(a, 0) for a in sys.argv[2:5])
    base = int(sys.argv[5], 0) if len(sys.argv) > 5 else 0
    index = pe.bytes(idx, count)
    targets = [pe.u32(tgt + 4 * i) for i in range(max(index) + 1)]
    cases = {}
    for i, b in enumerate(index):
        cases.setdefault(targets[b], []).append(i + base)
    for t, vals in sorted(cases.items(), key=lambda kv: kv[1][0]):
        print(f'{t:#x}: {", ".join(f"{v:#x}" for v in vals) if len(vals) < 12 else f"{len(vals)} values (default)"}')


if __name__ == '__main__':
    main()
