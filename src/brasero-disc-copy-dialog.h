/***************************************************************************
 *            disc-copy-dialog.h
 *
 *  ven jui 15 16:02:10 2005
 *  Copyright  2005  Philippe Rouquier
 *  brasero-app@wanadoo.fr
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

#ifndef DISC_COPY_DIALOG_H
#define DISC_COPY_DIALOG_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtkdialog.h>

#include "burn-session.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_DISC_COPY_DIALOG         (brasero_disc_copy_dialog_get_type ())
#define BRASERO_DISC_COPY_DIALOG(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_DISC_COPY_DIALOG, BraseroDiscCopyDialog))
#define BRASERO_DISC_COPY_DIALOG_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_DISC_COPY_DIALOG, BraseroDiscCopyDialogClass))
#define BRASERO_IS_DISC_COPY_DIALOG(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_DISC_COPY_DIALOG))
#define BRASERO_IS_DISC_COPY_DIALOG_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_DISC_COPY_DIALOG))
#define BRASERO_DISC_COPY_DIALOG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_DISC_COPY_DIALOG, BraseroDiscCopyDialogClass))

typedef struct {
	GtkDialog parent;
} BraseroDiscCopyDialog;

typedef struct {
	GtkDialogClass parent_class;
} BraseroDiscCopyDialogClass;

GType brasero_disc_copy_dialog_get_type ();

GtkWidget *
brasero_disc_copy_dialog_new ();

BraseroBurnSession *
brasero_disc_copy_dialog_get_session (BraseroDiscCopyDialog *self);

G_END_DECLS

#endif				/* DISC_COPY_DIALOG_H */
