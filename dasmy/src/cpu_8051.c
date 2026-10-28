/*
 * Intel 8051 (MCS-51)
 * decode [0x405230], pass1 [0x405280], format [0x4054a0], thread [0x408cd0],
 * name lookups [0x405450, 0x405470]
 *
 * Code addresses in instructions are big endian.  Direct addresses refer to
 * the separate internal data space: they are shown as SFR names where there
 * is one, and data labels are not printed in the code listing.
 */

#include <string.h>

#include "dasmy.h"
#include "cpu_8051.h"

/* addressing types (flags & OPF_TYPE_MASK) */
enum {
    AM_INHERENT = 1,    /* table text; with OPF_CODEREF this is jmp @a+dptr */
    AM_REL = 2,
    AM_ADDR16 = 4,      /* ljmp/lcall */
    AM_DIRECT = 5,
    AM_IMM8 = 6,
    AM_IMM16 = 7,
    AM_ADDR11 = 0x38,   /* ajmp/acall */
    AM_DIRECT_IMM = 0x41,
    AM_BIT = 0x48,
    AM_BIT_REL = 0x49,
    AM_IMM_REL = 0x4A,
    AM_DIRECT_REL = 0x4B,
    AM_DIRECT_DIRECT = 0x4C
};

uint32_t i8051_decode(uint32_t pc, OpEntry *e)
{
    *e = tab_8051[img_byte(pc)];
    return (pc + 1) & 0xFFFF;
}

/* [0x405450] bit address: bit name or byte value (static buffer) */
static const char *bit_name(uint8_t bit)
{
    static char buf[32];
    if (m8051_bit_names[bit] != NULL)
        return m8051_bit_names[bit];
    fmt_byte(buf, bit);
    return buf;
}

/* [0x405470] direct address: SFR name, or data label / value (static buffer) */
static const char *direct_name(uint8_t addr)
{
    static char buf[128];
    if (m8051_sfr_names[addr] != NULL)
        return m8051_sfr_names[addr];
    buf[0] = '\0';
    sym_append_name(buf, addr, 0);
    return buf;
}

static void label_direct(uint8_t addr)
{
    if (m8051_sfr_names[addr] == NULL)
        sym_auto_label(addr, 0);
}

static uint32_t rel_target(uint32_t at, uint32_t base)
{
    return ((uint32_t)(int8_t)img_byte(at) + base) & 0xFFFF;
}

static uint32_t addr11_target(uint32_t pc, uint32_t opaddr)
{
    return (uint32_t)(img_byte(pc) & 0xE0) << 3 | img_byte(opaddr) | ((opaddr + 1) & 0xF800);
}

void i8051_pass1(uint32_t pc, uint32_t opaddr, const OpEntry *e)
{
    switch (e->flags & OPF_TYPE_MASK) {
    case AM_REL:
        sym_auto_label(rel_target(opaddr, opaddr + 1), 1);
        break;
    case AM_ADDR16:
        sym_auto_label(img_word_be(opaddr), 1);
        break;
    case AM_DIRECT:
    case AM_DIRECT_IMM:
        label_direct(img_byte(opaddr));
        break;
    case AM_ADDR11:
        sym_auto_label(addr11_target(pc, opaddr), 1);
        break;
    case AM_BIT_REL:
    case AM_IMM_REL:
        sym_auto_label(rel_target(opaddr + 1, opaddr + 2), 1);
        break;
    case AM_DIRECT_REL:
        label_direct(img_byte(opaddr));
        sym_auto_label(rel_target(opaddr + 1, opaddr + 2), 1);
        break;
    case AM_DIRECT_DIRECT:
        label_direct(img_byte(opaddr));
        label_direct(img_byte(opaddr + 1));
        break;
    default:
        break;
    }
}

void i8051_format(uint32_t pc, uint32_t opaddr, OpEntry *e, char *buf, const char **info)
{
    char tmp[64];
    switch (e->flags & OPF_TYPE_MASK) {
    case AM_INHERENT:
        if (e->operands != NULL)
            strcpy(buf, e->operands);
        if (e->flags & OPF_CODEREF)
            *info = "indirect jump";
        break;
    case AM_REL:
        if (e->operands != NULL)
            strcpy(buf, e->operands);
        sym_append_name(buf, rel_target(opaddr, opaddr + 1), 1);
        break;
    case AM_ADDR16:
        sym_append_name(buf, img_word_be(opaddr), 1);
        break;
    case AM_DIRECT:
        sprintf(buf, e->operands, direct_name(img_byte(opaddr)));
        break;
    case AM_IMM8:
        tmp[0] = '#';
        fmt_byte(tmp + 1, img_byte(opaddr));
        sprintf(buf, e->operands, tmp);
        break;
    case AM_IMM16:
        tmp[0] = '#';
        fmt_word(tmp + 1, img_word_be(opaddr));
        sprintf(buf, e->operands, tmp);
        break;
    case AM_ADDR11:
        sym_append_name(buf, (uint32_t)(img_byte(pc) & 0xE0) << 3 | img_byte(opaddr) | ((pc + 2) & 0xF800), 1);
        break;
    case AM_DIRECT_IMM:
        fmt_byte(tmp, img_byte(opaddr + 1));
        sprintf(buf, "%s,#%s", direct_name(img_byte(opaddr)), tmp);
        break;
    case AM_BIT:
        sprintf(buf, e->operands, bit_name(img_byte(opaddr)));
        break;
    case AM_BIT_REL:
        sprintf(buf, e->operands, bit_name(img_byte(opaddr)));
        sym_append_name(buf, rel_target(opaddr + 1, opaddr + 2), 1);
        break;
    case AM_IMM_REL:
        tmp[0] = '#';
        fmt_byte(tmp + 1, img_byte(opaddr));
        sprintf(buf, e->operands, tmp);
        sym_append_name(buf, rel_target(opaddr + 1, opaddr + 2), 1);
        break;
    case AM_DIRECT_REL:
        if (e->operands != NULL)
            strcpy(buf, e->operands);
        strcat(buf, direct_name(img_byte(opaddr)));
        strcat(buf, ",");
        sym_append_name(buf, rel_target(opaddr + 1, opaddr + 2), 1);
        break;
    case AM_DIRECT_DIRECT:
        /* mov dest,src is encoded as src, dest */
        strcpy(buf, direct_name(img_byte(opaddr + 1)));
        strcat(buf, ",");
        strcat(buf, direct_name(img_byte(opaddr)));
        break;
    default:
        break;
    }
}

void i8051_thread(uint32_t pc, uint32_t opaddr, const OpEntry *e)
{
    if (!(e->flags & OPF_CODEREF))
        return;
    switch (e->flags & OPF_TYPE_MASK) {
    case AM_REL:
        thread_branch(rel_target(opaddr, opaddr + 1), pc);
        break;
    case AM_ADDR16:
        thread_branch(img_word_be(opaddr), pc);
        break;
    case AM_ADDR11:
        thread_branch(addr11_target(pc, opaddr), pc);
        break;
    case AM_BIT_REL:
    case AM_IMM_REL:
    case AM_DIRECT_REL:
        thread_branch(rel_target(opaddr + 1, opaddr + 2), pc);
        break;
    default:
        break;
    }
}
