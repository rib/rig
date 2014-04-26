#ifndef __ULIB_H
#define __ULIB_H

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <ctype.h>
#include <limits.h>
#include <float.h>

//TODO: remove (for temporary compatibility)
#include <glib.h>

#ifdef _MSC_VER
#pragma include_alias(<ulib-config.h>, <ulib-config.hw>)
#endif

#include <stdint.h>
#include <inttypes.h>

#include <ulib-config.h>

#ifdef U_HAVE_ALLOCA_H
#include <alloca.h>
#endif

#ifdef WIN32
/* For alloca */
#include <malloc.h>
#endif

#ifndef offsetof
#   define offsetof(s_name,n_name) (size_t)(char *)&(((s_name*)0)->m_name)
#endif

#define __ULIB_X11 1

#ifdef  __cplusplus
#define U_BEGIN_DECLS  extern "C" {
#define U_END_DECLS    }
#else
#define U_BEGIN_DECLS
#define U_END_DECLS
#endif

U_BEGIN_DECLS

#ifdef U_OS_WIN32
/* MSC and Cross-compilatin will use this */
int vasprintf (char **strp, const char *fmt, va_list ap);
#endif


/*
 * Basic data types
 */
typedef ssize_t  ussize;
typedef int32_t  uboolean;
typedef uint16_t uunichar2;
typedef uint32_t uunichar;


/*
 * Macros
 */
#define U_N_ELEMENTS(s)                   (sizeof(s) / sizeof ((s) [0]))
#define U_STRINGIFY_ARG(arg)              #arg
#define U_STRINGIFY(arg)                  U_STRINGIFY_ARG(arg)
#define U_PASTE_ARGS(part0, part1)        part0 ## part1
#define U_PASTE(part0, part1)             U_PASTE_ARGS (part0, part1)

#ifndef FALSE
#define FALSE                0
#endif

#ifndef TRUE
#define TRUE                 1
#endif

#define U_MINSHORT           SHRT_MIN
#define U_MAXSHORT           SHRT_MAX
#define U_MAXUSHORT          USHRT_MAX
#define U_MAXINT             INT_MAX
#define U_MININT             INT_MIN
#define U_MAXINT32           INT32_MAX
#define U_MAXUINT32          UINT32_MAX
#define U_MININT32           INT32_MIN
#define U_MININT64           INT64_MIN
#define U_MAXINT64	     INT64_MAX
#define U_MAXUINT64	     UINT64_MAX
#define U_MAXFLOAT           FLT_MAX

#define U_UINT64_FORMAT     PRIu64
#define U_INT64_FORMAT      PRId64
#define U_UINT32_FORMAT     PRIu32
#define U_INT32_FORMAT      PRId32

#define U_LITTLE_ENDIAN 1234
#define U_BIU_ENDIAN    4321
#define U_STMT_START    do
#define U_STMT_END      while (0)

#define U_USEC_PER_SEC  1000000
#define U_PI            3.141592653589793238462643383279502884L
#define U_PI_2          1.570796326794896619231321691639751442L

#define U_INT64_CONSTANT(val)   (val##LL)
#define U_UINT64_CONSTANT(val)  (val##UL)

#define U_POINTER_TO_INT(ptr)   ((int)(intptr_t)(ptr))
#define U_POINTER_TO_UINT(ptr)  ((unsigned int)(uintptr_t)(ptr))
#define U_INT_TO_POINTER(v)     ((void *)(intptr_t)(v))
#define U_UINT_TO_POINTER(v)    ((void *)(uintptr_t)(v))

#define U_STRUCT_OFFSET(p_type,field) offsetof(p_type,field)

#define U_STRLOC __FILE__ ":" U_STRINGIFY(__LINE__) ":"

#define U_CONST_RETURN const

#define U_BYTE_ORDER U_LITTLE_ENDIAN

#if defined(__GNUC__)
#  define U_GNUC_GNUSED                       __attribute__((__unused__))
#  define U_GNUC_NORETURN                     __attribute__((__noreturn__))
#  define U_LIKELY(expr)                      (__builtin_expect ((expr) != 0, 1))
#  define U_UNLIKELY(expr)                    (__builtin_expect ((expr) != 0, 0))
#  define U_GNUC_PRINTF(format_idx, arg_idx)  __attribute__((__format__ (__printf__, format_idx, arg_idx)))
#else
#  define U_GNUC_GNUSED
#  define U_GNUC_NORETURN
#  define U_LIKELY(expr) (expr)
#  define U_UNLIKELY(expr) (expr)
#  define U_GNUC_PRINTF(format_idx, arg_idx)
#endif

#if defined(__GNUC__)
#  define U_STRFUNC ((const char *)(__PRETTY_FUNCTION__))
#elif defined (_MSC_VER)
#  define U_STRFUNC ((const char*) (__FUNCTION__))
#else
#  define U_STRFUNC ((const char*) (__func__))
#endif

#if defined (_MSC_VER)
#  define U_VA_COPY(dest, src) ((dest) = (src))
#else
#  define U_VA_COPY(dest, src) va_copy (dest, src)
#endif

#ifdef OS_UNIX
#define U_BREAKPOINT() U_STMT_START { raise (SIGTRAP); } U_STMT_END
#else
#define U_BREAKPOINT()
#endif

#if defined (__native_client__)
#undef U_BREAKPOINT
#define U_BREAKPOINT()
#endif

#ifndef ABS
#define ABS(a)    ((a) > 0 ? (a) : -(a))
#endif

#ifndef MAX
#define MAX(a,b)  (((a)>(b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b)  (((a)<(b)) ? (a) : (b))
#endif

#ifndef CLAMP
#define CLAMP(a,low,high) (((a) < (low)) ? (low) : (((a) > (high)) ? (high) : (a)))
#endif



/*
 * Allocation
 */
void u_free (void *ptr);
void * u_realloc (void * obj, size_t size);
void * u_malloc (size_t x);
void * u_malloc0 (size_t x);
void * u_try_malloc (size_t x);
void * u_try_realloc (void * obj, size_t size);
void * u_memdup (const void * mem, unsigned int byte_size);

#define u_new(type,size)        ((type *) u_malloc (sizeof (type) * (size)))
#define u_new0(type,size)       ((type *) u_malloc0 (sizeof (type)* (size)))
#define u_newa(type,size)       ((type *) alloca (sizeof (type) * (size)))

#define u_memmove(dest,src,len) memmove (dest, src, len)
#define u_renew(struct_type, mem, n_structs) u_realloc (mem, sizeof (struct_type) * n_structs)
#define u_alloca(size)		alloca (size)

#define u_slice_new(type)         ((type *) u_malloc (sizeof (type)))
#define u_slice_new0(type)        ((type *) u_malloc0 (sizeof (type)))
#define u_slice_free(type, mem)   u_free (mem)
#define u_slice_free1(size, mem)  u_free (mem)
#define u_slice_alloc(size)       u_malloc (size)
#define u_slice_alloc0(size)      u_malloc0 (size)
#define u_slice_dup(type, mem)    u_memdup (mem, sizeof (type))
#define u_slice_copy(size, mem)   u_memdup (mem, size)

static inline char   *u_strdup (const char *str) { if (str) {return strdup (str);} return NULL; }
char **u_strdupv (char **str_array);
int u_strcmp0 (const char *str1, const char *str2);

typedef struct {
	void * (*malloc)      (size_t    n_bytes);
	void * (*realloc)     (void * mem, size_t n_bytes);
	void     (*free)        (void * mem);
	void * (*calloc)      (size_t    n_blocks, size_t n_block_bytes);
	void * (*try_malloc)  (size_t    n_bytes);
	void * (*try_realloc) (void * mem, size_t n_bytes);
} UMemVTable;

#define u_mem_set_vtable(x)

struct _UMemChunk {
	unsigned int alloc_size;
};

typedef struct _UMemChunk UMemChunk;
/*
 * Misc.
 */
#define u_atexit(func)	((void) atexit (func))

const char *    u_getenv(const char *variable);
uboolean         u_setenv(const char *variable, const char *value, uboolean overwrite);
void             u_unsetenv(const char *variable);

char*           u_win32_getlocale(void);

/*
 * Precondition macros
 */
#define u_warn_if_fail(x)  U_STMT_START { if (!(x)) { u_warning ("%s:%d: assertion '%s' failed", __FILE__, __LINE__, #x); } } U_STMT_END
#define u_warn_if_reached() u_warning ("%s:%d: code should not be reached!", __FILE__, __LINE__)

#define u_return_if_fail(x)  U_STMT_START { if (!(x)) { u_critical ("%s:%d: assertion '%s' failed", __FILE__, __LINE__, #x); return; } } U_STMT_END
#define u_return_val_if_fail(x,e)  U_STMT_START { if (!(x)) { u_critical ("%s:%d: assertion '%s' failed", __FILE__, __LINE__, #x); return (e); } } U_STMT_END
#define u_return_if_reached(e) U_STMT_START { u_warning ("%s:%d: code should not be reached, returning!",  __FILE__, __LINE__); return; } U_STMT_END
#define u_return_val_if_reached(e) U_STMT_START { u_warning ("%s:%d: code should not be reached, returning!",  __FILE__, __LINE__); return (e); } U_STMT_END

/*
 * DebugKeys
 */
typedef struct _UDebugKey {
	const char *key;
	unsigned int	     value;
} UDebugKey;

unsigned int   u_parse_debug_string (const char *string, const UDebugKey *keys, unsigned int nkeys);

/*
 * Quark
 */

typedef uint32_t UQuark;

UQuark
u_quark_from_static_string (const char *string);

/*
 * Errors
 */
typedef struct {
	UQuark  domain;
	int    code;
	char  *message;
} UError;

void      u_clear_error (UError **error);
void      u_error_free (UError *error);
UError   *u_error_new  (UQuark domain, int code, const char *format, ...);
UError   *u_error_new_valist (UQuark domain, int code, const char *format, va_list ap);
void      u_set_error  (UError **err, UQuark domain, int code, const char *format, ...);
void      u_propagate_error (UError **dest, UError *src);
UError   *u_error_copy (const UError *error);
uboolean  u_error_matches (const UError *error, UQuark domain, int code);

/*
 * Strings utility
 */
char       *u_strdup_printf  (const char *format, ...);
char       *u_strdup_vprintf (const char *format, va_list args);
char       *u_strndup        (const char *str, size_t n);
const char *u_strerror       (int errnum);
char       *u_strndup        (const char *str, size_t n);
void         u_strfreev       (char **str_array);
char       *u_strconcat      (const char *first, ...);
char      **u_strsplit       (const char *string, const char *delimiter, int max_tokens);
char      **u_strsplit_set   (const char *string, const char *delimiter, int max_tokens);
char       *u_strreverse     (char *str);
uboolean     u_str_has_prefix (const char *str, const char *prefix);
uboolean     u_str_has_suffix (const char *str, const char *suffix);
unsigned int        u_strv_length    (char **str_array);
char       *u_strjoin        (const char *separator, ...);
char       *u_strjoinv       (const char *separator, char **str_array);
char       *u_strchug        (char *str);
char       *u_strchomp       (char *str);
void         u_strdown        (char *string);
char       *u_strnfill       (size_t length, char fill_char);

char       *u_strdelimit     (char *string, const char *delimiters, char new_delimiter);
char       *u_strescape      (const char *source, const char *exceptions);

char       *u_filename_to_uri   (const char *filename, const char *hostname, UError **error);
char       *u_filename_from_uri (const char *uri, char **hostname, UError **error);

int         u_printf          (char const *format, ...);
int         u_fprintf         (FILE *file, char const *format, ...);
int         u_sprintf         (char *string, char const *format, ...);
int         u_snprintf        (char *string, unsigned long n, char const *format, ...);
#define u_vprintf vprintf
#define u_vfprintf vfprintf
#define u_vsprintf vsprintf
#define u_vsnprintf vsnprintf
#define u_vasprintf vasprintf

size_t   u_strlcpy            (char *dest, const char *src, size_t dest_size);
char  *u_stpcpy             (char *dest, const char *src);


char   u_ascii_tolower      (char c);
char   u_ascii_toupper      (char c);
char  *u_ascii_strdown      (const char *str, ussize len);
char  *u_ascii_strup        (const char *str, ussize len);
int    u_ascii_strncasecmp  (const char *s1, const char *s2, size_t n);
int    u_ascii_strcasecmp   (const char *s1, const char *s2);
int    u_ascii_xdigit_value (char c);
#define u_ascii_isspace(c)   (isspace (c) != 0)
#define u_ascii_isalpha(c)   (isalpha (c) != 0)
#define u_ascii_isprint(c)   (isprint (c) != 0)
#define u_ascii_isxdigit(c)  (isxdigit (c) != 0)

/* FIXME: u_strcasecmp supports utf8 unicode stuff */
#ifdef _MSC_VER
#define u_strcasecmp stricmp
#define u_strncasecmp strnicmp
#define u_strstrip(a) u_strchug (u_strchomp (a))
#else
#define u_strcasecmp strcasecmp
#define u_ascii_strtoull strtoull
#define u_strncasecmp strncasecmp
#define u_strstrip(a) u_strchug (u_strchomp (a))
#endif
#define u_ascii_strdup strdup


#define	U_STR_DELIMITERS "_-|> <."

/*
 * String type
 */
typedef struct {
	char *str;
	size_t len;
	size_t allocated_len;
} UString;

UString     *u_string_new           (const char *init);
UString     *u_string_new_len       (const char *init, ussize len);
UString     *u_string_sized_new     (size_t default_size);
char       *u_string_free          (UString *string, uboolean free_segment);
UString     *u_string_assign        (UString *string, const char *val);
UString     *u_string_append        (UString *string, const char *val);
void         u_string_printf        (UString *string, const char *format, ...);
void         u_string_append_printf (UString *string, const char *format, ...);
void         u_string_append_vprintf (UString *string, const char *format, va_list args);
UString     *u_string_append_unichar (UString *string, uunichar c);
UString     *u_string_append_c      (UString *string, char c);
UString     *u_string_append        (UString *string, const char *val);
UString     *u_string_append_len    (UString *string, const char *val, ussize len);
UString     *u_string_truncate      (UString *string, size_t len);
UString     *u_string_prepend       (UString *string, const char *val);
UString     *u_string_insert        (UString *string, ussize pos, const char *val);
UString     *u_string_set_size      (UString *string, size_t len);
UString     *u_string_erase         (UString *string, ussize pos, ussize len);

#define u_string_sprintfa u_string_append_printf

typedef void     (*UFunc)          (void * data, void * user_data);
typedef int     (*UCompareFunc)   (const void * a, const void * b);
typedef int     (*UCompareDataFunc) (const void * a, const void * b, void * user_data);
typedef void     (*UHFunc)         (void * key, void * value, void * user_data);
typedef uboolean (*UHRFunc)        (void * key, void * value, void * user_data);
typedef void     (*UDestroyNotify) (void * data);
typedef unsigned int    (*UHashFunc)      (const void * key);
typedef uboolean (*UEqualFunc)     (const void * a, const void * b);
typedef void     (*UFreeFunc)      (void *       data);

/*
 * Lists
 */
typedef struct _USList USList;
struct _USList {
	void * data;
	USList *next;
};

USList *u_slist_alloc         (void);
USList *u_slist_append        (USList        *list,
			       void *       data);
USList *u_slist_prepend       (USList        *list,
			       void *       data);
void    u_slist_free          (USList        *list);
void    u_slist_free_1        (USList        *list);
USList *u_slist_copy          (USList        *list);
USList *u_slist_concat        (USList        *list1,
			       USList        *list2);
void    u_slist_foreach       (USList        *list,
			       UFunc          func,
			       void *       user_data);
USList *u_slist_last          (USList        *list);
USList *u_slist_find          (USList        *list,
			       const void *  data);
USList *u_slist_find_custom   (USList	     *list,
			       const void *  data,
			       UCompareFunc   func);
USList *u_slist_remove        (USList        *list,
			       const void *  data);
USList *u_slist_remove_all    (USList        *list,
			       const void *  data);
USList *u_slist_reverse       (USList        *list);
unsigned int   u_slist_length        (USList        *list);
USList *u_slist_remove_link   (USList        *list,
			       USList        *link);
USList *u_slist_delete_link   (USList        *list,
			       USList        *link);
USList *u_slist_insert_sorted (USList        *list,
			       void *       data,
			       UCompareFunc   func);
USList *u_slist_insert_before (USList        *list,
			       USList        *sibling,
			       void *       data);
USList *u_slist_sort          (USList        *list,
			       UCompareFunc   func);
int    u_slist_index	      (USList        *list,
			       const void *  data);
USList *u_slist_nth	      (USList	     *list,
			       unsigned int	      n);
void * u_slist_nth_data     (USList	     *list,
			       unsigned int	      n);

#define u_slist_next(slist) ((slist) ? (((USList *) (slist))->next) : NULL)


typedef struct _UList UList;
struct _UList {
  void * data;
  UList *next;
  UList *prev;
};

#define u_list_next(list) ((list) ? (((UList *) (list))->next) : NULL)
#define u_list_previous(list) ((list) ? (((UList *) (list))->prev) : NULL)

UList *u_list_alloc         (void);
UList *u_list_append        (UList         *list,
			     void *       data);
UList *u_list_prepend       (UList         *list,
			     void *       data);
void   u_list_free          (UList         *list);
void   u_list_free_full     (UList         *list,
                             UDestroyNotify free_func);
void   u_list_free_1        (UList         *list);
UList *u_list_copy          (UList         *list);
unsigned int  u_list_length        (UList         *list);
int   u_list_index         (UList         *list,
			     const void *  data);
UList *u_list_nth           (UList         *list,
			     unsigned int          n);
void * u_list_nth_data      (UList         *list,
			     unsigned int          n);
UList *u_list_last          (UList         *list);
UList *u_list_concat        (UList         *list1,
			     UList         *list2);
void   u_list_foreach       (UList         *list,
			     UFunc          func,
			     void *       user_data);
UList *u_list_first         (UList         *list);
UList *u_list_find          (UList         *list,
			     const void *  data);
UList *u_list_find_custom   (UList	   *list,
			     const void *  data,
			     UCompareFunc   func);
UList *u_list_remove        (UList         *list,
			     const void *  data);
UList *u_list_remove_all    (UList         *list,
			     const void *  data);
UList *u_list_reverse       (UList         *list);
UList *u_list_remove_link   (UList         *list,
			     UList         *link);
UList *u_list_delete_link   (UList         *list,
			     UList         *link);
UList *u_list_insert_sorted (UList         *list,
			     void *       data,
			     UCompareFunc   func);
UList *u_list_insert_before (UList         *list,
			     UList         *sibling,
			     void *       data);
UList *u_list_sort          (UList         *sort,
			     UCompareFunc   func);

/*
 * HookLists
 */
typedef void (*UHookFunc) (void * data);

typedef struct _UHook UHook;
typedef struct _UHookList UHookList;

struct _UHook {
  UHook	*next;
  UHook	*prev;
  void * data;
  void * func;
  uboolean in_call;
};

struct _UHookList {
  UHook *hooks;
};

void
u_hook_list_init (UHookList *hook_list,
                  unsigned int hook_size);

void
u_hook_list_invoke (UHookList *hook_list,
                    uboolean may_recurse);

void
u_hook_list_clear (UHookList *hook_list);

UHook *
u_hook_alloc (UHookList *hook_list);

UHook *
u_hook_find_func_data (UHookList *hook_list,
                       uboolean need_valids,
                       void * func,
                       void * data);

void
u_hook_destroy_link (UHookList *hook_list,
                     UHook *hook);

void
u_hook_prepend (UHookList *hook_list,
                UHook *hook);


/*
 * Hashtables
 */
typedef struct _UHashTable UHashTable;
typedef struct _UHashTableIter UHashTableIter;

/* Private, but needed for stack allocation */
struct _UHashTableIter
{
	void * dummy [8];
};

UHashTable     *u_hash_table_new             (UHashFunc hash_func, UEqualFunc key_equal_func);
UHashTable     *u_hash_table_new_full        (UHashFunc hash_func, UEqualFunc key_equal_func,
					      UDestroyNotify key_destroy_func, UDestroyNotify value_destroy_func);
void            u_hash_table_insert_replace  (UHashTable *hash, void * key, void * value, uboolean replace);
unsigned int           u_hash_table_size            (UHashTable *hash);
UList          *u_hash_table_get_keys        (UHashTable *hash);
UList          *u_hash_table_get_values      (UHashTable *hash);
void *        u_hash_table_lookup          (UHashTable *hash, const void * key);
uboolean        u_hash_table_lookup_extended (UHashTable *hash, const void * key, void * *orig_key, void * *value);
void            u_hash_table_foreach         (UHashTable *hash, UHFunc func, void * user_data);
void *        u_hash_table_find            (UHashTable *hash, UHRFunc predicate, void * user_data);
uboolean        u_hash_table_remove          (UHashTable *hash, const void * key);
uboolean        u_hash_table_steal           (UHashTable *hash, const void * key);
void            u_hash_table_remove_all      (UHashTable *hash);
unsigned int           u_hash_table_foreach_remove  (UHashTable *hash, UHRFunc func, void * user_data);
unsigned int           u_hash_table_foreach_steal   (UHashTable *hash, UHRFunc func, void * user_data);
void            u_hash_table_destroy         (UHashTable *hash);
void            u_hash_table_print_stats     (UHashTable *table);

void            u_hash_table_iter_init       (UHashTableIter *iter, UHashTable *hash_table);
uboolean        u_hash_table_iter_next       (UHashTableIter *iter, void * *key, void * *value);

unsigned int           u_spaced_primes_closest      (unsigned int x);

#define u_hash_table_insert(h,k,v)    u_hash_table_insert_replace ((h),(k),(v),FALSE)
#define u_hash_table_replace(h,k,v)   u_hash_table_insert_replace ((h),(k),(v),TRUE)

uboolean u_direct_equal (const void * v1, const void * v2);
unsigned int    u_direct_hash  (const void * v1);
uboolean u_int_equal    (const void * v1, const void * v2);
unsigned int    u_int_hash     (const void * v1);
uboolean u_str_equal    (const void * v1, const void * v2);
unsigned int    u_str_hash     (const void * v1);

/*
 * ByteArray
 */

typedef struct _UByteArray UByteArray;
struct _UByteArray {
	uint8_t *data;
	int len;
};

UByteArray *u_byte_array_new      (void);
UByteArray *u_byte_array_append   (UByteArray *array, const uint8_t *data, unsigned int len);
UByteArray *u_byte_array_set_size (UByteArray *array, unsigned int len);
uint8_t     *u_byte_array_free     (UByteArray *array, uboolean free_segment);

/*
 * Array
 */

typedef struct _UArray UArray;
struct _UArray {
	char *data;
	int len;
};

UArray *u_array_new                   (uboolean zero_terminated, uboolean clear_, unsigned int element_size);
UArray *u_array_sized_new             (uboolean zero_terminated, uboolean clear_, unsigned int element_size, unsigned int reserved_size);
char*  u_array_free                   (UArray *array, uboolean free_segment);
UArray *u_array_append_vals           (UArray *array, const void * data, unsigned int len);
UArray* u_array_insert_vals           (UArray *array, unsigned int index_, const void * data, unsigned int len);
UArray* u_array_remove_index          (UArray *array, unsigned int index_);
UArray* u_array_remove_index_fast     (UArray *array, unsigned int index_);
unsigned int u_array_get_element_size (UArray *array);
void u_array_sort                     (UArray *array, UCompareFunc compare);
UArray *u_array_set_size              (UArray *array, int length);

#define u_array_append_val(a,v)   (u_array_append_vals((a),&(v),1))
#define u_array_insert_val(a,i,v) (u_array_insert_vals((a),(i),&(v),1))
#define u_array_index(a,t,i)      (((t*)(a)->data)[(i)])

/*
 * QSort
*/

void u_qsort_with_data (void * base, size_t nmemb, size_t size, UCompareDataFunc compare, void * user_data);

/*
 * Pointer Array
 */

typedef struct _UPtrArray UPtrArray;
struct _UPtrArray {
	void * *pdata;
	unsigned int len;
};

UPtrArray *u_ptr_array_new                (void);
UPtrArray *u_ptr_array_sized_new          (unsigned int reserved_size);
UPtrArray *u_ptr_array_new_with_free_func (UDestroyNotify element_free_func);
void       u_ptr_array_add                (UPtrArray *array, void * data);
uboolean   u_ptr_array_remove             (UPtrArray *array, void * data);
void *   u_ptr_array_remove_index       (UPtrArray *array, unsigned int index);
uboolean   u_ptr_array_remove_fast        (UPtrArray *array, void * data);
void *   u_ptr_array_remove_index_fast  (UPtrArray *array, unsigned int index);
void       u_ptr_array_sort               (UPtrArray *array, UCompareFunc compare_func);
void       u_ptr_array_sort_with_data     (UPtrArray *array, UCompareDataFunc compare_func, void * user_data);
void       u_ptr_array_set_size           (UPtrArray *array, int length);
void *  *u_ptr_array_free               (UPtrArray *array, uboolean free_seg);
void       u_ptr_array_foreach            (UPtrArray *array, UFunc func, void * user_data);
#define    u_ptr_array_index(array,index) (array)->pdata[(index)]

/*
 * Queues
 */
typedef struct {
	UList *head;
	UList *tail;
	unsigned int length;
} UQueue;

#define  U_QUEUE_INIT { NULL, NULL, 0 }

void     u_queue_init      (UQueue   *queue);
void * u_queue_peek_head (UQueue   *queue);
void * u_queue_pop_head  (UQueue   *queue);
void * u_queue_peek_tail (UQueue   *queue);
void * u_queue_pop_tail  (UQueue   *queue);
void     u_queue_push_head (UQueue   *queue,
			    void *  data);
void     u_queue_push_tail (UQueue   *queue,
			    void *  data);
uboolean u_queue_is_empty  (UQueue   *queue);
UQueue  *u_queue_new       (void);
void     u_queue_free      (UQueue   *queue);
void     u_queue_foreach   (UQueue   *queue, UFunc func, void * user_data);
UList   *u_queue_find      (UQueue   *queue, const void * data);
void     u_queue_clear     (UQueue *queue);

/*
 * Messages
 */
#ifndef U_LOG_DOMAIN
#define U_LOG_DOMAIN ((char*) 0)
#endif

typedef enum {
	U_LOG_FLAU_RECURSION          = 1 << 0,
	U_LOG_FLAU_FATAL              = 1 << 1,

	U_LOG_LEVEL_ERROR             = 1 << 2,
	U_LOG_LEVEL_CRITICAL          = 1 << 3,
	U_LOG_LEVEL_WARNING           = 1 << 4,
	U_LOG_LEVEL_MESSAGE           = 1 << 5,
	U_LOG_LEVEL_INFO              = 1 << 6,
	U_LOG_LEVEL_DEBUG             = 1 << 7,

	U_LOG_LEVEL_MASK              = ~(U_LOG_FLAU_RECURSION | U_LOG_FLAU_FATAL)
} ULogLevelFlags;

void           u_print                (const char *format, ...);
void           u_printerr             (const char *format, ...);
ULogLevelFlags u_log_set_always_fatal (ULogLevelFlags fatal_mask);
ULogLevelFlags u_log_set_fatal_mask   (const char *log_domain, ULogLevelFlags fatal_mask);
void           u_logv                 (const char *log_domain, ULogLevelFlags log_level, const char *format, va_list args);
void           u_log                  (const char *log_domain, ULogLevelFlags log_level, const char *format, ...);
void           u_assertion_message    (const char *format, ...) U_GNUC_NORETURN;

#ifdef HAVE_C99_SUPPORT
/* The for (;;) tells gc thats u_error () doesn't return, avoiding warnings */
#define u_error(format, ...)    do { u_log (U_LOG_DOMAIN, U_LOG_LEVEL_ERROR, format, __VA_ARGS__); for (;;); } while (0)
#define u_critical(format, ...) u_log (U_LOG_DOMAIN, U_LOG_LEVEL_CRITICAL, format, __VA_ARGS__)
#define u_warning(format, ...)  u_log (U_LOG_DOMAIN, U_LOG_LEVEL_WARNING, format, __VA_ARGS__)
#define u_message(format, ...)  u_log (U_LOG_DOMAIN, U_LOG_LEVEL_MESSAGE, format, __VA_ARGS__)
#define u_debug(format, ...)    u_log (U_LOG_DOMAIN, U_LOG_LEVEL_DEBUG, format, __VA_ARGS__)
#else   /* HAVE_C99_SUPPORT */
#define u_error(...)    do { u_log (U_LOG_DOMAIN, U_LOG_LEVEL_ERROR, __VA_ARGS__); for (;;); } while (0)
#define u_critical(...) u_log (U_LOG_DOMAIN, U_LOG_LEVEL_CRITICAL, __VA_ARGS__)
#define u_warning(...)  u_log (U_LOG_DOMAIN, U_LOG_LEVEL_WARNING, __VA_ARGS__)
#define u_message(...)  u_log (U_LOG_DOMAIN, U_LOG_LEVEL_MESSAGE, __VA_ARGS__)
#define u_debug(...)    u_log (U_LOG_DOMAIN, U_LOG_LEVEL_DEBUG, __VA_ARGS__)
#endif  /* ndef HAVE_C99_SUPPORT */
#define u_log_set_handler(a,b,c,d)

#define U_GNUC_INTERNAL

/*
 * Conversions
 */

UQuark u_convert_error_quark(void);


/*
 * Unicode Manipulation: most of this is not used by Mono by default, it is
 * only used if the old collation code is activated, so this is only the
 * bare minimum to build.
 */

typedef enum {
	U_UNICODE_CONTROL,
	U_UNICODE_FORMAT,
	U_UNICODE_UNASSIGNED,
	U_UNICODE_PRIVATE_USE,
	U_UNICODE_SURROGATE,
	U_UNICODE_LOWERCASE_LETTER,
	U_UNICODE_MODIFIER_LETTER,
	U_UNICODE_OTHER_LETTER,
	U_UNICODE_TITLECASE_LETTER,
	U_UNICODE_UPPERCASE_LETTER,
	U_UNICODE_COMBININU_MARK,
	U_UNICODE_ENCLOSINU_MARK,
	U_UNICODE_NON_SPACINU_MARK,
	U_UNICODE_DECIMAL_NUMBER,
	U_UNICODE_LETTER_NUMBER,
	U_UNICODE_OTHER_NUMBER,
	U_UNICODE_CONNECT_PUNCTUATION,
	U_UNICODE_DASH_PUNCTUATION,
	U_UNICODE_CLOSE_PUNCTUATION,
	U_UNICODE_FINAL_PUNCTUATION,
	U_UNICODE_INITIAL_PUNCTUATION,
	U_UNICODE_OTHER_PUNCTUATION,
	U_UNICODE_OPEN_PUNCTUATION,
	U_UNICODE_CURRENCY_SYMBOL,
	U_UNICODE_MODIFIER_SYMBOL,
	U_UNICODE_MATH_SYMBOL,
	U_UNICODE_OTHER_SYMBOL,
	U_UNICODE_LINE_SEPARATOR,
	U_UNICODE_PARAGRAPH_SEPARATOR,
	U_UNICODE_SPACE_SEPARATOR
} UUnicodeType;

typedef enum {
	U_UNICODE_BREAK_MANDATORY,
	U_UNICODE_BREAK_CARRIAGE_RETURN,
	U_UNICODE_BREAK_LINE_FEED,
	U_UNICODE_BREAK_COMBININU_MARK,
	U_UNICODE_BREAK_SURROGATE,
	U_UNICODE_BREAK_ZERO_WIDTH_SPACE,
	U_UNICODE_BREAK_INSEPARABLE,
	U_UNICODE_BREAK_NON_BREAKING_GLUE,
	U_UNICODE_BREAK_CONTINGENT,
	U_UNICODE_BREAK_SPACE,
	U_UNICODE_BREAK_AFTER,
	U_UNICODE_BREAK_BEFORE,
	U_UNICODE_BREAK_BEFORE_AND_AFTER,
	U_UNICODE_BREAK_HYPHEN,
	U_UNICODE_BREAK_NON_STARTER,
	U_UNICODE_BREAK_OPEN_PUNCTUATION,
	U_UNICODE_BREAK_CLOSE_PUNCTUATION,
	U_UNICODE_BREAK_QUOTATION,
	U_UNICODE_BREAK_EXCLAMATION,
	U_UNICODE_BREAK_IDEOGRAPHIC,
	U_UNICODE_BREAK_NUMERIC,
	U_UNICODE_BREAK_INFIX_SEPARATOR,
	U_UNICODE_BREAK_SYMBOL,
	U_UNICODE_BREAK_ALPHABETIC,
	U_UNICODE_BREAK_PREFIX,
	U_UNICODE_BREAK_POSTFIX,
	U_UNICODE_BREAK_COMPLEX_CONTEXT,
	U_UNICODE_BREAK_AMBIGUOUS,
	U_UNICODE_BREAK_UNKNOWN,
	U_UNICODE_BREAK_NEXT_LINE,
	U_UNICODE_BREAK_WORD_JOINER,
	U_UNICODE_BREAK_HANGUL_L_JAMO,
	U_UNICODE_BREAK_HANGUL_V_JAMO,
	U_UNICODE_BREAK_HANGUL_T_JAMO,
	U_UNICODE_BREAK_HANGUL_LV_SYLLABLE,
	U_UNICODE_BREAK_HANGUL_LVT_SYLLABLE
} UUnicodeBreakType;

uunichar       u_unichar_toupper (uunichar c);
uunichar       u_unichar_tolower (uunichar c);
uunichar       u_unichar_totitle (uunichar c);
UUnicodeType   u_unichar_type    (uunichar c);
uboolean       u_unichar_isspace (uunichar c);
uboolean       u_unichar_isxdigit (uunichar c);
int           u_unichar_xdigit_value (uunichar c);
UUnicodeBreakType   u_unichar_break_type (uunichar c);

#define  u_assert(x)     U_STMT_START { if (U_UNLIKELY (!(x))) u_assertion_message ("* Assertion at %s:%d, condition `%s' not met\n", __FILE__, __LINE__, #x);  } U_STMT_END
#define  u_assert_not_reached() U_STMT_START { u_assertion_message ("* Assertion: should not be reached at %s:%d\n", __FILE__, __LINE__); } U_STMT_END

#define u_assert_cmpstr(s1, cmp, s2) \
  U_STMT_START { \
    const char *_s1 = s1; \
    const char *_s2 = s2; \
    if (!(strcmp (_s1, _s2) cmp 0)) \
      u_assertion_message ("* Assertion at %s:%d, condition \"%s\" " #cmp " \"%s\" failed\n", __FILE__, __LINE__, _s1, _s2); \
  } U_STMT_END
#define u_assert_cmpint(n1, cmp, n2) \
  U_STMT_START { \
    int _n1 = n1; \
    int _n2 = n2; \
    if (!(_n1 cmp _n2)) \
      u_assertion_message ("* Assertion at %s:%d, condition %d " #cmp " %d failed\n", __FILE__, __LINE__, _n1, _n2); \
  } U_STMT_END
#define u_assert_cmpuint(n1, cmp, n2) \
  U_STMT_START { \
    int _n1 = n1; \
    int _n2 = n2; \
    if (!(_n1 cmp _n2)) \
      u_assertion_message ("* Assertion at %s:%d, condition %u " #cmp " %u failed\n", __FILE__, __LINE__, _n1, _n2); \
  } U_STMT_END
#define u_assert_cmpfloat(n1, cmp, n2) \
  U_STMT_START { \
    float _n1 = n1; \
    float _n2 = n2; \
    if (!(_n1 cmp _n2)) \
      u_assertion_message ("* Assertion at %s:%d, condition %f " #cmp " %f failed\n", __FILE__, __LINE__, _n1, _n2); \
  } U_STMT_END

/*
 * Unicode conversion
 */

#define U_CONVERT_ERROR u_convert_error_quark()

typedef enum {
	U_CONVERT_ERROR_NO_CONVERSION,
	U_CONVERT_ERROR_ILLEGAL_SEQUENCE,
	U_CONVERT_ERROR_FAILED,
	U_CONVERT_ERROR_PARTIAL_INPUT,
	U_CONVERT_ERROR_BAD_URI,
	U_CONVERT_ERROR_NOT_ABSOLUTE_PATH
} UConvertError;

char     *u_utf8_strup (const char *str, ussize len);
char     *u_utf8_strdown (const char *str, ussize len);
int       u_unichar_to_utf8 (uunichar c, char *outbuf);
uunichar  *u_utf8_to_ucs4_fast (const char *str, long len, long *items_written);
uunichar  *u_utf8_to_ucs4 (const char *str, long len, long *items_read, long *items_written, UError **err);
uunichar2 *u_utf8_to_utf16 (const char *str, long len, long *items_read, long *items_written, UError **err);
uunichar2 *eg_utf8_to_utf16_with_nuls (const char *str, long len, long *items_read, long *items_written, UError **err);
char     *u_utf16_to_utf8 (const uunichar2 *str, long len, long *items_read, long *items_written, UError **err);
uunichar  *u_utf16_to_ucs4 (const uunichar2 *str, long len, long *items_read, long *items_written, UError **err);
char     *u_ucs4_to_utf8  (const uunichar *str, long len, long *items_read, long *items_written, UError **err);
uunichar2 *u_ucs4_to_utf16 (const uunichar *str, long len, long *items_read, long *items_written, UError **err);

#define u8to16(str) u_utf8_to_utf16(str, (long)strlen(str), NULL, NULL, NULL)

#ifdef U_OS_WIN32
#define u16to8(str) u_utf16_to_utf8((uunichar2 *) (str), (long)wcslen((wchar_t *) (str)), NULL, NULL, NULL)
#else
#define u16to8(str) u_utf16_to_utf8(str, (long)strlen(str), NULL, NULL, NULL)
#endif

/*
 * Path
 */
char  *u_build_path           (const char *separator, const char *first_element, ...);
#define u_build_filename(x, ...) u_build_path(U_DIR_SEPARATOR_S, x, __VA_ARGS__)
char  *u_path_get_dirname     (const char *filename);
char  *u_path_get_basename    (const char *filename);
char  *u_find_program_in_path (const char *program);
char  *u_get_current_dir      (void);
uboolean u_path_is_absolute    (const char *filename);

const char *u_get_home_dir    (void);
const char *u_get_tmp_dir     (void);
const char *u_get_user_name   (void);
char *u_get_prgname           (void);
void  u_set_prgname            (const char *prgname);

/*
 * Shell
 */

UQuark u_shell_error_get_quark (void);

#define U_SHELL_ERROR u_shell_error_get_quark ()

typedef enum {
	U_SHELL_ERROR_BAD_QUOTING,
	U_SHELL_ERROR_EMPTY_STRING,
	U_SHELL_ERROR_FAILED
} UShellError;


uboolean  u_shell_parse_argv      (const char *command_line, int *argcp, char ***argvp, UError **error);
char    *u_shell_unquote         (const char *quoted_string, UError **error);
char    *u_shell_quote           (const char *unquoted_string);

/*
 * Spawn
 */
UQuark u_shell_error_get_quark (void);

#define U_SPAWN_ERROR u_shell_error_get_quark ()

typedef enum {
	U_SPAWN_ERROR_FORK,
	U_SPAWN_ERROR_READ,
	U_SPAWN_ERROR_CHDIR,
	U_SPAWN_ERROR_ACCES,
	U_SPAWN_ERROR_PERM,
	U_SPAWN_ERROR_TOO_BIG,
	U_SPAWN_ERROR_NOEXEC,
	U_SPAWN_ERROR_NAMETOOLONG,
	U_SPAWN_ERROR_NOENT,
	U_SPAWN_ERROR_NOMEM,
	U_SPAWN_ERROR_NOTDIR,
	U_SPAWN_ERROR_LOOP,
	U_SPAWN_ERROR_TXTBUSY,
	U_SPAWN_ERROR_IO,
	U_SPAWN_ERROR_NFILE,
	U_SPAWN_ERROR_MFILE,
	U_SPAWN_ERROR_INVAL,
	U_SPAWN_ERROR_ISDIR,
	U_SPAWN_ERROR_LIBBAD,
	U_SPAWN_ERROR_FAILED
} USpawnError;

typedef enum {
	U_SPAWN_LEAVE_DESCRIPTORS_OPEN = 1,
	U_SPAWN_DO_NOT_REAP_CHILD      = 1 << 1,
	U_SPAWN_SEARCH_PATH            = 1 << 2,
	U_SPAWN_STDOUT_TO_DEV_NULL     = 1 << 3,
	U_SPAWN_STDERR_TO_DEV_NULL     = 1 << 4,
	U_SPAWN_CHILD_INHERITS_STDIN   = 1 << 5,
	U_SPAWN_FILE_AND_ARGV_ZERO     = 1 << 6
} USpawnFlags;

typedef void (*USpawnChildSetupFunc) (void * user_data);

uboolean u_spawn_command_line_sync (const char *command_line, char **standard_output, char **standard_error, int *exit_status, UError **error);
uboolean u_spawn_async_with_pipes  (const char *working_directory, char **argv, char **envp, USpawnFlags flags, USpawnChildSetupFunc child_setup,
				void * user_data, UPid *child_pid, int *standard_input, int *standard_output, int *standard_error, UError **error);


/*
 * Timer
 */
typedef struct _UTimer UTimer;

UTimer *u_timer_new (void);
void u_timer_destroy (UTimer *timer);
double u_timer_elapsed (UTimer *timer, unsigned long *microseconds);
void u_timer_stop (UTimer *timer);
void u_timer_start (UTimer *timer);

/*
 * Date and time
 */
typedef struct {
	long tv_sec;
	long tv_usec;
} UTimeVal;

void u_get_current_time (UTimeVal *result);
void u_usleep (unsigned long microseconds);

/*
 * File
 */

UQuark u_file_error_quark (void);

#define U_FILE_ERROR u_file_error_quark ()

typedef enum {
	U_FILE_ERROR_EXIST,
	U_FILE_ERROR_ISDIR,
	U_FILE_ERROR_ACCES,
	U_FILE_ERROR_NAMETOOLONG,
	U_FILE_ERROR_NOENT,
	U_FILE_ERROR_NOTDIR,
	U_FILE_ERROR_NXIO,
	U_FILE_ERROR_NODEV,
	U_FILE_ERROR_ROFS,
	U_FILE_ERROR_TXTBSY,
	U_FILE_ERROR_FAULT,
	U_FILE_ERROR_LOOP,
	U_FILE_ERROR_NOSPC,
	U_FILE_ERROR_NOMEM,
	U_FILE_ERROR_MFILE,
	U_FILE_ERROR_NFILE,
	U_FILE_ERROR_BADF,
	U_FILE_ERROR_INVAL,
	U_FILE_ERROR_PIPE,
	U_FILE_ERROR_AGAIN,
	U_FILE_ERROR_INTR,
	U_FILE_ERROR_IO,
	U_FILE_ERROR_PERM,
	U_FILE_ERROR_NOSYS,
	U_FILE_ERROR_FAILED
} UFileError;

typedef enum {
	U_FILE_TEST_IS_REGULAR = 1 << 0,
	U_FILE_TEST_IS_SYMLINK = 1 << 1,
	U_FILE_TEST_IS_DIR = 1 << 2,
	U_FILE_TEST_IS_EXECUTABLE = 1 << 3,
	U_FILE_TEST_EXISTS = 1 << 4
} UFileTest;


uboolean   u_file_set_contents (const char *filename, const char *contents, ussize length, UError **error);
uboolean   u_file_get_contents (const char *filename, char **contents, size_t *length, UError **error);
UFileError u_file_error_from_errno (int err_no);
int       u_file_open_tmp (const char *tmpl, char **name_used, UError **error);
uboolean   u_file_test (const char *filename, UFileTest test);

#define u_open open
#define u_rename rename
#define u_stat stat
#define u_unlink unlink
#define u_fopen fopen
#define u_lstat lstat
#define u_rmdir rmdir
#define u_mkstemp mkstemp
#define u_ascii_isdigit isdigit
#define u_ascii_strtod strtod
#define u_ascii_isalnum isalnum

/*
 * Pattern matching
 */
typedef struct _UPatternSpec UPatternSpec;
UPatternSpec * u_pattern_spec_new (const char *pattern);
void           u_pattern_spec_free (UPatternSpec *pspec);
uboolean       u_pattern_match_string (UPatternSpec *pspec, const char *string);

/*
 * Directory
 */
typedef struct _UDir UDir;
UDir        *u_dir_open (const char *path, unsigned int flags, UError **error);
const char *u_dir_read_name (UDir *dir);
void         u_dir_rewind (UDir *dir);
void         u_dir_close (UDir *dir);

int          u_mkdir_with_parents (const char *pathname, int mode);
#define u_mkdir mkdir

/*
 * UMarkup
 */

UQuark u_markup_error_quark (void);

#define U_MARKUP_ERROR u_markup_error_quark()

typedef enum {
	U_MARKUP_ERROR_BAD_UTF8,
	U_MARKUP_ERROR_EMPTY,
	U_MARKUP_ERROR_PARSE,
	U_MARKUP_ERROR_UNKNOWN_ELEMENT,
	U_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
	U_MARKUP_ERROR_INVALID_CONTENT,
	U_MARKUP_ERROR_MISSINU_ATTRIBUTE
} UMarkupError;

typedef struct _UMarkupParseContext UMarkupParseContext;

typedef enum
{
	U_MARKUP_DO_NOT_USE_THIS_UNSUPPORTED_FLAG = 1 << 0,
	U_MARKUP_TREAT_CDATA_AS_TEXT              = 1 << 1
} UMarkupParseFlags;

typedef struct {
	void (*start_element)  (UMarkupParseContext *context,
				const char *element_name,
				const char **attribute_names,
				const char **attribute_values,
				void * user_data,
				UError **error);

	void (*end_element)    (UMarkupParseContext *context,
				const char         *element_name,
				void *             user_data,
				UError             **error);

	void (*text)           (UMarkupParseContext *context,
				const char         *text,
				size_t                text_len,
				void *             user_data,
				UError             **error);

	void (*passthrough)    (UMarkupParseContext *context,
				const char         *passthrough_text,
				size_t                text_len,
				void *             user_data,
				UError             **error);
	void (*error)          (UMarkupParseContext *context,
				UError              *error,
				void *             user_data);
} UMarkupParser;

UMarkupParseContext *u_markup_parse_context_new   (const UMarkupParser *parser,
						   UMarkupParseFlags flags,
						   void * user_data,
						   UDestroyNotify user_data_dnotify);
void                 u_markup_parse_context_free  (UMarkupParseContext *context);
uboolean             u_markup_parse_context_parse (UMarkupParseContext *context,
						   const char *text, ussize text_len,
						   UError **error);
uboolean         u_markup_parse_context_end_parse (UMarkupParseContext *context,
						   UError **error);

/*
 * Character set conversion
 */
typedef struct _UIConv *UIConv;

size_t u_iconv (UIConv cd, char **inbytes, size_t *inbytesleft, char **outbytes, size_t *outbytesleft);
UIConv u_iconv_open (const char *to_charset, const char *from_charset);
int u_iconv_close (UIConv cd);

uboolean  u_get_charset        (U_CONST_RETURN char **charset);
char    *u_locale_to_utf8     (const char *opsysstring, ussize len,
				size_t *bytes_read, size_t *bytes_written,
				UError **error);
char    *u_locale_from_utf8   (const char *utf8string, ussize len, size_t *bytes_read,
				size_t *bytes_written, UError **error);
char    *u_filename_from_utf8 (const char *utf8string, ussize len, size_t *bytes_read,
				size_t *bytes_written, UError **error);
char    *u_convert            (const char *str, ussize len,
				const char *to_codeset, const char *from_codeset,
				size_t *bytes_read, size_t *bytes_written, UError **error);

/*
 * Unicode manipulation
 */
extern const unsigned char u_utf8_jump_table[256];

uboolean  u_utf8_validate      (const char *str, ussize max_len, const char **end);
uunichar  u_utf8_get_char_validated (const char *str, ussize max_len);
char    *u_utf8_find_prev_char (const char *str, const char *p);
char    *u_utf8_prev_char     (const char *str);
#define   u_utf8_next_char(p)  ((p) + u_utf8_jump_table[(unsigned char)(*p)])
uunichar  u_utf8_get_char      (const char *src);
long     u_utf8_strlen        (const char *str, ussize max);
char    *u_utf8_offset_to_pointer (const char *str, long offset);
long     u_utf8_pointer_to_offset (const char *str, const char *pos);

/*
 * priorities
 */
#define U_PRIORITY_DEFAULT 0
#define U_PRIORITY_DEFAULT_IDLE 200

/*
 * Empty thread functions, not used by eglib
 */
#define u_thread_supported()   TRUE
#define u_thread_init(x)       U_STMT_START { if (x != NULL) { u_error ("No vtable supported in u_thread_init"); } } U_STMT_END

#define U_LOCK_DEFINE(name)        int name;
#define U_LOCK_DEFINE_STATIC(name) static int name;
#define U_LOCK_EXTERN(name)
#define U_LOCK(name)
#define U_TRYLOCK(name)
#define U_UNLOCK(name)

#define UUINT16_SWAP_LE_BE_CONSTANT(x) ((((uint16_t) x) >> 8) | ((((uint16_t) x) << 8)))

#define UUINT16_SWAP_LE_BE(x) ((uint16_t) (((uint16_t) x) >> 8) | ((((uint16_t)(x)) & 0xff) << 8))
#define UUINT32_SWAP_LE_BE(x) ((uint32_t) \
			       ( (((uint32_t) (x)) << 24)| \
				 ((((uint32_t) (x)) & 0xff0000) >> 8) | \
		                 ((((uint32_t) (x)) & 0xff00) << 8) | \
			         (((uint32_t) (x)) >> 24)) )

#define UUINT64_SWAP_LE_BE(x) ((uint64_t) (((uint64_t)(UUINT32_SWAP_LE_BE(((uint64_t)x) & 0xffffffff))) << 32) | \
	      	               UUINT32_SWAP_LE_BE(((uint64_t)x) >> 32))



#if U_BYTE_ORDER == U_LITTLE_ENDIAN
#   define UUINT64_FROM_BE(x) UUINT64_SWAP_LE_BE(x)
#   define UUINT32_FROM_BE(x) UUINT32_SWAP_LE_BE(x)
#   define UUINT16_FROM_BE(x) UUINT16_SWAP_LE_BE(x)
#   define UUINT_FROM_BE(x)   UUINT32_SWAP_LE_BE(x)
#   define UUINT64_FROM_LE(x) (x)
#   define UUINT32_FROM_LE(x) (x)
#   define UUINT16_FROM_LE(x) (x)
#   define UUINT_FROM_LE(x)   (x)
#   define UUINT64_TO_BE(x)   UUINT64_SWAP_LE_BE(x)
#   define UUINT32_TO_BE(x)   UUINT32_SWAP_LE_BE(x)
#   define UUINT16_TO_BE(x)   UUINT16_SWAP_LE_BE(x)
#   define UUINT_TO_BE(x)     UUINT32_SWAP_LE_BE(x)
#   define UUINT64_TO_LE(x)   (x)
#   define UUINT32_TO_LE(x)   (x)
#   define UUINT16_TO_LE(x)   (x)
#   define UUINT_TO_LE(x)     (x)
#else
#   define UUINT64_FROM_BE(x) (x)
#   define UUINT32_FROM_BE(x) (x)
#   define UUINT16_FROM_BE(x) (x)
#   define UUINT_FROM_BE(x)   (x)
#   define UUINT64_FROM_LE(x) UUINT64_SWAP_LE_BE(x)
#   define UUINT32_FROM_LE(x) UUINT32_SWAP_LE_BE(x)
#   define UUINT16_FROM_LE(x) UUINT16_SWAP_LE_BE(x)
#   define UUINT_FROM_LE(x)   UUINT32_SWAP_LE_BE(x)
#   define UUINT64_TO_BE(x)   (x)
#   define UUINT32_TO_BE(x)   (x)
#   define UUINT16_TO_BE(x)   (x)
#   define UUINT_TO_BE(x)     (x)
#   define UUINT64_TO_LE(x)   UUINT64_SWAP_LE_BE(x)
#   define UUINT32_TO_LE(x)   UUINT32_SWAP_LE_BE(x)
#   define UUINT16_TO_LE(x)   UUINT16_SWAP_LE_BE(x)
#   define UUINT_TO_LE(x)     UUINT32_SWAP_LE_BE(x)
#endif

#define UINT64_FROM_BE(x)   (UUINT64_TO_BE (x))
#define UINT32_FROM_BE(x)   (UUINT32_TO_BE (x))
#define UINT16_FROM_BE(x)   (UUINT16_TO_BE (x))
#define UINT64_FROM_LE(x)   (UUINT64_TO_LE (x))
#define UINT32_FROM_LE(x)   (UUINT32_TO_LE (x))
#define UINT16_FROM_LE(x)   (UUINT16_TO_LE (x))

#define _ULIB_MAJOR  2
#define _ULIB_MIDDLE 4
#define _ULIB_MINOR  0

#define ULIB_CHECK_VERSION(a,b,c) ((a < _ULIB_MAJOR) || (a == _ULIB_MAJOR && (b < _ULIB_MIDDLE || (b == _ULIB_MIDDLE && c <= _ULIB_MINOR))))

U_END_DECLS

#endif



