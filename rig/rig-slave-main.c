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

#ifdef USE_GSTREAMER
#include <cogl-gst/cogl-gst.h>
#endif

#include <rut.h>

#include "rig-slave.h"
#include "rig-engine.h"
#include "rig-avahi.h"
#include "rig-rpc-network.h"
#include "rig-pb.h"

#include "rig.pb-c.h"

static void
usage(void)
{
    fprintf(stderr, "Usage: rig-slave [OPTIONS]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "     --width=WIDTH     Width of slave window\n");
    fprintf(stderr, "     --height=HEIGHT   Height of slave window\n");
    fprintf(stderr, "  -s,--scale=SCALE     Device pixel scale factor\n");
    fprintf(stderr, "  -h,--help            Display this help message\n");
    exit(1);
}

int
main(int argc, char **argv)
{
    rig_slave_t *slave;
    int option_width = 0;
    int option_height = 0;
    double option_scale = 0;
    struct option opts[] = {
        { "width",   required_argument, NULL, 'W' },
        { "height",  required_argument, NULL, 'H' },
        { "scale",   required_argument, NULL, 's' },
        { "help",    no_argument,       NULL, 'h' },
        { 0,         0,                 NULL,  0  }
    };
    int c;

    rut_init_tls_state();

#ifdef USE_GSTREAMER
    gst_init(&argc, &argv);
#endif

    while ((c = getopt_long(argc, argv, "h", opts, NULL)) != -1) {
        switch(c) {
        case 'W':
            option_width = strtoul(optarg, NULL, 10);
            break;
        case 'H':
            option_height = strtoul(optarg, NULL, 10);
            break;
        case 's':
            option_scale = strtod(optarg, NULL);
            break;
        default:
            usage();
        }
    }

    slave = rig_slave_new(option_width, option_height, option_scale);

    rig_slave_run(slave);

    rut_object_unref(slave);

    return 0;
}
