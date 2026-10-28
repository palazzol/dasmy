/*
 * Intel 8048 (MCS-48)
 * decode [0x405960], pass1 [0x4059a0], format [0x405a10], thread [0x408e10]
 *
 * jmp/call addresses are 11 bits (the memory bank select is not tracked,
 * as the DASMx manual warns), and conditional jumps stay within the page.
 * Operands are fetched relative to pc: decode returns pc itself.
 */

#include <string.h>

#include "dasmy.h"
#include "cpu_8048.h"

/* addressing types (flags & OPF_TYPE_MASK) */
enum {
    AM_INHERENT = 1,    /* table text; with OPF_CODEREF this is jmpp @a */
    AM_IMM8 = 6,
    AM_PAGE = 0x30,     /* conditional jump within the current page */
    AM_ADDR11 = 0x31    /* jmp/call */
};

uint32_t i8048_decode(uint32_t pc, OpEntry *e)
{
    *e = tab_8048[img_byte(pc)];
    return pc;
}

static uint32_t page_target(uint32_t pc)
{
    return ((pc + 1) & 0xFF00) | img_byte(pc + 1);
}

static uint32_t addr11_target(uint32_t pc)
{
    return img_byte(pc + 1) | (uint32_t)(img_byte(pc) & 0xE0) << 3;
}

void i8048_pass1(uint32_t pc, uint32_t opaddr, const OpEntry *e)
{
    (void)opaddr;
    switch (e->flags & OPF_TYPE_MASK) {
    case AM_PAGE:
        sym_auto_label(page_target(pc), 1);
        break;
    case AM_ADDR11:
        sym_auto_label(addr11_target(pc), 1);
        break;
    default:
        break;
    }
}

void i8048_format(uint32_t pc, uint32_t opaddr, OpEntry *e, char *buf, const char **info)
{
    char tmp[64];
    (void)opaddr;
    switch (e->flags & OPF_TYPE_MASK) {
    case AM_INHERENT:
        if (e->operands != NULL)
            strcpy(buf, e->operands);
        if (e->flags & OPF_CODEREF)
            *info = "indirect jump";
        break;
    case AM_IMM8:
        tmp[0] = '#';
        fmt_byte(tmp + 1, img_byte(pc + 1));
        sprintf(buf, e->operands, tmp);
        break;
    case AM_PAGE:
        if (e->operands != NULL)
            strcpy(buf, e->operands);
        sym_append_name(buf, page_target(pc), 1);
        break;
    case AM_ADDR11:
        /* No table text for jmp/call */
        sym_append_name(buf, addr11_target(pc), 1);
        break;
    default:
        break;
    }
}

void i8048_thread(uint32_t pc, uint32_t opaddr, const OpEntry *e)
{
    (void)opaddr;
    if (!(e->flags & OPF_CODEREF))
        return;
    switch (e->flags & OPF_TYPE_MASK) {
    case AM_PAGE:
        thread_branch(page_target(pc), pc);
        break;
    case AM_ADDR11:
        thread_branch(addr11_target(pc), pc);
        break;
    default:
        break;
    }
}
