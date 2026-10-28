"""Test case definitions for compare.py.

A case is a dict: name, files {filename: bytes}, args [command line].
All images are synthetic (opcode sweeps and seeded random data).
"""

import os
import random
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXAMPLES = os.path.join(ROOT, 'original', 'examples')

M6800_CPUS = ['6800', '6801', '6301']
M6502_CPUS = ['6502', '65C00', '65C02']
M6805_CPUS = ['6805', '68HC05', '63L05', '6305']
# Opcode table used by each tested CPU type
TABLES = {'6800': 'tab_6800', '6801': 'tab_6801', '6301': 'tab_6301',
          '6502': 'tab_6502', '65C00': 'tab_65c00', '65C02': 'tab_65c02',
          '6805': 'tab_6805', '68HC05': 'tab_68hc05', '63L05': 'tab_63l05', '6305': 'tab_6305'}
NUMFORMATS = 'ACDIMRS'


def example(name):
    with open(os.path.join(EXAMPLES, name), 'rb') as f:
        return f.read()


def case(name, files, args):
    return {'name': name, 'files': files, 'args': args}


def sweep_image(slot=4, operands=(0x12, 0x34), base_op=0):
    """Every opcode at the start of its own slot; the last byte of each slot is skipped."""
    data = bytearray()
    for op in range(256):
        b = [(op + base_op) & 0xFF] + list(operands)
        data += bytes((b + [0] * slot)[:slot])
    return bytes(data)


def sweep_sym(cpu, origin, slot=4, numformat=None):
    lines = [f'cpu {cpu}', f'org {origin:#x}']
    if numformat:
        lines.append(f'numformat {numformat}')
    for op in range(256):
        lines.append(f'skip {origin + op * slot + slot - 1:#x} 1')
    return ('\n'.join(lines) + '\n').encode()


TABLE_DIR = os.path.join(ROOT, 'dasmy', 'src', 'tables')
_ENTRY = re.compile(r'/\* ([0-9A-F]{2}) \*/ \{ ("[^"]*"|NULL), ("[^"]*"|NULL), 0x([0-9A-F]+), (\d+), ')


def valid_opcodes(table):
    """(opcode, length) for every valid opcode in a generated table, e.g. 'tab_6805'."""
    for name in os.listdir(TABLE_DIR):
        text = open(os.path.join(TABLE_DIR, name)).read()
        start = text.find(f'const OpEntry {table}[')
        if start >= 0:
            body = text[start:text.index('};', start)]
            return [(int(m[1], 16), int(m[5])) for m in _ENTRY.finditer(body) if int(m[4], 16) != 0]
    raise KeyError(table)


def program_image(seed, size, table):
    """Random stream of valid instructions (random operands), so threads run far."""
    rng = random.Random(seed)
    ops = valid_opcodes(table)
    data = bytearray()
    while len(data) < size:
        op, length = rng.choice(ops)
        data += bytes([op] + [rng.randrange(256) for _ in range(length - 1)])
    return bytes(data[:size])


def random_image(seed, size):
    rng = random.Random(seed)
    return bytes(rng.randrange(256) for _ in range(size))


def family_cases(cpus, operand_sets=((0x12, 0x34), (0xC0, 0x10))):
    """Opcode sweeps and random images for each CPU of a family."""
    out = []
    for cpu in cpus:
        # Opcode sweeps: listing, wide listing and assembler output
        files = {'sweep.bin': sweep_image(operands=operand_sets[0]), 'sweep.sym': sweep_sym(cpu, 0xC000)}
        out.append(case(f'{cpu}-sweep', files, ['sweep.bin']))
        out.append(case(f'{cpu}-sweep-wide', files, ['-w', 'sweep.bin']))
        out.append(case(f'{cpu}-sweep-asm', files, ['-a', 'sweep.bin']))
        # Other operand values, e.g. pointing into the image so labels get generated
        for i, ops in enumerate(operand_sets[1:]):
            files = {'sweep.bin': sweep_image(operands=ops), 'sweep.sym': sweep_sym(cpu, 0xC000)}
            out.append(case(f'{cpu}-sweep-labels{i or ""}', files, ['sweep.bin']))
        # Random images, no threading
        for seed in range(3):
            files = {'rand.bin': random_image(seed, 2048)}
            out.append(case(f'{cpu}-random{seed}', files, [f'-c{cpu}', '-o0xF800', 'rand.bin']))
        out.append(case(f'{cpu}-random-wide', {'rand.bin': random_image(7, 2048)},
                        [f'-c{cpu}', '-w', '-o0xF800', 'rand.bin']))
    return out


def vector_thread_cases(cpu, vectors, big_endian, size=0x2000, seeds=3, origin=None, tag='', table=None):
    """Threading from the reset/interrupt vectors at the top of a random image."""
    out = []
    if origin is None:
        origin = 0x10000 - size
    sym = [f'cpu {cpu}'] + [f'vector {v:#x} vec{v:04X} ent{v:04X}' for v in vectors]
    sym = ('\n'.join(sym) + '\n').encode()
    for seed in range(seeds):
        if table:
            img = bytearray(program_image(300 + seed, size, table))
        else:
            img = bytearray(random_image(200 + seed, size))
        rng = random.Random(seed)
        for v in vectors:
            target = origin + rng.randrange(size - 0x100)
            hi, lo = target >> 8, target & 0xFF
            img[v - origin], img[v - origin + 1] = (hi, lo) if big_endian else (lo, hi)
        files = {'vec.bin': bytes(img), 'vec.sym': sym}
        out.append(case(f'{cpu}-thread{tag}{seed}', files, ['-t', f'-o{origin:#x}', 'vec.bin']))
        out.append(case(f'{cpu}-thread{tag}-wide{seed}', files, ['-t', '-w', f'-o{origin:#x}', 'vec.bin']))
    return out


def m6502_cases():
    out = family_cases(M6502_CPUS, ((0x12, 0x34), (0xC0, 0x10), (0xFF, 0xC1)))
    for cpu in M6502_CPUS:
        out += vector_thread_cases(cpu, (0xFFFA, 0xFFFC, 0xFFFE), big_endian=False)
        out += vector_thread_cases(cpu, (0xFFFA, 0xFFFC, 0xFFFE), big_endian=False, tag='-prog',
                                   table=TABLES[cpu])
    return out


def m6805_cases():
    out = family_cases(M6805_CPUS, ((0x12, 0x34), (0xC0, 0x10), (0x03, 0xF0)))
    for cpu in M6805_CPUS:
        # Vectors at the top of the (small) address space; origin 0 so the image covers them
        out += vector_thread_cases(cpu, (0xFFF6, 0xFFF8, 0xFFFA, 0xFFFC, 0xFFFE), big_endian=True)
        # Relative branches wrap at 11 bits, so also thread a 2K image at address 0
        out += vector_thread_cases(cpu, (0x7F6, 0x7F8, 0x7FA, 0x7FC, 0x7FE), big_endian=True,
                                   size=0x800, origin=0, tag='-low')
        out += vector_thread_cases(cpu, (0x7F6, 0x7F8, 0x7FA, 0x7FC, 0x7FE), big_endian=True,
                                   size=0x800, origin=0, tag='-prog', table=TABLES[cpu])
    return out


def m6800_cases():
    out = family_cases(M6800_CPUS)
    for cpu in M6800_CPUS:
        out += vector_thread_cases(cpu, (0xFFF8, 0xFFFA, 0xFFFC, 0xFFFE), big_endian=True, tag='-prog',
                                   table=TABLES[cpu])

    # Number formats
    for nf in NUMFORMATS:
        files = {'sweep.bin': sweep_image(operands=(0xC0, 0x10)), 'sweep.sym': sweep_sym('6301', 0xC000, numformat=nf)}
        out.append(case(f'numformat-{nf}', files, ['sweep.bin']))

    # Threading with the Eight Ball Champ symbol file on a random 16K image
    sym = example('ebcgame.sym')
    for seed in range(3):
        img = bytearray(random_image(100 + seed, 0x4000))
        # Make the vectors point into the image
        rng = random.Random(seed)
        for v in range(0x3FF0, 0x4000, 2):
            target = 0xC000 + rng.randrange(0x3000)
            img[v], img[v + 1] = target >> 8, target & 0xFF
        files = {'ebc.bin': bytes(img), 'ebc.sym': sym}
        out.append(case(f'ebcgame-thread{seed}', files, ['-t', '-o0xC000', 'ebc.bin']))
        out.append(case(f'ebcgame-nothread{seed}', files, ['-o0xC000', 'ebc.bin']))
        out.append(case(f'ebcgame-thread-asm{seed}', files, ['-t', '-a', '-o0xC000', 'ebc.bin']))
    return out


def option_cases():
    img = random_image(42, 256)
    return [
        case('opt-list', {}, ['-l']),
        case('opt-version', {}, ['-v']),
        case('opt-usage', {}, []),
        case('opt-bad-option', {'a.bin': img}, ['-q', 'a.bin']),
        case('opt-unknown-cpu', {'a.bin': img}, ['-cZ81', 'a.bin']),
        case('opt-no-cpu', {'a.bin': img}, ['a.bin']),
        case('opt-missing-file', {}, ['-c6800', 'nothere.bin']),
        case('opt-implicit-ext', {'a.bin': img}, ['-c6800', 'a']),
        case('opt-two-files', {'a.bin': img}, ['-c6800', 'a.bin', 'a.bin']),
        case('opt-bad-origin', {'a.bin': img}, ['-c6800', '-ozz', 'a.bin']),
        case('opt-thread-no-entry', {'a.bin': img}, ['-c6800', '-t', 'a.bin']),
        case('opt-thread-entry', {'a.bin': img}, ['-c6800', '-t', '-e0x10', 'a.bin']),
        case('opt-thread-entry-outside', {'a.bin': img}, ['-c6800', '-t', '-e0x1000', 'a.bin']),
        case('opt-cpu-case', {'a.bin': img}, ['-c6301', '-oFF00', 'a.bin']),
    ]


def symfile_cases():
    img = random_image(5, 512)
    def sym(text):
        return {'s.bin': img, 's.sym': text.encode()}
    return [
        case('sym-basic', sym('cpu 6800\norg 0x1000\nsymbol 0x1010 foo\nbyte 0x1020 tbl 5\n'
                              'word 0x1030 w 3\ndword 0x1040 d 2\nstring 0x1050 msg 12\n'
                              'code 0x1000 start\nvector 0x11FE resv reset\n'), ['s.bin']),
        case('sym-thread', sym('cpu 6800\norg 0x1000\ncode 0x1000 start 4\nvectab 0x1100 jt 4\n'
                               'addrtab 0x1110 at 3\nvector 0x11FE resv reset\n'), ['-t', 's.bin']),
        case('sym-message', sym('cpu 6800\nmessage hello   world\nmessage "quoted string" x\n'), ['s.bin']),
        case('sym-comment-cmd', sym('cpu 6800\ncomment 0x10 hi\n'), ['s.bin']),
        case('sym-invalid-cmd', sym('cpu 6800\nfrobnicate 1\n'), ['s.bin']),
        case('sym-bad-args', sym('cpu 6800\nbyte 0x10\n'), ['s.bin']),
        case('sym-bad-value', sym('cpu 6800\nsymbol zz foo\n'), ['s.bin']),
        case('sym-cpu-override', sym('cpu 6801\n'), ['-c6800', 's.bin']),
        case('sym-org-override', sym('cpu 6800\norg 0x2000\n'), ['-o0x1000', 's.bin']),
        case('sym-numformat-bad', sym('cpu 6800\nnumformat Q\n'), ['s.bin']),
        case('sym-numformat-long', sym('cpu 6800\nnumformat MM\n'), ['s.bin']),
        case('sym-skip', sym('cpu 6800\norg 0x1000\nskip 0x1010 0x20\nskip 0x1100 0x80\n'), ['s.bin']),
        case('sym-include', {'s.bin': img, 's.sym': b'include inc.sym\nsymbol 0x20 bar\n',
                             'inc.sym': b'cpu 6800\nmessage included\nsymbol 0x10 foo\n'}, ['s.bin']),
        case('sym-include-missing', sym('cpu 6800\ninclude nothere.sym\n'), ['s.bin']),
        case('sym-case', sym('CPU 6800\nORG 0x1000\nSymbol 0x1010 foo\n'), ['s.bin']),
        case('sym-many-warnings', sym('cpu 6800\n' + 'cpu 6801\n' * 12), ['s.bin']),
        case('sym-dup-labels', sym('cpu 6800\nsymbol 0x10 a\nsymbol 0x10 b\nbyte 0x10 c\ncode 0x10 d\n'), ['s.bin']),
    ]


def all_cases():
    return m6800_cases() + m6502_cases() + m6805_cases() + option_cases() + symfile_cases()
