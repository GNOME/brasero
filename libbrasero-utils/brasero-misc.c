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

static GHashTable *stringsH = NULL;
G_LOCK_DEFINE_STATIC (stringsH);


/**
 * Error reporting
 */

GQuark
brasero_utils_error_quark (void)
{
	static GQuark quark = 0;

	if (!quark)
		quark = g_quark_from_static_string ("Brasero_utils_error");

	return quark;
}

/**
 * Debug
 */

static gboolean use_debug = FALSE;

static const GOptionEntry options [] = {
	{ "brasero-utils-debug", 'g', 0, G_OPTION_ARG_NONE, &use_debug,
	  N_("Display debug statements on stdout for Brasero utilities library"),
	  NULL },
	{ NULL }
};

void
brasero_utils_set_use_debug (gboolean active)
{
	use_debug = active;
}

GOptionGroup *
brasero_utils_get_option_group (void)
{
	GOptionGroup *group;

	group = g_option_group_new ("brasero-utils",
				    N_("Brasero utilities library"),
				    N_("Display options for Brasero-utils library"),
				    NULL,
				    NULL);
	g_option_group_add_entries (group, options);
	return group;
}

void
brasero_utils_debug_message (const gchar *location,
			     const gchar *format,
			     ...)
{
	va_list arg_list;

	if (!use_debug)
		return;

	g_strdup_printf ("BraseroUtils: (at %s) ", location);

	va_start (arg_list, format);
	vprintf (format, arg_list);
	va_end (arg_list);

	putchar ('\n');
}

static gboolean
brasero_utils_clear_strings_cb (gchar *string,
				guint ref,
				gpointer NULL_data)
{
	g_free (string);
	return TRUE;
}

static void
brasero_utils_free (void)
{
	if (stringsH) {
		G_LOCK (stringsH);
		g_hash_table_foreach_remove (stringsH,
					     (GHRFunc) brasero_utils_clear_strings_cb,
					     NULL);
		g_hash_table_destroy (stringsH);
		stringsH = NULL;
		G_UNLOCK (stringsH);
	}
}

void
brasero_utils_init (void)
{
	g_atexit (brasero_utils_free);
}

gchar*
brasero_utils_validate_utf8 (const gchar *name)
{
	gchar *retval, *ptr;
	const gchar *invalid;

	if (!name)
		return NULL;

	if (g_utf8_validate (name, -1, &invalid))
		return g_strdup (name);

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

gchar *
brasero_utils_get_uri_name (const gchar *uri)
{
	gchar *utf8_name;
	GFile *vfs_uri;
	gchar *name;

	/* g_path_get_basename is not comfortable with uri related
	 * to the root directory so check that before */
	vfs_uri = g_file_new_for_uri (uri);
	name = g_file_get_basename (vfs_uri);
	g_object_unref (vfs_uri);

	/* NOTE and reminder names are already unescaped; the following is not
	 * needed: unescaped_name = g_uri_unescape_string (name, NULL); */

	/* NOTE: a graft should be added for non utf8 name since we
	 * modify them; in fact we use this function only in the next
	 * one which creates only grafted nodes. */
	utf8_name = brasero_utils_validate_utf8 (name);
	if (utf8_name) {
		g_free (name);
		return utf8_name;
	}

	return name;
}

/**
 * Allows multiple uses of the same string
 */

gchar *
brasero_utils_register_string (const gchar *string)
{
	gboolean success;
	gpointer key, reftmp;
	guint ref;

	if (!string) {
		g_warning ("Null string to be registered");
		return NULL;
	}

	G_LOCK (stringsH);

	if (!stringsH) {
		stringsH = g_hash_table_new (g_str_hash, g_str_equal);
		success = FALSE;
	}
	else
		success = g_hash_table_lookup_extended (stringsH,
							string,
							&key,
							&reftmp);

	if (!success) {
		key = g_strdup (string);
		g_hash_table_insert (stringsH,
				     key,
				     GINT_TO_POINTER (1));
		G_UNLOCK (stringsH);
		return key;
	}

	ref = GPOINTER_TO_INT(reftmp) + 1;
	g_hash_table_insert (stringsH,
			     key,
			     GINT_TO_POINTER (ref));

	G_UNLOCK (stringsH);
	return key;
}

void
brasero_utils_unregister_string (const gchar *string)
{
	gboolean success;
	gpointer key, reftmp;
	guint ref;

	if (!string) {
		g_warning ("Null string to be unregistered");
		return;
	}

	G_LOCK (stringsH);

	if (!stringsH) {
		G_UNLOCK (stringsH);
		return;
	}

	success = g_hash_table_lookup_extended (stringsH,
						string,
						&key,
						&reftmp);
	if (!success) {
		G_UNLOCK (stringsH);
		return;
	}

	ref = GPOINTER_TO_INT(reftmp) - 1;

	if (ref > 0)
		g_hash_table_insert (stringsH, key, GINT_TO_POINTER (ref));
	else if (ref <= 0) {
		g_hash_table_remove (stringsH, string);
		g_free (key);
	}

	G_UNLOCK (stringsH);
}

GtkWidget *
brasero_utils_properties_get_label (GtkWidget *properties)
{
	GList *children;
	GList *iter;

	children = gtk_container_get_children (GTK_CONTAINER (properties));
	for (iter = children; iter; iter = iter->next) {
		GtkWidget *widget;

		widget = iter->data;
		if (GTK_IS_LABEL (widget)) {
			g_list_free (children);
			return widget;
		}
	}

	g_list_free (children);
	return NULL;
}

GtkWidget *
brasero_utils_pack_properties_list (const gchar *title, GSList *list)
{
	GtkWidget *hbox, *vbox_main, *vbox_prop;
	GtkWidget *label;
	GSList *iter;

	vbox_main = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (vbox_main);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_end (GTK_BOX (vbox_main),
			  hbox,
			  TRUE,
			  TRUE,
			  6);

	label = gtk_label_new ("\t");
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox),
			    label,
			    FALSE,
			    TRUE,
			    0);

	vbox_prop = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_show (vbox_prop);
	gtk_box_pack_start (GTK_BOX (hbox),
			    vbox_prop,
			    TRUE,
			    TRUE,
			    0);

	for (iter = list; iter; iter = iter->next)
		gtk_box_pack_start (GTK_BOX (vbox_prop),
				    iter->data,
				    TRUE,
				    TRUE,
				    0);

	if (title) {
		GtkWidget *vbox;
		GtkWidget *label;

		vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

		label = gtk_label_new (title);
		gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
		gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);
		gtk_widget_show (label);

		gtk_box_pack_start (GTK_BOX (vbox), vbox_main, TRUE, TRUE, 0);

		gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
		gtk_widget_show (vbox);

		return vbox;
	}

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

GtkWidget *
brasero_utils_create_message_dialog (GtkWidget *parent,
				     const gchar *primary_message,
				     const gchar *secondary_message,
				     GtkMessageType type)
{
	GtkWidget *message;

	message = gtk_message_dialog_new (GTK_WINDOW (parent),
					  0,
					  type,
					  GTK_BUTTONS_CLOSE,
					  "%s",
					  primary_message);

	gtk_window_set_icon_name (GTK_WINDOW (message),
	                          parent? gtk_window_get_icon_name (GTK_WINDOW (parent)):"brasero");

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
