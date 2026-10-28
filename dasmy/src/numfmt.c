/* Hex number formatting styles [0x404200..0x404410] */

#include "dasmy.h"

static int numfmt = 'M';

void numfmt_set(int c)
{
    numfmt = c;
}

int fmt_byte(char *buf, uint32_t v)
{
    switch (numfmt) {
    case 'A': return sprintf(buf, "&%02X", v);
    case 'C': return sprintf(buf, "0x%02X", v);
    case 'D': return sprintf(buf, "%d", (int)v);
    case 'I': return sprintf(buf, "0%02XH", v);
    case 'M': return sprintf(buf, "$%02X", v);
    case 'R': return sprintf(buf, "#%02X", v);
    case 'S': return sprintf(buf, "H'%02X'", v);
    default:  return 0;
    }
}

int fmt_word(char *buf, uint32_t v)
{
    switch (numfmt) {
    case 'A': return sprintf(buf, "&%04X", v);
    case 'C': return sprintf(buf, "0x%04X", v);
    case 'D': return sprintf(buf, "%d", (int)v);
    case 'I': return sprintf(buf, "0%04XH", v);
    case 'M': return sprintf(buf, "$%04X", v);
    case 'R': return sprintf(buf, "#%04X", v);
    case 'S': return sprintf(buf, "H'%04X'", v);
    default:  return 0;
    }
}

int fmt_dword(char *buf, uint32_t v)
{
    switch (numfmt) {
    case 'A': return sprintf(buf, "&%08X", v);
    case 'C': return sprintf(buf, "0x%08X", v);
    case 'D': return sprintf(buf, "%d", (int)v);
    case 'I': return sprintf(buf, "0%08XH", v);
    case 'M': return sprintf(buf, "$%08X", v);
    case 'R': return sprintf(buf, "#%08X", v);
    case 'S': return sprintf(buf, "H'%08X'", v);
    default:  return 0;
    }
}
