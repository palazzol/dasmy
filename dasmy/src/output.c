/* Listing and assembler output, passes 1 and 2 [0x401190..0x402890, 0x401930..0x401de0] */

#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "dasmy.h"

/* [0x43d0d4] data pseudo-op, indexed by unit size */
static const char *const db_ops[] = { NULL, "db", "dw", "dd", "dd" };

/* Sequence break state [0x43ebc8, 0x43ebc4] */
static int last_was_code;
static int last_was_break;

/* Column where the source part of a line starts */
static int base_col(void)
{
    if (opt_asm)
        return 0;
    return opt_wide ? 56 : 32;
}

/* [0x4022e0] pad from column col to target with tabs then spaces */
int out_pad(int col, int target)
{
    int tab_stop = target & ~7;
    if (col < tab_stop) {
        int tabs = ((tab_stop - col - 1) >> 3) + 1;
        while (tabs--)
            fputc('\t', out_file);
        col = tab_stop;
    }
    while (col < target) {
        fputc(' ', out_file);
        col++;
    }
    return target;
}

/* [0x402330] printf, indented to the source column */
void out_tab_printf(const char *fmt, ...)
{
    va_list ap;
    out_pad(0, base_col());
    va_start(ap, fmt);
    vfprintf(out_file, fmt, ap);
    va_end(ap);
}

/* [0x402370] */
void out_comment(void)
{
    out_tab_printf("%s\n", ";");
}

/* [0x401190] */
void out_header(void)
{
    time_t now;
    time(&now);
    out_comment();
    out_tab_printf("%s\tDisassembled by:\n", ";");
    out_tab_printf("%s\t\t%s\n", ";", DASMY_NAME);
    out_tab_printf("%s\t\t%s\n", ";", DASMY_CREDIT);
    out_tab_printf("%s\t\tVersion %d.%02d (%s)\n", ";", DASMY_VER_MAJOR, DASMY_VER_MINOR, DASMY_DATE);
    out_comment();
    out_tab_printf("%s\tFile:\t\t%s\n", ";", src_name);
    out_comment();
    out_tab_printf("%s\tSize:\t\t%d bytes\n", ";", (int)src_len);
    out_tab_printf("%s\tChecksum:\t%04X\n", ";", img_checksum() & 0xFFFF);
    out_tab_printf("%s\tCRC-32:\t\t%08X\n", ";", img_crc32());
    out_comment();
    out_tab_printf("%s\tDate:\t\t%s", ";", ctime(&now));
    out_comment();
    out_tab_printf("%s\tCPU:\t\t%s %s (%s family)\n", ";", cpu->manufacturer, cpu->name, cpu->family);
    out_comment();
}

/* [0x402390] */
static void out_org(uint32_t addr)
{
    char buf[64];
    out_comment();
    out_comment();
    if (cpu->addr_max < 0x10000)
        fmt_word(buf, addr);
    else
        fmt_dword(buf, addr);
    out_tab_printf("\t%s\t%s\n", "org", buf);
}

/* [0x402400] separator comment between code and data, and after breaks */
static void seq_break(uint32_t flags)
{
    int is_code = (flags & 0xFF) != 0;
    if (is_code != last_was_code || (last_was_break && !(flags & OPF_BREAK))) {
        last_was_code = is_code;
        out_comment();
    }
    last_was_break = (flags >> 8) & 1;
}

/* [0x402450] one instruction or data line, plus continuation lines */
void out_line(uint32_t addr, const char *mn, const char *ops, uint32_t flags,
              uint32_t len, uint32_t cycles, const char *info)
{
    char buf[64];
    uint32_t per_line = (uint32_t)bytes_per_line();
    uint32_t first = len < per_line ? len : per_line;
    int hex_col = opt_wide ? 36 : 24;
    int base = base_col();
    int col = 0;

    if (!opt_asm) {
        col = fprintf(out_file, cpu->addr_max < 0x10000 ? "%04X" : "%08X", addr);
        fputs(" : ", out_file);
        int n = img_hex(buf, addr, first);
        fputs(buf, out_file);
        col = out_pad(col + 3 + n, hex_col);
        fputc('"', out_file);
        col += img_ascii(buf, addr, first);
        fputs(buf, out_file);
        fputc('"', out_file);
        col += 2;
        if (opt_wide && (cycles & 0xFF) != 0) {
            col = out_pad(col, 48);
            col += fprintf(out_file, "[%d", cycles & 0xFF);
            if (((cycles >> 8) & 0xFF) != 0) {
                col += fprintf(out_file, "/%d", ((cycles >> 8) & 0xFF) + (cycles & 0xFF));
            } else if (flags & OPF_CYCPLUS) {
                fputc('+', out_file);
                col++;
            }
            fputc(']', out_file);
            col++;
        }
    }
    col = out_pad(col, base + 8);
    fputs(mn, out_file);
    col += (int)strlen(mn);
    if (ops != NULL && *ops != '\0') {
        col = out_pad(col, base + 16);
        fputs(ops, out_file);
        col += (int)strlen(ops);
    }
    if (info != NULL) {
        out_pad(col, base + 64);
        fprintf(out_file, "%sINFO: %s", ";", info);
    }
    fputc('\n', out_file);

    if (opt_asm)
        return;
    for (uint32_t i = first; i < len; i += per_line) {
        int c = out_pad(0, cpu->addr_max > 0xFFFF ? 11 : 7);
        uint32_t n = len - i < per_line ? len - i : per_line;
        c += img_hex(buf, addr + i, n);
        fputs(buf, out_file);
        out_pad(c, hex_col);
        fputc('"', out_file);
        img_ascii(buf, addr + i, n);
        fputs(buf, out_file);
        fputs("\"\n", out_file);
    }
}

/* [0x402890] string data, with \xNN escapes for non-printables */
static void quote_string(char *buf, uint32_t addr, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        uint8_t c = img_byte(addr + i);
        if (c >= 0x20 && c < 0x7F) {
            *buf++ = (char)c;
        } else {
            sprintf(buf, "\\x%02X", c);
            buf += 4;
        }
    }
    *buf = '\0';
}

/* [0x408a30] untyped data: up to a line's worth of units */
static uint32_t format_db(char *buf, uint32_t addr)
{
    uint32_t per_line = (uint32_t)bytes_per_line();
    uint32_t n = 1;
    for (uint32_t a = addr + 1; n < per_line; a++, n++) {
        int t = mem_get(a);
        if (t == MEM_NONE || t == MEM_CODE || sym_find(a, SYM_DATA) != NULL)
            break;
    }
    img_numfmt(buf, addr, n);
    return n;
}

/* Does a code-defining label fall inside the instruction at pc? */
static int label_inside(uint32_t pc, uint32_t len)
{
    for (uint32_t i = 1; i < len; i++) {
        Symbol *s = sym_find(pc + i, SYM_CODE);
        if (s != NULL && s->kind == MEM_CODE)
            return 1;
    }
    return 0;
}

/* [0x401d00] collect labels; reject instructions that don't fit */
void pass1(uint32_t pc, uint32_t units)
{
    uint32_t end = pc + units;
    OpEntry e;
    while (pc < end) {
        if (mem_get(pc) == MEM_CODE) {
            uint32_t opaddr = cpu->decode(pc, &e);
            if (e.flags != 0) {
                int ok = 1;
                for (uint32_t i = 1; i < e.length; i++) {
                    if (mem_get(pc + i) != MEM_CODE) {
                        ok = 0;
                        break;
                    }
                }
                if (ok && !label_inside(pc, e.length)) {
                    cpu->pass1(pc, opaddr, &e);
                    pc += e.length;
                    continue;
                }
            }
            mem_set(pc, MEM_BADCODE);
        }
        pc++;
    }
}

/* [0x401de0] write the disassembly */
void pass2(uint32_t pc, uint32_t units)
{
    uint32_t end = pc + units;
    int need_org = 1;
    OpEntry e = { 0 };
    uint32_t opaddr = 0;
    char buf[1100];
    char str[1028];

    while (pc < end) {
        if (mem_get(pc) == MEM_NONE) {
            pc++;
            need_org = 1;
            continue;
        }
        if (need_org) {
            out_org(pc);
            need_org = 0;
        }
        if (mem_get(pc) == MEM_CODE) {
            opaddr = cpu->decode(pc, &e);
            if (label_inside(pc, e.length)) {
                e.flags = 0;
                e.length = 1;
            }
        } else {
            e.flags = 0;
            e.length = 1;
        }

        Symbol *data = sym_find(pc, SYM_DATA);
        if (data != NULL && !(data->kind > 3 && data->kind < 8))
            data = NULL;
        if (data != NULL) {
            e.flags = 0;
            e.length = data->length;
        }

        seq_break(e.flags);
        sym_print_labels(pc, SYM_CODE);
        if (cpu->addr2_bits == 0)
            sym_print_labels(pc, SYM_DATA);

        buf[0] = '\0';
        const char *info = NULL;
        if (data != NULL) {
            uint32_t i;
            switch (data->kind) {
            case MEM_BYTE:
                for (i = 0; i < data->length; i++) {
                    fmt_byte(buf, img_byte(pc + i));
                    out_line(pc + i, "db", buf, 0, 1, 0, info);
                }
                break;
            case MEM_WORD:
                for (i = 0; i < data->length; i += 2) {
                    buf[0] = '\0';
                    sym_append_any(buf, img_word(pc + i));
                    out_line(pc + i, "dw", buf, 0, 2, 0, info);
                }
                break;
            case MEM_DWORD:
                for (i = 0; i < data->length; i += 4) {
                    buf[0] = '\0';
                    sym_append_any_dword(buf, img_dword(pc + i));
                    out_line(pc + i, "dd", buf, 0, 4, 0, info);
                }
                break;
            case MEM_STRING:
                quote_string(str, pc, e.length > 0xFF ? 0x100 : e.length);
                sprintf(buf, "\"%s\"", str);
                out_line(pc, "db", buf, 0, e.length, 0, info);
                break;
            }
        } else if (e.flags == 0) {
            e.length = format_db(buf, pc);
            out_line(pc, db_ops[img_unit_bytes()], buf, 0, e.length, 0, info);
        } else {
            const char *mn, *ops;
            cpu->format(pc, opaddr, &e, buf, &info);
            if (e.mnemonic != NULL) {
                mn = e.mnemonic;
                ops = buf;
            } else {
                mn = buf;
                ops = NULL;
            }
            out_line(pc, mn, ops, e.flags, e.length, e.cycles, info);
        }
        pc += e.length;
    }
}

/* [0x401930] */
void print_stats(uint32_t start, uint32_t len)
{
    uint32_t ub = img_unit_bytes();
    uint32_t units = len / ub;
    int skipped = 0, code = 0, data = 0;
    for (uint32_t a = start; a < start + units; a++) {
        switch (mem_get(a)) {
        case MEM_NONE: skipped++; break;
        case MEM_CODE: code++; break;
        case MEM_BYTE: case MEM_WORD: case MEM_DWORD: case MEM_STRING: data++; break;
        default: break;
        }
    }
    uint32_t total = units - (uint32_t)skipped;
    uint32_t code_pct = total ? (uint32_t)(code * 100) / total : 0;
    uint32_t data_pct = total ? (uint32_t)(data * 100) / total : 0;
    printf("Total code bytes:\t%d (%d%%)\n", (int)(code * ub), (int)code_pct);
    printf("Total data bytes:\t%d (%d%%)\n", (int)(data * ub), (int)data_pct);
    printf("Total unknown bytes:\t%d (%d%%)\n", (int)(len - (uint32_t)(data + code) * ub),
           (int)((100 - data_pct) - code_pct));
    printf("Total skipped bytes:\t%d\n", (int)(skipped * ub));
}
