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

#include "gkrellmd.h"
#include "gkrellmd-private.h"

// we do have addrinfo on win32 but do not have getaddrinfo(), doh
#if !defined(HAVE_GETADDRINFO) && !defined(WIN32)
struct addrinfo
	{
	int		ai_flags;		/* AI_PASSIVE, AI_CANONNAME, AI_NUMERICHOST */
	int		ai_family;		/* PF_xxx */
	int		ai_socktype;	/* SOCK_xxx */
	int		ai_protocol;	/* 0 or IPPROTO_xxx for IPv4 and IPv6 */
	size_t	ai_addrlen;		/* length of ai_addr */
	char	*ai_canonname;	/* canonical name for hostname */
	struct	sockaddr *ai_addr;	/* binary address */
	struct	addrinfo *ai_next;	/* next structure in linked list */
	};
#endif // HAVE_GETADDRINFO && !WIN32

#if !defined(IPV6_V6ONLY) && defined(IPV6_BINDV6ONLY)
#define	IPV6_V6ONLY	IPV6_BINDV6ONLY
#endif

struct GkrellmdConfig	_GK;
GkrellmdTicks			GK;

GList			*gkrellmd_client_list,
				*gkrellmd_plugin_config_list;

static GList	*allow_host_list;

#if !defined(WIN32)
static gboolean	detach_flag;

struct
	{
	uid_t	uid;
	uid_t	gid;
	}
	drop_privs = { 0, 0 };
#endif /* !defined(WIN32) */


#if defined(WIN32)
/* 
	Flag that determines if gkrellmd was started as a console app (FALSE)
	or as a service (TRUE)
*/ 
static gboolean service_is_one = FALSE;

/*
	Flag that is TRUE while gkrellmd should stay in its main loop
*/
static gboolean service_running = FALSE;

/* Name for the installed windows service */
static TCHAR* service_name = TEXT("gkrellmd");

/* User visible name for the installed windows service */
static TCHAR* service_display_name = TEXT("GKrellM Daemon");

/*
	Current service status if running as a service, may be stopped or running
	(pausing is not supported)
*/
static SERVICE_STATUS service_status;

/*
	Handle that allows changing the service status.
	Main use is to stop the running service.
*/
static SERVICE_STATUS_HANDLE service_status_handle = 0;
#endif /* defined(WIN32) */



static void
make_pidfile(void)
	{
#if !defined(WIN32)
	FILE	*f;

	if (!_GK.pidfile)
		return;
	f = fopen(_GK.pidfile, "w");
	if (f)
		{
		fprintf(f, "%d\n", getpid());
		fclose(f);
		}
	else
		fprintf(stderr, "gkrellmd: Can't create pidfile %s\n", _GK.pidfile);
#endif
	}

static void
remove_pidfile(void)
	{
#if !defined(WIN32)
	if (_GK.pidfile)
		unlink(_GK.pidfile);
#endif
	}

static void
gkrellmd_cleanup()
	{
	gkrellm_sys_main_cleanup();
	remove_pidfile();
	}

static void
cb_sigterm(gint sig)
	{
	if (_GK.verbose)
		printf("gkrellmd: Got SIGINT/SIGTERM signal, exiting\n");
	gkrellmd_cleanup();
	exit(0);
	}

gint
gkrellmd_send_to_client(GkrellmdClient *client, gchar *buf)
	{
	gint	n;

	if (!client->alive)
		return 0;
#if defined(MSG_NOSIGNAL)
	n = send(client->fd, buf, strlen(buf), MSG_NOSIGNAL);
#else
	n = send(client->fd, buf, strlen(buf), 0);
#endif
	if (n < 0 && errno == EPIPE)
		{
		if (_GK.verbose)
			printf("Write on closed pipe to host %s\n", client->hostname);
		client->alive = FALSE;
		}
	return n;
	}

#if 0
static gint
getline(gint fd, gchar *buf, gint len)
	{
	fd_set			read_fds;
	struct timeval	tv;
	gchar			*s;
	gint			result, n;

	FD_ZERO(&read_fds);
	FD_SET(fd, &read_fds);
	tv.tv_usec = 0;
	tv.tv_sec = 15;
	s = buf;
	*s = '\0';
	for (n = 0; n < len - 1; ++n)
		{
		result = select(fd + 1, &read_fds, NULL, NULL, &tv);
		if (result <= 0 || read(fd, s, 1) != 1)
			break;
		if (*s == '\n')
			{
			*s = '\0';
			break;
			}
		*++s = '\0';
		}
	return n;
	}
#endif

#ifdef HAVE_GETADDRINFO
static gboolean
is_valid_reverse(char *addr, char *host, sa_family_t family)
	{
	struct addrinfo	hints, *res, *r;
	int		error, good;
	char		addrbuf[NI_MAXHOST];

	/* Reject numeric addresses */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = family;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST;
	if (getaddrinfo(host, NULL, &hints, &res) == 0)
		{
		freeaddrinfo(res);
		return 0;
		}

	/* Check for spoof */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = family;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if (getaddrinfo(host, NULL, &hints, &res) != 0)
		return 0;
	good = 0;
	for (r = res; good == 0 && r; r = r->ai_next)
		{
		error = getnameinfo(r->ai_addr, r->ai_addrlen,
				    addrbuf, sizeof(addrbuf), NULL, 0,
				    NI_NUMERICHOST | NI_WITHSCOPEID);
		if (error == 0 && strcmp(addr, addrbuf) == 0)
			{
			good = 1;
			break;
			}
		}
	freeaddrinfo(res);
	return good;
	}
#endif

/* Check for CIDR match.
 */
static gboolean
cidr_match(struct sockaddr *sa, socklen_t salen, char *allowed)
{
#ifdef HAVE_GETADDRINFO
	struct addrinfo hints, *res;
	struct sockaddr_storage	ss;
	char		*buf;
	char		*p, *ep;
	guchar		*addr, *pat;
	uint32_t	mask;
	int		plen;
#if defined(INET6)
	int		i;
#endif
	gboolean	result;

	buf = g_strdup(allowed);
	plen = -1;
	if ((p = strchr(buf, '/')) != NULL)
		{
		plen = strtoul(p + 1, &ep, 10);
		if (errno != 0 || ep == NULL || *ep != '\0' || plen < 0)
			{
			g_free(buf);
			return FALSE;
			}
		*p = '\0';
		allowed = buf;
		}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST;
	result = getaddrinfo(allowed, NULL, &hints, &res);
	g_free(buf);
	if (result != 0)
		return FALSE;
	memcpy(&ss, res->ai_addr, res->ai_addrlen);
	freeaddrinfo(res);

	if (sa->sa_family != ((struct sockaddr *)&ss)->sa_family)
		return FALSE;
	switch (sa->sa_family)
		{
#if defined(INET6)
		case AF_INET6:
			if (plen < 0)
				plen = 128;
			if (plen > 128)
				return FALSE;
			if (((struct sockaddr_in6 *)&ss)->sin6_scope_id != 0 &&
			    ((struct sockaddr_in6 *)&ss)->sin6_scope_id !=
			    ((struct sockaddr_in6 *)sa)->sin6_scope_id)
				return FALSE;
			addr = (guchar *)&((struct sockaddr_in6 *)sa)->sin6_addr;
			pat = (guchar *)&((struct sockaddr_in6 *)&ss)->sin6_addr;
			i = 0;
			while (plen > 0)
				{
				if (plen < 32)
					{
					mask = htonl(~(0xffffffff >> plen));
					if ((*(uint32_t *)&addr[i] & mask) !=
					    (*(uint32_t *)&pat[i] & mask))
						return FALSE;
					break;
					}
				if (*(uint32_t *)&addr[i] !=
				    *(uint32_t *)&pat[i])
					return FALSE;
				i += 4;
				plen -= 32;
				}
			break;
#endif
		case AF_INET:
			if (plen < 0)
				plen = 32;
			if (plen > 32)
				return FALSE;
			addr = (guchar *)&((struct sockaddr_in *)sa)->sin_addr;
			pat = (guchar *)&((struct sockaddr_in *)&ss)->sin_addr;
			mask = htonl(~(0xffffffff >> plen));
			if ((*(uint32_t *)addr & mask) !=
			    (*(uint32_t *)pat & mask))
				return FALSE;
			break;
		default:
			return FALSE;
		}
	return TRUE;
#else
	return FALSE;
#endif
}

static gboolean
allow_host(GkrellmdClient *client, struct sockaddr *sa, socklen_t salen)
	{
	GList			*list;
#ifdef HAVE_GETADDRINFO
	int error;
	char hostbuf[NI_MAXHOST], addrbuf[NI_MAXHOST];
#else
	struct hostent	*hostent;
#endif
	gchar			buf[128];
	gchar			*hostname = NULL,
					*addr = NULL;
	gchar			*s, *allowed;

#ifdef HAVE_GETADDRINFO
	error = getnameinfo(sa, salen, addrbuf, sizeof(addrbuf),
			    NULL, 0, NI_NUMERICHOST | NI_WITHSCOPEID);
	if (error == 0)
		{
		addr = addrbuf;
		error = getnameinfo(sa, salen, hostbuf, sizeof(hostbuf),
				    NULL, 0, NI_NAMEREQD);
		if (error == 0 &&
		    is_valid_reverse(addrbuf, hostbuf, sa->sa_family))
			hostname = hostbuf;
		}
#else
	hostent = gethostbyaddr((gchar *)&((struct sockaddr_in *)sa)->sin_addr,
				sizeof(struct in_addr), AF_INET);
	if (hostent)
		hostname = hostent->h_name;
	addr = inet_ntoa(((struct sockaddr_in *)sa)->sin_addr);
#endif

	client->hostname = g_strdup(hostname ? hostname : addr);

	if (!allow_host_list)
		return TRUE;

	for (list = allow_host_list; list; list = list->next)
		{
		allowed = (gchar *) list->data;
		if (   (hostname && !strcmp(hostname, allowed))
			|| (addr && !strcmp(addr, allowed))
			|| !strcmp("ALL", allowed)
		   )
			return TRUE;

		if (addr && cidr_match(sa, salen, allowed))
			return TRUE;

		/* Check for simple IPv4 subnet match.  Worry later about ranges and
		|  other hosts_access type patterns.
		*/
		if (   addr
			&& (s = strrchr(allowed, (int) '.')) != NULL
			&& *(s + 1) == '*' && *(s + 2) == '\0'
			&& !strncmp(addr, allowed, (gint) (s - allowed + 1))
		   )
				return TRUE;
		}

	snprintf(buf, sizeof(buf), _("Connection not allowed from %s\n"),
			hostname ? hostname : addr);
	fprintf(stderr, "gkrellmd: %s", buf),
	gkrellmd_send_to_client(client, "<error>\n");
	gkrellmd_send_to_client(client, buf);
	return FALSE;
	}

  /* client sends line: gkrellm x.y.z
  */
static GkrellmdClient *
accept_client(gint fd, struct sockaddr *sa, socklen_t salen)
	{
	GkrellmdClient	*client;
	gchar			buf[64], name[32];
	gboolean		client_limit;
	gint			err;

	client = g_new0(GkrellmdClient, 1);
	client->fd = fd;
	client->alive = TRUE;
	client_limit = (g_list_length(gkrellmd_client_list) >= _GK.max_clients);

	if (!allow_host(client, sa, salen) || client_limit)
		{
		if (client_limit)
			{
			fprintf(stderr, _("gkrellmd: too many clients, rejecting %s\n"),
					client->hostname);
			gkrellmd_send_to_client(client,
						"<error>\nClient limit exceeded.\n");
			}
		g_free(client->hostname);
		g_free(client);
		return NULL;
		}
	err = recv(fd, buf, sizeof(buf), 0);
	if (err > 0)
		buf[err] = '\0';
	else
		buf[0] = '\0';
	//getline(fd, buf, sizeof(buf));

	if (_GK.verbose)
		printf(_("connect string from client: %s\n"), buf);

	if (   sscanf(buf, "%31s %d.%d.%d", name, &client->major_version,
					&client->minor_version, &client->rev_version) == 4
		&& !strcmp(name, "gkrellm")
	   )
		{
		gkrellmd_client_list = g_list_append(gkrellmd_client_list, client);
		return client;
		}
	fprintf(stderr, _("gkrellmd: bad connect line from %s: %s\n"),
				client->hostname, buf);
	gkrellmd_send_to_client(client, "<error>\nBad connect string!");

	g_free(client->hostname);
	g_free(client);
	return NULL;
	}

static void
remove_client(gint fd)
	{
	GList			*list;
	GkrellmdClient	*client;

	for (list = gkrellmd_client_list; list; list = list->next)
		{
		client = (GkrellmdClient *) list->data;
		if (client->fd == fd)
			{
			if (_GK.verbose)
				printf("Removing client %s\n", client->hostname);
			close(fd);
			g_free(client->hostname);
			g_free(client);
			gkrellmd_client_list = g_list_remove(gkrellmd_client_list, client);
			break;
			}
		}
	}

static gint
parse_config(gchar *config, gchar *arg)
	{
	if (!strcmp(config, "clear-hosts") || !strcmp(config, "c"))
		{
		gkrellm_free_glist_and_data(&allow_host_list);
		return 0;
		}
#if !defined(WIN32)
	if (!strcmp(config, "detach") || !strcmp(config, "d"))
		{
		detach_flag = TRUE;
		return 0;
		}
#endif
	if (!arg || !*arg)
		return -1;
	if (!strcmp(config, "update-hz") || !strcmp(config, "u"))
		_GK.update_HZ = atoi(arg);
	else if (!strcmp(config, "port") || !strcmp(config, "P"))
		_GK.server_port = atoi(arg);
	else if (!strcmp(config, "address") || !strcmp(config, "A"))
		_GK.server_address = g_strdup(arg);
	else if (!strcmp(config, "max-clients") || !strcmp(config, "m"))
		_GK.max_clients = atoi(arg);
	else if (!strcmp(config, "allow-host") || !strcmp(config, "a"))
		allow_host_list = g_list_append(allow_host_list, g_strdup(arg));
	else if (!strcmp(config, "plugin-enable") || !strcmp(config, "pe"))
		gkrellmd_plugin_enable_list
				= g_list_append(gkrellmd_plugin_enable_list, g_strdup(arg));
	else if (!strcmp(config, "plugin") || !strcmp(config, "p"))
		_GK.command_line_plugin = g_strdup(arg);
	else if (!strcmp(config, "io-timeout"))
		_GK.io_timeout = atoi(arg);
	else if (!strcmp(config, "reconnect-timeout"))
		_GK.reconnect_timeout = atoi(arg);
	else if (!strcmp(config, "fs-interval"))
		_GK.fs_interval = atoi(arg);
	else if (!strcmp(config, "nfs-interval"))
		_GK.nfs_interval = atoi(arg);
	else if (!strcmp(config, "inet-interval"))
		_GK.inet_interval = atoi(arg);
	else if (!strcmp(config, "mbmon-port"))
		_GK.mbmon_port = atoi(arg);
	else if (!strcmp(config, "net-timer"))
		_GK.net_timer = g_strdup(arg);
	else if (!strcmp(config, "debug-level") || !strcmp(config, "debug"))
		_GK.debug_level = (gint) strtoul(arg, NULL, 0);
#if !defined(WIN32)
	else if (!strcmp(config, "pidfile"))
		_GK.pidfile = g_strdup(arg);
	else if (!strcmp(config, "mailbox"))
		gkrellmd_add_mailbox(arg);
	else if (!strcmp(config, "user") || !strcmp(config, "U"))
		{
		struct passwd *tmp;

		if ((tmp = getpwnam(arg)) != (struct passwd*) 0)
			drop_privs.uid = tmp->pw_uid;
		else
			return -1;
		}
	else if (!strcmp(config, "group") || !strcmp(config, "G"))
		{
		struct group *tmp;

		if ((tmp = getgrnam(arg)) != (struct group*) 0)
			drop_privs.gid = tmp->gr_gid;
		else
			return -1;
		}
#endif
	else
		return -1;
	return 1;
	}

static void
load_config(gchar *path)
	{
	FILE			*f;
	PluginConfigRec	*cfg;
	gchar			buf[128], config[32], arg[128];
	gchar			*s, *plugin_config_block = NULL;

	f = fopen(path, "r");
	if (!f)
		return;
	while (fgets(buf, sizeof(buf), f))
		{
		if (!buf[0] || buf[0] == '#')
			continue;
		if (buf[0] == '[' || buf[0] == '<')
			{
			if (buf[1] == '/')
				{
				g_free(plugin_config_block);
				plugin_config_block = NULL;
				}
			else
				{
				if (   (s = strchr(buf, ']')) != NULL
					|| (s = strchr(buf, '>')) != NULL
				   )
					*s = '\0';
				plugin_config_block = g_strdup(&buf[1]);
				}
			continue;
			}
		if (plugin_config_block)
			{
			cfg = g_new0(PluginConfigRec, 1);
			cfg->name = g_strdup(plugin_config_block);
			if ((s = strchr(buf, '\n')) != NULL)
				*s = '\0';
			cfg->line = g_strdup(buf);
			gkrellmd_plugin_config_list
						= g_list_append(gkrellmd_plugin_config_list, cfg);
			}
		else	/* main gkrellmd config line */
			{
			arg[0] = '\0';
			sscanf(buf, "%31s %127s", config, arg);
			parse_config(config, arg);
			}
		}
	fclose(f);
	}

const gchar *
gkrellmd_config_getline(GkrellmdMonitor *mon)
	{
	GList			*list;
	PluginConfigRec	*cfg;

	if (!mon->privat)
		{
		mon->privat = g_new0(GkrellmdMonitorPrivate, 1);
		mon->privat->config_list = gkrellmd_plugin_config_list;
		}

	for (list = mon->privat->config_list; list; list = list->next)
		{
		cfg = (PluginConfigRec *) list->data;
		if (!strcmp(cfg->name, mon->name))
			{
			mon->privat->config_list = list->next;
			return cfg->line;
			}
		}
	return NULL;
	}

static void
read_config(void)
	{
	gchar	*path;

	_GK.update_HZ = 3;
	_GK.debug_level = 0;
	_GK.max_clients = 2;
	_GK.server_port = GKRELLMD_SERVER_PORT;

	_GK.fs_interval = 2;
	_GK.nfs_interval = 16;
	_GK.inet_interval = 1;

#if defined(GKRELLMD_SYS_ETC)
	path = g_strdup_printf("%s/%s", GKRELLMD_SYS_ETC, GKRELLMD_CONFIG);
	load_config(path);
	g_free(path);
#endif

#if defined(GKRELLMD_LOCAL_ETC)
	path = g_strdup_printf("%s/%s", GKRELLMD_LOCAL_ETC, GKRELLMD_CONFIG);
	load_config(path);
	g_free(path);
#endif

	_GK.homedir = (gchar *) g_get_home_dir();
	if (!_GK.homedir)
		_GK.homedir = ".";
#if !defined(WIN32)
	path = g_strdup_printf("%s/.%s", _GK.homedir, GKRELLMD_CONFIG);
#else
	path = g_strdup_printf("%s/%s", _GK.homedir, GKRELLMD_CONFIG);
#endif
	load_config(path);
	g_free(path);
	}

static void
usage(void)
	{
#if defined(WIN32)

	printf(_("usage: gkrellmd command [options]\n"));
	printf(_("commands:\n"));
	printf(_("       --console             run gkrellmd on console (not as a service)\n"));
	printf(_("       --install             install gkrellmd service and exit\n"));
	printf(_("       --uninstall           uninstall gkrellmd service and exit\n"));
	printf(_("   -h, --help                display this help and exit\n"));
	printf(_("   -v, --version             output version information and exit\n"));
	printf(_("options (only for command '--console'):\n"));
	printf(_("   -u, --update-hz F         Monitor update frequency\n"));
	printf(_("   -m, --max-clients N       Number of simultaneous clients\n"));
	printf(_("   -A, --address A           Address of network interface to listen on\n"));
	printf(_("   -P, --port P              Server port to listen on\n"));
	printf(_("   -a, --allow-host host     Allow connections from specified hosts\n"));
	printf(_("   -c, --clear-hosts         Clears the current list of allowed hosts\n"));
	printf(_("       --io-timeout N        Close connection after N seconds of no I/O\n"));
	printf(_("       --reconnect-timeout N Try to connect every N seconds after\n"
	         "                             a disconnect\n"));
	printf(_("   -p,  --plugin name        Enable a command line plugin\n"));
	printf(_("   -pe, --plugin-enable name Enable an installed plugin\n"));
	printf(_("       --plist               List plugins and exit\n"));
	printf(_("       --plog                Print plugin install log\n"));
	printf(_("   -V, --verbose             increases the verbosity of gkrellmd\n"));

#else

	printf(_("usage: gkrellmd [options]\n"));
	printf(_("options:\n"));
	printf(_("   -u, --update-hz F         Monitor update frequency\n"));
	printf(_("   -m, --max-clients N       Number of simultaneous clients\n"));
	printf(_("   -A, --address A           Address of network interface to listen on\n"));
	printf(_("   -P, --port P              Server port to listen on\n"));
	printf(_("   -a, --allow-host host     Allow connections from specified hosts\n"));
	printf(_("   -c, --clear-hosts         Clears the current list of allowed hosts\n"));
	printf(_("       --io-timeout N        Close connection after N seconds of no I/O\n"));
	printf(_("       --reconnect-timeout N Try to connect every N seconds after\n"
	         "                             a disconnect\n"));
	printf(_("       --mailbox path        Send local mailbox counts to gkrellm clients.\n"));
	printf(_("   -d, --detach              Run in background and detach from terminal\n"));
	printf(_("   -U, --user username       Change to this username after startup\n"));
	printf(_("   -G, --group groupname     Change to this group after startup\n"));
	printf(_("   -p,  --plugin name        Enable a command line plugin\n"));
	printf(_("   -pe, --plugin-enable name Enable an installed plugin\n"));
	printf(_("       --plist               List plugins and exit\n"));
	printf(_("       --plog                Print plugin install log\n"));
	printf(_("       --pidfile path        Create a PID file\n"));
	printf(_("   -V, --verbose             increases the verbosity of gkrellmd\n"));
	printf(_("   -h, --help                display this help and exit\n"));
	printf(_("   -v, --version             output version information and exit\n"));

#endif
	}

static void
get_args(gint argc, gchar **argv)
	{
	gchar	*opt, *arg;
	gint	i, r;

	for (i = 1; i < argc; ++i)
		{
		opt = argv[i];
		arg = argv[i + 1];
		if (*opt == '-')
			{
			++opt;
			if (*opt == '-')
				++opt;
			}
		if (!strcmp(opt, "verbose") || !strcmp(opt, "V"))
			{
			_GK.verbose += 1;
			continue;
			}
		if (!strcmp(opt, "plist"))
			{
			_GK.list_plugins = TRUE;
			continue;
			}
		if (!strcmp(opt, "plog"))
			{
			_GK.log_plugins = TRUE;
			continue;
			}
#if !defined(WIN32)
		if (!strcmp(opt, "without-libsensors"))
			{
			_GK.without_libsensors = TRUE;
			continue;
			}
#endif /* !WIN32 */
		else if (   i < argc
				 && ((r = parse_config(opt, (i < argc - 1) ? arg : NULL)) >= 0)
				)
			{
			i += r;
			continue;
			}

		printf(_("Bad arg: %s\n"), argv[i]);
		usage();
		exit(0);
		}
	}


static int *
socksetup(int af)
	{
	struct addrinfo	hints, *res, *r;
	gint			maxs, *s, *socks;
#ifndef HAVE_GETADDRINFO
	struct sockaddr_in	sin;
#else
	gchar			portnumber[6];
	gint			error;
#endif

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
#ifdef HAVE_GETADDRINFO
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = af;
	snprintf(portnumber, sizeof(portnumber), "%d", _GK.server_port);
	if (!_GK.server_address || strlen(_GK.server_address) == 0)
		{
		error = getaddrinfo(NULL, portnumber, &hints, &res);
		}
	else
		{
		error = getaddrinfo(_GK.server_address, portnumber, &hints, &res);
		}
	if (error)
		{
		fprintf(stderr, "gkrellmd %s\n", gai_strerror(error));
		return NULL;
		}
#else
	/* Set up the address structure for the listen socket and bind the
	|  listen address to the socket.
	*/
	hints.ai_family = PF_INET;
	hints.ai_addrlen = sizeof(struct sockaddr_in);
	hints.ai_next = NULL;
	hints.ai_addr = (struct sockaddr *) &sin;
	sin.sin_family = PF_INET;
	if (!_GK.server_address || strlen(_GK.server_address) == 0)
		{
		sin.sin_addr.s_addr = INADDR_ANY;
		}
	else
		{
		sin.sin_addr.s_addr = inet_addr(_GK.server_address);
		}
	sin.sin_port = htons(_GK.server_port);
	res = &hints;
#endif

	/* count max number of sockets we may open */
	for (maxs = 0, r = res; r; r = r->ai_next, maxs++)
		;
	socks = malloc((maxs + 1) * sizeof(int));
	if (!socks)
		{
		fprintf(stderr,
			"gkrellmd couldn't allocate memory for sockets\n");
		return NULL;
		}

	*socks = 0;   /* num of sockets counter at start of array */
	s = socks + 1;
	for (r = res; r; r = r->ai_next)
		{
		*s = socket(r->ai_family, r->ai_socktype, r->ai_protocol);
		if (*s < 0)
			continue;

		/* SO_REUSEADDR flag allows the server to restart immediately
		*/
		if (1)
			{
#if defined(WIN32)
			const char on = 1;
#else
			const int on = 1;
#endif
			if (setsockopt(*s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
				{
				fprintf(stderr, "gkrellmd setsockopt (SO_REUSEADDR) failed\n");
				close(*s);
				continue;
				}
			}

#ifdef IPV6_V6ONLY
		if (r->ai_family == AF_INET6)
			{
			const int on = 1;

			if (setsockopt(*s, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on)) < 0)
				{
				fprintf(stderr, "gkrellmd setsockopt (IPV6_V6ONLY) failed\n");
				close(*s);
				continue;
				}
			}
#endif
		if (bind(*s, r->ai_addr, r->ai_addrlen) < 0)
			{
			close(*s);
			continue;
			}
		(*socks)++;
		s++;
	}

#ifdef HAVE_GETADDRINFO
	if (res)
		freeaddrinfo(res);
#endif

	if (*socks == 0)
		{
		fprintf(stderr, "gkrellmd couldn't bind to any socket\n");
		free(socks);
		return NULL;
		}
	return socks;
	}

#if !defined(WIN32)
/* XXX: Recent glibc seems to have daemon(), too. */
#if defined(BSD4_4)
#define HAVE_DAEMON
#endif

#if !defined(HAVE_DAEMON) && !defined(WIN32) && !defined(__solaris__)
#include <paths.h>
#endif

#if !defined(_PATH_DEVNULL)
#define _PATH_DEVNULL	"/dev/null"
#endif

static gboolean
detach_from_terminal(void)
	{
#if !defined(HAVE_DAEMON)
	gint	i, fd;
#endif /* HAVE_DAEMON */

	if (getppid() == 1)	 /* already a daemon */
		return TRUE;

#if defined(HAVE_DAEMON)
	if (daemon(0, 0))
		{
		fprintf(stderr, "gkrellmd detach failed: %s\n", strerror(errno));
		return FALSE;
		}
#else
	i = fork();
	if (i > 0)
		exit(0);

	if (i < 0 || setsid() == -1)		/* new session process group */
		{
		fprintf(stderr, "gkrellmd detach failed: %s\n", strerror(errno));
		return FALSE;
		}

	if ((fd = open(_PATH_DEVNULL, O_RDWR, 0)) != -1)
		{
		dup2(fd, STDIN_FILENO);
		dup2(fd, STDOUT_FILENO);
		dup2(fd, STDERR_FILENO);
		if (fd > 2)
			close(fd);
		}
	chdir("/");
#endif /* HAVE_DAEMON */

//	signal(SIGCHLD, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
#if !defined(MSG_NOSIGNAL)
	signal(SIGPIPE, SIG_IGN);
#endif /* MSG_NOSIGNAL */
	}
#endif /* !defined(WIN32) */


static void
drop_privileges(void)
	{
#if !defined(WIN32)
	if (drop_privs.gid > (uid_t)0)
		{
		(void) setgroups((size_t)0, (gid_t*)0);
		(void) setgid(drop_privs.gid);
		}
	if (drop_privs.uid > (uid_t)0)
		(void) setuid(drop_privs.uid);
#endif
	}


static gint
gkrellmd_run(gint argc, gchar **argv)
	{
#ifdef HAVE_GETADDRINFO
	struct sockaddr_storage client_addr;
#else
	struct sockaddr_in	client_addr;
#endif
	fd_set				read_fds, test_fds;
	struct timeval		tv;
	GkrellmdClient		*client;
	size_t				addr_len;
	gint				fd, server_fd, client_fd, i;
#if defined(WIN32)
	gulong				nbytes;
#else
	gint				nbytes;
#endif /* defined(WIN32) */
	gint				max_fd = -1;
	gint				listen_fds = 0;
	gint				interval, result;

	read_config();
	get_args(argc, argv);

	if (_GK.verbose)
		printf("update_HZ=%d\n", _GK.update_HZ);

#if defined(WIN32)
	if (!service_is_one)
		{
		signal(SIGTERM, cb_sigterm);
		signal(SIGINT, cb_sigterm);
		}
#else
	if (   detach_flag
	    && !_GK.log_plugins && !_GK.list_plugins && _GK.debug_level == 0
	   )
		{
		if (detach_from_terminal() == FALSE)
			return 1;
		}
	else
		{
		signal(SIGTERM, cb_sigterm);
		signal(SIGQUIT, cb_sigterm);
		signal(SIGTSTP, SIG_IGN);
		signal(SIGINT, cb_sigterm);
		}
#endif /* defined(WIN32) */

	make_pidfile();

	gkrellm_sys_main_init();

	drop_privileges();

#if GLIB_CHECK_VERSION(2,0,0)
	g_thread_init(NULL);
#endif

	_GK.start_time = time(0);
	if (_GK.update_HZ < 1 || _GK.update_HZ > 10)
		_GK.update_HZ = 3;
	if (_GK.fs_interval < 1 || _GK.fs_interval > 1000)
		_GK.fs_interval = 2;
	if (_GK.nfs_interval > 10000)
		_GK.nfs_interval = 16;
	if (_GK.inet_interval > 20)
		_GK.inet_interval = 20;

	gkrellmd_load_monitors();

	_GK.server_fd = socksetup(PF_UNSPEC);
	if (_GK.server_fd == NULL)
		{
		fprintf(stderr, "gkrellmd socket() failed: %s\n", strerror(errno));
		gkrellmd_cleanup();
		return 1;
		}

	/* Listen on the socket so a client gkrellm can connect.
	*/
	FD_ZERO(&read_fds);
	for (i = 1; i <= _GK.server_fd[0]; ++i)
		{
		if (listen(_GK.server_fd[i], 5) == -1)
			{
			close(_GK.server_fd[i]);
			continue;
			}
		++listen_fds;
		FD_SET(_GK.server_fd[i], &read_fds);
		if (max_fd < _GK.server_fd[i])
			max_fd = _GK.server_fd[i];
		}
	if (listen_fds <= 0)
		{
		fprintf(stderr, "gkrellmd listen() failed: %s\n", strerror(errno));
		gkrellmd_cleanup();
		return 1;
		}

	interval = 1000000 / _GK.update_HZ;

	// main event loop
#if defined(WIN32)
	/* endless loop if:
	   - we're a service and our service_running flag is TRUE
	   - we're a console-app (--console argument passed at startup
	*/
	while(service_running == TRUE || service_is_one == FALSE)
#else
	while(1)
#endif
		{
		test_fds = read_fds;
#ifdef HAVE_GETADDRINFO
		addr_len = sizeof(struct sockaddr_storage);
#else
		addr_len = sizeof(struct sockaddr_in);
#endif
		tv.tv_usec = interval;
		tv.tv_sec = 0;
		
		result = select(max_fd + 1, &test_fds, NULL, NULL, &tv);
		if (result == -1)
			{
			if (errno == EINTR)
				continue;
			fprintf(stderr, "gkrellmd select() failed: %s\n", strerror(errno));
			gkrellmd_cleanup();
			return 1;
			}

#if 0	/* BUG, result is 0 when test_fds has a set fd!! */
		if (result == 0)	
			{
			gkrellmd_update_monitors();
			continue;
			}
#endif

		for (fd = 0; fd <= max_fd; ++fd)
			{
			if (!FD_ISSET(fd, &test_fds))
				continue;
			server_fd = -1;
			for (i = 1; i <= _GK.server_fd[0]; ++i)
				{
				if (fd == _GK.server_fd[i])
					{
					server_fd = fd;
					break;
					}
				}
			if (server_fd >= 0)
				{
				client_fd = accept(server_fd,
						(struct sockaddr *) &client_addr,
						(socklen_t *) (void *)&addr_len);
				if (client_fd == -1)
					{
					fprintf(stderr, "gkrellmd accept() failed: %s\n",
							strerror(errno));
					gkrellmd_cleanup();
					return 1;
					}
				if (client_fd > max_fd)
					max_fd = client_fd;
				client = accept_client(client_fd,
							(struct sockaddr *)&client_addr, addr_len);
				if (!client)
					{
					close(client_fd);
					continue;
					}
				FD_SET(client_fd, &read_fds);
				gkrellmd_serve_setup(client);
				if (_GK.verbose)
					{
					printf("gkrellmd accepted client: %s:%u\n",
						client->hostname,
						ntohs(((struct sockaddr_in *)&client_addr)->sin_port));
					}
				}
			else
				{
				ioctl(fd, FIONREAD, &nbytes);
				if (nbytes == 0)
					{
					remove_client(fd);
					FD_CLR(fd, &read_fds);
					}
				else
					gkrellmd_client_read(fd, nbytes);
				}
			}
		gkrellmd_update_monitors();
		} /* while(1) */
	return 0;
	} // gkrellmd_main()


#if defined(WIN32)
static void service_update_status(DWORD newState)
{
	service_status.dwCurrentState = newState;
	SetServiceStatus(service_status_handle, &service_status);
}


void WINAPI service_control_handler(DWORD controlCode)
	{
	switch (controlCode)
		{
		case SERVICE_CONTROL_SHUTDOWN:
		case SERVICE_CONTROL_STOP:
			service_update_status(SERVICE_STOP_PENDING);
			service_running = FALSE;
			return;

		default:
			break;
		}
	}


void WINAPI service_main(DWORD argc, TCHAR* argv[])
	{
	/* Init service status */
	service_status.dwServiceType = SERVICE_WIN32;
	service_status.dwCurrentState = SERVICE_STOPPED;
	service_status.dwControlsAccepted = 0;
	service_status.dwWin32ExitCode = NO_ERROR;
	service_status.dwServiceSpecificExitCode = NO_ERROR;
	service_status.dwCheckPoint = 0;
	service_status.dwWaitHint = 0;

	service_status_handle = RegisterServiceCtrlHandler(service_name, service_control_handler);
	if (service_status_handle)
		{
		/* service is starting */
		service_update_status(SERVICE_START_PENDING);

		/* service is running */
		service_status.dwControlsAccepted |= (SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN);
		service_update_status(SERVICE_RUNNING);
		
		service_running = TRUE;
		/*
			gkrellmd_main returns on error or as soon as
			service_running is FALSE (see service_control_handler()) 
		*/
		gkrellmd_run(argc, argv);

		/* service was stopped */
		service_update_status(SERVICE_STOP_PENDING);

		/* service is now stopped */
		service_status.dwControlsAccepted &= ~(SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN);
		service_update_status(SERVICE_STOPPED);
		}
	}


void service_run()
	{
	SERVICE_TABLE_ENTRY serviceTable[] =
		{ {service_name, service_main}, { 0, 0 } };
	service_is_one = TRUE;
	/* Blocking system call, will return if service is not needed anymore */
	StartServiceCtrlDispatcher(serviceTable);
	}


static void service_install()
	{
	SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
	if (scm)
		{
		TCHAR path[_MAX_PATH + 1];
		if (GetModuleFileName(0, path, sizeof(path)/sizeof(path[0])) > 0)
			{
			SC_HANDLE sh;
			sh = CreateService(scm, service_name, service_display_name,
			                   SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
			                   SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, path,
			                   0, 0, 0, 0, 0);
			if (sh)
				{
				printf(_("Gkrellmd service was installed successfully.\n"));
				CloseServiceHandle(sh);
				}
			else
				{
				printf(_("Failed installing gkrellmd service.\n"));
				}
			}
		CloseServiceHandle(scm);
		}
	else
		{
			printf(_("Failed installing gkrellmd service, could not connect to service manager.\n"));
		}
	} /* install_service() */


static void service_uninstall()
	{
	SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
	if (scm)
		{
		SC_HANDLE sh;
		sh = OpenService(scm, service_name, SERVICE_QUERY_STATUS | DELETE);
		if (sh)
			{
			SERVICE_STATUS serviceStatus;
			if (QueryServiceStatus(sh, &serviceStatus))
				{
				if (serviceStatus.dwCurrentState == SERVICE_STOPPED)
					{
					BOOL delRet;
					delRet = DeleteService(sh);
					if (delRet != 0)
						{
						printf(_("Gkrellmd service was uninstalled successfully.\n"));
						}
					else
						{
						printf(_("Failed uninstalling gkrellmd service.\n"));
						}
					}
				else
					{
					printf(_("Failed uninstalling gkrellmd service, service is still running.\n"));
					}
				}
			CloseServiceHandle(sh);
			}
		else
			{
				printf(_("Failed uninstalling gkrellmd service, could not open gkrellmd service.\n"));
			}
		CloseServiceHandle(scm);
		}
	else
		{
			printf(_("Failed uninstalling gkrellmd service, could not connect to service manager.\n"));
		}
	} /* uninstall_service */
#endif /* defined(WIN32) */


GkrellmdTicks *
gkrellmd_ticks(void)
	{
	return &GK;
	}


gint
gkrellmd_get_timer_ticks(void)
	{
	return GK.timer_ticks;
	}


int main(int argc, char* argv[])
	{
	int i;
	char *opt;

#ifdef ENABLE_NLS
#ifdef LOCALEDIR
#if defined(WIN32)
	/*
	Prepend app install path to relative locale dir, don't rely on CWD being correct
	*/
	if (!G_IS_DIR_SEPARATOR(LOCALEDIR[0]))
		{
 		gchar* locale_dir;
		locale_dir = g_win32_get_package_installation_subdirectory(NULL, NULL, LOCALEDIR);
		if (locale_dir != NULL)
			{
			bindtextdomain(PACKAGE_D, locale_dir);
			g_free(locale_dir);
			}
		}
	else
		{
		bindtextdomain(PACKAGE_D, LOCALEDIR);
		}
#else
	bindtextdomain(PACKAGE_D, LOCALEDIR);
#endif /* !WIN32 */
#endif /* LOCALEDIR */
	textdomain(PACKAGE_D);
	bind_textdomain_codeset(PACKAGE_D, "UTF-8");
#endif	/* ENABLE_NLS */

	/* Parse arguments for actions which exit gkrellmd immediately */
	for (i = 1; i < argc; ++i)
		{
		opt = argv[i];
		if (*opt == '-')
			{
			++opt;
			if (*opt == '-')
				++opt;
			}

		if (!strcmp(opt, "help") || !strcmp(opt, "h"))
			{
			usage();
			return 0;
			}
		else if (!strcmp(opt, "version") || !strcmp(opt, "v"))
			{
			printf("gkrellmd %d.%d.%d%s\n", GKRELLMD_VERSION_MAJOR,
					GKRELLMD_VERSION_MINOR, GKRELLMD_VERSION_REV,
					GKRELLMD_EXTRAVERSION);
			return 0;
			}
#if defined(WIN32)
		else if (!strcmp(opt, "install"))
			{
			service_install();
			return 0;
			}
		else if (!strcmp(opt, "uninstall"))
			{
			service_uninstall();
			return 0;
			}
		else if (!strcmp(opt, "console"))
			{
			/*
			Special case for windows: run gkrellmd on console and not as
			a service. This is helpful for debugging purposes.
			*/
			int retVal;
			int newArgc = 0;
			char **newArgv = malloc((argc -1) * sizeof(char *));
			int j;
			for (j = 0; j < argc; ++j)
			{
				/* filter out option "--console" */
				if (j == i)
					continue;
				newArgv[newArgc++] = argv[j];
			}
			retVal = gkrellmd_run(newArgc, newArgv);
			free(newArgv);
			return retVal;
			}
#endif /* defined(WIN32) */
		} /* for() */

#if defined(WIN32)
	/* Win: register service and wait for the service to be started/stopped */
	service_run();
	return 0;
#else
	/* Unix: just enter main loop */
	return gkrellmd_run(argc, argv);
#endif
	} /* main() */
