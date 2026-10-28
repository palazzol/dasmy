#ifndef CPU_6502_H
#define CPU_6502_H

#include "dasmy.h"

extern const OpEntry tab_6502[256];
extern const OpEntry tab_65c00[256];
extern const OpEntry tab_65c02[256];

uint32_t m6502_decode(uint32_t pc, OpEntry *e);
uint32_t m65c00_decode(uint32_t pc, OpEntry *e);
uint32_t m65c02_decode(uint32_t pc, OpEntry *e);
void m6502_pass1(uint32_t pc, uint32_t opaddr, const OpEntry *e);
void m6502_format(uint32_t pc, uint32_t opaddr, OpEntry *e, char *buf, const char **info);
void m6502_thread(uint32_t pc, uint32_t opaddr, const OpEntry *e);

#endif
