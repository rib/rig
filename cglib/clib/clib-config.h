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

/* Things defined in clib-platform.h below end up exposed to anyone
 * that includes clib.h but we have a few portability defines we want
 * available across clib while compiling.
 */
#if defined _MSC_VER
#  define _WINSOCKAPI_
#  include <Windows.h>
#  include <io.h>
#  include <sys/types.h> // provides off_t

#  define R_OK 4
#  define W_OK 2
#  define X_OK R_OK
#  define F_OK 0

#  define access _access
#endif


/* Most platform configuration is handled at compile time in
 * clib-platform.h but if clib is built with autotools then it's
 * possible to defines some options at configure time...
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


/* referenced in ascii_snprintf.c */
#define HAVE_STDARG_H 1
#define HAVE_STDDEF_H 1
#define HAVE_STDINT_H 1


/* XXX: defines that should be removed if we can assume they
 * are always available... */
#define HAVE_FFS 1
#define HAVE_INTTYPES_H 1
#define HAVE_LANGINFO_H 1
#define HAVE_LIMITS_H 1

#define HAVE_STRDUP 1


#include "clib-platform.h"
