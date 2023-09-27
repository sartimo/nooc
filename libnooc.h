#ifndef LIBNOOC_H
#define LIBNOOC_H

#ifndef LIBNOOCAPI
# define LIBNOOCAPI
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct NOOCState;

typedef struct NOOCState NOOCState;

typedef void (*NOOCErrorFunc)(void *opaque, const char *msg);

/* create a new NOOC compilation context */
LIBNOOCAPI NOOCState *nooc_new(void);

/* free a NOOC compilation context */
LIBNOOCAPI void nooc_delete(NOOCState *s);

/* set CONFIG_NOOCDIR at runtime */
LIBNOOCAPI void nooc_set_lib_path(NOOCState *s, const char *path);

/* set error/warning display callback */
LIBNOOCAPI void nooc_set_error_func(NOOCState *s, void *error_opaque, NOOCErrorFunc error_func);

/* return error/warning callback */
LIBNOOCAPI NOOCErrorFunc nooc_get_error_func(NOOCState *s);

/* return error/warning callback opaque pointer */
LIBNOOCAPI void *nooc_get_error_opaque(NOOCState *s);

/* set options as from command line (multiple supported) */
LIBNOOCAPI int nooc_set_options(NOOCState *s, const char *str);

/*****************************/
/* preprocessor */

/* add include path */
LIBNOOCAPI int nooc_add_include_path(NOOCState *s, const char *pathname);

/* add in system include path */
LIBNOOCAPI int nooc_add_sysinclude_path(NOOCState *s, const char *pathname);

/* define preprocessor symbol 'sym'. value can be NULL, sym can be "sym=val" */
LIBNOOCAPI void nooc_define_symbol(NOOCState *s, const char *sym, const char *value);

/* undefine preprocess symbol 'sym' */
LIBNOOCAPI void nooc_undefine_symbol(NOOCState *s, const char *sym);

/*****************************/
/* compiling */

/* add a file (C file, dll, object, library, ld script). Return -1 if error. */
LIBNOOCAPI int nooc_add_file(NOOCState *s, const char *filename);

/* compile a string containing a C source. Return -1 if error. */
LIBNOOCAPI int nooc_compile_string(NOOCState *s, const char *buf);

/*****************************/
/* linking commands */

/* set output type. MUST BE CALLED before any compilation */
LIBNOOCAPI int nooc_set_output_type(NOOCState *s, int output_type);
#define NOOC_OUTPUT_MEMORY   1 /* output will be run in memory */
#define NOOC_OUTPUT_EXE      2 /* executable file */
#define NOOC_OUTPUT_DLL      4 /* dynamic library */
#define NOOC_OUTPUT_OBJ      3 /* object file */
#define NOOC_OUTPUT_PREPROCESS 5 /* only preprocess */

/* equivalent to -Lpath option */
LIBNOOCAPI int nooc_add_library_path(NOOCState *s, const char *pathname);

/* the library name is the same as the argument of the '-l' option */
LIBNOOCAPI int nooc_add_library(NOOCState *s, const char *libraryname);

/* add a symbol to the compiled program */
LIBNOOCAPI int nooc_add_symbol(NOOCState *s, const char *name, const void *val);

/* output an executable, library or object file. DO NOT call
   nooc_relocate() before. */
LIBNOOCAPI int nooc_output_file(NOOCState *s, const char *filename);

/* link and run main() function and return its value. DO NOT call
   nooc_relocate() before. */
LIBNOOCAPI int nooc_run(NOOCState *s, int argc, char **argv);

/* do all relocations (needed before using nooc_get_symbol()) */
LIBNOOCAPI int nooc_relocate(NOOCState *s1, void *ptr);
/* possible values for 'ptr':
   - NOOC_RELOCATE_AUTO : Allocate and manage memory internally
   - NULL              : return required memory size for the step below
   - memory address    : copy code to memory passed by the caller
   returns -1 if error. */
#define NOOC_RELOCATE_AUTO (void*)1

/* return symbol value or NULL if not found */
LIBNOOCAPI void *nooc_get_symbol(NOOCState *s, const char *name);

/* return symbol value or NULL if not found */
LIBNOOCAPI void nooc_list_symbols(NOOCState *s, void *ctx,
    void (*symbol_cb)(void *ctx, const char *name, const void *val));

#ifdef __cplusplus
}
#endif

#endif
