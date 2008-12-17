/* GKrellM
|  Copyright (C) 1999-2008 Bill Wilson
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


#ifndef GKRELLM_H
#define GKRELLM_H

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include "log.h"
#include "gkrellm-version.h"

#if !defined(WIN32)
#include <sys/param.h>
#endif

#include <gtk/gtk.h>
#include <glib/gstdio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <time.h>

#if !defined(WIN32)
#include <unistd.h>
#else
    #include <winsock2.h>
    #include <process.h>
#endif

#include <sys/stat.h>

#include <locale.h>

#if defined(__sun) && defined(__SVR4)
#define __solaris__
#endif

#if !defined(__FreeBSD__) && !defined(__linux__) && !defined(__NetBSD__) \
	&& !defined(__OpenBSD__) && !defined(__solaris__) && !defined(WIN32) \
	&& !defined(__APPLE__) && !defined(__DragonFly__)
#define  USE_LIBGTOP
#endif


#if !defined(PACKAGE)
#define PACKAGE	"gkrellm"
#endif

/* Internationalization support.
*/
#if defined (ENABLE_NLS)
#include <libintl.h>
#	undef _
#	define _(String) dgettext(PACKAGE,String)
#	if defined(gettext_noop)
#		define N_(String) gettext_noop(String)
#	else
#		define N_(String) (String)
#	endif	/* gettext_noop */
#else
#	define _(String) (String)
#	define N_(String) (String)
#	define textdomain(String) (String)
#	define gettext(String) (String)
#	define dgettext(Domain,String) (String)
#	define dcgettext(Domain,String,Type) (String)
#	define bindtextdomain(Domain,Directory) (Domain)
#endif	/* ENABLE_NLS */


#define	GKRELLM_DIR				".gkrellm2"
#define	GKRELLM_USER_CONFIG		".gkrellm2/user-config"
#define	GKRELLM_2_1_14_CONFIG	".gkrellm2/user_config"
#define	GKRELLM_THEME_CONFIG	".gkrellm2/theme_config"
#define GKRELLM_THEMES_DIR		".gkrellm2/themes"
#define	GKRELLM_DATA_DIR		".gkrellm2/data"
#define	GKRELLM_PLUGINS_DIR		".gkrellm2/plugins"
#define	GKRELLM_LOCK_FILE		".gkrellm2/lock"
#define GKRELLMRC				"gkrellmrc"

#define	PLUGIN_ENABLE_FILE		".gkrellm2/plugin_enable"
#define	PLUGIN_PLACEMENT_FILE	".gkrellm2/plugin_placement"


#if !defined(WIN32)

#define	LOCAL_THEMES_DIR		"/usr/local/share/gkrellm2/themes"
#if !defined(SYSTEM_THEMES_DIR)
#define	SYSTEM_THEMES_DIR		"/usr/share/gkrellm2/themes"
#endif
#define	LOCAL_PLUGINS_DIR		"/usr/local/lib/gkrellm2/plugins"
#if !defined(SYSTEM_PLUGINS_DIR)
#define	SYSTEM_PLUGINS_DIR		"/usr/lib/gkrellm2/plugins"
#endif

#else

#undef      LOCAL_THEMES_DIR
#undef      SYSTEM_THEMES_DIR
#undef      LOCAL_PLUGINS_DIR
#undef      SYSTEM_PLUGINS_DIR

#endif


#define	ON				1
#define	OFF				0

#define	CFG_BUFSIZE		384


  /* Features
  */
#define	GKRELLM_HAVE_THEME_SCALE			1
#define	GKRELLM_HAVE_DECAL_TEXT_INSERT		1
#define	GKRELLM_HAVE_CLIENT_MODE_PLUGINS 	1
#define	GKRELLM_USING_PANGO					1
#define	GKRELLM_HAVE_DECAL_SCROLL_TEXT		1


  /* Label midpoints are positioned as a percent of chart_width.
  */
#define	GKRELLM_LABEL_NONE		-1
#define	GKRELLM_LABEL_CENTER	50
#define GKRELLM_LABEL_MAX		100

#define	GRID_MODE_NORMAL		0
#define	GRID_MODE_RESTRAINED	1


  /* GkrellmDecals in the decal_misc_piximage.
  */
#define	D_MISC_BLANK			0
#define	D_MISC_AC				1
#define	D_MISC_BATTERY			2
#define	D_MISC_BATTERY_WARN		3
#define	D_MISC_LED0				4
#define	D_MISC_LED1				5
#define	D_MISC_FS_UMOUNTED		6
#define	D_MISC_FS_MOUNTED		7
#define D_MISC_FS_PRESSED		8
#define D_MISC_BUTTON_OUT		9
#define D_MISC_BUTTON_ON		10
#define D_MISC_BUTTON_IN		11
#define	N_MISC_DECALS			12


  /* For formatting sizes in decimal or binary abbreviated notation.
  */
#define	KB_SIZE(s)	((s) * 1e3)
#define	KiB_SIZE(s)	((s) * 1024.0)
#define	MB_SIZE(s)	((s) * 1e6)
#define	MiB_SIZE(s)	((s) * 1024.0 * 1024.0)
#define	GB_SIZE(s)	((s) * 1e9)
#define	GiB_SIZE(s)	((s) * 1024.0 * 1024.0 * 1024.0)
#define	TB_SIZE(s)	((s) * 1e12)
#define	TiB_SIZE(s)	((s) * 1024.0 * 1024.0 * 1024.0 * 1024.0)

typedef struct
	{
	gfloat	limit,
			divisor;
	gchar	*format;
	}
	GkrellmSizeAbbrev;


  /* Sensor types so CPU and Proc monitors  can get info from the Sensors
  |  module.  The sensor module readings for temperature and fan are
  |  reported on CPU and Proc panels.
  */
#define SENSOR_TEMPERATURE  0
#define SENSOR_FAN          1
#define SENSOR_VOLTAGE      2

#define	SENSOR_GROUP_MAINBOARD	0
#define	SENSOR_GROUP_DISK		1


#define	FULL_SCALE_GRIDS		5


typedef struct
	{
	PangoFontDescription *font;
	PangoFontDescription **font_seed;
	gpointer	privat;
	gint		effect;
	gint		internal;
	gint		flags;
	GdkColor	color;
	GdkColor	shadow_color;
	}
	GkrellmTextstyle;

typedef struct
	{
	gchar				*text;
	gint				x_off,
						y_off;
	gint				x_off_prev;		/* Kludge to check if d->x_off was */
										/* modified directly.		*/
	gpointer			privat;
	GkrellmTextstyle	text_style;
	}
	GkrellmText;

  /* Values for GkrellmDecal flags
  */
#define DF_TOP_LAYER			0x1
#define	DF_MOVED				0x2
#define DF_LOCAL_PIXMAPS		0x4
#define	DF_OVERLAY_PIXMAPS		0x8
#define	DF_TEXT_OVERLAPS		0x10
#define	DF_SCROLL_TEXT_DIVERTED	0x20
#define	DF_SCROLL_TEXT_CENTER	0x40
#define	DF_SCROLL_TEXT_H_LOOP	0x80
#define	DF_SCROLL_TEXT_V_LOOP	0x100
#define	DF_TEXT_USE_MARKUP		0x200

  /* Values for GkrellmDecal state
  */
#define	DS_INVISIBLE	0
#define	DS_VISIBLE		1

  /* A decal is a pixmap or a part of a pixmap drawn on a panel.  The
  |  pixmap can be a graphic, a vertical stack of graphics, or a drawn
  |  text string
  */
typedef struct
	{
	GdkPixmap	*pixmap;
	GdkBitmap	*mask;
	GdkBitmap	*stencil;
	gint		y_src;		/* Offset into pixmap if a vertical stack	*/
	gint		w, h;		/* Size of the decal						*/
	gint		x, y;		/* Position of decal in a drawable			*/
	gshort		flags,
				state;
	gint		value;		/* Index into stack, text value, etc	*/
	gboolean	modified;
	gint		x_off,
				y_ink;			/* Pixels from top of Pango glyph to 1st ink*/
	gint		x_old, y_old;	/* Used if decal was moved */
	GkrellmTextstyle text_style;	/* Used if decal is a drawn text string	*/
	gpointer	panel;
	GList		*text_list;
	gchar		*scroll_text;
	gint		scroll_width,
				scroll_height,
				y_off;

	guint		chart_sequence_id;	/* For drawing text decal on a chart */
	GdkRectangle ink;
	}
	GkrellmDecal;

  /* Get the first decal in a panel decal list.  Use if only one decal in list.
  */
#define	DECAL(p)	((GkrellmDecal *)(((p)->decal_list)->data))



typedef struct
	{
	gchar		*string;
	gpointer	privat;
	gint		old1,		/* olds are leftovers from GdkFont->Pango */
				old2,
				width,
				height,
				old3;
	gint		position;		/* 0 - 100 %	*/
	gint		x_panel,		/* x position of label start in panel */
				y_panel,
				h_panel;
	}
	GkrellmLabel;


typedef struct
	{
	gint		x_src;
	gint		y_src;
	gint		x_dst;
	gint		y_dst;
	gint		w;
	gint		h;
	}
	GkrellmDrawrec;


typedef struct
	{
	gint		left,
				right,
				top,
				bottom;
	}
	GkrellmMargin;

typedef struct
	{
	gint		left,
				right,
				top,
				bottom;
	}
	GkrellmBorder;


typedef struct
	{
	GdkPixbuf	*pixbuf;
	GkrellmBorder border;
	}
	GkrellmPiximage;


  /* Bit flags for setting styles in config.c.  Also used as override flags.
  */
#define	GKRELLMSTYLE_KRELL_YOFF			0x1
#define	GKRELLMSTYLE_KRELL_LEFT_MARGIN	0x2
#define	GKRELLMSTYLE_KRELL_RIGHT_MARGIN	0x4
#define	GKRELLMSTYLE_KRELL_EXPAND		0x8
#define	GKRELLMSTYLE_KRELL_X_HOT		0x10
#define	GKRELLMSTYLE_KRELL_DEPTH		0x20
#define	GKRELLMSTYLE_KRELL_EMA_PERIOD	0x40
#define	GKRELLMSTYLE_LABEL_POSITION		0x80
#define	GKRELLMSTYLE_LEFT_MARGIN		0x100
#define	GKRELLMSTYLE_RIGHT_MARGIN		0x200
#define	GKRELLMSTYLE_TOP_MARGIN			0x400
#define	GKRELLMSTYLE_BOTTOM_MARGIN		0x800
#define	GKRELLMSTYLE_TEXTCOLOR_A		0x1000
#define	GKRELLMSTYLE_TEXTCOLOR_B		0x2000
#define	GKRELLMSTYLE_TEXTFONT_A			0x4000
#define	GKRELLMSTYLE_TEXTFONT_B			0x8000
#define	GKRELLMSTYLE_BORDER				0x10000
#define	GKRELLMSTYLE_TRANSPARENCY		0x20000
#define	GKRELLMSTYLE_KRELL_YOFF_NOT_SCALABLE 0x40000
#define	GKRELLMSTYLE_SCROLL_TEXT_CACHE_OFF	 0x80000
#define	GKRELLMSTYLE_LABEL_YOFF			0x100000


  /* Some of these style entries do not apply to all monitors.
  */
typedef struct
	{
	gint		override;		/* Flag which entries have override status */
	gint		ref_count;

	gint		krell_yoff,
				krell_expand,
				krell_x_hot,
				krell_ema_period,
				krell_depth;
	gint		krell_left_margin,
				krell_right_margin;
	gboolean	krell_yoff_not_scalable;
	gint		label_yoff;
	gint		label_position;
	gboolean	scroll_text_cache_off;
	gint		spare0;

	gboolean	transparency;
	gint		themed;			/* Non zero if theme has custom assignments */

	GkrellmBorder border;			/* Border for background panel image */
	GkrellmMargin margin;
	GkrellmTextstyle label_tsA;
	GkrellmTextstyle label_tsB;
	}
	GkrellmStyle;



  /* The GkrellmPanel of each GkrellmChart or Meter can have a moving indicator
  |  representing a current sampled data value as a fraction of some full
  |  scale value.  Since there can be quite a few of them dancing around on
  |  a maxed out GKrellM display, I call them Krells - inspired by the
  |  wall full of power monitoring meters the Krell had in Forbidden Planet.
  */
  /* Krell.expand values */
#define	KRELL_EXPAND_NONE				0
#define	KRELL_EXPAND_LEFT				1
#define	KRELL_EXPAND_RIGHT				2
#define	KRELL_EXPAND_BAR_MODE			3
#define KRELL_EXPAND_LEFT_SCALED		4
#define KRELL_EXPAND_RIGHT_SCALED		5
#define	KRELL_EXPAND_BAR_MODE_SCALED	6

  /* Krell flags */
#define	KRELL_FLAG_BOTTOM_MARGIN	1

typedef struct
	{
	GdkPixmap	*pixmap;
	GdkBitmap	*mask;
	gint		w, h;	/* Of the full Krell - all the frames included */
	gint		depth;	/* How many vertical frames in the krell image */
	gint		flags;

	GdkBitmap	*stencil;
	gint		spare0;
	gint		reading;
	gint		last_reading;
	gint		full_scale;
	gulong		previous;
	gulong		value;

	gint		modified;	/* True if krell has moved.			*/

	gint		x_hot;		/* Offset in Krell pixmap to hot spot */
	gint		x_position;	/* Current position of x_hot in Panel */

	gint		x0,		/* Offset in Panel to start of Krell scale range */
				y0;
	gint		w_scale;	/* Width of active range, function of margins */
	gint		h_frame;	/* Height of one frame */

	gint		ema;		/* exponential moving average */
	gint		period;		/* of ema */
	gint		full_scale_expand;
	gboolean	monotonic;
	gint		bar_mode;

	gpointer	panel;
	GkrellmDrawrec	old_draw;	/* Last draw for restoring background.	*/
	GkrellmDrawrec	draw;		/* Parameters to currently drawn krell	*/
	}
	GkrellmKrell;

  /* Get the first krell in a panel krell list.  Use if only one krell in list.
  */
#define	KRELL(p)	((GkrellmKrell *)(((p)->krell_list)->data))

typedef struct
	{
	GtkWidget	*hbox;			/* Container box this area is packed into */
	GdkPixmap	*bg_text_layer_pixmap;  /* A bg_pixmap with text decals */
	GtkWidget	*drawing_area;
	GdkPixmap	*pixmap;		/* Expose pixmap */
	GdkBitmap	*bg_mask;
	GdkPixmap	*bg_pixmap;		/* Bg of panel, may be dirtied with label */
	GdkPixmap	*bg_clean_pixmap;	/* A no label bg_pixmap				*/
	GkrellmPiximage *bg_piximage;

	GkrellmLabel *label;
	GkrellmTextstyle *textstyle;
	GkrellmStyle *style;
	gpointer	monitor;
	gpointer	privat;
	GList		*decal_list;
	GList		*krell_list;
	GList		*button_list;
	gint		x, y, w, h;
	gint		h_configure;

	PangoLayout	*layout;

	gint		transparency;
	gboolean	modified;		/* True if decal/krell modified.	*/
	gboolean	keep_lists;
	gboolean	shown;
	gboolean	need_decal_overlap_check;
	gboolean	label_on_top_of_decals;
	gboolean	scroll_text_cache_off;
	gboolean	bg_piximage_override;
	gboolean	button_signals_connected;
	gint		id_press,		/* Signals if buttons in this panel	*/
				id_release,
				id_enter,
				id_leave;
	gint		y_mapped;
	}
	GkrellmPanel;


typedef struct
	{
	void		(*func)();
	gpointer	data;
	}
	GkrellmCallback;


#define CHART_WIDTH_MAX	1000
#define CHART_WIDTH_MIN	25

  /* Each chart must have a GkrellmChartconfig struct associated with it.
  */
#define	GKRELLM_CHARTCONFIG_KEYWORD	"chart_config"

  /* Values for GkrellmChartconfig flags */
#define	NO_CONFIG_AUTO_GRID_RESOLUTION	1
#define	NO_CONFIG_FIXED_GRIDS			2

typedef struct
	{
	gint		flags;
	gboolean	config_loaded;
	gboolean	log;

	gint		h;
	void		(*cb_height)();
	gpointer	cb_height_data;
	GtkWidget	*height_spin_button;

	/* grid_resolution must be an integer and will be mapped into an integer
	|  1,2,5 or 1,1.5,2,3,5,7 sequence if map_sequence is TRUE.  If
	|  map_sequence is false, grid_resolution will be the spin_button
	|  value * spin_factor so spin buttons may have non-integer settings.
	*/
	gint		grid_resolution;
	gboolean	auto_grid_resolution;
	gboolean	auto_resolution_stick;
	gboolean	sequence_125;
	void		(*cb_grid_resolution)();
	gpointer	cb_grid_resolution_data;
	GtkWidget	*grid_resolution_spin_button;
	GtkWidget	*auto_resolution_control_menubar;
	GtkItemFactory	*auto_resolution_item_factory;
	gchar		*grid_resolution_label;
	gboolean	adjustment_is_set;
	gboolean	map_sequence;
	gfloat		spin_factor;
	gfloat		low,		/* Stuff for the grid_resolution spin adjustment */
				high,
				step0,
				step1;
	gint		digits,
				width;

	gboolean	fixed_grids;
	void		(*cb_fixed_grids)();
	gpointer	cb_fixed_grids_data;
	GtkWidget	*fixed_grids_spin_button;

	GList		*cd_list;			/* Same list as in parent GkrellmChart */
	GList		**chart_cd_list;

	gboolean	cb_block;
	}
	GkrellmChartconfig;

  /* GkrellmCharts are drawn in layers and each data value drawn has its own
  |  layer (the GkrellmChartdata struct -> image/color of the drawn data and
  |  the data colored grid lines).  Each layer is a chart sized pixmap
  |  with grid lines drawn on it.  A chart is drawn by drawing the
  |  background followed by stenciling each data layer through a
  |  data_bitmap.
  */
typedef struct
	{
	GdkPixmap	*pixmap;			/* Working layer pixmap with grid lines	*/
	GdkPixmap	**src_pixmap;		/* The layer source image/color		*/
	GdkPixmap	*grid_pixmap;		/* The grid pixmap to draw on src 	*/
	}
	GkrellmChartlayer;


typedef struct
	{
	GtkWidget		*vbox,
					*image;
	GkrellmPiximage	*piximage;
	GdkPixmap		*pixmap,
					*clean_pixmap;
	GdkBitmap		*mask;
	gint			height;			/* Width is _GK.chart_width */
	}
	GkrellmSpacer;

typedef struct
	{
	gchar			*text;
	gint			x, y;
	gint			w, h;
	gint			baseline,
					y_ink;
	gboolean		cache_valid;
	}
	GkrellmTextRun;

typedef struct
	{
	GtkWidget	*box;			/* Container box this chart is packed into */
	GtkWidget	*drawing_area;

	GkrellmPanel *panel;
	GList		*cd_list;		/* Same cd_list as in GkrellmChartconfig struct */
	gint		cd_list_index;
	void		(*draw_chart)(gpointer);
	gpointer	draw_chart_data;

	gint		x, y, w, h;		/* h tracks h in GkrellmChartconfig	*/

	GkrellmChartconfig	*config;
	gboolean	shown;
	gboolean	redraw_all;

	gint		position,
				tail;
	gboolean	primed;

	gint		privat;
	gint		i0;				/* Draw start index */
	gint		alloc_width;

	gint		scale_max;
	gint		maxval,
				maxval_auto,
				maxval_auto_base,
				maxval_peak;
	gulong		previous_total;

	gint		style_id;

	GdkPixmap	*pixmap;			/* The expose pixmap.			*/

	GkrellmPiximage
				*bg_piximage,
				*bg_grid_piximage;
	GdkPixmap	*bg_pixmap,			/* Working bg with grid lines	*/
				*bg_src_pixmap,		/* bg with no grid lines		*/
				*bg_grid_pixmap,	/* This + bg_src_pixmap builds bg_pixmap*/
				*bg_clean_pixmap,	/* This + trans builds bg_src_pixmap */
				*bg_text_pixmap;	/* Pango rendered chart text layer */
	GdkBitmap	*bg_mask;
	gint		transparency;
	gboolean	bg_piximage_override;
	guint		bg_sequence_id;

	gchar		*text_format_string;
	GList		*text_run_list;		/* Caching chart text runs */
	gint		h_text,
				y_ink,
				baseline_ref;
	guint		text_run_sequence_id;
	gboolean	text_format_reuse;

	gint		y_mapped,
				auto_recalibrate_delay;

	GtkWidget	*config_window;

	GkrellmSpacer	top_spacer,
					bottom_spacer;

	GkrellmStyle	*style;
	gpointer		monitor;
	}
	GkrellmChart;


  /* Values for GkrellmChartdata draw_style */
#define	CHARTDATA_IMPULSE	0
#define	CHARTDATA_LINE		1

  /* Values for GkrellmChartdata flags */
#define CHARTDATA_ALLOW_HIDE			1
#define CHARTDATA_NO_CONFIG_DRAW_STYLE	2
#define CHARTDATA_NO_CONFIG_INVERTED	4
#define CHARTDATA_NO_CONFIG_SPLIT		8
#define CHARTDATA_NO_CONFIG				0xf

typedef struct
	{
	gint		flags;

	GkrellmChart *chart;			/* GkrellmChart this data belong to */
	gchar		*label;
	gulong		current,
				previous,
				discard;
	gboolean	monotonic;
	GkrellmChartlayer layer;	/* The image + grid for this data layer */
	GdkBitmap	*data_bitmap;	/* Draw data here, use as clipmask for layer*/
	gint		*data;

	gint		draw_style;
	gboolean	inverted;
	gboolean	hide;

	gboolean	split_chart;
	gfloat		split_fraction,
				split_share;
	GtkWidget	*split_fraction_spin_button;

	gint		y_p,
				y_pp;
	gboolean	y_p_valid;
	gint		y,				/* Each data layer draws at an offset y and */
				h,				/* within height h of its parent GkrellmChart*/
				w;				/* Width of data allocated.				*/
	gint		maxval;
	}
	GkrellmChartdata;


typedef struct
	{
	GkrellmPanel *panel;
	GkrellmDecal *decal;
	void		(*cb_button_click)();
	gpointer	data;
	gint		(*cb_in_button)();
	gpointer	in_button_data;
	gpointer	privat;
	gint		cur_index;
	gint		pressed_index;
	gint		saved_index;
	gint		sensitive;
	gint		type;
	void		(*cb_button_right_click)();
	gpointer	right_data;
	}
	GkrellmDecalbutton;



typedef struct
	{
	gchar		*command;
	gint		type;
	gint		pad;
	FILE		*pipe;		/* Read the output of some commands */
	GkrellmDecalbutton	*button;
	GtkTooltips	*tooltip;
	gchar		*tooltip_comment;
	GkrellmDecal *decal;		/* Used if DECAL_LAUNCHER type	*/
	GkrellmMargin margin;

	GtkWidget	*widget;
	}
	GkrellmLauncher;


/* ------- Alerts ------- */
#define	GKRELLM_ALERTCONFIG_KEYWORD			"alert_config"

typedef struct
	{
	struct _GkrellmMonitor	*mon;
	gchar		*name,
				*tab_name;
	void		(*warn_func)(),
				(*alarm_func)(),
				(*update_func)(),
				(*check_func)(),
				(*destroy_func)();
	void		(*config_create_func)(),
				(*config_apply_func)(),
				(*config_save_func)(),
				(*config_load_func)();
	}
	GkrellmAlertPlugin;

typedef struct
	{
	GkrellmAlertPlugin	*alert_plugin;
	gpointer			data;
	}
	GkrellmAlertPluginLink;

typedef struct
	{
	gboolean	warn_on,
				alarm_on;
	gfloat		warn_limit,
				alarm_limit;
	gint		warn_delay,
				alarm_delay;
	GtkWidget	*warn_limit_spin_button,
				*alarm_limit_spin_button;
	}
	GkrellmTrigger;

typedef struct
	{
	GkrellmDecal *decal;
	gint		x, y, w, h;
	GdkPixmap	*pixmap;
	GdkBitmap	*mask;
	gint		frame,
				nframes,
				dir;
	}
	GkrellmAlertdecal;

typedef struct
	{
	GkrellmKrell *krell;
	gint		krell_position;
	}
	GkrellmAlertkrell;

typedef struct
	{
	GkrellmPanel *panel;
	gchar		*name,
				*unit_string;
	GkrellmAlertdecal ad;
	GkrellmAlertkrell ak;

	gboolean	activated,
				freeze,
				do_panel_updates,
				suppress_command,
				check_low,
				check_high,
				check_hardwired;
	gchar		*warn_command,
				*alarm_command;
	gint		warn_repeat_set,
				warn_repeat,
				alarm_repeat_set,
				alarm_repeat;
	gint		delay;

	void		(*cb_trigger)();
	gpointer	cb_trigger_data;
	void		(*cb_stop)();
	gpointer	cb_stop_data;
	void		(*cb_config)();
	gpointer	cb_config_data;
	void		(*cb_config_create)();
	gpointer	cb_config_create_data;
	void		(*cb_command_process)();
	gpointer	cb_command_process_data;

	GtkWidget	*config_window,
				*warn_command_entry,
				*alarm_command_entry,
				*warn_repeat_spin_button,
				*alarm_repeat_spin_button,
				*delay_spin_button,
				*delete_button,
				*icon_box;
	gboolean	do_alarm_command,
				do_warn_command;

	gfloat		max_high,		/* limit adjustment values */
				min_low,
				step0,
				step1;
	gint		digits;

	gint		delay_high,		/* delay adjustment values */
				delay_low,
				delay_step;

	GkrellmTrigger low,
				high;

	gboolean	config_closing,
				config_modified;

	gchar		*id_string;		/* For unique alert names for alert plugins */
	GList		*plugin_list;
	}
	GkrellmAlert;

/* ------------------------ */


#define	GKRELLM_SPACER_CHART	0
#define	GKRELLM_SPACER_METER	1

#define	CHART_PANEL_TYPE	0
#define	METER_PANEL_TYPE	1
#define	N_PANEL_TYPES		2

#define	PANEL_LAUNCHER		CHART_PANEL_TYPE
#define	METER_LAUNCHER		METER_PANEL_TYPE
#define	DECAL_LAUNCHER		3

#define	DEFAULT_STYLE_ID	0


/* GkrellmStyle names for the builtin monitors.  Define them globally so plugins
|  can lookup a builtin style_id.  These names are used as subdirectory
|  names under the current theme where monitor specific images are located.
|  They also are used in the GkrellmStyle lines in the gkrellmrc
|		(eg. GkrellmStyleMeter  cpu.textcolor ....
*/
#define	CPU_STYLE_NAME			"cpu"
#define	DISK_STYLE_NAME			"disk"
#define	NET_STYLE_NAME			"net"
#define	PROC_STYLE_NAME			"proc"
#define	INET_STYLE_NAME			"inet"

#define	MEM_STYLE_NAME			"mem"
#define SWAP_STYLE_NAME			"swap"
#define	FS_STYLE_NAME			"fs"
#define	MAIL_STYLE_NAME			"mail"

  /* APM monitor is now named Battery, but don't want to break themes. */
#define	BATTERY_STYLE_NAME		"apm"
#define UPTIME_STYLE_NAME		"uptime"
#define	CLOCK_STYLE_NAME		"clock"
#define	CAL_STYLE_NAME			"cal"
#define	HOST_STYLE_NAME			"host"
#define	TIMER_STYLE_NAME		"timer"


typedef struct
	{
	gint		timer_ticks,
				second_tick,
				two_second_tick,
				five_second_tick,
				ten_second_tick,
				minute_tick,
				hour_tick,
				day_tick;

	}
	GkrellmTicks;

extern GkrellmTicks	GK;


/* ================= User Config ==================*/

  /* Values for GkrellmMonitor id
  |  Give an id number to all builtin monitors
  |				____ cccc ___i iiii
  */
#define	MON_CPU		0
#define	MON_PROC	1
#define	MON_DISK	2
#define	MON_NET		3
#define	MON_INET	4
#define	N_CHART_MONITORS 5

#define	MON_MEM		5
#define	MON_FS		6
#define	MON_MAIL	7
#define	MON_BATTERY	8
#define	MON_APM		MON_BATTERY
#define	MON_UPTIME	9
#define	MON_CLOCK	10
#define	MON_CAL		11
#define	MON_TIMER	12
#define	MON_HOST	13
#define	MON_SWAP	14
#define MON_VOLTAGE	15
#define N_BUILTIN_MONITORS	16
#define MON_PLUGIN	16

#define	MON_ID_MASK		0x1f

#define MON_INSERT_AFTER		0x200
#define MON_CONFIG_MASK			0xf00
#define	MON_GRAVITY_MASK		0xf000

#define	GRAVITY(n)			((n) << 12)
#define	PLUGIN_GRAVITY(m)	(((m)->insert_before_id & MON_GRAVITY_MASK) >> 12)

#define	PLUGIN_INSERT_BEFORE_ID(p)	((p)->insert_before_id & MON_ID_MASK)
#define PLUGIN_INSERT_AFTER(p)	\
							(((p)->insert_before_id & MON_INSERT_AFTER) >> 9)

#define	MONITOR_ID(m)			((m)->id & MON_ID_MASK)
#define	MONITOR_CONFIG(m,flag)	(((m)->id & MON_CONFIG_MASK) & flag)

#define	MONITOR_ENABLED(mon)	((mon)->privat->enabled)

typedef struct
	{
	GtkWidget		*main_vbox,
					*vbox;

	gint			config_page;
	GtkTreeRowReference	*row_reference;
	GtkWidget		*config_vbox;
	gboolean		config_created;

	void			(*cb_disable_plugin)(void);
	gint			insert_before_id,
					gravity,
					button_id;
	gboolean		insert_after;
	gboolean		enabled,
					created,
					from_command_line,
					spacers_shown,
					cap_images_off,
					spacer_overlap_off,
					instant_apply;
	GkrellmStyle	*panel_style,
					*chart_style;
	gchar			*style_name;
	gint			style_type,
					style_id;	/* helper until I consolidate image lists */
	gint			top_type,
					bottom_type;
	GkrellmSpacer	top_spacer,
					bottom_spacer;
	}
	GkrellmMonprivate;



typedef struct _GkrellmMonitor
	{
	gchar		*name;
	gint		id;
	void		(*create_monitor)(GtkWidget *, gint);
	void		(*update_monitor)(void);
	void		(*create_config)(GtkWidget *);
	void		(*apply_config)(void);

	void		(*save_user_config)(FILE *);
	void		(*load_user_config)(gchar *);
	gchar		*config_keyword;

	void		(*undef2)(void);
	void		(*undef1)(void);
	GkrellmMonprivate *privat;

	gint		insert_before_id;		/* If plugin, insert before this mon*/

	void		*handle;				/* If monitor is a plugin.	*/
	gchar		*path;					/* 	"						*/
	}
	GkrellmMonitor;


#include "gkrellm-public-proto.h"

#endif // GKRELLM_H

