/*
 * Copyright (c) 2008-2011, Dave Benson.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with
 * or without modification, are permitted provided that the
 * following conditions are met:
 *
 * Redistributions of source code must retain the above
 * copyright notice, this list of conditions and the following
 * disclaimer.

 * Redistributions in binary form must reproduce
 * the above copyright notice, this list of conditions and
 * the following disclaimer in the documentation and/or other
 * materials provided with the distribution.
 *
 * Neither the name
 * of "protobuf-c" nor the names of its contributors
 * may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Rig
 *
 * Copyright (C) 2013  Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 */

#include <config.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <glib.h>

#include "protobuf-c-rpc/rig-protobuf-c-rpc.h"

#include "rig-rpc-network.h"
#include "rig.pb-c.h"

typedef struct _ProtobufSource
{
  GSource source;

  RigProtobufCDispatch *dispatch;

  CoglBool pollfds_changed;

  int n_pollfds;
  GPollFD *pollfds;

} ProtobufSource;

static unsigned int
protobuf_events_to_gpollfd_events (unsigned int events)
{
  return ((events & PROTOBUF_C_EVENT_READABLE) ? G_IO_IN : 0) |
    ((events & PROTOBUF_C_EVENT_WRITABLE) ? G_IO_OUT : 0);
}

static unsigned int
gpollfd_events_to_protobuf_events (unsigned int events)
{
  return ((events & G_IO_IN) ? PROTOBUF_C_EVENT_READABLE : 0) |
    ((events & G_IO_OUT) ? PROTOBUF_C_EVENT_WRITABLE : 0);
}

static int
get_timeout (ProtobufSource *protobuf_source)
{
  RigProtobufCDispatch *dispatch = protobuf_source->dispatch;

  if (dispatch->has_timeout)
    {
      struct timeval tv;
      gettimeofday (&tv, NULL);

      if (dispatch->timeout_secs < (unsigned long) tv.tv_sec ||
          (dispatch->timeout_secs == (unsigned long) tv.tv_sec &&
           dispatch->timeout_usecs <= (unsigned) tv.tv_usec))
        {
          return 0;
        }
      else
        {
          int du = dispatch->timeout_usecs - tv.tv_usec;
          int ds = dispatch->timeout_secs - tv.tv_sec;
          if (du < 0)
            {
              du += 1000000;
              ds -= 1;
            }
          if (ds > INT_MAX / 1000)
            return INT_MAX / 1000 * 1000;
          else
            /* Round up, so that we ensure that something can run
               if they just wait the full duration */
            return ds * 1000 + (du + 999) / 1000;
        }
    }

  return -1;
}

static CoglBool
pollfds_up_to_date (ProtobufSource *protobuf_source)
{
  RigProtobufCDispatch *dispatch = protobuf_source->dispatch;
  int i;

  if (dispatch->n_notifies_desired != protobuf_source->n_pollfds)
    return FALSE;

  for (i = 0; i < protobuf_source->n_pollfds; i++)
    {
      ProtobufC_FDNotify *notify = &dispatch->notifies_desired[i];
      GPollFD *pollfd = &protobuf_source->pollfds[i];

      if (notify->fd != pollfd->fd)
        return FALSE;

      if (protobuf_events_to_gpollfd_events (notify->events) !=
          pollfd->events)
        return FALSE;
    }

  return TRUE;
}

static void
sync_pollfds (ProtobufSource *protobuf_source)
{
  RigProtobufCDispatch *dispatch = protobuf_source->dispatch;
  GSource *source = &protobuf_source->source;
  int i;

  if (pollfds_up_to_date (protobuf_source))
    return;

  for (i = 0; i < protobuf_source->n_pollfds; i++)
    g_source_remove_poll (source, &protobuf_source->pollfds[i]);
  g_free (protobuf_source->pollfds);

  protobuf_source->n_pollfds = dispatch->n_notifies_desired;
  protobuf_source->pollfds = g_new (GPollFD, dispatch->n_notifies_desired);
  for (i = 0; i < protobuf_source->n_pollfds; i++)
    {
      protobuf_source->pollfds[i].fd = dispatch->notifies_desired[i].fd;
      protobuf_source->pollfds[i].events =
        protobuf_events_to_gpollfd_events (dispatch->notifies_desired[i].events);

      g_source_add_poll (source, &protobuf_source->pollfds[i]);
    }
}

static gboolean
protobuf_source_prepare (GSource *source, int *timeout)
{
  ProtobufSource *protobuf_source = (ProtobufSource *) source;
  RigProtobufCDispatch *dispatch = protobuf_source->dispatch;

  *timeout = get_timeout (protobuf_source);
  if (*timeout == 0)
    return TRUE;

  if (protobuf_source->pollfds == NULL ||
      protobuf_source->pollfds_changed ||
      dispatch->n_changes)
    {
      /* XXX: it's possible that we could hit this path redundantly if
       * some other GSource reports that it can dispatch immediately
       * and so we will be asked to _prepare() again later and
       * dispatch->n_changes will still be set since we don't have
       * any api to clear the dispatch->changes[].
       */

      sync_pollfds (protobuf_source);
    }

  protobuf_source->pollfds_changed = FALSE;

  return FALSE;
}

static gboolean
protobuf_source_check (GSource *source)
{
  ProtobufSource *protobuf_source = (ProtobufSource *) source;
  RigProtobufCDispatch *dispatch = protobuf_source->dispatch;
  int i;

  /* XXX: when we call rig_protobuf_c_dispatch_dispatch() that will clear
   * dispatch->changes[] and so we make sure to check first if there
   * have been changes made to the pollfds so later when we prepare
   * to poll again we can update the GSource pollfds.
   */
  if (dispatch->n_changes)
    protobuf_source->pollfds_changed = TRUE;

  if (dispatch->has_idle)
    return TRUE;

  for (i = 0; i < protobuf_source->n_pollfds; i++)
    if (protobuf_source->pollfds[i].revents)
      return TRUE;

  if (get_timeout (protobuf_source) == 0)
    return TRUE;

  return FALSE;
}

static gboolean
protobuf_source_dispatch (GSource *source,
                          GSourceFunc callback,
                          void *user_data)
{
  ProtobufSource *protobuf_source = (ProtobufSource *) source;
  GPollFD *gpollfds = protobuf_source->pollfds;
  int i;
  int n_events;
  ProtobufC_FDNotify *events;
  void *to_free = NULL;

  n_events = 0;
  for (i = 0; i < protobuf_source->n_pollfds; i++)
    if (gpollfds[i].revents)
      n_events++;

  if (n_events < 128)
    events = alloca (sizeof (ProtobufC_FDNotify) * n_events);
  else
    to_free = events = g_new (ProtobufC_FDNotify, n_events);

  n_events = 0;
  for (i = 0; i < protobuf_source->n_pollfds; i++)
    if (gpollfds[i].revents)
      {
        events[n_events].fd = gpollfds[i].fd;
        events[n_events].events =
          gpollfd_events_to_protobuf_events (gpollfds[i].revents);

        /* note that we may actually wind up with fewer events after
         * calling gpollfd_events_to_protobuf_events() */
        if (events[n_events].events != 0)
          n_events++;
      }

  rig_protobuf_c_dispatch_dispatch (protobuf_source->dispatch, n_events, events);

  /* XXX: PROTOBUF-C BUG?
   *
   * rig_protobuf_c_dispatch_dispatch can return with dispatch->n_changes
   * == 0 even though the list of notifies may have changed during the
   * dispatch itself which means we have to resort to explicitly comparing
   * the gpollfds with dispatch->notifies_desired which is obviously
   * not ideal!
   */
  if (!g_source_is_destroyed (source))
    sync_pollfds (protobuf_source);

  if (to_free)
    g_free (to_free);

  return TRUE;
}

static GSourceFuncs
protobuf_source_funcs =
  {
    protobuf_source_prepare,
    protobuf_source_check,
    protobuf_source_dispatch,
    NULL
  };

static GSource *
protobuf_source_new (RigProtobufCDispatch *dispatch)
{
  GSource *source = g_source_new (&protobuf_source_funcs,
                                  sizeof (ProtobufSource));
  ProtobufSource *protobuf_source = (ProtobufSource *)source;

  protobuf_source->dispatch = dispatch;

  return source;
}

void
rig_rpc_server_shutdown (RigRPCServer *server)
{
  g_warning ("Stopping RPC server");

#warning "todo: explicitly shutdown via new rig_pb_rpc_server_shutdown() api"
  rut_object_unref (server->pb_rpc_server);
  server->pb_rpc_server = NULL;

  g_source_remove (server->source_id);
}

static void
_rig_rpc_server_free (void *object)
{
  RigRPCServer *server = object;

  rig_rpc_server_shutdown (server);

  rut_object_free (RigRPCServer, server);
}

static RutType rig_rpc_server_type;

static void
_rig_rpc_server_init_type (void)
{

  RutType *type = &rig_rpc_server_type;
#define TYPE RigRPCServer

  rut_type_init (type, G_STRINGIFY (TYPE), _rig_rpc_server_free);

#undef TYPE
}

RigRPCServer *
rig_rpc_server_new (ProtobufCService *service,
                    PB_RPC_Error_Func server_error_handler,
                    PB_RPC_Client_Connect_Func new_client_handler,
                    void *user_data)
{
  RigRPCServer *server = rut_object_alloc0 (RigRPCServer,
                                            &rig_rpc_server_type,
                                            _rig_rpc_server_init_type);
  RigProtobufCDispatch *dispatch =
    rig_protobuf_c_dispatch_new (&protobuf_c_default_allocator);
  struct sockaddr_in addr;
  socklen_t addr_len = sizeof (addr);
  int listening_fd;

  server->pb_rpc_server =
    rig_pb_rpc_server_new (PROTOBUF_C_RPC_ADDRESS_TCP,
                           "0",
                           service,
                           dispatch);

  listening_fd = rig_pb_rpc_server_get_listening_fd (server->pb_rpc_server);
  getsockname (listening_fd, (struct sockaddr *)&addr, &addr_len);

  if (addr.sin_family == AF_INET)
    server->port = ntohs (addr.sin_port);
  else
    server->port = 0;

  rig_pb_rpc_server_set_error_handler (server->pb_rpc_server,
                                       server_error_handler,
                                       user_data);

  rig_pb_rpc_server_set_client_connect_handler (server->pb_rpc_server,
                                                new_client_handler,
                                                user_data);

  server->protobuf_source = protobuf_source_new (dispatch);
  server->source_id = g_source_attach (server->protobuf_source, NULL);

  return server;
}

static void
_rig_rpc_client_free (void *object)
{
  RigRPCClient *rpc_client = object;

  rig_rpc_client_disconnect (rpc_client);

  g_free (rpc_client->hostname);

  rut_object_free (RigRPCClient, rpc_client);
}

static RutType rig_rpc_client_type;

static void
_rig_rpc_client_init_type (void)
{

  RutType *type = &rig_rpc_client_type;
#define TYPE RigRPCClient

  rut_type_init (type, G_STRINGIFY (TYPE), _rig_rpc_client_free);

#undef TYPE
}

RigRPCClient *
rig_rpc_client_new (const char *hostname,
                    int port,
                    ProtobufCServiceDescriptor *descriptor,
                    PB_RPC_Error_Func client_error_handler,
                    PB_RPC_Connect_Func connect_handler,
                    void *user_data)
{
  RigRPCClient *rpc_client = rut_object_alloc0 (RigRPCClient,
                                                &rig_rpc_client_type,
                                                _rig_rpc_client_init_type);
  char *addr_str = g_strdup_printf ("%s:%d", hostname, port);
  RigProtobufCDispatch *dispatch;
  PB_RPC_Client *pb_client;
  GSource *source;


  rpc_client->hostname = g_strdup (hostname);
  rpc_client->port = port;

  dispatch = rig_protobuf_c_dispatch_new (&protobuf_c_default_allocator);

  pb_client = (PB_RPC_Client *)
    rig_pb_rpc_client_new (PROTOBUF_C_RPC_ADDRESS_TCP,
                           addr_str,
                           descriptor,
                           dispatch);

  rig_pb_rpc_client_set_connect_handler (pb_client,
                                         connect_handler,
                                         user_data);

  g_free (addr_str);

  source = protobuf_source_new (dispatch);

  rpc_client->protobuf_source = source;

  rig_pb_rpc_client_set_error_handler (pb_client,
                                       client_error_handler, user_data);

  rpc_client->source_id = g_source_attach (source, NULL);

  rpc_client->pb_rpc_client = pb_client;

  return rpc_client;
}

void
rig_rpc_client_disconnect (RigRPCClient *rpc_client)
{
  if (!rpc_client->pb_rpc_client)
    return;

  g_source_remove (rpc_client->source_id);
  rpc_client->source_id = 0;

  g_source_unref (rpc_client->protobuf_source);
  rpc_client->protobuf_source = NULL;

#warning "TODO: need explicit rig_pb_rpc_client_disconnect() api"
  rut_object_unref (rpc_client->pb_rpc_client);
  rpc_client->pb_rpc_client = NULL;
}

static void
_rig_rpc_peer_free (void *object)
{
  RigRPCPeer *rpc_peer = object;

  rut_object_unref (rpc_peer->pb_rpc_client);
  rut_object_unref (rpc_peer->pb_rpc_server);
  rut_object_unref (rpc_peer->pb_rpc_peer);

  rut_object_free (RigRPCPeer, rpc_peer);
}

static RutType rig_rpc_peer_type;

static void
_rig_rpc_peer_init_type (void)
{

  RutType *type = &rig_rpc_peer_type;
#define TYPE RigRPCPeer

  rut_type_init (type, G_STRINGIFY (TYPE), _rig_rpc_peer_free);

#undef TYPE
}

static void
server_connect_handler (PB_RPC_Server *server,
                        PB_RPC_ServerConnection *conn,
                        void *user_data)
{
  rig_pb_rpc_server_connection_set_data (conn, user_data);
}

RigRPCPeer *
rig_rpc_peer_new (int fd,
                  ProtobufCService *server_service,
                  ProtobufCServiceDescriptor *client_descriptor,
                  PB_RPC_Error_Func peer_error_handler,
                  PB_RPC_Connect_Func connect_handler,
                  void *user_data)
{
  RigRPCPeer *rpc_peer = rut_object_alloc0 (RigRPCPeer,
                                            &rig_rpc_peer_type,
                                            _rig_rpc_peer_init_type);
  RigProtobufCDispatch *dispatch;
  PB_RPC_Peer *pb_peer;
  GSource *source;


  rpc_peer->fd = fd;

  dispatch = rig_protobuf_c_dispatch_new (&protobuf_c_default_allocator);

  pb_peer =
    rig_pb_rpc_peer_new (fd,
                         server_service,
                         client_descriptor,
                         dispatch);
  rpc_peer->pb_rpc_peer = pb_peer;

  rpc_peer->pb_rpc_client = rig_pb_rpc_peer_get_client (pb_peer);

  rig_pb_rpc_client_set_connect_handler (rpc_peer->pb_rpc_client,
                                         connect_handler,
                                         user_data);
  rig_pb_rpc_client_set_error_handler (rpc_peer->pb_rpc_client,
                                       peer_error_handler,
                                       user_data);

  rpc_peer->pb_rpc_server = rig_pb_rpc_peer_get_server (pb_peer);

  rig_pb_rpc_server_set_error_handler (rpc_peer->pb_rpc_server,
                                       peer_error_handler,
                                       user_data);

  rig_pb_rpc_server_set_client_connect_handler (rpc_peer->pb_rpc_server,
                                                server_connect_handler,
                                                user_data);

  source = protobuf_source_new (dispatch);
  rpc_peer->protobuf_source = source;
  rpc_peer->source_id = g_source_attach (source, NULL);

  return rpc_peer;
}
