/*
 * Intel 8080 and 8085
 * decode [0x404fb0, 0x405000], pass1 [0x405050], format [0x4050c0], thread [0x4051d0]
 *
 * The table's operand text is copied as is, or used as a printf format for
 * the operand value.
 */

#include <string.h>

#include "dasmy.h"
#include "cpu_8080.h"

/* addressing types (flags & OPF_TYPE_MASK) */
enum {
    AM_INHERENT = 1,
    AM_ADDR = 4,        /* jump/call target */
    AM_IMM8 = 6,
    AM_IMM16 = 7,       /* lxi: shown as a data symbol if there is one */
    AM_PORT = 0x10,
    AM_DATA16 = 0x11,   /* lda/sta/lhld/shld address */
    AM_RST = 0x17       /* target is opcode & 0x38 */
};

static uint32_t decode(const OpEntry *tab, uint32_t pc, OpEntry *e)
{
    *e = tab[img_byte(pc)];
    return (pc + 1) & 0xFFFF;
}

uint32_t i8080_decode(uint32_t pc, OpEntry *e) { return decode(tab_8080, pc, e); }
uint32_t i8085_decode(uint32_t pc, OpEntry *e) { return decode(tab_8085, pc, e); }

void i8080_pass1(uint32_t pc, uint32_t opaddr, const OpEntry *e)
{
    switch (e->flags & OPF_TYPE_MASK) {
    case AM_ADDR:
        sym_auto_label(img_word(opaddr), (e->flags & OPF_CODEREF) != 0);
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

void i8080_format(uint32_t pc, uint32_t opaddr, OpEntry *e, char *buf, const char **info)
{
    char tmp[64];
    (void)pc;
    if (e->operands != NULL)
        strcpy(buf, e->operands);
    switch (e->flags & OPF_TYPE_MASK) {
    case AM_INHERENT:
        if (e->flags & OPF_CODEREF)
            *info = "index jump";
        break;
    case AM_ADDR:
        sym_append_name(buf, img_word(opaddr), (e->flags & OPF_CODEREF) != 0);
        break;
    case AM_IMM8:
    case AM_PORT:
        fmt_byte(tmp, img_byte(opaddr));
        sprintf(buf, e->operands, tmp);
        break;
    case AM_IMM16:
    case AM_DATA16:
        tmp[0] = '\0';
        sym_append_name(tmp, img_word(opaddr), 0);
        sprintf(buf, e->operands, tmp);
        break;
    default:
        break;
    }
}

void i8080_thread(uint32_t pc, uint32_t opaddr, const OpEntry *e)
{
    if (!(e->flags & OPF_CODEREF))
        return;
    switch (e->flags & OPF_TYPE_MASK) {
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
