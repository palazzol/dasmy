"""Minimal PE32 reader for poking at Dasmx.exe data by virtual address.

    from pe import PE
    pe = PE('original/Dasmx.exe')
    pe.u32(0x43d058), pe.cstr(0x4130c0), pe.bytes(va, n)
"""

import struct
import sys


class PE:
    def __init__(self, path):
        self.data = open(path, 'rb').read()
        d = self.data
        pe_off = struct.unpack_from('<I', d, 0x3c)[0]
        nsect = struct.unpack_from('<H', d, pe_off + 6)[0]
        opt_size = struct.unpack_from('<H', d, pe_off + 20)[0]
        self.image_base = struct.unpack_from('<I', d, pe_off + 24 + 28)[0]
        self.sections = []
        sect_off = pe_off + 24 + opt_size
        for i in range(nsect):
            name, vsize, va, rawsize, rawptr = struct.unpack_from('<8sIIII', d, sect_off + 40 * i)
            self.sections.append((name.rstrip(b'\0').decode(), self.image_base + va, vsize, rawptr, rawsize))

    def off(self, va):
        for name, sva, vsize, rawptr, rawsize in self.sections:
            if sva <= va < sva + max(vsize, rawsize):
                rel = va - sva
                # Uninitialised (bss) part of a section reads as zero
                return rawptr + rel if rel < rawsize else None
        raise ValueError(f'VA {va:#x} not in any section')

    def bytes(self, va, n):
        o = self.off(va)
        return bytes(n) if o is None else self.data[o:o + n]

    def u8(self, va):
        return self.bytes(va, 1)[0]

    def u32(self, va):
        return struct.unpack('<I', self.bytes(va, 4))[0]

    def i32(self, va):
        return struct.unpack('<i', self.bytes(va, 4))[0]

    def cstr(self, va):
        if va == 0:
            return None
        out = bytearray()
        while (b := self.u8(va)) != 0:
            out.append(b)
            va += 1
        return out.decode('latin-1')


if __name__ == '__main__':
    pe = PE(sys.argv[1])
    for s in pe.sections:
        print(f'{s[0]:8s} va={s[1]:#x} vsize={s[2]:#x} raw={s[3]:#x} rawsize={s[4]:#x}')
