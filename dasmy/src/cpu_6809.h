#ifndef CPU_6809_H
#define CPU_6809_H

#include "dasmy.h"

extern const OpEntry tab_6809[256];
extern const OpEntry tab_6809_p10[256];
extern const OpEntry tab_6809_p11[256];
extern const signed char m6809_index_extra[256];

uint32_t m6809_decode(uint32_t pc, OpEntry *e);
void m6809_pass1(uint32_t pc, uint32_t opaddr, const OpEntry *e);
void m6809_format(uint32_t pc, uint32_t opaddr, OpEntry *e, char *buf, const char **info);
void m6809_thread(uint32_t pc, uint32_t opaddr, const OpEntry *e);

#endif
