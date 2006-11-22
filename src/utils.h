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
 *            gstreamer.h
 *
 *  Wed May 18 16:58:16 2005
 *  Copyright  2005  Philippe Rouquier
 *  <brasero-app@wanadoo.fr>
 ****************************************************************************/


#include <sys/types.h>
#include <unistd.h>

#include <glib.h>
#include <gtk/gtk.h>

#include <libgnomevfs/gnome-vfs.h>

#include "burn-basics.h"

#ifndef _UTILS_H
#define _UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#define BRASERO_STOCK_PLAYLIST	"brasero-playlist-icon"

#define BRASERO_ERROR brasero_error_quark()

typedef char *(*BraseroFormatTime) (double time,
				    gboolean with_unit,
				    gboolean round);

typedef enum {
	BRASERO_ERROR_NONE,
	BRASERO_ERROR_GENERAL
} BraSeroErrors;

void brasero_utils_init (void);

inline gboolean brasero_utils_is_gid_in_groups (gid_t gid);

GQuark brasero_error_quark (void);

gchar *brasero_utils_get_time_string (gint64 time,
				      gboolean with_unit,
				      gboolean round);

gchar *brasero_utils_get_time_string_from_size (gint64 time,
						gboolean with_unit,
						gboolean round);

gchar *brasero_utils_get_size_string (gint64 size,
				      gboolean with_unit,
				      gboolean round);
gchar *
brasero_utils_get_sectors_string (gint64 sectors,
				  gboolean time_format,
				  gboolean with_unit,
				  gboolean round);

GdkPixbuf *brasero_utils_get_icon_for_mime (const gchar *mime, gint size);
GdkPixbuf *brasero_utils_get_icon (const gchar *name, gint size);

GtkWidget *brasero_utils_pack_properties (const gchar *title, ...);
GtkWidget *brasero_utils_pack_properties_list (const gchar *title, GSList *list);

GtkWidget *brasero_utils_make_button (const gchar *text, const gchar *stock, const gchar *theme);

gboolean brasero_utils_remove (const gchar *uri);

gchar *brasero_utils_escape_string (const gchar *text);

gchar *brasero_utils_check_for_parent_symlink (const gchar *uri);

gboolean brasero_utils_get_symlink_target (const gchar *uri,
					   GnomeVFSFileInfo *info,
					   GnomeVFSFileInfoOptions flags);

gboolean
brasero_utils_str_equal_64 (gconstpointer v1,
			    gconstpointer v2);

guint
brasero_utils_str_hash_64 (gconstpointer v);

void
brasero_utils_launch_app (GtkWidget *widget,
			  GSList *list);

void
brasero_utils_show_menu (int nb_selected,
			 GtkUIManager *manager,
			 GdkEventButton *event);
GtkWidget *
brasero_utils_get_use_info_notebook (void);

gchar*
brasero_utils_validate_utf8 (const gchar *name);

#ifdef __cplusplus
}
#endif
#endif				/* _UTILS_H */
