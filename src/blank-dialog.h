/***************************************************************************
 *            blank-dialog.h
 *
 *  mar jui 26 12:23:01 2005
 *  Copyright  2005  Philippe Rouquier
 *  brasero-app@wanadoo.fr
 ***************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef BLANK_DIALOG_H
#define BLANK_DIALOG_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtkwidget.h>

#include "brasero-tool-dialog.h"

G_BEGIN_DECLS
#define BRASERO_TYPE_BLANK_DIALOG         (brasero_blank_dialog_get_type ())
#define BRASERO_BLANK_DIALOG(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_BLANK_DIALOG, BraseroBlankDialog))
#define BRASERO_BLANK_DIALOG_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_BLANK_DIALOG, BraseroBlankDialogClass))
#define BRASERO_IS_BLANK_DIALOG(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_BLANK_DIALOG))
#define BRASERO_IS_BLANK_DIALOG_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_BLANK_DIALOG))
#define BRASERO_BLANK_DIALOG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_BLANK_DIALOG, BraseroBlankDialogClass))
typedef struct BraseroBlankDialogPrivate BraseroBlankDialogPrivate;

typedef struct {
	BraseroToolDialog parent;
	BraseroBlankDialogPrivate *priv;
} BraseroBlankDialog;

typedef struct {
	BraseroToolDialogClass parent_class;
} BraseroBlankDialogClass;

GType brasero_blank_dialog_get_type ();
GtkWidget *brasero_blank_dialog_new ();

#endif				/* BLANK_DIALOG_H */
