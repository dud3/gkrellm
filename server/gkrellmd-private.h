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

#include "configure.h"

#include "../src/gkrellm-sysdeps.h"

#if defined(WIN32)
// Enable getaddrinfo on win32 if we target win xp or newer
#if _WIN32_WINNT > 0x0500
#define HAVE_GETADDRINFO	1
#endif
#endif

#if defined(__linux__)
#if defined(__GLIBC__) && ((__GLIBC__>2)||(__GLIBC__==2 && __GLIBC_MINOR__>=1))
#define HAVE_GETADDRINFO	1
#endif
#endif

#if defined(__DragonFly__)
#define HAVE_GETADDRINFO	1
#endif

#if defined(__FreeBSD__)
#include <sys/param.h>
#if __FreeBSD_version >= 400000
#define HAVE_GETADDRINFO	1
#endif
#endif

#if defined(__OpenBSD__)
#define HAVE_GETADDRINFO	1
#endif

#if defined(__NetBSD__)
#define HAVE_GETADDRINFO	1
#include <sys/param.h>
#  if __NetBSD_Version__ <= 105010000
#    define sa_family_t unsigned char
#  endif
#endif

#if defined(__solaris__)
# include <netconfig.h>
# if defined(NC_INET6)
#  define HAVE_GETADDRINFO	1
# endif
#endif

#if defined(__APPLE__)
# ifndef socklen_t
#  define socklen_t int
# endif
#define HAVE_GETADDRINFO   1
#endif

#ifndef	NI_WITHSCOPEID
#define	NI_WITHSCOPEID	0
#endif

#if !defined(__FreeBSD__) && !defined(__linux__) && !defined(__NetBSD__) \
    && !defined(__OpenBSD__) && !defined(__solaris__) && !defined(WIN32) \
    && !defined(__APPLE__) && !defined(__DragonFly__)
#define  USE_LIBGTOP
#endif

#define DEBUG_SYSDEP		0x1
#define DEBUG_SERVER		0x2
#define DEBUG_MAIL			0x10
#define DEBUG_NET			0x20
#define DEBUG_TIMER			0x40
#define DEBUG_SENSORS		0x80
#define DEBUG_NO_LIBSENSORS	0x100
#define DEBUG_INET          0x800
#define	DEBUG_BATTERY		0x8000


#define SENSOR_TEMPERATURE  0
#define SENSOR_FAN          1
#define SENSOR_VOLTAGE      2

#define	SENSOR_GROUP_MAINBOARD	0
#define	SENSOR_GROUP_DISK		1

#include <errno.h>

struct GkrellmdConfig
	{
	gint		update_HZ;
	gint		debug_level;
	gint		*server_fd;
	gint		max_clients;
	gint		server_port;
	gchar		*server_address;
	gint		verbose;
	time_t		start_time;
	time_t		time_now;
	gint		io_timeout;
	gint		reconnect_timeout;
	gint		mbmon_port;

	gint		fs_interval,
				nfs_interval,
				inet_interval;

	gboolean	without_libsensors;
	gboolean	use_acpi_battery;

	gboolean	list_plugins,
				log_plugins;
	gchar		*command_line_plugin;

	gchar		*pidfile;
	gchar		*homedir;

	gchar		*net_timer;
	};

typedef struct
	{
	gchar	*name,
			*line;
	}
	PluginConfigRec;

extern struct GkrellmdConfig	_GK;

extern gchar	*plugin_install_log;

typedef	void (*GkrellmdFunc)();

extern GList	*gkrellmd_client_list,
				*gkrellmd_plugin_enable_list,
				*gkrellmd_plugin_config_list;
extern GList	*gkrellmd_allow_host_list;


void			gkrellmd_monitor_read_client(GkrellmdClient *client, GString *str, gpointer user_data);
void			gkrellmd_client_read(gint client_fd, gint nbytes);
void			gkrellmd_load_monitors(void);
GList			*gkrellmd_plugins_load(void);
gboolean		gkrellmd_update_monitors(gpointer data);
void			gkrellmd_serve_setup(GkrellmdClient *client);

GkrellmdMonitor *gkrellmd_init_mail_monitor(void);

gint		gkrellm_connect_to(gchar *, gint);
