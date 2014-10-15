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


GtkWidget		*config_window;


void
gkrellm_message_dialog(gchar *title, gchar *message)
	{
	GtkWidget	*top_win;
	GtkWidget	*dialog;
	GtkWidget	*scrolled;
	GtkWidget	*vbox, *vbox1;
	GtkWidget	*label;
	gchar		*s;
	gint		nlines;

	if (!message)
		return;
	top_win = gkrellm_get_top_window();
	dialog = gtk_dialog_new_with_buttons(title ? title : "GKrellM",
				GTK_WINDOW(top_win),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_STOCK_OK, GTK_RESPONSE_NONE,
				NULL);
	g_signal_connect_swapped(GTK_OBJECT(dialog), "response",
				G_CALLBACK(gtk_widget_destroy), GTK_OBJECT(dialog));
	gtk_window_set_wmclass(GTK_WINDOW(dialog),
				"Gkrellm_dialog", "Gkrellm");

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), vbox,
				FALSE, FALSE, 0);

	label = gtk_label_new(message);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);

	for (nlines = 0, s = message; *s; ++s)
		if (*s == '\n')
			++nlines;
	if (nlines > 20)
		{
		vbox1 = gkrellm_gtk_scrolled_vbox(vbox, &scrolled,
					GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
		gtk_widget_set_size_request(scrolled, -1, 300);
		gtk_box_pack_start(GTK_BOX(vbox1), label, FALSE, FALSE, 0);
		}
	else
		gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	gtk_widget_show_all(dialog);
	}

void
gkrellm_config_message_dialog(gchar *title, gchar *message)
	{
	GtkWidget	*dialog;
	GtkWidget	*vbox;
	GtkWidget	*label;

	if (!message || !config_window)
		return;
	dialog = gtk_dialog_new_with_buttons(title ? title : "GKrellM",
				GTK_WINDOW(config_window),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_STOCK_OK, GTK_RESPONSE_OK,
				NULL);
	gtk_window_set_wmclass(GTK_WINDOW(dialog),
				"Gkrellm_dialog", "Gkrellm");
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), vbox,
				FALSE, FALSE, 0);

	label = gtk_label_new(message);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);

	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	gtk_widget_show_all(vbox);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	}

#undef gkrellm_message_window
#undef gkrellm_config_message_window

void
gkrellm_message_window(gchar *title, gchar *message, GtkWidget *widget)
	{
	gkrellm_message_dialog(title, message);
	}

void
gkrellm_config_message_window(gchar *title, gchar *message, GtkWidget *widget)
	{
	gkrellm_config_message_dialog(title, message);
	}

static void
text_view_append(GtkWidget *view, gchar *s)
	{
	GtkTextIter		iter;
	GtkTextBuffer	*buffer;

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
	gtk_text_buffer_get_end_iter(buffer, &iter);
//	gtk_text_iter_forward_to_end(&iter);

	if (strncmp(s, "<b>", 3) == 0)
		gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
					s + 3, -1, "bold", NULL);
	else if (strncmp(s, "<i>", 3) == 0)
		gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
					s + 3, -1, "italic", NULL);
	else if (strncmp(s, "<h>", 3) == 0)
		gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
					s + 3, -1, "heading", NULL);
	else if (strncmp(s, "<c>", 3) == 0)
		gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
					s + 3, -1, "center", NULL);
	else if (strncmp(s, "<ul>", 4) == 0)
		gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
					s + 4, -1, "underline", NULL);
	else
		gtk_text_buffer_insert(buffer, &iter, s, -1);
	}

void
gkrellm_gtk_text_view_append(GtkWidget *view, gchar *string)
	{
	static gchar	*tag;
	gchar			*s;

	s = string;
	if (   *s == '<'
		&& (   (*(s + 2) == '>' && !*(s + 3))
			|| (*(s + 3) == '>' && !*(s + 4))
		   )
	   )
		{
		tag = g_strdup(s);
		return;
		}
	if (tag)
		{
		s = g_strconcat(tag, string, NULL);
		text_view_append(view, s);
		g_free(s);
		g_free(tag);
		tag = NULL;
		}
	else
		text_view_append(view, string);
	}

void
gkrellm_gtk_text_view_append_strings(GtkWidget *view, gchar **string,
			gint n_strings)
	{
	gchar	*tag = NULL;
	gchar	*s, *t;
	gint	i;

	for (i = 0; i < n_strings; ++i)
		{
		s = string[i];
		if (   *s == '<'
			&& (   (*(s + 2) == '>' && !*(s + 3))
				|| (*(s + 3) == '>' && !*(s + 4))
			   )
		   )
			{
			tag = g_strdup(s);
			continue;
			}
#if defined(ENABLE_NLS)
		s = gettext(string[i]);
#else
		s = string[i];
#endif
		if (tag)
			{
			t = g_strconcat(tag, s, NULL);
			text_view_append(view, t);
			g_free(t);
			g_free(tag);
			tag = NULL;
			}
		else
			text_view_append(view, s);
		}
	}

GtkWidget *
gkrellm_gtk_scrolled_text_view(GtkWidget *box, GtkWidget **scr,
		GtkPolicyType h_policy, GtkPolicyType v_policy)
	{
	GtkWidget	*scrolled,
				*view;
	GtkTextBuffer *buffer;

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
			h_policy, v_policy);
	gtk_box_pack_start(GTK_BOX(box), scrolled, TRUE, TRUE, 0);

	view = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
	gtk_container_add(GTK_CONTAINER(scrolled), view);
	gtk_text_buffer_create_tag(buffer, "heading",
			"weight", PANGO_WEIGHT_BOLD,
			"size", 14 * PANGO_SCALE, NULL);
	gtk_text_buffer_create_tag(buffer, "italic",
			"style", PANGO_STYLE_ITALIC, NULL);
	gtk_text_buffer_create_tag (buffer, "bold",
			"weight", PANGO_WEIGHT_BOLD, NULL);
	gtk_text_buffer_create_tag (buffer, "center",
			"justification", GTK_JUSTIFY_CENTER, NULL);
	gtk_text_buffer_create_tag (buffer, "underline",
			"underline", PANGO_UNDERLINE_SINGLE, NULL);
	if (scr)
		*scr = scrolled;
	return view;
	}

GtkTreeSelection *
gkrellm_gtk_scrolled_selection(GtkTreeView *treeview, GtkWidget *box,
			GtkSelectionMode s_mode,
			GtkPolicyType h_policy, GtkPolicyType v_policy,
			void (*func_cb)(), gpointer data)
	{
	GtkTreeSelection	*selection;
	GtkWidget			*scrolled;

	if (!box || !treeview)
		return NULL;

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_box_pack_start(GTK_BOX(box), scrolled, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(scrolled), GTK_WIDGET(treeview));
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
				h_policy, v_policy);
	selection = gtk_tree_view_get_selection(treeview);
	gtk_tree_selection_set_mode(selection, s_mode);
	if (func_cb)
		g_signal_connect(G_OBJECT(selection), "changed",
					G_CALLBACK(func_cb), data);
	return selection;
	}

void
gkrellm_launch_button_cb(GkrellmDecalbutton *button)
	{
	GkrellmLauncher    *launch;

	launch = (GkrellmLauncher *) button->data;
	g_spawn_command_line_async(launch->command, NULL /* GError */);
	}

void
gkrellm_remove_launcher(GkrellmLauncher *launch)
	{
	if (launch->button)
		gkrellm_destroy_button(launch->button);
	launch->button = NULL;
	}

void
gkrellm_configure_tooltip(GkrellmPanel *p, GkrellmLauncher *launch)
	{
	launch->widget = p->drawing_area;
	if (*launch->tooltip_comment && *launch->command)
        {
            gtk_widget_set_tooltip_text(p->drawing_area,
                                        launch->tooltip_comment);
        }
	else
            gtk_widget_set_tooltip_text(p->drawing_area,
                                        NULL);
	}

void
gkrellm_apply_launcher(GtkWidget **launch_entry, GtkWidget **tooltip_entry,
			GkrellmPanel *p, GkrellmLauncher *launch, void (*func)())
	{
	gchar	*command, *tip_comment;

	if (!launch_entry || !launch || !p)
		return;
	command = gkrellm_gtk_entry_get_text(launch_entry);
	tip_comment = tooltip_entry ?
						gkrellm_gtk_entry_get_text(tooltip_entry) : "";
	if (   launch->command && !strcmp(command, launch->command)
		&& launch->tooltip_comment
		&& !strcmp(tip_comment, launch->tooltip_comment)
	   )
		return;
	if (*command)
		{
		if (!launch->button)
			{
			if (launch->type == METER_LAUNCHER)
				launch->button = gkrellm_put_label_in_meter_button(p, func,
							launch, launch->pad);
			else if (launch->type == PANEL_LAUNCHER)
				launch->button = gkrellm_put_label_in_panel_button(p, func,
							launch, launch->pad);
			else if (launch->type == DECAL_LAUNCHER)
				launch->button = gkrellm_put_decal_in_meter_button(p,
							launch->decal,
							gkrellm_launch_button_cb, launch, &launch->margin);
			}
		gkrellm_dup_string(&launch->command, command);
		}
	else
		{
		if (launch->button)
			gkrellm_destroy_button(launch->button);
		launch->button = NULL;
		launch->pipe = NULL;		/* Close?? */
		gkrellm_dup_string(&launch->command, "");
		}
	gkrellm_dup_string(&launch->tooltip_comment, tip_comment);
	gkrellm_configure_tooltip(p, launch);
	}

GtkWidget *
gkrellm_gtk_launcher_table_new(GtkWidget *vbox, gint n_launchers)
	{
	GtkWidget	*table;

	table = gtk_table_new(2 * n_launchers, 2, FALSE /*non-homogeneous*/);
	gtk_table_set_col_spacings(GTK_TABLE(table), 2);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 2);
	return table;
	}

void
gkrellm_gtk_config_launcher(GtkWidget *table, gint n, GtkWidget **launch_entry,
			GtkWidget **tooltip_entry, gchar *desc, GkrellmLauncher *launch)
	{
	GtkWidget	*label, *hbox;
	gchar		buf[64];
	gint		row;

	if (!table || !launch_entry)
		return;
	row = (tooltip_entry ? 2 : 1) * n;
	hbox = gtk_hbox_new(FALSE, 0);
    /* Attach left right top bottom */
    gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, row, row + 1,
				GTK_FILL, GTK_SHRINK, 0, 0);

	snprintf(buf, sizeof(buf), _("%s command"), desc);
    label = gtk_label_new(buf);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	*launch_entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(*launch_entry), 255);
    gtk_table_attach_defaults(GTK_TABLE(table), *launch_entry,
                1, 2, row, row + 1);
	gtk_entry_set_text(GTK_ENTRY(*launch_entry),
					(launch && launch->command) ? launch->command : "");

	if (tooltip_entry)
		{
		hbox = gtk_hbox_new(FALSE, 0);
	    gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, row + 1, row + 2,
					GTK_FILL, GTK_SHRINK, 0, 0);

	    label = gtk_label_new(_("comment"));
		gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
		gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);
		*tooltip_entry = gtk_entry_new();
		gtk_entry_set_max_length(GTK_ENTRY(*tooltip_entry), 255);
	    gtk_table_attach_defaults(GTK_TABLE(table), *tooltip_entry,
	                1, 2, row + 1, row + 2);
		gtk_entry_set_text(GTK_ENTRY(*tooltip_entry),
					(launch && launch->tooltip_comment)
					? launch->tooltip_comment : "");
		}
	}

  /* FIXME: this guy is called on panels at create events
  |  when this situation has occurred:  the GKrellM rebuild has destroyed
  |  the decal button list.  But existing launchers have not had their
  |  button pointer set to NULL!  This could cause a problem with
  |  code that tries to check for button pointers in the create routines.
  */
void
gkrellm_setup_launcher(GkrellmPanel *p, GkrellmLauncher *launch,
			gint type, gint pad)
	{
	if (!launch)
		return;
	if (!launch->command)
		launch->command = g_strdup("");
	if (!launch->tooltip_comment)
		launch->tooltip_comment = g_strdup("");
	launch->type = type;
	launch->pad = pad;
	if (p)
		{
		gkrellm_configure_tooltip(p, launch);
		if (*(launch->command))
			launch->button = gkrellm_put_label_in_meter_button(p,
					gkrellm_launch_button_cb, launch, launch->pad);
		else
			launch->button = NULL;	/* In case dangling pointer, see above */
		}
	}

void
gkrellm_setup_decal_launcher(GkrellmPanel *p, GkrellmLauncher *launch,
			GkrellmDecal *d)
	{
	if (!launch)
		return;
	if (!launch->command)
		launch->command = g_strdup("");
	if (!launch->tooltip_comment)
		launch->tooltip_comment = g_strdup("");
	launch->type = DECAL_LAUNCHER;
	launch->pad = 0;
	launch->decal = d;
	if (p)
		{
		gkrellm_configure_tooltip(p, launch);
		if (*(launch->command))
			launch->button = gkrellm_put_decal_in_meter_button(p, d,
					gkrellm_launch_button_cb, launch, &launch->margin);
		else
			launch->button = NULL;	/* In case dangling pointer, see above */
		}
	}

void
gkrellm_gtk_check_button(GtkWidget *box, GtkWidget **button, gboolean active,
			gboolean expand, gint pad, gchar *string)
	{
	GtkWidget	*b;

	if (!string)
		return;
	b = gtk_check_button_new_with_label(string);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b), active);
	if (box)
		gtk_box_pack_start(GTK_BOX(box), b, expand, FALSE, pad);
	if (button)
		*button = b;
	}

void
gkrellm_gtk_check_button_connected(GtkWidget *box, GtkWidget **button,
			gboolean active, gboolean expand, gboolean fill, gint pad,
			void (*cb_func)(), gpointer data, gchar *string)
	{
	GtkWidget	*b;

	if (!string)
		return;
	b = gtk_check_button_new_with_label(string);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b), active);
	if (box)
		{
		if (pad < 0)
			gtk_box_pack_end(GTK_BOX(box), b, expand, fill, -(pad + 1));
		else
			gtk_box_pack_start(GTK_BOX(box), b, expand, fill, pad);
		}
	if (cb_func)
		g_signal_connect(G_OBJECT(b), "toggled",
				G_CALLBACK(cb_func), data);
	if (button)
		*button = b;
	}

void
gkrellm_gtk_button_connected(GtkWidget *box, GtkWidget **button,
			gboolean expand, gboolean fill, gint pad,
			void (*cb_func)(), gpointer data, gchar *string)
	{
	GtkWidget	*b;

	if (!string)
		return;
	if (!strncmp(string, "gtk-", 4))
		b = gtk_button_new_from_stock(string);
	else
		b = gtk_button_new_with_label(string);
	if (box)
		{
		if (pad < 0)
			gtk_box_pack_end(GTK_BOX(box), b, expand, fill, -(pad + 1));
		else
			gtk_box_pack_start(GTK_BOX(box), b, expand, fill, pad);
		}
	if (cb_func)
		g_signal_connect(G_OBJECT(b), "clicked",
				G_CALLBACK(cb_func), data);
	if (button)
		*button = b;
	}

void
gkrellm_gtk_alert_button(GtkWidget *box, GtkWidget **button,
			gboolean expand, gboolean fill, gint pad, gboolean pack_start,
			void (*cb_func)(), gpointer data)
	{
	GtkWidget	*b, *hbox;
	GtkWidget	*image, *label;

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
	image = gtk_image_new_from_pixbuf(gkrellm_alert_pixbuf());
	label = gtk_label_new(_("Alerts"));
	gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);
	b = gtk_button_new();
	if (button)
		*button = b;
	if (cb_func)
		g_signal_connect(G_OBJECT(b), "clicked", G_CALLBACK(cb_func), data);
	gtk_widget_show_all(hbox);
	gtk_container_add(GTK_CONTAINER(b), hbox);
	if (box)
		{
		if (pack_start)
			gtk_box_pack_start(GTK_BOX(box), b, expand, fill, pad);
		else
			gtk_box_pack_end(GTK_BOX(box), b, expand, fill, pad);
		}
	}

void
gkrellm_gtk_spin_button(GtkWidget *box, GtkWidget **spin_button, gfloat value,
		gfloat low, gfloat high, gfloat step0, gfloat step1,
		gint digits, gint width,
		void (*cb_func)(), gpointer data, gboolean right_align, gchar *string)
	{
	GtkWidget		*hbox	= NULL,
					*label,
					*button;
	GtkSpinButton	*spin;
	GtkAdjustment	*adj;

	if (string && box)
		{
	    hbox = gtk_hbox_new (FALSE, 0);
    	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 2);
		box = hbox;
		}
    adj = (GtkAdjustment *) gtk_adjustment_new (value,
								low, high, step0, step1, 0.0);
    button = gtk_spin_button_new(adj, 0.5, digits);
	if (spin_button)
		*spin_button = button;
	if (width > 0)
		gtk_widget_set_size_request(button, width, -1);
    spin = GTK_SPIN_BUTTON(button);
    gtk_spin_button_set_numeric(spin, TRUE);
	if (!data)
		data = (gpointer) spin;
	if (cb_func)
		g_signal_connect(G_OBJECT(adj), "value_changed",
				G_CALLBACK(cb_func), data);
	if (box)
		{
		if (right_align && string)
			{
			label = gtk_label_new(string);
			gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
			gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 2);
			}
		gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 2);
		if (!right_align && string)
			{
			label = gtk_label_new(string);
			gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
			gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 2);
			}
		}
	}

GtkWidget *
gkrellm_gtk_scrolled_vbox(GtkWidget *box, GtkWidget **scr,
		GtkPolicyType h_policy, GtkPolicyType v_policy)
	{
	GtkWidget	*scrolled,
				*vbox;

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
			h_policy, v_policy);
	gtk_box_pack_start(GTK_BOX(box), scrolled, TRUE, TRUE, 0);
	vbox = gtk_vbox_new(FALSE, 2);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled), vbox);
	if (scr)
		*scr = scrolled;
	return vbox;
	}

  /* frame_border_width - border around outside of frame.
  |  vbox_pad - pad between widgets to be packed in returned vbox.
  |  vbox_border_width - border between returned vbox and frame.
  */
GtkWidget *
gkrellm_gtk_framed_vbox(GtkWidget *box, gchar *label, gint frame_border_width,
		gboolean frame_expand, gint vbox_pad, gint vbox_border_width)
	{
	GtkWidget	*frame, *lbl;
	GtkWidget	*vbox;

	frame = gtk_frame_new(NULL);
/*	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE); */
	lbl = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(lbl), label);
	gtk_frame_set_label_widget(GTK_FRAME(frame), lbl);

	gtk_container_set_border_width(GTK_CONTAINER(frame), frame_border_width);
    gtk_box_pack_start(GTK_BOX(box), frame, frame_expand, frame_expand, 0);
    vbox = gtk_vbox_new(FALSE, vbox_pad);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), vbox_border_width);
    gtk_container_add(GTK_CONTAINER(frame), vbox);
	return vbox;
	}

GtkWidget *
gkrellm_gtk_framed_vbox_end(GtkWidget *box, gchar *label,
		gint frame_border_width, gboolean frame_expand,
		gint vbox_pad, gint vbox_border_width)
	{
	GtkWidget	*frame;
	GtkWidget	*vbox;

	frame = gtk_frame_new(label);
	gtk_container_set_border_width(GTK_CONTAINER(frame), frame_border_width);
    gtk_box_pack_end(GTK_BOX(box), frame, frame_expand, frame_expand, 0);
    vbox = gtk_vbox_new(FALSE, vbox_pad);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), vbox_border_width);
    gtk_container_add(GTK_CONTAINER(frame), vbox);
	return vbox;
	}

GtkWidget *
gkrellm_gtk_category_vbox(GtkWidget *box, gchar *category_header,
		gint header_pad, gint box_pad, gboolean pack_start)
	{
	GtkWidget	*vbox, *vbox1, *hbox, *label;
	gchar		*s;

	vbox = gtk_vbox_new(FALSE, 0);
	if (pack_start)
		gtk_box_pack_start(GTK_BOX(box), vbox, FALSE, FALSE, 0);
	else
		gtk_box_pack_end(GTK_BOX(box), vbox, FALSE, FALSE, 0);

	if (category_header)
		{
		label = gtk_label_new(NULL);
		s = g_strconcat("<span weight=\"bold\">", category_header,
					"</span>",NULL);
		gtk_label_set_markup(GTK_LABEL(label), s);
		gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
		gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, header_pad);
		g_free(s);
		}

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new("    ");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	vbox1 = gtk_vbox_new(FALSE, box_pad);
	gtk_box_pack_start(GTK_BOX(hbox), vbox1, TRUE, TRUE, 0);

	/* Add some bottom pad */
	label = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	return vbox1;
	}

GtkWidget *
gkrellm_gtk_notebook_page(GtkWidget *tabs, char *name)
	{
	GtkWidget	*label;
	GtkWidget	*vbox;

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 0);

	label = gtk_label_new(name);
	gtk_notebook_append_page(GTK_NOTEBOOK(tabs), vbox, label);

	return vbox;
	}

GtkWidget *
gkrellm_gtk_framed_notebook_page(GtkWidget *tabs, char *name)
	{
	GtkWidget	*vbox;

	vbox = gkrellm_gtk_notebook_page(tabs, name);
	vbox = gkrellm_gtk_framed_vbox(vbox, NULL, 2, TRUE, 4, 4);
	return vbox;
	}

static void
create_about_tab(GtkWidget *vbox)
	{
	GtkWidget	*label;
	gchar		*buf;

	vbox = gkrellm_gtk_framed_vbox(vbox, NULL, 2, TRUE, 0, 0);
	label = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);

	buf = g_strdup_printf(_("GKrellM %d.%d.%d%s\nGNU Krell Monitors\n\n"
				"Copyright (c) 1999-2014 by Bill Wilson\n"
				"billw@gkrellm.net\n"
				"http://gkrellm.net\n\n"
				"Released under the GNU General Public License"),
				GKRELLM_VERSION_MAJOR, GKRELLM_VERSION_MINOR,
				GKRELLM_VERSION_REV, GKRELLM_EXTRAVERSION);
	label = gtk_label_new(buf);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
	g_free(buf);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	label = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);

#if defined(__APPLE__)
	buf = g_strdup_printf(_("Mac OS X code was contributed by:\n"
						"Ben Hines <bhines@alumni.ucsd.edu>\n"
						"and\n"
						"Hajimu UMEMOTO <ume@mahoroba.org>"));
	label = gtk_label_new(buf);
	g_free(buf);
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
#endif
#if defined(__FreeBSD__)
	buf = g_strdup_printf(
				_("FreeBSD system dependent code was contributed by:\n"
				"Hajimu UMEMOTO <ume@mahoroba.org>"));
	label = gtk_label_new(buf);
	g_free(buf);
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
#endif
#if defined(__DragonFly__)
	buf = g_strdup_printf(
				_("DragonFly system dependent code was contributed by:\n"
				"Joerg Sonnenberger <joerg@bec.de>"));
	label = gtk_label_new(buf);
	g_free(buf);
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
#endif
#if defined(__NetBSD__)
	buf = g_strdup_printf(
				_("NetBSD system dependent code was contributed by:\n"
				"Anthony Mallet <anthony.mallet@useless-ficus.net>"));
	label = gtk_label_new(buf);
	g_free(buf);
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
#endif
#if defined(__solaris__)
	buf = g_strdup_printf(
				_("Solaris system dependent code was contributed by:\n"
				"Daisuke Yabuki <dxy@acm.org>"));
	label = gtk_label_new(buf);
	g_free(buf);
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
#endif
#if defined(WIN32)
	buf = g_strdup_printf(
				_("Windows system dependent code was contributed by:\n"
				"Bill Nalen <bill@nalens.com>\n"
				"Stefan Gehn <stefan+gkrellm@srcbox.net>\n"));
	label = gtk_label_new(buf);
	g_free(buf);
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
#endif // !WIN32
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
	}


/* ------------------General Settings---------------------------------*/
static GtkWidget
			*enable_hst_button,
			*hostname_short_button,
			*enable_sysname_button,
			*save_position_button;

#if !defined(WIN32)
static GtkWidget
			*sticky_state_button,
			*dock_type_button,
			*decorated_button,
			*skip_taskbar_button,
			*skip_pager_button,
			*above_button,
			*below_button;
#endif // !WIN32

GtkWidget	*track_gtk_button,
			*allow_multiple_button;
GtkWidget	*on_top_button;



static void
cb_width_spin(GtkWidget *widget, GtkSpinButton *spin)
	{
	_GK.chart_width = gtk_spin_button_get_value_as_int(spin);
	gkrellm_build();
	}

static void
cb_HZ_spin(GtkWidget *widget, GtkSpinButton *spin)
	{
	gint	n;

	n = _GK.update_HZ;
	_GK.update_HZ = gtk_spin_button_get_value_as_int(spin);
	if (n != _GK.update_HZ)
		gkrellm_start_timer(_GK.update_HZ);
	}

static void
cb_hostname_sysname(GtkWidget *widget, gpointer data)
	{
	_GK.enable_hostname = GTK_TOGGLE_BUTTON(enable_hst_button)->active;
	if (hostname_short_button)
		_GK.hostname_short = GTK_TOGGLE_BUTTON(hostname_short_button)->active;
	_GK.enable_system_name = GTK_TOGGLE_BUTTON(enable_sysname_button)->active;
	gkrellm_apply_hostname_config();
	}

static void
cb_general(void)
	{
#if !defined(WIN32)
	gint		n;
	gboolean	new_state;
#endif

	if (allow_multiple_button)
		_GK.allow_multiple_instances =
					GTK_TOGGLE_BUTTON(allow_multiple_button)->active;
	if (on_top_button)
		_GK.on_top = GTK_TOGGLE_BUTTON(on_top_button)->active;

	_GK.save_position = GTK_TOGGLE_BUTTON(save_position_button)->active;
#if !defined(WIN32)
	if (sticky_state_button)
		{
		n = GTK_TOGGLE_BUTTON(sticky_state_button)->active;
		new_state = (n != _GK.sticky_state);
		_GK.sticky_state = n;
		if (new_state)
			{
			GtkWidget	*top_window = gkrellm_get_top_window();

			if (_GK.sticky_state)
				gtk_window_stick(GTK_WINDOW(top_window));
			else
				gtk_window_unstick(GTK_WINDOW(top_window));
			}
		}

	if (decorated_button)	/* restart for change to take effect */
		_GK.decorated = GTK_TOGGLE_BUTTON(decorated_button)->active;

	if (skip_taskbar_button)
		{
		n = GTK_TOGGLE_BUTTON(skip_taskbar_button)->active;
		new_state = (n != _GK.state_skip_taskbar);
		_GK.state_skip_taskbar = n;
		if (new_state)
			gkrellm_winop_state_skip_taskbar(n);
		}
	if (skip_pager_button)
		{
		n = GTK_TOGGLE_BUTTON(skip_pager_button)->active;
		new_state = (n != _GK.state_skip_pager);
		_GK.state_skip_pager = n;
		if (new_state)
			gkrellm_winop_state_skip_pager(n);
		}
	if (above_button)
		{
		n = GTK_TOGGLE_BUTTON(above_button)->active;
		new_state = (n != _GK.state_above);
		_GK.state_above = n;
		if (new_state)
			{
			if (n && _GK.state_below)
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(below_button),
						FALSE);
			gkrellm_winop_state_above(n);
			}
		}
	if (below_button)
		{
		n = GTK_TOGGLE_BUTTON(below_button)->active;
		new_state = (n != _GK.state_below);
		_GK.state_below = n;
		if (new_state)
			{
			if (n && _GK.state_above)
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(above_button),
						FALSE);
			gkrellm_winop_state_below(n);
			}
		}
#endif // !WIN32
	}

#ifndef WIN32
static void
cb_dock_type(GtkWidget *widget, gpointer data)
	{
	gboolean	sensitive;

	_GK.dock_type = GTK_TOGGLE_BUTTON(dock_type_button)->active;
	sensitive = !_GK.dock_type;
	if (!sensitive)
		{
		gtk_toggle_button_set_active(
					GTK_TOGGLE_BUTTON(decorated_button), FALSE);
		gtk_toggle_button_set_active(
					GTK_TOGGLE_BUTTON(skip_taskbar_button), FALSE);
		gtk_toggle_button_set_active(
					GTK_TOGGLE_BUTTON(skip_pager_button), FALSE);
		}
	gtk_widget_set_sensitive(decorated_button, sensitive);
	gtk_widget_set_sensitive(skip_taskbar_button, sensitive);
	gtk_widget_set_sensitive(skip_pager_button, sensitive);
	}
#endif // WIN32

static gchar	*general_info_text[]	=
{
N_("<h>Krells\n"),
N_("Krells are the horizontally moving indicators below each chart and\n"
"on meter style monitors.  Depending on the monitor, they show fast\n"
"response data rates, a percentage of some capacity, or something else.\n"),
"\n",
N_("<h>Charts\n"),
N_("The default for most charts is to automatically adjust the number of\n"
	"grid lines drawn and the resolution per grid so drawn data will be\n"
	"nicely visible.  You may change this to fixed grids of 1-5 and/or\n"
	"fixed grid resolutions in the chart config windows.  However,\n"
	"some combination of the auto scaling modes may give best results.\n"),
"\n",
N_("See the README or do a \"man gkrellm\" for more information.\n"),
"\n",
N_("<h>Chart Labels\n"),
N_("Chart label format strings place text on charts using position codes:\n"),
N_("\t\\t    top left\n"),
N_("\t\\b    bottom left\n"),
N_("\t\\n    next line\n"),
N_("\t\\N    next line only if last string had visible characters\n"),
N_("\t\\p    previous line\n"),
N_("\t\\c    center the text\n"),
N_("\t\\C    begin drawing text at the center\n"),
N_("\t\\r    right justify\n"),
N_("\t\\f    use alternate font for the next string\n"),
N_("\t\\w    use the following string to define a field width\n"),
N_("\t\\a    draw left justified in the defined field width\n"),
N_("\t\\e    draw right justified in the defined field width\n"),
N_("\t\\.     no-op.  Used to break a string into two strings.\n"),
N_("\t\\D0   bottom of charts first data view (D2 for second data view ...)\n"),
N_("\t\\D1   top of charts first data view (D3 for second data view ...)\n"),

N_("<h>\nCommands\n"),
N_("\tMany monitors can be configured to launch commands.  Just enter the\n"
   "\tcommand where you see a \"command\" entry and also a comment if you\n"
   "\twant a tooltip to appear for the command.  After a command is entered,\n"
   "\tfor a monitor, a button for launching it will become visible when you\n"
   "\tmove the mouse into the panel area of the monitor.\n\n"),
N_("See the README or do a \"man gkrellm\" for more information.\n"),
"\n",

N_("<h>\nMouse Button Actions:\n"),
N_("<b>\tLeft "),
N_("clicking on charts will toggle a display of some extra info.\n"),
N_("<b>\tRight "),
N_("clicking on charts brings up a chart configuration window.\n"),
N_("<b>\tRight "),
N_("clicking on many panels opens its monitor configuration window.\n")
};

static void
create_general_tab(GtkWidget *tab_vbox)
	{
	GtkWidget		*tabs;
	GtkWidget		*vbox, *vbox1;
#if !defined(WIN32)
	GtkWidget		*vbox2;
#endif
	GtkWidget		*hbox;
	GtkWidget		*label, *text;
	gint			i;

	tabs = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(tabs), GTK_POS_TOP);
	gtk_box_pack_start(GTK_BOX(tab_vbox), tabs, TRUE, TRUE, 0);

/* --Options tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Options"));
	vbox1 = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), vbox1, FALSE, FALSE, 0);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox, FALSE, FALSE, 0);

	gkrellm_gtk_check_button_connected(hbox, &enable_hst_button,
			_GK.enable_hostname, FALSE, FALSE, 0,
			cb_hostname_sysname, NULL,
			_("Hostname display"));

	if (gkrellm_hostname_can_shorten())
		gkrellm_gtk_check_button_connected(hbox, &hostname_short_button,
				_GK.hostname_short, FALSE, FALSE, 10,
				cb_hostname_sysname, NULL,
				_("Short hostname"));

	gkrellm_gtk_check_button_connected(vbox, &enable_sysname_button,
			_GK.enable_system_name, FALSE, FALSE, 0,
			cb_hostname_sysname, NULL,
			_("System name display"));

	gkrellm_gtk_check_button_connected(vbox, &save_position_button,
			_GK.save_position, FALSE, FALSE, 6,
			cb_general, NULL,
		_("Remember screen location at exit and move to it at next startup"));

#if !defined(WIN32)
	gkrellm_gtk_check_button_connected(vbox, &allow_multiple_button,
			_GK.allow_multiple_instances, FALSE, FALSE, 0,
			cb_general, NULL,
			_("Allow multiple instances"));
#endif // !WIN32

#if defined(WIN32)
	gkrellm_gtk_check_button_connected(vbox, &on_top_button,
			_GK.on_top, FALSE, FALSE, 0,
			cb_general, NULL,
_("Make gkrellm a topmost window (restart gkrellm for this to take effect)."));
#endif // WIN32

	if (_GK.client_mode)
		{
		hbox = gtk_hbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 6);
		gkrellm_gtk_alert_button(hbox, NULL, FALSE, FALSE, 4, TRUE,
					gkrellm_gkrellmd_disconnect_cb, NULL);
		label = gtk_label_new(_("gkrellmd server disconnect"));
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 4);
		}

	vbox = gkrellm_gtk_framed_vbox_end(vbox, NULL, 4, FALSE, 0, 2);
	gkrellm_gtk_spin_button(vbox, NULL, (gfloat) _GK.update_HZ,
			1.0, 20.0, 1.0, 1.0, 0, 55,
			cb_HZ_spin, NULL, FALSE,
			_("Krell and LED updates per second."));

	gkrellm_gtk_spin_button(vbox, NULL, (gfloat) _GK.chart_width,
			(gfloat) CHART_WIDTH_MIN, (gfloat) CHART_WIDTH_MAX,
			5.0, 10.0, 0, 55,
			cb_width_spin, NULL, FALSE,
			_("GKrellM width"));

#if !defined(WIN32)
/* --Window options tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Properties"));
	gkrellm_gtk_check_button_connected(vbox, &sticky_state_button,
			_GK.sticky_state, FALSE, FALSE, 0, cb_general, NULL,
			_("Set sticky state"));
	gkrellm_gtk_check_button_connected(vbox, &above_button,
			_GK.state_above, FALSE, FALSE, 0, cb_general, NULL,
			_("Set on top of other windows of the same type"));
	gkrellm_gtk_check_button_connected(vbox, &below_button,
			_GK.state_below, FALSE, FALSE, 0, cb_general, NULL,
			_("Set below other windows of the same type"));

	vbox1 = gkrellm_gtk_framed_vbox(vbox, NULL, 4, FALSE, 2, 0);
	vbox2 = gkrellm_gtk_framed_vbox(vbox1, NULL, 0, FALSE, 0, 0);
	gkrellm_gtk_check_button_connected(vbox2, &dock_type_button,
			_GK.dock_type, FALSE, FALSE, 0,
			cb_dock_type, NULL,			
			_("Set window type to be a dock or panel"));

	vbox2 = gkrellm_gtk_framed_vbox(vbox1, NULL, 0, FALSE, 0, 0);
	gkrellm_gtk_check_button_connected(vbox2, &decorated_button,
			_GK.decorated, FALSE, FALSE, 0, cb_general, NULL,
			_("Use window manager decorations"));

	gkrellm_gtk_check_button_connected(vbox2, &skip_taskbar_button,
			_GK.state_skip_taskbar, FALSE, FALSE, 0, cb_general, NULL,
			_("Do not include on a taskbar"));

	gkrellm_gtk_check_button_connected(vbox2, &skip_pager_button,
			_GK.state_skip_pager, FALSE, FALSE, 0, cb_general, NULL,
			_("Do not include on a pager"));

	if (_GK.dock_type)
		{
		gtk_widget_set_sensitive(decorated_button, FALSE);
		gtk_widget_set_sensitive(skip_taskbar_button, FALSE);
		gtk_widget_set_sensitive(skip_pager_button, FALSE);
		}

	text = gtk_label_new(
_("Some of these properties require a standards compliant window manager.\n"
  "You may have to restart gkrellm for them to take effect.\n"));
	gtk_box_pack_end(GTK_BOX(vbox), text, FALSE, TRUE, 4);
#endif // !WIN32

/* --Info tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Info"));
	text = gkrellm_gtk_scrolled_text_view(vbox, NULL,
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	for (i = 0; i < sizeof(general_info_text)/sizeof(gchar *); ++i)
		gkrellm_gtk_text_view_append(text, _(general_info_text[i]));
	}


/* ------------------Themes Tab----------------------------------*/

enum
	{
	THEME_COLUMN,
	PATH_COLUMN,
	N_THEME_COLUMNS
	};

typedef struct
	{
	gchar	*path;
	gchar	*name;
	}
	Theme;

static GtkTreeView      *theme_treeview;
static GtkTreeSelection *theme_selection;

static GList		*themes_list;
static GList		*theme_position_in_list;

static GtkWidget	*theme_alt_spin_button;
static GtkWidget	*theme_alt_label;
static GtkWidget	*theme_entry,
					*author_label;

static gboolean		theme_modified;


typedef struct
	{
	GtkFontSelectionDialog	*fontseldlg;
	GtkWidget				*entry;
	GtkWidget				*browse_button;
	gchar					*name;
	gchar					*string;
//	PangoFontDescription	*font_desc;
	}
	AltFontSelect;

static AltFontSelect
			large_font,
			normal_font,
			small_font;

gchar *
gkrellm_get_large_font_string(void)
	{
	return large_font.string;
	}

gchar *
gkrellm_get_normal_font_string(void)
	{
	return normal_font.string;
	}

gchar *
gkrellm_get_small_font_string(void)
	{
	return small_font.string;
	}

static gboolean
get_font_entries(void)
	{
	gchar		*s;
	gboolean	modified = FALSE;

	s = (gchar *) gtk_entry_get_text(GTK_ENTRY(large_font.entry));
	modified |= gkrellm_dup_string(&large_font.string, s);
	s = (gchar *) gtk_entry_get_text(GTK_ENTRY(normal_font.entry));
	modified |= gkrellm_dup_string(&normal_font.string, s);
	s = (gchar *) gtk_entry_get_text(GTK_ENTRY(small_font.entry));
	modified |= gkrellm_dup_string(&small_font.string, s);
	return modified;
	}

static void
cb_font_dialog_ok(GtkWidget *w, AltFontSelect *afs)
	{
	gchar	*fontname;

	fontname = gtk_font_selection_dialog_get_font_name(afs->fontseldlg);
	if (fontname)
		gtk_entry_set_text(GTK_ENTRY(afs->entry), fontname);
	gtk_widget_destroy(GTK_WIDGET(afs->fontseldlg));
	theme_modified = TRUE;
	get_font_entries();
	gkrellm_build();
	}

static void
cb_font_dialog(GtkWidget *widget, AltFontSelect *afs)
	{
	GtkWidget				*w;
	GtkFontSelectionDialog	*fsd;

	if (afs->fontseldlg)
		return;
	w = gtk_font_selection_dialog_new(_(afs->name));
	gtk_window_set_wmclass(GTK_WINDOW(w),
				"Gkrellm_dialog", "Gkrellm");
	fsd = GTK_FONT_SELECTION_DIALOG(w);
	afs->fontseldlg = fsd;
	gtk_font_selection_dialog_set_font_name(fsd, afs->string);
	g_signal_connect(G_OBJECT(fsd->ok_button), "clicked",
			G_CALLBACK(cb_font_dialog_ok), afs);
	g_signal_connect_swapped(G_OBJECT(fsd->cancel_button), "clicked",
			G_CALLBACK(gtk_widget_destroy), fsd);
	g_signal_connect(G_OBJECT(fsd), "destroy",
			G_CALLBACK(gtk_widget_destroyed), &afs->fontseldlg);
	gtk_widget_show(GTK_WIDGET(fsd));
	}

static gchar *
get_theme_author(gchar *path)
	{
	static gchar	buf[128];
	FILE			*f;
	gchar			*s, *q, *rcfile, line[128];

	buf[0] = '\0';
	if (!path || *path == '\0')
		return buf;
	rcfile = g_strdup_printf("%s/%s", path, GKRELLMRC);
	f = g_fopen(rcfile, "r");
	g_free(rcfile);
	if (!f)
		return buf;
	while (fgets(line, sizeof(line), f))
		{
		if (   (s = strtok(line, " :=\t\n")) == NULL
			|| strcmp(s, "author") != 0
		   )
			continue;
		s = strtok(NULL, "\n");		/* Rest of line is Author string */
		if (s)
			{
			while (   *s == ' ' || *s == '\t' || *s == '"' || *s == '='
					|| *s == ':')
				++s;
			q = strchr(s, (int) '"');
			if (q)
				*q = '\0';
			strcpy(buf, s);
			break;
			}
		}
	fclose(f);
	return buf;
	}

static Theme *
find_theme_in_list(gchar *name)
	{
	GList	*list;
	Theme	*theme;

	if (!name || !*name)
		return NULL;

	for (list = themes_list ; list; list = list->next)
		{
		theme = (Theme *) list->data;
		if (!strcmp(theme->name, name))
			return theme;
		}
	return NULL;
	}

static void
add_themes_to_list(gchar *theme_dir, gboolean in_gkrellm2)
	{
	GDir	*dir;
	Theme	*theme;
	gchar	*name;
	gchar	*path;

	if ((dir = g_dir_open(theme_dir, 0, NULL)) == NULL)
		return;
	while ((name = (gchar *) g_dir_read_name(dir)) != NULL)
		{
		if (find_theme_in_list(name))
			continue;
		if (in_gkrellm2)
			path = g_build_filename(theme_dir, name, "gkrellm2", NULL);
		else
			path = g_build_filename(theme_dir, name, NULL);

		if (g_file_test(path, G_FILE_TEST_IS_DIR))
			{
			theme = g_new0(Theme, 1);
			theme->path = path;
			theme->name = g_strdup(name);
			themes_list = g_list_append(themes_list, theme);
			}
		else
			g_free(path);
		}
	g_dir_close(dir);
	}

static void
find_theme_position_in_list(void)
	{
	GList	*list;
	Theme	*theme;
	gchar	*name;

	name = *(_GK.theme_path) ? _GK.theme_path : "Default";
	for (list = themes_list; list; list = list->next)
		{
		theme = (Theme *) list->data;
		if (!strcmp(name, theme->path))
			break;
		}
	theme_position_in_list = list ? list : themes_list;
	}

gint
theme_compare(Theme *th1, Theme *th2)
	{
	return strcmp(th1->name, th2->name);
	}

void
gkrellm_make_themes_list(void)
	{
	GList	*list;
	Theme	*theme;
	gchar	*theme_dir;

	for (list = themes_list; list; list = list->next)
		{
		theme = (Theme *) list->data;
		g_free(theme->path);
		g_free(theme->name);
		}
	gkrellm_free_glist_and_data(&themes_list);

	theme = g_new0(Theme, 1);
	theme->path = g_strdup("Default");
	theme->name = g_strdup(theme->path);
	themes_list = g_list_append(themes_list, theme);

	theme_dir = g_build_filename(gkrellm_homedir(), GKRELLM_THEMES_DIR, NULL);
	add_themes_to_list(theme_dir, FALSE);
	g_free(theme_dir);

	theme_dir = g_build_filename(gkrellm_homedir(), ".themes", NULL);
	add_themes_to_list(theme_dir, TRUE);
	g_free(theme_dir);

	theme_dir = gtk_rc_get_theme_dir();
	add_themes_to_list(theme_dir, TRUE);

#if defined(WIN32)
	gchar *install_path;
	install_path = g_win32_get_package_installation_directory_of_module(NULL);
	if (install_path != NULL)
		{
		theme_dir = g_build_filename(install_path, "share", "gkrellm2", "themes", NULL);
		add_themes_to_list(theme_dir, FALSE);
		g_free(theme_dir);
		g_free(install_path);
		}
#endif

#if defined(LOCAL_THEMES_DIR)
	add_themes_to_list(LOCAL_THEMES_DIR, FALSE);
#endif
#if defined(SYSTEM_THEMES_DIR)
	add_themes_to_list(SYSTEM_THEMES_DIR, FALSE);
#endif

	themes_list = g_list_sort(themes_list, (GCompareFunc) theme_compare);

	if (_GK.command_line_theme)
		{
		theme = g_new0(Theme, 1);
		theme->path = g_strdup(_GK.command_line_theme);
		theme->name = g_strdup(theme->path);
		themes_list = g_list_append(themes_list, theme);
		}
	find_theme_position_in_list();
	}

static GtkTreeModel *
theme_create_model(void)
	{
	GtkListStore	*store;
	GtkTreeIter		iter;
	GList			*list;
	Theme			*theme;

	gkrellm_make_themes_list();
	store = gtk_list_store_new(N_THEME_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);
	for (list = themes_list; list; list = list->next)
		{
		theme = (Theme *) list->data;
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
				THEME_COLUMN, theme->name,
				PATH_COLUMN, theme->path,
				-1);
		}
	return GTK_TREE_MODEL(store);
	}

static void
cb_theme_tree_selection_changed(GtkTreeSelection *selection, gpointer data)
	{
	GtkTreeIter		iter;
	GtkTreeModel	*model;
	gchar			*path;

	if (!gtk_tree_selection_get_selected(selection, &model, &iter))
		return;
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(theme_alt_spin_button), 0.0);
	gtk_tree_model_get(model, &iter, PATH_COLUMN, &path, -1);
	gtk_entry_set_text(GTK_ENTRY(theme_entry), path);
	gtk_label_set_text(GTK_LABEL(author_label), get_theme_author(path));
	if (gkrellm_dup_string(&_GK.theme_path, path))
		{
		find_theme_position_in_list();
		theme_modified = TRUE;
		gkrellm_build();
		}
	}

static void
cb_theme_alternative_changed(GtkWidget *widget, GtkSpinButton *spin)
	{
	gint	i;

	i = gtk_spin_button_get_value_as_int(spin);
	if (i > _GK.theme_n_alternatives)
		{
		i = _GK.theme_n_alternatives;
		gtk_spin_button_set_value(spin, (gfloat) i);
		}
	if (i != _GK.theme_alternative)
		{
		_GK.theme_alternative = i;
		theme_modified = TRUE;
		gkrellm_build();
		}
	}

static void
cb_theme_scale_changed(GtkWidget *widget, GtkSpinButton *spin)
	{
	gint	i;

	i = gtk_spin_button_get_value_as_int(spin);
	if (i != _GK.theme_scale)
		{
		_GK.theme_scale = i;
		theme_modified = TRUE;
		gkrellm_build();
		}
	}

void
gkrellm_set_theme_alternatives_label(void)
	{
	GtkSpinButton	*spin;
	gchar			buf[64];

	if (!config_window)
		return;
	spin = GTK_SPIN_BUTTON(theme_alt_spin_button);
	gtk_spin_button_set_value(spin, (gfloat) _GK.theme_alternative);
	snprintf(buf, sizeof(buf), _("%d total theme alternatives"),
					_GK.theme_n_alternatives);
	gtk_label_set_text(GTK_LABEL(theme_alt_label), buf);
	}

void
gkrellm_save_theme_config(void)
	{
	FILE	*f;
	gchar	*path;

	/* Assume gkrellm -t is for testing and don't save theme config changes.
	|  Similary for _GK.demo.
	*/
	if (!theme_modified || _GK.command_line_theme || _GK.demo || _GK.no_config)
		return;

	path = gkrellm_make_config_file_name(gkrellm_homedir(),
					GKRELLM_THEME_CONFIG);

	if ((f = g_fopen(path, "w")) != NULL)
		{
		fprintf(f, "%s\n", _GK.theme_path);
		fprintf(f, "%d\n", _GK.theme_alternative);
		fprintf(f, "%s\n", large_font.string);
		fprintf(f, "%s\n", normal_font.string);
		fprintf(f, "%s\n", small_font.string);
		fprintf(f, "%d\n", _GK.theme_scale);
		fclose(f);
		}
	g_free(path);
	theme_modified = FALSE;
	}

void
gkrellm_load_theme_config(void)
	{
	FILE	*f;
	gchar	*path, *s;
	gchar	buf[1024];
	gint	i;

	/* Need to load the theme from ~/.gkrellm/theme_config only at startup
	|  or if re-reading because of theme_event - these are only times 
	|  _GK.theme_path will be NULL.  Note: _GK.theme_path will not be NULL
	|  at startup if there is a command line theme, so no theme scaling if
	|  using command line theme.
	*/
	if (!_GK.theme_path)
		{
		path = gkrellm_make_config_file_name(gkrellm_homedir(),
					GKRELLM_THEME_CONFIG);
		f = g_fopen(path, "r");
		g_free(path);
		if (f && fgets(buf, sizeof(buf), f))
			{
			if ((s = strchr(buf, (gint) '\n')) != NULL)
				*s = '\0';
			gkrellm_debug(DEBUG_GUI, "gkrellm_load_theme_config: %s\n", buf);
			s = buf;
			if (s && *s != '#' && *s != '\0' && strcmp(s, "Default"))
				{
				if (*s == '/' || s[1] == ':')
					_GK.theme_path = g_strdup(s);
			  	else
					_GK.theme_path = g_strdup_printf("%s/%s/%s",
							gkrellm_homedir(), GKRELLM_THEMES_DIR, s);
				}
			for (i = 0; fgets(buf, sizeof(buf), f); ++i)
				{
				if ((s = strchr(buf, (gint) '\n')) != NULL)
					*s = '\0';
				gkrellm_debug(DEBUG_GUI, "gkrellm_load_theme_config: %s\n", buf);
				if (i == 0)
					sscanf(buf, "%d", &_GK.theme_alternative);
				if (i == 1 && !strstr(buf, "*-*"))	/* XXX Trap out GdkFont */
					gkrellm_dup_string(&large_font.string, buf);
				if (i == 2 && !strstr(buf, "*-*"))
					gkrellm_dup_string(&normal_font.string, buf);
				if (i == 3 && !strstr(buf, "*-*"))
					gkrellm_dup_string(&small_font.string, buf);
				if (i == 4)
					sscanf(buf, "%d", &_GK.theme_scale);
				}
			}
		if (f)
			fclose(f);
		}
	if (!_GK.theme_path || !g_file_test(_GK.theme_path, G_FILE_TEST_IS_DIR))
		gkrellm_dup_string(&_GK.theme_path, "");
	if (!large_font.string)
		gkrellm_dup_string(&large_font.string, "Serif 11");
	if (!normal_font.string)
		gkrellm_dup_string(&normal_font.string, "Serif 9");
	if (!small_font.string)
		gkrellm_dup_string(&small_font.string, "Serif 8");
	}


void
gkrellm_read_theme_event(GtkSettings  *settings)
	{
	Theme	*theme;
	gchar	*s, *theme_name = NULL;
	gint	alt = 0;

	if (settings)	/* called via "notify::gtk-theme-name" signal connect, */
					/* so get the current gtk theme name and switch to it  */
		{
		g_object_get(_GK.gtk_settings, "gtk-theme-name", &theme_name, NULL);
		if (theme_name)
			gkrellm_debug(DEBUG_GUI, "notify::gtk-theme-name: %s\n", theme_name);

		if (   gkrellm_dup_string(&_GK.gtk_theme_name, theme_name)
			&& _GK.track_gtk_theme_name
		   )
			{
			theme = find_theme_in_list(theme_name);
			if (!theme)
				{
				theme_name = g_strdup(_GK.default_track_theme);
				if ((s = strrchr(theme_name, ':')) != NULL)
					{
					*s++ = '\0';
					alt = atoi(s);
					}
				theme = find_theme_in_list(theme_name);
				g_free(theme_name);
				}
			if (   theme && theme->path
				&& gkrellm_dup_string(&_GK.theme_path,
							strcmp(theme->path, "Default") ? theme->path : "")
			   )
				{
				_GK.theme_alternative = alt;
				theme_modified = TRUE;
				gkrellm_save_theme_config();
				gkrellm_build();
				}
			}
		}
	else		/* Called from cb_client_event() because we were sent the    */
				/* _GKRELLM_READ_THEME client event, so reread theme config. */
		{
		g_free(_GK.theme_path);
		_GK.theme_path = NULL;	/* Forces reread of GKRELLM_THEME_CONFIG */
		gkrellm_build();
		}
	}

static void
cb_load_theme(GtkAction *action, GtkWidget *widget)
	{
	GtkTreeIter		iter;
	GtkTreeModel	*model;
	GtkTreePath		*path;
	Theme			*theme;
	gint			row;
        const gchar *act = gtk_action_get_name(action);

	++_GK.theme_reload_count;
	if (_GK.no_config)
		return;
	if (!themes_list)
		gkrellm_make_themes_list();
	if (strcmp(act, "ThemeAltNextAction") == 0 || strcmp(act, "ThemeAltPrevAction") == 0)
		{
		_GK.theme_alternative += ((strcmp(act, "ThemeAltNextAction") == 0) ? 1 : -1);
		if (_GK.theme_alternative > _GK.theme_n_alternatives)
			{
			_GK.theme_alternative = 0;
			act = "ThemeNextAction";
			}
		if (_GK.theme_alternative < 0)
			{
			_GK.theme_alternative = 100;
			act = "ThemePrevAction";
			}
		theme_modified = TRUE;
		}

	if (strcmp(act, "ThemeNextAction") == 0 || strcmp(act, "ThemePrevAction") == 0)
		{
		_GK.theme_alternative = 0;
		if (strcmp(act, "ThemeNextAction") == 0)
			{
			theme_position_in_list = theme_position_in_list->next;
			if (!theme_position_in_list)
				theme_position_in_list = themes_list;
			}
		else
			{
			theme_position_in_list = theme_position_in_list->prev;
			if (!theme_position_in_list)
				theme_position_in_list = g_list_last(themes_list);
			}
		if (config_window)
			{
			row = g_list_position(themes_list, theme_position_in_list);
			model = gtk_tree_view_get_model(theme_treeview);
			gtk_tree_model_iter_nth_child(model, &iter, NULL, row);
			path = gtk_tree_model_get_path(model, &iter);
			gtk_tree_view_set_cursor(theme_treeview, path, NULL, FALSE);
			return;		/* cb_theme_tree_selection_changed -> gkrellm_build()*/
			}
		theme = (Theme *) theme_position_in_list->data;
		gkrellm_dup_string(&_GK.theme_path,
					strcmp(theme->path, "Default") ? theme->path : "");
		theme_modified = TRUE;
		}
	if (strcmp(act, "ThemeScaleUp") == 0 && _GK.theme_scale < 380)
		{
		_GK.theme_scale += 20;
		theme_modified = TRUE;
		}
	else if (strcmp(act, "ThemeScaleDn") == 0 && _GK.theme_scale > 50)
		{
		_GK.theme_scale -= 20;
		theme_modified = TRUE;
		}
	gkrellm_build();
	}

static void
destroy_font_dialogs(void)
	{
	if (large_font.fontseldlg)
		gtk_widget_destroy(GTK_WIDGET(large_font.fontseldlg));
	if (normal_font.fontseldlg)
		gtk_widget_destroy(GTK_WIDGET(normal_font.fontseldlg));
	if (small_font.fontseldlg)
		gtk_widget_destroy(GTK_WIDGET(small_font.fontseldlg));
	}

static void
close_theme_config(gint from_close)
	{
	destroy_font_dialogs();
	}

static void
cb_font_entry_activate(GtkWidget *widget, gpointer *data)
	{
	if (!get_font_entries())
		return;
	theme_modified = TRUE;
	gkrellm_build();
	}

static void
cb_font_entry_changed(GtkWidget *widget, gpointer *data)
	{
	theme_modified = TRUE;
	}

gfloat
gkrellm_get_theme_scale(void)
	{
	return (gfloat) (_GK.theme_scale) / 100.0;
	}

#ifndef WIN32
static void
cb_track_gtk(GtkToggleButton *button, GtkWidget *box)
	{
	_GK.track_gtk_theme_name = button->active;
	gtk_widget_set_sensitive(box, _GK.track_gtk_theme_name);
	}

static void
cb_track_entry_changed(GtkWidget *widget, gpointer *data)
	{
	gchar	*s;

	s = gkrellm_gtk_entry_get_text(&widget);
	gkrellm_dup_string(&_GK.default_track_theme, s);
	gkrellm_config_modified();
	}
#endif // !WIN32

static void
create_theme_tab(GtkWidget *tabs_vbox)
	{
	GtkWidget		*tabs;
	GtkWidget		*vbox, *vbox1, *vbox2, *hbox;
	GtkWidget		*label;
#if !defined(WIN32)
	GtkWidget		*entry;
#endif // !WIN32
	GtkWidget		*scrolled;
	GtkTreeModel	*model;
	GtkCellRenderer	*renderer;
	gchar			*s;

	tabs = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(tabs), GTK_POS_TOP);
	gtk_box_pack_start(GTK_BOX(tabs_vbox), tabs, TRUE, TRUE, 0);

/* --Theme tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Theme"));

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	label = gtk_label_new(_("Theme:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);
	theme_entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(theme_entry), 128);
	if (_GK.theme_path)
		gtk_entry_set_text(GTK_ENTRY(theme_entry), _GK.theme_path);
	gtk_box_pack_start(GTK_BOX(hbox), theme_entry, TRUE, TRUE,0);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	label = gtk_label_new(_("Author:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);
	author_label = gtk_label_new(get_theme_author(_GK.theme_path));
	gtk_misc_set_alignment(GTK_MISC(author_label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), author_label, TRUE, TRUE, 5);

	vbox1 = gkrellm_gtk_framed_vbox(vbox, NULL, 4, TRUE, 0, 2);
	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(vbox1), scrolled, TRUE, TRUE, 0);

	model = theme_create_model();
	theme_treeview = GTK_TREE_VIEW(gtk_tree_view_new_with_model(model));
	g_object_unref(G_OBJECT(model));
	gtk_tree_view_set_rules_hint(theme_treeview, TRUE);
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(theme_treeview, -1,
				_("Theme"), renderer,
				"text", THEME_COLUMN, NULL);
	gtk_container_add(GTK_CONTAINER(scrolled), GTK_WIDGET(theme_treeview));
	theme_selection = gtk_tree_view_get_selection(theme_treeview);
	gtk_tree_selection_set_mode(theme_selection, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(theme_selection), "changed",
				G_CALLBACK(cb_theme_tree_selection_changed), NULL);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
	gkrellm_gtk_spin_button(hbox, &theme_alt_spin_button,
			(gfloat)_GK.theme_alternative, 0.0, 100.0, 1.0, 5.0, 0, 60,
			cb_theme_alternative_changed, NULL, FALSE, NULL);
	theme_alt_label = gtk_label_new("");
	gtk_box_pack_start (GTK_BOX (hbox), theme_alt_label, TRUE, TRUE, 4);
	gtk_misc_set_alignment(GTK_MISC(theme_alt_label), 0, 0.5);
	gkrellm_set_theme_alternatives_label();


/* -- Options tab */
	vbox = gkrellm_gtk_notebook_page(tabs, _("Options"));
	vbox = gkrellm_gtk_framed_vbox(vbox, NULL, 2, TRUE, 10, 4);

#if !defined(WIN32)
	vbox1 = gtk_vbox_new(FALSE, 0);
	gkrellm_gtk_check_button_connected(vbox, &track_gtk_button,
			_GK.track_gtk_theme_name, FALSE, FALSE, 0,
			cb_track_gtk, vbox1,
			_("Track Gtk theme changes for similarly named themes"));
	gtk_widget_set_sensitive(vbox1, _GK.track_gtk_theme_name);
	gtk_box_pack_start(GTK_BOX(vbox), vbox1, FALSE, FALSE, 0);

	vbox1 = gkrellm_gtk_category_vbox(vbox1, NULL, 0, 0, TRUE);
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox, FALSE, FALSE, 0);
	label = gtk_label_new(_("Default"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
	gtk_entry_set_text(GTK_ENTRY(entry), _GK.default_track_theme);
	g_signal_connect(G_OBJECT(entry), "changed",
				G_CALLBACK(cb_track_entry_changed), NULL);

#endif // !WIN32

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
	gkrellm_gtk_spin_button(hbox, NULL,
			(gfloat)_GK.theme_scale, 40.0, 400.0, 10.0, 20.0, 0, 60,
			cb_theme_scale_changed, NULL, FALSE, NULL);
	label = gtk_label_new(_("Scale"));
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 4);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);


/* --Fonts tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Fonts"));
	vbox1 = gkrellm_gtk_framed_vbox(vbox, NULL, 2, FALSE, 0, 2);

	vbox2 = gkrellm_gtk_framed_vbox(vbox1, _("Large font"), 4, FALSE, 0, 3);
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, TRUE, 0);

	large_font.entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), large_font.entry, TRUE, TRUE, 0);
	gtk_entry_set_text(GTK_ENTRY(large_font.entry), large_font.string);
	g_signal_connect(G_OBJECT(large_font.entry), "changed",
			G_CALLBACK(cb_font_entry_changed), NULL);
	g_signal_connect(G_OBJECT(large_font.entry), "activate",
			G_CALLBACK(cb_font_entry_activate), NULL);
	gkrellm_gtk_button_connected(hbox, &large_font.browse_button, FALSE, FALSE,
			0, cb_font_dialog, &large_font, _("Browse"));

	vbox2 = gkrellm_gtk_framed_vbox(vbox1, _("Normal font"), 4, FALSE, 0, 3);
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, TRUE, 0);
	normal_font.entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), normal_font.entry, TRUE, TRUE, 0);
	gtk_entry_set_text(GTK_ENTRY(normal_font.entry), normal_font.string);
	g_signal_connect(G_OBJECT(normal_font.entry), "changed",
			G_CALLBACK(cb_font_entry_changed), NULL);
	g_signal_connect(G_OBJECT(normal_font.entry), "activate",
			G_CALLBACK(cb_font_entry_activate), NULL);
	gkrellm_gtk_button_connected(hbox, &normal_font.browse_button,
			FALSE, FALSE, 0, cb_font_dialog, &normal_font,
			_("Browse"));

	vbox2 = gkrellm_gtk_framed_vbox(vbox1, _("Small font"), 4, FALSE, 0, 3);
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, TRUE, 0);
	small_font.entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), small_font.entry, TRUE, TRUE, 0);
	gtk_entry_set_text(GTK_ENTRY(small_font.entry), small_font.string);
	g_signal_connect(G_OBJECT(small_font.entry), "changed",
			G_CALLBACK(cb_font_entry_changed), NULL);
	g_signal_connect(G_OBJECT(small_font.entry), "activate",
			G_CALLBACK(cb_font_entry_activate), NULL);
	gkrellm_gtk_button_connected(hbox, &small_font.browse_button, FALSE, FALSE,
			0, cb_font_dialog, &small_font, _("Browse"));

	/* --Info tab*/
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Info"));
	s = g_strdup_printf(_("Untar your theme tar files in %s/%s"),
						gkrellm_homedir(), GKRELLM_THEMES_DIR);
	label = gtk_label_new(s);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	g_free(s);

	label = gtk_label_new(
			_("Download themes from the GKrellM theme site at www.muhri.net"));
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);


	}


/* ================================================================ */
/* A simple tree store model for selecting monitor configs.
*/
enum
	{
	NAME_COLUMN,
	MONITOR_COLUMN,
	PAGE_COLUMN,
	N_COLUMNS
	};

static GtkNotebook	*config_notebook;

static gboolean		expand_builtins,
					expand_plugins;

static GtkTreeView	*treeview;
static GtkTreeStore	*model;

static GtkWidget	*apply_button,
					*close_button;

static GkrellmMonitor	*selected_monitor;

static void
close_config(gpointer data)
	{
	GtkTreePath	*path;
	gint		from_close	= GPOINTER_TO_INT(data);

	path = gtk_tree_path_new_from_string("1");
	expand_builtins = gtk_tree_view_row_expanded(treeview, path);
	gtk_tree_path_free(path);

	path = gtk_tree_path_new_from_string("2");
	expand_plugins = gtk_tree_view_row_expanded(treeview, path);
	gtk_tree_path_free(path);

	g_object_unref(G_OBJECT(model));
	gtk_widget_destroy(config_window);
	config_window = NULL;

	gkrellm_plugins_config_close();
	close_theme_config(from_close);
	if (_GK.config_modified)
		gkrellm_save_user_config();
	gkrellm_save_theme_config();
	}


static void
apply_config(void)
	{
	GList	*list;
	GkrellmMonitor	*mon;

	gkrellm_freeze_side_frame_packing();
	for (list = gkrellm_monitor_list; list; list = list->next)
		{
		mon = (GkrellmMonitor *) list->data;
		if (   mon->apply_config && mon->privat->enabled
			&& (mon->privat->config_created || !mon->create_config)
		   )
			{
			gkrellm_record_state(APPLY_CONFIG, mon);
			(*(mon->apply_config))();
			gkrellm_record_state(INTERNAL, NULL);
			}
		}
	gkrellm_thaw_side_frame_packing();
	gkrellm_config_modified();
	}

static void
OK_config(void)
	{
	apply_config();
	close_config(GINT_TO_POINTER(0));
	}


static GtkWidget *
create_config_page(GkrellmMonitor *mon, GtkTreeStore *tree, GtkTreeIter *iter,
			GtkNotebook *notebook)
	{
	GtkWidget	*vbox;
	gint		page;

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_notebook_append_page(notebook, vbox, NULL);
	page = g_list_length(notebook->children) - 1;

	if (mon)
		mon->privat->config_page = page;

	gtk_tree_store_set(tree, iter,
			MONITOR_COLUMN, mon,
			PAGE_COLUMN, page,
			-1);

	gkrellm_debug(DEBUG_GUI, "create_config_page %d: %s\n", page,
		mon ? mon->name : "--");

	return vbox;
	}

static void
real_create_config(GkrellmMonitor *mon)
	{
	if (mon->privat->config_created || !mon->create_config)
		return;

	gkrellm_record_state(CREATE_CONFIG, mon);
	(*(mon->create_config))(mon->privat->config_vbox);
	gkrellm_record_state(INTERNAL, NULL);

	gtk_widget_show_all(mon->privat->config_vbox);
	mon->privat->config_created = TRUE;
	}

void
gkrellm_add_plugin_config_page(GkrellmMonitor *mon)
	{
	GtkTreeIter		iter, plugin_iter;
	GtkTreePath		*path;

	if (config_window && mon->create_config)
		{
		path = gtk_tree_path_new_from_string("2");
		gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &plugin_iter, path);
		gtk_tree_path_free(path);

		gtk_tree_store_append(model, &iter, &plugin_iter);
		gtk_tree_store_set(model, &iter, NAME_COLUMN, mon->name, -1);
		mon->privat->config_vbox =
					create_config_page(mon, model, &iter, config_notebook);
		mon->privat->config_created = FALSE;

		mon->privat->row_reference =
			gtk_tree_row_reference_new(GTK_TREE_MODEL(model),
					gtk_tree_model_get_path(GTK_TREE_MODEL(model), &iter));
		}
	else
		mon->privat->config_page = -1;
	}

void
gkrellm_remove_plugin_config_page(GkrellmMonitor *mon)
	{
	GtkTreePath		*path;
	GtkTreeIter		iter;
	GList			*list;
	GkrellmMonitor	*tmon;

	if (mon->privat->config_page > 0)
		{
		path = gtk_tree_row_reference_get_path(mon->privat->row_reference);
		gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, path);
		gtk_tree_store_remove(model, &iter);

		gtk_notebook_remove_page(config_notebook, mon->privat->config_page);

		/* When a config_page is removed, any greater plugin config_page must
		|  be decremented
		*/
		for (list = gkrellm_monitor_list; list; list = list->next)
			{
			tmon = (GkrellmMonitor *) list->data;
			if (mon->privat->config_page >= tmon->privat->config_page)
				continue;
			tmon->privat->config_page -= 1;
			gkrellm_debug(DEBUG_GUI, "config_page %d: %s\n",
						tmon->privat->config_page,  tmon->name);
			}
		}
	mon->privat->config_page = -1;
	}

  /* If a config page uses instant apply, hide the APPLY/CLOSE buttons and
  |  assume the config will be modified.
  */
static void
set_apply_mode(gboolean instant)
	{
	if (instant)
		{
		gtk_widget_hide(apply_button);
		gtk_widget_hide(close_button);
		gkrellm_config_modified();
		}
	else
		{
		gtk_widget_show(apply_button);
		gtk_widget_show(close_button);
		}
	}

static void
cb_tree_selection_changed(GtkTreeSelection *selection, gpointer data)
	{
	GtkTreeIter		iter;
	GtkTreeModel	*model;
	gint			page;
	gboolean		instant;

	if (!gtk_tree_selection_get_selected(selection, &model, &iter))
		return;
	if (selected_monitor && selected_monitor->apply_config)
		{
		gkrellm_record_state(APPLY_CONFIG, selected_monitor);
		(*(selected_monitor->apply_config))();
		gkrellm_record_state(INTERNAL, NULL);
		}
	gtk_tree_model_get(model, &iter,
				MONITOR_COLUMN, &selected_monitor,
				PAGE_COLUMN, &page,
				-1);
	if (selected_monitor && selected_monitor->privat)
		{
		page = selected_monitor->privat->config_page;
		real_create_config(selected_monitor);
		if (selected_monitor == gkrellm_get_sensors_mon())
			{	/* Special case dependencies in the configs */
			real_create_config(gkrellm_get_cpu_mon());
			real_create_config(gkrellm_get_proc_mon());
			}
		instant = (   selected_monitor->apply_config == NULL
				   || selected_monitor->privat->instant_apply);
		}
	else
		instant = TRUE;

	set_apply_mode(instant);

	gtk_notebook_set_current_page(config_notebook, page);
	gkrellm_debug(DEBUG_GUI, "tree_selection_changed %d: %s\n",
					page, selected_monitor ? selected_monitor->name : "--");
	}

  /* Monitors may want to present as instant apply monitors, but still need
  |  their apply function called when changing notebook pages or on OK button.
  */
void
gkrellm_config_instant_apply(GkrellmMonitor *mon)
	{
	mon->privat->instant_apply = TRUE;
	}

gboolean
gkrellm_config_window_shown(void)
	{
	return config_window ? TRUE : FALSE;
	}

void
create_config_window(void)
	{
	GtkWidget			*widget,
						*vbox,
						*main_vbox,
						*config_hbox,
						*hbox;
	GtkWidget			*scrolled;
	GtkWidget			*button;
	GtkTreeIter			iter, citer;
	GtkTreePath			*path;
	GtkCellRenderer		*renderer;
	GtkTreeViewColumn	*column;
	GtkTreeSelection	*select;
	GList				*list;
	GkrellmMonitor		*mon;
	gchar				*config_name, *window_title;
	
	if (config_window)
		{
		gtk_window_present(GTK_WINDOW(config_window));
		return;
		}
	selected_monitor = NULL;

	config_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(G_OBJECT(config_window), "delete_event",
			G_CALLBACK(close_config), GINT_TO_POINTER(1));

	config_name = gkrellm_make_config_file_name(NULL, "GKrellM");
	window_title = g_strdup_printf("%s %s", config_name, _("Configuration"));
	gtk_window_set_title(GTK_WINDOW(config_window), window_title);
	g_free(config_name);
	g_free(window_title);

	gtk_window_set_wmclass(GTK_WINDOW(config_window),
					"Gkrellm_conf", "Gkrellm");
	gtk_container_set_border_width(GTK_CONTAINER(config_window), 2);
	
	config_hbox = gtk_hbox_new(FALSE, 4);
	gtk_container_add(GTK_CONTAINER(config_window), config_hbox);

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
			GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(config_hbox), scrolled, FALSE, FALSE, 0);

	main_vbox = gtk_vbox_new(FALSE, 4);
	gtk_box_pack_start(GTK_BOX(config_hbox), main_vbox, TRUE, TRUE, 0);

	widget = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(main_vbox), widget, TRUE, TRUE, 0);
	config_notebook = GTK_NOTEBOOK(widget);
	gtk_notebook_set_show_tabs(config_notebook, FALSE);

	model = gtk_tree_store_new(N_COLUMNS,
				G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_INT);
	gtk_tree_store_append(model, &iter, NULL);
	gtk_tree_store_set(model, &iter, NAME_COLUMN, _("General"), -1);
	vbox = create_config_page(NULL, model, &iter, config_notebook);
	create_general_tab(vbox);

	gtk_tree_store_append(model, &iter, NULL);
	gtk_tree_store_set(model, &iter, NAME_COLUMN, _("Builtins"), -1);
	vbox = create_config_page(NULL, model, &iter, config_notebook);

	for (list = gkrellm_monitor_list; list; list = list->next)
		{
		mon = (GkrellmMonitor *) list->data;
		if (MONITOR_ID(mon) == MON_PLUGIN || ! mon->create_config)
			continue;

		gtk_tree_store_append(model, &citer, &iter);
		gtk_tree_store_set(model, &citer, NAME_COLUMN, mon->name, -1);
		mon->privat->config_vbox =
					create_config_page(mon, model, &citer, config_notebook);
		mon->privat->config_created = FALSE;
		mon->privat->row_reference =
			gtk_tree_row_reference_new(GTK_TREE_MODEL(model),
					gtk_tree_model_get_path(GTK_TREE_MODEL(model), &citer));
		}

	gtk_tree_store_append(model, &iter, NULL);
	gtk_tree_store_set(model, &iter, NAME_COLUMN, _("Plugins"), -1);
	vbox = create_config_page(NULL, model, &iter, config_notebook);
	gkrellm_plugins_config_create(vbox);

	gtk_tree_store_append(model, &iter, NULL);
	gtk_tree_store_set(model, &iter, NAME_COLUMN, _("Themes"), -1);
	vbox = create_config_page(NULL, model, &iter, config_notebook);
	create_theme_tab(vbox);

	gtk_tree_store_append(model, &iter, NULL);
	gtk_tree_store_set(model, &iter, NAME_COLUMN, _("About"), -1);
	vbox = create_config_page(NULL, model, &iter, config_notebook);
	create_about_tab(vbox);

	/* Add plugin notebook pages last since they may need special add/remove
	|  actions as plugins are enabled/disabled.
	*/
	for (list = gkrellm_monitor_list; list; list = list->next)
		{
		mon = (GkrellmMonitor *) list->data;
		if (   ! mon->create_config
			|| ! mon->privat->enabled
			|| MONITOR_ID(mon) != MON_PLUGIN
		   )
			continue;
		gkrellm_add_plugin_config_page(mon);
		}

	/* Create the tree view and don't unref the model because need to modify
	|  it when enabling plugins
	*/
	treeview =
			GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(model)));

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Monitors"), renderer,
					"text", NAME_COLUMN, NULL);
	gtk_tree_view_append_column(treeview, column);
	gtk_container_add(GTK_CONTAINER(scrolled), GTK_WIDGET(treeview));

	select = gtk_tree_view_get_selection(treeview);
	gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(select), "changed",
				G_CALLBACK(cb_tree_selection_changed), NULL);

	if (expand_builtins)
		{
		path = gtk_tree_path_new_from_string("1");
		gtk_tree_view_expand_row(treeview, path, TRUE);
		gtk_tree_path_free(path);
		}
	if (expand_plugins)
		{
		path = gtk_tree_path_new_from_string("2");
		gtk_tree_view_expand_row(treeview, path, TRUE);
		gtk_tree_path_free(path);
		}

	hbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(hbox), 5);
	gtk_box_pack_start(GTK_BOX(main_vbox), hbox, FALSE, FALSE, 0);

	apply_button = gtk_button_new_from_stock(GTK_STOCK_APPLY);
	GTK_WIDGET_SET_FLAGS(apply_button, GTK_CAN_DEFAULT);
	g_signal_connect(G_OBJECT(apply_button), "clicked",
				G_CALLBACK(apply_config), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), apply_button, TRUE, TRUE, 0);

	close_button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	GTK_WIDGET_SET_FLAGS(close_button, GTK_CAN_DEFAULT);
	g_signal_connect(G_OBJECT(close_button), "clicked",
				G_CALLBACK(close_config), GINT_TO_POINTER(1));
	gtk_box_pack_start(GTK_BOX(hbox), close_button, TRUE, TRUE, 0);

	button = gtk_button_new_from_stock(GTK_STOCK_OK);
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(OK_config), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
	gtk_widget_grab_default(button);

	gtk_widget_show_all(config_window);
	}

void
gkrellm_open_config_window(GkrellmMonitor *mon)
	{
	GtkTreePath	*path;

	if (!mon || !mon->create_config || _GK.no_config)
		return;
	create_config_window();
	if (MONITOR_ID(mon) == MON_PLUGIN)
		path = gtk_tree_path_new_from_string("2");
	else
		path = gtk_tree_path_new_from_string("1");

	gtk_tree_view_expand_row(treeview, path, TRUE);
	gtk_tree_path_free(path);

	path = gtk_tree_row_reference_get_path(mon->privat->row_reference);
	gtk_tree_view_set_cursor(treeview, path, NULL, FALSE);
	}

static const char *ui_items_no_config = "\
<ui>\
  <popup>\
    <separator/>\
    <menuitem name=\"Quit\" action=\"QuitAction\"/>\
    <separator/>\
  </popup>\
</ui>\
";

static const char *ui_items = "\
<ui>\
  <popup accelerators=\"true\">\
    <menuitem name=\"Configuration\" action=\"ConfigurationAction\"/>\
    <menu name=\"ThemeMenu\" action=\"ThemeMenuAction\">\
      <menuitem name=\"ThemeAltNext\" action=\"ThemeAltNextAction\"/>\
      <menuitem name=\"ThemeAltPrev\" action=\"ThemeAltPrevAction\"/>\
    </menu>\
    <separator/>\
    <menuitem name=\"Quit\" action=\"QuitAction\"/>\
  </popup>\
</ui>\
";

/*
static const char *ui_items_debug = "\
  <popup>\
    <menuitem name=\"ThemeNext\" action=\"ThemeNextAction\"/>\
    <menuitem name=\"ThemePrev\" action=\"ThemePrevAction\"/>\
    <menuitem name=\"MenuPopup\" action=\"MenuPopupAction\"/>\
    <menuitem name=\"ReloadTheme\" action=\"ReloadThemeAction\"/>\
    <menuitem name=\"ScaleThemeUp\" action=\"ScaleThemeUpAction\"/>\
    <menuitem name=\"ScaleThemeDn\" action=\"ScaleThemeDnAction\"/>\
  </popup>\
";
*/

static GtkActionEntry ui_entries[] = 
{
    { "QuitAction", NULL, N_("Quit"),
      NULL, NULL, G_CALLBACK(gtk_main_quit) },
    { "ConfigurationAction", NULL, N_("Configuration"),
      "F1", NULL, G_CALLBACK(create_config_window) },
    { "ThemeMenuAction", NULL, N_("Theme"),
      NULL, NULL, NULL },
    { "ThemeAltNextAction", NULL, N_("Next"),
      "Page_Up", NULL, G_CALLBACK(cb_load_theme) },
    { "ThemeAltPrevAction", NULL, N_("Prev"),
      "Page_Down", NULL, G_CALLBACK(cb_load_theme) },
    { "ThemeNextAction", NULL, N_("Theme next"),
      "<control>Page_Up", NULL, G_CALLBACK(cb_load_theme) },
    { "ThemePrevAction", NULL, N_("Theme prev"),
      "<control>Page_Down", NULL, G_CALLBACK(cb_load_theme) },
    { "MenuPopupAction", NULL, N_("Menu Popup"),
      "F2", NULL, G_CALLBACK(cb_load_theme) },
    { "ReloadThemeAction", NULL, N_("Reload Theme"),
      "F5", NULL, G_CALLBACK(cb_load_theme) },
    { "ScaleThemeUpAction", NULL, N_("Scale Theme Up"),
      "F6", NULL, G_CALLBACK(cb_load_theme) },
    { "ScaleThemeDnAction", NULL, N_("Scale Theme Dn"),
      "F7", NULL, G_CALLBACK(cb_load_theme) },
};
static guint n_ui_entries = G_N_ELEMENTS (ui_entries);

GtkUIManager *
gkrellm_create_ui_manager_popup(void)
	{
	GtkWidget		*top_win;
        GtkUIManager *ui_manager;
        GtkActionGroup *action_group;
        GError *error;

	top_win = gkrellm_get_top_window();
        action_group = gtk_action_group_new ("UiActions");
        gtk_action_group_add_actions (action_group, ui_entries, n_ui_entries, NULL);
        ui_manager = gtk_ui_manager_new ();
        gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
        error = NULL;
        if (_GK.no_config)
            gtk_ui_manager_add_ui_from_string (ui_manager, ui_items_no_config,
                                               strlen(ui_items_no_config), &error);
        else
            gtk_ui_manager_add_ui_from_string (ui_manager, ui_items,
                                               strlen(ui_items), &error);
        if (error)
        {
            g_message ("building menus failed: %s", error->message);
            g_error_free (error);
            return NULL;
        }
	gtk_window_add_accel_group(GTK_WINDOW(top_win),
                                   gtk_ui_manager_get_accel_group (ui_manager));

	return ui_manager;
	}

