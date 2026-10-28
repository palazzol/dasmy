/*
 * Motorola 6809
 * decode [0x405b60], pass1 [0x4060e0], format [0x4061f0], thread [0x408e80],
 * register lists [0x405c80, 0x405d20], indexed operands [0x405db0]
 */

#include <string.h>

#include "dasmy.h"
#include "cpu_6809.h"

/* addressing types (flags & OPF_TYPE_MASK) */
enum {
    AM_INHERENT = 1,
    AM_REL8 = 2,
    AM_INDEXED = 3,
    AM_EXTENDED = 4,
    AM_DIRECT = 5,
    AM_IMM8 = 6,
    AM_IMM16 = 7,
    AM_PAGE10 = 8,      /* prefix 0x10: second opcode table */
    AM_PAGE11 = 9,      /* prefix 0x11: third opcode table */
    AM_REGPAIR = 0xA,   /* tfr/exg */
    AM_REL16 = 0xB,
    AM_REGS_S_HI = 0xC, /* register lists, S stack names, listed from bit 7 */
    AM_REGS_S_HI2 = 0xD,
    AM_REGS_U_HI = 0xE, /* U stack names, listed from bit 7 */
    AM_REGS_U_LO = 0xF  /* U stack names, listed from bit 0 */
};

/* [0x43dbd8] tfr/exg register numbers; NULL = invalid */
static const char *const pair_regs[16] = {
    "d", "x", "y", "u", "s", "pc", NULL, NULL, "a", "b", "cc", "dp", NULL, NULL, NULL, NULL
};
/* [0x43dc18, 0x43dc38] push/pull register bits */
static const char *const s_regs[8] = { "cc", "a", "b", "dp", "x", "y", "s", "pc" };
static const char *const u_regs[8] = { "cc", "a", "b", "dp", "x", "y", "u", "pc" };
/* [0x43dc58] index registers */
static const char *const index_regs[4] = { "x", "y", "u", "s" };

uint32_t m6809_decode(uint32_t pc, OpEntry *e)
{
    uint32_t opaddr;
    *e = tab_6809[img_byte(pc)];
    if ((e->flags & OPF_TYPE_MASK) == AM_PAGE10) {
        *e = tab_6809_p10[img_byte(pc + 1)];
        opaddr = pc + 2;
    } else if ((e->flags & OPF_TYPE_MASK) == AM_PAGE11) {
        *e = tab_6809_p11[img_byte(pc + 1)];
        opaddr = pc + 2;
    } else {
        opaddr = pc + 1;
    }
    if ((e->flags & OPF_TYPE_MASK) == AM_INDEXED) {
        int extra = m6809_index_extra[img_byte(opaddr)];
        if (extra == -1)
            e->flags = 0;
        else
            e->length += (uint32_t)extra;
    } else if ((e->flags & OPF_TYPE_MASK) == AM_REGPAIR) {
        uint8_t pb = img_byte(opaddr);
        if (pair_regs[pb >> 4] == NULL || pair_regs[pb & 0xF] == NULL)
            e->flags = 0;
    }
    return opaddr & 0xFFFF;
}

void m6809_pass1(uint32_t pc, uint32_t opaddr, const OpEntry *e)
{
    int code = (e->flags & OPF_CODEREF) != 0;
    uint8_t pb;
    (void)pc;
    switch (e->flags & OPF_TYPE_MASK) {
    case AM_REL8:
        if (code)
            sym_auto_label(((uint32_t)(int8_t)img_byte(opaddr) + 1 + opaddr) & 0xFFFF, 1);
        break;
    case AM_INDEXED:
        pb = img_byte(opaddr);
        if ((pb & 0x80) && (pb & 0xF) == 0xF)
            sym_auto_label(img_word(opaddr + 1), 0);
        break;
    case AM_EXTENDED:
        sym_auto_label(img_word(opaddr), code);
        break;
    case AM_DIRECT:
        sym_auto_label(img_byte(opaddr), code);
        break;
    case AM_REL16:
        if (code)
            sym_auto_label(((uint32_t)img_sword(opaddr) + 2 + opaddr) & 0xFFFF, 1);
        break;
    default:
        break;
    }
}

/* [0x405c80] registers named by the set bits, from bit 7 down */
static void reg_list_hi(char *buf, uint8_t bits, const char *const *names)
{
    int first = 1;
    for (int i = 7; i >= 0; i--) {
        if (bits & (1u << i)) {
            if (!first)
                strcat(buf, ", ");
            first = 0;
            strcat(buf, names[i]);
        }
    }
}

/* [0x405d20] registers named by the set bits, from bit 0 up */
static void reg_list_lo(char *buf, uint8_t bits, const char *const *names)
{
    int first = 1;
    for (int i = 0; i < 8; i++) {
        if (bits & (1u << i)) {
            if (!first)
                strcat(buf, ", ");
            first = 0;
            strcat(buf, names[i]);
        }
    }
}

/* [0x405db0] indexed addressing post-byte at pb_addr */
static void index_operand(const OpEntry *e, char *buf, uint32_t pb_addr)
{
    uint8_t pb = img_byte(pb_addr);
    const char *reg = index_regs[(pb >> 5) & 3];
    int pc_relative = 0;
    int32_t offset = 0;
    uint32_t target = 0;
    char *p = buf;

    if (!(pb & 0x80)) {
        /* 5-bit signed offset */
        int off = pb & 0x1F;
        if (pb & 0x10)
            off -= 0x20;
        sprintf(buf, "%d,%s", off, reg);
        return;
    }
    if (pb & 0x10) {
        strcat(buf, "[");
        p = buf + 1;
    }
    switch (pb & 0xF) {
    case 0x0: sprintf(p, ",%s+", reg); break;
    case 0x1: sprintf(p, ",%s++", reg); break;
    case 0x2: sprintf(p, ",-%s", reg); break;
    case 0x3: sprintf(p, ",--%s", reg); break;
    case 0x4: sprintf(p, ",%s", reg); break;
    case 0x5: sprintf(p, "b,%s", reg); break;
    case 0x6: sprintf(p, "a,%s", reg); break;
    case 0x8: sprintf(p, "%d,%s", (int8_t)img_byte(pb_addr + 1), reg); break;
    case 0x9: sprintf(p, "%d,%s", (int)img_sword(pb_addr + 1), reg); break;
    case 0xB: sprintf(p, "d,%s", reg); break;
    case 0xC:
        offset = (int8_t)img_byte(pb_addr + 1);
        target = ((uint32_t)offset + 2 + pb_addr) & 0xFFFF;
        sprintf(p, "%d,pc", (int)offset);
        pc_relative = 1;
        break;
    case 0xD:
        offset = img_sword(pb_addr + 1);
        target = ((uint32_t)offset + 3 + pb_addr) & 0xFFFF;
        sprintf(p, "%d,pc", (int)offset);
        pc_relative = 1;
        break;
    case 0xF:
        sym_append_name(p, img_word(pb_addr + 1), (e->flags & OPF_CODEREF) != 0);
        break;
    default:
        break;
    }
    if (pb & 0x10)
        strcat(buf, "]");
    if (pc_relative) {
        /* The resolved target follows as a comment */
        strcat(buf, "\t");
        strcat(buf, "\t");
        strcat(buf, ";(");
        sym_append_name(buf, target, (e->flags & OPF_CODEREF) != 0);
        strcat(buf, ")");
    }
}

void m6809_format(uint32_t pc, uint32_t opaddr, OpEntry *e, char *buf, const char **info)
{
    int code = (e->flags & OPF_CODEREF) != 0;
    uint8_t pb;
    (void)pc;
    switch (e->flags & OPF_TYPE_MASK) {
    case AM_REL8:
        sym_append_name(buf, ((uint32_t)(int8_t)img_byte(opaddr) + 1 + opaddr) & 0xFFFF, 1);
        break;
    case AM_INDEXED:
        index_operand(e, buf, opaddr);
        if (code)
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
    case AM_REGPAIR:
        pb = img_byte(opaddr);
        sprintf(buf, "%s,%s", pair_regs[pb >> 4], pair_regs[pb & 0xF]);
        break;
    case AM_REL16:
        sym_append_name(buf, ((uint32_t)img_sword(opaddr) + 2 + opaddr) & 0xFFFF, 1);
        break;
    case AM_REGS_S_HI:
    case AM_REGS_S_HI2:
        reg_list_hi(buf, img_byte(opaddr), s_regs);
        break;
    case AM_REGS_U_HI:
        reg_list_hi(buf, img_byte(opaddr), u_regs);
        break;
    case AM_REGS_U_LO:
        reg_list_lo(buf, img_byte(opaddr), u_regs);
        break;
    default:
        break;
    }
}

void m6809_thread(uint32_t pc, uint32_t opaddr, const OpEntry *e)
{
    if (!(e->flags & OPF_CODEREF))
        return;
    switch (e->flags & OPF_TYPE_MASK) {
    case AM_REL8:
        thread_branch(((uint32_t)(int8_t)img_byte(opaddr) + opaddr + 1) & 0xFFFF, pc);
        break;
    case AM_REL16:
        thread_branch(((uint32_t)img_sword(opaddr) + opaddr + 2) & 0xFFFF, pc);
        break;
    case AM_DIRECT:
        thread_branch(img_byte(opaddr), pc);
        break;
    case AM_EXTENDED:
        thread_branch(img_word(opaddr), pc);
        break;
    case AM_INDEXED:
        printf("Ignoring index branch opcode %02X at address %04X\n", img_byte(pc), pc);
        break;
    default:
        break;
    }
}
