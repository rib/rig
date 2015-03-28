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
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <getopt.h>

#include <clib.h>

#include <rut.h>

#include "rig-simulator.h"
#include "rig-logs.h"

#ifdef __EMSCRIPTEN__

int
main(int argc, char **argv)
{
    rig_simulator_t *simulator;

    rig_simulator_logs_init();

    simulator = rig_simulator_new(RIG_FRONTEND_ID_DEVICE, NULL);

    rig_simulator_run(simulator);

    rut_object_unref(simulator);

    return 0;
}

#else /* __EMSCRIPTEN__ */

static void
usage(void)
{
    fprintf(stderr, "Usage: rig-simulator --frontend=[editor,device,slave] [OPTION]...\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -f,--frontend=[editor,device,slave]  The frontend that will be connected\n");
#ifdef __linux__
    fprintf(stderr, "  -a,--abstract-socket=NAME            Connect to named abstract socket\n");
#endif
    fprintf(stderr, "  -h,--help                            Display this help message\n");
    exit(1);
}

static bool
frontend_name_to_id(const char *frontend,
                    rig_frontend_id_t *id)
{
    bool ret = true;

    if (strcmp(frontend, "editor") == 0)
        *id = RIG_FRONTEND_ID_EDITOR;
    else if (strcmp(frontend, "slave") == 0)
        *id = RIG_FRONTEND_ID_SLAVE;
    else if (strcmp(frontend, "device") == 0)
        *id = RIG_FRONTEND_ID_DEVICE;
    else
        ret = false;

    return ret;
}

int
main(int argc, char **argv)
{
    rig_simulator_t *simulator;
    const char *ipc_fd_str = getenv("_RIG_IPC_FD");
    const char *frontend = getenv("_RIG_FRONTEND");
#ifdef __linux__
    const char *abstract_socket = NULL;
#endif
    rig_frontend_id_t frontend_id;
    int fd;
    struct option opts[] = {
        { "frontend",           required_argument, NULL, 'f' },
#ifdef __linux__
        { "abstract-socket",    required_argument, NULL, 'a' },
#endif
        { "help",               no_argument,       NULL, 'h' },
        { 0,                    0,                 NULL,  0  }
    };
    int c;

    rut_init_tls_state();

    if (frontend) {
        if (!frontend_name_to_id(frontend, &frontend_id)) {
            c_error("Failed to determine frontend via _RIG_FRONTEND "
                    "environment variable");
            return EXIT_FAILURE;
        }
    }

    while ((c = getopt_long(argc, argv, "f:a:h", opts, NULL)) != -1) {

        if (ipc_fd_str) {
            c_warning("Ignoring private _RIG_IPC_FD environment variable while running interactively");
            ipc_fd_str = NULL;
        }

        switch(c) {
        case 'f':
            if (frontend)
                c_warning("Overriding _RIG_FRONTEND environment variable while running interactively");

            frontend = optarg;

            if (!frontend_name_to_id(frontend, &frontend_id)) {
                c_warning("Unknown frontend \"%s\"", frontend);
                usage();
            }

            break;
#ifdef __linux__
        case 'a':
            abstract_socket = optarg;
            break;
#endif
        default:
            usage();
        }
    }

    if (!frontend) {
        c_warning("The frontend type must be specified with --frontend=[editor,device,slave]");
        usage();
    }

#ifdef __linux__
    if (abstract_socket) {
        while ((fd = rut_os_connect_to_abstract_socket(abstract_socket)) == -1) {
            static bool seen = false;
            if (seen)
                c_message("Waiting for frontend...");
            else
                seen = true;

            sleep(2);
        }
    } else
#endif /* linux */
    {
#if defined(__linux__) || defined(__APPLE__)
        /* Block SIGINT so that when we are interactively debugging the
         * frontend process with gdb, we don't kill the simulator
         * whenever we interupt the frontend process.
         */
        sigset_t set;

        sigemptyset(&set);
        sigaddset(&set, SIGINT);

        /* Block SIGINT from terminating the simulator, otherwise it's a
         * pain to debug the frontend in gdb because when we press Ctrl-C to
         * interrupt the frontend, gdb only blocks SIGINT from being passed
         * to the frontend and so we end up terminating the simulator.
         */
        pthread_sigmask(SIG_BLOCK, &set, NULL);
#endif

        if (!ipc_fd_str) {
            c_error("Failed to find ipc file descriptor via _RIG_IPC_FD "
                    "environment variable");
            return EXIT_FAILURE;
        }

        fd = strtol(ipc_fd_str, NULL, 10);
    }

    rig_simulator_logs_init();

    simulator = rig_simulator_new(frontend_id, NULL);
    rig_simulator_set_frontend_fd(simulator, fd);

    rig_simulator_run(simulator);

    rut_object_unref(simulator);

    return 0;
}

#endif /* __EMSCRIPTEN__ */
