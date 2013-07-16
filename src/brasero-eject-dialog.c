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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <gtk/gtk.h>

#include "brasero-eject-dialog.h"
#include "brasero-drive-selection.h"
#include "brasero-medium.h"
#include "brasero-drive.h"
#include "brasero-volume.h"
#include "brasero-utils.h"
#include "brasero-burn.h"
#include "brasero-misc.h"
#include "brasero-app.h"

typedef struct _BraseroEjectDialogPrivate BraseroEjectDialogPrivate;
struct _BraseroEjectDialogPrivate {
	GtkWidget *selector;
	GtkWidget *eject_button;
	gboolean cancelled;
};

#define BRASERO_EJECT_DIALOG_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_EJECT_DIALOG, BraseroEjectDialogPrivate))

G_DEFINE_TYPE (BraseroEjectDialog, brasero_eject_dialog, GTK_TYPE_DIALOG);

static void
brasero_eject_dialog_activate (GtkDialog *dialog,
			       GtkResponseType answer)
{
	BraseroDrive *drive;
	GError *error = NULL;
	BraseroEjectDialogPrivate *priv;

	if (answer != GTK_RESPONSE_OK)
		return;

	priv = BRASERO_EJECT_DIALOG_PRIVATE (dialog);

	gtk_widget_set_sensitive (GTK_WIDGET (priv->selector), FALSE);
	gtk_widget_set_sensitive (priv->eject_button, FALSE);

	/* In here we could also remove the lock held by any app (including 
	 * brasero) through brasero_drive_unlock. We'd need a warning
	 * dialog though which would identify why the lock is held and even
	 * better which application is holding the lock so the user does know
	 * if he can take the risk to remove the lock. */

	/* NOTE 2: we'd need also the ability to reset the drive through a SCSI
	 * command. The problem is brasero may need to be privileged then as
	 * cdrecord/cdrdao seem to be. */
	drive = brasero_drive_selection_get_active (BRASERO_DRIVE_SELECTION (priv->selector));
	brasero_drive_unlock (drive);

	/*if (brasero_volume_is_mounted (BRASERO_VOLUME (medium))
	&& !brasero_volume_umount (BRASERO_VOLUME (medium), TRUE, &error)) {
		BRASERO_BURN_LOG ("Error unlocking medium: %s", error?error->message:"Unknown error");
		return TRUE;
	}*/
	if (!brasero_drive_eject (drive, TRUE, &error)) {
		gchar *string;
		gchar *display_name;

		if (!priv->cancelled || error) {
			display_name = brasero_drive_get_display_name (drive);
			string = g_strdup_printf (_("The disc in \"%s\" cannot be ejected"), display_name);
			g_free (display_name);

			brasero_app_alert (brasero_app_get_default (),
			                   string,
			                   error?error->message:_("An unknown error occurred"),
			                   GTK_MESSAGE_ERROR);

			g_free (string);
		}

		g_clear_error (&error);
	}

	g_object_unref (drive);
}

gboolean
brasero_eject_dialog_cancel (BraseroEjectDialog *dialog)
{
	BraseroEjectDialogPrivate *priv;
	BraseroDrive *drive;

	priv = BRASERO_EJECT_DIALOG_PRIVATE (dialog);
	drive = brasero_drive_selection_get_active (BRASERO_DRIVE_SELECTION (priv->selector));

	if (drive) {
		priv->cancelled = TRUE;
		brasero_drive_cancel_current_operation (drive);
		g_object_unref (drive);
	}

	return TRUE;
}

static void
brasero_eject_dialog_cancel_cb (GtkWidget *button_cancel,
                                BraseroEjectDialog *dialog)
{
	brasero_eject_dialog_cancel (dialog);
}

static void
brasero_eject_dialog_class_init (BraseroEjectDialogClass *klass)
{
	GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroEjectDialogPrivate));

	dialog_class->response = brasero_eject_dialog_activate;
}

static void
brasero_eject_dialog_init (BraseroEjectDialog *obj)
{
	gchar *title_str;
	GtkWidget *box;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *button;
	BraseroEjectDialogPrivate *priv;

	priv = BRASERO_EJECT_DIALOG_PRIVATE (obj);
	priv->cancelled = FALSE;

	box = gtk_dialog_get_content_area (GTK_DIALOG (obj));

	priv->selector = brasero_drive_selection_new ();
	gtk_widget_show (GTK_WIDGET (priv->selector));

	title_str = g_strdup (_("Select a disc"));

	label = gtk_label_new (title_str);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_widget_show (label);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (box), hbox, FALSE, TRUE, 0);

	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), priv->selector, FALSE, TRUE, 0);
	g_free (title_str);

	brasero_drive_selection_show_type (BRASERO_DRIVE_SELECTION (priv->selector),
	                                   BRASERO_DRIVE_TYPE_ALL_BUT_FILE);

	button = gtk_dialog_add_button (GTK_DIALOG (obj),
	                                GTK_STOCK_CANCEL,
	                                GTK_RESPONSE_CANCEL);
	g_signal_connect (button,
	                  "clicked",
	                  G_CALLBACK (brasero_eject_dialog_cancel_cb),
	                  obj);

	button = brasero_utils_make_button (_("_Eject"),
					    NULL,
					    "media-eject",
					    GTK_ICON_SIZE_BUTTON);
	gtk_dialog_add_action_widget (GTK_DIALOG (obj),
	                              button,
	                              GTK_RESPONSE_OK);
	gtk_widget_show (button);
	priv->eject_button = button;
}

GtkWidget *
brasero_eject_dialog_new ()
{
	return g_object_new (BRASERO_TYPE_EJECT_DIALOG,
			     "title", (_("Eject Disc")),
			     NULL);
}

