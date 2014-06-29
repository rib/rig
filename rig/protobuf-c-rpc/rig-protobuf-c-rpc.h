#ifndef __PROTOBUF_C_RPC_H_
#define __PROTOBUF_C_RPC_H_

#include <stdbool.h>

/* Protocol is:
 *    client issues request with header:
 *         method_index              32-bit little-endian
 *         message_length            32-bit little-endian
 *         request_id                32-bit any-endian
 *    server responds with header:
 *         method_index              32-bit little-endian
 *         message_length            32-bit little-endian
 *         request_id                32-bit any-endian
 */
#include "rig-protobuf-c-dispatch.h"

typedef enum {
    PROTOBUF_C_RPC_ADDRESS_LOCAL, /* unix-domain socket */
    PROTOBUF_C_RPC_ADDRESS_TCP /* host/port tcp socket */
} PB_RPC_AddressType;

typedef enum {
    PB_RPC_ERROR_CODE_HOST_NOT_FOUND,
    PB_RPC_ERROR_CODE_CONNECTION_REFUSED,
    PB_RPC_ERROR_CODE_CONNECTION_FAILED,
    PB_RPC_ERROR_CODE_IO_ERROR,
    PB_RPC_ERROR_CODE_CLIENT_TERMINATED,
    PB_RPC_ERROR_CODE_BAD_REQUEST,
    PB_RPC_ERROR_CODE_PROXY_PROBLEM,
    PB_RPC_ERROR_CODE_UNPACK_ERROR
} PB_RPC_Error_Code;

typedef enum {
    PB_RPC_STATUS_CODE_SUCCESS,
    PB_RPC_STATUS_CODE_SERVICE_FAILED,
    PB_RPC_STATUS_CODE_TOO_MANY_PENDING
} PB_RPC_Status_Code;

typedef void (*PB_RPC_Error_Func)(PB_RPC_Error_Code code,
                                  const char *message,
                                  void *error_func_data);

/* --- Client API --- */
typedef struct _pb_rpc__client_t pb_rpc__client_t;

pb_rpc__client_t *
rig_pb_rpc_client_new(PB_RPC_AddressType type,
                      const char *name,
                      const ProtobufCServiceDescriptor *descriptor,
                      rig_protobuf_c_dispatch_t *dispatch);

ProtobufCService *rig_pb_rpc_client_get_service(pb_rpc__client_t *client);

/* --- configuring the client */

/* Pluginable async dns hooks */
/* TODO: use adns library or port evdns? ugh */
typedef void (*ProtobufC_NameLookup_Found)(const uint8_t *address,
                                           void *callback_data);
typedef void (*ProtobufC_NameLookup_Failed)(const char *error_message,
                                            void *callback_data);
typedef void (*ProtobufC_NameLookup_Func)(
    rig_protobuf_c_dispatch_t *dispatch,
    const char *name,
    ProtobufC_NameLookup_Found found_func,
    ProtobufC_NameLookup_Failed failed_func,
    void *callback_data);
void rig_pb_rpc_client_set_name_resolver(pb_rpc__client_t *client,
                                         ProtobufC_NameLookup_Func resolver);

/* Error handling */
void rig_pb_rpc_client_set_error_handler(pb_rpc__client_t *client,
                                         PB_RPC_Error_Func func,
                                         void *error_func_data);

typedef void (*PB_RPC_Connect_Func)(pb_rpc__client_t *client,
                                    void *callback_data);

void rig_pb_rpc_client_set_connect_handler(pb_rpc__client_t *client,
                                           PB_RPC_Connect_Func func,
                                           void *callback_data);

/* Configuring the autoreconnect behavior.
   If the client is disconnected, all pending requests get an error.
   If autoreconnect is set, and it is by default, try connecting again
   after a certain amount of time has elapsed. */
void rig_pb_rpc_client_disable_autoreconnect(pb_rpc__client_t *client);

void rig_pb_rpc_client_set_autoreconnect_period(pb_rpc__client_t *client,
                                                unsigned millis);

/* checking the state of the client */
bool rig_pb_rpc_client_is_connected(pb_rpc__client_t *client);

/* NOTE: we don't actually start connecting til the main-loop runs,
   so you may configure the client immediately after creation */

/* --- Server API --- */
typedef struct _pb_rpc__server_t pb_rpc__server_t;
pb_rpc__server_t *rig_pb_rpc_server_new(const char *bind_name,
                                        ProtobufC_FD listening_fd,
                                        ProtobufCService *service,
                                        rig_protobuf_c_dispatch_t *dispatch);

/* May return -1 if not listening */
int rig_pb_rpc_server_get_listening_fd(pb_rpc__server_t *server);

typedef struct _pb_rpc__server_connection_t pb_rpc__server_connection_t;

typedef void (*pb_rpc__client_t_Connect_Func)(pb_rpc__server_t *server,
                                              pb_rpc__server_connection_t *conn,
                                              void *user_data);

void rig_pb_rpc_server_set_client_connect_handler(
    pb_rpc__server_t *server,
    pb_rpc__client_t_Connect_Func callback,
    void *user_data);

typedef void (*pb_rpc__client_t_Close_Func)(pb_rpc__server_t *server,
                                            pb_rpc__server_connection_t *conn,
                                            void *user_data);

void
rig_pb_rpc_server_set_client_close_handler(pb_rpc__server_t *server,
                                           pb_rpc__client_t_Close_Func callback,
                                           void *user_data);

typedef void (*pb_rpc__server_connection_t_Close_Func)(
    pb_rpc__server_connection_t *conn, void *user_data);

void rig_pb_rpc_server_connection_set_close_handler(
    pb_rpc__server_connection_t *conn,
    pb_rpc__server_connection_t_Close_Func func,
    void *user_data);

typedef void (*pb_rpc__server_connection_t_Error_Func)(
    pb_rpc__server_connection_t *conn,
    PB_RPC_Error_Code error,
    const char *message,
    void *user_data);

void rig_pb_rpc_server_connection_set_error_handler(
    pb_rpc__server_connection_t *conn,
    pb_rpc__server_connection_t_Error_Func func,
    void *user_data);
void rig_pb_rpc_server_connection_set_data(pb_rpc__server_connection_t *conn,
                                           void *user_data);

/* NOTE: these do not have guaranteed semantics if called after there are
   actually
   clients connected to the server!
   NOTE 2:  The purist in me has left the default of no-autotimeout.
   The pragmatist in me knows thats going to be a pain for someone.
   Please set autotimeout, and if you really don't want it, disable it
   explicitly,
   because i might just go and make it the default! */
void rig_pb_rpc_server_disable_autotimeout(pb_rpc__server_t *server);

void rig_pb_rpc_server_set_autotimeout(pb_rpc__server_t *server,
                                       unsigned timeout_millis);

typedef bool (*PB_RPC_IsRpcThreadFunc)(pb_rpc__server_t *server,
                                       rig_protobuf_c_dispatch_t *dispatch,
                                       void *is_rpc_data);

void rig_pb_rpc_server_configure_threading(pb_rpc__server_t *server,
                                           PB_RPC_IsRpcThreadFunc func,
                                           void *is_rpc_data);

/* Error handling */
void rig_pb_rpc_server_set_error_handler(pb_rpc__server_t *server,
                                         PB_RPC_Error_Func func,
                                         void *error_func_data);

/* XXX: this is quite hacky since it's not type safe, but for now
 * this avoids up importing protoc-c into rig so that we can
 * change the prototype of rpc service functions.
 */
void *rig_pb_rpc_closure_get_connection_data(void *closure_data);

/* --- Peer API --- */

typedef struct _pb_rpc__peer_t pb_rpc__peer_t;

pb_rpc__peer_t *
rig_pb_rpc_peer_new(int fd,
                    ProtobufCService *server_service,
                    const ProtobufCServiceDescriptor *client_descriptor,
                    rig_protobuf_c_dispatch_t *dispatch);

pb_rpc__server_t *rig_pb_rpc_peer_get_server(pb_rpc__peer_t *peer);

/* The return value (the client) may be cast to ProtobufCService * */
pb_rpc__client_t *rig_pb_rpc_peer_get_client(pb_rpc__peer_t *peer);

#endif
