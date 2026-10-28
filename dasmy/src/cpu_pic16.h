#ifndef CPU_PIC16_H
#define CPU_PIC16_H

#include "dasmy.h"

extern const OpEntry tab_pic16[128];
extern const OpEntry tab_pic16_00[128];

uint32_t pic16_decode(uint32_t pc, OpEntry *e);
void pic16_pass1(uint32_t pc, uint32_t opaddr, const OpEntry *e);
void pic16_format(uint32_t pc, uint32_t opaddr, OpEntry *e, char *buf, const char **info);
void pic16_thread(uint32_t pc, uint32_t opaddr, const OpEntry *e);

#endif
