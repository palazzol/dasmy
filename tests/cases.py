"""Test case definitions for compare.py.

A case is a dict: name, files {filename: bytes}, args [command line].
All images are synthetic (opcode sweeps and seeded random data).
"""

import os
import random

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXAMPLES = os.path.join(ROOT, 'original', 'examples')

M6800_CPUS = ['6800', '6801', '6301']
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


def random_image(seed, size):
    rng = random.Random(seed)
    return bytes(rng.randrange(256) for _ in range(size))


def m6800_cases():
    out = []
    for cpu in M6800_CPUS:
        # Opcode sweeps: listing, wide listing and assembler output
        files = {'sweep.bin': sweep_image(), 'sweep.sym': sweep_sym(cpu, 0xC000)}
        out.append(case(f'{cpu}-sweep', files, ['sweep.bin']))
        out.append(case(f'{cpu}-sweep-wide', files, ['-w', 'sweep.bin']))
        out.append(case(f'{cpu}-sweep-asm', files, ['-a', 'sweep.bin']))
        # Operands that point into the image, so labels get generated
        files = {'sweep.bin': sweep_image(operands=(0xC0, 0x10)), 'sweep.sym': sweep_sym(cpu, 0xC000)}
        out.append(case(f'{cpu}-sweep-labels', files, ['sweep.bin']))
        # Random images, no threading
        for seed in range(3):
            files = {'rand.bin': random_image(seed, 2048)}
            out.append(case(f'{cpu}-random{seed}', files, [f'-c{cpu}', '-o0xF800', 'rand.bin']))
        out.append(case(f'{cpu}-random-wide', {'rand.bin': random_image(7, 2048)},
                        [f'-c{cpu}', '-w', '-o0xF800', 'rand.bin']))

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
    return m6800_cases() + option_cases() + symfile_cases()
