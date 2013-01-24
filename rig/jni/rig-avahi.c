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

#include <glib.h>

#include <avahi-client/client.h>
#include <avahi-client/publish.h>

#include <avahi-common/alternative.h>
#include <avahi-common/error.h>
#include <avahi-common/timeval.h>

#include <avahi-glib/glib-watch.h>
#include <avahi-glib/glib-malloc.h>

#include <rig-data.h>

static void
create_service (RigData *data);

static void
entry_group_callback (AvahiEntryGroup *group,
                      AvahiEntryGroupState state,
                      void *user_data)
{
  RigData *data = user_data;

  /* Called whenever the entry group state changes */

  switch (state)
    {
      case AVAHI_ENTRY_GROUP_ESTABLISHED:

        /* The entry group has been established successfully */
        g_message ("Service '%s' successfully established.\n",
                   data->avahi_service_name);
        break;

      case AVAHI_ENTRY_GROUP_COLLISION:
      {
        char *new_name;

        /* A service name collision with a remote service happened. Let's pick
         * a new name */
        new_name = avahi_alternative_service_name (data->avahi_service_name);
        avahi_free (data->avahi_service_name);
        data->avahi_service_name = new_name;

        g_warning ("Avahi service name collision, renaming service to '%s'\n",
                   data->avahi_service_name);

        /* And recreate the service */
        create_service (data);
        break;
      }

      case AVAHI_ENTRY_GROUP_FAILURE :
      {
        AvahiClient *client = avahi_entry_group_get_client (group);

        g_warning ("Avahi Entry group failure: %s\n",
                   avahi_strerror(avahi_client_errno (client)));

        /* Some kind of failure happened while we were registering our services */

        /* XXX: What should we do in this case? */
        break;
      }

      case AVAHI_ENTRY_GROUP_UNCOMMITED:
      case AVAHI_ENTRY_GROUP_REGISTERING:
          ;
    }
}

static void
create_service (RigData *data)
{
  AvahiClient *client = data->avahi_client;
  char *new_name;
  int ret;

  g_return_if_fail (client);

  /* If this is the first time we're called, let's create a new entry group if
   * necessary */

  if (!data->avahi_group)
    {
      data->avahi_group =
        avahi_entry_group_new (client, entry_group_callback, data);

      if (!data->avahi_group)
        {
          g_warning ("Failed create Avahi group: %s",
                     avahi_strerror (avahi_client_errno (client)));
          return;
        }
    }

  /* If the group is empty (either because it was just created, or
   * because it was reset previously, add our entries.  */

  if (avahi_entry_group_is_empty (data->avahi_group))
    {
      g_message ("Adding Avahi service '%s'\n", data->avahi_service_name);

      if ((ret = avahi_entry_group_add_service (data->avahi_group,
                                                AVAHI_IF_UNSPEC,
                                                AVAHI_PROTO_UNSPEC,
                                                0,
                                                data->avahi_service_name,
                                                "_rig._tcp",
#warning "FIXME: hacky 12345 port number"
                                                NULL, NULL, 12345 /* FIXME: port */,
                                                NULL)) < 0)
        {
          if (ret == AVAHI_ERR_COLLISION)
            goto collision;

          g_warning ("Failed to add _ipp._tcp service: %s\n",
                     avahi_strerror (ret));
          return;
        }

      /* Tell the server to register the service */
      if ((ret = avahi_entry_group_commit (data->avahi_group)) < 0)
        {
          g_warning ("Failed to commit entry group: %s\n",
                     avahi_strerror (ret));
          return;
        }
  }

  return;

collision:

  /* A service name collision with a local service happened. Let's
   * pick a new name */
  new_name = avahi_alternative_service_name (data->avahi_service_name);
  avahi_free (data->avahi_service_name);
  data->avahi_service_name = new_name;

  g_warning ("Service name collision, renaming service to '%s'\n", new_name);

  avahi_entry_group_reset (data->avahi_group);

  create_service (data);
  return;
}

static void
client_callback (AvahiClient *client,
                 AvahiClientState state,
                 void *user_data)
{
  RigData *data = user_data;

  /* Note: this callback may be called before avahi_client_new()
   * returns, before we would otherwise initialize data->avahi_client.
   */
  data->avahi_client = client;

  /* Called whenever the client or server state changes */

  switch (state)
    {
    case AVAHI_CLIENT_S_RUNNING:

      /* The server has startup successfully and registered its host
       * name on the network, so it's time to create our services */
      create_service (data);
      break;

    case AVAHI_CLIENT_FAILURE:

      g_message ("Avahi client failure: %s",
                 avahi_strerror (avahi_client_errno (client)));

      /* XXX: what should we do?
       *
       * We could install a timeout that tries re-initializing avahi from
       * scratch?
       */

      break;

    case AVAHI_CLIENT_S_COLLISION:

      /* Let's drop our registered services. When the server is back
       * in AVAHI_SERVER_RUNNING state we will register them
       * again with the new host name. */

    case AVAHI_CLIENT_S_REGISTERING:

      /* The server records are now being established. This
       * might be caused by a host name change. We need to wait
       * for our own records to register until the host name is
       * properly esatblished. */

      if (data->avahi_group)
        avahi_entry_group_reset (data->avahi_group);

      break;

    case AVAHI_CLIENT_CONNECTING:
      ;
    }
}

void
rig_avahi_register_service (RigData *data)
{
  const AvahiPoll *poll_api;
  AvahiGLibPoll *glib_poll;
  AvahiClient *client;
  int error;

  if (!data->avahi_service_name)
    data->avahi_service_name = g_strdup ("Rig Preview");

  avahi_set_allocator (avahi_glib_allocator ());

  /* Note: An AvahiGlibPoll is a GSource, but note that avahi_glib_poll_new()
   * will automatically add the source to the GMainContext so we don't have to
   * do that explicitly.
   */
  glib_poll = avahi_glib_poll_new (NULL, G_PRIORITY_DEFAULT);
  poll_api = avahi_glib_poll_get (glib_poll);

  client = avahi_client_new (poll_api,
                             0,
                             client_callback,
                             data,
                             &error);
  if (client == NULL)
    {
      g_warning ("Error initializing Avahi: %s", avahi_strerror (error));

      avahi_client_free (client);
      avahi_glib_poll_free (glib_poll);
      return;
    }

  data->avahi_client = client;
  data->avahi_poll_api = poll_api;
}
