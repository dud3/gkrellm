
#ifndef GK_CLIENT_H
#define GK_CLIENT_H

#include <glib.h>
#include <gio/gio.h>

// TODO: Move GkrellmdClient declaration here and make opaque data type
#include "gkrellmd.h"

GkrellmdClient *gkrellmd_client_new(GSocketConnection *connection);
GkrellmdClient *gkrellmd_client_ref(GkrellmdClient *client);
void gkrellmd_client_unref(GkrellmdClient *client);
void gkrellmd_client_close(GkrellmdClient *client);
gboolean gkrellmd_client_send(GkrellmdClient *client, const gchar *str);
gboolean gkrellmd_client_send_printf(GkrellmdClient *client,
		const gchar *format, ...) G_GNUC_PRINTF(2, 3);
void gkrellmd_client_set_read_callback(GkrellmdClient *client,
		GkrellmdClientReadFunc func, gpointer user_data);
void gkrellmd_client_set_close_callback(GkrellmdClient *client,
		GkrellmdClientFunc func, gpointer user_data);
GInetSocketAddress *gkrellmd_client_get_inet_socket_address(GkrellmdClient *client);
const gchar *gkrellmd_client_get_hostname(GkrellmdClient *client);
gboolean gkrellmd_client_check_version(GkrellmdClient *client, gint major, gint minor, gint rev);

#endif // GK_CLIENT_H
