/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-burn
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-burn is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-burn authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-burn. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-burn is distributed in the hope that it will be useful,
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
	GtkBox parent;
};

struct _BraseroImageTypeChooserClass {
	GtkBoxClass parent_class;
};

GType brasero_image_type_chooser_get_type (void);
GtkWidget *brasero_image_type_chooser_new (void);

guint
brasero_image_type_chooser_set_formats (BraseroImageTypeChooser *self,
				        BraseroImageFormat formats,
                                        gboolean show_autodetect,
                                        gboolean is_video);
void
brasero_image_type_chooser_set_format (BraseroImageTypeChooser *self,
				       BraseroImageFormat format);
void
brasero_image_type_chooser_get_format (BraseroImageTypeChooser *self,
				       BraseroImageFormat *format);
gboolean
brasero_image_type_chooser_get_VCD_type (BraseroImageTypeChooser *chooser);

void
brasero_image_type_chooser_set_VCD_type (BraseroImageTypeChooser *chooser,
                                         gboolean is_svcd);

G_END_DECLS

#endif /* BRASERO_IMAGE_TYPE_CHOOSER_H */
