/*
 * Motorola 6805, 68HC05/68HC705 and Hitachi 63L05, 6305
 * decode [0x406850, 0x4068a0, 0x4068f0, 0x406940], pass1 [0x406990],
 * format [0x406ac0], thread [0x406de0]
 *
 * Relative branch targets are masked to 11 bits for every variant, as in
 * the original (even those with a larger address space).
 */

#include <string.h>

#include "dasmy.h"
#include "cpu_6805.h"

/* addressing types (flags & OPF_TYPE_MASK) */
enum {
    AM_INHERENT = 1,    /* operand text, if any, comes from the table */
    AM_RELATIVE = 2,
    AM_EXTENDED = 4,
    AM_DIRECT = 5,      /* table text, if any, is a format such as "0,%s" */
    AM_IMM8 = 6,
    AM_BIT_REL = 0x4B,  /* brset/brclr n,addr,rel; table text is "n," */
    AM_IX1 = 0x50,      /* 8-bit offset,x */
    AM_IX2 = 0x51       /* 16-bit offset,x */
};

#define BRANCH_MASK 0x7FF

static uint32_t decode(const OpEntry *tab, uint32_t pc, OpEntry *e)
{
    *e = tab[img_byte(pc)];
    return (pc + 1) & 0xFFFF;
}

uint32_t m6805_decode(uint32_t pc, OpEntry *e) { return decode(tab_6805, pc, e); }
uint32_t m68hc05_decode(uint32_t pc, OpEntry *e) { return decode(tab_68hc05, pc, e); }
uint32_t m63l05_decode(uint32_t pc, OpEntry *e) { return decode(tab_63l05, pc, e); }
uint32_t m6305_decode(uint32_t pc, OpEntry *e) { return decode(tab_6305, pc, e); }

static uint32_t rel_target(uint32_t pc, uint32_t at, uint32_t len)
{
    return ((uint32_t)(int8_t)img_byte(at) + pc + len) & BRANCH_MASK;
}

void m6805_pass1(uint32_t pc, uint32_t opaddr, const OpEntry *e)
{
    int code = (e->flags & OPF_CODEREF) != 0;
    switch (e->flags & OPF_TYPE_MASK) {
    case AM_RELATIVE:
        sym_auto_label(rel_target(pc, opaddr, 2), 1);
        break;
    case AM_BIT_REL:
        sym_auto_label(img_byte(opaddr), 0);
        sym_auto_label(rel_target(pc, opaddr + 1, 3), 1);
        break;
    case AM_EXTENDED:
    case AM_IX2:
        sym_auto_label(img_word(opaddr), code);
        break;
    case AM_DIRECT:
    case AM_IX1:
        sym_auto_label(img_byte(opaddr), code);
        break;
    default:
        break;
    }
}

void m6805_format(uint32_t pc, uint32_t opaddr, OpEntry *e, char *buf, const char **info)
{
    char tmp[256];
    int code = (e->flags & OPF_CODEREF) != 0;
    switch (e->flags & OPF_TYPE_MASK) {
    case AM_INHERENT:
        if (e->operands != NULL)
            strcpy(buf, e->operands);
        if (code)
            *info = "index jump";
        break;
    case AM_RELATIVE:
        sym_append_name(buf, rel_target(pc, opaddr, 2), 1);
        break;
    case AM_BIT_REL:
        strcpy(buf, e->operands);
        sym_append_name(buf, img_byte(opaddr), 0);
        strcat(buf, ",");
        sym_append_name(buf, rel_target(pc, opaddr + 1, 3), 1);
        break;
    case AM_DIRECT:
        if (e->operands != NULL) {
            tmp[0] = '\0';
            sym_append_name(tmp, img_byte(opaddr), code);
            sprintf(buf, e->operands, tmp);
        } else {
            sym_append_name(buf, img_byte(opaddr), code);
        }
        break;
    case AM_IX1:
        sym_append_name(buf, img_byte(opaddr), code);
        strcat(buf, ",x");
        if (code)
            *info = "index jump";
        break;
    case AM_IX2:
        sym_append_name(buf, img_word(opaddr), code);
        strcat(buf, ",x");
        if (code)
            *info = "index jump";
        break;
    case AM_EXTENDED:
        sym_append_name(buf, img_word(opaddr), code);
        break;
    case AM_IMM8:
        buf[0] = '#';
        fmt_byte(buf + 1, img_byte(opaddr));
        break;
    default:
        break;
    }
}

void m6805_thread(uint32_t pc, uint32_t opaddr, const OpEntry *e)
{
    if (!(e->flags & OPF_CODEREF))
        return;
    switch (e->flags & OPF_TYPE_MASK) {
    case AM_RELATIVE:
        thread_branch(rel_target(pc, opaddr, 2), pc);
        break;
    case AM_BIT_REL:
        thread_branch(rel_target(pc, opaddr + 1, 3), pc);
        break;
    case AM_DIRECT:
        thread_branch(img_byte(opaddr), pc);
        break;
    case AM_EXTENDED:
        thread_branch(img_word(opaddr), pc);
        break;
    case AM_IX1:
        printf("Ignoring index branch opcode %02X %02X at address %04X\n",
               img_byte(pc), img_byte(pc + 1), pc);
        break;
    case AM_IX2:
        printf("Ignoring index branch opcode %02X %02X %02X at address %04X\n",
               img_byte(pc), img_byte(pc + 1), img_byte(pc + 2), pc);
        break;
    default:
        break;
    }
}
