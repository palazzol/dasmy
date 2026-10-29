# DASMy

DASMy is a cross-platform reimplementation of **DASMx 1.40**, the multi-CPU
disassembler by Conquest Consultants (2003). It takes a binary ROM image and
produces a listing (`.lst`) or assembler source (`.asm`) for one of 31 classic
8-bit microprocessors and microcontrollers.

DASMy is written in portable C99. Its output matches DASMx's byte for byte,
except for the lines that name the program and the date. An automated test
suite checks this against the original program.

## Supported processors

| Family | CPU types (`-c` / `cpu`) |
|---|---|
| Motorola 6800 | `6800` `6802` `6808` `6801` `6803`, Hitachi `6301` `6303` |
| Motorola 6805 | `6805` `68HC05` `68HC705`, Hitachi `63L05` `6305` |
| Motorola 6809 | `6809` |
| MOS 6502 | `6502`, Rockwell `65C00` `65C29` `65C02` `65C102` `65C112` |
| Zilog Z80 | `Z80`, National `NSC800` |
| Sharp (GameBoy) | `LR35902` |
| Intel 8080/8085 | `8080` `8085` |
| Intel MCS-48 | `8048` |
| Intel MCS-51 | `8051` |
| Microchip PIC | `PIC16F83` `PIC16F84` |
| Signetics | `2650` |
| RCA COSMAC | `1802` `1805` |

`dasmy -l` prints this list.

## Building

You need CMake 3.10 or later and a C99 compiler.

```sh
cmake -S dasmy -B dasmy/build -DCMAKE_BUILD_TYPE=Release
cmake --build dasmy/build
```

This produces `dasmy/build/dasmy`. It has been built and tested on Linux.

### Windows

Cross-compile from Linux with MinGW-w64 (on Arch: `pacman -S mingw-w64-gcc`):

```sh
cmake -S dasmy -B dasmy/build-win -DCMAKE_TOOLCHAIN_FILE="$PWD/dasmy/cmake/mingw-w64.cmake" -DCMAKE_BUILD_TYPE=Release
cmake --build dasmy/build-win
```

The toolchain path must be absolute, because CMake resolves a relative one
against the build directory.

This produces `dasmy/build-win/dasmy.exe`. It needs no MinGW DLLs, and uses
the Universal C Runtime that comes with Windows 10 and later. The full test
suite passes with it under Wine. The project should also build natively with
Visual Studio or MinGW on Windows, but that hasn't been tested.

## Usage

```
dasmy [-altvw] [-cTYPE] [-eNNNN] [-oNNNN] binfile[.ext]
```

| Option | Meaning |
|---|---|
| `-a` | Write assembler source (`.asm`) instead of a listing (`.lst`) |
| `-cTYPE` | CPU type, e.g. `-cZ80`. Overrides `cpu` in the symbol file |
| `-eNNNN` | Code entry point for threading |
| `-l` | List the supported CPU types and exit |
| `-oNNNN` | Origin: the address of the first byte of the image (default 0) |
| `-t` | Code threading: follow execution from the entry points to tell code from data |
| `-v` | Show version information and exit |
| `-w` | Wide listing: cycle counts and up to 8 bytes per line |

Numbers use C notation: `4096`, `0x1000` or `010000`.

If the image file can't be opened and its name has no extension, DASMy tries
again with `.bin` added. The output file gets the image's name with `.lst` or
`.asm` in place of its extension.

Example: disassemble a 6809 ROM that sits at `0xC000`, following code from its
reset and interrupt vectors. Put this in `game.sym` next to `game.bin`:

```
cpu     6809
org     0xC000
vector  0xFFFE  resetVector  reset
vector  0xFFF8  irqVector    irq
```

Then run:

```sh
dasmy -t game.bin
```

This writes `game.lst`. Threading needs at least one entry point, from `-e`,
`code`, `vector` or `vectab`.

### Symbol file

If a file with the image's base name and the extension `.sym` exists, DASMy
reads it. It's a text file with one command per line; `;` starts a comment.

| Command | Meaning |
|---|---|
| `cpu TYPE` | CPU type |
| `org ADDR` | Origin |
| `symbol VALUE NAME` | Name a value (usually an address) |
| `code ADDR [NAME [COUNT]]` | Code entry point for threading |
| `vector ADDR [NAME [TARGET]]` | A word that points to code (e.g. the reset vector) |
| `vectab ADDR NAME [COUNT]` | A table of COUNT code pointers |
| `addrtab ADDR NAME [COUNT]` | A table of COUNT data pointers |
| `byte` / `word` / `dword ADDR NAME [COUNT]` | Data of 1, 2 or 4 bytes per item |
| `string ADDR NAME [COUNT]` | Character data |
| `skip ADDR COUNT` | Leave bytes out of the output |
| `include FILE` | Read commands from another file |
| `message TEXT...` | Print a message while disassembling |
| `numformat X` | Hex number style: `A` (`&F12C`), `C` (`0xF12C`), `D` (decimal), `I` (`0F12CH`), `M` (`$F12C`), `R` (`#F12C`), `S` (`H'F12C'`) |

The DASMx manual (`DASMx.htm`, in the original distribution) describes
these in detail, along with code threading and the processor specifics.
DASMy follows DASMx's actual behaviour, which differs from the manual in a few places:

- **CPU type:** there is no default. A CPU must be given with `-c` or `cpu`.
- **`code` count:** a COUNT on `code` adds an entry point at each of the next COUNT *bytes*, not instructions.
- **`comment`:** the command is recognised but is an error ("not yet implemented").
- **Case:** command names and CPU types are matched without regard to case.

## Differences from DASMx

- **Program name:** the banner and the listing header name DASMy.
- **Out-of-range reads:** where DASMx read memory outside the image (e.g. an image placed beyond the CPU's
  address space, whose operands then point below it), it printed whatever was there. DASMy reads 0.
- **Files without an extension:** DASMx crashes on an input file that exists without an extension. DASMy adds
  `.lst`/`.asm` to the name.
- **Paths:** `/` counts as a directory separator as well as `\`.
- **Statistics:** threading statistics for an image whose bytes are all skipped are reported as 0% instead of
  crashing.

DASMx's other quirks are reproduced deliberately, even where they look like
bugs. For example, 6805 branch targets wrap at 11 bits, and the 6809 lists
`pulu` registers in the opposite order. `re/NOTES.md` lists them.

## Testing

`tests/compare.py` runs DASMx under [Wine](https://www.winehq.org/) and
DASMy on the same inputs, then compares the exit codes, console output and
generated files. The test images are synthetic: opcode sweeps, random data,
and programs built from valid instructions. They cover every opcode of every
CPU, all the options and symbol file commands, and the error messages.

You need the original `Dasmx.exe` in `original/`; it isn't part of this repository.

```sh
python3 tests/compare.py                 # all cases
python3 tests/compare.py -k 6809 -v      # matching cases, with diffs
python3 tests/compare.py --dasmy dasmy/build-win/dasmy.exe   # test the Windows build under Wine
```

DASMx's results are cached in `tests/.cache/`. The first run takes several
minutes; later runs take seconds.

## Repository layout

| Path | Contents |
|---|---|
| `dasmy/src/` | Core: command line, symbol file, symbol table, threading, output |
| `dasmy/src/cpu_*.c` | One module per CPU family |
| `dasmy/src/tables/` | Opcode tables, generated from `Dasmx.exe` by `re/scripts/gentables.py` |
| `re/` | Reverse-engineering notes (`NOTES.md`) and scripts (Ghidra export, PE reader, table generator) |
| `tests/` | Comparison harness and test case definitions |

## Origin and copyright

DASMy was produced by decompiling DASMx 1.40 with Ghidra and porting its logic
to C. The opcode tables are extracted directly from the DASMx executable.
DASMx is copyright Conquest Consultants. Its distribution terms allow it to be
copied freely, but say nothing about derived works. Bear that in mind before
publishing or distributing DASMy.

The original DASMx distribution is not included in this repository.  

It can still be found here:  
https://www.geocities.ws/pclareuk/DASMx/index.html
