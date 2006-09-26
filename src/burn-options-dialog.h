/***************************************************************************
 *            burn-options-dialog.h
 *
 *  mer mar 29 13:47:56 2006
 *  Copyright  2006  Rouquier Philippe
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

#ifndef BURN_OPTIONS_DIALOG_H
#define BURN_OPTIONS_DIALOG_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtkwindow.h>
#include <gtk/gtkdialog.h>

#include "burn-basics.h"
#include "burn-caps.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_BURN_OPTION_DIALOG         (brasero_burn_option_dialog_get_type ())
#define BRASERO_BURN_OPTION_DIALOG(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_BURN_OPTION_DIALOG, BraseroBurnOptionDialog))
#define BRASERO_BURN_OPTION_DIALOG_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_BURN_OPTION_DIALOG, BraseroBurnOptionDialogClass))
#define BRASERO_IS_BURN_OPTION_DIALOG(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_BURN_OPTION_DIALOG))
#define BRASERO_IS_BURN_OPTION_DIALOG_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_BURN_OPTION_DIALOG))
#define BRASERO_BURN_OPTION_DIALOG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_BURN_OPTION_DIALOG, BraseroBurnOptionDialogClass))

typedef struct BraseroBurnOptionDialogPrivate BraseroBurnOptionDialogPrivate;

typedef struct {
	GtkDialog parent;
	BraseroBurnOptionDialogPrivate *priv;
} BraseroBurnOptionDialog;

typedef struct {
	GtkDialogClass parent_class;
} BraseroBurnOptionDialogClass;

GType brasero_burn_option_dialog_get_type ();

GtkWidget *
brasero_burn_option_dialog_new (void);

void
brasero_burn_option_dialog_set_track (BraseroBurnOptionDialog *dialog,
				      const BraseroTrackSource *track);

gboolean
brasero_burn_option_dialog_get_session_param (BraseroBurnOptionDialog *dialog,
					      NautilusBurnDrive **drive,
					      gint *speed,
					      gchar **ouput,
					      BraseroBurnFlag *flags,
					      gchar **label,
					      BraseroImageFormat *format,
					      gboolean *checksum);
#endif /* BURN_OPTIONS_DIALOG_H */
