/*
 * Symbol (control) file parsing [0x402c70..0x402e90, 0x403790..0x404030]
 *
 * The file is read twice: pass A handles cpu and org (which are needed
 * before the image is loaded), pass B handles everything else.
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "dasmy.h"

#define MAX_TOKENS 20

enum {
    CMD_ORG, CMD_CPU, CMD_CODE, CMD_VECTOR, CMD_VECTAB, CMD_ADDRTAB, CMD_BYTE, CMD_WORD,
    CMD_DWORD, CMD_STRING, CMD_SYMBOL, CMD_COMMENT, CMD_SKIP, CMD_INCLUDE, CMD_MESSAGE,
    CMD_NUMFORMAT
};

/* [0x4128c0] */
static const char *const keywords[] = {
    "org", "cpu", "code", "vector", "vectab", "addrtab", "byte", "word",
    "dword", "string", "symbol", "comment", "skip", "include", "message", "numformat"
};

/* MSVC isspace() in the C locale */
static int is_space(char c)
{
    return c == ' ' || (c >= '\t' && c <= '\r');
}

static int ascii_stricmp(const char *a, const char *b)
{
    for (;; a++, b++) {
        int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
        if (ca != cb || ca == 0)
            return ca - cb;
    }
}

/* [0x4117c0] strip leading and trailing whitespace in place */
static char *trim(char *p)
{
    while (is_space(*p))
        p++;
    size_t n = strlen(p);
    while (n != 0 && is_space(p[n - 1]))
        n--;
    p[n] = '\0';
    return p;
}

/* [0x411850] split into whitespace-separated tokens; "..." groups a token */
static int tokenize(char *p, char **argv, int max)
{
    int n = 0, in_token = 0, in_quote = 0;
    char c = *p;
    for (;;) {
        if (c == '\0' || n >= max)
            return n;
        if (!is_space(*p) || in_quote) {
            if (*p == '"') {
                in_quote = !in_quote;
                if (in_quote) {
                    p++;
                    if (*p != '"')
                        goto start;
                    in_quote = 0;
                }
                *p = '\0';
            }
start:
            if (!in_token) {
                argv[n++] = p;
                in_token = 1;
            }
        } else {
            in_token = 0;
            *p = '\0';
        }
        c = p[1];
        p++;
    }
}

/* [0x4118f0] */
static int lookup_keyword(const char *word)
{
    for (int i = 0; i < (int)(sizeof keywords / sizeof keywords[0]); i++) {
        if (ascii_stricmp(word, keywords[i]) == 0)
            return i;
    }
    return -1;
}

/* Parse a number using C conventions, as sscanf("%i") */
static int parse_num(const char *s, uint32_t *v)
{
    int n;
    if (sscanf(s, "%i", &n) != 1)
        return 0;
    *v = (uint32_t)n;
    return 1;
}

/* [0x402c70] */
static void cmd_org(int argc, char **argv)
{
    uint32_t v;
    if (argc != 2)
        fatal("incorrect number of ORG arguments");
    if (!parse_num(argv[1], &v))
        fatal("invalid ORG value");
    if (origin == ADDR_UNSET)
        origin = v;
    else
        warning("org statement in symbol file ignored");
}

/* [0x402ce0] */
static void cmd_cpu(int argc, char **argv)
{
    if (argc != 2)
        fatal("incorrect number of CPU arguments");
    if (cpu == NULL) {
        if (!cpu_select(argv[1]))
            fatal("unknown CPU type \"%s\"", argv[1]);
    } else {
        warning("cpu statement in symbol file ignored");
    }
}

/* [0x402d30] */
static void cmd_skip(int argc, char **argv)
{
    uint32_t addr, len;
    if (argc != 3)
        fatal("incorrect number of SKIP arguments");
    if (!parse_num(argv[1], &addr))
        fatal("invalid SKIP address");
    if (!parse_num(argv[2], &len))
        fatal("invalid SKIP length");
    mem_fill(addr, MEM_NONE, len);
}

/* [0x402db0] */
static void cmd_message(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        fputs(argv[i], stdout);
        fputc(' ', stdout);
    }
    fputc('\n', stdout);
}

/* [0x402e00] */
static void cmd_numformat(int argc, char **argv)
{
    if (argc != 2)
        fatal("incorrect number of NUMFORMAT arguments");
    if (strlen(argv[1]) != 1)
        fatal("format specifier must be a single character");
    int c = toupper((unsigned char)argv[1][0]);
    switch (c) {
    case 'A': case 'C': case 'D': case 'I': case 'M': case 'R': case 'S':
        numfmt_set(c);
        break;
    default:
        fatal("format specifier not recognised");
    }
}

/* [0x403790] */
static void cmd_vector(int argc, char **argv)
{
    uint32_t addr;
    if (argc < 2 || argc > 4)
        fatal("incorrect number of VECTOR arguments");
    if (!parse_num(argv[1], &addr))
        fatal("invalid VECTOR value");
    if (argc > 2) {
        sym_add(argv[2], 2, MEM_WORD, addr, SYM_DATA);
        mem_fill(addr, MEM_WORD, 2);
    }
    if (mem_get(addr) == MEM_WORD && mem_get(addr + 1) == MEM_WORD) {
        uint32_t target = img_word(addr);
        thread_add_entry(target);
        if (argc == 4)
            sym_add(argv[3], 1, MEM_CODE, target, SYM_CODE);
    }
}

/* Common argument handling for vectab and addrtab */
static uint32_t table_args(int argc, char **argv, const char *nargs, const char *badval,
                           const char *badlen, uint32_t *addr)
{
    uint32_t count;
    if (argc < 3 || argc > 4)
        fatal(nargs);
    if (!parse_num(argv[1], addr))
        fatal(badval);
    if (argc == 4) {
        if (!parse_num(argv[3], &count))
            fatal(badlen);
    } else {
        count = 1;
    }
    sym_add(argv[2], count * 2, MEM_WORD, *addr, SYM_DATA);
    mem_fill(*addr, MEM_WORD, count * 2);
    return count;
}

/* [0x403890] */
static void cmd_vectab(int argc, char **argv)
{
    uint32_t addr;
    uint32_t count = table_args(argc, argv, "incorrect number of VECTAB arguments",
                                "invalid VECTAB value", "invalid VECTAB length", &addr);
    char name[128];
    while (count != 0) {
        count--;
        uint32_t target = img_word(addr);
        thread_add_entry(target);
        if (sym_find_code_def(target) == NULL && sym_find(target, SYM_CODE) == NULL) {
            sprintf(name, "L%04X", target);
            sym_add(name, 0, MEM_CODE, target, SYM_CODE);
        }
        addr += 2;
    }
}

/* [0x403a30] */
static void cmd_addrtab(int argc, char **argv)
{
    uint32_t addr;
    uint32_t count = table_args(argc, argv, "incorrect number of ADDRTAB arguments",
                                "invalid ADDRTAB value", "invalid ADDRTAB length", &addr);
    char name[128];
    while (count != 0) {
        count--;
        uint32_t target = img_word(addr);
        if (sym_find(target, SYM_DATA) == NULL) {
            sprintf(name, "X%04X", target);
            sym_add(name, 0, 0, target, SYM_DATA);
        }
        addr += 2;
    }
}

/* [0x403bf0] */
static void cmd_code(int argc, char **argv)
{
    uint32_t addr, count;
    if (argc < 2 || argc > 4)
        fatal("incorrect number of CODE arguments");
    if (!parse_num(argv[1], &addr))
        fatal("invalid CODE value");
    thread_add_entry(addr);
    if (argc == 2) {
        sym_auto_code(addr);
        return;
    }
    if (argc == 4) {
        if (!parse_num(argv[3], &count))
            fatal("invalid length in CODE record");
    } else {
        count = 0;
    }
    sym_add(argv[2], count, MEM_CODE, addr, SYM_CODE);
    if (count != 0) {
        while (--count != 0)
            thread_add_entry(++addr);
    }
}

/* [0x403cf0..0x403f20] byte, word, dword and string data */
static void cmd_data(int argc, char **argv, int kind, uint32_t unit, const char *what)
{
    uint32_t addr, count;
    char msg[64];
    if (argc < 3 || argc > 4) {
        sprintf(msg, "incorrect number of %s arguments", what);
        fatal(msg);
    }
    if (!parse_num(argv[1], &addr)) {
        sprintf(msg, "invalid %s value", what);
        fatal(msg);
    }
    if (argc == 4) {
        if (!parse_num(argv[3], &count)) {
            sprintf(msg, "invalid length in %s record", what);
            fatal(msg);
        }
        count *= unit;
    } else {
        count = unit;
    }
    sym_add(argv[2], count, (uint32_t)kind, addr, SYM_DATA);
    mem_fill(addr, kind, count);
}

/* [0x403fd0] */
static void cmd_symbol(int argc, char **argv)
{
    uint32_t value;
    if (argc != 3)
        fatal("incorrect number of SYMBOL arguments");
    if (!parse_num(argv[1], &value))
        fatal("invalid SYMBOL value");
    sym_add(argv[2], 0, 0, value, SYM_DATA);
}

static void read_file(FILE *f, int pass_b);

/* [0x4032a0] */
static void cmd_include(int argc, char **argv, int pass_b)
{
    if (argc != 2)
        fatal("incorrect number of INCLUDE arguments");
    FILE *f = fopen(argv[1], "r");
    if (f == NULL)
        fatal("unable to open INCLUDE file \"%s\"", argv[1]);
    read_file(f, pass_b);
    fclose(f);
}

/* [0x402e90] pass A and [0x404030] pass B */
static void read_file(FILE *f, int pass_b)
{
    char line[260];
    char *argv[MAX_TOKENS];
    while (fgets(line, 256, f) != NULL) {
        char *semi = strchr(line, ';');
        if (semi != NULL)
            *semi = '\0';
        int argc = tokenize(trim(line), argv, MAX_TOKENS);
        if (argc == 0)
            continue;
        int cmd = lookup_keyword(argv[0]);
        if (!pass_b) {
            switch (cmd) {
            case CMD_ORG:     cmd_org(argc, argv); break;
            case CMD_CPU:     cmd_cpu(argc, argv); break;
            case CMD_INCLUDE: cmd_include(argc, argv, 0); break;
            case -1:          fatal("invalid command \"%s\" in symbol file", argv[0]); break;
            default:          break;
            }
            continue;
        }
        switch (cmd) {
        case -1:
        case CMD_ORG:
        case CMD_CPU:       break;
        case CMD_CODE:      cmd_code(argc, argv); break;
        case CMD_VECTOR:    cmd_vector(argc, argv); break;
        case CMD_VECTAB:    cmd_vectab(argc, argv); break;
        case CMD_ADDRTAB:   cmd_addrtab(argc, argv); break;
        case CMD_BYTE:      cmd_data(argc, argv, MEM_BYTE, 1, "BYTE"); break;
        case CMD_WORD:      cmd_data(argc, argv, MEM_WORD, 2, "WORD"); break;
        case CMD_DWORD:     cmd_data(argc, argv, MEM_DWORD, 4, "DWORD"); break;
        case CMD_STRING:    cmd_data(argc, argv, MEM_STRING, 1, "STRING"); break;
        case CMD_SYMBOL:    cmd_symbol(argc, argv); break;
        case CMD_SKIP:      cmd_skip(argc, argv); break;
        case CMD_INCLUDE:   cmd_include(argc, argv, 1); break;
        case CMD_MESSAGE:   cmd_message(argc, argv); break;
        case CMD_NUMFORMAT: cmd_numformat(argc, argv); break;
        default:            fatal("command \"%s\" not yet implemented", argv[0]); break;
        }
    }
}

void symfile_pass_a(FILE *f)
{
    read_file(f, 0);
}

void symfile_pass_b(FILE *f)
{
    read_file(f, 1);
}
