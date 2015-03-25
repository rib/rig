/*
 * Copyright (C) 2015 Intel Corporation.
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

#ifndef _CLIB_PLATFORMS_H_
#define _CLIB_PLATFORMS_H_

/* As much as possible we try to avoid depending on build time checks
 * which don't work well for the various cross compiling use cases we
 * want to support...
 */

#ifdef __unix__
#  define C_SEARCHPATH_SEPARATOR_S ":"
#  define C_SEARCHPATH_SEPARATOR   ':'
#  define C_DIR_SEPARATOR          '/'
#  define C_DIR_SEPARATOR_S        "/"
#  define C_PIDTYPE                int
#  define C_HAVE_ALLOCA_H          1
#  define C_BREAKPOINT()    \
    C_STMT_START {          \
        raise(SIGTRAP);     \
    } C_STMT_END
#endif /* __unix__ */

/* Would like to assume all platforms support these now, but maybe
 * not... */
#define C_HAVE_STATIC_ASSERT     1
#define C_ALIGNOF(x)            (alignof(x))

#if defined(__ANDROID__)

#  define C_PLATFORM "Android"
#  define C_PLATFORM_ANDROID 1
#  define C_PLATFORM_HAS_FDS 1
#  define C_HAVE_PTHREADS 1

#elif defined( __linux__)

#  define C_PLATFORM "Linux"
#  define C_PLATFORM_LINUX 1
#  define C_PLATFORM_HAS_FDS 1
#  define C_HAVE_PTHREADS 1
#  define C_PLATFORM_HAS_XDG_DIRS 1

#  include <stdio.h>

#  ifdef __GLIBC__ && _GNU_SOURCE
#    ifndef HAVE_MEMMEM
#      define HAVE_MEMMEM
#    endif
#  endif /* gnu */

#elif defined(__APPLE__)

#  define C_PLATFORM "Darwin"
#  define C_PLATFORM_DARWIN 1
#  define C_PLATFORM_HAS_FDS 1
#  define C_HAVE_PTHREADS 1

#elif defined(__EMSCRIPTEN__)

#  define C_PLATFORM "Web"
#  define C_PLATFORM_IS_WEB 1
#  define C_PLATFORM_WEB 1
#  undef C_BREAKPOINT
#  define C_BREAKPOINT() emscripten_breakpoint()

#elif defined(WIN32)

#  define C_PLATFORM="Windows"
#  define C_PLATFORM_IS_WINDOWS 1
#  define C_PLATFORM_WINDOWS 1

#  define C_SEARCHPATH_SEPARATOR_S ";"
#  define C_SEARCHPATH_SEPARATOR   ';'
#  define C_DIR_SEPARATOR          '\\'
#  define C_DIR_SEPARATOR_S        "\\"
#  define C_PIDTYPE                void *

#else
#  error "Unknown platform"
#endif

typedef C_PIDTYPE c_pid_t;

#ifndef C_BREAKPOINT
#define C_BREAKPOINT()
#endif

#endif /* _CLIB_PLATFORMS_H_ */
