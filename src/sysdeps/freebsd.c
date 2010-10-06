/* GKrellM
|  Copyright (C) 1999-2010 Bill Wilson
|
|  Author:  Bill Wilson    billw@gkrellm.net
|  Latest versions might be found at:  http://gkrellm.net
|
|  freebsd.c code is Copyright (C) Hajimu UMEMOTO <ume@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/sysctl.h>
#include <osreldate.h>

#if __FreeBSD_version < 500000
#include <kvm.h>

kvm_t	*kvmd = NULL;
char	errbuf[_POSIX2_LINE_MAX];
#endif


// extern gboolean force_meminfo_update(void);
#if defined(__i386__) || defined(__amd64__)
static void scan_for_sensors();
#endif


void
gkrellm_sys_main_init(void)
	{
	/* We just ignore error, here.  Even if GKrellM doesn't have
	|  kmem privilege, it runs with available information.
	*/
#if __FreeBSD_version < 500000
	kvmd = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, errbuf);
#endif
	if (setgid(getgid()) != 0)
		{
		fprintf(stderr, "Can't drop setgid privileges.");
		exit(1);
		}
#if defined(__i386__) || defined(__amd64__)
	scan_for_sensors();
#endif
	if (setuid(getuid()) != 0)
		{
		fprintf(stderr, "Can't drop setuid privileges.");
		exit(1);
		}
	}

void
gkrellm_sys_main_cleanup(void)
	{
	}

static int
gk_sysctlnametomib(const char *name, int *mibp, size_t *lenp)
	{
	static int	oid_name2oid[2] = { 0, 3 };

	if (sysctl(oid_name2oid, 2, mibp, lenp,
		   (void *)name, strlen(name)) < 0)
		return -1;
	*lenp /= sizeof(int);
	return 0;
	}

/* ===================================================================== */
/* CPU monitor interface */

#if __FreeBSD_version >= 500101
#include <sys/resource.h>
#else
#include <sys/dkstat.h>
#endif

static int	oid_cp_time[CTL_MAXNAME + 2];
static int	oid_cp_times[CTL_MAXNAME + 2];
static size_t	oid_cp_time_len = sizeof(oid_cp_time);
static size_t	oid_cp_times_len = sizeof(oid_cp_times);
static gint	have_cp_time;
static gint	maxid;
static gint	ncpus;
static u_long	cpumask;
static long	*cp_times;

void
gkrellm_sys_cpu_read_data(void)
	{
	long		cp_time[CPUSTATES], *cp_timep;
	size_t		len;
#if __FreeBSD_version < 500000
	static struct nlist nl[] = {
#define N_CP_TIME	0
		{ "_cp_time" },
		{ "" }
	};
#endif

	if (have_cp_time)
		{
		len = sizeof(cp_time);
		if (sysctl(oid_cp_time, oid_cp_time_len, cp_time, &len,
			   NULL, 0) < 0)
			return;
		}
#if __FreeBSD_version < 500000
	else
		{
		if (kvmd == NULL)
			return;
		if (nl[0].n_type == 0)
			if (kvm_nlist(kvmd, nl) < 0 || nl[0].n_type == 0)
				return;
		if (kvm_read(kvmd, nl[N_CP_TIME].n_value, (char *)&cp_time,
			     sizeof(cp_time)) != sizeof(cp_time))
			return;
		}
#endif

	if (ncpus > 1)
		{
		gint	i, j;

		gkrellm_cpu_assign_composite_data(cp_time[CP_USER],
						  cp_time[CP_NICE],
						  cp_time[CP_SYS],
						  cp_time[CP_IDLE]);

		len = (maxid + 1) * sizeof(long) * CPUSTATES;
		if (sysctl(oid_cp_times, oid_cp_times_len, cp_times, &len,
			   NULL, 0) < 0)
			return;
		for (i = j = 0; i <= maxid; ++i)
			{
			if ((cpumask & (1ul << i)) == 0)
				continue;
			cp_timep = &cp_times[i * CPUSTATES];
			gkrellm_cpu_assign_data(j, cp_timep[CP_USER],
						cp_timep[CP_NICE],
						cp_timep[CP_SYS],
						cp_timep[CP_IDLE]);
			++j;
			}
		}
	else
		gkrellm_cpu_assign_data(0, cp_time[CP_USER], cp_time[CP_NICE],
					cp_time[CP_SYS], cp_time[CP_IDLE]);
	}

gboolean
gkrellm_sys_cpu_init(void)
	{
	gint	have_cp_times = FALSE;
	gint	maxcpus;
	size_t	len;
	long	*p;

	if (gk_sysctlnametomib("kern.cp_time", oid_cp_time,
			       &oid_cp_time_len) >= 0)
		have_cp_time = TRUE;

	len = sizeof(maxcpus);
	if (sysctlbyname("kern.smp.maxcpus", &maxcpus, &len, NULL, 0) >= 0)
		{
		gint	empty, i, j;

		if (gk_sysctlnametomib("kern.cp_times", oid_cp_times,
				       &oid_cp_times_len) < 0)
			goto pcpu_probe_done;
		len = maxcpus * sizeof(long) * CPUSTATES;
		if ((cp_times = malloc(len)) == NULL)
			goto pcpu_probe_done;
		if (sysctl(oid_cp_times, oid_cp_times_len, cp_times, &len,
			   NULL, 0) < 0)
			{
			free(cp_times);
			cp_times = NULL;
			goto pcpu_probe_done;
			}
		maxid = (len / CPUSTATES / sizeof(long)) - 1;
		cpumask = 0;
		ncpus = 0;
		for (i = 0; i <= maxid; ++i)
			{
			empty = 1;
			for (j = 0; empty && j < CPUSTATES; ++j)
				if (cp_times[i * CPUSTATES + j] != 0)
					empty = 0;
			if (!empty)
				{
				cpumask |= (1ul << i);
				++ncpus;
				}
			}
		if ((p  = realloc(cp_times, len)) != NULL)
			cp_times = p;
		have_cp_times = TRUE;
		}

pcpu_probe_done:
	if (!have_cp_times)
		ncpus = 1;
	gkrellm_cpu_set_number_of_cpus(ncpus);
	return TRUE;
	}


/* ===================================================================== */
/* Proc monitor interface */

#if __FreeBSD_version >= 400000
#include <sys/user.h>
#endif

/*
 * This is ugly, but we need PID_MAX, in anyway.  Since 5.0-RELEASE
 * will have vm.stats.vm.v_forks, this will be obsolete in the future.
 */
#if __FreeBSD_version >= 400000
#define	PID_MAX		99999
#else
#include <sys/signalvar.h>
#define KERNEL
#include <sys/proc.h>
#undef KERNEL
#endif

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
	static char	*name = "vm.stats.vm.v_forks";
	static char	*vname = "vm.stats.vm.v_vforks";
	static char	*rname = "vm.stats.vm.v_rforks";

	/* check if vm.stats.vm.v_forks is available */
	if (gk_sysctlnametomib(name, oid_v_forks, &oid_v_forks_len) < 0)
		return TRUE;
	if (gk_sysctlnametomib(vname, oid_v_vforks, &oid_v_vforks_len) < 0)
		return TRUE;
	if (gk_sysctlnametomib(rname, oid_v_rforks, &oid_v_rforks_len) < 0)
		return TRUE;
	++have_v_forks;
	return TRUE;
	}

void
gkrellm_sys_proc_read_data(void)
	{
#if __FreeBSD_version >= 400000
	static int	oid_proc[] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL };
#endif
	double		avenrun;
	static u_int	n_processes, n_forks = 0;
	u_int		n_vforks, n_rforks;
	gint		r_forks, r_vforks, r_rforks;
	size_t		len;
#if __FreeBSD_version < 500000
	static u_int	curpid = -1;
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
#if __FreeBSD_version < 500000
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

#if __FreeBSD_version >= 400000
	if (sysctl(oid_proc, 3, NULL, &len, NULL, 0) >= 0)
		n_processes = len / sizeof(struct kinfo_proc);
#else
	if (kvmd != NULL)
		kvm_getprocs(kvmd, KERN_PROC_ALL, 0, &n_processes);
#endif
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

#if __FreeBSD_version >= 300000
#include <devstat.h>
static struct statinfo	statinfo_cur;
#endif

gchar *
gkrellm_sys_disk_name_from_device(gint device_number, gint unit_number,
			gint *order)
	{
	return NULL;	/* Not implemented */
	}

#if __FreeBSD_version < 300000
gint
gkrellm_sys_disk_order_from_name(const gchar *name)
	{
	return 0;	/* Not implemented */
	}

void
gkrellm_sys_disk_read_data(void)
	{
	int		ndevs;
	long		*cur_dk_xfer;
	int		dn;
	GList		*list;
	static struct nlist nl[] = {
#define N_DK_NDRIVE	0
		{ "_dk_ndrive" },
#define N_DK_XFER	1
		{ "_dk_xfer" },
		{ "" }
	};
   
	if (kvmd == NULL)
		return;
	if (kvm_nlist(kvmd, nl) < 0 || nl[0].n_type == 0)
		return;
	(void) kvm_read(kvmd, nl[N_DK_NDRIVE].n_value,
			(char *)&ndevs, sizeof(ndevs));
	if (ndevs <= 0)
		return;
	if ((cur_dk_xfer = calloc(ndevs, sizeof(long))) == NULL)
		return;
	if (kvm_read(kvmd, nl[N_DK_XFER].n_value, (char *)cur_dk_xfer,
		     ndevs * sizeof(long)) == ndevs * sizeof(long))
		{
		for (dn = 0; dn < ndevs; ++dn)
			gkrellm_disk_assign_data_nth(dn, 0, cur_dk_xfer[dn], FALSE);
		}
	free(cur_dk_xfer);
	}

gboolean
gkrellm_sys_disk_init(void)
	{
	gkrellm_disk_units_are_blocks();
	return TRUE;
	}

#else

#if __FreeBSD_version >= 500107
#define getdevs(stats)	devstat_getdevs(NULL, stats)
#define selectdevs	devstat_selectdevs
#define bytes_read	bytes[DEVSTAT_READ]
#define bytes_written	bytes[DEVSTAT_WRITE]
#endif

gint
gkrellm_sys_disk_order_from_name(const gchar *name)
	{
	return -1;	/* Append as added */
	}

void
gkrellm_sys_disk_read_data(void)
	{
	int			ndevs;
	int			num_selected;
	int			num_selections;
	int			maxshowdevs = 10;
	struct device_selection	*dev_select = NULL;
	long			select_generation;
	int			dn;
	gchar		name[32];

	if (getdevs(&statinfo_cur) < 0)
		return;
	ndevs = statinfo_cur.dinfo->numdevs;
	if (selectdevs(&dev_select, &num_selected, &num_selections,
		       &select_generation, statinfo_cur.dinfo->generation,
		       statinfo_cur.dinfo->devices, ndevs,
		       NULL, 0, NULL, 0,
		       DS_SELECT_ONLY, maxshowdevs, 1) >= 0)
		{
		for (dn = 0; dn < ndevs; ++dn)
			{
			int		di;
			struct devstat	*dev;
//			int		block_size;
//			int		blocks_read, blocks_written;

			di = dev_select[dn].position;
			dev = &statinfo_cur.dinfo->devices[di];
//			block_size = (dev->block_size > 0)
//				   ? dev->block_size : 512;
//			blocks_read = dev->bytes_read / block_size;
//			blocks_written = dev->bytes_written / block_size;

			/* to use gkrellm_disk_assign_data_by_device() would need
			   gkrellm_sys_disk_name_from_device() implemented */
			snprintf(name, sizeof(name), "%s%d", dev->device_name,
					dev->unit_number);
			gkrellm_disk_assign_data_by_name(name,
					dev->bytes_read, dev->bytes_written, FALSE);
			}
		free(dev_select);
		}
	}

gboolean
gkrellm_sys_disk_init(void)
	{
	bzero(&statinfo_cur, sizeof(statinfo_cur));
	statinfo_cur.dinfo = (struct devinfo *)malloc(sizeof(struct devinfo));
	bzero(statinfo_cur.dinfo, sizeof(struct devinfo));
	return TRUE;
	}
#endif


/* ===================================================================== */
/* Inet monitor interface */

#include "../inet.h"

#if defined(INET6)
#include <netinet6/ip6_var.h>
#endif
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#if defined(INET6)
#include <netinet/ip6.h>
#endif
#include <netinet/tcp.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>

#if __FreeBSD_version < 300000
#if defined(INET6)
#include <netinet6/tcp6.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/tcp6_timer.h>
#include <netinet6/tcp6_var.h>
#endif

#include <nlist.h>
#endif


#if __FreeBSD_version < 300000
void
gkrellm_sys_inet_read_tcp_data(void)
	{
	ActiveTCP	tcp;
	gint		tcp_status;
	struct		inpcbhead head;
	struct		inpcb inpcb, *next;
#if defined(INET6)
	struct		in6pcb in6pcb, *prev6, *next6;
	u_long		off = nl[N_TCB6].n_value;
#endif
	static struct nlist nl[] = {
#define	N_TCB		0
		{ "_tcb" },
#if defined(INET6)
#define	N_TCB6		1
		{ "_tcb6" },
#endif
		{ "" },
	};


	if (kvmd == NULL)
		return;
	if (nl[0].n_type == 0)
		if (kvm_nlist(kvmd, nl) < 0 || nl[0].n_type == 0)
			return;
	if (kvm_read(kvmd, nl[N_TCB].n_value, (char *)&head,
		     sizeof(struct inpcbhead)) != sizeof(struct inpcbhead))
		return;

	for (next = head.lh_first; next != NULL;
	     next = inpcb.inp_list.le_next)
		{
		struct tcpcb tcpcb;

		if (kvm_read(kvmd, (u_long)next, (char *)&inpcb,
			     sizeof(inpcb)) != sizeof(inpcb))
			return;
		if (kvm_read(kvmd, (u_long)inpcb.inp_ppcb, (char *)&tcpcb,
			     sizeof(tcpcb)) != sizeof(tcpcb))
			return;
		tcp.local_port = ntohs(inpcb.inp_lport);
		tcp.remote_addr.s_addr = inpcb.inp_faddr.s_addr;
		tcp.remote_port = ntohs(inpcb.inp_fport);
		tcp_status = (tcpcb.t_state == TCPS_ESTABLISHED);
		tcp.family = AF_INET;
		if (tcp_status == TCP_ALIVE)
			gkrellm_inet_log_tcp_port_data(&tcp);
		}
#if defined(INET6)
	if (kvm_read(kvmd, off, (char *)&in6pcb,
		         sizeof(struct in6pcb)) != sizeof(struct in6pcb))
		return;
	prev6 = (struct in6pcb *)off;
	if (in6pcb.in6p_next == (struct in6pcb *)off)
		return;
	while (in6pcb.in6p_next != (struct in6pcb *)off)
		{
		struct tcp6cb tcp6cb;

		next6 = in6pcb.in6p_next;
		if (kvm_read(kvmd, (u_long)next6, (char *)&in6pcb,
			     sizeof(in6pcb)) != sizeof(in6pcb))
			return;
		if (in6pcb.in6p_prev != prev6)
			break;
		if (kvm_read(kvmd, (u_long)in6pcb.in6p_ppcb, (char *)&tcp6cb,
			     sizeof(tcp6cb)) != sizeof(tcp6cb))
			return;
		tcp.local_port = ntohs(in6pcb.in6p_lport);
		memcpy(&tcp.remote_addr6, &in6pcb.in6p_faddr,
		       sizeof(struct in6_addr));
		tcp.remote_port = ntohs(in6pcb.in6p_fport);
		tcp_status = (tcp6cb.t_state == TCPS_ESTABLISHED);
		tcp.family = AF_INET6;
		if (tcp_status == TCP_ALIVE)
			gkrellm_inet_log_tcp_port_data(&tcp);
		prev6 = next6;
		}
#endif
	}
#else
void
gkrellm_sys_inet_read_tcp_data(void)
	{
	static char	*name = "net.inet.tcp.pcblist";
	static int	oid_pcblist[CTL_MAXNAME + 2];
	static size_t	oid_pcblist_len = sizeof(oid_pcblist);
	static gint	initialized = 0;
	ActiveTCP	tcp;
	gint		tcp_status;
	struct xinpgen	*xig, *oxig;
	gchar		*buf;
	size_t		len = 0;

	if (!initialized)
		{
		if (gk_sysctlnametomib(name, oid_pcblist,
				       &oid_pcblist_len) < 0)
			return;
		++initialized;
		}
	if (sysctl(oid_pcblist, oid_pcblist_len, 0, &len, 0, 0) < 0)
		return;
	if ((buf = malloc(len)) == 0)
		return;
	if (sysctl(oid_pcblist, oid_pcblist_len, buf, &len, 0, 0) >= 0)
		{
		oxig = xig = (struct xinpgen *)buf;
		for (xig = (struct xinpgen *)((char *)xig + xig->xig_len);
		     xig->xig_len > sizeof(struct xinpgen);
		     xig = (struct xinpgen *)((char *)xig + xig->xig_len))
			{
			struct tcpcb *tp = &((struct xtcpcb *)xig)->xt_tp;
			struct inpcb *inp = &((struct xtcpcb *)xig)->xt_inp;
			struct xsocket *so = &((struct xtcpcb *)xig)->xt_socket;

			/* Ignore sockets for protocols other than tcp. */
			if (so->xso_protocol != IPPROTO_TCP)
				continue;

			/* Ignore PCBs which were freed during copyout. */
			if (inp->inp_gencnt > oxig->xig_gen)
				continue;

#if defined(INET6)
			if (inp->inp_vflag & INP_IPV4)
				{
				tcp.remote_addr.s_addr = inp->inp_faddr.s_addr;
				tcp.family = AF_INET;
				}
			else if (inp->inp_vflag & INP_IPV6)
				{
					memcpy(&tcp.remote_addr6,
					       &inp->in6p_faddr,
					       sizeof(struct in6_addr));
					tcp.family = AF_INET6;
				}
			else
				continue;
#else
			tcp.remote_addr.s_addr = inp->inp_faddr.s_addr;
			tcp.family = AF_INET;
#endif
			tcp.remote_port = ntohs(inp->inp_fport);
			tcp.local_port = ntohs(inp->inp_lport);
			tcp_status = (tp->t_state == TCPS_ESTABLISHED);
			if (tcp_status == TCP_ALIVE)
				gkrellm_inet_log_tcp_port_data(&tcp);
			}
		}
	free(buf);
	}
#endif


gboolean
gkrellm_sys_inet_init(void)
	{
	return TRUE;
	}


/* ===================================================================== */
/* Memory/Swap monitor interface */

#include <sys/conf.h>
#if __FreeBSD_version < 400000
#include <sys/rlist.h>
#endif
#include <sys/vmmeter.h>
#include <vm/vm_param.h>

#if __FreeBSD_version < 410000
static struct nlist nl_mem[] = {
#define N_CNT		0
	{ "_cnt" },
#if __FreeBSD_version < 400000
#define VM_SWAPLIST	1
	{ "_swaplist" },
#define VM_SWDEVT	2
	{ "_swdevt" },
#define VM_NSWAP	3
	{ "_nswap" },
#define VM_NSWDEV	4
	{ "_nswdev" },
#define VM_DMMAX	5
	{ "_dmmax" },
#if  __FreeBSD_version < 300000
#define N_BUFSPACE	6
	{ "_bufspace" },
#endif
#endif
	{ "" }
};
#endif

static int
swapmode(unsigned long long *retavail, unsigned long long *retfree)
	{
	guint64 used, avail;
#if  __FreeBSD_version >= 400000
	static int psize = -1;
	struct kvm_swap kvmswap;
#if __FreeBSD_version >= 500000
	struct xswdev xsw;
	size_t mibsize, size;
	int mib[16], n;
#endif
#else
	char *header;
	int hlen, nswap, nswdev, dmmax;
	int i, div, nfree, npfree;
	struct swdevt *sw;
	long blocksize, *perdev;
	u_long ptr;
	struct rlist head;
#  if __FreeBSD_version >= 220000
	struct rlisthdr swaplist;
#  else 
	struct rlist *swaplist;
#  endif
	struct rlist *swapptr;
#endif

	/*
	 * Counter for error messages. If we reach the limit,
	 * stop reading information from swap devices and
	 * return zero. This prevent endless 'bad address'
	 * messages.
	 */
	static int warning = 10;

	if (warning <= 0)
		{
		/* a single warning */
		if (!warning)
	    		{
			warning--;
			fprintf(stderr, "Too much errors, stop reading swap devices ...\n");
			}
		return(0);
		}
	warning--;		/* decrease counter, see end of function */

#if __FreeBSD_version >= 400000
#if __FreeBSD_version >= 500000
	mibsize = sizeof mib / sizeof mib[0];
	if (gk_sysctlnametomib("vm.swap_info", mib, &mibsize) == -1)
		return(0);
	kvmswap.ksw_total = 0;
	kvmswap.ksw_used = 0;
	for (n = 0; ; ++n)
		{
		mib[mibsize] = n;
		size = sizeof xsw;
		if (sysctl(mib, mibsize + 1, &xsw, &size, NULL, 0) == -1)
			break;
		kvmswap.ksw_total += xsw.xsw_nblks;
		kvmswap.ksw_used += xsw.xsw_used;
		}
#else
	if (kvmd == NULL)
		return(0);
	if (kvm_getswapinfo(kvmd, &kvmswap, 1, 0) < 0)
		return(0);

#endif
	if (psize < 0)
		psize = getpagesize();
	*retavail = avail = (quad_t)kvmswap.ksw_total * psize;
	used = (quad_t)kvmswap.ksw_used * psize;
	*retfree = avail - used;
#else
	if (kvmd == NULL)
		return(0);
	if (kvm_read(kvmd, nl_mem[VM_NSWAP].n_value,
		     &nswap, sizeof(nswap)) != sizeof(nswap))
		return(0);
	if (!nswap)
		{
		fprintf(stderr, "No swap space available\n");
		return(0);
		}

	if (kvm_read(kvmd, nl_mem[VM_NSWDEV].n_value,
		     &nswdev, sizeof(nswdev)) != sizeof(nswdev))
		return(0);
	if (kvm_read(kvmd, nl_mem[VM_DMMAX].n_value,
		     &dmmax, sizeof(dmmax)) != sizeof(dmmax))
		return(0);
	if (kvm_read(kvmd, nl_mem[VM_SWAPLIST].n_value,
		     &swaplist, sizeof(swaplist)) != sizeof(swaplist))
		return(0);

	if ((sw = (struct swdevt *)malloc(nswdev * sizeof(*sw))) == NULL ||
	    (perdev = (long *)malloc(nswdev * sizeof(*perdev))) == NULL)
		{
		perror("malloc");
		exit(1);
		}
	if (kvm_read(kvmd, nl_mem[VM_SWDEVT].n_value,
		     &ptr, sizeof ptr) != sizeof ptr)
		return(0);
	if (kvm_read(kvmd, ptr,
		     sw, nswdev * sizeof(*sw)) != nswdev * sizeof(*sw))
		return(0);

	/* Count up swap space. */
	nfree = 0;
	memset(perdev, 0, nswdev * sizeof(*perdev));
#if  __FreeBSD_version >= 220000
	swapptr = swaplist.rlh_list;
	while (swapptr)
#else
	while (swaplist)
#endif
		{
		int	top, bottom, next_block;
#if  __FreeBSD_version >= 220000
		if (kvm_read(kvmd, (u_long)swapptr, &head,
			     sizeof(struct rlist)) != sizeof(struct rlist))
			return (0);
#else
		if (kvm_read(kvmd, (u_long)swaplist, &head,
			     sizeof(struct rlist)) != sizeof(struct rlist))
			return (0);
#endif

		top = head.rl_end;
		bottom = head.rl_start;

		nfree += top - bottom + 1;

		/*
		 * Swap space is split up among the configured disks.
		 *
		 * For interleaved swap devices, the first dmmax blocks
		 * of swap space some from the first disk, the next dmmax
		 * blocks from the next, and so on up to nswap blocks.
		 *
		 * The list of free space joins adjacent free blocks,
		 * ignoring device boundries.  If we want to keep track
		 * of this information per device, we'll just have to
		 * extract it ourselves.
		 */

		while (top / dmmax != bottom / dmmax)
			{
			next_block = ((bottom + dmmax) / dmmax);
			perdev[(bottom / dmmax) % nswdev] +=
				next_block * dmmax - bottom;
			bottom = next_block * dmmax;
			}
		perdev[(bottom / dmmax) % nswdev] +=
			top - bottom + 1;

#if  __FreeBSD_version >= 220000
		swapptr = head.rl_next;
#else
		swaplist = head.rl_next;
#endif
		}

	header = getbsize(&hlen, &blocksize);
	div = blocksize / 512;
	avail = npfree = 0;
	for (i = 0; i < nswdev; i++)
		{
		int xsize, xfree;

		/*
		 * Don't report statistics for partitions which have not
		 * yet been activated via swapon(8).
		 */

		xsize = sw[i].sw_nblks;
		xfree = perdev[i];
		used = xsize - xfree;
		npfree++;
		avail += xsize;
		}

	/* 
	 * If only one partition has been set up via swapon(8), we don't
	 * need to bother with totals.
	 */
	*retavail = avail << 9;
	*retfree = nfree << 9;
	used = avail - nfree;
	free(sw); free(perdev);
#endif /* __FreeBSD_version >= 400000 */

	/* increase counter, no errors occurs */
	warning++; 

	return  (int)(((double)used / (double)avail * 100.0) + 0.5);
	}

static int
get_bufspace(guint64 *bufspacep)
	{
#if  __FreeBSD_version < 300000
	u_int	bufspace;

	if (kvm_read(kvmd, nl_mem[N_BUFSPACE].n_value, (char *)&bufspace,
		     sizeof(bufspace)) != sizeof(bufspace))
		return 0;
#else
	static char	*name = "vfs.bufspace";
	static int	oid_bufspace[CTL_MAXNAME + 2];
	static size_t	oid_bufspace_len = sizeof(oid_bufspace);
	static gint	initialized = 0;
	u_int		bufspace;
	size_t		bufspace_len = sizeof(bufspace);

	if (!initialized)
		{
		if (gk_sysctlnametomib(name, oid_bufspace,
				       &oid_bufspace_len) < 0)
			return 0;
		++initialized;
		}

	if (sysctl(oid_bufspace, oid_bufspace_len,
		   &bufspace, &bufspace_len, NULL, 0) < 0)
		return 0;
#endif
	*bufspacep = bufspace;
	return 1;	
	}

#if __FreeBSD_version >= 410000
struct mibtab {
    char	*name;
    int		oid[CTL_MAXNAME + 2];
    size_t	oid_len;
    u_int	value;
    size_t	value_len;
};

static struct mibtab mibs[] = {
#define MIB_V_PAGE_COUNT	0
    { "vm.stats.vm.v_page_count" },
#define MIB_V_FREE_COUNT	1
    { "vm.stats.vm.v_free_count" },
#define MIB_V_WIRE_COUNT	2
    { "vm.stats.vm.v_wire_count" },
#define MIB_V_ACTIVE_COUNT	3
    { "vm.stats.vm.v_active_count" },
#define MIB_V_INACTIVE_COUNT	4
    { "vm.stats.vm.v_inactive_count" },
#define MIB_V_CACHE_COUNT	5
    { "vm.stats.vm.v_cache_count" },
#define MIB_V_SWAPPGSIN		6
    { "vm.stats.vm.v_swappgsin" },
#define MIB_V_SWAPPGSOUT	7
    { "vm.stats.vm.v_swappgsout" },
    { NULL }
};

#define	PROC_MEMINFO_FILE	"/compat/linux/proc/meminfo"
#endif

#ifndef VM_TOTAL
#define VM_TOTAL	VM_METER
#endif

static guint64	swapin,
		swapout;
static unsigned long long	swap_total,
				swap_used;

void
gkrellm_sys_mem_read_data(void)
	{
	static gint	psize, pshift = 0;
	static gint	first_time_done = 0;
	static gint	swappgsin = -1;
	static gint	swappgsout = -1;
	gint		dpagein, dpageout;
	guint64		total, used, free, shared, buffers = 0, cached;
	struct vmtotal	vmt;
	size_t		length_vmt = sizeof(vmt);
	static int	oid_vmt[] = { CTL_VM, VM_TOTAL };
#if __FreeBSD_version >= 410000
	gint		i;
#else
	guint64		x_used;
	struct vmmeter	sum;
#endif

#if 0
	/* mem.c does a force_meminfo_update() before calling this */
	/* Collecting meminfo data is expensive under FreeBSD, so
	|  take extra precautions to minimize reading it.
	*/
	if (!GK.ten_second_tick && !force_meminfo_update())
		return;
#endif

	if (pshift == 0)
		{
		for (psize = getpagesize(); psize > 1; psize >>= 1)
			pshift++;
		}

	shared = 0;
#if __FreeBSD_version >= 410000
	if (!first_time_done)
		{
		for (i = 0; mibs[i].name; ++i)
			{
			mibs[i].oid_len = sizeof(mibs[i].oid);
			if (gk_sysctlnametomib(mibs[i].name, mibs[i].oid,
					       &mibs[i].oid_len) < 0)
				return;
			mibs[i].value_len = sizeof(mibs[i].value);
			}
		}
	for (i = 0; mibs[i].name; ++i)
		if (sysctl(mibs[i].oid, mibs[i].oid_len, &mibs[i].value,
			   &mibs[i].value_len, NULL, 0) < 0)
			return;
	total = (guint64)(mibs[MIB_V_PAGE_COUNT].value) << pshift;
	free = (guint64)(mibs[MIB_V_INACTIVE_COUNT].value +
			 mibs[MIB_V_FREE_COUNT].value) << pshift;
	if (sysctl(oid_vmt, 2, &vmt, &length_vmt, NULL, 0) == 0)
		shared = (guint64)vmt.t_rmshr << pshift;
	get_bufspace(&buffers);
	cached = (guint64)mibs[MIB_V_CACHE_COUNT].value << pshift;
	used = total - free;
	gkrellm_mem_assign_data(total, used, free, shared, buffers, cached);

	if (swappgsin < 0)
		{
		dpagein = 0;
		dpageout = 0;
		}
	else
		{
		dpagein = (mibs[MIB_V_SWAPPGSIN].value - swappgsin) << pshift;
		dpageout = (mibs[MIB_V_SWAPPGSOUT].value - swappgsout) << pshift;
		}
	swappgsin = mibs[MIB_V_SWAPPGSIN].value;
	swappgsout = mibs[MIB_V_SWAPPGSOUT].value;
#else
	if (kvmd == NULL)
		return;
	if (nl_mem[0].n_type == 0)
		if (kvm_nlist(kvmd, nl_mem) < 0 || nl_mem[0].n_type == 0)
			return;
	if (kvm_read(kvmd, nl_mem[N_CNT].n_value, (char *)&sum,
		     sizeof(sum)) != sizeof(sum))
		return;

	total = (sum.v_page_count - sum.v_wire_count) << pshift;
	x_used = (sum.v_active_count + sum.v_inactive_count) << pshift;
	free = sum.v_free_count << pshift;
	if (sysctl(oid_vmt, 2, &vmt, &length_vmt, NULL, 0) == 0)
		shared = vmt.t_rmshr << pshift;
	get_bufspace(&buffers);
	cached = sum.v_cache_count << pshift;
	used = x_used - buffers - cached;
	gkrellm_mem_assign_data(total, used, free, shared, buffers, cached);

	if (swappgsin < 0)
		{
		dpagein = 0;
		dpageout = 0;
		}
	else
		{
		dpagein = (sum.v_swappgsin - swappgsin) << pshift;
		dpageout = (sum.v_swappgsout - swappgsout) << pshift;
		}
	swappgsin = sum.v_swappgsin;
	swappgsout = sum.v_swappgsout;
#endif

	if (dpagein > 0 || dpageout > 0 || first_time_done == 0)
		{
		swapmode(&swap_total, &swap_used);
		swap_used = swap_total - swap_used;
		}
	first_time_done = 1;
	swapin = swappgsin;
	swapout = swappgsout;
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
/* Battery monitor interface */

#if defined(__i386__) || defined(__amd64__)
#if defined(__i386__)
#include <machine/apm_bios.h>
#define	APMDEV		"/dev/apm"

#define	L_NO_BATTERY	0x80
#define	L_ON_LINE		1
#define	L_CHARGING		3
#define L_UNKNOWN		0xFF
#endif

/* following two definitions are taken from sys/dev/acpica/acpiio.h */
#define ACPI_BATT_STAT_CHARGING		0x0002
#define ACPI_BATT_STAT_NOT_PRESENT	0x0007

#define ACPI_ACLINE	0
#define ACPI_BATT_LIFE	1
#define ACPI_BATT_TIME	2
#define ACPI_BATT_STATE	3

void
gkrellm_sys_battery_read_data(void)
	{
	static char	*name[] = { "hw.acpi.acline",
				    "hw.acpi.battery.life",
				    "hw.acpi.battery.time",
				    "hw.acpi.battery.state",
				    NULL };
	static int	oid[CTL_MAXNAME + 2][4];
	static size_t	oid_len[4] = { sizeof(oid[0]), sizeof(oid[1]),
				       sizeof(oid[2]), sizeof(oid[3]) };
	static gboolean	first_time_done = FALSE;
	static gboolean	acpi_enabled = FALSE;
	size_t		size;
	int		acpi_info[4];
	int		i;
#if defined(__i386__)
	int		f, r;
	struct apm_info	info;
	gint		batt_num = 0;
#endif
	gboolean	available, on_line, charging;
	gint		percent, time_left;

	if (!first_time_done)
		{
		first_time_done = TRUE;
#if defined(ACPI_SUPPORTS_MULTIPLE_BATTERIES) || defined(__amd64__)
		/*
		 * XXX: Disable getting battery information via ACPI
		 * to support multiple batteries via APM sim until
		 * ACPI sysctls support multiple batteries.
		 */
		for (i = 0; name[i] != NULL; ++i)
			{
			if (gk_sysctlnametomib(name[i], oid[i],
					       &oid_len[i]) < 0)
				break;
			}
		if (name[i] == NULL)
			acpi_enabled = TRUE;
#endif
		}

	if (acpi_enabled)
		{
			for (i = 0; name[i] != NULL; ++i)
				{
				size = sizeof(acpi_info[i]);
				if (sysctl(oid[i], oid_len[i],
					   &acpi_info[i], &size, NULL, 0) < 0)
					return;
				}
			available = (acpi_info[ACPI_BATT_STATE] != ACPI_BATT_STAT_NOT_PRESENT);
			on_line = acpi_info[ACPI_ACLINE];
			charging = (acpi_info[ACPI_BATT_STATE] == ACPI_BATT_STAT_CHARGING);
			percent = acpi_info[ACPI_BATT_LIFE];
			if (acpi_info[ACPI_BATT_TIME] == 0 && percent > 0)
				time_left = -1;
			else
				time_left = acpi_info[ACPI_BATT_TIME];
			gkrellm_battery_assign_data(
				GKRELLM_BATTERY_COMPOSITE_ID, available,
				on_line, charging, percent, time_left);
			return;
		}

#if defined(__i386__)
	if ((f = open(APMDEV, O_RDONLY)) == -1)
		return;
	if ((r = ioctl(f, APMIO_GETINFO, &info)) == -1) {
		close(f);
		return;
	}

	available = (info.ai_batt_stat != L_UNKNOWN ||
		     info.ai_acline == L_ON_LINE);
	on_line = (info.ai_acline == L_ON_LINE) ? TRUE : FALSE;
	charging = (info.ai_batt_stat == L_CHARGING) ? TRUE : FALSE;
	percent = info.ai_batt_life;
#if defined(APM_GETCAPABILITIES)
	if (info.ai_batt_time == -1 || (info.ai_batt_time == 0 && percent > 0))
		time_left = -1;
	else
		time_left = info.ai_batt_time / 60;
#else
	time_left = -1;
#endif
	gkrellm_battery_assign_data(GKRELLM_BATTERY_COMPOSITE_ID, available,
				    on_line, charging, percent, time_left);

#if defined(APMIO_GETPWSTATUS)
	if (info.ai_infoversion >= 1 && info.ai_batteries != (u_int) -1 &&
	    info.ai_batteries > 1)
		{
		gint i;
		struct apm_pwstatus aps;

		for (i = 0; i < info.ai_batteries; ++i)
			{
			bzero(&aps, sizeof(aps));
			aps.ap_device = PMDV_BATT0 + i;
			if (ioctl(f, APMIO_GETPWSTATUS, &aps) == -1 ||
			    (aps.ap_batt_flag != 255 &&
			     (aps.ap_batt_flag & APM_BATT_NOT_PRESENT)))
				continue;
			available = (aps.ap_batt_stat != L_UNKNOWN ||
				     aps.ap_acline == L_ON_LINE);
			on_line = (aps.ap_acline == L_ON_LINE) ? TRUE : FALSE;
			charging = (aps.ap_batt_stat == L_CHARGING) ?
				TRUE : FALSE;
			percent = aps.ap_batt_life;
			if (aps.ap_batt_time == -1 ||
			    (aps.ap_batt_time == 0 && percent > 0))
				time_left = -1;
			else
				time_left = aps.ap_batt_time / 60;
			gkrellm_battery_assign_data(batt_num++, available,
						    on_line, charging, percent,
						    time_left);
			}
		}
#endif

	close(f);
#endif
	}

gboolean
gkrellm_sys_battery_init(void)
	{
	return TRUE;
	}

#else

void
gkrellm_sys_battery_read_data(void)
	{
	}

gboolean
gkrellm_sys_battery_init(void)
	{
	return FALSE;
	}

#endif


/* ===================================================================== */
/* Sensor monitor interface */

#if defined(__i386__) || defined(__amd64__)

typedef struct
	{
	gchar	*name;
	gfloat	factor;
	gfloat	offset;
	gchar	*vref;
	}
	VoltDefault;

  /* Tables of voltage correction factors and offsets derived from the
  |  compute lines in sensors.conf.  See the README file.
  */
	/* "lm78-*" "lm78-j-*" "lm79-*" "w83781d-*" "sis5595-*" "as99127f-*" */
	/* Values from LM78/LM79 data sheets	*/
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

#include <dirent.h>
#include <machine/cpufunc.h>
#if __FreeBSD_version >= 500042
#include <dev/smbus/smb.h>
#elif __FreeBSD_version >= 300000
#include <machine/smb.h>
#endif

/* Interface types */
#define INTERFACE_IO		0
#define INTERFACE_SMB		1
#define INTERFACE_ACPI		2
#define INTERFACE_CORETEMP	3		/* Already in Celsius */
#define INTERFACE_AMDTEMP	4

/* Addresses to use for /dev/io */
#define WBIO1			0x295
#define WBIO2			0x296

/* LM78/79 addresses */
#define LM78_VOLT(val)		(0x20 + (val))
#define LM78_TEMP		0x27
#define LM78_FAN(val)		(0x28 + (val))
#define LM78_FANDIV		0x47

#define	SENSORS_DIR		"/dev"

#define TZ_ZEROC		2732
#define TZ_KELVTOC(x)		((gfloat)((x) - TZ_ZEROC) / 10.0)


static gint
get_data(int iodev, u_char command, int interface, u_char *ret)
	{
	u_char byte = 0;

	if (interface == INTERFACE_IO)
		{
		outb(WBIO1, command);
		byte = inb(WBIO2);
		}
#if __FreeBSD_version >= 300000
	else if (interface == INTERFACE_SMB)
		{
		struct smbcmd cmd;

		bzero(&cmd, sizeof(cmd));
		cmd.data.byte_ptr = (char *)&byte;
		cmd.slave         = 0x5a;
		cmd.cmd           = command;
		if (ioctl(iodev, SMB_READB, (caddr_t)&cmd) == -1)
			{
			close(iodev);
			return FALSE;
			}
		}
#endif
	else
		{
		return FALSE;
		}
	if (byte == 0xff)
		return FALSE;
	*ret = byte;
	return TRUE;
	}

gboolean
gkrellm_sys_sensors_get_temperature(gchar *path, gint id,
		gint iodev, gint interface, gfloat *temp)

	{
	u_char byte;
	int value;
	size_t size;

	if (interface == MBMON_INTERFACE)
		{
		gkrellm_sys_sensors_mbmon_check(FALSE);
		return gkrellm_sys_sensors_mbmon_get_value(path, temp);
		}

	if (interface == INTERFACE_ACPI || interface == INTERFACE_CORETEMP ||
	    interface == INTERFACE_AMDTEMP)
		{
		size = sizeof(value);
		if (sysctlbyname(path, &value, &size, NULL, 0) < 0)
			return FALSE;
		if (temp)
			*temp = (interface == INTERFACE_CORETEMP) ?
				(gfloat) value : (gfloat) TZ_KELVTOC(value);
		return TRUE;
		}

	if (get_data(iodev, LM78_TEMP, interface, &byte))
		{
		if (temp)
			*temp = (gfloat) byte;
		return TRUE;
		}
	return FALSE;
	}

gboolean
gkrellm_sys_sensors_get_fan(gchar *path, gint id,
		gint iodev, gint interface, gfloat *fan)
	{
	u_char byte;

	if (interface == MBMON_INTERFACE)
		{
		gkrellm_sys_sensors_mbmon_check(FALSE);
		return gkrellm_sys_sensors_mbmon_get_value(path, fan);
		}

	if (get_data(iodev, LM78_FAN(id), interface, &byte))
		{
		if (byte == 0)
			return FALSE;
		if (fan)
			*fan = 1.35E6 / (gfloat) byte;
		return TRUE;
		}
	return FALSE;
	}

gboolean
gkrellm_sys_sensors_get_voltage(gchar *path, gint id,
		gint iodev, gint interface, gfloat *volt)
	{
	u_char byte;

	if (interface == MBMON_INTERFACE)
		{
		gkrellm_sys_sensors_mbmon_check(FALSE);
		return gkrellm_sys_sensors_mbmon_get_value(path, volt);
		}

	if (get_data(iodev, LM78_VOLT(id), interface, &byte))
		{
		if (volt)
			*volt = (gfloat) byte / 64.0;
		return TRUE;
		}
	return FALSE;
	}

struct freebsd_sensor {
	gint type;
	gchar *id_name;
	gint id;
	gint iodev;
	gint inter;
	gfloat factor;
	gfloat offset;
	gchar *default_label;
};

static GList	*freebsd_sensor_list;

gboolean
gkrellm_sys_sensors_init(void)
	{
	gchar		mib_name[256], label[8], buf[BUFSIZ], *fmt;
	gint		interface, id;
	int		oid[CTL_MAXNAME + 2];
	size_t		oid_len, len;
	GList		*list;
	struct freebsd_sensor *sensor;

	/* Do intial daemon reads to get sensors loaded into sensors.c
	*/
	gkrellm_sys_sensors_mbmon_check(TRUE);

	/* ACPI Thermal */
	for (id = 0;; id++)
		{
		snprintf(mib_name, sizeof(mib_name),
			 "hw.acpi.thermal.tz%d.temperature", id);
		oid_len = sizeof(oid);
		if (gk_sysctlnametomib(mib_name, oid, &oid_len) < 0)
			break;
		interface = INTERFACE_ACPI;
		if (!gkrellm_sys_sensors_get_temperature(mib_name, 0, 0,
							 interface, NULL))
			continue;
		snprintf(label, sizeof(label), "tz%d", id);
		gkrellm_sensors_add_sensor(SENSOR_TEMPERATURE, NULL,
					   mib_name, 0, 0,
					   interface, 1.0, 0.0, NULL, label);
		}

	/* Coretemp and Amdtemp */
	for (id = 0;; id++)
		{
		snprintf(mib_name, sizeof(mib_name),
			 "dev.cpu.%d.temperature", id);
		oid_len = sizeof(oid) - sizeof(int) * 2;
		if (gk_sysctlnametomib(mib_name, oid + 2, &oid_len) < 0)
			break;
		oid[0] = 0;
		oid[1] = 4;
		len = sizeof(buf);
		if (sysctl(oid, oid_len + 2, buf, &len, 0, 0) < 0)
			break;
		fmt = (gchar *)(buf + sizeof(u_int));
		interface = (fmt[1] == 'K') ?
			INTERFACE_AMDTEMP : INTERFACE_CORETEMP;
		if (!gkrellm_sys_sensors_get_temperature(mib_name, 0, 0,
							 interface, NULL))
			continue;
		snprintf(label, sizeof(label), "cpu%d", id);
		gkrellm_sensors_add_sensor(SENSOR_TEMPERATURE, NULL,
					   mib_name, 0, 0,
					   interface, 1.0, 0.0, NULL, label);
		}


	if (freebsd_sensor_list)
		{
		for (list = freebsd_sensor_list; list; list = list->next)
			{
			sensor = (struct freebsd_sensor *)list->data;
			gkrellm_sensors_add_sensor(sensor->type, NULL,
					sensor->id_name, sensor->id,
					sensor->iodev, sensor->inter,
					sensor->factor, sensor->offset,
					NULL, sensor->default_label);
			}
		}

	return (TRUE);
	}

static gboolean
sensors_add_sensor(gint type, gchar *id_name, gint id, gint iodev, gint inter,
		   gfloat factor, gfloat offset, gchar *default_label)
{
	struct freebsd_sensor *sensor;

	if ((sensor = g_new0(struct freebsd_sensor, 1)) == NULL)
		return FALSE;
	sensor->type = type;
	sensor->id_name = g_strdup(id_name);
	sensor->id = id;
	sensor->iodev = iodev;
	sensor->inter = inter;
	sensor->factor = factor;
	sensor->offset = offset;
	sensor->default_label = default_label ? g_strdup(default_label) : NULL;
	freebsd_sensor_list = g_list_append(freebsd_sensor_list, sensor);
	return TRUE;
}

static void
scan_for_sensors(void)
	{
	gchar		*chip_dir = SENSORS_DIR;
#if __FreeBSD_version >= 300000
	DIR		*dir;
	struct dirent	*dentry;
#endif
	gchar		temp_file[256], id_name[8];
	gint		iodev = -1, interface = -1, id;
	gint		fandiv[3];
	u_char		byte;
	gboolean	found_sensors = FALSE;

#if __FreeBSD_version >= 300000
	if ((dir = opendir(chip_dir)) != NULL)
		{
		while ((dentry = readdir(dir)) != NULL)
			{
			if (dentry->d_name[0] == '.' || dentry->d_ino <= 0)
				continue;
			if (strncmp(dentry->d_name, "smb", 3) != 0)
				continue;
			snprintf(temp_file, sizeof(temp_file), "%s/%s",
						chip_dir, dentry->d_name);
			if ((iodev = open(temp_file, O_RDWR)) == -1)
				continue;
			snprintf(id_name, sizeof(id_name), "%s%3s", "temp", dentry->d_name + 3);
			interface = INTERFACE_SMB;
			if (!gkrellm_sys_sensors_get_temperature(NULL, 0, iodev,
					interface, NULL))
				{
				close(iodev);
				continue;
				}
			sensors_add_sensor(SENSOR_TEMPERATURE, id_name, 0,
					iodev, interface, 1.0, 0.0, NULL);
			found_sensors = TRUE;
			break;
			}
		closedir(dir);
		}
#endif
	if (!found_sensors)
		{
		snprintf(temp_file, sizeof(temp_file), "%s/%s",
			 chip_dir, "io");
		if ((iodev = open(temp_file, O_RDWR)) == -1)
			return;
		interface = INTERFACE_IO;
		if (!gkrellm_sys_sensors_get_temperature(NULL, 0, iodev,
				interface, NULL))
			{
			close(iodev);
			return;
			}
		sensors_add_sensor(SENSOR_TEMPERATURE, "temp0", 0, iodev,
				interface, 1.0, 0.0, NULL);
		found_sensors = TRUE;
		}
	if (found_sensors)
		{
		if (get_data(iodev, LM78_FANDIV, interface, &byte))
			{
			fandiv[0] = 1 << ((byte & 0x30) >> 4);
			fandiv[1] = 1 << ((byte & 0xc0) >> 6);
			fandiv[2] = 2;
			for (id = 0; id < 3; ++id)
				{
				if (!gkrellm_sys_sensors_get_fan(NULL, id, iodev,
						interface, NULL))
					continue;
				snprintf(id_name, sizeof(id_name), "%s%d", "fan", id);
				sensors_add_sensor(SENSOR_FAN, id_name, id,
						iodev, interface,
						1.0 / (gfloat) fandiv[id], 0.0,
						NULL);
				}
			}
		for (id = 0; id < 7; ++id)
			{
			if (!gkrellm_sys_sensors_get_voltage(NULL, id, iodev,
					interface, NULL))
				continue;
			snprintf(id_name, sizeof(id_name), "%s%d", "in", id);
			sensors_add_sensor(SENSOR_VOLTAGE, id_name, id,
					iodev, interface,
					voltdefault0[id].factor, 0.0,
					voltdefault0[id].name);
			}
		}
	}

#else

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

#endif
