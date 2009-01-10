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
#include "brasero-tool-dialog.h"
#include "brasero-medium.h"
#include "brasero-volume.h"
#include "burn-debug.h"
#include "brasero-utils.h"
#include "burn.h"

G_DEFINE_TYPE (BraseroEjectDialog, brasero_eject_dialog, BRASERO_TYPE_TOOL_DIALOG);

static void
brasero_eject_dialog_drive_changed (BraseroToolDialog *dialog,
				    BraseroMedium *medium)
{
	if (medium)
		brasero_tool_dialog_set_valid (dialog, BRASERO_MEDIUM_VALID (brasero_medium_get_status (medium)));
	else
		brasero_tool_dialog_set_valid (dialog, FALSE);
}

static gboolean
brasero_eject_dialog_activate (BraseroToolDialog *dialog,
			       BraseroMedium *medium)
{
	BraseroDrive *drive;
	GError *error = NULL;

	/* In here we could also remove the lock held by any app (including 
	 * brasero) through brasero_drive_unlock. We'd need a warning
	 * dialog though which would identify why the lock is held and even
	 * better which application is holding the lock so the user does know
	 * if he can take the risk to remove the lock. */

	/* NOTE 2: we'd need also the ability to reset the drive through a SCSI
	 * command. The problem is brasero may need to be privileged then as
	 * cdrecord/cdrdao seem to be. */
	drive = brasero_medium_get_drive (medium);
	BRASERO_BURN_LOG ("Asynchronous ejection of %s", brasero_drive_get_device (drive));

	brasero_drive_unlock (drive);

	/*if (brasero_volume_is_mounted (BRASERO_VOLUME (medium))
	&& !brasero_volume_umount (BRASERO_VOLUME (medium), TRUE, &error)) {
		BRASERO_BURN_LOG ("Error unlocking medium: %s", error?error->message:"Unknown error");
		return TRUE;
	}*/

	if (!brasero_volume_eject (BRASERO_VOLUME (medium), TRUE, &error)) {
		BRASERO_BURN_LOG ("Error ejecting medium: %s", error?error->message:"Unknown error");
		return TRUE;
	}

	/* we'd need also to check what are the results of our operations namely
	 * if we succeded to eject. To do that, the problem is the same as above
	 * that is, since ejection can take time (a drive needs to slow down if
	 * it was reading before ejection), we can't check if the drive is still
	 * closed now as ejection is not instantaneous.
	 * A message box announcing the results of the operation would be a good
	 * thing as well probably. */
	if (!brasero_drive_is_door_open (drive)) {
		//gtk_message_dialog_new ();
	}
	
	return TRUE;
}

static gboolean
brasero_eject_dialog_cancel (BraseroToolDialog *dialog)
{
	BraseroMedium *medium;

	medium = brasero_tool_dialog_get_medium (dialog);

	if (medium) {
		brasero_volume_cancel_current_operation (BRASERO_VOLUME (medium));
		g_object_unref (medium);
	}

	return TRUE;
}

static void
brasero_eject_dialog_class_init (BraseroEjectDialogClass *klass)
{
	BraseroToolDialogClass *tool_dialog_class = BRASERO_TOOL_DIALOG_CLASS (klass);

	tool_dialog_class->activate = brasero_eject_dialog_activate;
	tool_dialog_class->cancel = brasero_eject_dialog_cancel;

	tool_dialog_class->drive_changed = brasero_eject_dialog_drive_changed;
}

static void
brasero_eject_dialog_init (BraseroEjectDialog *obj)
{
	BraseroMedium *medium;

	brasero_tool_dialog_set_button (BRASERO_TOOL_DIALOG (obj),
					_("_Eject"),
					NULL,
					"media-eject");

	/* all kinds of media */
	brasero_tool_dialog_set_medium_type_shown (BRASERO_TOOL_DIALOG (obj),
						   BRASERO_MEDIA_TYPE_ALL_BUT_FILE);

	medium = brasero_tool_dialog_get_medium (BRASERO_TOOL_DIALOG (obj));
	if (medium) {
		brasero_tool_dialog_set_valid (BRASERO_TOOL_DIALOG (obj), BRASERO_MEDIUM_VALID (brasero_medium_get_status (medium)));
		g_object_unref (medium);
	}
	else
		brasero_tool_dialog_set_valid (BRASERO_TOOL_DIALOG (obj), FALSE);
}

GtkWidget *
brasero_eject_dialog_new ()
{
	return g_object_new (BRASERO_TYPE_EJECT_DIALOG,
			     "title", (_("Eject Disc")),
			     NULL);
}

