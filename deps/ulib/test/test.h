/*
 * ULib Unit Group/Test Runners
 *
 * Author:
 *   Aaron Bockover (abockover@novell.com)
 *
 * (C) 2006 Novell, Inc.
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
 
#ifndef _TEST_H
#define _TEST_H

#include <config.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ulib.h>

#ifdef _MSC_VER
/* disable the following warnings 
 * C4100: The formal parameter is not referenced in the body of the function. The unreferenced parameter is ignored. 
 * C4127: conditional expression is constant (test macros produce a lot of these)
*/
#pragma warning(disable:4100 4127)
#endif

typedef char * RESULT;

typedef struct _Test Test;
typedef struct _Group Group;

typedef char * (* RunTestHandler)();
typedef Test * (* LoadGroupHandler)();

struct _Test {
	const char *name;
	RunTestHandler handler;
};

struct _Group {
	const char *name;
	LoadGroupHandler handler;
};

uboolean run_group(Group *group, int iterations, uboolean quiet, 
	uboolean time, char *tests);
#undef FAILED
RESULT FAILED(const char *format, ...);
double get_timestamp();
char ** eg_strsplit (const char *string, const char *delimiter, int max_tokens);
void eg_strfreev (char **str_array);

#define OK NULL

#define DEFINE_TEST_GROUP_INIT(name, table) \
	Test * (name)() { return table; }

#define DEFINE_TEST_GROUP_INIT_H(name) \
	Test * (name)();

#endif /* _TEST_H */


