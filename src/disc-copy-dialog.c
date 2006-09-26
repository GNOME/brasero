/***************************************************************************
 *            disc-copy-dialog.c
 *
 *  ven jui 15 16:02:10 2005
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


#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <gdk/gdkcursor.h>

#include <gtk/gtkvbox.h>
#include <gtk/gtkwindow.h>

#include <nautilus-burn-drive.h>

#include "burn-basics.h"
#include "utils.h"
#include "burn-dialog.h"
#include "disc-copy-dialog.h"
#include "recorder-selection.h"
#include "brasero-ncb.h"

static void brasero_disc_copy_dialog_class_init (BraseroDiscCopyDialogClass *klass);
static void brasero_disc_copy_dialog_init (BraseroDiscCopyDialog *sp);
static void brasero_disc_copy_dialog_finalize (GObject *object);

static void
brasero_disc_copy_dialog_src_media_changed (BraseroRecorderSelection *src_selection,
					    NautilusBurnMediaType media_type,
					    BraseroDiscCopyDialog *dialog);
static void
brasero_disc_copy_dialog_burn_clicked_cb (GtkWidget *button,
					  BraseroDiscCopyDialog *dialog);
static void
brasero_disc_copy_dialog_cancel_clicked_cb (GtkWidget *button,
					    BraseroDiscCopyDialog *dialog);

struct BraseroDiscCopyDialogPrivate {
	GtkWidget *selection;
	GtkWidget *source;
};

static GObjectClass *parent_class = NULL;

GType
brasero_disc_copy_dialog_get_type ()
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroDiscCopyDialogClass),
			NULL,
			NULL,
			(GClassInitFunc) brasero_disc_copy_dialog_class_init,
			NULL,
			NULL,
			sizeof (BraseroDiscCopyDialog),
			0,
			(GInstanceInitFunc) brasero_disc_copy_dialog_init,
		};

		type = g_type_register_static (GTK_TYPE_DIALOG,
					       "BraseroDiscCopyDialog",
					       &our_info,
					       0);
	}

	return type;
}

static void
brasero_disc_copy_dialog_class_init (BraseroDiscCopyDialogClass * klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_disc_copy_dialog_finalize;
}

static void
brasero_disc_copy_dialog_init (BraseroDiscCopyDialog * obj)
{
	GtkWidget *burn;
	GtkWidget *cancel;
	NautilusBurnDrive *drive = NULL;

	obj->priv = g_new0 (BraseroDiscCopyDialogPrivate, 1);
	gtk_dialog_set_has_separator (GTK_DIALOG (obj), FALSE);
	gtk_window_set_title (GTK_WINDOW (obj), _("CD/DVD copy options"));

	/* take care of source media */
	obj->priv->source = brasero_recorder_selection_new ();
	g_object_set (G_OBJECT (obj->priv->source),
		      "show-properties", FALSE,
		      "show-recorders-only", TRUE,
		      NULL);

	/* There is some kind of small bug that I can't figure it out here,
	 * but this works. */
	g_object_set (G_OBJECT (obj->priv->source),
		      "show-recorders-only", FALSE,
		      NULL);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (obj)->vbox),
			    brasero_utils_pack_properties (_("<b>Select source drive to copy</b>"),
							   obj->priv->source,
							   NULL),
			    FALSE,
			    FALSE,
			    6);

	brasero_recorder_selection_select_default_drive (BRASERO_RECORDER_SELECTION (obj->priv->source),
							 BRASERO_MEDIA_WITH_DATA);

	/* destination drive */
	obj->priv->selection = brasero_recorder_selection_new ();
	g_object_set (G_OBJECT (obj->priv->selection),
		      "file-image", TRUE,
		      "show-recorders-only", TRUE,
		      NULL);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (obj)->vbox),
			    brasero_utils_pack_properties (_("<b>Select a drive to write to</b>"),
							   obj->priv->selection,
							   NULL),
			    FALSE,
			    FALSE,
			    6);

	brasero_recorder_selection_select_default_drive (BRASERO_RECORDER_SELECTION (obj->priv->selection),
							 BRASERO_MEDIA_WRITABLE);

	/* tell the destination what type of media to expect from source */
	g_signal_connect (G_OBJECT (obj->priv->source),
			  "media-changed",
			  G_CALLBACK (brasero_disc_copy_dialog_src_media_changed),
			  obj);

	brasero_recorder_selection_get_drive (BRASERO_RECORDER_SELECTION (obj->priv->source),
					      &drive,
					      NULL);

	if (drive && NCB_DRIVE_GET_TYPE (drive) != NAUTILUS_BURN_DRIVE_TYPE_FILE) {
		BraseroTrackSource source;
		NautilusBurnMediaType type;

		type = nautilus_burn_drive_get_media_type (drive);
		source.type = BRASERO_TRACK_SOURCE_DISC;
		source.contents.drive.disc = drive;
		brasero_recorder_selection_set_source_track (BRASERO_RECORDER_SELECTION (obj->priv->selection),
							     &source);

		nautilus_burn_drive_unref (drive);
	}

	/* buttons */
	cancel = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
	g_signal_connect (G_OBJECT (cancel),
			  "clicked",
			  G_CALLBACK (brasero_disc_copy_dialog_cancel_clicked_cb),
			  obj);
	gtk_dialog_add_action_widget (GTK_DIALOG (obj), cancel, GTK_RESPONSE_CANCEL);

	burn = brasero_utils_make_button (_("Copy"), GTK_STOCK_CDROM);
	g_signal_connect (G_OBJECT (burn),
			  "clicked",
			  G_CALLBACK (brasero_disc_copy_dialog_burn_clicked_cb),
			  obj);
	gtk_dialog_add_action_widget (GTK_DIALOG (obj), burn, GTK_RESPONSE_OK);
}

static void
brasero_disc_copy_dialog_finalize (GObject *object)
{
	BraseroDiscCopyDialog *cobj;

	cobj = BRASERO_DISC_COPY_DIALOG (object);

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkWidget *
brasero_disc_copy_dialog_new ()
{
	BraseroDiscCopyDialog *obj;

	obj = BRASERO_DISC_COPY_DIALOG (g_object_new (BRASERO_TYPE_DISC_COPY_DIALOG,
				       NULL));

	return GTK_WIDGET (obj);
}

static void
brasero_disc_copy_dialog_src_media_changed (BraseroRecorderSelection *src_selection,
					    NautilusBurnMediaType media_type,
					    BraseroDiscCopyDialog *dialog)
{
	NautilusBurnDrive *drive;

	brasero_recorder_selection_get_drive (BRASERO_RECORDER_SELECTION (dialog->priv->source),
					      &drive,
					      NULL);
	if (drive) {
		BraseroTrackSource source;
		NautilusBurnMediaType type;

		type = nautilus_burn_drive_get_media_type (drive);
		source.type = BRASERO_TRACK_SOURCE_DISC;
		source.contents.drive.disc = drive;
		brasero_recorder_selection_set_source_track (BRASERO_RECORDER_SELECTION (dialog->priv->selection),
							     &source);

		nautilus_burn_drive_unref (drive);
	}
}

static void
brasero_disc_copy_dialog_cancel_clicked_cb (GtkWidget *button,
					    BraseroDiscCopyDialog *dialog)
{
	gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
}

static void
brasero_disc_copy_dialog_burn_clicked_cb (GtkWidget *button,
					  BraseroDiscCopyDialog *dialog)
{
	gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
}

gboolean
brasero_disc_copy_dialog_get_session_param (BraseroDiscCopyDialog *dialog,
					    NautilusBurnDrive **drive,
					    gint *speed,
					    gchar **output,
					    BraseroTrackSource **source,
					    BraseroBurnFlag *flags)
{
	BraseroDriveProp props;
	BraseroTrackSource *track;
	NautilusBurnDrive *drive_source = NULL;

	/* get drives and flags */
	brasero_recorder_selection_get_drive (BRASERO_RECORDER_SELECTION (dialog->priv->selection),
					      drive,
					      &props);

	/* create the track */
	brasero_recorder_selection_get_drive (BRASERO_RECORDER_SELECTION (dialog->priv->source),
					      &drive_source,
					      NULL);

	track = g_new0 (BraseroTrackSource, 1);
	track->type = BRASERO_TRACK_SOURCE_DISC;
	track->contents.drive.disc = drive_source;

	if (NCB_DRIVE_GET_TYPE (*drive) == NAUTILUS_BURN_DRIVE_TYPE_FILE) {
		*output = props.output_path;
		track->format = props.props.image_format;
		*speed = 0;
	}
	else {
		*speed = props.props.drive_speed;
		track->format = BRASERO_IMAGE_FORMAT_ANY;
		*output = NULL;
	}

	*source = track;
	*flags = props.flags | BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE;

	return TRUE;
}
