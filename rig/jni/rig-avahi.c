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
      char *user_name;
      const char *user = g_get_real_name ();

      if (strcmp (user, "Unknown") == 0)
        user = g_get_user_name ();

      user_name = g_strconcat ("user=", user, NULL);

      g_message ("Adding Avahi service '%s'\n", data->avahi_service_name);

      ret = avahi_entry_group_add_service (data->avahi_group,
                                           AVAHI_IF_UNSPEC,
                                           AVAHI_PROTO_UNSPEC,
                                           0,
                                           data->avahi_service_name,
                                           "_rig._tcp",
                                           NULL, NULL,
                                           data->network_port,
                                           "version=1.0",
                                           user_name,
                                           NULL);
      g_free (user_name);

      if (ret < 0)
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
service_client_callback (AvahiClient *client,
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
                             service_client_callback,
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

static void
resolve_callback(AvahiServiceResolver *resolver,
                 AVAHI_GCC_UNUSED AvahiIfIndex interface,
                 AVAHI_GCC_UNUSED AvahiProtocol protocol,
                 AvahiResolverEvent event,
                 const char *name,
                 const char *type,
                 const char *domain,
                 const char *host_name,
                 const AvahiAddress *address,
                 uint16_t port,
                 AvahiStringList *txt,
                 AvahiLookupResultFlags flags,
                 void* user_data)
{
  RigData *data = user_data;
  AvahiClient *client = avahi_service_resolver_get_client (resolver);

  /* Called whenever a service has been resolved successfully or timed out */

  switch (event)
    {
    case AVAHI_RESOLVER_FAILURE:

      g_warning ("(Resolver) Failed to resolve service "
                 "'%s' of type '%s' in domain '%s': %s\n",
                 name, type, domain,
                 avahi_strerror (avahi_client_errno (client)));
      break;

    case AVAHI_RESOLVER_FOUND:
    {
      char a[AVAHI_ADDRESS_STR_MAX], *t;
      RigSlaveAddress *slave_address;
      GList *l;

      g_message ("Service '%s' of type '%s' in domain '%s':\n",
                 name, type, domain);

      avahi_address_snprint (a, sizeof (a), address);
      t = avahi_string_list_to_string (txt);
      g_message (
              "\t%s:%u (%s)\n"
              "\tTXT=%s\n"
              "\tcookie is %u\n"
              "\tis_local: %i\n"
              "\tour_own: %i\n"
              "\twide_area: %i\n"
              "\tmulticast: %i\n"
              "\tcached: %i\n",
              host_name, port, a,
              t,
              avahi_string_list_get_service_cookie (txt),
              !!(flags & AVAHI_LOOKUP_RESULT_LOCAL),
              !!(flags & AVAHI_LOOKUP_RESULT_OUR_OWN),
              !!(flags & AVAHI_LOOKUP_RESULT_WIDE_AREA),
              !!(flags & AVAHI_LOOKUP_RESULT_MULTICAST),
              !!(flags & AVAHI_LOOKUP_RESULT_CACHED));

        avahi_free (t);

        slave_address = g_slice_new0 (RigSlaveAddress);
        slave_address->name = g_strdup (name);
        slave_address->hostname = g_strdup (host_name);
        slave_address->port = port;

        data->slave_addresses =
          g_list_prepend (data->slave_addresses, slave_address);

        for (l = data->slave_addresses; l; l = l->next)
          {
            RigSlaveAddress *address = l->data;
            g_print ("Slave = %s\n", address->hostname);
          }
      }
  }

  avahi_service_resolver_free (resolver);
}

static void
free_slave_address (RigSlaveAddress *slave_address)
{
  g_free (slave_address->name);
  g_free (slave_address->hostname);
  g_slice_free (RigSlaveAddress, slave_address);
}

static void
browse_callback (AvahiServiceBrowser *browser,
                 AvahiIfIndex interface,
                 AvahiProtocol protocol,
                 AvahiBrowserEvent event,
                 const char *name,
                 const char *type,
                 const char *domain,
                 AvahiLookupResultFlags flags,
                 void *user_data)
{
  RigData *data = user_data;
  AvahiClient *client = avahi_service_browser_get_client (browser);
  GList *l;

  switch (event)
    {
    case AVAHI_BROWSER_FAILURE:

      g_warning ("(Browser) %s\n",
                 avahi_strerror (avahi_client_errno (client)));
      return;

    case AVAHI_BROWSER_NEW:

      g_message ("(Browser) NEW: service '%s' of type '%s' in domain '%s'\n",
                 name, type, domain);

      /* We ignore the returned resolver object. In the callback
         function we free it. If the server is terminated before
         the callback function is called the server will free
         the resolver for us. */

      if (!(avahi_service_resolver_new (client,
                                        interface,
                                        protocol,
                                        name,
                                        type,
                                        domain,
                                        AVAHI_PROTO_UNSPEC,
                                        0,
                                        resolve_callback,
                                        data)))
        {
          g_warning ("Failed to resolve service '%s': %s\n",
                     name, avahi_strerror (avahi_client_errno (client)));
        }

      break;

    case AVAHI_BROWSER_REMOVE:

      for (l = data->slave_addresses; l; l = l->next)
        {
          RigSlaveAddress *slave_address = l->data;

          if (strcmp (slave_address->name, name) == 0)
            {
              free_slave_address (slave_address);
              data->slave_addresses =
                g_list_remove_link (data->slave_addresses, l);
              g_message ("(Browser) REMOVE: service '%s' of type '%s' "
                         "in domain '%s'\n",
                         name, type, domain);
              break;
            }
        }

      break;

    case AVAHI_BROWSER_ALL_FOR_NOW:
    case AVAHI_BROWSER_CACHE_EXHAUSTED:
      g_message ("(Browser) %s\n",
                 event == AVAHI_BROWSER_CACHE_EXHAUSTED ?
                 "CACHE_EXHAUSTED" : "ALL_FOR_NOW");
      break;
  }
}

static void
browser_client_callback (AvahiClient *client,
                         AvahiClientState state,
                         void *user_data)
{
  //RigData *data = user_data;

  if (state == AVAHI_CLIENT_FAILURE)
    {
      g_warning ("Server connection failure: %s\n",
                 avahi_strerror (avahi_client_errno (client)));

      /* XXX: what should we do?
       *
       * We could install a timeout that tries re-initializing avahi from
       * scratch?
       */
    }
}

void
rig_avahi_run_browser (RigData *data)
{
  const AvahiPoll *poll_api;
  AvahiGLibPoll *glib_poll;
  AvahiClient *client;
  AvahiServiceBrowser *browser;
  int error;

  avahi_set_allocator (avahi_glib_allocator ());

  /* Note: An AvahiGlibPoll is a GSource, but note that avahi_glib_poll_new()
   * will automatically add the source to the GMainContext so we don't have to
   * do that explicitly.
   */
  glib_poll = avahi_glib_poll_new (NULL, G_PRIORITY_DEFAULT);
  poll_api = avahi_glib_poll_get (glib_poll);

  client = avahi_client_new (poll_api,
                             0,
                             browser_client_callback,
                             data,
                             &error);
  if (client == NULL)
    {
      g_warning ("Error initializing Avahi: %s", avahi_strerror (error));

      avahi_glib_poll_free (glib_poll);
      return;
    }

  browser = avahi_service_browser_new (client,
                                       AVAHI_IF_UNSPEC,
                                       AVAHI_PROTO_UNSPEC,
                                       "_rig._tcp",
                                       NULL, 0,
                                       browse_callback,
                                       data);
  if (browser == NULL)
    {
      g_warning ("Failed to create service browser: %s\n",
                 avahi_strerror (avahi_client_errno (client)));

      avahi_client_free (client);
      avahi_glib_poll_free (glib_poll);
      return;
    }

  data->avahi_client = client;
  data->avahi_poll_api = poll_api;
  data->avahi_browser = browser;
}
