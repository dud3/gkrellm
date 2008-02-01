/* GKrellM
|  Copyright (C) 1999-2007 Bill Wilson
|
|  Author:  Bill Wilson    billw@gkrellm.net
|  Latest versions might be found at:  http://gkrellm.net
|
|  darwin.c code is Copyright (c) Ben Hines <bhines@alumni.ucsd.edu>
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

#include <kvm.h>

#include <mach/mach_init.h>
#include <mach/mach_host.h>
#include <mach/vm_map.h>

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

static gint		n_cpus;

void
gkrellm_sys_cpu_read_data(void)
	{
		processor_cpu_load_info_data_t *pinfo;
		mach_msg_type_number_t info_count;
		int i = 0;

		if (host_processor_info (mach_host_self (),
						   PROCESSOR_CPU_LOAD_INFO,
						   &n_cpus,
						   (processor_info_array_t*)&pinfo,
						   &info_count)) {
			return;
		}

        for (i = 0; i < n_cpus; i++) {
			gkrellm_cpu_assign_data(i,
					pinfo[i].cpu_ticks [CPU_STATE_USER],
					pinfo[i].cpu_ticks [CPU_STATE_NICE],		
					pinfo[i].cpu_ticks [CPU_STATE_SYSTEM],
					pinfo[i].cpu_ticks [CPU_STATE_IDLE]);
		}
		vm_deallocate (mach_task_self (), (vm_address_t) pinfo, info_count);
	}

gboolean
gkrellm_sys_cpu_init(void)
	{
	processor_cpu_load_info_data_t *pinfo;
	mach_msg_type_number_t info_count;

	n_cpus = 0;
	
	if (host_processor_info (mach_host_self (),
						  PROCESSOR_CPU_LOAD_INFO,
						  &n_cpus,
						  (processor_info_array_t*)&pinfo,
						  &info_count)) {
		return FALSE;
	}
	gkrellm_cpu_set_number_of_cpus(n_cpus);
	return TRUE;
	}


/* ===================================================================== */
/* Proc monitor interface */

#include <sys/sysctl.h>
#include <sys/user.h>
#define	PID_MAX		30000

#include <kvm.h>
#include <limits.h>
#include <paths.h>
#include <utmp.h>

static int	oid_v_forks[CTL_MAXNAME + 2];
static int	oid_v_vforks[CTL_MAXNAME + 2];
static int	oid_v_rforks[CTL_MAXNAME + 2];
static size_t	oid_v_forks_len = sizeof(oid_v_forks);
static size_t	oid_v_vforks_len = sizeof(oid_v_vforks);
static size_t	oid_v_rforks_len = sizeof(oid_v_rforks);
static int	have_v_forks = 0;

gboolean
gkrellm_sys_proc_init(void)
	{
	static int	oid_name2oid[2] = { 0, 3 };
	static char	*name = "vm.stats.vm.v_forks";
	static char	*vname = "vm.stats.vm.v_vforks";
	static char	*rname = "vm.stats.vm.v_rforks";

	/* check if vm.stats.vm.v_forks is available */
	if (sysctl(oid_name2oid, 2, oid_v_forks, &oid_v_forks_len,
		   (void *)name, strlen(name)) < 0)
		return TRUE;
	if (sysctl(oid_name2oid, 2, oid_v_vforks, &oid_v_vforks_len,
		   (void *)vname, strlen(vname)) < 0)
		return TRUE;
	if (sysctl(oid_name2oid, 2, oid_v_rforks, &oid_v_rforks_len,
		   (void *)rname, strlen(rname)) < 0)
		return TRUE;
	oid_v_forks_len /= sizeof(int);
	oid_v_vforks_len /= sizeof(int);
	oid_v_rforks_len /= sizeof(int);
	++have_v_forks;
	return TRUE;
	}

void
gkrellm_sys_proc_read_data(void)
	{
	static int	oid_proc[] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL };
	double		avenrun;
	static u_int	n_processes, n_forks = 0, curpid = -1;
	u_int		n_vforks, n_rforks;
	gint		r_forks, r_vforks, r_rforks;
	size_t		len;
	gint		nextpid, nforked;
	static struct nlist nl[] = {
#define N_NEXTPID	0
		{ "_nextpid" },
		{ "" }
	};


	if (getloadavg(&avenrun, 1) <= 0)
		avenrun = 0;

	if (have_v_forks)
		{
		/* We don't want to just use sysctlbyname().  Because,
                 * we call it so often. */
		len = sizeof(n_forks);
		r_forks = sysctl(oid_v_forks, oid_v_forks_len,
				 &n_forks, &len, NULL, 0);
		len = sizeof(n_vforks);
		r_vforks = sysctl(oid_v_vforks, oid_v_vforks_len,
				  &n_vforks, &len, NULL, 0);
		len = sizeof(n_rforks);
		r_rforks = sysctl(oid_v_rforks, oid_v_rforks_len,
				  &n_rforks, &len, NULL, 0);
		if (r_forks >= 0 && r_vforks >= 0 && r_rforks >= 0)
			n_forks = n_forks + n_vforks + n_rforks;
		}
	else
		{
		/* workaround: Can I get total number of processes? */
		if (kvmd != NULL)
			{
			if (nl[0].n_type == 0)
				kvm_nlist(kvmd, nl);
			if (nl[0].n_type != 0 &&
			    kvm_read(kvmd, nl[N_NEXTPID].n_value,
				     (char *)&nextpid,
				     sizeof(nextpid)) == sizeof(nextpid))
				{
				if (curpid < 0)
					curpid = nextpid;
				if ((nforked = nextpid - curpid) < 0)
					n_forks += PID_MAX - 100;
				n_forks += nforked;
				curpid = nextpid;
				n_forks = n_forks;
				}
			}
		}

	if (sysctl(oid_proc, 3, NULL, &len, NULL, 0) >= 0)
		n_processes = len / sizeof(struct kinfo_proc);

	gkrellm_proc_assign_data(n_processes, 0, n_forks, avenrun);
	}

void
gkrellm_sys_proc_read_users(void)
	{
	gint		n_users;
	struct stat	sb, s;
	gchar		ttybuf[MAXPATHLEN];
	FILE		*ut;
	struct utmp	utmp;
	static time_t	utmp_mtime;

	if (stat(_PATH_UTMP, &s) != 0 || s.st_mtime == utmp_mtime)
		return;
	if ((ut = fopen(_PATH_UTMP, "r")) != NULL)
		{
		n_users = 0;
		while (fread(&utmp, sizeof(utmp), 1, ut))
			{
			if (utmp.ut_name[0] == '\0')
				continue;
			(void)snprintf(ttybuf, sizeof(ttybuf), "%s/%s",
				       _PATH_DEV, utmp.ut_line);
			/* corrupted record */
			if (stat(ttybuf, &sb))
				continue;
			++n_users;
			}
		(void)fclose(ut);
		gkrellm_proc_assign_users(n_users);
		}
	utmp_mtime = s.st_mtime;
	}


/* ===================================================================== */
/* Disk monitor interface */

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOBlockStorageDriver.h>
io_iterator_t       drivelist  = 0;  /* needs release */
mach_port_t         masterPort = 0;

static GList	*disk_list;		/* list of names */

gchar *
gkrellm_sys_disk_name_from_device(gint device_number, gint unit_number,
			gint *order)
	{
	return NULL;	/* Not implemented */
	}

gint
gkrellm_sys_disk_order_from_name(gchar *name)
	{
	/* implement this if you want disk charts to show up in a particular
	|  order in gkrellm.
	*/
	return -1;	/* Not implemented, disks show up in same order as disk_list */
	}

void
gkrellm_sys_disk_read_data(void)
{
    io_registry_entry_t drive      = 0;  /* needs release */
    UInt64         totalReadBytes  = 0;
    UInt64         totalReadCount  = 0;
    UInt64         totalWriteBytes = 0;
    UInt64         totalWriteCount = 0;
    kern_return_t status = 0;
	GList		*list;

	list = disk_list; 
    while ( (drive = IOIteratorNext(drivelist)) )
    {
        CFNumberRef number          = 0;  /* don't release */
        CFDictionaryRef properties  = 0;  /* needs release */
        CFDictionaryRef statistics  = 0;  /* don't release */
        UInt64 value                = 0;
    
        /* Obtain the properties for this drive object */
                
        status = IORegistryEntryCreateCFProperties (drive,
                                                    (CFMutableDictionaryRef *) &properties,
                                                    kCFAllocatorDefault,
                                                    kNilOptions);
        if (properties) {
    
            /* Obtain the statistics from the drive properties */
            statistics = (CFDictionaryRef) CFDictionaryGetValue(properties, CFSTR(kIOBlockStorageDriverStatisticsKey));
    
            if (statistics) {
                /* Obtain the number of bytes read from the drive statistics */
                number = (CFNumberRef) CFDictionaryGetValue (statistics,
                                                            CFSTR(kIOBlockStorageDriverStatisticsBytesReadKey));
                if (number) {
                        status = CFNumberGetValue(number, kCFNumberSInt64Type, &value);
                        totalReadBytes += value;
                }
                /* Obtain the number of reads from the drive statistics */
                number = (CFNumberRef) CFDictionaryGetValue (statistics,
                                                            CFSTR(kIOBlockStorageDriverStatisticsReadsKey));
                if (number) {
                        status = CFNumberGetValue(number, kCFNumberSInt64Type, &value);
                        totalReadCount += value;
                }
    
                /* Obtain the number of writes from the drive statistics */
                number = (CFNumberRef) CFDictionaryGetValue (statistics,
                                                            CFSTR(kIOBlockStorageDriverStatisticsWritesKey));
                if (number) {
                        status = CFNumberGetValue(number, kCFNumberSInt64Type, &value);
                        totalWriteCount += value;
                }
                /* Obtain the number of bytes written from the drive statistics */
                number = (CFNumberRef) CFDictionaryGetValue (statistics,
                                                            CFSTR(kIOBlockStorageDriverStatisticsBytesWrittenKey));
                if (number) {
                        status = CFNumberGetValue(number, kCFNumberSInt64Type, &value);
                        totalWriteBytes += value;
                }
                /* Release resources */
                CFRelease(properties); properties = 0;
            }
        }
        IOObjectRelease(drive); drive = 0;

		if (list)
			{
			gkrellm_disk_assign_data_by_name((gchar *) list->data,
					totalReadCount, totalWriteCount, FALSE);
	        list = list->next;
			}
        
    }
    IOIteratorReset(drivelist);
}

gboolean
gkrellm_sys_disk_init(void)
    {
    io_registry_entry_t drive      = 0;  /* needs release */
    io_registry_entry_t child      = 0;  /* needs release */
    
    /* get ports and services for drive stats */
    /* Obtain the I/O Kit communication handle */
    if (IOMasterPort(MACH_PORT_NULL, &masterPort)) return FALSE;

    /* Obtain the list of all drive objects */
    if (IOServiceGetMatchingServices(masterPort,
				     IOServiceMatching("IOBlockStorageDriver"),
				     &drivelist))
      return FALSE;

    while ( (drive = IOIteratorNext(drivelist)) )
    {
        gchar * name = malloc(128); /* io_name_t is char[128] */
	kern_return_t status = 0;
        int ptr = 0;
   		
   		if(!name) return FALSE;
   		
        /* Obtain the properties for this drive object */           
	status = IORegistryEntryGetChildEntry(drive, kIOServicePlane, &child );

        if(!status)
	status = IORegistryEntryGetName(child, name );

        /* Convert spaces to underscores, for prefs safety */
        if(!status)
        {
            for(ptr = 0; ptr < strlen(name); ptr++) 
            {
                if(name[ptr] == ' ') 
                    name[ptr] = '_';
            }
            disk_list = g_list_append(disk_list, name);
        }
        IOObjectRelease(drive); drive = 0;
    }
    IOIteratorReset(drivelist);
    
    return (disk_list != NULL) ? TRUE : FALSE;
}


/* ===================================================================== */
/* Inet monitor interface - not implemented */

void
gkrellm_sys_inet_read_tcp_data(void)
	{
	}

gboolean
gkrellm_sys_inet_init(void)
	{
	return FALSE;
	}


/* ===================================================================== */
/* Memory/Swap monitor interface */

#include <mach/mach_init.h>
#include <mach/mach_host.h>
#include <mach/host_info.h>
#include <mach/mach_error.h>
#include <sys/types.h>
#include <dirent.h>
#include <mach/mach_types.h>
#include <mach/machine/vm_param.h>

static guint64	swapin,
		swapout,
		swap_total,
		swap_used;

void
gkrellm_sys_mem_read_data(void)
	{
	static gint	psize, pshift, first_time_done = 0;
	vm_statistics_data_t vm_info;
	mach_msg_type_number_t info_count;
	kern_return_t	error;
	static DIR *dirp;
	struct dirent *dp;
	guint64		total, used, free, shared, buffers, cached;

	info_count = HOST_VM_INFO_COUNT;

	error = host_statistics (mach_host_self (), HOST_VM_INFO, (host_info_t)&vm_info, &info_count);
	if (error != KERN_SUCCESS)
	{
		mach_error("host_info", error);
		return;
	}

	if (pshift == 0)
	{
		for (psize = getpagesize(); psize > 1; psize >>= 1)
			pshift++;
	}
	
	used = (natural_t)(vm_info.active_count + vm_info.inactive_count + vm_info.wire_count) << pshift;
	free = (natural_t)vm_info.free_count << pshift;	
	total = (natural_t)(vm_info.active_count + vm_info.inactive_count + vm_info.free_count + vm_info.wire_count) << pshift;
	/* Don't know how to get cached or buffers. */
	buffers = 0;
	cached = 0;
	/* shared  0 for now, shared is a PITA */
        shared = 0;	
	gkrellm_mem_assign_data(total, used, free, shared, buffers, cached);

	/* Swap is available at same time as mem, so grab values here.
	*/
	swapin = vm_info.pageins;
	swapout = vm_info.pageouts;
	swap_used = vm_info.pageouts << pshift;
	
	/* Figure out total swap. This adds up the size of the swapfiles */
	if (!first_time_done)
	{
		dirp = opendir ("/private/var/vm");
		if (!dirp)
			return;
		++first_time_done;
	}
	swap_total = 0;
	while ((dp = readdir (dirp)) != NULL) {
		struct stat sb;
		char fname [MAXNAMLEN];
		if (strncmp (dp->d_name, "swapfile", 8))
			continue;
		strcpy (fname, "/private/var/vm/");
		strcat (fname, dp->d_name);
		if (stat (fname, &sb) < 0)
			continue;
		swap_total += sb.st_size;
	}
	/*  Save overhead, leave it open. where can we close it? */
	rewinddir(dirp);
	/*	closedir (dirp); */
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
/* Battery monitor interface - not implemented */

void
gkrellm_sys_battery_read_data(void)
	{
//	gkrellm_battery_assign_data(0, available, on_line, charging,
//					percent, time_left);
	}

gboolean
gkrellm_sys_battery_init(void)
	{
	return FALSE;
	}

/* ===================================================================== */
/* Sensor monitor interface - not implemented */

gboolean
gkrellm_sys_sensors_get_temperature(gchar *path, gint id,
		gint iodev, gint interface, gfloat *temp)

	{
	return FALSE;
	}

gboolean
gkrellm_sys_sensors_get_fan(gchar *path, gint id,
		gint iodev, gint interface, gfloat *fan)
	{
	return FALSE;
	}

gboolean
gkrellm_sys_sensors_get_voltage(gchar *path, gint id,
		gint iodev, gint interface, gfloat *volt)
	{
	return FALSE;
	}

gboolean
gkrellm_sys_sensors_init(void)
	{
	return FALSE;
	}
