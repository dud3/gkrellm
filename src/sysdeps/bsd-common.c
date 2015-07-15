/* GKrellM
|  Copyright (C) 1999-2014 Bill Wilson
|
|  Author:  Bill Wilson    billw@gkrellm.net
|  Latest versions might be found at:  http://gkrellm.net
|
|  bsd-common.c code is Copyright (C):
|            Hajimu UMEMOTO <ume@FreeBSD.org>
|            Anthony Mallet <anthony.mallet@useless-ficus.net>
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

#if !(defined(__FreeBSD__) && __FreeBSD_version < 410000) && \
    !(defined(__NetBSD__) && __NetBSD_version < 105000000) && \
    !(defined(__OpenBSD__) && OpenBSD < 200006) && \
    !defined(__APPLE__)
#define HAVE_GETIFADDRS	1
#endif

#if defined(HAVE_GETIFADDRS)

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <ifaddrs.h>

void
gkrellm_sys_net_read_data(void)
	{
	struct ifaddrs		*ifap, *ifa;
	struct if_data		*ifd;

	if (getifaddrs(&ifap) < 0)
		return;

	for (ifa = ifap; ifa; ifa = ifa->ifa_next)
		{
		if (ifa->ifa_flags & IFF_UP)
			{
			if (ifa->ifa_addr->sa_family != AF_LINK)
				continue;
			ifd = (struct if_data *)ifa->ifa_data;
			gkrellm_net_assign_data(ifa->ifa_name,
					ifd->ifi_ibytes, ifd->ifi_obytes);
			}
		}

	freeifaddrs(ifap);
	}

#else /* HAVE_GETIFADDRS */

#include <sys/sysctl.h>
#include <sys/socket.h> // For PF_ROUTE, etc
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>


static int	mib_net[] = { CTL_NET, PF_ROUTE, 0, 0, NET_RT_IFLIST, 0 };
static char	*buf;
static int	alloc;

void
gkrellm_sys_net_read_data(void)
	{
	struct if_msghdr	*ifm, *nextifm;
	struct sockaddr_dl	*sdl;
	char			*lim, *next;
	size_t			needed;
	gchar			s[32];

	if (sysctl(mib_net, 6, NULL, &needed, NULL, 0) < 0)
		return;
	if (alloc < needed)
		{
		if (buf != NULL)
			free(buf);
		buf = malloc(needed);
		if (buf == NULL)
			return;
		alloc = needed;
		}

	if (sysctl(mib_net, 6, buf, &needed, NULL, 0) < 0)
		return;
	lim = buf + needed;

	next = buf;
	while (next < lim)
		{
		ifm = (struct if_msghdr *)next;
		if (ifm->ifm_type != RTM_IFINFO)
			return;
		next += ifm->ifm_msglen;

		while (next < lim)
			{
			nextifm = (struct if_msghdr *)next;
			if (nextifm->ifm_type != RTM_NEWADDR)
				break;
			next += nextifm->ifm_msglen;
			}

		if (ifm->ifm_flags & IFF_UP)
			{
			sdl = (struct sockaddr_dl *)(ifm + 1);
			if (sdl->sdl_family != AF_LINK)
				continue;
			strncpy(s, sdl->sdl_data, sdl->sdl_nlen);
			s[sdl->sdl_nlen] = '\0';
			gkrellm_net_assign_data(s,
					ifm->ifm_data.ifi_ibytes, ifm->ifm_data.ifi_obytes);
			}
		}
	}

#endif /* HAVE_GETIFADDRS */

  /* This would be needed only if net up (or routed) state is available in
  |  a different way than for reading net stats.
  */
void
gkrellm_sys_net_check_routes(void)
	{
	return;
	}

gboolean
gkrellm_sys_net_isdn_online(void)
	{
	return FALSE;
	}

gboolean
gkrellm_sys_net_init(void)
	{
	gkrellm_net_set_lock_directory("/var/spool/lock");
	gkrellm_net_add_timer_type_ppp("tun0");
	gkrellm_net_add_timer_type_ppp("ppp0");
	return TRUE;
	}


/* ===================================================================== */
/* FS monitor interface */

#include <sys/mount.h>
#if !defined(__APPLE__)
#include <sys/cdio.h>
#endif
#include <sys/wait.h>

#if defined(__FreeBSD__)
#include <osreldate.h>
#if __FreeBSD_version < 300000
static char	*mnttype[] = INITMOUNTNAMES;
#endif
#endif

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
	gchar			dev[65], dir[129], type[65], opt[129];

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
	gchar		*s, *dev, *dir, *type;
#if (defined(__NetBSD__) && __NetBSD_Version__ >= 299000900) /* NetBSD 2.99.9 */
	struct statvfs	*mntbuf;
#else
	struct statfs	*mntbuf;
#endif
	gint		mntsize, i;

	if ((mntsize = getmntinfo(&mntbuf, MNT_NOWAIT)) == 0)
		return;
	for (i = 0; i < mntsize; i++)
		{
#if defined(__FreeBSD__) && __FreeBSD_version < 300000
		type = mnttype[mntbuf[i].f_type];
#else
		type = mntbuf[i].f_fstypename;
#endif
		dir = mntbuf[i].f_mntonname;
		dev = mntbuf[i].f_mntfromname;
		/* Strip trailing / from the directory.
		*/
		s = strrchr(dir, (int) '/');
		if (s && s != dir && *(s+1) == '\0')
			*s = '\0';
		gkrellm_fs_add_to_mounts_list(dir, dev, type);
		}
	}

void
gkrellm_sys_fs_get_fsusage(gpointer fs, gchar *dir)
	{
#if (defined(__NetBSD__) && __NetBSD_Version__ >= 299000900) /* NetBSD 2.99.9 */
	struct statvfs	st;
#else
	struct statfs	st;
#endif

#if (defined(__NetBSD__) && __NetBSD_Version__ >= 299000900) /* NetBSD 2.99.9 */
	if (!statvfs(dir, &st))
#else
	if (!statfs(dir, &st))
#endif
		gkrellm_fs_assign_fsusage_data(fs,
					(gint64) st.f_blocks, (gint64) st.f_bavail,
#if (defined(__NetBSD__) && __NetBSD_Version__ >= 299000900) /* NetBSD 2.99.9 */
					(gint64) st.f_bfree, (gint64) st.f_frsize);
#else
					(gint64) st.f_bfree, (gint64) st.f_bsize);
#endif
	else
		gkrellm_fs_assign_fsusage_data(fs, 0, 0, 0, 0);
	}


#if defined(__APPLE__)
#include <IOKit/storage/IOMediaBSDClient.h>

static void
eject_darwin_cdrom(gchar *device)
	{
    gint    d;
    
    if ((d = open(device, O_RDONLY)) >= 0)
            {
            ioctl(d, DKIOCEJECT);
            close(d);
            }
	}
#else	/* FreeBSD || NetBSD || OpenBSD */
static void
eject_bsd_cdrom(gchar *device)
	{
	gint	d;

	if ((d = open(device, O_RDONLY)) >= 0)
		{
		(void) ioctl(d, CDIOCALLOW);
		ioctl(d, CDIOCEJECT);
		close(d);
		}
	}
#endif


#if defined(__APPLE__)
gboolean
gkrellm_sys_fs_init(void)
	{
	gchar	*eject_command = NULL,
			*close_command = NULL;

#if defined(WEXITSTATUS)
	gint    n;

	n = system("hdiutil info > /dev/null 2>&1");
	if (WEXITSTATUS(n) == 0)
		{
        eject_command = "hdiutil eject %s";
        close_command = "disktool -o %s";
		}
#endif
	gkrellm_fs_setup_eject(eject_command, close_command,
				eject_darwin_cdrom, NULL);
	return TRUE;
	}
#endif

#if defined(__FreeBSD__) || defined(__DragonFly__)
gboolean
gkrellm_sys_fs_init(void)
	{
	gchar	*eject_command = NULL,
			*close_command = NULL;

#if defined(WEXITSTATUS)
	gint	n;

	n = system("cdcontrol quit > /dev/null 2>&1");
	if (WEXITSTATUS(n) == 0)
		{
		eject_command = "cdcontrol -f %s eject";
		close_command = "cdcontrol -f %s close";
		}
#endif
	gkrellm_fs_setup_eject(eject_command, close_command,
				eject_bsd_cdrom, NULL);
	return TRUE;
	}
#endif	/* __FreeBSD__ */


#if defined(__NetBSD__)
gboolean
gkrellm_sys_fs_init(void)
	{
	gchar	*eject_command = NULL,
			*close_command = NULL;

#if defined(WEXITSTATUS)
	gint	n;

	n = system("eject -n > /dev/null 2>&1");
	if (WEXITSTATUS(n) == 0)
		{
		eject_command = "eject %s";
		close_command = "eject -l %s";
		}
#endif
	gkrellm_fs_setup_eject(eject_command, close_command,
				eject_bsd_cdrom, NULL);
	return TRUE;
	}
#endif /* __NetBSD__ */


#if defined(__OpenBSD__)
gboolean
gkrellm_sys_fs_init(void)
	{
	gkrellm_fs_setup_eject(NULL, NULL, eject_bsd_cdrom, NULL);
	return TRUE;
	}
#endif /* __OpenBSD__ */


/* ===================================================================== */
/* Uptime monitor interface */

#include <sys/sysctl.h>

time_t
gkrellm_sys_uptime_read_uptime(void)
	{
	return (time_t) 0;	/* Will calculate using base_uptime */
	}

gboolean
gkrellm_sys_uptime_init(void)
	{
	static int		mib[] = { CTL_KERN, KERN_BOOTTIME };
	struct timeval		boottime;
	size_t			size = sizeof(boottime);
	time_t			base_uptime, now;

	base_uptime = (time_t) 0;
	if (sysctl(mib, 2, &boottime, &size, NULL, 0) != -1 &&
	    boottime.tv_sec != 0)
		{
		(void)time(&now);
		base_uptime = now - boottime.tv_sec;
		base_uptime += 30;
		}
	gkrellm_uptime_set_base_uptime(base_uptime);

	return (base_uptime == (time_t) 0) ? FALSE : TRUE;
	}

