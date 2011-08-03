
#include "client.h"
#include "gkrellmd-private.h"


static gboolean
gk_client_read(GObject *pollable_stream, gpointer user_data)
	{
	GError *err = NULL;
	gchar buf[1024];
	gssize rb;

	GPollableInputStream *istream = G_POLLABLE_INPUT_STREAM(pollable_stream);
	GkrellmdClient *client = (GkrellmdClient*)user_data;

	rb = g_pollable_input_stream_read_nonblocking(istream,
			buf, sizeof(buf) - 1, NULL, &err);
	if (err)
		{
		if ((G_IO_ERROR == err->domain) && (G_IO_ERROR_WOULD_BLOCK == err->code))
			{
			// We might get a read callback although there's nothing to
			// read, just ignore that and wait for next callback
			g_error_free(err);
			return TRUE;
			}
		else
			{
			client->alive = FALSE;
			g_warning(_("Reading from client %s failed: %s\n"),
					client->hostname, err->message);
			g_error_free(err);
			return FALSE; // stops read callback for client
			}
		}

	if (rb < 1)
		{
		// No data to read but no socket error => client dropped connection
		gkrellm_debug(DEBUG_SERVER, "No data to read, dropping client %s\n",
				client->hostname);
		client->alive = FALSE;
		return FALSE;
		}

	buf[rb] = '\0';
	gkrellm_debug(DEBUG_SERVER, "gk_client_read: received %ld bytes: '%s'\n", rb, buf);
	g_string_append_len(client->input_gstring, buf, rb);

	if (client->read_func)
		{
		client->read_func(client, client->input_gstring,
				client->read_func_user_data);
		}

	return TRUE;
	}


GkrellmdClient *
gkrellmd_client_new(GSocketConnection *connection)
	{
	gkrellm_debug(DEBUG_SERVER, "gkrellmd_client_new()\n");

	GkrellmdClient	*client = g_new0(GkrellmdClient, 1);
	client->input_gstring = g_string_sized_new(0);
	client->connection = connection;
	g_object_ref(client->connection);

	GPollableInputStream *istream = G_POLLABLE_INPUT_STREAM(
			g_io_stream_get_input_stream((GIOStream*)client->connection));
	g_assert(istream);
	g_assert(g_pollable_input_stream_can_poll(istream));

	client->read_source = g_pollable_input_stream_create_source(istream, NULL);
	g_source_set_callback(client->read_source, (GSourceFunc)gk_client_read,
			(gpointer)client, NULL);
	g_source_attach(client->read_source, NULL); // TODO: default context ok?

	return client;
	}


void
gkrellmd_client_free(GkrellmdClient *client)
	{
	if (!client)
		return;

	gkrellm_debug(DEBUG_SERVER, "gkrellmd_client_free()\n");

	g_object_unref(client->connection);
	g_string_free(client->input_gstring, TRUE);
	g_free(client->hostname);
	if (!g_source_is_destroyed(client->read_source))
		g_source_destroy(client->read_source);
	g_source_unref(client->read_source);
	g_free(client);
	}


gssize
gkrellmd_client_send(GkrellmdClient *client, const gchar *str)
	{
	GError *err = NULL;
	GOutputStream *os;
	gssize wb; // bytes actually written to socket

	g_assert(client != NULL);
	g_assert(str != NULL);

	os = g_io_stream_get_output_stream((GIOStream*)client->connection);
	g_assert(os != NULL);

	wb = g_output_stream_write(os, (const void *)str, strlen(str), NULL, &err);
	if (err)
		{
		g_warning("Write to client host %s failed: %s\n",
				client->hostname, err->message);
		g_error_free(err);
		client->alive = FALSE;
		return 0;
		}

	return wb;
	}


void
gkrellmd_client_set_read_callback(GkrellmdClient *client,
		GkrellmdClientReadFunc func, gpointer user_data)
	{
	g_assert(client);
	client->read_func = func;
	client->read_func_user_data = user_data;
	}


GInetSocketAddress *
gkrellmd_client_get_inet_socket_address(GkrellmdClient *client)
	{
	g_assert(client);
	GError *err = NULL;

	GSocketAddress *sock_addr = g_socket_connection_get_remote_address(
			client->connection, &err);
	if (err)
		{
		g_warning("Retrieving socket address for client %s failed: %s",
				client->hostname ? client->hostname : "unknown", err->message);
		g_error_free(err);
		return NULL;
		}

	GSocketFamily f = g_socket_address_get_family(sock_addr);
	if (f == G_SOCKET_FAMILY_IPV4 || f == G_SOCKET_FAMILY_IPV6)
		{
		return G_INET_SOCKET_ADDRESS(sock_addr);
		}

	return NULL;
	}

