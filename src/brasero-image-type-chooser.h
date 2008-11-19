/***************************************************************************
 *            brasero-image-type-chooser.h
 *
 *  mar oct  3 18:40:02 2006
 *  Copyright  2006  Philippe Rouquier
 *  bonfire-app@wanadoo.fr
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

#ifndef BRASERO_IMAGE_TYPE_CHOOSER_H
#define BRASERO_IMAGE_TYPE_CHOOSER_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_IMAGE_TYPE_CHOOSER         (brasero_image_type_chooser_get_type ())
#define BRASERO_IMAGE_TYPE_CHOOSER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_IMAGE_TYPE_CHOOSER, BraseroImageTypeChooser))
#define BRASERO_IMAGE_TYPE_CHOOSER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_IMAGE_TYPE_CHOOSER, BraseroImageTypeChooserClass))
#define BRASERO_IS_IMAGE_TYPE_CHOOSER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_IMAGE_TYPE_CHOOSER))
#define BRASERO_IS_IMAGE_TYPE_CHOOSER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_IMAGE_TYPE_CHOOSER))
#define BRASERO_IMAGE_TYPE_CHOOSER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_IMAGE_TYPE_CHOOSER, BraseroImageTypeChooserClass))

typedef struct _BraseroImageTypeChooser BraseroImageTypeChooser;
typedef struct _BraseroImageTypeChooserPrivate BraseroImageTypeChooserPrivate;
typedef struct _BraseroImageTypeChooserClass BraseroImageTypeChooserClass;

struct _BraseroImageTypeChooser {
	GtkHBox parent;
};

struct _BraseroImageTypeChooserClass {
	GtkHBoxClass parent_class;
};

GType brasero_image_type_chooser_get_type ();
GtkWidget *brasero_image_type_chooser_new ();

void
brasero_image_type_chooser_set_formats (BraseroImageTypeChooser *self,
				        BraseroImageFormat formats);
void
brasero_image_type_chooser_set_format (BraseroImageTypeChooser *self,
				       BraseroImageFormat format);
void
brasero_image_type_chooser_get_format (BraseroImageTypeChooser *self,
				       BraseroImageFormat *format);

G_END_DECLS

#endif /* BRASERO_IMAGE_TYPE_CHOOSER_H */
