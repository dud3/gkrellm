/* GKrellM
|  Copyright (C) 1999-2009 Bill Wilson
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


#include <limits.h>
#include <errno.h>
#include <locale.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

#include <inttypes.h>


static gboolean		need_locale_fix,
					have_diskstats,
					have_partition_stats,
					have_sysfs,
					have_sysfs_stats,
					have_sysfs_sensors;

static gchar		locale_decimal_point;


static gboolean		kernel_2_4,
					kernel_2_6;

static gint			os_major,
					os_minor,
					os_rev;

static gboolean
os_release(gint major, gint minor, gint rev)
	{
	if (   os_major > major
		|| (os_major == major && os_minor > minor)
		|| (os_major == os_major && os_minor == minor && os_rev >= rev)
	   )
		return TRUE;
	return FALSE;
	}


void
gkrellm_sys_main_init(void)
	{
	FILE	*f;
	gchar	buf[1024];

	if ((f = fopen("/proc/sys/kernel/osrelease", "r")) != NULL)
		{
		if (fgets(buf, sizeof(buf), f) != NULL)
			sscanf(buf, "%d.%d.%d", &os_major, &os_minor, &os_rev);
		fclose(f);
		kernel_2_4 = os_release(2, 4, 0);
		kernel_2_6 = os_release(2, 6, 0);
		}

	/* Various stats are in sysfs since 2.5.47, but it may not be mounted.
	*/
	if ((f = fopen("/proc/mounts", "r")) != NULL)
		{
		while (fgets(buf, sizeof(buf), f) != NULL)
			if (strstr(buf, "sysfs"))
				{
				have_sysfs = TRUE;
				break;
				}
		fclose(f);
		}
	}

void
gkrellm_sys_main_cleanup(void)
	{
	}


  /* Linux /proc always reports floats with '.' decimal points, but sscanf()
  |  for some locales needs commas in place of periods.  So, if current
  |  locale doesn't use periods, must insert the correct decimal point char.
  */
static void
locale_fix(gchar *buf)
	{
	gchar	*s;

	for (s = buf; *s; ++s)
		if (*s == '.')
			*s = locale_decimal_point;
	}


/* ===================================================================== */
/* CPU, disk, and swap monitor interfaces might all get data from /proc/stat
|  (depending on kernel version) so they will share reading of /proc/stat.
*/

#define	PROC_STAT_FILE	"/proc/stat"

static gint		n_cpus;
static gulong	swapin,
				swapout;

  /* CPU, and Disk monitors call this in their update routine. 
  |  Whoever calls it first will read the data for everyone.
  |
  | /proc/stat has cpu entries like:
  |		cpu		total_user	total_nice	total_sys	total_idle
  |		cpu0	cpu0_user	cpu0_nice	cpu0_sys	cpu0_idle
  |			...
  |		cpuN	cpuN_user	cpuN_nice	cpuN_sys	cpuN_idle
  |  where ticks for cpu are jiffies * smp_num_cpus
  |  and ticks for cpu[i] are jiffies (1/CLK_TCK)
  */
static void
linux_read_proc_stat(void)
	{
	static FILE	*f;
	gint		n, i, ncpu;
	gchar		*s, buf[1024];
	gulong		rblk[4], wblk[4];
	guint64		user, nice, sys, idle, iowait;
	static gint	data_read_tick	= -1;

	n = gkrellm_get_timer_ticks();
	if (data_read_tick == n)
		return;		/* Just one read per tick (multiple monitors call this) */

	if (!f && (f = fopen(PROC_STAT_FILE, "r")) == NULL)
		return;

	data_read_tick = n;
	gkrellm_disk_reset_composite();

	ncpu = 0;
	while ((fgets(buf, sizeof(buf), f)) != NULL)
		{
		if (buf[0] == 'c' && buf[1] == 'p')
			{
			n = sscanf(buf,
				"%*s %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64,
						&user, &nice, &sys, &idle, &iowait);

			if (n == 5)			/* iowait is new in kernel 2.5	*/
				idle += iowait;

			if (n_cpus > 1)
				{
				if (ncpu == 0)
					gkrellm_cpu_assign_composite_data(user, nice, sys, idle);
				else
					gkrellm_cpu_assign_data(ncpu - 1, user, nice, sys, idle);
				}
			/* If have cpu and cpu0 on single cpu machines, won't use cpu0.
			*/
			else if (ncpu == 0)
				gkrellm_cpu_assign_data(0, user, nice, sys, idle);
			++ncpu;
			continue;
			}
		if (!kernel_2_6 && !strncmp("swap", buf, 4))
			{
			sscanf(buf + 5, "%lu %lu", &swapin, &swapout);
			continue;
			}
		if (buf[0] != 'd')
			continue;

		if (!strncmp("disk_rblk", buf, 9))
			{
			s = buf + 10;
			for (i = 0; i < 4; ++i)
				rblk[i] = strtoul(s, &s, 0);
			}
		else if (!strncmp("disk_wblk", buf, 9))
			{
			s = buf + 10;
			for (i = 0; i < 4; ++ i)
				{
				wblk[i] = strtoul(s, &s, 0);
				gkrellm_disk_assign_data_nth(i,
							512 * rblk[i], 512 * wblk[i], FALSE);
				}
			}
		else if (!strncmp("disk_io:", buf, 8))		/* Kernel 2.4 only	*/
			{
			gint	major, i_disk;
			gulong	rblk, wblk, rb1, rb2, wb1, wb2;

			s = strtok(buf + 9, " \t\n");
			while (s)
				{
				/* disk_io lines in 2.4.x kernels have had 2 formats */
				n = sscanf(s, "(%d,%d):(%*d,%lu,%lu,%lu,%lu)",
						&major, &i_disk, &rb1, &rb2, &wb1, &wb2);
				if (n == 6)	/* patched as of 2.4.0-test1-ac9 */
					{		/* (major,disk):(total_io,rio,rblk,wio,wblk) */
					rblk = rb2;
					wblk = wb2;
					}
				else	/* 2.3.99-pre8 to 2.4.0-testX */
					{		/* (major,disk):(rio,rblk,wio,wblk) */
					rblk = rb1;
					wblk = wb1;
					}
				/* floppys and CDroms don't show up in /proc/partitions.
				*/
				if (have_partition_stats)
					{
					gchar	name[32];

					name[0] = '\0';
					if (major == 2)
						sprintf(name, "fd%d", i_disk);
					else if (major == 11)
						sprintf(name, "scd%d", i_disk);
					if (name[0])
						gkrellm_disk_assign_data_by_name(name,
								512 * rblk, 512 * wblk, FALSE);
					}
				else
					{
					gkrellm_disk_assign_data_by_device(major, i_disk,
								512 * rblk, 512 * wblk,
								(major == 9) ? TRUE : FALSE);
					}
				s = strtok(NULL, " \t\n");
				}
			}
		}
	rewind(f);
	}


/* ===================================================================== */
/* CPU monitor interface */

void
gkrellm_sys_cpu_read_data(void)
	{
	/* One routine reads cpu, disk, and swap data.  All three monitors will
	| call it, but only the first call per timer tick will do the work.
	*/
	linux_read_proc_stat();
	}

gboolean
gkrellm_sys_cpu_init(void)
	{
	FILE	*f;
	gchar	buf[1024];

	if ((f = fopen(PROC_STAT_FILE, "r")) == NULL)
		return FALSE;

	while (fgets(buf, sizeof(buf), f))
		{
		if (strncmp(buf, "cpu", 3) != 0)
			continue;
		++n_cpus;
		}
	fclose(f);

	/* If multiple CPUs, the first one will be a composite.  Report only real.
	*/
	if (n_cpus > 1)
		--n_cpus;
	gkrellm_cpu_set_number_of_cpus(n_cpus);
	return TRUE;
	}


/* ===================================================================== */
/* Disk monitor interface */

#define	PROC_PARTITIONS_FILE	"/proc/partitions"
#define	PROC_DISKSTATS_FILE		"/proc/diskstats"

#include <linux/major.h>
#if ! defined (SCSI_DISK0_MAJOR)
#define SCSI_DISK0_MAJOR	8
#endif
#if ! defined (MD_MAJOR)
#define MD_MAJOR	9
#endif
#if !defined(DM_MAJOR)
#define DM_MAJOR 254
#endif

#if !defined(IDE4_MAJOR)
#define IDE4_MAJOR	56
#endif
#if !defined(IDE5_MAJOR)
#define IDE5_MAJOR	57
#endif
#if !defined(IDE6_MAJOR)
#define IDE6_MAJOR	88
#endif
#if !defined(IDE7_MAJOR)
#define IDE7_MAJOR	89
#endif
#if !defined(IDE8_MAJOR)
#define IDE8_MAJOR	90
#endif
#if !defined(IDE9_MAJOR)
#define IDE9_MAJOR	91
#endif
#if !defined(DAC960_MAJOR)
#define DAC960_MAJOR	48
#endif
#if !defined(COMPAQ_SMART2_MAJOR)
#define COMPAQ_SMART2_MAJOR	72
#endif
#if !defined(COMPAQ_CISS_MAJOR)
#define COMPAQ_CISS_MAJOR	104
#endif
#if !defined(LVM_BLK_MAJOR)
#define LVM_BLK_MAJOR 58
#endif
#if !defined(NBD_MAJOR)
#define NBD_MAJOR 43
#endif
#if !defined(I2O_MAJOR)
#define I2O_MAJOR 80
#endif


struct _disk_name_map
	{
	gchar	*name;
	gint	major;
	gint	minor_mod;
	gchar	suffix_base;
	};

  /* Disk charts will appear in GKrellM in the same order as this table.
  */
static struct _disk_name_map
	disk_name_map[] =
	{
	{"hd",	IDE0_MAJOR,					64,	'a' },	/* 3:  hda, hdb */
	{"hd",	IDE1_MAJOR,					64,	'c' },	/* 22: hdc, hdd */
	{"hd",	IDE2_MAJOR,					64,	'e' },	/* 33: hde, hdf */
	{"hd",	IDE3_MAJOR,					64,	'g' },	/* 34: hdg, hdh */
	{"hd",	IDE4_MAJOR,					64,	'i' },	/* 56: hdi, hdj */
	{"hd",	IDE5_MAJOR,					64,	'k' },	/* 57: hdk, hdl */
	{"hd",	IDE6_MAJOR,					64,	'm' },	/* 88: hdm, hdn */
	{"hd",	IDE7_MAJOR,					64,	'o' },	/* 89: hdo, hdp */
	{"hd",	IDE8_MAJOR,					64,	'q' },	/* 90: hdq, hdr */
	{"hd",	IDE9_MAJOR,					64,	's' },	/* 91: hds, hdt */
	{"sd",	SCSI_DISK0_MAJOR,			16,	'a' },	/* 8:  sda-sdh */
	{"sg",	SCSI_GENERIC_MAJOR,			16,	'0' },	/* 21: sg0-sg16 */
	{"scd",	SCSI_CDROM_MAJOR,			16,	'0' },	/* 11: scd0-scd16 */
	{"sr",	SCSI_CDROM_MAJOR,			16,	'0' },	/* 11: sr0-sr16 */
	{"md",	MD_MAJOR,					0,	'0' },	/* 9:  md0-md3 */
	{"i2o/hd", I2O_MAJOR,				16,	'a' },	/* 80:  i2o/hd*    */

	{"c0d",	DAC960_MAJOR,				32,	'0' },	/* 48:  c0d0-c0d31 */
	{"c1d",	DAC960_MAJOR + 1,			32,	'0' },	/* 49:  c1d0-c1d31 */
	{"c2d",	DAC960_MAJOR + 2,			32,	'0' },	/* 50:  c2d0-c2d31 */
	{"c3d",	DAC960_MAJOR + 3,			32,	'0' },	/* 51:  c3d0-c3d31 */
	{"c4d",	DAC960_MAJOR + 4,			32,	'0' },	/* 52:  c4d0-c4d31 */
	{"c5d",	DAC960_MAJOR + 5,			32,	'0' },	/* 53:  c5d0-c5d31 */
	{"c6d",	DAC960_MAJOR + 6,			32,	'0' },	/* 54:  c6d0-c6d31 */
	{"c7d",	DAC960_MAJOR + 7,			32,	'0' },	/* 55:  c7d0-c7d31 */

	{"cs0d", COMPAQ_SMART2_MAJOR,		16,	'0' },	/* 72:  c0d0-c0d15 */
	{"cs1d", COMPAQ_SMART2_MAJOR + 1,	16,	'0' },	/* 73:  c1d0-c1d15 */
	{"cs2d", COMPAQ_SMART2_MAJOR + 2,	16,	'0' },	/* 74:  c2d0-c2d15 */
	{"cs3d", COMPAQ_SMART2_MAJOR + 3,	16,	'0' },	/* 75:  c3d0-c3d15 */
	{"cs4d", COMPAQ_SMART2_MAJOR + 4,	16,	'0' },	/* 76:  c4d0-c4d15 */
	{"cs5d", COMPAQ_SMART2_MAJOR + 5,	16,	'0' },	/* 77:  c5d0-c5d15 */
	{"cs6d", COMPAQ_SMART2_MAJOR + 6,	16,	'0' },	/* 78:  c6d0-c6d15 */
	{"cs7d", COMPAQ_SMART2_MAJOR + 7,	16,	'0' },	/* 79:  c7d0-c7d15 */

	{"cc0d", COMPAQ_CISS_MAJOR,			16,	'0' },	/* 104:  c0d0-c0d15 */
	{"cc1d", COMPAQ_CISS_MAJOR + 1,		16,	'0' },	/* 105:  c1d0-c1d15 */
	{"cc2d", COMPAQ_CISS_MAJOR + 2,		16,	'0' },	/* 106:  c2d0-c2d15 */
	{"cc3d", COMPAQ_CISS_MAJOR + 3,		16,	'0' },	/* 107:  c3d0-c3d15 */
	{"cc4d", COMPAQ_CISS_MAJOR + 4,		16,	'0' },	/* 108:  c4d0-c4d15 */
	{"cc5d", COMPAQ_CISS_MAJOR + 5,		16,	'0' },	/* 109:  c5d0-c5d15 */
	{"cc6d", COMPAQ_CISS_MAJOR + 6,		16,	'0' },	/* 110:  c6d0-c6d15 */
	{"cc7d", COMPAQ_CISS_MAJOR + 7,		16,	'0' },	/* 111:  c7d0-c7d15 */
	{"dm-",  DM_MAJOR,              	256, '0' },	/* 254:  dm-0 - dm-255 */

	{"fd",	FLOPPY_MAJOR,				0,	'0' }	/* 2:  fd0-fd3  */
	};

static gboolean
disk_major_ok(gint major)
	{
	gint		i;

	for (i = 0; i < sizeof(disk_name_map) / sizeof(struct _disk_name_map); ++i)
		{
		if (major == disk_name_map[i].major)
			return TRUE;
		}
	return FALSE;
	}

gchar *
gkrellm_sys_disk_name_from_device(gint device_number, gint unit_number,
			gint *order)
	{
	struct _disk_name_map	*dm	= NULL;
	gint		i;
	gchar		suffix;
	static gchar name[32];

	for (i = 0; i < sizeof(disk_name_map) / sizeof(struct _disk_name_map); ++i)
		{
		if (device_number != disk_name_map[i].major)
			continue;
		dm = &disk_name_map[i];
		break;
		}
	if (dm)
		{
		suffix = dm->suffix_base + unit_number;
		sprintf(name, "%s%c", dm->name, suffix);
		}
	else
		sprintf(name, "(%d,%d)", device_number, unit_number);
	*order = i;
	return name;
	}

gint
gkrellm_sys_disk_order_from_name(gchar *name)
	{
	struct _disk_name_map	*dm, *dm_next;
	gint		i, len, table_size;
	gchar		suffix;

	table_size = sizeof(disk_name_map) / sizeof(struct _disk_name_map);
	for (i = 0; i < table_size; ++i)
		{
		dm = &disk_name_map[i];
		len = strlen(dm->name);
		if (strncmp(dm->name, name, len))
			continue;
		suffix = name[len];			/* So far looked at only for "hd" series */
		if (i < table_size - 1)
			{
			dm_next = &disk_name_map[i + 1];
			if (   !strcmp(dm_next->name, dm->name)
				&& dm_next->suffix_base <= suffix
			   )
				continue;
			}
		break;
		}
	if (i >= table_size)
		i = -1;
	return i;
	}

  /* Given a /proc/partitions or /proc/diskstats line disk name in "partition",
  |  make "disk" have the whole disk name (eg hda) and "partition" have the
  |  partition (eg hda1) or NULL if not a partition.  For simple names,
  |  "disk" is expected to initially have the whole disk name from the
  |  previous call (or NULL if this is the first call per /proc file parse).
  */
static gboolean
disk_get_device_name(gint major, gint minor, gchar *disk, gchar *partition)
	{
	struct _disk_name_map	*dm	= NULL;
	gint					i, unit = 0;
	gchar					*p, *d;
		
	for (p = partition; *p; ++p)
		if (*p == '/')
			break;
	if (!*p)
		{	/* Have a simple name like hda, hda1, sda, ... */
		d = disk;
		p = partition;
		while (*d && *p && *d++ == *p++)
			;
		if (d == disk || *d || *p < '0' || *p > '9')
			{
			strcpy(disk, partition);
			partition[0] = '\0';
			}
		return TRUE;
		}

	/* Have a devfs name like ide/host0/bus0/target0/lun0/part1, so construct
	|  a name based on major, minor numbers and the disk_name_map[].
	*/
	for (i = 0; i < sizeof(disk_name_map) / sizeof(struct _disk_name_map); ++i)
		{
		if (major != disk_name_map[i].major)
			continue;
		dm = &disk_name_map[i];
		if (dm->minor_mod > 0 && minor >= dm->minor_mod)
			{
			unit = minor / dm->minor_mod;
			minor = minor % dm->minor_mod;
			}
		sprintf(disk, "%s%c", dm->name, dm->suffix_base + unit);
		if (minor > 0)
			sprintf(partition, "%s%d", disk, minor);
		else
			partition[0] = '\0';
		return TRUE;
		}
	return FALSE;
	}

  /* Kernels >= 2.5.69 have /proc/diskstats which can be more efficient to
  |  read than getting stats from sysfs.  See:
  |      /usr/src/linux/Documentation/iostats.txt
  |  But gkrellm calls this only for 2.6+ kernels since there were some
  |  format incompatible /proc/diskstats patches for 2.4.
  */
static void
linux_read_proc_diskstats(void)
	{
	static FILE		*f;
	gchar			buf[1024], part[128], disk[128];
	gint			major, minor, n;
	gulong			rd, wr, rd1, wr1;
	gboolean		inactivity_override, is_MD;
	static gboolean	initial_read = TRUE;

	if (!f && (f = fopen(PROC_DISKSTATS_FILE, "r")) == NULL)
		return;

	disk[0] = '\0';
	part[0] = '\0';
		
	while ((fgets(buf, sizeof(buf), f)) != NULL)
		{
		/* major minor name rio rmerge rsect ruse wio wmerge wsect wuse
		|               running use aveq
		|  --or for a partition--
		|  major minor name rio rsect wio wsect
		*/
		n = sscanf(buf, "%d %d %127s %*d %lu %lu %lu %*d %*d %lu",
					&major, &minor, part, &rd, &rd1, &wr, &wr1);
		if (n == 7)
			{
			rd = rd1;
			wr = wr1;
			}

		/* Make sure all real disks get reported (so they will be added to the
		|  disk monitor in order) the first time this function is called.
		|  Use disk_major_ok() instead of simply initial_read until I'm sure
		|  I'm testing for all the right "major" exclusions.
		|  Note: disk_get_device_name() assumes "part[]" retains results from
		|  previous calls and that disk/subdisk parsing will be in order
		|  (ie hda will be encountered before hda1).
		*/
		inactivity_override = initial_read ? disk_major_ok(major) : FALSE;

		if (   (n != 7 && n != 6)
			|| (rd == 0 && wr == 0 && !inactivity_override)
			|| major == LVM_BLK_MAJOR || major == NBD_MAJOR
			|| major == RAMDISK_MAJOR || major == LOOP_MAJOR
		    || major == DM_MAJOR
			|| !disk_get_device_name(major, minor, disk, part)
		   )
			continue;
		is_MD = (major == MD_MAJOR);
		if (part[0] == '\0')
			gkrellm_disk_assign_data_by_name(disk, 512 * rd, 512 * wr, is_MD);
		else
			gkrellm_disk_subdisk_assign_data_by_name(part, disk,
						512 * rd, 512 * wr);
		}
	rewind(f);
	initial_read = FALSE;
	}

  /* /proc/partitions can have diskstats in 2.4 kernels or in 2.5+ it's just
  |  a list of disk names which can be used to get names to look for in sysfs.
  |  But, with gkrellm 2.1.15 and for 2.6+kernels /proc/diskstats is
  |  used instead of sysfs.
  */
static void
linux_read_proc_partitions_or_sysfs(void)
	{
	FILE		*f, *sf;
	gchar		buf[1024], part[128], disk[128], sysfspath[256];
	gint		major, minor, n;
	gulong		sectors, rd, wr;
	gboolean	is_MD;

	if ((f = fopen(PROC_PARTITIONS_FILE, "r")) != NULL)
		{
		fgets(buf, sizeof(buf), f); /* header */
		fgets(buf, sizeof(buf), f); 
		disk[0] = '\0';
		part[0] = '\0';

		while ((fgets(buf, sizeof(buf), f)) != NULL)
			{
			major = 0;
			if (have_partition_stats)
				{
				/* major minor  #blocks  name
				|  rio rmerge rsect ruse wio wmerge wsect wuse running use aveq
				*/
				n = sscanf(buf, "%d %d %lu %127s %*d %*d %lu %*d %*d %*d %lu",
							&major, &minor, &sectors, part, &rd, &wr);
				if (   n < 6 || sectors <= 1 || major == LVM_BLK_MAJOR
					|| !disk_get_device_name(major, minor, disk, part)
				   )
					continue;
				}
			if (have_sysfs_stats)
				{
				n = sscanf(buf, "%d %d %lu %127s",
							&major, &minor, &sectors, part);
				if (   n < 4 || sectors <= 1 || major == LVM_BLK_MAJOR
					|| !disk_get_device_name(major, minor, disk, part)
				   )
					continue;
				if (part[0] == '\0')
					sprintf(sysfspath, "/sys/block/%s/stat", disk);
				else
					sprintf(sysfspath, "/sys/block/%s/%s/stat", disk, part);
				if ((sf = fopen(sysfspath, "r")) != NULL)
					{
					fgets(buf, sizeof(buf), sf);
					fclose(sf);
					if (part[0] == '\0')
						n = sscanf(buf,"%*d %*d %lu %*d %*d %*d %lu",&rd, &wr);
					else
						n = sscanf(buf,"%*d %lu %*d %lu", &rd, &wr);
					if (n < 2)
						continue;
					}
				}
			is_MD = (major == MD_MAJOR);
			if (part[0] == '\0')
				gkrellm_disk_assign_data_by_name(disk,
							512 * rd, 512 * wr, is_MD);
			else
				gkrellm_disk_subdisk_assign_data_by_name(part, disk,
							512 * rd, 512 * wr);
			}
		fclose(f);
		}
	}

void
gkrellm_sys_disk_read_data(void)
	{
	/* If have_partition_stats, still need to get floppy and CDrom data
	|  from /proc/stat
	*/
	if (!have_sysfs_stats && !have_diskstats)
		linux_read_proc_stat();	

	if (have_diskstats)
		linux_read_proc_diskstats();
	else if (have_partition_stats || have_sysfs_stats)
		linux_read_proc_partitions_or_sysfs();
	}

gboolean
gkrellm_sys_disk_init(void)
	{
	FILE	*f = NULL;
	gchar	buf[1024];

	/* There were some incompatible /proc/diskstats patches for 2.4
	*/
	if (os_release(2,6,0) && (f = fopen(PROC_DISKSTATS_FILE, "r")) != NULL)
		have_diskstats = TRUE;
	else if ((f = fopen(PROC_PARTITIONS_FILE, "r")) != NULL)
		{
		if (fgets(buf, sizeof(buf), f))
			{
			if (strstr(buf, "rsect"))
				have_partition_stats = TRUE;
			else 
				{
				if (have_sysfs)
					have_sysfs_stats = TRUE;
				}
			}
		}
	if (f)
		fclose(f);
	if (_GK.debug_level & DEBUG_SYSDEP)
		printf("diskstats=%d partition_stats=%d sysfs_stats=%d\n",
			have_diskstats, have_partition_stats, have_sysfs_stats);

	return TRUE;
	}


/* ===================================================================== */
/* Proc monitor interface */

#include <utmp.h>
#include <paths.h>

#define	PROC_LOADAVG_FILE	"/proc/loadavg"

void
gkrellm_sys_proc_read_data(void)
	{
	FILE	*f;
	gchar	buf[160];
	gint	n_running = 0, n_processes = 0;
	gulong	n_forks = 0;
	gfloat	fload = 0;

	if ((f = fopen(PROC_LOADAVG_FILE, "r")) != NULL)
		{
		/* sscanf(buf, "%f") might fail to convert because for some locales
		|  commas are used for decimal points.
		*/
		fgets(buf, sizeof(buf), f);
		if (need_locale_fix)
			locale_fix(buf);
		sscanf(buf,"%f %*f %*f %d/%d %lu", &fload,
						&n_running, &n_processes, &n_forks);
		fclose(f);
		gkrellm_proc_assign_data(n_processes, n_running, n_forks, fload);
		}
	}

void
gkrellm_sys_proc_read_users(void)
	{
	struct utmp		*ut;
	struct stat		s;
	static time_t	utmp_mtime;
	gint			n_users = 0;

	if (stat(_PATH_UTMP, &s) == 0 && s.st_mtime != utmp_mtime)
		{
		setutent();
		while ((ut = getutent()) != NULL)
			if (ut->ut_type == USER_PROCESS && ut->ut_name[0] != '\0')
				++n_users;
		endutent();
		utmp_mtime = s.st_mtime;
		gkrellm_proc_assign_users(n_users);
		}
	}

gboolean
gkrellm_sys_proc_init(void)
	{
	struct lconv	*lc;

	lc = localeconv();
	locale_decimal_point = *lc->decimal_point;
	if (locale_decimal_point != '.')
		need_locale_fix = TRUE;

	return TRUE;
	}


/* ===================================================================== */
/* Inet monitor interface */

#include "../inet.h"

#define	PROC_NET_TCP_FILE	"/proc/net/tcp"
#define	PROC_NET_UDP_FILE	"/proc/net/udp"
#if defined(INET6)
#define	PROC_NET_TCP6_FILE	"/proc/net/tcp6"
#define	PROC_NET_UDP6_FILE	"/proc/net/udp6"
#endif

static void
inet_read_data(gchar *fname, gboolean is_udp)
	{
	FILE		*f;
	ActiveTCP	tcp;
	gchar		buf[512];
	gint		tcp_status;
	gulong		addr;

	if ((f = fopen(fname, "r")) != NULL)
		{
		fgets(buf, sizeof(buf), f);		/* header */
		while (fgets(buf, sizeof(buf), f))
			{
			sscanf(buf, "%*d: %*x:%x %lx:%x %x", &tcp.local_port,
						&addr, &tcp.remote_port, &tcp_status);
			tcp.remote_addr.s_addr = (uint32_t) addr;
			tcp.family = AF_INET;
			if (tcp_status != TCP_ALIVE)
				continue;
			tcp.is_udp = is_udp;
			gkrellm_inet_log_tcp_port_data(&tcp);
			}
		fclose(f);
		}
	}

#if defined(INET6)
static void
inet6_read_data(gchar *fname, gboolean is_udp)
	{
	FILE		*f;
	ActiveTCP	tcp;
	gchar		buf[512];
	gint		tcp_status;

	if ((f = fopen(fname, "r")) != NULL)
		{
		fgets(buf, sizeof(buf), f);		/* header */
		while (fgets(buf, sizeof(buf), f))
			{
			sscanf(buf, "%*d: %*x:%x %8x%8x%8x%8x:%x %x",
			       &tcp.local_port,
			       &tcp.remote_addr6.s6_addr32[0],
			       &tcp.remote_addr6.s6_addr32[1],
			       &tcp.remote_addr6.s6_addr32[2],
			       &tcp.remote_addr6.s6_addr32[3],
			       &tcp.remote_port, &tcp_status);
			tcp.family = AF_INET6;
			if (tcp_status != TCP_ALIVE)
				continue;
			tcp.is_udp = is_udp;
			gkrellm_inet_log_tcp_port_data(&tcp);
			}
		fclose(f);
		}
	}
#endif	/* INET6 */

void
gkrellm_sys_inet_read_tcp_data(void)
	{
	inet_read_data(PROC_NET_TCP_FILE, FALSE);
	inet_read_data(PROC_NET_UDP_FILE, TRUE);
#if defined(INET6)
	inet6_read_data(PROC_NET_TCP6_FILE, FALSE);
	inet6_read_data(PROC_NET_UDP6_FILE, TRUE);
#endif	/* INET6 */
	}

gboolean
gkrellm_sys_inet_init(void)
	{
	return TRUE;
	}


/* ===================================================================== */
/* Net monitor interface */

#define	PROC_NET_DEV_FILE	"/proc/net/dev"
#define	PROC_NET_ROUTE_FILE	"/proc/net/route"

typedef struct
	{
	gchar		*name;
	gboolean	cur_up,
				up;
	}
	NetUp;

static GList	*net_routed_list;

static gint			rx_bytes_index,
					tx_bytes_index,
					rx_packets_index,
					tx_packets_index;


void
gkrellm_sys_net_check_routes(void)
	{
	static FILE		*f;
	GList			*list;
	NetUp			*net;
	gchar			*s;
	gchar			buf[512];


	for (list = net_routed_list; list; list = list->next)
		((NetUp *) list->data)->cur_up = FALSE;

	if (f || (f = fopen(PROC_NET_ROUTE_FILE, "r")) != NULL)
		{
		fgets(buf, sizeof(buf), f);		/* Waste the first line */
		while (fgets(buf, sizeof(buf), f))
			{
			if (   ((s = strtok(buf, " \t\n")) == NULL)
				|| !strncmp(s, "dummy", 5)
				|| (*s == '*' && *(s+1) == '\0')
			   )
				continue;
			for (list = net_routed_list; list; list = list->next)
				{
				net = (NetUp *) list->data;
				if (!strcmp(net->name, s))
					{
					net->cur_up = TRUE;
					break;
					}
				}
			if (!list)
				{
				net = g_new0(NetUp, 1);
				net_routed_list = g_list_append(net_routed_list, net);
				net->name = g_strdup(s);
				net->cur_up = TRUE;
				}
			}
		rewind(f);
		}
	for (list = net_routed_list; list; list = list->next)
		{
		net = (NetUp *) list->data;
		if (net->up && !net->cur_up)
			gkrellm_net_routed_event(net->name, FALSE);
		else if (!net->up && net->cur_up)
			gkrellm_net_routed_event(net->name, TRUE);
		net->up = net->cur_up;
		}
	}

  /* I read both the bytes (kernel 2.2.x) and packets (all kernels).  Some
  |  net drivers for 2.2.x do not update the bytes counters.
  */
void
gkrellm_sys_net_read_data(void)
	{
	static FILE	*f;
	gchar		buf[512];
	gchar		*name, *s, *s1;
	gint		i;
	gulong		rx, tx;
	gulong		rx_packets	= 0,
				tx_packets	= 0;
	guint64		ll;

	if (!f && (f = fopen(PROC_NET_DEV_FILE, "r")) == NULL)
		return;
	fgets(buf, sizeof(buf), f);		/* 2 line header */
	fgets(buf, sizeof(buf), f);
	while (fgets(buf, sizeof(buf), f))
		{
		/* Virtual net interfaces have a colon in the name, and a colon seps
		|  the name from data, + might be no space between data and name!
		|  Eg. this is possible -> eth2:0:11249029    0 ...
		|  So, replace the colon that seps data from the name with a space.
		*/
		s = strchr(buf, (int) ':');
		if (s)
			{
			s1 = strchr(s + 1, (int) ':');
			if (s1)
				*s1 = ' ';
			else
				*s = ' ';
			}
		if ((name = strtok(buf, " \t\n")) == NULL)	/* Get name of interface */
			{
			fclose(f);
			f = NULL;
			return;
			}
		if (!strncmp(name, "dummy", 5))
			continue;
		rx = tx = 0;
		for (i = 1; (s = strtok(NULL, " \t\n")) != NULL; ++i)
			{
			if (i == rx_bytes_index)
				{
				rx = strtoul(s, NULL, 0);

				/* Can have 32 bit library / 64 bit kernel mismatch.
				|  If so, just using the 32 low bits of a long long is OK.
				*/
				if (   rx == ULONG_MAX && errno == ERANGE
					&& sscanf(s, "%" PRIu64, &ll) == 1
				   )
					rx = (gulong) ll;
				}
			else if (i == tx_bytes_index)
				{
				tx = strtoul(s, NULL, 0);
				if (   tx == ULONG_MAX && errno == ERANGE
					&& sscanf(s, "%" PRIu64, &ll) == 1
				   )
					tx = (gulong) ll;
				}
			else if (i == rx_packets_index)
				rx_packets = strtoul(s, NULL, 0);
			else if (i == tx_packets_index)
				tx_packets = strtoul(s, NULL, 0);
			if (i > tx_bytes_index && i > tx_packets_index)
				break;
			}
		if (rx == 0 && tx == 0)
			{
			rx = rx_packets;
			tx = tx_packets;
			}
		gkrellm_net_assign_data(name, rx, tx);
		}
	rewind(f);
	}

gboolean
gkrellm_sys_net_isdn_online(void)
	{
	gint	f = 0;
	gchar	buffer[BUFSIZ], *p, *end;

	if (   (f = open("/dev/isdninfo", O_RDONLY)) == -1
		&& (f = open("/dev/isdn/isdninfo", O_RDONLY)) == -1
	   ) 
		{
		if (_GK.debug_level & DEBUG_NET)
			printf("sys_net_isdn__online: no /dev/isdninfo?\n");
		return FALSE;
		}
	memset(buffer, 0, BUFSIZ);

	if (read(f, buffer, BUFSIZ) <= 0)
		{
		close(f);
		return FALSE;
		}
	close(f);

	if ((p = strstr(buffer, "flags:")) == NULL)
		return FALSE;

	for(p += 6; *p; )
		{
		if (isspace(*p))
			{
			p++;
			continue;
			}
		for	(end = p; *end && !isspace(*end); end++)
			;
		if (*end == '\0' || *end == '\t')
			break;
		else
			*end = 0;
		if (!strcmp(p, "?") || !strcmp(p, "0"))
			{
			p = end+1;
			continue;
			}
		return TRUE;	/* ISDN is online */
		}
	return FALSE;	/* ISDN is off line */
	}

static const char	*delim	= " :|\t\n";

static void
get_io_indices(void)
	{
	FILE	*f;
	gchar	*s;
	gchar	buf[184];
	gint	i;

	if ((f = fopen(PROC_NET_DEV_FILE, "r")))
		{
		fgets(buf, sizeof(buf), f);		/* Waste the first line.	*/
		fgets(buf, sizeof(buf), f);		/* Look for "units" in this line */
		s = strtok(buf, delim);
		for (i = 0; s; ++i)
			{
			if (strcmp(s, "bytes") == 0)
				{
				if (rx_bytes_index == 0)
					rx_bytes_index = i;
				else
					tx_bytes_index = i;
				}
			if (strcmp(s, "packets") == 0)
				{
				if (rx_packets_index == 0)
					rx_packets_index = i;
				else
					tx_packets_index = i;
				}
			s = strtok(NULL, delim);
			}
		fclose(f);
		}
	if (_GK.debug_level & DEBUG_NET)
		printf("rx_bytes=%d tx_bytes=%d rx_packets=%d tx_packets=%d\n",
				rx_bytes_index, tx_bytes_index,
				rx_packets_index, tx_packets_index);
	}

gboolean
gkrellm_sys_net_init(void)
	{
	get_io_indices();
	gkrellm_net_set_lock_directory("/var/lock");
	gkrellm_net_add_timer_type_ppp("ppp0");
	gkrellm_net_add_timer_type_ippp("ippp0");
	gkrellm_net_use_routed(TRUE /* Always TRUE from sysdep code */);
	return TRUE;
	}


/* ===================================================================== */
/* Memory/Swap monitor interface */

#define	PROC_MEMINFO_FILE	"/proc/meminfo"
#define	PROC_VMSTAT_FILE	"/proc/vmstat"

static guint64	swap_total, swap_used;

  /* Kernels >= 2.5.x have tagged formats only in kb units.
  */
static void
tagged_format_meminfo(gchar *buf, guint64 *mint)
	{
	sscanf(buf,"%" PRIu64, mint);
	*mint *= 1024;
	}

void
gkrellm_sys_mem_read_data(void)
	{
	FILE		*f;
	gchar		buf[160];
	gboolean	using_tagged = FALSE;
	guint64		total, used, x_used, free, shared, buffers, cached, slab;

	/* Default  0, so we don't get arbitrary values if not found,
	|  e.g. MemShared is not present in 2.6.
	*/
	total = used = x_used = free = shared = buffers = cached = slab = 0LL;

	if ((f = fopen(PROC_MEMINFO_FILE, "r")) == NULL)
		return;
	while ((fgets(buf, sizeof(buf), f)) != NULL)
		{
		if (buf[0] == 'M')
			{
			if (!strncmp(buf, "Mem:", 4))
				sscanf(buf + 5,
		"%" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64,
						&total, &x_used, &free,
						&shared, &buffers, &cached);
			else if (!strncmp(buf, "MemTotal:", 9))
				{
				tagged_format_meminfo(buf + 10, &total);
				using_tagged = TRUE;
				}
			else if (!strncmp(buf, "MemFree:", 8))
				tagged_format_meminfo(buf + 9, &free);
			else if (!strncmp(buf, "MemShared:", 10))
				tagged_format_meminfo(buf + 11, &shared);
			}
		else if (buf[0] == 'S')
			{
			if (!strncmp(buf, "Swap:", 5))
				sscanf(buf + 6,"%" PRIu64 " %" PRIu64,
						&swap_total, &swap_used);
			else if (!strncmp(buf, "SwapTotal:", 10))
				tagged_format_meminfo(buf + 11, &swap_total);
			else if (!strncmp(buf, "SwapFree:", 9))
				tagged_format_meminfo(buf + 10, &swap_used);
			else if (!strncmp(buf, "Slab:", 5))
				tagged_format_meminfo(buf + 6, &slab);
			}
		else if (buf[0] == 'B' && !strncmp(buf, "Buffers:", 8))
			tagged_format_meminfo(buf + 9, &buffers);
		else if (buf[0] == 'C' && !strncmp(buf, "Cached:", 7))
			tagged_format_meminfo(buf + 8, &cached);
		}
	fclose(f);
	if (using_tagged)
		{
		x_used = total - free;
		swap_used = swap_total - swap_used;
		}
	used = x_used - buffers - cached - slab;
	gkrellm_mem_assign_data(total, used, free, shared, buffers, cached + slab);
	}

  /* Kernel >= 2.6 swap page in/out is in /proc/vmstat.
  |  Kernel <= 2.4 swap page in/out is in /proc/stat read in read_proc_stat().
  |  Swap total/used for all kernels is read in mem_read_data() above.
  */
void
gkrellm_sys_swap_read_data(void)
	{
	static FILE		*f;
	gchar			buf[128];

	if (!f && kernel_2_6)
		f = fopen(PROC_VMSTAT_FILE, "r");

	if (f)
		{
		while (fgets(buf, sizeof(buf), f) != NULL)
			{
			if (buf[0] != 'p' || buf[1] != 's')
				continue;
			sscanf(buf, "pswpin %lu", &swapin);
			if (fgets(buf, sizeof(buf), f) == NULL)
				break;
			sscanf(buf, "pswpout %lu", &swapout);
			}
		rewind(f);
		}
	gkrellm_swap_assign_data(swap_total, swap_used, swapin, swapout);
	}

gboolean
gkrellm_sys_mem_init(void)
	{
	return TRUE;
	}


/* ===================================================================== */
/* FS monitor interface */

#include <sys/vfs.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <linux/cdrom.h>
#include <mntent.h>

#define	PROC_MOUNTS_FILE	"/proc/mounts"

  /* A list of mounted file systems can be read from /proc/mounts or
  |  /etc/mtab (getmntent).  Using /proc/mounts eliminates disk accesses,
  |  but for some reason /proc/mounts reports a "." for the mounted
  |  directory for smbfs types.  So I use /proc/mounts with a fallback
  |  to using getmntent().
  */
#if !defined (_PATH_MOUNTED)
#define _PATH_MOUNTED   "/etc/mtab"
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
	FILE	*f;
	gchar	buf[1024], *s;
	gchar	dev[512], dir[512], type[128], opt[256];

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
		sscanf(s, "%511s %511s %127s %255s", dev, dir, type, opt);
		fix_fstab_name(dev);
		fix_fstab_name(dir);
		fix_fstab_name(type);
		fix_fstab_name(opt);

		if (   type[0] == '\0'
			|| !strcmp(type, "devpts")
			|| !strcmp(type, "swap")
			|| !strcmp(type, "proc")
			|| !strcmp(type, "sysfs")
			|| !strcmp(type, "usbdevfs")
			|| !strcmp(type, "usbfs")
			|| !strcmp(type, "ignore")
		   )
			continue;
		gkrellm_fs_add_to_fstab_list(dir, dev, type, opt);
		}
	fclose(f);
	}

static void
getmntent_fallback(gchar *dir, gint dirsize, gchar *dev)
	{
	FILE			*f;
	struct mntent	*mnt;

	if ((f = setmntent(_PATH_MOUNTED, "r")) == NULL)
		return;
	while ((mnt = getmntent(f)) != NULL)
		{
		if (!strcmp(dev, mnt->mnt_fsname))
			{
			snprintf(dir, dirsize, "%s", mnt->mnt_dir);
			break;
			}
		}
	endmntent(f);
	}

void
gkrellm_sys_fs_get_mounts_list(void)
	{
	FILE		*f;
	gchar		*s, buf[1024], dev[512], dir[512], type[128];

	if ((f = fopen(PROC_MOUNTS_FILE, "r")) == NULL)
		return;
	while (fgets(buf, sizeof(buf), f))
		{
		dev[0] = dir[0] = type[0] = '\0';
		sscanf(buf, "%512s %512s %127s", dev, dir, type);
		fix_fstab_name(dev);
		fix_fstab_name(dir);
		fix_fstab_name(type);

		if (   !strcmp(type, "devpts")
			|| !strcmp(type, "proc")
			|| !strcmp(type, "usbdevfs")
			|| !strcmp(type, "usbfs")
			|| !strcmp(type, "sysfs")
		   )
			continue;
		/* Strip trailing / from the directory.
		*/
		s = strrchr(dir, (int) '/');
		if (s && s != dir && *(s+1) == '\0')
			*s = '\0';
		if (dir[0] == '.')
			getmntent_fallback(dir, sizeof(dir), dev);
		gkrellm_fs_add_to_mounts_list(dir, dev, type);
		}
	fclose(f);
	}

void
gkrellm_sys_fs_get_fsusage(gpointer fs, gchar *dir)
	{
	struct statfs	st;

	if (!statfs(dir, &st))
		gkrellm_fs_assign_fsusage_data(fs,
					(glong) st.f_blocks, (glong) st.f_bavail,
					(glong) st.f_bfree, (glong) st.f_bsize);
	else
		gkrellm_fs_assign_fsusage_data(fs, 0, 0, 0, 0);
	}


static void
eject_linux_cdrom(gchar *device)
	{
#if defined(CDROMEJECT)
	gint	d;

	if ((d = open(device, O_RDONLY|O_NONBLOCK)) >= 0)
		{
		ioctl(d, CDROMEJECT);
		close(d);
		}
#endif
	}

gboolean
gkrellm_sys_fs_init(void)
	{
	gchar	*eject_command = NULL,
			*close_command = NULL;

#if defined(WEXITSTATUS)
	gint	n;

	n = system("eject -d > /dev/null 2>&1");
	if (WEXITSTATUS(n) == 0)
		{
		eject_command = "eject '%s'";
		close_command = "eject -t '%s'";
		}
#endif
	gkrellm_fs_setup_eject(eject_command, close_command,
				eject_linux_cdrom, NULL);
	return TRUE;
	}

/* ===================================================================== */
/* Battery monitor interface	*/

/* ---------------------- */
/* ACPI battery interface */

#define	ACPI_BATTERY_DIR			"/proc/acpi/battery/"
#define	ACPI_AC_ADAPTOR_DIR			"/proc/acpi/ac_adapter/"


typedef struct
	{
	gint		id;
	gchar		*info,
				*state;
	gint		full_cap;
	gboolean	got_full_cap,
				full_cap_bug;
	}
	BatteryFile;

static gchar	*acpi_ac_state_file;
static GList	*acpi_battery_list;

#if !defined(F_OK)
#define	F_OK	0
#endif

static gchar *
get_acpi_battery_file(gchar *dir, gchar *subdir, gchar *name)
	{
	gchar 	*path;

	/* dir is expected to have trailing '/'
	*/
	path = g_strconcat(dir, subdir, "/", name, NULL);
	if (!access(path, F_OK))
		return path;
	g_free(path);
	return NULL;
	}

static gboolean
setup_acpi_battery(gchar *bat)
	{
	BatteryFile	*bf; 
	gchar		*info;
	static gint	id;

	info = g_strconcat(ACPI_BATTERY_DIR, bat, "/info", NULL);
	if (_GK.debug_level & DEBUG_BATTERY)
		printf("setup_acpi_battery: %s\n", info);
	if (!access(info, F_OK))
		{
		bf = g_new0(BatteryFile, 1);
		bf->id = id++;
		bf->info = info;
		bf->state = get_acpi_battery_file(ACPI_BATTERY_DIR, bat, "state");
		if (!bf->state)
			bf->state = get_acpi_battery_file(ACPI_BATTERY_DIR, bat, "status");
		acpi_battery_list = g_list_append(acpi_battery_list, bf);
		if (_GK.debug_level & DEBUG_BATTERY)
			printf("setup_acpi_battery: %s\n",
						bf->state ? bf->state : "no state");
		return TRUE;
		}
	g_free(info);
	return FALSE;
	}

static gboolean
setup_ac_adapter(gchar **state, gchar *ac)
	{
	gchar	*path;

	path = get_acpi_battery_file(ACPI_AC_ADAPTOR_DIR, ac, "state");
	if (!path)
		path = get_acpi_battery_file(ACPI_AC_ADAPTOR_DIR, ac, "status");
	*state = path;
	if (_GK.debug_level & DEBUG_BATTERY)
		printf("setup_ac_adaptor: %s\n", path ? path : "no state");

	return path ? TRUE : FALSE;
	}

static void
acpi_setup(void)
	{
	DIR				*d;
	struct dirent	*de;

	if ((d = opendir(ACPI_BATTERY_DIR)) == NULL)
		return;
	
	while ((de = readdir(d)) != NULL)
		{
		if (   strcmp(de->d_name, ".")
			&& strcmp(de->d_name, "..")
		   )
			setup_acpi_battery(de->d_name);
		}
	closedir(d);

	if (!acpi_battery_list)
		return;
	
	if ((d = opendir(ACPI_AC_ADAPTOR_DIR)) != NULL)
		{
		while ((de = readdir(d)) != NULL)
			{
			if (   strcmp(de->d_name, ".")
				&& strcmp(de->d_name, "..")
				&& setup_ac_adapter(&acpi_ac_state_file, de->d_name)
			   )
				break;
			}
		closedir(d);
		}
	}


static gboolean
fgets_lower_case(gchar *buf, gint len, FILE *f)
	{
	guchar	*s;

	if (!fgets(buf, len, f))
		return FALSE;
	s = (guchar *) buf;
	if (isupper(*s))
		while (*s)
			{
			if (isupper(*s))
				*s = tolower(*s);
			++s;
			}
	return TRUE;
	}

static gboolean
acpi_battery_data(BatteryFile *bf)
	{
	FILE			*f;
	gchar			buf[128], s1[32];
	gboolean		on_line, charging, available, have_charging_state;
	gint			percent = 0, time_left = -1;
	gint			cur_cap = 0, cur_rate = 0;
	extern gint		gkrellm_battery_full_cap_fallback(void);

	if (!bf->got_full_cap)	/* get battery capacity */
		{
		if (_GK.debug_level & DEBUG_BATTERY)
			printf("getting full capacity: %s\n", bf->info);
		if ((f = fopen(bf->info, "r")) == NULL)
			return FALSE;
		bf->got_full_cap = TRUE;
		while (fgets_lower_case(buf, sizeof(buf), f))
			{
			/* present:					{yes|no}
			|  design capacity:			53280 mWh
			|  last full capacity:		51282 mWh
			|  battery technology:		rechargeable
			|  design voltage:			14800 mV
			|  design capacity warning:	5120 mWh
			|  design capacity low:		0 mWh
			|  capacity granularity 1:	1480 mWh
			|  capacity granularity 2:	1480 mWh
			|  model number:			6000
			|  serial number:			1
			|  battery type:			4
			|  OEM info:				XXXX
			*/
			if (sscanf(buf, "design capacity: %d", &bf->full_cap) == 1)
				if (_GK.debug_level & DEBUG_BATTERY)
					printf("%s: %d <- %s", bf->info, bf->full_cap, buf);
			if (sscanf(buf, "last full capacity: %d", &bf->full_cap) == 1)
				if (_GK.debug_level & DEBUG_BATTERY)
					printf("%s: %d <- %s", bf->info, bf->full_cap, buf);
			}
		fclose(f);
		}
	if (bf->full_cap == 0)
		{
		bf->full_cap = gkrellm_battery_full_cap_fallback();
		bf->full_cap_bug = TRUE;
		}

	on_line = FALSE;

	if (   acpi_ac_state_file
		&& (f = fopen(acpi_ac_state_file, "r")) != NULL
	   )
		{
		while (fgets_lower_case(buf, sizeof(buf), f))
			{
			/*	state: {on-line|off-line}
			*/
			if (   (   sscanf (buf, "state: %31s", s1) == 1
					|| sscanf (buf, "status: %31s", s1) == 1
				   )
				&& !strcmp(s1, "on-line")
			   )
				on_line = TRUE;
			}
		fclose(f);
		}

	if ((f = fopen(bf->state, "r")) == NULL)
		return FALSE;
	charging = FALSE;
	available = FALSE;
	have_charging_state = FALSE;
	while (fgets_lower_case(buf, sizeof(buf), f))
		{
		/*	present:			{yes|no}
		|	capacity state:		ok
		|	charging state:		{charged|charging|discharging|unknown}
		|	present rate:		15000 mW
		|	remaining capacity:	31282 mWh
		|	present voltage:	16652 mV
		*/
		if (sscanf(buf, "charging state: %31s", s1) == 1)
			{
			have_charging_state = TRUE;
			if (   (!strcmp(s1, "unknown") && on_line)
				|| !strcmp(s1, "charging")
				|| !strcmp(s1, "charged")
			   )
				charging = TRUE;
			continue;
			}
		if (sscanf(buf, "remaining capacity: %d", &cur_cap) == 1)
			{
			if (_GK.debug_level & DEBUG_BATTERY)
				printf("%s: %d <- %s", bf->state, cur_cap, buf);
			continue;
			}
		if (sscanf(buf, "present rate: %d", &cur_rate) == 1)
			continue;
		if (   sscanf(buf, "present: %31s", s1) == 1
			&& !strcmp(s1, "yes")
		   )
			available = TRUE;
		}
	fclose(f);

	if (_GK.debug_level & DEBUG_BATTERY)
		printf(
			"Battery: on_line=%d charging=%d have_charging=%d state_file=%d\n",
			on_line, charging, have_charging_state,
			acpi_ac_state_file ? TRUE : FALSE);

	if (!acpi_ac_state_file && charging)	/* Assumption for buggy ACPI */
		on_line = TRUE;

	if (!have_charging_state && on_line)	/* Another buggy ACPI */
		charging = TRUE;

	if (charging)
		bf->got_full_cap = FALSE;			/* reread full_cap */

	percent = cur_cap * 100 / bf->full_cap;
	if (percent > 100)
		{
		percent = 100;
		if (bf->full_cap_bug)
			bf->full_cap = cur_cap;
		else
			bf->got_full_cap = FALSE;			/* reread full_cap */
		}

	if (cur_rate > 0)
		{
		if (charging)
			time_left = 60 * (bf->full_cap - cur_cap) / cur_rate;
		else
			time_left = 60 * cur_cap / cur_rate;
		}

	if (_GK.debug_level & DEBUG_BATTERY)
		{
		printf("Battery %d: percent=%d time_left=%d cur_cap=%d full_cap=%d\n",
				bf->id, percent, time_left, cur_cap, bf->full_cap);
		printf("            available=%d on_line=%d charging=%d fc_bug=%d\n",
			available, on_line, charging, bf->full_cap_bug);
		}
	gkrellm_battery_assign_data(bf->id, available, on_line, charging,
				percent, time_left);
	return TRUE;
	}


/* ---------------------------- */
/* sysfs power interface		*/
#define	SYSFS_POWER_SUPPLIES		"/sys/class/power_supply/"
#define	SYSFS_TYPE_BATTERY			"battery"
#define	SYSFS_TYPE_AC_ADAPTER		"mains"


typedef struct syspower
	{
	gint		type;
	gint		id;
	gint		charge_units;
	gchar const	*sysdir;
	gchar const	*sys_charge_full;
	gchar const	*sys_charge_now;
	gboolean	present;
	gboolean	ac_present;
	gboolean	charging;
	}
	syspower;
#define	PWRTYPE_BATTERY		0
#define	PWRTYPE_UPS			1
#define	PWRTYPE_MAINS		2
#define	PWRTYPE_USB			3

#define	CHGUNITS_INVALID	0
#define	CHGUNITS_PERCENT	1	/*  'capacity'  */
#define	CHGUNITS_uWH		2	/*  'energy'	*/
#define	CHGUNITS_uAH		3	/*  'charge'	*/

/*
 * Ordering in this list is significant:  Mains power sources appear before
 * battery sources.
 */
static GList	*g_sysfs_power_list;
static gint		g_on_line;
static gint		g_pwr_id;


static gboolean
read_sysfs_entry (gchar *buf, gint buflen, gchar const *sysentry)
	{
	FILE *f;

	if ((f = fopen (sysentry, "r")))
		{
		if (fgets (buf, buflen, f))
			{
			gchar *nl;

			/*  Squash trailing newline if present.  */
			nl = buf + strlen (buf) - 1;
			if (*nl == '\n')
				*nl = '\0';
			fclose (f);
			if (_GK.debug_level & DEBUG_BATTERY)
				printf ("read_sysfs_entry: %s = %s\n",
		        	sysentry, buf);
			return TRUE;
			}
		fclose (f);
		}
	if (_GK.debug_level & DEBUG_BATTERY)
		printf ("read_sysfs_entry: cannot read %s\n", sysentry);
	return FALSE;
	}

static gboolean
sysfs_power_data (struct syspower *sp)
	{
	uint64_t	charge_full, charge_now;
	gint		time_left;
	gint		present;
	gint		percent;
	gchar		sysentry[128];
	gchar		buf[128];
	gchar		*syszap;
	gboolean	charging;
	gboolean	stat_full;

	time_left = -1;
	charge_full = charge_now = 0;
	present = 0;
	percent = 0;
	charging = FALSE;

	strcpy (sysentry, sp->sysdir);
	syszap = sysentry + strlen (sysentry);

	/*  What type of entry is this?  */
	if (sp->type == PWRTYPE_MAINS)
		{
		/*  Get the 'on-line' status.  */
		*syszap = '\0';
		strcat (sysentry, "/online");
		if (read_sysfs_entry (buf, sizeof (buf), sysentry))
			g_on_line = strtol (buf, NULL, 0);
		return TRUE;
		}
	
	/*
	 * The rest of this code doesn't know how to handle anything other than
	 * a battery.
	 */
	if (sp->type != PWRTYPE_BATTERY)
		return FALSE;

	/*  Is the battery still there?  */
	*syszap = '\0';
	strcat (sysentry, "/present");
	if (read_sysfs_entry (buf, sizeof (buf), sysentry))
		present = strtol (buf, NULL, 0);

	if (present)
		{
		if (read_sysfs_entry (buf, sizeof (buf), sp->sys_charge_full))
			{
			charge_full = strtoll (buf, NULL, 0);
			}
		if (read_sysfs_entry (buf, sizeof (buf), sp->sys_charge_now))
			{
			charge_now = strtoll (buf, NULL, 0);
			}
		if (sp->charge_units == CHGUNITS_PERCENT)
			{
			percent = charge_now;
			}
		else
			{
			if (charge_full > 0)
				percent = charge_now * 100 / charge_full;
			}

		/*  Get charging status.  */
		*syszap = '\0';
		strcat (sysentry, "/status");
		if (read_sysfs_entry (buf, sizeof (buf), sysentry))
			{
			charging = !strcasecmp (buf, "charging");
			stat_full = !strcasecmp (buf, "full");
			}
		}

	gkrellm_battery_assign_data (sp->id, present, g_on_line, charging,
	                             percent, time_left);
	return TRUE;
	}


static gboolean
setup_sysfs_ac_power (gchar const *sysdir)
	{
	syspower	*sp;

	if (_GK.debug_level & DEBUG_BATTERY)
		printf ("setup_sysfs_ac_power: %s\n", sysdir);
	sp = g_new0 (syspower, 1);
	sp->type			= PWRTYPE_MAINS;
	sp->id				= g_pwr_id++;
	sp->charge_units	= CHGUNITS_INVALID;
	sp->sysdir			= g_strdup (sysdir);
	sp->sys_charge_full	=
	sp->sys_charge_now	= NULL;

	/*  Add mains power sources to head of list.  */
	g_sysfs_power_list = g_list_prepend (g_sysfs_power_list, sp);

	return TRUE;
	}

static gboolean
setup_sysfs_battery (gchar const *sysdir)
	{
	syspower	*sp;
	gchar		*sys_charge_full = NULL,
				*sys_charge_now = NULL;
	gint		units;
	gboolean	retval = FALSE;

	/*
	 * There are three flavors of reporting:  'energy', 'charge', and
	 * 'capacity'.  Check for them in that order.  (Apologies for the
	 * ugliness; you try coding an unrolled 'if ((A || B) && C)' and make it
	 * pretty.)
	 */
	if (_GK.debug_level & DEBUG_BATTERY)
		printf ("setup_sysfs_battery: %s\n", sysdir);
	units = CHGUNITS_uWH;
	sys_charge_full = g_strconcat (sysdir, "/energy_full", NULL);
	if (access (sys_charge_full, F_OK | R_OK))
		{
		g_free (sys_charge_full);
		sys_charge_full = g_strconcat (sysdir, "/energy_full_design", NULL);
		if (access (sys_charge_full, F_OK | R_OK))
			{
			goto try_charge;	/*  Look down  */
			}
		}
	sys_charge_now = g_strconcat (sysdir, "/energy_now", NULL);
	if (!access (sys_charge_now, F_OK | R_OK))
		goto done;	/*  Look down  */

try_charge:
	if (sys_charge_full)	g_free (sys_charge_full), sys_charge_full = NULL;
	if (sys_charge_now)		g_free (sys_charge_now), sys_charge_now = NULL;

	units = CHGUNITS_uAH;
	sys_charge_full = g_strconcat (sysdir, "/charge_full", NULL);
	if (access (sys_charge_full, F_OK | R_OK))
		{
		g_free (sys_charge_full);
		sys_charge_full = g_strconcat (sysdir, "/charge_full_design", NULL);
		if (access (sys_charge_full, F_OK | R_OK))
			{
			goto try_capacity;	/*  Look down  */
			}
		}
	sys_charge_now = g_strconcat (sysdir, "/charge_now", NULL);
	if (!access (sys_charge_now, F_OK | R_OK))
		goto done;	/*  Look down  */

try_capacity:
	if (sys_charge_full)	g_free (sys_charge_full), sys_charge_full = NULL;
	if (sys_charge_now)		g_free (sys_charge_now), sys_charge_now = NULL;

	/*  This one's a little simpler...  */
	units = CHGUNITS_PERCENT;
	/*
	 * FIXME: I have no idea if 'capacity_full' actually shows up, since
	 * 'capacity' always defines "full" as always 100%
	 */
	sys_charge_full = g_strconcat (sysdir, "/capacity_full", NULL);
	if (access (sys_charge_full, F_OK | R_OK))
		goto ackphft;	/*  Look down  */

	sys_charge_now = g_strconcat (sysdir, "/capacity_now", NULL);
	if (access (sys_charge_now, F_OK | R_OK))
		goto ackphft;	/*  Look down  */

done:
	sp = g_new0 (syspower, 1);
	sp->type			= PWRTYPE_BATTERY;
	sp->id				= g_pwr_id++;
	sp->charge_units	= units;
	sp->sysdir			= g_strdup (sysdir);
	sp->sys_charge_full	= sys_charge_full;
	sp->sys_charge_now	= sys_charge_now;

	/*  Battery power sources are appended to the end of the list.  */
	g_sysfs_power_list = g_list_append (g_sysfs_power_list, sp);
	if (_GK.debug_level & DEBUG_BATTERY)
		printf ("setup_sysfs_battery: %s, %s\n",
		        sys_charge_full, sys_charge_now);
	retval = TRUE;

	if (0)
		{
ackphft:
		if (sys_charge_full)	g_free (sys_charge_full);
		if (sys_charge_now)		g_free (sys_charge_now);
		}
	return retval;
	}

static gboolean
setup_sysfs_power_entry (gchar const *sysentry)
	{
	gchar		*sysdir;
	gboolean	retval = FALSE;

	sysdir = g_strconcat (SYSFS_POWER_SUPPLIES, sysentry, NULL);
	if (!access (sysdir, F_OK | R_OK))
		{
		/*
		 * Read the type of this power source, and setup the appropriate
		 * entry for it.
		 */
		gchar *type;
		gchar buf[64];

		type = g_strconcat (sysdir, "/type", NULL);
		if (_GK.debug_level & DEBUG_BATTERY)
			printf ("setup_sysfs_power_entry: checking %s\n", type);
		if (read_sysfs_entry (buf, sizeof (buf), type))
			{
			if (!strcasecmp (buf, SYSFS_TYPE_AC_ADAPTER))
				retval = setup_sysfs_ac_power (sysdir);
			else if (!strcasecmp (buf, SYSFS_TYPE_BATTERY))
				retval = setup_sysfs_battery (sysdir);
			else if (_GK.debug_level & DEBUG_BATTERY)
				printf ("setup_sysfs_power_entry: unknown power type: %s\n",
						buf);
			}
		g_free (type);
		}
	g_free (sysdir);

	return retval;
	}

static gboolean
sysfs_power_setup (void)
	{
	DIR				*d;
	struct dirent	*de;
	gboolean		retval = FALSE;

	if (_GK.debug_level & DEBUG_BATTERY)
		printf ("sysfs_power_setup() entry\n");
	if ((d = opendir (SYSFS_POWER_SUPPLIES)) == NULL)
		return retval;

	while ((de = readdir (d)) != NULL)
		{
		if (    !strcmp (de->d_name, ".")
		    ||  !strcmp (de->d_name, ".."))
			{
			continue;
			}
		retval |= setup_sysfs_power_entry (de->d_name);
		}
	closedir (d);

	return retval;
	}


/* ---------------------------- */
/* APM battery interface		*/

#define	PROC_APM_FILE				"/proc/apm"

/* From: arch/i386/kernel/apm.c
|
| 0) Linux driver version (this will change if format changes)
| 1) APM BIOS Version.  Usually 1.0, 1.1 or 1.2.
| 2) APM flags from APM Installation Check (0x00):
|	bit 0: APM_16_BIT_SUPPORT
|	bit 1: APM_32_BIT_SUPPORT
|	bit 2: APM_IDLE_SLOWS_CLOCK
|	bit 3: APM_BIOS_DISABLED
|	bit 4: APM_BIOS_DISENGAGED
| 3) AC line status
|	0x00: Off-line
|	0x01: On-line
|	0x02: On backup power (BIOS >= 1.1 only)
|	0xff: Unknown
| 4) Battery status
|	0x00: High
|	0x01: Low
|	0x02: Critical
|	0x03: Charging
|	0x04: Selected battery not present (BIOS >= 1.2 only)
|	0xff: Unknown
| 5) Battery flag
|	bit 0: High
|	bit 1: Low
|	bit 2: Critical
|	bit 3: Charging
|	bit 7: No system battery
|	0xff: Unknown
| 6) Remaining battery life (percentage of charge):
|	0-100: valid
|	-1: Unknown
| 7) Remaining battery life (time units):
|	Number of remaining minutes or seconds
|	-1: Unknown
| 8) min = minutes; sec = seconds
*/

#define APM_BIOS_VERSION(major, minor)	\
			(   bios_major > (major)	\
			 || (bios_major == (major) && bios_minor >= (minor))	\
			)

  /* AC line status values */
#define	APM_ON_LINE		1

  /* Battery status values */
#define	APM_CHARGING	3
#define	APM_NOT_PRESENT	4

  /* Battery flag bits	*/
#define	APM_NO_BATTERY	0x80

#define APM_UNKNOWN		0xFF


static void
apm_battery_assign(gint id, gint bios_major, gint bios_minor,
			gint ac_line_status, gint battery_status, gint battery_flag,
			gint percent, gint time_left, gchar *units)
	{
	gboolean	available, on_line, charging;

	if (   (battery_flag != APM_UNKNOWN && (battery_flag & APM_NO_BATTERY))
		|| (battery_status == APM_UNKNOWN && ac_line_status == APM_UNKNOWN)
		|| (battery_status == APM_NOT_PRESENT && APM_BIOS_VERSION(1,2))
	   )
		available = FALSE;
	else
		available = TRUE;

	if (ac_line_status != APM_UNKNOWN)
		on_line = (ac_line_status == APM_ON_LINE) ? TRUE : FALSE;
	else
		on_line = (battery_status == APM_CHARGING) ? FALSE : TRUE;

	if (battery_status != APM_UNKNOWN)
		charging= (battery_status == APM_CHARGING) ? TRUE : FALSE;
	else
		charging = (ac_line_status == APM_ON_LINE) ? TRUE : FALSE;

	if (!strcmp(units, "sec") && time_left > 0)
		time_left /= 60;

	gkrellm_battery_assign_data(id, available, on_line, charging,
				percent, time_left);
	}

static gboolean
apm_battery_data(void)
	{
	FILE		*f;
	gchar		buf[128], units[32];
	gint		percent = 0, time_left = 0;
	gint		ac_line_status, battery_status, battery_flag;
	gint		bios_major, bios_minor;
	gint		id, n_batteries = 1;

	if ((f = fopen(PROC_APM_FILE, "r")) == NULL)
		return FALSE;
	fgets(buf, sizeof(buf), f);

	sscanf(buf, "%*s %d.%d %*x %x %x %x %d%% %d %31s\n",
				&bios_major, &bios_minor,
				&ac_line_status, &battery_status, &battery_flag,
				&percent, &time_left, units);

	/* If have APM dual battery patch, next line will be number of batteries.
	*/
	if (fgets(buf, sizeof(buf), f))
		sscanf(buf, "%d\n", &n_batteries);

	if (n_batteries < 2)
		apm_battery_assign(0, bios_major, bios_minor, ac_line_status,
					battery_status, battery_flag, percent, time_left, units);
	else
		{
		apm_battery_assign(GKRELLM_BATTERY_COMPOSITE_ID,
					bios_major, bios_minor, ac_line_status,
					battery_status, battery_flag, percent, time_left, units);
		while (n_batteries-- > 0 && fgets(buf, sizeof(buf), f))
			{
			sscanf(buf, "%d %x %x %d%% %d %31s\n", &id,
					&battery_status, &battery_flag,
					&percent, &time_left, units);
			apm_battery_assign(id - 1, bios_major, bios_minor, ac_line_status,
					battery_status, battery_flag, percent, time_left, units);
			}
		}

	fclose(f);
	return TRUE;
	}

void
gkrellm_sys_battery_read_data(void)
	{
	GList	*list;

	if (g_sysfs_power_list)
		{
		for (list = g_sysfs_power_list;  list;  list = list->next)
			sysfs_power_data ((syspower *) (list->data));
		}
	else if (acpi_battery_list)
		{
		for (list = acpi_battery_list;  list;  list = list->next)
			acpi_battery_data((BatteryFile *)(list->data));
		}
	else
		apm_battery_data();
	}

gboolean
gkrellm_sys_battery_init()
	{
	/*  Prefer sysfs power data to /proc/acpi (which is deprecated).
	|  But temporarily allow command line override in case transition trouble
	*/
	if (_GK.use_acpi_battery || !sysfs_power_setup ())
		acpi_setup();
	return TRUE;
	}



/* ===================================================================== */
/* Uptime monitor interface */

/* Calculating an uptime based on system time has a fuzzy meaning for
|  laptops since /proc/uptime does not include time system has been
|  sleeping.  So, read /proc/uptime always.
*/
time_t
gkrellm_sys_uptime_read_uptime(void)
    {
	FILE			*f;
	gulong			l	= 0;

	if ((f = fopen("/proc/uptime", "r")) != NULL)
		{
		fscanf(f, "%lu", &l);
		fclose(f);
		}
	return (time_t) l;
    }

gboolean
gkrellm_sys_uptime_init(void)
    {
	return TRUE;
    }


/* ===================================================================== */
/* Sensor monitor interface */
/* ------- Linux ------------------------------------------------------- */


#define	THERMAL_ZONE_DIR	"/proc/acpi/thermal_zone"
#define	THERMAL_DIR			"/proc/acpi/thermal"
#define	SENSORS_DIR			"/proc/sys/dev/sensors"
#define SYSFS_I2C_DIR		"/sys/bus/i2c/devices"
#define SYSFS_HWMON_DIR		"/sys/class/hwmon"
#define UNINORTH_DIR		"/sys/devices/temperatures"
#define WINDFARM_DIR		"/sys/devices/platform/windfarm.%d"

  /* mbmon and hddtemp sensor interfaces are handled in sensors-common.c
  */
#define	LIBSENSORS_INTERFACE		1
#define	THERMAL_INTERFACE			2
#define	THERMAL_ZONE_INTERFACE		3
#define	NVIDIA_SETTINGS_INTERFACE	4
#define	NVCLOCK_INTERFACE			5
#define IBM_ACPI_INTERFACE			6
#define UNINORTH_INTERFACE			7
#define WINDFARM_INTERFACE			8

#define IBM_ACPI_FAN_FILE	"/proc/acpi/ibm/fan"
#define IBM_ACPI_THERMAL	"/proc/acpi/ibm/thermal"
#define IBM_ACPI_CPU_TEMP        0
#define IBM_ACPI_PCI_TEMP        1
#define IBM_ACPI_HDD_TEMP        2
#define IBM_ACPI_GPU_TEMP        3
#define IBM_ACPI_BAT1_TEMP       4
#define IBM_ACPI_NA1_TEMP        5
#define IBM_ACPI_BAT2_TEMP       6
#define IBM_ACPI_NA2_TEMP        7
#define IBM_ACPI_FAN			 8

#define	SENSOR_NAMES_I2C			0
#define	SENSOR_NAMES_HWMON			1
#define	SENSOR_NAMES_LIBSENSORS		2

  /* 2.2.10 uses libsensors or SYSFS_HWMON_DIR if available,
  |  so try to save users old sensors-config that used SYSFS_I2C_DIR.
  */
typedef struct
	{
	gchar	*current_name,
			*i2c_name,
			*hwmon_name;
	}
	ConfigMap;

GList	*config_map_list;

static gboolean
sensors_config_migrate(gchar *current_name, gchar *config_name,
		gint current, gint config)
	{
	ConfigMap	*config_map;
	GList		*list;
	gchar		*p0, *p1;
	gint		n;

	/* Migrating to a libsensor name can be based on
	|  existing detected sysfs names stored in the config_map_list
	*/
	if (   (   current == SENSOR_NAMES_LIBSENSORS
	        && (config == SENSOR_NAMES_I2C || config == SENSOR_NAMES_HWMON)
	       )
	    || (current == SENSOR_NAMES_HWMON && config == SENSOR_NAMES_I2C)
	   )
		{
		for (list = config_map_list; list; list = list->next)
			{
			config_map = (ConfigMap *) list->data;
			if (   !strcmp(current_name, config_map->current_name)
			    && (   (   config_map->i2c_name
			            && !strcmp(config_name, config_map->i2c_name)
			           )
			        || (   config_map->hwmon_name
			            && !strcmp(config_name, config_map->hwmon_name)
			           )
			       )
			   )
				{
				if (_GK.debug_level & DEBUG_SENSORS)
					{
					printf("migrate name %s->%s: %s -> %s\n",
							(config == SENSOR_NAMES_I2C) ? "i2c" : "hwmon",
							(current == SENSOR_NAMES_LIBSENSORS) ?
									"libsensors" : "hwmon",
							config_name, current_name);
					}
				return TRUE;
				}
			}
		}

	/* But changing from a libsensor name to a sysfs name can only be guessed
	|  since libsensors may not be compiled in and so don't have a libsensor
	|  name to test against.
	*/
	if (   (   (current == SENSOR_NAMES_I2C || current == SENSOR_NAMES_HWMON)
	        && config == SENSOR_NAMES_LIBSENSORS
	       )
		/* This case should not happen unless downgrading kernel */
	    || (current == SENSOR_NAMES_I2C && config == SENSOR_NAMES_HWMON)
	   )
		{
		/* Eg, migrate current sysdep name: w83627ehf-hwmon0/temp1
		|      from a config sysdep name:   w83627ehf@a10/temp1
		*/
		p0 = strchr(current_name, (gint) '-');
		n = (p0 ? p0 - current_name : -1);
		if (n > 0 && !strncmp(current_name, config_name, n))
			{
			p0 = strrchr(current_name, (gint) '/');
			p1 = strrchr(config_name, (gint) '/');
			if (p0 && p1 && !strcmp(p0, p1))
				{
				if (_GK.debug_level & DEBUG_SENSORS)
					{
					printf("migrate name %s->%s: %s -> %s\n",
							(config == SENSOR_NAMES_LIBSENSORS) ?
									"libsensors" : "hwmon",
							(current == SENSOR_NAMES_I2C) ? "i2c" : "hwmon",
							config_name, current_name);
					}
				return TRUE;
				}
			}
		}
	return FALSE;
	}

static gboolean
check_voltage_name(const gchar *name, gint *id)
	{
	gint		i = 0,
				len;
	static gint	id_default;

	if (!name)
		return FALSE;
	len = strlen(name);
	if (!strncmp(name, "in", 2) && isdigit(name[2]))		/* inX */
		i = atoi(name + 2);
	else if (!strncmp(name, "vi", 2) && isdigit(name[3]))	/* vidX/vinX */
		i = atoi(name + 3);
	else if (!strcmp(name, "vdd"))
		i = 0;
	else if (isdigit(name[0]) && name[len - 1] == 'V')	/* via686a 2.0V etc */
		i = id_default++;
	else
		return FALSE;
	if (id)
		*id = i;
	return TRUE;
	}

#ifdef HAVE_LIBSENSORS
#include <sensors/sensors.h>

static gboolean
libsensors_init(void)
	{
	gint			nr, len, iodev;
	const sensors_chip_name	*name;
	gchar			*label, *busname, *s;
	gchar			id_name[512], sensor_path[512];
	gint			type, n_sensors_added;
	ConfigMap		*config_map;
#if SENSORS_API_VERSION < 0x400 /* libsensor 3 code */
	FILE			*f;

	f = fopen("/etc/sensors.conf", "r");
	if (!f)
		{
		if (_GK.debug_level & DEBUG_SENSORS)
			printf("libsensors: could not open /etc/sensors.conf\n");
		return FALSE;
		}

	if (sensors_init(f) != 0)
		{
		if (_GK.debug_level & DEBUG_SENSORS)
			printf("libsensors: init failed!\n");
		return FALSE;
		}
	fclose(f);

	if (_GK.debug_level & DEBUG_SENSORS)
		printf("libsensors: init OK\n");

	n_sensors_added = 0;
	nr = 0;
	while ((name = sensors_get_detected_chips(&nr)))
		{
		const sensors_feature_data	*feature;
		gint nr1 = 0, nr2 = 0;
		while ((feature = sensors_get_all_features(*name, &nr1, &nr2)))
			{
			if (   sensors_get_ignored(*name, feature->number)
			    && feature->mapping == SENSORS_NO_MAPPING
			   )
				{
				if (name->bus >= 0)
					snprintf(id_name, sizeof(id_name), "%s@%d:%x/%s",
						name->prefix, name->bus, name->addr, feature->name);
				else
					snprintf(id_name, sizeof(id_name), "%s@%x/%s",
						name->prefix, name->addr, feature->name);
				/* We need to store both the prefix and the busname, but we
				|  only have one string, so concat them together separated by :
				*/
				snprintf(sensor_path, sizeof (sensor_path), "%s:%s",
						name->prefix, name->busname ? name->busname : "NULL");

				if (!strncmp(feature->name, "temp", 4))
					type = SENSOR_TEMPERATURE;
				else if (!strncmp(feature->name, "fan", 3))
					type = SENSOR_FAN;
				else if (check_voltage_name(feature->name, NULL))
					type = SENSOR_VOLTAGE;
				else
					{
					if (_GK.debug_level & DEBUG_SENSORS)
						printf("libsensors: error determining type for: %s\n",
								id_name);
					continue;
					}

				/* failsafe tests, will bus and addr fit in 16 bits (signed)
				*/
				if (name->bus != ((name->bus << 16) >> 16))
					{
					if (_GK.debug_level & DEBUG_SENSORS)
						printf("libsensors: bus bigger than 16 bits: %s\n",
								id_name);
					continue;
					}
				if (name->addr != ((name->addr << 16) >> 16))
					{
					if (_GK.debug_level & DEBUG_SENSORS)
						printf("libsensors: addr bigger than 16 bits: %s\n",
								id_name);
					continue;
					}

				/* notice that we store the bus and addr both in iodev as
				|  2 _signed 16 bit ints. */
				iodev = (name->bus & 0xFFFF) | (name->addr << 16);
				busname = name->busname;
				
				if (sensors_get_label(*name, feature->number, &label) != 0)
					{
					if (_GK.debug_level & DEBUG_SENSORS)
						printf("libsensors: error getting label for: %s\n",
								id_name);
					label = NULL;
					}
#else /* libsensors4 code */
	if (sensors_init(NULL) != 0)
		{
		if (_GK.debug_level & DEBUG_SENSORS)
			printf("libsensors: init failed!\n");
		return FALSE;
		}

	if (_GK.debug_level & DEBUG_SENSORS)
		printf("libsensors: init OK\n");

	n_sensors_added = 0;
	nr = 0;
	while ((name = sensors_get_detected_chips(NULL, &nr)))
		{
		const sensors_subfeature *feature;
		const sensors_feature *main_feature;
		gint nr1 = 0;
		while ((main_feature = sensors_get_features(name, &nr1)))
			{
			switch (name->bus.type)
			  {
			  case SENSORS_BUS_TYPE_I2C:
			  case SENSORS_BUS_TYPE_SPI:
				snprintf(id_name, sizeof(id_name), "%s@%d:%x/%s",
					name->prefix, name->bus.nr, name->addr, main_feature->name);
				break;
			  default:
				snprintf(id_name, sizeof(id_name), "%s@%x/%s",
					name->prefix, name->addr, main_feature->name);
			          
			  }
			/* We need to store both the prefix and the path, but we
			|  only have one string, so concat them together separated by :
			*/
			snprintf(sensor_path, sizeof (sensor_path), "%s:%s",
				name->prefix, name->path ? name->path : "NULL");

			switch (main_feature->type)
			  {
			  case SENSORS_FEATURE_IN:
				type = SENSOR_VOLTAGE;
				feature = sensors_get_subfeature(name, 
						main_feature, SENSORS_SUBFEATURE_IN_INPUT);
			  	break;
			  case SENSORS_FEATURE_FAN:
				type = SENSOR_FAN;
				feature = sensors_get_subfeature(name, 
						main_feature, SENSORS_SUBFEATURE_FAN_INPUT);
				break;
			  case SENSORS_FEATURE_TEMP:
				type = SENSOR_TEMPERATURE;
				feature = sensors_get_subfeature(name, 
						main_feature, SENSORS_SUBFEATURE_TEMP_INPUT);
				break;
			  default:
				if (_GK.debug_level & DEBUG_SENSORS)
					printf("libsensors: error determining type for: %s\n",
							id_name);
				continue;
			  }

			if (!feature)
				{
				if (_GK.debug_level & DEBUG_SENSORS)
					printf("libsensors: error could not get input subfeature for: %s\n",
							id_name);
				continue;
				}

			/* failsafe tests, will bus type and nr fit in 8 bits
			   signed and addr fit in 16 bits signed ?
			*/
			if (name->bus.type != ((name->bus.type << 24) >> 24))
				{
				if (_GK.debug_level & DEBUG_SENSORS)
					printf("libsensors: bus-type bigger than 8 bits: %s\n",
							id_name);
				continue;
				}
			if (name->bus.nr != ((name->bus.nr << 24) >> 24))
				{
				if (_GK.debug_level & DEBUG_SENSORS)
					printf("libsensors: bus-nr bigger than 8 bits: %s\n",
							id_name);
				continue;
				}
			if (name->addr != ((name->addr << 16) >> 16))
				{
				if (_GK.debug_level & DEBUG_SENSORS)
					printf("libsensors: addr bigger than 16 bits: %s\n",
							id_name);
				continue;
				}

			/* notice that we store the bus id, type and addr in
			   iodev as 2 signed 8 bit ints and one 16 bit int */
			iodev = (name->bus.type & 0xFF) |
				((name->bus.nr & 0xFF) << 8) |
				(name->addr << 16);
			busname = name->path;

			label = sensors_get_label(name, main_feature);
			if (!label && (_GK.debug_level & DEBUG_SENSORS))
				printf("libsensors: error getting label for: %s\n",
						id_name);

			/* additional { to match "if (get_ignored(..) {"
			   from libsensors3 code */
				{
#endif
				if (label)
				{
				/* Strip some common post/prefixes for smaller default labels
				*/
				len = strlen(label);
				switch (type)
				  {
				  case SENSOR_TEMPERATURE:
					if (len > 5 && !strcasecmp(label + len - 5, " Temp"))
						label[len - 5] = 0;
					if (len > 12
						&& !strcasecmp(label + len - 12, " Temperature"))
						label[len - 12] = 0;
					break;
				  case SENSOR_FAN:
					if (len > 4 && !strcasecmp(label + len - 4, " Fan"))
						label[len - 4] = 0;
					if (len > 10
						&& !strcasecmp(label + len - 10, " FAN Speed"))
						label[len - 10] = 0;
					break;
				  case SENSOR_VOLTAGE:
					if (len > 8 && !strcasecmp(label + len - 8, " Voltage"))
						label[len - 8] = 0;
					if (!strncmp(label, "ATX ", 4))
						memmove(label, label + 4, strlen (label + 4) + 1);
					break;
				  }
				}
				/* Default factor of zero tells sensor.c
				|  that sensor formula is handled, ie via
				|  /etc/sensors.conf.
				*/
				gkrellm_sensors_add_sensor(type, sensor_path, id_name,
							feature->number, iodev,
							LIBSENSORS_INTERFACE, 0.0, 0.0,
							NULL, label);
				++n_sensors_added;
				if (label)
					free(label);

				if (_GK.debug_level & DEBUG_SENSORS)
					printf("%s %s %x\n",
							sensor_path, id_name,
							iodev);

				if (   busname
				    && (s = strrchr(busname, '/')) != NULL
				    && *(s + 1)
				   )
					{
					config_map = g_new0(ConfigMap, 1);
					config_map->current_name = g_strdup(id_name);
					config_map->i2c_name = g_strdup_printf("%s-%s/%s",
							name->prefix, s + 1, feature->name);
					config_map->hwmon_name = g_strdup_printf("%s-%s/%s",
							name->prefix, "hwmon0", feature->name);
					config_map_list =
							g_list_append(config_map_list, config_map);
 					}
				}
			}
		}
	return ((n_sensors_added > 0) ? TRUE : FALSE);
	}

gboolean
libsensors_get_value(char *sensor_path, int id, int iodev, float *value)
	{
	char *p;
	sensors_chip_name name;
	double val;
	int result;

	/* fill name */
	p = strchr(sensor_path, ':');
	if (!p)
		{
		if (_GK.debug_level & DEBUG_SENSORS)
			printf("libsensors: error parsing sensor_path: %s\n",
					sensor_path);
		return FALSE;
		}
	*p = 0;						/* We must undo this !! (or make a copy) */
	name.prefix = sensor_path;
	name.addr = iodev >> 16;
#if SENSORS_API_VERSION < 0x400 /* libsensor 3 code */
	name.bus = (iodev << 16) >> 16;	/* sign extend the low 16 bits */
	name.busname = p + 1;
	if (!strcmp(name.busname, "NULL"))
		name.busname = NULL;

	result = sensors_get_feature(name, id, &val) == 0;

	if (!result && (_GK.debug_level & DEBUG_SENSORS))
		{
		if (name.bus >= 0)
			printf(
				"libsensors: error getting value for: %s@%d:%x feature: %d\n",
				name.prefix, name.bus, name.addr, id);
		else
			printf("libsensors: error getting value for: %s@%x feature: %d\n",
					name.prefix, name.addr, id);
		}

#else /* libsensors4 code */
	name.bus.type = (iodev << 24) >> 24; /* sign extend the low 8 bits */
	name.bus.nr   = (iodev << 16) >> 24; /* sign extend the 2nd byte */
	name.path = p + 1;
	if (!strcmp(name.path, "NULL"))
		name.path = NULL;

	result = sensors_get_value(&name, id, &val) == 0;
	
	if (!result && (_GK.debug_level & DEBUG_SENSORS))
		{
			switch (name.bus.type)
			  {
			  case SENSORS_BUS_TYPE_I2C:
			  case SENSORS_BUS_TYPE_SPI:
				printf(
					"libsensors: error getting value for: %s@%d:%x feature: %d\n",
					name.prefix, (int)name.bus.nr, name.addr, id);
				break;
			  default:
				printf("libsensors: error getting value for: %s@%x feature: %d\n",
					name.prefix, name.addr, id);
			  }
		}
#endif

	if (value)
		*value = val;

	*p = ':';					/* !!! */
	return result;
	}

#endif	/* HAVE_LIBSENSORS */

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

	/* "w83782d-*" "w83783s-*" "w83627hf-*" */
static VoltDefault	voltdefault1[] =
	{
	{ "Vcor1",	1.0,   0, NULL },
	{ "Vcor2",	1.0,   0, NULL },
	{ "+3.3V",	1.0,   0, NULL },
	{ "+5V",	1.68,  0, NULL },		/* in3 ((6.8/10)+1)*@		*/
	{ "+12V",	3.80,  0, NULL },		/* in4 ((28/10)+1)*@		*/
	{ "-12V",	5.14 , -14.91, NULL },	/* in5 (5.14 * @) - 14.91	*/
	{ "-5V",	3.14 , -7.71,  NULL },	/* in6 (3.14 * @) -  7.71	*/
	{ "V5SB",	1.68,  0, NULL },		/* in7 ((6.8/10)+1)*@		*/
	{ "VBat",	1.0,   0, NULL }
	};

	/* lm80-*	*/
static VoltDefault	voltdefault2[] =
	{
	{ "+5V",	2.633, 0, NULL },		/* in0 (24/14.7 + 1) * @	*/
	{ "VTT",	1.0,   0, NULL },
	{ "+3.3V",	1.737, 0, NULL },		/* in2 (22.1/30 + 1) * @	*/
	{ "Vcore",	1.474, 0, NULL },		/* in3 (2.8/1.9) * @		*/
	{ "+12V",	6.316, 0, NULL },		/* in4 (160/30.1 + 1) * @	*/
	{ "-12V",	5.482, -4.482, "in0" },	/* in5 (160/35.7)*(@ - in0) + @	*/
	{ "-5V",	3.222, -2.222, "in0" }	/* in6 (36/16.2)*(@ - in0) + @	*/
	};

	/* gl518sm-*	*/
static VoltDefault	voltdefault3[] =
	{
	{ "+5V",	1.0,   0, NULL },		/* vdd */
	{ "+3.3V",	1.0,   0, NULL },		/* vin1 */
	{ "+12V",	4.192, 0, NULL },		/* vin2 (197/47)*@	*/
	{ "Vcore",	1.0,   0, NULL }		/* vin3 */
	};

	/* gl520sm-*	*/
static VoltDefault	voltdefault4[] =
	{
	{ "+5V",	1.0,   0,    NULL },	/* vdd */
	{ "+3.3V",	1.0,   0,    NULL },	/* vin1 */
	{ "+12V",	4.192, 0,    NULL },	/* vin2 (197/47)*@	*/
	{ "Vcore",	1.0,   0,    NULL },	/* vin3 */
	{ "-12V",	5.0,   -4.0, "vdd" }	/* vin4 (5*@)-(4*vdd)	*/
	};

	/* via686a-*	*/
static VoltDefault	voltdefault5[] =
	{
	{ "Vcor",	1.02,  0, NULL },		/* in0 */
	{ "+2.5V",	1.0,   0, NULL },		/* in1 */
	{ "I/O",	1.02,  0, NULL },		/* in2 */
	{ "+5V",	1.009, 0, NULL },		/* in3 */
	{ "+12V",	1.04,  0, NULL }		/* in4 */
	};

	/* mtp008-*	*/
static VoltDefault	voltdefault6[] =
	{
	{ "Vcor1",	1.0,   0,       NULL },		/* in0 */
	{ "+3.3V",	1.0,   0,       NULL },		/* in1 */
	{ "+12V",	3.8,   0,       NULL },		/* in2 @ * 38 / 10*/
	{ "Vcor2",	1.0,   0,       NULL },		/* in3 */
	{ "?",		1.0,   0,       NULL },		/* in4 */
	{ "-12V",	5.143, -16.944, NULL },		/* in5 (@ * 36 - 118.61) / 7*/
	{ "Vtt",	1.0,   0,       NULL }		/* in6 */
	};

	/* adm1025-*	adm9240-*	lm87-*	lm81-* ds1780-* */
static VoltDefault	voltdefault7[] =
	{
	{ "2.5V",	1.0,   0,       NULL },		/* in0 */
	{ "Vccp",	1.0,   0,       NULL },		/* in1 */
	{ "3.3V",	1.0,   0,       NULL },		/* in2 */
	{ "5V",		1.0,   0,       NULL },		/* in3 */
	{ "12V",	1.0,   0,       NULL },		/* in4 */
	{ "Vcc",	1.0,   0,       NULL }		/* in5 */
	};

	/* it87-*	it8705-*	it8712	*/
static VoltDefault	voltdefault8[] =
	{
	{ "Vcor1",	1.0,    0, 		NULL },
	{ "Vcor2",	1.0,    0, 		NULL },
	{ "+3.3V",	2.0,    0, 		NULL },		/* in2 (1 + 1)*@		*/
	{ "+5V",	1.68,   0, 		NULL },		/* in3 ((6.8/10)+1)*@	*/
	{ "+12V",	4.0,    0, 		NULL },		/* in4 ((30/10)+1)*@	*/
	{ "-12V",	7.67,   -27.36, NULL },		/* in5 (7.67 * @) - 27.36 */
	{ "-5V",	4.33,   -13.64, NULL },		/* in6 (4.33 * @) - 13.64 */
	{ "Stby",	1.68,   0, 		NULL },		/* in7 ((6.8/10)+1)*@	*/
	{ "Vbat",	1.0,    0, 		NULL }		/* in8					*/
	};

	/* fscpos	*/
static VoltDefault	voltdefault9[] =
	{
	{ "+12V",	1.0,    0, 		NULL },
	{ "+5V",	1.0,    0, 		NULL },
	{ "+3.3V",	1.0,    0, 		NULL }
	};

	/* abituguru2	*/
static VoltDefault	voltdefault10[] =
	{
	{ "Vcor",	1.0,    0, NULL },
	{ "DDR",	1.0,    0, NULL },
	{ "DDR VTT",1.0,    0, NULL },
	{ "CPU VTT",1.0,    0, NULL },
	{ "MCH 1.5",1.0,    0, NULL },
	{ "MCH 2.5",1.0,    0, NULL },
	{ "ICH",	1.0,    0, NULL },
	{ "+12V",	1.0,    0, NULL },
	{ "+12V 4P",1.0,    0, NULL },
	{ "+5V",	1.0,    0, NULL },
	{ "+3.3V",	1.0,    0, NULL },
	{ "5VSB",	1.0,    0, NULL }
	};

	/* abituguru - Several variations at:
	|  http://www.lm-sensors.org/wiki/Configurations/Abit
	*/
static VoltDefault	voltdefault11[] =
	{
	{ "Vcor",	1.0,	0, NULL },
	{ "DDR",	1.0,	0, NULL },
	{ "DDR VTT",1.0,	0, NULL },
	{ "NB",		1.0,	0, NULL },
	{ "SB",		1.0,	0, NULL },
	{ "HTT",	1.0,	0, NULL },
	{ "AGP",	1.0,	0, NULL },
	{ "+5V",	1.788,	0, NULL },		/* in7 @*1.788	*/
	{ "+3.3V",	1.248,	0, NULL },		/* in8 @*1.248	*/
	{ "5VSB",	1.788,	0, NULL },		/* in9 @*1.788	*/
	{ "3VDual",	1.248,	0, NULL }		/* in10 @*1.248 */
	};

	/* "w83627thf-*" "w83637hf-*"	*/
static VoltDefault	voltdefault12[] =
	{
	{ "Vcore",	1.0,   0, NULL },
	{ "+12V",	3.80,  0, NULL },		/* in1 ((28/10)+1)*@		*/
	{ "+3.3V",	1.0,   0, NULL },
	{ "+5V",	1.67,  0, NULL },		/* in3 ((34/51)+1)*@		*/
	{ "-12V",	5.14 , -14.91, NULL },	/* in4 (5.14 * @) - 14.91	*/
	{ "unused",	1.0,   0, NULL },
	{ "unused",	1.0,   0, NULL },
	{ "V5SB",	1.68,  0, NULL },		/* in7 ((6.8/10)+1)*@		*/
	{ "VBat",	1.0,   0, NULL }		/* in8 ((6.8/10)+1)*@		*/
	};


gboolean
gkrellm_sys_sensors_get_temperature(gchar *sensor_path, gint id,
		gint iodev, gint interface, gfloat *temp)
	{
	FILE		*f;
	gchar		buf[128], units[32];
	gint		n;
	gfloat		T, t[5],ibm_acpi_temp[8];
	gboolean	result = FALSE;

	if (   interface == THERMAL_INTERFACE
	    || interface == THERMAL_ZONE_INTERFACE
	   )
		{
		f = fopen(sensor_path, "r");
		if (f)
			{
			while (fgets(buf, sizeof(buf), f) != NULL)
				{
				if (need_locale_fix)
					locale_fix(buf);
				if ((n = sscanf(buf, "temperature: %f %31s", &T, units)) > 0)
					{
					if (n == 2 && !strcmp(units, "dK"))
						T = T / 10.0 - 273.0;
					*temp = T;
					result = TRUE;
					}
				}
			fclose(f);
			}
		return result;
		}

	if (interface == IBM_ACPI_INTERFACE)
		{
		f = fopen(sensor_path, "r");
		if (f) 
			{
			fgets(buf, sizeof(buf), f);
			sscanf(buf, "temperatures: %f %f %f %f %f %f %f %f",
						&ibm_acpi_temp[IBM_ACPI_CPU_TEMP],
						&ibm_acpi_temp[IBM_ACPI_PCI_TEMP],
						&ibm_acpi_temp[IBM_ACPI_HDD_TEMP],
						&ibm_acpi_temp[IBM_ACPI_GPU_TEMP],
						&ibm_acpi_temp[IBM_ACPI_BAT1_TEMP],
						&ibm_acpi_temp[IBM_ACPI_NA1_TEMP],
						&ibm_acpi_temp[IBM_ACPI_BAT2_TEMP],
						&ibm_acpi_temp[IBM_ACPI_NA2_TEMP]);

			*temp = ibm_acpi_temp[iodev];	
			result = TRUE;
			fclose(f);
			}
		return result;	
		}

	if (interface == HDDTEMP_INTERFACE)
		{
		gkrellm_sys_sensors_hddtemp_check();
		return gkrellm_sys_sensors_hddtemp_get_value(sensor_path, temp);
		}

	if (interface == MBMON_INTERFACE)
		{
		gkrellm_sys_sensors_mbmon_check(FALSE);
		return gkrellm_sys_sensors_mbmon_get_value(sensor_path, temp);
		}

	if (interface == NVIDIA_SETTINGS_INTERFACE)
		{
#if GLIB_CHECK_VERSION(2,0,0)
		gchar	*args[] = { "nvidia-settings", "-q", sensor_path, NULL };
		gchar	*output = NULL;

		result = g_spawn_sync(NULL, args, NULL,
					G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL,
					NULL, NULL, &output, NULL, NULL, NULL);

		if(result && output)
			{
			gfloat dummy;

			if(!temp)
				temp = &dummy;
			result = (sscanf(output, " Attribute %*s %*s %f", temp) == 1);
			}

		g_free(output);
		return result;
#else
		return FALSE;
#endif
		}

	if (interface == NVCLOCK_INTERFACE)
		{
#if GLIB_CHECK_VERSION(2,0,0)
		gchar	*args[] = { "nvclock", "-T", "-c", sensor_path, NULL };
		gchar	*output = NULL;
		gchar	*s = NULL;

		result = g_spawn_sync(NULL, args, NULL,
					G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL,
					NULL, NULL, &output, NULL, NULL, NULL);

		if(result && output)
			{
			gfloat dummy;

			if(!temp)
				temp = &dummy;
			s = strstr(output, "GPU");
			if (s)
				result = (sscanf(s, "GPU temperature:%f", temp) == 1);
			else
				result = FALSE;
			}

		g_free(output);
		return result;
#else
		return FALSE;
#endif
		}

	if (interface == UNINORTH_INTERFACE || interface == WINDFARM_INTERFACE)
		{
		if ((f = fopen(sensor_path, "r")))
			{
			fscanf(f, "%f", temp);
			fclose(f);
			return TRUE;
			}
		return FALSE;
		}

#ifdef HAVE_LIBSENSORS
	if (interface == LIBSENSORS_INTERFACE)
		return libsensors_get_value(sensor_path, id, iodev, temp);
#endif

	if ((f = fopen(sensor_path, "r")) == NULL)
		return FALSE;
	fgets(buf, sizeof(buf), f);
	fclose(f);

	if (!have_sysfs_sensors && need_locale_fix)
		locale_fix(buf);
	n = sscanf(buf, "%f %f %f %f %f", &t[0], &t[1], &t[2], &t[3], &t[4]);
	if (n < 1)
		return FALSE;
	T = t[n - 1];

	if (have_sysfs_sensors)
		T /= 1000.0;
	else if (T > 254.0)		/* Bogus read from BIOS if CHAR_MAX */
		return FALSE;

	if (temp)
		*temp = T;
	return TRUE;
	}

gboolean
gkrellm_sys_sensors_get_fan(gchar *sensor_path, gint id,
		gint iodev, gint interface, gfloat *fan)
	{
	FILE		*f;
	gchar		buf[64];
	gint		n;
	gfloat		t[4], T;
	gboolean	result = FALSE;

	if (interface == IBM_ACPI_INTERFACE)
		{
		f = fopen(sensor_path, "r");
		if (f) 
			{
			fgets(buf, sizeof(buf), f);
			fgets(buf, sizeof(buf), f);
			sscanf(buf, "speed: %f", &T); 
			fclose(f);
			*fan = T;
			result = TRUE;
			}
		return result;
		}

	if (interface == MBMON_INTERFACE)
		{
		gkrellm_sys_sensors_mbmon_check(FALSE);
		return gkrellm_sys_sensors_mbmon_get_value(sensor_path, fan);
		}

	if (interface == WINDFARM_INTERFACE)
		{
		f = fopen(sensor_path, "r");
		if (f)
			{
			fscanf(f, "%d", &n);
			fclose(f);
			*fan = n;
			result = TRUE;
			}
		return result;
		}

#ifdef HAVE_LIBSENSORS
	if (interface == LIBSENSORS_INTERFACE)
		return libsensors_get_value(sensor_path, id, iodev, fan);
#endif

	if ((f = fopen(sensor_path, "r")) == NULL)
		return FALSE;
	fgets(buf, sizeof(buf), f);
	fclose(f);

	if (!have_sysfs_sensors && need_locale_fix)
		locale_fix(buf);
	n = sscanf(buf, "%f %f %f %f", &t[0], &t[1], &t[2], &t[3]);
	if (n < 1)
		return FALSE;

	if (fan)
		*fan = t[n - 1];
	return TRUE;
	}

gboolean
gkrellm_sys_sensors_get_voltage(gchar *sensor_path, gint id,
		gint iodev, gint interface, gfloat *volt)
	{
	FILE		*f;
	gchar		buf[64];
	gfloat		V, t[3];
	gint		n;
	gboolean	result = FALSE;

	if (interface == MBMON_INTERFACE)
		{
		gkrellm_sys_sensors_mbmon_check(FALSE);
		return gkrellm_sys_sensors_mbmon_get_value(sensor_path, volt);
		}

	if (interface == WINDFARM_INTERFACE)
		{
		f = fopen(sensor_path, "r");
		if (f)
			{
			fscanf(f, "%f", volt);
			fclose(f);
			result = TRUE;
			}
		return result;
		}

#ifdef HAVE_LIBSENSORS
	if (interface == LIBSENSORS_INTERFACE)
		return libsensors_get_value(sensor_path, id, iodev, volt);
#endif

	if ((f = fopen(sensor_path, "r")) == NULL)
		return FALSE;
	fgets(buf, sizeof(buf), f);
	fclose(f);

	if (!have_sysfs_sensors && need_locale_fix)
		locale_fix(buf);
	n = sscanf(buf, "%f %f %f", &t[0], &t[1], &t[2]);
	if (n < 1)
		return FALSE;
	V = t[n - 1];

	if (have_sysfs_sensors)
		V /= 1000.0;
	else if (V > 254.0)		/* Bogus read from BIOS if CHAR_MAX */
		return FALSE;

	if (volt)
		*volt = V;
	return TRUE;
	}

static void
get_volt_default(gchar *chip_name, VoltDefault **vdf, gint *vdfsize)
	{
	if (!strncmp(chip_name, "it87", 4))
		{
		*vdf = &voltdefault8[0];
		*vdfsize = sizeof (voltdefault8) / sizeof (VoltDefault);
		}
	else if (   !strncmp(chip_name, "adm1025", 7)
			 || !strncmp(chip_name, "adm9240", 7)
			 || !strncmp(chip_name, "lm87", 4)
			 || !strncmp(chip_name, "lm81", 4)
			 || !strncmp(chip_name, "ds1780", 6)
			)
		{
		*vdf = &voltdefault7[0];
		*vdfsize = sizeof (voltdefault7) / sizeof (VoltDefault);
		}
	else if (!strncmp(chip_name, "mtp008", 6))
		{
		*vdf = &voltdefault6[0];
		*vdfsize = sizeof (voltdefault6) / sizeof (VoltDefault);
		}
	else if (!strncmp(chip_name, "via686", 6))
		{
		*vdf = &voltdefault5[0];
		*vdfsize = sizeof (voltdefault5) / sizeof (VoltDefault);
		}
	else if (!strncmp(chip_name, "gl520", 5))
		{
		*vdf = &voltdefault4[0];
		*vdfsize = sizeof (voltdefault4) / sizeof (VoltDefault);
		}
	else if (!strncmp(chip_name, "gl518", 5))
		{
		*vdf = &voltdefault3[0];
		*vdfsize = sizeof (voltdefault3) / sizeof (VoltDefault);
		}
	else if (!strncmp(chip_name, "lm80", 4))
		{
		*vdf = &voltdefault2[0];
		*vdfsize = sizeof (voltdefault2) / sizeof (VoltDefault);
		}
	else if (   !strncmp(chip_name, "w83627thf", 9)
		     || !strncmp(chip_name, "w83637hf", 8)
		    )
		{
		*vdf = &voltdefault12[0];
		*vdfsize = sizeof (voltdefault12) / sizeof (VoltDefault);
		}
	else if (!strncmp(chip_name, "w83", 3) && strncmp(chip_name, "w83781", 6))
		{
		*vdf = &voltdefault1[0];
		*vdfsize = sizeof (voltdefault1) / sizeof (VoltDefault);
		}
	else if (!strncmp(chip_name, "fscpos", 6))
		{
		*vdf = &voltdefault9[0];
		*vdfsize = sizeof (voltdefault9) / sizeof (VoltDefault);
		}
	else if (!strncmp(chip_name, "abituguru2", 10))
		{
		*vdf = &voltdefault10[0];
		*vdfsize = sizeof (voltdefault10) / sizeof (VoltDefault);
		}
	else if (!strncmp(chip_name, "abituguru", 9))
		{
		*vdf = &voltdefault11[0];
		*vdfsize = sizeof (voltdefault11) / sizeof (VoltDefault);
		}
	else
		{
		*vdf = &voltdefault0[0];
		*vdfsize = sizeof (voltdefault0) / sizeof (VoltDefault);
		}
	}


static gchar *
sysfs_get_chip_name(gchar *dir)
	{
	gchar	*name, buf[256], *p, *chip;
	FILE 	*f;

	name = g_strdup_printf("%s/%s", dir, "name");
	f = fopen(name, "r");
	g_free(name);
	if (!f)
		return NULL;

	buf[0] = '\0';
	fscanf(f, "%255[^\n]", buf);
	fclose(f);
	if (buf[0] == '\0')
		return NULL;

	if ((p = strchr(buf, ' ')) != NULL)		/* Remove when 2.6.0 is out */
		{									/* "w83627hf chip" -> "w83627hf" */
		*p++ = '\0';
		if (strcmp(p, "chip") && strcmp(p, "subclient"))
			return NULL;
		}

	chip = g_strdup(buf);
	for (p = chip; *p; p++)
		*p = tolower(*p);
	return chip;
	}

static void
sysfs_sensors_init(void)
	{
	GDir			*dir, *chip_dir, *dirx;
	VoltDefault		*voltdefault;
	gint			id = 0;
	gint			type, voltdefaultsize;
	gfloat			factor, offset;
	gchar			*name, *bus_name, *default_label, *vref,
					*id_name,  *chip_name, *s, *d, *sensor_path;
	const gchar		*old_bus_name;
	gchar			*old_id_name, *old_path;
	gchar			path[256], buf[256];
	gboolean		using_i2c_dir = FALSE;
	ConfigMap		*config_map;

	if (!have_sysfs)
		return;

	if ((dir = g_dir_open(SYSFS_HWMON_DIR, 0, NULL)) == NULL)
		{
		/* try again with the sysfs i2c dir for older 2.6 kernels */
		if ((dir = g_dir_open(SYSFS_I2C_DIR, 0, NULL)) == NULL)
 			return;
		using_i2c_dir = TRUE;
		gkrellm_sensors_config_migrate_connect(sensors_config_migrate,
				SENSOR_NAMES_I2C);
		}
	else
		gkrellm_sensors_config_migrate_connect(sensors_config_migrate,
				SENSOR_NAMES_HWMON);

	while ((bus_name = (gchar *) g_dir_read_name(dir)) != NULL)
		{
		if (using_i2c_dir)
			snprintf(path, sizeof(path), "%s/%s", SYSFS_I2C_DIR, bus_name);
		else
			snprintf(path, sizeof(path), "%s/%s/device",
					SYSFS_HWMON_DIR, bus_name);
		if ((chip_dir = g_dir_open(path, 0, NULL)) == NULL)
			continue;
		if ((chip_name = sysfs_get_chip_name(path)) == NULL)
			{
			g_dir_close(chip_dir);
			continue;
			}
		have_sysfs_sensors = TRUE;
		if (_GK.debug_level & DEBUG_SENSORS)
				printf("sysfs sensors dir: %s\n", path);

		get_volt_default(chip_name, &voltdefault, &voltdefaultsize);
		while ((name = (gchar *) g_dir_read_name(chip_dir)) != NULL)
			{
			snprintf(buf, sizeof(buf), "%s", name);
			if ((s = strstr(buf, "_input")) == NULL || s - buf > 6)
				continue;
			d = s + 6;		/* Can have xxxN_input, xxx_inputN, or xxx_input */
			while (isdigit(*d))
				*s++ = *d++;
			*s = '\0';
			while (isdigit(*(s-1)))
				--s;
			id = atoi(s);

			if (!strncmp(buf, "temp", 4))
				type = SENSOR_TEMPERATURE;
			else if (!strncmp(buf, "fan", 3))
				type = SENSOR_FAN;
			else if (!strncmp(buf, "in", 2))
				type = SENSOR_VOLTAGE;
			else
				continue;

			factor = 1.0;
			offset = 0;
			default_label = vref = NULL;

			if (type == SENSOR_VOLTAGE)
				{
				if (id < voltdefaultsize)
					{
					default_label = voltdefault[id].name;
					factor = voltdefault[id].factor;
					offset = voltdefault[id].offset;
					vref = voltdefault[id].vref;
					}
				else
					default_label =  buf;
				}
			id_name = g_strdup_printf("%s-%s/%s", chip_name, bus_name, buf);
			sensor_path = g_strdup_printf("%s/%s", path, name);
			gkrellm_sensors_add_sensor(type, sensor_path, id_name,
						id, 0, 0,
						factor, offset, vref, default_label);
			if (_GK.debug_level & DEBUG_SENSORS)
				printf("%s %s %d %d\n",
							sensor_path, id_name, id, type);

			if (!using_i2c_dir)
				{
				old_path = g_strdup_printf("%s/%s/device/bus/devices",
						SYSFS_HWMON_DIR, bus_name);
				if ((dirx = g_dir_open(old_path, 0, NULL)) != NULL)
					{
					while ((old_bus_name = g_dir_read_name(dirx)) != NULL)
						{
						config_map = g_new0(ConfigMap, 1);
						config_map->current_name = g_strdup(id_name);
						old_id_name = g_strdup_printf("%s-%s/%s",
								chip_name, old_bus_name, buf);
						config_map->i2c_name = g_strdup(old_id_name);
						config_map_list =
								g_list_append(config_map_list, config_map);
						g_free(old_id_name);
						}
					g_dir_close(dirx);
					}
				g_free(old_path);
				}

			g_free(id_name);
			g_free(sensor_path);
			}
		g_free(chip_name);
		g_dir_close(chip_dir);
		}
	g_dir_close(dir);
	}

static gint
sensors_nvidia_settings_ngpus(void)
	{
	gint		n = 0;
#if GLIB_CHECK_VERSION(2,0,0)
	gchar		*args[] = { "nvidia-settings", "-q", "gpus", NULL };
	gchar		*output = NULL;
	gboolean	result;

	result = g_spawn_sync(NULL, args, NULL,
				G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL,
				NULL, NULL, &output, NULL, NULL, NULL);

	if(result && output)
		sscanf(output, "%d", &n);
	g_free(output);
#endif
	if (_GK.debug_level & DEBUG_SENSORS)
		printf("nvidia-settings gpus = %d\n", n);
	return n;
	}

static gint
sensors_nvclock_ngpus(void)
	{
	gint		n = 0;
#if GLIB_CHECK_VERSION(2,0,0)
	gchar		*args[] = { "nvclock", "-s", NULL };
	gchar		*output = NULL, *s;
	gboolean	result;

	result = g_spawn_sync(NULL, args, NULL,
				G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL,
				NULL, NULL, &output, NULL, NULL, NULL);

	if(result && output)
		{
		s = g_strrstr(output, "Card number:");
		if (s)
			sscanf(s, "Card number: %d", &n);
		}
	g_free(output);
#endif
	if (_GK.debug_level & DEBUG_SENSORS)
		printf("nvclock gpus = %d\n", n);
	return n;
	}

static void
sensors_nvclock_init(gboolean enable)
	{
	gint	cnt, id;
	gchar	*sensor_path, *default_label;
	gchar	id_name[128];

	if (!enable)
		return;
	cnt = sensors_nvclock_ngpus();
	for (id = 1; id <= cnt; ++id)
		{
		sensor_path = g_strdup_printf("%d", id);
		if (gkrellm_sys_sensors_get_temperature(sensor_path, id, 0,
						NVCLOCK_INTERFACE, NULL))
			{
			snprintf(id_name, sizeof(id_name), "nvclock GPU:%d Core", id);
			if (cnt == 1)
				default_label = g_strdup_printf("GPU C");
			else
				default_label = g_strdup_printf("GPU:%d", id);
			gkrellm_sensors_add_sensor(SENSOR_TEMPERATURE,
						sensor_path, id_name,
						id, 0, NVCLOCK_INTERFACE,
						1.0, 0.0, NULL, default_label);
			g_free(default_label);
			}
		g_free(sensor_path);
		}
	}

  /* Remove embedded "-i2c-" or "-isa-" from /proc lm_sensors chip names so
  |  there can be a chance for config name /proc -> /sysfs compatibility.
  |  /proc was used in older 2.4 kerneles.
  |  Munge names like w83627hf-isa-0290 to w83627hf-0-0290
  |                or w83627hf-i2c-0-0290 to w83627hf-0-0290
  |  lm_sensor i2c bus address may not be 4 digits, eg: w83782d-i2c-0-2d
  |  and for that we want: w83782d-0-002d
  */
void
sensors_proc_name_fix(gchar *id_name)
	{
	gchar	*s;
	gint	len, bus = 0;
	guint	addr = 0;

	len = strlen(id_name) + 1;
	if ((s = strstr(id_name, "-i2c-")) != NULL)
		{
		sscanf(s + 5, "%d-%x", &bus, &addr);
		snprintf(s, len - (s - id_name), "-%d-%04x", bus, addr);
		}
	else if ((s = strstr(id_name, "-isa-")) != NULL)
		{
		*(s + 1) = '0';
		memmove(s + 2, s + 4, strlen(s + 4) + 1);
		}
	else if ((s = strstr(id_name, "-pci-")) != NULL)
		{
		*(s + 1) = '0';
		memmove(s + 2, s + 4, strlen(s + 4) + 1);
		}
	}


gboolean
gkrellm_sys_sensors_init(void)
	{
	FILE			*f;
	GDir			*dir, *chip_dir;
	VoltDefault		*voltdefault;
	gint			id = 0;
	gint			type, voltdefaultsize, cnt, ngpus_added;
	gfloat			factor, offset;
	gchar			*name, *chip_name, *fixed_chip_name, *path, *default_label;
	gchar			*vref, *sensor_path, *sensor_name, id_name[128], pbuf[128];
	struct lconv	*lc;

	lc = localeconv();
	locale_decimal_point = *lc->decimal_point;
	if (locale_decimal_point != '.')
		need_locale_fix = TRUE;

	if ((dir = g_dir_open(THERMAL_ZONE_DIR, 0, NULL)) != NULL)
		{
		while ((name = (gchar *) g_dir_read_name(dir)) != NULL)
			{
			path = g_build_filename(THERMAL_ZONE_DIR, name,
						"temperature", NULL);
			if (g_file_test(path, G_FILE_TEST_IS_REGULAR))
				{
				snprintf(id_name, sizeof(id_name), "thermal_zone/%s", name);
				gkrellm_sensors_add_sensor(SENSOR_TEMPERATURE,
							path, id_name,
							id, 0, THERMAL_ZONE_INTERFACE,
							1.0, 0.0, NULL, name);
				}
			g_free(path);
			}
		g_dir_close(dir);
		}

	if ((dir = g_dir_open(THERMAL_DIR, 0, NULL)) != NULL)
		{
		while ((name = (gchar *) g_dir_read_name(dir)) != NULL)
			{
			path = g_build_filename(THERMAL_DIR, name, "status", NULL);
			if (g_file_test(path, G_FILE_TEST_IS_REGULAR))
				{
				snprintf(id_name, sizeof(id_name), "thermal/%s", name);
				gkrellm_sensors_add_sensor(SENSOR_TEMPERATURE,
							path, id_name,
							id, 0, THERMAL_INTERFACE,
							1.0, 0.0, NULL, name);
				}
			g_free(path);
			}
		g_dir_close(dir);
		}

	/* Do intial daemon reads to get sensors loaded into sensors.c
	*/
	gkrellm_sys_sensors_hddtemp_check();
	gkrellm_sys_sensors_mbmon_check(TRUE);

	/* 
	 * IBM ACPI Temperature Sensors
	 */
	if ((f = fopen(IBM_ACPI_THERMAL, "r")) != NULL)
		{
		gkrellm_sensors_add_sensor(SENSOR_TEMPERATURE,
				IBM_ACPI_THERMAL, "IBM ACPI CPU",
				id, IBM_ACPI_CPU_TEMP, IBM_ACPI_INTERFACE,
				1.0, 0.0, NULL, "CPU");
	
		gkrellm_sensors_add_sensor(SENSOR_TEMPERATURE,
				IBM_ACPI_THERMAL, "IBM ACPI Mini PCI Module",
				id, IBM_ACPI_PCI_TEMP, IBM_ACPI_INTERFACE,
				1.0, 0.0, NULL, "PCI");
	
		gkrellm_sensors_add_sensor(SENSOR_TEMPERATURE,
				IBM_ACPI_THERMAL, "IBM ACPI HDD",
				id, IBM_ACPI_HDD_TEMP, IBM_ACPI_INTERFACE,
				1.0, 0.0, NULL, "HDD");
	
		gkrellm_sensors_add_sensor(SENSOR_TEMPERATURE,
				IBM_ACPI_THERMAL, "IBM ACPI GPU",
				id, IBM_ACPI_GPU_TEMP, IBM_ACPI_INTERFACE,
				1.0, 0.0, NULL, "GPU");
	
		gkrellm_sensors_add_sensor(SENSOR_TEMPERATURE,
				IBM_ACPI_THERMAL, "IBM ACPI Battery 1",
				id, IBM_ACPI_BAT1_TEMP, IBM_ACPI_INTERFACE,
				1.0, 0.0, NULL, "BAT1");
						
		gkrellm_sensors_add_sensor(SENSOR_TEMPERATURE,
				IBM_ACPI_THERMAL, "IBM ACPI Battery 2",
				id, IBM_ACPI_BAT2_TEMP, IBM_ACPI_INTERFACE,
				1.0, 0.0, NULL, "BAT2");
		fclose(f);
		}

	/* IBM ACPI Cooling Fan Sensor
	*/
	if ((f = fopen(IBM_ACPI_FAN_FILE, "r")) != NULL)
		{
		gkrellm_sensors_add_sensor(SENSOR_FAN,
				IBM_ACPI_FAN_FILE, "IBM ACPI Fan Sensor",
				id, IBM_ACPI_FAN, IBM_ACPI_INTERFACE,
				1.0, 0.0, NULL, "Fan");
		fclose(f);
		}

	/* nvidia-settings GPU core & ambient temperatues
	*/
	cnt = sensors_nvidia_settings_ngpus();
	ngpus_added = 0;
	if (cnt < 2)
		{
		if (gkrellm_sys_sensors_get_temperature("GPUCoreTemp", id, 0,
						NVIDIA_SETTINGS_INTERFACE, NULL))
			{
			gkrellm_sensors_add_sensor(SENSOR_TEMPERATURE,
						"GPUCoreTemp", "nVidia GPU Core",
						id, 0, NVIDIA_SETTINGS_INTERFACE,
						1.0, 0.0, NULL, "GPU C");
			++ngpus_added;
			}
		}
	else
		{
		for (id = 0; id < cnt; ++id)
			{
			sensor_path = g_strdup_printf("[gpu:%d]/GPUCoreTemp", id);
			if (gkrellm_sys_sensors_get_temperature(sensor_path, id, 0,
							NVIDIA_SETTINGS_INTERFACE, NULL))
				{
				snprintf(id_name, sizeof(id_name), "nVidia GPU:%d Core", id);
				default_label = g_strdup_printf("GPU:%d", id);
				gkrellm_sensors_add_sensor(SENSOR_TEMPERATURE,
							sensor_path, id_name,
							id, 0, NVIDIA_SETTINGS_INTERFACE,
							1.0, 0.0, NULL, default_label);
				g_free(default_label);
				++ngpus_added;
				}
			g_free(sensor_path);
			}
		}

	/* nvclock can get temps for some cards where nvidia-settings fails.
	|  nvclock card numbers start at 1.  Make an option because nvclock
	|  doesn't handle all nvidia chips (6150) where it will always reread
	|  GPU bios which slows gkrellm startup to several seconds.
	*/
	if (ngpus_added == 0)
		gkrellm_sensors_sysdep_option("use_nvclock",
			_("Use nvclock for NVIDIA GPU temperatures"),
			sensors_nvclock_init);

	id = 0;
	/* Try for ambient only for gpu:0 for now */
	if (gkrellm_sys_sensors_get_temperature("GPUAmbientTemp", id, 0,
				NVIDIA_SETTINGS_INTERFACE, NULL))
		{
		gkrellm_sensors_add_sensor(SENSOR_TEMPERATURE,
					"GPUAmbientTemp", "nVidia GPU Ambient",
					id, 0, NVIDIA_SETTINGS_INTERFACE,
					1.0, 0.0, NULL, "GPU A");
		}


	/* UNINORTH sensors
	*/
	if ((dir = g_dir_open(UNINORTH_DIR, 0, NULL)) != NULL)
		{
		while (( sensor_name = (gchar *) g_dir_read_name(dir)) != NULL )
			{
			if (strncmp(sensor_name, "sensor", 6) == 0)
				{
				sensor_path = g_build_filename(UNINORTH_DIR, sensor_name,NULL);
				if (strncmp(sensor_name + 8, "temperature", 11) == 0)
					{
					// linux <= 2.6.12 had [cg]pu_temperature instead of
					// sensor[12]_temperature
                    
					// TODO: read sensorN_location instead of fixed positions
					if (   sensor_name[6] == '1'
					    || (strncmp(sensor_name, "cpu", 3)) == 0
					   )
						gkrellm_sensors_add_sensor(SENSOR_TEMPERATURE,
								sensor_path, "cpu topside",
								id, 0, UNINORTH_INTERFACE,
								1.0, 0.0, NULL, "CPU");
					else if (   sensor_name[6] == '2'
					         || (strncmp(sensor_name, "gpu", 3)) == 0
					        ) 
						gkrellm_sensors_add_sensor(SENSOR_TEMPERATURE,
								sensor_path, "gpu on die",
								id, 0, UNINORTH_INTERFACE,
								1.0, 0.0, NULL, "GPU");
					}
				g_free(sensor_path);
				}
			}
		g_dir_close(dir);
		}

	/* Windfarm (PowerMac G5, others?)) */
	cnt = 0;
	do
		{
		snprintf(pbuf, sizeof(pbuf), WINDFARM_DIR, cnt++);
		if ((dir = g_dir_open(pbuf, 0, NULL)) != NULL)
			{
			while ((sensor_name = (gchar *) g_dir_read_name(dir)) != NULL)
				{
				sensor_path = g_build_filename(pbuf, sensor_name, NULL);
				if (strncmp(sensor_name, "cpu-temp-", 9) == 0)
					{
					sscanf(sensor_name + 9, "%d", &id);
					snprintf(id_name, sizeof(id_name), "CPU %d", id);
					gkrellm_sensors_add_sensor(SENSOR_TEMPERATURE,
								   sensor_path,
								   id_name, id, 0,
								   WINDFARM_INTERFACE,
								   1.0, 0.0, NULL, id_name);
					}
				else if (strcmp(sensor_name, "backside-temp") == 0)
					{
					gkrellm_sensors_add_sensor(SENSOR_TEMPERATURE,
								   sensor_path,
								   "Backside", 0, 0,
								   WINDFARM_INTERFACE,
								   1.0, 0.0, NULL, "Backside");
					}
				else if (strcmp(sensor_name, "backside-fan") == 0)
					{
					gkrellm_sensors_add_sensor(SENSOR_FAN,
								   sensor_path,
								   "Backside", 0, 0,
								   WINDFARM_INTERFACE,
								   1.0, 0.0, NULL, "Backside");
					}
				else if (strncmp(sensor_name, "cpu-front-fan-", 14) == 0)
					{
					sscanf(sensor_name + 14, "%d", &id);
					snprintf(id_name, sizeof(id_name), "CPU Front %d", id);
					gkrellm_sensors_add_sensor(SENSOR_FAN,
								   sensor_path,
								   id_name, id, 0,
								   WINDFARM_INTERFACE,
								   1.0, 0.0, NULL, id_name);
					}
				else if (strncmp(sensor_name, "cpu-rear-fan-", 13) == 0)
					{
					sscanf(sensor_name + 13, "%d", &id);
					snprintf(id_name, sizeof(id_name), "CPU Rear %d", id);
					gkrellm_sensors_add_sensor(SENSOR_FAN,
								   sensor_path,
								   id_name, id, 0,
								   WINDFARM_INTERFACE,
								   1.0, 0.0, NULL, id_name);
					}
				else if (strncmp(sensor_name, "cpu-pump-", 9) == 0)
					{
					/* <johill> well, a fan is just an airpump too, right ;) */
					sscanf(sensor_name + 9, "%d", &id);
					snprintf(id_name, sizeof(id_name), "CPU Pump %d", id);
					gkrellm_sensors_add_sensor(SENSOR_FAN,
								   sensor_path,
								   id_name, id, 0,
								   WINDFARM_INTERFACE,
								   1.0, 0.0, NULL, id_name);
					}
				else if (strcmp(sensor_name, "hd-temp") == 0)
					{
					gkrellm_sensors_add_sensor(SENSOR_TEMPERATURE,
								   sensor_path,
								   "Harddisk", 0, 0,
								   WINDFARM_INTERFACE,
								   1.0, 0.0, NULL, "Harddisk");
					}
				else if (strcmp(sensor_name, "slots-fan") == 0)
					{
					gkrellm_sensors_add_sensor(SENSOR_FAN,
								   sensor_path,
								   "Slots", 0, 0,
								   WINDFARM_INTERFACE,
								   1.0, 0.0, NULL, "Slots");
					}
				else if (strcmp(sensor_name, "drive-bay-fan") == 0)
					{
					gkrellm_sensors_add_sensor(SENSOR_FAN,
								   sensor_path,
								   "Drive Bay", 0, 0,
								   WINDFARM_INTERFACE,
								   1.0, 0.0, NULL, "Drive Bay");
					}
				else if (strncmp(sensor_name, "cpu-voltage-", 12) == 0)
					{
					sscanf(sensor_name + 12, "%d", &id);
					snprintf(id_name, sizeof(id_name), "CPU %d", id);
					gkrellm_sensors_add_sensor(SENSOR_VOLTAGE,
								   sensor_path,
								   id_name, id, 0,
								   WINDFARM_INTERFACE,
								   1.0, 0.0, NULL, id_name);
					}
				g_free(sensor_path);
				}
			g_dir_close(dir);
			}
		}
		while (dir != NULL);

#ifdef HAVE_LIBSENSORS
	if (!_GK.without_libsensors && libsensors_init())
		{
		gkrellm_sensors_config_migrate_connect(sensors_config_migrate,
				SENSOR_NAMES_LIBSENSORS);
		return TRUE;
		}
#else
	if (_GK.debug_level & DEBUG_SENSORS)
		printf("libsensors support is not compiled in.\n");

#endif

	if ((dir = g_dir_open(SENSORS_DIR, 0, NULL)) == NULL)
		{
		sysfs_sensors_init();
		return TRUE;
		}
	while ((chip_name = (gchar *) g_dir_read_name(dir)) != NULL)
		{
		fixed_chip_name = g_strdup(chip_name);
		sensors_proc_name_fix(fixed_chip_name);

		path = g_build_filename(SENSORS_DIR, chip_name, NULL);
		chip_dir = g_dir_open(path, 0, NULL);
		if (!chip_dir)
			{
			g_free(path);
			continue;
			}
		if (_GK.debug_level & DEBUG_SENSORS)
			printf("lm_sensors dir: %s\n", path);

		get_volt_default(chip_name, &voltdefault, &voltdefaultsize);
		while ((sensor_name = (gchar *) g_dir_read_name(chip_dir)) != NULL)
			{
			if (!strncmp(sensor_name, "temp", 4))
				type = SENSOR_TEMPERATURE;
			else if (   !strncmp(sensor_name, "fan", 3)
					 && (  !sensor_name[3]
						|| (isdigit(sensor_name[3]) && !sensor_name[4])
						)
					)
				type = SENSOR_FAN;
			else if (check_voltage_name(sensor_name, &id))
				type = SENSOR_VOLTAGE;
			else
				continue;

			factor = 1.0;
			offset = 0;
			default_label = vref = NULL;

			if (type == SENSOR_VOLTAGE)
				{
				if (id < voltdefaultsize)
					{
					default_label = voltdefault[id].name;
					factor = voltdefault[id].factor;
					offset = voltdefault[id].offset;
					vref = voltdefault[id].vref;
					}
				else
					default_label =  sensor_name;
				}
			sensor_path = g_strdup_printf("%s/%s", path, sensor_name);
			snprintf(id_name, sizeof(id_name), "%s/%s",
						fixed_chip_name, sensor_name);
			gkrellm_sensors_add_sensor(type, sensor_path, id_name,
					id, 0, 0,
					factor, offset, vref, default_label);

			if (_GK.debug_level & DEBUG_SENSORS)
				printf("%s %s %d %d\n",
							sensor_path, id_name, id, type);
			g_free(sensor_path);
			}
		g_dir_close(chip_dir);
		g_free(path);
		g_free(fixed_chip_name);
		}
	g_dir_close(dir);
	return TRUE;
	}
