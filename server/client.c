
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
					gkrellmd_client_get_hostname(client), err->message);
			g_error_free(err);
			gkrellmd_client_close(client);
			return FALSE; // stops read callback for client
			}
		}

	if (rb < 1)
		{
		// No data to read but no socket error => client dropped connection
		gkrellm_debug(DEBUG_SERVER, "No data to read, dropping client %s\n",
				gkrellmd_client_get_hostname(client));
		client->alive = FALSE;
		gkrellmd_client_close(client);
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
	client->write_buf = g_string_sized_new(0);
	client->write_source = NULL;

	GPollableInputStream *istream = G_POLLABLE_INPUT_STREAM(
			g_io_stream_get_input_stream((GIOStream*)client->connection));
	g_assert(istream);
	g_assert(g_pollable_input_stream_can_poll(istream));

	client->read_source = g_pollable_input_stream_create_source(istream, NULL);
	g_source_set_callback(client->read_source, (GSourceFunc)gk_client_read,
			(gpointer)client, NULL);
	g_source_attach(client->read_source, NULL); // TODO: default context ok?

	client->ref_count = 1;
	return client;
	}


static void
gk_client_free(GkrellmdClient *client)
	{
	if (!client)
		return;

	gkrellm_debug(DEBUG_SERVER, "gk_client_free: client %p\n", (void*)client);

	g_object_unref(client->connection);
	g_string_free(client->input_gstring, TRUE);
	g_free(client->hostname);
	if (!g_source_is_destroyed(client->read_source))
		g_source_destroy(client->read_source);
	g_source_unref(client->read_source);
	g_string_free(client->write_buf, TRUE);
	if (client->write_source)
		g_source_destroy(client->write_source);
	g_free(client->hostname_tmp);
	g_free(client);
	}


GkrellmdClient *
gkrellmd_client_ref(GkrellmdClient *client)
	{
	g_assert(client);
	++(client->ref_count);
	gkrellm_debug(DEBUG_SERVER, "gkrellmd_client_ref: client %p; ref_count %lu\n",
			(void*)client, client->ref_count);
	return client;
	}


void gkrellmd_client_unref(GkrellmdClient *client)
	{
	g_assert(client);
	g_assert(client->ref_count > 0);
	--(client->ref_count);
	gkrellm_debug(DEBUG_SERVER, "gkrellmd_client_unref: client %p; ref_count %lu\n",
			(void*)client, client->ref_count);
	if (client->ref_count == 0)
		gk_client_free(client);
	}


static void
gk_client_close_cb(GObject *source, GAsyncResult *res, gpointer user_data)
	{
	GkrellmdClient *client;
	GError *err;

	client = (GkrellmdClient*)user_data;

	gkrellm_debug(DEBUG_SERVER, "Finished closing connection to client %s\n",
			gkrellmd_client_get_hostname(client));

	err = NULL;
	g_io_stream_close_finish(G_IO_STREAM(source), res, &err);
	if (err)
		{
		g_warning(_("Closing client connection for %s failed: %s\n"),
				gkrellmd_client_get_hostname(client), err->message);
		g_error_free(err);
		}
	else if (client->close_func)
		{
		client->close_func(client, client->close_func_user_data);
		}
	gkrellmd_client_unref(client);
	}


void
gkrellmd_client_close(GkrellmdClient *client)
	{
	g_assert(client);
	gkrellm_debug(DEBUG_SERVER, "Closing connection to client %s\n",
			gkrellmd_client_get_hostname(client));
	gkrellmd_client_ref(client);
	g_io_stream_close_async(G_IO_STREAM(client->connection), 0,
			NULL, gk_client_close_cb,
			(gpointer)client);
	}


// forward decl
static gboolean gk_client_send_write_buf(GkrellmdClient *client);


static gboolean
gk_client_write(GObject *pollable_stream, gpointer user_data)
	{
	GkrellmdClient *client;

	client = (GkrellmdClient*)user_data;
	g_assert(client);
	g_assert(client->write_source);

	gkrellm_debug(DEBUG_SERVER, "gk_client_write: Retrying write\n");

	client->write_source = NULL;
	gk_client_send_write_buf(client); // Try send again

	gkrellmd_client_unref(client);
	return FALSE; // Removes (and frees?) GSource from context
	}


static gboolean
gk_client_send_write_buf(GkrellmdClient *client)
	{
	GPollableOutputStream *ostream;
	GError *err;
	gssize wb;

	g_assert(client);
	g_assert(client->write_source == NULL);

	gkrellm_debug(DEBUG_SERVER,
			"gk_client_send_write_buf: writing buffer with %ld bytes\n",
			client->write_buf->len);

	// TODO: cache ostream in client struct
	ostream = G_POLLABLE_OUTPUT_STREAM(g_io_stream_get_output_stream(
				(GIOStream*)client->connection));
	g_assert(ostream);

	err = NULL;
	wb = g_pollable_output_stream_write_nonblocking(ostream,
			(const void *)client->write_buf->str, client->write_buf->len,
			NULL, &err);
	if (err)
		{
		g_assert(wb < 1); // should not have written any bytes at all
		if ((G_IO_ERROR == err->domain) && (G_IO_ERROR_WOULD_BLOCK == err->code))
			{
			gkrellm_debug(DEBUG_SERVER,	"gk_client_send_write_buf: write would block, postponing write\n");
			g_error_free(err);
			client->write_source = g_pollable_output_stream_create_source(
					ostream, NULL);
			g_source_set_callback(client->write_source,
					(GSourceFunc)gk_client_write, (gpointer)client, NULL);
			gkrellmd_client_ref(client); // add ref for async callback
			g_source_attach(client->write_source, NULL); // TODO: default context ok?
			}
		else
			{
			g_warning(_("Write to client host %s failed: %s\n"),
					gkrellmd_client_get_hostname(client), err->message);
			g_error_free(err);
			client->alive = FALSE;
			gkrellmd_client_close(client);
			return FALSE;
			}
		}
	g_assert(wb <= client->write_buf->len); // can't write more than what's in the buffer
	g_string_truncate(client->write_buf, client->write_buf->len - wb);
	gkrellm_debug(DEBUG_SERVER,	"gk_client_send_write_buf: wrote %ld bytes\n", wb);
	return TRUE;
	}


gboolean
gkrellmd_client_send(GkrellmdClient *client, const gchar *str)
	{
	g_assert(client);
	g_assert(str);

	g_string_append(client->write_buf, str);

	// Only try to send if we are not already waiting for writing
	if (!client->write_source)
		return gk_client_send_write_buf(client);

	return TRUE;
	}

gboolean
gkrellmd_client_send_printf(GkrellmdClient *client, const gchar *format, ...)
	{
	va_list varargs;

	g_assert(client);
	g_assert(format);

	va_start(varargs, format);
	g_string_append_vprintf(client->write_buf, format, varargs);
	va_end(varargs);

	// Only try to send if we are not already waiting for writing
	if (!client->write_source)
		return gk_client_send_write_buf(client);

	return TRUE;
	}

void
gkrellmd_client_set_read_callback(GkrellmdClient *client,
		GkrellmdClientReadFunc func, gpointer user_data)
	{
	g_assert(client);
	client->read_func = func;
	client->read_func_user_data = user_data;
	}


void
gkrellmd_client_set_close_callback(GkrellmdClient *client,
		GkrellmdClientFunc func, gpointer user_data)
	{
	g_assert(client);
	client->close_func = func;
	client->close_func_user_data = user_data;
	}


void
gkrellmd_client_set_resolve_callback(GkrellmdClient *client,
		GkrellmdClientResolveFunc func, gpointer user_data)
	{
	g_assert(client);
	client->resolve_func = func;
	client->resolve_func_user_data = user_data;
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
				gkrellmd_client_get_hostname(client), err->message);
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


static void
gk_client_set_hostname(GkrellmdClient *client, gchar *hostname)
	{
	g_free(client->hostname);
	client->hostname = hostname;
	}


static gint
gk_inet_address_cmp(gconstpointer a, gconstpointer b)
	{
#if GLIB_CHECK_VERSION(2, 30, 0)
	return g_inet_address_equal((GInetAddress*)a, (GInetAddress*)b) ? 0 : -1;
#else
	GInetAddress *address = (GInetAddress*)a;
	GInetAddress *other_address = (GInetAddress*)b;

	g_return_val_if_fail (G_IS_INET_ADDRESS(address), -1);
	g_return_val_if_fail (G_IS_INET_ADDRESS(other_address), -1);

	if (g_inet_address_get_family(address) != g_inet_address_get_family(other_address))
		return -1;

	if (memcmp (g_inet_address_to_bytes(address),
				g_inet_address_to_bytes(other_address),
				g_inet_address_get_native_size(address)) != 0)
		return -1;

	return 0;
#endif
	}


static void
gk_resolve_address_cb(GObject *source_object, GAsyncResult *res,
		gpointer user_data)
	{
	GResolver *resolver;
	GkrellmdClient *client;
	GError *err;
	GList *addr_list;
	gboolean success;

	resolver = G_RESOLVER(source_object);
	client = (GkrellmdClient*)user_data;

	err = NULL;
	addr_list = g_resolver_lookup_by_name_finish(resolver, res, &err);
	if (err)
		{
		g_warning(_("Reverse lookup for client %s failed: %s\n"),
				client->hostname_tmp, err->message);
		g_error_free(err);
		g_free(client->hostname_tmp);
		success = FALSE;
		}
	else
		{
		GInetSocketAddress *isockaddr = gkrellmd_client_get_inet_socket_address(client);
		GInetAddress *iaddr = g_inet_socket_address_get_address(isockaddr);

		// Find client GInetAddress in address list
		if (g_list_find_custom(addr_list, (gconstpointer)iaddr,
					gk_inet_address_cmp))
			{
			gkrellm_debug(DEBUG_SERVER, "Reverse lookup for client %s valid\n",
					client->hostname_tmp);
			gk_client_set_hostname(client, client->hostname_tmp);
			success = TRUE;
			}
		else
			{
			g_warning(_("Reverse lookup for client %s did not match!\n"),
					client->hostname_tmp);
			g_free(client->hostname_tmp);
			success = FALSE;
			}
		g_object_unref(isockaddr);
		g_resolver_free_addresses(addr_list);
		}

	if (client->resolve_func)
		{
		client->resolve_func(client, success, client->resolve_func_user_data);
		}

	client->hostname_tmp = NULL;
	g_object_unref(resolver);
	gkrellmd_client_unref(client);
	}


static void
gk_resolve_name_cb(GObject *source_object, GAsyncResult *res,
		gpointer user_data)
	{
	GResolver *resolver;
	GkrellmdClient *client;
	GError *err;

	resolver = G_RESOLVER(source_object);
	client = (GkrellmdClient*)user_data;

	g_assert(!client->hostname_tmp);

	err = NULL;
	client->hostname_tmp = g_resolver_lookup_by_address_finish(resolver, res,
			&err);
	if (err)
		{
		g_warning(_("Address lookup for client %s failed: %s\n"),
				gkrellmd_client_get_hostname(client), err->message);
		g_error_free(err);
		if (client->resolve_func)
			{
			client->resolve_func(client, FALSE, client->resolve_func_user_data);
			}
		g_object_unref(resolver);
		gkrellmd_client_unref(client);
		}
	else
		{
		g_assert(client->hostname_tmp);
		gkrellm_debug(DEBUG_SERVER, "Resolved client hostname %s, "
				"verifying reverse lookup\n", client->hostname_tmp);
		// We keep resolver and client ref'd until the next async lookup
		// finishes.
		g_resolver_lookup_by_name_async(resolver, client->hostname_tmp, NULL,
				gk_resolve_address_cb, client);
		}
	}


void
gkrellmd_client_resolve(GkrellmdClient *client)
	{
	g_assert(client);

	gkrellm_debug(DEBUG_SERVER, "Resolving hostname for client %s\n",
			gkrellmd_client_get_hostname(client));

	GInetSocketAddress *isockaddr = gkrellmd_client_get_inet_socket_address(client);
	if (!isockaddr)
		{
		g_warning("Failed to retrieve inet socket address for client\n");
		return;
		}

	GInetAddress *iaddr = g_inet_socket_address_get_address(isockaddr);
	g_assert(iaddr); // docs say this never fails

	GResolver *resolver = g_resolver_get_default();
	g_assert(resolver); // GIO should always have a resolver

	gkrellmd_client_ref(client); // Keep client around for callback
	g_resolver_lookup_by_address_async(resolver, iaddr, NULL,
			gk_resolve_name_cb, client);

	g_object_unref(isockaddr);
	}


const gchar *gkrellmd_client_get_hostname(GkrellmdClient *client)
	{
	static const gchar *unres = "<unknown>";
	g_assert(client);
	return client->hostname ? client->hostname : unres;
	}


gboolean
gkrellmd_client_check_version(GkrellmdClient *client,
		gint major, gint minor, gint rev)
	{
	g_return_val_if_fail(client, FALSE);
	return (client->major_version > major
			|| (client->major_version == major && client->minor_version > minor)
			|| (client->major_version == major && client->minor_version == minor
				&& client->rev_version >= rev));
	}
