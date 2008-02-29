/***************************************************************************
 *            
 *
 *  Copyright  2008  Philippe Rouquier <brasero-app@wanadoo.fr>
 *  Copyright  2008  Luis Medinas <lmedinas@gmail.com>
 *
 *
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
#include "burn-drive.h"
#include "burn-debug.h"
#include "brasero-utils.h"
#include "burn.h"

G_DEFINE_TYPE (BraseroEjectDialog, brasero_eject_dialog, BRASERO_TYPE_TOOL_DIALOG);

static void
brasero_eject_dialog_drive_changed (BraseroToolDialog *dialog,
				    BraseroMedium *medium)
{
	brasero_tool_dialog_set_valid (dialog, BRASERO_MEDIUM_VALID (brasero_medium_get_status (medium)));
}

static gpointer
_eject_async (gpointer data)
{
	BraseroDrive *drive = BRASERO_DRIVE (data);

	brasero_drive_eject (drive);
	g_object_unref (drive);

	return NULL;
}

static gboolean
brasero_eject_dialog_activate (BraseroToolDialog *dialog,
			       BraseroMedium *medium)
{
	/* In here we could also remove the lock held by any app (including 
	 * brasero) through brasero_drive_unlock. We'd need a warning
	 * dialog though which would identify why the lock is held and even
	 * better which application is holding the lock so the user does know
	 * if he can take the risk to remove the lock. */

	/* NOTE 2: we'd need also the ability to reset the drive through a SCSI
	 * command. The problem is brasero may need to be privileged then as
	 * cdrecord/cdrdao seem to be. */

	GError *error = NULL;
	BraseroDrive *drive;

	BRASERO_BURN_LOG ("Asynchronous ejection");

	drive = brasero_medium_get_drive (medium);
	g_object_ref (drive);
	g_thread_create (_eject_async, drive, FALSE, &error);
	if (error) {
		g_warning ("Could not create thread %s\n", error->message);
		g_error_free (error);

		g_object_unref (drive);
		g_object_ref (drive);
	}


	/* we'd need also to check what are the results of our operations namely
	 * if we succeded to eject. To do that, the problem is the same as above
	 * that is, since ejection can take time (a drive needs to slow down if
	 * it was reading before ejection), we can't check if the drive is still
	 * closed now as ejection is not instantaneous.
	 * A message box announcing the results of the operation would be a good
	 * thing as well probably. */
	if (brasero_drive_is_door_open (drive)) {
		//gtk_message_dialog_new ();
	}
	
	return BRASERO_BURN_OK;
}

static void
brasero_eject_dialog_class_init (BraseroEjectDialogClass *klass)
{
	BraseroToolDialogClass *tool_dialog_class = BRASERO_TOOL_DIALOG_CLASS (klass);

	tool_dialog_class->activate = brasero_eject_dialog_activate;
	tool_dialog_class->drive_changed = brasero_eject_dialog_drive_changed;
}

static void
brasero_eject_dialog_init (BraseroEjectDialog *obj)
{
	brasero_tool_dialog_set_button (BRASERO_TOOL_DIALOG (obj),
					_("_Eject"),
					NULL,
					"media-eject");
}

GtkWidget *
brasero_eject_dialog_new ()
{
	return g_object_new (BRASERO_TYPE_EJECT_DIALOG,
			     "title", ("Eject Disc"),
			     NULL);
}

