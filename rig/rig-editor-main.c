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

#include <rig-config.h>

#include <stdlib.h>
#include <getopt.h>
#include <clib.h>

#include <rut.h>

#include "rig-editor.h"
#include "rig-curses-debug.h"

static void
usage(void)
{
    fprintf(stderr, "Usage: rig [UI.rig]\n");
    fprintf(stderr, "\n");

    fprintf(stderr, "  -s,--slave={tcp:<hostname>[:port],       Connect to specified slave device\n");
    fprintf(stderr, "              abstract:<name>}\n");
    fprintf(stderr, "E.g:\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  --slave=tcp:<ip>[:<port>]                Connect to a slave device via tcp \n");
    fprintf(stderr, "  --slave=\"abstract:my_slave\"            Connection to a slave device via an abstract socket\n");
    fprintf(stderr, "\n");

#ifdef RIG_ENABLE_DEBUG
    fprintf(stderr, "  -m,--simulator={tcp:<address>[:port],    Specify how to listen for a simulator connection\n");
    fprintf(stderr, "                  abstract:<name>,         (Simulator runs in a separate process by default)\n");
    fprintf(stderr, "                  mainloop,\n");
    fprintf(stderr, "                  thread,\n");
    fprintf(stderr, "                  process}\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -d,--disable-curses                      Disable curses debug console\n");
    fprintf(stderr, "\n");
#endif

    fprintf(stderr, "  -h,--help                                Display this help message\n");
    exit(1);
}

int
main(int argc, char **argv)
{
    rig_editor_t *editor;
    struct option long_opts[] = {

#ifdef RIG_ENABLE_DEBUG
        { "simulator",      required_argument, NULL, 'm' },
        { "disable-curses", no_argument,       NULL, 'd' },
#endif
        { "slave",          required_argument, NULL, 's' },

        { "help",           no_argument,       NULL, 'h' },
        { 0,                0,                 NULL,  0  }
    };

#ifdef RIG_ENABLE_DEBUG
    const char *short_opts = "m:ds:h";
    bool enable_curses_debug = true;
#else
    const char *short_opts = "s:h";
#endif

    int c;

    rut_init();

    rig_simulator_run_mode_option = RIG_SIMULATOR_RUN_MODE_PROCESS;

    while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
        switch(c) {
#ifdef RIG_ENABLE_DEBUG
        case 'm': {
            enum rig_simulator_run_mode mode;
            char *address;
            int port;

            rig_simulator_parse_run_mode(optarg,
                                         usage,
                                         false, /* integrated process (i.e. sim
                                                   is started by frontend) */
                                         true, /* listen */
                                         &mode,
                                         &address,
                                         &port);
            break;
        }
        case 'd':
            enable_curses_debug = false;
            break;
#endif
        case 's':
            rig_editor_slave_address_options =
                c_llist_prepend(rig_editor_slave_address_options, optarg);
            break;
        default:
            usage();
        }
    }

    if (optind > argc || !argv[optind]) {
        fprintf(stderr, "Needs a UI.rig filename\n\n");
        usage();
    }

#ifdef RIG_ENABLE_DEBUG
    if (enable_curses_debug)
        rig_curses_init();
#endif

    editor = rig_editor_new(argv[optind]);

    rig_editor_run(editor);

    rut_object_unref(editor);

    return 0;
}
