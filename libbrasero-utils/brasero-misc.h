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

#include <sys/types.h>
#include <unistd.h>

#include <glib.h>
#include <gtk/gtk.h>

#ifndef _BRASERO_MISC_H
#define _BRASERO_MISC_H

G_BEGIN_DECLS

GQuark brasero_utils_error_quark (void);

#define BRASERO_UTILS_ERROR				brasero_utils_error_quark()

typedef enum {
	BRASERO_UTILS_ERROR_NONE,
	BRASERO_UTILS_ERROR_GENERAL,
	BRASERO_UTILS_ERROR_SYMLINK_LOOP
} BraseroUtilsErrors;

void
brasero_utils_set_use_debug (gboolean active);

void
brasero_utils_debug_message (const gchar *location,
			     const gchar *format,
			     ...) G_GNUC_PRINTF (2, 3);

#define BRASERO_UTILS_LOG(format, ...)						\
	brasero_utils_debug_message (G_STRLOC,					\
				     format,					\
				     ##__VA_ARGS__);


#define BRASERO_GET_BASENAME_FOR_DISPLAY(uri, name)				\
{										\
    	gchar *escaped_basename;						\
	escaped_basename = g_path_get_basename (uri);				\
    	name = g_uri_unescape_string (escaped_basename, NULL);			\
	g_free (escaped_basename);						\
}

void
brasero_utils_init (void);

GOptionGroup *
brasero_utils_get_option_group (void);

gchar *
brasero_utils_get_uri_name (const gchar *uri);
gchar*
brasero_utils_validate_utf8 (const gchar *name);

gchar *
brasero_utils_register_string (const gchar *string);
void
brasero_utils_unregister_string (const gchar *string);

GtkWidget *
brasero_utils_properties_get_label (GtkWidget *widget);

GtkWidget *
brasero_utils_pack_properties (const gchar *title, ...);
GtkWidget *
brasero_utils_pack_properties_list (const gchar *title, GSList *list);
GtkWidget *
brasero_utils_make_button (const gchar *text,
			   const gchar *stock,
			   const gchar *theme,
			   GtkIconSize size);

GtkWidget *
brasero_utils_create_message_dialog (GtkWidget *parent,
				     const gchar *primary_message,
				     const gchar *secondary_message,
				     GtkMessageType type);

void
brasero_utils_message_dialog (GtkWidget *parent,
			      const gchar *primary_message,
			      const gchar *secondary_message,
			      GtkMessageType type);

G_END_DECLS

#endif				/* _MISC_H */
