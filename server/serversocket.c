
#include "serversocket.h"
#include "gkrellmd.h"
#include "gkrellmd-private.h"
#include "client.h"
#include <string.h>


static gboolean
gk_resolve_hostname(GkrellmdClient *client)
	{
	g_assert(client);
	g_assert(!client->hostname); // Cannot resolve more than once

	gkrellm_debug(DEBUG_SERVER, "Resolving hostname for client connection\n");

	// TODO: handle unix socket address
	GInetSocketAddress *inet_sock_addr = gkrellmd_client_get_inet_socket_address(client);
	if (!inet_sock_addr)
		{
		g_warning("Failed to retrieve inet socket address for client\n");
		return FALSE;
		}

	GInetAddress *inet_addr = g_inet_socket_address_get_address(inet_sock_addr);
	g_assert(inet_addr); // docs say this never fails

	GResolver *resolver = g_resolver_get_default();
	g_assert(resolver); // GIO should always have a resolver

	GError *err = NULL;
	client->hostname = g_resolver_lookup_by_address(resolver, inet_addr, NULL, &err);
	if (err)
		{
		// Continue with just the IP
		client->hostname = g_inet_address_to_string(inet_addr);
		g_warning("Address lookup for client %s failed: %s\n",
				gkrellmd_client_get_hostname(client), err->message);
		g_error_free(err);
		}
	else if (_GK.debug_level & DEBUG_SERVER)
		{
		gchar *addr_str = g_inet_address_to_string(inet_addr);
		gkrellm_debug(DEBUG_SERVER, "Client %s has hostname %s\n",
				addr_str, gkrellmd_client_get_hostname(client));
		g_free(addr_str);
		}

	g_object_unref(resolver);
	g_object_unref(inet_sock_addr);
	return TRUE;
	}

static gint
cmp_inet_addr(gconstpointer haystack_item, gconstpointer needle)
	{
	GInetAddress *item_inet_addr = (GInetAddress*)haystack_item;
	gsize item_size = g_inet_address_get_native_size(item_inet_addr);

	GInetAddress *needle_inet_addr = (GInetAddress*)needle;
	gsize needle_size = g_inet_address_get_native_size(needle_inet_addr);

	if (item_size == needle_size)
		{
		const guint8 *item_bytes = g_inet_address_to_bytes(item_inet_addr);
		const guint8 *needle_bytes = g_inet_address_to_bytes(needle_inet_addr);
		if (item_bytes && needle_bytes)
			{
			return memcmp(item_bytes, needle_bytes, needle_size);
			}
		}
	return -1; // dont care about order
	}

static gboolean
gk_check_reverse_address(GkrellmdClient *client)
	{
	g_assert(client);

	gboolean res;

	const gchar *client_hostname = gkrellmd_client_get_hostname(client);

	gkrellm_debug(DEBUG_SERVER, "Checking reverse lookup for client %s\n",
			client_hostname);

	// TODO: handle unix socket address
	GInetSocketAddress *inet_sock_addr = gkrellmd_client_get_inet_socket_address(client);
	if (!inet_sock_addr)
		{
		g_warning("Failed to retrieve inet socket address for client\n");
		return FALSE;
		}
	GInetAddress *inet_addr = g_inet_socket_address_get_address(inet_sock_addr);
	g_assert(inet_addr); // docs say this never fails

	GResolver *resolver = g_resolver_get_default();
	g_assert(resolver);

	GError *err = NULL;
	GList *address_list = g_resolver_lookup_by_name(resolver,
			client_hostname, NULL, &err);
	if (err)
		{
		g_warning("Reverse lookup for client %s failed: %s\n",
				client_hostname, err->message);
		g_error_free(err);
		res = FALSE;
		}
	else
		{
		// Find client GInetAddress in address list
		res = g_list_find_custom(address_list, (gconstpointer)inet_addr,
				cmp_inet_addr) != NULL;
		g_resolver_free_addresses(address_list);
		}

	g_object_unref(resolver);
	return res;
	}

static gboolean
gk_check_client_access(GkrellmdClient *client)
	{
	gkrellm_debug(DEBUG_SERVER, "Checking access for client connection\n");
	// TODO: implement me
	return TRUE;
	}


static void
gk_read_helo(GkrellmdClient *client, GString *str, gpointer user_data)
	{
	gchar *line;
	gchar name[32];

	g_assert(client);
	g_assert(str);
	g_assert(user_data);

	line = gkrellm_gstring_get_line(str);
	if (!line)
		return // incomplete line

	gkrellm_debug(DEBUG_SERVER, "Client helo line: '%s'\n", line);

	if (sscanf(line, "%31s %d.%d.%d", name, &client->major_version,
				&client->minor_version, &client->rev_version) != 4
		|| g_strcmp0(name, "gkrellm") != 0)
		{
		g_warning(_("Bad connect line from %s: %s\n"),
				gkrellmd_client_get_hostname(client), line);
		gkrellmd_send_to_client(client, "<error>\nBad connect string!");
		gkrellmd_client_set_read_callback(client, NULL, NULL);
		client->alive = FALSE;
		gkrellmd_client_close(client);
		}
	else
		{
		// Send initial information about monitors
		gkrellmd_serve_setup(client);

		// Add to client list
		GkServerSocket *serversocket = (GkServerSocket*)user_data;
		serversocket->client_list = g_list_prepend(serversocket->client_list,
				client);
		gkrellmd_client_list = serversocket->client_list; // TODO: remove global var

		// Forward client data to monitors
		gkrellmd_client_set_read_callback(client, gkrellmd_monitor_read_client,
				serversocket);

		g_message(_("Accepted client %s\n"),
				gkrellmd_client_get_hostname(client));
		}

	g_free(line);
	}

static void
gk_free_and_remove_client(GkrellmdClient *client, gpointer user_data)
{
	GkServerSocket *serversocket = (GkServerSocket*)user_data;

	gkrellm_debug(DEBUG_SERVER, "Removing client %s\n",
			gkrellmd_client_get_hostname(client));

	// Remove client from list
	serversocket->client_list = g_list_remove(serversocket->client_list,
			(gconstpointer)client);
	gkrellmd_client_list = serversocket->client_list; // TODO: remove global var

	// Make sure we're not called again somehow
	gkrellmd_client_set_close_callback(client, NULL, NULL);
	gkrellmd_client_set_read_callback(client, NULL, NULL);
	gkrellmd_client_set_resolve_callback(client, NULL, NULL);

	gkrellmd_client_unref(client);
}


static void
gk_finish_client_check(GkrellmdClient *client, gboolean success, gpointer user_data)
	{
	GkServerSocket *self = (GkServerSocket*)user_data;

	if (!gk_check_client_access(client))
		{
		g_message(_("Rejecting client %s, client access denied\n"),
				gkrellmd_client_get_hostname(client));
		gkrellmd_client_send_printf(client,
				"<error>\nConnection not allowed from %s\n",
				gkrellmd_client_get_hostname(client));
		gkrellmd_client_close(client);
		}
	else
		{
		gkrellmd_client_set_read_callback(client, gk_read_helo, self);
		}
	}


static gboolean
gk_serversocket_incoming(GSocketService *service, GSocketConnection *connection,
		GObject *source_object, gpointer user_data)
	{
	GkServerSocket *self = (GkServerSocket*)user_data;

	gkrellm_debug(DEBUG_SERVER, "Incoming client connection\n");

	GkrellmdClient *client = gkrellmd_client_new(connection);
	gkrellmd_client_set_close_callback(client, gk_free_and_remove_client, self);

	if (g_list_length(self->client_list) >= _GK.max_clients)
		{
		g_message(_("Rejecting client %s, connection limit (%d) reached\n"),
				gkrellmd_client_get_hostname(client), _GK.max_clients);
		gkrellmd_client_send(client, "<error>\nClient limit exceeded.\n");
		gkrellmd_client_close(client);
		return TRUE;
		}

	gkrellmd_client_set_resolve_callback(client, gk_finish_client_check, self);
	gkrellmd_client_resolve(client); // start async name resolving
	return TRUE;
	}


GkServerSocket *
gkrellmd_serversocket_new()
	{
	GkServerSocket *sock = g_new(GkServerSocket, 1);
	g_assert(gkrellmd_client_list == NULL); // TODO: remove global var
	sock->client_list = NULL;
	sock->service = g_socket_service_new();
	return sock;
	}


gboolean
gkrellmd_serversocket_setup(GkServerSocket *socket)
	{
	g_assert(socket);
	g_assert(socket->service);

	GError *err = NULL;

	gkrellm_debug(DEBUG_SERVER, "Setting up listening socket\n");

	// TODO: Configurable port and listening addresses
	g_socket_listener_add_inet_port(G_SOCKET_LISTENER(socket->service),
			19150, NULL, &err);
	if (err)
		{
		g_warning("Setting up listening socket failed: %s\n", err->message);
		g_error_free(err);
		return FALSE;
		}

	g_signal_connect(G_OBJECT(socket->service), "incoming::",
			G_CALLBACK(gk_serversocket_incoming), socket);

	g_socket_service_start(socket->service);

	return TRUE;
	}

static void
free_client(gpointer data)
	{
	// Make sure we're not called again somehow
	gkrellmd_client_set_close_callback((GkrellmdClient*)data, NULL, NULL);
	gkrellmd_client_unref((GkrellmdClient*)data);
	}

void
gkrellmd_serversocket_free(GkServerSocket *socket)
	{
	if (socket == NULL)
		return;

	if (socket->service)
		{
		gkrellm_debug(DEBUG_SERVER, "Cleaning up listening socket(s)\n");
		if (g_socket_service_is_active(socket->service))
			g_socket_service_stop(socket->service);
		g_object_unref(socket->service);
		}
	gkrellmd_client_list = NULL; // TODO: remove global var
	if (socket->client_list)
		{
		gkrellm_debug(DEBUG_SERVER, "Cleaning up list of connected clients\n");
		g_list_free_full(socket->client_list, free_client);
		}
	g_free(socket);
	}
