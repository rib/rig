/*
 * Copyright 息 2006 Keith Packard
 * Copyright 息 2010 Behdad Esfahbod
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the author(s) not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The authors make no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * THE AUTHOR(S) DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* This header file is supposed to be included in config.h */
#pragma once

#ifdef SIZEOF_VOID_P
#undef SIZEOF_VOID_P
#endif
#ifdef ALIGNOF_VOID_P
#undef ALIGNOF_VOID_P
#endif
#ifdef ALIGNOF_DOUBLE
#undef ALIGNOF_DOUBLE
#endif


/* just a hack to build the fat binaries:
 * https://bugs.freedesktop.org/show_bug.cgi?id=20208
 */
#ifdef __APPLE__
# include <machine/endian.h>
# ifdef __LP64__
#  define SIZEOF_VOID_P 8
#  define ALIGNOF_DOUBLE 8
# else
#  define SIZEOF_VOID_P 4
#  define ALIGNOF_DOUBLE 4
# endif
#elif defined(__unix__)
# if __SIZEOF_POINTER__ == 8
#  define ALIGNOF_DOUBLE 8
#  define SIZEOF_VOID_P 8
#  define ALIGNOF_VOID_P 8
# else
#  define ALIGNOF_DOUBLE 4
#  define SIZEOF_VOID_P 4
#  define ALIGNOF_VOID_P 4
# endif
#elif defined(_WIN32)
#  define ALIGNOF_DOUBLE 8
# ifdef _M_X64
#  define SIZEOF_VOID_P 8
#  define ALIGNOF_VOID_P 8
# else
#  define SIZEOF_VOID_P 4
#  define ALIGNOF_VOID_P 4
# endif
#endif

#ifndef SIZEOF_VOID_P
#error "undetermined pointer/double size + alignment"
#endif

struct _alignof_double_check {
    char a;
    double b;
};

_Static_assert(sizeof(struct _alignof_double_check) == (ALIGNOF_DOUBLE + 8),
               "ALIGNOF_DOUBLE not correct");

struct _alignof_pointer_check {
    char a;
    void *b;
};

_Static_assert(sizeof(struct _alignof_pointer_check) == (ALIGNOF_VOID_P + SIZEOF_VOID_P),
               "ALIGNOF_VOID_P + SIZEOF_VOID_P not correct");

