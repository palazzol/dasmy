/*
 * Zilog Z80 / National Semiconductor NSC800, and Sharp LR35902 (GameBoy)
 * Z80: decode [0x408b40], pass1 [0x404510], format [0x4045e0]
 * LR35902: decode [0x404c10], pass1 [0x404ca0], format [0x404d80]
 * thread (shared) [0x408ac0]
 *
 * The table's operand text is copied as is, or used as a printf format for
 * the operand value(s).
 */

#include <string.h>

#include "dasmy.h"
#include "cpu_z80.h"

/* addressing types (flags & OPF_TYPE_MASK) */
enum {
    AM_INHERENT = 1,
    AM_REL = 2,         /* jr/djnz */
    AM_DISP = 3,        /* (ix+d) */
    AM_ADDR = 4,        /* jump/call target */
    AM_HIGH = 5,        /* LR35902: $FF00 + n */
    AM_IMM8 = 6,
    AM_IMM16 = 7,
    AM_PORT = 0x10,
    AM_DATA16 = 0x11,
    AM_PREFIX_CB = 0x12,
    AM_PREFIX_ED = 0x13,
    AM_PREFIX_DD = 0x14,
    AM_PREFIX_FD = 0x15,
    AM_DISP_IMM = 0x16, /* (ix+d),n */
    AM_RST = 0x17,      /* target is opcode & 0x38 */
    AM_HIGH_C = 0x90    /* LR35902: ($FF00+c) */
};

static int type_of(const OpEntry *e)
{
    return (int)(e->flags & OPF_TYPE_MASK);
}

uint32_t z80_decode(uint32_t pc, OpEntry *e)
{
    *e = tab_z80[img_byte(pc)];
    switch (type_of(e)) {
    case AM_PREFIX_CB:
        *e = tab_z80_cb[img_byte(pc + 1)];
        break;
    case AM_PREFIX_ED:
        *e = tab_z80_ed[img_byte(pc + 1)];
        break;
    case AM_PREFIX_DD:
        /* DD CB d op: the opcode follows the displacement */
        *e = tab_z80_dd[img_byte(pc + 1)];
        if (type_of(e) == AM_PREFIX_CB)
            *e = tab_z80_ddcb[img_byte(pc + 3)];
        break;
    case AM_PREFIX_FD:
        *e = tab_z80_fd[img_byte(pc + 1)];
        if (type_of(e) == AM_PREFIX_CB)
            *e = tab_z80_fdcb[img_byte(pc + 3)];
        break;
    default:
        return (pc + 1) & 0xFFFF;
    }
    return (pc + 2) & 0xFFFF;
}

uint32_t lr35902_decode(uint32_t pc, OpEntry *e)
{
    *e = tab_lr35902[img_byte(pc)];
    if (type_of(e) == AM_PREFIX_CB) {
        *e = tab_lr35902_cb[img_byte(pc + 1)];
        return (pc + 2) & 0xFFFF;
    }
    return (pc + 1) & 0xFFFF;
}

static uint32_t rel_target(uint32_t opaddr)
{
    return ((uint32_t)(int8_t)img_byte(opaddr) + opaddr + 1) & 0xFFFF;
}

/* Shared by both CPUs; AM_HIGH only occurs in LR35902 tables */
static void common_pass1(uint32_t pc, uint32_t opaddr, const OpEntry *e)
{
    switch (type_of(e)) {
    case AM_REL:
        sym_auto_label(rel_target(opaddr), 1);
        break;
    case AM_ADDR:
        sym_auto_label(img_word(opaddr), (e->flags & OPF_CODEREF) != 0);
        break;
    case AM_HIGH:
        sym_auto_label(img_byte(opaddr) + 0xFF00, 0);
        break;
    case AM_DATA16:
        sym_auto_label(img_word(opaddr), 0);
        break;
    case AM_RST:
        sym_auto_label(img_byte(pc) & 0x38, 1);
        break;
    default:
        break;
    }
}

void z80_pass1(uint32_t pc, uint32_t opaddr, const OpEntry *e)
{
    common_pass1(pc, opaddr, e);
}

void lr35902_pass1(uint32_t pc, uint32_t opaddr, const OpEntry *e)
{
    common_pass1(pc, opaddr, e);
}

/* Operand text from the table, filled in by type */
static void common_format(uint32_t pc, uint32_t opaddr, OpEntry *e, char *buf, const char **info, int lr35902)
{
    char a[64], b[64];
    if (e->operands != NULL)
        strcpy(buf, e->operands);
    switch (type_of(e)) {
    case AM_INHERENT:
        if (e->flags & OPF_CODEREF)
            *info = "index jump";
        break;
    case AM_REL:
        sym_append_name(buf, rel_target(opaddr), 1);
        break;
    case AM_DISP:
        if (lr35902)
            break;
        /* fall through */
    case AM_IMM8:
    case AM_PORT:
        fmt_byte(a, img_byte(opaddr));
        sprintf(buf, e->operands, a);
        break;
    case AM_ADDR:
        sym_append_name(buf, img_word(opaddr), (e->flags & OPF_CODEREF) != 0);
        break;
    case AM_HIGH:
        if (!lr35902)
            break;
        a[0] = '\0';
        sym_append_name(a, img_byte(opaddr) + 0xFF00, 0);
        sprintf(buf, e->operands, a);
        break;
    case AM_IMM16:
    case AM_DATA16:
        a[0] = '\0';
        sym_append_name(a, img_word(opaddr), 0);
        sprintf(buf, e->operands, a);
        break;
    case AM_DISP_IMM:
        if (lr35902)
            break;
        fmt_byte(a, img_byte(opaddr));
        fmt_byte(b, img_byte(opaddr + 1));
        sprintf(buf, e->operands, a, b);
        break;
    case AM_RST:
        sym_append_name(buf, img_byte(pc) & 0x38, 1);
        break;
    case AM_HIGH_C:
        if (!lr35902)
            break;
        fmt_word(a, 0xFF00);
        sprintf(buf, e->operands, a);
        break;
    default:
        break;
    }
}

void z80_format(uint32_t pc, uint32_t opaddr, OpEntry *e, char *buf, const char **info)
{
    common_format(pc, opaddr, e, buf, info, 0);
}

void lr35902_format(uint32_t pc, uint32_t opaddr, OpEntry *e, char *buf, const char **info)
{
    common_format(pc, opaddr, e, buf, info, 1);
}

/* [0x408ac0] */
void z80_thread(uint32_t pc, uint32_t opaddr, const OpEntry *e)
{
    if (!(e->flags & OPF_CODEREF))
        return;
    switch (type_of(e)) {
    case AM_REL:
        thread_branch(rel_target(opaddr), pc);
        break;
    case AM_ADDR:
        thread_branch(img_word(opaddr), pc);
        break;
    case AM_RST:
        thread_branch(img_byte(pc) & 0x38, pc);
        break;
    default:
        break;
    }
}
