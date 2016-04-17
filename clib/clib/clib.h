/*
 * Copyright (C) 2014-2015 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <clib-platform.h>

#ifdef __EMSCRIPTEN__
#include <clib-web.h>
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <ctype.h>
#include <limits.h>
#include <float.h>
#ifdef C_HAVE_PTHREADS
#include <pthread.h>
#endif

#include <stdint.h>
#include <inttypes.h>

#ifdef C_HAVE_ALLOCA_H
#include <alloca.h>
#endif

#ifdef WIN32
/* For alloca */
#include <malloc.h>
#endif

#ifdef USE_UV
#include <uv.h>
#endif

#ifndef offsetof
#define offsetof(s_name, n_name) (size_t)(char *)&(((s_name *)0)->m_name)
#endif

#define __CLIB_X11 1

#ifdef __cplusplus
#define C_BEGIN_DECLS extern "C" {
#define C_END_DECLS }
#else
#define C_BEGIN_DECLS
#define C_END_DECLS
#endif

C_BEGIN_DECLS

#ifdef WIN32
/* MSC and Cross-compilation will use this */
int vasprintf(char **strp, const char *fmt, va_list ap);
int mkstemp(char *tmp_template);
#endif

/*
 * Basic data types
 */
typedef uint16_t c_utf16_t;
typedef uint32_t c_codepoint_t;

/*
 * Macros
 */
#define C_N_ELEMENTS(s) (sizeof(s) / sizeof((s)[0]))
#define C_STRINGIFY_ARG(arg) #arg
#define C_STRINGIFY(arg) C_STRINGIFY_ARG(arg)
#define C_PASTE_ARGS(part0, part1) part0##part1
#define C_PASTE(part0, part1) C_PASTE_ARGS(part0, part1)

#ifndef __cplusplus
#  ifndef false
#  define false 0
#  endif

#  ifndef true
#  define true 1
#  endif
#endif

#define C_MINSHORT SHRT_MIN
#define C_MAXSHORT SHRT_MAX
#define C_MAXUSHORT USHRT_MAX
#define C_MAXINT INT_MAX
#define C_MININT INT_MIN
#define C_MAXINT32 INT32_MAX
#define C_MAXUINT32 UINT32_MAX
#define C_MININT32 INT32_MIN
#define C_MININT64 INT64_MIN
#define C_MAXINT64 INT64_MAX
#define C_MAXUINT64 UINT64_MAX
#define C_MAXFLOAT FLT_MAX

#define C_ASCII_DTOSTR_BUF_SIZE 40

#define C_UINT64_FORMAT PRIu64
#define C_INT64_FORMAT PRId64
#define C_UINT32_FORMAT PRIu32
#define C_INT32_FORMAT PRId32

#define C_STMT_START do
#define C_STMT_END while (0)

#define C_USEC_PER_SEC 1000000
#define C_PI   3.141592653589793238462643383279502884L
#define C_PI_2 1.570796326794896619231321691639751442L
#define C_PI_4 0.785398163397448309615660845819875721L
#define C_E    2.718281828459045235360287471352662497L

#define C_INT64_CONSTANT(val) (val##LL)
#define C_UINT64_CONSTANT(val) (val##UL)

#define C_POINTER_TO_INT(ptr) ((int)(intptr_t)(ptr))
#define C_POINTER_TO_UINT(ptr) ((unsigned int)(uintptr_t)(ptr))
#define C_INT_TO_POINTER(v) ((void *)(intptr_t)(v))
#define C_UINT_TO_POINTER(v) ((void *)(uintptr_t)(v))

#define C_STRUCT_OFFSET(p_type, field) offsetof(p_type, field)

#define C_STRLOC __FILE__ ":" C_STRINGIFY(__LINE__) ":"

#define C_CONST_RETURN const

#define C_BYTE_ORDER C_LITTLE_ENDIAN

#define C_UINT32_FROM_BE(X) ntohl(X)
#define C_UINT32_TO_BE(X) htonl(X)

#if defined(__GNUC__)
#define C_GNUC_UNUSED __attribute__((__unused__))
#define C_GNUC_NORETURN __attribute__((__noreturn__))
#define C_LIKELY(expr) (__builtin_expect((expr) != 0, 1))
#define C_UNLIKELY(expr) (__builtin_expect((expr) != 0, 0))
#define C_GNUC_PRINTF(format_idx, arg_idx) \
    __attribute__((__format__(__printf__, format_idx, arg_idx)))
#define C_GNUC_NULL_TERMINATED __attribute__((__sentinel__))
#define C_GNUC_DEPRECATED __attribute__((__deprecated__))
#define C_DEPRECATED(MSG) __attribute__((deprecated(MSG)))
#else
#define C_GNUC_GNUSED
#define C_GNUC_NORETURN
#define C_LIKELY(expr) (expr)
#define C_UNLIKELY(expr) (expr)
#define C_GNUC_PRINTF(format_idx, arg_idx)
#define C_GNUC_NULL_TERMINATED
#define C_GNUC_DEPRECATED
#define C_DEPRECATED(MSG)
#endif

#ifdef C_HAVE_STATIC_ASSERT
#  ifdef __cplusplus
#    define C_STATIC_ASSERT(EXPRESSION, MESSAGE) \
        static_assert(EXPRESSION, MESSAGE);
#  else
#    define C_STATIC_ASSERT(EXPRESSION, MESSAGE) \
        _Static_assert(EXPRESSION, MESSAGE);
#  endif
#else
#define C_STATIC_ASSERT(EXPRESSION, MESSAGE)
#endif

#if defined(__GNUC__)
#define C_STRFUNC ((const char *)(__PRETTY_FUNCTION__))
#elif defined(_MSC_VER)
#define C_STRFUNC ((const char *)(__FUNCTION__))
#else
#define C_STRFUNC ((const char *)(__func__))
#endif

#if defined(_MSC_VER)
#define C_VA_COPY(dest, src) ((dest) = (src))
#else
#define C_VA_COPY(dest, src) va_copy(dest, src)
#endif

#ifndef ABS
#define ABS(a) ((a) > 0 ? (a) : -(a))
#endif

#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef CLAMP
#define CLAMP(a, low, high)                                                    \
    (((a) < (low)) ? (low) : (((a) > (high)) ? (high) : (a)))
#endif

/* This is a replacement for the nearbyint function which always
 * rounds to the nearest integer without referring to the
 * fesetround(3) state as the c99 nearbyint does. Also it seems in
 * glibc nearbyint is defined as a function call so this macro could
 * end up faster too.
 *
 * NB: We can't just add 0.5f because it will break for negative
 * numbers.
 */
#define c_nearbyint(x) ((int)((x) < 0.0f ? (x) - 0.5f : (x) + 0.5f))

/* _C_STATIC_ASSERT:
 * @expression: An expression to assert evaluates to true at compile
 *              time.
 * @message: A message to print to the console if the assertion fails
 *           at compile time.
 *
 * Allows you to assert that an expression evaluates to true at
 * compile time and aborts compilation if not. If possible message
 * will also be printed if the assertion fails.
 *
 * Note: Only Gcc >= 4.6 supports the c11 _Static_assert which lets us
 * print a nice message if the compile time assertion fails.
 */
#ifdef HAVE_STATIC_ASSERT
#define _C_STATIC_ASSERT(EXPRESSION, MESSAGE) \
    _Static_assert(EXPRESSION, MESSAGE);
#else
#define _C_STATIC_ASSERT(EXPRESSION, MESSAGE)
#endif

/* To help catch accidental changes to public structs that should
 * be stack allocated we use this macro to compile time assert that
 * a struct size is as expected.
 */
#define _C_STRUCT_SIZE_ASSERT(TYPE, SIZE)           \
    typedef struct {                                \
        char compile_time_assert_##TYPE##_size      \
        [(sizeof(TYPE) == (SIZE)) ? 1 : -1];        \
    } _##TYPE##SizeCheck

/* Some structures are meant to be opaque but they have public
   definitions because we want the size to be public so they can be
   allocated on the stack. This macro is used to ensure that users
   don't accidentally access private members */
#ifdef _C_COMPILATION
#define _C_PRIVATE(x) x
#else
#define _C_PRIVATE(x) private_member_##x
#endif

/* We forward declare these to avoid circular dependencies
 * between cmatrix.h, ceuler.h and cquaterion.h */
typedef struct _c_matrix_t c_matrix_t;
typedef struct _c_quaternion_t c_quaternion_t;
typedef struct _c_euler_t c_euler_t;

#include <cvector.h>
#include <cmatrix.h>
#include <ceuler.h>
#include <cquaternion.h>

/*
 * Allocation
 */
void c_free(void *ptr);
void *c_realloc(void *obj, size_t size);
void *c_malloc(size_t x);
void *c_malloc0(size_t x);
void *c_try_malloc(size_t x);
void *c_try_realloc(void *obj, size_t size);
void *c_memdup(const void *mem, unsigned int byte_size);

#define c_new(type, size) ((type *)c_malloc(sizeof(type) * (size)))
#define c_new0(type, size) ((type *)c_malloc0(sizeof(type) * (size)))
#define c_newa(type, size) ((type *)alloca(sizeof(type) * (size)))

#define c_memmove(dest, src, len) memmove(dest, src, len)
#define c_renew(struct_type, mem, n_structs)                                   \
    c_realloc(mem, sizeof(struct_type) * n_structs)
#define c_alloca(size) alloca(size)

#define c_slice_new(type) ((type *)c_malloc(sizeof(type)))
#define c_slice_new0(type) ((type *)c_malloc0(sizeof(type)))
#define c_slice_free(type, mem) c_free(mem)
#define c_slice_free1(size, mem) c_free(mem)
#define c_slice_alloc(size) c_malloc(size)
#define c_slice_alloc0(size) c_malloc0(size)
#define c_slice_dup(type, mem) c_memdup(mem, sizeof(type))
#define c_slice_copy(size, mem) c_memdup(mem, size)

static inline char *
c_strdup(const char *str)
{
    if (str) {
        return strdup(str);
    }
    return NULL;
}

#if defined(_GNU_SOURCE) && defined(__GNUC__)
#define c_strdupa(s) strdupa(s)
#define c_strndupa(s) strndupa(s)
#else
#define c_strdupa(str) strcpy(alloca(strlen(str) + 1, str))
#define c_strndupa(str, n) strncpy(alloca(strnlen(str, n) + 1, str))
#endif

#ifdef WIN32
#define strtok_r strtok_s
#endif

char **c_strdupv(char **str_array);
int c_strcmp0(const char *str1, const char *str2);

typedef struct {
    void *(*malloc)(size_t n_bytes);
    void *(*realloc)(void *mem, size_t n_bytes);
    void (*free)(void *mem);
    void *(*calloc)(size_t n_blocks, size_t n_block_bytes);
    void *(*try_malloc)(size_t n_bytes);
    void *(*try_realloc)(void *mem, size_t n_bytes);
} c_mem_vtable_t;

#define c_mem_set_vtable(x)

struct _c_mem_chunk_t {
    unsigned int alloc_size;
};

typedef struct _c_mem_chunk_t c_mem_chunk_t;

#ifdef C_HAVE_MEMMEM
#define c_memmem memmem
#else
char *c_memmem(const void *haystack,
               size_t haystack_len,
               const void *needle,
               size_t needle_len);
#endif

/*
 * Misc.
 */
#define c_atexit(func) ((void)atexit(func))

const char *c_getenv(const char *variable);
bool c_setenv(const char *variable, const char *value, bool overwrite);
void c_unsetenv(const char *variable);

char *c_win32_getlocale(void);

#ifdef C_DEBUG
/*
 * Precondition macros
 */
#define c_warn_if_fail(x)                                                      \
    C_STMT_START                                                               \
    {                                                                          \
        if (!(x)) {                                                            \
            c_warning("%s:%d: assertion '%s' failed", __FILE__, __LINE__, #x); \
        }                                                                      \
    }                                                                          \
    C_STMT_END
#define c_warn_if_reached()                                                    \
    c_warning("%s:%d: code should not be reached!", __FILE__, __LINE__)

#define c_return_if_fail(x)                                                    \
    C_STMT_START                                                               \
    {                                                                          \
        if (!(x)) {                                                            \
            c_critical(                                                        \
                "%s:%d: assertion '%s' failed", __FILE__, __LINE__, #x);       \
            return;                                                            \
        }                                                                      \
    }                                                                          \
    C_STMT_END
#define c_return_val_if_fail(x, e)                                             \
    C_STMT_START                                                               \
    {                                                                          \
        if (!(x)) {                                                            \
            c_critical(                                                        \
                "%s:%d: assertion '%s' failed", __FILE__, __LINE__, #x);       \
            return (e);                                                        \
        }                                                                      \
    }                                                                          \
    C_STMT_END
#define c_return_if_reached(e)                                                 \
    C_STMT_START                                                               \
    {                                                                          \
        c_warning("%s:%d: code should not be reached, returning!",             \
                  __FILE__,                                                    \
                  __LINE__);                                                   \
        return;                                                                \
    }                                                                          \
    C_STMT_END
#define c_return_val_if_reached(e)                                             \
    C_STMT_START                                                               \
    {                                                                          \
        c_warning("%s:%d: code should not be reached, returning!",             \
                  __FILE__,                                                    \
                  __LINE__);                                                   \
        return (e);                                                            \
    }                                                                          \
    C_STMT_END

#else

#define c_warn_if_fail(x) do { if (0 && !(x)) /* pass */; } while(0)
#define c_warn_if_reached() do { } while(0)
#define c_return_if_fail(x) do { if (0 && !(x)) return; } while(0)
#define c_return_val_if_fail(x, e) do { if (0 && !(x)) return (e); } while(0)
#define c_return_if_reached(e) return e
#define c_return_val_if_reached(e) return e

#endif /* C_DEBUG */

/*
 * DebugKeys
 */
typedef struct _c_debug_key_t {
    const char *key;
    unsigned int value;
} c_debug_key_t;

unsigned int c_parse_debug_string(const char *string,
                                  const c_debug_key_t *keys,
                                  unsigned int nkeys);

/*
 * Quark
 */

typedef uint32_t c_quark_t;

c_quark_t c_quark_from_static_string(const char *string);
c_quark_t c_quark_from_string(const char *string);
const char *c_intern_static_string(const char *string);
const char *c_intern_string(const char *string);

/*
 * Messages
 */
#ifndef C_LOG_DOMAIN
#define C_LOG_DOMAIN ((char *)0)
#endif

typedef enum {
    C_LOG_FLAG_RECURSION = 1 << 0,
    C_LOG_FLAG_FATAL = 1 << 1,
    C_LOG_LEVEL_ERROR = 1 << 2,
    C_LOG_LEVEL_CRITICAL = 1 << 3,
    C_LOG_LEVEL_WARNING = 1 << 4,
    C_LOG_LEVEL_MESSAGE = 1 << 5,
    C_LOG_LEVEL_INFO = 1 << 6,
    C_LOG_LEVEL_DEBUG = 1 << 7,
    C_LOG_LEVEL_MASK = ~(C_LOG_FLAG_RECURSION | C_LOG_FLAG_FATAL)
} c_log_level_flags_t;

void c_print(const char *format, ...);
void c_printerr(const char *format, ...);

typedef struct _c_log_context c_log_context_t;

extern void (*c_log_hook)(c_log_context_t *lctx,
                          const char *log_domain,
                          c_log_level_flags_t log_level,
                          const char *message);
void c_logv(c_log_context_t *lctx,
            const char *log_domain,
            c_log_level_flags_t log_level,
            const char *format,
            va_list args);
void c_log(c_log_context_t *lctx,
           const char *log_domain,
           c_log_level_flags_t log_level,
           const char *format,
           ...);
void c_assertion_message(const char *format, ...) C_GNUC_NORETURN;

#ifdef HAVE_C99_SUPPORT
/* The for (;;) tells gc thats c_error () doesn't return, avoiding warnings */
#define c_error(format, ...)                                                   \
    do {                                                                       \
        c_log(NULL, C_LOG_DOMAIN, C_LOG_LEVEL_ERROR, format, __VA_ARGS__);      \
        for (;; )                                                               \
            ;                                                                  \
    } while (0)
#define c_critical(format, ...)                                                \
    c_log(NULL, C_LOG_DOMAIN, C_LOG_LEVEL_CRITICAL, format, __VA_ARGS__)
#define c_warning(format, ...)                                                 \
    c_log(NULL, C_LOG_DOMAIN, C_LOG_LEVEL_WARNING, format, __VA_ARGS__)
#define c_message(format, ...)                                                 \
    c_log(NULL, C_LOG_DOMAIN, C_LOG_LEVEL_MESSAGE, format, __VA_ARGS__)
#define c_debug(format, ...)                                                   \
    c_log(NULL, C_LOG_DOMAIN, C_LOG_LEVEL_DEBUG, format, __VA_ARGS__)
#else /* HAVE_C99_SUPPORT */
#define c_error(...)                                                           \
    do {                                                                       \
        c_log(NULL, C_LOG_DOMAIN, C_LOG_LEVEL_ERROR, __VA_ARGS__);             \
        for (;; )                                                               \
            ;                                                                  \
    } while (0)
#define c_critical(...) c_log(NULL, C_LOG_DOMAIN, C_LOG_LEVEL_CRITICAL, __VA_ARGS__)
#define c_warning(...) c_log(NULL, C_LOG_DOMAIN, C_LOG_LEVEL_WARNING, __VA_ARGS__)
#define c_message(...) c_log(NULL, C_LOG_DOMAIN, C_LOG_LEVEL_MESSAGE, __VA_ARGS__)
#define c_debug(...) c_log(NULL, C_LOG_DOMAIN, C_LOG_LEVEL_DEBUG, __VA_ARGS__)
#endif /* ndef HAVE_C99_SUPPORT */
#define c_log_set_handler(a, b, c, d)


/*
 * Backtraces
 */

/* Returns array of frame pointers
 * (Points to internal buffer which caller shouldn't free) */
void **c_backtrace(int *n_frames);

/* Tries to resolve human readable descriptions for each frame
 * (No need to free returned strings)
 */
void c_backtrace_symbols(void **addresses,
                         char **frames,
                         int n_frames);

typedef struct _c_backtrace c_backtrace_t;

c_backtrace_t *c_backtrace_new(void);
int c_backtrace_get_n_frames(c_backtrace_t *backtrace);
/* Tries to resolve human readable description for each frame
 * (No need to free returned strings)
 */
void c_backtrace_get_frame_symbols(c_backtrace_t *backtrace,
                                   char **frames,
                                   int n_frames);
void c_backtrace_log(c_backtrace_t *backtrace,
                     c_log_context_t *lctx,
                     const char *log_domain,
                     c_log_level_flags_t log_level);
void c_backtrace_log_error(c_backtrace_t *backtrace);
c_backtrace_t *c_backtrace_copy(c_backtrace_t *backtrace);
void c_backtrace_free(c_backtrace_t *backtrace);


/*
 * Errors
 */
typedef struct {
    c_quark_t domain;
    int code;
    char *message;
    c_backtrace_t *backtrace;
} c_error_t;

void c_clear_error(c_error_t **error);
void c_error_free(c_error_t *error);
c_error_t *c_error_new(c_quark_t domain, int code, const char *format, ...);
c_error_t *
c_error_new_valist(c_quark_t domain, int code, const char *format, va_list ap);
void c_set_error(
    c_error_t **err, c_quark_t domain, int code, const char *format, ...);
void c_propagate_error(c_error_t **dest, c_error_t *src);
c_error_t *c_error_copy(const c_error_t *error);
bool c_error_matches(const c_error_t *error, c_quark_t domain, int code);

/*
 * Strings utility
 */
char *c_strdup_printf(const char *format, ...);
char *c_strdup_vprintf(const char *format, va_list args);
char *c_strndup(const char *str, size_t n);
const char *c_strerror(int errnum);
void c_strfreev(char **str_array);
char *c_strconcat(const char *first, ...);
char **c_strsplit(const char *string, const char *delimiter, int max_tokens);
char **
c_strsplit_set(const char *string, const char *delimiter, int max_tokens);
char *c_strreverse(char *str);
char *c_strrstr(const char *str1, const char *str2);
bool c_str_has_prefix(const char *str, const char *prefix);
bool c_str_has_suffix(const char *str, const char *suffix);
unsigned int c_strv_length(char **str_array);
char *c_strjoin(const char *separator, ...);
char *c_strjoinv(const char *separator, char **str_array);
char *c_strchug(char *str);
char *c_strchomp(char *str);
void c_strdown(char *string);
char *c_strnfill(size_t length, char fill_char);

char *c_strdelimit(char *string, const char *delimiters, char new_delimiter);
char *c_strescape(const char *source, const char *exceptions);

char *c_filename_to_uri(const char *filename,
                        const char *hostname,
                        c_error_t **error);
char *c_filename_from_uri(const char *uri, char **hostname, c_error_t **error);

int c_printf(char const *format, ...);
int c_fprintf(FILE *file, char const *format, ...);
int c_sprintf(char *string, char const *format, ...);
int c_snprintf(char *string, unsigned long n, char const *format, ...);
#define c_vprintf vprintf
#define c_vfprintf vfprintf
#define c_vsprintf vsprintf
#define c_vsnprintf vsnprintf
#define c_vasprintf vasprintf

size_t c_strlcpy(char *dest, const char *src, size_t dest_size);
char *c_stpcpy(char *dest, const char *src);

char c_ascii_tolower(char c);
char c_ascii_toupper(char c);

/* XXX: unlike glib, these modify the strings in place! */
char *c_ascii_strdown(char *str, c_ssize_t len);
char *c_ascii_strup(char *str, c_ssize_t len);

int c_ascii_strncasecmp(const char *s1, const char *s2, size_t n);
int c_ascii_strcasecmp(const char *s1, const char *s2);
int c_ascii_xdigit_value(char c);
int c_ascii_snprintf(char *, size_t, const char *, ...);
double c_ascii_strtod(const char *nptr, char **endptr);
#define c_ascii_isspace(c) (isspace(c) != 0)
#define c_ascii_isalpha(c) (isalpha(c) != 0)
#define c_ascii_isprint(c) (isprint(c) != 0)
#define c_ascii_isxdigit(c) (isxdigit(c) != 0)

/* FIXME: c_strcasecmp supports utf8 unicode stuff */
#ifdef _MSC_VER
#define c_strcasecmp stricmp
#define c_strncasecmp strnicmp
#define c_strstrip(a) c_strchug(c_strchomp(a))
#else
#define c_strcasecmp strcasecmp
#define c_ascii_strtoull strtoull
#define c_strncasecmp strncasecmp
#define c_strstrip(a) c_strchug(c_strchomp(a))
#endif
#define c_ascii_strdup strdup

#define C_STR_DELIMITERS "_-|> <."

/*
 * String type
 */
typedef struct {
    char *str;
    size_t len;
    size_t allocated_len;
} c_string_t;

c_string_t *c_string_new(const char *init);
c_string_t *c_string_new_len(const char *init, c_ssize_t len);
c_string_t *c_string_sized_new(size_t default_size);
char *c_string_free(c_string_t *string, bool free_segment);
c_string_t *c_string_assign(c_string_t *string, const char *val);
c_string_t *c_string_append(c_string_t *string, const char *val);
void c_string_printf(c_string_t *string, const char *format, ...);
void c_string_append_printf(c_string_t *string, const char *format, ...);
void
c_string_append_vprintf(c_string_t *string, const char *format, va_list args);
c_string_t *c_string_append_unichar(c_string_t *string, c_codepoint_t c);
c_string_t *c_string_append_c(c_string_t *string, char c);
c_string_t *
c_string_append_len(c_string_t *string, const char *val, c_ssize_t len);
c_string_t *c_string_truncate(c_string_t *string, size_t len);
c_string_t *c_string_prepend(c_string_t *string, const char *val);
c_string_t *c_string_insert(c_string_t *string, c_ssize_t pos, const char *val);
c_string_t *c_string_set_size(c_string_t *string, size_t len);
c_string_t *c_string_erase(c_string_t *string, c_ssize_t pos, c_ssize_t len);

#define c_string_sprintfa c_string_append_printf

typedef void (*c_iter_func_t)(void *data, void *user_data);
typedef int (*c_compare_func_t)(const void *a, const void *b);
typedef int (*c_compare_data_func_t)(const void *a,
                                     const void *b,
                                     void *user_data);
typedef void (*c_hash_iter_func_t)(void *key, void *value, void *user_data);
typedef bool (*c_hash_iter_remove_func_t)(void *key,
                                          void *value,
                                          void *user_data);
typedef void (*c_destroy_func_t)(void *data);
typedef unsigned int (*c_hash_func_t)(const void *key);
typedef bool (*c_equal_func_t)(const void *a, const void *b);
typedef void (*c_free_func_t)(void *data);

/**
 * c_list_t - linked list
 *
 * The list head is of "c_list_t" type, and must be initialized
 * using c_list_init().  All entries in the list must be of the same
 * type.  The item type must have a "c_list_t" member. This
 * member will be initialized by c_list_insert(). There is no need to
 * call c_list_init() on the individual item. To query if the list is
 * empty in O(1), use c_list_empty().
 *
 * Let's call the list reference "c_list_t foo_list", the item type as
 * "item_t", and the item member as "c_list_t link". The following code
 *
 * The following code will initialize a list:
 *
 *      c_list_init(foo_list);
 *      c_list_insert(foo_list, item1);      Pushes item1 at the head
 *      c_list_insert(foo_list, item2);      Pushes item2 at the head
 *      c_list_insert(item2, item3);         Pushes item3 after item2
 *
 * The list now looks like [item2, item3, item1]
 *
 * Will iterate the list in ascending order:
 *
 *      item_t *item;
 *      c_list_for_each(item, foo_list, link) {
 *              Do_something_with_item(item);
 *      }
 */

typedef struct _c_list_t c_list_t;

struct _c_list_t {
    c_list_t *prev;
    c_list_t *next;
};

void c_list_init(c_list_t *list);
void c_list_insert(c_list_t *list, c_list_t *elm);
void c_list_remove(c_list_t *elm);
int c_list_length(c_list_t *list);
int c_list_empty(c_list_t *list);
void c_list_prepend_list(c_list_t *list, c_list_t *other);
void c_list_append_list(c_list_t *list, c_list_t *other);

/* Only for the cool C programmers... */
#ifndef __cplusplus

/* This assigns to iterator first so that taking a reference to it
 * later in the second step won't be an undefined operation. It
 * assigns the value of list_node rather than 0 so that it is possible
 * have list_node be based on the previous value of iterator. In that
 * respect iterator is just used as a convenient temporary variable.
 * The compiler optimises all of this down to a single subtraction by
 * a constant */
#define c_list_set_iterator(list_node, iterator, member)                    \
    ((iterator) = (void *)(list_node),                                      \
     (iterator) =                                                           \
         (void *)((char *)(iterator) -                                      \
                  (((char *)&(iterator)->member) - (char *)(iterator))))

#define c_container_of(ptr, type, member)                                   \
    (type *)((char *)(ptr) - offsetof(type, member))

#define c_list_first(list, type, member)                                    \
    c_list_empty(list) ? NULL : c_container_of((list)->next, type, member);

#define c_list_last(list, type, member)                                     \
    c_list_empty(list) ? NULL : c_container_of((list)->prev, type, member);

#define c_list_for_each(pos, head, member)                                  \
    for (c_list_set_iterator((head)->next, pos, member);                    \
         &pos->member != (head);                                            \
         c_list_set_iterator(pos->member.next, pos, member))

#define c_list_for_each_safe(pos, tmp, head, member)                        \
    for (c_list_set_iterator((head)->next, pos, member),                    \
         c_list_set_iterator((pos)->member.next, tmp, member);              \
         &pos->member != (head);                                            \
         pos = tmp, c_list_set_iterator(pos->member.next, tmp, member))

#define c_list_for_each_reverse(pos, head, member)                          \
    for (c_list_set_iterator((head)->prev, pos, member);                    \
         &pos->member != (head);                                            \
         c_list_set_iterator(pos->member.prev, pos, member))

#define c_list_for_each_reverse_safe(pos, tmp, head, member)                \
    for (c_list_set_iterator((head)->prev, pos, member),                    \
         c_list_set_iterator((pos)->member.prev, tmp, member);              \
         &pos->member != (head);                                            \
         pos = tmp, c_list_set_iterator(pos->member.prev, tmp, member))

#endif /* __cplusplus */

/*
 * Link Lists (I.e. these linked list apis allocate separate links
 * that can point to any data as opposed to the c_list_t api which
 * embeds the links within the data)
 */
typedef struct _c_sllist_t c_sllist_t;
struct _c_sllist_t {
    void *data;
    c_sllist_t *next;
};

c_sllist_t *c_sllist_alloc(void);
c_sllist_t *c_sllist_append(c_sllist_t *list, void *data);
c_sllist_t *c_sllist_prepend(c_sllist_t *list, void *data);
void c_sllist_free(c_sllist_t *list);
void c_sllist_free_1(c_sllist_t *list);
c_sllist_t *c_sllist_copy(c_sllist_t *list);
c_sllist_t *c_sllist_concat(c_sllist_t *list1, c_sllist_t *list2);
void c_sllist_foreach(c_sllist_t *list, c_iter_func_t func, void *user_data);
c_sllist_t *c_sllist_last(c_sllist_t *list);
c_sllist_t *c_sllist_find(c_sllist_t *list, const void *data);
c_sllist_t *
c_sllist_find_custom(c_sllist_t *list, const void *data, c_compare_func_t func);
c_sllist_t *c_sllist_remove(c_sllist_t *list, const void *data);
c_sllist_t *c_sllist_remove_all(c_sllist_t *list, const void *data);
c_sllist_t *c_sllist_reverse(c_sllist_t *list);
unsigned int c_sllist_length(c_sllist_t *list);
c_sllist_t *c_sllist_remove_link(c_sllist_t *list, c_sllist_t *link);
c_sllist_t *c_sllist_delete_link(c_sllist_t *list, c_sllist_t *link);
c_sllist_t *
c_sllist_insert_sorted(c_sllist_t *list, void *data, c_compare_func_t func);
c_sllist_t *
c_sllist_insert_before(c_sllist_t *list, c_sllist_t *sibling, void *data);
c_sllist_t *c_sllist_sort(c_sllist_t *list, c_compare_func_t func);
int c_sllist_index(c_sllist_t *list, const void *data);
c_sllist_t *c_sllist_nth(c_sllist_t *list, unsigned int n);
void *c_sllist_nth_data(c_sllist_t *list, unsigned int n);

#define c_sllist_next(slist) ((slist) ? (((c_sllist_t *)(slist))->next) : NULL)

typedef struct _c_llist_t c_llist_t;
struct _c_llist_t {
    void *data;
    c_llist_t *next;
    c_llist_t *prev;
};

#define c_llist_next(list) ((list) ? (((c_llist_t *)(list))->next) : NULL)
#define c_llist_previous(list) ((list) ? (((c_llist_t *)(list))->prev) : NULL)

c_llist_t *c_llist_alloc(void);
c_llist_t *c_llist_append(c_llist_t *list, void *data);
c_llist_t *c_llist_prepend(c_llist_t *list, void *data);
void c_llist_free(c_llist_t *list);
void c_llist_free_full(c_llist_t *list, c_destroy_func_t free_func);
void c_llist_free_1(c_llist_t *list);
c_llist_t *c_llist_copy(c_llist_t *list);
unsigned int c_llist_length(c_llist_t *list);
int c_llist_index(c_llist_t *list, const void *data);
c_llist_t *c_llist_nth(c_llist_t *list, unsigned int n);
void *c_llist_nth_data(c_llist_t *list, unsigned int n);
c_llist_t *c_llist_last(c_llist_t *list);
c_llist_t *c_llist_concat(c_llist_t *list1, c_llist_t *list2);
void c_llist_foreach(c_llist_t *list, c_iter_func_t func, void *user_data);
c_llist_t *c_llist_first(c_llist_t *list);
c_llist_t *c_llist_find(c_llist_t *list, const void *data);
c_llist_t *
c_llist_find_custom(c_llist_t *list, const void *data, c_compare_func_t func);
c_llist_t *c_llist_remove(c_llist_t *list, const void *data);
c_llist_t *c_llist_remove_all(c_llist_t *list, const void *data);
c_llist_t *c_llist_reverse(c_llist_t *list);
c_llist_t *c_llist_remove_link(c_llist_t *list, c_llist_t *link);
c_llist_t *c_llist_delete_link(c_llist_t *list, c_llist_t *link);
c_llist_t *
c_llist_insert_sorted(c_llist_t *list, void *data, c_compare_func_t func);
c_llist_t *c_llist_insert_before(c_llist_t *list, c_llist_t *sibling, void *data);
c_llist_t *c_llist_sort(c_llist_t *sort, c_compare_func_t func);


/*
 * Hashtables
 */
typedef struct _c_hash_table_t c_hash_table_t;
typedef struct _c_hash_table_iter_t c_hash_table_iter_t;

/* Private, but needed for stack allocation */
struct _c_hash_table_iter_t {
    void *dummy[8];
};

c_hash_table_t *c_hash_table_new(c_hash_func_t hash_func,
                                 c_equal_func_t key_equal_func);
c_hash_table_t *c_hash_table_new_full(c_hash_func_t hash_func,
                                      c_equal_func_t key_equal_func,
                                      c_destroy_func_t key_destroy_func,
                                      c_destroy_func_t value_destroy_func);
void c_hash_table_insert_replace(c_hash_table_t *hash,
                                 void *key,
                                 void *value,
                                 bool replace);
unsigned int c_hash_table_size(c_hash_table_t *hash);
c_llist_t *c_hash_table_get_keys(c_hash_table_t *hash);
void **c_hash_table_get_keys_array(c_hash_table_t *hash);
c_llist_t *c_hash_table_get_values(c_hash_table_t *hash);
void **c_hash_table_get_values_array(c_hash_table_t *hash);
void *c_hash_table_lookup(c_hash_table_t *hash, const void *key);
bool c_hash_table_lookup_extended(c_hash_table_t *hash,
                                  const void *key,
                                  void **orig_key,
                                  void **value);
bool c_hash_table_contains(c_hash_table_t *hash, const void *key);
void c_hash_table_foreach(c_hash_table_t *hash,
                          c_hash_iter_func_t func,
                          void *user_data);
void *c_hash_table_find(c_hash_table_t *hash,
                        c_hash_iter_remove_func_t predicate,
                        void *user_data);
bool c_hash_table_remove(c_hash_table_t *hash, const void *key);

/* assuming a value remains valid to reference after being removed
 * this allows combining a lookup and removal... */
void *c_hash_table_remove_value(c_hash_table_t *hash, const void *key);

bool c_hash_table_steal(c_hash_table_t *hash, const void *key);
void c_hash_table_remove_all(c_hash_table_t *hash);
unsigned int c_hash_table_foreach_remove(c_hash_table_t *hash,
                                         c_hash_iter_remove_func_t func,
                                         void *user_data);
unsigned int c_hash_table_foreach_steal(c_hash_table_t *hash,
                                        c_hash_iter_remove_func_t func,
                                        void *user_data);
void c_hash_table_destroy(c_hash_table_t *hash);
void c_hash_table_print_stats(c_hash_table_t *table);
void c_hash_table_print(c_hash_table_t *table);

void c_hash_table_iter_init(c_hash_table_iter_t *iter,
                            c_hash_table_t *hash_table);
bool
c_hash_table_iter_next(c_hash_table_iter_t *iter, void **key, void **value);

unsigned int c_spaced_primes_closest(unsigned int x);

#define c_hash_table_insert(h, k, v)                                           \
    c_hash_table_insert_replace((h), (k), (v), false)
#define c_hash_table_replace(h, k, v)                                          \
    c_hash_table_insert_replace((h), (k), (v), true)

bool c_direct_equal(const void *v1, const void *v2);
unsigned int c_direct_hash(const void *v1);
bool c_int_equal(const void *v1, const void *v2);
unsigned int c_int_hash(const void *v1);
bool c_int64_equal(const void *v1, const void *v2);
unsigned int c_int64_hash(const void *v1);
bool c_str_equal(const void *v1, const void *v2);
unsigned int c_str_hash(const void *v1);

/*
 * ByteArray
 */

typedef struct _c_byte_array_t c_byte_array_t;
struct _c_byte_array_t {
    uint8_t *data;
    int len;
};

c_byte_array_t *c_byte_array_new(void);
c_byte_array_t *c_byte_array_append(c_byte_array_t *array,
                                    const uint8_t *data,
                                    unsigned int len);
c_byte_array_t *c_byte_array_set_size(c_byte_array_t *array, unsigned int len);
uint8_t *c_byte_array_free(c_byte_array_t *array, bool free_segment);

/*
 * Array
 */

typedef struct _c_array_t c_array_t;
struct _c_array_t {
    char *data;
    int len;
};

c_array_t *
c_array_new(bool zero_terminated, bool clear_, unsigned int element_size);
c_array_t *c_array_sized_new(bool zero_terminated,
                             bool clear_,
                             unsigned int element_size,
                             unsigned int reserved_size);
char *c_array_free(c_array_t *array, bool free_segment);
c_array_t *
c_array_append_vals(c_array_t *array, const void *data, unsigned int len);
c_array_t *c_array_insert_vals(c_array_t *array,
                               unsigned int index_,
                               const void *data,
                               unsigned int len);
c_array_t *c_array_remove_index(c_array_t *array, unsigned int index_);
c_array_t *c_array_remove_index_fast(c_array_t *array, unsigned int index_);
unsigned int c_array_get_element_size(c_array_t *array);
void c_array_sort(c_array_t *array, c_compare_func_t compare);
c_array_t *c_array_set_size(c_array_t *array, unsigned int length);

#define c_array_append_val(a, v) (c_array_append_vals((a), &(v), 1))
#define c_array_insert_val(a, i, v) (c_array_insert_vals((a), (i), &(v), 1))
#define c_array_index(a, t, i) (((t *)(a)->data)[(i)])

/*
 * QSort
 */

void c_qsort_with_data(void *base,
                       size_t nmemb,
                       size_t size,
                       c_compare_data_func_t compare,
                       void *user_data);

/*
 * Pointer Array
 */

typedef struct _c_ptr_array_t c_ptr_array_t;
struct _c_ptr_array_t {
    void **pdata;
    unsigned int len;
};

c_ptr_array_t *c_ptr_array_new(void);
c_ptr_array_t *c_ptr_array_sized_new(unsigned int reserved_size);
c_ptr_array_t *
c_ptr_array_new_with_free_func(c_destroy_func_t element_free_func);
void c_ptr_array_add(c_ptr_array_t *array, void *data);
bool c_ptr_array_remove(c_ptr_array_t *array, void *data);
void *c_ptr_array_remove_index(c_ptr_array_t *array, unsigned int index);
bool c_ptr_array_remove_fast(c_ptr_array_t *array, void *data);
void *c_ptr_array_remove_index_fast(c_ptr_array_t *array, unsigned int index);
void c_ptr_array_sort(c_ptr_array_t *array, c_compare_func_t compare_func);
void c_ptr_array_sort_with_data(c_ptr_array_t *array,
                                c_compare_data_func_t compare_func,
                                void *user_data);
void c_ptr_array_set_size(c_ptr_array_t *array, int length);
void **c_ptr_array_free(c_ptr_array_t *array, bool free_seg);
void
c_ptr_array_foreach(c_ptr_array_t *array, c_iter_func_t func, void *user_data);
#define c_ptr_array_index(array, index) (array)->pdata[(index)]

/*
 * Queues
 */
typedef struct {
    c_llist_t *head;
    c_llist_t *tail;
    unsigned int length;
} c_queue_t;

#define C_QUEUE_INIT                                                           \
    {                                                                          \
        NULL, NULL, 0                                                          \
    }

void c_queue_init(c_queue_t *queue);
void *c_queue_peek_head(c_queue_t *queue);
void *c_queue_pop_head(c_queue_t *queue);
void *c_queue_peek_tail(c_queue_t *queue);
void *c_queue_pop_tail(c_queue_t *queue);
void c_queue_push_head(c_queue_t *queue, void *data);
void c_queue_push_tail(c_queue_t *queue, void *data);
bool c_queue_is_empty(c_queue_t *queue);
c_queue_t *c_queue_new(void);
void c_queue_free(c_queue_t *queue);
void c_queue_foreach(c_queue_t *queue, c_iter_func_t func, void *user_data);
c_llist_t *c_queue_find(c_queue_t *queue, const void *data);
void c_queue_clear(c_queue_t *queue);


/*
 * Conversions
 */

c_quark_t c_convert_error_quark(void);

/*
 * Unicode Manipulation: most of this is not used by Mono by default, it is
 * only used if the old collation code is activated, so this is only the
 * bare minimum to build.
 */

typedef enum {
    C_UNICODE_CONTROL,
    C_UNICODE_FORMAT,
    C_UNICODE_UNASSIGNED,
    C_UNICODE_PRIVATE_USE,
    C_UNICODE_SURROGATE,
    C_UNICODE_LOWERCASE_LETTER,
    C_UNICODE_MODIFIER_LETTER,
    C_UNICODE_OTHER_LETTER,
    C_UNICODE_TITLECASE_LETTER,
    C_UNICODE_UPPERCASE_LETTER,
    C_UNICODE_COMBININC_MARK,
    C_UNICODE_ENCLOSINC_MARK,
    C_UNICODE_NON_SPACINC_MARK,
    C_UNICODE_DECIMAL_NUMBER,
    C_UNICODE_LETTER_NUMBER,
    C_UNICODE_OTHER_NUMBER,
    C_UNICODE_CONNECT_PUNCTUATION,
    C_UNICODE_DASH_PUNCTUATION,
    C_UNICODE_CLOSE_PUNCTUATION,
    C_UNICODE_FINAL_PUNCTUATION,
    C_UNICODE_INITIAL_PUNCTUATION,
    C_UNICODE_OTHER_PUNCTUATION,
    C_UNICODE_OPEN_PUNCTUATION,
    C_UNICODE_CURRENCY_SYMBOL,
    C_UNICODE_MODIFIER_SYMBOL,
    C_UNICODE_MATH_SYMBOL,
    C_UNICODE_OTHER_SYMBOL,
    C_UNICODE_LINE_SEPARATOR,
    C_UNICODE_PARAGRAPH_SEPARATOR,
    C_UNICODE_SPACE_SEPARATOR
} c_unicode_type_t;

typedef enum {
    C_UNICODE_BREAK_MANDATORY,
    C_UNICODE_BREAK_CARRIAGE_RETURN,
    C_UNICODE_BREAK_LINE_FEED,
    C_UNICODE_BREAK_COMBININC_MARK,
    C_UNICODE_BREAK_SURROGATE,
    C_UNICODE_BREAK_ZERO_WIDTH_SPACE,
    C_UNICODE_BREAK_INSEPARABLE,
    C_UNICODE_BREAK_NON_BREAKING_GLUE,
    C_UNICODE_BREAK_CONTINGENT,
    C_UNICODE_BREAK_SPACE,
    C_UNICODE_BREAK_AFTER,
    C_UNICODE_BREAK_BEFORE,
    C_UNICODE_BREAK_BEFORE_AND_AFTER,
    C_UNICODE_BREAK_HYPHEN,
    C_UNICODE_BREAK_NON_STARTER,
    C_UNICODE_BREAK_OPEN_PUNCTUATION,
    C_UNICODE_BREAK_CLOSE_PUNCTUATION,
    C_UNICODE_BREAK_QUOTATION,
    C_UNICODE_BREAK_EXCLAMATION,
    C_UNICODE_BREAK_IDEOGRAPHIC,
    C_UNICODE_BREAK_NUMERIC,
    C_UNICODE_BREAK_INFIX_SEPARATOR,
    C_UNICODE_BREAK_SYMBOL,
    C_UNICODE_BREAK_ALPHABETIC,
    C_UNICODE_BREAK_PREFIX,
    C_UNICODE_BREAK_POSTFIX,
    C_UNICODE_BREAK_COMPLEX_CONTEXT,
    C_UNICODE_BREAK_AMBIGUOUS,
    C_UNICODE_BREAK_UNKNOWN,
    C_UNICODE_BREAK_NEXT_LINE,
    C_UNICODE_BREAK_WORD_JOINER,
    C_UNICODE_BREAK_HANGUL_L_JAMO,
    C_UNICODE_BREAK_HANGUL_V_JAMO,
    C_UNICODE_BREAK_HANGUL_T_JAMO,
    C_UNICODE_BREAK_HANGUL_LV_SYLLABLE,
    C_UNICODE_BREAK_HANGUL_LVT_SYLLABLE
} c_unicode_break_type_t;

c_codepoint_t c_codepoint_toupper(c_codepoint_t c);
c_codepoint_t c_codepoint_tolower(c_codepoint_t c);
c_codepoint_t c_codepoint_totitle(c_codepoint_t c);
c_unicode_type_t c_codepoint_type(c_codepoint_t c);
bool c_codepoint_isspace(c_codepoint_t c);
bool c_codepoint_isxdigit(c_codepoint_t c);
int c_codepoint_xdigit_value(c_codepoint_t c);
c_unicode_break_type_t c_codepoint_break_type(c_codepoint_t c);

#define c_assert(x)                                                            \
    C_STMT_START                                                               \
    {                                                                          \
        if (C_UNLIKELY(!(x)))                                                  \
            c_assertion_message(                                               \
                "* Assertion at %s:%d, condition `%s' not met\n",              \
                __FILE__,                                                      \
                __LINE__,                                                      \
                #x);                                                           \
    }                                                                          \
    C_STMT_END
#define c_assert_not_reached()                                                 \
    C_STMT_START                                                               \
    {                                                                          \
        c_assertion_message("* Assertion: should not be reached at %s:%d\n",   \
                            __FILE__,                                          \
                            __LINE__);                                         \
    }                                                                          \
    C_STMT_END

#define c_assert_cmpstr(s1, cmp, s2)                                           \
    C_STMT_START                                                               \
    {                                                                          \
        const char *_s1 = s1;                                                  \
        const char *_s2 = s2;                                                  \
        if (!(strcmp(_s1, _s2) cmp 0))                                         \
            c_assertion_message("* Assertion at %s:%d, condition \"%s\" " #cmp \
                                " \"%s\" failed\n",                            \
                                __FILE__,                                      \
                                __LINE__,                                      \
                                _s1,                                           \
                                _s2);                                          \
    }                                                                          \
    C_STMT_END
#define c_assert_cmpint(n1, cmp, n2)                                           \
    C_STMT_START                                                               \
    {                                                                          \
        int _n1 = n1;                                                          \
        int _n2 = n2;                                                          \
        if (!(_n1 cmp _n2))                                                    \
            c_assertion_message("* Assertion at %s:%d, condition %d " #cmp     \
                                " %d failed\n",                                \
                                __FILE__,                                      \
                                __LINE__,                                      \
                                _n1,                                           \
                                _n2);                                          \
    }                                                                          \
    C_STMT_END
#define c_assert_cmpuint(n1, cmp, n2)                                          \
    C_STMT_START                                                               \
    {                                                                          \
        int _n1 = n1;                                                          \
        int _n2 = n2;                                                          \
        if (!(_n1 cmp _n2))                                                    \
            c_assertion_message("* Assertion at %s:%d, condition %u " #cmp     \
                                " %u failed\n",                                \
                                __FILE__,                                      \
                                __LINE__,                                      \
                                _n1,                                           \
                                _n2);                                          \
    }                                                                          \
    C_STMT_END
#define c_assert_cmpfloat(n1, cmp, n2)                                         \
    C_STMT_START                                                               \
    {                                                                          \
        float _n1 = n1;                                                        \
        float _n2 = n2;                                                        \
        if (!(_n1 cmp _n2))                                                    \
            c_assertion_message("* Assertion at %s:%d, condition %f " #cmp     \
                                " %f failed\n",                                \
                                __FILE__,                                      \
                                __LINE__,                                      \
                                _n1,                                           \
                                _n2);                                          \
    }                                                                          \
    C_STMT_END

/*
 * Unicode conversion
 */

#define C_CONVERT_ERROR c_convert_error_quark()

typedef enum {
    C_CONVERT_ERROR_NO_CONVERSION,
    C_CONVERT_ERROR_ILLEGAL_SEQUENCE,
    C_CONVERT_ERROR_FAILED,
    C_CONVERT_ERROR_PARTIAL_INPUT,
    C_CONVERT_ERROR_BAD_URI,
    C_CONVERT_ERROR_NOT_ABSOLUTE_PATH
} c_convert_error_t;

char *c_utf8_strup(const char *str, c_ssize_t len);
char *c_utf8_strdown(const char *str, c_ssize_t len);
int c_codepoint_to_utf8(c_codepoint_t c, char *outbuf);
c_codepoint_t *
c_utf8_to_ucs4_fast(const char *str, long len, long *items_written);
c_codepoint_t *c_utf8_to_ucs4(const char *str,
                              long len,
                              long *items_read,
                              long *items_written,
                              c_error_t **err);
c_utf16_t *c_utf8_to_utf16(const char *str,
                           long len,
                           long *items_read,
                           long *items_written,
                           c_error_t **err);
c_utf16_t *eg_utf8_to_utf16_with_nuls(const char *str,
                                      long len,
                                      long *items_read,
                                      long *items_written,
                                      c_error_t **err);
char *c_utf16_to_utf8(const c_utf16_t *str,
                      long len,
                      long *items_read,
                      long *items_written,
                      c_error_t **err);
c_codepoint_t *c_utf16_to_ucs4(const c_utf16_t *str,
                               long len,
                               long *items_read,
                               long *items_written,
                               c_error_t **err);
char *c_ucs4_to_utf8(const c_codepoint_t *str,
                     long len,
                     long *items_read,
                     long *items_written,
                     c_error_t **err);
c_utf16_t *c_ucs4_to_utf16(const c_codepoint_t *str,
                           long len,
                           long *items_read,
                           long *items_written,
                           c_error_t **err);

#define u8to16(str) c_utf8_to_utf16(str, (long)strlen(str), NULL, NULL, NULL)

#ifdef WIN32
#define u16to8(str)                                                            \
    c_utf16_to_utf8(                                                           \
        (c_utf16_t *)(str), (long)wcslen((wchar_t *)(str)), NULL, NULL, NULL)
#else
#define u16to8(str) c_utf16_to_utf8(str, (long)strlen(str), NULL, NULL, NULL)
#endif

/*
 * Path
 */
char *c_build_path(const char *separator, const char *first_element, ...);
#define c_build_filename(x, ...) c_build_path(C_DIR_SEPARATOR_S, x, __VA_ARGS__)
char *c_path_get_dirname(const char *filename);
char *c_path_get_basename(const char *filename);
char *c_find_program_in_path(const char *program);
char *c_get_current_dir(void);
bool c_path_is_absolute(const char *filename);
char *c_path_get_relative_path(const char *parent,
                               const char *descendant);
char *c_path_normalize(char *filename, int *len_in_out);

const char *c_get_home_dir(void);
const char *c_get_tmp_dir(void);
const char *c_get_user_name(void);
char *c_get_prgname(void);
void c_set_prgname(const char *prgname);

/*
 * Shell
 */

c_quark_t c_shell_error_get_quark(void);

#define C_SHELL_ERROR c_shell_error_get_quark()

typedef enum {
    C_SHELL_ERROR_BAD_QUOTING,
    C_SHELL_ERROR_EMPTY_STRING,
    C_SHELL_ERROR_FAILED
} c_shell_error_t;

bool c_shell_parse_argv(const char *command_line,
                        int *argcp,
                        char ***argvp,
                        c_error_t **error);
char *c_shell_unquote(const char *quoted_string, c_error_t **error);
char *c_shell_quote(const char *unquoted_string);

/*
 * Spawn
 */

#define C_SPAWN_ERROR c_shell_error_get_quark()

typedef enum {
    C_SPAWN_ERROR_FORK,
    C_SPAWN_ERROR_READ,
    C_SPAWN_ERROR_CHDIR,
    C_SPAWN_ERROR_ACCES,
    C_SPAWN_ERROR_PERM,
    C_SPAWN_ERROR_TOO_BIG,
    C_SPAWN_ERROR_NOEXEC,
    C_SPAWN_ERROR_NAMETOOLONG,
    C_SPAWN_ERROR_NOENT,
    C_SPAWN_ERROR_NOMEM,
    C_SPAWN_ERROR_NOTDIR,
    C_SPAWN_ERROR_LOOP,
    C_SPAWN_ERROR_TXTBUSY,
    C_SPAWN_ERROR_IO,
    C_SPAWN_ERROR_NFILE,
    C_SPAWN_ERROR_MFILE,
    C_SPAWN_ERROR_INVAL,
    C_SPAWN_ERROR_ISDIR,
    C_SPAWN_ERROR_LIBBAD,
    C_SPAWN_ERROR_FAILED
} c_spawn_error_t;

typedef enum {
    C_SPAWN_LEAVE_DESCRIPTORS_OPEN = 1,
    C_SPAWN_DO_NOT_REAP_CHILD = 1 << 1,
    C_SPAWN_SEARCH_PATH = 1 << 2,
    C_SPAWN_STDOUT_TO_DEV_NULL = 1 << 3,
    C_SPAWN_STDERR_TO_DEV_NULL = 1 << 4,
    C_SPAWN_CHILD_INHERITS_STDIN = 1 << 5,
    C_SPAWN_FILE_AND_ARGV_ZERO = 1 << 6
} c_spawn_flags_t;

typedef void (*c_spawn_child_setup_func_t)(void *user_data);

bool c_spawn_sync(const char *working_dir,
                  char **argv,
                  char **envp,
                  c_spawn_flags_t flags,
                  c_spawn_child_setup_func_t child_setup,
                  void *user_data,
                  char **standard_output,
                  char **standard_error,
                  int *exit_status,
                  c_error_t **error);
bool c_spawn_command_line_sync(const char *command_line,
                               char **standard_output,
                               char **standard_error,
                               int *exit_status,
                               c_error_t **error);
bool c_spawn_async_with_pipes(const char *working_directory,
                              char **argv,
                              char **envp,
                              c_spawn_flags_t flags,
                              c_spawn_child_setup_func_t child_setup,
                              void *user_data,
                              c_pid_t *child_pid,
                              int *standard_input,
                              int *standard_output,
                              int *standard_error,
                              c_error_t **error);

/*
 * Timer
 */
typedef struct _c_timer_t c_timer_t;

int64_t c_get_monotonic_time(void);

c_timer_t *c_timer_new(void);
void c_timer_destroy(c_timer_t *timer);
double c_timer_elapsed(c_timer_t *timer, unsigned long *microseconds);
void c_timer_stop(c_timer_t *timer);
void c_timer_start(c_timer_t *timer);

/*
 * Date and time
 */
typedef struct {
    long tv_sec;
    long tv_usec;
} c_timeval_t;

void c_get_current_time(c_timeval_t *result);
void c_usleep(unsigned long microseconds);

/*
 * File
 */

c_quark_t c_file_error_quark(void);

#define C_FILE_ERROR c_file_error_quark()

typedef enum {
    C_FILE_ERROR_EXIST,
    C_FILE_ERROR_ISDIR,
    C_FILE_ERROR_ACCES,
    C_FILE_ERROR_NAMETOOLONG,
    C_FILE_ERROR_NOENT,
    C_FILE_ERROR_NOTDIR,
    C_FILE_ERROR_NXIO,
    C_FILE_ERROR_NODEV,
    C_FILE_ERROR_ROFS,
    C_FILE_ERROR_TXTBSY,
    C_FILE_ERROR_FAULT,
    C_FILE_ERROR_LOOP,
    C_FILE_ERROR_NOSPC,
    C_FILE_ERROR_NOMEM,
    C_FILE_ERROR_MFILE,
    C_FILE_ERROR_NFILE,
    C_FILE_ERROR_BADF,
    C_FILE_ERROR_INVAL,
    C_FILE_ERROR_PIPE,
    C_FILE_ERROR_AGAIN,
    C_FILE_ERROR_INTR,
    C_FILE_ERROR_IO,
    C_FILE_ERROR_PERM,
    C_FILE_ERROR_NOSYS,
    C_FILE_ERROR_FAILED
} c_file_error_t;

typedef enum {
    C_FILE_TEST_IS_REGULAR = 1 << 0,
    C_FILE_TEST_IS_SYMLINK = 1 << 1,
    C_FILE_TEST_IS_DIR = 1 << 2,
    C_FILE_TEST_IS_EXECUTABLE = 1 << 3,
    C_FILE_TEST_EXISTS = 1 << 4
} c_file_test_t;

bool c_file_set_contents(const char *filename,
                         const char *contents,
                         c_ssize_t length,
                         c_error_t **error);
bool c_file_get_contents(const char *filename,
                         char **contents,
                         size_t *length,
                         c_error_t **error);
c_file_error_t c_file_error_from_errno(int err_no);
int c_file_open_tmp(const char *tmpl, char **name_used, c_error_t **error);
bool c_file_test(const char *filename, c_file_test_t test);

#define c_open open
#define c_rename rename
#define c_stat stat
#define c_unlink unlink
#define c_fopen fopen
#define c_lstat lstat
#define c_rmdir rmdir
#define c_mkstemp mkstemp
#define c_ascii_isdigit isdigit
#define c_ascii_isalnum isalnum

typedef struct _c_mem_file_t c_mem_file_t;
int c_mem_file_write(c_mem_file_t *file, const char *buf, int len);
int c_mem_file_seek(c_mem_file_t *file, int pos, int whence);
int c_mem_file_close(c_mem_file_t *file);
c_mem_file_t *c_mem_file_open(void *buf, size_t size, const char *mode);

/*
 * Directory
 */
typedef struct _c_dir_t c_dir_t;
c_dir_t *c_dir_open(const char *path, unsigned int flags, c_error_t **error);
const char *c_dir_read_name(c_dir_t *dir);
void c_dir_rewind(c_dir_t *dir);
void c_dir_close(c_dir_t *dir);

int c_mkdir_with_parents(const char *pathname, int mode);
#define c_mkdir mkdir

/*
 * Character set conversion
 */
typedef struct _c_iconv_t *c_iconv_t;

size_t c_iconv(c_iconv_t cd,
               char **inbytes,
               size_t *inbytesleft,
               char **outbytes,
               size_t *outbytesleft);
c_iconv_t c_iconv_open(const char *to_charset, const char *from_charset);
int c_iconv_close(c_iconv_t cd);

bool c_get_charset(C_CONST_RETURN char **charset);
char *c_locale_to_utf8(const char *opsysstring,
                       c_ssize_t len,
                       size_t *bytes_read,
                       size_t *bytes_written,
                       c_error_t **error);
char *c_locale_from_utf8(const char *utf8string,
                         c_ssize_t len,
                         size_t *bytes_read,
                         size_t *bytes_written,
                         c_error_t **error);
char *c_filename_from_utf8(const char *utf8string,
                           c_ssize_t len,
                           size_t *bytes_read,
                           size_t *bytes_written,
                           c_error_t **error);
char *c_convert(const char *str,
                c_ssize_t len,
                const char *to_codeset,
                const char *from_codeset,
                size_t *bytes_read,
                size_t *bytes_written,
                c_error_t **error);
char *c_filename_display_name(const char *filename);

/*
 * Unicode manipulation
 */
extern const unsigned char c_utf8_jump_table[256];

bool c_utf8_validate(const char *str, c_ssize_t max_len, const char **end);
c_codepoint_t c_utf8_get_char_validated(const char *str, c_ssize_t max_len);
char *c_utf8_find_prev_char(const char *str, const char *p);
char *c_utf8_prev_char(const char *str);
#define c_utf8_next_char(p) ((p) + c_utf8_jump_table[(unsigned char)(*p)])
c_codepoint_t c_utf8_get_char(const char *src);
long c_utf8_strlen(const char *str, c_ssize_t max);
char *c_utf8_offset_to_pointer(const char *str, long offset);
long c_utf8_pointer_to_offset(const char *str, const char *pos);

/*
 * priorities
 */
#define C_PRIORITY_DEFAULT 0
#define C_PRIORITY_DEFAULT_IDLE 200

#if defined(C_HAVE_PTHREADS)
#  define C_SUPPORTS_THREADS 1
typedef pthread_key_t c_tls_t;
#  elif defined(WIN32)
#  define C_SUPPORTS_THREADS 1
typedef struct _c_tls_t c_tls_t;
struct _c_tls_t {
    DWORD key;
    void (*destroy)(void *tls_data);
    c_tls_t *next;
};
#else
typedef struct _c_tls_t {
    void *data;
} c_tls_t;
#endif

/* Note: it's the caller's responsibility to ensure c_tls_init() is
 * only called once per c_tls_t */

#if defined(C_HAVE_PTHREADS)
void c_tls_init(c_tls_t *tls, void (*destroy)(void *data));

static inline void
c_tls_set(c_tls_t *tls, void *data)
{
    pthread_key_t key = *(pthread_key_t *)tls;
    pthread_setspecific(key, data);
}

static inline void *
c_tls_get(c_tls_t *tls)
{
    pthread_key_t key = *(pthread_key_t *)tls;
    return pthread_getspecific(key);
}

#elif defined(WIN32)

void c_tls_init(c_tls_t *tls, void (*destroy)(void *data));

static inline void
c_tls_set(c_tls_t *tls, void *data)
{
    TlsSetValue(tls->key, data);
}

static inline void *
c_tls_get(c_tls_t *tls)
{
    return TlsGetValue(tls->key);
}
#else

static inline void
c_tls_init(c_tls_t *tls, void (*destroy)(void *data)) { }

static inline void
c_tls_set(c_tls_t *tls, void *data)
{
    tls->data = data;
}

static inline void *
c_tls_get(c_tls_t *tls)
{
    return tls->data;
}

#endif

#ifdef USE_UV
/* N.B This is not a recursive mutex */
typedef uv_mutex_t c_mutex_t;

static inline void c_mutex_init(c_mutex_t *mutex)
{
    int r = uv_mutex_init((uv_mutex_t *)mutex);
    if (C_UNLIKELY(r))
        c_error("Failed to initialize mutex: %s", uv_strerror(r));
}

static inline void c_mutex_destroy(c_mutex_t *mutex)
{
    uv_mutex_destroy((uv_mutex_t *)mutex);
}

static inline void c_mutex_lock(c_mutex_t *mutex)
{
    uv_mutex_lock(mutex);
}

static inline bool c_mutex_trylock(c_mutex_t *mutex)
{
    return uv_mutex_trylock((uv_mutex_t *)mutex) == 0;
}

static inline void c_mutex_unlock(c_mutex_t *mutex)
{
    uv_mutex_unlock((uv_mutex_t *)mutex);
}
#else
typedef int c_mutex_t;

#  ifndef C_SUPPORTS_THREADS
static inline void c_mutex_init(c_mutex_t *mutex) {}
static inline void c_mutex_destroy(c_mutex_t *mutex) {}
static inline void c_mutex_lock(c_mutex_t *mutex) {}
static inline bool c_mutex_trylock(c_mutex_t *mutex) { return true; }
static inline void c_mutex_unlock(c_mutex_t *mutex) {}
#  else
static inline void c_mutex_init(c_mutex_t *mutex) { c_assert_not_reached(); }
static inline void c_mutex_destroy(c_mutex_t *mutex) { c_assert_not_reached(); }
static inline void c_mutex_lock(c_mutex_t *mutex) { c_assert_not_reached(); }
static inline bool c_mutex_trylock(c_mutex_t *mutex) { c_assert_not_reached(); return true; }
static inline void c_mutex_unlock(c_mutex_t *mutex) { c_assert_not_reached(); }
#  endif
#endif

#define _CLIB_MAJOR 2
#define _CLIB_MIDDLE 4
#define _CLIB_MINOR 0

#define CLIB_CHECK_VERSION(a, b, c)                                            \
    ((a < _CLIB_MAJOR) ||                                                      \
     (a == _CLIB_MAJOR &&(b < _CLIB_MIDDLE ||                                  \
                          (b == _CLIB_MIDDLE &&c <= _CLIB_MINOR))))

/*
 * Random numbers
 */
typedef struct _c_rand_t c_rand_t;

c_rand_t *c_rand_new(void);
c_rand_t *c_rand_new_with_seed_array(uint32_t *array, int len);
c_rand_t *c_rand_new_with_seed(uint32_t seed);
void c_rand_free(c_rand_t *rand);
float c_rand_float(c_rand_t *rand);
float c_rand_float_range(c_rand_t *rand, float begin, float end);
double c_rand_double(c_rand_t *rand);
double c_rand_double_range(c_rand_t *rand, double begin, double end);
uint32_t c_rand_uint32(c_rand_t *rand);
int32_t c_rand_int32_range(c_rand_t *rand, int32_t begin, int32_t end);
bool c_rand_boolean(c_rand_t *rand);

/* XXX: Not thread safe!!! */
float c_random_float(void);
float c_random_float_range(float begin, float end);
double c_random_double(void);
double c_random_double_range(double begin, double end);
uint32_t c_random_uint32(void);
int32_t c_random_int32_range(int32_t begin, int32_t end);
bool c_random_boolean(void);

#ifdef C_PLATFORM_LINUX
/* XDG dirs */
const char *c_get_xdg_data_home(void);
const char *c_get_xdg_data_dirs(void);
typedef void (*c_xdg_dir_callback_t)(const char *dir, void *data);
void c_foreach_xdg_data_dir(c_xdg_dir_callback_t callback, void *user_data);
#endif


enum c_url_fields
{
    C_URL_SCHEMA           = 0,
    C_URL_HOST             = 1,
    C_URL_PORT             = 2,
    C_URL_PATH             = 3,
    C_URL_QUERY            = 4,
    C_URL_FRAGMENT         = 5,
    C_URL_USERINFO         = 6,
    C_URL_MAX              = 7,
};


/* Result structure for c_parse_url().
 *
 * Callers should index into field_data[] with C_RL_* values iff field_set
 * has the relevant (1 << C_URL_*) bit set. As a courtesy to clients (and
 * because we probably have padding left over), we convert any port to
 * a uint16_t.
 */
struct c_url {
    uint16_t field_set;           /* Bitmask of (1 << UF_*) values */
    uint16_t port;                /* Converted C_URL_PORT string */

    struct {
        uint16_t off;               /* Offset into buffer in which field starts */
        uint16_t len;               /* Length of run in buffer */
    } field_data[C_URL_MAX];
};

int
c_parse_url(const char *buf,
            size_t buflen,
            int is_connect,
            struct c_url *u);

C_END_DECLS

#include <crbtree.h>
