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

#define COVER_HEIGHT_FRONT_MM		120.0
#define COVER_WIDTH_FRONT_MM		120.0
#define COVER_WIDTH_FRONT_INCH		4.724
#define COVER_HEIGHT_FRONT_INCH		4.724

#define COVER_HEIGHT_BACK_MM		118.0
#define COVER_WIDTH_BACK_MM		150.0
#define COVER_HEIGHT_BACK_INCH		4.646
#define COVER_WIDTH_BACK_INCH		5.906

#define COVER_HEIGHT_SIDE_MM		COVER_HEIGHT_BACK_MM
#define COVER_WIDTH_SIDE_MM		6.0
#define COVER_HEIGHT_SIDE_INCH		COVER_HEIGHT_BACK_INCH
#define COVER_WIDTH_SIDE_INCH		0.236

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
brasero_jacket_view_add_default_tag (BraseroJacketView *self,
				     GtkTextTag *tag);

void
brasero_jacket_view_set_side (BraseroJacketView *view,
			      BraseroJacketSide side);

void
brasero_jacket_view_set_color (BraseroJacketView *view,
			       BraseroJacketColorStyle style,
			       GdkColor *color,
			       GdkColor *color2);

const gchar *
brasero_jacket_view_get_image (BraseroJacketView *self);

const gchar *
brasero_jacket_view_set_image (BraseroJacketView *view,
			       BraseroJacketImageStyle style,
			       const gchar *path);

void
brasero_jacket_view_configure_background (BraseroJacketView *view);

guint
brasero_jacket_view_print (BraseroJacketView *view,
			   GtkPrintContext *context,
			   gdouble x,
			   gdouble y);

GtkTextBuffer *
brasero_jacket_view_get_active_buffer (BraseroJacketView *view);

GtkTextBuffer *
brasero_jacket_view_get_body_buffer (BraseroJacketView *view);

GtkTextBuffer *
brasero_jacket_view_get_side_buffer (BraseroJacketView *view);

GtkTextAttributes *
brasero_jacket_view_get_attributes (BraseroJacketView *view,
				    GtkTextIter *iter);

G_END_DECLS

#endif /* _BRASERO_JACKET_VIEW_H_ */
