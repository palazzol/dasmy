/* CPU descriptors [0x43d058] and selection [0x401c30, 0x401ca0] */

#include <ctype.h>

#include "dasmy.h"
#include "cpu_6502.h"
#include "cpu_6800.h"

const CpuDesc *cpu;

#define M6800_FAMILY(name, part, mfr, fam, decode) \
    { name, part, mfr, fam, 1, 1, 3, 0, 0xFFFF, 8, 0, 0, 0, 'M', \
      decode, m6800_pass1, m6800_format, m6800_thread }

#define M6502_FAMILY(name, part, mfr, fam, decode) \
    { name, part, mfr, fam, 0, 1, 3, 0, 0xFFFF, 8, 0, 0, 0, 'M', \
      decode, m6502_pass1, m6502_format, m6502_thread }

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
    &cpu_6502, &cpu_65c00, &cpu_65c29, &cpu_65c02, &cpu_65c102, &cpu_65c112,
    &cpu_6301, &cpu_6303,
    &cpu_6800, &cpu_6801, &cpu_6802, &cpu_6803, &cpu_6808,
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
