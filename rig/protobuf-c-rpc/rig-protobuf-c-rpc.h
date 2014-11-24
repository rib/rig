#ifndef __PROTOBUF_C_RPC_H_
#define __PROTOBUF_C_RPC_H_

#include <stdbool.h>

#include <protobuf-c/protobuf-c.h>

/* Forward declarations, since they are used in -c-stream.h
 * which has a circular dependency a.t.m
 */
typedef struct _rig_pb_rpc_server_connection_t rig_pb_rpc_server_connection_t;
typedef struct _rig_pb_rpc_client_t rig_pb_rpc_client_t;

#include "rig-protobuf-c-stream.h"

#define RIG_PB_RPC_SERVER_ERROR rig_pb_rpc_server_error_quark()
#define RIG_PB_RPC_CLIENT_ERROR rig_pb_rpc_client_error_quark()

typedef enum {
    RIG_PB_RPC_ERROR_CODE_CONNECTION_FAILED,
    RIG_PB_RPC_ERROR_CODE_IO_ERROR,
    RIG_PB_RPC_ERROR_CODE_BAD_REQUEST,
    RIG_PB_RPC_ERROR_CODE_UNPACK_ERROR
} rig_pb_rpc_error_code_t;

typedef void (*rig_pb_rpc_error_func_t)(rig_pb_rpc_error_code_t code,
                                        const char *message,
                                        void *error_func_data);

/* --- Client API --- */
ProtobufCService *rig_pb_rpc_client_get_service(rig_pb_rpc_client_t *client);

/* Error handling */
void rig_pb_rpc_client_set_error_handler(rig_pb_rpc_client_t *client,
                                         rig_pb_rpc_error_func_t func,
                                         void *error_func_data);

typedef void (*rig_pb_rpc_connect_func_t)(rig_pb_rpc_client_t *client,
                                    void *callback_data);

void rig_pb_rpc_client_set_connect_handler(rig_pb_rpc_client_t *client,
                                           rig_pb_rpc_connect_func_t func,
                                           void *callback_data);

/* checking the state of the client */
bool rig_pb_rpc_client_is_connected(rig_pb_rpc_client_t *client);


/* --- Server API --- */
typedef struct _rig_pb_rpc_server_t rig_pb_rpc_server_t;

typedef void (*rig_pb_rpc_client_t_Connect_Func)(rig_pb_rpc_server_t *server,
                                                 rig_pb_rpc_server_connection_t *conn,
                                                 void *user_data);

void rig_pb_rpc_server_set_client_connect_handler(
    rig_pb_rpc_server_t *server,
    rig_pb_rpc_client_t_Connect_Func callback,
    void *user_data);

typedef void (*rig_pb_rpc_client_t_Close_Func)(rig_pb_rpc_server_t *server,
                                            rig_pb_rpc_server_connection_t *conn,
                                            void *user_data);

void
rig_pb_rpc_server_set_client_close_handler(rig_pb_rpc_server_t *server,
                                           rig_pb_rpc_client_t_Close_Func callback,
                                           void *user_data);

typedef void (*rig_pb_rpc_server_connection_t_Close_Func)(
    rig_pb_rpc_server_connection_t *conn, void *user_data);

void rig_pb_rpc_server_connection_set_close_handler(
    rig_pb_rpc_server_connection_t *conn,
    rig_pb_rpc_server_connection_t_Close_Func func,
    void *user_data);

typedef void (*rig_pb_rpc_server_connection_t_Error_Func)(
    rig_pb_rpc_server_connection_t *conn,
    rig_pb_rpc_error_code_t error,
    const char *message,
    void *user_data);

void rig_pb_rpc_server_connection_set_error_handler(
    rig_pb_rpc_server_connection_t *conn,
    rig_pb_rpc_server_connection_t_Error_Func func,
    void *user_data);
void rig_pb_rpc_server_connection_set_data(rig_pb_rpc_server_connection_t *conn,
                                           void *user_data);

/* Error handling */
void rig_pb_rpc_server_set_error_handler(rig_pb_rpc_server_t *server,
                                         rig_pb_rpc_error_func_t func,
                                         void *error_func_data);

/* XXX: this is quite hacky since it's not type safe, but for now
 * this avoids up importing protoc-c into rig so that we can
 * change the prototype of rpc service functions.
 */
void *rig_pb_rpc_closure_get_connection_data(void *closure_data);

/* --- Peer API --- */

typedef struct _rig_pb_rpc_peer_t rig_pb_rpc_peer_t;

rig_pb_rpc_peer_t *
rig_pb_rpc_peer_new(rig_pb_stream_t *stream,
                    ProtobufCService *server_service,
                    const ProtobufCServiceDescriptor *client_descriptor);

rig_pb_stream_t *rig_pb_rpc_peer_get_stream(rig_pb_rpc_peer_t *peer);

rig_pb_rpc_server_t *rig_pb_rpc_peer_get_server(rig_pb_rpc_peer_t *peer);

/* The return value (the client) may be cast to ProtobufCService * */
rig_pb_rpc_client_t *rig_pb_rpc_peer_get_client(rig_pb_rpc_peer_t *peer);

#endif
