#ifndef __CLIB_H
#define __CLIB_H

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
#pragma include_alias(<clib-config.h>, <clib-config.hw>)
#endif

#include <stdint.h>
#include <inttypes.h>

#include <clib-config.h>

#ifdef C_HAVE_ALLOCA_H
#include <alloca.h>
#endif

#ifdef WIN32
/* For alloca */
#include <malloc.h>
#endif

#ifndef offsetof
#   define offsetof(s_name,n_name) (size_t)(char *)&(((s_name*)0)->m_name)
#endif

#define __CLIB_X11 1

#ifdef  __cplusplus
#define C_BEGIN_DECLS  extern "C" {
#define C_END_DECLS    }
#else
#define C_BEGIN_DECLS
#define C_END_DECLS
#endif

C_BEGIN_DECLS

#ifdef C_OS_WIN32
/* MSC and Cross-compilatin will use this */
int vasprintf (char **strp, const char *fmt, va_list ap);
#endif


/*
 * Basic data types
 */
typedef ssize_t  ussize;
typedef int32_t  cboolean;
typedef uint16_t cunichar2;
typedef uint32_t cunichar;


/*
 * Macros
 */
#define C_N_ELEMENTS(s)                   (sizeof(s) / sizeof ((s) [0]))
#define C_STRINGIFY_ARG(arg)              #arg
#define C_STRINGIFY(arg)                  C_STRINGIFY_ARG(arg)
#define C_PASTE_ARGS(part0, part1)        part0 ## part1
#define C_PASTE(part0, part1)             C_PASTE_ARGS (part0, part1)

#ifndef FALSE
#define FALSE                0
#endif

#ifndef TRUE
#define TRUE                 1
#endif

#define C_MINSHORT           SHRT_MIN
#define C_MAXSHORT           SHRT_MAX
#define C_MAXUSHORT          USHRT_MAX
#define C_MAXINT             INT_MAX
#define C_MININT             INT_MIN
#define C_MAXINT32           INT32_MAX
#define C_MAXUINT32          UINT32_MAX
#define C_MININT32           INT32_MIN
#define C_MININT64           INT64_MIN
#define C_MAXINT64	     INT64_MAX
#define C_MAXUINT64	     UINT64_MAX
#define C_MAXFLOAT           FLT_MAX

#define C_UINT64_FORMAT     PRIu64
#define C_INT64_FORMAT      PRId64
#define C_UINT32_FORMAT     PRIu32
#define C_INT32_FORMAT      PRId32

#define C_LITTLE_ENDIAN 1234
#define C_BIC_ENDIAN    4321
#define C_STMT_START    do
#define C_STMT_END      while (0)

#define C_USEC_PER_SEC  1000000
#define C_PI            3.141592653589793238462643383279502884L
#define C_PI_2          1.570796326794896619231321691639751442L

#define C_INT64_CONSTANT(val)   (val##LL)
#define C_UINT64_CONSTANT(val)  (val##UL)

#define C_POINTER_TO_INT(ptr)   ((int)(intptr_t)(ptr))
#define C_POINTER_TO_UINT(ptr)  ((unsigned int)(uintptr_t)(ptr))
#define C_INT_TO_POINTER(v)     ((void *)(intptr_t)(v))
#define C_UINT_TO_POINTER(v)    ((void *)(uintptr_t)(v))

#define C_STRUCT_OFFSET(p_type,field) offsetof(p_type,field)

#define C_STRLOC __FILE__ ":" C_STRINGIFY(__LINE__) ":"

#define C_CONST_RETURN const

#define C_BYTE_ORDER C_LITTLE_ENDIAN

#if defined(__GNUC__)
#  define C_GNUC_GNUSED                       __attribute__((__unused__))
#  define C_GNUC_NORETURN                     __attribute__((__noreturn__))
#  define C_LIKELY(expr)                      (__builtin_expect ((expr) != 0, 1))
#  define C_UNLIKELY(expr)                    (__builtin_expect ((expr) != 0, 0))
#  define C_GNUC_PRINTF(format_idx, arg_idx)  __attribute__((__format__ (__printf__, format_idx, arg_idx)))
#else
#  define C_GNUC_GNUSED
#  define C_GNUC_NORETURN
#  define C_LIKELY(expr) (expr)
#  define C_UNLIKELY(expr) (expr)
#  define C_GNUC_PRINTF(format_idx, arg_idx)
#endif

#if defined(__GNUC__)
#  define C_STRFUNC ((const char *)(__PRETTY_FUNCTION__))
#elif defined (_MSC_VER)
#  define C_STRFUNC ((const char*) (__FUNCTION__))
#else
#  define C_STRFUNC ((const char*) (__func__))
#endif

#if defined (_MSC_VER)
#  define C_VA_COPY(dest, src) ((dest) = (src))
#else
#  define C_VA_COPY(dest, src) va_copy (dest, src)
#endif

#ifdef OS_UNIX
#define C_BREAKPOINT() C_STMT_START { raise (SIGTRAP); } C_STMT_END
#else
#define C_BREAKPOINT()
#endif

#if defined (__native_client__)
#undef C_BREAKPOINT
#define C_BREAKPOINT()
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
void c_free (void *ptr);
void * c_realloc (void * obj, size_t size);
void * c_malloc (size_t x);
void * c_malloc0 (size_t x);
void * c_try_malloc (size_t x);
void * c_try_realloc (void * obj, size_t size);
void * c_memdup (const void * mem, unsigned int byte_size);

#define c_new(type,size)        ((type *) c_malloc (sizeof (type) * (size)))
#define c_new0(type,size)       ((type *) c_malloc0 (sizeof (type)* (size)))
#define c_newa(type,size)       ((type *) alloca (sizeof (type) * (size)))

#define c_memmove(dest,src,len) memmove (dest, src, len)
#define c_renew(struct_type, mem, n_structs) c_realloc (mem, sizeof (struct_type) * n_structs)
#define c_alloca(size)		alloca (size)

#define c_slice_new(type)         ((type *) c_malloc (sizeof (type)))
#define c_slice_new0(type)        ((type *) c_malloc0 (sizeof (type)))
#define c_slice_free(type, mem)   c_free (mem)
#define c_slice_free1(size, mem)  c_free (mem)
#define c_slice_alloc(size)       c_malloc (size)
#define c_slice_alloc0(size)      c_malloc0 (size)
#define c_slice_dup(type, mem)    c_memdup (mem, sizeof (type))
#define c_slice_copy(size, mem)   c_memdup (mem, size)

static inline char   *c_strdup (const char *str) { if (str) {return strdup (str);} return NULL; }
char **c_strdupv (char **str_array);
int c_strcmp0 (const char *str1, const char *str2);

typedef struct {
	void * (*malloc)      (size_t    n_bytes);
	void * (*realloc)     (void * mem, size_t n_bytes);
	void     (*free)        (void * mem);
	void * (*calloc)      (size_t    n_blocks, size_t n_block_bytes);
	void * (*try_malloc)  (size_t    n_bytes);
	void * (*try_realloc) (void * mem, size_t n_bytes);
} UMemVTable;

#define c_mem_set_vtable(x)

struct _UMemChunk {
	unsigned int alloc_size;
};

typedef struct _UMemChunk UMemChunk;
/*
 * Misc.
 */
#define c_atexit(func)	((void) atexit (func))

const char *    c_getenv(const char *variable);
cboolean         c_setenv(const char *variable, const char *value, cboolean overwrite);
void             c_unsetenv(const char *variable);

char*           c_win32_getlocale(void);

/*
 * Precondition macros
 */
#define c_warn_if_fail(x)  C_STMT_START { if (!(x)) { c_warning ("%s:%d: assertion '%s' failed", __FILE__, __LINE__, #x); } } C_STMT_END
#define c_warn_if_reached() c_warning ("%s:%d: code should not be reached!", __FILE__, __LINE__)

#define c_return_if_fail(x)  C_STMT_START { if (!(x)) { c_critical ("%s:%d: assertion '%s' failed", __FILE__, __LINE__, #x); return; } } C_STMT_END
#define c_return_val_if_fail(x,e)  C_STMT_START { if (!(x)) { c_critical ("%s:%d: assertion '%s' failed", __FILE__, __LINE__, #x); return (e); } } C_STMT_END
#define c_return_if_reached(e) C_STMT_START { c_warning ("%s:%d: code should not be reached, returning!",  __FILE__, __LINE__); return; } C_STMT_END
#define c_return_val_if_reached(e) C_STMT_START { c_warning ("%s:%d: code should not be reached, returning!",  __FILE__, __LINE__); return (e); } C_STMT_END

/*
 * DebugKeys
 */
typedef struct _CDebugKey {
	const char *key;
	unsigned int	     value;
} CDebugKey;

unsigned int   c_parse_debug_string (const char *string, const CDebugKey *keys, unsigned int nkeys);

/*
 * Quark
 */

typedef uint32_t UQuark;

UQuark
c_quark_from_static_string (const char *string);

/*
 * Errors
 */
typedef struct {
	UQuark  domain;
	int    code;
	char  *message;
} UError;

void      c_clear_error (UError **error);
void      c_error_free (UError *error);
UError   *c_error_new  (UQuark domain, int code, const char *format, ...);
UError   *c_error_new_valist (UQuark domain, int code, const char *format, va_list ap);
void      c_set_error  (UError **err, UQuark domain, int code, const char *format, ...);
void      c_propagate_error (UError **dest, UError *src);
UError   *c_error_copy (const UError *error);
cboolean  c_error_matches (const UError *error, UQuark domain, int code);

/*
 * Strings utility
 */
char       *c_strdup_printf  (const char *format, ...);
char       *c_strdup_vprintf (const char *format, va_list args);
char       *c_strndup        (const char *str, size_t n);
const char *c_strerror       (int errnum);
char       *c_strndup        (const char *str, size_t n);
void         c_strfreev       (char **str_array);
char       *c_strconcat      (const char *first, ...);
char      **c_strsplit       (const char *string, const char *delimiter, int max_tokens);
char      **c_strsplit_set   (const char *string, const char *delimiter, int max_tokens);
char       *c_strreverse     (char *str);
cboolean     c_str_has_prefix (const char *str, const char *prefix);
cboolean     c_str_has_suffix (const char *str, const char *suffix);
unsigned int        c_strv_length    (char **str_array);
char       *c_strjoin        (const char *separator, ...);
char       *c_strjoinv       (const char *separator, char **str_array);
char       *c_strchug        (char *str);
char       *c_strchomp       (char *str);
void         c_strdown        (char *string);
char       *c_strnfill       (size_t length, char fill_char);

char       *c_strdelimit     (char *string, const char *delimiters, char new_delimiter);
char       *c_strescape      (const char *source, const char *exceptions);

char       *c_filename_to_uri   (const char *filename, const char *hostname, UError **error);
char       *c_filename_from_uri (const char *uri, char **hostname, UError **error);

int         c_printf          (char const *format, ...);
int         c_fprintf         (FILE *file, char const *format, ...);
int         c_sprintf         (char *string, char const *format, ...);
int         c_snprintf        (char *string, unsigned long n, char const *format, ...);
#define c_vprintf vprintf
#define c_vfprintf vfprintf
#define c_vsprintf vsprintf
#define c_vsnprintf vsnprintf
#define c_vasprintf vasprintf

size_t   c_strlcpy            (char *dest, const char *src, size_t dest_size);
char  *c_stpcpy             (char *dest, const char *src);


char   c_ascii_tolower      (char c);
char   c_ascii_toupper      (char c);
char  *c_ascii_strdown      (const char *str, ussize len);
char  *c_ascii_strup        (const char *str, ussize len);
int    c_ascii_strncasecmp  (const char *s1, const char *s2, size_t n);
int    c_ascii_strcasecmp   (const char *s1, const char *s2);
int    c_ascii_xdigit_value (char c);
#define c_ascii_isspace(c)   (isspace (c) != 0)
#define c_ascii_isalpha(c)   (isalpha (c) != 0)
#define c_ascii_isprint(c)   (isprint (c) != 0)
#define c_ascii_isxdigit(c)  (isxdigit (c) != 0)

/* FIXME: c_strcasecmp supports utf8 unicode stuff */
#ifdef _MSC_VER
#define c_strcasecmp stricmp
#define c_strncasecmp strnicmp
#define c_strstrip(a) c_strchug (c_strchomp (a))
#else
#define c_strcasecmp strcasecmp
#define c_ascii_strtoull strtoull
#define c_strncasecmp strncasecmp
#define c_strstrip(a) c_strchug (c_strchomp (a))
#endif
#define c_ascii_strdup strdup


#define	C_STR_DELIMITERS "_-|> <."

/*
 * String type
 */
typedef struct {
	char *str;
	size_t len;
	size_t allocated_len;
} CString;

CString     *c_string_new           (const char *init);
CString     *c_string_new_len       (const char *init, ussize len);
CString     *c_string_sized_new     (size_t default_size);
char       *c_string_free          (CString *string, cboolean free_segment);
CString     *c_string_assign        (CString *string, const char *val);
CString     *c_string_append        (CString *string, const char *val);
void         c_string_printf        (CString *string, const char *format, ...);
void         c_string_append_printf (CString *string, const char *format, ...);
void         c_string_append_vprintf (CString *string, const char *format, va_list args);
CString     *c_string_append_unichar (CString *string, cunichar c);
CString     *c_string_append_c      (CString *string, char c);
CString     *c_string_append        (CString *string, const char *val);
CString     *c_string_append_len    (CString *string, const char *val, ussize len);
CString     *c_string_truncate      (CString *string, size_t len);
CString     *c_string_prepend       (CString *string, const char *val);
CString     *c_string_insert        (CString *string, ussize pos, const char *val);
CString     *c_string_set_size      (CString *string, size_t len);
CString     *c_string_erase         (CString *string, ussize pos, ussize len);

#define c_string_sprintfa c_string_append_printf

typedef void     (*CFunc)          (void * data, void * user_data);
typedef int     (*CCompareFunc)   (const void * a, const void * b);
typedef int     (*CCompareDataFunc) (const void * a, const void * b, void * user_data);
typedef void     (*CHFunc)         (void * key, void * value, void * user_data);
typedef cboolean (*CHRFunc)        (void * key, void * value, void * user_data);
typedef void     (*CDestroyNotify) (void * data);
typedef unsigned int    (*CHashFunc)      (const void * key);
typedef cboolean (*CEqualFunc)     (const void * a, const void * b);
typedef void     (*CFreeFunc)      (void *       data);

/*
 * Lists
 */
typedef struct _CSList CSList;
struct _CSList {
	void * data;
	CSList *next;
};

CSList *c_slist_alloc         (void);
CSList *c_slist_append        (CSList        *list,
			       void *       data);
CSList *c_slist_prepend       (CSList        *list,
			       void *       data);
void    c_slist_free          (CSList        *list);
void    c_slist_free_1        (CSList        *list);
CSList *c_slist_copy          (CSList        *list);
CSList *c_slist_concat        (CSList        *list1,
			       CSList        *list2);
void    c_slist_foreach       (CSList        *list,
			       CFunc          func,
			       void *       user_data);
CSList *c_slist_last          (CSList        *list);
CSList *c_slist_find          (CSList        *list,
			       const void *  data);
CSList *c_slist_find_custom   (CSList	     *list,
			       const void *  data,
			       CCompareFunc   func);
CSList *c_slist_remove        (CSList        *list,
			       const void *  data);
CSList *c_slist_remove_all    (CSList        *list,
			       const void *  data);
CSList *c_slist_reverse       (CSList        *list);
unsigned int   c_slist_length        (CSList        *list);
CSList *c_slist_remove_link   (CSList        *list,
			       CSList        *link);
CSList *c_slist_delete_link   (CSList        *list,
			       CSList        *link);
CSList *c_slist_insert_sorted (CSList        *list,
			       void *       data,
			       CCompareFunc   func);
CSList *c_slist_insert_before (CSList        *list,
			       CSList        *sibling,
			       void *       data);
CSList *c_slist_sort          (CSList        *list,
			       CCompareFunc   func);
int    c_slist_index	      (CSList        *list,
			       const void *  data);
CSList *c_slist_nth	      (CSList	     *list,
			       unsigned int	      n);
void * c_slist_nth_data     (CSList	     *list,
			       unsigned int	      n);

#define c_slist_next(slist) ((slist) ? (((CSList *) (slist))->next) : NULL)


typedef struct _CList CList;
struct _CList {
  void * data;
  CList *next;
  CList *prev;
};

#define c_list_next(list) ((list) ? (((CList *) (list))->next) : NULL)
#define c_list_previous(list) ((list) ? (((CList *) (list))->prev) : NULL)

CList *c_list_alloc         (void);
CList *c_list_append        (CList         *list,
			     void *       data);
CList *c_list_prepend       (CList         *list,
			     void *       data);
void   c_list_free          (CList         *list);
void   c_list_free_full     (CList         *list,
                             CDestroyNotify free_func);
void   c_list_free_1        (CList         *list);
CList *c_list_copy          (CList         *list);
unsigned int  c_list_length        (CList         *list);
int   c_list_index         (CList         *list,
			     const void *  data);
CList *c_list_nth           (CList         *list,
			     unsigned int          n);
void * c_list_nth_data      (CList         *list,
			     unsigned int          n);
CList *c_list_last          (CList         *list);
CList *c_list_concat        (CList         *list1,
			     CList         *list2);
void   c_list_foreach       (CList         *list,
			     CFunc          func,
			     void *       user_data);
CList *c_list_first         (CList         *list);
CList *c_list_find          (CList         *list,
			     const void *  data);
CList *c_list_find_custom   (CList	   *list,
			     const void *  data,
			     CCompareFunc   func);
CList *c_list_remove        (CList         *list,
			     const void *  data);
CList *c_list_remove_all    (CList         *list,
			     const void *  data);
CList *c_list_reverse       (CList         *list);
CList *c_list_remove_link   (CList         *list,
			     CList         *link);
CList *c_list_delete_link   (CList         *list,
			     CList         *link);
CList *c_list_insert_sorted (CList         *list,
			     void *       data,
			     CCompareFunc   func);
CList *c_list_insert_before (CList         *list,
			     CList         *sibling,
			     void *       data);
CList *c_list_sort          (CList         *sort,
			     CCompareFunc   func);

/*
 * HookLists
 */
typedef void (*CHookFunc) (void * data);

typedef struct _CHook CHook;
typedef struct _CHookList CHookList;

struct _CHook {
  CHook	*next;
  CHook	*prev;
  void * data;
  void * func;
  cboolean in_call;
};

struct _CHookList {
  CHook *hooks;
};

void
c_hook_list_init (CHookList *hook_list,
                  unsigned int hook_size);

void
c_hook_list_invoke (CHookList *hook_list,
                    cboolean may_recurse);

void
c_hook_list_clear (CHookList *hook_list);

CHook *
c_hook_alloc (CHookList *hook_list);

CHook *
c_hook_find_func_data (CHookList *hook_list,
                       cboolean need_valids,
                       void * func,
                       void * data);

void
c_hook_destroy_link (CHookList *hook_list,
                     CHook *hook);

void
c_hook_prepend (CHookList *hook_list,
                CHook *hook);


/*
 * Hashtables
 */
typedef struct _CHashTable CHashTable;
typedef struct _CHashTableIter CHashTableIter;

/* Private, but needed for stack allocation */
struct _CHashTableIter
{
	void * dummy [8];
};

CHashTable     *c_hash_table_new             (CHashFunc hash_func, CEqualFunc key_equal_func);
CHashTable     *c_hash_table_new_full        (CHashFunc hash_func, CEqualFunc key_equal_func,
					      CDestroyNotify key_destroy_func, CDestroyNotify value_destroy_func);
void            c_hash_table_insert_replace  (CHashTable *hash, void * key, void * value, cboolean replace);
unsigned int           c_hash_table_size            (CHashTable *hash);
CList          *c_hash_table_get_keys        (CHashTable *hash);
CList          *c_hash_table_get_values      (CHashTable *hash);
void *        c_hash_table_lookup          (CHashTable *hash, const void * key);
cboolean        c_hash_table_lookup_extended (CHashTable *hash, const void * key, void * *orig_key, void * *value);
void            c_hash_table_foreach         (CHashTable *hash, CHFunc func, void * user_data);
void *        c_hash_table_find            (CHashTable *hash, CHRFunc predicate, void * user_data);
cboolean        c_hash_table_remove          (CHashTable *hash, const void * key);
cboolean        c_hash_table_steal           (CHashTable *hash, const void * key);
void            c_hash_table_remove_all      (CHashTable *hash);
unsigned int           c_hash_table_foreach_remove  (CHashTable *hash, CHRFunc func, void * user_data);
unsigned int           c_hash_table_foreach_steal   (CHashTable *hash, CHRFunc func, void * user_data);
void            c_hash_table_destroy         (CHashTable *hash);
void            c_hash_table_print_stats     (CHashTable *table);

void            c_hash_table_iter_init       (CHashTableIter *iter, CHashTable *hash_table);
cboolean        c_hash_table_iter_next       (CHashTableIter *iter, void * *key, void * *value);

unsigned int           c_spaced_primes_closest      (unsigned int x);

#define c_hash_table_insert(h,k,v)    c_hash_table_insert_replace ((h),(k),(v),FALSE)
#define c_hash_table_replace(h,k,v)   c_hash_table_insert_replace ((h),(k),(v),TRUE)

cboolean c_direct_equal (const void * v1, const void * v2);
unsigned int    c_direct_hash  (const void * v1);
cboolean c_int_equal    (const void * v1, const void * v2);
unsigned int    c_int_hash     (const void * v1);
cboolean c_str_equal    (const void * v1, const void * v2);
unsigned int    c_str_hash     (const void * v1);

/*
 * ByteArray
 */

typedef struct _CByteArray CByteArray;
struct _CByteArray {
	uint8_t *data;
	int len;
};

CByteArray *c_byte_array_new      (void);
CByteArray *c_byte_array_append   (CByteArray *array, const uint8_t *data, unsigned int len);
CByteArray *c_byte_array_set_size (CByteArray *array, unsigned int len);
uint8_t     *c_byte_array_free     (CByteArray *array, cboolean free_segment);

/*
 * Array
 */

typedef struct _CArray CArray;
struct _CArray {
	char *data;
	int len;
};

CArray *c_array_new                   (cboolean zero_terminated, cboolean clear_, unsigned int element_size);
CArray *c_array_sized_new             (cboolean zero_terminated, cboolean clear_, unsigned int element_size, unsigned int reserved_size);
char*  c_array_free                   (CArray *array, cboolean free_segment);
CArray *c_array_append_vals           (CArray *array, const void * data, unsigned int len);
CArray* c_array_insert_vals           (CArray *array, unsigned int index_, const void * data, unsigned int len);
CArray* c_array_remove_index          (CArray *array, unsigned int index_);
CArray* c_array_remove_index_fast     (CArray *array, unsigned int index_);
unsigned int c_array_get_element_size (CArray *array);
void c_array_sort                     (CArray *array, CCompareFunc compare);
CArray *c_array_set_size              (CArray *array, int length);

#define c_array_append_val(a,v)   (c_array_append_vals((a),&(v),1))
#define c_array_insert_val(a,i,v) (c_array_insert_vals((a),(i),&(v),1))
#define c_array_index(a,t,i)      (((t*)(a)->data)[(i)])

/*
 * QSort
*/

void c_qsort_with_data (void * base, size_t nmemb, size_t size, CCompareDataFunc compare, void * user_data);

/*
 * Pointer Array
 */

typedef struct _UPtrArray UPtrArray;
struct _UPtrArray {
	void * *pdata;
	unsigned int len;
};

UPtrArray *c_ptr_array_new                (void);
UPtrArray *c_ptr_array_sized_new          (unsigned int reserved_size);
UPtrArray *c_ptr_array_new_with_free_func (CDestroyNotify element_free_func);
void       c_ptr_array_add                (UPtrArray *array, void * data);
cboolean   c_ptr_array_remove             (UPtrArray *array, void * data);
void *   c_ptr_array_remove_index       (UPtrArray *array, unsigned int index);
cboolean   c_ptr_array_remove_fast        (UPtrArray *array, void * data);
void *   c_ptr_array_remove_index_fast  (UPtrArray *array, unsigned int index);
void       c_ptr_array_sort               (UPtrArray *array, CCompareFunc compare_func);
void       c_ptr_array_sort_with_data     (UPtrArray *array, CCompareDataFunc compare_func, void * user_data);
void       c_ptr_array_set_size           (UPtrArray *array, int length);
void *  *c_ptr_array_free               (UPtrArray *array, cboolean free_seg);
void       c_ptr_array_foreach            (UPtrArray *array, CFunc func, void * user_data);
#define    c_ptr_array_index(array,index) (array)->pdata[(index)]

/*
 * Queues
 */
typedef struct {
	CList *head;
	CList *tail;
	unsigned int length;
} CQueue;

#define  C_QUEUE_INIT { NULL, NULL, 0 }

void     c_queue_init      (CQueue   *queue);
void * c_queue_peek_head (CQueue   *queue);
void * c_queue_pop_head  (CQueue   *queue);
void * c_queue_peek_tail (CQueue   *queue);
void * c_queue_pop_tail  (CQueue   *queue);
void     c_queue_push_head (CQueue   *queue,
			    void *  data);
void     c_queue_push_tail (CQueue   *queue,
			    void *  data);
cboolean c_queue_is_empty  (CQueue   *queue);
CQueue  *c_queue_new       (void);
void     c_queue_free      (CQueue   *queue);
void     c_queue_foreach   (CQueue   *queue, CFunc func, void * user_data);
CList   *c_queue_find      (CQueue   *queue, const void * data);
void     c_queue_clear     (CQueue *queue);

/*
 * Messages
 */
#ifndef C_LOG_DOMAIN
#define C_LOG_DOMAIN ((char*) 0)
#endif

typedef enum {
	C_LOG_FLAC_RECURSION          = 1 << 0,
	C_LOG_FLAC_FATAL              = 1 << 1,

	C_LOG_LEVEL_ERROR             = 1 << 2,
	C_LOG_LEVEL_CRITICAL          = 1 << 3,
	C_LOG_LEVEL_WARNING           = 1 << 4,
	C_LOG_LEVEL_MESSAGE           = 1 << 5,
	C_LOG_LEVEL_INFO              = 1 << 6,
	C_LOG_LEVEL_DEBUG             = 1 << 7,

	C_LOG_LEVEL_MASK              = ~(C_LOG_FLAC_RECURSION | C_LOG_FLAC_FATAL)
} CLogLevelFlags;

void           c_print                (const char *format, ...);
void           c_printerr             (const char *format, ...);
CLogLevelFlags c_log_set_always_fatal (CLogLevelFlags fatal_mask);
CLogLevelFlags c_log_set_fatal_mask   (const char *log_domain, CLogLevelFlags fatal_mask);
void           c_logv                 (const char *log_domain, CLogLevelFlags log_level, const char *format, va_list args);
void           c_log                  (const char *log_domain, CLogLevelFlags log_level, const char *format, ...);
void           c_assertion_message    (const char *format, ...) C_GNUC_NORETURN;

#ifdef HAVE_C99_SUPPORT
/* The for (;;) tells gc thats c_error () doesn't return, avoiding warnings */
#define c_error(format, ...)    do { c_log (C_LOG_DOMAIN, C_LOG_LEVEL_ERROR, format, __VA_ARGS__); for (;;); } while (0)
#define c_critical(format, ...) c_log (C_LOG_DOMAIN, C_LOG_LEVEL_CRITICAL, format, __VA_ARGS__)
#define c_warning(format, ...)  c_log (C_LOG_DOMAIN, C_LOG_LEVEL_WARNING, format, __VA_ARGS__)
#define c_message(format, ...)  c_log (C_LOG_DOMAIN, C_LOG_LEVEL_MESSAGE, format, __VA_ARGS__)
#define c_debug(format, ...)    c_log (C_LOG_DOMAIN, C_LOG_LEVEL_DEBUG, format, __VA_ARGS__)
#else   /* HAVE_C99_SUPPORT */
#define c_error(...)    do { c_log (C_LOG_DOMAIN, C_LOG_LEVEL_ERROR, __VA_ARGS__); for (;;); } while (0)
#define c_critical(...) c_log (C_LOG_DOMAIN, C_LOG_LEVEL_CRITICAL, __VA_ARGS__)
#define c_warning(...)  c_log (C_LOG_DOMAIN, C_LOG_LEVEL_WARNING, __VA_ARGS__)
#define c_message(...)  c_log (C_LOG_DOMAIN, C_LOG_LEVEL_MESSAGE, __VA_ARGS__)
#define c_debug(...)    c_log (C_LOG_DOMAIN, C_LOG_LEVEL_DEBUG, __VA_ARGS__)
#endif  /* ndef HAVE_C99_SUPPORT */
#define c_log_set_handler(a,b,c,d)

#define C_GNUC_INTERNAL

/*
 * Conversions
 */

UQuark c_convert_error_quark(void);


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
} UUnicodeType;

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
} UUnicodeBreakType;

cunichar       c_unichar_toupper (cunichar c);
cunichar       c_unichar_tolower (cunichar c);
cunichar       c_unichar_totitle (cunichar c);
UUnicodeType   c_unichar_type    (cunichar c);
cboolean       c_unichar_isspace (cunichar c);
cboolean       c_unichar_isxdigit (cunichar c);
int           c_unichar_xdigit_value (cunichar c);
UUnicodeBreakType   c_unichar_break_type (cunichar c);

#define  c_assert(x)     C_STMT_START { if (C_UNLIKELY (!(x))) c_assertion_message ("* Assertion at %s:%d, condition `%s' not met\n", __FILE__, __LINE__, #x);  } C_STMT_END
#define  c_assert_not_reached() C_STMT_START { c_assertion_message ("* Assertion: should not be reached at %s:%d\n", __FILE__, __LINE__); } C_STMT_END

#define c_assert_cmpstr(s1, cmp, s2) \
  C_STMT_START { \
    const char *_s1 = s1; \
    const char *_s2 = s2; \
    if (!(strcmp (_s1, _s2) cmp 0)) \
      c_assertion_message ("* Assertion at %s:%d, condition \"%s\" " #cmp " \"%s\" failed\n", __FILE__, __LINE__, _s1, _s2); \
  } C_STMT_END
#define c_assert_cmpint(n1, cmp, n2) \
  C_STMT_START { \
    int _n1 = n1; \
    int _n2 = n2; \
    if (!(_n1 cmp _n2)) \
      c_assertion_message ("* Assertion at %s:%d, condition %d " #cmp " %d failed\n", __FILE__, __LINE__, _n1, _n2); \
  } C_STMT_END
#define c_assert_cmpuint(n1, cmp, n2) \
  C_STMT_START { \
    int _n1 = n1; \
    int _n2 = n2; \
    if (!(_n1 cmp _n2)) \
      c_assertion_message ("* Assertion at %s:%d, condition %u " #cmp " %u failed\n", __FILE__, __LINE__, _n1, _n2); \
  } C_STMT_END
#define c_assert_cmpfloat(n1, cmp, n2) \
  C_STMT_START { \
    float _n1 = n1; \
    float _n2 = n2; \
    if (!(_n1 cmp _n2)) \
      c_assertion_message ("* Assertion at %s:%d, condition %f " #cmp " %f failed\n", __FILE__, __LINE__, _n1, _n2); \
  } C_STMT_END

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
} UConvertError;

char     *c_utf8_strup (const char *str, ussize len);
char     *c_utf8_strdown (const char *str, ussize len);
int       c_unichar_to_utf8 (cunichar c, char *outbuf);
cunichar  *c_utf8_to_ucs4_fast (const char *str, long len, long *items_written);
cunichar  *c_utf8_to_ucs4 (const char *str, long len, long *items_read, long *items_written, UError **err);
cunichar2 *c_utf8_to_utf16 (const char *str, long len, long *items_read, long *items_written, UError **err);
cunichar2 *eg_utf8_to_utf16_with_nuls (const char *str, long len, long *items_read, long *items_written, UError **err);
char     *c_utf16_to_utf8 (const cunichar2 *str, long len, long *items_read, long *items_written, UError **err);
cunichar  *c_utf16_to_ucs4 (const cunichar2 *str, long len, long *items_read, long *items_written, UError **err);
char     *c_ucs4_to_utf8  (const cunichar *str, long len, long *items_read, long *items_written, UError **err);
cunichar2 *c_ucs4_to_utf16 (const cunichar *str, long len, long *items_read, long *items_written, UError **err);

#define u8to16(str) c_utf8_to_utf16(str, (long)strlen(str), NULL, NULL, NULL)

#ifdef C_OS_WIN32
#define u16to8(str) c_utf16_to_utf8((cunichar2 *) (str), (long)wcslen((wchar_t *) (str)), NULL, NULL, NULL)
#else
#define u16to8(str) c_utf16_to_utf8(str, (long)strlen(str), NULL, NULL, NULL)
#endif

/*
 * Path
 */
char  *c_build_path           (const char *separator, const char *first_element, ...);
#define c_build_filename(x, ...) c_build_path(C_DIR_SEPARATOR_S, x, __VA_ARGS__)
char  *c_path_get_dirname     (const char *filename);
char  *c_path_get_basename    (const char *filename);
char  *c_find_program_in_path (const char *program);
char  *c_get_current_dir      (void);
cboolean c_path_is_absolute    (const char *filename);

const char *c_get_home_dir    (void);
const char *c_get_tmp_dir     (void);
const char *c_get_user_name   (void);
char *c_get_prgname           (void);
void  c_set_prgname            (const char *prgname);

/*
 * Shell
 */

UQuark c_shell_error_get_quark (void);

#define C_SHELL_ERROR c_shell_error_get_quark ()

typedef enum {
	C_SHELL_ERROR_BAD_QUOTING,
	C_SHELL_ERROR_EMPTY_STRING,
	C_SHELL_ERROR_FAILED
} UShellError;


cboolean  c_shell_parse_argv      (const char *command_line, int *argcp, char ***argvp, UError **error);
char    *c_shell_unquote         (const char *quoted_string, UError **error);
char    *c_shell_quote           (const char *unquoted_string);

/*
 * Spawn
 */
UQuark c_shell_error_get_quark (void);

#define C_SPAWN_ERROR c_shell_error_get_quark ()

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
} USpawnError;

typedef enum {
	C_SPAWN_LEAVE_DESCRIPTORS_OPEN = 1,
	C_SPAWN_DO_NOT_REAP_CHILD      = 1 << 1,
	C_SPAWN_SEARCH_PATH            = 1 << 2,
	C_SPAWN_STDOUT_TO_DEV_NULL     = 1 << 3,
	C_SPAWN_STDERR_TO_DEV_NULL     = 1 << 4,
	C_SPAWN_CHILD_INHERITS_STDIN   = 1 << 5,
	C_SPAWN_FILE_AND_ARGV_ZERO     = 1 << 6
} USpawnFlags;

typedef void (*USpawnChildSetupFunc) (void * user_data);

cboolean c_spawn_command_line_sync (const char *command_line, char **standard_output, char **standard_error, int *exit_status, UError **error);
cboolean c_spawn_async_with_pipes  (const char *working_directory, char **argv, char **envp, USpawnFlags flags, USpawnChildSetupFunc child_setup,
				void * user_data, UPid *child_pid, int *standard_input, int *standard_output, int *standard_error, UError **error);


/*
 * Timer
 */
typedef struct _UTimer UTimer;

UTimer *c_timer_new (void);
void c_timer_destroy (UTimer *timer);
double c_timer_elapsed (UTimer *timer, unsigned long *microseconds);
void c_timer_stop (UTimer *timer);
void c_timer_start (UTimer *timer);

/*
 * Date and time
 */
typedef struct {
	long tv_sec;
	long tv_usec;
} CTimeVal;

void c_get_current_time (CTimeVal *result);
void c_usleep (unsigned long microseconds);

/*
 * File
 */

UQuark c_file_error_quark (void);

#define C_FILE_ERROR c_file_error_quark ()

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
} UFileError;

typedef enum {
	C_FILE_TEST_IS_REGULAR = 1 << 0,
	C_FILE_TEST_IS_SYMLINK = 1 << 1,
	C_FILE_TEST_IS_DIR = 1 << 2,
	C_FILE_TEST_IS_EXECUTABLE = 1 << 3,
	C_FILE_TEST_EXISTS = 1 << 4
} UFileTest;


cboolean   c_file_set_contents (const char *filename, const char *contents, ussize length, UError **error);
cboolean   c_file_get_contents (const char *filename, char **contents, size_t *length, UError **error);
UFileError c_file_error_from_errno (int err_no);
int       c_file_open_tmp (const char *tmpl, char **name_used, UError **error);
cboolean   c_file_test (const char *filename, UFileTest test);

#define c_open open
#define c_rename rename
#define c_stat stat
#define c_unlink unlink
#define c_fopen fopen
#define c_lstat lstat
#define c_rmdir rmdir
#define c_mkstemp mkstemp
#define c_ascii_isdigit isdigit
#define c_ascii_strtod strtod
#define c_ascii_isalnum isalnum

/*
 * Pattern matching
 */
typedef struct _UPatternSpec UPatternSpec;
UPatternSpec * c_pattern_spec_new (const char *pattern);
void           c_pattern_spec_free (UPatternSpec *pspec);
cboolean       c_pattern_match_string (UPatternSpec *pspec, const char *string);

/*
 * Directory
 */
typedef struct _UDir UDir;
UDir        *c_dir_open (const char *path, unsigned int flags, UError **error);
const char *c_dir_read_name (UDir *dir);
void         c_dir_rewind (UDir *dir);
void         c_dir_close (UDir *dir);

int          c_mkdir_with_parents (const char *pathname, int mode);
#define c_mkdir mkdir

/*
 * UMarkup
 */

UQuark c_markup_error_quark (void);

#define C_MARKUP_ERROR c_markup_error_quark()

typedef enum {
	C_MARKUP_ERROR_BAD_UTF8,
	C_MARKUP_ERROR_EMPTY,
	C_MARKUP_ERROR_PARSE,
	C_MARKUP_ERROR_UNKNOWN_ELEMENT,
	C_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
	C_MARKUP_ERROR_INVALID_CONTENT,
	C_MARKUP_ERROR_MISSINC_ATTRIBUTE
} UMarkupError;

typedef struct _UMarkupParseContext UMarkupParseContext;

typedef enum
{
	C_MARKUP_DO_NOT_USE_THIS_UNSUPPORTED_FLAG = 1 << 0,
	C_MARKUP_TREAT_CDATA_AS_TEXT              = 1 << 1
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

UMarkupParseContext *c_markup_parse_context_new   (const UMarkupParser *parser,
						   UMarkupParseFlags flags,
						   void * user_data,
						   CDestroyNotify user_data_dnotify);
void                 c_markup_parse_context_free  (UMarkupParseContext *context);
cboolean             c_markup_parse_context_parse (UMarkupParseContext *context,
						   const char *text, ussize text_len,
						   UError **error);
cboolean         c_markup_parse_context_end_parse (UMarkupParseContext *context,
						   UError **error);

/*
 * Character set conversion
 */
typedef struct _UIConv *UIConv;

size_t c_iconv (UIConv cd, char **inbytes, size_t *inbytesleft, char **outbytes, size_t *outbytesleft);
UIConv c_iconv_open (const char *to_charset, const char *from_charset);
int c_iconv_close (UIConv cd);

cboolean  c_get_charset        (C_CONST_RETURN char **charset);
char    *c_locale_to_utf8     (const char *opsysstring, ussize len,
				size_t *bytes_read, size_t *bytes_written,
				UError **error);
char    *c_locale_from_utf8   (const char *utf8string, ussize len, size_t *bytes_read,
				size_t *bytes_written, UError **error);
char    *c_filename_from_utf8 (const char *utf8string, ussize len, size_t *bytes_read,
				size_t *bytes_written, UError **error);
char    *c_convert            (const char *str, ussize len,
				const char *to_codeset, const char *from_codeset,
				size_t *bytes_read, size_t *bytes_written, UError **error);

/*
 * Unicode manipulation
 */
extern const unsigned char c_utf8_jump_table[256];

cboolean  c_utf8_validate      (const char *str, ussize max_len, const char **end);
cunichar  c_utf8_get_char_validated (const char *str, ussize max_len);
char    *c_utf8_find_prev_char (const char *str, const char *p);
char    *c_utf8_prev_char     (const char *str);
#define   c_utf8_next_char(p)  ((p) + c_utf8_jump_table[(unsigned char)(*p)])
cunichar  c_utf8_get_char      (const char *src);
long     c_utf8_strlen        (const char *str, ussize max);
char    *c_utf8_offset_to_pointer (const char *str, long offset);
long     c_utf8_pointer_to_offset (const char *str, const char *pos);

/*
 * priorities
 */
#define C_PRIORITY_DEFAULT 0
#define C_PRIORITY_DEFAULT_IDLE 200

/*
 * Empty thread functions, not used by eglib
 */
#define c_thread_supported()   TRUE
#define c_thread_init(x)       C_STMT_START { if (x != NULL) { c_error ("No vtable supported in c_thread_init"); } } C_STMT_END

#define C_LOCK_DEFINE(name)        int name;
#define C_LOCK_DEFINE_STATIC(name) static int name;
#define C_LOCK_EXTERN(name)
#define C_LOCK(name)
#define C_TRYLOCK(name)
#define C_UNLOCK(name)


#define _CLIB_MAJOR  2
#define _CLIB_MIDDLE 4
#define _CLIB_MINOR  0

#define CLIB_CHECK_VERSION(a,b,c) ((a < _CLIB_MAJOR) || (a == _CLIB_MAJOR && (b < _CLIB_MIDDLE || (b == _CLIB_MIDDLE && c <= _CLIB_MINOR))))

C_END_DECLS

#endif



