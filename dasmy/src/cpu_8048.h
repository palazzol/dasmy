#ifndef CPU_8048_H
#define CPU_8048_H

#include "dasmy.h"

extern const OpEntry tab_8048[256];

uint32_t i8048_decode(uint32_t pc, OpEntry *e);
void i8048_pass1(uint32_t pc, uint32_t opaddr, const OpEntry *e);
void i8048_format(uint32_t pc, uint32_t opaddr, OpEntry *e, char *buf, const char **info);
void i8048_thread(uint32_t pc, uint32_t opaddr, const OpEntry *e);

#endif
