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

#ifndef _BRASERO_JACKET_FONT_H_
#define _BRASERO_JACKET_FONT_H_

#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_JACKET_FONT             (brasero_jacket_font_get_type ())
#define BRASERO_JACKET_FONT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_JACKET_FONT, BraseroJacketFont))
#define BRASERO_JACKET_FONT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_JACKET_FONT, BraseroJacketFontClass))
#define BRASERO_IS_JACKET_FONT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_JACKET_FONT))
#define BRASERO_IS_JACKET_FONT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_JACKET_FONT))
#define BRASERO_JACKET_FONT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_JACKET_FONT, BraseroJacketFontClass))

typedef struct _BraseroJacketFontClass BraseroJacketFontClass;
typedef struct _BraseroJacketFont BraseroJacketFont;

struct _BraseroJacketFontClass
{
	GtkHBoxClass parent_class;
};

struct _BraseroJacketFont
{
	GtkHBox parent_instance;
};

GType brasero_jacket_font_get_type (void) G_GNUC_CONST;

GtkWidget *
brasero_jacket_font_new (void);

void
brasero_jacket_font_set_name (BraseroJacketFont *font,
			      const gchar *name);

gchar *
brasero_jacket_font_get_name (BraseroJacketFont *font);

G_END_DECLS

#endif /* _BRASERO_JACKET_FONT_H_ */
