/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Whether alignof can be used or not */
/* #undef HAVE_ALIGNOF */

/* Whether __alignof__ can be used or not */
#define HAVE_ALIGNOF_UNDERSCORE 1

/* Define to 1 if you have the <alloca.h> header file. */
#define HAVE_ALLOCA_H 1


/* Have GLES 2.0 for rendering */
#define HAVE_CG_GLES2 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the <EGL/eglext.h> header file. */
#define HAVE_EGL_EGLEXT_H 1

/* Define to 1 if you have the <EGL/egl.h> header file. */
#define HAVE_EGL_EGL_H 1

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the <getopt.h> header file. */
#define HAVE_GETOPT_H 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define to 1 if you have the <malloc.h> header file. */
#define HAVE_MALLOC_H 1

/* Define to 1 if you have the `memmem' function. */
#define HAVE_MEMMEM 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `putenv' function. */
#define HAVE_PUTENV 1

/* Define to 1 if you have the <signal.h> header file. */
#define HAVE_SIGNAL_H 1

/* Whether _Static_assert can be used or not */
#define HAVE_STATIC_ASSERT 1

/* Define to 1 if you have the <stdarg.h> header file. */
#define HAVE_STDARG_H 1

/* Define to 1 if you have the <stddef.h> header file. */
#define HAVE_STDDEF_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strndup' function. */
#define HAVE_STRNDUP 1

/* Define to 1 if you have the `strtok_r' function. */
#define HAVE_STRTOK_R 1

/* Define to 1 if you have the <sys/poll.h> header file. */
#define HAVE_SYS_POLL_H 1

/* Define to 1 if you have the <sys/select.h> header file. */
#define HAVE_SYS_SELECT_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/wait.h> header file. */
#define HAVE_SYS_WAIT_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the `vasprintf' function. */
#define HAVE_VASPRINTF 1

/* Define to the sub-directory in which libtool stores uninstalled libraries.
   */
#define LT_OBJDIR ".libs/"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME "rig"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "rig 2"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "rig"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "2"

/* Define as the return type of signal handlers (`int' or `void'). */
#define RETSIGTYPE void

/* Whether to build the editor or not */
/* #undef RIG_EDITOR_ENABLED 1 */

/* Define if backtracing is available */
/* #undef RUT_ENABLE_BACKTRACE 1 */

/* Define to enable refcount debugging */
#define RUT_ENABLE_REFCOUNT_DEBUG 1

/* path to source data dir */
/* #undef SHARE_UNINSTALLED_DIR */

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Defined if GTK should be used */
/* #undef USE_GTK */

/* On all platforms besides Android we currently use SDL */
/* #undef USE_SDL 1 */

#define USE_UV 1

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */


/*
 * System-dependent settings
 */
#define C_SEARCHPATH_SEPARATOR_S ":"
#define C_SEARCHPATH_SEPARATOR   ':'
#define C_DIR_SEPARATOR          '/'
#define C_DIR_SEPARATOR_S        "/"
#define C_OS_UNIX

#if 1 == 1
#define C_HAVE_ALLOCA_H
#endif

#if 1 == 1
#define C_HAVE_STATIC_ASSERT
#endif
