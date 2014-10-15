/* GKrellM
|  Copyright (C) 1999-2014 Bill Wilson
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

#include <errno.h>
#if !defined(F_TLOCK)
#include <sys/file.h>
#endif

#include <pwd.h>
#include <sys/types.h>

#include <gdk/gdkx.h>
#include <X11/Xmd.h>
#include <X11/SM/SMlib.h>
#include <X11/Xatom.h>

#define	_NET_WM_STATE_REMOVE	0
#define	_NET_WM_STATE_ADD		1
#define	_NET_WM_STATE_TOGGLE	2


static Pixmap	root_xpixmap	= None;
static GdkGC	*trans_gc;

static gchar 	*client_id;


void
gkrellm_winop_reset(void)
	{
	root_xpixmap = None;
	}


static void
cb_smc_save_yourself(SmcConn smc_conn, SmPointer client_data, gint save_type,
			gboolean shutdown, gint interact_style, gboolean fast)
	{
	gkrellm_save_all();
	SmcSaveYourselfDone(smc_conn, True);
	}

static void
cb_smc_die(SmcConn smc_conn, SmPointer client_data)
	{
	SmcCloseConnection(smc_conn, 0, NULL);
	gtk_main_quit();
	}

static void
cb_smc_save_complete(SmcConn smc_conn, SmPointer client_data)
	{
	}

static void
cb_smc_shutdown_cancelled(SmcConn smc_conn, SmPointer client_data)
	{
	}

static void
cb_ice_connection_messages(IceConn ice_connection, gint source,
		GdkInputCondition condition)
	{
	IceProcessMessages(ice_connection, NULL, NULL);
	}

static void
smc_connect(gint argc, gchar **argv)
	{
	SmProp			userid, program, restart, clone, pid, *props[6];
#if 0
	SmProp			restart_style;
	CARD8			restartstyle;
	SmPropValue		restart_style_val;
#endif
	SmPropValue		userid_val, pid_val;
	SmcCallbacks	*callbacks;
	SmcConn			smc_connection;
	IceConn			ice_connection;
	struct passwd	*pwd;
	uid_t			uid;
	gchar			error_string[256], pid_str[16], userid_string[256];
	gulong			mask;
	gint			i, j;

	/* Session manager callbacks
	*/
	callbacks = g_new0(SmcCallbacks, 1);
	callbacks->save_yourself.callback = cb_smc_save_yourself;
	callbacks->die.callback = cb_smc_die;
	callbacks->save_complete.callback = cb_smc_save_complete;
	callbacks->shutdown_cancelled.callback = cb_smc_shutdown_cancelled;

	mask = SmcSaveYourselfProcMask | SmcDieProcMask | SmcSaveCompleteProcMask
				| SmcShutdownCancelledProcMask;

	smc_connection = SmcOpenConnection(NULL /* SESSION_MANAGER env variable */,
						NULL /* share ICE connection */,
						SmProtoMajor, SmProtoMinor, mask,
						callbacks,
						_GK.session_id, &client_id,
						sizeof(error_string), error_string);
	g_free(callbacks);
	if (!smc_connection)
		return;

	gdk_set_sm_client_id(client_id);

	/* Session manager properties - 4 are required.
	*/
	userid.name = SmUserID;
	userid.type = SmARRAY8;
	userid.num_vals = 1;
	userid.vals = &userid_val;
	uid = getuid();
	if ((pwd = getpwuid(uid)) != NULL)
		snprintf(userid_string, sizeof(userid_string), "%s", pwd->pw_name);
	else
		snprintf(userid_string, sizeof(userid_string), "%d", uid);
	userid_val.value = userid_string;
	userid_val.length = strlen(userid_string);

	pid.name = SmProcessID;
	pid.type = SmARRAY8;
	pid.num_vals = 1;
	pid.vals = &pid_val;
	sprintf(pid_str, "%i", getpid());
	pid_val.value = (SmPointer) pid_str;
	pid_val.length = strlen(pid_str);

	restart.name = SmRestartCommand;
	restart.type = SmLISTofARRAY8;
	restart.vals = g_new0(SmPropValue, argc + 2);
	j = 0;
	for (i = 0; i < argc; ++i) {
		if ( strcmp(argv[i], "--sm-client-id") ) {
			restart.vals[j].value = (SmPointer) argv[i];
			restart.vals[j++].length = strlen(argv[i]);
		} else
			i++;
	}
	restart.vals[j].value = (SmPointer) "--sm-client-id";
	restart.vals[j++].length = strlen("--sm-client-id");
	restart.vals[j].value = (SmPointer) client_id;
	restart.vals[j++].length = strlen(client_id);
	restart.num_vals = j;

#if 0
	restartstyle = SmRestartImmediately;
	restart_style.name = SmRestartStyleHint;
	restart_style.type = SmCARD8;
	restart_style.num_vals = 1;
	restart_style.vals = &restart_style_val;
	restart_style_val.value = (SmPointer) &restartstyle;
	restart_style_val.length = 1;
#endif

	clone.name = SmCloneCommand;
	clone.type = SmLISTofARRAY8;
	clone.vals = restart.vals;
	clone.num_vals = restart.num_vals - 2;

	program.name = SmProgram;
	program.type = SmARRAY8;
	program.vals = restart.vals;
	program.num_vals = 1;

	props[0] = &program;
	props[1] = &userid;
	props[2] = &restart;
	props[3] = &clone;
	props[4] = &pid;
#if 0
	/* Make this an option? */
	props[5] = &restart_style;
	SmcSetProperties(smc_connection, 6, props);
#else
	SmcSetProperties(smc_connection, 5, props);
#endif

	g_free(restart.vals);

	ice_connection = SmcGetIceConnection(smc_connection);
	gdk_input_add(IceConnectionNumber(ice_connection), GDK_INPUT_READ,
				(GdkInputFunction) cb_ice_connection_messages, ice_connection);
	}

static void
net_wm_state(gchar *hint, gboolean state)
	{
	XEvent	xev;

	xev.type = ClientMessage;
	xev.xclient.type = ClientMessage;
	xev.xclient.window = GDK_WINDOW_XWINDOW(gkrellm_get_top_window()->window);
	xev.xclient.message_type = gdk_x11_get_xatom_by_name("_NET_WM_STATE");
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = state ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
	xev.xclient.data.l[1] = gdk_x11_get_xatom_by_name(hint);
	xev.xclient.data.l[2] = 0;

	XSendEvent(GDK_DISPLAY(), GDK_ROOT_WINDOW(),
				False, SubstructureNotifyMask|SubstructureRedirectMask, &xev);
	}

void
gkrellm_winop_state_skip_taskbar(gboolean state)
	{
	if (!_GK.is_dock_type)
		net_wm_state("_NET_WM_STATE_SKIP_TASKBAR", state);
	}

void
gkrellm_winop_state_skip_pager(gboolean state)
	{
	if (!_GK.is_dock_type)
		net_wm_state("_NET_WM_STATE_SKIP_PAGER", state);
	}

void
gkrellm_winop_state_above(gboolean state)
	{
	net_wm_state("_NET_WM_STATE_ABOVE", state);
	/* Apparently KDE 3.1.0 and possibly below does not implement
	 * _NET_WM_STATE_ABOVE but _NET_WM_STATE_STAYS_ON_TOP that implies
	 * approximately the same thing
	 */
	net_wm_state("_NET_WM_STATE_STAYS_ON_TOP", state);
	}

void
gkrellm_winop_state_below(gboolean state)
	{
	net_wm_state("_NET_WM_STATE_BELOW", state);
	}

static FILE		*f_lock;

static gboolean
_gkrellm_get_lock(void)
	{
	gchar	*lock_dir, *lock_file, *display, *s;
	gchar	buf[32];

	snprintf(buf, sizeof(buf), "LCK..gkrellm");

#if defined(F_TLOCK)
	lock_dir = g_strdup_printf("/var/lock/gkrellm-%d", (gint) getuid());
	if (!g_file_test(lock_dir, G_FILE_TEST_IS_DIR))
		mkdir(lock_dir, 0755);

	lock_file = gkrellm_make_config_file_name(lock_dir, buf);
	g_free(lock_dir);
	display = XDisplayName(NULL);
	if (display)
		{
		s = g_strconcat(lock_file, "_", display, NULL);
		g_free(lock_file);
		lock_file = s;
		}
	f_lock = fopen(lock_file, "w+");	/* buffering does not apply here */
	g_free(lock_file);
	if (   f_lock
		&& lockf(fileno(f_lock), F_TLOCK, 0) != 0
		&& errno == EAGAIN
	   )
		return FALSE;
	if (f_lock)
		{
		fprintf(f_lock, "%10d\n", (gint) getpid());
		fflush(f_lock); 
		}
#endif
	return TRUE;
	}

enum
	{
	STRUT_LEFT = 0,
	STRUT_RIGHT = 1,
	STRUT_TOP = 2,
	STRUT_BOTTOM = 3,
	STRUT_LEFT_START = 4,
	STRUT_LEFT_END = 5,
	STRUT_RIGHT_START = 6,
	STRUT_RIGHT_END = 7,
	STRUT_TOP_START = 8,
	STRUT_TOP_END = 9,
	STRUT_BOTTOM_START = 10,
	STRUT_BOTTOM_END = 11
	};

static Atom net_wm_strut_partial = None;
static Atom net_wm_strut = None;

void
gkrellm_winop_update_struts(void)
	{
	gulong	struts[12] = { 0, };
	Display	*display;
	Window	window;
	gint	width; 
	gint	height;

	if (!_GK.is_dock_type)
		return;

	display = GDK_WINDOW_XDISPLAY(gkrellm_get_top_window()->window);
	window  = GDK_WINDOW_XWINDOW(gkrellm_get_top_window()->window);

	if (net_wm_strut_partial == None)
		{
		net_wm_strut_partial
				= XInternAtom(display, "_NET_WM_STRUT_PARTIAL", False);
		}
	if (net_wm_strut == None)
		{
		net_wm_strut = XInternAtom(display, "_NET_WM_STRUT", False);
		}

	gtk_window_get_size(GTK_WINDOW(gkrellm_get_top_window()), &width, &height);

	if (_GK.x_position == 0)
		{
		struts[STRUT_LEFT] = width;
		struts[STRUT_LEFT_START] = _GK.y_position;
		struts[STRUT_LEFT_END] = _GK.y_position + height;
		}
	else if (_GK.x_position == _GK.w_display - width)
		{
		struts[STRUT_RIGHT] = width;
		struts[STRUT_RIGHT_START] = _GK.y_position;
		struts[STRUT_RIGHT_END] = _GK.y_position + height;
		}

	gdk_error_trap_push();
	XChangeProperty (display, window, net_wm_strut,
					XA_CARDINAL, 32, PropModeReplace,
					(guchar *) &struts, 4);
	XChangeProperty (display, window, net_wm_strut_partial,
					XA_CARDINAL, 32, PropModeReplace,
					(guchar *) &struts, 12);
	gdk_error_trap_pop();
	}

void
gkrellm_winop_options(gint argc, gchar **argv)
	{
	Display		*display;
	Window		window;
	Atom		atoms[4];
	gint		n = 0;

	if (   !_GK.allow_multiple_instances_real
		&& !_GK.force_host_config
		&& !_gkrellm_get_lock()
	   )
		{
		g_message("gkrellm: %s\n",
			_("Exiting because multiple instances option is off.\n"));
		exit(0);
		}
	smc_connect(argc, argv);
	display = GDK_WINDOW_XDISPLAY(gkrellm_get_top_window()->window);
	window  = GDK_WINDOW_XWINDOW(gkrellm_get_top_window()->window);

	/* Set window type or list of states using standard EWMH hints.
	|  See http://www.freedesktop.org/
	|  At least KDE3 and GNOME2 are EWMH compliant.
	*/
	if (_GK.dock_type && !_GK.command_line_decorated)
		{
		atoms[0] = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DOCK", False);
    
		XChangeProperty(display, window,
					XInternAtom(display, "_NET_WM_WINDOW_TYPE", False),
					XA_ATOM, 32, PropModeReplace, (guchar *) atoms, 1);
		_GK.is_dock_type = TRUE;
		_GK.state_skip_taskbar = FALSE;
		_GK.state_skip_pager = FALSE;
		}
	if (_GK.state_skip_taskbar)
		{
		atoms[n] = XInternAtom(display, "_NET_WM_STATE_SKIP_TASKBAR", False);
		++n;
		}
	if (_GK.state_skip_pager)
		{
		atoms[n] = XInternAtom(display, "_NET_WM_STATE_SKIP_PAGER", False);
		++n;
		}
	if (_GK.state_above)
		{
		atoms[n++] = XInternAtom(display, "_NET_WM_STATE_ABOVE", False);
		/* see gkrellm_winop_state_above() */
		atoms[n++] = XInternAtom(display, "_NET_WM_STATE_STAYS_ON_TOP", False);
		_GK.state_below = FALSE;
		}
	if (_GK.state_below)
		{
		atoms[n] = XInternAtom(display, "_NET_WM_STATE_BELOW", False);
		++n;
		}
	if (n > 0)
		XChangeProperty(display, window,
					XInternAtom(display, "_NET_WM_STATE", False),
					XA_ATOM, 32, PropModeReplace, (guchar *) atoms, n);
	} 

void
gkrellm_winop_withdrawn(void)
	{
	Display		*display;
	Window		window;

	if (!_GK.withdrawn)
		return;
	display = GDK_WINDOW_XDISPLAY(gkrellm_get_top_window()->window);
	window  = GDK_WINDOW_XWINDOW(gkrellm_get_top_window()->window);

	if (!_GK.is_dock_type)
		{
		XWMHints mywmhints; 
		mywmhints.initial_state = WithdrawnState; 
		mywmhints.flags=StateHint;

		XSetWMHints(display, window, &mywmhints); 
		}
	else
		gkrellm_message_dialog(NULL,
			_("Warning: -w flag is ignored when the window dock type is set"));
	}

  /* Use XParseGeometry, but width and height are ignored.
  |  If GKrellM is moved, update _GK.y_position.
  */
void
gkrellm_winop_place_gkrellm(gchar *geom)
    {
	gint	place, x, y, w_gkrell, h_gkrell;

	x = y = 0;
	place = XParseGeometry(geom, &x, &y,
				(guint *) &w_gkrell, (guint *) &h_gkrell);

	w_gkrell = _GK.chart_width + _GK.frame_left_width + _GK.frame_right_width;
	h_gkrell = _GK.monitor_height + _GK.total_frame_height;

	if (place & YNegative)
		y = _GK.h_display - h_gkrell + y;
	if (place & XNegative)
		x = _GK.w_display - w_gkrell + x;
	gdk_window_move(gkrellm_get_top_window()->window, x, y);
	_GK.y_position = y;
	_GK.x_position = x;
	_GK.position_valid = TRUE;
	gkrellm_debug(DEBUG_POSITION, "geometry moveto %d %d\n", x, y);
	}

void
gkrellm_winop_flush_motion_events(void)
	{
	XEvent			xevent;

	gdk_flush();
	while (XCheckTypedEvent(GDK_DISPLAY(), MotionNotify, &xevent))
		;
	}

  /* Check if background has changed
  */
gboolean
gkrellm_winop_updated_background(void)
	{
	Pixmap	root_pix = None;
	Atom	prop, ret_type = (Atom) 0;
	guchar	*prop_return = NULL;
	gint	fmt;
	gulong	nitems, bytes_after;
  
	if (!_GK.any_transparency)
		return FALSE;
	prop = XInternAtom(GDK_DISPLAY(), "_XROOTPMAP_ID", True);
	if (prop == None)
		return FALSE;
  
	XGetWindowProperty(GDK_DISPLAY(), GDK_ROOT_WINDOW(), prop, 0L, 1L, False,
			AnyPropertyType, &ret_type, &fmt, &nitems, &bytes_after,
			&prop_return);
	if (prop_return && ret_type == XA_PIXMAP)
		{
		root_pix = *((Pixmap *) prop_return);
		XFree(prop_return);
		}
	else
		return FALSE;

	if (root_pix != root_xpixmap)
		{
		root_xpixmap = root_pix;
		return TRUE;
		}
	return FALSE;
	}

gboolean
gkrellm_winop_draw_rootpixmap_onto_transparent_chart(GkrellmChart *cp)
	{
	Window			child;
	GkrellmMargin	*m;
	gint			x, y;

	if (   root_xpixmap == None || trans_gc == NULL || !cp->transparency
		|| !cp->drawing_area || !cp->drawing_area->window
	   )
		return FALSE;
	XTranslateCoordinates(GDK_DISPLAY(),
			GDK_WINDOW_XWINDOW(cp->drawing_area->window),
			GDK_ROOT_WINDOW(),
			0, 0, &x, &y, &child);
	XSetTSOrigin(GDK_DISPLAY(), GDK_GC_XGC(trans_gc), -x, -y);

	/* First make the chart totally transparent
	*/
	gdk_draw_rectangle(cp->bg_src_pixmap, trans_gc,
					TRUE, 0, 0, cp->w, cp->h);

	/* If mode permits, stencil on non transparent parts of bg_clean_pixmap.
	*/
	if (cp->transparency == 2 && cp->bg_mask)
		{
		gdk_gc_set_clip_mask(_GK.text_GC, cp->bg_mask);
		gdk_draw_drawable(cp->bg_src_pixmap, _GK.text_GC,
				cp->bg_clean_pixmap, 0, 0, 0, 0, cp->w, cp->h);
		}
	m = &cp->style->margin;
	if (cp->top_spacer.pixmap)
		{
		XSetTSOrigin(GDK_DISPLAY(), GDK_GC_XGC(trans_gc), -x, -(y - m->top));
		gdk_draw_rectangle(cp->top_spacer.pixmap, trans_gc,
					TRUE, 0, 0, cp->w, cp->style->margin.top);
		if (cp->transparency == 2 && cp->top_spacer.mask)
			{
			gdk_gc_set_clip_mask(_GK.text_GC, cp->top_spacer.mask);
			gdk_draw_drawable(cp->top_spacer.pixmap, _GK.text_GC,
						cp->top_spacer.clean_pixmap, 0, 0, 0, 0,
						cp->w, cp->style->margin.top);
			}
		gtk_image_set_from_pixmap(GTK_IMAGE(cp->top_spacer.image),
					cp->top_spacer.pixmap, NULL);
		}

	if (cp->bottom_spacer.pixmap)
		{
		XSetTSOrigin(GDK_DISPLAY(), GDK_GC_XGC(trans_gc),
					-x, -(y + cp->h - m->bottom));
		gdk_draw_rectangle(cp->bottom_spacer.pixmap, trans_gc,
					TRUE, 0, 0, cp->w, cp->style->margin.bottom);
		if (cp->transparency == 2 && cp->bottom_spacer.mask)
			{
			gdk_gc_set_clip_mask(_GK.text_GC, cp->bottom_spacer.mask);
			gdk_draw_drawable(cp->bottom_spacer.pixmap, _GK.text_GC,
						cp->bottom_spacer.clean_pixmap, 0, 0, 0, 0,
						cp->w, cp->style->margin.bottom);
			}
		gtk_image_set_from_pixmap(GTK_IMAGE(cp->bottom_spacer.image),
					cp->bottom_spacer.pixmap, NULL);
		}
	gdk_gc_set_clip_mask(_GK.text_GC, NULL);
	cp->bg_sequence_id += 1;
	return TRUE;
	}	

gboolean
gkrellm_winop_draw_rootpixmap_onto_transparent_panel(GkrellmPanel *p)
	{
	Window	child;
	gint	x, y;

	if (   root_xpixmap == None || trans_gc == NULL || !p->transparency
		|| !p->drawing_area || !p->drawing_area->window
	   )
		return FALSE;
	XTranslateCoordinates(GDK_DISPLAY(),
			GDK_WINDOW_XWINDOW(p->drawing_area->window),
			GDK_ROOT_WINDOW(),
			0, 0, &x, &y, &child);
	XSetTSOrigin(GDK_DISPLAY(), GDK_GC_XGC(trans_gc), -x, -y);

	/* First make the panel totally transparent
	*/
	gdk_draw_rectangle(p->bg_pixmap, trans_gc, TRUE, 0, 0, p->w, p->h);

	/* If mode permits, stencil on non transparent parts of bg_clean_pixmap.
	*/
	if (p->transparency == 2 && p->bg_mask)
		{
        gdk_gc_set_clip_mask(_GK.text_GC, p->bg_mask);
        gdk_draw_drawable(p->bg_pixmap, _GK.text_GC, p->bg_clean_pixmap,
                    0, 0, 0, 0, p->w, p->h);
        gdk_gc_set_clip_mask(_GK.text_GC, NULL);
		}
	return TRUE;
	}	

static void
draw_rootpixmap_onto_transparent_spacers(GkrellmMonitor *mon, gint xr, gint yr)
	{
	GkrellmMonprivate	*mp = mon->privat;
	gint				x, y;

	if (mp->top_spacer.image)
		{
		x = xr + mp->top_spacer.image->allocation.x;
		y = yr + mp->top_spacer.image->allocation.y;
		XSetTSOrigin(GDK_DISPLAY(), GDK_GC_XGC(trans_gc), -x, -y);
		gdk_draw_rectangle(mp->top_spacer.pixmap, trans_gc,
				TRUE, 0, 0, _GK.chart_width, mp->top_spacer.height);
		if (mp->top_spacer.mask)
			{
			gdk_gc_set_clip_mask(_GK.text_GC, mp->top_spacer.mask);
			gdk_draw_drawable(mp->top_spacer.pixmap, _GK.text_GC,
						mp->top_spacer.clean_pixmap, 0, 0, 0, 0,
						_GK.chart_width, mp->top_spacer.height);
			}
		gtk_image_set_from_pixmap(GTK_IMAGE(mp->top_spacer.image),
				mp->top_spacer.pixmap, NULL);
		}
	if (mp->bottom_spacer.image)
		{
		x = xr + mp->bottom_spacer.image->allocation.x;
		y = yr + mp->bottom_spacer.image->allocation.y;
		XSetTSOrigin(GDK_DISPLAY(), GDK_GC_XGC(trans_gc), -x, -y);
		gdk_draw_rectangle(mp->bottom_spacer.pixmap, trans_gc,
				TRUE, 0, 0, _GK.chart_width, mp->bottom_spacer.height);
		if (mp->bottom_spacer.mask)
			{
			gdk_gc_set_clip_mask(_GK.text_GC, mp->bottom_spacer.mask);
			gdk_draw_drawable(mp->bottom_spacer.pixmap, _GK.text_GC,
						mp->bottom_spacer.clean_pixmap, 0, 0, 0, 0,
						_GK.chart_width, mp->bottom_spacer.height);
			}
		gtk_image_set_from_pixmap(GTK_IMAGE(mp->bottom_spacer.image),
				mp->bottom_spacer.pixmap, NULL);
		}
	}

void
gkrellm_winop_apply_rootpixmap_transparency(void)
	{
	Window			child;
	GtkWidget		*top_window = gkrellm_get_top_window();
	Atom			prop,
					ret_type	= (Atom) 0;
	GList			*list;
	GkrellmMonitor	*mon;
	GkrellmChart	*cp;
	GkrellmPanel	*p;
	guchar			*prop_return = NULL;
	gint			fmt;
	gulong			nitems, bytes_after;
	gint			depth_visual;
	Window			root_return;
	guint			w_ret, h_ret, bw_ret, depth_ret;
	gint			x_ret, y_ret, x_root, y_root;

	if (!_GK.any_transparency)
		return;
	prop = XInternAtom(GDK_DISPLAY(), "_XROOTPMAP_ID", True);
	if (prop == None)
		return;
	XGetWindowProperty(GDK_DISPLAY(), GDK_ROOT_WINDOW(), prop, 0L, 1L, False,
			AnyPropertyType, &ret_type, &fmt, &nitems, &bytes_after,
			&prop_return);
	if (prop_return && ret_type == XA_PIXMAP)
		{
		root_xpixmap = *((Pixmap *) prop_return);
		XFree(prop_return);
		}
	if (root_xpixmap == None)
		return;
	if (trans_gc == NULL)
		{
		trans_gc = gdk_gc_new(top_window->window);
		gdk_gc_copy(trans_gc, _GK.draw1_GC);
		}

	depth_ret = 0;
	depth_visual = gdk_drawable_get_visual(top_window->window)->depth;
	if (   !XGetGeometry(GDK_DISPLAY(), root_xpixmap, &root_return,
				&x_ret, &y_ret, &w_ret, &h_ret, &bw_ret, &depth_ret)
		|| depth_ret != depth_visual
	   )
		{
		root_xpixmap = None;
		return;
		}

	/* I could use gdk_pixmap_foreign_new() and stay in the gdk domain,
	|  but it fails (in XGetGeometry()) if I change backgrounds.
	*/
	XSetTile(GDK_DISPLAY(), GDK_GC_XGC(trans_gc), root_xpixmap);
	XSetFillStyle(GDK_DISPLAY(), GDK_GC_XGC(trans_gc), FillTiled);
	for (list = gkrellm_get_chart_list(); list; list = list->next)
		{
		cp = (GkrellmChart *) list->data;
		if (!cp->transparency || !cp->shown)
			continue;
		gkrellm_winop_draw_rootpixmap_onto_transparent_chart(cp);
		gkrellm_refresh_chart(cp);
		}
	for (list = gkrellm_get_panel_list(); list; list = list->next)
		{
		p = (GkrellmPanel *) list->data;
		if (!p->transparency || !p->shown)
			continue;
		gkrellm_draw_panel_label(p);
		}
	XTranslateCoordinates(GDK_DISPLAY(),
			GDK_WINDOW_XWINDOW(top_window->window),
			GDK_ROOT_WINDOW(),
			0, 0, &x_root, &y_root, &child);
	for (list = gkrellm_monitor_list; list; list = list->next)
		{
		mon = (GkrellmMonitor *) list->data;
		draw_rootpixmap_onto_transparent_spacers(mon, x_root, y_root);
		}
	gdk_gc_set_clip_mask(_GK.text_GC, NULL);
	}
