/*
 * Output and debugging functions
 *
 * Author:
 *   Miguel de Icaza (miguel@novell.com)
 *
 * (C) 2006 Novell, Inc.
 * Copyright 2011 Xamarin Inc.
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
#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <clib.h>

/* The current fatal levels, error is always fatal */
static c_log_level_flags_t fatal = C_LOG_LEVEL_ERROR;

#if PLATFORM_ANDROID
#include <android/log.h>

static android_LogPriority
to_android_priority(c_log_level_flags_t log_level)
{
    switch (log_level & C_LOG_LEVEL_MASK) {
    case C_LOG_LEVEL_ERROR:
        return ANDROID_LOG_FATAL;
    case C_LOG_LEVEL_CRITICAL:
        return ANDROID_LOG_ERROR;
    case C_LOG_LEVEL_WARNING:
        return ANDROID_LOG_WARN;
    case C_LOG_LEVEL_MESSAGE:
        return ANDROID_LOG_INFO;
    case C_LOG_LEVEL_INFO:
        return ANDROID_LOG_DEBUG;
    case C_LOG_LEVEL_DEBUG:
        return ANDROID_LOG_VERBOSE;
    }
    return ANDROID_LOG_UNKNOWN;
}

static void
out_vfprintf(FILE *ignore, const char *format, va_list args)
{
    /* TODO: provide a proper app name */
    __android_log_vprint(ANDROID_LOG_ERROR, "mono", format, args);
}
#elif MONOTOUCH &&defined(__arm__)
#include <asl.h>

static int
to_asl_priority(c_log_level_flags_t log_level)
{
    switch (log_level & C_LOG_LEVEL_MASK) {
    case C_LOG_LEVEL_ERROR:
        return ASL_LEVEL_CRIT;
    case C_LOG_LEVEL_CRITICAL:
        return ASL_LEVEL_ERR;
    case C_LOG_LEVEL_WARNING:
        return ASL_LEVEL_WARNING;
    case C_LOG_LEVEL_MESSAGE:
        return ASL_LEVEL_NOTICE;
    case C_LOG_LEVEL_INFO:
        return ASL_LEVEL_INFO;
    case C_LOG_LEVEL_DEBUG:
        return ASL_LEVEL_DEBUG;
    }
    return ASL_LEVEL_ERR;
}

static void
out_vfprintf(FILE *ignore, const char *format, va_list args)
{
    asl_vlog(NULL, NULL, ASL_LEVEL_WARNING, format, args);
}

#else
static void
out_vfprintf(FILE *file, const char *format, va_list args)
{
    vfprintf(file, format, args);
}
#endif

void
c_print(const char *format, ...)
{
    va_list args;

    va_start(args, format);

    out_vfprintf(stdout, format, args);

    va_end(args);
}

void
c_printerr(const char *format, ...)
{
    va_list args;

    va_start(args, format);

    out_vfprintf(stderr, format, args);

    va_end(args);
}

c_log_level_flags_t
c_log_set_always_fatal(c_log_level_flags_t fatal_mask)
{
    c_log_level_flags_t old_fatal = fatal;

    fatal |= fatal_mask;

    return old_fatal;
}

c_log_level_flags_t
c_log_set_fatal_mask(const char *log_domain,
                     c_log_level_flags_t fatal_mask)
{
    /*
     * Mono does not use a C_LOG_DOMAIN currently, so we just assume things are
     * fatal
     * if we decide to set C_LOG_DOMAIN (we probably should) we should implement
     * this.
     */
    return fatal_mask;
}

void (*c_log_hook)(c_log_context_t *lctx,
                   const char *log_domain,
                   c_log_level_flags_t log_level,
                   const char *message);

void
c_logv(c_log_context_t *lctx,
       const char *log_domain,
       c_log_level_flags_t log_level,
       const char *format,
       va_list args)
{
#if PLATFORM_ANDROID
    __android_log_vprint(
        to_android_priority(log_level), log_domain, format, args);
#elif MONOTOUCH &&defined(__arm__)
    asl_vlog(NULL, NULL, to_asl_priority(log_level), format, args);
#else
    char *msg;

    if (vasprintf(&msg, format, args) < 0)
        return;

    if (c_log_hook) {
        c_log_hook(lctx, log_domain, log_level, msg);
        goto logged;
    }

#ifdef C_OS_WIN32
    printf("%s%s%s\n",
           log_domain != NULL ? log_domain : "",
           log_domain != NULL ? ": " : "",
           msg);
#else
#if MONOTOUCH
    FILE *target = stderr;
#else
    FILE *target = stdout;
#endif

    fprintf(target,
            "%s%s%s\n",
            log_domain != NULL ? log_domain : "",
            log_domain != NULL ? ": " : "",
            msg);
#endif

logged:
    free(msg);
    if (log_level & fatal) {
        fflush(stdout);
        fflush(stderr);
    }
#endif
    if (log_level & fatal)
        abort();
}

void
c_log(c_log_context_t *lctx,
      const char *log_domain,
      c_log_level_flags_t log_level,
      const char *format,
      ...)
{
    va_list args;

    va_start(args, format);
    c_logv(lctx, log_domain, log_level, format, args);
    va_end(args);
}

void
c_assertion_message(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    c_logv(NULL, C_LOG_DOMAIN, C_LOG_LEVEL_ERROR, format, args);
    va_end(args);
    abort();
}
