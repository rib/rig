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

#pragma once

/* As much as possible we try to avoid depending on build time checks
 * which don't work well for the various cross compiling use cases we
 * want to support...
 */
#ifndef _ALL_SOURCE
# define _ALL_SOURCE 1
#endif
#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif

#define C_LITTLE_ENDIAN 1234
#define C_BIC_ENDIAN 4321

#if (defined(BYTE_ORDER) && (BYTE_ORDER == BIG_ENDIAN)) || \
    (defined(_BYTE_ORDER) && (_BYTE_ORDER == _BIG_ENDIAN))
#  define C_BYTE_ORDER C_BIG_ENDIAN
#else
#  define C_BYTE_ORDER C_LITTLE_ENDIAN
#endif

#if defined(__unix__) || defined(__APPLE__)
#  define C_PLATFORM_UNIX	   1

#  define C_SEARCHPATH_SEPARATOR_S ":"
#  define C_SEARCHPATH_SEPARATOR   ':'
#  define C_DIR_SEPARATOR          '/'
#  define C_DIR_SEPARATOR_S        "/"
#  define C_PIDTYPE                int
#  define C_HAVE_ALLOCA_H          1
#  define C_HAVE_FMEMOPEN          1
#  define C_HAVE_PTHREADS          1
#  define C_HAVE_GETPWUID_R        1
#  define C_HAVE_MEMMEM            1

#  define C_PLATFORM_HAS_FDS       1

#  define C_BREAKPOINT()    \
    C_STMT_START {          \
        raise(SIGTRAP);     \
    } C_STMT_END
#endif /* __unix__ || __APPLE__ */

#define C_HAVE_STATIC_ASSERT     1

#if defined _MSC_VER
#  define C_ALIGNOF(x) __alignof(x)
#  include <basetsd.h>
typedef SSIZE_T c_ssize_t;
#else
#  include <sys/types.h>
#  define C_ALIGNOF(x) __alignof__(x)
#  define C_HAVE_ICONV 1
#  define C_HAVE_ICONV_H 1
#  define HAVE_STPCPY 1
typedef ssize_t c_ssize_t;
#endif

/* missing on ios/windows */
#define C_HAVE_STRNDUP 1

#if defined(__ANDROID__)

#  define C_PLATFORM "Android"
#  define C_PLATFORM_ANDROID 1
#  define C_HAVE_PTHREADS 1
#  undef HAVE_GETPWUID_R

#elif defined( __linux__)

#  define C_PLATFORM "Linux"
#  define C_PLATFORM_LINUX 1
#  define C_HAVE_PTHREADS 1

#elif defined(__APPLE__)

#  include <TargetConditionals.h>
#  ifdef TARGET_OS_IPHONE
#    define C_PLATFORM "IOS"
#    define C_PLATFORM_IOS 1
#  else
#    define C_PLATFORM "Mac"
#    define C_PLATFORM_MAC 1
#  endif
#  define C_PLATFORM_DARWIN 1
#  undef  C_HAVE_FMEMOPEN
#  undef  C_HAVE_STRNDUP

#elif defined(__EMSCRIPTEN__)

#  define C_PLATFORM "Web"
#  define C_PLATFORM_WEB 1
#  undef C_HAVE_PTHREADS
#  undef C_BREAKPOINT
#  define C_BREAKPOINT() _c_web_console_assert(0, "breakpoint")

#elif defined(_WIN32)

#  define C_PLATFORM "Windows"
#  define C_PLATFORM_WINDOWS 1
#  undef  C_HAVE_FMEMOPEN
#  undef  C_HAVE_MEMMEM
#  undef  C_HAVE_STRNDUP

#  define C_SEARCHPATH_SEPARATOR_S ";"
#  define C_SEARCHPATH_SEPARATOR   ';'
#  define C_DIR_SEPARATOR          '\\'
#  define C_DIR_SEPARATOR_S        "\\"
#  define C_PIDTYPE                void *

#  ifndef __cplusplus
#    define inline __inline
#  endif
#else
#  error "Unknown platform"
#endif

typedef C_PIDTYPE c_pid_t;

#ifndef C_BREAKPOINT
#define C_BREAKPOINT()
#endif
