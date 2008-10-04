/* GKrellM
|  Copyright (C) 1999-2007 Bill Wilson
|
|  Author:  Bill Wilson    billw@gkrellm.net
|  Latest versions might be found at:  http://gkrellm.net
|
|  netbsd.c code is Copyright (C):
|             Anthony Mallet <anthony.mallet@useless-ficus.net>
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
	if (setgid(getgid()) != 0)
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

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/sched.h>

static gint ncpus;

static gint get_ncpus(void);

void
gkrellm_sys_cpu_read_data(void)
{
   static int mib[] = { CTL_KERN, KERN_CP_TIME };
   u_int64_t cp_time[ncpus][CPUSTATES];
   int n;
   size_t len;

   if (ncpus > 1) {
       len = sizeof(cp_time[0]);
       /* The sysctl() is magic -- it returns the aggregate if
	  there's not enough room for all CPU's. */
       if (sysctl(mib, 2, cp_time, &len, NULL, 0) == 0)
	   gkrellm_cpu_assign_composite_data(cp_time[0][CP_USER],
	       cp_time[0][CP_NICE], cp_time[0][CP_SYS], cp_time[0][CP_IDLE]);
   }

   len = sizeof(cp_time);
   if (sysctl(mib, 2, cp_time, &len, NULL, 0) < 0) return;
   for (n = 0; n < ncpus; n++)
	gkrellm_cpu_assign_data(n, cp_time[n][CP_USER],
	   cp_time[n][CP_NICE], cp_time[n][CP_SYS], cp_time[n][CP_IDLE]);
}

gboolean
gkrellm_sys_cpu_init(void)
    {
	ncpus = get_ncpus();
	gkrellm_cpu_set_number_of_cpus(ncpus);
	return TRUE;
	}

static gint
get_ncpus(void)
{
	static int mib[] = { CTL_HW, HW_NCPU };
	int ncpus;
	size_t len = sizeof(int);

	if (sysctl(mib, 2, &ncpus, &len, NULL, 0) < 0)
		return 1;
	else
		return ncpus;
}


/* ===================================================================== */
/* Proc monitor interface */

#include <sys/proc.h>
#include <sys/sysctl.h>
#include <uvm/uvm_extern.h>

#include <utmp.h>

void
gkrellm_sys_proc_read_data(void)
{
   int mib[6];
   double avenrun;
	guint	n_forks = 0, n_processes = 0;
   struct uvmexp_sysctl uvmexp;
   size_t size;

   mib[0] = CTL_KERN;
   mib[1] = KERN_PROC2;
   mib[2] = KERN_PROC_ALL;
   mib[3] = 0;
   mib[4] = sizeof(struct kinfo_proc2);
   mib[5] = 0;
   if (sysctl(mib, 6, NULL, &size, NULL, 0) >= 0) {
      n_processes = size / sizeof(struct kinfo_proc2);
   }

   mib[0] = CTL_VM;
   mib[1] = VM_UVMEXP2;
   size = sizeof(uvmexp);
   if (sysctl(mib, 2, &uvmexp, &size, NULL, 0) >= 0) {
      n_forks = uvmexp.forks;
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


void
gkrellm_sys_mem_read_data(void)
{
   int mib[2];
   guint64 total, used, free, shared, buffers, cached;
   struct vmtotal vmt;
   struct uvmexp_sysctl uvmexp;
   size_t len;

   mib[0] = CTL_VM;
   mib[1] = VM_METER;
   len = sizeof(vmt);
   if (sysctl(mib, 2, &vmt, &len, NULL, 0) < 0)
      memset(&vmt, 0, sizeof(vmt));

   mib[0] = CTL_VM;
   mib[1] = VM_UVMEXP2;
   len = sizeof(uvmexp);
   if (sysctl(mib, 2, &uvmexp, &len, NULL, 0) < 0)
      memset(&uvmexp, 0, sizeof(uvmexp));

   total = uvmexp.npages << uvmexp.pageshift;

   /* not sure of what must be computed */
   free = (uvmexp.inactive + uvmexp.free) << uvmexp.pageshift;
   shared = vmt.t_rmshr << uvmexp.pageshift;

   /* can't use "uvmexp.active << uvmexp.pageshift" here because the
    * display for "free" uses "total - used" which is very wrong. */
   used = total - free;

   /* don't know how to get those values */
   buffers = 0;
   cached = 0;

   gkrellm_mem_assign_data(total, used, free, shared, buffers, cached);

}

void
gkrellm_sys_swap_read_data(void)
{
   static int pgout, pgin;
   int mib[2];
   struct uvmexp_sysctl uvmexp;
   size_t len;
   static gulong swapin = 0, swapout = 0;
   guint64 swap_total, swap_used;

   mib[0] = CTL_VM;
   mib[1] = VM_UVMEXP2;
   len = sizeof(uvmexp);
   if (sysctl(mib, 2, &uvmexp, &len, NULL, 0) < 0)
      memset(&uvmexp, 0, sizeof(uvmexp));

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

   gkrellm_swap_assign_data(swap_total, swap_used, swapin, swapout);
}

gboolean
gkrellm_sys_mem_init(void)
	{
	return TRUE;
	}


/* ===================================================================== */
/* Sensor monitor interface */

  /* Tables of voltage correction factors and offsets derived from the
  |  compute lines in sensors.conf.  See the README file.
  */
	/* "lm78-*" "lm78-j-*" "lm79-*" "w83781d-*" "sis5595-*" "as99127f-*" */
	/* Values from LM78/LM79 data sheets	*/
#if 0
static VoltDefault	voltdefault0[] =
	{
	{ "Vcor1",	1.0,    0, NULL },
	{ "Vcor2",	1.0,    0, NULL },
	{ "+3.3V",	1.0,    0, NULL },
	{ "+5V",	1.68,   0, NULL },		/* in3 ((6.8/10)+1)*@	*/
	{ "+12V",	4.0,    0, NULL },		/* in4 ((30/10)+1)*@	*/
	{ "-12V",	-4.0,   0, NULL },		/* in5 -(240/60)*@		*/
	{ "-5V",	-1.667, 0, NULL }		/* in6 -(100/60)*@		*/
	};
#endif

/* The SENSORS_DIR is not defined as a directory, but directly points on
 * the "sysmon" device, which implements envsys(4) API. This #define is
 * not really useful for NetBSD since every sensor will use that device,
 * but it still provides the location of the device inside the GUI. */
#define SENSORS_DIR	"/dev/sysmon"

#include <sys/envsys.h>


/*
 * get_netbsd_sensor ----------------------------------------------------
 *
 * Perform sensor reading
 */
#include <sys/ioctl.h>

gboolean
gkrellm_sys_sensors_get_temperature(gchar *path, gint id,
        gint iodev, gint interface, gfloat *temp)
{
   envsys_tre_data_t data;	/* sensor data */

   data.sensor = interface;
   if (ioctl(iodev, ENVSYS_GTREDATA, &data) < 0) return FALSE;
   if (!(data.validflags & (ENVSYS_FVALID|ENVSYS_FCURVALID))) return FALSE;

   if (data.units == ENVSYS_STEMP) {
      if (temp)	/* values in uK */
	     *temp = (data.cur.data_us / 1.0e6) - 273.15/*0K*/ ;
	 return TRUE;
   }

   return FALSE;
}

gboolean
gkrellm_sys_sensors_get_fan(gchar *path, gint id,
        gint iodev, gint interface, gfloat *fan)
{
   envsys_tre_data_t data;	/* sensor data */

   data.sensor = interface;
   if (ioctl(iodev, ENVSYS_GTREDATA, &data) < 0) return FALSE;
   if (!(data.validflags & (ENVSYS_FVALID|ENVSYS_FCURVALID))) return FALSE;

   if (data.units == ENVSYS_SFANRPM) {
      if (fan)	/* values in RPM */
	    *fan = data.cur.data_us;
	 return TRUE;
   }
   return FALSE;
}

gboolean
gkrellm_sys_sensors_get_voltage(gchar *path, gint id,
        gint iodev, gint interface, gfloat *volt)
{
   envsys_tre_data_t data;	/* sensor data */

   data.sensor = interface;
   if (ioctl(iodev, ENVSYS_GTREDATA, &data) < 0) return FALSE;
   if (!(data.validflags & (ENVSYS_FVALID|ENVSYS_FCURVALID))) return FALSE;

   if (data.units == ENVSYS_SVOLTS_DC) {
      if (volt)		/* values in uV */
	    *volt = data.cur.data_s / 1.0e6;
	 return TRUE;
   }

   return FALSE;
}

/*
 *
 * At the moment, only two chips are supported: lm78 and alike (see
 * lm(4)), and VT82C68A South Bridge for VIA chipsets (see viaenv(4)).
 * Both support the envsys(4) API.
 *
 * XXX /dev/sysmon is opened but never closed. This is a problem since
 * the driver wants an exclusive lock (e.g. envstat won't work when
 * GKrellM will be running). But, at this time, I don't want to
 * open/close sysmon each time a reading is needed. See README for
 * details.
 */

gboolean
gkrellm_sys_sensors_init(void)
{
   envsys_basic_info_t info;	/* sensor misc. info */
   int fd;			/* file desc. for /dev/sysmon */
   int id = 0;			/* incremented for each sensor */
   int type;
   char *s, base_name[33];
   gboolean	found_sensors = FALSE;

   /* check if some sensor is configured */
   fd = open(SENSORS_DIR, O_RDONLY); /* never closed */ 
   if (fd < 0) return FALSE;

   /* iterate through available sensors, until the first invalid */
   for(info.sensor=0; ; info.sensor++) {

      /* stop if we can't ioctl() */
      if (ioctl(fd, ENVSYS_GTREINFO, &info) < 0) break;
      /* stop if that sensor is not valid */
      if (!(info.validflags & ENVSYS_FVALID)) break;

      switch(info.units) {
	 case ENVSYS_STEMP:
	    type = SENSOR_TEMPERATURE;	break;
	 case ENVSYS_SFANRPM:
	    type = SENSOR_FAN;		break;
	 case ENVSYS_SVOLTS_DC:
	    type = SENSOR_VOLTAGE;		break;
	 default:
	    /* unwanted sensor type: continue */
	    continue;
      }

      /* ok, we've got one working sensor */
      sprintf(base_name, "%32s", info.desc);
      /* must map spaces into something else (for config file items) */
      for(s=strchr(base_name, ' '); s != NULL; s=strchr(s, ' '))
	 *s++ = '_';

      gkrellm_sensors_add_sensor(type, SENSORS_DIR, base_name,
           id, fd, info.sensor, 1.0,
           0.0, NULL, NULL);
      found_sensors = TRUE;
   }
   return found_sensors;
}



/* ===================================================================== */
/* Battery monitor interface */

#if defined(__i386__) || defined(__powerpc__)
# include <sys/ioctl.h>
# include <machine/apmvar.h>

# define	APMDEV		"/dev/apm"
#endif

static int battery_use_apm = 1;

/* battery data, index by [battery number] */
static struct battery_acpi_data {
  gboolean available;	int available_index;
  gboolean on_line;	int on_line_index;
  gboolean charging;	int charging_index;
  gint full_cap;	int full_cap_index;
  gint cap;		int cap_index;
  gint discharge_rate;	int discharge_rate_index;
  gint charge_rate;	int charge_rate_index;
} *battery_acpi_data;
static int battery_acpi_data_items;

static int
gkrellm_battery_data_alloc(int bat)
{
  if (bat + 1 > battery_acpi_data_items)
    battery_acpi_data_items = bat + 1;

  battery_acpi_data = realloc(battery_acpi_data,
			      battery_acpi_data_items*
			      sizeof(*battery_acpi_data));
  return battery_acpi_data?1:0;
}

void
gkrellm_sys_battery_read_data(void)
{
  int i;
  int fd;			/* file desc. for /dev/sysmon or /dev/apm */
  envsys_tre_data_t data;	/* sensor data */
  int time_left;

  if (battery_use_apm) {
#if defined(__i386__) || defined(__powerpc__)
    int			r;
    struct apm_power_info apminfo;
    gboolean available, on_line, charging;
    gint percent, time_left;

    if ((fd = open(APMDEV, O_RDONLY)) == -1) return;
    memset(&apminfo, 0, sizeof(apminfo));
    r = ioctl(fd, APM_IOC_GETPOWER, &apminfo);
    close(fd);
    if (r == -1) return;

    available = (apminfo.battery_state != APM_BATT_UNKNOWN);
    on_line = (apminfo.ac_state == APM_AC_ON) ? TRUE : FALSE;
    charging = (apminfo.battery_state == APM_BATT_CHARGING) ? TRUE : FALSE;
    percent = apminfo.battery_life;
    time_left = apminfo.minutes_left;
    gkrellm_battery_assign_data(0, available, on_line, charging,
				percent, time_left);
#else
    return;
#endif
  }

   fd = open(SENSORS_DIR, O_RDONLY);
   if (fd < 0) return;

   data.sensor = battery_acpi_data[0].on_line_index;
   if (ioctl(fd, ENVSYS_GTREDATA, &data) < 0) return;
   if (!(data.validflags & ENVSYS_FVALID)) return;
   battery_acpi_data[0].on_line = data.cur.data_us ? TRUE:FALSE;

   /* iterate through available batteries */
   for(i=0; i<battery_acpi_data_items; i++) {
#define read_sensor(x)							\
     do {								\
       data.sensor = battery_acpi_data[i].x ## _index;			\
       if (ioctl(fd, ENVSYS_GTREDATA, &data) < 0) continue;		\
       if (!(data.validflags & ENVSYS_FCURVALID))			\
	 battery_acpi_data[i].x = -1;					\
       else								\
	 battery_acpi_data[i].x = data.cur.data_s;			\
     } while(0)

     read_sensor(available);
     read_sensor(charging);
     read_sensor(full_cap);
     read_sensor(cap);
     read_sensor(discharge_rate);
     read_sensor(charge_rate);
#undef read_sensor

     if (battery_acpi_data[i].discharge_rate > 0)
       time_left =
	 battery_acpi_data[i].cap * 60 / battery_acpi_data[i].discharge_rate;
     else if (battery_acpi_data[i].charge_rate > 0)
       time_left =
	 (battery_acpi_data[i].full_cap - battery_acpi_data[i].cap) * 60 /
	 battery_acpi_data[i].charge_rate;
     else
       time_left = -1;

     if (battery_acpi_data[i].available) {
       gkrellm_battery_assign_data(i,
				   battery_acpi_data[i].available,
				   battery_acpi_data[0].on_line,
				   battery_acpi_data[i].charging,

				   /* percent */
				   battery_acpi_data[i].cap * 100 /
				   battery_acpi_data[i].full_cap,

				   /* time left (minutes) */
				   time_left);
     } else
       gkrellm_battery_assign_data(i, 0, 0, 0, -1, -1);
   }

   close(fd);
}

gboolean
gkrellm_sys_battery_init()
{
   int fd;			/* file desc. for /dev/sysmon or /dev/apm */
   envsys_basic_info_t info;	/* sensor misc. info */
   gboolean found_sensors = FALSE;
   char fake[2];
   int r, bat;

   /* --- check APM first --- */

#if defined(__i386__) || defined(__powerpc__)
   do {
     struct apm_power_info info;

     if ((fd = open(APMDEV, O_RDONLY)) == -1) break;
     r = ioctl(fd, APM_IOC_GETPOWER, &info);
     close(fd);
     if (r != -1) {
       battery_use_apm = 1;
       return TRUE;
     }
   } while(0);
#endif

   /* --- check for some envsys(4) acpi battery --- */

   battery_use_apm = 0;
   battery_acpi_data_items = 0;
   battery_acpi_data = NULL; /* this is never freed */

   fd = open(SENSORS_DIR, O_RDONLY);
   if (fd < 0) return FALSE;

   /* iterate through available sensors */
   for(info.sensor=0; ; info.sensor++) {
      /* stop if we can't ioctl() */
      if (ioctl(fd, ENVSYS_GTREINFO, &info) < 0) break;
      /* stop if that sensor is not valid */
      if (!(info.validflags & ENVSYS_FVALID)) break;

      do {
	if (info.units == ENVSYS_INDICATOR &&
	    sscanf(info.desc, "acpibat%d presen%1[t]", &bat, fake) == 2) {
	  if (!gkrellm_battery_data_alloc(bat)) return FALSE;
	  battery_acpi_data[bat].available_index = info.sensor;
	  found_sensors = TRUE;

	} else if (info.units == ENVSYS_INDICATOR &&
		   sscanf(info.desc, "acpiacad%*d connecte%1[d]", fake) == 1) {
	  if (!gkrellm_battery_data_alloc(0)) return FALSE;
	  battery_acpi_data[0].on_line_index = info.sensor;

	} else if (info.units == ENVSYS_INDICATOR &&
		   sscanf(info.desc,
			  "acpibat%d chargin%1[g]", &bat, fake) == 2) {
	  if (!gkrellm_battery_data_alloc(bat)) return FALSE;
	  battery_acpi_data[bat].charging_index = info.sensor;
	  found_sensors = TRUE;

	} else if (info.units == ENVSYS_SAMPHOUR &&
		   sscanf(info.desc,
			  "acpibat%d design ca%1[p]", &bat, fake) == 2) {
	  if (!gkrellm_battery_data_alloc(bat)) return FALSE;
	  battery_acpi_data[bat].full_cap_index = info.sensor;
	  found_sensors = TRUE;

	} else if (info.units == ENVSYS_SAMPHOUR &&
		   sscanf(info.desc,
			  "acpibat%d charg%1[e]", &bat, fake) == 2) {
	  if (!gkrellm_battery_data_alloc(bat)) return FALSE;
	  battery_acpi_data[bat].cap_index = info.sensor;
	  found_sensors = TRUE;

	} else if (info.units == ENVSYS_SAMPS &&
		   sscanf(info.desc,
			  "acpibat%d discharge rat%1[e]", &bat, fake) == 2) {
	  if (!gkrellm_battery_data_alloc(bat)) return FALSE;
	  battery_acpi_data[bat].discharge_rate_index = info.sensor;
	  found_sensors = TRUE;

	} else if (info.units == ENVSYS_SAMPS &&
		   sscanf(info.desc,
			  "acpibat%d charge rat%1[e]", &bat, fake) == 2) {
	  if (!gkrellm_battery_data_alloc(bat)) return FALSE;
	  battery_acpi_data[bat].charge_rate_index = info.sensor;
	  found_sensors = TRUE;
	}
      } while(0);
   }

   close(fd);

   return found_sensors;
}


/* ===================================================================== */
/* Disk monitor interface */

#include <sys/dkstat.h>
#include <sys/disk.h>
#include <sys/sysctl.h>

#ifdef HW_IOSTATS
#define HW_DISKSTATS	HW_IOSTATS
#define disk_sysctl	io_sysctl
#define dk_rbytes	rbytes
#define dk_wbytes	wbytes
#define dk_name		name
#endif

gboolean
gkrellm_sys_disk_init(void)
{
	int mib[3] = { CTL_HW, HW_DISKSTATS, sizeof(struct disk_sysctl) };
	size_t size;

	/* Just test if the sysctl call works */
	if (sysctl(mib, 3, NULL, &size, NULL, 0) == -1)
		return (FALSE);

	return (TRUE);
}

void
gkrellm_sys_disk_read_data(void)
{
	int i, n_disks, mib[3] = { CTL_HW, HW_DISKSTATS, sizeof(struct disk_sysctl) };
	size_t size;
	guint64 rbytes, wbytes;
	struct disk_sysctl *dk_drives;

	if (sysctl(mib, 3, NULL, &size, NULL, 0) == -1)
		return;
	dk_drives = malloc(size);
	if (dk_drives == NULL)
		return;
	n_disks = size / sizeof(struct disk_sysctl);

	if (sysctl(mib, 3, dk_drives, &size, NULL, 0) == -1)
		return;

	for (i = 0; i < n_disks; i++) {
#if __NetBSD_Version__ >= 106110000
		rbytes = dk_drives[i].dk_rbytes;
		wbytes = dk_drives[i].dk_wbytes;
#else
		rbytes = dk_drives[i].dk_bytes;
		wbytes = 0;
#endif

		gkrellm_disk_assign_data_by_name(dk_drives[i].dk_name, rbytes, wbytes, FALSE);
	}

	free(dk_drives);
}

gchar *
gkrellm_sys_disk_name_from_device(gint device_number, gint unit_number,
			gint *order)
	{
	return NULL;	/* disk data by device not implemented */
	}

gint
gkrellm_sys_disk_order_from_name(gchar *name)
	{
	return -1;  /* append disk charts as added */
	}

#if __NetBSD_Version__ >= 399000100

#include "../inet.h"

#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp_fsm.h>

static const struct gkrellm_inet_fam {
	sa_family_t family;
	const char *mib;
} families[] = { {AF_INET, "net.inet.tcp.pcblist"},
#ifdef INET6
    {AF_INET6, "net.inet6.tcp6.pcblist"},
#endif
    {0, NULL} };

void
gkrellm_sys_inet_read_tcp_data()
{
	ActiveTCP tcp;
	int mib[CTL_MAXNAME], i;
	size_t sz;
	u_int namelen;
	struct kinfo_pcb *pcbt = NULL;
	const struct gkrellm_inet_fam *pf = families;

	while (pf->mib != NULL) {
		sz = CTL_MAXNAME;
		if (sysctlnametomib(pf->mib, mib, &sz) == -1)
			return;
		namelen = sz;

		mib[namelen++] = PCB_ALL;
		mib[namelen++] = 0;
		mib[namelen++] = sizeof(struct kinfo_pcb);
		mib[namelen++] = INT_MAX;

		sz = 0;
		pcbt = NULL;
		if (sysctl(&mib[0], namelen, pcbt, &sz, NULL, 0) == -1)
			return;
		pcbt = malloc(sz);
		if (pcbt == NULL)
			return;
		if (sysctl(&mib[0], namelen, pcbt, &sz, NULL, 0) == -1)
			return;

		sz /= sizeof(struct kinfo_pcb);
		for (i = 0; i < sz; i++) {
			tcp.family = pf->family;
			if (pf->family == AF_INET) {
				struct sockaddr_in *sin =
				    (struct sockaddr_in *)&pcbt[i].ki_dst;
				tcp.remote_addr.s_addr = sin->sin_addr.s_addr;
				tcp.remote_port = sin->sin_port;

				sin = (struct sockaddr_in *)&pcbt[i].ki_src;
				tcp.local_port = sin->sin_port;
#ifdef INET6
			} else { /* AF_INET6 */
				struct sockaddr_in6 *sin =
				    (struct sockaddr_in6 *)&pcbt[i].ki_dst;
				memcpy(&tcp.remote_addr6, &sin->sin6_addr,
				    sizeof(struct in6_addr));
				tcp.remote_port = sin->sin6_port;

				sin = (struct sockaddr_in6 *)&pcbt[i].ki_src;
				tcp.local_port = sin->sin6_port;
#endif
			}
			if (pcbt[i].ki_tstate == TCPS_ESTABLISHED)
				gkrellm_inet_log_tcp_port_data(&tcp);
		}
		free(pcbt);
		pf++;
	}
}
#endif
