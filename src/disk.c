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

#include "gkrellm.h"
#include "gkrellm-private.h"
#include "gkrellm-sysdeps.h"


#define DISK_ASSIGN_BY_DEVICE   0
#define DISK_ASSIGN_NTH         1
#define DISK_ASSIGN_BY_NAME     2

#define	DISK_CONFIG_KEYWORD	"disk"

#define	MIN_GRID_RES		10
#define	MAX_GRID_RES		100000000
#define	DEFAULT_GRID_RES	2000

typedef struct
	{
	gchar			*name,
					*label;			/* Possibly translated name */
	GtkWidget		*vbox;
	GtkWidget		*enable_button;
	GkrellmChart	*chart;
	GkrellmChartdata *read_cd,
					 *write_cd;
	GkrellmChartconfig *chart_config;
	GkrellmAlert	*alert;
	GtkWidget		*alert_config_read_button,
					*alert_config_write_button;
	gboolean		alert_uses_read,
					alert_uses_write,
					new_text_format;

	GkrellmDecal	*temp_decal;
	gint			save_label_position;

	gint			enabled;
	gint			major,
					minor,
					subdisk,
					order;
	gint			new_disk;
	gint			extra_info;
	GkrellmLauncher	launch;
	GtkWidget		*launch_entry,
					*tooltip_entry;
	GtkWidget		*launch_table;

	guint64			rb,
					wb;
	}
	DiskMon;


static void	cb_alert_config(GkrellmAlert *ap, DiskMon *disk);
static void	cb_alert_config_create(GkrellmAlert *ap, GtkWidget *vbox,
					DiskMon *disk);


static GkrellmMonitor	*mon_disk;

static gint		n_disks;
static GList	*disk_mon_list;


static DiskMon	*composite_disk;
static gint		ascent;
static gint		style_id;
static gint		assign_method;
static gboolean	sys_handles_composite_reset;
static gboolean	units_are_blocks;

static void		(*read_disk_data)();
static gchar	*(*name_from_device)();
static gint		(*order_from_name)();



DiskMon *
lookup_disk_by_device(gint major, gint minor)
	{
	DiskMon	*disk;
	GList	*list;

	for (list = disk_mon_list->next; list; list = list->next)
		{
		disk = (DiskMon * ) list->data;
		if (disk->major == major && disk->minor == minor)
			return disk;
		}
	return NULL;
	}

static DiskMon *
lookup_disk_by_name(gchar *name)
	{
	DiskMon	*disk;
	GList	*list;

	for (list = disk_mon_list; list; list = list->next)
		{
		disk = (DiskMon * ) list->data;
		if (!strcmp(name, disk->name))
			return disk;

		/* Pre 2.1.15 config compatibility where translated "Disk" was
		|  written into the config.  XXX remove this eventually.
		*/
		if (   (disk == composite_disk || assign_method == DISK_ASSIGN_NTH)
			&& !strcmp(name, disk->label))
			return disk;
		}
	return NULL;
	}

static DiskMon *
disk_new(gchar *name, gchar *label)
	{
	DiskMon	*disk;

	disk = g_new0(DiskMon, 1);
	disk->name = g_strdup(name);
	disk->label = g_strdup(label);
	disk->launch.command = g_strdup("");
	disk->launch.tooltip_comment = g_strdup("");
	disk->alert_uses_read = disk->alert_uses_write = TRUE;
	disk->extra_info = TRUE;
	return disk;
	}

static DiskMon *
add_disk(gchar *name, gchar *label, gint major, gint minor, gint order)
	{
	DiskMon	*disk;
	GList	*list;
	gint	i;

	if (lookup_disk_by_name(name))
		return NULL;
	disk = disk_new(name, label);

	disk->major = major;
	disk->minor = minor;
	disk->order = order;
	disk->subdisk = -1;
	if (order >= 0)
		{
		++disk->order;		/* Skip the composite disk */
		for (i = 1, list = disk_mon_list->next; list; list = list->next, ++i)
			if (disk->order < ((DiskMon *) list->data)->order)
				break;
		disk_mon_list = g_list_insert(disk_mon_list, disk, i);
		}
	else
		disk_mon_list = g_list_append(disk_mon_list, disk);
	++n_disks;
	return disk;
	}

static DiskMon *
add_subdisk(gchar *subdisk_name, gchar *disk_name, gint subdisk)
	{
	DiskMon	*disk, *sdisk;
	GList	*list = NULL;

	for (list = disk_mon_list->next; list; list = list->next)
		{
		disk = (DiskMon * ) list->data;
		if (!strcmp(disk_name, disk->name))
			break;
		}
	if (!list)
		return NULL;
	sdisk = disk_new(subdisk_name, subdisk_name);
	sdisk->order = disk->order;
	sdisk->subdisk = subdisk;
	for (list = list->next; list; list = list->next)
		{
		disk = (DiskMon * ) list->data;
		if (disk->subdisk == -1 || disk->subdisk > subdisk)
			break;
		}
	disk_mon_list = g_list_insert_before(disk_mon_list, list, sdisk);
	++n_disks;
	return sdisk;
	}

static void
disk_assign_data(DiskMon *disk, guint64 rb, guint64 wb, gboolean virtual)
	{
	if (!disk)
		return;
	disk->rb = rb;
	disk->wb = wb;

	/* Add data to composite disk if this is not a subdisk (partition) and
	|  not a virtual disk (eg /dev/mdX software RAID multi-disk).
	*/
	if (disk->subdisk == -1 && !virtual)
		{
		composite_disk->rb += rb;
		composite_disk->wb += wb;
		}
	}

static gboolean
setup_disk_interface(void)
	{
	if (!read_disk_data && !_GK.client_mode && gkrellm_sys_disk_init())
		{
		read_disk_data = gkrellm_sys_disk_read_data;
		name_from_device = gkrellm_sys_disk_name_from_device;
		order_from_name = gkrellm_sys_disk_order_from_name;
		}
	/* Get a read in so I'll know the assign_method before config is loaded.
	*/
	if (read_disk_data)
		(*read_disk_data)();
	return read_disk_data ? TRUE : FALSE;
	}

/* ------------- Disk monitor to system dependent interface ------------- */
void
gkrellm_disk_client_divert(void (*read_func)(),
		gchar *(*name_from_device_func)(), gint (*order_from_name_func)())
	{
	read_disk_data = read_func;
	name_from_device = name_from_device_func;
	order_from_name = order_from_name_func;
	}

void
gkrellm_disk_reset_composite(void)
	{
	composite_disk->rb = 0;
	composite_disk->wb = 0;
	sys_handles_composite_reset = TRUE;
	}

void
gkrellm_disk_units_are_blocks(void)
	{
	units_are_blocks = TRUE;
	}

void
gkrellm_disk_assign_data_by_device(gint device_number, gint unit_number,
			guint64 rb, guint64 wb, gboolean virtual)
	{
	DiskMon	*disk;
	gchar	*name;
	gint	order = -1;

	assign_method = DISK_ASSIGN_BY_DEVICE;
	disk = lookup_disk_by_device(device_number, unit_number);
	if (!disk && name_from_device)
		{
		name = (*name_from_device)(device_number, unit_number, &order);
		if (name)
			disk = add_disk(name, name, device_number, unit_number, order);
		}
	disk_assign_data(disk, rb, wb, virtual);
	}

void
gkrellm_disk_assign_data_nth(gint n, guint64 rb, guint64 wb, gboolean virtual)
	{
	DiskMon	*disk;
	gchar	name[32], label[32];

	assign_method = DISK_ASSIGN_NTH;
	if (n < n_disks)
		disk = (DiskMon *) g_list_nth_data(disk_mon_list, n + 1);
	else
		{
		sprintf(name, "%s%c", "Disk", 'A' + n);
		sprintf(label, "%s%c", _("Disk"), 'A' + n);
		disk = add_disk(name, label, 0, 0, n);
		}
	disk_assign_data(disk, rb, wb, virtual);
	}

void
gkrellm_disk_assign_data_by_name(gchar *name, guint64 rb, guint64 wb,
			gboolean virtual)
	{
	DiskMon	*disk;
	gint	order = -1;

	assign_method = DISK_ASSIGN_BY_NAME;
	if (!name)
		return;
	disk = lookup_disk_by_name(name);
	if (!disk)
		{
		if (order_from_name)
			order = (*order_from_name)(name);
		disk = add_disk(name, name, 0, 0, order);
		}
	disk_assign_data(disk, rb, wb, virtual);
	}

void
gkrellm_disk_subdisk_assign_data_by_name(gchar *subdisk_name, gchar *disk_name,
					guint64 rb, guint64 wb)
	{
	DiskMon	*disk;
	gchar	*s, *endptr;
	gint	subdisk;

	assign_method = DISK_ASSIGN_BY_NAME;
	if (!subdisk_name || !disk_name)
		return;
	disk = lookup_disk_by_name(subdisk_name);
	if (!disk)
		{
		/* A subdisk name is expected to be the disk_name with a number string
		|  appended.  Eg. "hda1" is a subdisk_name of disk_name "hda"
		*/
		s = subdisk_name + strlen(disk_name);
		subdisk = strtol(s, &endptr, 0);
		if (!*s || *endptr)
			return;
		disk = add_subdisk(subdisk_name, disk_name, subdisk);
		}
	disk_assign_data(disk, rb, wb, FALSE);
	}


/* ----------- End of Disk monitor to system dependent interface ---------- */


static GkrellmSizeAbbrev	disk_blocks_abbrev[]	=
	{
	{ KB_SIZE(1),		1,				"%.0f" },
	{ KB_SIZE(20),		KB_SIZE(1),		"%.1fK" },
	{ MB_SIZE(1),		KB_SIZE(1),		"%.0fK" },
	{ MB_SIZE(20),		MB_SIZE(1),		"%.1fM" },
	{ MB_SIZE(50),		MB_SIZE(1),		"%.0fM" }
	};


static gchar    *text_format,
				*text_format_locale;

static void
format_disk_data(DiskMon *disk, gchar *src_string, gchar *buf, gint size)
	{
	GkrellmChart	*cp;
	gchar			c, *s;
	size_t			tbl_size;
	gint			len, r_blocks, w_blocks, blocks;

	if (!buf || size < 1)
		return;
	--size;
	*buf = '\0';
	if (!src_string)
		return;
	cp = disk->chart;
	r_blocks = gkrellm_get_current_chartdata(disk->read_cd);
	w_blocks = gkrellm_get_current_chartdata(disk->write_cd);
	tbl_size = sizeof(disk_blocks_abbrev) / sizeof(GkrellmSizeAbbrev);
	for (s = src_string; *s != '\0' && size > 0; ++s)
		{
		len = 1;
		if (*s == '$' && *(s + 1) != '\0')
			{
			blocks = -1;
			if ((c = *(s + 1)) == 'T')
				blocks = r_blocks + w_blocks;
			else if (c == 'M')
				blocks = gkrellm_get_chart_scalemax(cp);
			else if (c == 'r')
				blocks = r_blocks;
			else if (c == 'w')
				blocks = w_blocks;
			else if (c == 'L')
				len = snprintf(buf, size, "%s", disk->name);
			else if (c == 'H')
				len = snprintf(buf, size, "%s", gkrellm_sys_get_host_name());
			else
				{
				*buf = *s;
				if (size > 1)
					{
					*(buf + 1) = *(s + 1);
					++len;
					}
				}
			if (blocks >= 0)
				len = gkrellm_format_size_abbrev(buf, size, (gfloat) blocks,
						&disk_blocks_abbrev[0], tbl_size);
			++s;
			}
		else
			*buf = *s;
		size -= len;
		buf += len;
		}
	*buf = '\0';
	}

static void
draw_disk_extra(DiskMon *disk)
	{
	gchar	buf[128];

	if (!disk->chart || !disk->extra_info)
		return;
	format_disk_data(disk, text_format_locale, buf, sizeof(buf));
	if (!disk->new_text_format)
		gkrellm_chart_reuse_text_format(disk->chart);
	disk->new_text_format = FALSE;
	gkrellm_draw_chart_text(disk->chart, style_id, buf);
	}

static void
cb_command_process(GkrellmAlert *alert, gchar *src, gchar *dst, gint len,
			DiskMon *disk)
	{
	if (disk->chart)
		format_disk_data(disk, src, dst, len);
	}

static void
draw_disk_chart(DiskMon *disk)
	{
	gkrellm_draw_chartdata(disk->chart);
	draw_disk_extra(disk);
	gkrellm_draw_chart_to_screen(disk->chart);
	}

static void
cb_disk_temp_alert_trigger(GkrellmAlert *alert, DiskMon *disk)
    {
    GkrellmAlertdecal   *ad;
    GkrellmDecal        *d;

    if (alert && disk && disk->chart)
        {
        ad = &alert->ad;
        d = disk->temp_decal;
        if (d)
            {
            ad->x = d->x - 1;
            ad->y = d->y - 1;
            ad->w = d->w + 2;
            ad->h = d->h + 2;
            gkrellm_render_default_alert_decal(alert);
            }
        alert->panel = disk->chart->panel;
        }
    }

gboolean
gkrellm_disk_temperature_display(gpointer sr, gchar *id_name, gfloat t,
			gchar units)
	{
	GList				*list;
	DiskMon				*disk;
	GkrellmPanel		*p;
	GkrellmDecal		*decal;
	gchar				*disk_name;
	gint				len;
	gboolean			display_done = FALSE, display_possible = FALSE;

	if ((disk_name = strrchr(id_name, '/')) != NULL)
		++disk_name;
	else
		disk_name = id_name;

	len = strlen(disk_name);
	for (list = disk_mon_list; list; list = list->next)
		{
		disk = (DiskMon *) list->data;
		if (   strncmp(disk->name, disk_name, len)
			|| (disk->name[len] != '\0' && !isdigit((unsigned char)disk->name[len]))
		   )
			continue;
		if (!disk->enabled || !disk->chart || !disk->chart->panel->decal_list)
			{
			display_possible |= disk->enabled;	/* Not created or rebuilding */
			continue;
			}
		decal = disk->temp_decal;
		p = disk->chart->panel;
		if (display_done)
			{
			if (gkrellm_is_decal_visible(decal))
				{
				gkrellm_make_decal_invisible(p, decal);
				p->label->position = disk->save_label_position;
				gkrellm_draw_panel_label(p);
				gkrellm_draw_panel_layers(p);
				}
			continue;
			}
		if (!gkrellm_is_decal_visible(decal))
			{
			gkrellm_make_decal_visible(p, decal);
			disk->save_label_position = p->label->position;
			if (p->label->position >= 0)
				{
				p->label->position = 0;
				gkrellm_draw_panel_label(p);
				}
			gkrellm_sensor_alert_connect(sr, cb_disk_temp_alert_trigger, disk);
			}
		gkrellm_sensor_draw_temperature_decal(p, decal, t, units);
		gkrellm_draw_panel_layers(p);
		display_done = TRUE;
		}
	return (display_possible || display_done);
	}

void
gkrellm_disk_temperature_remove(gchar *id_name)
	{
	GList			*list;
	DiskMon			*disk;
	GkrellmPanel	*p;
	GkrellmDecal	*decal;
	gchar			*disk_name;
	gint			len;

	if ((disk_name = strrchr(id_name, '/')) != NULL)
		++disk_name;
	else
		disk_name = id_name;

	len = strlen(disk_name);
	for (list = disk_mon_list; list; list = list->next)
		{
		disk = (DiskMon *) list->data;
		if (!disk->chart)
			continue;
		if (   strncmp(disk->name, disk_name, len)
			|| (disk->name[len] != '\0' && !isdigit((unsigned char)disk->name[len]))
		   )
			continue;
		p = disk->chart->panel;
		decal = disk->temp_decal;
		if (gkrellm_is_decal_visible(decal))
			{
			gkrellm_make_decal_invisible(p, decal);
			p->label->position = disk->save_label_position;
			gkrellm_draw_panel_label(p);
			gkrellm_draw_panel_layers(p);
			}
		}
	}


static void
update_disk(void)
	{
	GList			*list;
	DiskMon			*disk;
	GkrellmChart	*cp;
	gint			bytes;

	if (!sys_handles_composite_reset)
		{
		composite_disk->rb = 0;
		composite_disk->wb = 0;
		}
	(*read_disk_data)();
	if (n_disks == 0)
		return;

	for (list = disk_mon_list; list; list = list->next)
		{
		disk = (DiskMon *) list->data;
		if ((cp = disk->chart) == NULL)		/* or disk->enabled FALSE */
			continue;
		if (GK.second_tick)
			{
			gkrellm_store_chartdata(cp, 0,
						(gulong) disk->wb, (gulong) disk->rb);
			if (disk->alert)
				{
				bytes = 0;
				if (disk->alert_uses_read)
					bytes += gkrellm_get_current_chartdata(disk->read_cd);
				if (disk->alert_uses_write)
					bytes += gkrellm_get_current_chartdata(disk->write_cd);
				gkrellm_check_alert(disk->alert, bytes);
				}
			gkrellm_panel_label_on_top_of_decals(cp->panel,
						gkrellm_alert_decal_visible(disk->alert));
			draw_disk_chart(disk);
			}
		gkrellm_update_krell(cp->panel, KRELL(cp->panel),
					(gulong) (disk->wb + disk->rb));
		gkrellm_draw_panel_layers(cp->panel);
		}
	}


static gint
disk_expose_event(GtkWidget *widget, GdkEventExpose *ev)
	{
	GList			*list;
	GkrellmChart	*cp;
	GdkPixmap		*pixmap	= NULL;

	for (list = disk_mon_list; list; list = list->next)
		{
		if ((cp = ((DiskMon *) list->data)->chart) == NULL)
			continue;
		if (widget == cp->drawing_area)
			pixmap = cp->pixmap;
		else if (widget == cp->panel->drawing_area)
			pixmap = cp->panel->pixmap;
		if (pixmap)
			{
			gdk_draw_drawable(widget->window, gkrellm_draw_GC(1), pixmap,
				  ev->area.x, ev->area.y, ev->area.x, ev->area.y,
				  ev->area.width, ev->area.height);
			break;
			}
		}
	return FALSE;
	}

static gint
cb_disk_extra(GtkWidget *widget, GdkEventButton *ev)
	{
	GList	*list;
	DiskMon	*disk;

	for (list = disk_mon_list; list; list = list->next)
		{
		disk = (DiskMon *) list->data;
		if (!disk->enabled || widget != disk->chart->drawing_area)
			continue;
		if (ev->button == 1 && ev->type == GDK_BUTTON_PRESS)
			{
			disk->extra_info = !disk->extra_info;
			draw_disk_chart(disk);
			gkrellm_config_modified();
			}
		else if (   ev->button == 3
				 || (ev->button == 1 && ev->type == GDK_2BUTTON_PRESS)
				)
			gkrellm_chartconfig_window_create(disk->chart);
		break;
		}
	return FALSE;
	}

static void
setup_disk_scaling(GkrellmChartconfig *cf, GkrellmChart *cp)
	{
	gint	grids, res;

	grids = gkrellm_get_chartconfig_fixed_grids(cf);
	if (!grids)
		grids = FULL_SCALE_GRIDS;
	res = gkrellm_get_chartconfig_grid_resolution(cf);

	KRELL(cp->panel)->full_scale = res * grids / gkrellm_update_HZ();
	}

  /* Destroy everything in a DiskMon structure except for the vbox which
  |  is preserved so disk ordering will be maintained.  Compare this to
  |  destroying an InetMon where really everything is destroyed including
  |  the InetMon structure.  Here the DiskMon structure is not destroyed.
  */
static void
destroy_disk_monitor(DiskMon *disk)
	{
	if (disk->launch_table)
		gtk_widget_destroy(disk->launch_table);
	disk->launch_table = NULL;
	if (disk->launch.button)
		gkrellm_destroy_button(disk->launch.button);
	disk->launch.button = NULL;
	disk->launch.tooltip = NULL;
	gkrellm_dup_string(&disk->launch.command, "");
	gkrellm_dup_string(&disk->launch.tooltip_comment, "");
	gkrellm_chart_destroy(disk->chart);
	disk->chart = NULL;
	disk->enabled = FALSE;
	}

static gint
cb_panel_press(GtkWidget *widget, GdkEventButton *ev)
	{
	if (ev->button == 3)
		gkrellm_open_config_window(mon_disk);
	return FALSE;
	}

static void
create_disk_monitor(DiskMon *disk, gint first_create)
	{
	GkrellmChart		*cp;
	GkrellmPanel		*p;
	GkrellmDecal		*d;
	GkrellmStyle		*style;
	GkrellmTextstyle	*ts;
	GkrellmMargin		*m;

	if (first_create)
		{
		disk->chart = gkrellm_chart_new0();
		disk->chart->panel = gkrellm_panel_new0();
		}
	cp = disk->chart;
	p = cp->panel;

	style = gkrellm_panel_style(style_id);
	gkrellm_create_krell(p, gkrellm_krell_panel_piximage(style_id), style);

	gkrellm_chart_create(disk->vbox, mon_disk, cp, &disk->chart_config);
	disk->write_cd = gkrellm_add_default_chartdata(cp, _("Write bytes"));
	disk->read_cd = gkrellm_add_default_chartdata(cp, _("Read bytes"));
	gkrellm_set_draw_chart_function(cp, draw_disk_chart, disk);

	gkrellm_chartconfig_fixed_grids_connect(cp->config,
				setup_disk_scaling, cp);
	gkrellm_chartconfig_grid_resolution_connect(cp->config,
				setup_disk_scaling, cp);
	gkrellm_chartconfig_grid_resolution_adjustment(cp->config, TRUE,
				0, (gfloat) MIN_GRID_RES, (gfloat) MAX_GRID_RES, 0, 0, 0, 0);
	gkrellm_chartconfig_grid_resolution_label(cp->config,
				units_are_blocks ?
				_("Disk I/O blocks per sec") : _("Disk I/O bytes per sec"));
	if (gkrellm_get_chartconfig_grid_resolution(cp->config) < MIN_GRID_RES)
		gkrellm_set_chartconfig_grid_resolution(cp->config, DEFAULT_GRID_RES);
	gkrellm_alloc_chartdata(cp);

	setup_disk_scaling(cp->config, cp);

	ts = gkrellm_panel_alt_textstyle(style_id);
	disk->temp_decal = gkrellm_create_decal_text(p, "188.8F",
					ts, style, -1, -1, 0);
	gkrellm_make_decal_invisible(p, disk->temp_decal);

	gkrellm_panel_configure(p, disk->label, style);
	gkrellm_panel_create(disk->vbox, mon_disk, p);
	disk->enabled = TRUE;
	disk->new_text_format = TRUE;

	/* Position the temp decal to right edge and vertically align it
	|  wrt the label.
	*/
	d = disk->temp_decal;
	m = gkrellm_get_style_margins(style);
	gkrellm_move_decal(p, d,
				gkrellm_chart_width() - m->right - d->w,
 				m->top + (p->label->height - d->h + 1) / 2);

	if (first_create)
		{
		g_signal_connect(G_OBJECT(cp->drawing_area), "expose_event",
				G_CALLBACK(disk_expose_event), NULL);
		g_signal_connect(G_OBJECT (p->drawing_area), "expose_event",
				G_CALLBACK(disk_expose_event), NULL);

		g_signal_connect(G_OBJECT(cp->drawing_area), "button_press_event",
				G_CALLBACK(cb_disk_extra), NULL);
		g_signal_connect(G_OBJECT(p->drawing_area), "button_press_event",
				G_CALLBACK(cb_panel_press), NULL);
		}
	else
		draw_disk_chart(disk);

	gkrellm_configure_tooltip(p, &disk->launch);
	if (*(disk->launch.command) != '\0')
		disk->launch.button = gkrellm_put_label_in_panel_button(p,
				gkrellm_launch_button_cb, &disk->launch, disk->launch.pad);
	}


static GtkWidget	*disk_vbox;

static void
create_disk(GtkWidget *vbox, gint first_create)
	{
	GList		*list;
	DiskMon		*disk;
	gboolean	any = FALSE;

	ascent = 0;
	disk_vbox = vbox;
	for (list = disk_mon_list; list; list = list->next)
		{
		disk = (DiskMon *) list->data;
		if (first_create)
			{
			disk->vbox = gtk_vbox_new(FALSE, 0);
			gtk_box_pack_start(GTK_BOX(vbox), disk->vbox, FALSE, FALSE, 0);
			gtk_widget_show(disk->vbox);
			}
		gkrellm_setup_launcher(NULL, &disk->launch, CHART_PANEL_TYPE, 4);
		if (disk->enabled)
			{
			create_disk_monitor(disk, first_create);
			any = TRUE;
			}
		}
	if (any)
		gkrellm_spacers_show(mon_disk);
	else
		gkrellm_spacers_hide(mon_disk);
	}

  /* Kernel 2.4 will not show a disk until it has I/O, and some systems
  |  may dynamically add a drive.
  */
static void
check_for_new_disks(void)
	{
	GList	*list;
	DiskMon	*disk;

	for (list = disk_mon_list; list; list = list->next)
		{
		disk = (DiskMon *) list->data;
		if (disk->vbox == NULL)
			{
			disk->vbox = gtk_vbox_new(FALSE, 0);
			gtk_box_pack_start(GTK_BOX(disk_vbox), disk->vbox, FALSE, FALSE,0);
			gtk_widget_show(disk->vbox);
			gkrellm_setup_launcher(NULL, &disk->launch, CHART_PANEL_TYPE, 4);
			}
		}
	}

static void
cb_alert_trigger(GkrellmAlert *alert, DiskMon *disk)
	{
	/* Full panel alert, default decal.
	*/
	alert->panel = disk->chart->panel;
	}


static void
create_alert(DiskMon *disk)
	{
	disk->alert = gkrellm_alert_create(NULL, disk->name,
			_("Bytes per second"),
			TRUE, FALSE, TRUE,
			1e9, 1000, 1000, 10000, 0);
	gkrellm_alert_delay_config(disk->alert, 1, 60 * 180, 0);

	gkrellm_alert_trigger_connect(disk->alert, cb_alert_trigger, disk);
	gkrellm_alert_config_connect(disk->alert, cb_alert_config, disk);
	gkrellm_alert_config_create_connect(disk->alert,
							cb_alert_config_create, disk);
	gkrellm_alert_command_process_connect(disk->alert,
				cb_command_process, disk);
	}

static gboolean
any_enabled_subdisks(GList *dlist)
	{
	GList	*list;
	DiskMon	*disk;

	for (list = dlist; list; list = list->next)
		{
		disk = (DiskMon *) list->data;
		if (disk->subdisk == -1)
			break;
		if (disk->enabled)
			return TRUE;
		}
	return FALSE;
	}

static void
save_disk_config(FILE *f)
	{
	GList		*list;
	DiskMon		*disk;
	gboolean	have_enabled_subdisks;

	if (n_disks == 0)
		return;
	fprintf(f, "%s assign_method %d\n", DISK_CONFIG_KEYWORD, assign_method);
	for (list = disk_mon_list; list; list = list->next)
		{
		disk = (DiskMon *) list->data;

		/* To deal with possible disk hardware/partition changes at next load
		|  of the config, record if any subdisks are enabled to determine if
		|  a disk saved into the config here should be artificially created
		|  should it not be present at next gkrellm startup.
		*/
		have_enabled_subdisks = (disk->subdisk == -1) ?
					any_enabled_subdisks(list->next) : FALSE;

		fprintf(f, "%s device %s %d %d %d %d %d %d %d\n",
					DISK_CONFIG_KEYWORD,
					disk->name, disk->major, disk->minor, disk->order,
					disk->enabled, disk->extra_info, disk->subdisk,
					have_enabled_subdisks);
		if (*(disk->launch.command) != '\0')
			fprintf(f, "%s launch %s %s\n", DISK_CONFIG_KEYWORD,
						disk->name, disk->launch.command);
		if (*(disk->launch.tooltip_comment) != '\0')
			fprintf(f, "%s tooltip_comment %s %s\n", DISK_CONFIG_KEYWORD,
						disk->name, disk->launch.tooltip_comment);
		gkrellm_save_chartconfig(f, disk->chart_config,
					DISK_CONFIG_KEYWORD, disk->name);
		if (disk->alert && disk->enabled)
			{
			gkrellm_save_alertconfig(f, disk->alert,
						DISK_CONFIG_KEYWORD, disk->name);
			fprintf(f, "%s extra_alert_config %s %d %d\n", DISK_CONFIG_KEYWORD,
						disk->name,
						disk->alert_uses_read, disk->alert_uses_write);
			}
		}
	fprintf(f, "%s text_format %s\n", DISK_CONFIG_KEYWORD, text_format);
	}

static void
load_disk_config(gchar *arg)
	{
	DiskMon		*disk = NULL;
	gchar		config[32], item[CFG_BUFSIZE],
				name[32], item1[CFG_BUFSIZE];
	gint		major, minor, enabled, extra, order, subdisk = -1;
	gint		n;
	gboolean	enabled_subdisks = TRUE;
	static gchar *parent;
	static gint	config_assign_method;

	n = sscanf(arg, "%31s %[^\n]", config, item);
	if (n == 2)
		{
		if (!strcmp(config, "text_format"))
			gkrellm_locale_dup_string(&text_format, item, &text_format_locale);
		else if (!strcmp(config, "assign_method"))
			sscanf(item, "%d", &config_assign_method);
		else if (sscanf(item, "%31s %[^\n]", name, item1) == 2)
			disk = lookup_disk_by_name(name);

		if (!strcmp(config, "device"))
			{
			/* Disk config can be invalid (different disk naming scheme)
			|  if user changes kernel version.
			*/
			if (   config_assign_method == assign_method
				&& sscanf(item1, "%d %d %d %d %d %d %d",
							&major, &minor, &order,
							&enabled, &extra,
							&subdisk, &enabled_subdisks) >= 5
			   )
				{
				/* A disk in the config may not have been found in the above
				|  lookup because of removable drives or hardware/partition
				|  changes since last gkrellm run.
				|  Also Linux <= 2.4 may be getting disk stats from /proc/stat
				|  where disks won't appear until they have I/O.
				|  So, artificially create these not present disks if they
				|  were previously enabled.
				*/
				if (subdisk == -1)
					gkrellm_dup_string(&parent, name);
				if (   !disk && subdisk == -1
					&& (enabled || enabled_subdisks)
				   )
					disk = add_disk(name, name, major, minor, order - 1);
				else if (!disk && subdisk >= 0 && enabled)
					disk = add_subdisk(name, parent, subdisk);
				if (disk)
					{
					disk->enabled = enabled;
					disk->extra_info = extra;
					}
				}
			return;
			}
		if (!disk)
			return;
		if (!strcmp(config, GKRELLM_CHARTCONFIG_KEYWORD))
			gkrellm_load_chartconfig(&disk->chart_config, item1, 2);
		else if (!strcmp(config, GKRELLM_ALERTCONFIG_KEYWORD))
			{
			if (!disk->alert)
				create_alert(disk);
			gkrellm_load_alertconfig(&disk->alert, item1);
			}
		else if (!strcmp(config, "extra_alert_config"))
			sscanf(item1, "%d %d",
							&disk->alert_uses_read, &disk->alert_uses_write);
		else if (!strcmp(config, "launch"))
			disk->launch.command = g_strdup(item1);
		else if (!strcmp(config, "tooltip_comment"))
			disk->launch.tooltip_comment = g_strdup(item1);
		}
	}


/* --------------------------------------------------------------------	*/

enum
	{
	NAME_COLUMN,
	ENABLE_COLUMN,
	DISK_COLUMN,
	IMAGE_COLUMN,
	N_COLUMNS
	};

static GtkTreeView			*treeview;

static GtkTreeRowReference	*row_reference;
static GtkTreeSelection		*selection;

static GtkWidget			*launch_vbox,
							*text_format_combo_box;

static GtkWidget			*alert_button;



static GtkTreeModel *
create_model(void)
	{
	GtkTreeStore	*tree;
	GtkTreeIter		iter, citer;
	GList			*list, *clist;
	DiskMon			*disk;

	tree = gtk_tree_store_new(N_COLUMNS,
				G_TYPE_STRING,
                G_TYPE_BOOLEAN,
				G_TYPE_POINTER,
				GDK_TYPE_PIXBUF);
	for (list = disk_mon_list; list; )
		{
		disk = (DiskMon *) list->data;
		gtk_tree_store_append(tree, &iter, NULL);
		if (list == disk_mon_list)
			gtk_tree_store_set(tree, &iter,
				NAME_COLUMN, _("Composite chart combines data for all disks"),
				ENABLE_COLUMN, disk->enabled,
				DISK_COLUMN, disk,
				-1);
		else
			{
			gtk_tree_store_set(tree, &iter,
					NAME_COLUMN, disk->label,
					ENABLE_COLUMN, disk->enabled,
					DISK_COLUMN, disk,
					-1);
			if (disk->alert)
				gtk_tree_store_set(tree, &iter,
						IMAGE_COLUMN, gkrellm_alert_pixbuf(), -1);
			}
		for (clist = list->next; clist; clist = clist->next)
			{
			disk = (DiskMon *) clist->data;
			if (disk->subdisk == -1)
				break;
			gtk_tree_store_append(tree, &citer, &iter);
			gtk_tree_store_set(tree, &citer,
					NAME_COLUMN, disk->name,
					ENABLE_COLUMN, disk->enabled,
					DISK_COLUMN, disk,
					-1);
			if (disk->alert)
				gtk_tree_store_set(tree, &citer,
						IMAGE_COLUMN, gkrellm_alert_pixbuf(), -1);
			}
		list = clist;
		}

	return GTK_TREE_MODEL(tree);
	}

static void
cb_launch_entry(GtkWidget *widget, DiskMon *disk)
	{
	if (disk->enabled)
		gkrellm_apply_launcher(&disk->launch_entry, &disk->tooltip_entry,
				disk->chart->panel, &disk->launch, gkrellm_launch_button_cb);
	}

static void
add_launch_entry(GtkWidget *vbox, DiskMon *disk)
	{
	disk->launch_table = gkrellm_gtk_launcher_table_new(vbox, 1);
	gkrellm_gtk_config_launcher(disk->launch_table, 0,  &disk->launch_entry,
				&disk->tooltip_entry, disk->label,
				&disk->launch);
	g_signal_connect(G_OBJECT(disk->launch_entry), "changed",
				G_CALLBACK(cb_launch_entry), disk);
	g_signal_connect(G_OBJECT(disk->tooltip_entry), "changed",
				G_CALLBACK(cb_launch_entry), disk);
	gtk_widget_show_all(disk->launch_table);
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
cb_alert_config(GkrellmAlert *ap, DiskMon *disk)
	{
	GtkTreeModel	*model;
	GtkTreeIter		iter, citer;
	DiskMon			*disk_test;
	GdkPixbuf		*pixbuf;
	gboolean		valid;

	disk->alert_uses_read =
				GTK_TOGGLE_BUTTON(disk->alert_config_read_button)->active;
	disk->alert_uses_write =
				GTK_TOGGLE_BUTTON(disk->alert_config_write_button)->active;
	if (!gkrellm_config_window_shown())
		return;
	model = gtk_tree_view_get_model(treeview);
	pixbuf = ap->activated ? gkrellm_alert_pixbuf() : NULL;
	valid = gtk_tree_model_get_iter_first(model, &iter);
	while (valid)
		{
		gtk_tree_model_get(model, &iter, DISK_COLUMN, &disk_test, -1);
		if (disk == disk_test)
			{
			gtk_tree_store_set(GTK_TREE_STORE(model), &iter,
						IMAGE_COLUMN, pixbuf, -1);
			return;
			}
		if (gtk_tree_model_iter_children(model, &citer, &iter))
			do
				{
				gtk_tree_model_get(model, &citer, DISK_COLUMN, &disk_test, -1);
				if (disk == disk_test)
					{
					gtk_tree_store_set(GTK_TREE_STORE(model), &citer,
								IMAGE_COLUMN, pixbuf, -1);
					return;
					}
				}
			while (gtk_tree_model_iter_next(model, &citer));

		valid = gtk_tree_model_iter_next(model, &iter);
		}
	}

static void
cb_alert_config_button(GtkWidget *button, DiskMon *disk)
	{
	gboolean	read, write;

	read = GTK_TOGGLE_BUTTON(disk->alert_config_read_button)->active;
	write = GTK_TOGGLE_BUTTON(disk->alert_config_write_button)->active;
	if (!read && !write)
		{
		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON(disk->alert_config_read_button), TRUE);
		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON(disk->alert_config_write_button), TRUE);
		}
	}

static void
cb_alert_config_create(GkrellmAlert *ap, GtkWidget *vbox, DiskMon *disk)
	{
    gkrellm_gtk_check_button_connected(vbox, &disk->alert_config_read_button,
				disk->alert_uses_read, FALSE, FALSE, 2,
				cb_alert_config_button, disk, _("Read bytes"));
    gkrellm_gtk_check_button_connected(vbox, &disk->alert_config_write_button,
				disk->alert_uses_write, FALSE, FALSE, 2,
				cb_alert_config_button, disk, _("Write bytes"));
	}

static void
cb_set_alert(GtkWidget *button, gpointer data)
	{
	GtkTreeModel	*model;
	GtkTreePath		*path;
	GtkTreeIter		iter;
	DiskMon			*disk;

	if (!row_reference)
		return;
	model = gtk_tree_view_get_model(treeview);
	path = gtk_tree_row_reference_get_path(row_reference);
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_model_get(model, &iter, DISK_COLUMN, &disk, -1);

	if (!disk->alert)
		create_alert(disk);
	gkrellm_alert_config_window(&disk->alert);
	}


static void
cb_tree_selection_changed(GtkTreeSelection *selection, gpointer data)
	{
	GtkTreeIter     iter;
	GtkTreeModel    *model;
	GtkTreePath     *path;
	DiskMon			*disk;
	gint            *indices, depth;

	if (!gtk_tree_selection_get_selected(selection, &model, &iter))
		{
		gtk_widget_set_sensitive(alert_button, FALSE);
		return;
		}
	path = gtk_tree_model_get_path(model, &iter);
	indices = gtk_tree_path_get_indices(path);
	depth = gtk_tree_path_get_depth(path);
// printf("selection: indices=[%d,%d]:%d, path=%s\n",
//          indices[0], indices[1], gtk_tree_path_get_depth(path),
//          gtk_tree_path_to_string(path));
	change_row_reference(model, path);

	gtk_tree_model_get(model, &iter,
			DISK_COLUMN, &disk, -1);

	if (!disk->enabled || (depth == 1 && indices[0] == 0))
		gtk_widget_set_sensitive(alert_button, FALSE);
	else
		gtk_widget_set_sensitive(alert_button, TRUE);
	gtk_tree_path_free(path);
	}

static void
cb_enable(GtkCellRendererText *cell, gchar *path_string, gpointer data)
	{
	GtkTreeModel	*model;
	GtkTreeIter		iter;
	GtkTreePath		*path;
	GList			*list;
	DiskMon			*disk;
	gboolean		enabled;

	model = GTK_TREE_MODEL(data);
	path = gtk_tree_path_new_from_string(path_string);
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_path_free(path);

	gtk_tree_model_get(model, &iter,
				ENABLE_COLUMN, &enabled,
				DISK_COLUMN, &disk,
				-1);
	gtk_tree_store_set(GTK_TREE_STORE(model), &iter,
				ENABLE_COLUMN, !enabled,
				-1);
	if (enabled)
		{
		destroy_disk_monitor(disk);
		gtk_widget_set_sensitive(alert_button, FALSE);
		}
	else
		{
		create_disk_monitor(disk, TRUE);
		add_launch_entry(launch_vbox, disk);
		gtk_widget_set_sensitive(alert_button, TRUE);
		}
	disk->enabled = !enabled;
	enabled = FALSE;
	for (list = disk_mon_list; list; list = list->next)
		if (((DiskMon *) list->data)->enabled)
			enabled = TRUE;
	if (enabled)
		gkrellm_spacers_show(mon_disk);
	else
		gkrellm_spacers_hide(mon_disk);
	}

static void
cb_text_format(GtkWidget *widget, gpointer data)
	{
	GList	*list;
	DiskMon	*disk;
	gchar	*s;
	GtkWidget *entry;

	entry = gtk_bin_get_child(GTK_BIN(text_format_combo_box));
	s = gkrellm_gtk_entry_get_text(&entry);
	gkrellm_locale_dup_string(&text_format, s, &text_format_locale);
	for (list = disk_mon_list; list; list = list->next)
		{
		disk = (DiskMon *) list->data;
		disk->new_text_format = TRUE;
		draw_disk_chart(disk);
		}
	}


#define	DEFAULT_TEXT_FORMAT	"$T"

static gchar	*disk_info_text[] =
{
N_("<h>Chart Labels\n"),
N_("Substitution variables for the format string for chart labels:\n"),
N_("\t$L    the Disk label\n"),
N_("\t$M    maximum chart value\n"),
N_("\t$T    total read bytes + write bytes\n"),
N_("\t$r    read bytes\n"),
N_("\t$w    write bytes\n"),
"\n",
N_("Substitution variables may be used in alert commands.\n")
};

static void
create_disk_tab(GtkWidget *tab_vbox)
	{
	GtkWidget		*tabs;
	GtkWidget		*vbox, *vbox1, *hbox;
	GtkWidget		*text;
	GtkWidget		*scrolled;
	GtkTreeModel	*model;
	GtkCellRenderer	*renderer;
	GList			*list;
	DiskMon			*disk;
	gint			i;

	check_for_new_disks();
	if (n_disks == 0)
		return;

	tabs = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(tabs), GTK_POS_TOP);
	gtk_box_pack_start(GTK_BOX(tab_vbox), tabs, TRUE, TRUE, 0);

/* -- Options tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Options"));

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);

	model = create_model();

	treeview = GTK_TREE_VIEW(gtk_tree_view_new_with_model(model));
	gtk_tree_view_set_rules_hint(treeview, TRUE);

	renderer = gtk_cell_renderer_toggle_new();
	gtk_tree_view_insert_column_with_attributes(treeview, -1, _("Enable"),
				renderer,
				"active", ENABLE_COLUMN,
				NULL);
	g_signal_connect(G_OBJECT(renderer), "toggled",
				G_CALLBACK(cb_enable), model);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(treeview, -1, _("Disk"),
				renderer,
				"text", NAME_COLUMN,
				NULL);

	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_insert_column_with_attributes(treeview, -1, "",
				renderer,
				"pixbuf", IMAGE_COLUMN,
				NULL);

	gtk_container_add(GTK_CONTAINER(scrolled), GTK_WIDGET(treeview));
	g_object_unref(G_OBJECT(model));
	selection = gtk_tree_view_get_selection(treeview);
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(selection), "changed",
				G_CALLBACK(cb_tree_selection_changed), NULL);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 3);
	gkrellm_gtk_alert_button(hbox, &alert_button, FALSE, FALSE, 4, FALSE,
				cb_set_alert, NULL);
	gtk_widget_set_sensitive(alert_button, FALSE);

/* -- Setup tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Setup"));

	vbox1 = gkrellm_gtk_category_vbox(vbox,
				_("Format String for Chart Labels"),
				4, 0, TRUE);
	text_format_combo_box = gtk_combo_box_entry_new_text();
	gtk_box_pack_start(GTK_BOX(vbox1), text_format_combo_box, FALSE, FALSE, 0);
	gtk_combo_box_append_text(GTK_COMBO_BOX(text_format_combo_box),
		text_format);
	gtk_combo_box_append_text(GTK_COMBO_BOX(text_format_combo_box),
		DEFAULT_TEXT_FORMAT);
	gtk_combo_box_append_text(GTK_COMBO_BOX(text_format_combo_box),
		"\\c\\f$M\\n$T");
	gtk_combo_box_append_text(GTK_COMBO_BOX(text_format_combo_box),
		"\\c\\f$M\\b$T");
	gtk_combo_box_append_text(GTK_COMBO_BOX(text_format_combo_box),
		_("\\f\\ww\\r\\f$M\\D2\\f\\ar\\. $r\\D1\\f\\aw\\. $w"));
	gtk_combo_box_set_active(GTK_COMBO_BOX(text_format_combo_box), 0);
	g_signal_connect(G_OBJECT(GTK_COMBO_BOX(text_format_combo_box)), "changed",
			G_CALLBACK(cb_text_format), NULL);

	vbox1 = gkrellm_gtk_category_vbox(vbox,
				_("Launch Commands"),
				4, 0, TRUE);
	launch_vbox = gkrellm_gtk_scrolled_vbox(vbox, NULL,
						GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_show(launch_vbox);
	gtk_widget_realize(launch_vbox);
	for (i = 0, list = disk_mon_list; list; list = list->next, ++i)
		{
		disk = (DiskMon *) list->data;
		if (disk->enabled)
			add_launch_entry(launch_vbox, disk);
		}

/* --Info tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Info"));
	text = gkrellm_gtk_scrolled_text_view(vbox, NULL,
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	for (i = 0; i < sizeof(disk_info_text)/sizeof(gchar *); ++i)
		gkrellm_gtk_text_view_append(text, _(disk_info_text[i]));
	}

static GkrellmMonitor	monitor_disk =
	{
	N_("Disk"),			/* Name, for config tab.	*/
	MON_DISK,			/* Id,  0 if a plugin		*/
	create_disk,		/* The create function		*/
	update_disk,		/* The update function		*/
	create_disk_tab,	/* The config tab create function	*/
	NULL,				/* Instant apply */

	save_disk_config,	/* Save user conifg			*/
	load_disk_config,	/* Load user config			*/
	DISK_CONFIG_KEYWORD, /* config keyword			*/

	NULL,				/* Undef 2	*/
	NULL,				/* Undef 1	*/
	NULL,				/* Undef 0	*/

	0,					/* insert_before_id - place plugin before this mon */

	NULL,				/* Handle if a plugin, filled in by GKrellM		*/
	NULL				/* path if a plugin, filled in by GKrellM		*/
	};

GkrellmMonitor *
gkrellm_init_disk_monitor(void)
	{
	DiskMon	*disk;

	monitor_disk.name = _(monitor_disk.name);

	disk = disk_new("Disk", _("Disk"));
	disk_mon_list = g_list_append(disk_mon_list, disk);

	composite_disk = disk;
	gkrellm_locale_dup_string(&text_format, DEFAULT_TEXT_FORMAT,
				&text_format_locale);
	mon_disk = &monitor_disk;

	if (setup_disk_interface())
		{
		if (n_disks > 0)
			composite_disk->enabled = TRUE;
		style_id = gkrellm_add_chart_style(&monitor_disk, DISK_STYLE_NAME);
		return &monitor_disk;
		}
	return NULL;
	}
