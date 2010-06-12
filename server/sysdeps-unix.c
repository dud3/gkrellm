/* GKrellM
|  Copyright (C) 1999-2009 Bill Wilson
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

#include "gkrellmd.h"
#include "gkrellmd-private.h"

#include "../src/gkrellm-sysdeps.h"

#if defined(__linux__)
#include "../src/sysdeps/linux.c"
#include "../src/sysdeps/sensors-common.c"
#endif

#if defined(__APPLE__)
#include "../src/sysdeps/darwin.c"
#include "../src/sysdeps/bsd-common.c"
#endif

#if defined(__FreeBSD__)
#include "../src/sysdeps/freebsd.c"
#include "../src/sysdeps/bsd-common.c"
#include "../src/sysdeps/sensors-common.c"
#endif

#if defined(__DragonFly__)
#include "../src/sysdeps/dragonfly.c"
#include "../src/sysdeps/bsd-common.c"
#include "../src/sysdeps/sensors-common.c"
#endif

#if defined(__NetBSD__)
#include "../src/sysdeps/netbsd.c"
#include "../src/sysdeps/bsd-net-open.c"
#include "../src/sysdeps/bsd-common.c"
#include "../src/sysdeps/sensors-common.c"
#endif

#if defined(__OpenBSD__)
#include "../src/sysdeps/openbsd.c"
#include "../src/sysdeps/bsd-net-open.c"
#include "../src/sysdeps/bsd-common.c"
#endif


#if defined(__solaris__)
#include "../src/sysdeps/solaris.c"
#endif

#if defined(USE_LIBGTOP)
#include "../src/sysdeps/gtop.c"
#endif

#if defined(WIN32)
#include "../src/sysdeps/win32.c"
#endif

#if !defined(WIN32)
#include <sys/utsname.h>
#endif

gchar *
gkrellm_sys_get_host_name(void)
	{
	static gboolean	have_it;
	static gchar	buf[128];

	if (!have_it && gethostname(buf, sizeof(buf)))
		strcpy(buf, "unknown");
	have_it = TRUE;
	return buf;
	}

#if !defined(WIN32)
gchar *
gkrellm_sys_get_system_name(void)
	{
	static gchar	*sname;
	struct utsname	utsn;

	if (!sname && uname(&utsn) > -1)
		sname = g_strdup_printf("%s %s", utsn.sysname, utsn.release);
	if (!sname)
		sname = g_strdup("unknown name");
	return sname;
	}
#endif

  /* Remove embedded "-i2c-" or "-isa-" from lm_sensors chip names so
  |  there can be a chance for config name sysfs compatibility.  This function
  |  here in sensors.c is a kludge.  Give user configs a chance to get
  |  converted and then move this function to sysdeps/linux.c where it
  |  belongs.
  |  Munge names like w83627hf-isa-0290 to w83627hf-0290
  |                or w83627hf-i2c-0-0290 to w83627hf-0-0290
  */
void
gkrellm_sensors_linux_name_fix(gchar *id_name)
	{
#if defined(__linux__)
	gchar	*s;
	gint	len, bus = 0;
	guint	addr = 0;

	len = strlen(id_name) + 1;
	if ((s = strstr(id_name, "-i2c-")) != NULL)
		{
		sscanf(s + 5, "%d-%x", &bus, &addr);
		snprintf(s, len - (s - id_name), "-%d-%04x", bus, addr);
		}
	else if ((s = strstr(id_name, "-isa-")) != NULL)
		{
		*(s + 1) = '0';
		memmove(s + 2, s + 4, strlen(s + 4) + 1);
		}
#endif
	}

#ifdef SENSORS_COMMON
gint
gkrellm_connect_to(gchar *server, gint server_port)
	{
	gint		fd	= -1;
#ifdef HAVE_GETADDRINFO
	gint 		rv	= 0;
	struct addrinfo	hints, *res, *res0;
	gchar		portnumber[6];
#else
	struct hostent	*addr;
	struct sockaddr_in s;
#endif // HAVE_GETADDRINFO

#ifdef HAVE_GETADDRINFO
	snprintf (portnumber, sizeof(portnumber), "%d", server_port);
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	if ((rv = getaddrinfo(server, portnumber, &hints, &res0)) != 0)
		return -1;

	for (res = res0; res; res = res->ai_next)
		{
		if ((fd = socket(res->ai_family, res->ai_socktype,
				res->ai_protocol)) < 0)
			continue;
		gkrellm_debug(DEBUG_SENSORS,
			"\t[gkrellm_connect_to: (%d,%d,%d) %s:%d]\n", res->ai_family,
			res->ai_socktype, res->ai_protocol, server, server_port);
		if (connect(fd, res->ai_addr, res->ai_addrlen) >= 0)
			break;
#ifdef WIN32
		closesocket(fd);
#else
		close(fd);
#endif // WIN32
		fd = -1;
		}
	freeaddrinfo(res0);
#else
	gkrellm_debug(DEBUG_SENSORS, "\t[gkrellm_connect_to: %s:%d]\n", server,
		server_port);
	addr = gethostbyname(server);
	if (addr)
		{
		fd = socket(AF_INET, SOCK_STREAM, 0);
		if (fd >= 0)
			{
			memset(&s, 0, sizeof(s));
			memcpy(&s.sin_addr.s_addr, he->h_addr, he->h_length);
			s.sin_family = AF_INET;
			s.sin_port = htons(server_port);
			if (connect(fd, (struct sockaddr *)&s, sizeof (s)) < 0)
				{
#ifdef WIN32
				closesocket(fd);
#else
				close(fd);
#endif // WIN32
				fd = -1;
				}
			}
		}
#endif // HAVE_GETADDRINFO
	if (fd < 0)
		return -1;

	return fd;
	}
#endif // SENSORS_COMMON
