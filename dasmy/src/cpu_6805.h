#ifndef CPU_6805_H
#define CPU_6805_H

#include "dasmy.h"

extern const OpEntry tab_6805[256];
extern const OpEntry tab_68hc05[256];
extern const OpEntry tab_63l05[256];
extern const OpEntry tab_6305[256];

uint32_t m6805_decode(uint32_t pc, OpEntry *e);
uint32_t m68hc05_decode(uint32_t pc, OpEntry *e);
uint32_t m63l05_decode(uint32_t pc, OpEntry *e);
uint32_t m6305_decode(uint32_t pc, OpEntry *e);
void m6805_pass1(uint32_t pc, uint32_t opaddr, const OpEntry *e);
void m6805_format(uint32_t pc, uint32_t opaddr, OpEntry *e, char *buf, const char **info);
void m6805_thread(uint32_t pc, uint32_t opaddr, const OpEntry *e);

#endif
