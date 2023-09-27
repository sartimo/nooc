/*
 *  NOOC - New Object Oriented C
 *
 *  Copyright (c) 2001-2004, 2022-2023 Timo Sarkar, Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#if !defined ONE_SOURCE || ONE_SOURCE
#include "noocpp.c"
#include "noocgen.c"
#include "noocdbg.c"
#include "noocasm.c"
#include "noocelf.c"
#include "noocrun.c"
#ifdef NOOC_TARGET_I386
#include "i386-gen.c"
#include "i386-link.c"
#include "i386-asm.c"
#elif defined(NOOC_TARGET_ARM)
#include "arm-gen.c"
#include "arm-link.c"
#include "arm-asm.c"
#elif defined(NOOC_TARGET_ARM64)
#include "arm64-gen.c"
#include "arm64-link.c"
#include "arm-asm.c"
#elif defined(NOOC_TARGET_C67)
#include "c67-gen.c"
#include "c67-link.c"
#include "nooccoff.c"
#elif defined(NOOC_TARGET_X86_64)
#include "x86_64-gen.c"
#include "x86_64-link.c"
#include "i386-asm.c"
#elif defined(NOOC_TARGET_RISCV64)
#include "riscv64-gen.c"
#include "riscv64-link.c"
#include "riscv64-asm.c"
#else
#error unknown target
#endif
#ifdef NOOC_TARGET_PE
#include "noocpe.c"
#endif
#ifdef NOOC_TARGET_MACHO
#include "noocmacho.c"
#endif
#endif /* ONE_SOURCE */

#include "nooc.h"

/********************************************************/
/* global variables */

/* XXX: get rid of this ASAP (or maybe not) */
ST_DATA struct NOOCState *nooc_state;
NOOC_SEM(static nooc_compile_sem);
/* an array of pointers to memory to be free'd after errors */
ST_DATA void** stk_data;
ST_DATA int nb_stk_data;

/********************************************************/
#ifdef _WIN32
ST_FUNC char *normalize_slashes(char *path)
{
    char *p;
    for (p = path; *p; ++p)
        if (*p == '\\')
            *p = '/';
    return path;
}

#if defined LIBNOOC_AS_DLL && !defined CONFIG_NOOCDIR
static HMODULE nooc_module;
BOOL WINAPI DllMain (HINSTANCE hDll, DWORD dwReason, LPVOID lpReserved)
{
    if (DLL_PROCESS_ATTACH == dwReason)
        nooc_module = hDll;
    return TRUE;
}
#else
#define nooc_module NULL /* NULL means executable itself */
#endif

#ifndef CONFIG_NOOCDIR
/* on win32, we suppose the lib and includes are at the location of 'nooc.exe' */
static inline char *config_noocdir_w32(char *path)
{
    char *p;
    GetModuleFileName(nooc_module, path, MAX_PATH);
    p = nooc_basename(normalize_slashes(strlwr(path)));
    if (p > path)
        --p;
    *p = 0;
    return path;
}
#define CONFIG_NOOCDIR config_noocdir_w32(alloca(MAX_PATH))
#endif

#ifdef NOOC_TARGET_PE
static void nooc_add_systemdir(NOOCState *s)
{
    char buf[1000];
    GetSystemDirectory(buf, sizeof buf);
    nooc_add_library_path(s, normalize_slashes(buf));
}
#endif
#endif

/********************************************************/
#if CONFIG_NOOC_SEMLOCK
#if defined _WIN32
ST_FUNC void wait_sem(NOOCSem *p)
{
    if (!p->init)
        InitializeCriticalSection(&p->cr), p->init = 1;
    EnterCriticalSection(&p->cr);
}
ST_FUNC void post_sem(NOOCSem *p)
{
    LeaveCriticalSection(&p->cr);
}
#elif defined __APPLE__
/* Half-compatible MacOS doesn't have non-shared (process local)
   semaphores.  Use the dispatch framework for lightweight locks.  */
ST_FUNC void wait_sem(NOOCSem *p)
{
    if (!p->init)
        p->sem = dispatch_semaphore_create(1), p->init = 1;
    dispatch_semaphore_wait(p->sem, DISPATCH_TIME_FOREVER);
}
ST_FUNC void post_sem(NOOCSem *p)
{
    dispatch_semaphore_signal(p->sem);
}
#else
ST_FUNC void wait_sem(NOOCSem *p)
{
    if (!p->init)
        sem_init(&p->sem, 0, 1), p->init = 1;
    while (sem_wait(&p->sem) < 0 && errno == EINTR);
}
ST_FUNC void post_sem(NOOCSem *p)
{
    sem_post(&p->sem);
}
#endif
#endif

PUB_FUNC void nooc_enter_state(NOOCState *s1)
{
    if (s1->error_set_jmp_enabled)
        return;
    WAIT_SEM(&nooc_compile_sem);
    nooc_state = s1;
}

PUB_FUNC void nooc_exit_state(NOOCState *s1)
{
    if (s1->error_set_jmp_enabled)
        return;
    nooc_state = NULL;
    POST_SEM(&nooc_compile_sem);
}

/********************************************************/
/* copy a string and truncate it. */
ST_FUNC char *pstrcpy(char *buf, size_t buf_size, const char *s)
{
    char *q, *q_end;
    int c;

    if (buf_size > 0) {
        q = buf;
        q_end = buf + buf_size - 1;
        while (q < q_end) {
            c = *s++;
            if (c == '\0')
                break;
            *q++ = c;
        }
        *q = '\0';
    }
    return buf;
}

/* strcat and truncate. */
ST_FUNC char *pstrcat(char *buf, size_t buf_size, const char *s)
{
    size_t len;
    len = strlen(buf);
    if (len < buf_size)
        pstrcpy(buf + len, buf_size - len, s);
    return buf;
}

ST_FUNC char *pstrncpy(char *out, const char *in, size_t num)
{
    memcpy(out, in, num);
    out[num] = '\0';
    return out;
}

/* extract the basename of a file */
PUB_FUNC char *nooc_basename(const char *name)
{
    char *p = strchr(name, 0);
    while (p > name && !IS_DIRSEP(p[-1]))
        --p;
    return p;
}

/* extract extension part of a file
 *
 * (if no extension, return pointer to end-of-string)
 */
PUB_FUNC char *nooc_fileextension (const char *name)
{
    char *b = nooc_basename(name);
    char *e = strrchr(b, '.');
    return e ? e : strchr(b, 0);
}

ST_FUNC char *nooc_load_text(int fd)
{
    int len = lseek(fd, 0, SEEK_END);
    char *buf = load_data(fd, 0, len + 1);
    buf[len] = 0;
    return buf;
}

/********************************************************/
/* memory management */

#undef free
#undef malloc
#undef realloc

void mem_error(const char *msg)
{
    fprintf(stderr, "%s\n", msg);
    exit (1);
}

#ifndef MEM_DEBUG

PUB_FUNC void nooc_free(void *ptr)
{
    free(ptr);
}

PUB_FUNC void *nooc_malloc(unsigned long size)
{
    void *ptr;
    ptr = malloc(size ? size : 1);
    if (!ptr)
        mem_error("memory full (malloc)");
    return ptr;
}

PUB_FUNC void *nooc_mallocz(unsigned long size)
{
    void *ptr;
    ptr = nooc_malloc(size);
    if (size)
        memset(ptr, 0, size);
    return ptr;
}

PUB_FUNC void *nooc_realloc(void *ptr, unsigned long size)
{
    void *ptr1;
    if (size == 0) {
	free(ptr);
	ptr1 = NULL;
    }
    else {
        ptr1 = realloc(ptr, size);
        if (!ptr1)
            mem_error("memory full (realloc)");
    }
    return ptr1;
}

PUB_FUNC char *nooc_strdup(const char *str)
{
    char *ptr;
    ptr = nooc_malloc(strlen(str) + 1);
    strcpy(ptr, str);
    return ptr;
}

#else

#define MEM_DEBUG_MAGIC1 0xFEEDDEB1
#define MEM_DEBUG_MAGIC2 0xFEEDDEB2
#define MEM_DEBUG_MAGIC3 0xFEEDDEB3
#define MEM_DEBUG_FILE_LEN 40
#define MEM_DEBUG_CHECK3(header) \
    ((mem_debug_header_t*)((char*)header + header->size))->magic3
#define MEM_USER_PTR(header) \
    ((char *)header + offsetof(mem_debug_header_t, magic3))
#define MEM_HEADER_PTR(ptr) \
    (mem_debug_header_t *)((char*)ptr - offsetof(mem_debug_header_t, magic3))

struct mem_debug_header {
    unsigned magic1;
    unsigned size;
    struct mem_debug_header *prev;
    struct mem_debug_header *next;
    int line_num;
    char file_name[MEM_DEBUG_FILE_LEN + 1];
    unsigned magic2;
    ALIGNED(16) unsigned char magic3[4];
};

typedef struct mem_debug_header mem_debug_header_t;

NOOC_SEM(static mem_sem);
static mem_debug_header_t *mem_debug_chain;
static unsigned mem_cur_size;
static unsigned mem_max_size;
static int nb_states;

static mem_debug_header_t *malloc_check(void *ptr, const char *msg)
{
    mem_debug_header_t * header = MEM_HEADER_PTR(ptr);
    if (header->magic1 != MEM_DEBUG_MAGIC1 ||
        header->magic2 != MEM_DEBUG_MAGIC2 ||
        read32le(MEM_DEBUG_CHECK3(header)) != MEM_DEBUG_MAGIC3 ||
        header->size == (unsigned)-1) {
        fprintf(stderr, "%s check failed\n", msg);
        if (header->magic1 == MEM_DEBUG_MAGIC1)
            fprintf(stderr, "%s:%u: block allocated here.\n",
                header->file_name, header->line_num);
        exit(1);
    }
    return header;
}

PUB_FUNC void *nooc_malloc_debug(unsigned long size, const char *file, int line)
{
    int ofs;
    mem_debug_header_t *header;

    header = malloc(sizeof(mem_debug_header_t) + size);
    if (!header)
        mem_error("memory full (malloc)");

    header->magic1 = MEM_DEBUG_MAGIC1;
    header->magic2 = MEM_DEBUG_MAGIC2;
    header->size = size;
    write32le(MEM_DEBUG_CHECK3(header), MEM_DEBUG_MAGIC3);
    header->line_num = line;
    ofs = strlen(file) - MEM_DEBUG_FILE_LEN;
    strncpy(header->file_name, file + (ofs > 0 ? ofs : 0), MEM_DEBUG_FILE_LEN);
    header->file_name[MEM_DEBUG_FILE_LEN] = 0;

    WAIT_SEM(&mem_sem);
    header->next = mem_debug_chain;
    header->prev = NULL;
    if (header->next)
        header->next->prev = header;
    mem_debug_chain = header;
    mem_cur_size += size;
    if (mem_cur_size > mem_max_size)
        mem_max_size = mem_cur_size;
    POST_SEM(&mem_sem);

    return MEM_USER_PTR(header);
}

PUB_FUNC void nooc_free_debug(void *ptr)
{
    mem_debug_header_t *header;
    if (!ptr)
        return;
    header = malloc_check(ptr, "nooc_free");

    WAIT_SEM(&mem_sem);
    mem_cur_size -= header->size;
    header->size = (unsigned)-1;
    if (header->next)
        header->next->prev = header->prev;
    if (header->prev)
        header->prev->next = header->next;
    if (header == mem_debug_chain)
        mem_debug_chain = header->next;
    POST_SEM(&mem_sem);
    free(header);
}

PUB_FUNC void *nooc_mallocz_debug(unsigned long size, const char *file, int line)
{
    void *ptr;
    ptr = nooc_malloc_debug(size,file,line);
    memset(ptr, 0, size);
    return ptr;
}

PUB_FUNC void *nooc_realloc_debug(void *ptr, unsigned long size, const char *file, int line)
{
    mem_debug_header_t *header;
    int mem_debug_chain_update = 0;
    if (!ptr)
        return nooc_malloc_debug(size, file, line);
    header = malloc_check(ptr, "nooc_realloc");

    WAIT_SEM(&mem_sem);
    mem_cur_size -= header->size;
    mem_debug_chain_update = (header == mem_debug_chain);
    header = realloc(header, sizeof(mem_debug_header_t) + size);
    if (!header)
        mem_error("memory full (realloc)");
    header->size = size;
    write32le(MEM_DEBUG_CHECK3(header), MEM_DEBUG_MAGIC3);
    if (header->next)
        header->next->prev = header;
    if (header->prev)
        header->prev->next = header;
    if (mem_debug_chain_update)
        mem_debug_chain = header;
    mem_cur_size += size;
    if (mem_cur_size > mem_max_size)
        mem_max_size = mem_cur_size;
    POST_SEM(&mem_sem);

    return MEM_USER_PTR(header);
}

PUB_FUNC char *nooc_strdup_debug(const char *str, const char *file, int line)
{
    char *ptr;
    ptr = nooc_malloc_debug(strlen(str) + 1, file, line);
    strcpy(ptr, str);
    return ptr;
}

PUB_FUNC void nooc_memcheck(int d)
{
    WAIT_SEM(&mem_sem);
    nb_states += d;
    if (0 == nb_states && mem_cur_size) {
        mem_debug_header_t *header = mem_debug_chain;
        fflush(stdout);
        fprintf(stderr, "MEM_DEBUG: mem_leak= %d bytes, mem_max_size= %d bytes\n",
            mem_cur_size, mem_max_size);
        while (header) {
            fprintf(stderr, "%s:%u: error: %u bytes leaked\n",
                header->file_name, header->line_num, header->size);
            header = header->next;
        }
        fflush(stderr);
        mem_cur_size = 0;
        mem_debug_chain = NULL;
#if MEM_DEBUG-0 == 2
        exit(2);
#endif
    }
    POST_SEM(&mem_sem);
}

#endif /* MEM_DEBUG */

#ifdef _WIN32
# define realpath(file, buf) _fullpath(buf, file, 260)
#endif

/* for #pragma once */
ST_FUNC int normalized_PATHCMP(const char *f1, const char *f2)
{
    char *p1, *p2;
    int ret = 1;
    if (!!(p1 = realpath(f1, NULL))) {
        if (!!(p2 = realpath(f2, NULL))) {
            ret = PATHCMP(p1, p2);
            free(p2); /* using original free */
        }
        free(p1);
    }
    return ret;
}

#define free(p) use_nooc_free(p)
#define malloc(s) use_nooc_malloc(s)
#define realloc(p, s) use_nooc_realloc(p, s)

/********************************************************/
/* dynarrays */

ST_FUNC void dynarray_add(void *ptab, int *nb_ptr, void *data)
{
    int nb, nb_alloc;
    void **pp;

    nb = *nb_ptr;
    pp = *(void ***)ptab;
    /* every power of two we double array size */
    if ((nb & (nb - 1)) == 0) {
        if (!nb)
            nb_alloc = 1;
        else
            nb_alloc = nb * 2;
        pp = nooc_realloc(pp, nb_alloc * sizeof(void *));
        *(void***)ptab = pp;
    }
    pp[nb++] = data;
    *nb_ptr = nb;
}

ST_FUNC void dynarray_reset(void *pp, int *n)
{
    void **p;
    for (p = *(void***)pp; *n; ++p, --*n)
        if (*p)
            nooc_free(*p);
    nooc_free(*(void**)pp);
    *(void**)pp = NULL;
}

static void nooc_split_path(NOOCState *s, void *p_ary, int *p_nb_ary, const char *in)
{
    const char *p;
    do {
        int c;
        CString str;

        cstr_new(&str);
        for (p = in; c = *p, c != '\0' && c != PATHSEP[0]; ++p) {
            if (c == '{' && p[1] && p[2] == '}') {
                c = p[1], p += 2;
                if (c == 'B')
                    cstr_cat(&str, s->nooc_lib_path, -1);
                if (c == 'R')
                    cstr_cat(&str, CONFIG_SYSROOT, -1);
                if (c == 'f' && file) {
                    /* substitute current file's dir */
                    const char *f = file->true_filename;
                    const char *b = nooc_basename(f);
                    if (b > f)
                        cstr_cat(&str, f, b - f - 1);
                    else
                        cstr_cat(&str, ".", 1);
                }
            } else {
                cstr_ccat(&str, c);
            }
        }
        if (str.size) {
            cstr_ccat(&str, '\0');
            dynarray_add(p_ary, p_nb_ary, nooc_strdup(str.data));
        }
        cstr_free(&str);
        in = p+1;
    } while (*p);
}

/********************************************************/
/* warning / error */

/* warn_... option bits */
#define WARN_ON  1 /* warning is on (-Woption) */
#define WARN_ERR 2 /* warning is an error (-Werror=option) */
#define WARN_NOE 4 /* warning is not an error (-Wno-error=option) */

/* error1() modes */
enum { ERROR_WARN, ERROR_NOABORT, ERROR_ERROR };

static void error1(int mode, const char *fmt, va_list ap)
{
    BufferedFile **pf, *f;
    NOOCState *s1 = nooc_state;
    CString cs;

    nooc_exit_state(s1);

    if (mode == ERROR_WARN) {
        if (s1->warn_error)
            mode = ERROR_ERROR;
        if (s1->warn_num) {
            /* handle nooc_warning_c(warn_option)(fmt, ...) */
            int wopt = *(&s1->warn_none + s1->warn_num);
            s1->warn_num = 0;
            if (0 == (wopt & WARN_ON))
                return;
            if (wopt & WARN_ERR)
                mode = ERROR_ERROR;
            if (wopt & WARN_NOE)
                mode = ERROR_WARN;
        }
        if (s1->warn_none)
            return;
    }

    cstr_new(&cs);
    f = NULL;
    if (s1->error_set_jmp_enabled) { /* we're called while parsing a file */
        /* use upper file if inline ":asm:" or token ":paste:" */
        for (f = file; f && f->filename[0] == ':'; f = f->prev)
            ;
    }
    if (f) {
        for(pf = s1->include_stack; pf < s1->include_stack_ptr; pf++)
            cstr_printf(&cs, "In file included from %s:%d:\n",
                (*pf)->filename, (*pf)->line_num - 1);
        cstr_printf(&cs, "%s:%d: ",
            f->filename, f->line_num - !!(tok_flags & TOK_FLAG_BOL));
    } else if (s1->current_filename) {
        cstr_printf(&cs, "%s: ", s1->current_filename);
    }
    if (0 == cs.size)
        cstr_printf(&cs, "nooc: ");
    cstr_printf(&cs, mode == ERROR_WARN ? "warning: " : "error: ");
    cstr_vprintf(&cs, fmt, ap);
    if (!s1 || !s1->error_func) {
        /* default case: stderr */
        if (s1 && s1->output_type == NOOC_OUTPUT_PREPROCESS && s1->ppfp == stdout)
            printf("\n"); /* print a newline during nooc -E */
        fflush(stdout); /* flush -v output */
        fprintf(stderr, "%s\n", (char*)cs.data);
        fflush(stderr); /* print error/warning now (win32) */
    } else {
        s1->error_func(s1->error_opaque, (char*)cs.data);
    }
    cstr_free(&cs);
    if (mode != ERROR_WARN)
        s1->nb_errors++;
    if (mode == ERROR_ERROR && s1->error_set_jmp_enabled) {
        while (nb_stk_data)
            nooc_free(*(void**)stk_data[--nb_stk_data]);
        longjmp(s1->error_jmp_buf, 1);
    }
}

LIBNOOCAPI void nooc_set_error_func(NOOCState *s, void *error_opaque, NOOCErrorFunc error_func)
{
    s->error_opaque = error_opaque;
    s->error_func = error_func;
}

LIBNOOCAPI NOOCErrorFunc nooc_get_error_func(NOOCState *s)
{
    return s->error_func;
}

LIBNOOCAPI void *nooc_get_error_opaque(NOOCState *s)
{
    return s->error_opaque;
}

/* error without aborting current compilation */
PUB_FUNC int _nooc_error_noabort(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    error1(ERROR_NOABORT, fmt, ap);
    va_end(ap);
    return -1;
}

#undef _nooc_error
PUB_FUNC void _nooc_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    error1(ERROR_ERROR, fmt, ap);
    exit(1);
}
#define _nooc_error use_nooc_error_noabort

PUB_FUNC void _nooc_warning(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    error1(ERROR_WARN, fmt, ap);
    va_end(ap);
}


/********************************************************/
/* I/O layer */

ST_FUNC void nooc_open_bf(NOOCState *s1, const char *filename, int initlen)
{
    BufferedFile *bf;
    int buflen = initlen ? initlen : IO_BUF_SIZE;

    bf = nooc_mallocz(sizeof(BufferedFile) + buflen);
    bf->buf_ptr = bf->buffer;
    bf->buf_end = bf->buffer + initlen;
    bf->buf_end[0] = CH_EOB; /* put eob symbol */
    pstrcpy(bf->filename, sizeof(bf->filename), filename);
#ifdef _WIN32
    normalize_slashes(bf->filename);
#endif
    bf->true_filename = bf->filename;
    bf->line_num = 1;
    bf->ifdef_stack_ptr = s1->ifdef_stack_ptr;
    bf->fd = -1;
    bf->prev = file;
    file = bf;
    tok_flags = TOK_FLAG_BOL | TOK_FLAG_BOF;
}

ST_FUNC void nooc_close(void)
{
    NOOCState *s1 = nooc_state;
    BufferedFile *bf = file;
    if (bf->fd > 0) {
        close(bf->fd);
        total_lines += bf->line_num - 1;
    }
    if (bf->true_filename != bf->filename)
        nooc_free(bf->true_filename);
    file = bf->prev;
    nooc_free(bf);
}

static int _nooc_open(NOOCState *s1, const char *filename)
{
    int fd;
    if (strcmp(filename, "-") == 0)
        fd = 0, filename = "<stdin>";
    else
        fd = open(filename, O_RDONLY | O_BINARY);
    if ((s1->verbose == 2 && fd >= 0) || s1->verbose == 3)
        printf("%s %*s%s\n", fd < 0 ? "nf":"->",
               (int)(s1->include_stack_ptr - s1->include_stack), "", filename);
    return fd;
}

ST_FUNC int nooc_open(NOOCState *s1, const char *filename)
{
    int fd = _nooc_open(s1, filename);
    if (fd < 0)
        return -1;
    nooc_open_bf(s1, filename, 0);
    file->fd = fd;
    return 0;
}

/* compile the file opened in 'file'. Return non zero if errors. */
static int nooc_compile(NOOCState *s1, int filetype, const char *str, int fd)
{
    /* Here we enter the code section where we use the global variables for
       parsing and code generation (noocpp.c, noocgen.c, <target>-gen.c).
       Other threads need to wait until we're done.

       Alternatively we could use thread local storage for those global
       variables, which may or may not have advantages */

    nooc_enter_state(s1);
    s1->error_set_jmp_enabled = 1;

    if (setjmp(s1->error_jmp_buf) == 0) {
        s1->nb_errors = 0;

        if (fd == -1) {
            int len = strlen(str);
            nooc_open_bf(s1, "<string>", len);
            memcpy(file->buffer, str, len);
        } else {
            nooc_open_bf(s1, str, 0);
            file->fd = fd;
        }

        preprocess_start(s1, filetype);
        noocgen_init(s1);

        if (s1->output_type == NOOC_OUTPUT_PREPROCESS) {
            nooc_preprocess(s1);
        } else {
            noocelf_begin_file(s1);
            if (filetype & (AFF_TYPE_ASM | AFF_TYPE_ASMPP)) {
                nooc_assemble(s1, !!(filetype & AFF_TYPE_ASMPP));
            } else {
                noocgen_compile(s1);
            }
            noocelf_end_file(s1);
        }
    }
    noocgen_finish(s1);
    preprocess_end(s1);
    s1->error_set_jmp_enabled = 0;
    nooc_exit_state(s1);
    return s1->nb_errors != 0 ? -1 : 0;
}

LIBNOOCAPI int nooc_compile_string(NOOCState *s, const char *str)
{
    return nooc_compile(s, s->filetype, str, -1);
}

/* define a preprocessor symbol. value can be NULL, sym can be "sym=val" */
LIBNOOCAPI void nooc_define_symbol(NOOCState *s1, const char *sym, const char *value)
{
    const char *eq;
    if (NULL == (eq = strchr(sym, '=')))
        eq = strchr(sym, 0);
    if (NULL == value)
        value = *eq ? eq + 1 : "1";
    cstr_printf(&s1->cmdline_defs, "#define %.*s %s\n", (int)(eq-sym), sym, value);
}

/* undefine a preprocessor symbol */
LIBNOOCAPI void nooc_undefine_symbol(NOOCState *s1, const char *sym)
{
    cstr_printf(&s1->cmdline_defs, "#undef %s\n", sym);
}


LIBNOOCAPI NOOCState *nooc_new(void)
{
    NOOCState *s;

    s = nooc_mallocz(sizeof(NOOCState));
    if (!s)
        return NULL;
#ifdef MEM_DEBUG
    nooc_memcheck(1);
#endif

#undef gnu_ext

    s->gnu_ext = 1;
    s->nooc_ext = 1;
    s->nocommon = 1;
    s->dollars_in_identifiers = 1; /*on by default like in gcc/clang*/
    s->cversion = 199901; /* default unless -std=c11 is supplied */
    s->warn_implicit_function_declaration = 1;
    s->warn_discarded_qualifiers = 1;
    s->ms_extensions = 1;

#ifdef CHAR_IS_UNSIGNED
    s->char_is_unsigned = 1;
#endif
#ifdef NOOC_TARGET_I386
    s->seg_size = 32;
#endif
    /* enable this if you want symbols with leading underscore on windows: */
#if defined NOOC_TARGET_MACHO /* || defined NOOC_TARGET_PE */
    s->leading_underscore = 1;
#endif
#ifdef NOOC_TARGET_ARM
    s->float_abi = ARM_FLOAT_ABI;
#endif
#ifdef CONFIG_NEW_DTAGS
    s->enable_new_dtags = 1;
#endif
    s->ppfp = stdout;
    /* might be used in error() before preprocess_start() */
    s->include_stack_ptr = s->include_stack;

    nooc_set_lib_path(s, CONFIG_NOOCDIR);
    return s;
}

LIBNOOCAPI void nooc_delete(NOOCState *s1)
{
    /* free sections */
    noocelf_delete(s1);

    /* free library paths */
    dynarray_reset(&s1->library_paths, &s1->nb_library_paths);
    dynarray_reset(&s1->crt_paths, &s1->nb_crt_paths);

    /* free include paths */
    dynarray_reset(&s1->include_paths, &s1->nb_include_paths);
    dynarray_reset(&s1->sysinclude_paths, &s1->nb_sysinclude_paths);

    nooc_free(s1->nooc_lib_path);
    nooc_free(s1->soname);
    nooc_free(s1->rpath);
    nooc_free(s1->elf_entryname);
    nooc_free(s1->init_symbol);
    nooc_free(s1->fini_symbol);
    nooc_free(s1->mapfile);
    nooc_free(s1->outfile);
    nooc_free(s1->deps_outfile);
#if defined NOOC_TARGET_MACHO
    nooc_free(s1->install_name);
#endif
    dynarray_reset(&s1->files, &s1->nb_files);
    dynarray_reset(&s1->target_deps, &s1->nb_target_deps);
    dynarray_reset(&s1->pragma_libs, &s1->nb_pragma_libs);
    dynarray_reset(&s1->argv, &s1->argc);
    cstr_free(&s1->cmdline_defs);
    cstr_free(&s1->cmdline_incl);
    cstr_free(&s1->linker_arg);
#ifdef NOOC_IS_NATIVE
    /* free runtime memory */
    nooc_run_free(s1);
#endif
    nooc_free(s1->dState);
    nooc_free(s1);
#ifdef MEM_DEBUG
    nooc_memcheck(-1);
#endif
}

LIBNOOCAPI int nooc_set_output_type(NOOCState *s, int output_type)
{
#ifdef CONFIG_NOOC_PIE
    if (output_type == NOOC_OUTPUT_EXE)
        output_type |= NOOC_OUTPUT_DYN;
#endif
    s->output_type = output_type;

    if (!s->nostdinc) {
        /* default include paths */
        /* -isystem paths have already been handled */
        nooc_add_sysinclude_path(s, CONFIG_NOOC_SYSINCLUDEPATHS);
    }

    if (output_type == NOOC_OUTPUT_PREPROCESS) {
        s->do_debug = 0;
        return 0;
    }

    noocelf_new(s);
    if (s->do_debug) {
        /* add debug sections */
        nooc_debug_new(s);
    }
#ifdef CONFIG_NOOC_BCHECK
    if (s->do_bounds_check) {
        /* if bound checking, then add corresponding sections */
        noocelf_bounds_new(s);
    }
#endif

    if (output_type == NOOC_OUTPUT_OBJ) {
        /* always elf for objects */
        s->output_format = NOOC_OUTPUT_FORMAT_ELF;
        return 0;
    }

    nooc_add_library_path(s, CONFIG_NOOC_LIBPATHS);

#ifdef NOOC_TARGET_PE
# ifdef _WIN32
    /* allow linking with system dll's directly */
    nooc_add_systemdir(s);
# endif
    /* target PE has its own startup code in libnooc1.a */
    return 0;

#elif defined NOOC_TARGET_MACHO
# ifdef NOOC_IS_NATIVE
    nooc_add_macos_sdkpath(s);
# endif
    /* Mach-O with LC_MAIN doesn't need any crt startup code.  */
    return 0;

#else
    /* paths for crt objects */
    nooc_split_path(s, &s->crt_paths, &s->nb_crt_paths, CONFIG_NOOC_CRTPREFIX);

    /* add libc crt1/crti objects */
    if (output_type != NOOC_OUTPUT_MEMORY && !s->nostdlib) {
#if TARGETOS_OpenBSD
        if (output_type != NOOC_OUTPUT_DLL)
	    nooc_add_crt(s, "crt0.o");
        if (output_type == NOOC_OUTPUT_DLL)
            nooc_add_crt(s, "crtbeginS.o");
        else
            nooc_add_crt(s, "crtbegin.o");
#elif TARGETOS_FreeBSD
        if (output_type != NOOC_OUTPUT_DLL)
            nooc_add_crt(s, "crt1.o");
        nooc_add_crt(s, "crti.o");
        if (s->static_link)
            nooc_add_crt(s, "crtbeginT.o");
        else if (output_type & NOOC_OUTPUT_DYN)
            nooc_add_crt(s, "crtbeginS.o");
        else
            nooc_add_crt(s, "crtbegin.o");
#elif TARGETOS_NetBSD
        if (output_type != NOOC_OUTPUT_DLL)
            nooc_add_crt(s, "crt0.o");
        nooc_add_crt(s, "crti.o");
        if (s->static_link)
            nooc_add_crt(s, "crtbeginT.o");
        else if (output_type & NOOC_OUTPUT_DYN)
            nooc_add_crt(s, "crtbeginS.o");
        else
            nooc_add_crt(s, "crtbegin.o");
#elif defined TARGETOS_ANDROID
        if (output_type != NOOC_OUTPUT_DLL)
            nooc_add_crt(s, "crtbegin_dynamic.o");
        else
            nooc_add_crt(s, "crtbegin_so.o");
#else
        if (output_type != NOOC_OUTPUT_DLL)
            nooc_add_crt(s, "crt1.o");
        nooc_add_crt(s, "crti.o");
#endif
    }
    return 0;
#endif
}

LIBNOOCAPI int nooc_add_include_path(NOOCState *s, const char *pathname)
{
    nooc_split_path(s, &s->include_paths, &s->nb_include_paths, pathname);
    return 0;
}

LIBNOOCAPI int nooc_add_sysinclude_path(NOOCState *s, const char *pathname)
{
    nooc_split_path(s, &s->sysinclude_paths, &s->nb_sysinclude_paths, pathname);
    return 0;
}

/* add/update a 'DLLReference', Just find if level == -1  */
ST_FUNC DLLReference *nooc_add_dllref(NOOCState *s1, const char *dllname, int level)
{
    DLLReference *ref = NULL;
    int i;
    for (i = 0; i < s1->nb_loaded_dlls; i++)
        if (0 == strcmp(s1->loaded_dlls[i]->name, dllname)) {
            ref = s1->loaded_dlls[i];
            break;
        }
    if (level == -1)
        return ref;
    if (ref) {
        if (level < ref->level)
            ref->level = level;
        ref->found = 1;
        return ref;
    }
    ref = nooc_mallocz(sizeof(DLLReference) + strlen(dllname));
    strcpy(ref->name, dllname);
    dynarray_add(&s1->loaded_dlls, &s1->nb_loaded_dlls, ref);
    ref->level = level;
    ref->index = s1->nb_loaded_dlls;
    return ref;
}

/* OpenBSD: choose latest from libxxx.so.x.y versions */
#if defined TARGETOS_OpenBSD && !defined _WIN32
#include <glob.h>
static int nooc_glob_so(NOOCState *s1, const char *pattern, char *buf, int size)
{
    const char *star;
    glob_t g;
    char *p;
    int i, v, v1, v2, v3;

    star = strchr(pattern, '*');
    if (!star || glob(pattern, 0, NULL, &g))
        return -1;
    for (v = -1, i = 0; i < g.gl_pathc; ++i) {
        p = g.gl_pathv[i];
        if (2 != sscanf(p + (star - pattern), "%d.%d.%d", &v1, &v2, &v3))
            continue;
        if ((v1 = v1 * 1000 + v2) > v)
            v = v1, pstrcpy(buf, size, p);
    }
    globfree(&g);
    return v;
}
#endif

ST_FUNC int nooc_add_file_internal(NOOCState *s1, const char *filename, int flags)
{
    int fd, ret = -1;

#if defined TARGETOS_OpenBSD && !defined _WIN32
    char buf[1024];
    if (nooc_glob_so(s1, filename, buf, sizeof buf) >= 0)
        filename = buf;
#endif

    /* ignore binary files with -E */
    if (s1->output_type == NOOC_OUTPUT_PREPROCESS
        && (flags & AFF_TYPE_BIN))
        return 0;

    /* open the file */
    fd = _nooc_open(s1, filename);
    if (fd < 0) {
        if (flags & AFF_PRINT_ERROR)
            nooc_error_noabort("file '%s' not found", filename);
        return ret;
    }

    s1->current_filename = filename;
    if (flags & AFF_TYPE_BIN) {
        ElfW(Ehdr) ehdr;
        int obj_type;

        obj_type = nooc_object_type(fd, &ehdr);
        lseek(fd, 0, SEEK_SET);

        switch (obj_type) {

        case AFF_BINTYPE_REL:
            ret = nooc_load_object_file(s1, fd, 0);
            break;

        case AFF_BINTYPE_AR:
            ret = nooc_load_archive(s1, fd, !(flags & AFF_WHOLE_ARCHIVE));
            break;

#ifdef NOOC_TARGET_PE
        default:
            ret = pe_load_file(s1, fd, filename);
            goto check_success;

#elif defined NOOC_TARGET_MACHO
        case AFF_BINTYPE_DYN:
        case_dyn_or_tbd:
            if (s1->output_type == NOOC_OUTPUT_MEMORY) {
#ifdef NOOC_IS_NATIVE
                void* dl;
                const char* soname = filename;
                if (obj_type != AFF_BINTYPE_DYN)
                    soname = macho_tbd_soname(filename);
                dl = dlopen(soname, RTLD_GLOBAL | RTLD_LAZY);
                if (dl)
                    nooc_add_dllref(s1, soname, 0)->handle = dl, ret = 0;
	        if (filename != soname)
		    nooc_free((void *)soname);
#endif
            } else if (obj_type == AFF_BINTYPE_DYN) {
                ret = macho_load_dll(s1, fd, filename, (flags & AFF_REFERENCED_DLL) != 0);
            } else {
                ret = macho_load_tbd(s1, fd, filename, (flags & AFF_REFERENCED_DLL) != 0);
            }
            goto check_success;
        default:
        {
            const char *ext = nooc_fileextension(filename);
            if (!strcmp(ext, ".tbd"))
                goto case_dyn_or_tbd;
            if (!strcmp(ext, ".dylib")) {
                obj_type = AFF_BINTYPE_DYN;
                goto case_dyn_or_tbd;
            }
            goto check_success;
        }

#else /* unix */
        case AFF_BINTYPE_DYN:
            if (s1->output_type == NOOC_OUTPUT_MEMORY) {
#ifdef NOOC_IS_NATIVE
                void* dl = dlopen(filename, RTLD_GLOBAL | RTLD_LAZY);
                if (dl)
                    nooc_add_dllref(s1, filename, 0)->handle = dl, ret = 0;
#endif
            } else
                ret = nooc_load_dll(s1, fd, filename, (flags & AFF_REFERENCED_DLL) != 0);
            break;

        default:
            /* as GNU ld, consider it is an ld script if not recognized */
            ret = nooc_load_ldscript(s1, fd);
            goto check_success;

#endif /* pe / macos / unix */

check_success:
            if (ret < 0)
                nooc_error_noabort("%s: unrecognized file type", filename);
            break;

#ifdef NOOC_TARGET_COFF
        case AFF_BINTYPE_C67:
            ret = nooc_load_coff(s1, fd);
            break;
#endif
        }
        close(fd);
    } else {
        /* update target deps */
        dynarray_add(&s1->target_deps, &s1->nb_target_deps, nooc_strdup(filename));
        ret = nooc_compile(s1, flags, filename, fd);
    }
    s1->current_filename = NULL;
    return ret;
}

LIBNOOCAPI int nooc_add_file(NOOCState *s, const char *filename)
{
    int filetype = s->filetype;
    if (0 == (filetype & AFF_TYPE_MASK)) {
        /* use a file extension to detect a filetype */
        const char *ext = nooc_fileextension(filename);
        if (ext[0]) {
            ext++;
            if (!strcmp(ext, "S"))
                filetype = AFF_TYPE_ASMPP;
            else if (!strcmp(ext, "s"))
                filetype = AFF_TYPE_ASM;
            else if (!PATHCMP(ext, "nc")
                     || !PATHCMP(ext, "h")
                     || !PATHCMP(ext, "i"))
                filetype = AFF_TYPE_C;
            else
                filetype |= AFF_TYPE_BIN;
        } else {
            filetype = AFF_TYPE_C;
        }
    }
    return nooc_add_file_internal(s, filename, filetype | AFF_PRINT_ERROR);
}

LIBNOOCAPI int nooc_add_library_path(NOOCState *s, const char *pathname)
{
    nooc_split_path(s, &s->library_paths, &s->nb_library_paths, pathname);
    return 0;
}

static int nooc_add_library_internal(NOOCState *s, const char *fmt,
    const char *filename, int flags, char **paths, int nb_paths)
{
    char buf[1024];
    int i;

    for(i = 0; i < nb_paths; i++) {
        snprintf(buf, sizeof(buf), fmt, paths[i], filename);
        if (nooc_add_file_internal(s, buf, flags | AFF_TYPE_BIN) == 0)
            return 0;
    }
    return -1;
}

/* find and load a dll. Return non zero if not found */
ST_FUNC int nooc_add_dll(NOOCState *s, const char *filename, int flags)
{
    return nooc_add_library_internal(s, "%s/%s", filename, flags,
        s->library_paths, s->nb_library_paths);
}

/* find [cross-]libnooc1.a and nooc helper objects in library path */
ST_FUNC void nooc_add_support(NOOCState *s1, const char *filename)
{
    char buf[100];
    if (CONFIG_NOOC_CROSSPREFIX[0])
        filename = strcat(strcpy(buf, CONFIG_NOOC_CROSSPREFIX), filename);
    if (nooc_add_dll(s1, filename, 0) < 0)
        nooc_error_noabort("%s not found", filename);
}

#if !defined NOOC_TARGET_PE && !defined NOOC_TARGET_MACHO
ST_FUNC int nooc_add_crt(NOOCState *s1, const char *filename)
{
    if (-1 == nooc_add_library_internal(s1, "%s/%s",
        filename, 0, s1->crt_paths, s1->nb_crt_paths))
        return nooc_error_noabort("file '%s' not found", filename);
    return 0;
}
#endif

/* the library name is the same as the argument of the '-l' option */
LIBNOOCAPI int nooc_add_library(NOOCState *s, const char *libraryname)
{
#if defined NOOC_TARGET_PE
    static const char * const libs[] = { "%s/%s.def", "%s/lib%s.def", "%s/%s.dll", "%s/lib%s.dll", "%s/lib%s.a", NULL };
    const char * const *pp = s->static_link ? libs + 4 : libs;
#elif defined NOOC_TARGET_MACHO
    static const char * const libs[] = { "%s/lib%s.dylib", "%s/lib%s.tbd", "%s/lib%s.a", NULL };
    const char * const *pp = s->static_link ? libs + 2 : libs;
#elif defined TARGETOS_OpenBSD
    static const char * const libs[] = { "%s/lib%s.so.*", "%s/lib%s.a", NULL };
    const char * const *pp = s->static_link ? libs + 1 : libs;
#else
    static const char * const libs[] = { "%s/lib%s.so", "%s/lib%s.a", NULL };
    const char * const *pp = s->static_link ? libs + 1 : libs;
#endif
    int flags = s->filetype & AFF_WHOLE_ARCHIVE;
    while (*pp) {
        if (0 == nooc_add_library_internal(s, *pp,
            libraryname, flags, s->library_paths, s->nb_library_paths))
            return 0;
        ++pp;
    }
    return -1;
}

PUB_FUNC int nooc_add_library_err(NOOCState *s1, const char *libname)
{
    int ret = nooc_add_library(s1, libname);
    if (ret < 0)
        nooc_error_noabort("library '%s' not found", libname);
    return ret;
}

/* handle #pragma comment(lib,) */
ST_FUNC void nooc_add_pragma_libs(NOOCState *s1)
{
    int i;
    for (i = 0; i < s1->nb_pragma_libs; i++)
        nooc_add_library_err(s1, s1->pragma_libs[i]);
}

LIBNOOCAPI int nooc_add_symbol(NOOCState *s1, const char *name, const void *val)
{
#ifdef NOOC_TARGET_PE
    /* On x86_64 'val' might not be reachable with a 32bit offset.
       So it is handled here as if it were in a DLL. */
    pe_putimport(s1, 0, name, (uintptr_t)val);
#else
    char buf[256];
    if (s1->leading_underscore) {
        buf[0] = '_';
        pstrcpy(buf + 1, sizeof(buf) - 1, name);
        name = buf;
    }
    set_global_sym(s1, name, NULL, (addr_t)(uintptr_t)val); /* NULL: SHN_ABS */
#endif
    return 0;
}

LIBNOOCAPI void nooc_set_lib_path(NOOCState *s, const char *path)
{
    nooc_free(s->nooc_lib_path);
    s->nooc_lib_path = nooc_strdup(path);
}

/********************************************************/
/* options parser */

static int strstart(const char *val, const char **str)
{
    const char *p, *q;
    p = *str;
    q = val;
    while (*q) {
        if (*p != *q)
            return 0;
        p++;
        q++;
    }
    *str = p;
    return 1;
}

/* Like strstart, but automatically takes into account that ld options can
 *
 * - start with double or single dash (e.g. '--soname' or '-soname')
 * - arguments can be given as separate or after '=' (e.g. '-Wl,-soname,x.so'
 *   or '-Wl,-soname=x.so')
 *
 * you provide `val` always in 'option[=]' form (no leading -)
 */
static int link_option(const char *str, const char *val, const char **ptr)
{
    const char *p, *q;
    int ret;

    /* there should be 1 or 2 dashes */
    if (*str++ != '-')
        return 0;
    if (*str == '-')
        str++;

    /* then str & val should match (potentially up to '=') */
    p = str;
    q = val;

    ret = 1;
    if (q[0] == '?') {
        ++q;
        if (strstart("no-", &p))
            ret = -1;
    }

    while (*q != '\0' && *q != '=') {
        if (*p != *q)
            return 0;
        p++;
        q++;
    }

    /* '=' near eos means ',' or '=' is ok */
    if (*q == '=') {
        if (*p == 0)
            *ptr = p;
        if (*p != ',' && *p != '=')
            return 0;
        p++;
    } else if (*p) {
        return 0;
    }
    *ptr = p;
    return ret;
}

static const char *skip_linker_arg(const char **str)
{
    const char *s1 = *str;
    const char *s2 = strchr(s1, ',');
    *str = s2 ? s2++ : (s2 = s1 + strlen(s1));
    return s2;
}

static void copy_linker_arg(char **pp, const char *s, int sep)
{
    const char *q = s;
    char *p = *pp;
    int l = 0;
    if (p && sep)
        p[l = strlen(p)] = sep, ++l;
    skip_linker_arg(&q);
    pstrncpy(l + (*pp = nooc_realloc(p, q - s + l + 1)), s, q - s);
}

static void args_parser_add_file(NOOCState *s, const char* filename, int filetype)
{
    struct filespec *f = nooc_malloc(sizeof *f + strlen(filename));
    f->type = filetype;
    strcpy(f->name, filename);
    dynarray_add(&s->files, &s->nb_files, f);
}

/* set linker options */
static int nooc_set_linker(NOOCState *s, const char *option)
{
    NOOCState *s1 = s;
    while (*option) {

        const char *p = NULL;
        char *end = NULL;
        int ignoring = 0;
        int ret;

        if (link_option(option, "Bsymbolic", &p)) {
            s->symbolic = 1;
        } else if (link_option(option, "nostdlib", &p)) {
            s->nostdlib = 1;
        } else if (link_option(option, "e=", &p)
               ||  link_option(option, "entry=", &p)) {
            copy_linker_arg(&s->elf_entryname, p, 0);
        } else if (link_option(option, "fini=", &p)) {
            copy_linker_arg(&s->fini_symbol, p, 0);
            ignoring = 1;
        } else if (link_option(option, "image-base=", &p)
                || link_option(option, "Ttext=", &p)) {
            s->text_addr = strtoull(p, &end, 16);
            s->has_text_addr = 1;
        } else if (link_option(option, "init=", &p)) {
            copy_linker_arg(&s->init_symbol, p, 0);
            ignoring = 1;
        } else if (link_option(option, "Map=", &p)) {
            copy_linker_arg(&s->mapfile, p, 0);
            ignoring = 1;
        } else if (link_option(option, "oformat=", &p)) {
#if defined(NOOC_TARGET_PE)
            if (strstart("pe-", &p)) {
#elif PTR_SIZE == 8
            if (strstart("elf64-", &p)) {
#else
            if (strstart("elf32-", &p)) {
#endif
                s->output_format = NOOC_OUTPUT_FORMAT_ELF;
            } else if (!strcmp(p, "binary")) {
                s->output_format = NOOC_OUTPUT_FORMAT_BINARY;
#ifdef NOOC_TARGET_COFF
            } else if (!strcmp(p, "coff")) {
                s->output_format = NOOC_OUTPUT_FORMAT_COFF;
#endif
            } else
                goto err;

        } else if (link_option(option, "as-needed", &p)) {
            ignoring = 1;
        } else if (link_option(option, "O", &p)) {
            ignoring = 1;
        } else if (link_option(option, "export-all-symbols", &p)) {
            s->rdynamic = 1;
        } else if (link_option(option, "export-dynamic", &p)) {
            s->rdynamic = 1;
        } else if (link_option(option, "rpath=", &p)) {
            copy_linker_arg(&s->rpath, p, ':');
        } else if (link_option(option, "enable-new-dtags", &p)) {
            s->enable_new_dtags = 1;
        } else if (link_option(option, "section-alignment=", &p)) {
            s->section_align = strtoul(p, &end, 16);
        } else if (link_option(option, "soname=", &p)) {
            copy_linker_arg(&s->soname, p, 0);
        } else if (link_option(option, "install_name=", &p)) {
            copy_linker_arg(&s->soname, p, 0);
#ifdef NOOC_TARGET_PE
        } else if (link_option(option, "large-address-aware", &p)) {
            s->pe_characteristics |= 0x20;
        } else if (link_option(option, "file-alignment=", &p)) {
            s->pe_file_align = strtoul(p, &end, 16);
        } else if (link_option(option, "stack=", &p)) {
            s->pe_stack_size = strtoul(p, &end, 10);
        } else if (link_option(option, "subsystem=", &p)) {
#if defined(NOOC_TARGET_I386) || defined(NOOC_TARGET_X86_64)
            if (!strcmp(p, "native")) {
                s->pe_subsystem = 1;
            } else if (!strcmp(p, "console")) {
                s->pe_subsystem = 3;
            } else if (!strcmp(p, "gui") || !strcmp(p, "windows")) {
                s->pe_subsystem = 2;
            } else if (!strcmp(p, "posix")) {
                s->pe_subsystem = 7;
            } else if (!strcmp(p, "efiapp")) {
                s->pe_subsystem = 10;
            } else if (!strcmp(p, "efiboot")) {
                s->pe_subsystem = 11;
            } else if (!strcmp(p, "efiruntime")) {
                s->pe_subsystem = 12;
            } else if (!strcmp(p, "efirom")) {
                s->pe_subsystem = 13;
#elif defined(NOOC_TARGET_ARM)
            if (!strcmp(p, "wince")) {
                s->pe_subsystem = 9;
#endif
            } else
                goto err;
#endif
#ifdef NOOC_TARGET_MACHO
        } else if (link_option(option, "all_load", &p)) {
	    s->filetype |= AFF_WHOLE_ARCHIVE;
        } else if (link_option(option, "force_load", &p)) {
	    s->filetype |= AFF_WHOLE_ARCHIVE;
            args_parser_add_file(s, p, AFF_TYPE_LIB | (s->filetype & ~AFF_TYPE_MASK));
            s->nb_libraries++;
        } else if (link_option(option, "single_module", &p)) {
            ignoring = 1;
#endif
        } else if (ret = link_option(option, "?whole-archive", &p), ret) {
            if (ret > 0)
                s->filetype |= AFF_WHOLE_ARCHIVE;
            else
                s->filetype &= ~AFF_WHOLE_ARCHIVE;
        } else if (link_option(option, "z=", &p)) {
            ignoring = 1;
        } else if (p) {
            return 0;
        } else {
    err:
            return nooc_error_noabort("unsupported linker option '%s'", option);
        }
        if (ignoring)
            nooc_warning_c(warn_unsupported)("unsupported linker option '%s'", option);
        option = skip_linker_arg(&p);
    }
    return 1;
}

typedef struct NOOCOption {
    const char *name;
    uint16_t index;
    uint16_t flags;
} NOOCOption;

enum {
    NOOC_OPTION_ignored = 0,
    NOOC_OPTION_HELP,
    NOOC_OPTION_HELP2,
    NOOC_OPTION_v,
    NOOC_OPTION_I,
    NOOC_OPTION_D,
    NOOC_OPTION_U,
    NOOC_OPTION_P,
    NOOC_OPTION_L,
    NOOC_OPTION_B,
    NOOC_OPTION_l,
    NOOC_OPTION_bench,
    NOOC_OPTION_bt,
    NOOC_OPTION_b,
    NOOC_OPTION_ba,
    NOOC_OPTION_g,
    NOOC_OPTION_c,
    NOOC_OPTION_dumpversion,
    NOOC_OPTION_d,
    NOOC_OPTION_static,
    NOOC_OPTION_std,
    NOOC_OPTION_shared,
    NOOC_OPTION_soname,
    NOOC_OPTION_o,
    NOOC_OPTION_r,
    NOOC_OPTION_Wl,
    NOOC_OPTION_Wp,
    NOOC_OPTION_W,
    NOOC_OPTION_O,
    NOOC_OPTION_mfloat_abi,
    NOOC_OPTION_m,
    NOOC_OPTION_f,
    NOOC_OPTION_isystem,
    NOOC_OPTION_iwithprefix,
    NOOC_OPTION_include,
    NOOC_OPTION_nostdinc,
    NOOC_OPTION_nostdlib,
    NOOC_OPTION_print_search_dirs,
    NOOC_OPTION_rdynamic,
    NOOC_OPTION_pthread,
    NOOC_OPTION_run,
    NOOC_OPTION_w,
    NOOC_OPTION_E,
    NOOC_OPTION_M,
    NOOC_OPTION_MD,
    NOOC_OPTION_MF,
    NOOC_OPTION_MM,
    NOOC_OPTION_MMD,
    NOOC_OPTION_x,
    NOOC_OPTION_ar,
    NOOC_OPTION_impdef,
    NOOC_OPTION_dynamiclib,
    NOOC_OPTION_flat_namespace,
    NOOC_OPTION_two_levelnamespace,
    NOOC_OPTION_undefined,
    NOOC_OPTION_install_name,
    NOOC_OPTION_compatibility_version ,
    NOOC_OPTION_current_version,
};

#define NOOC_OPTION_HAS_ARG 0x0001
#define NOOC_OPTION_NOSEP   0x0002 /* cannot have space before option and arg */

static const NOOCOption nooc_options[] = {
    { "h", NOOC_OPTION_HELP, 0 },
    { "-help", NOOC_OPTION_HELP, 0 },
    { "?", NOOC_OPTION_HELP, 0 },
    { "hh", NOOC_OPTION_HELP2, 0 },
    { "v", NOOC_OPTION_v, NOOC_OPTION_HAS_ARG | NOOC_OPTION_NOSEP },
    { "-version", NOOC_OPTION_v, 0 }, /* handle as verbose, also prints version*/
    { "I", NOOC_OPTION_I, NOOC_OPTION_HAS_ARG },
    { "D", NOOC_OPTION_D, NOOC_OPTION_HAS_ARG },
    { "U", NOOC_OPTION_U, NOOC_OPTION_HAS_ARG },
    { "P", NOOC_OPTION_P, NOOC_OPTION_HAS_ARG | NOOC_OPTION_NOSEP },
    { "L", NOOC_OPTION_L, NOOC_OPTION_HAS_ARG },
    { "B", NOOC_OPTION_B, NOOC_OPTION_HAS_ARG },
    { "l", NOOC_OPTION_l, NOOC_OPTION_HAS_ARG },
    { "bench", NOOC_OPTION_bench, 0 },
#ifdef CONFIG_NOOC_BACKTRACE
    { "bt", NOOC_OPTION_bt, NOOC_OPTION_HAS_ARG | NOOC_OPTION_NOSEP },
#endif
#ifdef CONFIG_NOOC_BCHECK
    { "b", NOOC_OPTION_b, 0 },
#endif
    { "g", NOOC_OPTION_g, NOOC_OPTION_HAS_ARG | NOOC_OPTION_NOSEP },
#ifdef NOOC_TARGET_MACHO
    { "compatibility_version", NOOC_OPTION_compatibility_version, NOOC_OPTION_HAS_ARG },
    { "current_version", NOOC_OPTION_current_version, NOOC_OPTION_HAS_ARG },
#endif
    { "c", NOOC_OPTION_c, 0 },
#ifdef NOOC_TARGET_MACHO
    { "dynamiclib", NOOC_OPTION_dynamiclib, 0 },
#endif
    { "dumpversion", NOOC_OPTION_dumpversion, 0},
    { "d", NOOC_OPTION_d, NOOC_OPTION_HAS_ARG | NOOC_OPTION_NOSEP },
    { "static", NOOC_OPTION_static, 0 },
    { "std", NOOC_OPTION_std, NOOC_OPTION_HAS_ARG | NOOC_OPTION_NOSEP },
    { "shared", NOOC_OPTION_shared, 0 },
    { "soname", NOOC_OPTION_soname, NOOC_OPTION_HAS_ARG },
    { "o", NOOC_OPTION_o, NOOC_OPTION_HAS_ARG },
    { "pthread", NOOC_OPTION_pthread, 0},
    { "run", NOOC_OPTION_run, NOOC_OPTION_HAS_ARG | NOOC_OPTION_NOSEP },
    { "rdynamic", NOOC_OPTION_rdynamic, 0 },
    { "r", NOOC_OPTION_r, 0 },
    { "Wl,", NOOC_OPTION_Wl, NOOC_OPTION_HAS_ARG | NOOC_OPTION_NOSEP },
    { "Wp,", NOOC_OPTION_Wp, NOOC_OPTION_HAS_ARG | NOOC_OPTION_NOSEP },
    { "W", NOOC_OPTION_W, NOOC_OPTION_HAS_ARG | NOOC_OPTION_NOSEP },
    { "O", NOOC_OPTION_O, NOOC_OPTION_HAS_ARG | NOOC_OPTION_NOSEP },
#ifdef NOOC_TARGET_ARM
    { "mfloat-abi", NOOC_OPTION_mfloat_abi, NOOC_OPTION_HAS_ARG },
#endif
    { "m", NOOC_OPTION_m, NOOC_OPTION_HAS_ARG | NOOC_OPTION_NOSEP },
#ifdef NOOC_TARGET_MACHO
    { "flat_namespace", NOOC_OPTION_flat_namespace, 0 },
#endif
    { "f", NOOC_OPTION_f, NOOC_OPTION_HAS_ARG | NOOC_OPTION_NOSEP },
    { "isystem", NOOC_OPTION_isystem, NOOC_OPTION_HAS_ARG },
    { "include", NOOC_OPTION_include, NOOC_OPTION_HAS_ARG },
    { "nostdinc", NOOC_OPTION_nostdinc, 0 },
    { "nostdlib", NOOC_OPTION_nostdlib, 0 },
    { "print-search-dirs", NOOC_OPTION_print_search_dirs, 0 },
    { "w", NOOC_OPTION_w, 0 },
    { "E", NOOC_OPTION_E, 0},
    { "M", NOOC_OPTION_M, 0},
    { "MD", NOOC_OPTION_MD, 0},
    { "MF", NOOC_OPTION_MF, NOOC_OPTION_HAS_ARG },
    { "MM", NOOC_OPTION_MM, 0},
    { "MMD", NOOC_OPTION_MMD, 0},
    { "x", NOOC_OPTION_x, NOOC_OPTION_HAS_ARG },
    { "ar", NOOC_OPTION_ar, 0},
#ifdef NOOC_TARGET_PE
    { "impdef", NOOC_OPTION_impdef, 0},
#endif
#ifdef NOOC_TARGET_MACHO
    { "install_name", NOOC_OPTION_install_name, NOOC_OPTION_HAS_ARG },
    { "two_levelnamespace", NOOC_OPTION_two_levelnamespace, 0 },
    { "undefined", NOOC_OPTION_undefined, NOOC_OPTION_HAS_ARG },
#endif
    /* ignored (silently, except after -Wunsupported) */
    { "arch", 0, NOOC_OPTION_HAS_ARG},
    { "C", 0, 0 },
    { "-param", 0, NOOC_OPTION_HAS_ARG },
    { "pedantic", 0, 0 },
    { "pipe", 0, 0 },
    { "s", 0, 0 },
    { "traditional", 0, 0 },
    { NULL, 0, 0 },
};

typedef struct FlagDef {
    uint16_t offset;
    uint16_t flags;
    const char *name;
} FlagDef;

#define WD_ALL    0x0001 /* warning is activated when using -Wall */
#define FD_INVERT 0x0002 /* invert value before storing */

static const FlagDef options_W[] = {
    { offsetof(NOOCState, warn_all), WD_ALL, "all" },
    { offsetof(NOOCState, warn_error), 0, "error" },
    { offsetof(NOOCState, warn_write_strings), 0, "write-strings" },
    { offsetof(NOOCState, warn_unsupported), 0, "unsupported" },
    { offsetof(NOOCState, warn_implicit_function_declaration), WD_ALL, "implicit-function-declaration" },
    { offsetof(NOOCState, warn_discarded_qualifiers), WD_ALL, "discarded-qualifiers" },
    { 0, 0, NULL }
};

static const FlagDef options_f[] = {
    { offsetof(NOOCState, char_is_unsigned), 0, "unsigned-char" },
    { offsetof(NOOCState, char_is_unsigned), FD_INVERT, "signed-char" },
    { offsetof(NOOCState, nocommon), FD_INVERT, "common" },
    { offsetof(NOOCState, leading_underscore), 0, "leading-underscore" },
    { offsetof(NOOCState, ms_extensions), 0, "ms-extensions" },
    { offsetof(NOOCState, dollars_in_identifiers), 0, "dollars-in-identifiers" },
    { offsetof(NOOCState, test_coverage), 0, "test-coverage" },
    { 0, 0, NULL }
};

static const FlagDef options_m[] = {
    { offsetof(NOOCState, ms_bitfields), 0, "ms-bitfields" },
#ifdef NOOC_TARGET_X86_64
    { offsetof(NOOCState, nosse), FD_INVERT, "sse" },
#endif
    { 0, 0, NULL }
};

static int set_flag(NOOCState *s, const FlagDef *flags, const char *name)
{
    int value, mask, ret;
    const FlagDef *p;
    const char *r;
    unsigned char *f;

    r = name, value = !strstart("no-", &r), mask = 0;

    /* when called with options_W, look for -W[no-]error=<option> */
    if ((flags->flags & WD_ALL) && strstart("error=", &r))
        value = value ? WARN_ON|WARN_ERR : WARN_NOE, mask = WARN_ON;

    for (ret = -1, p = flags; p->name; ++p) {
        if (ret) {
            if (strcmp(r, p->name))
                continue;
        } else {
            if (0 == (p->flags & WD_ALL))
                continue;
        }

        f = (unsigned char *)s + p->offset;
        *f = (*f & mask) | (value ^ !!(p->flags & FD_INVERT));

        if (ret) {
            ret = 0;
            if (strcmp(r, "all"))
                break;
        }
    }
    return ret;
}

static int args_parser_make_argv(const char *r, int *argc, char ***argv)
{
    int ret = 0, q, c;
    CString str;
    for(;;) {
        while (c = (unsigned char)*r, c && c <= ' ')
          ++r;
        if (c == 0)
            break;
        q = 0;
        cstr_new(&str);
        while (c = (unsigned char)*r, c) {
            ++r;
            if (c == '\\' && (*r == '"' || *r == '\\')) {
                c = *r++;
            } else if (c == '"') {
                q = !q;
                continue;
            } else if (q == 0 && c <= ' ') {
                break;
            }
            cstr_ccat(&str, c);
        }
        cstr_ccat(&str, 0);
        //printf("<%s>\n", str.data), fflush(stdout);
        dynarray_add(argv, argc, nooc_strdup(str.data));
        cstr_free(&str);
        ++ret;
    }
    return ret;
}

/* read list file */
static int args_parser_listfile(NOOCState *s,
    const char *filename, int optind, int *pargc, char ***pargv)
{
    NOOCState *s1 = s;
    int fd, i;
    char *p;
    int argc = 0;
    char **argv = NULL;

    fd = open(filename, O_RDONLY | O_BINARY);
    if (fd < 0)
        return nooc_error_noabort("listfile '%s' not found", filename);

    p = nooc_load_text(fd);
    for (i = 0; i < *pargc; ++i)
        if (i == optind)
            args_parser_make_argv(p, &argc, &argv);
        else
            dynarray_add(&argv, &argc, nooc_strdup((*pargv)[i]));

    nooc_free(p);
    dynarray_reset(&s->argv, &s->argc);
    *pargc = s->argc = argc, *pargv = s->argv = argv;
    return 0;
}

#if defined NOOC_TARGET_MACHO
static uint32_t parse_version(NOOCState *s1, const char *version)
{
    uint32_t a = 0;
    uint32_t b = 0;
    uint32_t c = 0;
    char* last;

    a = strtoul(version, &last, 10);
    if (*last == '.') {
        b = strtoul(&last[1], &last, 10);
        if (*last == '.')
             c = strtoul(&last[1], &last, 10);
    }
    if (*last || a > 0xffff || b > 0xff || c > 0xff)
        nooc_error_noabort("version a.b.c not correct: %s", version);
    return (a << 16) | (b << 8) | c;
}
#endif

PUB_FUNC int nooc_parse_args(NOOCState *s, int *pargc, char ***pargv, int optind)
{
    NOOCState *s1 = s;
    const NOOCOption *popt;
    const char *optarg, *r;
    const char *run = NULL;
    int x;
    int tool = 0, arg_start = 0, noaction = optind;
    char **argv = *pargv;
    int argc = *pargc;

    cstr_reset(&s->linker_arg);

    while (optind < argc) {
        r = argv[optind];
        if (r[0] == '@' && r[1] != '\0') {
            if (args_parser_listfile(s, r + 1, optind, &argc, &argv))
                return -1;
            continue;
        }
        optind++;
        if (tool) {
            if (r[0] == '-' && r[1] == 'v' && r[2] == 0)
                ++s->verbose;
            continue;
        }
reparse:
        if (r[0] != '-' || r[1] == '\0') {
            args_parser_add_file(s, r, s->filetype);
            if (run) {
dorun:
                if (nooc_set_options(s, run))
                    return -1;
                arg_start = optind - 1;
                break;
            }
            continue;
        }

        /* allow "nooc files... -run -- args ..." */
        if (r[1] == '-' && r[2] == '\0' && run)
            goto dorun;

        /* find option in table */
        for(popt = nooc_options; ; ++popt) {
            const char *p1 = popt->name;
            const char *r1 = r + 1;
            if (p1 == NULL)
                return nooc_error_noabort("invalid option -- '%s'", r);
            if (!strstart(p1, &r1))
                continue;
            optarg = r1;
            if (popt->flags & NOOC_OPTION_HAS_ARG) {
                if (*r1 == '\0' && !(popt->flags & NOOC_OPTION_NOSEP)) {
                    if (optind >= argc)
                arg_err:
                        return nooc_error_noabort("argument to '%s' is missing", r);
                    optarg = argv[optind++];
                }
            } else if (*r1 != '\0')
                continue;
            break;
        }

        switch(popt->index) {
        case NOOC_OPTION_HELP:
            x = OPT_HELP;
            goto extra_action;
        case NOOC_OPTION_HELP2:
            x = OPT_HELP2;
            goto extra_action;
        case NOOC_OPTION_I:
            nooc_add_include_path(s, optarg);
            break;
        case NOOC_OPTION_D:
            nooc_define_symbol(s, optarg, NULL);
            break;
        case NOOC_OPTION_U:
            nooc_undefine_symbol(s, optarg);
            break;
        case NOOC_OPTION_L:
            nooc_add_library_path(s, optarg);
            break;
        case NOOC_OPTION_B:
            /* set nooc utilities path (mainly for nooc development) */
            nooc_set_lib_path(s, optarg);
            ++noaction;
            break;
        case NOOC_OPTION_l:
            args_parser_add_file(s, optarg, AFF_TYPE_LIB | (s->filetype & ~AFF_TYPE_MASK));
            s->nb_libraries++;
            break;
        case NOOC_OPTION_pthread:
            s->option_pthread = 1;
            break;
        case NOOC_OPTION_bench:
            s->do_bench = 1;
            break;
#ifdef CONFIG_NOOC_BACKTRACE
        case NOOC_OPTION_bt:
            s->rt_num_callers = atoi(optarg); /* zero = default (6) */
        enable_backtrace:
            s->do_backtrace = 1;
            s->do_debug = 1;
	    s->dwarf = DWARF_VERSION;
            break;
#ifdef CONFIG_NOOC_BCHECK
        case NOOC_OPTION_b:
            s->do_bounds_check = 1;
            goto enable_backtrace;
#endif
#endif
        case NOOC_OPTION_g:
            s->do_debug = 2;
            s->dwarf = DWARF_VERSION;
            if (strstart("dwarf", &optarg)) {
                s->dwarf = (*optarg) ? (0 - atoi(optarg)) : DEFAULT_DWARF_VERSION;
            } else if (isnum(*optarg)) {
                x = *optarg - '0';
                /* -g0 = no info, -g1 = lines/functions only, -g2 = full info */
                if (x <= 2)
                    s->do_debug = x;
#ifdef NOOC_TARGET_PE
            } else if (0 == strcmp(".pdb", optarg)) {
                s->dwarf = 5, s->do_debug |= 16;
#endif
            }
            break;
        case NOOC_OPTION_c:
            x = NOOC_OUTPUT_OBJ;
        set_output_type:
            if (s->output_type)
                nooc_warning("-%s: overriding compiler action already specified", popt->name);
            s->output_type = x;
            break;
        case NOOC_OPTION_d:
            if (*optarg == 'D')
                s->dflag = 3;
            else if (*optarg == 'M')
                s->dflag = 7;
            else if (*optarg == 't')
                s->dflag = 16;
            else if (isnum(*optarg))
                s->g_debug |= atoi(optarg);
            else
                goto unsupported_option;
            break;
        case NOOC_OPTION_static:
            s->static_link = 1;
            break;
        case NOOC_OPTION_std:
            if (strcmp(optarg, "=c11") == 0)
                s->cversion = 201112;
            break;
        case NOOC_OPTION_shared:
            x = NOOC_OUTPUT_DLL;
            goto set_output_type;
        case NOOC_OPTION_soname:
            s->soname = nooc_strdup(optarg);
            break;
        case NOOC_OPTION_o:
            if (s->outfile) {
                nooc_warning("multiple -o option");
                nooc_free(s->outfile);
            }
            s->outfile = nooc_strdup(optarg);
            break;
        case NOOC_OPTION_r:
            /* generate a .o merging several output files */
            s->option_r = 1;
            x = NOOC_OUTPUT_OBJ;
            goto set_output_type;
        case NOOC_OPTION_isystem:
            nooc_add_sysinclude_path(s, optarg);
            break;
        case NOOC_OPTION_include:
            cstr_printf(&s->cmdline_incl, "#include \"%s\"\n", optarg);
            break;
        case NOOC_OPTION_nostdinc:
            s->nostdinc = 1;
            break;
        case NOOC_OPTION_nostdlib:
            s->nostdlib = 1;
            break;
        case NOOC_OPTION_run:
#ifndef NOOC_IS_NATIVE
            return nooc_error_noabort("-run is not available in a cross compiler");
#else
            run = optarg;
            x = NOOC_OUTPUT_MEMORY;
            goto set_output_type;
#endif
        case NOOC_OPTION_v:
            do ++s->verbose; while (*optarg++ == 'v');
            ++noaction;
            break;
        case NOOC_OPTION_f:
            if (set_flag(s, options_f, optarg) < 0)
                goto unsupported_option;
            break;
#ifdef NOOC_TARGET_ARM
        case NOOC_OPTION_mfloat_abi:
            /* nooc doesn't support soft float yet */
            if (!strcmp(optarg, "softfp")) {
                s->float_abi = ARM_SOFTFP_FLOAT;
            } else if (!strcmp(optarg, "hard"))
                s->float_abi = ARM_HARD_FLOAT;
            else
                return nooc_error_noabort("unsupported float abi '%s'", optarg);
            break;
#endif
        case NOOC_OPTION_m:
            if (set_flag(s, options_m, optarg) < 0) {
                if (x = atoi(optarg), x != 32 && x != 64)
                    goto unsupported_option;
                if (PTR_SIZE != x/8)
                    return x;
                ++noaction;
            }
            break;
        case NOOC_OPTION_W:
            s->warn_none = 0;
            if (optarg[0] && set_flag(s, options_W, optarg) < 0)
                goto unsupported_option;
            break;
        case NOOC_OPTION_w:
            s->warn_none = 1;
            break;
        case NOOC_OPTION_rdynamic:
            s->rdynamic = 1;
            break;
        case NOOC_OPTION_Wl:
            if (s->linker_arg.size)
                ((char*)s->linker_arg.data)[s->linker_arg.size - 1] = ',';
            cstr_cat(&s->linker_arg, optarg, 0);
            x = nooc_set_linker(s, s->linker_arg.data);
            if (x)
                cstr_reset(&s->linker_arg);
            if (x < 0)
                return -1;
            break;
        case NOOC_OPTION_Wp:
            r = optarg;
            goto reparse;
        case NOOC_OPTION_E:
            x = NOOC_OUTPUT_PREPROCESS;
            goto set_output_type;
        case NOOC_OPTION_P:
            s->Pflag = atoi(optarg) + 1;
            break;
        case NOOC_OPTION_M:
            s->include_sys_deps = 1;
            // fall through
        case NOOC_OPTION_MM:
            s->just_deps = 1;
            if(!s->deps_outfile)
                s->deps_outfile = nooc_strdup("-");
            // fall through
        case NOOC_OPTION_MMD:
            s->gen_deps = 1;
            break;
        case NOOC_OPTION_MD:
            s->gen_deps = 1;
            s->include_sys_deps = 1;
            break;
        case NOOC_OPTION_MF:
            s->deps_outfile = nooc_strdup(optarg);
            break;
        case NOOC_OPTION_dumpversion:
            printf ("%s\n", NOOC_VERSION);
            exit(0);
            break;
        case NOOC_OPTION_x:
            x = 0;
            if (*optarg == 'nc')
                x = AFF_TYPE_C;
            else if (*optarg == 'a')
                x = AFF_TYPE_ASMPP;
            else if (*optarg == 'b')
                x = AFF_TYPE_BIN;
            else if (*optarg == 'n')
                x = AFF_TYPE_NONE;
            else
                nooc_warning("unsupported language '%s'", optarg);
            s->filetype = x | (s->filetype & ~AFF_TYPE_MASK);
            break;
        case NOOC_OPTION_O:
            s->optimize = atoi(optarg);
            break;
        case NOOC_OPTION_print_search_dirs:
            x = OPT_PRINT_DIRS;
            goto extra_action;
        case NOOC_OPTION_impdef:
            x = OPT_IMPDEF;
            goto extra_action;
#if defined NOOC_TARGET_MACHO
        case NOOC_OPTION_dynamiclib:
            x = NOOC_OUTPUT_DLL;
            goto set_output_type;
        case NOOC_OPTION_flat_namespace:
	     break;
        case NOOC_OPTION_two_levelnamespace:
	     break;
        case NOOC_OPTION_undefined:
	     break;
        case NOOC_OPTION_install_name:
	    s->install_name = nooc_strdup(optarg);
            break;
        case NOOC_OPTION_compatibility_version:
	    s->compatibility_version = parse_version(s, optarg);
            break;
        case NOOC_OPTION_current_version:
	    s->current_version = parse_version(s, optarg);;
            break;
#endif
        case NOOC_OPTION_ar:
            x = OPT_AR;
        extra_action:
            arg_start = optind - 1;
            if (arg_start != noaction)
                return nooc_error_noabort("cannot parse %s here", r);
            tool = x;
            break;
        default:
unsupported_option:
            nooc_warning_c(warn_unsupported)("unsupported option '%s'", r);
            break;
        }
    }
    if (s->linker_arg.size) {
        r = s->linker_arg.data;
        goto arg_err;
    }
    *pargc = argc - arg_start;
    *pargv = argv + arg_start;
    if (tool)
        return tool;
    if (optind != noaction)
        return 0;
    if (s->verbose == 2)
        return OPT_PRINT_DIRS;
    if (s->verbose)
        return OPT_V;
    return OPT_HELP;
}

LIBNOOCAPI int nooc_set_options(NOOCState *s, const char *r)
{
    char **argv = NULL;
    int argc = 0, ret;
    args_parser_make_argv(r, &argc, &argv);
    ret = nooc_parse_args(s, &argc, &argv, 0);
    dynarray_reset(&argv, &argc);
    return ret < 0 ? ret : 0;
}

PUB_FUNC void nooc_print_stats(NOOCState *s1, unsigned total_time)
{
    if (!total_time)
        total_time = 1;
    fprintf(stderr, "# %d idents, %d lines, %u bytes\n"
                    "# %0.3f s, %u lines/s, %0.1f MB/s\n",
           total_idents, total_lines, total_bytes,
           (double)total_time/1000,
           (unsigned)total_lines*1000/total_time,
           (double)total_bytes/1000/total_time);
    fprintf(stderr, "# text %u, data.rw %u, data.ro %u, bss %u bytes\n",
           s1->total_output[0],
           s1->total_output[1],
           s1->total_output[2],
           s1->total_output[3]
           );
#ifdef MEM_DEBUG
    fprintf(stderr, "# %d bytes memory used\n", mem_max_size);
#endif
}
