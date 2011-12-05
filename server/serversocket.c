
#include "serversocket.h"
#include "gkrellmd.h"
#include "gkrellmd-private.h"
#include "client.h"
#include <string.h>

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

	// TODO: Support multiple socket listeners and Unix sockets

	if (_GK.server_address && _GK.server_address[0] != '\0')
		{
		gkrellm_debug(DEBUG_SERVER, "Adding TCP socket listener for %s:%d\n",
				_GK.server_address, _GK.server_port);

		GInetAddress *listen_inet_addr = g_inet_address_new_from_string(_GK.server_address);
		if (!listen_inet_addr)
			{
			g_warning("Setting up listening socket failed: "
					"Could not parse address %s\n", _GK.server_address);
			return FALSE;
			}

		GSocketAddress *listen_sock_addr = g_inet_socket_address_new(
				listen_inet_addr, _GK.server_port);
		g_assert(listen_sock_addr);

		g_socket_listener_add_address(G_SOCKET_LISTENER(socket->service),
				listen_sock_addr, G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_TCP,
				NULL, NULL, &err);
		}
	else
		{
		gkrellm_debug(DEBUG_SERVER,
				"Adding socket listener for TCP port %d on all interfaces\n",
				_GK.server_port);

		g_socket_listener_add_inet_port(G_SOCKET_LISTENER(socket->service),
				_GK.server_port, NULL, &err);
		}

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
