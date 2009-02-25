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
 *            utils.h
 *
 *  Wed May 18 16:58:16 2005
 *  Copyright  2005  Philippe Rouquier
 *  <brasero-app@wanadoo.fr>
 ****************************************************************************/


#include <sys/types.h>
#include <unistd.h>

#include <glib.h>
#include <gtk/gtk.h>

#include "burn-basics.h"

#ifndef _UTILS_H
#define _UTILS_H

G_BEGIN_DECLS

#define BRASERO_ERROR brasero_error_quark()

typedef char *(*BraseroFormatTime) (double time,
				    gboolean with_unit,
				    gboolean round);

typedef enum {
	BRASERO_ERROR_NONE,
	BRASERO_ERROR_GENERAL,
	BRASERO_ERROR_SYMLINK_LOOP
} BraseroErrors;

#define BRASERO_DEFAULT_ICON		"text-x-preview"

void brasero_utils_init (void);

GQuark brasero_error_quark (void);

gchar *
brasero_utils_register_string (const gchar *string);
void
brasero_utils_unregister_string (const gchar *string);

GtkWidget *
brasero_utils_pack_properties (const gchar *title, ...);
GtkWidget *
brasero_utils_pack_properties_list (const gchar *title, GSList *list);
GtkWidget *
brasero_utils_make_button (const gchar *text,
			   const gchar *stock,
			   const gchar *theme,
			   GtkIconSize size);

void
brasero_utils_launch_app (GtkWidget *widget,
			  GSList *list);

gchar*
brasero_utils_validate_utf8 (const gchar *name);

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

#endif				/* _UTILS_H */
