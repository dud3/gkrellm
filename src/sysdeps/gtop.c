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

#include <glibtop.h>
#include <glibtop/open.h>
#include <glibtop/close.h>
#include <glibtop/xmalloc.h>
#include <glibtop/parameter.h>
#include <glibtop/netload.h>


void
gkrellm_sys_main_init(void)
	{
	glibtop_init();
	}

void
gkrellm_sys_main_cleanup(void)
	{
	}

/* ===================================================================== */
/* CPU monitor interface */

#include <glibtop/cpu.h>

void
gkrellm_sys_cpu_read_data(void)
	{
	glibtop_cpu	glt_cpu;

	glibtop_get_cpu(&glt_cpu);
	gkrellm_cpu_assign_data(0, 
		(gulong) glt_cpu.user, (gulong) glt_cpu.nice,
		(gulong) glt_cpu.sys, (gulong) glt_cpu.idle);
	}


gboolean
gkrellm_sys_cpu_init(void)
	{
	gkrellm_cpu_set_number_of_cpus(1);
	return TRUE;
	}


/* ===================================================================== */
/* Proc monitor interface */

#include <glibtop/loadavg.h>

void
gkrellm_sys_proc_read_data(void)
	{
	glibtop_loadavg		glt_loadavg;

	/* If this is expensive, may want to second_tick it
	*/
	glibtop_get_loadavg (&glt_loadavg);
	gkrellm_proc_assign_data(glt_loadavg.nr_tasks,
				glt_loadavg.nr_running,
				glt_loadavg.last_pid,
				(gfloat) glt_loadavg.loadavg[0]);
	}

void
gkrellm_sys_proc_read_users(void)
	{
	gkrellm_proc_assign_users(0);
	}

gboolean
gkrellm_sys_proc_init(void)
	{
	return TRUE;
	}

/* ===================================================================== */
/* Disk monitor interface */

gchar *
gkrellm_sys_disk_name_from_device(gint major, gint minor,gint *order)
	{
	return NULL;
	}

gint
gkrellm_sys_disk_order_from_name(const gchar *name)
	{
	return -1;
	}

void
gkrellm_sys_disk_read_data(void)
	{
	}

gboolean
gkrellm_sys_disk_init(void)
	{
	return FALSE;		/* Disk monitor not implemented */
	}



/* ===================================================================== */
/* Net monitor interface */

#include <glibtop/netload.h>
#include <glibtop/ppp.h>

  /* Make an example list.  GKrellM expects to be able to automatically
  |  detect any UP net interface.
  */
static gchar	*glt_net_names[] =
	{ "eth0", "ppp0", "eth1", "eth2", "ippp0", "plip0" };

#define	GLT_IF_UP(flags)	(flags & (1 << GLIBTOP_IF_FLAGS_UP))


void
gkrellm_sys_net_check_routes(void)
	{
	}

void
gkrellm_sys_net_read_data(void)
	{
	glibtop_netload	netload;
	gulong			rx, tx;
	gulong			rx_packets, tx_packets;
	gint			i;

	for (i = 0; i < sizeof (glt_net_names) / sizeof (gchar *); ++i)
		{
		glibtop_get_netload (&netload, glt_net_names[i]);
		if (!GLT_IF_UP(netload.if_flags))
			continue;
		rx_packets = (gulong) netload.packets_in;
		tx_packets = (gulong) netload.packets_out;
		rx = (gulong) netload.bytes_in;
		tx = (gulong) netload.bytes_out;
		if (rx == 0 && tx == 0)
			gkrellm_net_assign_data(glt_net_names[i], rx_packets, tx_packets);
		else
			gkrellm_net_assign_data(glt_net_names[i], rx, tx);
		}
	}

gboolean
gkrellm_sys_net_isdn_online(void)
	{
	glibtop_ppp	isdn;

	glibtop_get_ppp(&isdn, 0  /* Reads /dev/isdninfo */);
	if (isdn.state == GLIBTOP_PPP_STATE_ONLINE)
		return TRUE;
	return FALSE;
	}

gboolean
gkrellm_sys_net_init(void)
	{
	gkrellm_net_set_lock_directory("/var/lock");
	gkrellm_net_add_timer_type_ppp("ppp0");
	gkrellm_net_add_timer_type_ippp("ippp0");
	return TRUE;
    }


/* ===================================================================== */
/* Memory/Swap monitor interface */

#include <glibtop/mem.h>
#include <glibtop/swap.h>


void
gkrellm_sys_mem_read_data(void)
	{
	glibtop_mem		glt_mem;
	guint64			total, used, x_used, free, shared, buffers, cached;

	glibtop_get_mem (&glt_mem);
	total   = (guint64) glt_mem.total;
	x_used    = (guint64) glt_mem.used;
	free    = (guint64) glt_mem.free;
	shared  = (guint64) glt_mem.shared;
	buffers = (guint64) glt_mem.buffer;
	cached  = (guint64) glt_mem.cached;

	/* Not sure why, but glibtop has a used memory calculation:
	|  glt_mem.used = mem.total - mem.free - mem.shared
	|			- mem.buffers;
	|  while the free command calculates a "used" the way I have here:
	*/
	used = x_used - buffers - cached;
	gkrellm_mem_assign_data(total, used, free, shared, buffers, cached);
	}

void
gkrellm_sys_swap_read_data(void)
	{
	glibtop_swap	glt_swap;
	guint64			swap_total, swap_used, swap_in, swap_out;

	glibtop_get_swap (&glt_swap);
	swap_total = (guint64) glt_swap.total;
	swap_used  = (guint64) glt_swap.used;

	if (glt_swap.flags
				& ((1 << GLIBTOP_SWAP_PAGEIN) + (1 << GLIBTOP_SWAP_PAGEOUT)))
		{
		swap_in = (gulong) glt_swap.pagein;
		swap_out = (gulong) glt_swap.pageout;
		}
	else
		{
		swap_in = 0;
		swap_out = 0;
		}
	gkrellm_swap_assign_data(swap_total, swap_used, swap_in, swap_out);
	}

gboolean
gkrellm_sys_mem_init(void)
	{
	return TRUE;
	}

/* ===================================================================== */
/* FS monitor interface */

#include <glibtop/mountlist.h>
#include <glibtop/fsusage.h>

static void
fix_fstab_name(gchar *buf)
	{
	gchar	*rp,
			*wp;

	if (buf[0] == '\0')
		return;
	rp = buf;
	wp = buf;
	do	/* This loop same as in libc6 getmntent()	*/
		if (rp[0] == '\\' && rp[1] == '0' && rp[2] == '4' && rp[3] == '0')
			{
			*wp++ = ' ';		/* \040 is a SPACE.  */
			rp += 3;
			}
		else if (rp[0] == '\\' && rp[1] == '0' && rp[2] == '1' && rp[3] == '2')
			{
			*wp++ = '\t';		/* \012 is a TAB.  */
			rp += 3;
			}
		else if (rp[0] == '\\' && rp[1] == '\\')
			{
			*wp++ = '\\';		/* \\ is a \	*/
			rp += 1;
			}
		else
			*wp++ = *rp;
	while (*rp++ != '\0');
	}

gboolean
gkrellm_sys_fs_fstab_modified(void)
	{
	struct stat		s;
	static time_t	fstab_mtime;
	gint			modified = FALSE;

	if (stat("/etc/fstab", &s) == 0 && s.st_mtime != fstab_mtime)
		modified = TRUE;
	fstab_mtime = s.st_mtime;
	return modified;
	}


void
gkrellm_sys_fs_get_fstab_list(void)
	{
	FILE			*f;
	gchar			buf[1024], *s;
	gchar			dev[64], dir[128], type[64], opt[128];

	if ((f = fopen("/etc/fstab", "r")) == NULL)
		return;
	while (fgets(buf, sizeof(buf), f))
		{
		s = buf;
		while (*s == ' ' || *s == '\t')
			++s;
		if (*s == '\0' || *s == '#' || *s == '\n')
			continue;
		dev[0] = dir[0] = type[0] = opt[0] = '\0';
		sscanf(s, "%64s %128s %64s %128s", dev, dir, type, opt);
		fix_fstab_name(dev);
		fix_fstab_name(dir);
		fix_fstab_name(type);
		fix_fstab_name(opt);

		if (   type[0] == '\0'
			|| !strcmp(type, "devpts")
			|| !strcmp(type, "swap")
			|| !strcmp(type, "proc")
			|| !strcmp(type, "usbdevfs")
			|| !strcmp(type, "ignore")
		   )
			continue;
		gkrellm_fs_add_to_fstab_list(dir, dev, type, opt);
		}
	fclose(f);
	}

void
gkrellm_sys_fs_get_mounts_list(void)
	{
	glibtop_mountlist	mount_list;
	glibtop_mountentry	*mount_entries;
	gint				i;

	mount_entries = glibtop_get_mountlist(&mount_list, 1);
	for (i = 0; i < mount_list.number; ++i)
		{
		gkrellm_fs_add_to_mounts_list(mount_entries[i].mountdir,
					mount_entries[i].devname,
					mount_entries[i].type);
		}
	glibtop_free(mount_entries);
	}

void
gkrellm_sys_fs_get_fsusage(gpointer fs, gchar *dir)
	{
	glibtop_fsusage	fsusage;

	glibtop_get_fsusage(&fsusage, dir);
	gkrellm_fs_assign_fsusage_data(fs, (glong) fsusage.blocks,
				(glong) fsusage.bavail, (glong) fsusage.bfree, 512);
	}

gboolean
gkrellm_sys_fs_init(void)
	{
	return TRUE;
	}

/* ===================================================================== */
/* Uptime monitor interface */

#include <glibtop/uptime.h>

time_t
gkrellm_sys_uptime_read_uptime(void)
    {
	glibtop_uptime	glt_uptime;
	time_t			uptime = 0;

	glibtop_get_uptime (&glt_uptime);
	if (glt_uptime.flags & (1 << GLIBTOP_UPTIME_UPTIME))
		uptime = (time_t) glt_uptime.uptime;
	return uptime;
    }

gboolean
gkrellm_sys_uptime_init(void)
	{
	return TRUE;
	}

/* ===================================================================== */
/* Sensor monitor interface - not implemented */

gboolean
gkrellm_sys_sensors_get_temperature(gchar *device_name, gint id,
		gint iodev, gint interface, gfloat *temp)
	{
	return FALSE;
	}

gboolean
gkrellm_sys_sensors_get_fan(gchar *device_name, gint id,
		gint iodev, gint interface, gfloat *fan)
	{
	return FALSE;
	}

gboolean
gkrellm_sys_sensors_get_voltage(gchar *device_name, gint id,
		gint iodev, gint interface, gfloat *volt)
	{
	return FALSE;
	}

gboolean
gkrellm_sys_sensors_init(void)
	{
	return FALSE;
	}


/* ===================================================================== */
/* Battery monitor interface */

void
gkrellm_sys_battery_read_data(void)
	{
	}

gboolean
gkrellm_sys_battery_init()
	{
	return FALSE;
	}


/* ===================================================================== */
/* Inet monitor interface */

void
gkrellm_sys_inet_read_tcp_data(void)
	{
	}

gboolean
gkrellm_sys_inet_init(void)
	{
	return FALSE;
	}
