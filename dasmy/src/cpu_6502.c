/*
 * MOS Technology 6502, Rockwell 65C00/21 and 65C29, Rockwell 65C02/65C102/65C112
 * decode [0x406f70, 0x406fc0, 0x407010], pass1 [0x407060],
 * format [0x407240], thread [0x409050]
 */

#include <string.h>

#include "dasmy.h"
#include "cpu_6502.h"

/* addressing types (flags & OPF_TYPE_MASK) */
enum {
    AM_IMPLIED = 1,     /* operand text, if any, comes from the table ("a") */
    AM_RELATIVE = 2,
    AM_ABSOLUTE = 4,
    AM_ZEROPAGE = 5,
    AM_IMM8 = 6,
    AM_IMM16 = 7,
    AM_ZP_X_IND = 0x18, /* (zp,x) */
    AM_ZP_IND_Y = 0x19, /* (zp),y */
    AM_ZP_X = 0x1A,
    AM_ZP_Y = 0x1B,
    AM_ABS_X = 0x1C,
    AM_ABS_Y = 0x1D,
    AM_ABS_IND = 0x1E,  /* jmp (abs) */
    AM_ZP_IND = 0x20,   /* (zp), 65C02 */
    AM_ABS_X_IND = 0x21,/* jmp (abs,x), 65C02 */
    AM_ZP_REL = 0x4B    /* bbr/bbs zp,rel */
};

static uint32_t decode(const OpEntry *tab, uint32_t pc, OpEntry *e)
{
    *e = tab[img_byte(pc)];
    return (pc + 1) & 0xFFFF;
}

uint32_t m6502_decode(uint32_t pc, OpEntry *e) { return decode(tab_6502, pc, e); }
uint32_t m65c00_decode(uint32_t pc, OpEntry *e) { return decode(tab_65c00, pc, e); }
uint32_t m65c02_decode(uint32_t pc, OpEntry *e) { return decode(tab_65c02, pc, e); }

static uint32_t rel_target(uint32_t pc, uint32_t at, uint32_t len)
{
    return ((uint32_t)(int8_t)img_byte(at) + pc + len) & 0xFFFF;
}

/* Is the pointer at addr inside the image? */
static int pointer_in_image(uint32_t addr)
{
    return mem_get(addr) != MEM_NONE && mem_get(addr + 1) != MEM_NONE;
}

void m6502_pass1(uint32_t pc, uint32_t opaddr, const OpEntry *e)
{
    uint32_t p;
    switch (e->flags & OPF_TYPE_MASK) {
    case AM_RELATIVE:
        sym_auto_label(rel_target(pc, opaddr, 2), 1);
        break;
    case AM_ZP_REL:
        sym_auto_label(img_byte(opaddr), 0);
        sym_auto_label(rel_target(pc, opaddr + 1, 3), 1);
        break;
    case AM_ABSOLUTE:
        sym_auto_label(img_word(opaddr), (e->flags & OPF_CODEREF) != 0);
        break;
    case AM_ABS_X:
    case AM_ABS_Y:
        sym_auto_label(img_word(opaddr), 0);
        break;
    case AM_ZEROPAGE:
    case AM_ZP_X_IND:
    case AM_ZP_IND_Y:
    case AM_ZP_X:
    case AM_ZP_Y:
        sym_auto_label(img_byte(opaddr), 0);
        break;
    case AM_ABS_IND:
        p = img_word(opaddr);
        sym_auto_label(p, 0);
        if (pointer_in_image(p))
            sym_auto_label(img_word(p), 1);
        break;
    case AM_ZP_IND:
        p = img_byte(opaddr);
        sym_auto_label(p, 0);
        if (pointer_in_image(p))
            sym_auto_label(img_word(p), 0);
        break;
    default:
        break;
    }
}

/* Append a data label or value, wrapped in prefix and suffix */
static void operand(char *buf, const char *prefix, uint32_t value, const char *suffix)
{
    strcat(buf, prefix);
    sym_append_name(buf, value, 0);
    strcat(buf, suffix);
}

void m6502_format(uint32_t pc, uint32_t opaddr, OpEntry *e, char *buf, const char **info)
{
    switch (e->flags & OPF_TYPE_MASK) {
    case AM_IMPLIED:
        if (e->operands != NULL)
            strcpy(buf, e->operands);
        break;
    case AM_RELATIVE:
        sym_append_name(buf, rel_target(pc, opaddr, 2), 1);
        break;
    case AM_ZP_REL:
        sym_append_name(buf, img_byte(opaddr), 0);
        strcat(buf, ",");
        sym_append_name(buf, rel_target(pc, opaddr + 1, 3), 1);
        break;
    case AM_ABSOLUTE:
        sym_append_name(buf, img_word(opaddr), (e->flags & OPF_CODEREF) != 0);
        break;
    case AM_ZEROPAGE:
        sym_append_name(buf, img_byte(opaddr), 0);
        break;
    case AM_IMM8:
        buf[0] = '#';
        fmt_byte(buf + 1, img_byte(opaddr));
        break;
    case AM_IMM16:
        buf[0] = '#';
        fmt_word(buf + 1, img_word(opaddr));
        break;
    case AM_ZP_X_IND:
        strcpy(buf, "(");
        operand(buf, "", img_byte(opaddr), ",x)");
        break;
    case AM_ZP_IND_Y:
        strcpy(buf, "(");
        operand(buf, "", img_byte(opaddr), "),y");
        break;
    case AM_ZP_X:
        operand(buf, "", img_byte(opaddr), ",x");
        break;
    case AM_ZP_Y:
        operand(buf, "", img_byte(opaddr), ",y");
        break;
    case AM_ABS_X:
        operand(buf, "", img_word(opaddr), ",x");
        break;
    case AM_ABS_Y:
        operand(buf, "", img_word(opaddr), ",y");
        break;
    case AM_ABS_IND:
        operand(buf, "(", img_word(opaddr), ")");
        /* The NMOS 6502 fetches the high byte from the start of the same page */
        if (img_byte(opaddr) == 0xFF) {
            printf("Indirect jump across page boundary at %04X\n", pc);
            *info = "indirect jump across page boundary";
        } else {
            *info = "indirect jump";
        }
        break;
    case AM_ZP_IND:
        operand(buf, "(", img_byte(opaddr), ")");
        break;
    case AM_ABS_X_IND:
        operand(buf, "(", img_word(opaddr), ",x)");
        *info = "index jump";
        break;
    default:
        break;
    }
}

void m6502_thread(uint32_t pc, uint32_t opaddr, const OpEntry *e)
{
    if (!(e->flags & OPF_CODEREF))
        return;
    switch (e->flags & OPF_TYPE_MASK) {
    case AM_RELATIVE:
        thread_branch(rel_target(pc, opaddr, 2), pc);
        break;
    case AM_ZP_REL:
        thread_branch(rel_target(pc, opaddr + 1, 3), pc);
        break;
    case AM_ABSOLUTE:
        thread_branch(img_word(opaddr), pc);
        break;
    case AM_ABS_IND: {
        uint32_t p = img_word(opaddr);
        if (pointer_in_image(p))
            thread_branch(img_word(p), pc);
        else
            printf("Ignoring indirect jump outside ROM via %04X at address %04X\n", p, pc);
        break;
    }
    default:
        break;
    }
}
