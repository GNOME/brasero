/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-misc
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-misc is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-misc authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-misc. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-misc is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtk.h>

#include "brasero-misc.h"

GtkWidget *
brasero_utils_pack_properties_list (const gchar *title, GSList *list)
{
	GtkWidget *hbox, *vbox_main, *vbox_prop;
	GtkWidget *label;
	GSList *iter;

	vbox_main = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox_main);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_end (GTK_BOX (vbox_main),
			  hbox,
			  TRUE,
			  TRUE,
			  6);

	label = gtk_label_new ("\t");
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

	vbox_prop = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox_prop);
	gtk_box_pack_start (GTK_BOX (hbox),
			    vbox_prop,
			    TRUE,
			    TRUE,
			    0);

	for (iter = list; iter; iter = iter->next) {
		gtk_box_pack_start (GTK_BOX (vbox_prop),
				    iter->data,
				    TRUE,
				    TRUE,
				    0);
	}

	if (title) {
		GtkWidget *frame;

		frame = gtk_frame_new (title);
		gtk_widget_show (frame);
		gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);

		label = gtk_frame_get_label_widget (GTK_FRAME (frame));
		gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

		gtk_container_set_border_width (GTK_CONTAINER (frame), 6);
		gtk_container_add (GTK_CONTAINER (frame), vbox_main);
		return frame;
	}
	else
		gtk_container_set_border_width (GTK_CONTAINER (vbox_main), 6);

	return vbox_main;
}

GtkWidget *
brasero_utils_pack_properties (const gchar *title, ...)
{
	va_list vlist;
	GtkWidget *child;
	GtkWidget *result;
	GSList *list = NULL;

	va_start (vlist, title);
	while ((child = va_arg (vlist, GtkWidget *)))
		list = g_slist_prepend (list, child);
	va_end (vlist);

	result = brasero_utils_pack_properties_list (title, list);
	g_slist_free (list);

	return result;
}

GtkWidget *
brasero_utils_make_button (const gchar *text,
			   const gchar *stock,
			   const gchar *theme, 
			   GtkIconSize size)
{
	GtkWidget *image = NULL;
	GtkWidget *button;

	if (theme)
		image = gtk_image_new_from_icon_name (theme, size);

	if (!image && stock)
		image = gtk_image_new_from_stock (stock, size);

	button = gtk_button_new ();

	if (image)
		gtk_button_set_image (GTK_BUTTON (button), image);

	gtk_button_set_label (GTK_BUTTON (button), text);
	gtk_button_set_use_underline (GTK_BUTTON (button), TRUE);
	return button;
}

gchar*
brasero_utils_validate_utf8 (const gchar *name)
{
	gchar *retval, *ptr;
	const gchar *invalid;

	if (!name)
		return NULL;

	if (g_utf8_validate (name, -1, &invalid))
		return NULL;

	retval = g_strdup (name);
	ptr = retval + (invalid - name);
	*ptr = '_';
	ptr++;

	while (!g_utf8_validate (ptr, -1, &invalid)) {
		ptr = (gchar*) invalid;
		*ptr = '?';
		ptr ++;
	}

	return retval;
}

GtkWidget *
brasero_utils_create_message_dialog (GtkWidget *parent,
				     const gchar *primary_message,
				     const gchar *secondary_message,
				     GtkMessageType type)
{
	GtkWidget *message;

	message = gtk_message_dialog_new (GTK_WINDOW (parent),
					  GTK_DIALOG_MODAL |
					  GTK_DIALOG_DESTROY_WITH_PARENT,
					  type,
					  GTK_BUTTONS_CLOSE,
					  "%s",
					  primary_message);

	gtk_window_set_title (GTK_WINDOW (message), "");

	if (secondary_message)
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
							  "%s.",
							  secondary_message);

	return message;
}

void
brasero_utils_message_dialog (GtkWidget *parent,
			      const gchar *primary_message,
			      const gchar *secondary_message,
			      GtkMessageType type)
{
	GtkWidget *message;

	message = brasero_utils_create_message_dialog (parent,
						       primary_message,
						       secondary_message,
						       type);

	gtk_dialog_run (GTK_DIALOG (message));
	gtk_widget_destroy (message);
}
