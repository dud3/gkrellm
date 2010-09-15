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
 
#if !defined(WIN32)
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
// Enable IPV6 on win32 if we target win xp or newer
#if defined(IPPROTO_IPV6) && (_WIN32_WINNT > 0x0500)
#define INET6
#endif
#endif

#if defined(__linux__)
#if defined(IPPROTO_IPV6)
#define INET6
#endif
#endif /* __linux__ */

#if defined(__FreeBSD__) || defined(__DragonFly__)
#include <osreldate.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <net/route.h>
#if defined(__KAME__) && !defined(INET6)
#define INET6
#endif
#endif  /* __FreeBSD__ */

#if defined(__APPLE__)
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <net/route.h>
#define INET6
#endif /* __APPLE__ */

#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#endif	/* __NetBSD__ || __OpenBSD__ */

#if defined(__solaris__)
/* IPv6 ? */
#include <netconfig.h>
#if defined(NC_INET6)
#define INET6
#endif
#endif /* __solaris__ */


  /* Values for state.
  */
#define	TCP_DEAD	0
#define	TCP_ALIVE	1

typedef struct
	{
	gint			state;
	gint			family;
	gint			local_port;
	struct in_addr	remote_addr;
#if defined(INET6)
	struct in6_addr	remote_addr6;
#endif
	gint			remote_port;
	gint			new_hit;
	gboolean		is_udp;
	}
	ActiveTCP;

