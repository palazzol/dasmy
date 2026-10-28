/* Command line, file handling and the top-level run [0x401000..0x401930] */

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dasmy.h"

int opt_asm;
int opt_wide;
int opt_thread;
uint32_t opt_entry = ADDR_UNSET;
uint32_t origin = ADDR_UNSET;
FILE *out_file;
char src_name[260];
long src_len;

static const char *in_arg;
static FILE *in_file;
static FILE *sym_file;
static int n_warnings;

/* Format into a buffer and strip trailing newlines */
static void format_msg(char *buf, size_t size, const char *fmt, va_list ap)
{
    int n = vsnprintf(buf, size, fmt, ap);
    if (n < 0)
        n = 0;
    if ((size_t)n >= size)
        n = (int)size - 1;
    while (n > 0 && buf[n - 1] == '\n')
        buf[--n] = '\0';
}

/* [0x401000] */
void fatal(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    format_msg(buf, sizeof buf, fmt, ap);
    va_end(ap);
    fprintf(stderr, "ERROR - %s\n", buf);
    exit(1);
}

/* [0x401060] the tenth warning is fatal */
void warning(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    format_msg(buf, sizeof buf, fmt, ap);
    va_end(ap);
    fprintf(stderr, "WARNING - %s\n", buf);
    if (++n_warnings == 10)
        fatal("maximum number of warnings exceeded");
}

/* [0x4010e0] */
static void banner(void)
{
    printf("\n%s\n", DASMY_NAME);
    for (int i = 0; i < 30; i++)
        fputc('=', stdout);
    printf("\n%s\n", DASMY_CREDIT);
    printf("Version %d.%02d (%s)\n\n", DASMY_VER_MAJOR, DASMY_VER_MINOR, DASMY_DATE);
}

/* [0x4012b0] */
static void usage(void)
{
    fprintf(stderr, "Usage: dasmy [-altvw] [-cTYPE] [-eNNNN] [-oNNNN] binfile[.ext]\n");
    fprintf(stderr, "\t-a\tAssembler output\n");
    fprintf(stderr, "\t-cTYPE\tSet CPU type (e.g. -cZ80)\n");
    fprintf(stderr, "\t-eNNNN\tSpecify code entry point NNNN for threading\n");
    fprintf(stderr, "\t-l\tList all processors supported and exit\n");
    fprintf(stderr, "\t-oNNNN\tSet origin to NNNN\n");
    fprintf(stderr, "\t-t\tEnable code threading\n");
    fprintf(stderr, "\t-v\tDisplay version info and exit\n");
    fprintf(stderr, "\t-w\tWide format list file output\n");
    fprintf(stderr, "\n\tUse C language notation to specify numbers (i.e. 0xNNNN for hex)\n");
    exit(1);
}

/* [0x401a70] getopt, with error messages disabled as in the original */
static int optind_ = 1;
static const char *optarg_;
static const char *place = "";

static int get_opt(int argc, char **argv, const char *opts)
{
    int c;
    if (*place == '\0') {
        if (optind_ >= argc || *(place = argv[optind_]) != '-') {
            place = "";
            return -1;
        }
        c = '-';
        if (place[1] != '\0') {
            c = place[1];
            place++;
            if (c == '-') {
                optind_++;
                place = "";
                return -1;
            }
        }
    } else {
        c = *place;
    }
    place++;
    const char *p = strchr(opts, c);
    if (c != ':' && p != NULL) {
        if (p[1] != ':') {
            optarg_ = NULL;
            if (*place == '\0')
                optind_++;
            return c;
        }
        if (*place == '\0') {
            optind_++;
            if (optind_ >= argc) {
                place = "";
                return '?';
            }
            optarg_ = argv[optind_];
        } else {
            optarg_ = place;
        }
        place = "";
        optind_++;
        return c;
    }
    if (c == '-')
        return -1;
    if (*place == '\0')
        optind_++;
    return '?';
}

/* [0x401360] */
static void parse_args(int argc, char **argv)
{
    int c, n;
    while ((c = get_opt(argc, argv, "ac:e:lo:tvw")) != -1) {
        switch (c) {
        case 'a':
            opt_asm = 1;
            break;
        case 'c':
            if (!cpu_select(optarg_))
                fatal("unknown CPU type \"%s\"", optarg_);
            break;
        case 'e':
            if (sscanf(optarg_, "%i", &n) != 1)
                fatal("incorrect code entry switch format");
            opt_entry = (uint32_t)n;
            break;
        case 'l':
            banner();
            cpu_print_list(stdout);
            exit(0);
        case 'o':
            if (sscanf(optarg_, "%i", &n) != 1)
                fatal("incorrect origin switch format");
            origin = (uint32_t)n;
            break;
        case 't':
            opt_thread = 1;
            break;
        case 'v':
            banner();
            exit(0);
        case 'w':
            opt_wide = 1;
            break;
        default:
            usage();
        }
    }
    if (argc - optind_ == 1)
        in_arg = argv[optind_];
}

/* [0x401c00] the extension of a file name, or NULL if it has none */
static char *find_ext(char *name)
{
    char *dot = strrchr(name, '.');
    if (dot != NULL && (strchr(dot, '\\') != NULL || strchr(dot, '/') != NULL))
        return NULL;
    return dot;
}

/* [0x4014e0] */
static void open_files(void)
{
    char name[260];
    strncpy(name, in_arg, 255);
    name[255] = '\0';
    in_file = fopen(name, "rb");
    if (in_file == NULL) {
        if (find_ext(name) == NULL) {
            strcat(name, ".bin");
            in_file = fopen(name, "rb");
        }
        if (in_file == NULL)
            fatal("unable to open input file \"%s\"", name);
    }
    strcpy(src_name, name);

    /* The original crashes here for an existing file without an extension */
    char *ext = find_ext(name);
    if (ext == NULL)
        ext = name + strlen(name);
    strcpy(ext, opt_asm ? ".asm" : ".lst");
    out_file = fopen(name, "w");
    if (out_file == NULL)
        fatal("unable to create file \"%s\"", name);
    *find_ext(name) = '\0';
    strcat(name, ".sym");
    sym_file = fopen(name, "r");
}

/* [0x401660] */
static void get_length(void)
{
    if (fseek(in_file, 0, SEEK_END) != 0)
        fatal("unable to read source file");
    src_len = ftell(in_file);
    if (src_len == -1)
        fatal("unable to read source file");
    if (fseek(in_file, 0, SEEK_SET) != 0)
        fatal("unable to read source file");
    printf("Source file length: %d bytes\n", (int)src_len);
}

/* [0x4016f0] */
static void run(void)
{
    if (sym_file != NULL) {
        symfile_pass_a(sym_file);
        if (fseek(sym_file, 0, SEEK_SET) != 0)
            fatal("unable to read symbol file");
    }
    if (cpu == NULL)
        fatal("must specify CPU type");
    printf("CPU: %s %s (%s family)\n", cpu->manufacturer, cpu->name, cpu->family);
    get_length();
    uint32_t units = (uint32_t)src_len / (uint32_t)(cpu->data_bits / 8);
    numfmt_set(cpu->numfmt);
    thread_init();
    if (origin == ADDR_UNSET)
        origin = 0;
    if (!img_init(origin, cpu->data_bits, (uint32_t)src_len / ((uint32_t)(cpu->data_bits + 7) >> 3),
                  cpu->big_endian)
        || !img_load(in_file, origin, (uint32_t)src_len))
        fatal("unable to read source file");
    mem_fill(origin, opt_thread ? MEM_UNKNOWN : MEM_CODE, units);
    sym_init();
    if (sym_file != NULL)
        symfile_pass_b(sym_file);
    if (opt_thread) {
        if (opt_entry != ADDR_UNSET)
            thread_add_entry(opt_entry);
        fputs("Threading code...\n", stdout);
        thread_run();
    }
    fputs("Pass 1...\n", stdout);
    pass1(origin, units);
    fputs("Pass 2...\n", stdout);
    out_header();
    pass2(origin, units);
    if (!opt_asm)
        sym_print_table();
    if (opt_thread)
        print_stats(origin, (uint32_t)src_len);
    sym_free();
    img_free();
}

/* [0x401170] */
static void on_signal(int sig)
{
    (void)sig;
    fputs("\nAbnormal termination\n", stdout);
    exit(1);
}

/* [0x4018e0] */
int main(int argc, char **argv)
{
    parse_args(argc, argv);
    banner();
    if (in_arg == NULL)
        usage();
    signal(SIGINT, on_signal);
    open_files();
    run();
    fclose(in_file);
    fclose(out_file);
    if (sym_file != NULL)
        fclose(sym_file);
    return 0;
}
