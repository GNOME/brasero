/*
 * Brasero is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * Brasero is distributed in the hope that it will be useful,
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
/***************************************************************************
 *            utils.c
 *
 *  Wed May 18 16:58:16 2005
 *  Copyright  2005  Philippe Rouquier
 *  <brasero-app@wanadoo.fr>
 ****************************************************************************/


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

#include <gst/gst.h>

#include "brasero-app.h"
#include "brasero-utils.h"
#include "brasero-data-tree-model.h"

#define BRASERO_ERROR brasero_error_quark()

static GHashTable *stringsH = NULL;
G_LOCK_DEFINE_STATIC (stringsH);

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

GQuark
brasero_error_quark (void)
{
	static GQuark quark = 0;

	if (!quark)
		quark = g_quark_from_static_string ("BraSero_error");

	return quark;
}

void
brasero_utils_init (void)
{
	/* This function does two things:
	 * - register brasero icons
	 * - load all the gid of the user
	 */
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
					   BRASERO_DATADIR "/icons");

	g_atexit (brasero_utils_free);
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

gchar *
brasero_utils_get_time_string (guint64 time,
			       gboolean with_unit,
			       gboolean round)
{
	gint64 second, minute, hour;

	time /= 1000000000;
	hour = time / 3600;
	time = time % 3600;
	minute = time / 60;

	if (round) {
		if ((time % 60) > 30)
			minute ++;

		second = 0;
	}
	else
		second = time % 60;

	if (hour) {
		if (with_unit) {
			if (hour && minute && second)
				return g_strdup_printf ("%lli h %02lli min %02lli",
							 hour,
							 minute,
							 second);
			else if (hour && minute)
				return g_strdup_printf ("%lli h %02lli",
							 hour,
							 minute);
			else
				return g_strdup_printf ("%lli h",hour);
		}
		else if (hour && minute && second)
			return g_strdup_printf ("%lli:%02lli:%02lli",
						 hour,
						 minute,
						 second);
		else if (hour && minute)
			return g_strdup_printf ("%lli:%02lli", hour, minute);
	}

	if (with_unit) {
		if (!second)
			return g_strdup_printf (_("%lli min"), minute);
		else
			return g_strdup_printf (_("%lli:%02lli min"), minute, second);
	}
	else
		return g_strdup_printf ("%lli:%02lli", minute, second);
}

gchar *
brasero_utils_get_time_string_from_size (gint64 size,
					 gboolean with_unit,
					 gboolean round)
{
	guint64 time = 0;

	time = BRASERO_BYTES_TO_DURATION (size);
	return brasero_utils_get_time_string (time, with_unit, round);
}

enum {
	BRASERO_UTILS_NO_UNIT,
	BRASERO_UTILS_KO,
	BRASERO_UTILS_MO,
	BRASERO_UTILS_GO
};

gchar *
brasero_utils_get_size_string (gint64 dsize,
			       gboolean with_unit,
			       gboolean round)
{
	int unit;
	int size;
	int remain = 0;

	if (with_unit)
		return g_format_size_for_display (dsize);

	if (dsize < 1024) {
		unit = BRASERO_UTILS_NO_UNIT;
		size = (int) dsize;
		goto end;
	}

	size = (int) (dsize / 1024);
	if (size < 1024) {
		unit = BRASERO_UTILS_KO;
		goto end;
	}

	size = (int) (size / 1024);
	if (size < 1024) {
		unit = BRASERO_UTILS_MO;
		goto end;
	}

	remain = (size % 1024) / 100;
	size = size / 1024;
	unit = BRASERO_UTILS_GO;

      end:

	if (round && size > 10) {
		gint remains;

		remains = size % 10;
		size -= remains;
	}

	if (remain)
		return g_strdup_printf ("%i.%i",
					size,
					remain);
	else
		return g_strdup_printf ("%i",
					size);
}

gchar *
brasero_utils_get_sectors_string (gint64 sectors,
				  gboolean time_format,
				  gboolean with_unit,
				  gboolean round)
{
	gint64 size;

	if (time_format) {
		size = sectors * GST_SECOND / 75;
		return brasero_utils_get_time_string (size, with_unit, round);
	}
	else {
		size = sectors * 2048;
		return brasero_utils_get_size_string (size, with_unit, round);
	}
}

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

/* Copied from glib-2.8.3 (glib.c) but slightly
 * modified to use only the first 64 bytes */
gboolean
brasero_utils_str_equal_64 (gconstpointer v1,
			    gconstpointer v2)
{
  const gchar *string1 = v1;
  const gchar *string2 = v2;
  
  return strncmp (string1, string2, 64) == 0;
}

/* Copied from glib-2.8.3 (glib.c) but slightly
 * modified to use only the first 64 bytes */
guint
brasero_utils_str_hash_64 (gconstpointer v)
{
  /* 31 bit hash function */
  const signed char *p = v;
  guint32 h = *p;
  int i;

  if (h)
    for (p += 1, i = 0; *p != '\0' && i < 64; p++, i++)
      h = (h << 5) - h + *p;

  return h;
}

void
brasero_utils_launch_app (GtkWidget *widget,
			  GSList *list)
{
	GSList *item;

	for (item = list; item; item = item->next) {
		GError *error;
		gchar *uri;

		error = NULL;
		uri = item->data;

		if (!g_app_info_launch_default_for_uri (uri, NULL, &error)) {
			gchar *string;

			string = g_strdup_printf ("\"%s\" could not be opened", uri);
			brasero_app_alert (brasero_app_get_default (),
					   string,
					   error->message,
					   GTK_MESSAGE_ERROR);
			g_free (string);
			g_error_free (error);
			continue;
		}
	}
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
