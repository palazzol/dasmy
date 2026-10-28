# DASMx 1.40 reverse-engineering notes

Working notes for building Dasmy, a cross-platform C reimplementation of DASMx 1.40
(`original/Dasmx.exe`, Win32 console, MSVC, built Oct 18 2003).

## Status (2026-10-28)

**Done**
- **Core ported** to C99 in `dasmy/src/`: command line, file handling, symbol file (both passes), symbol table (same hash and probe order),
  memory map, threading, passes 1 and 2, listing/asm/wide output, header, symbol table listing, statistics.
- **CPUs ported:**
  - 6800 family (6800, 6801, 6802, 6803, 6808, 6301, 6303): `cpu_6800.c`.
  - 6502 family (6502, 65C00, 65C29, 65C02, 65C102, 65C112): `cpu_6502.c`.
  - 6805 family (6805, 68HC05, 68HC705, 63L05, 6305): `cpu_6805.c`.
  - 6809: `cpu_6809.c`. All the Motorola-style CPUs are now done.
  - 8080, 8085: `cpu_8080.c`.
  - Z80, NSC800, LR35902: `cpu_z80.c`.
  - 8048: `cpu_8048.c`. 8051: `cpu_8051.c`.

  Opcode tables are generated from the binary by `re/scripts/gentables.py` into `dasmy/src/tables/`.
- **Build:** `cmake -S dasmy -B dasmy/build && cmake --build dasmy/build`. It compiles cleanly with `-Wall -Wextra`.
- **Test harness:** `python3 tests/compare.py [-k NAME] [-v] [--keep]` runs Dasmx.exe under Wine and Dasmy on the same synthetic
  inputs (`tests/cases.py`). It compares exit code, stdout, stderr and the .lst/.asm, masking only the banner and the date.
  The original's results are cached in `tests/.cache/`; a full run takes seconds once cached, about 5 minutes cold.
- **Result: 407/408 cases pass.** The only failure is `opt-list`, because the other CPUs aren't in Dasmy yet.

**Not done yet**
- **Remaining CPU modules** (see the CPU table below): PIC16F8x, 2650, 1802/1805. Each needs its tables added to `gentables.py`, its decode/pass1/format/thread functions ported
  (mostly from `core.asm`, since Ghidra didn't create functions for pointer-only entry points), and sweep tests.
  Workflow per family:
  1. Find the table addresses in the decode functions.
  2. Map each case with `re/scripts/switch.py`.
  3. Read each case's stack offsets in `core.asm`.
  4. Add tests with `family_cases()` / `vector_thread_cases()` in `tests/cases.py`, and the CPU's table to `TABLES` there.
     `program_image()` builds images from valid instructions only, so threading reaches much more code than with random bytes.
- **Second address space:** the 8051's separate data space (`addr2_bits`) is now covered by tests. In the original, its only effect is that
  data labels aren't printed in the code listing; data symbols still name operands and appear in the symbol table.
- **Unit size:** 16-bit units (PIC) are untested end to end.
- **SIGINT handler:** check its exact behaviour against 0x401170. Currently it prints "Abnormal termination" and exits 1.
- **README:** Dasmy has no README or usage documentation of its own yet.

**Deliberate differences from DASMx**
- **Banner and header:** these name Dasmy instead of DASMx (`DASMY_*` in `dasmy.h`).
- **Out-of-image reads:** where the original read outside the image buffer (undefined behaviour), Dasmy reads 0.
- **Output file name:** an input file that exists without an extension crashes the original. Dasmy appends `.lst`/`.asm`.
- **Path separators:** both `/` and `\` count as path separators when finding the extension.
- **Statistics:** a statistics run with no unskipped bytes divides by zero in the original. Dasmy prints 0%.

## Tooling

- `re/scripts/ExportDecomp.java`: Ghidra headless export. It writes `re/out/decompiled.c`, `functions.txt` and `strings.txt`.
  ```
  /opt/ghidra/support/analyzeHeadless <projdir> dasmx -import original/Dasmx.exe -overwrite \
      -scriptPath re/scripts -postScript ExportDecomp.java re/out
  ```
- `re/out/core.asm`: `objdump -d -M intel --start-address=0x401000 --stop-address=0x409200 original/Dasmx.exe`.
  Use it where Ghidra lost arguments. Many internal functions pass arguments in registers (`in_EAX`, `unaff_EDI`, `unaff_ESI`),
  and the varargs to `out_tab_printf` (0x402330) were dropped.
- `re/scripts/pe.py`: reads `Dasmx.exe` data by virtual address (`u32`, `cstr`, `bytes`).
- `re/scripts/switch.py`: decodes an MSVC switch (byte index table + jump table) into case value → target address.
- `re/out/` is not in git. Regenerate it with the two commands above if it's missing.
- **Reference runs:** `WINEDEBUG=-all wine Dasmx.exe ...` works. Strip `\r`. Ignore the MESA-EGL noise on stderr.
- **Address ranges:** DASMx's own code is 0x401000–~0x408f80. The C runtime follows from 0x4091a2.

## Top-level flow

| VA | Role |
|---|---|
| 0x4018e0 | `main`: parse_args, banner, (no file → usage), signals, open_files, run, close |
| 0x401360 | parse_args: getopt `"ac:e:lo:tvw"` (custom getopt at 0x401a70) |
| 0x4014e0 | open_files: open input `rb`; if it fails and the name has no extension, append `.bin`. Output is `<base>.lst`/`.asm` (`w`). Symbol file is `<base>.sym` (`r`, optional) |
| 0x4016f0 | run, see below |
| 0x401000 / 0x401060 | `error()`: "ERROR - %s\n" to stderr, then exit(1). `warning()`: "WARNING - %s\n"; the 10th warning is fatal: "maximum number of warnings exceeded". Trailing `\n`s are stripped before printing |
| 0x4010e0 | Banner on stdout: `"\n%s\n"` title, 30 `=`, `"\n%s\n"` copyright, `"Version %d.%02d (%s)\n\n"` 1, 40, "Oct 18 2003" |

**run (0x4016f0):**
1. If there's a .sym file: run sym pass A (`cpu`, `org`, `include` only), then rewind.
2. If no CPU is set: error "must specify CPU type". The docs say the default is 6800, but it isn't.
3. Print `"CPU: %s %s (%s family)\n"` (manufacturer, short CPU name such as "6800", family). Get the file length and print `"Source file length: %d bytes\n"`.
4. Units = length / (data_bits/8). Number format = the CPU default.
5. Origin: the sentinel 0xFFFF means unset, so it becomes 0 ("Origin now defaults to 0" in 1.40). Note that `-o0xFFFF` therefore also gives 0.
6. Load the image. Mark memory type 1 (unknown ROM) if threading, otherwise 2 (code).
7. Allocate the symbol tables. If there's a .sym file: run sym pass B (all other commands).
8. If `-t`: add the `-e` entry if given, print "Threading code...\n", thread.
9. "Pass 1...\n" (collect labels), "Pass 2...\n", header, listing.
10. Symbol table (listing mode only). If threading: statistics (code/data/unknown/skipped bytes).

## Command line

| Option | Effect |
|---|---|
| `-a` | Assembler output (`.asm`) |
| `-cTYPE` | CPU; error if unknown. Matched case-insensitively against `cpu.name` |
| `-eNNNN` | Entry point (`%i`) |
| `-l` | Banner + CPU list + exit(0) |
| `-oNNNN` | Origin (`%i`) |
| `-t` | Threading |
| `-v` | Banner + exit(0) |
| `-w` | Wide listing |

The last positional argument is used only when exactly one is left (`argc - optind == 1`); otherwise the usage text is printed.

## CPU descriptor (0x43d058: NULL-terminated array of pointers to 72-byte structs)

`{ name, part, manufacturer, family, big_endian, unit_bytes?, max_insn_len, addr_min, addr_max, data_bits, ?, addr2_max, addr2_bits, numformat, decode, pass1, format, thread }`

| CPU | decode | pass1 | format | thread | notes |
|---|---|---|---|---|---|
| 6800/6802/6808 | 0x4063e0 | 0x4064d0 | 0x4065d0 | 0x408f90 | big endian, max len 3, 'M' |
| 6801/6803 | 0x406430 | " | " | " | |
| 6301/6303 | 0x406480 | " | " | " | |
| 6809 | 0x405b60 | 0x4060e0 | 0x4061f0 | 0x408e80 | max len 5 |
| 6805 / 68HC05 / 63L05 / 6305 | 0x406850 / 0x4068a0 / 0x4068f0 / 0x406940 | 0x406990 | 0x406ac0 | 0x406de0 | addr_max 0xfff/0x1fff/0xfff/0x3fff |
| 6502 / 65C00,65C29 / 65C02,65C102,65C112 | 0x406f70 / 0x406fc0 / 0x407010 | 0x407060 | 0x407240 | 0x409050 | little endian |
| Z80, NSC800 | 0x408b40 | 0x404510 | 0x4045e0 | 0x408ac0 | max len 4, 'I' |
| LR35902 | 0x404c10 | 0x404ca0 | 0x404d80 | 0x408ac0 | |
| 8080 / 8085 | 0x404fb0 / 0x405000 | 0x405050 | 0x4050c0 | 0x4051d0 | |
| 8048 | 0x405960 | 0x4059a0 | 0x405a10 | 0x408e10 | addr_max 0xfff |
| 8051 | 0x405230 | 0x405280 | 0x4054a0 | 0x408cd0 | second address space 0..0xffff |
| PIC16F83 / F84 | 0x404940 | 0x404970 | 0x4049c0 | 0x404bc0 | 16-bit units, addr_max 0x1ff/0x3ff, 'C' |
| 2650 | 0x4076c0 | 0x407710 | 0x4078a0 | 0x407c70 | addr_max 0x7fff, 'S' |
| 1802 / 1805 | 0x407e60 / 0x407ec0 | 0x407f50 | 0x408050 | 0x408230 | 'R' |

Opcode tables use 20-byte entries `{ mnemonic, operand_fmt, flags, cycles?, ? }`.
- **`flags & 0xff`:** the addressing type.
- **`flags & 0x100`:** ends a thread / sequence break (unconditional jump, return).
- **`flags & 0x200`:** indexed / indirect jump.
- **`flags & 0x1000`:** shows a `+` after the cycle count.
- **Cycles:** low byte = minimum; the next byte is the extra amount added for the maximum (`[min/max]`).

## 6502 family module

- **Tables:** 6502 0x429f68, 65C00/65C29 0x42b368, 65C02/65C102/65C112 0x42c768. Little endian, default number format 'M'.
- **Addressing types:**
  - 1: implied; operand text comes from the table, e.g. `a`.
  - 2: relative. 4: absolute. 5: zp. 6: imm8.
  - 0x18 `(zp,x)`, 0x19 `(zp),y`, 0x1A `zp,x`, 0x1B `zp,y`, 0x1C `abs,x`, 0x1D `abs,y`.
  - 0x1E: `jmp (abs)`.
  - 0x20: `(zp)` (65C02).
  - 0x21: `jmp (abs,x)` (65C02, INFO "index jump").
  - 0x4B: `bbr`/`bbs zp,rel` (target = pc+3+rel).
- **`jmp (abs)`:** INFO "indirect jump". If the pointer's low byte is 0xFF, the format function prints
  "Indirect jump across page boundary at %04X" to stdout *during pass 2*, and the INFO becomes "indirect jump across page boundary".
  - Pass 1 labels the pointer (X) and, if both pointer bytes are in the image, its target (L).
  - Threading follows the target, or prints "Ignoring indirect jump outside ROM via %04X at address %04X".
- **`(zp)`:** pass 1 labels the zp pointer and, if in the image, the word it points to (as data).

## 6805 family module

- **Tables:** 6805 0x424e18, 68HC05/68HC705 0x428a18, 63L05 0x426218, 6305 0x427618. Big endian, 'M'.
  addr_max is 0xFFF / 0x1FFF / 0xFFF / 0x3FFF, but that only affects the `%04X`/`%08X` choice.
- **Relative branch targets are masked with 0x7FF for every variant** (pass 1, format and threading), so branches above 2K are wrong in the original.
- **Addressing types:**
  - 1: inherent. The table text (e.g. `",x"`) is copied; with OPF_CODEREF (`jmp`/`jsr ,x`) it adds INFO "index jump".
  - 2: rel. 4: extended. 6: imm8.
  - 5: direct. The table text, if present, is a printf format such as `"0,%s"` (`bset`/`bclr`).
  - 0x4B: `brset`/`brclr`. Table text `"n,"`, then the direct address, `","`, and the target (pc+3+rel).
  - 0x50 / 0x51: 8-bit / 16-bit `offset,x`. The offset goes through the symbol lookup, so it's an `L` label when OPF_CODEREF.
- **Threading:** indexed jumps print "Ignoring index branch opcode %02X %02X [%02X] at address %04X".

## 6809 module

- **Tables:** page 0 0x41d230, page 0x10 0x41e630, page 0x11 0x41fa30. Indexed post-byte extra length 0x41d130 (int8, −1 = invalid post-byte).
  Big endian, max length 5, 'M'.
- **Decode:**
  - Type 8/9 entries are the 0x10/0x11 prefixes. The next byte is looked up in the page table, and opaddr = pc+2.
  - Type 3 (indexed): length += extra[post-byte]; an extra of −1 makes the instruction invalid.
  - Type 0xA (`tfr`/`exg`): invalid unless both nibbles name a register (0x43dbd8: d x y u s pc - - a b cc dp).
- **Relative branches:** rel8 target = opaddr+1+off, rel16 = opaddr+2+off. Pass 1 labels them only when OPF_CODEREF is set.
- **Indexed operand (0x405db0):**
  - 5-bit offset `"%d,r"`.
  - Otherwise `,r+` `,r++` `,-r` `,--r` `,r` `b,r` `a,r` `%d,r` (8- and 16-bit) `d,r`, or `[addr]` (symbol lookup).
  - Bit 4 wraps the operand in `[...]`.
  - PC-relative (0x8C/0x8D) prints `"%d,pc"` followed by `"\t\t;(" target ")"`.
  - Pass 1 labels only the extended indirect `[addr]` (as data).
- **Register lists (`psh`/`pul`), joined by `", "`:** types 0xC and 0xD use the S names and list from bit 7 down. 0xE uses the U names from bit 7 down;
  **0xF uses the U names from bit 0 up**. This asymmetry is in the original.
- **Threading:** follows rel8, rel16, direct and extended. Indexed jumps print "Ignoring index branch opcode %02X at address %04X".

## 8080/8085, Z80/NSC800 and LR35902 modules

All three use the table's operand text either as is (copied first, always) or as a printf format for the operand.
Little endian, 'I'. Relative targets are opaddr+1+off.

- **8080** 0x417b18, **8085** 0x418f18 (adds `rim`/`sim`). Format types:
  - 1: with OPF_CODEREF, INFO "index jump" (`pchl`).
  - 4: address, appended (data or code).
  - 6 / 0x10: byte into the format.
  - 7 / 0x11: data symbol or word into the format.
  - 0x17: `rst`; the target is `opcode & 0x38`, labelled and threaded.
- **Z80** main 0x432138; prefix entries are types 0x12–0x15:
  - CB 0x435d38, ED 0x439938, DD 0x433538, FD 0x434938.
  - DD/FD CB entries are type 0x12, and then the opcode at pc+3 selects DDCB 0x437138 / FDCB 0x438538.
  - opaddr = pc+2 for every prefixed instruction.
  - Extra types: 2 rel, 3 `(ix+%s)` displacement (unsigned byte format), 0x16 `(ix+%s),%s` displacement + immediate.
- **LR35902** main 0x414ed0, CB 0x4162d0. Extra types: 5 `$FF00+n` (data label), 0x90 `($FF00+c)` (prints fmt_word(0xFF00)).
- **Thread:** 0x408ac0 is shared by Z80 and LR35902; it follows rel, address and rst.

## 8048 and 8051 modules

- **8048** table 0x41bb70, addr_max 0xFFF, max length 2, 'I'. Decode returns **pc itself** as opaddr, so operands are fetched at pc+1.
  - 0x30: in-page conditional jump, target `((pc+1) & 0xFF00) | byte`.
  - 0x31: `jmp`/`call`, target `(op & 0xE0) << 3 | byte`, 11 bits; the bank bit is not tracked.
  - 6: imm8 `#n` into the table format.
  - 1 with OPF_CODEREF: `jmpp @a`, INFO "indirect jump".
- **8051** table 0x41a498, 'I'. Code addresses are big endian (`img_word_be`), even though the CPU is marked little endian.
  - Name tables: bit names 0x43d348 (`p0.0`…`b.7`), SFR names 0x43d748 (`p0`, `sp`, `dpl`…).
    A direct address prints its SFR name, otherwise a data label or value. Pass 1 adds an X label only for non-SFR direct addresses.
    A bit address prints its bit name, otherwise a byte value.
  - Types: 2 rel, 4 `ljmp`/`lcall`, 5 direct, 6/7 imm, 0x38 `ajmp`/`acall` (`(op & 0xE0) << 3 | byte | (pc+2) & 0xF800`),
    0x41 `direct,#imm`, 0x48 bit, 0x49 `bit,rel`, 0x4A `#imm,rel`, 0x4B `direct,rel`.
    0x4C `mov direct,direct` prints the second operand byte first (the encoding is src, dst).
  - 1 with OPF_CODEREF: `jmp @a+dptr`, INFO "indirect jump".

## Memory map (object at 0x43eba0) types

`0` = skipped/outside, `1` = unknown ROM (threading not yet reached it), `2` = code, `4` = byte, `5` = word, `6` = dword, `7` = string.

## Symbol table

- **Layout:** an open-addressed hash of 0xfff1 slots × 20 bytes. The hash is `byteswap16(value) % 0xfff1`, with linear probing.
  Entry: `{ name*, length, subtype, value, type(u8) }`. Names live in a 400000-byte string pool.
- **Full table:** above 0xffef entries, `warning("symbol table full")`.
- **Types:** 4 = code label (L), 5 = data label (X). User `symbol` entries are also type 5 (Data); type 3 "Val " is never used.
  Type names for the table are at 0x43d0e8 (still to dump).
- **Generated names:** `L%04X` (code), `X%04X` (data); `%c%04X` picks L or X.
- **Listing order:** the table is walked by value 0..0xffff. Within a value, entries come out in probe order from the home slot.
- **Format:** `"%04X\t\t%s\t%s\n"` (value, type name, name); `%08X\t%s\t%s\n` when addr_max > 0xffff.
  Then `"\nNumber of symbols:\t\t\t%d\n\n"` and the dashed rule.

## .sym commands (keyword table at 0x4128c0)

Index: 0 org, 1 cpu, 2 code, 3 vector, 4 vectab, 5 addrtab, 6 byte, 7 word, 8 dword, 9 string, 10 symbol,
11 comment (→ error "command "comment" not yet implemented"), 12 skip, 13 include, 14 message, 15 numformat.
(Verified by dumping 0x4128c0.)

- **Line handling:** each line is read with `fgets(256)`, truncated at the first `;`, split on whitespace (max 20 tokens).
  The keyword match is case-insensitive (`_stricmp`, 0x4118f0).
- **Pass A:** org, cpu, include (recursive, pass A). An unknown keyword gives `error("invalid command "%s" in symbol file")`.
- **Pass B:** all the others, plus include (recursive, pass B).
- **Number parsing:** all numbers use `sscanf("%i")`.
- `org` with origin already set: warning "org statement in symbol file ignored". `cpu` with CPU already set: warning "cpu statement in symbol file ignored".
- `code a [name] [count]`: adds an entry point. With a name, adds a type-4 symbol; without one, adds an auto `L%04X`.
  With a count, adds entry points at a+1 … a+count−1 (**count is in units, not instructions**, despite the docs).
- `byte/word/dword/string a name [count]`: adds a symbol and marks memory types 4/5/6/7 for count×1/2/4/1 bytes.
- `vector a [vname] [dname]`: a vname marks the word as type 5. If the word is type 5, its target becomes an entry point; a dname names the target.
- `vectab/addrtab a name [count]`: marks count words as type 5. vectab adds each target as an entry point plus an `L` label; addrtab adds `X` labels.
- `skip a count`: marks type 0.
- `message ...`: prints the arguments separated by spaces, then `\n`, to stdout.
- `numformat X`: must be one character from A C D I M R S (case-insensitive).

## Number formats (0x404210 byte, 0x404310 word, 0x404410 dword)

| Fmt | Byte | Word | Dword |
|---|---|---|---|
| A | `&%02X` | `&%04X` | `&%08X` |
| C | `0x%02X` | `0x%04X` | `0x%08X` |
| D | `%d` | `%d` | `%d` |
| I | `0%02XH` | `0%04XH` | `0%08XH` |
| M | `$%02X` | `$%04X` | `$%08X` |
| R | `#%02X` | `#%04X` | `#%08X` |
| S | `H'%02X'` | `H'%04X'` | `H'%08X'` |

## Listing output

- **Line format (0x402450):** `"%04X : "`, hex bytes, tab-pad, `"ascii"`, (wide: tab-pad `[n]`, `[n/m]` or `[n+]`), tab-pad, mnemonic, tab-pad, operands, optional `INFO: ...`.
  In listing mode, continuation lines show the bytes that don't fit on the first line.
  Bytes per line come from the table at 0x413950, indexed by unit size, and are doubled when wide.
- **Tab padding (0x4022e0):** pads to a column with tabs, then spaces.
- **Sequence break (0x402400):** a separator line whenever code/data changes, or after a flags & 0x100 instruction.
- **Labels (0x403320):** `"%04X\t\t\t\t"` (listing; plus `"\t\t\t"` when wide), then `"%s:\n"`. Code labels (type 4) are printed always; data labels (type 5) only when not in asm mode (**check**: the condition is `DAT_0043eb80 == 0`).
- **Header (0x401190):** ";" lines, Disassembled by, File, Size, Checksum (16-bit sum, `%04X`), CRC-32 (CCITT, `%08X`), Date (`ctime`), CPU.
  In listing mode each line is prefixed with 4 tabs (to be confirmed). Then `org`.

## Threading (0x402c30 / 0x402b40)

- **Entry stack:** at most 0x7ff entries ("code threading entry table full"). It's processed LIFO.
- **Per thread:**
  1. Stop if the start is already code.
  2. While the memory type is 1: decode; if any byte of the instruction isn't type 1, print "Premature end of thread".
  3. On an invalid instruction, print "Thread terminated: invalid instruction at address %04X".
  4. Otherwise mark the bytes as code (2) and call cpu.thread (which pushes branch/call targets).
  5. Stop on flags & 0x100.
- **Non-ROM targets:** entries outside ROM print "Ignoring non-ROM code entry point %04X". Branches outside ROM print "Ignoring branch outside ROM to %04X at address %04X".

## Differences from the manual (behaviour to replicate)

- There's no default CPU; it errors instead (the manual says 6800).
- The default origin is 0 (the manual's option table says top of memory, but the 1.40 changelog says 0).
- The `code` count is in bytes/units.
- `comment` is a keyword, but it is "not yet implemented" (fatal).
- `-l` also lists 68HC705, 65C29 and NSC800. 65C59 appears in the manual's `cpu` syntax but isn't a real type.
