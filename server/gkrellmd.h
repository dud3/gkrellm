/* GKrellM
|  Copyright (C) 1999-2010 Bill Wilson
|
|  Author:  Bill Wilson    billw@gkrellm.net
|  Latest versions might be found at:  http://gkrellm.net
|
|
|  GKrellM is free software: you can redistribute it and/or modify it
|  under the terms of the GNU General Public License as published by
|  the Free Software Foundation, either version 3 of the License, or
|  (at your option) any later version.
|
|  GKrellM is distributed in the hope that it will be useful, but WITHOUT
|  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
|  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
|  License for more details.
|
|  You should have received a copy of the GNU General Public License
|  along with this program. If not, see http://www.gnu.org/licenses/
|
|
|  Additional permission under GNU GPL version 3 section 7
|
|  If you modify this program, or any covered work, by linking or
|  combining it with the OpenSSL project's OpenSSL library (or a
|  modified version of that library), containing parts covered by
|  the terms of the OpenSSL or SSLeay licenses, you are granted
|  additional permission to convey the resulting work.
|  Corresponding Source for a non-source form of such a combination
|  shall include the source code for the parts of OpenSSL used as well
|  as that of the covered work.
*/
#ifndef GKRELLMD_H
#define GKRELLMD_H

#include "log.h"

#include <glib.h>
#include <gio/gio.h>
#include <glib/gstdio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <time.h>

#if (defined(__sun) && defined(__SVR4)) || defined(SOLARIS_8)
#define __solaris__
#endif

#if !defined(WIN32)
	#include <unistd.h>
	#include <utime.h>
	#include <sys/socket.h>
	#include <sys/time.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <netdb.h>
	#include <sys/ioctl.h>
	#include <pwd.h>
	#include <grp.h>
	#if defined(__solaris__)
		#include <sys/filio.h>
	#endif /* defined(__solaris__) */
	#include <sys/select.h>
	#include <sys/wait.h>
#else
	#include <winsock2.h>
	#include <ws2tcpip.h>
	typedef int sa_family_t; // WIN32 uses int for ai_family;
	#include <stdint.h> // defines uint32_t
#endif /* !defined(WIN32) */

#include <sys/stat.h>
#include <sys/types.h>
#include <locale.h>
#include <signal.h>
#include <errno.h>


#if !defined(PACKAGE_D)
#define	PACKAGE_D	"gkrellmd"
#endif

/* Internationalization support.
*/
#if defined (ENABLE_NLS)
#include <libintl.h>
#	undef _
#	define _(String) dgettext(PACKAGE_D,String)
#   if defined(gettext_noop)
#       define N_(String) gettext_noop(String)
#   else
#       define N_(String) (String)
#   endif   /* gettext_noop */
#else
#   define _(String) (String)
#   define N_(String) (String)
#   define textdomain(String) (String)
#   define gettext(String) (String)
#   define dgettext(Domain,String) (String)
#   define dcgettext(Domain,String,Type) (String)
#   define bindtextdomain(Domain,Directory) (Domain)
#endif  /* ENABLE_NLS */

/* -------------------------------------------------------------------
*/
#define GKRELLMD_VERSION_MAJOR   2
#define GKRELLMD_VERSION_MINOR   3
#define GKRELLMD_VERSION_REV     5
#define GKRELLMD_EXTRAVERSION    ""

#define GKRELLMD_CHECK_VERSION(major,minor,rev)    \
(GKRELLMD_VERSION_MAJOR > (major) || \
(GKRELLMD_VERSION_MAJOR == (major) && GKRELLMD_VERSION_MINOR > (minor)) || \
(GKRELLMD_VERSION_MAJOR == (major) && GKRELLMD_VERSION_MINOR == (minor) && \
GKRELLMD_VERSION_REV >= (rev)))

#define GKRELLMD_CONFIG				"gkrellmd.conf"
#if defined(WIN32)
	// no dot in front of config-filename on win32
	#define GKRELLMD_USER_CONFIG  GKRELLMD_CONFIG
#else
	#define GKRELLMD_USER_CONFIG	".gkrellmd.conf"
#endif

#define GKRELLMD_PLUGINS_DIR		".gkrellm2/plugins-gkrellmd"
#if !defined(WIN32)
	#define GKRELLMD_LOCAL_PLUGINS_DIR	"/usr/local/lib/gkrellm2/plugins-gkrellmd"
	#if !defined(GKRELLMD_SYSTEM_PLUGINS_DIR)
		#define GKRELLMD_SYSTEM_PLUGINS_DIR	"/usr/lib/gkrellm2/plugins-gkrellmd"
	#endif
	#define GKRELLMD_SYS_ETC	"/etc"
	#define GKRELLMD_LOCAL_ETC	"/usr/local/etc"
#endif // !defined(WIN32)

typedef struct _GkrellmdClient GkrellmdClient;

typedef void (*GkrellmdClientReadFunc)(GkrellmdClient *client, GString *str,
		gpointer user_data);

typedef struct _GkrellmdClient
	{
	gint		major_version,
				minor_version,
				rev_version;
	gchar		*hostname;

	gint		fd;
	gboolean	served,
				alive,
				last_client;
	gboolean	feature_subdisk;
	GString		*input_gstring;
	void		(*input_func)(struct _GkrellmdClient *, gchar *);

	GSocketConnection *connection;
	GSource *read_source;
	GkrellmdClientReadFunc read_func;
	gpointer read_func_user_data;
	}
	GkrellmdClient;


typedef struct
	{
	gint	timer_ticks,
			second_tick,
			two_second_tick,
			five_second_tick,
			ten_second_tick,
			minute_tick;
	}
	GkrellmdTicks;

extern GkrellmdTicks			GK;


typedef struct
	{
	gboolean		need_serve;
	const gchar		*serve_name;
	gboolean		serve_name_sent;
	GString			*serve_gstring;
	GkrellmdClient	*client;

	GList			*config_list;

	gboolean		is_plugin;
	void			*handle;
	gchar			*path;
	void			(*client_input_func)(GkrellmdClient *, gchar *);
	}
	GkrellmdMonitorPrivate;


typedef struct _GkrellmdMonitor
	{
	gchar		*name;
	void		(*update_monitor)(struct _GkrellmdMonitor *mon,
							gboolean first_update);
	void		(*serve_data)(struct _GkrellmdMonitor *mon,
							gboolean first_serve);
	void		(*serve_setup)(struct _GkrellmdMonitor *mon);

	GkrellmdMonitorPrivate
				*privat;
	}
	GkrellmdMonitor;



  /* gkrellmd serve data functions used by builtins and plugins.
  */
void		gkrellmd_plugin_serve_setup(GkrellmdMonitor *mon,
						gchar *name, gchar *line);
void		gkrellmd_need_serve(GkrellmdMonitor *mon);
void		gkrellmd_set_serve_name(GkrellmdMonitor *mon, const gchar *name);
void		gkrellmd_serve_data(GkrellmdMonitor *mon, gchar *line);
void		gkrellmd_add_serveflag_done(gboolean *);
gboolean	gkrellmd_check_client_version(GkrellmdMonitor *mon,
						gint major, gint minor, gint rev);

const gchar	*gkrellmd_config_getline(GkrellmdMonitor *mon);

void		gkrellmd_client_input_connect(GkrellmdMonitor *mon,
						void (*func)(GkrellmdClient *, gchar *));


  /* Small set of useful functions duplicated from src/utils.c.
  |  These really should just be in the gkrellm_ namespace for sysdep code
  |  common to gkrellm and gkrellmd, but for convenience, offer them in
  |  both gkrellm_ and gkrellmd_ namespaces.
  */
void		gkrellmd_free_glist_and_data(GList **list_head);
gboolean	gkrellmd_getline_from_gstring(GString **, gchar *, gint);
gchar		*gkrellmd_dup_token(gchar **string, gchar *delimeters);
gboolean	gkrellmd_dup_string(gchar **dst, gchar *src);

void		gkrellm_free_glist_and_data(GList **list_head);
gboolean	gkrellm_getline_from_gstring(GString **, gchar *, gint);
gchar		*gkrellm_gstring_get_line(GString *gstr);
gchar		*gkrellm_dup_token(gchar **string, gchar *delimeters);
gboolean	gkrellm_dup_string(gchar **dst, gchar *src);


  /* Plugins should use above data serve functions instead of this.
  */
gint		gkrellmd_send_to_client(GkrellmdClient *client, gchar *buf);


  /* Misc
  */
void		gkrellmd_add_mailbox(gchar *);
GkrellmdTicks *gkrellmd_ticks(void);
gint		gkrellmd_get_timer_ticks(void);


#if !GLIB_CHECK_VERSION(2,0,0)

/* glib2 compatibility functions
*/
#define G_FILE_TEST_EXISTS		1
#define G_FILE_TEST_IS_DIR		2
#define	G_FILE_TEST_IS_REGULAR	4

#include <dirent.h>

typedef struct
    {
    DIR     *dir;
	}
    GDir;

GDir		*g_dir_open(gchar *path, guint flags, gpointer error);
gchar		*g_dir_read_name(GDir *dir);
void		g_dir_close(GDir *dir);
gboolean	g_file_test(gchar *filename, gint test);
gchar		*g_build_filename(gchar *first, ...);
gchar		*g_path_get_basename(gchar *file_name);

#endif

#endif // GKRELLMD_H
