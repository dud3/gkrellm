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

#include "gkrellm.h"
#include "gkrellm-private.h"
#include "gkrellm-sysdeps.h"


#define	DEFAULT_DATA_FORMAT	(_("$t - $f free"))
#define	ALT1_DATA_FORMAT (_("$t - $u used"))
#define	ALT2_DATA_FORMAT (_("$t - $U"))


  /* Values for force_fs_check	*/
#define	FORCE_REDRAW	1
#define	FORCE_UPDATE	2

#define	FS_MOUNTING_ENABLED(fs)	\
				((fs)->fstab_mounting || *((fs)->launch_umount.command))

typedef struct
	{
	gchar		*directory;
	gchar		*device;
	gchar		*type;
	gchar		*options;
	}
	Mount;

typedef struct
	{
	gint		idx;
	GkrellmPanel *panel;
	GkrellmDecalbutton *md_button,
				*eject_button,
				*drawer_button;
	GkrellmDecal *mount_decal,
				*eject_decal,
				*label_decal,
				*data_decal;
	GkrellmKrell *krell;
	gchar		*label,			/* Actual utf8 label */
				*label_shadow;	/* Shadow label for gdk_draw functions */
	gboolean	label_is_data,
				restore_label,
				mouse_entered;

	Mount		mount;
	gboolean	fstab_mounting;
	GkrellmLauncher	launch_mount,
				launch_umount;

	GkrellmAlert *alert;

	gboolean	secondary,
				show_if_mounted,
				is_mounted,
				ejectable,
				is_nfs_fs;
	gchar		*eject_device;
	gint		eject_pending;
	gint		x_eject_button_target;

	GString		*pipe_gstring;		/* output of mount commands */

	gulong		krell_factor;		/* avoid krell math overflow */

	gboolean	busy;
	glong		blocks,
				bfree,
				bavail,
				bsize;
	}
	FSmon;

static void cb_alert_config(GkrellmAlert *ap, FSmon *fs);

static GkrellmMonitor
				*mon_fs;

static GList	*fs_mon_list,
				*mounts_list;
static GList	*fstab_list;

static gint		uid;

void			(*get_mounts_list)(),
				(*get_fsusage)(),
				(*get_fstab_list)();
gboolean		(*get_fstab_modified)();


/* If ejecting is available via an ioctl() or if there is an eject command,
|  set these up in gkrellm_sys_fs_init() by calling gkrellm_fs_setup_eject().
*/
void			(*eject_cdrom_func)(),
				(*close_cdrom_func)();
static gchar	*eject_cdrom_command,
				*close_cdrom_command;
static gboolean	cdrom_thread_busy;		/* for the cdrom_funcs */


static GtkWidget
				*fs_main_vbox,
				*fs_secondary_vbox;

static gboolean	fs_check_timeout	= 2,
				nfs_check_timeout	= 16;
static gint		check_tick;

static gint		secondary_monitors_shown;

static gint		n_fs_monitors;
static gint		force_fs_check;
static FSmon	*fs_in_motion;
static gint		x_fs_motion;
static gint		x_moved;
static gint		x_eject_button_open,
				x_eject_button_closed;

static gint		x_scroll;
static gint		data_decal_width;
static gint		cdrom_auto_eject;
static gint		binary_units;
static gboolean	mounting_supported = TRUE,
				ejecting_supported = FALSE;
static gboolean	have_secondary_panels;
static gchar	*data_format,
				*data_format_locale;

static gint		style_id;

static gchar *remote_fs_types[]	=
	{
	"cifs",
	"nfs",
	"smbfs"
	};


static gboolean
setup_fs_interface(void)
    {
#ifdef WIN32
	uid = 0; /* nothing comparable available on windows */
#else
	uid = getuid();	/* only real root is allowed to mount/umount always */
#endif
	if (!get_fsusage && !_GK.client_mode && gkrellm_sys_fs_init())
		{
		get_fsusage = gkrellm_sys_fs_get_fsusage;
		get_mounts_list = gkrellm_sys_fs_get_mounts_list;
		get_fstab_list = gkrellm_sys_fs_get_fstab_list;
		get_fstab_modified = gkrellm_sys_fs_fstab_modified;
		}
	return get_fsusage ? TRUE : FALSE;
	}

void
gkrellm_fs_client_divert(void (*get_fsusage_func)(),
                void (*get_mounts_func)(), void (*get_fstab_func)(),
                gboolean (*fstab_modified_func)())
	{
	get_fsusage = get_fsusage_func;
	get_mounts_list = get_mounts_func;
	get_fstab_list = get_fstab_func;
	get_fstab_modified = fstab_modified_func;
	}

void
gkrellm_fs_setup_eject(gchar *eject_tray, gchar *close_tray,
			void (*eject_func)(), void (*close_func)())
	{
	eject_cdrom_command = g_strdup(eject_tray);
	close_cdrom_command = g_strdup(close_tray);
	eject_cdrom_func = eject_func;
	close_cdrom_func = close_func;
	if (eject_cdrom_command || eject_cdrom_func)
		ejecting_supported = TRUE;
	}

void
gkrellm_fs_add_to_mounts_list(gchar *dir, gchar *dev, gchar *type)
	{
	Mount	*m;

	m = g_new0(Mount, 1);
	m->directory = g_strdup(dir);
	m->device = g_strdup(dev);
	m->type = g_strdup(type);
	mounts_list = g_list_append(mounts_list, m);
	}

void
gkrellm_fs_add_to_fstab_list(gchar *dir, gchar *dev, gchar *type, gchar *opt)
	{
	Mount	*m;

	m = g_new0(Mount, 1);
	m->directory = g_strdup(dir);
	m->device = g_strdup(dev);
	m->type = g_strdup(type);
	m->options =  g_strdup(opt);
	fstab_list = g_list_append(fstab_list, m);
	}

void
gkrellm_fs_assign_fsusage_data(gpointer fspointer,
			glong blocks, glong bavail, glong bfree, glong bsize)
	{
	FSmon	*fs = (FSmon *) fspointer;

	fs->blocks = blocks;
	fs->bavail = bavail;
	fs->bfree  = bfree;
	fs->bsize  = bsize;
	}

void
gkrellm_fs_mounting_unsupported(void)
	{
	mounting_supported = FALSE;
	}

/* ======================================================================== */


static Mount *
in_fstab_list(gchar *s)
	{
	GList	*list;
	Mount	*m;

	for (list = fstab_list; list; list = list->next)
		{
		m = (Mount *)list->data;
		if (strcmp(s, m->directory) == 0)
			return m;
		}
	return NULL;
	}

static void
refresh_mounts_list(void)
	{
	Mount	*m;

	while (mounts_list)
		{
		m = (Mount *) mounts_list->data;
		g_free(m->directory);
		g_free(m->device);
		g_free(m->type);
		g_free(mounts_list->data);
		mounts_list = g_list_remove(mounts_list, mounts_list->data);
		}
	(*get_mounts_list)();
	}

static void
refresh_fstab_list(void)
	{
	Mount	*m;

	while (fstab_list)
		{
		m = (Mount *) fstab_list->data;
		g_free(m->device);
		g_free(m->directory);
		g_free(m->type);
		g_free(m->options);
		g_free(m);
		fstab_list = g_list_remove(fstab_list, fstab_list->data);
		}
	(*get_fstab_list)();
	}

static gint
fs_is_mounted(FSmon *fs)
	{
	Mount	*m_fs, *m_mounted;
	GList	*list;
	gint	i;

	fs->is_mounted = FALSE;
	m_fs = &fs->mount;
	for (list = mounts_list; list; list = list->next)
		{
		m_mounted = (Mount *) list->data;
		if (strcmp(m_fs->directory, m_mounted->directory))
			continue;
		fs->is_mounted = TRUE;
		fs->is_nfs_fs = FALSE;
		for (i = 0; i < (sizeof(remote_fs_types) / sizeof(gchar *)); ++i)
			{
			if (!strcmp(m_mounted->type, remote_fs_types[i]))
				{
				fs->is_nfs_fs = TRUE;
				break;
				}
			}
		}
	return fs->is_mounted;
	}

static GkrellmSizeAbbrev	fs_decimal_abbrev[] =
	{
	{ MB_SIZE(10),		MB_SIZE(1),		"%.2fM" },
	{ GB_SIZE(1),		MB_SIZE(1),		"%.0fM" },
	{ GB_SIZE(10),		GB_SIZE(1),		"%.2fG" },
	{ GB_SIZE(100),		GB_SIZE(1),		"%.1fG" },
	{ TB_SIZE(1),		GB_SIZE(1),		"%.0fG" },
	{ TB_SIZE(10),		TB_SIZE(1),		"%.2fT" },
	{ TB_SIZE(100),		TB_SIZE(1),		"%.1fT" }
	};

static GkrellmSizeAbbrev	fs_binary_abbrev[] =
	{
	{ MiB_SIZE(10),		MiB_SIZE(1),	"%.2fM" },
	{ GiB_SIZE(1),		MiB_SIZE(1),	"%.0fM" },
	{ GiB_SIZE(10),		GiB_SIZE(1),	"%.2fG" },
	{ GiB_SIZE(100),	GiB_SIZE(1),	"%.1fG" },
	{ TiB_SIZE(1),		GiB_SIZE(1),	"%.0fG" },
	{ TiB_SIZE(10),		TiB_SIZE(1),	"%.2fT" },
	{ TiB_SIZE(100),	TiB_SIZE(1),	"%.1fT" }
	};

static gint
format_fs_data(FSmon *fs, gchar *src_string, gchar *buf, gint size)
	{
	glong		b, u, a;
	gint		len;
	gchar		*s;
	gchar		tbuf[32], ubuf[32], abuf[32];
	gfloat		bsize, val;
	GkrellmSizeAbbrev *tbl;
	size_t		tbl_size;

	if (!buf || size < 1)
		return -1;
	--size;
	*buf = '\0';
	if (!src_string)
		return -1;

	b = fs->blocks;
	u = fs->blocks - fs->bfree;
	a = fs->bavail;					/* Can be negative on BSD	*/
	bsize = (gfloat) fs->bsize;

	tbl = binary_units ? &fs_binary_abbrev[0] : &fs_decimal_abbrev[0];
	tbl_size = binary_units
			? (sizeof(fs_binary_abbrev) / sizeof(GkrellmSizeAbbrev))
			: (sizeof(fs_decimal_abbrev) / sizeof(GkrellmSizeAbbrev));

	gkrellm_format_size_abbrev(tbuf, sizeof(tbuf), (gfloat) b * bsize,
				tbl, tbl_size);
	gkrellm_format_size_abbrev(ubuf, sizeof(ubuf), (gfloat) u * bsize,
				tbl, tbl_size);
	gkrellm_format_size_abbrev(abuf, sizeof(abuf), (gfloat) a * bsize,
				tbl, tbl_size);

	for (s = src_string; *s != '\0' && size > 0; ++s)
		{
		len = 1;
		if (*s == '$' && *(s + 1) != '\0')
			{
			switch(*(s + 1))
				{
				case 'D':
					if (fs->mount.directory)
						len = snprintf(buf, size, "%s", fs->mount.directory);
					break;
				case 'l':
				case 'L':
					len = snprintf(buf, size, "%s", fs->label_shadow);
					break;
				case 't':
					len = snprintf(buf, size, "%s", tbuf);
					break;
				case 'u':
					len = snprintf(buf, size, "%s", ubuf);
					break;
				case 'U':
					if (u + a > 0)
						val = 100.0 * (gfloat) u / (gfloat) (u + a);
					else
						val = 0;
					len = snprintf(buf, size, "%.0f%%", val);
					break;
				case 'f':
					len = snprintf(buf, size, "%s", abuf);
					break;
				case 'F':
					if (u + a > 0)
						val = 100.0 * (gfloat) a / (gfloat) (u + a);
					else
						val = 0;
					len = snprintf(buf, size, "%.0f%%", val);
					break;
				case 'H':
					len = snprintf(buf, size, "%s",
									gkrellm_sys_get_host_name());
					break;
				default:
					*buf = *s;
					if (size > 1)
						{
						*(buf + 1) = *(s + 1);
						++len;
						}
					break;
				}
			++s;
			}
		else
			*buf = *s;
		size -= len;
		buf += len;
		}
	*buf = '\0';
	return u + 1;
	}

  /* Draw the fs label or toggle the fs total blocks and blocks avail.
  */
static gint
fs_draw_decal_text(FSmon *fs, gint value)
	{
	GkrellmDecal		*d;
	GkrellmTextstyle	ts_save;
	gchar				buf[128];
	gint				x_off, w;

	if (value == 0)
		{
		gkrellm_make_decal_invisible(fs->panel, fs->data_decal);
		d = fs->label_decal;
		gkrellm_make_decal_visible(fs->panel, d);
		gkrellm_decal_text_set_offset(d, 0, 0);
		gkrellm_draw_decal_markup(fs->panel, d, fs->label_shadow);
		}
	else if (!fs->busy)
		{
		gkrellm_make_decal_invisible(fs->panel, fs->label_decal);
		d = fs->data_decal;
		gkrellm_make_decal_visible(fs->panel, d);
		ts_save = d->text_style;
		d->text_style = *gkrellm_meter_alt_textstyle(style_id);

		format_fs_data(fs, data_format_locale, buf, sizeof(buf));
		gkrellm_decal_scroll_text_set_markup(fs->panel, d, buf);
		gkrellm_decal_scroll_text_get_size(d, &w, NULL);
		if (w > d->w)
			x_off = d->w / 3 - x_scroll;
		else
			x_off = 0;
		gkrellm_decal_text_set_offset(d, x_off, 0);

		d->text_style = ts_save;
		}
	return w;
	}

static void
cb_command_process(GkrellmAlert *alert, gchar *src, gchar *dst, gint len,
			FSmon *fs)
	{
	format_fs_data(fs, src, dst, len);
	}

static gpointer
close_cdrom_thread(void *device)
	{
	(*close_cdrom_func)((gchar *) device);
	cdrom_thread_busy = FALSE;
	return NULL;
	}

static void
close_tray(FSmon *fs)
	{
	Mount			*m;
	static gchar	*close_target;
	gchar			buf[512];

	close_target = fs->eject_device;
	if (close_cdrom_command)
		{
		snprintf(buf, sizeof(buf), close_cdrom_command,
			*close_target ? close_target : fs->mount.directory);
		g_spawn_command_line_async(buf, NULL /* GError */);
		}
	else if (close_cdrom_func && !cdrom_thread_busy)
		{
		if (!*close_target && (m = in_fstab_list(fs->mount.directory)) != NULL)
			close_target = m->device;
		if (*close_target)
			{
			cdrom_thread_busy = TRUE;
			g_thread_new("close_cdrom", close_cdrom_thread, close_target);
			}
		}
	}

static gpointer
eject_cdrom_thread(void *device)
	{
	(*eject_cdrom_func)((gchar *) device);
	cdrom_thread_busy = FALSE;
	return NULL;
	}

static void
eject_tray(FSmon *fs)
	{
	Mount			*m;
	static gchar	*eject_target;
	gchar			buf[512];

	eject_target = fs->eject_device;
	if (eject_cdrom_command)
		{
		snprintf(buf, sizeof(buf), eject_cdrom_command,
			*eject_target ? eject_target : fs->mount.directory);
		g_spawn_command_line_async(buf, NULL /* GError */);
		}
	else if (eject_cdrom_func && !cdrom_thread_busy)
		{
		if (!*eject_target && (m = in_fstab_list(fs->mount.directory)) != NULL)
			eject_target = m->device;
		if (*eject_target)
			{
			cdrom_thread_busy = TRUE;
			g_thread_new("eject_cdrom", eject_cdrom_thread, eject_target);
			}
		}
	}

static void
accumulate_pipe_gstring(FSmon *fs)
	{
	gchar	buf[512];
	gint	n;

	n = fread(buf, 1, sizeof(buf) - 1, fs->launch_mount.pipe);
	buf[n] = '\0';
	if (n > 0)
		{
		if (fs->pipe_gstring)
			g_string_append(fs->pipe_gstring, buf);
		else
			fs->pipe_gstring = g_string_new(buf);
		}
	if (feof(fs->launch_mount.pipe))
		{
		pclose(fs->launch_mount.pipe);
		fs->launch_mount.pipe = NULL;
		}
	}

static void
pipe_command(FSmon *fs, gchar *command)
	{
	gchar	buf[512];

	if (fs->launch_mount.pipe)	/* Still running? */
		return;
	snprintf(buf, sizeof(buf), "%s 2>&1", command);
	if ((fs->launch_mount.pipe = popen(buf, "r")) == NULL)
		return;
#ifndef WIN32
	fcntl(fileno(fs->launch_mount.pipe), F_SETFL, O_NONBLOCK);
#endif
	}

static void
mount_command(FSmon *fs)
	{
	gchar	cmd[CFG_BUFSIZE];

	if (! FS_MOUNTING_ENABLED(fs))
		return;
	if (fs->is_mounted)
		{
		if (fs->fstab_mounting)
			snprintf(cmd, sizeof(cmd), "umount '%s'", fs->mount.directory);
		else
			snprintf(cmd, sizeof(cmd), "%s", fs->launch_umount.command);
		fs->label_is_data = FALSE;
		fs_draw_decal_text(fs, 0);
		pipe_command(fs, cmd);
		if (cdrom_auto_eject)
			fs->eject_pending = GK.timer_ticks + 5;	/* at least 1/2 sec delay*/
		}
	else
		{
		if (fs->ejectable)
			close_tray(fs);
		if (fs->fstab_mounting)
			snprintf(cmd, sizeof(cmd), "mount '%s'", fs->mount.directory);
		else
			snprintf(cmd, sizeof(cmd), "%s", fs->launch_mount.command);
		fs->blocks = fs->bfree = fs->bavail = fs->bsize = 0;
		pipe_command(fs, cmd);
		}
	force_fs_check = FORCE_REDRAW;	/* An update triggers when pipe closes */
	}

static void
hide_secondary_monitors(void)
	{
	FSmon	*fs;
	GList	*list;

	if (!secondary_monitors_shown)
		return;
	secondary_monitors_shown = FALSE;
	gkrellm_freeze_side_frame_packing();
	for (list = fs_mon_list; list; list = list->next)
		{
		fs = (FSmon *) list->data;
		if (fs->secondary && (!fs_is_mounted(fs) || !fs->show_if_mounted))
			gkrellm_panel_hide(fs->panel);
		}
	gkrellm_thaw_side_frame_packing();
	}

static void
show_secondary_monitors(void)
	{
	FSmon	*fs;
	GList	*list;

	if (secondary_monitors_shown)
		return;
	secondary_monitors_shown = TRUE;
	gkrellm_freeze_side_frame_packing();
	for (list = fs_mon_list; list; list = list->next)
		{
		fs = (FSmon *) list->data;
		if (fs->secondary)
			gkrellm_panel_show(fs->panel);
		}
	gkrellm_thaw_side_frame_packing();
	}

static gpointer
get_fsusage_thread(void *data)
	{
	FSmon	*fs = (FSmon *) data;

	(*get_fsusage)(fs, fs->mount.directory);
	fs->busy = FALSE;
	return NULL;
	}

static gboolean
animate_eject_button(FSmon *fs, gboolean force_close)
	{
	gint	dx, target;

	if (force_close)
		target = x_eject_button_closed;
	else
		target = fs->x_eject_button_target;
	dx = target - fs->eject_decal->x;
	if (dx > 0)
		gkrellm_move_decal(fs->panel, fs->eject_decal,
			fs->eject_decal->x + 1 + dx / 4, fs->eject_decal->y);
	else if (dx < 0)
		gkrellm_move_decal(fs->panel, fs->eject_decal,
			fs->eject_decal->x - 1 + dx / 4, fs->eject_decal->y);
	if (fs->eject_decal->x < x_eject_button_closed)
		gkrellm_show_button(fs->eject_button);
	else
		gkrellm_hide_button(fs->eject_button);
	if (fs->eject_decal->x != target)
		return TRUE;
	return FALSE;
	}

static void
fs_update(void)
	{
	FSmon			*fs;
	GkrellmPanel	*p;
	GkrellmKrell	*k;
	GList			*list;
	glong			used, avail;
	gint			full_scale, index, w_scroll, w;
	gboolean		fs_check, nfs_check, force_check, force_draw,
					mounting_enabled;

	if (!fs_mon_list)
		return;

	w = w_scroll = 0;
	for (list = fs_mon_list; list; list = list->next)
		{
		fs = (FSmon *) list->data;
		if (fs->label_is_data && !fs_in_motion)
			{
			w = fs_draw_decal_text(fs, 1);
			if (w > w_scroll)
				w_scroll = w;
			gkrellm_draw_panel_layers(fs->panel);
			}
		}
	if (!fs_in_motion)
		{
		if (w_scroll > data_decal_width)
			x_scroll = (x_scroll + ((gkrellm_update_HZ() < 7) ? 2 : 1))
					% (w_scroll - data_decal_width / 3);
		else
			x_scroll = 0;
		}
	if (GK.second_tick)
		++check_tick;
	fs_check = (check_tick % fs_check_timeout) ? FALSE : TRUE;
	if (_GK.client_mode)
		nfs_check = fs_check;
	else
		nfs_check = (check_tick % nfs_check_timeout) ? FALSE : TRUE;

	if (!force_fs_check && (!GK.second_tick || (!fs_check && !nfs_check)))
		return;
//g_debug("fs update %d nfs %d force %d\n", fs_check, nfs_check, force_fs_check);

	refresh_mounts_list();

	force_check = force_draw = FALSE;

	for (list = fs_mon_list; list; list = list->next)
		{
		fs = (FSmon *) list->data;
		p  = fs->panel;
		k = fs->krell;
		mounting_enabled = FS_MOUNTING_ENABLED(fs);
		if (fs_is_mounted(fs))
			{
			if (mounting_enabled)
				{	/* Blink it while busy or pipe is open.	*/
				if (   (fs->launch_mount.pipe || fs->busy)
					&& fs->md_button->cur_index == D_MISC_FS_MOUNTED
				   )
					index = D_MISC_FS_UMOUNTED;
				else
					index = D_MISC_FS_MOUNTED;
				gkrellm_set_decal_button_index(fs->md_button, index);
				}
			else
				{
				if (fs->busy && fs->md_button->cur_index == D_MISC_LED1)
					index = D_MISC_LED0;
				else
					index = mounting_supported ? D_MISC_LED1 : D_MISC_BLANK;
				gkrellm_set_decal_button_index(fs->md_button, index);
				}
			if (   force_fs_check == FORCE_UPDATE
				|| (fs_check && !fs->is_nfs_fs)
				|| (nfs_check && fs->is_nfs_fs)
			   )
				{
				if (!fs->is_nfs_fs || _GK.client_mode)
					(*get_fsusage)(fs, fs->mount.directory);
				else if (!fs->busy)
					{
					fs->busy = TRUE;
					g_thread_new("get_fsusage", get_fsusage_thread, fs);
					}
				fs->krell_factor = fs->blocks > 2097152 ? 1024 : 1;
				}

			avail = fs->bavail >= 0 ? fs->bavail : 0;
			used = fs->blocks - fs->bfree;
			full_scale = (gint) (used + avail) / fs->krell_factor;
			used /= fs->krell_factor;
			gkrellm_set_krell_full_scale(k, full_scale, 1);

			if (!fs->busy)
				{
				if (   (fs_in_motion && fs->label_is_data && x_moved)
					|| fs->mouse_entered
				   )
					gkrellm_update_krell(p, k, 0);
				else
					gkrellm_update_krell(p, k, used);
				if (full_scale > 0)
					gkrellm_check_alert(fs->alert,
							100.0 * (gfloat) used / (gfloat) full_scale);
				}
			else
				force_draw = TRUE;

			if (fs->secondary && fs->show_if_mounted)
				gkrellm_panel_show(fs->panel);
			if (fs->eject_decal)
				force_draw |= animate_eject_button(fs, mounting_supported);
			}
		else	/* not mounted */
			{
			gkrellm_reset_alert(fs->alert);
			if (mounting_enabled)
				{	/* Blink it while pipe is open.	*/
				if (   fs->launch_mount.pipe
					&& fs->md_button->cur_index == D_MISC_FS_UMOUNTED
				   )
					index = D_MISC_FS_MOUNTED;
				else
					index = D_MISC_FS_UMOUNTED;
				gkrellm_set_decal_button_index(fs->md_button, index);
				}
			else
				gkrellm_set_decal_button_index(fs->md_button, D_MISC_LED0);
			gkrellm_set_krell_full_scale(k, 100, 1);	/* Arbitrary > 0 */
			gkrellm_update_krell(p, k, 0);
			if (!secondary_monitors_shown && fs->secondary)
				gkrellm_panel_hide(fs->panel);
			if (fs->eject_decal)
				force_draw |= animate_eject_button(fs, FALSE);
			}
		gkrellm_set_button_sensitive(fs->md_button, mounting_enabled);
		if (!fs->label_is_data && fs != fs_in_motion)
			fs_draw_decal_text(fs, 0);

		if (_GK.client_mode)
			{
			if (fs->blocks == 0)
				gkrellm_remove_krell(p, k);
			else
				gkrellm_insert_krell(p, k, FALSE);
			}

		gkrellm_draw_panel_layers(p);

		if (   fs->ejectable
			&& fs->eject_pending && fs->eject_pending < GK.timer_ticks
		   )
			{
			eject_tray(fs);
			fs->eject_pending = 0;
			}
		if (fs->launch_mount.pipe)
			{
			accumulate_pipe_gstring(fs);
			if (fs->launch_mount.pipe == NULL)	/* Command is done */
				{
				if (fs->pipe_gstring)
					{
					gkrellm_message_dialog(_("GKrellM Mount Error"),
							fs->pipe_gstring->str);
					g_string_free(fs->pipe_gstring, 1);
					fs->pipe_gstring = NULL;
					}
				force_check = TRUE;
				}
			else
				force_draw = TRUE; 	/* Keep it going */
			}
		}
	force_fs_check = force_check ? FORCE_UPDATE : force_draw;
	}

static gint
fs_expose_event(GtkWidget *widget, GdkEventExpose *ev)
	{
	FSmon	*fs;
	GList	*list;

	for (list = fs_mon_list; list; list = list->next)
		{
		fs = (FSmon *) list->data;
		if (widget == fs->panel->drawing_area)
			{
			gdk_draw_drawable(widget->window, gkrellm_draw_GC(1),
					fs->panel->pixmap,
					ev->area.x, ev->area.y, ev->area.x, ev->area.y,
					ev->area.width, ev->area.height);
			break;
			}
		}
	return FALSE;
	}

static gint
cb_panel_enter(GtkWidget *w, GdkEventButton *ev, FSmon *fs)
	{
	if (fs->label_is_data)
		{
		fs->mouse_entered = TRUE;
		force_fs_check = FORCE_REDRAW;
		}
	if (fs->ejectable)
		{
		fs->x_eject_button_target = x_eject_button_open;
		force_fs_check = FORCE_REDRAW;
		}
	if (fs != fs_in_motion)
		gkrellm_show_button(fs->drawer_button);
	return FALSE;
	}

static gint
cb_panel_leave(GtkWidget *w, GdkEventButton *ev, FSmon *fs)
	{
	if (fs->mouse_entered)
		force_fs_check = FORCE_REDRAW;
	fs->mouse_entered = FALSE;
	if (fs->ejectable)
		{
		fs->x_eject_button_target = x_eject_button_closed;
		force_fs_check = FORCE_REDRAW;
		}
	return FALSE;
	}


static void
cb_drawer_button(GkrellmDecalbutton *button, FSmon *fs)
	{
	if (secondary_monitors_shown)
		hide_secondary_monitors();
	else
		show_secondary_monitors();
	}

static gint
cb_panel_scroll(GtkWidget *widget, GdkEventScroll *ev)
	{
	if (ev->direction == GDK_SCROLL_UP)
		hide_secondary_monitors();
	if (ev->direction == GDK_SCROLL_DOWN)
		show_secondary_monitors();
	return FALSE;
	}

static gint
cb_panel_release(GtkWidget *widget, GdkEventButton *ev, FSmon *fs)
	{
	if (ev->button == 3)
		return TRUE;
	if (fs_in_motion)
		{
		if (fs_in_motion->restore_label)
			{
			if (fs_in_motion->label_is_data)
				gkrellm_config_modified();
			fs_in_motion->label_is_data = FALSE;
			fs_draw_decal_text(fs_in_motion, 0);
			gkrellm_show_button(fs_in_motion->drawer_button);
			gkrellm_draw_panel_layers(fs_in_motion->panel);
			}
		fs_in_motion->restore_label = TRUE;
		}
	force_fs_check = FORCE_REDRAW;		/* Move krells back */
	fs_in_motion = NULL;
	x_moved = 0;
	return FALSE;
	}

static gint
cb_panel_press(GtkWidget *widget, GdkEventButton *ev, FSmon *fs)
	{
	GkrellmDecal	*d;

	d = fs->eject_decal ? fs->eject_decal : fs->mount_decal;

	if (ev->button == 3 && ev->x < d->x)
		{
		gkrellm_open_config_window(mon_fs);
		return TRUE;
		}
#if 0
	if (   ev->button == 1
		&& (   (fs->drawer_button
				&& gkrellm_in_decal(fs->drawer_button->decal, ev))
			|| ev->x >= d->x
		   )
	   )
		return FALSE;
#endif

	if (!fs->label_is_data)
		{
		fs->label_is_data = TRUE;
		fs->restore_label = FALSE;
		fs->mouse_entered = TRUE;
		gkrellm_config_modified();
		}
	x_fs_motion = ev->x;
	fs_draw_decal_text(fs, 1);
	gkrellm_draw_panel_layers(fs->panel);
	fs_in_motion = fs;
	x_moved = 0;
	gkrellm_hide_button(fs->drawer_button);
	return TRUE;
	}

static gint
cb_panel_motion(GtkWidget *widget, GdkEventButton *ev)
	{
	GdkModifierType	state;
	GList			*list;
	FSmon			*fs;
	GkrellmDecal	*d;
	gchar			buf[128];
	gint			w, x_delta	= 0;

	state = ev->state;
	if (   !fs_in_motion
		|| !(state & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK))
		|| !fs_in_motion->label_is_data
	   )
		{
		fs_in_motion = NULL;
		return FALSE;
		}

	d = fs_in_motion->data_decal;
	format_fs_data(fs_in_motion, data_format_locale, buf, sizeof(buf));
	gkrellm_decal_scroll_text_get_size(d, &w, NULL);
	if (w > d->w)
		{
		x_delta = ev->x - x_fs_motion;
		x_fs_motion = ev->x;
		d->x_off += x_delta;
		if (d->x_off < -w)
			d->x_off = -w;
		if (d->x_off > d->w)
			d->x_off = d->w;
		x_scroll = d->w / 3 - d->x_off;
		for (list = fs_mon_list; list; list = list->next)
			{
			fs = (FSmon *) list->data;
			if (fs->label_is_data)
				{
				fs_draw_decal_text(fs, 1);
				gkrellm_draw_panel_layers(fs->panel);
				}
			}
		if (x_moved > 0)
			fs_in_motion->restore_label = FALSE;
		}
	if (x_moved == 0)
		force_fs_check = FORCE_REDRAW;	/* Move krells out of the way */
	x_moved += (x_delta > 0) ? x_delta : -x_delta;
	return FALSE;
	}

static void
cb_fs_mount_button(GkrellmDecalbutton *button)
	{
	if (button)
		mount_command((FSmon *) button->data);
	}

static void
cb_fs_eject_button(GkrellmDecalbutton *button, FSmon *fs)
	{
	if (button)
		eject_tray(fs);
	}

static void
cb_fs_close_tray(GkrellmDecalbutton *button, FSmon *fs)
	{
	if (button)
		close_tray(fs);
	}

static void
fs_monitor_create(GtkWidget *vbox, FSmon *fs, gint index, gint first_create)
	{
	GkrellmStyle		*style;
	GkrellmTextstyle	*ts;
	GkrellmMargin		*m;
	GkrellmPanel		*p;
	gchar				buf[256];
	gint				h, label_x_position, label_y_off;
	gint				h_data, h_label;

	if (first_create)
		fs->panel = gkrellm_panel_new0();
	p = fs->panel;
	fs->idx = index;
	++n_fs_monitors;
	fs->krell_factor = 1;

	style = gkrellm_meter_style(style_id);
	ts = gkrellm_meter_textstyle(style_id);
	m = gkrellm_get_style_margins(style);

	gkrellm_panel_label_get_position(style, &label_x_position, &label_y_off);

	format_fs_data(fs, data_format_locale, buf, sizeof(buf));
	fs->data_decal = gkrellm_create_decal_text_markup(p, buf,
			ts, style, -1,
			(label_y_off > 0) ? label_y_off : -1,
			-1);
	gkrellm_decal_get_size(fs->data_decal, NULL, &h_data);

	fs->label_decal = gkrellm_create_decal_text_markup(p, fs->label_shadow,
			ts, style, -1,
			(label_y_off > 0) ? label_y_off : -1,
			-1);
	gkrellm_decal_get_size(fs->label_decal, NULL, &h_label);

	if (h_data > h_label)
		gkrellm_move_decal(p, fs->label_decal, fs->label_decal->x,
				fs->label_decal->y + (h_data - h_label + 1) / 2);
	else if (h_data < h_label)
		gkrellm_move_decal(p, fs->data_decal, fs->data_decal->x,
				fs->data_decal->y + (h_label - h_data + 1) / 2);

	fs->mount_decal = gkrellm_create_decal_pixmap(p,
			gkrellm_decal_misc_pixmap(), gkrellm_decal_misc_mask(),
			N_MISC_DECALS, style, -1, -1);
	fs->mount_decal->x =
				gkrellm_chart_width() - fs->mount_decal->w - m->right;

	if (fs->ejectable)
		{
		fs->eject_decal = gkrellm_create_decal_pixmap(p,
				gkrellm_decal_misc_pixmap(), gkrellm_decal_misc_mask(),
				N_MISC_DECALS, style, -1, -1);
		if (mounting_supported)
			{
			x_eject_button_closed = fs->mount_decal->x;
			x_eject_button_open = fs->mount_decal->x - fs->eject_decal->w + 1;
			}
		else
			{
			x_eject_button_closed = gkrellm_chart_width() - 2;
			x_eject_button_open = x_eject_button_closed - fs->eject_decal->w;
			}
		fs->x_eject_button_target = x_eject_button_closed;
		fs->eject_decal->x = x_eject_button_closed;
		}

	/* Usable width to determine various scrolling parameters.
	*/
	data_decal_width = fs->mount_decal->x - fs->data_decal->x;

	fs->krell = gkrellm_create_krell(p,
						gkrellm_krell_meter_piximage(style_id), style);
	gkrellm_monotonic_krell_values(fs->krell, FALSE);

	gkrellm_panel_configure(p, NULL, style);
	gkrellm_panel_create(vbox, mon_fs, p);

	fs->md_button = gkrellm_make_decal_button(p, fs->mount_decal,
			cb_fs_mount_button, fs, D_MISC_FS_UMOUNTED, D_MISC_FS_PRESSED);
	if (index == 0 && have_secondary_panels)
		{
		if ((h = p->h / 2) > 7)
			h = 7;
		fs->drawer_button = gkrellm_make_scaled_button(p, NULL,
					cb_drawer_button, fs, TRUE, TRUE,
					0, 0, 0,	/* NULL image => builtin depth & indices */
					(gkrellm_chart_width() - 20) / 2, 0,
					20, h);
		/* Make it appear under the label decal */
		gkrellm_remove_decal(p, fs->drawer_button->decal);
		gkrellm_insert_decal_nth(p, fs->drawer_button->decal, 0);
		}
	if (fs->eject_decal)
		{
		fs->eject_button = gkrellm_make_decal_button(p, fs->eject_decal,
			cb_fs_eject_button, fs, D_MISC_BUTTON_OUT, D_MISC_BUTTON_IN);
		gkrellm_hide_button(fs->eject_button);
		if (close_cdrom_command || close_cdrom_func)
			gkrellm_decal_button_right_connect(fs->eject_button,
						cb_fs_close_tray, fs);
		}
	if (first_create)
		{
		g_signal_connect(G_OBJECT(p->drawing_area), "expose_event",
				G_CALLBACK(fs_expose_event), NULL);
		g_signal_connect(G_OBJECT(p->drawing_area),"button_press_event",
				G_CALLBACK(cb_panel_press), fs);
		g_signal_connect(G_OBJECT(p->drawing_area),"button_release_event",
				G_CALLBACK(cb_panel_release), fs);
		g_signal_connect(G_OBJECT(p->drawing_area),"scroll_event",
				G_CALLBACK(cb_panel_scroll), fs);
		g_signal_connect(G_OBJECT(p->drawing_area),"motion_notify_event",
				G_CALLBACK(cb_panel_motion), NULL);
		g_signal_connect(G_OBJECT(p->drawing_area), "enter_notify_event",
				G_CALLBACK(cb_panel_enter), fs);
		g_signal_connect(G_OBJECT(p->drawing_area), "leave_notify_event",
				G_CALLBACK(cb_panel_leave), fs);
		if (   !secondary_monitors_shown && fs->secondary
			&& (!fs_is_mounted(fs) || !fs->show_if_mounted))
			gkrellm_panel_hide(fs->panel);
		}

	fs_draw_decal_text(fs, 0);
	force_fs_check = FORCE_UPDATE;

	if (fs->launch_mount.command == NULL)
		fs->launch_mount.command = g_strdup("");
	if (fs->launch_umount.command == NULL)
		fs->launch_umount.command = g_strdup("");
	}

static void
free_fsmon_strings(FSmon *fs)
	{
	g_free(fs->label);
	g_free(fs->label_shadow);
	g_free(fs->mount.directory);
	g_free(fs->eject_device);
	g_free(fs->launch_mount.command);
	g_free(fs->launch_umount.command);
	}

static void
destroy_fs_monitor(FSmon *fs)
	{
	gkrellm_reset_alert(fs->alert);
	free_fsmon_strings(fs);
	gkrellm_panel_destroy(fs->panel);
	g_free(fs);
	--n_fs_monitors;
	}

static void
fs_create(GtkWidget *vbox, gint first_create)
	{
	GList	*list;
	FSmon	*fs;
	gint	i;

	if (fs_main_vbox == NULL)
		{
		fs_main_vbox = gtk_vbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), fs_main_vbox, FALSE, FALSE, 0);
		gtk_widget_show(fs_main_vbox);

		fs_secondary_vbox = gtk_vbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), fs_secondary_vbox, FALSE, FALSE, 0);
		gtk_widget_show(fs_secondary_vbox);
		secondary_monitors_shown = FALSE;
		}
	n_fs_monitors = 0;
	for (i = 0, list = fs_mon_list; list; ++i, list = list->next)
		{
		fs = (FSmon *)list->data;
		fs_monitor_create(fs->secondary ? fs_secondary_vbox : fs_main_vbox,fs,
					i, first_create);
		}
	if (g_list_length(fs_mon_list) == 0)
		gkrellm_spacers_hide(mon_fs);
	}

#define	FS_CONFIG_KEYWORD	"fs"

static void
cb_alert_trigger(GkrellmAlert *alert, FSmon *fs)
	{
	/* Full panel alert, default decal.
	*/
	alert->panel = fs->panel;
	}

static void
create_alert(FSmon *fs)
	{
	fs->alert = gkrellm_alert_create(NULL, fs->label,
				_("Percent Usage"),
				TRUE, FALSE, TRUE,
				100, 10, 1, 10, 0);
	gkrellm_alert_trigger_connect(fs->alert, cb_alert_trigger, fs);
	gkrellm_alert_config_connect(fs->alert, cb_alert_config, fs);
	gkrellm_alert_command_process_connect(fs->alert, cb_command_process, fs);
	}

static void
fs_config_save(FILE *f)
	{
	GList	*list;
	FSmon	*fs;
	gchar	quoted_label[64], quoted_dir[512];

	for (list = fs_mon_list; list; list = list->next)
		{
		fs = (FSmon *) list->data;
		snprintf(quoted_label, sizeof(quoted_label), "\"%s\"", fs->label);
		snprintf(quoted_dir, sizeof(quoted_dir), "\"%s\"",fs->mount.directory);
		fprintf(f, "%s %s %s %d %d %d %d %d\n", FS_CONFIG_KEYWORD,
				quoted_label, quoted_dir,
				fs->fstab_mounting, fs->secondary,
				fs->show_if_mounted, fs->label_is_data, fs->ejectable);
		if (*(fs->launch_mount.command))
			fprintf(f, "%s mount_command %s\n", FS_CONFIG_KEYWORD,
					fs->launch_mount.command);
		if (*(fs->launch_umount.command))
			fprintf(f, "%s umount_command %s\n", FS_CONFIG_KEYWORD,
					fs->launch_umount.command);
		if (*(fs->eject_device))
			fprintf(f, "%s eject_device %s\n", FS_CONFIG_KEYWORD,
					fs->eject_device);
		if (fs->alert)
			gkrellm_save_alertconfig(f, fs->alert,
					FS_CONFIG_KEYWORD, quoted_label);
		}
	if (!_GK.client_mode)
		{
		fprintf(f, "%s fs_check_timeout %d\n", FS_CONFIG_KEYWORD,
				fs_check_timeout);
		fprintf(f, "%s nfs_check_timeout %d\n", FS_CONFIG_KEYWORD,
				nfs_check_timeout);
		fprintf(f, "%s auto_eject %d\n", FS_CONFIG_KEYWORD, cdrom_auto_eject);
		}
	fprintf(f, "%s binary_units %d\n", FS_CONFIG_KEYWORD, binary_units);
	fprintf(f, "%s data_format %s\n", FS_CONFIG_KEYWORD, data_format);
	}

static gboolean
fstab_user_permission(Mount *m)
	{
	struct stat my_stat;

	stat(m->device, &my_stat);
	if (   strstr(m->options, "user")
		|| (strstr(m->options, "owner") && my_stat.st_uid == uid)
	   )
		return TRUE;
	return FALSE;
	}

static gint
fix_fstab_mountable_changed(FSmon *fs)
	{
	Mount	*m;

	if (!mounting_supported)
		return FALSE;
	m = in_fstab_list(fs->mount.directory);
	if (   (!m || (!fstab_user_permission(m) && uid != 0))
		&& fs->fstab_mounting
	   )
		{
		fs->fstab_mounting = FALSE;
		return TRUE;
		}
	return FALSE;
	}

static FSmon *
lookup_fs(gchar *name)
	{
	GList	*list;
	FSmon	*fs;

	if (!name)
		return NULL;
	for (list = fs_mon_list; list; list = list->next)
		{
		fs = (FSmon *) list->data;
		if (!strcmp(fs->label, name))
			return fs;
		}
	return NULL;
	}

static void
fs_config_load(gchar *arg)
	{
	static FSmon	*fs_prev;
	FSmon			*fs;
	gchar			*cut_label, *cut_dir;
	gchar			config[32], item[CFG_BUFSIZE];
	gchar			name[64], item1[CFG_BUFSIZE];
	gint 			n;

	if ((n = sscanf(arg, "%31s %[^\n]", config, item)) != 2)
		return;

	if (!strcmp(config, "fs_check_timeout"))
		{
		sscanf(item, "%d", &fs_check_timeout);
		if (fs_check_timeout < 2)
			fs_check_timeout = 2;
		}
	else if (!strcmp(config, "nfs_check_timeout"))
		{
		sscanf(item, "%d", &nfs_check_timeout);
		if (nfs_check_timeout < 5)
			nfs_check_timeout = 5;
		}
	else if (!strcmp(config, "auto_eject"))
		sscanf(item, "%d", &cdrom_auto_eject);
	else if (!strcmp(config, "binary_units"))
		sscanf(item, "%d", &binary_units);
	else if (!strcmp(config, "data_format"))
		gkrellm_locale_dup_string(&data_format, item, &data_format_locale);
	else if (fs_prev && !strcmp(config, "mount_command"))
		gkrellm_dup_string(&fs_prev->launch_mount.command, item);
	else if (fs_prev && !strcmp(config, "umount_command"))
		gkrellm_dup_string(&fs_prev->launch_umount.command, item);
	else if (fs_prev && !strcmp(config, "eject_device"))
		{
		if (fs_prev->ejectable)
			gkrellm_dup_string(&fs_prev->eject_device, item);
		}
	else if (!strcmp(config, GKRELLM_ALERTCONFIG_KEYWORD))
		{
		if (   sscanf(item, "\"%63[^\"]\" %[^\n]", name, item1) == 2
			&& (fs = lookup_fs(name)) != NULL
		   )
			{
			if (!fs->alert)
				create_alert(fs);
			gkrellm_load_alertconfig(&fs->alert, item1);
			}
		}
	else
		{
		if (   (cut_label = gkrellm_cut_quoted_string(arg, &arg)) != NULL
			&& (cut_dir = gkrellm_cut_quoted_string(arg, &arg)) != NULL
	       )
			{
			fs = g_new0(FSmon, 1);
			gkrellm_locale_dup_string(&fs->label, cut_label,&fs->label_shadow);

			sscanf(arg, "%d %d %d %d %d", &fs->fstab_mounting,
					&fs->secondary, &fs->show_if_mounted,
					&fs->label_is_data, &fs->ejectable);
			if (fs->fstab_mounting > 1)		/* pre 2.0.0 config fix */
				fs->fstab_mounting = FALSE;
			if (!ejecting_supported)
				fs->ejectable = FALSE;
			if (!mounting_supported)
				fs->fstab_mounting = fs->show_if_mounted = FALSE;
			if (fs->secondary)
				have_secondary_panels = TRUE;
			fs->mount.directory = g_strdup(cut_dir);
			fs->restore_label = fs->label_is_data;

			fix_fstab_mountable_changed(fs);
			fs->krell_factor = 1;
			fs->launch_mount.command = g_strdup("");
			fs->launch_umount.command = g_strdup("");
			fs->eject_device = g_strdup("");
			fs_mon_list = g_list_append(fs_mon_list, fs);
			fs_prev = fs;	/* XXX */
			}
		}
	}


/* --------------------------------------------------------------------- */

enum
	{
	NAME_COLUMN,
	MOUNT_POINT_COLUMN,
	SHOW_COLUMN,
	FSTAB_COLUMN,
	MOUNT_COMMAND_COLUMN,
	UMOUNT_COMMAND_COLUMN,
	EJECTABLE_COLUMN,
	DEVICE_COLUMN,
	FSMON_COLUMN,
	ALERT_COLUMN,
	SHOW_DATA_COLUMN,
	VISIBLE_COLUMN,
	IMAGE_COLUMN,
	N_COLUMNS
	};

static GtkTreeView		*treeview;
static GtkTreeRowReference *row_reference;
static GtkTreeSelection	*selection;

static GtkWidget
				*label_entry,
				*dir_combo_box,
				*mount_entry,
				*umount_entry,
				*mounting_button,
				*ejectable_button,
				*device_entry,
				*secondary_button,
				*show_button,
				*delete_button,
				*new_apply_button;

static GtkWidget	*alert_button;

static GtkWidget	*data_format_combo_box;

static gboolean	(*original_row_drop_possible)();


static void
set_tree_store_model_data(GtkTreeStore *tree, GtkTreeIter *iter, FSmon *fs)
	{
	gtk_tree_store_set(tree, iter,
			NAME_COLUMN, fs->label,
			MOUNT_POINT_COLUMN, fs->mount.directory,
			SHOW_COLUMN, fs->show_if_mounted,
			FSTAB_COLUMN, fs->fstab_mounting,
			MOUNT_COMMAND_COLUMN, fs->launch_mount.command,
			UMOUNT_COMMAND_COLUMN, fs->launch_umount.command,
			EJECTABLE_COLUMN, fs->ejectable,
			DEVICE_COLUMN, fs->eject_device,
			FSMON_COLUMN, fs,
			ALERT_COLUMN, fs->alert,
			SHOW_DATA_COLUMN, fs->label_is_data,
			VISIBLE_COLUMN, TRUE,
			-1);
	if (fs->alert)
		gtk_tree_store_set(tree, iter,
				IMAGE_COLUMN, gkrellm_alert_pixbuf(),
				-1);
	}

static GtkTreeModel *
create_model(void)
	{
	GtkTreeStore	*tree;
	GtkTreeIter		iter, citer;
	GList			*list;
	FSmon			*fs;

	tree = gtk_tree_store_new(N_COLUMNS,
				G_TYPE_STRING, G_TYPE_STRING,
				G_TYPE_BOOLEAN, G_TYPE_BOOLEAN,
				G_TYPE_STRING, G_TYPE_STRING,
				G_TYPE_BOOLEAN, G_TYPE_STRING,
				G_TYPE_POINTER, G_TYPE_POINTER,
				G_TYPE_BOOLEAN,
				G_TYPE_BOOLEAN, GDK_TYPE_PIXBUF);

	gtk_tree_store_append(tree, &iter, NULL);
	gtk_tree_store_set(tree, &iter,
			NAME_COLUMN, _("Primary"),
			VISIBLE_COLUMN, FALSE,
			-1);
	for (list = fs_mon_list; list; list = list->next)
		{
		fs = (FSmon *) list->data;
		if (fs->secondary)
			continue;
		gtk_tree_store_append(tree, &citer, &iter);
		set_tree_store_model_data(tree, &citer, fs);
		}

	gtk_tree_store_append(tree, &iter, NULL);
	gtk_tree_store_set(tree, &iter,
			NAME_COLUMN, _("Secondary"),
			VISIBLE_COLUMN, FALSE,
			-1);
	for (list = fs_mon_list; list; list = list->next)
		{
		fs = (FSmon *) list->data;
		if (!fs->secondary)
			continue;
		gtk_tree_store_append(tree, &citer, &iter);
		set_tree_store_model_data(tree, &citer, fs);
		}
	return GTK_TREE_MODEL(tree);
	}

static void
change_row_reference(GtkTreeModel *model, GtkTreePath *path)
	{
	gtk_tree_row_reference_free(row_reference);
	if (model && path)
		row_reference = gtk_tree_row_reference_new(model, path);
	else
		row_reference = NULL;
	}

static void
cb_set_alert(GtkWidget *button, gpointer data)
	{
	GtkTreeModel	*model;
	GtkTreePath		*path;
	GtkTreeIter		iter;
	FSmon			*fs;

	if (!row_reference)
		return;
	model = gtk_tree_view_get_model(treeview);
	path = gtk_tree_row_reference_get_path(row_reference);
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_model_get(model, &iter, FSMON_COLUMN, &fs, -1);

	if (!fs->alert)
		create_alert(fs);
	gkrellm_alert_config_window(&fs->alert);
	gtk_tree_store_set(GTK_TREE_STORE(model), &iter,
						ALERT_COLUMN, fs->alert, -1);
	}

static gboolean
get_child_iter(GtkTreeModel *model, gchar *parent_node, GtkTreeIter *citer)
	{
	GtkTreePath     *path;
	GtkTreeIter     iter;

	path = gtk_tree_path_new_from_string(parent_node);
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_path_free(path);
	return gtk_tree_model_iter_children(model, citer, &iter);
	}

  /* Callback for a created or destroyed alert.  Find the sensor in the model
  |  and set the IMAGE_COLUMN.
  */
static void
cb_alert_config(GkrellmAlert *ap, FSmon *fs)
	{
	GtkTreeModel    *model;
	GtkTreeIter     iter;
	FSmon			*fs_test;
	GdkPixbuf       *pixbuf;
	gchar           node[2];
	gint            i;

	if (!gkrellm_config_window_shown())
		return;
	model = gtk_tree_view_get_model(treeview);
	pixbuf = ap->activated ? gkrellm_alert_pixbuf() : NULL;
	for (i = 0; i < 2; ++i)
		{
		node[0] = '0' + i;      /* toplevel Primary or Secondary node */
		node[1] = '\0';
		if (get_child_iter(model, node, &iter))
			do
				{
				gtk_tree_model_get(model, &iter, FSMON_COLUMN, &fs_test, -1);
				if (fs != fs_test)
					continue;
				gtk_tree_store_set(GTK_TREE_STORE(model), &iter,
							IMAGE_COLUMN, pixbuf, -1);
				return;
				}
			while (gtk_tree_model_iter_next(model, &iter));
		}
	}

  /* Watch what is going into the directory combo entry, compare it to
  |  fstab entries and accordingly set sensitivity of the mounting_button.
  */
static void
cb_combo_changed(GtkComboBox *widget, gpointer user_data)
	{
	Mount	*m;
	gchar	*s;
	GtkWidget *entry;

	if (!mounting_supported || _GK.client_mode)
		return;

	entry = gtk_bin_get_child(GTK_BIN(dir_combo_box));
	s = gkrellm_gtk_entry_get_text(&entry);
	m = in_fstab_list(s);
	if (m && (fstab_user_permission(m) || uid == 0))
		{
		gtk_widget_set_sensitive(mounting_button, TRUE);
		if (GTK_TOGGLE_BUTTON(mounting_button)->active)
			{
			gtk_entry_set_text(GTK_ENTRY(mount_entry), "");
			gtk_entry_set_text(GTK_ENTRY(umount_entry), "");
			gtk_widget_set_sensitive(mount_entry, FALSE);
			gtk_widget_set_sensitive(umount_entry, FALSE);
			}
		}
	else
		{
		if (GTK_TOGGLE_BUTTON(mounting_button)->active)
			gtk_toggle_button_set_active(
						GTK_TOGGLE_BUTTON(mounting_button), FALSE);
		else
			{
			gtk_widget_set_sensitive(mount_entry, TRUE);
			gtk_widget_set_sensitive(umount_entry, TRUE);
			}
		gtk_widget_set_sensitive(mounting_button, FALSE);
		}
	}

static void
cb_mount_button_clicked(GtkWidget *widget)
	{
	if (!mounting_supported || _GK.client_mode)
		return;
	if (GTK_TOGGLE_BUTTON(mounting_button)->active)
		{
		gtk_entry_set_text(GTK_ENTRY(mount_entry), "");
		gtk_entry_set_text(GTK_ENTRY(umount_entry), "");
		gtk_widget_set_sensitive(mount_entry, FALSE);
		gtk_widget_set_sensitive(umount_entry, FALSE);
		if (device_entry)
			{
			gtk_entry_set_text(GTK_ENTRY(device_entry), "");
			gtk_widget_set_sensitive(device_entry, FALSE);
			}
		}
	else
		{
		gtk_widget_set_sensitive(mount_entry, TRUE);
		gtk_widget_set_sensitive(umount_entry, TRUE);
		if (   ejectable_button
			&& GTK_TOGGLE_BUTTON(ejectable_button)->active
		   )
			gtk_widget_set_sensitive(device_entry, TRUE);
		}
	}

static void
cb_ejectable_button_clicked(GtkWidget *widget)
	{
	gboolean	fstab_mounting;

	if (!mounting_supported || _GK.client_mode)
		return;
	fstab_mounting = GTK_TOGGLE_BUTTON(mounting_button)->active;
	if (GTK_TOGGLE_BUTTON(ejectable_button)->active)
		{
		gtk_widget_set_sensitive(device_entry, !fstab_mounting);
		}
	else
		{
		gtk_entry_set_text(GTK_ENTRY(device_entry), "");
		gtk_widget_set_sensitive(device_entry, FALSE);
		}
	}

static void
cb_secondary_button_clicked(GtkWidget *widget)
	{
	if (!mounting_supported)	/* Show button is in client mode */
		return;
	if (GTK_TOGGLE_BUTTON(secondary_button)->active)
		gtk_widget_set_sensitive(show_button, TRUE);
	else
		{
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(show_button), FALSE);
		gtk_widget_set_sensitive(show_button, FALSE);
		}
	}

static void
reset_entries(gboolean level0)
	{
	gtk_entry_set_text(GTK_ENTRY(label_entry), "");
	gtk_combo_box_set_active(GTK_COMBO_BOX(dir_combo_box), -1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(secondary_button), level0);
	if (mounting_button)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mounting_button),FALSE);
	if (show_button)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(show_button), FALSE);
	if (ejectable_button)
		{
		gtk_toggle_button_set_active(
					GTK_TOGGLE_BUTTON(ejectable_button), FALSE);
		if (device_entry)
			gtk_entry_set_text(GTK_ENTRY(device_entry), "");
		}
	if (mount_entry)
		{
		gtk_entry_set_text(GTK_ENTRY(mount_entry), "");
		gtk_entry_set_text(GTK_ENTRY(umount_entry), "");
		}
	change_row_reference(NULL, NULL);
	gtk_tree_selection_unselect_all(selection);
	}

static FSmon *
fs_new_from_model(GtkTreeModel *model, GtkTreeIter *iter)
	{
	FSmon	*fs;
	gchar	*label;

	fs = g_new0(FSmon, 1);
	gtk_tree_model_get(model, iter,
				NAME_COLUMN, &label,
				MOUNT_POINT_COLUMN, &fs->mount.directory,
				SHOW_COLUMN, &fs->show_if_mounted,
				FSTAB_COLUMN, &fs->fstab_mounting,
				MOUNT_COMMAND_COLUMN, &fs->launch_mount.command,
				UMOUNT_COMMAND_COLUMN, &fs->launch_umount.command,
				EJECTABLE_COLUMN, &fs->ejectable,
				DEVICE_COLUMN, &fs->eject_device,
				ALERT_COLUMN, &fs->alert,
				SHOW_DATA_COLUMN, &fs->label_is_data,
				-1);
	gkrellm_locale_dup_string(&fs->label, label, &fs->label_shadow);
	g_free(label);
	return fs;
	}

static void
cb_tree_selection_changed(GtkTreeSelection *selection, gpointer data)
	{
	GtkTreeIter		iter;
	GtkTreeModel	*model;
	GtkTreePath		*path;
	FSmon			*fs;
	gint			*indices, depth, secondary;

	if (!gtk_tree_selection_get_selected(selection, &model, &iter))
		{
		reset_entries(FALSE);
		gtk_button_set_label(GTK_BUTTON(new_apply_button), GTK_STOCK_NEW);
		gtk_widget_set_sensitive(delete_button, FALSE);
		gtk_widget_set_sensitive(alert_button, FALSE);
		return;
		}
	path = gtk_tree_model_get_path(model, &iter);
	indices = gtk_tree_path_get_indices(path);
	secondary = indices[0];
	depth = gtk_tree_path_get_depth(path);
// g_debug("selection: indices=[%d,%d]:%d, path=%s\n",
//			indices[0], indices[1], gtk_tree_path_get_depth(path),
//			gtk_tree_path_to_string(path));
	change_row_reference(model, path);
	gtk_tree_path_free(path);

	if (depth == 1)
		{
		reset_entries(secondary);
		gtk_button_set_label(GTK_BUTTON(new_apply_button), GTK_STOCK_NEW);
		gtk_widget_set_sensitive(delete_button, FALSE);
		gtk_widget_set_sensitive(alert_button, FALSE);
		return;
		}
	gtk_button_set_label(GTK_BUTTON(new_apply_button), GTK_STOCK_APPLY);
	gtk_widget_set_sensitive(delete_button, TRUE);
	gtk_widget_set_sensitive(alert_button, TRUE);

	fs = fs_new_from_model(model, &iter);
	if (!secondary)
		fs->show_if_mounted = FALSE;	/* in case dragged secondary->primary*/

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(secondary_button),
					secondary);
	gtk_entry_set_text(GTK_ENTRY(label_entry), fs->label);
	gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(dir_combo_box))),
			fs->mount.directory);
	if (show_button)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(show_button),
					fs->show_if_mounted);
	if (mounting_button)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mounting_button),
					fs->fstab_mounting);
	if (ejectable_button)
		{
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ejectable_button),
					fs->ejectable);
		if (device_entry)
			{
			gtk_entry_set_text(GTK_ENTRY(device_entry), fs->eject_device);
			if (!fs->ejectable)
				gtk_widget_set_sensitive(device_entry, FALSE);
			}
		}
	if (mount_entry)
		{
		gtk_entry_set_text(GTK_ENTRY(mount_entry), fs->launch_mount.command);
		gtk_entry_set_text(GTK_ENTRY(umount_entry), fs->launch_umount.command);
		}
	free_fsmon_strings(fs);
	g_free(fs);
	}

static void
sync_fs_panels(void)
	{
	GtkTreeModel	*model;
	GtkTreePath		*path;
	GtkTreeIter		iter0, iter1, iter;
	GList			*list;
	FSmon			*fs;

	model = gtk_tree_view_get_model(treeview);

	path = gtk_tree_path_new_from_string("0");
	gtk_tree_model_get_iter(model, &iter0, path);
	gtk_tree_path_free(path);

	path = gtk_tree_path_new_from_string("1");
	gtk_tree_model_get_iter(model, &iter1, path);
	gtk_tree_path_free(path);

	if (   gtk_tree_model_iter_has_child(model, &iter1)
		&& !gtk_tree_model_iter_has_child(model, &iter0)
	   )
		{
		gkrellm_config_message_dialog(_("Entry Error"),
N_("At least one primary fs monitor must exist to click on in order to show\n"
"secondary ones.\n"));
		}

	for (list = fs_mon_list; list; list = list->next)
		destroy_fs_monitor((FSmon *) list->data);
	g_list_free(fs_mon_list);
	fs_mon_list = NULL;
	n_fs_monitors = 0;
	have_secondary_panels = gtk_tree_model_iter_has_child(model, &iter1);

	if (gtk_tree_model_iter_children(model, &iter, &iter0))
		{
		do
			{
			fs = fs_new_from_model(model, &iter);
			fs->secondary = FALSE;
			fs->show_if_mounted = FALSE;
			fs_mon_list = g_list_append(fs_mon_list, fs);
			fs_monitor_create(fs_main_vbox, fs, n_fs_monitors, TRUE);
			gtk_tree_store_set(GTK_TREE_STORE(model), &iter,
						FSMON_COLUMN, fs, -1);
			gkrellm_alert_trigger_connect(fs->alert, cb_alert_trigger, fs);
			gkrellm_alert_config_connect(fs->alert, cb_alert_config, fs);
			}
		while (gtk_tree_model_iter_next(model, &iter));
		}

	if (gtk_tree_model_iter_children(model, &iter, &iter1))
		{
		do
			{
			fs = fs_new_from_model(model, &iter);
			fs->secondary = TRUE;
			fs_mon_list = g_list_append(fs_mon_list, fs);
			fs_monitor_create(fs_secondary_vbox, fs, n_fs_monitors, TRUE);
			gtk_tree_store_set(GTK_TREE_STORE(model), &iter,
						FSMON_COLUMN, fs, -1);
			gkrellm_alert_trigger_connect(fs->alert, cb_alert_trigger, fs);
			gkrellm_alert_config_connect(fs->alert, cb_alert_config, fs);
			}
		while (gtk_tree_model_iter_next(model, &iter));
		}
	else
		secondary_monitors_shown = FALSE;

	force_fs_check = FORCE_UPDATE;
	if (g_list_length(fs_mon_list) > 0)
		gkrellm_spacers_show(mon_fs);
	else
		gkrellm_spacers_hide(mon_fs);
	}

static void
add_cb(GtkWidget *widget)
	{
	GtkTreeModel	*model;
	GtkTreePath		*path = NULL;
	GtkTreeIter		iter, parent_iter;
	FSmon			*fs;
	gchar			*label;
	gint			secondary, *indices;
	gboolean		a, b, err = FALSE;
	GtkWidget		*entry;

	fs = g_new0(FSmon, 1);
	fs->launch_mount.command = g_strdup("");
	fs->launch_umount.command = g_strdup("");
	fs->eject_device = g_strdup("");

	label = gkrellm_gtk_entry_get_text(&label_entry);
	gkrellm_locale_dup_string(&fs->label, label, &fs->label_shadow);

	entry = gtk_bin_get_child(GTK_BIN(dir_combo_box));
	fs->mount.directory = g_strdup(gkrellm_gtk_entry_get_text(&entry));

	if (show_button)
		fs->show_if_mounted = GTK_TOGGLE_BUTTON(show_button)->active;
	if (mounting_button)
		fs->fstab_mounting = GTK_TOGGLE_BUTTON(mounting_button)->active;
	if (mount_entry)
		{
		gkrellm_dup_string(&(fs->launch_mount.command),
					gkrellm_gtk_entry_get_text(&mount_entry));
		gkrellm_dup_string(&(fs->launch_umount.command),
					gkrellm_gtk_entry_get_text(&umount_entry));
		}
	if (ejectable_button)
		{
		fs->ejectable = GTK_TOGGLE_BUTTON(ejectable_button)->active;
		if (fs->ejectable && !fs->fstab_mounting && device_entry)
			gkrellm_dup_string(&(fs->eject_device),
					gkrellm_gtk_entry_get_text(&device_entry));
		}
	if (!*(fs->label) || !*(fs->mount.directory))
		{
		gkrellm_config_message_dialog(_("Entry Error"),
#if !defined(WIN32)
			_("Both a label and a mount point must be entered."));
#else
			"Both a label and a disk must be entered.");
#endif
		err = TRUE;
		}
	a = (*(fs->launch_mount.command) != '\0');
	b = (*(fs->launch_umount.command) != '\0');
	if (mounting_supported && !_GK.client_mode && ((a && !b) || (!a && b)))
		{
		gkrellm_config_message_dialog(_("Entry Error"),
			_("Both mount and umount commands must be entered."));
		err = TRUE;
		}
	if (   mounting_supported && !_GK.client_mode
		&& fs->ejectable && !fs->fstab_mounting && !*fs->eject_device
	   )
		{
		gkrellm_config_message_dialog(_("Entry Error"),
			_("Missing ejectable device entry."));
		err = TRUE;
		}
	if (err)
		{
		free_fsmon_strings(fs);
		g_free(fs);
		return;
		}

	model = gtk_tree_view_get_model(treeview);
	secondary = GTK_TOGGLE_BUTTON(secondary_button)->active;
	if (row_reference)
		{
		path = gtk_tree_row_reference_get_path(row_reference);
		gtk_tree_model_get_iter(model, &iter, path);
		indices = gtk_tree_path_get_indices(path);

		/* Have a row ref, but if user changed secondary button, remove the
		|  row ref node and set path NULL so iter will be set as function of
		|  secondary.  row_ref will be invalidated below.
		*/
		if (indices[0] != secondary)
			{
			gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
			path = NULL;
			}
		}
	if (!path)
		{
		gtk_tree_model_iter_nth_child(model, &parent_iter, NULL, secondary);
		gtk_tree_store_append(GTK_TREE_STORE(model), &iter, &parent_iter);
		}
	set_tree_store_model_data(GTK_TREE_STORE(model), &iter, fs);
	if (!path)
		{
		/* If appending (not editing existing node) leave cursor at root level
		|  and clear the entries anticipating another new entry.
		*/
		path = gtk_tree_path_new_from_string(secondary ? "1" : "0");
		gtk_tree_view_set_cursor(treeview, path, NULL, FALSE);
		gtk_tree_path_free(path);
		reset_entries(secondary);
		}
	free_fsmon_strings(fs);
	g_free(fs);
	sync_fs_panels();
	}

static void
cb_delete(GtkWidget *widget)
	{
	GtkTreeModel	*model;
	GtkTreePath		*path;
	GtkTreeIter		iter;
	FSmon			*fs;

	if (!row_reference)
		return;
	model = gtk_tree_view_get_model(treeview);
	path = gtk_tree_row_reference_get_path(row_reference);
	gtk_tree_model_get_iter(model, &iter, path);

	gtk_tree_model_get(model, &iter, FSMON_COLUMN, &fs, -1);
	gkrellm_alert_destroy(&fs->alert);

	gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
	reset_entries(FALSE);
	sync_fs_panels();
	}

static void
cb_data_format(GtkWidget *widget, gpointer data)
	{
	GList			*list;
	FSmon			*fs;
	GkrellmDecal	*d;
	gchar			*s, buf[256];
	gint			h_data, h_label;
	GtkWidget *entry;

	entry = gtk_bin_get_child(GTK_BIN(data_format_combo_box));
	s = gkrellm_gtk_entry_get_text(&entry);

	/* In case Pango markup tags, don't accept line unless valid markup.
	|  Ie, markup like <span ...> xxx </span> or <b> xxx </b>
	*/

	if (   strchr(s, '<') != NULL
		&& !pango_parse_markup(s, -1, 0, NULL, NULL, NULL, NULL)
	   )
		return;

	if (gkrellm_locale_dup_string(&data_format, s, &data_format_locale))
		{
		for (list = fs_mon_list; list; list = list->next)
			{
			fs = (FSmon *) list->data;
			fs->label_decal->value = -1;	/* Force redraw */

			d = fs->data_decal;

			format_fs_data(fs, data_format_locale, buf, sizeof(buf));
			gkrellm_text_markup_extents(d->text_style.font, buf, strlen(buf),
					NULL, &h_data, NULL, NULL);
			h_data += d->text_style.effect;
			h_label = fs->label_decal->h;

			/* Rebuild fs mon panels if new format string height extents
			|  are wrong for current label/data decals.
			*/
			if (   (d->h >= h_label && h_data != d->h)
			    || (d->h < h_label && h_data >= h_label)
			   )
				{
				sync_fs_panels();
				break;
				}
			if (d->h < h_label && h_data < h_label && d->h != h_data)
				gkrellm_move_decal(fs->panel, d, d->x,
						d->y + (d->h - h_data) / 2);
			}
		}
	}

static void
cb_auto_eject(GtkWidget *button, gpointer data)
	{
	cdrom_auto_eject = GTK_TOGGLE_BUTTON(button)->active;
	}

static void
cb_binary_units(GtkWidget *button, gpointer data)
	{
	binary_units = GTK_TOGGLE_BUTTON(button)->active;
	}

static void
cb_check_interval(GtkWidget *widget, GtkSpinButton *spin)
	{
	fs_check_timeout = gtk_spin_button_get_value_as_int(spin);
	}

static void
cb_nfs_check_interval(GtkWidget *widget, GtkSpinButton *spin)
	{
	nfs_check_timeout = gtk_spin_button_get_value_as_int(spin);
	}

  /* Allow destination drops only on depth 2 paths and don't allow drops from
  |  source depths of 1 (top level nodes).  Note: for some reason if I allow
  |  drops on depth 3 nodes (destination is on top of a second level node) I
  |  am not getting "drag_end" callbacks.
  */
static gboolean
row_drop_possible(GtkTreeDragDest *drag_dest, GtkTreePath *path,
			GtkSelectionData *selection_data)
	{
	GtkTreePath	*src_path;

	if (!row_reference)
		return FALSE;
	src_path = gtk_tree_row_reference_get_path(row_reference);

	if (   gtk_tree_path_get_depth(src_path) == 1
		|| gtk_tree_path_get_depth(path) != 2
	   )
		return FALSE;

	return (*original_row_drop_possible)(drag_dest, path,
									selection_data);
	}

  /* At each drag, divert the original Gtk row_drop_possible function to my
  |  custom row_drop_possible so I can control tree structure.  The original
  |  row_drop_possible function must be restored at "drag_end" else other
  |  monitors doing drag n' drop could get screwed.
  */
static gboolean
cb_drag_begin(GtkWidget *widget, GdkDragContext *context, gpointer data)
	{
	GtkTreeModel			*model;
	GtkTreeDragDestIface	*dest_iface;

	model = gtk_tree_view_get_model(treeview);
	dest_iface = GTK_TREE_DRAG_DEST_GET_IFACE(GTK_TREE_DRAG_DEST(model));
	if (!original_row_drop_possible)
		original_row_drop_possible = dest_iface->row_drop_possible;
	dest_iface->row_drop_possible = row_drop_possible;
	return FALSE;
	}

static gboolean
cb_drag_end(GtkWidget *widget, GdkDragContext *context, gpointer data)
	{
	GtkTreeModel			*model;
	GtkTreeDragDestIface	*dest_iface;

	model = gtk_tree_view_get_model(treeview);
	dest_iface = GTK_TREE_DRAG_DEST_GET_IFACE(GTK_TREE_DRAG_DEST(model));
	dest_iface->row_drop_possible = original_row_drop_possible;

	reset_entries(FALSE);
	sync_fs_panels();
	return FALSE;
	}


static void
create_fs_panels_page(GtkWidget *vbox)
	{
	GtkWidget				*table;
	GtkWidget				*hbox, *hbox1, *vbox1;
	GtkWidget				*label;
	GtkWidget				*scrolled;
	GtkTreeModel			*model;
	GtkCellRenderer			*renderer;
	GList					*list;


	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	vbox1 = gkrellm_gtk_framed_vbox(hbox, NULL, 1, TRUE, 0, 2);

	table = gtk_table_new(2, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox1), table, FALSE, FALSE, 0);

	label = gtk_label_new(_("Label"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
				GTK_SHRINK, GTK_SHRINK, 2, 1);
	label_entry = gtk_entry_new();
//	gtk_entry_set_max_length(GTK_ENTRY(label_entry), 16);
	gtk_widget_set_size_request(label_entry, 100, -1);
	gtk_table_attach_defaults(GTK_TABLE(table), label_entry, 1, 2, 0, 1);

#if !defined(WIN32)
	label = gtk_label_new(_("Mount Point"));
#else
	label = gtk_label_new("Disk");
#endif
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
				GTK_SHRINK, GTK_SHRINK, 2, 1);
	dir_combo_box = gtk_combo_box_entry_new_text();
	gtk_table_attach_defaults(GTK_TABLE(table), dir_combo_box, 1, 2, 1, 2);
	for (list = fstab_list; list; list = list->next)
		{
		gtk_combo_box_append_text(GTK_COMBO_BOX(dir_combo_box),
				((Mount *)list->data)->directory);
		}
	gtk_combo_box_set_active(GTK_COMBO_BOX(dir_combo_box), -1);
	g_signal_connect(G_OBJECT(GTK_COMBO_BOX(dir_combo_box)),
			"changed", G_CALLBACK(cb_combo_changed), NULL);

	vbox1 = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox1, FALSE, FALSE, 0);
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox, FALSE, FALSE, 0);

	gkrellm_gtk_check_button_connected(hbox, &secondary_button, 0,
					TRUE, TRUE, 2, cb_secondary_button_clicked, NULL,
					_("Secondary"));


	if (mounting_supported)
		{
		gkrellm_gtk_check_button_connected(hbox, &show_button, 0,
					TRUE, TRUE, 2, NULL, NULL,
					_("Show if mounted"));
		gtk_widget_set_sensitive(show_button, FALSE);
		if (!_GK.client_mode)
			{
			gkrellm_gtk_check_button_connected(vbox1, &mounting_button, 0,
						FALSE, FALSE, 2, cb_mount_button_clicked, NULL,
						_("Enable /etc/fstab mounting"));
			gtk_widget_set_sensitive(mounting_button, FALSE);
			}
		}

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	if (ejecting_supported && !_GK.client_mode)
		{
		vbox1 = gkrellm_gtk_framed_vbox(hbox, NULL, 1, FALSE, 0, 2);

		gkrellm_gtk_check_button_connected(vbox1, &ejectable_button, 0,
					FALSE, FALSE, 0, cb_ejectable_button_clicked, NULL,
					_("Ejectable"));

		if (mounting_supported)
			{
			hbox1 = gtk_hbox_new(FALSE, 0);
			gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 0);
			label = gtk_label_new(_("Device"));
			gtk_box_pack_start(GTK_BOX(hbox1), label, FALSE, FALSE, 2);
			device_entry = gtk_entry_new();
			gtk_entry_set_max_length(GTK_ENTRY(device_entry), 64);
			gtk_widget_set_size_request(device_entry, 100, -1);
			gtk_box_pack_start(GTK_BOX(hbox1), device_entry,
					FALSE, FALSE, 2);
			}
		}

	if (mounting_supported && !_GK.client_mode)
		{
		vbox1 = gkrellm_gtk_framed_vbox(hbox, NULL, 1, TRUE, 0, 2);
		table = gkrellm_gtk_launcher_table_new(vbox1, 1);  /* Won't have tooltip */
		gkrellm_gtk_config_launcher(table, 0, &mount_entry,
					NULL, _("mount"), NULL);
		gkrellm_gtk_config_launcher(table, 1, &umount_entry,
					NULL, _("umount"), NULL);
		}
	hbox = gtk_hbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

	vbox1 = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), vbox1, FALSE, FALSE, 5);

	gkrellm_gtk_button_connected(vbox1, &new_apply_button, FALSE, FALSE, 4,
			add_cb, NULL, GTK_STOCK_NEW);
	GTK_WIDGET_SET_FLAGS(new_apply_button, GTK_CAN_DEFAULT);
	gtk_widget_grab_default(new_apply_button);

	gkrellm_gtk_button_connected(vbox1, &delete_button, FALSE, FALSE, 4,
			cb_delete, NULL, GTK_STOCK_DELETE);
	gtk_widget_set_sensitive(delete_button, FALSE);

	gkrellm_gtk_alert_button(vbox1, &alert_button, FALSE, FALSE, 4, FALSE,
			cb_set_alert, NULL);
	gtk_widget_set_sensitive(alert_button, FALSE);

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_end(GTK_BOX(hbox), scrolled, TRUE, TRUE, 0);

	model = create_model();

	treeview = GTK_TREE_VIEW(gtk_tree_view_new_with_model(model));
	g_object_unref(G_OBJECT(model));
	gtk_tree_view_set_rules_hint(treeview, TRUE);
	gtk_tree_view_set_reorderable(treeview, TRUE);
	g_signal_connect(G_OBJECT(treeview), "drag_begin",
				G_CALLBACK(cb_drag_begin), NULL);
	g_signal_connect(G_OBJECT(treeview), "drag_end",
				G_CALLBACK(cb_drag_end), NULL);

	renderer = gtk_cell_renderer_text_new();
//	g_object_set(G_OBJECT(renderer), "xalign", 0.0, NULL);
	gtk_tree_view_insert_column_with_attributes(treeview, -1, _("Label"),
				renderer,
				"text", NAME_COLUMN, NULL);
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(treeview, -1, _("Mount Point"),
				renderer,
				"text", MOUNT_POINT_COLUMN,
//				"visible", VISIBLE_COLUMN,
				NULL);

	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_insert_column_with_attributes(treeview, -1, "",
				renderer,
				"pixbuf", IMAGE_COLUMN,
				NULL);

	gtk_container_add(GTK_CONTAINER(scrolled), GTK_WIDGET(treeview));

	selection = gtk_tree_view_get_selection(treeview);
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(selection), "changed",
				G_CALLBACK(cb_tree_selection_changed), NULL);
	}


static gchar *fs_info_text0[] =
{
N_("<h>Mounting\n"),
N_("Enter file system mount points to monitor and enter a label to describe\n"
"the mount point.  The krell shows the ratio of blocks used to total blocks\n"
"available.  Mounting commands can be enabled for mount points in one\n"
"of two ways:\n\n"),

N_("<b>\t1) Mounting using /etc/fstab: "),

N_("If a mount point is in your\n"
"\t/etc/fstab and you have mount permission then mount and umount\n"
"\tcommands can be enabled and executed for that mount point simply\n"
"\tby checking the \"Enable /etc/fstab mounting\" option.\n"
"\tMount table entries in /etc/fstab need the \"user\" or \"owner\" option\n"
"\tset to grant this permission unless GKrellM is run as root.\n"
"\tFor example, if you run GKrellM as a normal user and you want\n"
"\tto be able to mount your floppy, your /etc/fstab could have\n"
"\teither of:\n"),

N_("<i>\t\t/dev/fd0 /mnt/floppy ext2 user,noauto,rw,exec 0 0\n"),
N_("\tor\n"),
N_("<i>\t\t/dev/fd0 /mnt/floppy ext2 user,defaults 0 0\n\n"),

N_("<b>\t2) Mounting using custom commands: "),
N_("If GKrellM is run as root or if\n"
"\tyou have sudo permission to run the mount commands, then a custom\n"
"\tmount command can be entered into the \"mount command\" entry\n"
"\tbox.  A umount command must also be entered if you choose this\n"
"\tmethod.  Example mount and umount entries using sudo:\n"),

N_("<i>\t\tsudo /bin/mount -t msdos /dev/fd0 /mnt/A\n"),
N_("<i>\t\tsudo /bin/umount /mnt/A\n"),

N_("\tNotes: the mount point specified in a custom mount command\n"
   "\t(/mnt/A in this example) must be the same as entered in the\n"
   "\t\"Mount Point\" entry.  You should have the NOPASSWD option set\n"
   "\tin /etc/sudoers if using sudo.\n\n"),

N_("<h>Primary and Secondary Monitors\n"), /* xgettext:no-c-format */
N_("File system monitors can be created as primary (always visible)\n"
	"or secondary which can be hidden and then shown when they are of\n"
	"interest.  For example, you might make primary file system monitors\n"
	"for root, home, or user so they will be always visible, but make\n"
	"secondary monitors for less frequently used mount points such as\n"
	"floppy, zip, backup partitions, foreign file system types, etc.\n"
	"Secondary FS monitors can also be configured to always be visible if they\n"
	"are mounted by checking the \"Show if mounted\" option.   Using this\n"
	"feature you can show the secondary group, mount a file system, and have\n"
	"that FS monitor remain visible even when the secondary group is hidden.\n"
	"A standard cdrom mount will show as 100% full but a monitor for it\n"
	"could be created with mounting enabled just to have the\n"
	"mount/umount convenience.\n\n")
};

static gchar *fs_info_text1[] =
{
N_("<h>Panel Labels\n"),
N_("Substitution variables for the format string for file system panels:\n"),
N_("\t$t    total capacity\n"),
N_("\t$u    used space\n"),
N_("\t$f    free space\n"),
N_("\t$U    used %,\n"),
N_("\t$F    free %\n"),
N_("\t$l    the panel label\n"),
#if !defined(WIN32)
N_("\t$D    the mount point\n"),
#else
N_("\t$D    the disk\n"),
#endif
"\n",
N_("Substitution variables may be used in alert commands.\n"),
"\n",
N_("<h>Mouse Button Actions:\n"),
N_("<b>\tLeft "),
N_("click on a panel to scroll a programmable display\n"),
N_("\t\tof file system capacities (default is total and free space).\n"),
N_("<b>\tWheel "),
N_("Shows and hides the secondary file system monitors.\n")
};

static void
fs_tab_create(GtkWidget *tab_vbox)
	{
	GtkWidget		*tabs;
	GtkWidget		*vbox, *vbox1;
	GtkWidget		*text;
	gint			i;

	row_reference = NULL;
	refresh_fstab_list();

	tabs = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(tabs), GTK_POS_TOP);
	gtk_box_pack_start(GTK_BOX(tab_vbox), tabs, TRUE, TRUE, 0);

	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Panels"));
	create_fs_panels_page(vbox);

//	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Primary"));
//	fill_fs_tab(vbox, &primary_widgets);

//	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Secondary"));
//	fill_fs_tab(vbox, &secondary_widgets);

/* --Setup tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Setup"));

	vbox1 = gkrellm_gtk_category_vbox(vbox,
				_("Options"),
				4, 0, TRUE);
	gkrellm_gtk_check_button_connected(vbox1, NULL,
			binary_units, FALSE, FALSE, 0,
			cb_binary_units, NULL,
			_("Use binary units (MiB, GiG) for reporting disk capacities."));

	if (mounting_supported && ejecting_supported && !_GK.client_mode)
		gkrellm_gtk_check_button_connected(vbox1, NULL,
				cdrom_auto_eject, FALSE, FALSE, 0,
				cb_auto_eject, NULL,
				_("Auto eject when ejectable devices are unmounted"));

	if (!_GK.client_mode)
		{
		vbox1 = gkrellm_gtk_framed_vbox_end(vbox, NULL, 4, FALSE, 0, 2);
		gkrellm_gtk_spin_button(vbox1, NULL,
			(gfloat) fs_check_timeout,
			2.0, 15.0, 1.0, 5.0, 0 /*digits*/, 0 /*width*/,
			cb_check_interval, NULL, FALSE,
			_("Seconds between data updates for local mounted file systems"));
		gkrellm_gtk_spin_button(vbox1, NULL,
			(gfloat) nfs_check_timeout,
			5.0, 60.0, 1.0, 5.0, 0, 0,
			cb_nfs_check_interval, NULL, FALSE,
			_("Seconds between data updates for remote mounted file systems"));
		}

	vbox1 = gkrellm_gtk_category_vbox(vbox,
				_("Format String for Panel Labels"),
				4, 0, TRUE);
	data_format_combo_box = gtk_combo_box_entry_new_text();
	gtk_box_pack_start(GTK_BOX(vbox1), data_format_combo_box, FALSE, FALSE, 2);
	gtk_combo_box_append_text(GTK_COMBO_BOX(data_format_combo_box), data_format);
	gtk_combo_box_append_text(GTK_COMBO_BOX(data_format_combo_box), DEFAULT_DATA_FORMAT);
	gtk_combo_box_append_text(GTK_COMBO_BOX(data_format_combo_box), ALT1_DATA_FORMAT);
	gtk_combo_box_append_text(GTK_COMBO_BOX(data_format_combo_box), ALT2_DATA_FORMAT);
	gtk_combo_box_set_active(GTK_COMBO_BOX(data_format_combo_box), 0);
	g_signal_connect(G_OBJECT(GTK_COMBO_BOX(data_format_combo_box)), "changed",
			G_CALLBACK(cb_data_format), NULL);

/* --Info tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Info"));
	text = gkrellm_gtk_scrolled_text_view(vbox, NULL,
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	if (mounting_supported && !_GK.client_mode)
		for (i = 0; i < sizeof(fs_info_text0)/sizeof(gchar *); ++i)
			gkrellm_gtk_text_view_append(text, _(fs_info_text0[i]));
	for (i = 0; i < sizeof(fs_info_text1)/sizeof(gchar *); ++i)
		gkrellm_gtk_text_view_append(text, _(fs_info_text1[i]));
	}



static GkrellmMonitor	monitor_fs =
	{
	N_("File System"),	/* Name, for config tab.	*/
	MON_FS,				/* Id,  0 if a plugin		*/
	fs_create,			/* The create function		*/
	fs_update,			/* The update function		*/
	fs_tab_create,		/* The config tab create function	*/
	NULL,				/* Instant config */

	fs_config_save,		/* Save user conifg			*/
	fs_config_load,		/* Load user config			*/
	FS_CONFIG_KEYWORD,	/* config keyword			*/

	NULL,				/* Undef 2	*/
	NULL,				/* Undef 1	*/
	NULL,				/* Undef 0	*/

	0,					/* insert_before_id - place plugin before this mon */

	NULL,				/* Handle if a plugin, filled in by GKrellM		*/
	NULL				/* path if a plugin, filled in by GKrellM		*/
	};

GkrellmMonitor *
gkrellm_init_fs_monitor(void)
	{
	monitor_fs.name = _(monitor_fs.name);
	style_id = gkrellm_add_meter_style(&monitor_fs, FS_STYLE_NAME);
	gkrellm_locale_dup_string(&data_format, DEFAULT_DATA_FORMAT,
				&data_format_locale);

	mon_fs = &monitor_fs;
	if (setup_fs_interface())
		{
		refresh_fstab_list();
		refresh_mounts_list();
		return &monitor_fs;
		}
	return NULL;
	}
