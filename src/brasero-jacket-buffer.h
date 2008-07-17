/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2005-2008 <bonfire-app@wanadoo.fr>
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

#ifndef _BRASERO_JACKET_BUFFER_H_
#define _BRASERO_JACKET_BUFFER_H_

#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_JACKET_BUFFER             (brasero_jacket_buffer_get_type ())
#define BRASERO_JACKET_BUFFER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_JACKET_BUFFER, BraseroJacketBuffer))
#define BRASERO_JACKET_BUFFER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_JACKET_BUFFER, BraseroJacketBufferClass))
#define BRASERO_IS_JACKET_BUFFER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_JACKET_BUFFER))
#define BRASERO_IS_JACKET_BUFFER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_JACKET_BUFFER))
#define BRASERO_JACKET_BUFFER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_JACKET_BUFFER, BraseroJacketBufferClass))

typedef struct _BraseroJacketBufferClass BraseroJacketBufferClass;
typedef struct _BraseroJacketBuffer BraseroJacketBuffer;

struct _BraseroJacketBufferClass
{
	GtkTextBufferClass parent_class;
};

struct _BraseroJacketBuffer
{
	GtkTextBuffer parent_instance;
};

GType brasero_jacket_buffer_get_type (void) G_GNUC_CONST;

BraseroJacketBuffer *
brasero_jacket_buffer_new (void);

void
brasero_jacket_buffer_add_default_tag (BraseroJacketBuffer *buffer,
				       GtkTextTag *tag);

void
brasero_jacket_buffer_set_default_text (BraseroJacketBuffer *self,
					const gchar *default_text);

void
brasero_jacket_buffer_show_default_text (BraseroJacketBuffer *self,
					 gboolean show);

gchar *
brasero_jacket_buffer_get_text (BraseroJacketBuffer *self,
				GtkTextIter *start,
				GtkTextIter *end,
				gboolean invisible_chars,
				gboolean get_default_text);

G_END_DECLS

#endif /* _BRASERO_JACKET_BUFFER_H_ */
