/***************************************************************************
 *            brasero-sum-dialog.h
 *
 *  ven sep  1 19:35:13 2006
 *  Copyright  2006  Rouquier Philippe
 *  bonfire-app@wanadoo.fr
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

#ifndef BRASERO_SUM_DIALOG_H
#define BRASERO_SUM_DIALOG_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtkwidget.h>

#include "brasero-tool-dialog.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_SUM_DIALOG         (brasero_sum_dialog_get_type ())
#define BRASERO_SUM_DIALOG(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_SUM_DIALOG, BraseroSumDialog))
#define BRASERO_SUM_DIALOG_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_SUM_DIALOG, BraseroSumDialogClass))
#define BRASERO_IS_SUM_DIALOG(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_SUM_DIALOG))
#define BRASERO_IS_SUM_DIALOG_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_SUM_DIALOG))
#define BRASERO_SUM_DIALOG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_SUM_DIALOG, BraseroSumDialogClass))

typedef struct _BraseroSumDialog BraseroSumDialog;
typedef struct _BraseroSumDialogPrivate BraseroSumDialogPrivate;
typedef struct _BraseroSumDialogClass BraseroSumDialogClass;

struct _BraseroSumDialog {
	BraseroToolDialog parent;
	BraseroSumDialogPrivate *priv;
};

struct _BraseroSumDialogClass {
	BraseroToolDialogClass parent_class;
};

GType brasero_sum_dialog_get_type ();
GtkWidget *brasero_sum_dialog_new ();

G_END_DECLS

#endif /* BRASERO_SUM_DIALOG_H */
