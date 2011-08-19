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

#include "gkrellmd.h"
#include "gkrellmd-private.h"
#include <inttypes.h>

GList			*gkrellmd_monitor_list;

static GList	*serveflag_done_list;

static struct tm	gkrellmd_current_tm;

gint
gkrellm_get_timer_ticks(void)
	{
	return GK.timer_ticks;
	}

gboolean
gkrellmd_check_client_version(GkrellmdMonitor *mon,
				gint major, gint minor, gint rev)
	{
	GkrellmdClient	*client = mon->privat->client;

	if (   client->major_version > major
		|| (client->major_version == major && client->minor_version > minor)
	    || (   client->major_version == major && client->minor_version == minor
			&& client->rev_version >= rev
		   )
	   )
		return TRUE;
	return FALSE;
	}

void
gkrellmd_add_serveflag_done(gboolean *flag)
	{
	serveflag_done_list = g_list_append(serveflag_done_list, flag);
	}


void
gkrellmd_set_serve_name(GkrellmdMonitor *mon, const gchar *tag)
	{
	GkrellmdMonitorPrivate	*mp = mon->privat;

	mp->serve_name = tag;
	mp->serve_name_sent = FALSE;
	}

void
gkrellmd_serve_data(GkrellmdMonitor *mon, gchar *line)
	{
	GkrellmdMonitorPrivate	*mp = mon->privat;
	gchar					buf[128];

	if (!line || !*line)
		return;
	if (!mp->serve_name_sent)
		{
		if (mp->serve_name)
			{
			snprintf(buf, sizeof(buf), "<%s>\n", mp->serve_name);
			gkrellm_debug(DEBUG_SERVER, "%s", buf);
			mp->serve_gstring = g_string_append(mp->serve_gstring, buf);
			mp->serve_name_sent = TRUE;
			}
		else
			{
			g_warning("gkrellmd: %s forgot to gkrellmd_set_serve_name()\n",
					mon->name);
			return;
			}
		}
	gkrellm_debug(DEBUG_SERVER,"%s", line);
	mp->serve_gstring = g_string_append(mp->serve_gstring, line);
	}

/* ======================================================= */
typedef struct
	{
	gint	instance;
	gulong	user,
			nice,
			sys,
			idle;
	}
	CpuData;

static gchar	*n_cpus_setup;
static gboolean	nice_time_unsupported;

static GList	*cpu_list;
static GList	*instance_list;

void
gkrellm_cpu_set_number_of_cpus(gint n)
	{
	CpuData	*cpu;
	GList	*list;
	gint	i;

	n_cpus_setup = g_strdup_printf("n_cpus %d\n", n);
	for (i = 0; i < n; ++i)
		{
		cpu = g_new0(CpuData, 1);
		cpu_list = g_list_append(cpu_list, cpu);

		if (instance_list && (list = g_list_nth(instance_list, i)) != NULL)
			cpu->instance = GPOINTER_TO_INT(list->data);
		else
			cpu->instance = i;
		}
	}

void
gkrellm_cpu_add_instance(gint instance)
	{
	instance_list = g_list_append(instance_list, GINT_TO_POINTER(instance));
	}

void
gkrellm_cpu_nice_time_unsupported(void)
	{
	nice_time_unsupported = TRUE;
	}

void
gkrellm_cpu_assign_composite_data(gulong user, gulong nice,
			gulong sys, gulong idle)
	{
	return;		/* let client gkrellm compute it */
	}

void
gkrellm_cpu_assign_data(gint n, gulong user, gulong nice,
			gulong sys, gulong idle)
	{
	CpuData	*cpu = NULL;
	GList	*list;

	for (list = cpu_list; list; list = list->next)
		{
		cpu = (CpuData *) list->data;
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
update_cpu(GkrellmdMonitor *mon, gboolean first_update)
	{
	gkrellm_sys_cpu_read_data();
	gkrellmd_need_serve(mon);
	}

static void
serve_cpu_data(GkrellmdMonitor *mon, gboolean first_serve)
	{
	CpuData	*cpu;
	GList	*list;
	gchar	buf[128];

	gkrellmd_set_serve_name(mon, "cpu");
	for (list = cpu_list; list; list = list->next)
		{
		cpu = (CpuData *) list->data;
		snprintf(buf, sizeof(buf), "%d %lu %lu %lu %lu\n", cpu->instance,
				cpu->user, cpu->nice, cpu->sys, cpu->idle);
		gkrellmd_serve_data(mon, buf);
		}
	}

static void
serve_cpu_setup(GkrellmdMonitor *mon)
	{
	GkrellmdClient	*client = mon->privat->client;
	GList			*list;
	gchar			buf[64];

	gkrellmd_send_to_client(client, "<cpu_setup>\n");
	for (list = instance_list; list; list = list->next)
		{
		snprintf(buf, sizeof(buf), "cpu_instance %d\n",
				GPOINTER_TO_INT(list->data));
		gkrellmd_send_to_client(client, buf);
		}
	gkrellmd_send_to_client(client, n_cpus_setup);
	if (nice_time_unsupported)
		gkrellmd_send_to_client(client, "nice_time_unsupported\n");
	}

static GkrellmdMonitor cpu_monitor =
	{
	"cpu",
	update_cpu,
	serve_cpu_data,
	serve_cpu_setup
	};


static GkrellmdMonitor *
init_cpu_monitor(void)
	{
	if (gkrellm_sys_cpu_init())
		return &cpu_monitor;
	return NULL;
	}

/* ======================================================= */
struct
	{
	gboolean	changed;
	gint		n_processes,
				n_running,
				n_users;
	gulong		n_forks;
	gfloat		fload;
	}
	proc;

void
gkrellm_proc_assign_data(gint n_processes, gint n_running,
			gulong n_forks, gfloat load)
	{
	if (   proc.n_processes != n_processes
		|| proc.n_running != n_running
		|| proc.n_forks != n_forks
		|| proc.fload != load
	   )
		{
		proc.n_processes = n_processes;
		proc.n_running = n_running;
		proc.n_forks = n_forks;
		proc.fload = load;
		proc.changed = TRUE;
		}
	}

void
gkrellm_proc_assign_users(gint n_users)
	{
	if (proc.n_users != n_users)
		{
		proc.n_users = n_users;
		proc.changed = TRUE;
		}
	}

static void
update_proc(GkrellmdMonitor *mon, gboolean first_update)
	{
	proc.changed = FALSE;
	gkrellm_sys_proc_read_data();
	if (first_update || GK.five_second_tick)
		gkrellm_sys_proc_read_users();

	if (proc.changed)
		gkrellmd_need_serve(mon);
	}

static void
serve_proc_data(GkrellmdMonitor *mon, gboolean first_serve)
	{
	gchar	buf[128];

	gkrellmd_set_serve_name(mon, "proc");
	snprintf(buf, sizeof(buf), "%d %d %lu %.2f %d\n",
			 proc.n_processes, proc.n_running,
			 proc.n_forks, proc.fload, proc.n_users);
	gkrellmd_serve_data(mon, buf);
	}

static GkrellmdMonitor proc_monitor =
	{
	"proc",
	update_proc,
	serve_proc_data,
	NULL
	};


static GkrellmdMonitor *
init_proc_monitor(void)
	{
	if (!gkrellm_sys_proc_init())
		return NULL;
	serveflag_done_list = g_list_append(serveflag_done_list, &proc.changed);
	return &proc_monitor;
	}

/* ======================================================= */
typedef struct
	{
	gchar		*name;
	gchar		*subdisk_parent;
	gint		order,
				subdisk,
				changed;
	gint		device_number,
				unit_number;
	gboolean	virtual;
	guint64		rb,
				wb;
	}
	DiskData;

static GList	*disk_list;
static gint		n_disks;
static gboolean	units_are_blocks;


static DiskData *
add_disk(const gchar *name, gint order, gint device_number, gint unit_number)
	{
	DiskData	*disk;
	GList		*list;
	gint		i;

	disk = g_new0(DiskData, 1);
	disk->name = g_strdup(name);
	disk->order = order;
	disk->subdisk = -1;
	disk->device_number = device_number;
	disk->unit_number = unit_number;
	if (order >= 0)
		{
		for (i = 0, list = disk_list; list; list = list->next, ++i)
			if (disk->order < ((DiskData *) list->data)->order)
				break;
		disk_list = g_list_insert(disk_list, disk, i);
		}
	else
		disk_list = g_list_append(disk_list, disk);
	++n_disks;
	return disk;
	}

static DiskData *
add_subdisk(gchar *subdisk_name, gchar *disk_name, gint subdisk)
	{
	DiskData	*sdisk = NULL;
#if GLIB_CHECK_VERSION(2,0,0)
	DiskData	*disk;
	GList		*list = NULL;

	for (list = disk_list; list; list = list->next)
		{
		disk = (DiskData * ) list->data;
		if (!strcmp(disk_name, disk->name))
			break;
		}
	if (!list)
		return NULL;
	sdisk = g_new0(DiskData, 1);
	sdisk->name = g_strdup(subdisk_name);
	sdisk->subdisk_parent = g_strdup(disk_name);
	sdisk->order = disk->order;
	sdisk->subdisk = subdisk;

	for (list = list->next; list; list = list->next)
		{
		disk = (DiskData * ) list->data;
		if (disk->subdisk == -1 || disk->subdisk > subdisk)
			break;
		}
	disk_list = g_list_insert_before(disk_list, list, sdisk);
	++n_disks;
#endif
	return sdisk;
	}

static void
disk_assign_data(DiskData *disk, guint64 rb, guint64 wb, gboolean virtual)
	{
	if (disk)
		{
		if (disk->rb != rb || disk->wb != wb)
			disk->changed = TRUE;
		else
			disk->changed = FALSE;
		disk->rb = rb;
		disk->wb = wb;
		disk->virtual = virtual;
		}
	}

void
gkrellm_disk_reset_composite(void)
	{
	/* Don't handle this. */
	}

void
gkrellm_disk_units_are_blocks(void)
	{
	units_are_blocks = TRUE;
	}

void
gkrellm_disk_add_by_name(const gchar *name, const gchar *label)
	{
	gint	order = -1;

    if (NULL == name) // Cannot add disk without a name
		return;
    order = gkrellm_sys_disk_order_from_name(name);
    /* FIXME: gkrellmd currently has no support for disk labels. Extend
       network-protocol and server to support disks with both name and label. */
    add_disk(name, order, 0, 0);
	}

void
gkrellm_disk_assign_data_by_device(gint device_number, gint unit_number,
			guint64 rb, guint64 wb, gboolean virtual)
	{
	GList		*list;
	DiskData	*disk = NULL;
	gchar		*name;
	gint		order = -1;

	for (list = disk_list; list; list = list->next)
		{
		disk = (DiskData * ) list->data;
		if (   disk->device_number == device_number
			&& disk->unit_number == unit_number
		   )
			break;
		disk = NULL;
		}
	if (!disk)
		{
		name = gkrellm_sys_disk_name_from_device(device_number,
					unit_number, &order);
		if (name)
			disk = add_disk(name, order, device_number, unit_number);
		}
	disk_assign_data(disk, rb, wb, virtual);
	}

void
gkrellm_disk_assign_data_nth(gint n, guint64 rb, guint64 wb, gboolean virtual)
	{
	DiskData	*disk;
	gchar		name[32];

	if (n < n_disks)
		disk = (DiskData *) g_list_nth_data(disk_list, n);
	else
		{
		snprintf(name, sizeof(name), "%s%c", _("Disk"), 'A' + n);
		disk = add_disk(name, n, 0, 0);
		}
	disk_assign_data(disk, rb, wb, virtual);
	}

void
gkrellm_disk_assign_data_by_name(gchar *name,
			guint64 rb, guint64 wb, gboolean virtual)
	{
	GList		*list;
	DiskData	*disk = NULL;
	gint		order = -1;

	for (list = disk_list; list; list = list->next)
		{
		disk = (DiskData * ) list->data;
		if (!strcmp(name, disk->name))
			break;
		disk = NULL;
		}
	if (!disk)
		{
		order = gkrellm_sys_disk_order_from_name(name);
		disk = add_disk(name, order, 0, 0);
		}
	disk_assign_data(disk, rb, wb, virtual);
	}

void
gkrellm_disk_subdisk_assign_data_by_name(gchar *subdisk_name, gchar *disk_name,
				guint64 rb, guint64 wb)
	{
	GList		*list;
	DiskData	*disk = NULL;
	gchar		*s, *endptr;
	gint		subdisk;

	if (!subdisk_name || !disk_name)
		return;
	for (list = disk_list; list; list = list->next)
		{
		disk = (DiskData * ) list->data;
		if (!strcmp(subdisk_name, disk->name))
			break;
		disk = NULL;
		}
	if (!disk)
		{
		/* A subdisk name is expected to be the disk_name with a number string
		|  appended.  Eg. "hda1" is a subdisk_name of disk_name "hda"
		*/
		s = subdisk_name + strlen(disk_name);
		subdisk = strtol(s, &endptr, 0);
		if (!*s || *endptr)
			return;
		disk = add_subdisk(subdisk_name, disk_name, subdisk);
		}
	disk_assign_data(disk, rb, wb, FALSE);
	}

static void
update_disk(GkrellmdMonitor *mon, gboolean first_update)
	{
	GList		*list;
	DiskData	*disk = NULL;

	gkrellm_sys_disk_read_data();
	for (list = disk_list; list; list = list->next)
		{
		disk = (DiskData * ) list->data;
		if (disk->changed)
			{
			gkrellmd_need_serve(mon);
			break;
			}
		}
	}


static void
serve_disk_data(GkrellmdMonitor *mon, gboolean first_serve)
	{
	DiskData	*disk;
	GList		*list;
	gchar		*buf = NULL;

	gkrellmd_set_serve_name(mon, "disk");
	for (list = disk_list; list; list = list->next)
		{
		disk = (DiskData *) list->data;
		if (!disk->changed && !first_serve)
			continue;
		if (!disk->subdisk_parent)
			{
			if (gkrellmd_check_client_version(mon, 2, 2, 7) && disk->virtual)
                buf = g_strdup_printf("%s virtual %" PRIu64 " %" PRIu64 "\n",
							disk->name, disk->rb, disk->wb);
			else
				buf = g_strdup_printf("%s %" PRIu64 " %" PRIu64 "\n",
							disk->name, disk->rb, disk->wb);
			}
		else if (mon->privat->client->feature_subdisk)
			buf = g_strdup_printf("%s %s %" PRIu64 " %" PRIu64 "\n",
						disk->name, disk->subdisk_parent, disk->rb, disk->wb);
		else
			continue;
		gkrellmd_serve_data(mon, buf);
        g_free(buf);
        buf = NULL;
		}
	}

static void
serve_disk_setup(GkrellmdMonitor *mon)
	{
	GkrellmdClient	*client = mon->privat->client;

	if (units_are_blocks)
		gkrellmd_send_to_client(client, "<disk_setup>\nunits_are_blocks\n");
	if (gkrellmd_check_client_version(mon, 2,1,3))
		client->feature_subdisk = TRUE;
	}

static GkrellmdMonitor disk_monitor =
	{
	"disk",
	update_disk,
	serve_disk_data,
	serve_disk_setup
	};


static GkrellmdMonitor *
init_disk_monitor(void)
	{
	if (gkrellm_sys_disk_init())
		return &disk_monitor;
	return NULL;
	}

/* ======================================================= */
#include "../src/inet.h"

typedef struct
	{
	ActiveTCP	tcp;
	gboolean	alive,
				new_connection;
	}
	InetData;

static GList	*inet_list,
				*inet_dead_list;

static gboolean	inet_unsupported,
				inet_new;

void
gkrellm_inet_log_tcp_port_data(gpointer data)
	{
	GList		*list;
	InetData	*in;
	ActiveTCP	*tcp, *active_tcp = NULL;
	gchar		*ap, *aap;
	gint		slen;

	tcp = (ActiveTCP *) data;
	for (list = inet_list; list; list = list->next)
		{
		in = (InetData *) list->data;
		active_tcp = &in->tcp;
		if (tcp->family == AF_INET)
			{
			ap = (char *)&tcp->remote_addr;
			aap = (char *)&active_tcp->remote_addr;
			slen = sizeof(struct in_addr);
			}
#if defined(INET6)
		else if (tcp->family == AF_INET6)
			{
			ap = (char *)&tcp->remote_addr6;
			aap = (char *)&active_tcp->remote_addr6;
			slen = sizeof(struct in6_addr);
			}
#endif
		else
			return;
		if (   memcmp(aap, ap, slen) == 0
			&& active_tcp->remote_port == tcp->remote_port
			&& active_tcp->local_port == tcp->local_port
		   )
			{
			in->alive = TRUE;	/* Old alive connection still alive */
			return;
			}
		}
	inet_new = TRUE;
	in = g_new0(InetData, 1);
	in->tcp = *tcp;
	in->alive = TRUE;
	in->new_connection = TRUE;
	inet_list = g_list_append(inet_list, in);
	}

static void
update_inet(GkrellmdMonitor *mon, gboolean first_update)
	{
	GList		*list;
	InetData	*in;
	static gint	check_tcp;

	
	if (!first_update && !GK.second_tick)
		return;

	if (first_update || check_tcp == 0)
		{
		gkrellm_free_glist_and_data(&inet_dead_list);
		inet_new = FALSE;
		for (list = inet_list; list; list = list->next)
			{
			in = (InetData *) list->data;
			in->alive = FALSE;
			in->new_connection = FALSE;
			}

		gkrellm_sys_inet_read_tcp_data();

		for (list = inet_list; list; )
			{
			in = (InetData *) list->data;
			if (!in->alive)
				{
				if (list == inet_list)
					inet_list = inet_list->next;
				list = g_list_remove(list, in);
				inet_dead_list = g_list_append(inet_dead_list, in);
				}
			else
				list = list->next;
			}
		if (inet_new || inet_dead_list)
			gkrellmd_need_serve(mon);
		}
	check_tcp = (check_tcp + 1) % _GK.inet_interval;
	}

static void
serve_inet_data(GkrellmdMonitor *mon, gboolean first_serve)
	{
	InetData	*in;
	ActiveTCP	*tcp;
	GList		*list;
	gchar		buf[128], *cp;
#if defined(INET6) && defined(HAVE_GETADDRINFO)
	struct sockaddr_in6	sin6;
	char		addrbuf[NI_MAXHOST];
#endif

	if (inet_new || first_serve)
		{
		gkrellmd_set_serve_name(mon, "inet");
		for (list = inet_list; list; list = list->next)
			{
			in = (InetData *) list->data;
			tcp = &in->tcp;
			if (   tcp->family == AF_INET
				&& (in->new_connection || first_serve)
			   )
				{
				cp = inet_ntoa(tcp->remote_addr);
				snprintf(buf, sizeof(buf), "+0 %x %s:%x\n",
							tcp->local_port, cp, tcp->remote_port);
				}
#if defined(INET6) && defined(HAVE_GETADDRINFO)
			else if (tcp->family == AF_INET6
				 && (in->new_connection || first_serve))
				{
				memset(&sin6, 0, sizeof(sin6));
				memcpy(&sin6.sin6_addr, &tcp->remote_addr6,
				       sizeof(struct in6_addr));
				sin6.sin6_family = AF_INET6;
#ifdef SIN6_LEN
				sin6.sin6_len = sizeof(struct sockaddr_in6);
#endif
				if (getnameinfo((struct sockaddr *)&sin6,
						sizeof(struct sockaddr_in6),
						addrbuf, sizeof(addrbuf),
						NULL, 0,
						NI_NUMERICHOST|NI_WITHSCOPEID)
				    != 0)
					continue;
				snprintf(buf, sizeof(buf), "+6 %x [%s]:%x\n",
					 tcp->local_port, addrbuf,
					 tcp->remote_port);
				}
#endif
			else
				continue;

			gkrellmd_serve_data(mon, buf);
			}
		}
	if (!first_serve)
		{
		gkrellmd_set_serve_name(mon, "inet");
		for (list = inet_dead_list; list; list = list->next)
			{
			in = (InetData *) list->data;
			tcp = &in->tcp;
			if (tcp->family == AF_INET)
				{
				cp = inet_ntoa(tcp->remote_addr);
				snprintf(buf, sizeof(buf), "-0 %x %s:%x\n",
							tcp->local_port, cp, tcp->remote_port);
				}
#if defined(INET6) && defined(HAVE_GETADDRINFO)
			else if (tcp->family == AF_INET6)
				{
				memset(&sin6, 0, sizeof(sin6));
				memcpy(&sin6.sin6_addr, &tcp->remote_addr6,
				       sizeof(struct in6_addr));
				sin6.sin6_family = AF_INET6;
#ifdef SIN6_LEN
				sin6.sin6_len = sizeof(struct sockaddr_in6);
#endif
				if (getnameinfo((struct sockaddr *)&sin6,
						sizeof(struct sockaddr_in6),
						addrbuf, sizeof(addrbuf),
						NULL, 0,
						NI_NUMERICHOST|NI_WITHSCOPEID)
				    != 0)
					continue;
				snprintf(buf, sizeof(buf), "-6 %x [%s]:%x\n",
					 tcp->local_port, addrbuf,
					 tcp->remote_port);
				}
#endif
			else
				continue;

			gkrellmd_serve_data(mon, buf);
			}
		}
	}

static void
serve_inet_setup(GkrellmdMonitor *mon)
	{
	GkrellmdClient	*client = mon->privat->client;

	if (inet_unsupported)
		gkrellmd_send_to_client(client, "<inet_setup>\ninet_unsupported\n");
	}

static GkrellmdMonitor inet_monitor =
	{
	"inet",
	update_inet,
	serve_inet_data,
	serve_inet_setup
	};


static GkrellmdMonitor *
init_inet_monitor(void)
	{
	if (_GK.inet_interval > 0 && gkrellm_sys_inet_init())
		return &inet_monitor;
	inet_unsupported = TRUE;
	return NULL;
	}

/* ======================================================= */

#define TIMER_TYPE_NONE	0
#define TIMER_TYPE_PPP	1
#define TIMER_TYPE_IPPP	2

typedef struct
	{
	gchar		*name;
	gboolean	changed,
				up,
				up_prev,
				up_event,
				down_event;

	gboolean	timed_changed;
	time_t		up_time;

	gulong		rx,
				tx;
	}
	NetData;

static NetData	*net_timer;

static GList	*net_list,
				*net_sys_list;

static time_t	net_timer0;
static gint		net_timer_type;

static gboolean	net_use_routed;


gchar *
gkrellm_net_mon_first(void)
	{
	gchar	*name = NULL;

	net_sys_list = net_list;
	if (net_sys_list)
		{
		name = ((NetData *) (net_sys_list->data))->name;
		net_sys_list = net_sys_list->next;
		}
	return name;
	}

gchar *
gkrellm_net_mon_next(void)
	{
	gchar	*name = NULL;

	if (net_sys_list)
		{
		name = ((NetData *) (net_sys_list->data))->name;
		net_sys_list = net_sys_list->next;
		}
	return name;
	}

void
gkrellm_net_use_routed(gboolean real_routed /* not applicable in server */)
	{
	net_use_routed = TRUE;
	}

static NetData *
net_new(gchar *name)
	{
	NetData	*net;

	net = g_new0(NetData, 1);
	net->name = g_strdup(name);
	net_list = g_list_append(net_list, net);

	if (net_timer_type != TIMER_TYPE_NONE && !strcmp(_GK.net_timer, net->name))
		net_timer = net;

	return net;
	}

void
gkrellm_net_assign_data(gchar *name, gulong rx, gulong tx)
	{
	GList	*list;
	NetData	*net;

	for (list = net_list; list; list = list->next)
		{
		net = (NetData *) list->data;
		if (!strcmp(net->name, name))
			{
			if (net->rx != rx || net->tx != tx)
				net->changed = TRUE;
			else
				net->changed = FALSE;
			break;
			}
		}
	if (!list)
		net = net_new(name);

	if (GK.second_tick && !net_use_routed)
		net->up = TRUE;
	net->rx = rx;
	net->tx = tx;
	}

void
gkrellm_net_routed_event(gchar *name, gboolean routed)
	{
	GList	*list;
	NetData	*net;

	for (list = net_list; list; list = list->next)
		{
		net = (NetData *) list->data;
		if (!strcmp(net->name, name))
			break;
		}
	if (!list)
		net = net_new(name);

	if (routed)
		net->up_event = TRUE;
	else
		net->down_event = TRUE;
	net->up = routed;
	}

void
gkrellm_net_add_timer_type_ppp(gchar *name)
	{
	if (!_GK.net_timer || !name)
		return;
	if (name && !strncmp(_GK.net_timer, name, strlen(name) - 1))
		net_timer_type = TIMER_TYPE_PPP;
	}

void
gkrellm_net_add_timer_type_ippp(gchar *name)
	{
	if (!_GK.net_timer || !name)
		return;
	if (name && !strncmp(_GK.net_timer, name, strlen(name) - 1))
		net_timer_type = TIMER_TYPE_IPPP;
	}

void
gkrellm_net_set_lock_directory(gchar *dir)
	{
	/* Not supported remotely */
	}

static void
update_net(GkrellmdMonitor *mon, gboolean first_update)
	{
	GList		*list;
	NetData		*net;
	gint		up_time = 0;

	if (GK.second_tick)
		{
		if (!net_use_routed)
			{
			for (list = net_list; list; list = list->next)
				{
				net = (NetData *) list->data;
				net->up_prev = net->up;
				net->up = FALSE;
				}
			}
		else
			gkrellm_sys_net_check_routes();
		}
	gkrellm_sys_net_read_data();
	
	if (GK.second_tick && !net_use_routed)
		{
		for (list = net_list; list; list = list->next)
			{
			net = (NetData *) list->data;
			if (net->up && !net->up_prev)
				net->up_event = TRUE;
			else if (!net->up && net->up_prev)
				net->down_event = TRUE;
			}
		}

	if (net_timer && GK.second_tick)
		{
		if (net_timer_type == TIMER_TYPE_PPP)
			{
			struct stat		st;
			gchar			buf[256];

			if (net_timer->up_event)
				{
				snprintf(buf, sizeof(buf), "/var/run/%s.pid", net_timer->name);
				if (g_stat(buf, &st) == 0)
					net_timer0 = st.st_mtime;
				else
					time(&net_timer0);
				}
			if (net_timer->up)
				up_time = (int) (time(0) - net_timer0);
			}
		else if (net_timer_type == TIMER_TYPE_IPPP)
			{
			/* get all isdn status from its connect state because the
			|  net_timer->up can be UP even with isdn line not connected.
			|  Can't get time history if gkrellmd started after connects.
			*/
			static gboolean		old_connected;
			gboolean			connected;

			connected = gkrellm_sys_net_isdn_online();
			if (connected && !old_connected)
				time(&net_timer0);			/* New session just started */
			old_connected = connected;

			up_time = (int) (time(0) - net_timer0);
			}
		if (up_time != net_timer->up_time)
			net_timer->timed_changed = TRUE;
		net_timer->up_time = up_time;
		}

	gkrellmd_need_serve(mon);	/* serve func checks for changed */
	}

static void
serve_net_data(GkrellmdMonitor *mon, gboolean first_serve)
	{
	NetData		*net;
	GList		*list;
	gchar		buf[128];
	gboolean	fake_up_event;

	gkrellmd_set_serve_name(mon, "net");
	for (list = net_list; list; list = list->next)
		{
		net = (NetData *) list->data;
		if (net->changed || first_serve)
			{
			snprintf(buf, sizeof(buf), "%s %lu %lu\n", net->name, net->rx, net->tx);
			gkrellmd_serve_data(mon, buf);
			}
		}

	/* Since the server transmits changes only, use the routed interface
	|  to the client regardless if the sysdep code uses routed.
	*/
	if (GK.second_tick || first_serve)
		{
		gkrellmd_set_serve_name(mon, "net_routed");
		for (list = net_list; list; list = list->next)
			{
			net = (NetData *) list->data;
			fake_up_event = (first_serve && net->up);
			if (net->up_event || net->down_event || fake_up_event)
				{
				snprintf(buf, sizeof(buf), "%s %d\n", net->name,
						fake_up_event ? TRUE : net->up_event);
				gkrellmd_serve_data(mon, buf);
				}
			if (mon->privat->client->last_client)
				net->up_event = net->down_event = FALSE;
			}
		}

	if (net_timer && GK.second_tick)
		{
		if (net_timer->timed_changed || first_serve)
			{
			gkrellmd_set_serve_name(mon, "net_timer");
			snprintf(buf, sizeof(buf), "%s %d\n", net_timer->name, (gint)net_timer->up_time);
			gkrellmd_serve_data(mon, buf);
			}
		}
	}

static void
serve_net_setup(GkrellmdMonitor *mon)
	{
	GkrellmdClient	*client = mon->privat->client;
	gchar			buf[128];

	/* The client <-> server link always uses routed mode, but the client
	|  needs to know if server sysdep uses routed for config purposes.
	*/
	if (net_use_routed)
		gkrellmd_send_to_client(client, "<net_setup>\nnet_use_routed\n");

	if (net_timer_type != TIMER_TYPE_NONE)
		{
		snprintf(buf, sizeof(buf),
				"<net_setup>\nnet_timer %s\n", _GK.net_timer);
		gkrellmd_send_to_client(client, buf);
		}
	}

static GkrellmdMonitor net_monitor =
	{
	"net",
	update_net,
	serve_net_data,
	serve_net_setup
	};


static GkrellmdMonitor *
init_net_monitor(void)
	{
	net_timer_type = TIMER_TYPE_NONE;
	if (gkrellm_sys_net_init())
		return &net_monitor;
	return NULL;
	}


/* ======================================================= */
struct
	{
	gboolean	mem_changed;
	guint64		total,
				used,
				free,
				shared,
				buffers,
				cached;

	gboolean	swap_changed;
	guint64		swap_total,
				swap_used;
	gulong		swap_in,
				swap_out;
	}
	mem;

void
gkrellm_mem_assign_data(guint64 total, guint64 used, guint64 free,
			guint64 shared, guint64 buffers, guint64 cached)
	{
	if (   mem.total != total
		|| mem.used != used
		|| mem.free != free
		|| mem.shared != shared
		|| mem.buffers != buffers
		|| mem.cached != cached
	   )
		{
		mem.total = total;
		mem.used = used;
		mem.free = free;
		mem.shared = shared;
		mem.buffers = buffers;
		mem.cached = cached;
		mem.mem_changed = TRUE;
		}
	}

void
gkrellm_swap_assign_data(guint64 total, guint64 used,
			gulong swap_in, gulong swap_out)
	{
	if (   mem.swap_total != total
		|| mem.swap_used != used
		|| mem.swap_in != swap_in
		|| mem.swap_out != swap_out
	   )
		{
		mem.swap_total = total;
		mem.swap_used = used;
		mem.swap_in = swap_in;
		mem.swap_out = swap_out;
		mem.swap_changed = TRUE;
		}
	}

static void
update_mem(GkrellmdMonitor *mon, gboolean first_update)
	{
	mem.mem_changed = mem.swap_changed = FALSE;

	gkrellm_sys_swap_read_data();
	if (first_update || GK.five_second_tick)
		gkrellm_sys_mem_read_data();

	if (mem.mem_changed || mem.swap_changed)
		gkrellmd_need_serve(mon);
	}

static void
serve_mem_data(GkrellmdMonitor *mon, gboolean first_serve)
	{
	gchar	buf[128];

	if (mem.mem_changed || first_serve)
		{
		gkrellmd_set_serve_name(mon, "mem");
		snprintf(buf, sizeof(buf), "%" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 "\n",
				mem.total, mem.used, mem.free,
				mem.shared, mem.buffers, mem.cached);
		gkrellmd_serve_data(mon, buf);
		}

	if (mem.swap_changed || first_serve)
		{
		gkrellmd_set_serve_name(mon, "swap");
		snprintf(buf, sizeof(buf), "%" PRIu64 " %" PRIu64 " %lu %lu\n",
				mem.swap_total, mem.swap_used,
				mem.swap_in, mem.swap_out);
		gkrellmd_serve_data(mon, buf);
		}
	}

static GkrellmdMonitor mem_monitor =
	{
	"mem",
	update_mem,
	serve_mem_data,
	NULL
	};


static GkrellmdMonitor *
init_mem_monitor(void)
	{
	if (!gkrellm_sys_mem_init())
		return NULL;
	serveflag_done_list = g_list_append(serveflag_done_list, &mem.mem_changed);
	serveflag_done_list = g_list_append(serveflag_done_list,&mem.swap_changed);
	return &mem_monitor;
	}

/* ======================================================= */
typedef struct
	{
	gboolean	busy,
				deleted,
				is_mounted,
				is_nfs,
				changed;
	gchar		*directory,
				*device,
				*type,
				*options;
	glong		blocks,
				bavail,
				bfree,
				bsize;
	}
	Mount;

static GList	*mounts_list,
				*fstab_list;

static gboolean	nfs_check,
				fs_check,
				fs_need_serve,
				fstab_list_modified,
				mounts_list_modified,
				mounting_unsupported;

static gchar *remote_fs_types[] =
	{
	"nfs",
	"smbfs"
	};

void
gkrellm_fs_setup_eject(gchar *eject_tray, gchar *close_tray,
			void (*eject_func)(), void (*close_func)())
	{
	/* Not supported remotely */
	}

void
gkrellm_fs_add_to_mounts_list(gchar *dir, gchar *dev, gchar *type)
	{
	GList	*list;
	Mount	*m;
	gint	i;

	for (list = mounts_list; list; list = list->next)
		{
		m = (Mount *) list->data;
		if (   !strcmp(m->directory, dir)
			&& !strcmp(m->device, dev)
			&& !strcmp(m->type, type)
		   )
			break;
		}
	if (!list)
		{
		m = g_new0(Mount, 1);
		m->directory = g_strdup(dir);
		m->device = g_strdup(dev);
		m->type = g_strdup(type);
		mounts_list = g_list_append(mounts_list, m);
		mounts_list_modified = TRUE;
		serveflag_done_list = g_list_append(serveflag_done_list, &m->changed);

		for (i = 0; i < (sizeof(remote_fs_types) / sizeof(gchar *)); ++i)
			{
			if (!strcmp(m->type, remote_fs_types[i]))
				{
				m->is_nfs = TRUE;
				break;
				}
			}
		}
	m->is_mounted = TRUE;
	}

void
gkrellm_fs_add_to_fstab_list(gchar *dir, gchar *dev, gchar *type, gchar *opt)
	{
	Mount	*m;

	m = g_new0(Mount, 1);
	m->directory = g_strdup(dir);
	m->device = g_strdup(dev);
	m->type = g_strdup(type);
	fstab_list = g_list_append(fstab_list, m);
	}

void
gkrellm_fs_assign_fsusage_data(gpointer pointer,
			glong blocks, glong bavail, glong bfree, glong bsize)
	{
	Mount	*m = (Mount *) pointer;

	if (   m->blocks != blocks
		|| m->bavail != bavail
		|| m->bfree  != bfree
		|| m->bsize  != bsize
	   )
		{
		m->blocks = blocks;
		m->bavail = bavail;
		m->bfree  = bfree;
		m->bsize  = bsize;

		m->changed = TRUE;
		}
	}

void
gkrellm_fs_mounting_unsupported(void)
	{
	mounting_unsupported = TRUE;
	}

static void
refresh_mounts_list(void)
	{
	GList	*list;
	Mount	*m;

	for (list = mounts_list; list; list = list->next)
		((Mount *) list->data)->is_mounted = FALSE;

	gkrellm_sys_fs_get_mounts_list();

	for (list = mounts_list; list; )
		{
		m = (Mount *) list->data;
		if (!m->is_mounted)
			{
			if (list == mounts_list)
				mounts_list = mounts_list->next;
			list = g_list_remove_link(list, list);
			g_free(m->directory);
			g_free(m->device);
			g_free(m->type);
			serveflag_done_list = g_list_remove(serveflag_done_list,
						&m->changed);
			if (m->busy)
				m->deleted = TRUE;
			else
				g_free(m);
			mounts_list_modified = TRUE;
			}
		else
			list = list->next;
		}
	}

static void
refresh_fstab_list(void)
	{
	Mount	*m;

	while (fstab_list)
		{
		m = (Mount *) fstab_list->data;
		g_free(m->directory);
		g_free(m->device);
		g_free(m->type);
		g_free(m);
		fstab_list = g_list_remove(fstab_list, fstab_list->data);
		}
	gkrellm_sys_fs_get_fstab_list();
	fstab_list_modified = TRUE;
	}

#if GLIB_CHECK_VERSION(2,0,0)
static gpointer
get_fsusage_thread(void *data)
	{
	Mount	*m = (Mount *) data;

	gkrellm_sys_fs_get_fsusage(m, m->directory);

	if (m->deleted)
		g_free(m);
	else
		{
		if (m->changed)
			fs_need_serve = TRUE;
		m->busy = FALSE;
		}
	return NULL;
	}
#endif

static void
update_fs(GkrellmdMonitor *mon, gboolean first_update)
	{
	GList		*list;
	Mount		*m;
	static gint	check_tick;

	if (fs_need_serve)		/* Asynchronous change in fsusage thread? */
		gkrellmd_need_serve(mon);
	fs_need_serve = FALSE;

	if (GK.second_tick)
		++check_tick;
	fs_check = !(check_tick % _GK.fs_interval);

	if (_GK.nfs_interval > 0)
		nfs_check = !(check_tick % _GK.nfs_interval);
	else
		nfs_check = 0;

	if (!first_update && (!GK.second_tick || (!fs_check && !nfs_check)))
		return;
	refresh_mounts_list();
	for (list = mounts_list; list; list = list->next)
		{
		m = (Mount *) list->data;
		if (fs_check && !m->is_nfs)
			gkrellm_sys_fs_get_fsusage(m, m->directory);
		else if (nfs_check && m->is_nfs && !m->busy)
			{
#if GLIB_CHECK_VERSION(2,0,0)
			m->busy = TRUE;
			g_thread_create(get_fsusage_thread, m, FALSE, NULL);
#else
			gkrellm_sys_fs_get_fsusage(m, m->directory);
#endif
			}
		}
	if (first_update || gkrellm_sys_fs_fstab_modified())
		refresh_fstab_list();

	gkrellmd_need_serve(mon);
	}

static void
serve_fs_data(GkrellmdMonitor *mon, gboolean first_serve)
	{
	Mount	*m;
	GList	*list;
	gchar	buf[128];

	if (mounts_list_modified || first_serve)
		{
		gkrellmd_set_serve_name(mon, "fs_mounts");
		gkrellmd_serve_data(mon, ".clear\n");
		for (list = mounts_list; list; list = list->next)
			{
			m = (Mount *) list->data;
			snprintf(buf, sizeof(buf), "%s %s %s %ld %ld %ld %ld\n",
					m->directory, m->device, m->type,
					m->blocks, m->bavail, m->bfree, m->bsize);
			/*gkrellm_debug(DEBUG_SERVER,
				"Adding mount-line for %s to serve-data\n", m->directory);*/
			gkrellmd_serve_data(mon, buf);
			}
		}
	else
		{
		gkrellmd_set_serve_name(mon, "fs");
		for (list = mounts_list; list; list = list->next)
			{
			m = (Mount *) list->data;
			if (!m->changed)
				continue;
			snprintf(buf, sizeof(buf), "%s %s %ld %ld %ld %ld\n",
					m->directory, m->device,
					m->blocks, m->bavail, m->bfree, m->bsize);
			/*gkrellm_debug(DEBUG_SERVER,
				"Updating fs %s in serve-data\n", m->directory);*/
			gkrellmd_serve_data(mon, buf);
			}
		}
	if (fstab_list_modified || first_serve)
		{
		gkrellmd_set_serve_name(mon, "fs_fstab");
		gkrellmd_serve_data(mon, ".clear\n");
		for (list = fstab_list; list; list = list->next)
			{
			m = (Mount *) list->data;
			snprintf(buf, sizeof(buf), "%s %s %s\n",
					m->directory, m->device, m->type);
			/*gkrellm_debug(DEBUG_SERVER,
				"Adding fstab-line for %s to serve-data\n", m->directory);*/
			gkrellmd_serve_data(mon, buf);
			}
		}
	}

static void
serve_fs_setup(GkrellmdMonitor *mon)
	{
	GkrellmdClient	*client = mon->privat->client;

	if (mounting_unsupported)
		gkrellmd_send_to_client(client, "<fs_setup>\nmounting_unsupported\n");
	}

static GkrellmdMonitor fs_monitor =
	{
	"fs",
	update_fs,
	serve_fs_data,
	serve_fs_setup
	};


static GkrellmdMonitor *
init_fs_monitor(void)
	{
	if (!gkrellm_sys_fs_init())
		return NULL;
	serveflag_done_list =
				g_list_append(serveflag_done_list, &fstab_list_modified);
	serveflag_done_list =
				g_list_append(serveflag_done_list, &mounts_list_modified);
	return &fs_monitor;
	}

/* ======================================================= */


typedef struct
	{
	gboolean	changed,
				have_data;

	gint		id;
	gboolean	present,
				on_line,
				charging;
	gint		percent;
	gint		time_left;
	}
	Battery;

static GList	*battery_list;

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
			serveflag_done_list = g_list_append(serveflag_done_list,
								&composite_battery->changed);
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
		serveflag_done_list = g_list_append(serveflag_done_list,
								&bat->changed);
		}
	return bat;
	}

void
gkrellm_battery_assign_data(gint id, gboolean present, gboolean on_line,
			gboolean charging, gint percent, gint time_left)
	{
	Battery	*bat;

	bat = battery_nth(id);
	if (!bat)
		return;

	if (   present   != bat->present
		|| on_line   != bat->on_line
		|| charging  != bat->charging
		|| percent   != bat->percent
		|| time_left != bat->time_left
	   )
		{
		bat->present = present;
		bat->on_line = on_line;
		bat->charging = charging;
		bat->percent = percent;
		bat->time_left = time_left;
		bat->changed = TRUE;
		}

	bat->have_data = TRUE;
	}

gint
gkrellm_battery_full_cap_fallback()
	{
	return 5000;	/* XXX Linux ACPI bug not handled by server */
	}

static void
update_battery(GkrellmdMonitor *mon, gboolean first_update)
	{
	GList	*list;
	Battery	*bat;

	if (!first_update && !GK.five_second_tick)
		return;

	for (list = battery_list; list; list = list->next)
		{
		bat = (Battery *) list->data;
		bat->have_data = FALSE;
		bat->changed = FALSE;
		}
	gkrellm_sys_battery_read_data();

	for (list = battery_list; list; list = list->next)
		{
		bat = (Battery *) list->data;
		if (!bat->have_data && bat->present)
			{
			bat->present = FALSE;
			bat->changed = TRUE;
			}
		if (bat->changed)
			gkrellmd_need_serve(mon);
		}
	}

static void
serve_battery_data(GkrellmdMonitor *mon, gboolean first_serve)
	{
	Battery		*bat;
	GList		*list;
	gchar		buf[128];

	gkrellmd_set_serve_name(mon, "battery");
	for (list = battery_list; list; list = list->next)
		{
		bat = (Battery *) list->data;

		if (   (!bat->changed && !first_serve)
			|| (   !gkrellmd_check_client_version(mon, 2,1,9)
				&& bat->id > 0
			   )
		   )
			continue;
		snprintf(buf, sizeof(buf), "%d %d %d %d %d %d\n",
					bat->present, bat->on_line, bat->charging,
					bat->percent, bat->time_left, bat->id);
		gkrellmd_serve_data(mon, buf);
		}
	}

static void
serve_battery_setup(GkrellmdMonitor *mon)
	{
	GkrellmdClient	*client = mon->privat->client;

	gkrellm_sys_battery_read_data();
	if (battery_list)
		gkrellmd_send_to_client(client,
					"<battery_setup>\nbattery_available\n");
	}

static GkrellmdMonitor battery_monitor =
	{
	"battery",
	update_battery,
	serve_battery_data,
	serve_battery_setup
	};

static GkrellmdMonitor *
init_battery_monitor(void)
	{
	if (!gkrellm_sys_battery_init())
		return NULL;
	return &battery_monitor;
	}

/* ======================================================= */

typedef struct
	{
	gboolean	changed;
	gint		type;

	gchar		*path;			/* Pathname to sensor data or device file */

	gchar		*id_name;		/* These 4 are unique sensor identification */
	gint		id;				/*   of a particular sensor type */
	gint		iodev;			/* One or any combination may be used. */
	gint		inter;

	gchar		*vref;
	gchar		*default_label;
	gint		group;

	gfloat		factor;
	gfloat		offset;
	gfloat		raw_value;
	}
	Sensor;

static GList	*sensors_list;

static gboolean thread_busy,
				sensors_need_serve;

static gpointer
read_sensors(void *data)
	{
	GList		*list;
	Sensor		*sensor;
	gfloat		tmp;
	gboolean	need_serve = FALSE;

	for (list = sensors_list; list; list = list->next)
		{
		sensor = (Sensor *) list->data;
		tmp = sensor->raw_value;
		if (sensor->type == SENSOR_TEMPERATURE)
			gkrellm_sys_sensors_get_temperature(sensor->path, sensor->id,
				sensor->iodev, sensor->inter, &sensor->raw_value);
		else if (sensor->type == SENSOR_FAN)
			gkrellm_sys_sensors_get_fan(sensor->path, sensor->id,
				sensor->iodev, sensor->inter, &sensor->raw_value);
		else if (sensor->type == SENSOR_VOLTAGE)
			gkrellm_sys_sensors_get_voltage(sensor->path, sensor->id,
				sensor->iodev, sensor->inter, &sensor->raw_value);
		if (sensor->raw_value != tmp)
			{
			sensor->changed = TRUE;
			need_serve = TRUE;
			}
		else
			sensor->changed = FALSE;
		}
	thread_busy = FALSE;
	sensors_need_serve = need_serve; /* Thread, so set after data collected */

	return NULL;
	}

static void
run_sensors_thread(void)
	{
#if GLIB_CHECK_VERSION(2,0,0)
	if (thread_busy)
		return;
	thread_busy = TRUE;
	g_thread_create(read_sensors, NULL, FALSE, NULL);
#else
	read_sensors(NULL);
#endif
	}


void
gkrellm_sensors_config_migrate_connect(gboolean (*func)(), gint sysdep_version)
	{
	}

void
gkrellm_sensors_update_volt_order_base(void)
	{
	}

void
gkrellm_sensors_set_group(gpointer sr, gint group)
	{
	Sensor	*sensor = (Sensor *) sr;

	if (sensor)
		sensor->group = group;
	}

void
gkrellm_sensors_sysdep_option(gchar *keyword, gchar *label, void (*func)())
	{
	}

  /* A sensor within a type is uniquely identified by its id_name.
  |  A sysdep interface may additionally use any of the triple integer
  |  set (id, iodev, inter) for internal identification.
  |  Monitor code here uses path to read the sensor values, but id_name is only
  |  passed to the client since that is all that is needed for identification
  |  (the client is no longer interfacing to sysdep code).
  */
gpointer
gkrellm_sensors_add_sensor(gint type, gchar *sensor_path, gchar *id_name,
		gint id, gint iodev, gint inter,
		gfloat factor, gfloat offset, gchar *vref, gchar *default_label)
	{
	Sensor	*sensor;

	if (!id_name || !*id_name || type < 0 || type > 2)
		return NULL;

	sensor = g_new0(Sensor, 1);
	sensor->id_name = g_strdup(id_name);

	if (sensor_path)
		sensor->path = g_strdup(sensor_path);
	else
		sensor->path = g_strdup(id_name);

	sensor->vref = g_strdup(vref ? vref : "NONE");
	sensor->default_label = g_strdup(default_label ? default_label : "NONE");

	sensor->factor = factor;
	sensor->offset = offset;
	sensor->type = type;
	sensor->id = id;
	sensor->iodev = iodev;
	sensor->inter = inter;
	sensors_list = g_list_append(sensors_list, sensor);
	return sensor;
	}

static void
update_sensors(GkrellmdMonitor *mon, gboolean first_update)
	{
	if (sensors_need_serve)		/* Asynchronously set in thread */
		gkrellmd_need_serve(mon);
	sensors_need_serve = FALSE;

	if (!GK.five_second_tick && !first_update)
		return;
	if (first_update)
		read_sensors(NULL);		/* No thread on first read */
	else
		run_sensors_thread();
	}

static void
serve_sensors_data(GkrellmdMonitor *mon, gboolean first_serve)
	{
	Sensor			*sr;
	GList			*list;
	gchar			buf[128];
	gboolean		sensor_disk_ok;

	gkrellmd_set_serve_name(mon, "sensors");
	sensor_disk_ok = gkrellmd_check_client_version(mon, 2,2,0);
	for (list = sensors_list; list; list = list->next)
		{
		sr = (Sensor *) list->data;
		if (sr->group == SENSOR_GROUP_DISK && !sensor_disk_ok)
			continue;
		if (sr->changed || first_serve)
			{
			snprintf(buf, sizeof(buf), "%d \"%s\" %d %d %d %.2f\n",
					sr->type, sr->id_name,
					sr->id, sr->iodev, sr->inter, sr->raw_value);
			gkrellmd_serve_data(mon, buf);
			}
		}
	}

static void
serve_sensors_setup(GkrellmdMonitor *mon)
	{
	GkrellmdClient	*client = mon->privat->client;
	GList			*list;
	Sensor			*s;
	gchar			buf[256];
	gboolean		sensor_disk_ok;

	gkrellmd_send_to_client(client, "<sensors_setup>\n");
	sensor_disk_ok = gkrellmd_check_client_version(mon, 2,2,0);
	for (list = sensors_list; list; list = list->next)
		{
		s = (Sensor *) list->data;
		if (s->group == SENSOR_GROUP_DISK && !sensor_disk_ok)
			continue;
		if (sensor_disk_ok)
			snprintf(buf, sizeof(buf), "%d \"%s\" %d %d %d %.4f %.4f \"%s\" \"%s\" %d\n",
					s->type, s->id_name,
					s->id, s->iodev, s->inter,
					s->factor, s->offset, s->vref, s->default_label, s->group);
		else
			snprintf(buf, sizeof(buf), "%d \"%s\" %d %d %d %.4f %.4f \"%s\" \"%s\"\n",
					s->type, s->id_name,
					s->id, s->iodev, s->inter,
					s->factor, s->offset, s->vref, s->default_label);
		gkrellmd_send_to_client(client, buf);
		}
	}

static GkrellmdMonitor sensors_monitor =
	{
	"sensors",
	update_sensors,
	serve_sensors_data,
	serve_sensors_setup
	};

static GkrellmdMonitor *
init_sensors_monitor(void)
	{
	if (!gkrellm_sys_sensors_init())
		return NULL;
	return &sensors_monitor;
	}

/* ======================================================= */
static time_t	base_uptime,
				up_seconds;
static gulong	up_minutes = -1;

void
gkrellm_uptime_set_base_uptime(time_t base)
	{
	base_uptime = base;
	}

static void
update_uptime(GkrellmdMonitor *mon, gboolean first_update)
	{
	gint	prev_up;

	if (GK.ten_second_tick || up_minutes < 0 || first_update)
		{
		prev_up = up_minutes;
		up_seconds = gkrellm_sys_uptime_read_uptime();
		if (up_seconds > 0)
			up_minutes = (gint) (up_seconds / 60);
		else
			up_minutes = (gint)(time(0) - _GK.start_time + base_uptime) / 60;
		if (up_minutes != prev_up)
			gkrellmd_need_serve(mon);
		}
	}

static void
serve_uptime_data(GkrellmdMonitor *mon, gboolean first_serve)
	{
	gchar	buf[128];

	gkrellmd_set_serve_name(mon, "uptime");
	snprintf(buf, sizeof(buf), "%lu\n", (gulong) up_minutes);
	gkrellmd_serve_data(mon, buf);
	}

static GkrellmdMonitor uptime_monitor =
	{
	"uptime",
	update_uptime,
	serve_uptime_data,
	NULL
	};

static GkrellmdMonitor *
init_uptime_monitor(void)
	{
	if (!gkrellm_sys_uptime_init())
		return NULL;
	return &uptime_monitor;
	}

/* ======================================================= */
static void
send_time(GkrellmdClient *client)
	{
	struct tm	*t;
	gchar		buf[128];

	t = &gkrellmd_current_tm;
	snprintf(buf, sizeof(buf), "<time>\n%d %d %d %d %d %d %d %d %d\n",
		t->tm_sec, t->tm_min, t->tm_hour,
		t->tm_mday, t->tm_mon, t->tm_year,
		t->tm_wday, t->tm_yday, t->tm_isdst);
	gkrellmd_send_to_client(client, buf);
	}

/* ======================================================= */

void
gkrellmd_plugin_serve_setup(GkrellmdMonitor *mon, gchar *name, gchar *line)
	{
	GkrellmdClient	*client = mon->privat->client;
	gchar			buf[256];

	if (!mon || !name || !line)
		return;
	gkrellmd_send_to_client(client, "<plugin_setup>\n");
	snprintf(buf, sizeof(buf), "%s %s\n", name, line);
	gkrellmd_send_to_client(client, buf);
	}

static void
add_monitor(GkrellmdMonitor *mon)
	{
	if (!mon)
		return;
	mon->privat = g_new0(GkrellmdMonitorPrivate, 1);
	mon->privat->serve_gstring = g_string_new("");
	gkrellmd_monitor_list = g_list_append(gkrellmd_monitor_list, mon);
	}

void
gkrellmd_load_monitors(void)
	{
	GList			*list;
	GkrellmdMonitor	*mon;

	add_monitor(init_sensors_monitor());
	add_monitor(init_cpu_monitor());
	add_monitor(init_proc_monitor());
	add_monitor(init_disk_monitor());
	add_monitor(init_net_monitor());
	add_monitor(init_inet_monitor());
	add_monitor(init_mem_monitor());
	add_monitor(init_fs_monitor());
	add_monitor(gkrellmd_init_mail_monitor());
	add_monitor(init_battery_monitor());
	add_monitor(init_uptime_monitor());

	list = gkrellmd_plugins_load();
	if (_GK.list_plugins)
		exit(0);
	if (_GK.log_plugins)
		g_message("%s\n", plugin_install_log ? plugin_install_log :
				_("No plugins found\n"));
	for (  ; list; list = list->next)
		{
		mon = (GkrellmdMonitor *) list->data;
		mon->privat->serve_gstring = g_string_new("");
		mon->privat->is_plugin = TRUE;
		gkrellmd_monitor_list = g_list_append(gkrellmd_monitor_list, mon);
		}
	}

void
gkrellmd_need_serve(GkrellmdMonitor *mon)
	{
	if (mon)
		mon->privat->need_serve = TRUE;
	}

gboolean
gkrellmd_update_monitors(gpointer unused)
	{
	GList			*list, *c_list;
	GkrellmdMonitor			*mon;
	GkrellmdMonitorPrivate	*mp;
	GkrellmdClient			*client;
	struct tm				*pCur;
	static time_t			time_prev;
	gchar					buf[64];
	static gboolean			first_update = TRUE;
	static GString			*serve_gstring;

	time(&_GK.time_now);
	GK.second_tick = (_GK.time_now == time_prev) ? FALSE : TRUE;
	time_prev = _GK.time_now;

	if (GK.second_tick)
		{
		pCur = localtime(&_GK.time_now);
		GK.two_second_tick  = ((pCur->tm_sec % 2) == 0) ? TRUE : FALSE;
		GK.five_second_tick = ((pCur->tm_sec % 5) == 0) ? TRUE : FALSE;
		GK.ten_second_tick  = ((pCur->tm_sec % 10) == 0) ? TRUE : FALSE;
		GK.minute_tick = (pCur->tm_min  != gkrellmd_current_tm.tm_min);
		gkrellmd_current_tm = *pCur;
		}
	else
		{
		GK.two_second_tick = FALSE;
		GK.five_second_tick = FALSE;
		GK.ten_second_tick = FALSE;
		GK.minute_tick = FALSE;
		}

	for (list = gkrellmd_monitor_list; list; list = list->next)
		{
		mon = (GkrellmdMonitor *) list->data;
		if (mon->update_monitor)
			(*(mon->update_monitor))(mon, first_update);
		}
	++GK.timer_ticks;
	if (!serve_gstring)
		serve_gstring = g_string_new("");
	for (c_list = gkrellmd_client_list; c_list; c_list = c_list->next)
		{
		client = (GkrellmdClient *) c_list->data;
		client->last_client = !c_list->next;
		if (!client->served)
			gkrellmd_send_to_client(client, "<initial_update>\n");
		for (list = gkrellmd_monitor_list; list; list = list->next)
			{
			mon = (GkrellmdMonitor *) list->data;
			mp = mon->privat;
			if (!mon->serve_data || (!mp->need_serve && client->served))
				continue;
			mp->client = client;
			mp->serve_name_sent = FALSE;
			(*(mon->serve_data))(mon, client->served ? FALSE : TRUE);
			if (mp->serve_gstring->len > 0)
				{
				serve_gstring =
					g_string_append(serve_gstring, mp->serve_gstring->str);
				mp->serve_gstring = g_string_truncate(mp->serve_gstring, 0);
				}
			}
		gkrellmd_send_to_client(client, serve_gstring->str);
		serve_gstring = g_string_truncate(serve_gstring, 0);
		
		if (GK.minute_tick || !client->served)
			send_time(client);
		else if (GK.second_tick)
			{
			snprintf(buf, sizeof(buf), "<.%d>\n", gkrellmd_current_tm.tm_sec);
			gkrellmd_send_to_client(client, buf);
			}

		if (!client->served)
			gkrellmd_send_to_client(client, "</initial_update>\n");
		client->served = TRUE;
		}

	for (list = gkrellmd_monitor_list; list; list = list->next)
		{
		mon = (GkrellmdMonitor *) list->data;
		mp = mon->privat;
		mp->need_serve = FALSE;
		}

	for (list = serveflag_done_list; list; list = list->next)
		*((gboolean *) list->data) = FALSE;

	first_update = FALSE;
	return TRUE;
	}

void
gkrellmd_serve_setup(GkrellmdClient *client)
	{
	GList			*list;
	GkrellmdMonitor	*mon;
	struct lconv	*lc;
	gchar			buf[32], *s, *name;

	gkrellmd_send_to_client(client, "<gkrellmd_setup>\n");

	s = g_strdup_printf("<version>\ngkrellmd %d.%d.%d%s\n",
				GKRELLMD_VERSION_MAJOR, GKRELLMD_VERSION_MINOR,
				GKRELLMD_VERSION_REV, GKRELLMD_EXTRAVERSION);
	gkrellmd_send_to_client(client, s);
	g_free(s);

	lc = localeconv();
	snprintf(buf, sizeof(buf), "%c\n", *lc->decimal_point);
	s = g_strconcat("<decimal_point>\n", buf, "\n", NULL);
	gkrellmd_send_to_client(client, s);
	g_free(s);

	for (list = gkrellmd_monitor_list; list; list = list->next)
		{
		mon = (GkrellmdMonitor *) list->data;
		mon->privat->client = client;
		if (mon->serve_setup)
			(*(mon->serve_setup))(mon);
		}
	name = gkrellm_sys_get_host_name();
	s = g_strconcat("<hostname>\n", name, "\n", NULL);
	gkrellmd_send_to_client(client, s);
	g_free(s);

	name = gkrellm_sys_get_system_name();
	s = g_strconcat("<sysname>\n", name, "\n", NULL);
	gkrellmd_send_to_client(client, s);
	g_free(s);

	send_time(client);

	gkrellmd_send_to_client(client, "<monitors>\n");
	for (list = gkrellmd_monitor_list; list; list = list->next)
		{
		mon = (GkrellmdMonitor *) list->data;
		snprintf(buf, sizeof(buf), "%s\n", mon->name);
		gkrellmd_send_to_client(client, buf);
		}

	snprintf(buf, sizeof(buf), "%d\n", _GK.io_timeout);
	s = g_strconcat("<io_timeout>\n", buf, "\n", NULL);
	gkrellmd_send_to_client(client, s);
	g_free(s);

	snprintf(buf, sizeof(buf), "%d\n", _GK.reconnect_timeout);
	s = g_strconcat("<reconnect_timeout>\n", buf, "\n", NULL);
	gkrellmd_send_to_client(client, s);
	g_free(s);

	gkrellmd_send_to_client(client, "</gkrellmd_setup>\n");
	}

void
gkrellmd_client_input_connect(GkrellmdMonitor *mon,
			void (*func)(GkrellmdClient *, gchar *))
	{
	mon->privat->client_input_func = func;
	}

void
gkrellmd_client_read(gint fd, gint nbytes)
	{
	GList					*list;
	GkrellmdClient			*client = NULL;
	GkrellmdMonitor			*mon;
	GkrellmdMonitorPrivate	*mp;
	gchar					buf[513], *s, *e;
	gint					n, buflen;

	for (list = gkrellmd_client_list; list; list = list->next)
		{
		client = (GkrellmdClient *) list->data;
		if (client->fd != fd)
			continue;

		if (!client->input_gstring)
			client->input_gstring = g_string_new("");

		buflen = sizeof(buf) - 1;
		while (nbytes > 0)
			{
			n = (nbytes > buflen) ? buflen : nbytes;
			n = recv(fd, buf, n, 0);
			if (n <= 0)
				break;
			nbytes -= n;
			buf[n] = '\0';
			client->input_gstring =
						g_string_append(client->input_gstring, buf);
			}
		break;
		}
	if (!list)
		return;

	while (gkrellmd_getline_from_gstring(&client->input_gstring,
				buf, sizeof(buf) - 1))
		{
		if (*buf == '<')
			{
			client->input_func = NULL;
			s = buf + 1;
			for (list = gkrellmd_monitor_list; list; list = list->next)
				{
				mon = (GkrellmdMonitor *) list->data;
				mp = mon->privat;
				if (!mp->serve_name)
					continue;
				n = strlen(mp->serve_name);
				e = s + n;
				if (*e == '>' && !strncmp(mp->serve_name, s, n))
					{
					client->input_func = mp->client_input_func;
					break;
					}
				}
			}
		else if (client->input_func)
			(*client->input_func)(client, buf);
//		printf("%s: %s", client->hostname, buf);
		}
	}


void
gkrellmd_monitor_read_client(GkrellmdClient *client, GString *str, gpointer user_data)
	{
	GList					*list;
	GkrellmdMonitor			*mon;
	GkrellmdMonitorPrivate	*mp;
	gchar					*line, *s, *e;
	gint					n;

	while ((line = gkrellm_gstring_get_line(str)))
		{
		if (*line == '<')
			{
			gkrellm_debug(DEBUG_SERVER,
				"gkrellmd_monitor_read_client: read command '%s'\n", line);

			client->input_func = NULL;
			s = line + 1;
			for (list = gkrellmd_monitor_list; list; list = list->next)
				{
				mon = (GkrellmdMonitor *) list->data;
				mp = mon->privat;
				if (!mp->serve_name)
					continue;
				n = strlen(mp->serve_name);
				e = s + n;
				if (*e == '>' && !strncmp(mp->serve_name, s, n))
					{
					client->input_func = mp->client_input_func;
					break;
					}
				}
			}
		else if (client->input_func)
			{
			gkrellm_debug(DEBUG_SERVER,
				"gkrellmd_monitor_read_client: read data '%s'\n", line);

			(*client->input_func)(client, line);
			}

		g_free(line);
		}
	}
