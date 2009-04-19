/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-burn
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-burn is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-burn authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-burn. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-burn is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <gdk/gdk.h>

#include <gtk/gtk.h>

#include "brasero-tray.h"
#include "brasero-burn-dialog.h"
#include "brasero-session-cfg.h"
#include "brasero-session-helper.h"

#include "burn-basics.h"
#include "burn-debug.h"
#include "brasero-progress.h"
#include "brasero-cover.h"
#include "brasero-track-type-private.h"

#include "brasero-tags.h"
#include "brasero-session.h"
#include "brasero-track-image.h"

#include "brasero-medium.h"
#include "brasero-drive.h"

#include "brasero-misc.h"


static void brasero_burn_dialog_class_init (BraseroBurnDialogClass *klass);
static void brasero_burn_dialog_init (BraseroBurnDialog *obj);
static void brasero_burn_dialog_finalize (GObject *object);
static void brasero_burn_dialog_destroy (GtkObject *object);

static gboolean
brasero_burn_dialog_delete (GtkWidget *widget,
			    GdkEventAny *event);

static void
brasero_burn_dialog_cancel_clicked_cb (GtkWidget *button,
				       BraseroBurnDialog *dialog);

static void
brasero_burn_dialog_tray_cancel_cb (BraseroTrayIcon *tray,
				    BraseroBurnDialog *dialog);
static void
brasero_burn_dialog_tray_show_dialog_cb (BraseroTrayIcon *tray,
					 gboolean show,
					 GtkWidget *dialog);

typedef struct BraseroBurnDialogPrivate BraseroBurnDialogPrivate;
struct BraseroBurnDialogPrivate {
	BraseroBurn *burn;
	BraseroTrackType input;
	BraseroBurnSession *session;

	GtkWidget *progress;
	GtkWidget *header;
	GtkWidget *cancel;
	GtkWidget *image;
	BraseroTrayIcon *tray;

	/* for our final statistics */
	GTimer *total_time;
	gint64 total_size;
	GSList *rates;

	guint is_writing:1;
	guint is_creating_image:1;
};

#define BRASERO_BURN_DIALOG_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_BURN_DIALOG, BraseroBurnDialogPrivate))

#define TIMEOUT	10000

static GObjectClass *parent_class = NULL;

GType
brasero_burn_dialog_get_type ()
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroBurnDialogClass),
			NULL,
			NULL,
			(GClassInitFunc) brasero_burn_dialog_class_init,
			NULL,
			NULL,
			sizeof (BraseroBurnDialog),
			0,
			(GInstanceInitFunc) brasero_burn_dialog_init,
		};

		type = g_type_register_static (GTK_TYPE_DIALOG,
					       "BraseroBurnDialog",
					       &our_info, 0);
	}

	return type;
}

static void
brasero_burn_dialog_class_init (BraseroBurnDialogClass * klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroBurnDialogPrivate));

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_burn_dialog_finalize;
	gtk_object_class->destroy = brasero_burn_dialog_destroy;
	widget_class->delete_event = brasero_burn_dialog_delete;
}

/**
 * NOTE: if input is DISC then media is the media input
 */

static void
brasero_burn_dialog_update_info (BraseroBurnDialog *dialog,
				 BraseroTrackType *input,
				 BraseroMedia media)
{
	gchar *title = NULL;
	gchar *header = NULL;
	BraseroBurnFlag flags;
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	flags = brasero_burn_session_get_flags (priv->session);
	if (media == BRASERO_MEDIUM_FILE) {
		/* we are creating an image to the hard drive */
		gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
					      "iso-image-new",
					      GTK_ICON_SIZE_DIALOG);

		header = g_strdup_printf ("<big><b>%s</b></big>", _("Creating image"));
		title = g_strdup (_("Brasero - Creating Image"));
	}
	else if (media & BRASERO_MEDIUM_DVD) {
		if (input->type == BRASERO_TRACK_TYPE_STREAM
		&&  BRASERO_STREAM_FORMAT_HAS_VIDEO (input->subtype.stream_format)) {
			if (flags & BRASERO_BURN_FLAG_DUMMY) {
				title = g_strdup (_("Brasero - Burning DVD (Simulation)"));
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Simulation of video DVD burning"));
			}
			else {
				title = g_strdup (_("Brasero - Burning DVD"));
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Burning video DVD"));
			}

			gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
						      "media-optical-video-new",
						      GTK_ICON_SIZE_DIALOG);
		}
		else if (input->type == BRASERO_TRACK_TYPE_DATA) {
			if (flags & BRASERO_BURN_FLAG_DUMMY) {
				title = g_strdup (_("Brasero - Burning DVD (Simulation)"));
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Simulation of data DVD burning"));
			}
			else {
				title = g_strdup (_("Brasero - Burning DVD"));
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Burning data DVD"));
			}

			gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
						      "media-optical-data-new",
						      GTK_ICON_SIZE_DIALOG);
		}
		else if (input->type == BRASERO_TRACK_TYPE_IMAGE) {
			if (flags & BRASERO_BURN_FLAG_DUMMY) {
				title = g_strdup (_("Burning DVD (Simulation)"));
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Simulation of image to DVD burning"));
			}
			else {
				title = g_strdup (_("Burning DVD"));
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Burning image to DVD"));
			}

			gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
						      "media-optical",
						      GTK_ICON_SIZE_DIALOG);
		}
		else if (input->type == BRASERO_TRACK_TYPE_DISC) {
			if (flags & BRASERO_BURN_FLAG_DUMMY) {
				title = g_strdup (_("Brasero - Copying DVD (Simulation)"));
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Simulation of data DVD copying"));
			}
			else {
				title = g_strdup (_("Brasero - Copying DVD"));
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Copying data DVD"));
			}

			gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
						      "media-optical-copy",
						      GTK_ICON_SIZE_DIALOG);
		}
	}
	else if (media & BRASERO_MEDIUM_CD) {
		if (input->type == BRASERO_TRACK_TYPE_STREAM
		&&  BRASERO_STREAM_FORMAT_HAS_VIDEO (input->subtype.stream_format)) {
			if (flags & BRASERO_BURN_FLAG_DUMMY) {
				title = g_strdup (_("Brasero - Burning CD (Simulation)"));
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Simulation of (S)VCD burning"));
			}
			else {
				title = g_strdup (_("Brasero - Burning CD"));
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Burning (S)VCD"));
			}

			gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
						      "drive-removable-media",
						      GTK_ICON_SIZE_DIALOG);
		}
		else if (input->type == BRASERO_TRACK_TYPE_STREAM) {
			if (flags & BRASERO_BURN_FLAG_DUMMY) {
				title = g_strdup (_("Brasero - Burning CD (Simulation)"));
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Simulation of audio CD burning"));
			}
			else {
				title = g_strdup (_("Brasero - Burning CD"));
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Burning audio CD"));
			}

			gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
						      "media-optical-audio-new",
						      GTK_ICON_SIZE_DIALOG);
		}
		else if (input->type == BRASERO_TRACK_TYPE_DATA) {
			if (flags & BRASERO_BURN_FLAG_DUMMY) {
				title = g_strdup (_("Brasero - Burning CD (Simulation)"));
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Simulation of data CD burning"));
			}
			else {
				title = g_strdup (_("Brasero - Burning CD"));
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Burning data CD"));
			}

			gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
						      "media-optical-data-new",
						      GTK_ICON_SIZE_DIALOG);
		}
		else if (input->type == BRASERO_TRACK_TYPE_DISC) {
			if (flags & BRASERO_BURN_FLAG_DUMMY) {
				title = g_strdup (_("Brasero - Copying CD (Simulation)"));
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Simulation of CD copying"));
			}
			else {
				title = g_strdup (_("Brasero - Copying CD"));
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Copying CD"));
			}

			gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
						      "media-optical-copy",
						      GTK_ICON_SIZE_DIALOG);
		}
		else if (input->type == BRASERO_TRACK_TYPE_IMAGE) {
			if (flags & BRASERO_BURN_FLAG_DUMMY) {
				title = g_strdup (_("Brasero - Burning CD (Simulation)"));
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Simulation of image to CD burning"));
			}
			else {
				title = g_strdup (_("Brasero - Burning CD"));
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Burning image to CD"));
			}
		
			gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
						      "media-optical",
						      GTK_ICON_SIZE_DIALOG);
		}
	}
	else if (input->type == BRASERO_TRACK_TYPE_STREAM
	     &&  BRASERO_STREAM_FORMAT_HAS_VIDEO (input->subtype.stream_format)) {
		if (flags & BRASERO_BURN_FLAG_DUMMY) {
			title = g_strdup (_("Brasero - Burning disc (Simulation)"));
			header = g_strdup_printf ("<big><b>%s</b></big>", _("Simulation of video disc burning"));
		}
		else {
			title = g_strdup (_("Brasero - Burning disc"));
			header = g_strdup_printf ("<big><b>%s</b></big>", _("Burning video disc"));
		}

		gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
					      "drive-removable-media",
					      GTK_ICON_SIZE_DIALOG);
	}
	else if (input->type == BRASERO_TRACK_TYPE_STREAM) {
		if (flags & BRASERO_BURN_FLAG_DUMMY) {
			title = g_strdup (_("Brasero - Burning CD (Simulation)"));
			header = g_strdup_printf ("<big><b>%s</b></big>", _("Simulation of audio CD burning"));
		}
		else {
			title = g_strdup (_("Brasero - Burning CD"));
			header = g_strdup_printf ("<big><b>%s</b></big>", _("Burning audio CD"));
		}

		gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
					      "drive-removable-media",
					      GTK_ICON_SIZE_DIALOG);
	}
	else if (input->type == BRASERO_TRACK_TYPE_DATA) {
		if (flags & BRASERO_BURN_FLAG_DUMMY) {
			title = g_strdup (_("Brasero - Burning Disc (Simulation)"));
			header = g_strdup_printf ("<big><b>%s</b></big>", _("Simulation of data disc burning"));
		}
		else {
			title = g_strdup (_("Brasero - Burning Disc"));
			header = g_strdup_printf ("<big><b>%s</b></big>", _("Burning data disc"));
		}

		gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
					      "drive-removable-media",
					      GTK_ICON_SIZE_DIALOG);
	}
	else if (input->type == BRASERO_TRACK_TYPE_DISC) {
		if (flags & BRASERO_BURN_FLAG_DUMMY) {
			title = g_strdup (_("Brasero - Copying Disc (Simulation)"));
			header = g_strdup_printf ("<big><b>%s</b></big>", _("Simulation of disc copying"));
		}
		else {
			title = g_strdup (_("Brasero - Copying Disc"));
			header = g_strdup_printf ("<big><b>%s</b></big>", _("Copying disc"));
		}
		gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
					      "drive-removable-media",
					      GTK_ICON_SIZE_DIALOG);
	}
	else if (input->type == BRASERO_TRACK_TYPE_IMAGE) {
		if (flags & BRASERO_BURN_FLAG_DUMMY) {
			title = g_strdup (_("Brasero - Burning Disc (Simulation)"));
			header = g_strdup_printf ("<big><b>%s</b></big>", _("Simulation of image to disc burning"));
		}
		else {
			title = g_strdup (_("Brasero - Burning Disc"));
			header = g_strdup_printf ("<big><b>%s</b></big>", _("Burning image to disc"));
		}

		gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
					      "drive-removable-media",
					      GTK_ICON_SIZE_DIALOG);
	}


	if (title) {
		gtk_window_set_title (GTK_WINDOW (dialog), title);
		g_free (title);
	}

	gtk_label_set_text (GTK_LABEL (priv->header), header);
	gtk_label_set_use_markup (GTK_LABEL (priv->header), TRUE);
	g_free (header);

	/* it may have changed */
	gtk_window_set_icon_name (GTK_WINDOW (dialog), "brasero");
}

static gchar *
brasero_burn_dialog_get_media_type_string (BraseroBurn *burn,
					   BraseroMedia type,
					   gboolean insert)
{
	gchar *message = NULL;

	if (type & BRASERO_MEDIUM_HAS_DATA) {
		if (!insert) {
			if (type & BRASERO_MEDIUM_REWRITABLE)
				message = g_strdup (_("Please replace the disc with a rewritable disc holding data."));
			else
				message = g_strdup (_("Please replace the disc with a disc holding data."));
		}
		else {
			if (type & BRASERO_MEDIUM_REWRITABLE)
				message = g_strdup (_("Please insert a rewritable disc holding data."));
			else
				message = g_strdup (_("Please insert a disc holding data."));
		}
	}
	else if (type & BRASERO_MEDIUM_WRITABLE) {
		gint64 isosize = 0;
	
		brasero_burn_status (burn,
				     NULL,
				     &isosize,
				     NULL,
				     NULL);

		if ((type & BRASERO_MEDIUM_CD) && !(type & BRASERO_MEDIUM_DVD)) {
			if (!insert) {
				if (isosize)
					message = g_strdup_printf (_("Please replace the disc with a recordable CD with at least %i MiB of free space."), 
								   (int) (isosize / 1048576));
				else
					message = g_strdup (_("Please replace the disc with a recordable CD."));
			}
			else {
				if (isosize)
					message = g_strdup_printf (_("Please insert a recordable CD with at least %i MiB of free space."), 
								   (int) (isosize / 1048576));
				else
					message = g_strdup (_("Please insert a recordable CD."));
			}
		}
		else if (!(type & BRASERO_MEDIUM_CD) && (type & BRASERO_MEDIUM_DVD)) {
			if (!insert) {
				if (isosize)
					message = g_strdup_printf (_("Please replace the disc with a recordable DVD with at least %i MiB of free space."), 
								   (int) (isosize / 1048576));
				else
					message = g_strdup (_("Please replace the disc with a recordable DVD."));
			}
			else {
				if (isosize)
					message = g_strdup_printf (_("Please insert a recordable DVD with at least %i MiB of free space."), 
								   (int) (isosize / 1048576));
				else
					message = g_strdup (_("Please insert a recordable DVD."));
			}
		}
		else if (!insert) {
			if (isosize)
				message = g_strdup_printf (_("Please replace the disc with a recordable CD or DVD with at least %i MiB of free space."), 
							   (int) (isosize / 1048576));
			else
				message = g_strdup (_("Please replace the disc with a recordable CD or DVD."));
		}
		else {
			if (isosize)
				message = g_strdup_printf (_("Please insert a recordable CD or DVD with at least %i MiB of free space."), 
							   (int) (isosize / 1048576));
			else
				message = g_strdup (_("Please insert a recordable CD or DVD."));
		}
	}

	return message;
}

static void
brasero_burn_dialog_wait_for_insertion (BraseroDrive *drive,
					BraseroMedium *medium,
					GtkDialog *message)
{
	/* we might have a dialog waiting for the 
	 * insertion of a disc if so close it */
	gtk_dialog_response (GTK_DIALOG (message), GTK_RESPONSE_OK);
}

static BraseroBurnResult
brasero_burn_dialog_insert_disc_cb (BraseroBurn *burn,
				    BraseroDrive *drive,
				    BraseroBurnError error,
				    BraseroMedia type,
				    BraseroBurnDialog *dialog)
{
	gint result;
	gint added_id;
	gchar *drive_name;
	GtkWindow *window;
	GtkWidget *message;
	gboolean hide = FALSE;
	BraseroBurnDialogPrivate *priv;
	gchar *main_message = NULL, *secondary_message = NULL;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	if (!GTK_WIDGET_VISIBLE (dialog)) {
		gtk_widget_show (GTK_WIDGET (dialog));
		hide = TRUE;
	}

	g_timer_stop (priv->total_time);

	if (drive)
		drive_name = brasero_drive_get_display_name (drive);
	else
		drive_name = NULL;

	if (error == BRASERO_BURN_WARNING_INSERT_AFTER_COPY) {
		secondary_message = g_strdup (_("An image of the disc has been created on your hard drive."
						"\nBurning will begin as soon as a recordable disc is inserted."));
		main_message = brasero_burn_dialog_get_media_type_string (burn, type, FALSE);
	}
	else if (error == BRASERO_BURN_WARNING_CHECKSUM) {
		secondary_message = g_strdup (_("A data integrity test will begin as soon as the disc is inserted."));
		main_message = g_strdup (_("Please re-insert the disc in the CD/DVD burner."));
	}
	else if (error == BRASERO_BURN_ERROR_DRIVE_BUSY) {
		/* Translators: %s is the name of a drive */
		main_message = g_strdup_printf (_("\"%s\" is busy."), drive_name);
		secondary_message = g_strdup_printf ("%s.", _("Make sure another application is not using it"));
	} 
	else if (error == BRASERO_BURN_ERROR_MEDIUM_NONE) {
		secondary_message = g_strdup_printf (_("There is no disc in \"%s\"."), drive_name);
		main_message = brasero_burn_dialog_get_media_type_string (burn, type, TRUE);
	}
	else if (error == BRASERO_BURN_ERROR_MEDIUM_INVALID) {
		secondary_message = g_strdup_printf (_("The disc in \"%s\" is not supported."), drive_name);
		main_message = brasero_burn_dialog_get_media_type_string (burn, type, TRUE);
	}
	else if (error == BRASERO_BURN_ERROR_MEDIUM_NOT_REWRITABLE) {
		secondary_message = g_strdup_printf (_("The disc in \"%s\" is not rewritable."), drive_name);
		main_message = brasero_burn_dialog_get_media_type_string (burn, type, FALSE);
	}
	else if (error == BRASERO_BURN_ERROR_MEDIUM_NO_DATA) {
		secondary_message = g_strdup_printf (_("The disc in \"%s\" is empty."), drive_name);
		main_message = brasero_burn_dialog_get_media_type_string (burn, type, FALSE);
	}
	else if (error == BRASERO_BURN_ERROR_MEDIUM_NOT_WRITABLE) {
		secondary_message = g_strdup_printf (_("The disc in \"%s\" is not writable."), drive_name);
		main_message = brasero_burn_dialog_get_media_type_string (burn, type, FALSE);
	}
	else if (error == BRASERO_BURN_ERROR_MEDIUM_SPACE) {
		secondary_message = g_strdup_printf (_("Not enough space available on the disc in \"%s\"."), drive_name);
		main_message = brasero_burn_dialog_get_media_type_string (burn, type, FALSE);
	}
	else if (error == BRASERO_BURN_ERROR_NONE) {
		main_message = brasero_burn_dialog_get_media_type_string (burn, type, TRUE);
		secondary_message = NULL;
	}
	else if (error == BRASERO_BURN_ERROR_MEDIUM_NEED_RELOADING) {
		secondary_message = g_strdup_printf (_("The disc in \"%s\" needs to be reloaded."), drive_name);
		main_message = g_strdup (_("Please eject the disc and reload it."));
	}

	g_free (drive_name);

	window = GTK_WINDOW (dialog);

	if (secondary_message) {
		message = gtk_message_dialog_new (window,
						  GTK_DIALOG_DESTROY_WITH_PARENT|
						  GTK_DIALOG_MODAL,
						  GTK_MESSAGE_WARNING,
						  GTK_BUTTONS_CANCEL,
						  "%s", main_message);

		if (secondary_message) {
			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
								  "%s", secondary_message);
			g_free (secondary_message);
		}
	}
	else {
		gchar *string;

		message = gtk_message_dialog_new_with_markup (window,
							      GTK_DIALOG_DESTROY_WITH_PARENT|
							      GTK_DIALOG_MODAL,
							      GTK_MESSAGE_WARNING,
							      GTK_BUTTONS_CANCEL,
							      NULL);

		string = g_strdup_printf ("<b><big>%s</big></b>", main_message);
		gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (message), string);
		g_free (string);
	}

	g_free (main_message);

	/* connect to signals to be warned when media is inserted */
	added_id = g_signal_connect_after (drive,
					   "medium-added",
					   G_CALLBACK (brasero_burn_dialog_wait_for_insertion),
					   message);

	result = gtk_dialog_run (GTK_DIALOG (message));

	g_signal_handler_disconnect (drive, added_id);
	gtk_widget_destroy (message);

	/* see if we should update the infos */
	brasero_burn_dialog_update_info (dialog,
					 &priv->input, 
					 brasero_burn_session_get_dest_media (priv->session));

	if (hide)
		gtk_widget_hide (GTK_WIDGET (dialog));

	g_timer_start (priv->total_time);

	if (result != GTK_RESPONSE_OK)
		return BRASERO_BURN_CANCEL;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_dialog_image_error (BraseroBurn *burn,
				 GError *error,
				 gboolean is_temporary,
				 BraseroBurnDialog *dialog)
{
	gint result;
	gchar *path;
	gchar *string;
	GtkWindow *window;
	GtkWidget *message;
	gboolean hide = FALSE;
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	if (!GTK_WIDGET_VISIBLE (dialog)) {
		gtk_widget_show (GTK_WIDGET (dialog));
		hide = TRUE;
	}

	g_timer_stop (priv->total_time);

	window = GTK_WINDOW (dialog);

	string = g_strdup_printf ("%s. %s",
				  is_temporary?
				  _("A file could not be created at the location specified for temporary files"):
				  _("The image could not be created at the specified location"),
				  _("Do you want to specify another location for this session or retry with the current location?"));

	message = gtk_message_dialog_new (window,
					  GTK_DIALOG_DESTROY_WITH_PARENT|
					  GTK_DIALOG_MODAL,
					  GTK_MESSAGE_ERROR,
					  GTK_BUTTONS_NONE,
					  "%s",
					  string);
	g_free (string);

	if (error && error->code == BRASERO_BURN_ERROR_DISK_SPACE)
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
							 "%s.\n%s.",
							  error->message,
							  _("You may want to free some space on the disc and retry"));
	else
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
							 "%s.",
							  error->message);

	gtk_dialog_add_buttons (GTK_DIALOG (message),
				_("_Keep Current Location"), GTK_RESPONSE_OK,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				_("_Change Location"), GTK_RESPONSE_ACCEPT,
				NULL);

	result = gtk_dialog_run (GTK_DIALOG (message));
	gtk_widget_destroy (message);

	if (hide)
		gtk_widget_hide (GTK_WIDGET (dialog));

	if (result == GTK_RESPONSE_OK) {
		g_timer_start (priv->total_time);
		return BRASERO_BURN_OK;
	}

	if (result != GTK_RESPONSE_ACCEPT) {
		g_timer_start (priv->total_time);
		return BRASERO_BURN_CANCEL;
	}

	/* Show a GtkFileChooserDialog */
	if (!is_temporary) {
		gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (message), TRUE);
		message = gtk_file_chooser_dialog_new (_("Location for Image File"),
						       GTK_WINDOW (dialog),
						       GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
						       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						       GTK_STOCK_SAVE, GTK_RESPONSE_OK,
						       NULL);
	}
	else
		message = gtk_file_chooser_dialog_new (_("Location for Temporary Files"),
						       GTK_WINDOW (dialog),
						       GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
						       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						       GTK_STOCK_SAVE, GTK_RESPONSE_OK,
						       NULL);

	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (message), TRUE);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (message), g_get_home_dir ());

	result = gtk_dialog_run (GTK_DIALOG (message));
	if (result != GTK_RESPONSE_OK) {
		gtk_widget_destroy (message);
		g_timer_start (priv->total_time);
		return BRASERO_BURN_CANCEL;
	}

	path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (message));
	gtk_widget_destroy (message);

	if (!is_temporary) {
		BraseroImageFormat format;
		gchar *image = NULL;
		gchar *toc = NULL;

		format = brasero_burn_session_get_output_format (priv->session);
		brasero_burn_session_get_output (priv->session,
						 &image,
						 &toc,
						 NULL);

		if (toc) {
			gchar *name;

			name = g_path_get_basename (toc);
			g_free (toc);

			toc = g_build_filename (path, name, NULL);
			BRASERO_BURN_LOG ("New toc location %s", toc);
		}

		if (image) {
			gchar *name;

			name = g_path_get_basename (image);
			g_free (image);

			image = g_build_filename (path, name, NULL);
			BRASERO_BURN_LOG ("New image location %s", toc);
		}

		brasero_burn_session_set_image_output_full (priv->session,
							    format,
							    image,
							    toc);
	}
	else
		brasero_burn_session_set_tmpdir (priv->session, path);

	g_free (path);

	g_timer_start (priv->total_time);
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_dialog_loss_warnings_cb (GtkDialog *dialog, 
				      const gchar *main_message,
				      const gchar *secondary_message,
				      const gchar *button_text,
				      const gchar *button_icon)
{
	gint result;
	GtkWindow *window;
	GtkWidget *button;
	GtkWidget *message;
	gboolean hide = FALSE;
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	if (!GTK_WIDGET_VISIBLE (dialog)) {
		gtk_widget_show (GTK_WIDGET (dialog));
		hide = TRUE;
	}

	g_timer_stop (priv->total_time);

	window = GTK_WINDOW (dialog);
	message = gtk_message_dialog_new (window,
					  GTK_DIALOG_DESTROY_WITH_PARENT|
					  GTK_DIALOG_MODAL,
					  GTK_MESSAGE_WARNING,
					  GTK_BUTTONS_NONE,
					  "%s", main_message);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
						 "%s", secondary_message);

	button = gtk_dialog_add_button (GTK_DIALOG (message),
					_("_Replace Disc"),
					GTK_RESPONSE_ACCEPT);
	gtk_button_set_image (GTK_BUTTON (button),
			      gtk_image_new_from_stock (GTK_STOCK_REFRESH,
							GTK_ICON_SIZE_BUTTON));

	gtk_dialog_add_button (GTK_DIALOG (message),
			       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

	button = gtk_dialog_add_button (GTK_DIALOG (message),
					button_text,
					GTK_RESPONSE_OK);
	gtk_button_set_image (GTK_BUTTON (button),
			      gtk_image_new_from_icon_name (button_icon,
							    GTK_ICON_SIZE_BUTTON));

	result = gtk_dialog_run (GTK_DIALOG (message));
	gtk_widget_destroy (message);

	if (hide)
		gtk_widget_hide (GTK_WIDGET (dialog));

	g_timer_start (priv->total_time);

	if (result == GTK_RESPONSE_ACCEPT)
		return BRASERO_BURN_NEED_RELOAD;

	if (result != GTK_RESPONSE_OK)
		return BRASERO_BURN_CANCEL;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_dialog_data_loss_cb (BraseroBurn *burn,
				  GtkDialog *dialog)
{
	return brasero_burn_dialog_loss_warnings_cb (dialog,
						     _("Do you really want to erase the current disc?"),
						     _("The disc in the drive holds data."),
						     _("_Erase Disc"),
						     "media-optical-blank");
}

static BraseroBurnResult
brasero_burn_dialog_previous_session_loss_cb (BraseroBurn *burn,
					      GtkDialog *dialog)
{
	gchar *secondary;
	BraseroBurnResult result;

	secondary = g_strdup_printf ("%s\n%s",
				     _("Already burnt files will be invisible (though still readable)."),
				     _("Do you want to continue anyway?"));
				     
	result = brasero_burn_dialog_loss_warnings_cb (dialog,
						       _("Appending new files to a multisession disc is not advised."),
						       secondary,
						       _("_Continue"),
						       "media-optical-burn");
	g_free (secondary);
	return result;
}

static BraseroBurnResult
brasero_burn_dialog_audio_to_appendable_cb (BraseroBurn *burn,
					    GtkDialog *dialog)
{
	gchar *secondary;
	BraseroBurnResult result;

	secondary = g_strdup_printf ("%s\n%s",
				     _("You might not be able to listen to them with stereos and CD-TEXT won't be written."),
				     _("Do you want to continue anyway?"));

	result = brasero_burn_dialog_loss_warnings_cb (dialog,
						       _("Appending audio tracks to a CD is not advised."),
						       secondary,
						       _("_Continue"),
						       "media-optical-burn");
	g_free (secondary);
	return result;
}

static BraseroBurnResult
brasero_burn_dialog_rewritable_cb (BraseroBurn *burn,
				   GtkDialog *dialog)
{
	gchar *secondary;
	BraseroBurnResult result;

	secondary = g_strdup_printf ("%s\n%s",
				     _("You might not be able to listen to it with stereos."),
				     _("Do you want to continue anyway?"));

	result = brasero_burn_dialog_loss_warnings_cb (dialog,
						       _("Recording audio tracks on a rewritable disc is not advised."),
						       secondary,
						       _("_Continue"),
						       "media-optical-burn");
	g_free (secondary);
	return result;
}

static BraseroBurnResult
brasero_burn_dialog_disable_joliet_cb (BraseroBurn *burn,
				       GtkDialog *dialog)
{
	gint result;
	GtkWindow *window;
	GtkWidget *button;
	GtkWidget *message;
	gboolean hide = FALSE;
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	if (!GTK_WIDGET_VISIBLE (dialog)) {
		gtk_widget_show (GTK_WIDGET (dialog));
		hide = TRUE;
	}

	g_timer_stop (priv->total_time);

	window = GTK_WINDOW (dialog);
	message = gtk_message_dialog_new (window,
					  GTK_DIALOG_DESTROY_WITH_PARENT|
					  GTK_DIALOG_MODAL,
					  GTK_MESSAGE_WARNING,
					  GTK_BUTTONS_NONE,
					  _("Do you want to continue with full Windows compatibility disabled?"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
						  _("Some files don't have a suitable name for a fully Windows-compatible CD."));

	gtk_dialog_add_button (GTK_DIALOG (message),
			       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

	button = gtk_dialog_add_button (GTK_DIALOG (message),
					_("_Continue"),
					GTK_RESPONSE_OK);
	gtk_button_set_image (GTK_BUTTON (button),
			      gtk_image_new_from_stock (GTK_STOCK_OK,
							GTK_ICON_SIZE_BUTTON));
	
	result = gtk_dialog_run (GTK_DIALOG (message));
	gtk_widget_destroy (message);

	if (hide)
		gtk_widget_hide (GTK_WIDGET (dialog));

	g_timer_start (priv->total_time);

	if (result != GTK_RESPONSE_OK)
		return BRASERO_BURN_CANCEL;

	return BRASERO_BURN_OK;
}

static void
brasero_burn_dialog_update_title_writing_progress (BraseroBurnDialog *dialog,
						   BraseroTrackType *input,
						   BraseroMedia media,
						   guint percent)
{
	BraseroBurnDialogPrivate *priv;
	BraseroBurnFlag flags;
	gchar *title = NULL;
	gchar *icon_name;
	guint remains;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	flags = brasero_burn_session_get_flags (priv->session);

	/* This is used only when actually writing to a disc */
	if (media == BRASERO_MEDIUM_FILE)
		title = g_strdup_printf (_("Brasero - Creating Image (%i%% Done)"), percent);
	else if (media & BRASERO_MEDIUM_DVD) {
		if (input->type == BRASERO_TRACK_TYPE_DISC) {
			if (flags & BRASERO_BURN_FLAG_DUMMY)
				title = g_strdup (_("Brasero - Copying DVD (Simulation)"));
			else
				title = g_strdup_printf (_("Brasero - Copying DVD (%i%% Done)"), percent);
		}
		else {
			if (flags & BRASERO_BURN_FLAG_DUMMY)
				title = g_strdup (_("Brasero - Burning DVD (Simulation)"));
			else
				title = g_strdup_printf (_("Brasero - Burning DVD (%i%% Done)"), percent);
		}
	}
	else if (media & BRASERO_MEDIUM_CD) {
		if (input->type == BRASERO_TRACK_TYPE_DISC) {
			if (flags & BRASERO_BURN_FLAG_DUMMY)
				title = g_strdup (_("Brasero - Copying CD (Simulation)"));
			else
				title = g_strdup_printf (_("Brasero - Copying CD (%i%% Done)"), percent);
		}
		else {
			if (flags & BRASERO_BURN_FLAG_DUMMY)
				title = g_strdup (_("Brasero - Burning CD (simulation)"));
			else
				title = g_strdup_printf (_("Brasero - Burning CD (%i%% Done)"), percent);
		}
	}
	else if (input->type == BRASERO_TRACK_TYPE_DISC) {
		if (flags & BRASERO_BURN_FLAG_DUMMY)
			title = g_strdup (_("Brasero - Copying Disc (Simulation)"));
		else
			title = g_strdup_printf (_("Brasero - Copying Disc (%i%% Done)"), percent);
	}
	else {
		if (flags & BRASERO_BURN_FLAG_DUMMY)
			title = g_strdup (_("Brasero - Burning Disc (Simulation)"));
		else
			title = g_strdup_printf (_("Brasero - Burning Disc (%i%% Done)"), percent);
	}

	gtk_window_set_title (GTK_WINDOW (dialog), title);
	g_free (title);

	/* also update the icon */
	remains = percent % 5;
	if (remains > 3)
		percent += 5 - remains;
	else
		percent -= remains;

	if (percent < 0 || percent > 100)
		return;

	icon_name = g_strdup_printf ("brasero-disc-%02i", percent);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), icon_name);
	g_free (icon_name);
}

static void
brasero_burn_dialog_progress_changed_real (BraseroBurnDialog *dialog,
					   gint64 written,
					   gint64 isosize,
					   gint64 rate,
					   gdouble overall_progress,
					   gdouble task_progress,
					   glong remaining,
					   BraseroMedia media)
{
	gint mb_isosize = -1;
	gint mb_written = -1;
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	if (written >= 0)
		mb_written = (gint) (written / 1048576);
	
	if (isosize > 0)
		mb_isosize = (gint) (isosize / 1048576);

	if (task_progress >= 0.0 && priv->is_writing)
		brasero_burn_dialog_update_title_writing_progress (dialog,
								   &priv->input,
								   media,
								   (guint) (task_progress * 100.0));

	brasero_burn_progress_set_status (BRASERO_BURN_PROGRESS (priv->progress),
					  media,
					  overall_progress,
					  task_progress,
					  remaining,
					  mb_isosize,
					  mb_written,
					  rate);

	brasero_tray_icon_set_progress (BRASERO_TRAYICON (priv->tray),
					task_progress,
					remaining);

	if (rate > 0 && priv->is_writing)
		priv->rates = g_slist_prepend (priv->rates, GINT_TO_POINTER ((gint) rate));
}

static void
brasero_burn_dialog_progress_changed_cb (BraseroBurn *burn, 
					 gdouble overall_progress,
					 gdouble task_progress,
					 glong remaining,
					 BraseroBurnDialog *dialog)
{
	BraseroMedia media = BRASERO_MEDIUM_NONE;
	BraseroBurnDialogPrivate *priv;
	gint64 isosize = -1;
	gint64 written = -1;
	gint64 rate = -1;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	brasero_burn_status (priv->burn,
			     &media,
			     &isosize,
			     &written,
			     &rate);

	brasero_burn_dialog_progress_changed_real (dialog,
						   written,
						   isosize,
						   rate,
						   overall_progress,
						   task_progress,
						   remaining,
						   media);
	if ((priv->is_writing || priv->is_creating_image) && isosize > 0)
		priv->total_size = isosize;
}

static void
brasero_burn_dialog_action_changed_real (BraseroBurnDialog *dialog,
					 BraseroBurnAction action,
					 const gchar *string)
{
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	brasero_burn_progress_set_action (BRASERO_BURN_PROGRESS (priv->progress),
					  action,
					  string);
	brasero_tray_icon_set_action (BRASERO_TRAYICON (priv->tray),
				      action,
				      string);
}

static void
brasero_burn_dialog_action_changed_cb (BraseroBurn *burn, 
				       BraseroBurnAction action,
				       BraseroBurnDialog *dialog)
{
	BraseroBurnDialogPrivate *priv;
	gchar *string = NULL;
	gboolean is_writing;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	is_writing = (action == BRASERO_BURN_ACTION_RECORDING ||
		      action == BRASERO_BURN_ACTION_DRIVE_COPY ||
		      action == BRASERO_BURN_ACTION_RECORDING_CD_TEXT ||
		      action == BRASERO_BURN_ACTION_LEADIN ||
		      action == BRASERO_BURN_ACTION_LEADOUT ||
		      action == BRASERO_BURN_ACTION_FIXATING);

	if (action == BRASERO_BURN_ACTION_START_RECORDING
	|| (priv->is_writing && !is_writing)) {
		BraseroMedia media = BRASERO_MEDIUM_NONE;

		brasero_burn_status (burn, &media, NULL, NULL, NULL);
		brasero_burn_dialog_update_info (dialog,
						 &priv->input,
						 media);
	}

	priv->is_creating_image = (action == BRASERO_BURN_ACTION_CREATING_IMAGE);
	priv->is_writing = is_writing;

	brasero_burn_get_action_string (priv->burn, action, &string);
	brasero_burn_dialog_action_changed_real (dialog, action, string);
	g_free (string);
}

static gboolean
brasero_burn_dialog_dummy_success_timeout (gpointer data)
{
	GtkDialog *dialog = data;
	gtk_dialog_response (dialog, GTK_RESPONSE_OK);
	return FALSE;
}

static BraseroBurnResult
brasero_burn_dialog_dummy_success_cb (BraseroBurn *burn,
				      BraseroBurnDialog *dialog)
{
	BraseroBurnDialogPrivate *priv;
	GtkResponseType answer;
	GtkWidget *message;
	GtkWindow *window;
	GtkWidget *button;
	gboolean hide;
	gint id;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	if (!GTK_WIDGET_MAPPED (dialog)) {
		gtk_widget_show (GTK_WIDGET (dialog));
		hide = TRUE;
	}
	else
		hide = FALSE;

	g_timer_stop (priv->total_time);

	window = GTK_WINDOW (dialog);
	message = gtk_message_dialog_new (window,
					  GTK_DIALOG_DESTROY_WITH_PARENT|
					  GTK_DIALOG_MODAL,
					  GTK_MESSAGE_INFO,
					  GTK_BUTTONS_CANCEL,
					  _("The simulation was successful."));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
						  _("Real disc burning will take place in 10 seconds."));

	button = gtk_dialog_add_button (GTK_DIALOG (message),
					_("Burn _Now"),
					GTK_RESPONSE_OK);
	gtk_button_set_image (GTK_BUTTON (button),
			      gtk_image_new_from_icon_name ("media-optical-burn",
							    GTK_ICON_SIZE_BUTTON));
	id = g_timeout_add_seconds (10,
				    brasero_burn_dialog_dummy_success_timeout,
				    message);

	gtk_widget_show (GTK_WIDGET (dialog));
	gtk_window_set_urgency_hint (GTK_WINDOW (dialog), TRUE);

	answer = gtk_dialog_run (GTK_DIALOG (message));
	gtk_widget_destroy (message);

	gtk_window_set_urgency_hint (GTK_WINDOW (dialog), FALSE);

	if (hide)
		gtk_widget_hide (GTK_WIDGET (dialog));

	g_timer_start (priv->total_time);

	if (answer == GTK_RESPONSE_OK) {
		if (id)
			g_source_remove (id);

		return BRASERO_BURN_OK;
	}

	if (id)
		g_source_remove (id);

	return BRASERO_BURN_CANCEL;
}

static void
brasero_burn_dialog_init (BraseroBurnDialog * obj)
{
	GtkWidget *box;
	GtkWidget *vbox;
	GtkWidget *alignment;
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (obj);

	gtk_window_set_default_size (GTK_WINDOW (obj), 500, 0);

	gtk_dialog_set_has_separator (GTK_DIALOG (obj), FALSE);

	priv->tray = brasero_tray_icon_new ();
	g_signal_connect (priv->tray,
			  "cancel",
			  G_CALLBACK (brasero_burn_dialog_tray_cancel_cb),
			  obj);
	g_signal_connect (priv->tray,
			  "show-dialog",
			  G_CALLBACK (brasero_burn_dialog_tray_show_dialog_cb),
			  obj);

	alignment = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
	gtk_widget_show (alignment);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 8, 6, 6);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (obj)->vbox),
			    alignment,
			    TRUE,
			    TRUE,
			    0);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (alignment), vbox);

	box = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (box);
	gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, TRUE, 0);

	priv->header = gtk_label_new (NULL);
	gtk_widget_show (priv->header);
	gtk_misc_set_alignment (GTK_MISC (priv->header), 0.0, 0.5);
	gtk_misc_set_padding (GTK_MISC (priv->header), 0, 18);
	gtk_box_pack_start (GTK_BOX (box), priv->header, FALSE, TRUE, 0);

	priv->image = gtk_image_new ();
	gtk_misc_set_alignment (GTK_MISC (priv->image), 1.0, 0.5);
	gtk_widget_show (priv->image);
	gtk_box_pack_start (GTK_BOX (box), priv->image, TRUE, TRUE, 0);

	priv->progress = brasero_burn_progress_new ();
	gtk_widget_show (priv->progress);
	gtk_box_pack_start (GTK_BOX (vbox),
			    priv->progress,
			    FALSE,
			    TRUE,
			    0);

	/* buttons */
	priv->cancel = gtk_dialog_add_button (GTK_DIALOG (obj),
						   GTK_STOCK_CANCEL,
						   GTK_RESPONSE_CANCEL);
}

static void
brasero_burn_dialog_destroy (GtkObject * object)
{
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (object);

	if (priv->burn) {
		g_object_unref (priv->burn);
		priv->burn = NULL;
	}

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
brasero_burn_dialog_finalize (GObject * object)
{
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (object);

	if (priv->burn) {
		brasero_burn_cancel (priv->burn, TRUE);
		g_object_unref (priv->burn);
		priv->burn = NULL;
	}

	if (priv->tray) {
		g_object_unref (priv->tray);
		priv->tray = NULL;
	}

	if (priv->session) {
		g_object_unref (priv->session);
		priv->session = NULL;
	}

	if (priv->total_time) {
		g_timer_destroy (priv->total_time);
		priv->total_time = NULL;
	}

	if (priv->rates) {
		g_slist_free (priv->rates);
		priv->rates = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkWidget *
brasero_burn_dialog_new (void)
{
	BraseroBurnDialog *obj;

	obj = BRASERO_BURN_DIALOG (g_object_new (BRASERO_TYPE_BURN_DIALOG, NULL));

	return GTK_WIDGET (obj);
}

static void
brasero_burn_dialog_activity_start (BraseroBurnDialog *dialog)
{
	GdkCursor *cursor;
	GdkWindow *window;
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	window = gtk_widget_get_window (GTK_WIDGET (dialog));
	if (window) {
		cursor = gdk_cursor_new (GDK_WATCH);
		gdk_window_set_cursor (window, NULL);
		gdk_cursor_unref (cursor);
	}

	gtk_button_set_use_stock (GTK_BUTTON (priv->cancel), TRUE);
	gtk_button_set_label (GTK_BUTTON (priv->cancel), GTK_STOCK_CANCEL);
	gtk_window_set_urgency_hint (GTK_WINDOW (dialog), FALSE);

	g_signal_connect (priv->cancel,
			  "clicked",
			  G_CALLBACK (brasero_burn_dialog_cancel_clicked_cb),
			  dialog);

	/* Reset or set the speed info */
	g_object_set (priv->progress,
		      "show-info", TRUE,
		      NULL);
	brasero_burn_progress_set_status (BRASERO_BURN_PROGRESS (priv->progress),
					  FALSE,
					  0.0,
					  0.0,
					  -1,
					  -1,
					  -1,
					  -1);
}

static void
brasero_burn_dialog_activity_stop (BraseroBurnDialog *dialog,
				   const gchar *message)
{
	gchar *markup;
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	gdk_window_set_cursor (GTK_WIDGET (dialog)->window, NULL);

	markup = g_strdup_printf ("<b><big>%s</big></b>", message);
	gtk_label_set_text (GTK_LABEL (priv->header), markup);
	gtk_label_set_use_markup (GTK_LABEL (priv->header), TRUE);
	g_free (markup);

	gtk_button_set_use_stock (GTK_BUTTON (priv->cancel), TRUE);
	gtk_button_set_label (GTK_BUTTON (priv->cancel), GTK_STOCK_CLOSE);

	g_signal_handlers_disconnect_by_func (priv->cancel,
					      brasero_burn_dialog_cancel_clicked_cb,
					      dialog);

	brasero_burn_progress_set_status (BRASERO_BURN_PROGRESS (priv->progress),
					  FALSE,
					  1.0,
					  1.0,
					  -1,
					  -1,
					  -1,
					  -1);

	gtk_widget_show (GTK_WIDGET (dialog));
	gtk_window_set_urgency_hint (GTK_WINDOW (dialog), TRUE);
}

static BraseroBurnResult
brasero_burn_dialog_setup_session (BraseroBurnDialog *dialog,
				   GError **error)
{
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	brasero_burn_session_start (priv->session);

	priv->burn = brasero_burn_new ();
	g_signal_connect (priv->burn,
			  "insert-media",
			  G_CALLBACK (brasero_burn_dialog_insert_disc_cb),
			  dialog);
	g_signal_connect (priv->burn,
			  "location-request",
			  G_CALLBACK (brasero_burn_dialog_image_error),
			  dialog);
	g_signal_connect (priv->burn,
			  "warn-data-loss",
			  G_CALLBACK (brasero_burn_dialog_data_loss_cb),
			  dialog);
	g_signal_connect (priv->burn,
			  "warn-previous-session-loss",
			  G_CALLBACK (brasero_burn_dialog_previous_session_loss_cb),
			  dialog);
	g_signal_connect (priv->burn,
			  "warn-audio-to-appendable",
			  G_CALLBACK (brasero_burn_dialog_audio_to_appendable_cb),
			  dialog);
	g_signal_connect (priv->burn,
			  "warn-rewritable",
			  G_CALLBACK (brasero_burn_dialog_rewritable_cb),
			  dialog);
	g_signal_connect (priv->burn,
			  "disable-joliet",
			  G_CALLBACK (brasero_burn_dialog_disable_joliet_cb),
			  dialog);
	g_signal_connect (priv->burn,
			  "progress-changed",
			  G_CALLBACK (brasero_burn_dialog_progress_changed_cb),
			  dialog);
	g_signal_connect (priv->burn,
			  "action-changed",
			  G_CALLBACK (brasero_burn_dialog_action_changed_cb),
			  dialog);
	g_signal_connect (priv->burn,
			  "dummy-success",
			  G_CALLBACK (brasero_burn_dialog_dummy_success_cb),
			  dialog);

	brasero_burn_progress_set_status (BRASERO_BURN_PROGRESS (priv->progress),
					  FALSE,
					  0.0,
					  -1.0,
					  -1,
					  -1,
					  -1,
					  -1);

	brasero_tray_icon_set_progress (BRASERO_TRAYICON (priv->tray),
					0.0,
					-1);

	brasero_burn_progress_set_action (BRASERO_BURN_PROGRESS (priv->progress),
					  BRASERO_BURN_ACTION_NONE,
					  NULL);

	brasero_tray_icon_set_action (BRASERO_TRAYICON (priv->tray),
				      BRASERO_BURN_ACTION_NONE,
				      NULL);

	if (priv->total_time)
		g_timer_destroy (priv->total_time);

	priv->total_time = g_timer_new ();
	g_timer_start (priv->total_time);

	return BRASERO_BURN_OK;
}

static void
brasero_burn_dialog_save_log (BraseroBurnDialog *dialog)
{
	gchar *contents;
	gchar *path = NULL;
	GtkWidget *chooser;
	GtkResponseType answer;
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	chooser = gtk_file_chooser_dialog_new (_("Save Current Session"),
					       GTK_WINDOW (dialog),
					       GTK_FILE_CHOOSER_ACTION_SAVE,
					       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					       GTK_STOCK_SAVE, GTK_RESPONSE_OK,
					       NULL);

	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), TRUE);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser),
					     g_get_home_dir ());
	gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (chooser),
					   "brasero-session.log");
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (chooser), TRUE);

	gtk_widget_show (chooser);
	answer = gtk_dialog_run (GTK_DIALOG (chooser));
	if (answer != GTK_RESPONSE_OK) {
		gtk_widget_destroy (chooser);
		return;
	}

	path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
	gtk_widget_destroy (chooser);

	if (!path)
		return;

	if (path && *path == '\0') {
		g_free (path);
		return;
	}

	g_file_get_contents (brasero_burn_session_get_log_path (priv->session),
			     &contents,
			     NULL,
			     NULL);
	g_file_set_contents (path, contents, -1, NULL);

	g_free (contents);
	g_free (path);
}

static void
brasero_burn_dialog_show_log (BraseroBurnDialog *dialog)
{
	gint words_num;
	GtkWidget *view;
	GtkTextIter iter;
	struct stat stats;
	GtkWidget *message;
	GtkWidget *scrolled;
	GtkTextBuffer *text;
	const gchar *logfile;
	GtkTextTag *object_tag;
	GtkTextTag *domain_tag;
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	message = gtk_dialog_new_with_buttons (_("Session Log"),
					       GTK_WINDOW (dialog),
					       GTK_DIALOG_DESTROY_WITH_PARENT |
					       GTK_DIALOG_MODAL,
					       GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
					       NULL);
	gtk_window_set_default_size (GTK_WINDOW (message), 500, 375);
	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_set_border_width (GTK_CONTAINER (scrolled), 6);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled),
					     GTK_SHADOW_ETCHED_IN);
	gtk_box_pack_end (GTK_BOX (GTK_DIALOG (message)->vbox),
			  scrolled,
			  TRUE,
			  TRUE,
			  0);

	view = gtk_text_view_new ();
	gtk_text_view_set_editable (GTK_TEXT_VIEW (view), FALSE);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled), view);

	/* we better make sure the session log is not too big < 10 MB otherwise
	 * everything will freeze and will take a huge part of memory. If it is
	 * bigger then only show the end which is the most relevant. */
	logfile = brasero_burn_session_get_log_path (priv->session);
	if (g_stat (logfile, &stats) == -1) {
		brasero_utils_message_dialog (GTK_WIDGET (dialog),
					      _("The session log cannot be displayed."),
					      _("The log file could not be found"),
					      GTK_MESSAGE_ERROR);
		gtk_widget_destroy (message);
		return;
	}

	text = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	if (stats.st_size > 1 * 1024 * 1024) {
		gchar contents [1 * 1024 * 1024];
		GtkTextIter iter;
		FILE *file;

		gtk_text_buffer_get_start_iter (text, &iter);
		gtk_text_buffer_insert (text,
					&iter,
					_("This is a excerpt from the session log (the last 10 MiB):\n\n"),
					-1);

		file = g_fopen (logfile, "r");
		if (!file) {
			int errsv = errno;

			brasero_utils_message_dialog (GTK_WIDGET (dialog),
						      _("The session log cannot be displayed."),
						      g_strerror (errsv),
						      GTK_MESSAGE_ERROR);
			gtk_widget_destroy (message);
			return;
		}

		if (fread (contents, 1, sizeof (contents), file) != sizeof (contents)) {
			int errsv = errno;

			brasero_utils_message_dialog (GTK_WIDGET (dialog),
						      _("The session log cannot be displayed."),
						      g_strerror (errsv),
						      GTK_MESSAGE_ERROR);
			gtk_widget_destroy (message);
			return;
		}

		gtk_text_buffer_insert (text, &iter, contents, sizeof (contents));
	}
	else {
		gchar *contents;

		/* fill the buffer */
		g_file_get_contents (brasero_burn_session_get_log_path (priv->session),
				     &contents,
				     NULL,
				     NULL);
		gtk_text_buffer_set_text (text, contents, -1);
		g_free (contents);
	}

	/* create tags and apply them */
	object_tag = gtk_text_buffer_create_tag (text,
						 NULL,
						 "foreground", "red",
						 "weight", PANGO_WEIGHT_BOLD,
						 NULL);
	domain_tag = gtk_text_buffer_create_tag (text,
						 NULL,
						 "foreground", "blue",
						 NULL);
	gtk_text_buffer_get_start_iter (text, &iter);
	words_num = 0;
	while (gtk_text_iter_forward_word_end (&iter)) {
		GtkTextIter start = iter;

		gtk_text_iter_backward_word_start (&start);

		if (words_num == 2)
			gtk_text_buffer_apply_tag (text, object_tag, &start, &iter);
		else if (gtk_text_iter_starts_line (&start)) {
			words_num = 1;
			gtk_text_buffer_apply_tag (text, domain_tag, &start, &iter);
		}

		words_num ++;
	}

	/* run everything */
	gtk_widget_show_all (scrolled);
	gtk_dialog_run (GTK_DIALOG (message));

	gtk_widget_destroy (message);
}

static void
brasero_burn_dialog_notify_error (BraseroBurnDialog *dialog,
				  GError *error)
{
	gchar *secondary;
	GtkWidget *button;
	GtkWidget *message;
	GtkResponseType response;

	if (error) {
		secondary =  g_strdup (error->message);
		g_error_free (error);
	}
	else
		secondary = g_strdup (_("An unknown error occured."));

	if (!GTK_WIDGET_VISIBLE (dialog))
		gtk_widget_show (GTK_WIDGET (dialog));

	message = gtk_message_dialog_new (GTK_WINDOW (dialog),
					  GTK_DIALOG_DESTROY_WITH_PARENT |
					  GTK_DIALOG_MODAL,
					  GTK_MESSAGE_ERROR,
					  GTK_BUTTONS_NONE,
					  _("Error while burning."));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
						  "%s",
						  secondary);
	g_free (secondary);

	button = gtk_dialog_add_button (GTK_DIALOG (message),
					_("_Save Log"),
					GTK_RESPONSE_APPLY);
	gtk_button_set_image (GTK_BUTTON (button),
			      gtk_image_new_from_stock (GTK_STOCK_SAVE_AS,
							GTK_ICON_SIZE_BUTTON));

	button = gtk_dialog_add_button (GTK_DIALOG (message),
					_("_View Log"),
					GTK_RESPONSE_OK);
	gtk_button_set_image (GTK_BUTTON (button),
			      gtk_image_new_from_stock (GTK_STOCK_SAVE_AS,
							GTK_ICON_SIZE_BUTTON));

	gtk_dialog_add_button (GTK_DIALOG (message),
			       GTK_STOCK_CLOSE,
			       GTK_RESPONSE_CLOSE);

	response = gtk_dialog_run (GTK_DIALOG (message));
	while (1) {
		if (response == GTK_RESPONSE_APPLY)
			brasero_burn_dialog_save_log (dialog);
		else if (response == GTK_RESPONSE_OK)
			brasero_burn_dialog_show_log (dialog);
		else
			break;

		response = gtk_dialog_run (GTK_DIALOG (message));
	}

	gtk_widget_destroy (message);
}

static gboolean
brasero_burn_dialog_success_run (BraseroBurnDialog *dialog)
{
	gint answer;
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	answer = gtk_dialog_run (GTK_DIALOG (dialog));
	if (answer == GTK_RESPONSE_CLOSE) {
		GtkWidget *window;

		window = brasero_session_edit_cover (priv->session, GTK_WIDGET (dialog));
		gtk_dialog_run (GTK_DIALOG (window));
		gtk_widget_destroy (window);
		return FALSE;
	}

	return (answer == GTK_RESPONSE_OK);
}

static gboolean
brasero_burn_dialog_notify_success (BraseroBurnDialog *dialog)
{
	gint64 rate;
	gboolean res;
	BraseroMedia media;
	BraseroDrive *drive;
	gchar *primary = NULL;
	GtkWidget *make_another = NULL;
	GtkWidget *create_cover = NULL;
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	drive = brasero_burn_session_get_burner (priv->session);
	if (priv->input.type != BRASERO_TRACK_TYPE_DISC)
		media = brasero_burn_session_get_dest_media (priv->session);
	else
		media = priv->input.subtype.media;

	switch (priv->input.type) {
	case BRASERO_TRACK_TYPE_STREAM:
		primary = g_strdup (_("Audio CD successfully burnt"));
		break;
	case BRASERO_TRACK_TYPE_DISC:
		if (!brasero_drive_is_fake (drive)) {
			if (media & BRASERO_MEDIUM_DVD)
				primary = g_strdup (_("DVD successfully copied"));
			else
				primary = g_strdup (_("CD successfully copied"));
		}
		else {
			if (media & BRASERO_MEDIUM_DVD)
				primary = g_strdup (_("Image of DVD successfully created"));
			else
				primary = g_strdup (_("Image of CD successfully created"));
		}
		break;
	case BRASERO_TRACK_TYPE_IMAGE:
		if (!brasero_drive_is_fake (drive)) {
			if (media & BRASERO_MEDIUM_DVD)
				primary = g_strdup (_("Image successfully burnt to DVD"));
			else
				primary = g_strdup (_("Image successfully burnt to CD"));
		}
		break;
	default:
		if (!brasero_drive_is_fake (drive)) {
			if (media & BRASERO_MEDIUM_DVD)
				primary = g_strdup (_("Data DVD successfully burnt"));
			else
				primary = g_strdup (_("Data CD successfully burnt"));
		}
		else
			primary = g_strdup (_("Image successfully created"));
		break;
	}

	brasero_burn_dialog_activity_stop (dialog, primary);
	g_free (primary);

	if (!brasero_burn_session_is_dest_file (priv->session)) {
		/* Useful button but it shouldn't be used for images */
		make_another = gtk_dialog_add_button (GTK_DIALOG (dialog),
						      _("Make _Another Copy"),
						      GTK_RESPONSE_OK);
	}

	/* show total required time and average speed */
	rate = 0;
	if (priv->rates) {
		int num = 0;
		GSList *iter;

		for (iter = priv->rates; iter; iter = iter->next) {
			rate += GPOINTER_TO_INT (iter->data);
			num ++;
		}
		rate /= num;
	}

	brasero_burn_progress_display_session_info (BRASERO_BURN_PROGRESS (priv->progress),
						    g_timer_elapsed (priv->total_time, NULL),
						    rate,
						    media,
						    priv->total_size);

	if (priv->input.type == BRASERO_TRACK_TYPE_STREAM
	|| (priv->input.type == BRASERO_TRACK_TYPE_DISC
	&& (priv->input.subtype.media & BRASERO_MEDIUM_HAS_AUDIO))) {
		/* since we succeed offer the possibility to create cover if that's an audio disc */
		create_cover = gtk_dialog_add_button (GTK_DIALOG (dialog),
						      _("_Create Cover"),
						      GTK_RESPONSE_CLOSE);
	}

	res = brasero_burn_dialog_success_run (dialog);

	if (make_another)
		gtk_widget_destroy (make_another);

	if (create_cover)
		gtk_widget_destroy (create_cover);

	return res;
}

static void
brasero_burn_dialog_add_track_to_recent (BraseroTrack *track)
{
	gchar *uri = NULL;
	GtkRecentManager *recent;
	BraseroImageFormat format;
	gchar *groups [] = { "brasero", NULL };
	gchar *mimes [] = { "application/x-cd-image",
			    "application/x-cue",
			    "application/x-toc",
			    "application/x-cdrdao-toc" };
	GtkRecentData recent_data = { NULL,
				      NULL,
				      NULL,
				      "brasero",
				      "brasero -p %u",
				      groups,
				      FALSE };

	if (!BRASERO_IS_TRACK_IMAGE (track))
		return;

	format = brasero_track_image_get_format (BRASERO_TRACK_IMAGE (track));
	if (format == BRASERO_IMAGE_FORMAT_NONE)
		return;

	/* Add it to recent file manager */
	switch (format) {
	case BRASERO_IMAGE_FORMAT_BIN:
		recent_data.mime_type = mimes [0];
		uri = brasero_track_image_get_source (BRASERO_TRACK_IMAGE (track), TRUE);
		break;

	case BRASERO_IMAGE_FORMAT_CUE:
		recent_data.mime_type = mimes [1];
		uri = brasero_track_image_get_toc_source (BRASERO_TRACK_IMAGE (track), TRUE);
		break;

	case BRASERO_IMAGE_FORMAT_CLONE:
		recent_data.mime_type = mimes [2];
		uri = brasero_track_image_get_toc_source (BRASERO_TRACK_IMAGE (track), TRUE);
		break;

	case BRASERO_IMAGE_FORMAT_CDRDAO:
		recent_data.mime_type = mimes [3];
		uri = brasero_track_image_get_toc_source (BRASERO_TRACK_IMAGE (track), TRUE);
		break;

	default:
		break;
	}

	if (!uri)
		return;

	recent = gtk_recent_manager_get_default ();
	gtk_recent_manager_add_full (recent,
				     uri,
				     &recent_data);
	g_free (uri);
}

static gboolean
brasero_burn_dialog_end_session (BraseroBurnDialog *dialog,
				 BraseroBurnResult result,
				 GError *error)
{
	gboolean retry = FALSE;
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	if (priv->total_time)
		g_timer_stop (priv->total_time);

	brasero_burn_session_stop (priv->session);

	if (result == BRASERO_BURN_CANCEL) {
		/* nothing to do */
	}
	else if (error || result != BRASERO_BURN_OK) {
		brasero_burn_dialog_notify_error (dialog, error);
	}
	else {
		/* see if an image was created. If so, add it to GtkRecent */
		if (brasero_burn_session_is_dest_file (priv->session)) {
			GSList *tracks;

			tracks = brasero_burn_session_get_tracks (priv->session);
			for (; tracks; tracks = tracks->next) {
				BraseroTrack *track;

				track = tracks->data;
				brasero_burn_dialog_add_track_to_recent (track);
			}
		}

		retry = brasero_burn_dialog_notify_success (dialog);
	}

	if (priv->burn) {
		g_object_unref (priv->burn);
		priv->burn = NULL;
	}

	if (priv->rates) {
		g_slist_free (priv->rates);
		priv->rates = NULL;
	}

	if (priv->total_time) {
		g_timer_destroy (priv->total_time);
		priv->total_time = NULL;
	}

	return retry;
}

static BraseroBurnResult
brasero_burn_dialog_record_session (BraseroBurnDialog *dialog,
				    BraseroMedia media)
{
	gboolean retry;
	GError *error = NULL;
	BraseroBurnResult result;
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	brasero_burn_dialog_update_info (dialog, &priv->input, media);

	/* start the recording session */
	brasero_burn_dialog_activity_start (dialog);
	result = brasero_burn_dialog_setup_session (dialog, &error);
	if (result != BRASERO_BURN_OK)
		return result;

	result = brasero_burn_record (priv->burn,
				      priv->session,
				      &error);

	retry = brasero_burn_dialog_end_session (dialog,
						 result,
						 error);

	if (result == BRASERO_BURN_RETRY)
		return result;

	if (retry)
		return BRASERO_BURN_RETRY;

	return BRASERO_BURN_OK;
}

static gboolean
brasero_burn_dialog_wait_for_ready_state (BraseroBurnDialog *dialog)
{
	BraseroBurnDialogPrivate *priv;
	BraseroBurnResult result;
	BraseroStatus *status;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	status = brasero_status_new ();
	result = brasero_burn_session_get_status (priv->session, status);
	while (brasero_status_get_result (status) == BRASERO_BURN_NOT_READY) {
		gdouble progress;
		gchar *action;

		action = brasero_status_get_current_action (status);
		brasero_burn_dialog_action_changed_real (dialog,
							 BRASERO_BURN_ACTION_GETTING_SIZE,
							 action);
		g_free (action);

		progress = brasero_status_get_progress (status);
		brasero_burn_dialog_progress_changed_real (dialog,
							   0,
							   0,
							   0,
							   progress,
							   progress,
							   -1.0,
							   brasero_burn_session_get_dest_media (priv->session));

		result = brasero_burn_session_get_status (priv->session, status);
	}
	brasero_status_free (status);

	return (result == BRASERO_BURN_OK);
}

gboolean
brasero_burn_dialog_run (BraseroBurnDialog *dialog,
			 BraseroBurnSession *session)
{
	BraseroMedia media;
	BraseroBurnResult result;
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	g_object_ref (session);
	priv->session = session;

	/* wait for ready state */
	if (!brasero_burn_dialog_wait_for_ready_state (dialog))
		return FALSE;

	/* disable autoconfiguration */
	if (BRASERO_IS_SESSION_CFG (priv->session))
		brasero_session_cfg_disable (BRASERO_SESSION_CFG (priv->session));

	/* update what we should display */
	brasero_burn_session_get_input_type (session, &priv->input);
	if (brasero_burn_session_is_dest_file (session))
		media = BRASERO_MEDIUM_FILE;
	else if (priv->input.type != BRASERO_TRACK_TYPE_DISC)
		media = brasero_burn_session_get_dest_media (session);
	else {
		BraseroMedium *medium;

		medium = brasero_burn_session_get_src_medium (priv->session);
		media = brasero_medium_get_status (medium);
	}

	do {
		result = brasero_burn_dialog_record_session (dialog, media);
	} while (result == BRASERO_BURN_RETRY);

	priv->session = NULL;
	g_object_unref (session);

	return (result == BRASERO_BURN_OK);
}

static gboolean
brasero_burn_dialog_cancel_dialog (GtkWidget *toplevel)
{
	gint result;
	GtkWidget *button;
	GtkWidget *message;

	message = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					  GTK_DIALOG_DESTROY_WITH_PARENT |
					  GTK_DIALOG_MODAL,
					  GTK_MESSAGE_WARNING,
					  GTK_BUTTONS_NONE,
					  _("Do you really want to quit?"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG
						  (message),
						  _("Interrupting the process may make disc unusable."));

	button = gtk_dialog_add_button (GTK_DIALOG (message),
					_("C_ontinue Burning"),
					GTK_RESPONSE_OK);
	gtk_button_set_image (GTK_BUTTON (button),
			      gtk_image_new_from_stock (GTK_STOCK_OK,
							GTK_ICON_SIZE_BUTTON));

	button = gtk_dialog_add_button (GTK_DIALOG (message),
					_("_Cancel Burning"),
					GTK_RESPONSE_CANCEL);
	gtk_button_set_image (GTK_BUTTON (button),
			      gtk_image_new_from_stock (GTK_STOCK_CANCEL,
							GTK_ICON_SIZE_BUTTON));
	
	result = gtk_dialog_run (GTK_DIALOG (message));
	gtk_widget_destroy (message);

	return (result != GTK_RESPONSE_OK);
}

gboolean
brasero_burn_dialog_cancel (BraseroBurnDialog *dialog,
			    gboolean force_cancellation)
{
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	if (!priv->burn)
		return TRUE;

	if (brasero_burn_cancel (priv->burn, (force_cancellation == TRUE)) == BRASERO_BURN_DANGEROUS) {
		if (!brasero_burn_dialog_cancel_dialog (GTK_WIDGET (dialog)))
			return FALSE;

		brasero_burn_cancel (priv->burn, FALSE);
	}

	return TRUE;
}

static gboolean
brasero_burn_dialog_delete (GtkWidget *widget, 
			    GdkEventAny *event)
{
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (widget);

	brasero_tray_icon_set_show_dialog (BRASERO_TRAYICON (priv->tray), FALSE);
 	return TRUE;
}

static void
brasero_burn_dialog_cancel_clicked_cb (GtkWidget *button,
				       BraseroBurnDialog *dialog)
{
	/* a burning is ongoing cancel it */
	brasero_burn_dialog_cancel (dialog, FALSE);
}

static void
brasero_burn_dialog_tray_cancel_cb (BraseroTrayIcon *tray,
				    BraseroBurnDialog *dialog)
{
	brasero_burn_dialog_cancel (dialog, FALSE);
}

static void
brasero_burn_dialog_tray_show_dialog_cb (BraseroTrayIcon *tray,
					 gboolean show,
					 GtkWidget *dialog)
{
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	/* we prevent to show the burn dialog once the success dialog has been 
	 * shown to avoid the following strange behavior:
	 * Steps:
	 * - start burning
	 * - move to another workspace (ie, virtual desktop)
	 * - when the burning finishes, double-click the notification icon
	 * - you'll be unable to dismiss the dialogues normally and their behaviour will
	 *   be generally strange */
	if (!priv->burn)
		return;

	if (show)
		gtk_widget_show (dialog);
	else
		gtk_widget_hide (dialog);
}
