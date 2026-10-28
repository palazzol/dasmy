/*
 * Dasmy - a cross-platform reimplementation of the DASMx 1.40 disassembler.
 *
 * Comments of the form [0x4xxxxx] give the address of the corresponding
 * function in the original Dasmx.exe (see re/NOTES.md).
 */
#ifndef DASMY_H
#define DASMY_H

#include <stdint.h>
#include <stdio.h>

#define DASMY_NAME      "Dasmy object code disassembler"
#define DASMY_CREDIT    "Reimplementation of DASMx 1.40"
#define DASMY_VER_MAJOR 0
#define DASMY_VER_MINOR 10
#define DASMY_DATE      "Oct 27 2026"

/* Sentinel used by DASMx for "not set" origin and entry point */
#define ADDR_UNSET 0xFFFFu

/* ---- Opcode table entry (20 bytes in the original) ---- */

/* flags */
#define OPF_TYPE_MASK   0x00FFu /* addressing type, CPU specific; 0 = invalid */
#define OPF_BREAK       0x0100u /* ends a code sequence (jump, return) */
#define OPF_CODEREF     0x0200u /* operand refers to code (jump/call target) */
#define OPF_CYCPLUS     0x1000u /* show '+' after the cycle count */

typedef struct {
    const char *mnemonic;
    const char *operands;
    uint32_t flags;
    uint32_t length;
    uint32_t cycles;    /* low byte = minimum, next byte = extra for maximum */
} OpEntry;

/* ---- CPU descriptor (72 bytes in the original) ---- */

typedef uint32_t (*CpuDecodeFn)(uint32_t pc, OpEntry *e);
typedef void (*CpuPass1Fn)(uint32_t pc, uint32_t opaddr, const OpEntry *e);
typedef void (*CpuFormatFn)(uint32_t pc, uint32_t opaddr, OpEntry *e, char *buf, const char **info);
typedef void (*CpuThreadFn)(uint32_t pc, uint32_t opaddr, const OpEntry *e);

typedef struct {
    const char *name;
    const char *part;
    const char *manufacturer;
    const char *family;
    int big_endian;
    int unknown5;
    int max_len;
    uint32_t addr_min;
    uint32_t addr_max;
    int data_bits;
    int unknown10;
    uint32_t addr2_max;
    int addr2_bits;     /* non-zero: separate data address space (8051) */
    int numfmt;
    CpuDecodeFn decode;
    CpuPass1Fn pass1;
    CpuFormatFn format;
    CpuThreadFn thread;
} CpuDesc;

extern const CpuDesc *const cpu_list[];
extern const CpuDesc *cpu;          /* selected CPU, NULL if none */

int cpu_select(const char *name);
void cpu_print_list(FILE *f);

/* ---- Global options and state ---- */

extern int opt_asm;         /* -a */
extern int opt_wide;        /* -w */
extern int opt_thread;      /* -t */
extern uint32_t opt_entry;  /* -e */
extern uint32_t origin;     /* -o / org */
extern FILE *out_file;
extern char src_name[];
extern long src_len;

/* ---- Diagnostics [0x401000, 0x401060] ---- */

void fatal(const char *fmt, ...);
void warning(const char *fmt, ...);

/* ---- Number formatting [0x404200..0x404410] ---- */

void numfmt_set(int c);
int fmt_byte(char *buf, uint32_t v);
int fmt_word(char *buf, uint32_t v);
int fmt_dword(char *buf, uint32_t v);

/* ---- Image and memory map [0x408360..0x408a30] ---- */

enum {
    MEM_NONE = 0,       /* skipped / outside image */
    MEM_UNKNOWN = 1,    /* not yet reached by threading */
    MEM_CODE = 2,
    MEM_BADCODE = 3,    /* rejected as code in pass 1 */
    MEM_BYTE = 4,
    MEM_WORD = 5,
    MEM_DWORD = 6,
    MEM_STRING = 7
};

int img_init(uint32_t base, int bits, uint32_t units, int big_endian);
int img_load(FILE *f, uint32_t base, uint32_t len);
void img_free(void);
uint32_t img_unit_bytes(void);
uint32_t img_unit(uint32_t addr);
uint8_t img_byte(uint32_t addr);
uint32_t img_word(uint32_t addr);
uint32_t img_word_be(uint32_t addr);
int32_t img_sword(uint32_t addr);
uint32_t img_dword(uint32_t addr);
int mem_get(uint32_t addr);
void mem_set(uint32_t addr, int type);
void mem_fill(uint32_t addr, int type, uint32_t count);
int img_hex(char *buf, uint32_t addr, uint32_t n);
int img_ascii(char *buf, uint32_t addr, uint32_t n);
int img_numfmt(char *buf, uint32_t addr, uint32_t n);
uint32_t img_checksum(void);
uint32_t img_crc32(void);
int bytes_per_line(void);

/* ---- Symbol table [0x402fb0..0x403680] ---- */

enum { SYM_VAL = 3, SYM_CODE = 4, SYM_DATA = 5, SYM_ANY = 1 };

typedef struct {
    const char *name;
    uint32_t length;
    uint32_t kind;      /* memory type it defines (2 = code, 4..7 = data), 0 = none */
    uint32_t value;
    uint8_t type;       /* 0 = empty slot, else SYM_* */
} Symbol;

int sym_init(void);
void sym_free(void);
void sym_add(const char *name, uint32_t length, uint32_t kind, uint32_t value, int type);
Symbol *sym_find(uint32_t value, int type);
Symbol *sym_find_code_def(uint32_t value);
void sym_print_labels(uint32_t addr, int type);
void sym_print_table(void);
void sym_auto_label(uint32_t addr, int is_code);
void sym_auto_code(uint32_t addr);
void sym_vector_label(uint32_t addr);
void sym_pointer_label(uint32_t addr);
void sym_append_name(char *buf, uint32_t value, int is_code);
void sym_append_any(char *buf, uint32_t value);
void sym_append_any_dword(char *buf, uint32_t value);

/* ---- Symbol file [0x402e90, 0x404030] ---- */

void symfile_pass_a(FILE *f);
void symfile_pass_b(FILE *f);

/* ---- Code threading [0x402a70..0x402c30] ---- */

void thread_init(void);
void thread_add_entry(uint32_t addr);
void thread_branch(uint32_t target, uint32_t from);
void thread_run(void);

/* ---- Output [0x401190..0x402890] ---- */

int out_pad(int col, int target);
void out_tab_printf(const char *fmt, ...);
void out_comment(void);
void out_header(void);
void out_line(uint32_t addr, const char *mn, const char *ops, uint32_t flags,
              uint32_t len, uint32_t cycles, const char *info);
void pass1(uint32_t start, uint32_t units);
void pass2(uint32_t start, uint32_t units);
void print_stats(uint32_t start, uint32_t len);

#endif
