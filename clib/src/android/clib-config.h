#ifndef __CLIB_CONFIG_H
#define __CLIB_CONFIG_H

/*
 * System-dependent settings
 */
#define C_SEARCHPATH_SEPARATOR_S ":"
#define C_SEARCHPATH_SEPARATOR   ':'
#define C_DIR_SEPARATOR          '/'
#define C_DIR_SEPARATOR_S        "/"
#define C_PLATFORM_UNIX

#if 1 == 1
#define C_HAVE_ALLOCA_H
#endif

#if 1 == 1
#define C_HAVE_STATIC_ASSERT
#endif

typedef int c_pid_t;

#endif
