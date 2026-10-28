/*
 * Signetics 2650
 * decode [0x4076c0], pass1 [0x407710], format [0x4078a0], thread [0x407c70]
 *
 * Addresses are 15 bits in four 8K pages; relative and indexed addressing
 * stay within the current page.  Bit 7 of a relative offset (bit 15 of an
 * absolute address) means indirect, shown as a leading '*'.  For indexed
 * instructions the mnemonic itself is a format taking the register number.
 */

#include <string.h>

#include "dasmy.h"
#include "cpu_2650.h"

/* addressing types (flags & OPF_TYPE_MASK) */
enum {
    AM_INHERENT = 1,
    AM_REL = 2,         /* 7-bit relative within the page */
    AM_ABS = 3,         /* 13-bit absolute with index mode in bits 13-14 */
    AM_IMM8 = 6,
    AM_ABS_BRANCH = 0x38,
    AM_ZERO_REL = 0x39  /* zbrr/zbsr: 7-bit relative to address 0 */
};

#define INDIRECT_REL 0x80
#define INDIRECT_ABS 0x8000

uint32_t s2650_decode(uint32_t pc, OpEntry *e)
{
    *e = tab_2650[img_byte(pc)];
    return (pc + 1) & 0x7FFF;
}

/* 7-bit signed offset */
static int32_t offset7(uint8_t b)
{
    int32_t off = b & 0x7F;
    if (b & 0x40)
        off |= ~0x3F;
    return off;
}

static uint32_t rel_target(uint32_t pc, uint8_t b)
{
    return (((uint32_t)offset7(b) + 2 + pc) & 0x1FFF) | ((pc + 2) & 0x6000);
}

static uint32_t abs_target(uint32_t pc, uint32_t w)
{
    return ((pc + 3) & 0x6000) | (w & 0x1FFF);
}

void s2650_pass1(uint32_t pc, uint32_t opaddr, const OpEntry *e)
{
    uint8_t b;
    uint32_t w, t;
    int code = (e->flags & OPF_CODEREF) != 0;
    switch (e->flags & OPF_TYPE_MASK) {
    case AM_REL:
        b = img_byte(opaddr);
        t = rel_target(pc, b);
        if (b & INDIRECT_REL) {
            if (code)
                sym_vector_label(t);
            else
                sym_pointer_label(t);
        } else {
            sym_auto_label(t, code);
        }
        break;
    case AM_ABS:
        w = img_word(opaddr);
        t = abs_target(pc, w);
        if (w & INDIRECT_ABS)
            sym_pointer_label(t);
        else
            sym_auto_label(t, 0);
        break;
    case AM_ABS_BRANCH:
        w = img_word(opaddr);
        if (w & INDIRECT_ABS)
            sym_vector_label(w & 0x7FFF);
        else
            sym_auto_label(w & 0x7FFF, 1);
        break;
    case AM_ZERO_REL:
        b = img_byte(opaddr);
        t = (uint32_t)offset7(b) & 0x1FFF;
        if (b & INDIRECT_REL)
            sym_vector_label(t);
        else
            sym_auto_label(t, 1);
        break;
    default:
        break;
    }
}

/* Writes '*' at buf (the operand text is always empty) and returns the rest */
static char *indirect(char *buf)
{
    buf[0] = '*';
    buf[1] = '\0';
    return buf + 1;
}

void s2650_format(uint32_t pc, uint32_t opaddr, OpEntry *e, char *buf, const char **info)
{
    static char mnemonic[64];
    uint8_t b;
    uint32_t w, t, reg;
    int code = (e->flags & OPF_CODEREF) != 0;

    if (e->operands != NULL)
        strcpy(buf, e->operands);
    switch (e->flags & OPF_TYPE_MASK) {
    case AM_REL:
        b = img_byte(opaddr);
        t = rel_target(pc, b);
        if (b & INDIRECT_REL) {
            buf = indirect(buf);
            if (code)
                *info = "indirect jump";
        }
        /* An indirect target is a pointer, so it gets a data label */
        sym_append_name(buf, t, code && !(b & INDIRECT_REL));
        break;
    case AM_ABS:
        reg = img_byte(pc) & 3;
        w = img_word(opaddr);
        if (w & INDIRECT_ABS)
            buf = indirect(buf);
        sym_append_name(buf, abs_target(pc, w), 0);
        switch (w & 0x6000) {
        case 0x0000:
            sprintf(mnemonic, e->mnemonic, reg);
            break;
        case 0x2000:
            sprintf(mnemonic, e->mnemonic, 0);
            strcat(buf, s2650_index_regs[reg]);
            strcat(buf, ",+");
            break;
        case 0x4000:
            sprintf(mnemonic, e->mnemonic, 0);
            strcat(buf, s2650_index_regs[reg]);
            strcat(buf, ",-");
            break;
        case 0x6000:
            sprintf(mnemonic, e->mnemonic, 0);
            strcat(buf, s2650_index_regs[reg]);
            break;
        }
        e->mnemonic = mnemonic;
        break;
    case AM_IMM8:
        fmt_byte(buf, img_byte(opaddr));
        break;
    case AM_ABS_BRANCH:
        w = img_word(opaddr);
        if (w & INDIRECT_ABS) {
            buf = indirect(buf);
            *info = "indirect jump";
            sym_append_name(buf, w & 0x7FFF, 0);
        } else {
            sym_append_name(buf, w & 0x7FFF, 1);
        }
        break;
    case AM_ZERO_REL:
        b = img_byte(opaddr);
        if (b & INDIRECT_REL) {
            buf = indirect(buf);
            if (code)
                *info = "indirect jump";
        }
        sym_append_name(buf, (uint32_t)offset7(b) & 0x1FFF, !(b & INDIRECT_REL));
        break;
    default:
        break;
    }
}

/* Follow a direct or indirect branch to t; both bytes at t must be in the image */
static void follow(uint32_t t, int is_indirect, uint32_t pc)
{
    if (mem_get(t) == MEM_NONE || mem_get(t + 1) == MEM_NONE) {
        printf("Ignoring vector outside ROM at %04X, PC %04X\n", t, pc);
        return;
    }
    if (!is_indirect) {
        thread_add_entry(t);
        return;
    }
    uint32_t p = img_word(t) & 0x7FFF;
    if (mem_get(p) == MEM_NONE)
        printf("Ignoring branch indirect outside ROM to %04X via address %04X\n", p, t);
    else
        thread_add_entry(p);
}

void s2650_thread(uint32_t pc, uint32_t opaddr, const OpEntry *e)
{
    uint8_t b;
    uint32_t w;
    if (!(e->flags & OPF_CODEREF))
        return;
    switch (e->flags & OPF_TYPE_MASK) {
    case AM_REL:
        b = img_byte(opaddr);
        follow(rel_target(pc, b), (b & INDIRECT_REL) != 0, pc);
        break;
    case AM_ABS_BRANCH:
        w = img_word(opaddr);
        follow(w & 0x7FFF, (w & INDIRECT_ABS) != 0, pc);
        break;
    case AM_ZERO_REL:
        b = img_byte(opaddr);
        follow((uint32_t)offset7(b) & 0x1FFF, (b & INDIRECT_REL) != 0, pc);
        break;
    default:
        break;
    }
}
