#ifndef CPU_6800_H
#define CPU_6800_H

#include "dasmy.h"

extern const OpEntry tab_6800[256];
extern const OpEntry tab_6801[256];
extern const OpEntry tab_6301[256];

uint32_t m6800_decode(uint32_t pc, OpEntry *e);
uint32_t m6801_decode(uint32_t pc, OpEntry *e);
uint32_t m6301_decode(uint32_t pc, OpEntry *e);
void m6800_pass1(uint32_t pc, uint32_t opaddr, const OpEntry *e);
void m6800_format(uint32_t pc, uint32_t opaddr, OpEntry *e, char *buf, const char **info);
void m6800_thread(uint32_t pc, uint32_t opaddr, const OpEntry *e);

#endif
