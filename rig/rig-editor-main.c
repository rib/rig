/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2013,2014  Intel Corporation.
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
 */

#include <config.h>

#include <stdlib.h>
#include <getopt.h>
#include <clib.h>

#include <rut.h>
#ifdef USE_GSTREAMER
#include <cogl-gst/cogl-gst.h>
#endif

#include "rig-editor.h"

static void
usage(void)
{
    fprintf(stderr, "Usage: rig [UI.rig]\n");
    fprintf(stderr, "  -h,--help    Display this help message\n");
    exit(1);
}

int
main(int argc, char **argv)
{
    rig_editor_t *editor;
    struct option opts[] = {
        { "help",    no_argument,       NULL, 'h' },
        { 0,         0,                 NULL,  0  }
    };
    int c;

    rut_init_tls_state();

#ifdef USE_GSTREAMER
    gst_init(&argc, &argv);
#endif

    while ((c = getopt_long(argc, argv, "h", opts, NULL)) != -1) {
        /* only one option a.t.m... */
        usage();
    }

    if (optind > argc || !argv[optind]) {
        fprintf(stderr, "Needs a UI.rig filename\n\n");
        usage();
    }

    editor = rig_editor_new(argv[optind]);

    rig_editor_run(editor);

    rut_object_unref(editor);

    return 0;
}
