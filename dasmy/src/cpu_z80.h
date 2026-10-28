#ifndef CPU_Z80_H
#define CPU_Z80_H

#include "dasmy.h"

extern const OpEntry tab_z80[256];
extern const OpEntry tab_z80_cb[256];
extern const OpEntry tab_z80_ed[256];
extern const OpEntry tab_z80_dd[256];
extern const OpEntry tab_z80_fd[256];
extern const OpEntry tab_z80_ddcb[256];
extern const OpEntry tab_z80_fdcb[256];
extern const OpEntry tab_lr35902[256];
extern const OpEntry tab_lr35902_cb[256];

uint32_t z80_decode(uint32_t pc, OpEntry *e);
void z80_pass1(uint32_t pc, uint32_t opaddr, const OpEntry *e);
void z80_format(uint32_t pc, uint32_t opaddr, OpEntry *e, char *buf, const char **info);
void z80_thread(uint32_t pc, uint32_t opaddr, const OpEntry *e);
uint32_t lr35902_decode(uint32_t pc, OpEntry *e);
void lr35902_pass1(uint32_t pc, uint32_t opaddr, const OpEntry *e);
void lr35902_format(uint32_t pc, uint32_t opaddr, OpEntry *e, char *buf, const char **info);

#endif
