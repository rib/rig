/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2009 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#include <cglib-config.h>

#include <stdlib.h>
#include <string.h>

#include <clib.h>

#include <test-fixtures/test-cg-fixtures.h>

#include "cg-device-private.h"
#include "cg-debug.h"
#include "cg-blend-string.h"
#include "cg-error-private.h"

typedef enum _parser_state_t {
    PARSER_STATE_EXPECT_DEST_CHANNELS,
    PARSER_STATE_SCRAPING_DEST_CHANNELS,
    PARSER_STATE_EXPECT_FUNCTION_NAME,
    PARSER_STATE_SCRAPING_FUNCTION_NAME,
    PARSER_STATE_EXPECT_ARG_START,
    PARSER_STATE_EXPECT_STATEMENT_END
} parser_state_t;

typedef enum _parser_arg_state_t {
    PARSER_ARG_STATE_START,
    PARSER_ARG_STATE_EXPECT_MINUS,
    PARSER_ARG_STATE_EXPECT_COLOR_SRC_NAME,
    PARSER_ARG_STATE_SCRAPING_COLOR_SRC_NAME,
    PARSER_ARG_STATE_MAYBE_COLOR_MASK,
    PARSER_ARG_STATE_SCRAPING_MASK,
    PARSER_ARG_STATE_MAYBE_MULT,
    PARSER_ARG_STATE_EXPECT_OPEN_PAREN,
    PARSER_ARG_STATE_EXPECT_FACTOR,
    PARSER_ARG_STATE_MAYBE_SRC_ALPHA_SATURATE,
    PARSER_ARG_STATE_MAYBE_MINUS,
    PARSER_ARG_STATE_EXPECT_CLOSE_PAREN,
    PARSER_ARG_STATE_EXPECT_END
} parser_arg_state_t;

#define DEFINE_COLOR_SOURCE(NAME, NAME_LEN)                                    \
    { /*.type = */                                                             \
        CG_BLEND_STRING_COLOR_SOURCE_##NAME,                                   \
        /*.name = */ #NAME,                                                \
        /*.name_len = */ NAME_LEN                                          \
    }

static cg_blend_string_color_source_info_t blending_color_sources[] = {
    DEFINE_COLOR_SOURCE(SRC_COLOR, 9), DEFINE_COLOR_SOURCE(DST_COLOR, 9),
    DEFINE_COLOR_SOURCE(CONSTANT, 8)
};

#undef DEFINE_COLOR_SOURCE

#define DEFINE_FUNCTION(NAME, NAME_LEN, ARGC)                                  \
    { /*.type = */                                                             \
        CG_BLEND_STRING_FUNCTION_##NAME,                                       \
        /*.name = */ #NAME,                                                \
        /*.name_len = */ NAME_LEN,                                         \
        /*.argc = */ ARGC                                                  \
    }

static cg_blend_string_function_info_t blend_functions[] = { DEFINE_FUNCTION(
                                                                 ADD, 3, 2) };

#undef DEFINE_FUNCTION

uint32_t
cg_blend_string_error_domain(void)
{
    return c_quark_from_static_string("cg-blend-string-error-quark");
}

void
_cg_blend_string_split_rgba_statement(cg_blend_string_statement_t *statement,
                                      cg_blend_string_statement_t *rgb,
                                      cg_blend_string_statement_t *a)
{
    int i;

    memcpy(rgb, statement, sizeof(cg_blend_string_statement_t));
    memcpy(a, statement, sizeof(cg_blend_string_statement_t));

    rgb->mask = CG_BLEND_STRING_CHANNEL_MASK_RGB;
    a->mask = CG_BLEND_STRING_CHANNEL_MASK_ALPHA;

    for (i = 0; i < statement->function->argc; i++) {
        cg_blend_string_argument_t *arg = &statement->args[i];
        cg_blend_string_argument_t *rgb_arg = &rgb->args[i];
        cg_blend_string_argument_t *a_arg = &a->args[i];

        if (arg->source.mask == CG_BLEND_STRING_CHANNEL_MASK_RGBA) {
            rgb_arg->source.mask = CG_BLEND_STRING_CHANNEL_MASK_RGB;
            a_arg->source.mask = CG_BLEND_STRING_CHANNEL_MASK_ALPHA;
        }

        if (arg->factor.is_color &&
            arg->factor.source.mask == CG_BLEND_STRING_CHANNEL_MASK_RGBA) {
            rgb_arg->factor.source.mask = CG_BLEND_STRING_CHANNEL_MASK_RGB;
            a_arg->factor.source.mask = CG_BLEND_STRING_CHANNEL_MASK_ALPHA;
        }
    }
}

static bool
validate_blend_statements(cg_device_t *dev,
                          cg_blend_string_statement_t *statements,
                          int n_statements,
                          cg_error_t **error)
{
    int i, j;
    const char *error_string;
    cg_blend_string_error_t detail = CG_BLEND_STRING_ERROR_INVALID_ERROR;

    if (n_statements == 2 && !dev->glBlendEquationSeparate &&
        statements[0].function->type != statements[1].function->type) {
        error_string = "Separate blend functions for the RGB an A "
                       "channels isn't supported by the driver";
        detail = CG_BLEND_STRING_ERROR_GPU_UNSUPPORTED_ERROR;
        goto error;
    }

    for (i = 0; i < n_statements; i++)
        for (j = 0; j < statements[i].function->argc; j++) {
            cg_blend_string_argument_t *arg = &statements[i].args[j];

            if (arg->source.is_zero)
                continue;

            if ((j == 0 && arg->source.info->type !=
                 CG_BLEND_STRING_COLOR_SOURCE_SRC_COLOR) ||
                (j == 1 && arg->source.info->type !=
                 CG_BLEND_STRING_COLOR_SOURCE_DST_COLOR)) {
                error_string = "For blending you must always use SRC_COLOR "
                               "for arg0 and DST_COLOR for arg1";
                goto error;
            }

            if (!_cg_has_private_feature(dev,
                                         CG_PRIVATE_FEATURE_BLEND_CONSTANT) &&
                arg->factor.is_color &&
                (arg->factor.source.info->type ==
                 CG_BLEND_STRING_COLOR_SOURCE_CONSTANT)) {
                error_string = "Driver doesn't support constant blend factors";
                detail = CG_BLEND_STRING_ERROR_GPU_UNSUPPORTED_ERROR;
                goto error;
            }
        }

    return true;

error:
    _cg_set_error(error,
                  CG_BLEND_STRING_ERROR,
                  detail,
                  "Invalid blend string: %s",
                  error_string);
    return false;
}

static bool
validate_statements(cg_device_t *dev,
                    cg_blend_string_statement_t *statements,
                    int n_statements,
                    cg_error_t **error)
{
    const char *error_string;

    if (n_statements == 1) {
        if (statements[0].mask == CG_BLEND_STRING_CHANNEL_MASK_ALPHA) {
            error_string =
                "You need to also give a blend statement for the RGB "
                "channels";
            goto error;
        } else if (statements[0].mask == CG_BLEND_STRING_CHANNEL_MASK_RGB) {
            error_string = "You need to also give a blend statement for the "
                           "Alpha channel";
            goto error;
        }
    }

    return validate_blend_statements(dev, statements, n_statements,
                                     error);

error:
    _cg_set_error(error,
                  CG_BLEND_STRING_ERROR,
                  CG_BLEND_STRING_ERROR_INVALID_ERROR,
                  "Invalid blend string: %s",
                  error_string);

    if (CG_DEBUG_ENABLED(CG_DEBUG_BLEND_STRINGS))
        c_debug("Invalid blend string: %s", error_string);

    return false;
}

static void
print_argument(cg_blend_string_argument_t *arg)
{
    const char *mask_names[] = { "RGB", "A", "RGBA" };

    c_print(" Arg:\n");
    c_print("  is zero = %s\n", arg->source.is_zero ? "yes" : "no");
    if (!arg->source.is_zero) {
        c_print("  color source = %s\n", arg->source.info->name);
        c_print("  one minus = %s\n", arg->source.one_minus ? "yes" : "no");
        c_print("  mask = %s\n", mask_names[arg->source.mask]);
        c_print("  texture = %d\n", arg->source.texture);
        c_print("\n");
        c_print("  factor is_one = %s\n", arg->factor.is_one ? "yes" : "no");
        c_print("  factor is_src_alpha_saturate = %s\n",
                arg->factor.is_src_alpha_saturate ? "yes" : "no");
        c_print("  factor is_color = %s\n",
                arg->factor.is_color ? "yes" : "no");
        if (arg->factor.is_color) {
            c_print("  factor color:is zero = %s\n",
                    arg->factor.source.is_zero ? "yes" : "no");
            c_print("  factor color:color source = %s\n",
                    arg->factor.source.info->name);
            c_print("  factor color:one minus = %s\n",
                    arg->factor.source.one_minus ? "yes" : "no");
            c_print("  factor color:mask = %s\n",
                    mask_names[arg->factor.source.mask]);
            c_print("  factor color:texture = %d\n",
                    arg->factor.source.texture);
        }
    }
}

static void
print_statement(int num, cg_blend_string_statement_t *statement)
{
    const char *mask_names[] = { "RGB", "A", "RGBA" };
    int i;
    c_print("Statement %d:\n", num);
    c_print(" Destination channel mask = %s\n", mask_names[statement->mask]);
    c_print(" Function = %s\n", statement->function->name);
    for (i = 0; i < statement->function->argc; i++)
        print_argument(&statement->args[i]);
}

static const cg_blend_string_function_info_t *
get_function_info(const char *mark, const char *p)
{
    size_t len = p - mark;
    cg_blend_string_function_info_t *functions;
    size_t array_len;
    int i;

    functions = blend_functions;
    array_len = C_N_ELEMENTS(blend_functions);

    for (i = 0; i < array_len; i++) {
        if (len >= functions[i].name_len &&
            strncmp(mark, functions[i].name, functions[i].name_len) == 0)
            return &functions[i];
    }
    return NULL;
}

static const cg_blend_string_color_source_info_t *
get_color_src_info(const char *mark, const char *p)
{
    size_t len = p - mark;
    cg_blend_string_color_source_info_t *sources;
    size_t array_len;
    int i;

    sources = blending_color_sources;
    array_len = C_N_ELEMENTS(blending_color_sources);

    for (i = 0; i < array_len; i++) {
        if (len >= sources[i].name_len &&
            strncmp(mark, sources[i].name, sources[i].name_len) == 0)
            return &sources[i];
    }

    return NULL;
}

static bool
is_symbol_char(const char c)
{
    return (c_ascii_isalpha(c) || c == '_') ? true : false;
}

static bool
is_alphanum_char(const char c)
{
    return (c_ascii_isalnum(c) || c == '_') ? true : false;
}

static bool
parse_argument(const char *string,             /* original user string */
               const char **ret_p,             /* start of argument IN:OUT */
               const cg_blend_string_statement_t *statement,
               int current_arg,
               cg_blend_string_argument_t *arg,             /* OUT */
               cg_error_t **error)
{
    const char *p = *ret_p;
    const char *mark = NULL;
    const char *error_string = NULL;
    parser_arg_state_t state = PARSER_ARG_STATE_START;
    bool parsing_factor = false;
    bool implicit_factor_brace;

    arg->source.is_zero = false;
    arg->source.info = NULL;
    arg->source.texture = 0;
    arg->source.one_minus = false;
    arg->source.mask = statement->mask;

    arg->factor.is_one = false;
    arg->factor.is_color = false;
    arg->factor.is_src_alpha_saturate = false;

    arg->factor.source.is_zero = false;
    arg->factor.source.info = NULL;
    arg->factor.source.texture = 0;
    arg->factor.source.one_minus = false;
    arg->factor.source.mask = statement->mask;

    do {
        if (c_ascii_isspace(*p))
            continue;

        if (*p == '\0') {
            error_string = "Unexpected end of string while parsing argument";
            goto error;
        }

        switch (state) {
        case PARSER_ARG_STATE_START:
            if (*p == '1')
                state = PARSER_ARG_STATE_EXPECT_MINUS;
            else if (*p == '0') {
                arg->source.is_zero = true;
                state = PARSER_ARG_STATE_EXPECT_END;
            } else {
                p--; /* backtrack */
                state = PARSER_ARG_STATE_EXPECT_COLOR_SRC_NAME;
            }
            continue;

        case PARSER_ARG_STATE_EXPECT_MINUS:
            if (*p != '-') {
                error_string = "expected a '-' following the 1";
                goto error;
            }
            arg->source.one_minus = true;
            state = PARSER_ARG_STATE_EXPECT_COLOR_SRC_NAME;
            continue;

        case PARSER_ARG_STATE_EXPECT_COLOR_SRC_NAME:
            if (!is_symbol_char(*p)) {
                error_string = "expected a color source name";
                goto error;
            }
            state = PARSER_ARG_STATE_SCRAPING_COLOR_SRC_NAME;
            mark = p;
            if (parsing_factor)
                arg->factor.is_color = true;

        /* fall through */
        case PARSER_ARG_STATE_SCRAPING_COLOR_SRC_NAME:
            if (!is_symbol_char(*p)) {
                cg_blend_string_color_source_t *source =
                    parsing_factor ? &arg->factor.source : &arg->source;
                source->info = get_color_src_info(mark, p);
                if (!source->info) {
                    error_string = "Unknown color source name";
                    goto error;
                }
                state = PARSER_ARG_STATE_MAYBE_COLOR_MASK;
            } else
                continue;

        /* fall through */
        case PARSER_ARG_STATE_MAYBE_COLOR_MASK:
            if (*p != '[') {
                p--; /* backtrack */
                if (!parsing_factor)
                    state = PARSER_ARG_STATE_MAYBE_MULT;
                else
                    state = PARSER_ARG_STATE_EXPECT_END;
                continue;
            }
            state = PARSER_ARG_STATE_SCRAPING_MASK;
            mark = p;

        /* fall through */
        case PARSER_ARG_STATE_SCRAPING_MASK:
            if (*p == ']') {
                size_t len = p - mark;
                cg_blend_string_color_source_t *source =
                    parsing_factor ? &arg->factor.source : &arg->source;

                if (len == 5 && strncmp(mark, "[RGBA", len) == 0) {
                    if (statement->mask != CG_BLEND_STRING_CHANNEL_MASK_RGBA) {
                        error_string =
                            "You can't use an RGBA color mask if the "
                            "statement hasn't also got an RGBA= mask";
                        goto error;
                    }
                    source->mask = CG_BLEND_STRING_CHANNEL_MASK_RGBA;
                } else if (len == 4 && strncmp(mark, "[RGB", len) == 0)
                    source->mask = CG_BLEND_STRING_CHANNEL_MASK_RGB;
                else if (len == 2 && strncmp(mark, "[A", len) == 0)
                    source->mask = CG_BLEND_STRING_CHANNEL_MASK_ALPHA;
                else {
                    error_string = "Expected a channel mask of [RGBA]"
                                   "[RGB] or [A]";
                    goto error;
                }
                if (parsing_factor)
                    state = PARSER_ARG_STATE_EXPECT_CLOSE_PAREN;
                else
                    state = PARSER_ARG_STATE_MAYBE_MULT;
            }
            continue;

        case PARSER_ARG_STATE_EXPECT_OPEN_PAREN:
            if (*p != '(') {
                if (is_alphanum_char(*p)) {
                    p--; /* compensate for implicit brace and ensure this
                          * char gets considered part of the blend factor */
                    implicit_factor_brace = true;
                } else {
                    error_string = "Expected '(' around blend factor or alpha "
                                   "numeric character for blend factor name";
                    goto error;
                }
            } else
                implicit_factor_brace = false;
            parsing_factor = true;
            state = PARSER_ARG_STATE_EXPECT_FACTOR;
            continue;

        case PARSER_ARG_STATE_EXPECT_FACTOR:
            if (*p == '1')
                state = PARSER_ARG_STATE_MAYBE_MINUS;
            else if (*p == '0') {
                arg->source.is_zero = true;
                state = PARSER_ARG_STATE_EXPECT_CLOSE_PAREN;
            } else {
                state = PARSER_ARG_STATE_MAYBE_SRC_ALPHA_SATURATE;
                mark = p;
            }
            continue;

        case PARSER_ARG_STATE_MAYBE_SRC_ALPHA_SATURATE:
            if (!is_symbol_char(*p)) {
                size_t len = p - mark;
                if (len >= strlen("SRC_ALPHA_SATURATE") &&
                    strncmp(mark, "SRC_ALPHA_SATURATE", len) == 0) {
                    arg->factor.is_src_alpha_saturate = true;
                    state = PARSER_ARG_STATE_EXPECT_CLOSE_PAREN;
                } else {
                    state = PARSER_ARG_STATE_EXPECT_COLOR_SRC_NAME;
                    p = mark - 1; /* backtrack */
                }
            }
            continue;

        case PARSER_ARG_STATE_MAYBE_MINUS:
            if (*p == '-') {
                if (implicit_factor_brace) {
                    error_string =
                        "Expected ( ) braces around blend factor with "
                        "a subtraction";
                    goto error;
                }
                arg->factor.source.one_minus = true;
                state = PARSER_ARG_STATE_EXPECT_COLOR_SRC_NAME;
            } else {
                arg->factor.is_one = true;
                state = PARSER_ARG_STATE_EXPECT_CLOSE_PAREN;
            }
            continue;

        case PARSER_ARG_STATE_EXPECT_CLOSE_PAREN:
            if (implicit_factor_brace) {
                p--;
                state = PARSER_ARG_STATE_EXPECT_END;
                continue;
            }
            if (*p != ')') {
                error_string =
                    "Expected closing parenthesis after blend factor";
                goto error;
            }
            state = PARSER_ARG_STATE_EXPECT_END;
            continue;

        case PARSER_ARG_STATE_MAYBE_MULT:
            if (*p == '*') {
                state = PARSER_ARG_STATE_EXPECT_OPEN_PAREN;
                continue;
            }
            arg->factor.is_one = true;
            state = PARSER_ARG_STATE_EXPECT_END;

        /* fall through */
        case PARSER_ARG_STATE_EXPECT_END:
            if (*p != ',' && *p != ')') {
                error_string = "expected , or )";
                goto error;
            }

            *ret_p = p - 1;
            return true;
        }
    } while (p++);

error: {
        int offset = p - string;
        _cg_set_error(error,
                      CG_BLEND_STRING_ERROR,
                      CG_BLEND_STRING_ERROR_ARGUMENT_PARSE_ERROR,
                      "Syntax error for argument %d at offset %d: %s",
                      current_arg,
                      offset,
                      error_string);

        if (CG_DEBUG_ENABLED(CG_DEBUG_BLEND_STRINGS)) {
            c_debug("Syntax error for argument %d at offset %d: %s",
                    current_arg,
                    offset,
                    error_string);
        }
        return false;
    }
}

bool
_cg_blend_string_compile(cg_device_t *dev,
                         const char *string,
                         cg_blend_string_statement_t *statements,
                         cg_error_t **error)
{
    const char *p = string;
    const char *mark = NULL;
    const char *error_string;
    parser_state_t state = PARSER_STATE_EXPECT_DEST_CHANNELS;
    cg_blend_string_statement_t *statement = statements;
    int current_statement = 0;
    int current_arg = 0;
    int remaining_argc = 0;

#if 0
    CG_DEBUG_SET_FLAG (CG_DEBUG_BLEND_STRINGS);
#endif

    if (CG_DEBUG_ENABLED(CG_DEBUG_BLEND_STRINGS)) {
        CG_NOTE(BLEND_STRINGS,
                "Compiling blend string:\n%s\n",
                string);
    }

    do {
        if (c_ascii_isspace(*p))
            continue;

        if (*p == '\0') {
            switch (state) {
            case PARSER_STATE_EXPECT_DEST_CHANNELS:
                if (current_statement != 0)
                    goto finished;
                error_string = "Empty statement";
                goto error;
            case PARSER_STATE_SCRAPING_DEST_CHANNELS:
                error_string = "Expected an '=' following the destination "
                               "channel mask";
                goto error;
            case PARSER_STATE_EXPECT_FUNCTION_NAME:
                error_string = "Expected a function name";
                goto error;
            case PARSER_STATE_SCRAPING_FUNCTION_NAME:
                error_string = "Expected parenthesis after the function name";
                goto error;
            case PARSER_STATE_EXPECT_ARG_START:
                error_string = "Expected to find the start of an argument";
                goto error;
            case PARSER_STATE_EXPECT_STATEMENT_END:
                error_string = "Expected closing parenthesis for statement";
                goto error;
            }
        }

        switch (state) {
        case PARSER_STATE_EXPECT_DEST_CHANNELS:
            mark = p;
            state = PARSER_STATE_SCRAPING_DEST_CHANNELS;

        /* fall through */
        case PARSER_STATE_SCRAPING_DEST_CHANNELS:
            if (*p != '=')
                continue;
            if (strncmp(mark, "RGBA", 4) == 0)
                statement->mask = CG_BLEND_STRING_CHANNEL_MASK_RGBA;
            else if (strncmp(mark, "RGB", 3) == 0)
                statement->mask = CG_BLEND_STRING_CHANNEL_MASK_RGB;
            else if (strncmp(mark, "A", 1) == 0)
                statement->mask = CG_BLEND_STRING_CHANNEL_MASK_ALPHA;
            else {
                error_string = "Unknown destination channel mask; "
                               "expected RGBA=, RGB= or A=";
                goto error;
            }
            state = PARSER_STATE_EXPECT_FUNCTION_NAME;
            continue;

        case PARSER_STATE_EXPECT_FUNCTION_NAME:
            mark = p;
            state = PARSER_STATE_SCRAPING_FUNCTION_NAME;

        /* fall through */
        case PARSER_STATE_SCRAPING_FUNCTION_NAME:
            if (*p != '(') {
                if (!is_alphanum_char(*p)) {
                    error_string = "non alpha numeric character in function"
                                   "name";
                    goto error;
                }
                continue;
            }
            statement->function = get_function_info(mark, p);
            if (!statement->function) {
                error_string = "Unknown function name";
                goto error;
            }
            remaining_argc = statement->function->argc;
            current_arg = 0;
            state = PARSER_STATE_EXPECT_ARG_START;

        /* fall through */
        case PARSER_STATE_EXPECT_ARG_START:
            if (*p != '(' && *p != ',')
                continue;
            if (remaining_argc) {
                p++; /* parse_argument expects to see the first char of the arg
                      */
                if (!parse_argument(string,
                                    &p,
                                    statement,
                                    current_arg,
                                    &statement->args[current_arg],
                                    error))
                    return 0;
                current_arg++;
                remaining_argc--;
            }
            if (!remaining_argc)
                state = PARSER_STATE_EXPECT_STATEMENT_END;
            continue;

        case PARSER_STATE_EXPECT_STATEMENT_END:
            if (*p != ')') {
                error_string = "Expected end of statement";
                goto error;
            }
            state = PARSER_STATE_EXPECT_DEST_CHANNELS;
            if (current_statement++ == 1)
                goto finished;
            statement = &statements[current_statement];
        }
    } while (p++);

finished:

    if (CG_DEBUG_ENABLED(CG_DEBUG_BLEND_STRINGS)) {
        if (current_statement > 0)
            print_statement(0, &statements[0]);
        if (current_statement > 1)
            print_statement(1, &statements[1]);
    }

    if (!validate_statements(dev, statements, current_statement, error))
        return 0;

    return current_statement;

error: {
        int offset = p - string;
        _cg_set_error(error,
                      CG_BLEND_STRING_ERROR,
                      CG_BLEND_STRING_ERROR_PARSE_ERROR,
                      "Syntax error for string \"%s\" at offset %d: %s",
                      string,
                      offset,
                      error_string);

        if (CG_DEBUG_ENABLED(CG_DEBUG_BLEND_STRINGS)) {
            c_debug("Syntax error at offset %d: %s", offset, error_string);
        }
        return 0;
    }
}

TEST(blend_string_parsing)
{
    struct {
        const char *string;
        bool should_pass;
    } tests[] =
    {
      { "RGBA = ADD(SRC_COLOR*(SRC_COLOR[A]), "
              "DST_COLOR*(1-SRC_COLOR[A]))",
              true, },
      { "RGBA = ADD(SRC_COLOR,\nDST_COLOR*(0))",
              true, },
      { "RGBA = ADD(SRC_COLOR, 0)",
              true, },
      { "RGBA = ADD()",
              false, /* missing arguments */
      },
      { "RGBA = ADD(SRC_COLOR, DST_COLOR)",
              true, },
      { NULL } };
    int i;

    test_cg_init();

    cg_error_t *error = NULL;
    for (i = 0; tests[i].string; i++) {
        cg_blend_string_statement_t statements[2];
        _cg_blend_string_compile(
            test_dev, tests[i].string, statements, &error);
        if (tests[i].should_pass) {
            if (error) {
                c_debug("Unexpected parse error for string \"%s\"",
                        tests[i].string);
                c_assert_cmpstr("", ==, error->message);
            }
        } else {
            c_assert(error);
            cg_error_free(error);
            error = NULL;
        }
    }

    test_cg_fini();
}
