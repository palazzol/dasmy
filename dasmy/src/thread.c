/* Code threading [0x402a70..0x402c30] */

#include "dasmy.h"

#define MAX_ENTRIES 0x7FF

static uint32_t entries[MAX_ENTRIES];
static int n_entries;

/* [0x402b20] */
void thread_init(void)
{
    n_entries = 0;
}

/* [0x402a70] queue an entry point; code already found is ignored */
void thread_add_entry(uint32_t addr)
{
    if (mem_get(addr) == MEM_CODE)
        return;
    if (mem_get(addr) == MEM_UNKNOWN) {
        if (n_entries < MAX_ENTRIES)
            entries[n_entries++] = addr;
        else
            fatal("code threading entry table full");
        return;
    }
    printf("Ignoring non-ROM code entry point %04X\n", addr);
}

/* [0x402ae0] */
void thread_branch(uint32_t target, uint32_t from)
{
    if (mem_get(target) == MEM_NONE) {
        printf("Ignoring branch outside ROM to %04X at address %04X\n", target, from);
        return;
    }
    thread_add_entry(target);
}

/* [0x402b40] follow one thread of execution */
static void follow(uint32_t pc)
{
    OpEntry e;
    if (mem_get(pc) == MEM_CODE)
        return;
    while (mem_get(pc) == MEM_UNKNOWN) {
        uint32_t opaddr = cpu->decode(pc, &e);
        for (uint32_t i = 1; i < e.length; i++) {
            if (mem_get(pc + i) != MEM_UNKNOWN)
                goto premature;
        }
        if (e.flags == 0) {
            printf("Thread terminated: invalid instruction at address %04X\n", pc);
            return;
        }
        mem_fill(pc, MEM_CODE, e.length);
        cpu->thread(pc, opaddr, &e);
        if (e.flags & OPF_BREAK)
            return;
        pc += e.length;
        if (mem_get(pc) == MEM_CODE)
            return;
    }
premature:
    printf("Premature end of thread at address %04X\n", pc);
}

/* [0x402c30] entries are processed last-in, first-out */
void thread_run(void)
{
    if (n_entries == 0)
        fatal("need at least one code entry point");
    while (n_entries > 0) {
        n_entries--;
        follow(entries[n_entries]);
    }
}
