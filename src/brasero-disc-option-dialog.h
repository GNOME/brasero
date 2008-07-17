/***************************************************************************
 *            brasero-disc-option-dialog.h
 *
 *  jeu sep 28 17:28:45 2006
 *  Copyright  2006  Philippe Rouquier
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

#ifndef BRASERO_DISC_OPTION_DIALOG_H
#define BRASERO_DISC_OPTION_DIALOG_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtkwidget.h>
#include <gtk/gtkdialog.h>

#include "brasero-disc.h"
#include "burn-session.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_DISC_OPTION_DIALOG         (brasero_disc_option_dialog_get_type ())
#define BRASERO_DISC_OPTION_DIALOG(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_DISC_OPTION_DIALOG, BraseroDiscOptionDialog))
#define BRASERO_DISC_OPTION_DIALOG_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_DISC_OPTION_DIALOG, BraseroDiscOptionDialogClass))
#define BRASERO_IS_DISC_OPTION_DIALOG(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_DISC_OPTION_DIALOG))
#define BRASERO_IS_DISC_OPTION_DIALOG_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_DISC_OPTION_DIALOG))
#define BRASERO_DISC_OPTION_DIALOG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_DISC_OPTION_DIALOG, BraseroDiscOptionDialogClass))

typedef struct _BraseroDiscOptionDialog BraseroDiscOptionDialog;
typedef struct _BraseroDiscOptionDialogClass BraseroDiscOptionDialogClass;

struct _BraseroDiscOptionDialog {
	GtkDialog parent;
};

struct _BraseroDiscOptionDialogClass {
	GtkDialogClass parent_class;
};

GType brasero_disc_option_dialog_get_type ();
GtkWidget *brasero_disc_option_dialog_new ();

void
brasero_disc_option_dialog_set_disc (BraseroDiscOptionDialog *dialog,
				     BraseroDisc *disc);

BraseroBurnSession *
brasero_disc_option_dialog_get_session (BraseroDiscOptionDialog *dialog);

G_END_DECLS

#endif /* BRASERO_DISC_OPTION_DIALOG_H */
