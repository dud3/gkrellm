
#ifndef GK_CLIENT_H
#define GK_CLIENT_H

#include <glib.h>
#include <gio/gio.h>

// TODO: Move GkrellmdClient declaration here and make opaque data type
#include "gkrellmd.h"

GkrellmdClient *gkrellmd_client_new(GSocketConnection *connection);
void gkrellmd_client_free(GkrellmdClient *client);
gssize gkrellmd_client_send(GkrellmdClient *client, const gchar *str);
void gkrellmd_client_set_read_callback(GkrellmdClient *client,
		GkrellmdClientReadFunc func, gpointer user_data);
GInetSocketAddress *gkrellmd_client_get_inet_socket_address(GkrellmdClient *client);

#endif // GK_CLIENT_H
