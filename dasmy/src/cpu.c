/* CPU descriptors [0x43d058] and selection [0x401c30, 0x401ca0] */

#include <ctype.h>

#include "dasmy.h"
#include "cpu_1802.h"
#include "cpu_2650.h"
#include "cpu_6502.h"
#include "cpu_6800.h"
#include "cpu_6805.h"
#include "cpu_6809.h"
#include "cpu_8048.h"
#include "cpu_8051.h"
#include "cpu_8080.h"
#include "cpu_pic16.h"
#include "cpu_z80.h"

const CpuDesc *cpu;

#define M6800_FAMILY(name, part, mfr, fam, decode) \
    { name, part, mfr, fam, 1, 1, 3, 0, 0xFFFF, 8, 0, 0, 0, 'M', \
      decode, m6800_pass1, m6800_format, m6800_thread }

#define M6502_FAMILY(name, part, mfr, fam, decode) \
    { name, part, mfr, fam, 0, 1, 3, 0, 0xFFFF, 8, 0, 0, 0, 'M', \
      decode, m6502_pass1, m6502_format, m6502_thread }

#define M6805_FAMILY(name, part, mfr, fam, addr_max, decode) \
    { name, part, mfr, fam, 1, 1, 3, 0, addr_max, 8, 0, 0, 0, 'M', \
      decode, m6805_pass1, m6805_format, m6805_thread }

static const CpuDesc cpu_63l05 = M6805_FAMILY("63L05", "HD63L05", "Hitachi", "63L05", 0xFFF, m63l05_decode);
static const CpuDesc cpu_6305 = M6805_FAMILY("6305", "HD6305", "Hitachi", "6305", 0x3FFF, m6305_decode);
static const CpuDesc cpu_6805 = M6805_FAMILY("6805", "MC6805", "Motorola", "6805", 0xFFF, m6805_decode);
static const CpuDesc cpu_68hc05 = M6805_FAMILY("68HC05", "MC68HC05", "Motorola", "68HC05", 0x1FFF, m68hc05_decode);
static const CpuDesc cpu_68hc705 = M6805_FAMILY("68HC705", "MC68HC705", "Motorola", "68HC05", 0x1FFF, m68hc05_decode);

#define INTEL_STYLE(name, part, mfr, fam, max_len, decode, pass1, format, thread) \
    { name, part, mfr, fam, 0, 1, max_len, 0, 0xFFFF, 8, 0, 0, 0, 'I', decode, pass1, format, thread }

static const CpuDesc cpu_8048 = { "8048", "I8048", "Intel", "MCS-48", 0, 1, 2, 0, 0xFFF, 8, 0, 0, 0, 'I',
                                  i8048_decode, i8048_pass1, i8048_format, i8048_thread };
/* The only CPU with a separate data address space */
static const CpuDesc cpu_8051 = { "8051", "I8051", "Intel", "MCS-51", 0, 1, 3, 0, 0xFFFF, 8, 0, 0xFFFF, 8, 'I',
                                  i8051_decode, i8051_pass1, i8051_format, i8051_thread };
static const CpuDesc cpu_8080 = INTEL_STYLE("8080", "I8080", "Intel", "MCS-80/85", 3,
                                            i8080_decode, i8080_pass1, i8080_format, i8080_thread);
static const CpuDesc cpu_8085 = INTEL_STYLE("8085", "I8085", "Intel", "MCS-80/85", 3,
                                            i8085_decode, i8080_pass1, i8080_format, i8080_thread);
static const CpuDesc cpu_lr35902 = INTEL_STYLE("LR35902", "LR35902", "Sharp", "GameBoy", 3,
                                               lr35902_decode, lr35902_pass1, lr35902_format, z80_thread);
static const CpuDesc cpu_nsc800 = INTEL_STYLE("NSC800", "NSC800", "National Semiconductor", "Z80", 4,
                                              z80_decode, z80_pass1, z80_format, z80_thread);
static const CpuDesc cpu_z80 = INTEL_STYLE("Z80", "Z80", "Zilog", "Z80", 4,
                                           z80_decode, z80_pass1, z80_format, z80_thread);

/* 16-bit units (one 14-bit instruction each), default number format C */
static const CpuDesc cpu_pic16f83 = { "PIC16F83", "PIC16F83", "Microchip", "PIC16CXXX", 0, 2, 2, 0, 0x1FF, 16, 0, 0, 0, 'C',
                                      pic16_decode, pic16_pass1, pic16_format, pic16_thread };
static const CpuDesc cpu_pic16f84 = { "PIC16F84", "PIC16F84", "Microchip", "PIC16CXXX", 0, 2, 2, 0, 0x3FF, 16, 0, 0, 0, 'C',
                                      pic16_decode, pic16_pass1, pic16_format, pic16_thread };

static const CpuDesc cpu_6809 = { "6809", "M6809", "Motorola", "6809", 1, 1, 5, 0, 0xFFFF, 8, 0, 0, 0, 'M',
                                  m6809_decode, m6809_pass1, m6809_format, m6809_thread };

static const CpuDesc cpu_1802 = { "1802", "CDP1802", "RCA", "1802 COSMAC", 1, 1, 3, 0, 0xFFFF, 8, 0, 0, 0, 'R',
                                  r1802_decode, r1802_pass1, r1802_format, r1802_thread };
static const CpuDesc cpu_1805 = { "1805", "CDP1805", "RCA", "1805/1806", 1, 1, 3, 0, 0xFFFF, 8, 0, 0, 0, 'R',
                                  r1805_decode, r1802_pass1, r1802_format, r1802_thread };
static const CpuDesc cpu_2650 = { "2650", "SCN2650", "Signetics", "2650", 1, 1, 3, 0, 0x7FFF, 8, 0, 0, 0, 'S',
                                  s2650_decode, s2650_pass1, s2650_format, s2650_thread };

static const CpuDesc cpu_6502 = M6502_FAMILY("6502", "MCS6502", "MOS Technology", "MCS6500", m6502_decode);
static const CpuDesc cpu_65c00 = M6502_FAMILY("65C00", "R65C00", "Rockwell", "65C00/21 & 65C29", m65c00_decode);
static const CpuDesc cpu_65c29 = M6502_FAMILY("65C29", "R65C29", "Rockwell", "65C00/21 & 65C29", m65c00_decode);
static const CpuDesc cpu_65c02 = M6502_FAMILY("65C02", "R65C02", "Rockwell", "65C02/65C102/65C112", m65c02_decode);
static const CpuDesc cpu_65c102 = M6502_FAMILY("65C102", "R65C102", "Rockwell", "65C02/65C102/65C112", m65c02_decode);
static const CpuDesc cpu_65c112 = M6502_FAMILY("65C112", "R65C112", "Rockwell", "65C02/65C102/65C112", m65c02_decode);

static const CpuDesc cpu_6301 = M6800_FAMILY("6301", "HD6301", "Hitachi", "6301/6303", m6301_decode);
static const CpuDesc cpu_6303 = M6800_FAMILY("6303", "HD6303", "Hitachi", "6301/6303", m6301_decode);
static const CpuDesc cpu_6800 = M6800_FAMILY("6800", "M6800", "Motorola", "6800/6802/6808", m6800_decode);
static const CpuDesc cpu_6801 = M6800_FAMILY("6801", "M6801", "Motorola", "6801/6803", m6801_decode);
static const CpuDesc cpu_6802 = M6800_FAMILY("6802", "M6802", "Motorola", "6800/6802/6808", m6800_decode);
static const CpuDesc cpu_6803 = M6800_FAMILY("6803", "M6803", "Motorola", "6801/6803", m6801_decode);
static const CpuDesc cpu_6808 = M6800_FAMILY("6808", "M6808", "Motorola", "6800/6802/6808", m6800_decode);

/* In the order of the original's table (and its -l listing) */
const CpuDesc *const cpu_list[] = {
    &cpu_1802, &cpu_1805, &cpu_2650,
    &cpu_6502, &cpu_65c00, &cpu_65c29, &cpu_65c02, &cpu_65c102, &cpu_65c112,
    &cpu_6301, &cpu_6303, &cpu_63l05, &cpu_6305,
    &cpu_6800, &cpu_6801, &cpu_6802, &cpu_6803, &cpu_6805, &cpu_68hc05, &cpu_68hc705, &cpu_6808, &cpu_6809,
    &cpu_8048, &cpu_8051, &cpu_8080, &cpu_8085, &cpu_lr35902, &cpu_nsc800,
    &cpu_pic16f83, &cpu_pic16f84, &cpu_z80,
    NULL
};

static int ascii_stricmp(const char *a, const char *b)
{
    for (;; a++, b++) {
        int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
        if (ca != cb || ca == 0)
            return ca - cb;
    }
}

/* [0x401c30] */
int cpu_select(const char *name)
{
    for (int i = 0; cpu_list[i] != NULL; i++) {
        if (ascii_stricmp(name, cpu_list[i]->name) == 0) {
            cpu = cpu_list[i];
            return 1;
        }
    }
    return 0;
}

/* [0x401ca0] */
void cpu_print_list(FILE *f)
{
    fputs("CPU             Manufacturer            Processor family\n", f);
    fputs("---             ------------            ----------------\n", f);
    for (int i = 0; cpu_list[i] != NULL; i++)
        fprintf(f, "%-16s%-24s%s\n", cpu_list[i]->name, cpu_list[i]->manufacturer, cpu_list[i]->family);
}
