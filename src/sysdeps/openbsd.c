/* GKrellM
|  Copyright (C) 1999-2014 Bill Wilson
|
|  Author:  Bill Wilson    billw@gkrellm.net
|  Latest versions might be found at:  http://gkrellm.net
|
|  OpenBSD code derived from FreeBSD code by: Hajimu UMEMOTO <ume@FreeBSD.org>
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

#include <kvm.h>

kvm_t	*kvmd = NULL;
char	errbuf[_POSIX2_LINE_MAX];


void
gkrellm_sys_main_init(void)
	{
	/* We just ignore error, here.  Even if GKrellM doesn't have
	|  kmem privilege, it runs with available information.
	*/
	kvmd = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, errbuf);
	if (setgid(getegid()) != 0)
		{
		fprintf(stderr, "Can't drop setgid privileges.");
		exit(1);
		}
	}

void
gkrellm_sys_main_cleanup(void)
	{
	}


/* ===================================================================== */
/* CPU monitor interface */

#include <sys/dkstat.h>
#include <kvm.h>

extern	kvm_t	*kvmd;

void
gkrellm_sys_cpu_read_data(void)
	{
	long		cp_time[CPUSTATES];
	static struct nlist nl[] = {
#define N_CP_TIME	0
		{ "_cp_time" },
		{ "" }
	};


	if (kvmd == NULL)
		return;
	if (nl[0].n_type == 0)
		if (kvm_nlist(kvmd, nl) < 0 || nl[0].n_type == 0)
			return;
	if (kvm_read(kvmd, nl[N_CP_TIME].n_value,
		     (char *)&cp_time, sizeof(cp_time)) != sizeof(cp_time))
		return;

	/* Currently, SMP is not supported */
	gkrellm_cpu_assign_data(0, cp_time[CP_USER], cp_time[CP_NICE],
				cp_time[CP_SYS], cp_time[CP_IDLE]);
	}

gboolean
gkrellm_sys_cpu_init(void)
    {
	gkrellm_cpu_set_number_of_cpus(1);
	return TRUE;
	}


/* ===================================================================== */
/* Proc monitor interface */

#include <sys/proc.h>
#include <sys/sysctl.h>
#include <uvm/uvm_extern.h>
#include <kvm.h>

#include <utmp.h>

extern	kvm_t	*kvmd;

void
gkrellm_sys_proc_read_data(void)
{
   static int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL };
   double avenrun;
	guint	n_forks = 0, n_processes = 0;
   struct uvmexp *uvmexp;
   int i;
   size_t len;

	static struct nlist nl[] = {
#define X_UVM_EXP    0
	   { "_uvmexp" },
	   { NULL }
	};


   if (sysctl(mib, 3, NULL, &len, NULL, 0) >= 0) {
      n_processes = len / sizeof(struct kinfo_proc);
   }

   /* get name list if it is not done yet */
   if (kvmd == NULL) return;
   if (nl[0].n_type == 0) kvm_nlist(kvmd, nl);

   if (nl[0].n_type != 0) {
      uvmexp = (struct uvmexp *)nl[X_UVM_EXP].n_value;
      if (kvm_read(kvmd, (u_long)&uvmexp->forks, &i, sizeof(i)) == sizeof(i))
	 n_forks = i;
   }

   if (getloadavg(&avenrun, 1) <= 0)
		avenrun = 0;
	gkrellm_proc_assign_data(n_processes, 0, n_forks, avenrun);
}

void
gkrellm_sys_proc_read_users(void)
	{
	gint	n_users;
   static time_t utmp_mtime;
   struct utmp utmp;
   struct stat s;
   FILE *ut;

	if (stat(_PATH_UTMP, &s) == 0 && s.st_mtime != utmp_mtime)
		{
		if ((ut = fopen(_PATH_UTMP, "r")) != NULL)
			{
			n_users = 0;
			while (fread(&utmp, sizeof(utmp), 1, ut))
				{
				if (utmp.ut_name[0] == '\0') continue;
					++n_users;
				}
			(void)fclose(ut);
			gkrellm_proc_assign_users(n_users);
			}
		utmp_mtime = s.st_mtime;
		}
	}

gboolean
gkrellm_sys_proc_init(void)
	{
	return TRUE;
	}


/* ===================================================================== */
/* Memory/Swap monitor interface */

#include <sys/vmmeter.h>
#include <sys/sysctl.h>
#include <uvm/uvm_extern.h>
#include <kvm.h>

static gulong	swapin,
				swapout;
static guint64	swap_total,
				swap_used;

void
gkrellm_sys_mem_read_data(void)
{
   static int mib[] = { CTL_VM, VM_METER };
   static int pgout, pgin;
   unsigned long	total, used, x_used, free, shared, buffers, cached;
   struct vmtotal vmt;
   struct uvmexp uvmexp;
   size_t len;
   static struct nlist nl[] = {
#define X_UVM_EXP    0
   { "_uvmexp" },
   { NULL }
};


   if (kvmd == NULL) return;

   /* get the name list if it's not done yet */
   if (nl[0].n_type == 0) kvm_nlist(kvmd, nl);

   if (nl[0].n_type != 0)
      if (kvm_read(kvmd, nl[X_UVM_EXP].n_value,
		   &uvmexp, sizeof(uvmexp)) != sizeof(uvmexp))
	 memset(&uvmexp, 0, sizeof(uvmexp));

   if (sysctl(mib, 2, &vmt, &len, NULL, 0) < 0)
      memset(&vmt, 0, sizeof(vmt));

   total = (uvmexp.npages - uvmexp.wired) << uvmexp.pageshift;

   /* not sure of what must be computed */
   x_used = (uvmexp.active + uvmexp.inactive) << uvmexp.pageshift;
   free = uvmexp.free << uvmexp.pageshift;
   shared = vmt.t_rmshr << uvmexp.pageshift;

   /* want to see only this in the chat. this could be changed */
   used = uvmexp.active << uvmexp.pageshift;

   /* don't know how to get those values */
   buffers = 0;
   cached = 0;

   gkrellm_mem_assign_data(total, used, free, shared, buffers, cached);

   /* show only the pages located on the disk and not in memory */
   swap_total = (guint64) (uvmexp.swpages << uvmexp.pageshift);
   swap_used = (guint64) (uvmexp.swpgonly << uvmexp.pageshift);

   /* For page in/out operations, uvmexp struct doesn't seem to be reliable */

   /* if the number of swapped pages that are in memory (inuse - only) is
    * greater that the previous value (pgin), we count this a "page in" */
   if (uvmexp.swpginuse - uvmexp.swpgonly > pgin)
      swapin += uvmexp.swpginuse - uvmexp.swpgonly - pgin;
   pgin = uvmexp.swpginuse - uvmexp.swpgonly;

   /* same for page out */
   if (uvmexp.swpgonly > pgout)
      swapout += uvmexp.swpgonly - pgout;
   pgout = uvmexp.swpgonly;
}

void
gkrellm_sys_swap_read_data(void)
	{
	gkrellm_swap_assign_data(swap_total, swap_used, swapin, swapout);
	}

gboolean
gkrellm_sys_mem_init(void)
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
#include <sys/ioctl.h>

#if defined(__i386__) || defined(__powerpc__)

#include <machine/apmvar.h>
#define	APMDEV		"/dev/apm"

void
gkrellm_sys_battery_read_data(void)
	{
	int			f, r;
	struct apm_power_info info;
	gboolean    available, on_line, charging;
	gint        percent, time_left;

	if ((f = open(APMDEV, O_RDONLY)) == -1) return;
	memset(&info, 0, sizeof(info));
	r = ioctl(f, APM_IOC_GETPOWER, &info);
	close(f);
	if (r == -1) return;

	available = (info.battery_state != APM_BATT_UNKNOWN);
	on_line = (info.ac_state == APM_AC_ON) ? TRUE : FALSE;
	charging = (info.battery_state == APM_BATT_CHARGING) ? TRUE : FALSE;
	percent = info.battery_life;
	time_left = info.minutes_left;
	gkrellm_battery_assign_data(0, available, on_line, charging,
				percent, time_left);
	}

gboolean
gkrellm_sys_battery_init()
	{
	return TRUE;
	}

#else

void
gkrellm_sys_battery_read_data(void)
	{
	}

gboolean
gkrellm_sys_battery_init()
	{
	return FALSE;
	}
#endif


/* ===================================================================== */
/* Disk monitor interface */

#include <sys/dkstat.h>
#include <sys/disk.h>
#include <kvm.h>

static struct nlist nl_disk[] = {
#define X_DISK_COUNT    0
   { "_disk_count" },      /* number of disks */
#define X_DISKLIST      1
   { "_disklist" },        /* TAILQ of disks */
   { NULL },
};

static struct disk *dkdisks;	/* kernel disk list head */

extern	kvm_t	*kvmd;

static gint	n_disks;

gchar *
gkrellm_sys_disk_name_from_device(gint device_number, gint unit_number,
			gint *order)
	{
	return NULL;	/* disk data by device not implemented */
	}

gint
gkrellm_sys_disk_order_from_name(const gchar *name)
	{
	return -1;  /* append disk charts as added */
	}


void
gkrellm_sys_disk_read_data(void)
{
   struct disk	d, *p;
   gint			i;
   char			buf[20];
   guint64		rbytes, wbytes;

   if (kvmd == NULL) return;
   if (n_disks <= 0) return;		/* computed by register_disks() */
   if (nl_disk[0].n_type == 0) return;	/* ditto. */

   for (i = 0, p = dkdisks; i < n_disks; p = d.dk_link.tqe_next, ++i)
	{
	if (kvm_read(kvmd, (u_long)p, &d, sizeof(d)) == sizeof(d))
		{
		if (kvm_read(kvmd, (u_long)d.dk_name, buf, sizeof(buf)) != sizeof(buf))
			/* fallback to default name if kvm_read failed */
			sprintf(buf, "%s%c", _("Disk"), 'A' + i);
	 
		/* It seems impossible to get the read and write transfers
		 * separately. Its just a matter of choice to put the total in
		 * the rbytes member but I like the blue color so much.
		*/

		/* Separate read/write stats were implemented in NetBSD 1.6K.
		*/

#if __NetBSD_Version__ >= 106110000
		rbytes = d.dk_rbytes;
		wbytes = d.dk_wbytes;
#else
		rbytes = d.dk_bytes;
		wbytes = 0;
#endif

		gkrellm_disk_assign_data_by_name(buf, rbytes, wbytes);
		}
	}
}

gboolean
gkrellm_sys_disk_init(void)
{
   struct disklist_head head;


   if (kvmd == NULL) return FALSE;

   /* get disk count */
   if (kvm_nlist(kvmd, nl_disk) >= 0 && nl_disk[0].n_type != 0)
      if (kvm_read(kvmd, nl_disk[X_DISK_COUNT].n_value,
		   (char *)&n_disks, sizeof(n_disks)) != sizeof(n_disks))
	 n_disks = 0;

   /* get first disk */
   if (n_disks > 0) {
      if (kvm_read(kvmd, nl_disk[X_DISKLIST].n_value, 
		   &head, sizeof(head)) != sizeof(head))
	 n_disks = 0;

      dkdisks = head.tqh_first;
   }
   return TRUE;
}
