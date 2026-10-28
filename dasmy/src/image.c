/*
 * Source image and memory-type map [0x408360..0x408a30]
 *
 * The image holds `units` units of `unit_bytes` bytes each, starting at
 * address `base`.  Every unit has a memory type (MEM_*).
 *
 * Where the original read outside the image without bounds checks
 * (undefined behaviour), these functions return 0 instead.
 */

#include <stdlib.h>
#include <string.h>

#include "dasmy.h"

static struct {
    uint32_t base;
    int bits;
    uint32_t unit_bytes;
    uint32_t units;
    uint32_t size;
    int big_endian;
    uint8_t *data;
    int *map;
} img;

/* [0x413950] bytes shown per listing line, indexed by unit size */
static const int bpl_table[5] = { 0, 4, 2, 1, 1 };

int img_init(uint32_t base, int bits, uint32_t units, int big_endian)
{
    img.base = base;
    img.bits = bits;
    img.unit_bytes = (uint32_t)(bits + 7) >> 3;
    img.units = units;
    img.size = img.unit_bytes * units;
    img.big_endian = big_endian;
    img.data = calloc(img.size ? img.size : 1, 1);
    img.map = calloc(units ? units : 1, sizeof(int));
    return img.data != NULL && img.map != NULL;
}

int img_load(FILE *f, uint32_t base, uint32_t len)
{
    size_t count = (len / img.unit_bytes) * img.unit_bytes;
    return fread(img.data + (base - img.base) * img.unit_bytes, 1, count, f) == count;
}

void img_free(void)
{
    free(img.data);
    free(img.map);
    img.data = NULL;
    img.map = NULL;
}

uint32_t img_unit_bytes(void)
{
    return img.unit_bytes;
}

int bytes_per_line(void)
{
    int n = bpl_table[img.unit_bytes];
    return opt_wide ? n * 2 : n;
}

/* Raw byte at offset from the start of the image data, 0 outside */
static uint8_t raw(uint32_t off)
{
    return off < img.size ? img.data[off] : 0;
}

/* Byte offset of an address, for the byte-addressed accessors */
static uint32_t byte_off(uint32_t addr)
{
    return addr - img.base;
}

/* [0x408450] one unit, in CPU byte order */
uint32_t img_unit(uint32_t addr)
{
    uint32_t off = (addr - img.base) * img.unit_bytes;
    switch (img.unit_bytes) {
    case 1:
        return raw(off);
    case 2:
        return img.big_endian ? (uint32_t)raw(off) << 8 | raw(off + 1)
                              : (uint32_t)raw(off + 1) << 8 | raw(off);
    case 3:
        return img.big_endian ? (uint32_t)raw(off) << 16 | (uint32_t)raw(off + 1) << 8 | raw(off + 2)
                              : (uint32_t)raw(off + 2) << 16 | (uint32_t)raw(off + 1) << 8 | raw(off);
    case 4:
        return img.big_endian ? (uint32_t)raw(off) << 24 | (uint32_t)raw(off + 1) << 16
                                    | (uint32_t)raw(off + 2) << 8 | raw(off + 3)
                              : (uint32_t)raw(off + 3) << 24 | (uint32_t)raw(off + 2) << 16
                                    | (uint32_t)raw(off + 1) << 8 | raw(off);
    default:
        return 0;
    }
}

/* [0x408540, 0x408620] */
uint8_t img_byte(uint32_t addr)
{
    if (addr < img.base || addr >= img.base + img.units)
        return 0;
    return raw(byte_off(addr));
}

/* [0x408590] word in CPU byte order */
uint32_t img_word(uint32_t addr)
{
    uint32_t off = byte_off(addr);
    return img.big_endian ? (uint32_t)raw(off) << 8 | raw(off + 1)
                          : (uint32_t)raw(off + 1) << 8 | raw(off);
}

/* [0x408570] word, always big endian */
uint32_t img_word_be(uint32_t addr)
{
    uint32_t off = byte_off(addr);
    return (uint32_t)raw(off) << 8 | raw(off + 1);
}

/* [0x408650] signed word in CPU byte order */
int32_t img_sword(uint32_t addr)
{
    uint32_t v = img_word(addr);
    return v > 0x7FFF ? (int32_t)v - 0x10000 : (int32_t)v;
}

/* [0x4085d0] dword in CPU byte order */
uint32_t img_dword(uint32_t addr)
{
    uint32_t off = byte_off(addr);
    if (img.big_endian)
        return (uint32_t)raw(off) << 24 | (uint32_t)raw(off + 1) << 16
             | (uint32_t)raw(off + 2) << 8 | raw(off + 3);
    return (uint32_t)raw(off + 3) << 24 | (uint32_t)raw(off + 2) << 16
         | (uint32_t)raw(off + 1) << 8 | raw(off);
}

/* [0x408690] */
int mem_get(uint32_t addr)
{
    if (addr >= img.base && addr < img.base + img.units)
        return img.map[addr - img.base];
    return MEM_NONE;
}

/* [0x4086c0] */
void mem_set(uint32_t addr, int type)
{
    if (addr >= img.base && addr < img.base + img.units)
        img.map[addr - img.base] = type;
}

/* [0x4086e0] only applied when the whole range lies inside the image */
void mem_fill(uint32_t addr, int type, uint32_t count)
{
    if (addr >= img.base && addr + count <= img.base + img.units) {
        for (uint32_t i = 0; i < count; i++)
            img.map[addr - img.base + i] = type;
    }
}

/* [0x408730] one unit as hex digits */
static int hex_unit(char *buf, uint32_t addr)
{
    uint32_t v = img_unit(addr);
    switch (img.unit_bytes) {
    case 1: return sprintf(buf, "%02X", v & 0xFF);
    case 2: return sprintf(buf, "%04X", v & 0xFFFF);
    case 3: return sprintf(buf, "%06X", v & 0xFFFFFF);
    case 4: return sprintf(buf, "%08X", v);
    default: return 0;
    }
}

/* [0x4087d0] n units as space-separated hex */
int img_hex(char *buf, uint32_t addr, uint32_t n)
{
    int len = 0;
    buf[0] = '\0';
    for (uint32_t i = 0; i < n; i++) {
        len += hex_unit(buf + len, addr + i);
        if (i < n - 1)
            buf[len++] = ' ';
    }
    buf[len] = '\0';
    return len;
}

/* [0x408820] one unit as characters; non-printables become spaces */
static int ascii_unit(char *buf, uint32_t addr)
{
    uint32_t off = (addr - img.base) * img.unit_bytes;
    for (uint32_t i = 0; i < img.unit_bytes; i++) {
        signed char c = (signed char)raw(off + i);
        buf[i] = (c < ' ' || c > '~') ? ' ' : c;
    }
    buf[img.unit_bytes] = '\0';
    return (int)img.unit_bytes;
}

/* [0x408880] */
int img_ascii(char *buf, uint32_t addr, uint32_t n)
{
    int len = 0;
    buf[0] = '\0';
    for (uint32_t i = 0; i < n; i++)
        len += ascii_unit(buf + len, addr + i);
    return len;
}

/* [0x4088c0] one unit in the current number format */
static int numfmt_unit(char *buf, uint32_t addr)
{
    uint32_t v = img_unit(addr);
    switch (img.unit_bytes) {
    case 1: return fmt_byte(buf, v);
    case 2: return fmt_word(buf, v);
    case 3:
    case 4: return fmt_dword(buf, v);
    default: return 0;
    }
}

/* [0x408940] n units in the number format, separated by ", " */
int img_numfmt(char *buf, uint32_t addr, uint32_t n)
{
    int len = 0;
    buf[0] = '\0';
    for (uint32_t i = 0; i < n; i++) {
        len += numfmt_unit(buf + len, addr + i);
        if (i < n - 1) {
            buf[len++] = ',';
            buf[len++] = ' ';
        }
    }
    buf[len] = '\0';
    return len;
}

/* [0x408990] */
uint32_t img_checksum(void)
{
    uint32_t sum = 0;
    for (uint32_t i = 0; i < img.size; i++)
        sum += img.data[i];
    return sum;
}

/* [0x4089b0] standard CRC-32 (as zlib) */
uint32_t img_crc32(void)
{
    static uint32_t table[256];
    static int have_table;
    if (!have_table) {
        for (uint32_t n = 0; n < 256; n++) {
            uint32_t c = n;
            for (int k = 0; k < 8; k++)
                c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[n] = c;
        }
        have_table = 1;
    }
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < img.size; i++)
        crc = crc >> 8 ^ table[(img.data[i] ^ crc) & 0xFF];
    return ~crc;
}
