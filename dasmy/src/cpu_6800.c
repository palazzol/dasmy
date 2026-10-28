/*
 * Motorola 6800/6801/6803/6802/6808 and Hitachi 6301/6303
 * decode [0x4063e0, 0x406430, 0x406480], pass1 [0x4064d0],
 * format [0x4065d0], thread [0x408f90]
 */

#include <string.h>

#include "dasmy.h"
#include "cpu_6800.h"

/* addressing types (flags & OPF_TYPE_MASK) */
enum {
    AM_INHERENT = 1,
    AM_RELATIVE = 2,
    AM_INDEXED = 3,
    AM_EXTENDED = 4,
    AM_DIRECT = 5,
    AM_IMM8 = 6,
    AM_IMM16 = 7,
    AM_IMM_INDEXED = 0x40,  /* 6301 aim/oim/eim/tim #imm,off,x */
    AM_IMM_DIRECT = 0x41    /* 6301 aim/oim/eim/tim #imm,addr */
};

static uint32_t decode(const OpEntry *tab, uint32_t pc, OpEntry *e)
{
    *e = tab[img_byte(pc)];
    return (pc + 1) & 0xFFFF;
}

uint32_t m6800_decode(uint32_t pc, OpEntry *e) { return decode(tab_6800, pc, e); }
uint32_t m6801_decode(uint32_t pc, OpEntry *e) { return decode(tab_6801, pc, e); }
uint32_t m6301_decode(uint32_t pc, OpEntry *e) { return decode(tab_6301, pc, e); }

static uint32_t rel_target(uint32_t pc, uint32_t opaddr)
{
    return ((uint32_t)(int8_t)img_byte(opaddr) + pc + 2) & 0xFFFF;
}

void m6800_pass1(uint32_t pc, uint32_t opaddr, const OpEntry *e)
{
    int code = (e->flags & OPF_CODEREF) != 0;
    switch (e->flags & OPF_TYPE_MASK) {
    case AM_RELATIVE:
        sym_auto_label(rel_target(pc, opaddr), 1);
        break;
    case AM_EXTENDED:
        sym_auto_label(img_word(opaddr), code);
        break;
    case AM_DIRECT:
        sym_auto_label(img_byte(opaddr), code);
        break;
    case AM_IMM_DIRECT:
        sym_auto_label(img_byte(opaddr + 1), 0);
        break;
    default:
        break;
    }
}

void m6800_format(uint32_t pc, uint32_t opaddr, OpEntry *e, char *buf, const char **info)
{
    char a[64], b[64];
    int code = (e->flags & OPF_CODEREF) != 0;
    switch (e->flags & OPF_TYPE_MASK) {
    case AM_RELATIVE:
        sym_append_name(buf, rel_target(pc, opaddr), 1);
        break;
    case AM_INDEXED:
        fmt_byte(buf, img_byte(opaddr));
        strcat(buf, ",x");
        if (e->flags & OPF_CODEREF)
            *info = "index jump";
        break;
    case AM_EXTENDED:
        sym_append_name(buf, img_word(opaddr), code);
        break;
    case AM_DIRECT:
        sym_append_name(buf, img_byte(opaddr), code);
        break;
    case AM_IMM8:
        buf[0] = '#';
        fmt_byte(buf + 1, img_byte(opaddr));
        break;
    case AM_IMM16:
        buf[0] = '#';
        fmt_word(buf + 1, img_word(opaddr));
        break;
    case AM_IMM_INDEXED:
        fmt_byte(a, img_byte(opaddr));
        fmt_byte(b, img_byte(opaddr + 1));
        sprintf(buf, "#%s,%s,x", a, b);
        break;
    case AM_IMM_DIRECT:
        /* No separator between the operands - as in the original */
        buf[0] = '#';
        fmt_byte(buf + 1, img_byte(opaddr));
        sym_append_name(buf, img_byte(opaddr + 1), 0);
        break;
    default:
        break;
    }
}

void m6800_thread(uint32_t pc, uint32_t opaddr, const OpEntry *e)
{
    if (!(e->flags & OPF_CODEREF))
        return;
    switch (e->flags & OPF_TYPE_MASK) {
    case AM_RELATIVE:
        thread_branch(rel_target(pc, opaddr), pc);
        break;
    case AM_INDEXED:
        printf("Ignoring index branch opcode %02X at address %04X\n", img_byte(pc), pc);
        break;
    case AM_EXTENDED:
        thread_branch(img_word(opaddr), pc);
        break;
    case AM_DIRECT:
        thread_branch(img_byte(opaddr), pc);
        break;
    default:
        break;
    }
}
