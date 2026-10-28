#ifndef CPU_1802_H
#define CPU_1802_H

#include "dasmy.h"

extern const OpEntry tab_1802[256];
extern const OpEntry tab_1805_68[256];

uint32_t r1802_decode(uint32_t pc, OpEntry *e);
uint32_t r1805_decode(uint32_t pc, OpEntry *e);
void r1802_pass1(uint32_t pc, uint32_t opaddr, const OpEntry *e);
void r1802_format(uint32_t pc, uint32_t opaddr, OpEntry *e, char *buf, const char **info);
void r1802_thread(uint32_t pc, uint32_t opaddr, const OpEntry *e);

#endif
