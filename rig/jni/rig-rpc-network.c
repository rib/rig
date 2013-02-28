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
 * Note: Some small parts were copied from the avahi examples although
 * those files have don't name any copyright owners.
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

#include "protobuf-c/rig-protobuf-c-rpc.h"

#include "rig-rpc-network.h"
#include "rig-data.h"
#include "rig-avahi.h"
#include "rig.pb-c.h"

typedef struct _ProtobufSource
{
  GSource source;

  RigData *data;

  ProtobufCDispatch *dispatch;

  PB_RPC_Client *pb_client;

  CoglBool pollfds_changed;

  int n_pollfds;
  GPollFD *pollfds;

  RigRPCClientConnectCallback connected;
  void *connected_data;

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
  ProtobufCDispatch *dispatch = protobuf_source->dispatch;

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
  ProtobufCDispatch *dispatch = protobuf_source->dispatch;
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
  ProtobufCDispatch *dispatch = protobuf_source->dispatch;
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
  ProtobufCDispatch *dispatch = protobuf_source->dispatch;

  if (protobuf_source->pb_client &&
      rig_pb_rpc_client_is_connected (protobuf_source->pb_client))
    {
      return TRUE;
    }

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
  ProtobufCDispatch *dispatch = protobuf_source->dispatch;
  int i;

  /* XXX: when we call protobuf_c_dispatch_dispatch() that will clear
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

  if (protobuf_source->pb_client &&
      rig_pb_rpc_client_is_connected (protobuf_source->pb_client))
    {
      if (protobuf_source->connected)
        {
          protobuf_source->connected (protobuf_source->pb_client,
                                      protobuf_source->connected_data);
          protobuf_source->connected = NULL;
        }
    }

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

  protobuf_c_dispatch_dispatch (protobuf_source->dispatch, n_events, events);

  /* XXX: PROTOBUF-C BUG?
   *
   * protobuf_c_dispatch_dispatch can return with dispatch->n_changes
   * == 0 even though the list of notifies may have changed during the
   * dispatch itself which means we have to resort to explicitly comparing
   * the gpollfds with dispatch->notifies_desired which is obviously
   * not ideal!
   */
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
protobuf_source_new (RigData *data,
                     ProtobufCDispatch *dispatch)
{
  GSource *source = g_source_new (&protobuf_source_funcs,
                                  sizeof (ProtobufSource));
  ProtobufSource *protobuf_source = (ProtobufSource *)source;

  protobuf_source->dispatch = dispatch;

  protobuf_source->data = data;

  return source;
}

void
rig_rpc_stop_server (RigData *data)
{
  g_return_if_fail (data->rpc_server != NULL);

  g_warning ("Stopping RPC server");

  rig_pb_rpc_server_destroy (data->rpc_server, TRUE);
  data->rpc_server = NULL;

  rig_avahi_unregister_service (data);

  g_source_remove (data->rpc_server_source_id);
}

void
rig_rpc_start_server (RigData *data,
                      ProtobufCService *service,
                      PB_RPC_Error_Func server_error_handler,
                      PB_RPC_Client_Connect_Func new_client_handler,
                      void *user_data)
{
  GSource *source;
  int fd;
  struct sockaddr_in addr;
  socklen_t addr_len = sizeof (addr);
  ProtobufCDispatch *dispatch =
    protobuf_c_dispatch_new (&protobuf_c_default_allocator);

  data->rpc_server =
    rig_pb_rpc_server_new (PROTOBUF_C_RPC_ADDRESS_TCP,
                           "0",
                           service,
                           dispatch);

  fd = rig_pb_rpc_server_get_fd (data->rpc_server);
  getsockname (fd, (struct sockaddr *)&addr, &addr_len);

  if (addr.sin_family == AF_INET)
    data->rpc_server_port = ntohs (addr.sin_port);
  else
    data->rpc_server_port = 0;

  rig_pb_rpc_server_set_error_handler (data->rpc_server,
                                       server_error_handler,
                                       user_data);

  rig_pb_rpc_server_set_client_connect_handler (data->rpc_server,
                                                new_client_handler,
                                                user_data);

  source = protobuf_source_new (data, dispatch);
  data->rpc_server_source_id = g_source_attach (source, NULL);

  rig_avahi_register_service (data);
}

static void
_rig_rpc_client_free (void *object)
{
  RigRPCClient *rpc_client = object;

  rig_rpc_client_disconnect (rpc_client);

  g_free (rpc_client->hostname);

  g_slice_free (RigRPCClient, rpc_client);
}

static RutType rig_rpc_client_type;

static void
_rig_rpc_client_init_type (void)
{
  static RutRefCountableVTable refable_vtable = {
      rut_refable_simple_ref,
      rut_refable_simple_unref,
      _rig_rpc_client_free
  };

  RutType *type = &rig_rpc_client_type;
#define TYPE RigRPCClient

  rut_type_init (type, G_STRINGIFY (TYPE));
  rut_type_add_interface (type,
                          RUT_INTERFACE_ID_REF_COUNTABLE,
                          offsetof (TYPE, ref_count),
                          &refable_vtable);

#undef TYPE
}

RigRPCClient *
rig_rpc_client_new (RigData *data,
                    const char *hostname,
                    int port,
                    ProtobufCServiceDescriptor *descriptor,
                    PB_RPC_Error_Func client_error_handler,
                    RigRPCClientConnectCallback connected,
                    void *user_data)
{
  RigRPCClient *rpc_client = g_slice_new (RigRPCClient);
  char *addr_str = g_strdup_printf ("%s:%d", hostname, port);
  ProtobufCDispatch *dispatch;
  PB_RPC_Client *pb_client;
  GSource *source;
  ProtobufSource *protobuf_source;

  static CoglBool initialized = FALSE;

  if (initialized == FALSE)
    {
      _rig_rpc_client_init_type ();
      initialized = TRUE;
    }

  rut_object_init (&rpc_client->_parent, &rig_rpc_client_type);

  rpc_client->ref_count = 1;

  rpc_client->hostname = g_strdup (hostname);
  rpc_client->port = port;

  dispatch = protobuf_c_dispatch_new (&protobuf_c_default_allocator);

  pb_client = (PB_RPC_Client *)
    rig_pb_rpc_client_new (PROTOBUF_C_RPC_ADDRESS_TCP,
                           addr_str,
                           descriptor,
                           dispatch);

  g_free (addr_str);

  source = protobuf_source_new (data, dispatch);
  protobuf_source = (ProtobufSource *)source;

  /* FIXME: We should make rig_pb_rpc_client_new take a
   * connected_handler and user_data instead... */
  protobuf_source->pb_client = pb_client;
  protobuf_source->connected = connected;
  protobuf_source->connected_data = user_data;

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

  protobuf_c_service_destroy ((ProtobufCService *)rpc_client->pb_rpc_client);
  rpc_client->pb_rpc_client = NULL;
}
