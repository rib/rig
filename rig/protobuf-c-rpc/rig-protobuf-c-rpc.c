/* KNOWN DEFECTS:
    - server does not obey max_pending_requests_per_connection
    - no ipv6 support
 */
#include <config.h>

#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib.h>
#include "rig-protobuf-c-rpc.h"
#include "rig-protobuf-c-data-buffer.h"
#include "gsklistmacros.h"

#include <rut.h>

/* enabled for efficiency, can be useful to disable for debugging */
#define RECYCLE_REQUESTS 1

#define MAX_FAILED_MSG_LENGTH   512

typedef enum
{
  PB_RPC_CLIENT_STATE_INIT,
  PB_RPC_CLIENT_STATE_NAME_LOOKUP,
  PB_RPC_CLIENT_STATE_CONNECTING,
  PB_RPC_CLIENT_STATE_CONNECTED,
  PB_RPC_CLIENT_STATE_FAILED_WAITING,
  PB_RPC_CLIENT_STATE_FAILED,               /* if no autoreconnect */
  PB_RPC_CLIENT_STATE_DESTROYED
} PB_RPC_ClientState;

typedef struct _Closure Closure;
struct _Closure
{
  /* these will be NULL for unallocated request ids */
  const ProtobufCMessageDescriptor *response_type;
  ProtobufCClosure closure;

  /* this is the next request id, or 0 for none */
  void *closure_data;
};

typedef struct _Stream
{
  RutObjectBase _base;


  RigProtobufCDispatch *dispatch;

  int fd;

  ProtobufCDataBuffer incoming;
  ProtobufCDataBuffer outgoing;

  PB_RPC_ServerConnection *conn;
  PB_RPC_Client *client;

} Stream;

struct _PB_RPC_Client
{
  RutObjectBase _base;


  ProtobufCService service;
  Stream *stream;
  ProtobufCAllocator *allocator;
  RigProtobufCDispatch *dispatch;
  PB_RPC_AddressType address_type;
  char *name;

  bool autoreconnect;
  unsigned autoreconnect_millis;
  ProtobufC_NameLookup_Func resolver;
  PB_RPC_Error_Func error_handler;
  void *error_handler_data;
  PB_RPC_Connect_Func connect_handler;
  void *connect_handler_data;
  PB_RPC_ClientState state;
  union {
    struct {
      RigProtobufCDispatchIdle *idle;
    } init;
    struct {
      uint16_t port;
    } name_lookup;
    struct {
      unsigned closures_alloced;
      unsigned first_free_request_id;
      /* indexed by (request_id-1) */
      Closure *closures;
    } connected;
    struct {
      RigProtobufCDispatchTimer *timer;
      char *error_message;
    } failed_waiting;
    struct {
      char *error_message;
    } failed;
  } info;
};

/* === Server === */

typedef struct _ServerRequest ServerRequest;
struct _ServerRequest
{
  uint32_t request_id;                  /* in little-endian */
  uint32_t method_index;                /* in native-endian */
  PB_RPC_ServerConnection *conn;
  PB_RPC_Server *server;
  ProtobufCMessage *message;
  union {
    /* if conn != NULL, then the request is alive: */
    struct { ServerRequest *prev, *next; } alive;

    /* if conn == NULL, then the request is defunct: */
    struct { ProtobufCAllocator *allocator; } defunct;

    /* well, if it is in the recycled list, then it's recycled :/ */
    struct { ServerRequest *next; } recycled;
  } info;
};

struct _PB_RPC_ServerConnection
{
  Stream *stream;

  PB_RPC_Server *server;
  PB_RPC_ServerConnection *prev, *next;

  unsigned n_pending_requests;
  ServerRequest *first_pending_request, *last_pending_request;

  PB_RPC_ServerConnection_Close_Func close_handler;
  void *close_handler_data;

  PB_RPC_ServerConnection_Error_Func error_handler;
  void *error_handler_data;

  void *user_data;
};


/* When we get a response in the wrong thread,
   we proxy it over a system pipe.  Actually, we allocate one
   of these structures and pass the pointer over the pipe.  */
typedef struct _ProxyResponse ProxyResponse;
struct _ProxyResponse
{
  ServerRequest *request;
  unsigned len;
  /* data follows the structure */
};

struct _PB_RPC_Server
{
  RutObjectBase _base;


  RigProtobufCDispatch *dispatch;
  ProtobufCAllocator *allocator;
  ProtobufCService *service;
  PB_RPC_AddressType address_type;
  char *bind_name;
  PB_RPC_ServerConnection *first_connection, *last_connection;
  ProtobufC_FD listening_fd;

  ServerRequest *recycled_requests;

  /* multithreading support */
  PB_RPC_IsRpcThreadFunc is_rpc_thread_func;
  void * is_rpc_thread_data;
  int proxy_pipe[2];
  unsigned proxy_extra_data_len;
  uint8_t proxy_extra_data[sizeof (void*)];

  PB_RPC_Error_Func error_handler;
  void *error_handler_data;

  PB_RPC_Client_Connect_Func client_connect_handler;
  void *client_connect_handler_data;

  PB_RPC_Client_Close_Func client_close_handler;
  void *client_close_handler_data;

  /* configuration */
  unsigned max_pending_requests_per_connection;
};

struct _PB_RPC_Peer
{
  RutObjectBase _base;


  Stream *stream;

  PB_RPC_Server *server;
  PB_RPC_Client *client;

  RigProtobufCDispatchIdle *idle;

};

static void begin_name_lookup (PB_RPC_Client *client);

static void handle_stream_fd_events (int fd,
                                     unsigned events,
                                     void *data);

static uint32_t
uint32_to_le (uint32_t le)
{
#if IS_LITTLE_ENDIAN
  return le;
#else
  return (le << 24) | (le >> 24)
       | ((le >> 8) & 0xff00)
       | ((le << 8) & 0xff0000);
#endif
}
#define uint32_from_le uint32_to_le /* make the code more readable, i guess */

static void
_rig_pb_rpc_client_free (void *object)
{
  PB_RPC_Client *client = object;
  PB_RPC_ClientState state = client->state;
  int i;
  int n_closures = 0;
  Closure *closures = NULL;

  switch (state)
    {
    case PB_RPC_CLIENT_STATE_INIT:
      if (client->info.init.idle)
        rig_protobuf_c_dispatch_remove_idle (client->info.init.idle);
      break;
    case PB_RPC_CLIENT_STATE_NAME_LOOKUP:
      break;
    case PB_RPC_CLIENT_STATE_CONNECTING:
      break;
    case PB_RPC_CLIENT_STATE_CONNECTED:
      n_closures = client->info.connected.closures_alloced;
      closures = client->info.connected.closures;
      break;
    case PB_RPC_CLIENT_STATE_FAILED_WAITING:
      rig_protobuf_c_dispatch_remove_timer (client->info.failed_waiting.timer);
      client->allocator->free (client->allocator,
                               client->info.failed_waiting.error_message);
      break;
    case PB_RPC_CLIENT_STATE_FAILED:
      client->allocator->free (client->allocator,
                               client->info.failed.error_message);
      break;
    case PB_RPC_CLIENT_STATE_DESTROYED:
      g_warn_if_reached ();
      break;
    }

  client->stream->client = NULL; /* disassociate client from stream state */
  rut_object_unref (client->stream);

  rig_protobuf_c_data_buffer_clear (&client->stream->incoming);
  rig_protobuf_c_data_buffer_clear (&client->stream->outgoing);
  client->state = PB_RPC_CLIENT_STATE_DESTROYED;

  g_free (client->name);

  /* free closures only once we are in the destroyed state */
  for (i = 0; i < n_closures; i++)
    if (closures[i].response_type != NULL)
      closures[i].closure (NULL, closures[i].closure_data);
  if (closures)
    client->allocator->free (client->allocator, closures);

  g_slice_free (PB_RPC_Client, client);
}

static RutType rig_pb_rpc_client_type;

static void
_rig_pb_rpc_client_init_type (void)
{
  RutType *type = &rig_pb_rpc_client_type;
#define TYPE PB_RPC_Client

  rut_type_init (type, G_STRINGIFY (TYPE), _rig_pb_rpc_client_free);

#undef TYPE
}

static void
error_handler (PB_RPC_Error_Code code,
               const char *message,
               void *error_func_data)
{
  g_warning ("PB RPC: %s: %s\n", (char*) error_func_data, message);
}

static void
set_fd_nonblocking (int fd)
{
  int flags = fcntl (fd, F_GETFL);

  g_return_if_fail (flags >= 0);

  fcntl (fd, F_SETFL, flags | O_NONBLOCK);
}

static void
handle_autoreconnect_timeout (RigProtobufCDispatch *dispatch,
                              void *func_data)
{
  PB_RPC_Client *client = func_data;

  g_return_if_fail (client->state == PB_RPC_CLIENT_STATE_FAILED_WAITING);

  client->allocator->free (client->allocator,
                           client->info.failed_waiting.error_message);
  begin_name_lookup (client);
}

static void
client_failed (PB_RPC_Client *client,
               PB_RPC_Error_Code code,
               const char *format_str,
               ...)
{
  va_list args;
  char buf[MAX_FAILED_MSG_LENGTH];
  size_t msg_len;
  char *msg;

  switch (client->state)
    {
    case PB_RPC_CLIENT_STATE_NAME_LOOKUP:
      /* nothing to do */
      break;
    case PB_RPC_CLIENT_STATE_CONNECTING:
      /* nothing to do */
      break;
    case PB_RPC_CLIENT_STATE_CONNECTED:
      /* nothing to do */
      break;

      /* should not get here */
    case PB_RPC_CLIENT_STATE_INIT:
    case PB_RPC_CLIENT_STATE_FAILED_WAITING:
    case PB_RPC_CLIENT_STATE_FAILED:
    case PB_RPC_CLIENT_STATE_DESTROYED:
      g_warn_if_reached ();
      break;
    }

  /* NB: the stream state may also be shared with a server connection
   * so only close the fd if we own it... */
  if (client->stream->fd >= 0 && client->stream->conn == NULL)
    {
      rig_protobuf_c_dispatch_close_fd (client->dispatch, client->stream->fd);
      client->stream->fd = -1;
    }
  rig_protobuf_c_data_buffer_reset (&client->stream->incoming);
  rig_protobuf_c_data_buffer_reset (&client->stream->outgoing);

  /* Compute the message */
  va_start (args, format_str);
  vsnprintf (buf, sizeof (buf), format_str, args);
  va_end (args);
  buf[sizeof(buf)-1] = 0;
  msg_len = strlen (buf);
  msg = client->allocator->alloc (client->allocator, msg_len + 1);
  strcpy (msg, buf);

  /* go to one of the failed states */
  if (client->autoreconnect && client->name)
    {
      client->state = PB_RPC_CLIENT_STATE_FAILED_WAITING;
      client->info.failed_waiting.timer
        = rig_protobuf_c_dispatch_add_timer_millis (client->dispatch,
                                                client->autoreconnect_millis,
                                                handle_autoreconnect_timeout,
                                                client);
      client->info.failed_waiting.error_message = msg;
    }
  else
    {
      client->state = PB_RPC_CLIENT_STATE_FAILED;
      client->info.failed.error_message = msg;
    }

  if (client->error_handler)
    client->error_handler (code, msg, client->error_handler_data);

  if (client->state == PB_RPC_CLIENT_STATE_CONNECTED)
    {
      int n_closures = client->info.connected.closures_alloced;
      Closure *closures = client->info.connected.closures;

      /* we defer calling the closures to avoid
         any re-entrancy issues (e.g. people further RPC should
         not see a socket in the "connected" state-- at least,
         it shouldn't be accessing the array of closures that we are considering */
      if (closures != NULL)
        {
          unsigned i;

          for (i = 0; i < n_closures; i++)
            if (closures[i].response_type != NULL)
              closures[i].closure (NULL, closures[i].closure_data);
          client->allocator->free (client->allocator, closures);
        }
    }
}

static inline bool
errno_is_ignorable (int e)
{
#ifdef EWOULDBLOCK              /* for windows */
  if (e == EWOULDBLOCK)
    return 1;
#endif
  return e == EINTR || e == EAGAIN;
}

static void
set_state_connected (PB_RPC_Client *client)
{
  client->state = PB_RPC_CLIENT_STATE_CONNECTED;

  client->info.connected.closures_alloced = 1;
  client->info.connected.first_free_request_id = 1;
  client->info.connected.closures =
    client->allocator->alloc (client->allocator, sizeof (Closure));
  client->info.connected.closures[0].closure = NULL;
  client->info.connected.closures[0].response_type = NULL;
  client->info.connected.closures[0].closure_data = GUINT_TO_POINTER (0);

  if (client->connect_handler)
    client->connect_handler (client, client->connect_handler_data);
}

static RigProtobufCDispatch *
stream_get_dispatch (Stream *stream)
{
  if (stream->conn)
    return stream->conn->server->dispatch;
  else
    return stream->client->dispatch;
}

static void
update_stream_fd_watch (Stream *stream)
{
  unsigned events = PROTOBUF_C_EVENT_READABLE;
  RigProtobufCDispatch *dispatch = stream_get_dispatch (stream);

  g_return_if_fail (stream->fd >= 0);

  if (stream->outgoing.size > 0)
    events |= PROTOBUF_C_EVENT_WRITABLE;

  rig_protobuf_c_dispatch_watch_fd (dispatch,
                                    stream->fd,
                                    events,
                                    handle_stream_fd_events,
                                    stream);
}

static void
handle_client_fd_connect_events (int fd,
                                 unsigned events,
                                 void *callback_data)
{
  PB_RPC_Client *client = callback_data;
  socklen_t size_int = sizeof (int);
  int fd_errno = EINVAL;
  if (getsockopt (fd, SOL_SOCKET, SO_ERROR, &fd_errno, &size_int) < 0)
    {
      /* Note: this behavior is vaguely hypothetically broken,
       *       in terms of ignoring getsockopt's error;
       *       however, this shouldn't happen, and EINVAL is ok if it does.
       *       Furthermore some broken OS's return an error code when
       *       fetching SO_ERROR!
       */
    }

  if (fd_errno == 0)
    {
      /* goto state CONNECTED */
      set_state_connected (client);
      update_stream_fd_watch (client->stream);
    }
  else if (errno_is_ignorable (fd_errno))
    {
      /* remain in CONNECTING state */
      return;
    }
  else
    {
      PB_RPC_Error_Code code;

      /* Call error handler */
      if (fd_errno == ECONNREFUSED)
        code = PB_RPC_ERROR_CODE_CONNECTION_REFUSED;
      else
        code = PB_RPC_ERROR_CODE_CONNECTION_FAILED;

      client_failed (client,
                     code,
                     "failed connecting to server: %s",
                     strerror (fd_errno));
    }
}

static void
begin_connecting (PB_RPC_Client *client,
                  struct sockaddr *address,
                  size_t addr_len)
{
  g_return_if_fail (client->state == PB_RPC_CLIENT_STATE_NAME_LOOKUP);

  client->state = PB_RPC_CLIENT_STATE_CONNECTING;
  client->stream->fd = socket (address->sa_family, SOCK_STREAM, 0);
  if (client->stream->fd < 0)
    {
      client_failed (client,
                     PB_RPC_ERROR_CODE_CONNECTION_FAILED,
                     "error creating socket: %s", strerror (errno));
      return;
    }
  set_fd_nonblocking (client->stream->fd);
  if (connect (client->stream->fd, address, addr_len) < 0)
    {
      if (errno == EINPROGRESS)
        {
          /* register interest in fd */
          rig_protobuf_c_dispatch_watch_fd (client->dispatch,
                                        client->stream->fd,
                                        PROTOBUF_C_EVENT_READABLE |
                                        PROTOBUF_C_EVENT_WRITABLE,
                                        handle_client_fd_connect_events,
                                        client);
          return;
        }

      client_failed (client,
                     PB_RPC_ERROR_CODE_CONNECTION_FAILED,
                     "error connecting to remote host: %s", strerror (errno));
      return;
    }

  set_state_connected (client);
}

static void
handle_name_lookup_success (const uint8_t *address,
                            void *callback_data)
{
  PB_RPC_Client *client = callback_data;
  struct sockaddr_in addr;

  g_return_if_fail (client->state == PB_RPC_CLIENT_STATE_NAME_LOOKUP);

  memset (&addr, 0, sizeof (addr));
  addr.sin_family = AF_INET;
  memcpy (&addr.sin_addr, address, 4);
  addr.sin_port = htons (client->info.name_lookup.port);
  begin_connecting (client, (struct sockaddr *) &addr, sizeof (addr));
}

static void
handle_name_lookup_failure (const char *error_message,
                            void *callback_data)
{
  PB_RPC_Client *client = callback_data;

  g_return_if_fail (client->state == PB_RPC_CLIENT_STATE_NAME_LOOKUP);

  client_failed (client,
                 PB_RPC_ERROR_CODE_CONNECTION_FAILED,
                 "name lookup failed (for name from %s): %s",
                 client->name, error_message);
}

static void
begin_name_lookup (PB_RPC_Client *client)
{
  g_return_if_fail (client->state == PB_RPC_CLIENT_STATE_INIT
                 ||  client->state == PB_RPC_CLIENT_STATE_FAILED_WAITING
                 ||  client->state == PB_RPC_CLIENT_STATE_FAILED);

  client->state = PB_RPC_CLIENT_STATE_NAME_LOOKUP;

  switch (client->address_type)
    {
    case PROTOBUF_C_RPC_ADDRESS_LOCAL:
      {
        struct sockaddr_un addr;
        addr.sun_family = AF_UNIX;
        strncpy (addr.sun_path, client->name, sizeof (addr.sun_path));
        begin_connecting (client, (struct sockaddr *) &addr,
                          sizeof (addr));
        return;
      }

    case PROTOBUF_C_RPC_ADDRESS_TCP:
      {
        /* parse hostname:port from client->name */
        const char *colon = strchr (client->name, ':');
        char *host;
        unsigned port;
        if (colon == NULL)
          {
            client_failed (client,
                           PB_RPC_ERROR_CODE_CONNECTION_FAILED,
                           "name '%s' does not have a : in it "
                           "(supposed to be HOST:PORT)",
                           client->name);
            return;
          }
        host = client->allocator->alloc (client->allocator,
                                         colon + 1 - client->name);
        memcpy (host, client->name, colon - client->name);
        host[colon - client->name] = 0;
        port = atoi (colon + 1);

        client->info.name_lookup.port = port;
        client->resolver (client->dispatch,
                          host,
                          handle_name_lookup_success,
                          handle_name_lookup_failure,
                          client);

        /* cleanup */
        client->allocator->free (client->allocator, host);
        return;
      }
    default:
      assert (0);
    }
}

static void
handle_init_idle (RigProtobufCDispatch *dispatch,
                  void *data)
{
  PB_RPC_Client *client = data;

  g_return_if_fail (client->state == PB_RPC_CLIENT_STATE_INIT);

  begin_name_lookup (client);
}

static void
grow_closure_array (PB_RPC_Client *client)
{
  /* resize array */
  unsigned old_size = client->info.connected.closures_alloced;
  unsigned new_size = old_size * 2;
  int i;
  Closure *new_closures =
    client->allocator->alloc (client->allocator, sizeof (Closure) * new_size);
  memcpy (new_closures,
          client->info.connected.closures,
          sizeof (Closure) * old_size);

  /* build new free list */
  for (i = old_size; i < new_size - 1; i++)
    {
      new_closures[i].response_type = NULL;
      new_closures[i].closure = NULL;
      new_closures[i].closure_data = GUINT_TO_POINTER (i+2);
    }
  new_closures[i].closure_data =
    GUINT_TO_POINTER (client->info.connected.first_free_request_id);
  new_closures[i].response_type = NULL;
  new_closures[i].closure = NULL;
  client->info.connected.first_free_request_id = old_size + 1;

  client->allocator->free (client->allocator, client->info.connected.closures);
  client->info.connected.closures = new_closures;
  client->info.connected.closures_alloced = new_size;
}

static void
enqueue_request (PB_RPC_Client *client,
                 unsigned method_index,
                 const ProtobufCMessage *input,
                 ProtobufCClosure closure,
                 void *closure_data)
{
  uint32_t request_id;
  struct {
    uint32_t method_index;
    uint32_t packed_size;
    uint32_t request_id;
  } header;
  size_t packed_size;
  uint8_t *packed_data;
  Closure *cl;
  const ProtobufCServiceDescriptor *desc = client->service.descriptor;
  const ProtobufCMethodDescriptor *method = desc->methods + method_index;
  int had_outgoing = (client->stream->outgoing.size > 0);

  g_return_if_fail (client->state == PB_RPC_CLIENT_STATE_CONNECTED);
  g_return_if_fail (method_index < desc->n_methods);

  /* Allocate request_id */
  if (client->info.connected.first_free_request_id == 0)
    grow_closure_array (client);
  request_id = client->info.connected.first_free_request_id;
  cl = client->info.connected.closures + (request_id - 1);
  client->info.connected.first_free_request_id =
    GPOINTER_TO_UINT (cl->closure_data);

  /* Pack message */
  packed_size = protobuf_c_message_get_packed_size (input);
  if (packed_size < client->allocator->max_alloca)
    packed_data = alloca (packed_size);
  else
    packed_data = client->allocator->alloc (client->allocator, packed_size);
  protobuf_c_message_pack (input, packed_data);

  /* Append to buffer */
  g_assert (sizeof (header) == 12);
  header.method_index = uint32_to_le (method_index);
  header.packed_size = uint32_to_le (packed_size);
  header.request_id = request_id;
  rig_protobuf_c_data_buffer_append (&client->stream->outgoing, &header, 12);
  rig_protobuf_c_data_buffer_append (&client->stream->outgoing,
                                     packed_data, packed_size);

  /* Clean up if not using alloca() */
  if (packed_size >= client->allocator->max_alloca)
    client->allocator->free (client->allocator, packed_data);

  /* Add closure to request-tree */
  cl->response_type = method->output;
  cl->closure = closure;
  cl->closure_data = closure_data;

  if (client->state == PB_RPC_CLIENT_STATE_CONNECTED &&
      !had_outgoing)
    update_stream_fd_watch (client->stream);
}

static void
invoke_client_rpc (ProtobufCService *service,
                   unsigned method_index,
                   const ProtobufCMessage *input,
                   ProtobufCClosure closure,
                   void *closure_data)
{
  PB_RPC_Client *client = rut_container_of (service, client, service);

  g_return_if_fail (service->invoke == invoke_client_rpc);

  switch (client->state)
    {
    case PB_RPC_CLIENT_STATE_INIT:
    case PB_RPC_CLIENT_STATE_NAME_LOOKUP:
    case PB_RPC_CLIENT_STATE_CONNECTING:
    case PB_RPC_CLIENT_STATE_CONNECTED:
      enqueue_request (client, method_index, input, closure, closure_data);
      break;

    case PB_RPC_CLIENT_STATE_FAILED_WAITING:
    case PB_RPC_CLIENT_STATE_FAILED:
    case PB_RPC_CLIENT_STATE_DESTROYED:
      closure (NULL, closure_data);
      break;
    }
}

static void
trivial_sync_libc_resolver (RigProtobufCDispatch *dispatch,
                            const char *name,
                            ProtobufC_NameLookup_Found found_func,
                            ProtobufC_NameLookup_Failed failed_func,
                            void *callback_data)
{
  struct hostent *ent;
  ent = gethostbyname (name);
  if (ent == NULL)
    failed_func (hstrerror (h_errno), callback_data);
  else
    found_func ((const uint8_t *) ent->h_addr_list[0], callback_data);
}

PB_RPC_Client *
client_new (const ProtobufCServiceDescriptor *descriptor,
            RigProtobufCDispatch *dispatch,
            Stream *stream)
{
  PB_RPC_Client *client = rut_object_alloc0 (PB_RPC_Client,
                                             &rig_pb_rpc_client_type,
                                             _rig_pb_rpc_client_init_type);


  client->service.descriptor = descriptor;
  client->service.invoke = invoke_client_rpc;
  client->service.destroy = NULL; /* we rely on ref-counting instead */

  client->stream = rut_object_ref (stream);
  stream->client = client;

  client->allocator = rig_protobuf_c_dispatch_peek_allocator (dispatch);
  client->dispatch = dispatch;
  client->state = PB_RPC_CLIENT_STATE_INIT;
  client->autoreconnect = false;
  client->autoreconnect_millis = 2 * 1000;
  client->resolver = trivial_sync_libc_resolver;
  client->error_handler = error_handler;
  client->error_handler_data = "protobuf-c rpc client";

  return client;
}

static void
_stream_free (void *object)
{
  Stream *stream = object;

#warning "track whether stream->fd is foreign"
  if (stream->fd != -1)
    rig_protobuf_c_dispatch_close_fd (stream->dispatch, stream->fd);

  rig_protobuf_c_data_buffer_clear (&stream->incoming);
  rig_protobuf_c_data_buffer_clear (&stream->outgoing);

  g_slice_free (Stream, stream);
}

static RutType stream_type;

static void
_stream_init_type (void)
{
  RutType *type = &stream_type;
#define TYPE Stream

  rut_type_init (type, G_STRINGIFY (TYPE), _stream_free);

#undef TYPE
}

Stream *
stream_new (RigProtobufCDispatch *dispatch,
            int fd)
{
  ProtobufCAllocator *allocator = rig_protobuf_c_dispatch_peek_allocator (dispatch);
  Stream *stream = rut_object_alloc0 (Stream,
                                      &stream_type,
                                      _stream_init_type);


  /* We have to explicitly track the dispatch with the stream since
   * it may be referenced when freeing the stream after any client
   * or server connection have been disassociated from the stream.
   */
  stream->dispatch = dispatch;

  stream->fd = fd;
  rig_protobuf_c_data_buffer_init (&stream->incoming, allocator);
  rig_protobuf_c_data_buffer_init (&stream->outgoing, allocator);

  return stream;
}

PB_RPC_Client *
rig_pb_rpc_client_new (PB_RPC_AddressType type,
                       const char *name,
                       const ProtobufCServiceDescriptor *descriptor,
                       RigProtobufCDispatch *orig_dispatch)
{
  RigProtobufCDispatch *dispatch =
    orig_dispatch ? orig_dispatch : rig_protobuf_c_dispatch_default ();
  Stream *stream = stream_new (dispatch, -1);
  PB_RPC_Client *client = client_new (descriptor, dispatch, stream);

  /* After calling client_new() the client will have taken ownership
   * of the stream */
  rut_object_unref (stream);

  client->address_type = type;
  client->name = g_strdup (name);
  client->autoreconnect = true;

  client->info.init.idle =
    rig_protobuf_c_dispatch_add_idle (client->dispatch,
                                      handle_init_idle, client);

  return client;
}

ProtobufCService *
rig_pb_rpc_client_get_service (PB_RPC_Client *client)
{
  return &client->service;
}

bool
rig_pb_rpc_client_is_connected (PB_RPC_Client *client)
{
  return client->state == PB_RPC_CLIENT_STATE_CONNECTED;
}

void
rig_pb_rpc_client_set_autoreconnect_period (PB_RPC_Client *client,
                                            unsigned millis)
{
  client->autoreconnect = 1;
  client->autoreconnect_millis = millis;
}

void
rig_pb_rpc_client_set_error_handler (PB_RPC_Client *client,
                                     PB_RPC_Error_Func func,
                                     void *func_data)
{
  client->error_handler = func;
  client->error_handler_data = func_data;
}

void
rig_pb_rpc_client_set_connect_handler (PB_RPC_Client *client,
                                       PB_RPC_Connect_Func func,
                                       void *func_data)
{
  client->connect_handler = func;
  client->connect_handler_data = func_data;
}

void
rig_pb_rpc_client_disable_autoreconnect (PB_RPC_Client *client)
{
  client->autoreconnect = 0;
}

#define GET_PENDING_REQUEST_LIST(conn) \
  ServerRequest *, \
  conn->first_pending_request, \
  conn->last_pending_request, \
  info.alive.prev, \
  info.alive.next
#define GET_CONNECTION_LIST(server) \
  PB_RPC_ServerConnection *, \
  server->first_connection, \
  server->last_connection, prev, next

static void
server_connection_close (PB_RPC_ServerConnection *conn)
{
  ProtobufCAllocator *allocator = conn->server->allocator;

  /* Check if already closed... */
  if (!conn->stream)
    return;

  if (conn->server->client_close_handler)
    {
      conn->server->client_close_handler (conn->server,
                                          conn,
                                          conn->server->client_close_handler_data);
    }

  if (conn->close_handler)
    conn->close_handler (conn, conn->close_handler_data);

  /* general cleanup */

  conn->stream->conn = NULL; /* disassociate server connection
                              * from stream state */
  rut_object_unref (conn->stream);
  conn->stream = NULL;

  /* remove this connection from the server's list */
  GSK_LIST_REMOVE (GET_CONNECTION_LIST (conn->server), conn);

  /* disassocate all the requests from the connection */
  while (conn->first_pending_request != NULL)
    {
      ServerRequest *req = conn->first_pending_request;
      conn->first_pending_request = req->info.alive.next;
      req->conn = NULL;
      req->info.defunct.allocator = allocator;
    }
}

static void
_rig_pb_rpc_server_free (void *object)
{
  PB_RPC_Server *server = object;

  /* We assume that a server service is declared statically with no
   * destroy callback... */
  g_warn_if_fail (server->service->destroy == NULL);

  while (server->first_connection != NULL)
    {
      server_connection_close (server->first_connection);
      rut_object_unref (server->first_connection);
    }

  if (server->address_type == PROTOBUF_C_RPC_ADDRESS_LOCAL)
    unlink (server->bind_name);
  server->allocator->free (server->allocator, server->bind_name);

  while (server->recycled_requests != NULL)
    {
      ServerRequest *req = server->recycled_requests;
      server->recycled_requests = req->info.recycled.next;
      server->allocator->free (server->allocator, req);
    }

  if (server->listening_fd >= 0)
    rig_protobuf_c_dispatch_close_fd (server->dispatch, server->listening_fd);

  g_slice_free (PB_RPC_Server, server);
}

static RutType rig_pb_rpc_server_type;

static void
_rig_pb_rpc_server_init_type (void)
{
  RutType *type = &rig_pb_rpc_server_type;
#define TYPE PB_RPC_Server

  rut_type_init (type, G_STRINGIFY (TYPE), _rig_pb_rpc_server_free);

#undef TYPE
}

static void
server_failed_literal (PB_RPC_Server *server,
                       PB_RPC_Error_Code code,
                       const char *msg)
{
  if (server->error_handler != NULL)
    server->error_handler (code, msg, server->error_handler_data);
}

#if 0
static void
server_failed (PB_RPC_Server *server,
               PB_RPC_Error_Code code,
               const char           *format,
               ...)
{
  va_list args;
  char buf[MAX_FAILED_MSG_LENGTH];
  va_start (args, format);
  vsnprintf (buf, sizeof (buf), format, args);
  buf[sizeof(buf)-1] = 0;
  va_end (args);

  server_failed_literal (server, code, buf);
}
#endif

static bool
address_to_name (const struct sockaddr *addr,
                 unsigned addr_len,
                 char *name_out,
                 unsigned name_out_buf_length)
{
  if (addr->sa_family == PF_INET)
    {
      /* convert to dotted address + port */
      const struct sockaddr_in *addr_in = (const struct sockaddr_in *) addr;
      const uint8_t *addr = (const uint8_t *) &(addr_in->sin_addr);
      uint16_t port = htons (addr_in->sin_port);
      snprintf (name_out, name_out_buf_length,
                "%u.%u.%u.%u:%u",
                addr[0], addr[1], addr[2], addr[3], port);
      return true;
    }
  return false;
}

static void
server_connection_failed (PB_RPC_ServerConnection *conn,
                          PB_RPC_Error_Code code,
                          const char *format,
                          ...)
{
  PB_RPC_Server *server = conn->server;
  char remote_addr_name[64];
  char msg[MAX_FAILED_MSG_LENGTH];
  char *msg_end = msg + sizeof (msg);
  char *msg_at;
  struct sockaddr addr;
  socklen_t addr_len = sizeof (addr);
  va_list args;

  /* if we can, find the remote name of this connection */
  if (getpeername (conn->stream->fd, &addr, &addr_len) == 0
   && address_to_name (&addr, addr_len,
                       remote_addr_name, sizeof (remote_addr_name)))
    snprintf (msg, sizeof (msg), "connection to %s from %s: ",
              conn->server->bind_name, remote_addr_name);
  else
    snprintf (msg, sizeof (msg), "connection to %s: ",
              conn->server->bind_name);
  msg[sizeof(msg)-1] = 0;
  msg_at = strchr (msg, 0);

  /* do vsnprintf() */
  va_start (args, format);
  vsnprintf(msg_at, msg_end - msg_at, format, args);
  va_end (args);
  msg[sizeof(msg)-1] = 0;

  /* In case the connection's error handler tries to clean up the
   * connection we take a reference on the server and connection
   * until we are done... */
  rut_object_ref (server);
  rut_object_ref (conn);

  /* Note: This could end up freeing the connection */
  if (conn->error_handler)
    conn->error_handler (conn, code, msg, conn->error_handler_data);

  /* invoke server error hook */
  server_failed_literal (server, code, msg);

  /* Explicitly disconnect, in case the above error handler didn't already */
  server_connection_close (conn);
  rut_object_unref (conn);

  /* Drop our transient references (see above) */
  rut_object_ref (conn);
  rut_object_unref (server);
}

static ServerRequest *
create_server_request (PB_RPC_ServerConnection *conn,
                       uint32_t request_id,
                       uint32_t method_index,
                       ProtobufCMessage *message)
{
  ServerRequest *rv;
  if (conn->server->recycled_requests != NULL)
    {
      rv = conn->server->recycled_requests;
      conn->server->recycled_requests = rv->info.recycled.next;
    }
  else
    {
      ProtobufCAllocator *allocator = conn->server->allocator;
      rv = allocator->alloc (allocator, sizeof (ServerRequest));
    }
  rv->server = conn->server;
  rv->conn = conn;
  rv->request_id = request_id;
  rv->method_index = method_index;
  rv->message = message;
  conn->n_pending_requests++;
  GSK_LIST_APPEND (GET_PENDING_REQUEST_LIST (conn), rv);
  return rv;
}

static void
free_server_request (PB_RPC_Server *server,
                     ServerRequest *request)
{
  protobuf_c_message_free_unpacked (request->message, server->allocator);

#if RECYCLE_REQUESTS
  /* recycle request */
  request->info.recycled.next = server->recycled_requests;
  server->recycled_requests = request;
#else
  /* free the request immediately */
  server->allocator->free (server->allocator, request);
#endif
}

static void
server_connection_response_closure (const ProtobufCMessage *message,
                                    void *closure_data)
{
  ServerRequest *request = closure_data;
  PB_RPC_Server *server = request->server;
  bool must_proxy = 0;
  ProtobufCAllocator *allocator = server->allocator;
  uint8_t buffer_slab[512];
  ProtobufCBufferSimple buffer_simple =
    PROTOBUF_C_BUFFER_SIMPLE_INIT (buffer_slab);
  uint32_t header[3];

  /* XXX: we removed the ability to return an error status so we now
   * assert that the response points to a valid message... */
  g_return_if_fail (message != NULL);

  if (server->is_rpc_thread_func != NULL)
    {
      must_proxy = !server->is_rpc_thread_func (server,
                                                server->dispatch,
                                                server->is_rpc_thread_data);
    }

  /* Note: The header[] for client replies have the same layout as the
   * headers for server requests except that that the method_index is
   * set to ~0 so requests and replies can be distinguished by peer to
   * peer clients.
   */
  header[0] = ~0;
  header[2] = request->request_id;
  protobuf_c_buffer_simple_append (&buffer_simple.base,
                                   12, (uint8_t *) header);
  protobuf_c_message_pack_to_buffer (message, &buffer_simple.base);
  ((uint32_t *)buffer_simple.data)[1] =
    uint32_to_le (buffer_simple.len - 12);

  if (must_proxy)
    {
      ProxyResponse *pr =
        allocator->alloc (allocator,
                          sizeof (ProxyResponse) + buffer_simple.len);
      int rv;
      pr->request = request;
      pr->len = buffer_simple.len;
      memcpy (pr + 1, buffer_simple.data, buffer_simple.len);

      /* write pointer to proxy pipe */
retry_write:
      rv = write (server->proxy_pipe[1], &pr, sizeof (void*));
      if (rv < 0)
        {
          if (errno == EINTR || errno == EAGAIN)
            goto retry_write;
          server_failed_literal (server, PB_RPC_ERROR_CODE_PROXY_PROBLEM,
                                 "error writing to proxy-pipe");
          allocator->free (allocator, pr);
        }
      else if (rv < sizeof (void *))
        {
          server_failed_literal (server, PB_RPC_ERROR_CODE_PROXY_PROBLEM,
                                 "partial write to proxy-pipe");
          allocator->free (allocator, pr);
        }
    }
  else if (request->conn == NULL)
    {
      /* defunct request */
      free_server_request (server, request);
    }
  else
    {
      PB_RPC_ServerConnection *conn = request->conn;
      rig_protobuf_c_data_buffer_append (&conn->stream->outgoing,
                                         buffer_simple.data,
                                         buffer_simple.len);
      update_stream_fd_watch (conn->stream);

      GSK_LIST_REMOVE (GET_PENDING_REQUEST_LIST (conn), request);
      conn->n_pending_requests--;

      free_server_request (server, request);
    }
  PROTOBUF_C_BUFFER_SIMPLE_CLEAR (&buffer_simple);
}

static void
read_client_reply (PB_RPC_Client *client,
                   uint32_t method_index,
                   uint32_t message_length,
                   uint32_t request_id)
{
  Closure *closure;
  uint8_t *packed_data;
  ProtobufCMessage *msg;

  g_return_if_fail (client->state == PB_RPC_CLIENT_STATE_CONNECTED);

  /* lookup request by id */
  if (request_id > client->info.connected.closures_alloced
      || request_id == 0
      || client->info.connected.closures[request_id-1].response_type == NULL)
    {
      client_failed (client,
                     PB_RPC_ERROR_CODE_BAD_REQUEST,
                     "bad request-id in response from server: %d", request_id);
      return;
    }
  closure = client->info.connected.closures + (request_id - 1);

  /* read message and unpack */
  rig_protobuf_c_data_buffer_discard (&client->stream->incoming, 12);
  packed_data = client->allocator->alloc (client->allocator,
                                          message_length);
  rig_protobuf_c_data_buffer_read (&client->stream->incoming, packed_data,
                                   message_length);

  /* TODO: use fast temporary allocator */
  msg = protobuf_c_message_unpack (closure->response_type,
                                   client->allocator,
                                   message_length,
                                   packed_data);
  if (msg == NULL)
    {
      fprintf(stderr, "unable to unpack msg of length %u", message_length);
      client_failed (client,
                     PB_RPC_ERROR_CODE_UNPACK_ERROR,
                     "failed to unpack message");
      client->allocator->free (client->allocator, packed_data);
      return;
    }

  /* invoke closure */
  closure->closure (msg, closure->closure_data);
  closure->response_type = NULL;
  closure->closure = NULL;
  closure->closure_data =
    GUINT_TO_POINTER (client->info.connected.first_free_request_id);
  client->info.connected.first_free_request_id = request_id;

  /* clean up */
  protobuf_c_message_free_unpacked (msg, client->allocator);
  client->allocator->free (client->allocator, packed_data);
}

static void
read_server_request (PB_RPC_ServerConnection *conn,
                     uint32_t method_index,
                     uint32_t message_length,
                     uint32_t request_id)
{
  ProtobufCService *service = conn->server->service;
  ProtobufCAllocator *allocator = conn->server->allocator;
  uint8_t *packed_data;
  ProtobufCMessage *message;
  ServerRequest *server_request;

  if ((method_index >=
       conn->server->service->descriptor->n_methods))
    {
      server_connection_failed (conn,
                                PB_RPC_ERROR_CODE_BAD_REQUEST,
                                "bad method_index %u", method_index);
      return;
    }

  /* Read message */
  rig_protobuf_c_data_buffer_discard (&conn->stream->incoming, 12);
  packed_data = allocator->alloc (allocator, message_length);
  rig_protobuf_c_data_buffer_read (&conn->stream->incoming,
                                   packed_data,
                                   message_length);

  /* Unpack message */
  message = protobuf_c_message_unpack (service->descriptor->methods[method_index].input,
                                       allocator, message_length, packed_data);
  allocator->free (allocator, packed_data);
  if (message == NULL)
    {
      server_connection_failed (conn,
                                PB_RPC_ERROR_CODE_BAD_REQUEST,
                                "error unpacking message");
      return;
    }

  /* Invoke service (note that it may call back immediately) */
  server_request =
    create_server_request (conn, request_id, method_index, message);
  service->invoke (service, method_index, message,
                   server_connection_response_closure, server_request);
}

static void
handle_stream_fd_events (int fd,
                         unsigned events,
                         void *data)
{
  Stream *stream = data;

  /* NB: A stream may represent a server connection, a client or both
   * and it's only once we've read the header for any messages that
   * we can determine whether we are dealing with a request to the
   * server or a reply back to the client....
   */

  PB_RPC_ServerConnection *conn = stream->conn;
  PB_RPC_Client *client = stream->client;

  if (events & PROTOBUF_C_EVENT_READABLE)
    {
      int read_rv = rig_protobuf_c_data_buffer_read_in_fd (&stream->incoming, fd);
      if (read_rv < 0)
        {
          if (!errno_is_ignorable (errno))
            {
              /* Note: in the peer to peer case we only report a
               * server error to avoid problems with the error handler
               * disconnecting the client. */
              if (conn)
                {
                  server_connection_failed (conn,
                                            PB_RPC_ERROR_CODE_CLIENT_TERMINATED,
                                            "reading from file-descriptor: %s",
                                            strerror (errno));
                }
              else
                {
                  client_failed (client,
                                 PB_RPC_ERROR_CODE_IO_ERROR,
                                 "reading from file-descriptor: %s",
                                 strerror (errno));
                }

              return;
            }
        }
      else if (read_rv == 0)
        {
          /* Note: in the peer to peer case we only report a server
           * error to avoid problems with the error handler
           * disconnecting the client. */
          if (conn)
            {
              if (conn->first_pending_request != NULL)
                server_connection_failed (conn,
                                          PB_RPC_ERROR_CODE_CLIENT_TERMINATED,
                                          "closed while calls pending");
              else
                {
#warning "fixme: in the peer to peer case this is an error"
                  server_connection_close (conn);
                  rut_object_unref (conn);
                  conn = NULL;
                }
            }
          else
            {
              client_failed (client,
                             PB_RPC_ERROR_CODE_IO_ERROR,
                             "got end-of-file from server "
                             "[%u bytes incoming, %u bytes outgoing]",
                             stream->incoming.size, stream->outgoing.size);
            }

          return;
        }
      else
        while (stream->incoming.size >= 12)
          {
            uint32_t header[3];
            uint32_t method_index, message_length, request_id;

            rig_protobuf_c_data_buffer_peek (&stream->incoming, header, 12);
            method_index = uint32_from_le (header[0]);
            message_length = uint32_from_le (header[1]);
            request_id = header[2]; /* store in whatever endianness it comes in */

            if (stream->incoming.size < 12 + message_length)
              break;

            /* XXX: it's possible if we were sent a mallformed message
             * that looks like it's for a client or server but the
             * corresponding object is NULL...
             */

            if (client && method_index == ~0)
              {
                read_client_reply (client, method_index,
                                   message_length, request_id);
              }
            else if (conn && method_index != ~0)
              {
                read_server_request (conn, method_index,
                                     message_length, request_id);
              }
            else
              {
                if (conn)
                  {
                    server_connection_failed (conn,
                                              PB_RPC_ERROR_CODE_BAD_REQUEST,
                                              "bad method_index %u", method_index);
                  }
                else
                  {
                    client_failed (client,
                                   PB_RPC_ERROR_CODE_BAD_REQUEST,
                                   "bad method_index in response from server: %d",
                                   method_index);
                  }
              }
          }
    }

  if ((events & PROTOBUF_C_EVENT_WRITABLE) != 0
      && stream->outgoing.size > 0)
    {
      int write_rv = rig_protobuf_c_data_buffer_writev (&stream->outgoing, fd);
      if (write_rv < 0)
        {
          if (!errno_is_ignorable (errno))
            {
              /* Note: in the peer to peer case we only report a
               * server error to avoid problems with the error handler
               * disconnecting the client. */
              if (conn)
                {
                  server_connection_failed (conn,
                                            PB_RPC_ERROR_CODE_CLIENT_TERMINATED,
                                            "writing to file-descriptor: %s",
                                            strerror (errno));
                }
              else
                {
                  client_failed (client,
                                 PB_RPC_ERROR_CODE_IO_ERROR,
                                 "writing to file-descriptor: %s",
                                 strerror (errno));
                }

              return;
            }
        }

      update_stream_fd_watch (stream);
    }
}

static void
_server_connection_free (void *object)
{
  PB_RPC_ServerConnection *conn = object;

  server_connection_close (conn);

  rut_object_free (PB_RPC_ServerConnection, conn);
}

static RutType _server_connection_type;

static void
_server_connection_init_type (void)
{
  rut_type_init (&_server_connection_type,
                 "PB_RPC_ServerConnection", _server_connection_free);
}

static PB_RPC_ServerConnection *
server_connection_new (PB_RPC_Server *server,
                       Stream *stream)
{
  PB_RPC_ServerConnection *conn =
    rut_object_alloc0 (PB_RPC_ServerConnection,
                       &_server_connection_type,
                       _server_connection_init_type);

  conn->stream = rut_object_ref (stream);
  stream->conn = conn;

  conn->server = server;

  return conn;
}

static PB_RPC_ServerConnection *
server_add_connection_with_stream (PB_RPC_Server *server,
                                   Stream *stream)
{
  PB_RPC_ServerConnection *conn = server_connection_new (server, stream);

  GSK_LIST_APPEND (GET_CONNECTION_LIST (server), conn);

  update_stream_fd_watch (conn->stream);

  if (server->client_connect_handler)
    {
      server->client_connect_handler (server,
                                      conn,
                                      server->client_connect_handler_data);
    }

  return conn;
}

static void
handle_server_listener_readable (int fd,
                                 unsigned events,
                                 void *data)
{
  PB_RPC_Server *server = data;
  struct sockaddr addr;
  socklen_t addr_len = sizeof (addr);
  int new_fd = accept (fd, &addr, &addr_len);
  Stream *stream;

  if (new_fd < 0)
    {
      if (errno_is_ignorable (errno))
        return;
      fprintf (stderr, "error accept()ing file descriptor: %s\n",
               strerror (errno));
      return;
    }

  stream = stream_new (server->dispatch, new_fd);

  server_add_connection_with_stream (server, stream);

  /* Once the connection has been added the connection itself will
   * maintain a reference to the stream */
  rut_object_unref (stream);
}

static PB_RPC_Server *
server_new (ProtobufCService *service,
            RigProtobufCDispatch *dispatch)
{
  PB_RPC_Server *server = rut_object_alloc0 (PB_RPC_Server,
                                             &rig_pb_rpc_server_type,
                                             _rig_pb_rpc_server_init_type);

  server->dispatch = dispatch;
  server->allocator = rig_protobuf_c_dispatch_peek_allocator (dispatch);
  server->service = service;
  server->max_pending_requests_per_connection = 32;
  server->error_handler = error_handler;
  server->error_handler_data = "protobuf-c rpc server";
  server->proxy_pipe[0] = server->proxy_pipe[1] = -1;
  server->proxy_extra_data_len = 0;
  server->listening_fd = -1;

  return server;
}

static PB_RPC_Server *
server_new_from_listening_fd (ProtobufC_FD listening_fd,
                              PB_RPC_AddressType address_type,
                              const char *bind_name,
                              ProtobufCService *service,
                              RigProtobufCDispatch *orig_dispatch)
{
  RigProtobufCDispatch *dispatch =
    orig_dispatch ? orig_dispatch : rig_protobuf_c_dispatch_default ();
  PB_RPC_Server *server = server_new (service, dispatch);
  ProtobufCAllocator *allocator = server->allocator;

  server->address_type = address_type;

  server->bind_name = allocator->alloc (allocator, strlen (bind_name) + 1);
  strcpy (server->bind_name, bind_name);

  server->listening_fd = listening_fd;
  set_fd_nonblocking (listening_fd);

  rig_protobuf_c_dispatch_watch_fd (server->dispatch, listening_fd,
                                PROTOBUF_C_EVENT_READABLE,
                                handle_server_listener_readable, server);
  return server;
}

/* this function is for handling the common problem
   that we bind over-and-over again to the same
   unix path.

   ideally, you'd think the OS's SO_REUSEADDR flag would
   cause this to happen, but it doesn't,
   at least on my linux 2.6 box.

   in fact, we really need a way to test without
   actually connecting to the remote server,
   which might annoy it.

   XXX: we should survey what others do here... like x-windows...
 */
/* NOTE: stolen from gsk, obviously */
static void
_gsk_socket_address_local_maybe_delete_stale_socket (const char *path,
                                                     struct sockaddr *addr,
                                                     unsigned addr_len)
{
  int fd;
  struct stat statbuf;
  if (stat (path, &statbuf) < 0)
    return;
  if (!S_ISSOCK (statbuf.st_mode))
    {
      fprintf (stderr, "%s existed but was not a socket\n", path);
      return;
    }

  fd = socket (PF_UNIX, SOCK_STREAM, 0);
  if (fd < 0)
    return;
  set_fd_nonblocking (fd);
  if (connect (fd, addr, addr_len) < 0)
    {
      if (errno == EINPROGRESS)
        {
          close (fd);
          return;
        }
    }
  else
    {
      close (fd);
      return;
    }

  /* ok, we should delete the stale socket */
  close (fd);
  if (unlink (path) < 0)
    fprintf (stderr, "unable to delete %s: %s\n",
             path, strerror(errno));
}

PB_RPC_Server *
rig_pb_rpc_server_new (PB_RPC_AddressType type,
                       const char *name,
                       ProtobufCService *service,
                       RigProtobufCDispatch *orig_dispatch)
{
  int fd = -1;
  int protocol_family;
  struct sockaddr *address;
  socklen_t address_len;
  struct sockaddr_un addr_un;
  struct sockaddr_in addr_in;
  bool need_bind = true;

  switch (type)
    {
    case PROTOBUF_C_RPC_ADDRESS_LOCAL:
      protocol_family = PF_UNIX;
      memset (&addr_un, 0, sizeof (addr_un));
      addr_un.sun_family = AF_LOCAL;
      strncpy (addr_un.sun_path, name, sizeof (addr_un.sun_path));
      address_len = sizeof (addr_un);
      address = (struct sockaddr *) (&addr_un);
      _gsk_socket_address_local_maybe_delete_stale_socket (name,
                                                           address,
                                                           address_len);
      break;
    case PROTOBUF_C_RPC_ADDRESS_TCP:
      protocol_family = PF_INET;
      memset (&addr_in, 0, sizeof (addr_in));
      addr_in.sin_family = AF_INET;
      {
        unsigned port = atoi (name);
        addr_in.sin_port = htons (port);
      }
      address_len = sizeof (addr_in);
      address = (struct sockaddr *) (&addr_in);
      if (addr_in.sin_port == 0)
        need_bind = false;
      break;
    default:
      g_warn_if_reached ();
    }

  fd = socket (protocol_family, SOCK_STREAM, 0);
  if (fd < 0)
    {
      fprintf (stderr, "pb_rpc_server_new: socket() failed: %s\n",
               strerror (errno));
      return NULL;
    }
  if (need_bind &&
      bind (fd, address, address_len) < 0)
    {
      fprintf (stderr, "pb_rpc_server_new: error binding to port: %s\n",
               strerror (errno));
      return NULL;
    }
  if (listen (fd, 255) < 0)
    {
      fprintf (stderr, "pb_rpc_server_new: listen() failed: %s\n",
               strerror (errno));
      return NULL;
    }

  return server_new_from_listening_fd (fd, type, name,
                                       service, orig_dispatch);
}

int
rig_pb_rpc_server_get_listening_fd (PB_RPC_Server *server)
{
  return server->listening_fd;
}

void
rig_pb_rpc_server_set_client_connect_handler (PB_RPC_Server *server,
                                              PB_RPC_Client_Connect_Func func,
                                              void *user_data)
{
  server->client_connect_handler = func;
  server->client_connect_handler_data = user_data;
}

void
rig_pb_rpc_server_set_client_close_handler (PB_RPC_Server *server,
                                            PB_RPC_Client_Close_Func func,
                                            void *user_data)
{
  server->client_close_handler = func;
  server->client_close_handler_data = user_data;
}

void
rig_pb_rpc_server_connection_set_close_handler (PB_RPC_ServerConnection *conn,
                                                PB_RPC_ServerConnection_Close_Func func,
                                                void *user_data)
{
  conn->close_handler = func;
  conn->close_handler_data = user_data;
}

void
rig_pb_rpc_server_connection_set_error_handler (PB_RPC_ServerConnection *conn,
                                                PB_RPC_ServerConnection_Error_Func func,
                                                void *user_data)
{
  conn->error_handler = func;
  conn->error_handler_data = user_data;
}

/* Number of proxied requests to try to grab in a single read */
#define PROXY_BUF_SIZE   256
static void
handle_proxy_pipe_readable (ProtobufC_FD fd,
                            unsigned events,
                            void *callback_data)
{
  int nread;
  PB_RPC_Server *server = callback_data;
  ProtobufCAllocator *allocator = server->allocator;
  union {
    char buf[sizeof(void*) * PROXY_BUF_SIZE];
    ProxyResponse *responses[PROXY_BUF_SIZE];
  } u;
  unsigned amt, i;

  memcpy (u.buf, server->proxy_extra_data, server->proxy_extra_data_len);
  nread = read (fd, u.buf + server->proxy_extra_data_len,
                sizeof (u.buf) - server->proxy_extra_data_len);
  if (nread <= 0)
    return; /* TODO: handle 0 and non-retryable errors separately */

  amt = server->proxy_extra_data_len + nread;
  for (i = 0; i < amt / sizeof(void*); i++)
    {
      ProxyResponse *pr = u.responses[i];
      ServerRequest *request = pr->request;
      if (request->conn == NULL)
        {
          /* defunct request */
          allocator->free (allocator, request);
        }
      else
        {
          PB_RPC_ServerConnection *conn = request->conn;
          rig_protobuf_c_data_buffer_append (&conn->stream->outgoing,
                                             (uint8_t*)(pr+1), pr->len);
          update_stream_fd_watch (conn->stream);

          GSK_LIST_REMOVE (GET_PENDING_REQUEST_LIST (conn), request);
          conn->n_pending_requests--;

          free_server_request (conn->server, request);
        }
      allocator->free (allocator, pr);
    }
  memcpy (server->proxy_extra_data,
          u.buf + i * sizeof(void*), amt - i * sizeof(void*));
  server->proxy_extra_data_len = amt - i * sizeof(void*);
}

void
rig_pb_rpc_server_configure_threading (PB_RPC_Server *server,
                                       PB_RPC_IsRpcThreadFunc func,
                                       void *is_rpc_data)
{
  server->is_rpc_thread_func = func;
  server->is_rpc_thread_data = is_rpc_data;

retry_pipe:
  if (pipe (server->proxy_pipe) < 0)
    {
      if (errno == EINTR || errno == EAGAIN)
        goto retry_pipe;
      server_failed_literal (server, PB_RPC_ERROR_CODE_PROXY_PROBLEM,
                             "error creating pipe for thread-proxying");
      return;
    }

  /* make the read side non-blocking, since we will use it from the main-loop;
     leave the write side blocking, since it will be used from foreign threads */
  set_fd_nonblocking (server->proxy_pipe[0]);
  rig_protobuf_c_dispatch_watch_fd (server->dispatch, server->proxy_pipe[0],
                                PROTOBUF_C_EVENT_READABLE,
                                handle_proxy_pipe_readable, server);
}

void
rig_pb_rpc_server_set_error_handler (PB_RPC_Server *server,
                                     PB_RPC_Error_Func func,
                                     void *error_func_data)
{
  server->error_handler = func;
  server->error_handler_data = error_func_data;
}

void
rig_pb_rpc_server_connection_set_data (PB_RPC_ServerConnection *conn,
                                       void *user_data)
{
  conn->user_data = user_data;
}

void *
rig_pb_rpc_closure_get_connection_data (void *closure_data)
{
  ServerRequest *request = closure_data;
  return request->conn->user_data;
}

static void
_rig_pb_rpc_peer_free (void *object)
{
  PB_RPC_Peer *peer = object;

  if (peer->idle)
    rig_protobuf_c_dispatch_remove_idle (peer->idle);

  rut_object_unref (peer->server);
  rut_object_unref (peer->client);

  rut_object_unref (peer->stream);

  rut_object_free (PB_RPC_Peer, peer);
}

static RutType rig_pb_rpc_peer_type;

static void
_rig_pb_rpc_peer_init_type (void)
{
  rut_type_init (&rig_pb_rpc_peer_type, "PB_RPC_Peer", _rig_pb_rpc_peer_free);
}

static void
handle_peer_connect_idle (RigProtobufCDispatch *dispatch,
                          void *data)
{
  PB_RPC_Peer *peer = data;
  Stream *stream = peer->stream;
  PB_RPC_Server *server = peer->server;
  PB_RPC_Client *client = peer->client;

  server_add_connection_with_stream (server, stream);

  g_warn_if_fail (client->state == PB_RPC_CLIENT_STATE_INIT);
  set_state_connected (client);

  peer->idle = NULL;
}

PB_RPC_Peer *
rig_pb_rpc_peer_new (int fd,
                     ProtobufCService *server_service,
                     const ProtobufCServiceDescriptor *client_descriptor,
                     RigProtobufCDispatch *orig_dispatch)
{
  RigProtobufCDispatch *dispatch =
    orig_dispatch ? orig_dispatch : rig_protobuf_c_dispatch_default ();
  PB_RPC_Peer *peer = rut_object_alloc0 (PB_RPC_Peer,
                                         &rig_pb_rpc_peer_type,
                                         _rig_pb_rpc_peer_init_type);


  peer->stream = stream_new (dispatch, fd);

  peer->server = server_new (server_service, dispatch);
  peer->client = client_new (client_descriptor, dispatch, peer->stream);

  /* Note: we actually defer connecting the server and client to an
   * idle handler because we want to give a chance for the user to
   * configure them such as by setting _connect_handlers...
   */

  peer->idle =
    rig_protobuf_c_dispatch_add_idle (dispatch,
                                      handle_peer_connect_idle, peer);

  return peer;
}

PB_RPC_Server *
rig_pb_rpc_peer_get_server (PB_RPC_Peer *peer)
{
  return peer->server;
}

PB_RPC_Client *
rig_pb_rpc_peer_get_client (PB_RPC_Peer *peer)
{
  return peer->client;
}
