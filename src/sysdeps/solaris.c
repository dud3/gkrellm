/* GKrellM
|  Copyright (C) 1999-2010 Bill Wilson
|
|  Author:  Bill Wilson    billw@gkrellm.net
|  Latest versions might be found at:  http://gkrellm.net
|
|  solaris.c code is Copyright (C) Daisuke Yabuki <dxy@acm.org>
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

#include <kstat.h>
#include <kvm.h>
#include <fcntl.h>

kstat_ctl_t *kc;
kvm_t *kd = NULL;

struct nlist nl[] = { 
    { "mpid" },
    { 0 }
};

extern void solaris_list_harddisks(void);


void
gkrellm_sys_main_init(void)
	{
	/* 
	 * Most of stats (cpu, proc, disk, memory, net and uptime) are 
	 * unavailable if kstat_open() failed. So we just exit in that case.
	 */
	if ((kc = kstat_open()) == NULL) {
		perror("kstat_open");
		exit(1);
	}

	/*
	 * kvm is utilized solely for getting a value for proc.n_forks 
	 * from kernel variable called mpid. Even if kvm_open() fails,
	 * we proceed without it.
	 */
	if ((kd = kvm_open(NULL, NULL, NULL, O_RDONLY, NULL)) != NULL) { 
		kvm_nlist(kd, nl);
	}

        /*
         * a function called by the following requires sys gid privilege.
         * the folowing function should be performed here just for that reason.
         */ 
        solaris_list_harddisks();

        if (setgid(getgid()) != 0) {
		perror("Failed to drop setgid privilege");
		exit(1);
        }
	}

void
gkrellm_sys_main_cleanup(void)
	{
	}


/* ===================================================================== */
/* CPU monitor interface */

#include <kstat.h>
#include <sys/sysinfo.h>

void
gkrellm_sys_cpu_read_data(void)
{
    extern kstat_ctl_t *kc;
    kstat_t *ksp;
    cpu_stat_t cs;

    if (kstat_chain_update(kc) == -1) {
        perror("kstat_chain_update");
        return;
    }

    for (ksp = kc->kc_chain; ksp; ksp = ksp->ks_next) {
        if (strcmp(ksp->ks_module, "cpu_stat")) 
            continue;
        if (kstat_read(kc, ksp, &cs) == -1) {
            perror("kstat_read");
            continue;
        }
		gkrellm_cpu_assign_data(ksp->ks_instance,
				cs.cpu_sysinfo.cpu[CPU_USER],
				cs.cpu_sysinfo.cpu[CPU_WAIT],
				cs.cpu_sysinfo.cpu[CPU_KERNEL],
				cs.cpu_sysinfo.cpu[CPU_IDLE]);

    }
}

/*
 * note: on some SPARC systems, you can monitor temperature of CPUs 
 * with kstat (unix::temperature:[min/max/state/trend...])
 */

gboolean
gkrellm_sys_cpu_init() {
    extern kstat_ctl_t *kc;
    kstat_t *ksp;
	gint	n_cpus = 0;

    if(kstat_chain_update(kc) == -1) {
        perror("kstat_chain_update");
        return FALSE;
    } 

    for (ksp = kc->kc_chain; ksp; ksp = ksp->ks_next) {
        if (strcmp(ksp->ks_module, "cpu_stat")) 
            continue;
        if (kstat_read(kc, ksp, NULL) != -1) {
			gkrellm_cpu_add_instance(ksp->ks_instance);
			++n_cpus;
			}
    }
	gkrellm_cpu_set_number_of_cpus(n_cpus);
	return TRUE;
}


/* ===================================================================== */
/* Proc monitor interface */

#include <utmp.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/loadavg.h>
#include <kstat.h>
#include <fcntl.h>
#include <kvm.h>
#include <sys/sysinfo.h>

void
gkrellm_sys_proc_read_data(void)
{

    double avenrun[LOADAVG_NSTATS], fload = 0;
	guint	n_processes = 0, n_forks = 0; 
    int last_pid;
    extern kstat_ctl_t *kc;
    kstat_t *ksp;
    kstat_named_t *knp;

    extern kvm_t *kd;
    extern struct nlist nl[];

    if (!GK.second_tick) /* Only one read per second */
        return;

    if (getloadavg(avenrun, LOADAVG_NSTATS) > 0)
        fload = avenrun[LOADAVG_1MIN];

    if (kstat_chain_update(kc) == -1) {
        perror("kstat_chain_update");
        return;
    }
    ksp = kstat_lookup(kc, "unix", -1, "system_misc");
    if (ksp && kstat_read(kc, ksp, NULL) >= 0) {
        knp = (kstat_named_t *)kstat_data_lookup(ksp, "nproc");
        if (knp) { 
            n_processes = knp->value.ui32;
        }
    }

    if (kd) {
        if (kvm_kread(kd, nl[0].n_value, (char *)&last_pid, sizeof(int)) != -1)
            n_forks = last_pid;
    } else {
        n_forks = 0;
    }
    /* NOTE: code to get 'n_running' is not implemented (stays untouched).
     * but it wouldn't do any harm since nobody seems to refer to it.
     */
	gkrellm_proc_assign_data(n_processes, 0, n_forks, fload);
}


void
gkrellm_sys_proc_read_users(void)
	{
    static struct utmp *utmpp;
	gint	n_users;

    n_users = 0;
    setutent();
    while ((utmpp = getutent()) != NULL) {
        if (utmpp->ut_type == USER_PROCESS && utmpp->ut_name[0] != '\0')
            n_users++;
    }    
	gkrellm_proc_assign_users(n_users);
}

gboolean
gkrellm_sys_proc_init(void)
    {
	return TRUE;
	}



/* ===================================================================== */
/* Disk monitor interface */

#include <sys/types.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <kstat.h>
#include <libdevinfo.h>
#include <errno.h>
#include <sys/dkio.h>

#define UNIT_SHIFT 3

#define NAME2MAJOR 0
#define MAJOR2NAME 1

typedef struct {
    gint major;
    gchar name[32];
} name_to_major_t;

static gint check_media_type(kstat_t *);
static gint isharddisk(kstat_t *);
void solaris_list_harddisks(void);                   /* called from main.c */

static gint lookup_name_to_major(name_to_major_t *, int);
static gint get_major(gchar *);
static gint get_devname(gint, gchar *);
static gint get_minor(gint);
static gint get_instance(gint); 

typedef struct {
    char name[8];
} probed_harddisk;

GList *hard_disk_list;


gchar *
gkrellm_sys_disk_name_from_device(gint device_number, gint unit_number,
			gint *order)
	{
	return NULL;	/* Disk data by device not implemented in Solaris */
	}

gint
gkrellm_sys_disk_order_from_name(gchar *name)
	{
	return -1;  /* Append as added */
	}

void
gkrellm_sys_disk_read_data(void)
{
    probed_harddisk *drive;
    GList *list;

    extern kstat_ctl_t *kc;
    kstat_t *ksp;
    kstat_io_t kios;

    if (kstat_chain_update(kc) == -1) {
        perror("kstat_chain_update");
        return;
    }
    for (ksp = kc->kc_chain; ksp; ksp = ksp->ks_next) {
	for (list = hard_disk_list; list; list = list->next) {
            drive = (probed_harddisk *)list->data;

            if(strcmp(drive->name, ksp->ks_name))
                continue;

            memset((void *)&kios, 0, sizeof(kstat_io_t));
            kstat_read(kc, ksp, &kios);

	    gkrellm_disk_assign_data_by_name(drive->name,
						kios.nread, kios.nwritten, FALSE);
	}
    }
}

gboolean
gkrellm_sys_disk_init(void)
	{
	return TRUE;
	}


  /* Is this needed any longer? */
static gint
lookup_name_to_major(name_to_major_t *name_to_major, gint type) {
    FILE *fp;
    char line[80];
    char *name, *maj;
    gint name2major, major2name;
    gint majnum;

    name2major = major2name = 0;
    switch (type) {
        case NAME2MAJOR:
            name2major = 1;
            break;
        case MAJOR2NAME:
            major2name = 1;
            break;
        default:
            break;
    }

    if ((fp = fopen("/etc/name_to_major", "r")) == NULL) {
        perror("fopen");
        return -1;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        name = strtok(line, " \t");
        if (name == NULL)
            continue;

        maj = strtok(NULL, "\n");
        if (maj == NULL)
            continue;
        majnum = (gint) atol(maj); 

        if (name2major) {
            if (strcmp(name_to_major->name, name) == 0) {
                name_to_major->major = majnum;
                fclose(fp);
                return 0;
            }
        } else if (major2name) {
            if (name_to_major->major == majnum) {
                strcpy(name_to_major->name, name);
                fclose(fp);
                return 0; 
            }
        }
    }
    fclose(fp);
    return -1;
}

#if 0
  /* Is this needed any longer? */
static gint
get_major(gchar *devname) {
    /* xlation from device name to major (e.g. sd -> 32) */
    name_to_major_t name_to_major;

    strcpy(name_to_major.name, devname);
    if (lookup_name_to_major(&name_to_major, NAME2MAJOR) < 0)
        return -1;
    return name_to_major.major;
}

  /* Is this needed any longer? */
static gint
get_devname(gint major, gchar *devname) {
    /* xlation from major to device name (e.g. 118 -> ssd) */
    name_to_major_t name_to_major;

    name_to_major.major = major;
    if (lookup_name_to_major(&name_to_major, MAJOR2NAME) < 0)
        return -1;
    strcpy(devname, name_to_major.name);
    return 0;
}

  /* Is this needed any longer? */
static gint
get_minor(gint instance) {
    return instance << UNIT_SHIFT;
}

  /* Is this needed any longer? */
static gint
get_instance(gint minor) {
    return minor >> UNIT_SHIFT;
}
#endif

/* 
 * An sd instance could be a cdrom or a harddrive. You can't simply tell,
 * from contents of kstat, which type of device an sd device is 
 * (well, maybe you could, but at least i can't.)
 * It, however, doesn't make much sense to count cdrom read/write as 
 * "Disk" activity. So I'd like to exclude removable media's from 
 * monitoring target. In order to do this, I try to open a physical 
 * device of a corresponding sd instance. If it's succeeded, I assume
 * it's a hard drive. If I get ENXIO or EBUSY, I'll guess it's CDROM.
 * If you come up with a better (simpler or safer) way to tell it's 
 * a removable media or a hard drive, please drop me an e-mail at 
 * Daisuke Yabuki <dxy@acm.org>. 
 * I don't know any other driver which handle both hard drive and 
 * removable media, by the way. I hope it wouldn't do any harm on
 * other type of devices, i.e. ssd, or IDE drivers. 
 */ 
static gint
check_media_type(kstat_t *ksp) {
    gint fd;
    char *phys_path, devices_path[256]; /* or OBP_MAXPATHLEN? */
    di_node_t node;
    static di_node_t root_node = NULL;
#if 0
    /* Not supported on Solaris 7 */
    struct dk_minfo dk;
#else
    int dkRemovable;
#endif

    if (root_node == NULL) {
        if ((root_node = di_init("/", DINFOCPYALL)) == DI_NODE_NIL) {
                perror("di_init");
                return -1;
        }
    }

    node = di_drv_first_node(ksp->ks_module, root_node);
    while (node != DI_NODE_NIL) {
        if (di_instance(node) != ksp->ks_instance) {
            node = di_drv_next_node(node);
            continue;
        }
        if ((phys_path = di_devfs_path(node)) == NULL) {
            perror("di_devfs_path");
            return -1;
        }
        if (sprintf(devices_path, "/devices%s:c,raw", phys_path) <= 0) {
            di_devfs_path_free(phys_path);
            return -1;
        }
        if ((fd = open(devices_path, O_RDONLY)) == -1) {
            if (errno == ENXIO || errno == EBUSY) {
                close(fd);
                di_devfs_path_free(phys_path);
                return 0; /* guess it's removable media */
            } else {
#ifdef DEBUG
                g_message("opening %s\n", devices_path);
                g_message("unexpected errno: %d\n", errno);
                g_message("disabled auto-detection/exclusion of removable media\n");
#endif
                close(fd);
                di_devfs_path_free(phys_path);
                return -1; /* EACCESS (unless setgid sys) or suchlike */
            }
        }
#if 0
	/* Not supported on Solaris 7 */
        if (ioctl(fd, DKIOCGMEDIAINFO, &dk) < 0)
#else
	if (ioctl(fd, DKIOCREMOVABLE, &dkRemovable) < 0)
#endif
	{
            close(fd);
            di_devfs_path_free(phys_path);
            return -1;
        }
#if 0
        if (dk.dki_media_type == DK_FIXED_DISK)
#else
        if (!dkRemovable)
#endif
	{
	   close(fd);
	   di_devfs_path_free(phys_path);
	   return 1;
        }
	return 0;
    }
    return -1; /* shouldn't be reached */
} 

static gint
isharddisk(kstat_t *ksp) {
    if (ksp->ks_type != KSTAT_TYPE_IO) 
        return 0;
    if (strncmp(ksp->ks_class, "disk", 4)) 
        return 0; /* excluding nfs etc. */
    if (!strcmp(ksp->ks_module, "fd"))
        return 0; /* excluding fd */
    if (check_media_type(ksp) == 0)
        return 0; /* guess it's removable media (e.g. CD-ROM, CD-R/W etc) */
    return 1;
}

/* 
 * creating a preliminary list of drives, which should be a complete
 * list of drives available on the system. the list is not supposed to
 * contain nfs, fd, cdrom, cdrw etc.
 */
void
solaris_list_harddisks(void) {
    extern kstat_ctl_t *kc;
    kstat_t *ksp;
    probed_harddisk *drive;

    if (kstat_chain_update(kc) == -1) {
        perror("kstat_chain_update");
        return;
    }
    
    for (ksp = kc->kc_chain; ksp; ksp = ksp->ks_next) {
        if(isharddisk(ksp)) {
             drive = g_new0(probed_harddisk, 1);
             hard_disk_list = g_list_append(hard_disk_list, drive);
             strcpy(drive->name, ksp->ks_name);
        }
    }
}


/* ===================================================================== */
/* Inet monitor interface */

#include "../inet.h"

#include <stropts.h>
#include <inet/mib2.h>
#include <fcntl.h>
#include <sys/tihdr.h>

void
gkrellm_sys_inet_read_tcp_data() {

    ActiveTCP tcp;
    gint tcp_status;

    static int tcpfd = 0;

    mib2_tcpConnEntry_t *tp;
#if defined(INET6)
    mib2_tcp6ConnEntry_t *tp6;
#endif 

    char buf[512];
    int i, flags, getcode, num_ent;
    struct strbuf ctlbuf, databuf;
    struct T_optmgmt_req *tor = (struct T_optmgmt_req *)buf;
    struct T_optmgmt_ack *toa = (struct T_optmgmt_ack *)buf;
    struct T_error_ack   *tea = (struct T_error_ack *)buf;
    struct opthdr        *mibhdr;

    if (tcpfd == 0) {
        if ((tcpfd = open("/dev/tcp", O_RDWR)) == -1) {
            perror("open");
        }
    }

    tor->PRIM_type = T_SVR4_OPTMGMT_REQ;
    tor->OPT_offset = sizeof (struct T_optmgmt_req);
    tor->OPT_length = sizeof (struct opthdr);
    tor->MGMT_flags = T_CURRENT;
    mibhdr = (struct opthdr *)&tor[1];
    mibhdr->level = MIB2_TCP;
    mibhdr->name  = 0;
    mibhdr->len   = 0;

    ctlbuf.buf = buf;
    ctlbuf.len = tor->OPT_offset + tor->OPT_length;
    flags = 0; /* request to be sent in non-priority */

    if (putmsg(tcpfd, &ctlbuf, (struct strbuf *)0, flags) == -1) {
        perror("putmsg");
    }

    mibhdr = (struct opthdr *)&toa[1];
    ctlbuf.maxlen = sizeof (buf);

    /* now receiving response from stream */

    for (;;) {
        flags = 0; /* read any messages available */
        getcode = getmsg(tcpfd, &ctlbuf, (struct strbuf *)0, &flags);

        if (getcode != MOREDATA ||
                 ctlbuf.len < sizeof (struct T_optmgmt_ack) ||
                 toa->PRIM_type != T_OPTMGMT_ACK ||
                 toa->MGMT_flags != T_SUCCESS) {
             break;
        } 

        if (ctlbuf.len >= sizeof (struct T_error_ack) &&
                 tea->PRIM_type == T_ERROR_ACK) {
             perror("ERROR_ACK");
             return;
        }

        if (getcode == 0 &&
                 ctlbuf.len >= sizeof (struct T_optmgmt_ack) &&
                 toa->PRIM_type == T_OPTMGMT_ACK &&
                 toa->MGMT_flags == T_SUCCESS) {
             return;
        } 

        /* prepare for receiving data */
        databuf.maxlen = mibhdr->len;
        databuf.len    = 0;
        databuf.buf    = (char *)malloc((int)mibhdr->len);
        if(!databuf.buf) {
            perror("malloc");
            break;
        }
        flags = 0;

        getcode = getmsg(tcpfd, (struct strbuf *)0, &databuf, &flags);

        if (mibhdr->level == MIB2_TCP && mibhdr->name == MIB2_TCP_13) {
            tp = (mib2_tcpConnEntry_t *)databuf.buf;
            num_ent = mibhdr->len / sizeof(mib2_tcpConnEntry_t);
            for (i = 0; i < num_ent; i++, tp++) {
                if (tp->tcpConnState != MIB2_TCP_established)
                    continue;
                tcp.local_port         = tp->tcpConnLocalPort;
                tcp.remote_addr.s_addr = tp->tcpConnRemAddress;
                tcp.remote_port        = tp->tcpConnRemPort;
                tcp.family             = AF_INET;
                tcp_status = (tp->tcpConnState == MIB2_TCP_established);
                if (tcp_status == TCP_ALIVE)
                    gkrellm_inet_log_tcp_port_data(&tcp);
            }
        }

#if defined(INET6)
        if (mibhdr->level == MIB2_TCP6 && mibhdr->name == MIB2_TCP6_CONN) {
            tp6 = (mib2_tcp6ConnEntry_t *)databuf.buf;
            num_ent = mibhdr->len / sizeof(mib2_tcp6ConnEntry_t);
            for (i = 0; i < num_ent; i++, tp6++) {
                if (tp6->tcp6ConnState != MIB2_TCP_established)
                    continue;
                tcp.local_port          = tp6->tcp6ConnLocalPort;
                tcp.remote_port         = tp6->tcp6ConnRemPort;
                memcpy(&tcp.remote_addr6, &tp6->tcp6ConnRemAddress, 
			sizeof(struct in6_addr));
                tcp.family              = AF_INET6;
                tcp_status = (tp6->tcp6ConnState == MIB2_TCP_established);
                if (tcp_status == TCP_ALIVE)
                    gkrellm_inet_log_tcp_port_data(&tcp);
            }  
        }
#endif /* INET6 */

        free(databuf.buf);
    }

}

gboolean
gkrellm_sys_inet_init(void)
	{
	return TRUE;
	}


/* ===================================================================== */
/* Net monitor interface */

#include <kstat.h>
#include <net/if.h>
#include <sys/sockio.h>

/*
 * FIXME: I haven't tested Net timer (and never will), but I believe it's 
 * not going to work. Name of the lock file is different from one on Linux. 
 * If you need this functionality, feel free to modify my code. 
 */ 

void
gkrellm_sys_net_read_data(void)
	{
	gulong	rx, tx;
    extern kstat_ctl_t *kc;
    kstat_t *ksp;
    kstat_named_t *knp;

	if (kstat_chain_update(kc) == -1) {
			perror("kstat_chain_update");
			return;
	}

    for (ksp = kc->kc_chain; ksp; ksp = ksp->ks_next) {
	    if (!strcmp(ksp->ks_class, "net")) {
		    kstat_read(kc, ksp, NULL);

		    knp = kstat_data_lookup(ksp, "rbytes");
		    if (knp == NULL)
			    continue;
		    rx = knp->value.ui32;

		    knp = kstat_data_lookup(ksp, "obytes");
		    if (knp == NULL)
			    continue;
		    tx = knp->value.ui32;

		    gkrellm_net_assign_data(ksp->ks_name, rx, tx);
	    }
    }

	}

#if 0
/* New way is for above gkrellm_sys_net_read_data() to just assign data
|  for all net interfaces.
*/
void
gkrellm_sys_net_sync(void)
	{
    GList *list;	
    int numifs, numifreqs;
    int i, sockfd;
    size_t bufsize;
    gchar	*buf;
    struct ifreq ifr, *ifrp;
    struct ifconf ifc;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
	return;
    
    if (ioctl(sockfd, SIOCGIFNUM, (char *)&numifs) < 0) {
        perror("SIOCGIFNUM");
        close(sockfd);
        return;
    }

    bufsize = ((size_t)numifs) * sizeof(struct ifreq);
    buf = (char *)malloc(bufsize);
    if (!buf) {
        perror("malloc");
        close(sockfd);
        return;
    }

    ifc.ifc_len = bufsize; 
    ifc.ifc_buf = buf;

    if (ioctl(sockfd, SIOCGIFCONF, (char *)&ifc) < 0) {
        perror("SIOCGIFCONF");
        free(buf);
        close(sockfd);
        return;
    }

#ifdef DEBUG
    g_message("interfaces probed: ");
    for (i=0; i < numifs; i++) {
        g_message("%s ", ifc.ifc_req[i].ifr_name);
    }
    g_message("\n");
#endif
    
    ifrp = ifc.ifc_req;
    numifreqs = ifc.ifc_len / sizeof(struct ifreq);

    for (i = 0; i < numifreqs; i++, ifrp++)
		{
		memset((char *)&ifr, 0, sizeof(ifr));
		strncpy(ifr.ifr_name, ifrp->ifr_name, sizeof(ifr.ifr_name));
		if (!strncmp(ifr.ifr_name, "lo", 2)) 
			continue;
		if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0)
			{
			perror("SIOCGIFFLAGS");
			continue;
			}
		if (ifr.ifr_flags & IFF_UP)
			gkrellm_net_interface_is_up(ifr.ifr_name);
    	}
    free(buf);
    close(sockfd);
	}
#endif

gboolean
gkrellm_sys_net_isdn_online(void)
	{
	return FALSE;
	}

void
gkrellm_sys_net_check_routes(void)
{
}

gboolean
gkrellm_sys_net_init(void)
	{
	gkrellm_net_set_lock_directory("/var/spool/locks");
	gkrellm_net_add_timer_type_ppp("ipdptp0");
	gkrellm_net_add_timer_type_ppp("ppp0");
	return TRUE;
	}



/* ===================================================================== */
/* Memory/Swap monitor interface */

#include <unistd.h>
#include <kstat.h>
#include <sys/stat.h>
#include <sys/swap.h>

static guint64  swap_total, swap_used;

void
gkrellm_sys_mem_read_data() {

    gulong pagesize;
	guint64	total, used = 0, free = 0;
    static gulong pageshift = 0, physpages = 0;
    extern kstat_ctl_t *kc;
    kstat_t *ksp;
    kstat_named_t *knp;  

    struct anoninfo ai;

    if (!GK.second_tick)
        return;

    if (pageshift == 0) {
        for (pagesize = sysconf(_SC_PAGESIZE); pagesize > 1; pagesize >>= 1)
            pageshift++;
    }
    if (physpages == 0) {
        physpages = sysconf(_SC_PHYS_PAGES);
    }

    total = physpages;
    total <<= pageshift;

    ksp = kstat_lookup(kc, "unix", -1, "system_pages");
    if (ksp && kstat_read(kc, ksp, NULL) >= 0) {
        knp = (kstat_named_t *)kstat_data_lookup(ksp, "pagesfree");
        if (knp) {
            free = knp->value.ui32;
            free <<= pageshift;
            used = total - free;
        }
    }
	gkrellm_mem_assign_data(total, used, free, 0, 0, 0);
    if (swapctl(SC_AINFO, &ai) == -1) {
        perror("swapctl");
    }
	swap_total = ai.ani_max;
	swap_total <<= pageshift;

    swap_used  = ai.ani_resv;
    swap_used  <<= pageshift;

    /* NEED TO BE COMPLETED
     * mem.x_used, mem.shared, mem.buffers, mem.cached 
     * swap_chart.page_in, swap_chart.page_out (for swap pages in/out chart) 
     */ 

}

void
gkrellm_sys_swap_read_data(void)
	{
    /* page in/out UNIMPLEMENTED */
	gkrellm_swap_assign_data(swap_total, swap_used, 0, 0);
	}

gboolean
gkrellm_sys_mem_init(void)
	{
	return TRUE;
	}


/* ===================================================================== */
/* FS monitor interface */

#include <sys/mnttab.h>
#include <sys/vfstab.h>
#include <sys/statvfs.h>
#include <sys/cdio.h>

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
gkrellm_sys_fs_get_fstab_list(){
    FILE *fp;
    struct vfstab vfsbuf;

    if ((fp = fopen(VFSTAB, "r")) == NULL)
        return;

    while (getvfsent(fp, &vfsbuf) == 0) {
        if (!vfsbuf.vfs_fstype || strcmp(vfsbuf.vfs_fstype, "ufs"))
            continue;
		gkrellm_fs_add_to_fstab_list(
				vfsbuf.vfs_mountp  ? vfsbuf.vfs_mountp  : "",
				vfsbuf.vfs_special ? vfsbuf.vfs_special : "",				
				vfsbuf.vfs_fstype  ? vfsbuf.vfs_fstype  : "",
				vfsbuf.vfs_mntopts ? vfsbuf.vfs_mntopts : "");
    }

    fclose(fp);
}

void 
gkrellm_sys_fs_get_mounts_list(void){
    FILE *fp;
    struct mnttab mntbuf;

    if ((fp = fopen(MNTTAB, "r")) == NULL)
        return;

    while (getmntent(fp, &mntbuf) == 0) {
        if (strcmp(mntbuf.mnt_fstype, "ufs") && 
                                  strcmp(mntbuf.mnt_fstype, "nfs"))
            continue;
		gkrellm_fs_add_to_mounts_list(
				mntbuf.mnt_mountp  ? mntbuf.mnt_mountp  : "",
				mntbuf.mnt_special ? mntbuf.mnt_special : "",
				mntbuf.mnt_fstype  ? mntbuf.mnt_fstype  : "");
    }
    fclose(fp);
}

void
gkrellm_sys_fs_get_fsusage(gpointer fs, gchar *dir){
    struct statvfs st;

	if (dir && statvfs(dir, &st) == 0) {
		gkrellm_fs_assign_fsusage_data(fs,
					(glong) st.f_blocks, (glong) st.f_bavail,
					(glong) st.f_bfree, (glong) st.f_bsize);
    } else {
		gkrellm_fs_assign_fsusage_data(fs, 0, 0, 0, 0);
    } 
}

static void
eject_solaris_cdrom(gchar *device) {
#if defined(CDROMEJECT)
        gint    d;

		if ((d = open(device, O_RDONLY)) >= 0) {
				ioctl(d, CDROMEJECT);
				close(d);
        }
#endif
}

gboolean
gkrellm_sys_fs_init(void)
	{
	gkrellm_fs_setup_eject(NULL, NULL, eject_solaris_cdrom, NULL);
	return TRUE;
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
/* Uptime monitor interface */

#include <time.h>
#include <kstat.h>

time_t
gkrellm_sys_uptime_read_uptime(void)
	{
	return (time_t) 0;  /* Will calculate using base_uptime */
	}

gboolean
gkrellm_sys_uptime_init(void) {
    time_t      boot, now, base_uptime;

    extern kstat_ctl_t *kc;
    kstat_t *ksp;
    kstat_named_t *knp;

    boot = 0;

    if (kstat_chain_update(kc) == -1) {
        perror("kstat_chain_update");
        return FALSE;
    }
    ksp = kstat_lookup(kc, "unix", -1, "system_misc");
    if (ksp && kstat_read(kc, ksp, NULL) >= 0) {
        knp = (kstat_named_t *)kstat_data_lookup(ksp, "boot_time");
        if (knp) { 
            boot = knp->value.ui32;
        }
    }
    if (time(&now) < 0)
        return FALSE;
    if (now <= boot) 
        return FALSE;

    base_uptime = now - boot;
    base_uptime += 30;
	gkrellm_uptime_set_base_uptime(base_uptime);

    return (base_uptime == (time_t) 0) ? FALSE : TRUE; 
}


/* ===================================================================== */
/* Sensor monitor interface */
/* (nonfunctional) */

gboolean
gkrellm_sys_sensors_init(void)
{
	return FALSE;
}

gboolean
gkrellm_sys_sensors_get_temperature(gchar *name, gint id, gint iodev, gint inter, gfloat *t)
{
	return FALSE;
}

gboolean
gkrellm_sys_sensors_get_fan(gchar *name, gint id, gint iodev, gint inter, gfloat *t)
{
	return FALSE;
}

gboolean
gkrellm_sys_sensors_get_voltage(gchar *name, gint id, gint iodev, gint inter, gfloat *t)
{
	return FALSE;
}

