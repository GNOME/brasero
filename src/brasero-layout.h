/***************************************************************************
 *            brasero-layout.h
 *
 *  mer mai 24 15:14:42 2006
 *  Copyright  2006  Rouquier Philippe
 *  brasero-app@wanadoo.fr
 ***************************************************************************/

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

#ifndef BRASERO_LAYOUT_H
#define BRASERO_LAYOUT_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_LAYOUT         (brasero_layout_get_type ())
#define BRASERO_LAYOUT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_LAYOUT, BraseroLayout))
#define BRASERO_LAYOUT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_LAYOUT, BraseroLayoutClass))
#define BRASERO_IS_LAYOUT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_LAYOUT))
#define BRASERO_IS_LAYOUT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_LAYOUT))
#define BRASERO_LAYOUT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_LAYOUT, BraseroLayoutClass))

typedef struct BraseroLayoutPrivate BraseroLayoutPrivate;

typedef enum {
	BRASERO_LAYOUT_NONE		= 0,
	BRASERO_LAYOUT_AUDIO		= 1,
	BRASERO_LAYOUT_DATA		= 1 << 1,
	BRASERO_LAYOUT_VIDEO		= 1 << 2
} BraseroLayoutType;

typedef struct {
	GtkPaned parent;
	BraseroLayoutPrivate *priv;
} BraseroLayout;

typedef struct {
	GtkPanedClass parent_class;
} BraseroLayoutClass;

GType brasero_layout_get_type (void);
GtkWidget *brasero_layout_new (void);

void
brasero_layout_add_project (BraseroLayout *layout,
			    GtkWidget *project);
void
brasero_layout_add_preview (BraseroLayout*layout,
			    GtkWidget *preview);

void
brasero_layout_add_source (BraseroLayout *layout,
			   GtkWidget *child,
			   const gchar *id,
			   const gchar *subtitle,
			   const gchar *icon_name,
			   BraseroLayoutType types);
void
brasero_layout_load (BraseroLayout *layout,
		     BraseroLayoutType type);

void
brasero_layout_register_ui (BraseroLayout *layout,
			    GtkUIManager *manager);

G_END_DECLS

#endif /* BRASERO_LAYOUT_H */
