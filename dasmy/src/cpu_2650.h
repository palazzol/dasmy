#ifndef CPU_2650_H
#define CPU_2650_H

#include "dasmy.h"

extern const OpEntry tab_2650[256];
extern const char *const s2650_index_regs[4];

uint32_t s2650_decode(uint32_t pc, OpEntry *e);
void s2650_pass1(uint32_t pc, uint32_t opaddr, const OpEntry *e);
void s2650_format(uint32_t pc, uint32_t opaddr, OpEntry *e, char *buf, const char **info);
void s2650_thread(uint32_t pc, uint32_t opaddr, const OpEntry *e);

#endif
