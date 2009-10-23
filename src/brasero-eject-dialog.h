/***************************************************************************
 *            
 *
 *  Copyright  2008  Philippe Rouquier <brasero-app@wanadoo.fr>
 *  Copyright  2008  Luis Medinas <lmedinas@gmail.com>
 *
 *
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
 *
 */

#ifndef BRASERO_EJECT_DIALOG_H
#define BRASERO_EJECT_DIALOG_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_EJECT_DIALOG         (brasero_eject_dialog_get_type ())
#define BRASERO_EJECT_DIALOG(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_EJECT_DIALOG, BraseroEjectDialog))
#define BRASERO_EJECT_DIALOG_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_EJECT_DIALOG, BraseroEjectDialogClass))
#define BRASERO_IS_EJECT_DIALOG(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_EJECT_DIALOG))
#define BRASERO_IS_EJECT_DIALOG_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_EJECT_DIALOG))
#define BRASERO_EJECT_DIALOG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_EJECT_DIALOG, BraseroEjectDialogClass))

typedef struct _BraseroEjectDialog BraseroEjectDialog;
typedef struct _BraseroEjectDialogClass BraseroEjectDialogClass;

struct _BraseroEjectDialog {
	GtkDialog parent;
};

struct _BraseroEjectDialogClass {
	GtkDialogClass parent_class;
};

GType brasero_eject_dialog_get_type (void);
GtkWidget *brasero_eject_dialog_new (void);

gboolean
brasero_eject_dialog_cancel (BraseroEjectDialog *dialog);

G_END_DECLS

#endif /* BRASERO_Eject_DIALOG_H */
