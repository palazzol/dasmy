/*
 * Symbol table [0x402fb0..0x403680]
 *
 * Open addressing with linear probing over SLOTS entries, hashed on the
 * byte-swapped low 16 bits of the value.  The hash and probing must match
 * the original exactly: the listing walks the table in probe order.
 */

#include <stdlib.h>
#include <string.h>

#include "dasmy.h"

#define SLOTS       0xFFF1u
#define MAX_SYMS    0xFFEFu
#define POOL_SIZE   400000u

static Symbol *table;
static uint32_t count;
static char *pool;
static char *pool_next;

/* [0x43d0e8] type names for the symbol table listing */
static const char *const type_names[] = { "    ", "    ", "    ", "Val ", "Code", "Data" };

static uint32_t hash(uint32_t value)
{
    return (((value & 0xFF) << 8) | ((value >> 8) & 0xFF)) % SLOTS;
}

static Symbol *next_slot(Symbol *s)
{
    return ++s == table + SLOTS ? table : s;
}

/* [0x403220] */
int sym_init(void)
{
    pool = malloc(POOL_SIZE);
    if (pool == NULL)
        fatal("unable to allocate memory for string table");
    pool_next = pool;
    table = calloc(SLOTS, sizeof(Symbol));
    if (table == NULL)
        fatal("unable to allocate memory for symbol table");
    count = 0;
    return 1;
}

/* [0x4031f0] */
void sym_free(void)
{
    free(pool);
    free(table);
    pool = NULL;
    table = NULL;
}

/* [0x403190] */
static const char *pool_add(const char *name)
{
    size_t len = strlen(name) + 1;
    if (pool_next + len > pool + POOL_SIZE) {
        warning("string table full");
        return NULL;
    }
    char *s = pool_next;
    memcpy(s, name, len);
    pool_next += len;
    return s;
}

/* [0x4033f0] values above 16 bits are silently ignored */
void sym_add(const char *name, uint32_t length, uint32_t kind, uint32_t value, int type)
{
    if (value >= 0x10000)
        return;
    if (count > MAX_SYMS) {
        warning("symbol table full");
        return;
    }
    const char *s = pool_add(name);
    if (s == NULL)
        return;
    Symbol *e = table + hash(value);
    while (e->type != 0)
        e = next_slot(e);
    e->name = s;
    e->value = value;
    e->type = (uint8_t)type;
    e->kind = kind;
    e->length = length;
    count++;
}

/* [0x4030d0] first symbol with this value and type (SYM_ANY matches any) */
Symbol *sym_find(uint32_t value, int type)
{
    for (Symbol *e = table + hash(value); e->type != 0; e = next_slot(e)) {
        if (e->value == value && (type == SYM_ANY || e->type == type))
            return e;
    }
    return NULL;
}

/* [0x403130] a code label that defines a code area */
Symbol *sym_find_code_def(uint32_t value)
{
    for (Symbol *e = table + hash(value); e->type != 0; e = next_slot(e)) {
        if (e->value == value && e->type == SYM_CODE && e->kind == MEM_CODE)
            return e;
    }
    return NULL;
}

/* [0x403320] */
void sym_print_labels(uint32_t addr, int type)
{
    for (Symbol *e = table + hash(addr); e->type != 0; e = next_slot(e)) {
        if (e->value == addr && e->type == type) {
            if (!opt_asm) {
                fprintf(out_file, cpu->addr_max < 0x10000 ? "%04X\t\t\t\t" : "%08X\t\t\t", addr);
                if (opt_wide)
                    fputs("\t\t\t", out_file);
            }
            fprintf(out_file, "%s:\n", e->name);
        }
    }
}

/*
 * [0x402fb0] Walks values 0..0xFFFF; for each it prints the run of entries
 * with that value starting at its home slot, stopping at the first entry
 * with a different value (so entries displaced past another value's entry
 * are not listed - as in the original).
 */
void sym_print_table(void)
{
    if (count == 0)
        return;
    fputc('\n', out_file);
    fputs(";--------------------------------------------------------------\n", out_file);
    fputs("Symbol table\n============\n\n", out_file);
    fputs("Value\t\tType\tName\n", out_file);
    fputs("-----\t\t----\t----\n", out_file);
    for (uint32_t v = 0; v < 0x10000; v++) {
        for (Symbol *e = table + hash(v); e->value == v && e->type != 0; e = next_slot(e)) {
            fprintf(out_file, cpu->addr_max < 0x10000 ? "%04X\t\t%s\t%s\n" : "%08X\t%s\t%s\n",
                    e->value, type_names[e->type], e->name);
        }
    }
    fprintf(out_file, "\nNumber of symbols:\t\t\t%d\n\n", (int)count);
    fputs(";--------------------------------------------------------------\n", out_file);
}

/* [0x403490] label for a `code` directive without a name */
void sym_auto_code(uint32_t addr)
{
    char name[128];
    if (sym_find(addr, SYM_CODE) == NULL) {
        sprintf(name, "L%04X", addr);
        sym_add(name, 0, MEM_CODE, addr, SYM_CODE);
    }
}

/* [0x4034f0] automatic L/X label for a referenced address */
void sym_auto_label(uint32_t addr, int is_code)
{
    char name[128];
    int type = is_code ? SYM_CODE : SYM_DATA;
    if (sym_find(addr, type) == NULL) {
        sprintf(name, "%c%04X", is_code ? 'L' : 'X', addr);
        sym_add(name, 0, 0, addr, type);
    }
}

/* [0x403570] word at addr is a pointer to code */
void sym_vector_label(uint32_t addr)
{
    char name[128];
    if (sym_find(addr, SYM_DATA) == NULL) {
        sprintf(name, "X%04X", addr);
        sym_add(name, 2, MEM_WORD, addr, SYM_DATA);
    }
    if (mem_get(addr) != MEM_NONE && mem_get(addr + 1) != MEM_NONE) {
        if (mem_get(addr) == MEM_UNKNOWN && mem_get(addr + 1) == MEM_UNKNOWN)
            mem_fill(addr, MEM_WORD, 2);
        uint32_t target = img_word(addr);
        if (sym_find(target, SYM_CODE) == NULL) {
            sprintf(name, "L%04X", target);
            sym_add(name, 1, MEM_CODE, target, SYM_CODE);
        }
    }
}

/* [0x403680] word at addr is a pointer to data */
void sym_pointer_label(uint32_t addr)
{
    char name[128];
    if (sym_find(addr, SYM_DATA) == NULL) {
        sprintf(name, "X%04X", addr);
        sym_add(name, 2, MEM_WORD, addr, SYM_DATA);
    }
    if (mem_get(addr) != MEM_NONE && mem_get(addr + 1) != MEM_NONE) {
        if (mem_get(addr) == MEM_UNKNOWN && mem_get(addr + 1) == MEM_UNKNOWN)
            mem_fill(addr, MEM_WORD, 2);
        uint32_t target = img_word(addr);
        if (sym_find(target, SYM_DATA) == NULL) {
            sprintf(name, "X%04X", target);
            sym_add(name, 1, MEM_BADCODE, target, SYM_DATA);
        }
    }
}

/* [0x4029d0] append the code/data symbol name for value, or the value as a word */
void sym_append_name(char *buf, uint32_t value, int is_code)
{
    Symbol *s = sym_find(value, is_code ? SYM_CODE : SYM_DATA);
    buf += strlen(buf);
    if (s != NULL)
        strcpy(buf, s->name);
    else
        fmt_word(buf, value);
}

/* [0x4028f0] append the name of any symbol for value, or the value as a word */
void sym_append_any(char *buf, uint32_t value)
{
    Symbol *s = sym_find(value, SYM_ANY);
    buf += strlen(buf);
    if (s != NULL)
        strcpy(buf, s->name);
    else
        fmt_word(buf, value);
}

/* [0x402960] as sym_append_any, but formats the value as a dword */
void sym_append_any_dword(char *buf, uint32_t value)
{
    Symbol *s = sym_find(value, SYM_ANY);
    buf += strlen(buf);
    if (s != NULL)
        strcpy(buf, s->name);
    else
        fmt_dword(buf, value);
}
