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

#ifndef _BRASERO_JACKET_BACKGROUND_H_
#define _BRASERO_JACKET_BACKGROUND_H_

#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum {
	BRASERO_JACKET_IMAGE_NONE	= 0,
	BRASERO_JACKET_IMAGE_CENTER	= 1,
	BRASERO_JACKET_IMAGE_TILE,
	BRASERO_JACKET_IMAGE_STRETCH
} BraseroJacketImageStyle;

typedef enum {
	BRASERO_JACKET_COLOR_NONE	= 0,
	BRASERO_JACKET_COLOR_SOLID	= 1,
	BRASERO_JACKET_COLOR_HGRADIENT,
	BRASERO_JACKET_COLOR_VGRADIENT
} BraseroJacketColorStyle;

#define BRASERO_TYPE_JACKET_BACKGROUND             (brasero_jacket_background_get_type ())
#define BRASERO_JACKET_BACKGROUND(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_JACKET_BACKGROUND, BraseroJacketBackground))
#define BRASERO_JACKET_BACKGROUND_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_JACKET_BACKGROUND, BraseroJacketBackgroundClass))
#define BRASERO_IS_JACKET_BACKGROUND(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_JACKET_BACKGROUND))
#define BRASERO_IS_JACKET_BACKGROUND_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_JACKET_BACKGROUND))
#define BRASERO_JACKET_BACKGROUND_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_JACKET_BACKGROUND, BraseroJacketBackgroundClass))

typedef struct _BraseroJacketBackgroundClass BraseroJacketBackgroundClass;
typedef struct _BraseroJacketBackground BraseroJacketBackground;

struct _BraseroJacketBackgroundClass
{
	GtkDialogClass parent_class;
};

struct _BraseroJacketBackground
{
	GtkDialog parent_instance;
};

GType brasero_jacket_background_get_type (void) G_GNUC_CONST;

GtkWidget *
brasero_jacket_background_new (void);

BraseroJacketColorStyle
brasero_jacket_background_get_color_style (BraseroJacketBackground *back);

BraseroJacketImageStyle
brasero_jacket_background_get_image_style (BraseroJacketBackground *back);

gchar *
brasero_jacket_background_get_image_path (BraseroJacketBackground *back);

void
brasero_jacket_background_get_color (BraseroJacketBackground *back,
				     GdkColor *color,
				     GdkColor *color2);

void
brasero_jacket_background_set_color_style (BraseroJacketBackground *back,
					   BraseroJacketColorStyle style);

void
brasero_jacket_background_set_image_style (BraseroJacketBackground *back,
					   BraseroJacketImageStyle style);

void
brasero_jacket_background_set_image_path (BraseroJacketBackground *back,
					  const gchar *path);

void
brasero_jacket_background_set_color (BraseroJacketBackground *back,
				     GdkColor *color,
				     GdkColor *color2);
G_END_DECLS

#endif /* _BRASERO_JACKET_BACKGROUND_H_ */
