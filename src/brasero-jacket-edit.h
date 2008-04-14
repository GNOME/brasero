/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Rouquier Philippe 2008 <bonfire-app@wanadoo.fr>
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

#ifndef _BRASERO_JACKET_EDIT_H_
#define _BRASERO_JACKET_EDIT_H_

#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_JACKET_EDIT             (brasero_jacket_edit_get_type ())
#define BRASERO_JACKET_EDIT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_JACKET_EDIT, BraseroJacketEdit))
#define BRASERO_JACKET_EDIT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_JACKET_EDIT, BraseroJacketEditClass))
#define BRASERO_IS_JACKET_EDIT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_JACKET_EDIT))
#define BRASERO_IS_JACKET_EDIT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_JACKET_EDIT))
#define BRASERO_JACKET_EDIT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_JACKET_EDIT, BraseroJacketEditClass))

typedef struct _BraseroJacketEditClass BraseroJacketEditClass;
typedef struct _BraseroJacketEdit BraseroJacketEdit;

struct _BraseroJacketEditClass
{
	GtkVBoxClass parent_class;
};

struct _BraseroJacketEdit
{
	GtkVBox parent_instance;
};

GType brasero_jacket_edit_get_type (void) G_GNUC_CONST;

GtkWidget *
brasero_jacket_edit_new (void);

GtkWidget *
brasero_jacket_edit_dialog_new (GtkWidget *toplevel,
				GtkWidget **dialog);

void
brasero_jacket_edit_set_audio_tracks (BraseroJacketEdit *self,
				      const gchar *label,
				      GSList *tracks);

G_END_DECLS

#endif /* _BRASERO_JACKET_EDIT_H_ */
