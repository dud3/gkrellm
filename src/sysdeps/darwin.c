/* GKrellM
|  Copyright (C) 1999-2010 Bill Wilson
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

#ifdef HAVE_KVM_H
#include <kvm.h>
#endif

#include <mach/mach_init.h>
#include <mach/mach_host.h>
#include <mach/vm_map.h>

#ifdef HAVE_KVM_H
kvm_t	*kvmd = NULL;
char	errbuf[_POSIX2_LINE_MAX];
#endif

static void gkrellm_sys_disk_cleanup(void);


void
gkrellm_sys_main_init(void)
	{
#ifdef HAVE_KVM_H
	/* We just ignore error, here.  Even if GKrellM doesn't have
	|  kmem privilege, it runs with available information.
	*/
	kvmd = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, errbuf);
	if (setgid(getgid()) != 0)
		{
		fprintf(stderr, "Can't drop setgid privileges.");
		exit(1);
		}
#endif
	}

void
gkrellm_sys_main_cleanup(void)
	{
    gkrellm_sys_disk_cleanup();
	}

/* ===================================================================== */
/* CPU monitor interface */

static guint		n_cpus;

void
gkrellm_sys_cpu_read_data(void)
	{
		processor_cpu_load_info_data_t *pinfo;
		mach_msg_type_number_t info_count;
		int i;

		if (host_processor_info (mach_host_self (),
						   PROCESSOR_CPU_LOAD_INFO,
						   &n_cpus,
						   (processor_info_array_t*)&pinfo,
						   &info_count) != KERN_SUCCESS) {
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
						  &info_count) != KERN_SUCCESS) {
		return FALSE;
	}
	vm_deallocate (mach_task_self (), (vm_address_t) pinfo, info_count);
	gkrellm_cpu_set_number_of_cpus(n_cpus);
	return TRUE;
	}


/* ===================================================================== */
/* Proc monitor interface */

#include <sys/sysctl.h>
#include <sys/user.h>

#ifdef HAVE_KVM_H
#define	PID_MAX		30000
#include <kvm.h>
#endif
#include <limits.h>
#include <paths.h>
#include <utmpx.h>

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
	static u_int	n_processes, n_forks = 0;
#ifdef HAVE_KVM_H
	static u_int	curpid = -1;
#endif
	u_int		n_vforks, n_rforks;
	gint		r_forks, r_vforks, r_rforks;
	size_t		len;
#ifdef HAVE_KVM_H
	gint		nextpid, nforked;
	static struct nlist nl[] = {
#define N_NEXTPID	0
		{ "_nextpid" },
		{ "" }
	};
#endif

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
#ifdef HAVE_KVM_H
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
#endif
        
	if (sysctl(oid_proc, 3, NULL, &len, NULL, 0) >= 0)
		n_processes = len / sizeof(struct kinfo_proc);

	gkrellm_proc_assign_data(n_processes, 0, n_forks, avenrun);
	}

void
gkrellm_sys_proc_read_users(void)
	{
    struct utmpx  *utmpx_entry;
	gchar          ttybuf[MAXPATHLEN];
	struct stat    sb;
    gint           n_users;

    n_users = 0;
    setutxent();
    while((utmpx_entry = getutxent()))
        {
        if (utmpx_entry->ut_type != USER_PROCESS)
            continue; // skip other entries (reboot, runlevel changes etc.)
        
        (void)snprintf(ttybuf, sizeof(ttybuf), "%s/%s",
            _PATH_DEV, utmpx_entry->ut_line);

        if (stat(ttybuf, &sb))
            continue; // tty of entry missing, no real user

        ++n_users;
        
        }
    endutxent();
	gkrellm_proc_assign_users(n_users);
	}


/* ===================================================================== */
/* Disk monitor interface */

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOBlockStorageDevice.h>
#include <IOKit/storage/IOBlockStorageDriver.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>


typedef struct _GK_DISK
	{
    io_service_t  service;
    io_string_t   path;
	} GK_DARWIN_DISK;

static GPtrArray *s_disk_ptr_array = NULL;


static GK_DARWIN_DISK *
gk_darwin_disk_new()
{
	return g_new0(GK_DARWIN_DISK, 1);
}

static void
gk_darwin_disk_free(GK_DARWIN_DISK *disk)
{
    if (disk->service != MACH_PORT_NULL)
        IOObjectRelease(disk->service);
	g_free(disk);
}

static gboolean
dict_get_int64(CFDictionaryRef dict, CFStringRef key, gint64 *value)
{
    CFNumberRef number_ref;

    number_ref = (CFNumberRef) CFDictionaryGetValue(dict, key);
    if ((NULL == number_ref) ||
        !CFNumberGetValue(number_ref, kCFNumberSInt64Type, value))
    {
        *value = 0;
        return FALSE;
    }
    return TRUE;
}

static gboolean
dict_get_string(CFDictionaryRef dict, CFStringRef key, char *buf, size_t buf_len)
{
    CFStringRef string_ref;

    string_ref = (CFStringRef)CFDictionaryGetValue(dict, key);
    if ((NULL == string_ref) ||
        !CFStringGetCString(string_ref, buf, buf_len, kCFStringEncodingUTF8))
    {
        buf[0] = '\0';
        return FALSE;
    }
    return TRUE;
}

static gboolean
add_storage_device(io_registry_entry_t service)
{
    GK_DARWIN_DISK *disk;
    CFMutableDictionaryRef chars_dict; /* needs release */
    gchar vendor_str[128];
    gchar product_str[128];
    gchar *disk_label;

    gkrellm_debug(DEBUG_SYSDEP, "add_storage_device(); START\n");

    disk = gk_darwin_disk_new();
    disk->service = service;

    if (IORegistryEntryGetPath(service, kIOServicePlane, disk->path)
        != kIOReturnSuccess)
    {
        g_warning("Could not fetch io registry path for disk\n");
        gk_darwin_disk_free(disk);
        return FALSE;
    }

    chars_dict = (CFMutableDictionaryRef)IORegistryEntryCreateCFProperty(
        service, CFSTR(kIOPropertyDeviceCharacteristicsKey),
        kCFAllocatorDefault, 0);

    if (NULL == chars_dict)
    {
        g_warning("Could not fetch properties for disk\n");
        gk_darwin_disk_free(disk);
        return FALSE;
    }

    gkrellm_debug(DEBUG_SYSDEP, "Getting vendor name\n");
    dict_get_string(chars_dict, CFSTR(kIOPropertyVendorNameKey),
        vendor_str, sizeof(vendor_str));
    g_strstrip(vendor_str); // remove leading/trailing whitespace

    gkrellm_debug(DEBUG_SYSDEP, "Getting product name\n");
    dict_get_string(chars_dict, CFSTR(kIOPropertyProductNameKey),
        product_str, sizeof(product_str));
    g_strstrip(product_str); // remove leading/trailing whitespace

    if (strlen(vendor_str) > 0)
        disk_label = g_strdup_printf("%s %s", vendor_str, product_str);
    else
        disk_label = g_strdup(product_str);

    gkrellm_debug(DEBUG_SYSDEP, "Adding disk '%s' with fancy label '%s'\n",
        disk->path, disk_label);

    // Add disk to internal list
    g_ptr_array_add(s_disk_ptr_array, disk);

    // Add disk to gkrellm list
    gkrellm_disk_add_by_name(disk->path, disk_label);

    /* we don't need to store the label, it is only for GUI display */
    g_free(disk_label);
    CFRelease(chars_dict);

    gkrellm_debug(DEBUG_SYSDEP, "add_storage_device(); END\n");
    return TRUE;
}


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
    int i;
    GK_DARWIN_DISK *disk;

	for (i = 0; i < s_disk_ptr_array->len; i++)
        {
        io_registry_entry_t storage_driver; /* needs release */
        CFDictionaryRef storage_driver_stats; /* needs release */
        gint64 bytes_read;
        gint64 bytes_written;

		disk = (GK_DARWIN_DISK *)g_ptr_array_index(s_disk_ptr_array, i);

        //gkrellm_debug(DEBUG_SYSDEP, "Fetching disk stats for '%s'\n", disk->path);

        /* get subitem of device, has to be some kind of IOStorageDriver */
        if (IORegistryEntryGetChildEntry(disk->service, kIOServicePlane,
            &storage_driver) != kIOReturnSuccess)
        {
            gkrellm_debug(DEBUG_SYSDEP,
                "No driver child found in storage device, skipping disk '%s'\n",
                disk->path);
            // skip devices that have no driver child
            continue;
        }

        storage_driver_stats = IORegistryEntryCreateCFProperty(storage_driver,
            CFSTR(kIOBlockStorageDriverStatisticsKey), kCFAllocatorDefault, 0);

        if (NULL == storage_driver_stats)
        {
            gkrellm_debug(DEBUG_SYSDEP,
                "No statistics dict found in storage driver, skipping disk '%s'\n",
                disk->path);
            CFRelease(storage_driver_stats);
            IOObjectRelease(storage_driver);
            continue;
        }

        /* Obtain the number of bytes read/written from the drive statistics */
        if (dict_get_int64(storage_driver_stats,
                CFSTR(kIOBlockStorageDriverStatisticsBytesReadKey), &bytes_read)
            &&
            dict_get_int64(storage_driver_stats,
                CFSTR(kIOBlockStorageDriverStatisticsBytesWrittenKey), &bytes_written)
           )
        {
            gkrellm_disk_assign_data_by_name(disk->path, bytes_read,
                bytes_written, FALSE);
        }
        else
        {
            gkrellm_debug(DEBUG_SYSDEP,
                "could not fetch read/write stats for disk '%s'\n",
                disk->path);
        }

        CFRelease(storage_driver_stats);
        IOObjectRelease(storage_driver);
    } // for()
}

gboolean
gkrellm_sys_disk_init(void)
    {
    /* needs release */
    io_iterator_t iter = MACH_PORT_NULL;

    /* needs release (if add_storage_device() failed) */
    io_service_t service = MACH_PORT_NULL;

    gkrellm_debug(DEBUG_SYSDEP, "gkrellm_sys_disk_init();\n");

    s_disk_ptr_array = g_ptr_array_new();

    if (IOServiceGetMatchingServices(kIOMasterPortDefault,
        IOServiceMatching(kIOBlockStorageDeviceClass),
        &iter) == kIOReturnSuccess)
        {
        while ((service = IOIteratorNext(iter)) != MACH_PORT_NULL)
            {
            if (!add_storage_device(service))
                IOObjectRelease(service);
            }
        IOObjectRelease(iter);
        }

	gkrellm_debug(DEBUG_SYSDEP,
        "gkrellm_sys_disk_init(); Found %u disk(s) for monitoring.\n",
		s_disk_ptr_array->len);

	return (s_disk_ptr_array->len == 0 ? FALSE : TRUE);
    }

static void
gkrellm_sys_disk_cleanup(void)
{
	guint i;

    if (NULL == s_disk_ptr_array)
		return;
    gkrellm_debug(DEBUG_SYSDEP,
        "gkrellm_sys_disk_cleanup() Freeing counters for %u disk(s)\n",
		s_disk_ptr_array->len);
	for (i = 0; i < s_disk_ptr_array->len; i++)
		gk_darwin_disk_free(g_ptr_array_index(s_disk_ptr_array, i));
	g_ptr_array_free(s_disk_ptr_array, TRUE);
}

/* ===================================================================== */
/* Inet monitor interface */

#include "../inet.h"

#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#ifdef INET6
#include <netinet/ip6.h>
#endif /* INET6 */
#include <netinet/in_pcb.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp_var.h>
#include <netinet/igmp_var.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_seq.h>
#define TCPSTATES
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <sys/types.h>

 void
 gkrellm_sys_inet_read_tcp_data(void)
{
	ActiveTCP	tcp;
    const char *mibvar="net.inet.tcp.pcblist";
	char *buf;
	struct tcpcb *tp = NULL;
	struct inpcb *inp;
	struct xinpgen *xig, *oxig;
	struct xsocket *so;
	size_t len=0;
	if (sysctlbyname(mibvar, 0, &len, 0, 0) < 0) {
		if (errno != ENOENT)
			g_warning("sysctl: %s\n", mibvar);
		return;
	}        
	if ((buf = malloc(len)) == 0) {
		g_warning("malloc %lu bytes\n", (u_long)len);
		return;
 	}
	if (sysctlbyname(mibvar, buf, &len, 0, 0) < 0) {
		g_warning("sysctl: %s\n", mibvar);
		free(buf);
		return;
	}
     /*
         * Bail-out to avoid logic error in the loop below when
         * there is in fact no more control block to process
         */
        if (len <= sizeof(struct xinpgen)) {
            free(buf);
            return;
        }
 	oxig = xig = (struct xinpgen *)buf;
	for (xig = (struct xinpgen *)((char *)xig + xig->xig_len);
	     xig->xig_len > sizeof(struct xinpgen);
	     xig = (struct xinpgen *)((char *)xig + xig->xig_len)) {
    	tp = &((struct xtcpcb *)xig)->xt_tp;
	   	inp = &((struct xtcpcb *)xig)->xt_inp;
		so = &((struct xtcpcb *)xig)->xt_socket;
    if (so->xso_protocol != IPPROTO_TCP)
 			continue;
		/* Ignore PCBs which were freed during copyout. */
		if (inp->inp_gencnt > oxig->xig_gen)
			continue;
	if ((inp->inp_vflag & INP_IPV4) == 0
#ifdef INET6
		    && (inp->inp_vflag & INP_IPV6) == 0
#endif /* INET6 */
			)
			continue;
                /*
                 * Local address is not an indication of listening socket or
                 * server sockey but just rather the socket has been bound.
                 * That why many UDP sockets were not displayed in the original code.
                 */
                if (tp->t_state <= TCPS_LISTEN){
                    continue;
                    }
			if (inp->inp_vflag & INP_IPV4) {
			     tcp.local_port=ntohs(inp->inp_lport);
			     tcp.remote_addr.s_addr=(uint32_t)inp->inp_faddr.s_addr;
			     tcp.remote_port=ntohs(inp->inp_fport);
			     tcp.family=AF_INET;
			     gkrellm_inet_log_tcp_port_data(&tcp);
            }
#ifdef INET6
			else if (inp->inp_vflag & INP_IPV6) {
			     tcp.local_port=ntohs(inp->inp_lport);
    			 memcpy(&(tcp.remote_addr6),&(inp->in6p_faddr),sizeof(struct in6_addr));
			     tcp.remote_port=ntohs(inp->inp_fport);
			     tcp.family=AF_INET6;
			     gkrellm_inet_log_tcp_port_data(&tcp);
			} /* else nothing printed now */
#endif /* INET6 */
}  
free(buf);
}
 
 gboolean
 gkrellm_sys_inet_init(void)
 	{
	return TRUE;
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
	
	used = (guint64)(vm_info.active_count) << pshift;
	free = (guint64)vm_info.free_count << pshift;	
	total = (guint64)(vm_info.active_count + vm_info.inactive_count + vm_info.free_count + vm_info.wire_count) << pshift;
	/* Don't know how to get cached or buffers. */
	buffers =  (guint64) (vm_info.wire_count) << pshift;
	cached = (guint64) (vm_info.inactive_count) << pshift;
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
