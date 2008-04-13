/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2008 <bonfire-app@wanadoo.fr>
 * 
 * brasero is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * brasero is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with brasero.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifndef _BRASERO_JACKET_VIEW_H_
#define _BRASERO_JACKET_VIEW_H_

#include <glib-object.h>

#include <gtk/gtk.h>

#include "brasero-jacket-background.h"

G_BEGIN_DECLS

typedef enum {
	BRASERO_JACKET_FRONT		= 0,
	BRASERO_JACKET_BACK		= 1,
} BraseroJacketSide;

#define COVER_HEIGHT_FRONT_MM		120
#define COVER_WIDTH_FRONT_MM		120
#define COVER_WIDTH_FRONT_INCH		4.724
#define COVER_HEIGHT_FRONT_INCH		4.724

#define COVER_HEIGHT_BACK_MM		117.5
#define COVER_WIDTH_BACK_MM		152
#define COVER_HEIGHT_BACK_INCH		4.646
#define COVER_WIDTH_BACK_INCH		5.984

#define COVER_HEIGHT_SIDE_MM		117.5
#define COVER_WIDTH_SIDE_MM		6
#define COVER_HEIGHT_SIDE_INCH		4.625
#define COVER_WIDTH_SIDE_INCH		0.235

#define COVER_TEXT_MARGIN		/*1.*/0.03 //0.079

#define BRASERO_TYPE_JACKET_VIEW             (brasero_jacket_view_get_type ())
#define BRASERO_JACKET_VIEW(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_JACKET_VIEW, BraseroJacketView))
#define BRASERO_JACKET_VIEW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_JACKET_VIEW, BraseroJacketViewClass))
#define BRASERO_IS_JACKET_VIEW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_JACKET_VIEW))
#define BRASERO_IS_JACKET_VIEW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_JACKET_VIEW))
#define BRASERO_JACKET_VIEW_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_JACKET_VIEW, BraseroJacketViewClass))

typedef struct _BraseroJacketViewClass BraseroJacketViewClass;
typedef struct _BraseroJacketView BraseroJacketView;

struct _BraseroJacketViewClass
{
	GtkContainerClass parent_class;
};

struct _BraseroJacketView
{
	GtkContainer parent_instance;
};

GType brasero_jacket_view_get_type (void) G_GNUC_CONST;

GtkWidget *
brasero_jacket_view_new (void);

void
brasero_jacket_view_set_side (BraseroJacketView *view,
			      BraseroJacketSide side);

void
brasero_jacket_view_set_image_style (BraseroJacketView *view,
				     BraseroJacketImageStyle style);

void
brasero_jacket_view_set_color_background (BraseroJacketView *view,
					  GdkColor *color,
					  GdkColor *color2);
void
brasero_jacket_view_set_color_style (BraseroJacketView *view,
				     BraseroJacketColorStyle style);

const gchar *
brasero_jacket_view_get_image (BraseroJacketView *self);

const gchar *
brasero_jacket_view_set_image (BraseroJacketView *view,
			       const gchar *path);

guint
brasero_jacket_view_print (BraseroJacketView *view,
			   GtkPrintContext *context,
			   guint x,
			   guint y);

cairo_surface_t *
brasero_jacket_view_snapshot (BraseroJacketView *self);

GtkTextBuffer *
brasero_jacket_view_get_active_buffer (BraseroJacketView *view);

GtkTextAttributes *
brasero_jacket_view_get_default_attributes (BraseroJacketView *view);

G_END_DECLS

#endif /* _BRASERO_JACKET_VIEW_H_ */
