/*
 * Microchip PIC16F83 / PIC16F84 (PIC16CXXX family)
 * decode [0x404940, 0x4048a0], pass1 [0x404970], format [0x4049c0], thread [0x404bc0],
 * register names [0x4047c0, 0x404840]
 *
 * The image is made of 16-bit little-endian units, each holding one 14-bit
 * instruction; addresses count units.  Operands are fields of the opcode.
 */

#include <string.h>

#include "dasmy.h"
#include "cpu_pic16.h"

/* addressing types (flags & OPF_TYPE_MASK) */
enum {
    AM_INHERENT = 1,
    AM_ADDR11 = 5,      /* goto/call */
    AM_GROUP = 0x70,    /* opcode 0x0000-0x007F: second table */
    AM_FILE = 0x71,     /* file register in bits 0-6 */
    AM_LITERAL = 0x72,
    AM_SKIP = 0x73,     /* btfsc/btfss/decfsz/incfsz: may skip the next word */
    AM_TRIS = 0x74      /* tris: register in bits 0-2 */
};

uint32_t pic16_decode(uint32_t pc, OpEntry *e)
{
    uint32_t op = img_unit(pc);
    if (op & 0xFFFFC000u)
        fatal("source image incorrect, non 14-bit opcode found");
    *e = tab_pic16[op >> 7];
    if (e->flags == AM_GROUP) {
        *e = tab_pic16_00[op & 0x7F];
    } else if (op == 0x0082 || op == 0x0782) {
        /* movwf pcl / addwf pcl,f: a computed jump */
        e->flags |= OPF_BREAK | OPF_CODEREF;
    }
    return (pc + 1) & 0xFFFF;
}

/* [0x4047c0] core register name, or the number (static buffer) */
static const char *reg_name(uint32_t reg)
{
    static char buf[32];
    switch (reg) {
    case 0x00: return "indf";
    case 0x02: return "pcl";
    case 0x03: return "status";
    case 0x04: return "fsr";
    case 0x0A: return "pclath";
    case 0x0B: return "intcon";
    default:
        fmt_byte(buf, reg);
        return buf;
    }
}

/* [0x404840] registers whose name depends on the bank */
static const char *bank_reg_name(uint32_t reg)
{
    switch (reg) {
    case 0x01: return "tmr0/option_reg";
    case 0x05: return "porta/trisa";
    case 0x06: return "portb/trisb";
    case 0x08: return "eedata/eecon1";
    case 0x09: return "eeadr/eecon2";
    default:   return NULL;
    }
}

static const char *bank_info(uint32_t reg)
{
    static char buf[64];
    const char *name = bank_reg_name(reg);
    if (name == NULL)
        return NULL;
    sprintf(buf, "register %s (bank 0/1)", name);
    return buf;
}

void pic16_pass1(uint32_t pc, uint32_t opaddr, const OpEntry *e)
{
    (void)opaddr;
    switch (e->flags & OPF_TYPE_MASK) {
    case AM_ADDR11:
        sym_auto_label(img_unit(pc) & 0x7FF, 1);
        break;
    case AM_SKIP:
        sym_auto_label(pc + 1, 1);
        break;
    default:
        break;
    }
}

void pic16_format(uint32_t pc, uint32_t opaddr, OpEntry *e, char *buf, const char **info)
{
    uint32_t op = img_unit(pc);
    uint32_t reg;
    const char *s;
    (void)opaddr;
    switch (e->flags & OPF_TYPE_MASK) {
    case AM_FILE:
        reg = op & 0x7F;
        sprintf(buf, e->operands, reg_name(reg));
        if (e->flags & OPF_CODEREF) {
            *info = "index jump";
            break;
        }
        if ((s = bank_info(reg)) != NULL) {
            *info = s;
            break;
        }
        /* Well-known uses of the status register */
        switch (op) {
        case 0x1003: *info = "clear carry"; break;
        case 0x1403: *info = "set carry"; break;
        case 0x1283: *info = "select bank 0"; break;
        case 0x1683: *info = "select bank 1"; break;
        default: break;
        }
        break;
    case AM_SKIP:
    case AM_TRIS:
        reg = op & ((e->flags & OPF_TYPE_MASK) == AM_SKIP ? 0x7F : 0x07);
        sprintf(buf, e->operands, reg_name(reg));
        if ((s = bank_info(reg)) != NULL)
            *info = s;
        break;
    case AM_ADDR11:
        sym_append_name(buf, op & 0x7FF, 1);
        break;
    case AM_LITERAL:
        fmt_byte(buf, op & 0xFF);
        break;
    default:
        break;
    }
}

void pic16_thread(uint32_t pc, uint32_t opaddr, const OpEntry *e)
{
    (void)opaddr;
    if (!(e->flags & OPF_CODEREF))
        return;
    switch (e->flags & OPF_TYPE_MASK) {
    case AM_ADDR11:
        thread_branch(img_unit(pc) & 0x7FF, pc);
        break;
    case AM_SKIP:
        thread_branch(pc + 1, pc);
        break;
    default:
        break;
    }
}
