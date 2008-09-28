/* GKrellM
|  Copyright (C) 1999-2007 Bill Wilson
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
*/

#include "gkrellm.h"
#include "gkrellm-private.h"
#include "gkrellm-sysdeps.h"



#if defined(__linux__)
#include "sysdeps/linux.c"
#include "sysdeps/sensors-common.c"
#endif

#if defined(__APPLE__)
#include "sysdeps/darwin.c"
#include "sysdeps/bsd-common.c"
#endif

#if defined(__FreeBSD__)
#include "sysdeps/freebsd.c"
#include "sysdeps/bsd-common.c"
#include "sysdeps/sensors-common.c"
#endif

#if defined(__DragonFly__)
#include "sysdeps/dragonfly.c"
#include "sysdeps/bsd-common.c"
#include "sysdeps/sensors-common.c"
#endif

#if defined(__NetBSD__)
#include "sysdeps/netbsd.c"
#include "sysdeps/bsd-net-open.c"
#include "sysdeps/bsd-common.c"
#include "sysdeps/sensors-common.c"
#endif

#if defined(__OpenBSD__)
#include "sysdeps/openbsd.c"
#include "sysdeps/bsd-net-open.c"
#include "sysdeps/bsd-common.c"
#endif

#if defined(__solaris__)
#include "sysdeps/solaris.c"
#endif

#if defined(USE_LIBGTOP)
#include "sysdeps/gtop.c"
#endif

#if defined(WIN32)
#include "sysdeps/win32.c"
#endif

#if !defined(WIN32)
#include <sys/utsname.h>
#endif

#if !defined(SENSORS_COMMON) && !defined(WIN32)
static gboolean (*mbmon_check_func)();
#endif

gchar *
gkrellm_sys_get_host_name(void)
	{
	static gboolean	have_it;
	static gchar	buf[128];

	if (!have_it && gethostname(buf, sizeof(buf)) != 0)
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

gboolean
gkrellm_sys_sensors_mbmon_port_change(gint port)
	{
	gboolean	result = FALSE;
#if !defined(WIN32)
	_GK.mbmon_port = port;

	/* mbmon_check_func will be set if sysdep code has included
	|  sensors_common.c and has run gkrellm_sys_sensors_mbmon_check()
	*/
	if (mbmon_check_func)
		{
		gkrellm_sensors_interface_remove(MBMON_INTERFACE);
		result = (*mbmon_check_func)(TRUE);
		gkrellm_sensors_model_update();
		gkrellm_sensors_rebuild(TRUE, TRUE, TRUE);
		}
#endif
	return result;
	}

gboolean
gkrellm_sys_sensors_mbmon_supported(void)
	{
#if !defined(WIN32)
	return mbmon_check_func ? TRUE : FALSE;
#else
	return FALSE;
#endif
	}
