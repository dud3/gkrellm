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

#include <inttypes.h>

#if !defined(WIN32)
#include <sys/socket.h>
#include <utime.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#else
#define uint32_t u_long
#include <winsock2.h>
#endif

#include <errno.h>



#if defined(__linux__)
#if defined(__GLIBC__) && ((__GLIBC__>2)||(__GLIBC__==2 && __GLIBC_MINOR__>=1))
#define HAVE_GETADDRINFO	1
#endif
#endif

#if defined(__DragonFly__)
#define HAVE_GETADDRINFO	1
#endif

#if defined(__FreeBSD__)
#if __FreeBSD_version >= 400000
#define HAVE_GETADDRINFO	1
#endif
#endif

#if defined(__OpenBSD__)
#define HAVE_GETADDRINFO	1
#endif

#if defined(__NetBSD__)
#define HAVE_GETADDRINFO	1
#endif

#if defined(__solaris__)
/* getaddrinfo is related to IPv6 stuff */
# include <netconfig.h>
# if defined(NC_INET6)
#  define HAVE_GETADDRINFO	1
# endif
#endif


typedef struct
	{
	gchar	*key;
	void	(*func)();
	}
	KeyTable;

static gint		server_major_version,
				server_minor_version,
				server_rev_version;

static gint		client_input_id,
				client_fd;
static gboolean	server_alive;

static gchar	server_buf[4097];
static gint		buf_index;

static gchar	locale_decimal_point;
static gchar	server_decimal_point = '.';
static gboolean	need_locale_fix = FALSE;


gboolean
gkrellm_client_check_server_version(gint major, gint minor, gint rev)
    {
    if (   server_major_version > major
        || (server_major_version == major && server_minor_version > minor)
        || (   server_major_version == major && server_minor_version == minor
            && server_rev_version >= rev
           )
       )
        return TRUE;
    return FALSE;
    }


  /* There can be a locale decimal point mismatch with the server.
  */
static void
locale_fix(gchar *buf)
	{
	gchar   *s;

	for (s = buf; *s; ++s)
		if (*s == server_decimal_point)
			*s = locale_decimal_point;
	}


/* ================================================================= */
/* CPU */

typedef struct
	{
	gint	instance;
	gulong	user,
			nice,
			sys,
			idle;
	}
	Cpu;

static GList	*cpu_list,
				*instance_list;
static gint		n_cpus = 1;
static gboolean	nice_time_supported = TRUE;

static void
client_cpu_line_from_server(gchar *line)
	{
	GList	*list;
	Cpu		*cpu = NULL;
	gint	n;
	guint64	user, nice, sys, idle;

	sscanf(line, "%d %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64, &n, &user, &nice, &sys, &idle);
	for (list = cpu_list; list; list = list->next)
		{
		cpu = (Cpu *) list->data;
		if (cpu->instance == n)
			break;
		}
	if (list)
		{
		cpu->user = user;
		cpu->nice = nice;
		cpu->sys = sys;
		cpu->idle = idle;
		}
	}

static void
cpu_read_data(void)
	{
	Cpu		*cpu;
	GList 	*list;

	for (list = cpu_list; list; list = list->next)
		{
		cpu = (Cpu *) list->data;
		gkrellm_cpu_assign_data(cpu->instance, cpu->user, cpu->nice,
					cpu->sys, cpu->idle);
		}
	}

static void
client_sys_cpu_init(void)
	{
	GList	*list;
	Cpu		*cpu;
	gint	n;

	/* Do initialization based on info received in client_cpu_setup().  Here
	|  we need to get the same effective work done as would be done in the
	|  sysdep gkrellm_sys_cpu_init() when not running in client mode.
	*/
	for (n = 0; n < n_cpus; ++ n)
		{
		cpu = g_new0(Cpu, 1);
		if (instance_list && (list = g_list_nth(instance_list, n)) != NULL)
			{
			cpu->instance = GPOINTER_TO_INT(list->data);
			gkrellm_cpu_add_instance(cpu->instance);
			}
		else
			cpu->instance = n;
		cpu_list = g_list_append(cpu_list, cpu);
		}
	gkrellm_cpu_set_number_of_cpus(n_cpus);
	if (!nice_time_supported)
		gkrellm_cpu_nice_time_unsupported();

	/* Diverting the cpu_read_data function in cpu.c causes the cpu monitor
	|  to not call the gkrellm_sys_cpu_init() sysdep function and also to call
	|  our client cpu_read_data function instead of gkrellm_sys_cpu_read_data()
	*/
	gkrellm_cpu_client_divert(cpu_read_data);
	}

static void
client_cpu_setup(gchar *line)
	{
	gint	instance;

	if (!strncmp(line, "n_cpus", 6))
		sscanf(line, "%*s %d", &n_cpus);
	else if (!strncmp(line, "cpu_instance", 12))
		{
		sscanf(line, "%*s %d", &instance);
		instance_list = g_list_append(instance_list,
				GINT_TO_POINTER(instance));
		}
	else if (!strcmp(line, "nice_time_unsupported"))
		nice_time_supported = FALSE;
	}

/* ================================================================= */
static struct
	{
	gint	n_processes,
			n_running,
			n_users;
	gulong	n_forks;
	gfloat	load;
	}
	proc;

static void
client_proc_line_from_server(gchar *line)
	{
	if (need_locale_fix)
		locale_fix(line);
	sscanf(line, "%d %d %lu %f %d", &proc.n_processes, &proc.n_running,
			&proc.n_forks, &proc.load, &proc.n_users);
	}

static void
read_proc_data(void)
	{
	gkrellm_proc_assign_data(proc.n_processes, proc.n_running,
				proc.n_forks, proc.load);
	}

static void
read_user_data(void)
	{
	gkrellm_proc_assign_users(proc.n_users);
	}

static void
client_sys_proc_init(void)
	{
	gkrellm_proc_client_divert(read_proc_data, read_user_data);
	}

/* ================================================================= */
typedef struct
	{
	gchar		*name;
	gchar		*subdisk_parent;
	guint64		rblk,
				wblk;
	gboolean	virtual;
	}
	DiskData;

static GList	*disk_list;

static gboolean	units_are_blocks;

static void
client_disk_line_from_server(gchar *line)
	{
	DiskData	*disk = NULL;
	GList		*list;
	gchar		name[16], s1[32], s2[32], s3[32];
	guint64		rblk, wblk;
	gint		n;
	gboolean	virtual = FALSE;

	n = sscanf(line, "%15s %31s %31s %31s", name, s1, s2, s3);
	if (n == 4)
		{
		if (   gkrellm_client_check_server_version(2, 2, 7)
			&& !strcmp(s1, "virtual")
		   )
			{
			virtual = TRUE;
			s1[0] = '\0';
			}
		rblk = strtoull(s2, NULL, 0);
		wblk = strtoull(s3, NULL, 0);
		}
	else if (n == 3)
		{
		rblk = strtoull(s1, NULL, 0);
		wblk = strtoull(s2, NULL, 0);
		s1[0] = '\0';
		}
	else
		return;
	for (list = disk_list; list; list = list->next)
		{
		disk = (DiskData *) list->data;
		if (!strcmp(disk->name, name))
			break;
		}
	if (!list)
		{
		disk = g_new0(DiskData, 1);
		disk->name = g_strdup(name);
		if (s1[0])		/* I expect server to send in order */
			disk->subdisk_parent = g_strdup(s1);
		disk_list = g_list_append(disk_list, disk);
		}
	if (disk)
		{
		disk->rblk = rblk;
		disk->wblk = wblk;
		disk->virtual = virtual;
		}
	}

static void
read_disk_data(void)
	{
	GList		*list;
	DiskData	*disk;

	for (list = disk_list; list; list = list->next)
		{
		disk = (DiskData *) list->data;
		if (disk->subdisk_parent)
			gkrellm_disk_subdisk_assign_data_by_name(disk->name,
					disk->subdisk_parent, disk->rblk, disk->wblk);
		else
			gkrellm_disk_assign_data_by_name(disk->name,
					disk->rblk, disk->wblk, disk->virtual);
		}
	}

static gint
order_from_name(gchar *name)
	{
	return -1;
	}

static void
client_sys_disk_init(void)
	{
	gkrellm_disk_client_divert(read_disk_data, NULL, order_from_name);
	if (units_are_blocks)
		gkrellm_disk_units_are_blocks();

	/* Disk monitor config needs to know will be using assign by name
	*/
	gkrellm_disk_assign_data_by_name(NULL, 0, 0, FALSE);
	}

static void
client_disk_setup(gchar *line)
	{
	if (!strcmp(line, "units_are_blocks"))
		units_are_blocks = TRUE;
	}

/* ================================================================= */
typedef struct
	{
	gchar		*name;
	gboolean	up_event,
				down_event;

	gint		up_time;

	gulong		rx,
				tx;
	}
	NetData;

static NetData	*net_timer;

static GList	*net_list;

static gchar	*net_timer_name;

static gboolean	net_server_use_routed;

static void
client_net_line_from_server(gchar *line)
	{
	GList		*list;
	NetData		*net;
	gchar		name[32];
	guint64		rx, tx;

	sscanf(line, "%31s %" PRIu64 " %" PRIu64, name, &rx, &tx);
	for (list = net_list; list; list = list->next)
		{
		net = (NetData *) list->data;
		if (!strcmp(name, net->name))
			{
			net->rx = rx;
			net->tx = tx;
			break;
			}
		}
	if (!list)
		{
		net = g_new0(NetData, 1);
		net->name = g_strdup(name);
		net_list = g_list_append(net_list, net);
		net->rx = rx;
		net->tx = tx;

		if (net_timer_name && !strcmp(net_timer_name, net->name))
			net_timer = net;
		}
	}

static void
client_net_routed_line_from_server(gchar *line)
	{
	GList		*list;
	NetData		*net = NULL;
	gchar		name[32];
	gboolean	routed;

	sscanf(line, "%31s %d", name, &routed);
	for (list = net_list; list; list = list->next)
		{
		net = (NetData *) list->data;
		if (!strcmp(name, net->name))
			break;
		}
	if (!list)
		{
		net = g_new0(NetData, 1);
		net->name = g_strdup(name);
		net_list = g_list_append(net_list, net);
		}
	if (net)
		{
		if (routed)
			net->up_event = TRUE;
		else
			net->down_event = TRUE;
		}
	}

gint
gkrellm_client_server_get_net_timer(void)
	{
	return net_timer ? net_timer->up_time : 0;
	}

static void
client_net_timer_line_from_server(gchar *line)
	{
	gchar		name[32];

	if (!net_timer)
		return;
	sscanf(line, "%s %d", name, &net_timer->up_time);
	}

static void
check_net_routes(void)
	{
	GList	*list;
	NetData	*net;

	for (list = net_list; list; list = list->next)
		{
		net = (NetData *) list->data;
		if (net->up_event || net->down_event)
			gkrellm_net_routed_event(net->name, net->up_event);
		net->up_event = net->down_event = FALSE;
		}
	}

static void
read_net_data(void)
	{
	GList	*list;
	NetData	*net;

	for (list = net_list; list; list = list->next)
		{
		net = (NetData *) list->data;
		gkrellm_net_assign_data(net->name, net->rx, net->tx);
		}
	}

  /* gkrellmd to gkrellm server/client interface always uses the net routed
  |  mode regardless if the server uses routed in its sysdep code.
  */
static void
client_sys_net_init(void)
	{
	gkrellm_net_client_divert(read_net_data, check_net_routes, NULL);
	gkrellm_net_use_routed(net_server_use_routed);

	if (net_timer_name)
		gkrellm_net_server_has_timer();
	}

static void
client_net_setup(gchar *line)
	{
	gchar	buf[32];

	/* This is the server sysdep net_use_routed value.  The client <-> server
	|  link always uses routed mode.
	*/
	if (!strcmp(line, "net_use_routed"))
		net_server_use_routed = TRUE;

	if (!strncmp(line, "net_timer", 9))
		{
		sscanf(line, "%*s %31s\n", buf);
		net_timer_name = g_strdup(buf);
		}
	}


/* ================================================================= */
#include "../src/inet.h"

static GList	*inet_list;

static gboolean	inet_unsupported;

static void
client_inet_line_from_server(gchar *line)
	{
	GList		*list;
	ActiveTCP	tcp, *t;
	gchar		*ap, *aap;
#if defined(INET6) && defined(HAVE_GETADDRINFO)
	struct addrinfo	hints, *res;
	gchar		buf[NI_MAXHOST];
#else
	gchar		buf[128];
#endif
	gint		n, slen;

	if (_GK.debug_level & DEBUG_INET)
		g_print("inet server: %s\n", line);
	if (*(line + 1) == '0')
		{
		n = sscanf(line + 3, "%x %127[^:]:%x", &tcp.local_port,
			   buf, &tcp.remote_port);
		if (n != 3 || inet_aton(buf, &tcp.remote_addr) == 0)
			return;
		tcp.family = AF_INET;
		}
#if defined(INET6) && defined(HAVE_GETADDRINFO)
	else if (*(line + 1) == '6')
		{
#define STR(x)	#x
#define XSTR(x)	STR(x)
		n = sscanf(line + 3, "%x [%" XSTR(NI_MAXHOST) "[^]]]:%x",
			   &tcp.local_port, buf, &tcp.remote_port);
		if (n != 3)
			return;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET6;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST;
		if (getaddrinfo(buf, NULL, &hints, &res) != 0)
			return;
		memcpy(&tcp.remote_addr6,
		       &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr,
		       sizeof(struct in6_addr));
		freeaddrinfo(res);
		tcp.family = AF_INET6;
		}
#endif
	if (*line == '+')
		{
		t = g_new0(ActiveTCP, 1);
		*t = tcp;
		inet_list = g_list_append(inet_list, t);
		}
	else if (*line == '-')
		{
		for (list = inet_list; list; list = list->next)
			{
			t = (ActiveTCP *) list->data;
			if (t->family == AF_INET)
				{
				ap = (gchar *) &t->remote_addr;
				aap = (gchar *) &tcp.remote_addr;
				slen = sizeof(struct in_addr);
				}
#if defined(INET6)
			else if (t->family == AF_INET6)
				{
				ap = (gchar *) &t->remote_addr6;
				aap = (gchar *) &tcp.remote_addr6;
				slen = sizeof(struct in6_addr);
				}
#endif
			else
				return;
			if (   memcmp(aap, ap, slen) == 0
				&& tcp.remote_port == t->remote_port
				&& tcp.local_port == t->local_port
			   )
				{
				g_free(t);
				inet_list = g_list_remove_link(inet_list, list);
				break;
				}
			}
		}
	}

static void
read_tcp_data(void)
	{
	GList		*list;
	ActiveTCP	*tcp;

	for (list = inet_list; list; list = list->next)
		{
		tcp = (ActiveTCP *) list->data;
		gkrellm_inet_log_tcp_port_data(tcp);
		}
	}

static void
client_inet_reset(void)
	{
	gkrellm_free_glist_and_data(&inet_list);
	}

static void
client_sys_inet_init(void)
	{
	if (inet_unsupported)
		return;
	gkrellm_inet_client_divert(read_tcp_data);
	}

static void
client_inet_setup(gchar *line)
	{
	if (!strcmp(line, "inet_unsupported"))
		inet_unsupported = TRUE;
	}

/* ================================================================= */
struct
	{
	guint64		total,
				used,
				free,
				shared,
				buffers,
				cached;
	guint64		swap_total,
				swap_used;
	gulong		swap_in,
				swap_out;
	}
	mem;

static void
client_mem_line_from_server(gchar *line)
	{
	sscanf(line, "%" PRIu64 "%" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64,
			&mem.total, &mem.used, &mem.free,
			&mem.shared, &mem.buffers, &mem.cached);
	}

static void
client_swap_line_from_server(gchar *line)
	{
	sscanf(line, "%" PRIu64 " %" PRIu64 " %lu %lu",
			&mem.swap_total, &mem.swap_used,
			&mem.swap_in, &mem.swap_out);
	}

static void
read_mem_data(void)
	{
	gkrellm_mem_assign_data(mem.total, mem.used, mem.free, mem.shared,
				mem.buffers, mem.cached);
	}

static void
read_swap_data(void)
	{
	gkrellm_swap_assign_data(mem.swap_total, mem.swap_used,
				mem.swap_in, mem.swap_out);
	}

static void
client_sys_mem_init(void)
	{
	gkrellm_mem_client_divert(read_mem_data, read_swap_data);
	}

/* ================================================================= */
typedef struct
	{
	gchar	*directory,
			*device,
			*type;
	gulong	blocks,
			bavail,
			bfree,
			bsize;
	}
	Mount;

static GList	*mounts_list,
				*fstab_list;

static gboolean fstab_modified,
				mounting_unsupported;

static void
client_fstab_line_from_server(gchar *line)
	{
	GList	*list;
	Mount	*m;
	gchar	dir[128], dev[64], type[64];

	if (!strcmp(line, ".clear"))
		{
		for (list = fstab_list; list; list = list->next)
			{
			m = (Mount *) list->data;
			g_free(m->directory);
			g_free(m->device);
			g_free(m->type);
			g_free(m);
			}
		g_list_free(fstab_list);
		fstab_list = NULL;
		fstab_modified = TRUE;
		}
	else
		{
		m = g_new0(Mount, 1);
		sscanf(line, "%127s %63s %63s", dir, dev, type);
		m->directory = g_strdup(dir);
		m->device = g_strdup(dev);
		m->type = g_strdup(type);
		fstab_list = g_list_append(fstab_list, m);
		}
	}

static void
client_mounts_line_from_server(gchar *line)
	{
	GList	*list;
	Mount	*m;
	gchar	dir[128], dev[64], type[64];

	if (!strcmp(line, ".clear"))
		{
		for (list = mounts_list; list; list = list->next)
			{
			m = (Mount *) list->data;
			g_free(m->directory);
			g_free(m->device);
			g_free(m->type);
			g_free(m);
			}
		g_list_free(mounts_list);
		mounts_list = NULL;
		}
	else
		{
		m = g_new0(Mount, 1);
		sscanf(line, "%127s %63s %63s %lu %lu %lu %lu", dir, dev, type,
				&m->blocks, &m->bavail, &m->bfree, &m->bsize);
		m->directory = g_strdup(dir);
		m->device = g_strdup(dev);
		m->type = g_strdup(type);
		mounts_list = g_list_append(mounts_list, m);
		}
	}

static void
client_fs_line_from_server(gchar *line)
	{
	GList	*list;
	Mount	*m;
	gchar	dir[128], dev[64];
	gulong	blocks, bavail, bfree, bsize;

	sscanf(line, "%127s %63s %lu %lu %lu %lu", dir, dev,
				&blocks, &bavail, &bfree, &bsize);
	for (list = mounts_list; list; list = list->next)
		{
		m = (Mount *) list->data;
		if (!strcmp(m->directory, dir) && !strcmp(m->device, dev))
			{
			m->blocks = blocks;
			m->bavail = bavail;
			m->bfree = bfree;
			m->bsize = bsize;
			break;
			}
		}
	}

static void
get_fsusage(gpointer fs, gchar *dir)
	{
	GList	*list;
	Mount	*m;

	for (list = mounts_list; list; list = list->next)
		{
		m = (Mount *) list->data;
		if (!strcmp(m->directory, dir))
			{
			gkrellm_fs_assign_fsusage_data(fs, m->blocks, m->bavail,
						m->bfree, m->bsize);
			break;
			}
		}
	}

static void
get_mounts_list(void)
	{
	GList	*list;
	Mount	*m;

	for (list = mounts_list; list; list = list->next)
		{
		m = (Mount *) list->data;
		gkrellm_fs_add_to_mounts_list(m->directory, m->device, m->type);
		}
	}

static void
get_fstab_list(void)
	{
	GList	*list;
	Mount	*m;

	for (list = fstab_list; list; list = list->next)
		{
		m = (Mount *) list->data;
		gkrellm_fs_add_to_fstab_list(m->directory, m->device,
				m->type, "none" /* options NA since no mounting */ );
		}
	fstab_modified = FALSE;
	}

static gboolean
get_fstab_modified(void)
	{
	return fstab_modified;
	}

static void
client_sys_fs_init(void)
	{
	gkrellm_fs_client_divert(get_fsusage, get_mounts_list,
				get_fstab_list, get_fstab_modified);
	if (mounting_unsupported)
		gkrellm_fs_mounting_unsupported();
	}

static void
client_fs_setup(gchar *line)
	{
	if (!strcmp(line, "mounting_unsupported"))
		mounting_unsupported = TRUE;
	}

/* ================================================================= */
typedef struct
	{
	gchar		*path;
	gint		total,
				new;
	gpointer	mbox_ptr;
	}
	Mailbox;

static GList	*mailbox_list;

static gboolean
check_mail(Mailbox *mbox)
	{
	gkrellm_set_external_mbox_counts(mbox->mbox_ptr, mbox->total, mbox->new);
	return TRUE;
	}

static void
client_mail_line_from_server(gchar *line)
	{
	Mailbox		*mbox = NULL;
	GList		*list;
	gchar		path[256];
//	gchar		*s;
	gint		total, new;

	if (sscanf(line, "%255s %d %d", path, &total, &new) < 3)
		return;
	for (list = mailbox_list; list; list = list->next)
		{
		mbox = (Mailbox *) list->data;
		if (!strcmp(path, mbox->path))
			break;
		}
	if (!list)
		{
		mbox = g_new0(Mailbox, 1);
		mbox->path = g_strdup(path);
		mailbox_list = g_list_append(mailbox_list, mbox);
		mbox->mbox_ptr =
				gkrellm_add_external_mbox(check_mail, FALSE, mbox);
		gkrellm_set_external_mbox_tooltip(mbox->mbox_ptr, mbox->path);

//		s = g_strdup_printf("%s:%s", _GK.server_hostname, mbox->path);
//		gkrellm_set_external_mbox_tooltip(mbox->mbox_ptr, s);
//		g_free(s);
		}
	if (mbox)
		{
		mbox->total = total;
		mbox->new = new;
		}
	}

static void
client_sys_mail_init(void)
	{
	}

static void
client_mail_setup(gchar *line)
	{
	}

/* ================================================================= */
GList		*battery_list;

gboolean 	batteries_available;

typedef struct
	{
	gint		id;
    gboolean	present,
				on_line,
				charging;
	gint		percent;
	gint		time_left;
	}
	Battery;

static Battery	*composite_battery;

static Battery *
battery_nth(gint n)
	{
	Battery		*bat;
	static gint	n_batteries;

	if (n > 10)
		return NULL;
	if (n < 0)
		{
		if (!composite_battery)
			{
			bat = g_new0(Battery, 1);
			battery_list = g_list_prepend(battery_list, bat);
			bat->id = GKRELLM_BATTERY_COMPOSITE_ID;
			composite_battery = bat;
			}
		return composite_battery;
		}

	if (composite_battery)
		++n;

	while ((bat = (Battery *)g_list_nth_data(battery_list, n)) == NULL)
		{
		bat = g_new0(Battery, 1);
		battery_list = g_list_append(battery_list, bat);
		bat->id = n_batteries++;
		}
	return bat;
	}


static void
client_battery_line_from_server(gchar *line)
	{
	Battery		*bat;
	gboolean	present, on_line, charging;
	gint		percent, time_left, n = 0;

	/* 2.1.9 adds 6th arg battery id number
	*/
	if (sscanf(line, "%d %d %d %d %d %d",
				&present, &on_line, &charging, &percent, &time_left, &n) < 5)
		return;

	bat = battery_nth(n);
	if (!bat)
		return;
	bat->present = present;
	bat->on_line = on_line;
	bat->charging = charging;
	bat->percent = percent;
	bat->time_left = time_left;
	}

static void
read_battery_data(void)
	{
	GList	*list;
	Battery	*bat;

	for (list = battery_list; list; list = list->next)
		{
		bat = (Battery *) list->data;
		gkrellm_battery_assign_data(bat->id, bat->present,
				bat->on_line, bat->charging,
				bat->percent, bat->time_left);
		}
	}

static void
client_sys_battery_init(void)
	{
	if (batteries_available)
		gkrellm_battery_client_divert(read_battery_data);
	}

static void
client_battery_setup(gchar *line)
	{
	if (   !strcmp(line, "apm_available")
		|| !strcmp(line, "battery_available")
	   )
		batteries_available = TRUE;
	}

/* ================================================================= */
typedef struct
	{
	gint		type;

	gchar		*basename;
	gint		id;
	gint		iodev;
	gint		inter;

	gint		group;

	gchar		*vref;
	gchar		*default_label;

	gfloat		factor;
	gfloat		offset;
	gfloat		raw_value;
	}
	Sensor;

static GList	*sensors_list;


static void
client_sensors_line_from_server(gchar *line)
	{
	GList	*list;
	Sensor	s, *sensor;
	gchar	basename[128];

	if (need_locale_fix)
		locale_fix(line);
	sscanf(line, "%d \"%127[^\"]\" %d %d %d %f",
			&s.type, basename, &s.id, &s.iodev, &s.inter, &s.raw_value);
	for (list = sensors_list; list; list = list->next)
		{
		sensor = (Sensor *) list->data;
		if (   sensor->type == s.type && !strcmp(sensor->basename, basename)
			&& sensor->id == s.id && sensor->iodev == s.iodev
			&& sensor->inter == s.inter
		   )
			{
			sensor->raw_value = s.raw_value;
			break;
			}
		}
	}

static gboolean
get_temperature(gchar *path, gint id, gint iodev, gint inter, gfloat *value)
	{
	GList	*list;
	Sensor	*s;

	for (list = sensors_list; list; list = list->next)
		{
		s = (Sensor *) list->data;
		if (   s->type == SENSOR_TEMPERATURE && !strcmp(s->basename, path)
			&& s->id == id && s->iodev == iodev && s->inter == inter
		   )
			{
			*value = s->raw_value;
			return TRUE;
			}
		}
	return FALSE;
	}

static gboolean
get_fan(gchar *path, gint id, gint iodev, gint inter, gfloat *value)
	{
	GList	*list;
	Sensor	*s;

	for (list = sensors_list; list; list = list->next)
		{
		s = (Sensor *) list->data;
		if (   s->type == SENSOR_FAN && !strcmp(s->basename, path)
			&& s->id == id && s->iodev == iodev && s->inter == inter
		   )
			{
			*value = s->raw_value;
			return TRUE;
			}
		}
	return FALSE;
	}

static gboolean
get_voltage(gchar *path, gint id, gint iodev, gint inter, gfloat *value)
	{
	GList	*list;
	Sensor	*s;

	for (list = sensors_list; list; list = list->next)
		{
		s = (Sensor *) list->data;
		if (   s->type == SENSOR_VOLTAGE && !strcmp(s->basename, path)
			&& s->id == id && s->iodev == iodev && s->inter == inter
		   )
			{
			*value = s->raw_value;
			return TRUE;
			}
		}
	return FALSE;
	}

static void
client_sys_sensors_init(void)
	{
	GList			*list;
	Sensor			*s;
	gpointer		sr;

	if (!sensors_list)
		return;

	gkrellm_sensors_client_divert(get_temperature, get_fan, get_voltage);
	for (list = sensors_list; list; list = list->next)
		{
		s = (Sensor *) list->data;

		/* The sysdep code in the server may be using the dir arg to get sensor
		|  values, but dir is no longer needed.
		*/
		sr = gkrellm_sensors_add_sensor(s->type, NULL, s->basename,
					s->id, s->iodev, s->inter,
					s->factor, s->offset,
					s->vref, s->default_label);
		gkrellm_sensors_set_group(sr, s->group);
		}
	}

static void
client_sensors_setup(gchar *line)
	{
	Sensor	*s;
	gchar	basename[128], vref[32], default_label[32];

	if (need_locale_fix)
		locale_fix(line);

	s = g_new0(Sensor, 1);
	s->group = SENSOR_GROUP_MAINBOARD;	/* Not in pre 2.2.0 versions */
	if (sscanf(line,
			"%d \"%127[^\"]\" %d %d %d %f %f \"%31[^\"]\" \"%31[^\"]\" %d",
			&s->type, basename, &s->id, &s->iodev, &s->inter,
			&s->factor, &s->offset, vref, default_label, &s->group) >= 9)
		{
		s->basename = g_strdup(basename);
		if (strcmp(vref, "NONE"))
			s->vref = g_strdup(vref);
		if (strcmp(default_label, "NONE"))
			s->default_label = g_strdup(default_label);

		sensors_list = g_list_append(sensors_list, s);
		}
	else
		g_free(s);
	}

/* ================================================================= */
static time_t		server_uptime;

static void
client_uptime_line_from_server(gchar *s)
	{
	gulong		up_minutes;

	sscanf(s, "%lu", &up_minutes);
	server_uptime = ((time_t) up_minutes) * 60;
	}

static time_t
client_read_uptime(void)
	{
	return server_uptime;
	}

static void
client_sys_uptime_init(void)
	{
	gkrellm_uptime_client_divert(client_read_uptime);
	}

/* ================================================================= */
static struct tm	server_time;

  /* clock monitor doesn't have a sysdep interface, so it needs a hook
  |  to get server system time when in client mode.
  */
struct tm *
gkrellm_client_server_time(void)
	{
	return &server_time;
	}

static void
client_time_line_from_server(gchar *s)
	{
	struct tm	*t;

	t = &server_time;
	sscanf(s, "%d %d %d %d %d %d %d %d %d",
			&t->tm_sec, &t->tm_min, &t->tm_hour,
			&t->tm_mday, &t->tm_mon, &t->tm_year,
			&t->tm_wday, &t->tm_yday, &t->tm_isdst);
	}

/* ================================================================= */
static void
client_server_version_setup(gchar *line)
	{
	sscanf(line, "%*s %d.%d.%d", &server_major_version,
                    &server_minor_version, &server_rev_version);
	}

static void
client_hostname_setup(gchar *s)
	{
	g_free(_GK.server_hostname);
	_GK.server_hostname = g_strdup(s);
	}

static void
client_sysname_setup(gchar *s)
	{
	g_free(_GK.server_sysname);
	_GK.server_sysname = g_strdup(s);
	}


KeyTable	monitor_table[] =
	{
	{"sensors",		client_sys_sensors_init },
	{"cpu",			client_sys_cpu_init },
	{"proc",		client_sys_proc_init },
	{"disk",		client_sys_disk_init },
	{"net",			client_sys_net_init },
	{"inet",		client_sys_inet_init },
	{"mem",			client_sys_mem_init },
	{"fs",			client_sys_fs_init },
	{"mail",		client_sys_mail_init },
	{"apm",			client_sys_battery_init },
	{"battery",		client_sys_battery_init },
	{"uptime",		client_sys_uptime_init },
	};


static gboolean	setup_done;		/* only one sys init */


  /* Setup lines are received before monitor init functions are called, so
  |  for plugins must save the strings until plugins are loaded.
  */
static GList	*client_plugin_setup_line_list;

static void
client_plugin_add_setup_line(gchar *line)
	{
	client_plugin_setup_line_list
				= g_list_append(client_plugin_setup_line_list, g_strdup(line));
	}

  /* Plugins should call this in their gkrellm_init_plugin() function.
  */
void
gkrellm_client_plugin_get_setup(gchar *key_name,
				void (*setup_func_cb)(gchar *str))
	{
	GList	*list;
	gchar	*line, *s;
	gint	n;

	if (!_GK.client_mode || !key_name || !setup_func_cb)
		return;
	for (list = client_plugin_setup_line_list; list; list = list->next)
		{
		line = (gchar *) list->data;
		n = strlen(key_name);
		s = line + n;
		if (!strncmp(line, key_name, n) && *s == ' ')
			{
			while (*s == ' ')
				++s;
			(*setup_func_cb)(s);
			}
		}
	}

static void
client_monitor_setup(gchar *line)
	{
	void			(*func)();
	gchar			buf[64];
	gint			i;
	gboolean		found_builtin = FALSE;

	if (!*line || setup_done)
		return;
	for (i = 0; i < sizeof(monitor_table) / sizeof(KeyTable); ++i)
		{
		if (!strcmp(line, monitor_table[i].key))
			{
			func = monitor_table[i].func;
			(*func)();
			found_builtin = TRUE;
			break;
			}
		}
	/* The client mode init work of a plugin must be defered since they
	|  aren't loaded yet.  Set up so they will can get an "available" flag.
	*/
	if (!found_builtin)
		{
		snprintf(buf, sizeof(buf), "%s available", line);
		client_plugin_add_setup_line(buf);
		}
	}

static void
client_server_error(gchar *line)
	{
	fprintf(stderr, "gkrellmd error: %s\n", line);
	exit(0);
	}


static void
locale_sync(void)
	{
	struct lconv	*lc;

	lc = localeconv();
	locale_decimal_point = *lc->decimal_point;
	if (locale_decimal_point != server_decimal_point)
		need_locale_fix = TRUE;
	}

static void
client_server_decimal_point(gchar *line)
	{
	sscanf(line, "%c", &server_decimal_point);
	locale_sync();
	}

static void
client_server_io_timeout(gchar *line)
	{
	sscanf(line, "%d", &_GK.client_server_io_timeout);
	if (_GK.client_server_io_timeout < 2)
		_GK.client_server_io_timeout = 0;
	}

static void
client_server_reconnect_timeout(gchar *line)
	{
	sscanf(line, "%d", &_GK.client_server_reconnect_timeout);
	if (_GK.client_server_reconnect_timeout < 2)
		_GK.client_server_reconnect_timeout = 0;
	}


/* ================================================================= */
KeyTable	setup_table[] =
	{
	{"<version>",		client_server_version_setup },
	{"<sensors_setup>",	client_sensors_setup },
	{"<hostname>",		client_hostname_setup },
	{"<sysname>",		client_sysname_setup },
	{"<cpu_setup>",		client_cpu_setup },
	{"<disk_setup>",	client_disk_setup },
	{"<inet_setup>",	client_inet_setup },
	{"<net_setup>",		client_net_setup },
	{"<fs_setup>",		client_fs_setup },
	{"<mail_setup>",	client_mail_setup },
	{"<apm_setup>",		client_battery_setup },
	{"<battery_setup>",	client_battery_setup },
	{"<time>",			client_time_line_from_server},
	{"<monitors>",		client_monitor_setup },
	{"<decimal_point>",	client_server_decimal_point },
	{"<error>",			client_server_error },
	{"<io_timeout>",	client_server_io_timeout},
	{"<reconnect_timeout>", client_server_reconnect_timeout},

	{"<plugin_setup>",	client_plugin_add_setup_line},
	};


typedef struct
	{
	GkrellmMonitor	*mon;
	gchar			*key_name;
	void			(*func_cb)(gchar *line);
	}
	ClientPlugin;

static GList	*client_plugin_serve_data_list;
static GList	*plugin_initial_update_list;

static GkrellmFunc
client_plugin_func(gchar *line)
	{
	GList			*list;
	ClientPlugin	*plug;
	void			(*func)() = NULL;
	gchar			*s;
	gint			n;

	for (list = client_plugin_serve_data_list; list; list = list->next)
		{
		plug = (ClientPlugin *) list->data;
		n = strlen(plug->key_name);
		s = line + n;
		if (*s == '>' && !strncmp(plug->key_name, line, n))
			{
			func = plug->func_cb;
			break;
			}
		}
	return func;
	}

static void
client_plugin_initial_update(ClientPlugin *plug)
	{
	GList	*list;
	void	(*func)(gchar *);
	gchar	*line, *serve_name;

	func = NULL;
	serve_name = g_strdup_printf("<%s>", plug->key_name);
	for (list = plugin_initial_update_list; list; list = list->next)
		{
		line = (gchar *) list->data;
		if (*line == '<')
			{
			func = NULL;
			if (!strcmp(line, serve_name))
				func = plug->func_cb;
			}
		else if (func)
			(*func)(line);
		}	
	g_free(serve_name);
	}

void
gkrellm_client_plugin_serve_data_connect(GkrellmMonitor *mon,
			gchar *key_name, void (*func_cb)(gchar *line))
	{
	ClientPlugin	*plug;

	if (!mon || !key_name || !func_cb)
		return;
	plug = g_new0(ClientPlugin, 1);
	plug->mon = mon;
	plug->key_name = g_strdup(key_name);
	plug->func_cb = func_cb;
	client_plugin_serve_data_list
				= g_list_append(client_plugin_serve_data_list, plug);

	client_plugin_initial_update(plug);
	}

static gboolean
client_send_to_server(gchar *buf)
	{
	gint	n;

	if (!server_alive || client_fd < 0)
		return FALSE;
#if defined(MSG_NOSIGNAL)
	n = send(client_fd, buf, strlen(buf), MSG_NOSIGNAL);
#else
	n = send(client_fd, buf, strlen(buf), 0);
#endif
	if (n < 0 && errno == EPIPE)
		{
		if (_GK.debug_level & DEBUG_CLIENT)
			printf("Write on closed pipe to gkrellmd server.\n");
		server_alive = FALSE;
		return FALSE;
		}
	return TRUE;
	}

gboolean
gkrellm_client_send_to_server(gchar *key_name, gchar *line)
	{
	gchar		*str;
	gboolean	result;

	if (!key_name || !line || !*line)
		return FALSE;
	str = g_strdup_printf("<%s>\n", key_name);
	client_send_to_server(str);
	g_free(str);
	if (line[strlen(line) - 1] != '\n')
		{
		str = g_strdup_printf("%s\n", line);
		result = client_send_to_server(str);
		g_free(str);
		}
	else
		result = client_send_to_server(line);

	return result;
	}

KeyTable	update_table[] =
	{
	{"<cpu>",			client_cpu_line_from_server},
	{"<proc>",			client_proc_line_from_server},
	{"<disk>",			client_disk_line_from_server},
	{"<net>",			client_net_line_from_server},
	{"<net_routed>",	client_net_routed_line_from_server},
	{"<net_timer>",		client_net_timer_line_from_server},
	{"<mem>",			client_mem_line_from_server},
	{"<swap>",			client_swap_line_from_server},
	{"<fs>",			client_fs_line_from_server},
	{"<fs_fstab>",		client_fstab_line_from_server},
	{"<fs_mounts>",		client_mounts_line_from_server},
	{"<inet>",			client_inet_line_from_server},
	{"<mail>",			client_mail_line_from_server},
	{"<apm>",			client_battery_line_from_server},
	{"<battery>",		client_battery_line_from_server},
	{"<sensors>",		client_sensors_line_from_server},
	{"<time>",			client_time_line_from_server},
	{"<uptime>",		client_uptime_line_from_server},

//	{"<>",			client__line_from_server},
	};



static gint
getline(gint fd, gchar *buf, gint len)
	{
	fd_set			read_fds;
	struct timeval	tv;
	gchar			*s;
	gint			result, n, nread = 0;

	FD_ZERO(&read_fds);
	FD_SET(fd, &read_fds);
	tv.tv_usec = 0;
	tv.tv_sec = 15;
	s = buf;
	*s = '\0';
	for (n = 0; n < len - 1; ++n)
		{
		nread = 0;
		result = select(fd + 1, &read_fds, NULL, NULL, &tv);
		if (result <= 0 || (nread = recv(fd, s, 1, 0)) != 1)
			break;
		if (*s == '\n')
			{
			*s = '\0';
			break;
			}
		*++s = '\0';
		}
	if (nread < 0 && errno != EINTR)
		{
		fprintf(stderr, "Broken server connection\n");
		exit(0);
		}
	if (_GK.debug_level & DEBUG_CLIENT)
		printf("%s\n", buf);
	return n;
	}

static void
process_server_line(KeyTable *table, gint table_size, gchar *line)
	{
	static void		(*func)(gchar *);
	gint			i;

	if (!*line || *line == '#')
		return;

	if (*line == '<')
		{
		func = NULL;
		if (line[1] == '.')
			{
			server_time.tm_sec = atoi(line + 2);
			return;
			}
		for (i = 0; i < table_size; ++i)
			{
			if (!strcmp(table[i].key, line))
				{
				func = table[i].func;
				break;
				}
			}
		if (!func)
			func = client_plugin_func(line + 1);
		}
	else if (func)
		(*func)(line);
	if (!func && !setup_done)
		plugin_initial_update_list
				= g_list_append(plugin_initial_update_list, g_strdup(line));
	}


  /* Read setup info from gkrellmd server.  Stuff needed before the
  |  client_init calls must be read here.
  */
static void
read_server_setup(gint fd)
	{
	gchar			buf[256];
	gint			table_size;

	/* Pre 2.1.6 gkrellmd does not send <decimal_point>, so put a fallback
	|  locale_sync() here.
	*/
	locale_sync();
	_GK.client_server_read_time = time(0);
	table_size = sizeof(setup_table) / sizeof(KeyTable);

	gkrellm_free_glist_and_data(&client_plugin_setup_line_list);

	while (1)
		{
		getline(fd, buf, sizeof(buf));
		if (!strcmp(buf, "</gkrellmd_setup>"))
			break;
		process_server_line(&setup_table[0], table_size, buf);
		}

	/* Reset any data that is not cumulative.  gkrellmd sends .clear tags
	|  for fstab and mounts, but inet does not.  So fix it here.
	*/
	client_inet_reset();

	/* Read the initial update data
	*/
	table_size = sizeof(update_table) / sizeof(KeyTable);
	while (1)
		{
		getline(fd, buf, sizeof(buf));
		if (!strcmp(buf, "</initial_update>"))
			break;
		process_server_line(&update_table[0], table_size, buf);
		}
	setup_done = TRUE;
	}

void
gkrellm_client_mode_disconnect(void)
	{
	if (client_fd >= 0)
		close(client_fd);
	client_fd = -1;
	server_alive = FALSE;
	gdk_input_remove(client_input_id);
	client_input_id = 0;
	}

static void
read_server_input(gpointer data, gint fd, GdkInputCondition condition)
	{
	gchar	*line, *eol;
	gint	count, n, table_size;

	n = sizeof(server_buf) - buf_index - 1;
	count = recv(fd, server_buf + buf_index, n, 0);
	if (count <= 0)
		{
		gkrellm_client_mode_disconnect();
		return;
		}
	if (_GK.time_now > 0)
		_GK.client_server_read_time = _GK.time_now;
	server_buf[buf_index + count] = '\0';
	line = server_buf;
	table_size = sizeof(update_table) / sizeof(KeyTable);
	while (*line && (eol = strchr(line, '\n')) != NULL)
		{
		*eol = '\0';
		if (_GK.debug_level & DEBUG_CLIENT)
			printf("%s\n", line);
		process_server_line(&update_table[0], table_size, line);
		line = eol + 1;
		}
	if (line != server_buf && *line)
		{
		buf_index = strlen(line);
		memmove(server_buf, line, buf_index);
		}
	else if (!*line)
		buf_index = 0;
	else
		{
		buf_index += count;
		if (buf_index >= sizeof(server_buf) - 2)
			buf_index = 0;
		}

	server_buf[buf_index] = '\0';
	}


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
#endif

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
		if (_GK.debug_level & DEBUG_CLIENT)
			printf("\t[gkrellm_connect_to: (%d,%d,%d) %s:%d]\n",
			       res->ai_family, res->ai_socktype,
			       res->ai_protocol, server, server_port);
		if (connect(fd, res->ai_addr, res->ai_addrlen) >= 0)
			break;
#ifdef WIN32
		closesocket(fd);
#else
		close(fd);
#endif
		fd = -1;
		}
	freeaddrinfo(res0);
#else
	if (_GK.debug_level & DEBUG_CLIENT)
		printf("\t[gkrellm_connect_to: %s:%d]\n", server, server_port);
	addr = gethostbyname(server);
	if (addr)
		{
		fd = socket(AF_INET, SOCK_STREAM, 0);
		if (fd >= 0)
			{
			memset(&s, 0, sizeof(s));
			memcpy(&s.sin_addr.s_addr, addr->h_addr, addr->h_length);
			s.sin_family = AF_INET;
			s.sin_port = htons(server_port);
			if (connect(fd, (struct sockaddr *)&s, sizeof (s)) < 0)
				{
#ifdef WIN32
				closesocket(fd);
#else
				close(fd);
#endif
				fd = -1;
				}
			}
		}
#endif
	if (fd < 0)
		return -1;

	return fd;
	}

gboolean
gkrellm_client_mode_connect(void)
	{
	gchar	buf[128];

	if (_GK.server_port == 0)
		_GK.server_port = GKRELLMD_SERVER_PORT;

	client_fd = gkrellm_connect_to(_GK.server, _GK.server_port);
	if (client_fd < 0) {
		printf("%s\n", _("Unable to connect."));
		return FALSE;
	}

	snprintf(buf, sizeof(buf), "gkrellm %d.%d.%d%s\n",
			GKRELLM_VERSION_MAJOR, GKRELLM_VERSION_MINOR,
			GKRELLM_VERSION_REV, GKRELLM_EXTRAVERSION);
	send(client_fd, buf, strlen(buf), 0);

	/* Initial setup lines from server are read in blocking mode.
	*/
	read_server_setup(client_fd);

	/* Extra stuff not handled in read_server_setup()
	*/
	gkrellm_mail_local_unsupported();

	/* Now switch to non blocking and set up a read handler.
	*/
#ifndef WIN32
	fcntl(client_fd, F_SETFL, O_NONBLOCK);
#endif
	client_input_id = gdk_input_add(client_fd, GDK_INPUT_READ,
					(GdkInputFunction) read_server_input, NULL);

	server_alive = TRUE;

	return TRUE;
	}


static gboolean	client_mode_thread_busy;

gint
gkrellm_client_server_connect_state(void)
	{
	if (client_mode_thread_busy)			/* reconnect in progress? */
		return 2;
	if (_GK.client_mode && client_input_id > 0)	/* Currently connected? */
		return 1;
	else if (_GK.client_mode)
		return 0;
	else
		return -1;
	}

static gpointer
client_mode_connect_thread(void *data)
	{
	gkrellm_client_mode_connect();
	client_mode_thread_busy = FALSE;
	return NULL;
	}

void
gkrellm_client_mode_connect_thread(void)
	{
	if (client_mode_thread_busy || !_GK.client_mode)
		return;
	client_mode_thread_busy = TRUE;
	g_thread_create(client_mode_connect_thread, NULL, FALSE, NULL);
	}


gboolean
gkrellm_client_mode(void)
	{
	return _GK.client_mode;
	}
