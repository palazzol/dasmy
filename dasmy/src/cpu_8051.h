#ifndef CPU_8051_H
#define CPU_8051_H

#include "dasmy.h"

extern const OpEntry tab_8051[256];
extern const char *const m8051_bit_names[256];
extern const char *const m8051_sfr_names[256];

uint32_t i8051_decode(uint32_t pc, OpEntry *e);
void i8051_pass1(uint32_t pc, uint32_t opaddr, const OpEntry *e);
void i8051_format(uint32_t pc, uint32_t opaddr, OpEntry *e, char *buf, const char **info);
void i8051_thread(uint32_t pc, uint32_t opaddr, const OpEntry *e);

#endif
