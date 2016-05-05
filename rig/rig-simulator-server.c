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

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <sys/stat.h>

#include <uv.h>

#include <h2o.h>
#include <h2o/websocket.h>

#include <clib.h>

#include <rut.h>

#include "rig-simulator.h"
#include "rig-logs.h"

#ifdef USE_NCURSES
#include "rig-curses-debug.h"
#endif

static rig_simulator_t *simulator;

static void on_ws_message(h2o_websocket_conn_t *conn, const struct wslay_event_on_msg_recv_arg *arg)
{
    c_debug("on_ws_message\n");

    if (arg == NULL) {
        h2o_websocket_close(conn);
        return;
    }

    rig_pb_stream_websocket_message(simulator->stream, arg);

#if 0
    if (!wslay_is_ctrl_frame(arg->opcode)) {
        struct wslay_event_msg msgarg = {arg->opcode, arg->msg, arg->msg_length};
        wslay_event_queue_msg(conn->ws_ctx, &msgarg);
    }
#endif
}

static int on_req(h2o_handler_t *self, h2o_req_t *req)
{
    const char *client_key;
    ssize_t proto_header_index;
    h2o_websocket_conn_t *conn;

    c_debug("on_req\n");

    if (h2o_is_websocket_handshake(req, &client_key) != 0 || client_key == NULL) {
        return -1;
    }

    proto_header_index = h2o_find_header_by_str(&req->headers,
                                                "sec-websocket-protocol",
                                                strlen("sec-websocket-protocol"),
                                                SIZE_MAX);
    if (proto_header_index != -1) {
        c_debug("sec-websocket-protocols found\n");
        h2o_add_header_by_str(&req->pool, &req->res.headers,
                              "sec-websocket-protocol",
                              strlen("sec-websocket-protocol"),
                              0, "binary", strlen("binary"));
    }

    conn = h2o_upgrade_to_websocket(req, client_key, NULL, on_ws_message);

    rig_pb_stream_set_wslay_server_event_ctx(simulator->stream, conn->ws_ctx);

    return 0;
}

static h2o_globalconf_t config;
static h2o_context_t ctx;
static SSL_CTX *ssl_ctx;

static void on_connect(uv_stream_t *server, int status)
{
    uv_tcp_t *conn;
    h2o_socket_t *sock;

    c_debug("on_connect\n");

    if (status != 0)
        return;

    conn = h2o_mem_alloc(sizeof(*conn));
    uv_tcp_init(server->loop, conn);
    if (uv_accept(server, (uv_stream_t *)conn) != 0) {
        uv_close((uv_handle_t *)conn, (uv_close_cb)free);
        return;
    }

    sock = h2o_uv_socket_create((uv_stream_t *)conn, NULL, 0, (uv_close_cb)free);
    if (ssl_ctx != NULL)
        h2o_accept_ssl(&ctx, ctx.globalconf->hosts, sock, ssl_ctx);
    else
        h2o_http1_accept(&ctx, ctx.globalconf->hosts, sock);
}

static int setup_ssl(const char *cert_file, const char *key_file)
{
    SSL_load_error_strings();
    SSL_library_init();
    OpenSSL_add_all_algorithms();

    ssl_ctx = SSL_CTX_new(SSLv23_server_method());
    SSL_CTX_set_options(ssl_ctx, SSL_OP_NO_SSLv2);

    /* load certificate and private key */
    if (SSL_CTX_use_certificate_file(ssl_ctx, cert_file, SSL_FILETYPE_PEM) != 1) {
        c_error("an error occurred while trying to load server certificate file:%s\n", cert_file);
        return -1;
    }
    if (SSL_CTX_use_PrivateKey_file(ssl_ctx, key_file, SSL_FILETYPE_PEM) != 1) {
        c_error("an error occurred while trying to load private key file:%s\n", key_file);
        return -1;
    }

    return 0;
}

static void
usage(void)
{
    fprintf(stderr, "Usage: rig-simulator-server UI.rig [OPTIONS]...\n");
    fprintf(stderr, "  -h,--help                                Display this help message\n");
    exit(1);
}

int main(int argc, char **argv)
{
    rut_shell_t *shell;
    uv_loop_t *loop;
    uv_tcp_t listener;
    struct sockaddr_in sockaddr;
    h2o_hostconf_t *hostconf;
    h2o_pathconf_t *pathconf;
    char *ui_filename;
    char *dir;
    int r;

    struct option long_opts[] = {
        { "help",               no_argument,       NULL, 'h' },
        { 0,                    0,                 NULL,  0  }
    };
    const char *short_opts = "h";

    int c;

    rut_init();

    while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
        switch(c) {

        /* XXX: add opts. */

        default:
            usage();
        }
    }

    if (optind > argc || !argv[optind]) {
        fprintf(stderr, "Needs a UI.rig filename\n\n");
        usage();
    }

    ui_filename = argv[optind];

    dir = c_path_get_dirname(ui_filename);
    if (!dir)
        usage();

#if defined(RIG_ENABLE_DEBUG) && defined(USE_NCURSES)
    rig_curses_init();
#else
    rig_simulator_logs_init();
#endif

    simulator = rig_simulator_new(NULL);
    shell = simulator->shell;

    rig_simulator_queue_ui_load_on_connect(simulator, ui_filename);

#ifdef USE_NCURSES
    rig_curses_add_to_shell(shell);
#endif

    loop = rut_uv_shell_get_loop(shell);

    if ((r = uv_tcp_init(loop, &listener)) != 0) {
        c_error("uv_tcp_init:%s\n", uv_strerror(r));
        goto error;
    }
    uv_ip4_addr("0.0.0.1", 7890, &sockaddr);
    if ((r = uv_tcp_bind(&listener, (struct sockaddr *)&sockaddr, sizeof(sockaddr))) != 0) {
        c_error("uv_tcp_bind:%s\n", uv_strerror(r));
        goto error;
    }
    if ((r = uv_listen((uv_stream_t *)&listener, 128, on_connect)) != 0) {
        c_error("uv_listen:%s\n", uv_strerror(r));
        goto error;
    }


    h2o_config_init(&config);
    hostconf = h2o_config_register_host(&config, h2o_iovec_init(H2O_STRLIT("default")), 7890);
    pathconf = h2o_config_register_path(hostconf, "/simulator");
    h2o_create_handler(pathconf, sizeof(h2o_handler_t))->on_req = on_req;
    h2o_file_register(h2o_config_register_path(hostconf, "/"), dir, NULL, NULL, 0);

    h2o_context_init(&ctx, loop, &config);

    /* disabled by default: uncomment the block below to use HTTPS instead of HTTP */
    /*
    if (setup_ssl("server.crt", "server.key") != 0)
        goto Error;
    */

    rig_simulator_run(simulator);
    rut_object_unref(simulator);

    return 0;

error:
    return 1;
}
