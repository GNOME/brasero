/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2007-2008 <bonfire-app@wanadoo.fr>
 * 
 *  Brasero is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#ifndef _BRASERO_SPLIT_DIALOG_H_
#define _BRASERO_SPLIT_DIALOG_H_

#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

struct _BraseroAudioSlice {
	gint64 start;
	gint64 end;
};
typedef struct _BraseroAudioSlice BraseroAudioSlice;

#define BRASERO_TYPE_SPLIT_DIALOG             (brasero_split_dialog_get_type ())
#define BRASERO_SPLIT_DIALOG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_SPLIT_DIALOG, BraseroSplitDialog))
#define BRASERO_SPLIT_DIALOG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_SPLIT_DIALOG, BraseroSplitDialogClass))
#define BRASERO_IS_SPLIT_DIALOG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_SPLIT_DIALOG))
#define BRASERO_IS_SPLIT_DIALOG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_SPLIT_DIALOG))
#define BRASERO_SPLIT_DIALOG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_SPLIT_DIALOG, BraseroSplitDialogClass))

typedef struct _BraseroSplitDialogClass BraseroSplitDialogClass;
typedef struct _BraseroSplitDialog BraseroSplitDialog;

struct _BraseroSplitDialogClass
{
	GtkDialogClass parent_class;
};

struct _BraseroSplitDialog
{
	GtkDialog parent_instance;
};

GType brasero_split_dialog_get_type (void) G_GNUC_CONST;

GtkWidget *
brasero_split_dialog_new (void);

void
brasero_split_dialog_set_uri (BraseroSplitDialog *dialog,
			      const gchar *uri,
                              const gchar *title,
                              const gchar *artist);
void
brasero_split_dialog_set_boundaries (BraseroSplitDialog *dialog,
				     gint64 start,
				     gint64 end);

GSList *
brasero_split_dialog_get_slices (BraseroSplitDialog *self);

G_END_DECLS

#endif /* _BRASERO_SPLIT_DIALOG_H_ */
