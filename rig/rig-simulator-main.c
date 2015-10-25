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

    simulator = rig_simulator_new(NULL);

    rig_simulator_run(simulator);

    rut_object_unref(simulator);

    return 0;
}

#else /* __EMSCRIPTEN__ */

static void
usage(void)
{
    fprintf(stderr, "Usage: rig-simulator UI.rig [OPTION]...\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -f,--frontend={tcp:<address>[:port],     Specify how to connect to frontend\n");
    fprintf(stderr, "                 abstract:<name>}\n");
    fprintf(stderr, "  -l,--listen={tcp:<address>[:port],       Specify how to listen for a frontend\n");
    fprintf(stderr, "               abstract:<name>}\n");
    fprintf(stderr, "  -h,--help                                Display this help message\n");
    exit(1);
}

#ifdef USE_UV
static void
handle_frontend_tcp_connect_cb(uv_stream_t *server, int status)
{
    rig_frontend_t *frontend = server->data;
    rig_pb_stream_t *stream;

    if (status != 0) {
        c_warning("Connection failure: %s", uv_strerror(status));
        return;
    }

    c_message("Simulator tcp connect request received!");

    if (frontend->connected) {
        c_warning("Ignoring simulator connection while there's already one connected");
        return;
    }

    stream = rig_pb_stream_new(frontend->engine->shell);
    rig_pb_stream_accept_tcp_connection(stream, &frontend->listening_socket);

    c_message("Simulator connected!");

    /* frontend_start_service will take ownership of the stream */
    rut_object_unref(stream);
}

static void
bind_to_tcp_socket(rig_simulator_t *simulator,
                   const char *address,
                   int port)
{
    rut_shell_t *shell = simulator->shell;
    uv_loop_t *loop = rut_uv_shell_get_loop(shell);
    struct sockaddr_in bind_addr;
    struct sockaddr name;
    int namelen;
    int err;

    uv_tcp_init(loop, &simulator->listening_socket);
    simulator->listening_socket.data = simulator;

    uv_ip4_addr(address, port, &bind_addr);
    uv_tcp_bind(&simulator->listening_socket, (struct sockaddr *)&bind_addr, 0);
    err = uv_listen((uv_stream_t*)&simulator->listening_socket,
                    128, handle_frontend_tcp_connect_cb);
    if (err < 0) {
        c_critical("Failed to starting listening for simulator connection: %s",
                   uv_strerror(err));
        return;
    }

    err = uv_tcp_getsockname(&simulator->listening_socket, &name, &namelen);
    if (err != 0) {
        c_critical("Failed to query peer address of listening tcp socket");
        return;
    } else {
        struct sockaddr_in *addr = (struct sockaddr_in *)&name;
        char ip_address[17] = {'\0'};

        c_return_if_fail(name.sa_family == AF_INET);

        uv_ip4_name(addr, ip_address, 16);
        simulator->listening_address = c_strdup(ip_address);
        simulator->listening_port = ntohs(addr->sin_port);
    }
}
#endif /* USE_UV */

#ifdef __linux__
static void
handle_frontend_abstract_connect_cb(void *user_data, int listen_fd, int revents)
{
    rig_simulator_t *simulator = user_data;
    struct sockaddr addr;
    socklen_t addr_len = sizeof(addr);
    int fd;

    c_return_if_fail(revents & RUT_POLL_FD_EVENT_IN);

    c_message("Frontend connect request received!");

    if (simulator->connected) {
        c_warning("Ignoring frontend connection while there's already one connected");
        return;
    }

    fd = accept(simulator->listen_fd, &addr, &addr_len);
    if (fd != -1) {
        rig_pb_stream_set_fd_transport(simulator->stream, fd);

        c_message("Frontend connected!");
    } else
        c_message("Failed to accept frontend connection: %s!",
                  strerror(errno));
}

static bool
bind_to_abstract_socket(rig_simulator_t *simulator,
                        const char *address)
{
    rut_exception_t *catch = NULL;
    int fd = rut_os_listen_on_abstract_socket(address, &catch);

    if (fd < 0) {
        c_critical("Failed to listen on abstract \"rig-simulator\" socket: %s",
                   catch->message);
        return false;
    }

    simulator->listen_fd = fd;

    rut_poll_shell_add_fd(simulator->shell,
                          simulator->listen_fd,
                          RUT_POLL_FD_EVENT_IN,
                          NULL, /* prepare */
                          handle_frontend_abstract_connect_cb, /* dispatch */
                          simulator);

    c_message("Waiting for simulator to connect to abstract socket \"%s\"...",
              address);

    return true;
}
#endif /* linux */

int
main(int argc, char **argv)
{
    rig_simulator_t *simulator;
    const char *ipc_fd_str = getenv("_RIG_IPC_FD");
    int fd;
    struct option opts[] = {
        { "frontend",   required_argument, NULL, 'f' },
        { "listen",     required_argument, NULL, 'l' },
        { "help",       no_argument,       NULL, 'h' },
        { 0,            0,                 NULL,  0  }
    };
    int c;
    char *ui_filename = NULL;
    enum rig_simulator_run_mode mode;
    char *address;
    int port;

    rut_init();

    mode = RIG_SIMULATOR_RUN_MODE_PROCESS;

    while ((c = getopt_long(argc, argv, "f:l:h", opts, NULL)) != -1) {

        if (ipc_fd_str) {
            c_warning("Ignoring private _RIG_IPC_FD environment variable while running interactively");
            ipc_fd_str = NULL;
        }

        switch(c) {
        case 'f':
            rig_simulator_parse_run_mode(optarg,
                                         usage,
                                         RIG_SIMULATOR_STANDALONE,
                                         &mode,
                                         &address,
                                         &port);
            break;
        case 'l':
            rig_simulator_parse_run_mode(optarg,
                                         usage,
                                         RIG_SIMULATOR_STANDALONE | RIG_SIMULATOR_LISTEN,
                                         &mode,
                                         &address,
                                         &port);
            break;

        default:
            usage();
        }
    }


    if (optind <= argc && argv[optind])
        ui_filename = argv[optind];

    rig_simulator_logs_init();

    simulator = rig_simulator_new(NULL);

    if (ui_filename) {
        char *assets_location = c_path_get_dirname(ui_filename);

        rut_shell_set_assets_location(simulator->shell, assets_location);

        c_free(assets_location);
    }

    rig_simulator_queue_ui_load_on_connect(simulator, ui_filename);

    switch (mode) {
#ifdef USE_UV
    case RIG_SIMULATOR_RUN_MODE_LISTEN_TCP:
        bind_to_tcp_socket(simulator, address, port);
        break;
    case RIG_SIMULATOR_RUN_MODE_CONNECT_TCP:
        c_error("TODO: support connecting from a simulator to a frontend via tcp");
        break;
#endif
#ifdef __linux__
    case RIG_SIMULATOR_RUN_MODE_LISTEN_ABSTRACT_SOCKET:
        bind_to_abstract_socket(simulator, address);
        break;
    case RIG_SIMULATOR_RUN_MODE_CONNECT_ABSTRACT_SOCKET:
        while ((fd = rut_os_connect_to_abstract_socket(address)) == -1) {
            static bool seen = false;
            if (seen)
                c_message("Waiting for frontend...");
            else
                seen = true;

            sleep(2);
        }

        rig_simulator_set_frontend_fd(simulator, fd);
        break;
#endif /* linux */

    case RIG_SIMULATOR_RUN_MODE_PROCESS:
#if defined(__linux__) || defined(__APPLE__)
        {
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
        }
#endif

        if (!ipc_fd_str) {
            c_error("Failed to find ipc file descriptor via _RIG_IPC_FD "
                    "environment variable");
            return EXIT_FAILURE;
        }

        fd = strtol(ipc_fd_str, NULL, 10);
        rig_simulator_set_frontend_fd(simulator, fd);

        break;

    case RIG_SIMULATOR_RUN_MODE_MAINLOOP:
    case RIG_SIMULATOR_RUN_MODE_THREADED:
        /* This modes are only possible if the simulator is run
         * in the same process as the frontend so don't make
         * sense for a standalone simulator binary */
        c_assert_not_reached();
    }

    rig_simulator_run(simulator);

    rut_object_unref(simulator);

    return 0;
}

#endif /* __EMSCRIPTEN__ */
