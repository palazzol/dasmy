#ifndef CPU_8080_H
#define CPU_8080_H

#include "dasmy.h"

extern const OpEntry tab_8080[256];
extern const OpEntry tab_8085[256];

uint32_t i8080_decode(uint32_t pc, OpEntry *e);
uint32_t i8085_decode(uint32_t pc, OpEntry *e);
void i8080_pass1(uint32_t pc, uint32_t opaddr, const OpEntry *e);
void i8080_format(uint32_t pc, uint32_t opaddr, OpEntry *e, char *buf, const char **info);
void i8080_thread(uint32_t pc, uint32_t opaddr, const OpEntry *e);

#endif
