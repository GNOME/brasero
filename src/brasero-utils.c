/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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

#include <libgnomeui/libgnomeui.h>

#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-file-info.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>

#include <nautilus-burn-drive.h>

#include <gst/gst.h>

#include "brasero-utils.h"
#include "brasero-data-tree-model.h"

#define BRASERO_ERROR brasero_error_quark()

static gid_t *groups = NULL;
G_LOCK_DEFINE (groups_mutex);

static GHashTable *stringsH = NULL;
G_LOCK_DEFINE (strings_mutex);

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
	if (groups) {
		G_LOCK (groups_mutex);
		g_free (groups);
		groups = NULL;
		G_UNLOCK (groups_mutex);
	}

	if (stringsH) {
		G_LOCK (strings_mutex);
		g_hash_table_foreach_remove (stringsH,
					     (GHRFunc) brasero_utils_clear_strings_cb,
					     NULL);
		g_hash_table_destroy (stringsH);
		stringsH = NULL;
		G_UNLOCK (strings_mutex);
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

	/* load gids of the user */
	if (groups == NULL) {
		gint nb;

		nb = getgroups (0, NULL);
		G_LOCK (groups_mutex);
		groups = g_new0 (gid_t, nb + 1);
		nb = getgroups (nb, groups);
		G_UNLOCK (groups_mutex);
	}

	g_atexit (brasero_utils_free);
}

inline gboolean
brasero_utils_is_gid_in_groups (gid_t gid)
{
	gid_t *group;

	G_LOCK (groups_mutex);
	if (!groups) {
		G_UNLOCK (groups_mutex);
		return FALSE;
	}

	for (group = groups; group && *group; group ++) {
		if (*group == gid) {
			G_UNLOCK (groups_mutex);
			return TRUE;
		}
	}

	G_UNLOCK (groups_mutex);
	return FALSE;
}

/**
 * Allows multiple uses of the same string
 */

gchar *
brasero_utils_register_string (const gchar *string)
{
	gboolean success;
	gpointer key;
	guint ref;

	if (!string) {
		g_warning ("Null string to be registered");
		return NULL;
	}

	G_LOCK (strings_mutex);

	if (!stringsH) {
		stringsH = g_hash_table_new (g_str_hash, g_str_equal);
		success = FALSE;
	}
	else
		success = g_hash_table_lookup_extended (stringsH,
							string,
							&key,
							(gpointer) &ref);

	if (!success) {
		key = g_strdup (string);
		g_hash_table_insert (stringsH,
				     key,
				     GINT_TO_POINTER (1));
		G_UNLOCK (strings_mutex);
		return key;
	}

	ref ++;
	g_hash_table_insert (stringsH,
			     key,
			     GINT_TO_POINTER (ref));

	G_UNLOCK (strings_mutex);
	return key;
}

void
brasero_utils_unregister_string (const gchar *string)
{
	gboolean success;
	gpointer key;
	guint ref;

	if (!string) {
		g_warning ("Null string to be unregistered");
		return;
	}

	G_LOCK (strings_mutex);

	if (!stringsH) {
		G_UNLOCK (strings_mutex);
		return;
	}

	success = g_hash_table_lookup_extended (stringsH,
						string,
						&key,
						(gpointer) &ref);
	if (!success) {
		G_UNLOCK (strings_mutex);
		return;
	}

	ref --;

	if (ref > 0)
		g_hash_table_insert (stringsH, key, GINT_TO_POINTER (ref));
	else if (ref <= 0) {
		g_hash_table_remove (stringsH, string);
		g_free (key);
	}

	G_UNLOCK (strings_mutex);
}

gchar *
brasero_utils_get_time_string (gint64 time,
			       gboolean with_unit,
			       gboolean round)
{
	int second, minute, hour;

	time = time / 1000000000;
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
				return g_strdup_printf ("%i h %02i min %02i",
							 hour,
							 minute,
							 second);
			else if (hour && minute)
				return g_strdup_printf ("%i h %02i",
							 hour,
							 minute);
			else
				return g_strdup_printf ("%i h",hour);
		}
		else if (hour && minute && second)
			return g_strdup_printf ("%i:%02i:%02i",
						 hour,
						 minute,
						 second);
		else if (hour && minute)
			return g_strdup_printf ("%i:%02i", hour, minute);
	}

	if (with_unit) {
		if (!second)
			return g_strdup_printf (_("%i min"), minute);
		else
			return g_strdup_printf (_("%i:%02i min"), minute, second);
	}
	else
		return g_strdup_printf ("%i:%02i", minute, second);
}

gchar *
brasero_utils_get_time_string_from_size (gint64 size,
					 gboolean with_unit,
					 gboolean round)
{
	int second = 0;
	int minute = 0;
	gint64 time = 0.0;

	time = NAUTILUS_BURN_DRIVE_SIZE_TO_TIME (size);
	minute = time / 60;
	if (!round)
		second = time % 60;

	if (minute > 120) {
		int hour;

		hour = minute / 60;
		minute = minute % 60;

		if (with_unit == TRUE)
			return g_strdup_printf (ngettext ("%d:%02i hour", "%i:%02i hours", hour), hour, minute);
		else
			return g_strdup_printf ("%i:%02i:%02i", hour,
						minute, second);
	}
	else if (with_unit == TRUE) {
		if (!second)
			return g_strdup_printf (_("%i min"), minute);
		else
			return g_strdup_printf (_("%i:%02i min"), minute, second);
	}
	else
		return g_strdup_printf ("%i:%02i", minute, second);
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
	const char *units[] = { "", N_("KiB"), N_("MiB"), N_("GiB") };

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

	if (with_unit == TRUE && unit != BRASERO_UTILS_NO_UNIT) {
		if (remain)
			return g_strdup_printf ("%i.%i %s",
						size,
						remain,
						_(units[unit]));
		else
			return g_strdup_printf ("%i %s",
						size,
						_(units[unit]));
	}
	else if (remain)
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
	gtk_box_pack_end (GTK_BOX (vbox_main), hbox, TRUE, TRUE, 6);

	label = gtk_label_new ("\t");
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	vbox_prop = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox_prop);
	gtk_box_pack_start (GTK_BOX (hbox), vbox_prop, TRUE, TRUE, 0);

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

static gboolean
brasero_utils_empty_dir (const gchar *uri, GnomeVFSFileInfo * info)
{
	GnomeVFSDirectoryHandle *handle;
	gchar *file_uri, *name;

	/* NOTE : we don't follow uris as certain files are linked by content-data */
	if (gnome_vfs_directory_open
	    (&handle, uri, GNOME_VFS_FILE_INFO_DEFAULT) != GNOME_VFS_OK) {
		g_warning ("Can't open directory %s\n", uri);
		return FALSE;
	}
	gnome_vfs_file_info_clear (info);
	while (gnome_vfs_directory_read_next (handle, info) ==
	       GNOME_VFS_OK) {
		if (!strcmp (info->name, "..")
		    || *info->name == '.')
			continue;

		name = gnome_vfs_escape_string (info->name);
		file_uri = g_strconcat (uri, "/", name, NULL);
		g_free (name);

		if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
			brasero_utils_empty_dir (file_uri, info);
			g_free (file_uri);
		} else if (gnome_vfs_unlink (file_uri) != GNOME_VFS_OK)
			g_warning ("Cannot remove file %s\n", file_uri);

		gnome_vfs_file_info_clear (info);
	}
	gnome_vfs_directory_close (handle);
	if (gnome_vfs_remove_directory (uri) != GNOME_VFS_OK) {
		g_warning ("Cannot remove directory %s\n", uri);
		return FALSE;
	}

	return TRUE;
}

gboolean
brasero_utils_remove (const gchar *uri)
{
	GnomeVFSFileInfo *info;
	gboolean result = TRUE;

	if (!uri)
		return TRUE;

	info = gnome_vfs_file_info_new ();
	/* NOTE : we don't follow uris as certain files are simply linked by content-data */
	gnome_vfs_get_file_info (uri, info, GNOME_VFS_FILE_INFO_DEFAULT);

	if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
		result = brasero_utils_empty_dir (uri, info);
	else if (gnome_vfs_unlink (uri) != GNOME_VFS_OK) {
		g_warning ("Cannot remove file %s\n", uri);
		result = FALSE;
	}

	gnome_vfs_file_info_clear (info);
	gnome_vfs_file_info_unref (info);

	return result;
}

gchar *
brasero_utils_escape_string (const gchar *text)
{
	gchar *ptr, *result;
	gint len = 1;

	ptr = (gchar *) text;
	while (*ptr != '\0') {
		if (*ptr == '\\' || *ptr == '=')
			len++;

		len++;
		ptr++;
	}

	result = g_new (gchar, len);

	ptr = result;
	while (*text != '\0') {
		if (*text == '\\' || *text == '=') {
			*ptr = '\\';
			ptr++;
		}

		*ptr = *text;
		ptr++;
		text++;
	}

	*ptr = '\0';

	return result;
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
	gchar *uri;
	gchar *mime;
	GList *uris;
	GSList *item;
	GnomeVFSResult result;
	GnomeVFSMimeApplication *application;

	for (item = list; item; item = item->next) {
		uri = item->data;

		mime = gnome_vfs_get_mime_type (uri);
		if (!mime)
			continue;

		application = gnome_vfs_mime_get_default_application (mime);
		g_free (mime);

		if (!application) {
			GtkWidget *dialog;
			GtkWidget *toplevel;

			toplevel = gtk_widget_get_toplevel (GTK_WIDGET (widget));
			dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
							 GTK_DIALOG_DESTROY_WITH_PARENT|
							 GTK_DIALOG_MODAL,
							 GTK_MESSAGE_ERROR,
							 GTK_BUTTONS_CLOSE,
							 _("This file can't be opened:"));

			gtk_window_set_title (GTK_WINDOW (dialog), _("File error"));

			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
								  _("there is no application defined for this file."));

			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
			continue;
		}

		uris = g_list_prepend (NULL, uri);
		result = gnome_vfs_mime_application_launch (application, uris);
		g_list_free (uris);

		if (result != GNOME_VFS_OK) {
			GtkWidget *dialog;
			GtkWidget *toplevel;

			toplevel = gtk_widget_get_toplevel (GTK_WIDGET (widget));
			dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
							 GTK_DIALOG_DESTROY_WITH_PARENT |
							 GTK_DIALOG_MODAL,
							 GTK_MESSAGE_ERROR,
							 GTK_BUTTONS_CLOSE,
							 _("File can't be opened:"));

			gtk_window_set_title (GTK_WINDOW (dialog), _("File error"));

			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
								 _("application %s can't be started."),
								 application->name);

			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
		}

		gnome_vfs_mime_application_free (application);
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
