/***************************************************************************
*            mime_filter.h
*
*  dim mai 22 18:39:03 2005
*  Copyright  2005  Philippe Rouquier
*  brasero-app@wanadoo.fr
****************************************************************************/

/*
 *  Brasero is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Brasero is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifndef MIME_FILTER_H
#define MIME_FILTER_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS
#define BRASERO_TYPE_MIME_FILTER         (brasero_mime_filter_get_type ())
#define BRASERO_MIME_FILTER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_MIME_FILTER, BraseroMimeFilter))
#define BRASERO_MIME_FILTER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_MIME_FILTER, BraseroMimeFilterClass))
#define BRASERO_IS_MIME_FILTER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_MIME_FILTER))
#define BRASERO_IS_MIME_FILTER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_MIME_FILTER))
#define BRASERO_MIME_FILTER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_MIME_FILTER, BraseroMimeFilterClass))
typedef struct BraseroMimeFilterPrivate BraseroMimeFilterPrivate;

typedef struct {
	GtkHBox parent;

	/* Public */
	GtkWidget *combo;

	/* Private */
	BraseroMimeFilterPrivate *priv;
} BraseroMimeFilter;


typedef struct {
	GtkHBoxClass parent_class;
	/* Signal Functions */
	void (*changed) (BraseroMimeFilter * filter);
} BraseroMimeFilterClass;

GType brasero_mime_filter_get_type ();
GtkWidget *brasero_mime_filter_new ();

void brasero_mime_filter_add_filter (BraseroMimeFilter * filter,
				     GtkFileFilter * item);
void brasero_mime_filter_add_mime (BraseroMimeFilter * filter,
				   const char *mime);
void brasero_mime_filter_unref_mime (BraseroMimeFilter * filter,
				     char *mime);
gboolean brasero_mime_filter_filter (BraseroMimeFilter * filter,
				     char *filename, char *uri,
				     char *display_name, char *mime_type);

G_END_DECLS

#endif				/* MIME_FILTER_H */
