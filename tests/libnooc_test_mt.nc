/*
 * Multi-thread Test for libnooc
 */

#ifndef FIB
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "libnooc.h"

#define M 20 /* number of states */
#define F(n) (n % 20 + 2) /* fib argument */

#ifdef _WIN32
#include <windows.h>
#define TF_TYPE(func, param) DWORD WINAPI func(void *param)
typedef TF_TYPE(ThreadFunc, x);
HANDLE hh[M];
void create_thread(ThreadFunc f, int n)
{
    DWORD tid;
    hh[n] = CreateThread(NULL, 0, f, (void*)(size_t)n, 0, &tid);
}
void wait_threads(int n)
{
    WaitForMultipleObjects(n, hh, TRUE, INFINITE);
    while (n)
        CloseHandle(hh[--n]);
}
void sleep_ms(unsigned n)
{
    Sleep(n);
}
#else
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#define TF_TYPE(func, param) void* func(void *param)
typedef TF_TYPE(ThreadFunc, x);
pthread_t hh[M];
void create_thread(ThreadFunc f, int n)
{
    pthread_create(&hh[n], NULL, f, (void*)(size_t)n);
}
void wait_threads(int n)
{
    while (n)
        pthread_join(hh[--n], NULL);

}
void sleep_ms(unsigned n)
{
    usleep(n * 1000);
}
#endif

void handle_error(void *opaque, const char *msg)
{
    fprintf(opaque, "%s\n", msg);
}

/* this function is called by the generated code */
int add(int a, int b)
{
    return a + b;
}

#define _str(s) #s
#define str(s) _str(s)
/* as a trick, prepend #line directive for better error/warning messages */
#define PROG(lbl) \
    char lbl[] = "#line " str(__LINE__) " " str(__FILE__) "\n\n"

PROG(my_program)
"#include <nooclib.h>\n" /* include the "Simple libc header for NOOC" */
"int add(int a, int b);\n"
"int fib(int n)\n"
"{\n"
"    if (n <= 2)\n"
"        return 1;\n"
"    else\n"
"        return add(fib(n-1),fib(n-2));\n"
"}\n"
"\n"
"int foo(int n)\n"
"{\n"
"    printf(\" %d\", fib(n));\n"
"    return 0;\n"
"#  warning is this the correct file:line...\n"
"}\n";

int g_argc; char **g_argv;

void parse_args(NOOCState *s)
{
    int i;
    /* if nooclib.h and libnooc1.a are not installed, where can we find them */
    for (i = 1; i < g_argc; ++i) {
        char *a = g_argv[i];
        if (a[0] == '-') {
            if (a[1] == 'B')
                nooc_set_lib_path(s, a+2);
            else if (a[1] == 'I')
                nooc_add_include_path(s, a+2);
            else if (a[1] == 'L')
                nooc_add_library_path(s, a+2);
            else if (a[1] == 'D')
                nooc_define_symbol(s, a+2, NULL);
        }
    }
}

NOOCState *new_state(int w)
{
    NOOCState *s = nooc_new();
    if (!s) {
        fprintf(stderr, __FILE__ ": could not create nooc state\n");
        exit(1);
    }
    nooc_set_error_func(s, stdout, handle_error);
    parse_args(s);
    if (!w) nooc_set_options(s, "-w");
    nooc_set_output_type(s, NOOC_OUTPUT_MEMORY);
    return s;
}

void *reloc_state(NOOCState *s, const char *entry)
{
    void *func;
    nooc_add_symbol(s, "add", add);
    if (nooc_relocate(s, NOOC_RELOCATE_AUTO) < 0) {
        fprintf(stderr, __FILE__ ": could not relocate nooc state.\n");
        return NULL;
    }
    func = nooc_get_symbol(s, entry);
    if (!func)
        fprintf(stderr, __FILE__ ": could not get entry symbol.\n");
    return func;
}

/* work with several states at the same time */
int state_test(void)
{
    NOOCState *s[M];
    int (*func[M])(int);
    int n;

    for (n = 0; n < M + 4; ++n) {
        unsigned a = n, b = n - 1, c = n - 2, d = n - 3, e = n - 4;
        if (a < M)
            s[a] = new_state(0);
        if (b < M)
            if (nooc_compile_string(s[b], my_program) == -1)
                break;
        if (c < M)
            func[c] = reloc_state(s[c], "foo");
        if (d < M && func[d])
            func[d](F(d));
        if (e < M)
            nooc_delete(s[e]);
    }
    return 0;
}

/* simple compilation in threads */
TF_TYPE(thread_test_simple, vn)
{
    NOOCState *s;
    int (*func)(int);
    int ret;
    int n = (size_t)vn;

    s = new_state(0);
    sleep_ms(1);
    ret = nooc_compile_string(s, my_program);
    sleep_ms(1);
    if (ret >= 0) {
        func = reloc_state(s, "foo");
        if (func)
            func(F(n));
    }
    nooc_delete(s);
    return 0;
}

/* more complex compilation in threads */
TF_TYPE(thread_test_complex, vn)
{
    NOOCState *s;
    int ret;
    int n = (size_t)vn;
    char *argv[30], b[10];
    int argc = 0, i;

    sprintf(b, "%d", F(n));

    for (i = 1; i < g_argc; ++i) argv[argc++] = g_argv[i];
#if 0
    argv[argc++] = "-run";
    for (i = 1; i < g_argc; ++i) argv[argc++] = g_argv[i];
#endif
    argv[argc++] = "-DFIB";
    argv[argc++] = "-run";
    argv[argc++] = __FILE__;
    argv[argc++] = b;
    argv[argc] = NULL;

    s = new_state(1);
    sleep_ms(2);
    ret = nooc_add_file(s, argv[0]);
    sleep_ms(3);
    if (ret == 0)
        nooc_run(s, argc, argv);
    nooc_delete(s);
    fflush(stdout);
    return 0;
}

void time_nooc(int n, const char *src)
{
    NOOCState *s;
    int ret, i = 0;
    while (i++ < n) {
        s = new_state(1);
        printf(" %d", i), fflush(stdout);
        ret = nooc_add_file(s, src);
        nooc_delete(s);
        if (ret < 0)
            exit(1);
    }
}

static unsigned getclock_ms(void)
{
#ifdef _WIN32
    return GetTickCount();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec*1000 + (tv.tv_usec+500)/1000;
#endif
}

int main(int argc, char **argv)
{
    int n;
    unsigned t;

    g_argc = argc;
    g_argv = argv;

    if (argc < 2) {
        fprintf(stderr, "usage: libnooc_test_mt nooc.c <options>\n");
        return 1;
    }

#if 1
    printf("running fib with mixed calls\n "), fflush(stdout);
    t = getclock_ms();
    state_test();
    printf("\n (%u ms)\n", getclock_ms() - t);
#endif
#if 1
    printf("running fib in threads\n "), fflush(stdout);
    t = getclock_ms();
    for (n = 0; n < M; ++n)
        create_thread(thread_test_simple, n);
    wait_threads(n);
    printf("\n (%u ms)\n", getclock_ms() - t);
#endif
#if 1
    printf("running nooc.c in threads to run fib\n "), fflush(stdout);
    t = getclock_ms();
    for (n = 0; n < M; ++n)
        create_thread(thread_test_complex, n);
    wait_threads(n);
    printf("\n (%u ms)\n", getclock_ms() - t);
#endif
#if 1
    printf("compiling nooc.c 10 times\n "), fflush(stdout);
    t = getclock_ms();
    time_nooc(10, argv[1]);
    printf("\n (%u ms)\n", getclock_ms() - t), fflush(stdout);
#endif
    return 0;
}

#else
#include <nooclib.h>

unsigned int sleep(unsigned int seconds);

int fib(n)
{
    return (n <= 2) ? 1 : fib(n-1) + fib(n-2);
}

int main(int argc, char **argv)
{
    sleep(1);
    printf(" %d", fib(atoi(argv[1])));
    return 0;
}
#endif
