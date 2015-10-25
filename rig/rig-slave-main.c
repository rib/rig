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
    fprintf(stderr, "  -W,--width=WIDTH                         Width of slave window\n");
    fprintf(stderr, "  -H,--height=HEIGHT                       Height of slave window\n");
    fprintf(stderr, "  -S,--scale=SCALE                         Device pixel scale factor\n");
    fprintf(stderr, "  -f,--fullscreen                          Run fullscreen\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -l,--listen={tcp:<address>[:port],       Specify how to listen for an editor connection\n");
    fprintf(stderr, "               abstract:<name>}            (listens on free tcp/ipv4 port by default)\n");
    fprintf(stderr, "\n");
#ifdef RIG_ENABLE_DEBUG
    fprintf(stderr, "  -m,--simulator={tcp:<address>[:port],    Specify how to listen for a simulator connection\n");
    fprintf(stderr, "                  abstract:<name>,         (Simulator runs in a separate thread by default)\n");
    fprintf(stderr, "                  mainloop,\n");
    fprintf(stderr, "                  thread,\n");
    fprintf(stderr, "                  process}\n");
#endif
    fprintf(stderr, "\n");
    fprintf(stderr, "  -h,--help                                Display this help message\n");
    exit(1);
}

int
main(int argc, char **argv)
{
    rig_slave_t *slave;
    int option_width = 0;
    int option_height = 0;
    double option_scale = 0;
    struct option long_opts[] = {
        { "width",      required_argument, NULL, 'W' },
        { "height",     required_argument, NULL, 'H' },
        { "scale",      required_argument, NULL, 'S' },
        { "fullscreen", no_argument,       NULL, 'f' },

        { "listen",     required_argument, NULL, 'l' },
#ifdef RIG_ENABLE_DEBUG
        { "simulator",  required_argument, NULL, 'm' },
#endif /* RIG_ENABLE_DEBUG */

        { "help",       no_argument,   NULL, 'h' },
        { 0,            0,             NULL,  0  }
    };

#ifdef RIG_ENABLE_DEBUG
    const char *short_opts = "W:H:S:fol:m:h";
#else
    const char *short_opts = "W:H:S:fol:h";
#endif

    int c;

    rut_init();

    rig_simulator_run_mode_option = RIG_SIMULATOR_RUN_MODE_THREADED;
    rig_slave_connect_mode_option = RIG_SLAVE_CONNECT_MODE_TCP;

    while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
        switch(c) {
        case 'W':
            option_width = strtoul(optarg, NULL, 10);
            break;
        case 'H':
            option_height = strtoul(optarg, NULL, 10);
            break;
        case 'S':
            option_scale = strtod(optarg, NULL);
            break;
        case 'f':
            rig_slave_fullscreen_option = true;
            break;

        case 'l': {
            char **strv = c_strsplit(optarg, ":", -1);

            if (strv[0]) {
                if (strcmp(strv[0], "tcp") == 0) {
                    char *address;
                    int port;

                    rig_slave_connect_mode_option = RIG_SLAVE_CONNECT_MODE_TCP;

                    if (!strv[1]) {
                        address = c_strdup("0.0.0.0");
                        port = 0;
                    } else {
                        address = c_strdup(strv[1]);
                        port = strv[2] ? strtoul(strv[2], NULL, 10) : 0;
                    }

                    rig_slave_address_option = address;
                    rig_slave_port_option = port;
                } else if (strcmp(strv[0], "abstract") == 0) {
#ifdef __linux__
                    rig_slave_connect_mode_option =
                        RIG_SLAVE_CONNECT_MODE_ABSTRACT_SOCKET;
                    if (strv[1])
                        rig_slave_abstract_socket_option = c_strdup(strv[1]);
                    else {
                        fprintf(stderr, "Missing abstract socket name in form \"abstract:my_socket_name\"\n");
                        usage();
                    }
#else
                    c_critical("Abstract sockets are only supported on Linux");
#endif
                } else {
                    fprintf(stderr, "Unsupported -l,--listen= mode \"%s\"\n", optarg);
                    usage();
                }
            } else
                usage();

            c_strfreev(strv);
            break;

        }
#ifdef RIG_ENABLE_DEBUG
        case 'm': {
            rig_simulator_parse_option(optarg, usage);
            break;
        }
#endif /* RIG_ENABLE_DEBUG */

        default:
            usage();
        }
    }

    slave = rig_slave_new(option_width, option_height, option_scale);

    rig_slave_run(slave);

    rut_object_unref(slave);

    return 0;
}
