/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2008 <bonfire-app@wanadoo.fr>
 * 
 * brasero is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * brasero is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _BRASERO_JACKET_BACKGROUND_H_
#define _BRASERO_JACKET_BACKGROUND_H_

#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum {
	BRASERO_JACKET_IMAGE_CENTER	= 0,
	BRASERO_JACKET_IMAGE_TILE,
	BRASERO_JACKET_IMAGE_STRETCH
} BraseroJacketImageStyle;

typedef enum {
	BRASERO_JACKET_COLOR_SOLID	= 0,
	BRASERO_JACKET_COLOR_HGRADIENT	= 1,
	BRASERO_JACKET_COLOR_VGRADIENT 	= 2
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
