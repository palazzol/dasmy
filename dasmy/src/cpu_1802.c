/*
 * RCA CDP1802 COSMAC and CDP1805/1806
 * decode [0x407e60, 0x407ec0], pass1 [0x407f50], format [0x408050], thread [0x408230]
 *
 * Opcode 0x68 (type AM_PREFIX) is invalid on the 1802 and selects the
 * extended instruction table on the 1805/1806.
 */

#include <string.h>

#include "dasmy.h"
#include "cpu_1802.h"

/* addressing types (flags & OPF_TYPE_MASK) */
enum {
    AM_INHERENT = 1,
    AM_LONG = 4,        /* long branch: 16-bit target */
    AM_SHORT = 5,       /* short branch: target in the current page */
    AM_IMM8 = 6,
    AM_SKIP1 = 0x60,    /* skips 1 byte */
    AM_SKIP2 = 0x61,    /* skips 2 bytes */
    AM_PREFIX = 0x62
};

uint32_t r1802_decode(uint32_t pc, OpEntry *e)
{
    *e = tab_1802[img_byte(pc)];
    if ((e->flags & OPF_TYPE_MASK) == AM_PREFIX)
        e->flags = 0;
    return (pc + 1) & 0xFFFF;
}

uint32_t r1805_decode(uint32_t pc, OpEntry *e)
{
    *e = tab_1802[img_byte(pc)];
    if ((e->flags & OPF_TYPE_MASK) == AM_PREFIX) {
        *e = tab_1805_68[img_byte(pc + 1)];
        return (pc + 2) & 0xFFFF;
    }
    return (pc + 1) & 0xFFFF;
}

static uint32_t short_target(uint32_t pc, uint32_t opaddr)
{
    return ((pc + 1) & 0xFF00) | img_byte(opaddr);
}

void r1802_pass1(uint32_t pc, uint32_t opaddr, const OpEntry *e)
{
    switch (e->flags & OPF_TYPE_MASK) {
    case AM_LONG:
        sym_auto_label(img_word(opaddr), 1);
        break;
    case AM_SHORT:
        sym_auto_label(short_target(pc, opaddr), 1);
        break;
    case AM_SKIP1:
        sym_auto_label(pc + 2, 1);
        break;
    case AM_SKIP2:
        sym_auto_label(pc + 3, 1);
        break;
    default:
        break;
    }
}

void r1802_format(uint32_t pc, uint32_t opaddr, OpEntry *e, char *buf, const char **info)
{
    static char skip_info[128];
    switch (e->flags & OPF_TYPE_MASK) {
    case AM_INHERENT:
        if (e->operands != NULL)
            strcpy(buf, e->operands);
        break;
    case AM_LONG:
        sym_append_name(buf, img_word(opaddr), 1);
        break;
    case AM_SHORT:
        sym_append_name(buf, short_target(pc, opaddr), 1);
        break;
    case AM_IMM8:
        fmt_byte(buf, img_byte(opaddr));
        break;
    case AM_SKIP1:
    case AM_SKIP2:
        strcpy(skip_info, "skip to address ");
        sym_append_name(skip_info, pc + ((e->flags & OPF_TYPE_MASK) == AM_SKIP1 ? 2 : 3), 1);
        *info = skip_info;
        break;
    default:
        break;
    }
}

void r1802_thread(uint32_t pc, uint32_t opaddr, const OpEntry *e)
{
    if (!(e->flags & OPF_CODEREF))
        return;
    switch (e->flags & OPF_TYPE_MASK) {
    case AM_LONG:
        thread_branch(img_word(opaddr), pc);
        break;
    case AM_SHORT:
        thread_branch(short_target(pc, opaddr), pc);
        break;
    case AM_SKIP1:
        thread_branch(pc + 2, pc);
        break;
    case AM_SKIP2:
        thread_branch(pc + 3, pc);
        break;
    default:
        break;
    }
}
