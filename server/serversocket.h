
#ifndef GK_SERVERSOCKET_H
#define GK_SERVERSOCKET_H

#include <glib.h>
#include <gio/gio.h>

typedef struct GkServerSocket_
	{
	GList *client_list;
	GSocketService *service;
	}
	GkServerSocket;

GkServerSocket *gkrellmd_serversocket_new();
gboolean gkrellmd_serversocket_setup(GkServerSocket *socket);
void gkrellmd_serversocket_free(GkServerSocket *socket);

#endif // GK_SOCKET_H
