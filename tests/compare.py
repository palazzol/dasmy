#!/usr/bin/env python3
"""Run DASMx (under Wine) and Dasmy on the same inputs and compare the results.

Usage: python3 tests/compare.py [-k PATTERN] [-v] [--keep] [--dasmy PATH]

Each test case runs both programs in a fresh scratch directory and compares
the exit code, stdout, stderr and the generated .lst/.asm file.  The banner
and header lines that name the program, and the date line, are masked.

--dasmy selects the Dasmy binary to test (default dasmy/build/dasmy).  A
Windows build (a path ending in .exe) also runs under Wine, and then line
endings must match exactly too, since both programs should write CRLF.
"""

import argparse
import difflib
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
DASMX = os.path.join(ROOT, 'original', 'Dasmx.exe')
DASMY = os.path.join(ROOT, 'dasmy', 'build', 'dasmy')
CACHE = os.path.join(HERE, '.cache')

sys.path.insert(0, HERE)
import cases  # noqa: E402

# Lines that legitimately differ between the two programs
MASKS = [
    (re.compile(r'DASMx object code disassembler|Dasmy object code disassembler'), 'TITLE'),
    (re.compile(r'\(c\) Copyright 1996-2003   Conquest Consultants|Reimplementation of DASMx 1\.40'), 'CREDIT'),
    (re.compile(r'Version \d+\.\d\d \([A-Z][a-z]{2} \d\d \d{4}\)'), 'VERSION'),
    (re.compile(r'(;\tDate:\t\t).*'), r'\1DATE'),
    (re.compile(r'^Usage: dasm[xy]'), 'Usage: dasmx'),
]
WINE_NOISE = re.compile(r'MESA|pci id for fd')


def normalise(text, keep_cr=False):
    if not keep_cr:
        text = text.replace('\r\n', '\n')
    out = []
    for line in text.split('\n'):
        for pat, rep in MASKS:
            line = pat.sub(rep, line)
        out.append(line)
    return '\n'.join(out)


def is_windows_build():
    return DASMY.lower().endswith('.exe')


def run(prog, args, cwd):
    if prog == 'dasmx':
        cmd = ['wine', os.path.join(cwd, 'Dasmx.exe')] + args
        env = dict(os.environ, WINEDEBUG='-all')
    elif is_windows_build():
        cmd = ['wine', DASMY] + args
        env = dict(os.environ, WINEDEBUG='-all')
    else:
        cmd = [DASMY] + args
        env = os.environ
    p = subprocess.run(cmd, cwd=cwd, env=env, capture_output=True, timeout=120)
    return p.returncode, p.stdout.decode('latin-1'), p.stderr.decode('latin-1')


def strip_wine_noise(text):
    return '\n'.join(l for l in text.replace('\r\n', '\n').split('\n') if not WINE_NOISE.search(l))


def cache_key(case):
    h = hashlib.sha256(json.dumps(case['args']).encode())
    for name in sorted(case['files']):
        h.update(name.encode() + b'\0' + case['files'][name])
    return h.hexdigest()


def run_case(case, keep=False):
    """Return a list of (what, diff) for every mismatch."""
    results = {}
    dirs = []
    cache_file = os.path.join(CACHE, cache_key(case) + '.json')
    if os.path.exists(cache_file):
        with open(cache_file) as f:
            results['dasmx'] = tuple(json.load(f))
    for prog in ('dasmx', 'dasmy'):
        if prog in results:
            continue
        d = tempfile.mkdtemp(prefix=f'{case["name"]}-{prog}-')
        dirs.append(d)
        for name, data in case['files'].items():
            with open(os.path.join(d, name), 'wb') as f:
                f.write(data)
        if prog == 'dasmx':
            shutil.copy(DASMX, d)
        code, out, err = run(prog, case['args'], d)
        outputs = {}
        for name in sorted(os.listdir(d)):
            if name.endswith(('.lst', '.asm')):
                with open(os.path.join(d, name), 'rb') as f:
                    outputs[name.lower()] = f.read().decode('latin-1')
        results[prog] = (code, out, err, outputs)
        if prog == 'dasmx':
            os.makedirs(CACHE, exist_ok=True)
            with open(cache_file, 'w') as f:
                json.dump(results[prog], f)

    problems = []
    x, y = results['dasmx'], results['dasmy']
    x = (x[0], x[1], strip_wine_noise(x[2]), x[3])
    if is_windows_build():
        y = (y[0], y[1], strip_wine_noise(y[2]), y[3])
    strict = is_windows_build()
    if x[0] != y[0]:
        problems.append(('exit code', f'dasmx={x[0]} dasmy={y[0]}'))
    for what, a, b, keep_cr in (('stdout', x[1], y[1], strict), ('stderr', x[2], y[2], False)):
        if normalise(a, keep_cr).strip() != normalise(b, keep_cr).strip():
            problems.append((what, diff(a, b)))
    if set(x[3]) != set(y[3]):
        problems.append(('output files', f'dasmx={sorted(x[3])} dasmy={sorted(y[3])}'))
    for name in x[3]:
        if name in y[3] and normalise(x[3][name], strict) != normalise(y[3][name], strict):
            problems.append((name, diff(x[3][name], y[3][name])))

    if not keep:
        for d in dirs:
            shutil.rmtree(d, ignore_errors=True)
    elif problems:
        problems.append(('dirs', '\n'.join(dirs)))
    return problems


def diff(a, b, limit=40):
    if normalise(a) == normalise(b):
        return '(only the line endings differ)'
    lines = list(difflib.unified_diff(normalise(a).split('\n'), normalise(b).split('\n'),
                                      'dasmx', 'dasmy', lineterm='', n=2))
    if len(lines) > limit:
        lines = lines[:limit] + [f'... ({len(lines) - limit} more diff lines)']
    return '\n'.join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('-k', help='only run cases whose name contains this')
    ap.add_argument('-v', action='store_true', help='show diffs')
    ap.add_argument('--keep', action='store_true', help='keep scratch dirs of failing cases')
    ap.add_argument('--dasmy', help='Dasmy binary to test; a .exe runs under Wine')
    opts = ap.parse_args()

    global DASMY
    if opts.dasmy:
        DASMY = os.path.abspath(opts.dasmy)

    if not os.path.exists(DASMY):
        sys.exit(f'build Dasmy first: {DASMY} not found')
    all_cases = [c for c in cases.all_cases() if not opts.k or opts.k in c['name']]
    failed = 0
    for case in all_cases:
        problems = run_case(case, opts.keep)
        status = 'ok' if not problems else 'FAIL'
        print(f'{status:4}  {case["name"]}', flush=True)
        if problems:
            failed += 1
            for what, d in problems:
                print(f'      {what}:')
                if opts.v:
                    print('\n'.join('        ' + l for l in d.split('\n')))
    print(f'\n{len(all_cases) - failed}/{len(all_cases)} passed')
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
