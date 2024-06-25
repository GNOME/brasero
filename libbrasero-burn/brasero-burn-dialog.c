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
#include <gdk/gdkx.h>

#include <gtk/gtk.h>

#include <canberra-gtk.h>
#include <libnotify/notify.h>

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
#include "brasero-pk.h"

G_DEFINE_TYPE (BraseroBurnDialog, brasero_burn_dialog, GTK_TYPE_DIALOG);

static void
brasero_burn_dialog_cancel_clicked_cb (GtkWidget *button,
				       BraseroBurnDialog *dialog);

typedef struct BraseroBurnDialogPrivate BraseroBurnDialogPrivate;
struct BraseroBurnDialogPrivate {
	BraseroBurn *burn;
	BraseroBurnSession *session;

	/* This is to remember some settins after ejection */
	BraseroTrackType input;
	BraseroMedia media;

	GtkWidget *progress;
	GtkWidget *header;
	GtkWidget *cancel;
	GtkWidget *image;

	/* for our final statistics */
	GTimer *total_time;
	gint64 total_size;
	GSList *rates;

	GMainLoop *loop;
	gint wait_ready_state_id;
	GCancellable *cancel_plugin;

	gchar *initial_title;
	gchar *initial_icon;

	guint num_copies;

	guint is_writing:1;
	guint is_creating_image:1;
};

#define BRASERO_BURN_DIALOG_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_BURN_DIALOG, BraseroBurnDialogPrivate))

#define TIMEOUT	10000

static void
brasero_burn_dialog_update_media (BraseroBurnDialog *dialog)
{
	BraseroBurnDialogPrivate *priv;
	BraseroMedia media;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	if (brasero_burn_session_is_dest_file (priv->session))
		media = BRASERO_MEDIUM_FILE;
	else if (!brasero_track_type_get_has_medium (&priv->input))
		media = brasero_burn_session_get_dest_media (priv->session);
	else {
		BraseroMedium *medium;

		medium = brasero_burn_session_get_src_medium (priv->session);
		if (!medium)
			media = brasero_burn_session_get_dest_media (BRASERO_BURN_SESSION (priv->session));
		else
			media = brasero_medium_get_status (medium);
	}

	priv->media = media;
}

static void
brasero_burn_dialog_notify_daemon_close (NotifyNotification *notification,
                                         BraseroBurnDialog *dialog)
{
	g_object_unref (notification);
}

static gboolean
brasero_burn_dialog_notify_daemon (BraseroBurnDialog *dialog,
                                   const char *message)
{
        NotifyNotification *notification;
	GError *error = NULL;
        gboolean result;

	if (!notify_is_initted ()) {
		notify_init (_("Brasero notification"));
	}

        notification = notify_notification_new (message,
                                                NULL,
                                                "media-optical");

	if (!notification)
                return FALSE;

	g_signal_connect (notification,
                          "closed",
                          G_CALLBACK (brasero_burn_dialog_notify_daemon_close),
                          dialog);

	notify_notification_set_timeout (notification, 10000);
	notify_notification_set_urgency (notification, NOTIFY_URGENCY_NORMAL);
	notify_notification_set_hint_string (notification, "desktop-entry",
                                             "brasero");

	result = notify_notification_show (notification, &error);
	if (error) {
		g_warning ("Error showing notification");
		g_error_free (error);
	}

	return result;
}

static GtkWidget *
brasero_burn_dialog_create_message (BraseroBurnDialog *dialog,
                                    GtkMessageType type,
                                    GtkButtonsType buttons,
                                    const gchar *main_message)
{
	const gchar *icon_name;
	GtkWidget *message;

	icon_name = gtk_window_get_icon_name (GTK_WINDOW (dialog));
	message = gtk_message_dialog_new (GTK_WINDOW (dialog),
	                                  GTK_DIALOG_DESTROY_WITH_PARENT|
	                                  GTK_DIALOG_MODAL,
	                                  type,
	                                  buttons,
	                                  "%s", main_message);
	gtk_window_set_icon_name (GTK_WINDOW (message), icon_name);
	return message;
}

static gchar *
brasero_burn_dialog_create_dialog_title_for_action (BraseroBurnDialog *dialog,
                                                    const gchar *action,
                                                    gint percent)
{
	gchar *tmp;
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	if (priv->initial_title)
		action = priv->initial_title;

	if (percent >= 0 && percent <= 100) {
		/* Translators: This string is used in the title bar %s is the action currently performed */
		tmp = g_strdup_printf (_("%s (%i%% Done)"),
				       action,
				       percent);
	}
	else 
		/* Translators: This string is used in the title bar %s is the action currently performed */
		tmp = g_strdup (action);

	return tmp;
}

static void
brasero_burn_dialog_update_title (BraseroBurnDialog *dialog,
                                  BraseroTrackType *input)
{
	gchar *title = NULL;
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	if (priv->media == BRASERO_MEDIUM_FILE)
		title = brasero_burn_dialog_create_dialog_title_for_action (dialog,
		                                                            _("Creating Image"),
		                                                            -1);
	else if (priv->media & BRASERO_MEDIUM_DVD) {
		if (!brasero_track_type_get_has_medium (input))
			title = brasero_burn_dialog_create_dialog_title_for_action (dialog,
										    _("Burning DVD"),
										    -1);
		else
			title = brasero_burn_dialog_create_dialog_title_for_action (dialog,
										    _("Copying DVD"),
										    -1);
	}
	else if (priv->media & BRASERO_MEDIUM_CD) {
		if (!brasero_track_type_get_has_medium (input))
			title = brasero_burn_dialog_create_dialog_title_for_action (dialog,
										    _("Burning CD"),
										    -1);
		else
			title = brasero_burn_dialog_create_dialog_title_for_action (dialog,
										    _("Copying CD"),
										    -1);
	}
	else {
		if (!brasero_track_type_get_has_medium (input))
			title = brasero_burn_dialog_create_dialog_title_for_action (dialog,
										    _("Burning Disc"),
										    -1);
		else
			title = brasero_burn_dialog_create_dialog_title_for_action (dialog,
										    _("Copying Disc"),
										    -1);
	}

	if (title) {
		gtk_window_set_title (GTK_WINDOW (dialog), title);
		g_free (title);
	}
}

/**
 * NOTE: if input is DISC then media is the media input
 */

static void
brasero_burn_dialog_update_info (BraseroBurnDialog *dialog,
				 BraseroTrackType *input,
                                 gboolean dummy)
{
	gchar *header = NULL;
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	if (priv->media == BRASERO_MEDIUM_FILE) {
		/* we are creating an image to the hard drive */
		gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
					      "iso-image-new",
					      GTK_ICON_SIZE_DIALOG);

		header = g_strdup_printf ("<big><b>%s</b></big>", _("Creating image"));
	}
	else if (priv->media & BRASERO_MEDIUM_DVD) {
		if (brasero_track_type_get_has_stream (input)
		&&  BRASERO_STREAM_FORMAT_HAS_VIDEO (input->subtype.stream_format)) {
			if (dummy)
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Simulation of video DVD burning"));
			else
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Burning video DVD"));

			gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
						      "media-optical-video-new",
						      GTK_ICON_SIZE_DIALOG);
		}
		else if (brasero_track_type_get_has_data (input)) {
			if (dummy)
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Simulation of data DVD burning"));
			else
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Burning data DVD"));

			gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
						      "media-optical-data-new",
						      GTK_ICON_SIZE_DIALOG);
		}
		else if (brasero_track_type_get_has_image (input)) {
			if (dummy)
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Simulation of image to DVD burning"));
			else
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Burning image to DVD"));

			gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
						      "media-optical",
						      GTK_ICON_SIZE_DIALOG);
		}
		else if (brasero_track_type_get_has_medium (input)) {
			if (dummy)
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Simulation of data DVD copying"));
			else
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Copying data DVD"));

			gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
						      "media-optical-copy",
						      GTK_ICON_SIZE_DIALOG);
		}
	}
	else if (priv->media & BRASERO_MEDIUM_CD) {
		if (brasero_track_type_get_has_stream (input)
		&&  BRASERO_STREAM_FORMAT_HAS_VIDEO (input->subtype.stream_format)) {
			if (dummy)
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Simulation of (S)VCD burning"));
			else
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Burning (S)VCD"));

			gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
						      "drive-removable-media",
						      GTK_ICON_SIZE_DIALOG);
		}
		else if (brasero_track_type_get_has_stream (input)) {
			if (dummy)
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Simulation of audio CD burning"));
			else
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Burning audio CD"));

			gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
						      "media-optical-audio-new",
						      GTK_ICON_SIZE_DIALOG);
		}
		else if (brasero_track_type_get_has_data (input)) {
			if (dummy)
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Simulation of data CD burning"));
			else
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Burning data CD"));

			gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
						      "media-optical-data-new",
						      GTK_ICON_SIZE_DIALOG);
		}
		else if (brasero_track_type_get_has_medium (input)) {
			if (dummy)
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Simulation of CD copying"));
			else
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Copying CD"));

			gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
						      "media-optical-copy",
						      GTK_ICON_SIZE_DIALOG);
		}
		else if (brasero_track_type_get_has_image (input)) {
			if (dummy)
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Simulation of image to CD burning"));
			else
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Burning image to CD"));
		
			gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
						      "media-optical",
						      GTK_ICON_SIZE_DIALOG);
		}
	}
	else {
		if (brasero_track_type_get_has_stream (input)
		&&  BRASERO_STREAM_FORMAT_HAS_VIDEO (input->subtype.stream_format)) {
			if (dummy)
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Simulation of video disc burning"));
			else
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Burning video disc"));

			gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
						      "drive-removable-media",
						      GTK_ICON_SIZE_DIALOG);
		}
		else if (brasero_track_type_get_has_stream (input)) {
			if (dummy)
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Simulation of audio CD burning"));
			else
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Burning audio CD"));

			gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
						      "drive-removable-media",
						      GTK_ICON_SIZE_DIALOG);
		}
		else if (brasero_track_type_get_has_data (input)) {
			if (dummy)
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Simulation of data disc burning"));
			else
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Burning data disc"));

			gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
						      "drive-removable-media",
						      GTK_ICON_SIZE_DIALOG);
		}
		else if (brasero_track_type_get_has_medium (input)) {
			if (dummy)
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Simulation of disc copying"));
			else
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Copying disc"));

			gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
						      "drive-removable-media",
						      GTK_ICON_SIZE_DIALOG);
		}
		else if (brasero_track_type_get_has_image (input)) {
			if (dummy)
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Simulation of image to disc burning"));
			else
				header = g_strdup_printf ("<big><b>%s</b></big>", _("Burning image to disc"));

			gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
						      "drive-removable-media",
						      GTK_ICON_SIZE_DIALOG);
		}
	}

	gtk_label_set_text (GTK_LABEL (priv->header), header);
	gtk_label_set_use_markup (GTK_LABEL (priv->header), TRUE);
	g_free (header);
}

static void
brasero_burn_dialog_wait_for_insertion_cb (BraseroDrive *drive,
					   BraseroMedium *medium,
					   GtkDialog *message)
{
	/* we might have a dialog waiting for the 
	 * insertion of a disc if so close it */
	gtk_dialog_response (GTK_DIALOG (message), GTK_RESPONSE_OK);
}

static gint
brasero_burn_dialog_wait_for_insertion (BraseroBurnDialog *dialog,
					BraseroDrive *drive,
					const gchar *main_message,
					const gchar *secondary_message,
                                        gboolean sound_clue)
{
	gint result;
	gulong added_id;
	GtkWidget *message;
	gboolean hide = FALSE;
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	if (!gtk_widget_get_visible (GTK_WIDGET (dialog))) {
		gtk_widget_show (GTK_WIDGET (dialog));
		hide = TRUE;
	}

	g_timer_stop (priv->total_time);

	if (secondary_message) {
		message = brasero_burn_dialog_create_message (dialog,
		                                              GTK_MESSAGE_WARNING,
		                                              GTK_BUTTONS_CANCEL,
		                                              main_message);

		if (secondary_message)
			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
								  "%s", secondary_message);
	}
	else {
		gchar *string;

		message = brasero_burn_dialog_create_message (dialog,
							      GTK_MESSAGE_WARNING,
							      GTK_BUTTONS_CANCEL,
							      NULL);

		string = g_strdup_printf ("<b><big>%s</big></b>", main_message);
		gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (message), string);
		g_free (string);
	}

	/* connect to signals to be warned when media is inserted */
	added_id = g_signal_connect_after (drive,
					   "medium-added",
					   G_CALLBACK (brasero_burn_dialog_wait_for_insertion_cb),
					   message);

	if (sound_clue) {
		gtk_widget_show (GTK_WIDGET (message));
		ca_gtk_play_for_widget (GTK_WIDGET (message), 0,
		                        CA_PROP_EVENT_ID, "complete-media-burn",
		                        CA_PROP_EVENT_DESCRIPTION, main_message,
		                        NULL);
	}

	result = gtk_dialog_run (GTK_DIALOG (message));

	g_signal_handler_disconnect (drive, added_id);
	gtk_widget_destroy (message);

	if (hide)
		gtk_widget_hide (GTK_WIDGET (dialog));

	g_timer_continue (priv->total_time);

	return result;
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
				if (isosize > 0)
					message = g_strdup_printf (_("Please replace the disc with a writable CD with at least %i MiB of free space."), 
								   (int) (isosize / 1048576));
				else
					message = g_strdup (_("Please replace the disc with a writable CD."));
			}
			else {
				if (isosize > 0)
					message = g_strdup_printf (_("Please insert a writable CD with at least %i MiB of free space."), 
								   (int) (isosize / 1048576));
				else
					message = g_strdup (_("Please insert a writable CD."));
			}
		}
		else if (!(type & BRASERO_MEDIUM_CD) && (type & BRASERO_MEDIUM_DVD)) {
			if (!insert) {
				if (isosize > 0)
					message = g_strdup_printf (_("Please replace the disc with a writable DVD with at least %i MiB of free space."), 
								   (int) (isosize / 1048576));
				else
					message = g_strdup (_("Please replace the disc with a writable DVD."));
			}
			else {
				if (isosize > 0)
					message = g_strdup_printf (_("Please insert a writable DVD with at least %i MiB of free space."), 
								   (int) (isosize / 1048576));
				else
					message = g_strdup (_("Please insert a writable DVD."));
			}
		}
		else if (!insert) {
			if (isosize)
				message = g_strdup_printf (_("Please replace the disc with a writable CD or DVD with at least %i MiB of free space."), 
							   (int) (isosize / 1048576));
			else
				message = g_strdup (_("Please replace the disc with a writable CD or DVD."));
		}
		else {
			if (isosize)
				message = g_strdup_printf (_("Please insert a writable CD or DVD with at least %i MiB of free space."), 
							   (int) (isosize / 1048576));
			else
				message = g_strdup (_("Please insert a writable CD or DVD."));
		}
	}

	return message;
}

static BraseroBurnResult
brasero_burn_dialog_insert_disc_cb (BraseroBurn *burn,
				    BraseroDrive *drive,
				    BraseroBurnError error,
				    BraseroMedia type,
				    BraseroBurnDialog *dialog)
{
	gint result;
	gchar *drive_name;
	BraseroBurnDialogPrivate *priv;
	gchar *main_message = NULL, *secondary_message = NULL;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	if (drive)
		drive_name = brasero_drive_get_display_name (drive);
	else
		drive_name = NULL;

	if (error == BRASERO_BURN_WARNING_INSERT_AFTER_COPY) {
		secondary_message = g_strdup (_("An image of the disc has been created on your hard drive."
						"\nBurning will begin as soon as a writable disc is inserted."));
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

	result = brasero_burn_dialog_wait_for_insertion (dialog, drive, main_message, secondary_message, FALSE);
	g_free (main_message);
	g_free (secondary_message);

	if (result != GTK_RESPONSE_OK)
		return BRASERO_BURN_CANCEL;

	brasero_burn_dialog_update_media (dialog);
	brasero_burn_dialog_update_title (dialog, &priv->input);
	brasero_burn_dialog_update_info (dialog,
	                                 &priv->input,
	                                 (brasero_burn_session_get_flags (priv->session) & BRASERO_BURN_FLAG_DUMMY) != 0);

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
	GtkWidget *message;
	gboolean hide = FALSE;
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	if (!gtk_widget_get_visible (GTK_WIDGET (dialog))) {
		gtk_widget_show (GTK_WIDGET (dialog));
		hide = TRUE;
	}

	g_timer_stop (priv->total_time);

	string = g_strdup_printf ("%s. %s",
				  is_temporary?
				  _("A file could not be created at the location specified for temporary files"):
				  _("The image could not be created at the specified location"),
				  _("Do you want to specify another location for this session or retry with the current location?"));

	message = brasero_burn_dialog_create_message (dialog,
	                                              GTK_MESSAGE_ERROR,
	                                              GTK_BUTTONS_NONE,
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
		g_timer_continue (priv->total_time);
		return BRASERO_BURN_OK;
	}

	if (result != GTK_RESPONSE_ACCEPT) {
		g_timer_continue (priv->total_time);
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
		g_timer_continue (priv->total_time);
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
						 &toc);

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

	g_timer_continue (priv->total_time);
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_dialog_loss_warnings_cb (BraseroBurnDialog *dialog, 
				      const gchar *main_message,
				      const gchar *secondary_message,
				      const gchar *button_text,
				      const gchar *button_icon,
                                      const gchar *second_button_text,
                                      const gchar *second_button_icon)
{
	gint result;
	GtkWidget *button;
	GtkWidget *message;
	gboolean hide = FALSE;
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	if (!gtk_widget_get_visible (GTK_WIDGET (dialog))) {
		gtk_widget_show (GTK_WIDGET (dialog));
		hide = TRUE;
	}

	g_timer_stop (priv->total_time);

	message = brasero_burn_dialog_create_message (dialog,
	                                              GTK_MESSAGE_WARNING,
	                                              GTK_BUTTONS_NONE,
	                                              main_message);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
						 "%s", secondary_message);

	if (second_button_text) {
		button = gtk_dialog_add_button (GTK_DIALOG (message),
						second_button_text,
						GTK_RESPONSE_YES);

		if (second_button_icon)
			gtk_button_set_image (GTK_BUTTON (button),
					      gtk_image_new_from_icon_name (second_button_icon,
									    GTK_ICON_SIZE_BUTTON));
	}

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

	g_timer_continue (priv->total_time);

	if (result == GTK_RESPONSE_YES)
		return BRASERO_BURN_RETRY;

	if (result == GTK_RESPONSE_ACCEPT)
		return BRASERO_BURN_NEED_RELOAD;

	if (result != GTK_RESPONSE_OK)
		return BRASERO_BURN_CANCEL;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_dialog_data_loss_cb (BraseroBurn *burn,
				  BraseroBurnDialog *dialog)
{
	return brasero_burn_dialog_loss_warnings_cb (dialog,
						     _("Do you really want to blank the current disc?"),
						     _("The disc in the drive holds data."),
	                                             /* Translators: Blank is a verb here */
						     _("_Blank Disc"),
						     "media-optical-blank",
	                                             NULL,
	                                             NULL);
}

static BraseroBurnResult
brasero_burn_dialog_previous_session_loss_cb (BraseroBurn *burn,
					      BraseroBurnDialog *dialog)
{
	gchar *secondary;
	BraseroBurnResult result;

	secondary = g_strdup_printf ("%s\n%s",
				     _("If you import them you will be able to see and use them once the current selection of files is burned."),
	                             _("If you don't, they will be invisible (though still readable)."));
				     
	result = brasero_burn_dialog_loss_warnings_cb (dialog,
						       _("There are files already burned on this disc. Would you like to import them?"),
						       secondary,
						       _("_Import"), NULL,
	                                               _("Only _Append"), NULL);
	g_free (secondary);
	return result;
}

static BraseroBurnResult
brasero_burn_dialog_audio_to_appendable_cb (BraseroBurn *burn,
					    BraseroBurnDialog *dialog)
{
	gchar *secondary;
	BraseroBurnResult result;

	secondary = g_strdup_printf ("%s\n%s",
				     _("CD-RW audio discs may not play correctly in older CD players and CD-Text won't be written."),
				     _("Do you want to continue anyway?"));

	result = brasero_burn_dialog_loss_warnings_cb (dialog,
						       _("Appending audio tracks to a CD is not advised."),
						       secondary,
						       _("_Continue"),
						       "media-optical-burn",
	                                               NULL,
	                                               NULL);
	g_free (secondary);
	return result;
}

static BraseroBurnResult
brasero_burn_dialog_rewritable_cb (BraseroBurn *burn,
				   BraseroBurnDialog *dialog)
{
	gchar *secondary;
	BraseroBurnResult result;

	secondary = g_strdup_printf ("%s\n%s",
				     _("CD-RW audio discs may not play correctly in older CD players."),
				     _("Do you want to continue anyway?"));

	result = brasero_burn_dialog_loss_warnings_cb (dialog,
						       _("Recording audio tracks on a rewritable disc is not advised."),
						       secondary,
						       _("_Continue"),
						       "media-optical-burn",
	                                               NULL,
	                                               NULL);
	g_free (secondary);
	return result;
}

static void
brasero_burn_dialog_wait_for_ejection_cb (BraseroDrive *drive,
                                          BraseroMedium *medium,
                                          GtkDialog *message)
{
	/* we might have a dialog waiting for the 
	 * insertion of a disc if so close it */
	gtk_dialog_response (GTK_DIALOG (message), GTK_RESPONSE_OK);
}

static BraseroBurnResult
brasero_burn_dialog_eject_failure_cb (BraseroBurn *burn,
                                      BraseroDrive *drive,
                                      BraseroBurnDialog *dialog)
{
	gint result;
	gchar *name;
	gchar *string;
	gint removal_id;
	GtkWidget *message;
	gboolean hide = FALSE;
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);
	
	if (!gtk_widget_get_visible (GTK_WIDGET (dialog))) {
		gtk_widget_show (GTK_WIDGET (dialog));
		hide = TRUE;
	}

	g_timer_stop (priv->total_time);

	name = brasero_drive_get_display_name (drive);
	/* Translators: %s is the name of a drive */
	string = g_strdup_printf (_("Please eject the disc from \"%s\" manually."), name);
	g_free (name);
	message = brasero_burn_dialog_create_message (dialog,
	                                              GTK_MESSAGE_WARNING,
	                                              GTK_BUTTONS_NONE,
	                                              string);
	g_free (string);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
	                                          _("The disc could not be ejected though it needs to be removed for the current operation to continue."));

	gtk_dialog_add_button (GTK_DIALOG (message),
			       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

	/* connect to signals to be warned when media is removed */
	removal_id = g_signal_connect_after (drive,
	                                     "medium-removed",
	                                     G_CALLBACK (brasero_burn_dialog_wait_for_ejection_cb),
	                                     message);

	result = gtk_dialog_run (GTK_DIALOG (message));

	g_signal_handler_disconnect (drive, removal_id);
	gtk_widget_destroy (message);

	if (hide)
		gtk_widget_hide (GTK_WIDGET (dialog));

	g_timer_continue (priv->total_time);

	if (result == GTK_RESPONSE_ACCEPT)
		return BRASERO_BURN_OK;

	return BRASERO_BURN_CANCEL;
}

static BraseroBurnResult
brasero_burn_dialog_continue_question (BraseroBurnDialog *dialog,
                                       const gchar *primary_message,
                                       const gchar *secondary_message,
                                       const gchar *button_message)
{
	gint result;
	GtkWidget *button;
	GtkWidget *message;
	gboolean hide = FALSE;
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	if (!gtk_widget_get_visible (GTK_WIDGET (dialog))) {
		gtk_widget_show (GTK_WIDGET (dialog));
		hide = TRUE;
	}

	g_timer_stop (priv->total_time);

	message = brasero_burn_dialog_create_message (dialog,
	                                              GTK_MESSAGE_WARNING,
	                                              GTK_BUTTONS_NONE,
	                                              primary_message);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
						  "%s",
	                                          secondary_message);

	gtk_dialog_add_button (GTK_DIALOG (message),
			       GTK_STOCK_CANCEL,
	                       GTK_RESPONSE_CANCEL);

	button = gtk_dialog_add_button (GTK_DIALOG (message),
					button_message,
					GTK_RESPONSE_OK);
	gtk_button_set_image (GTK_BUTTON (button),
			      gtk_image_new_from_stock (GTK_STOCK_OK,
							GTK_ICON_SIZE_BUTTON));
	
	result = gtk_dialog_run (GTK_DIALOG (message));
	gtk_widget_destroy (message);

	if (hide)
		gtk_widget_hide (GTK_WIDGET (dialog));

	g_timer_continue (priv->total_time);

	if (result != GTK_RESPONSE_OK)
		return BRASERO_BURN_CANCEL;

	return BRASERO_BURN_OK;	
}

static BraseroBurnResult
brasero_burn_dialog_blank_failure_cb (BraseroBurn *burn,
                                      BraseroBurnDialog *dialog)
{
	return brasero_burn_dialog_continue_question (dialog,
	                                              _("Do you want to replace the disc and continue?"),
	                                              _("The currently inserted disc could not be blanked."),
	                                              _("_Replace Disc"));
}

static BraseroBurnResult
brasero_burn_dialog_disable_joliet_cb (BraseroBurn *burn,
				       BraseroBurnDialog *dialog)
{
	return brasero_burn_dialog_continue_question (dialog,
	                                              _("Do you want to continue with full Windows compatibility disabled?"),
	                                              _("Some files don't have a suitable name for a fully Windows-compatible CD."),
	                                              _("C_ontinue"));
}

static void
brasero_burn_dialog_update_title_writing_progress (BraseroBurnDialog *dialog,
						   BraseroTrackType *input,
						   BraseroMedia media,
						   guint percent)
{
	gchar *title = NULL;
	gchar *icon_name;
	guint remains;

	/* This is used only when actually writing to a disc */
	if (media == BRASERO_MEDIUM_FILE)
		title = brasero_burn_dialog_create_dialog_title_for_action (dialog,
		                                                            _("Creating Image"),
		                                                            percent);
	else if (media & BRASERO_MEDIUM_DVD) {
		if (brasero_track_type_get_has_medium (input))
			title = brasero_burn_dialog_create_dialog_title_for_action (dialog,
			                                                            _("Copying DVD"),
			                                                            percent);
		else
			title = brasero_burn_dialog_create_dialog_title_for_action (dialog,
			                                                            _("Burning DVD"),
			                                                            percent);
	}
	else if (media & BRASERO_MEDIUM_CD) {
		if (brasero_track_type_get_has_medium (input))
			title = brasero_burn_dialog_create_dialog_title_for_action (dialog,
			                                                            _("Copying CD"),
			                                                            percent);
		else
			title = brasero_burn_dialog_create_dialog_title_for_action (dialog,
			                                                            _("Burning CD"),
			                                                            percent);
	}
	else {
		if (brasero_track_type_get_has_medium (input))
			title = brasero_burn_dialog_create_dialog_title_for_action (dialog,
			                                                            _("Copying Disc"),
			                                                            percent);
		else
			title = brasero_burn_dialog_create_dialog_title_for_action (dialog,
			                                                            _("Burning Disc"),
			                                                            percent);
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
	goffset isosize = -1;
	goffset written = -1;
	guint64 rate = -1;

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
	GtkWidget *button;
	gboolean hide;
	gint id;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	if (!gtk_widget_get_mapped (GTK_WIDGET (dialog))) {
		gtk_widget_show (GTK_WIDGET (dialog));
		hide = TRUE;
	} else
		hide = FALSE;

	g_timer_stop (priv->total_time);

	message = brasero_burn_dialog_create_message (dialog,
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

	gtk_widget_show (GTK_WIDGET (message));
	ca_gtk_play_for_widget (GTK_WIDGET (message), 0,
	                        CA_PROP_EVENT_ID, "complete-media-burn-test",
	                        CA_PROP_EVENT_DESCRIPTION, _("The simulation was successful."),
	                        NULL);

	answer = gtk_dialog_run (GTK_DIALOG (message));
	gtk_widget_destroy (message);

	gtk_window_set_urgency_hint (GTK_WINDOW (dialog), FALSE);

	if (hide)
		gtk_widget_hide (GTK_WIDGET (dialog));

	g_timer_continue (priv->total_time);

	if (answer == GTK_RESPONSE_OK) {
		if (priv->initial_icon)
			gtk_window_set_icon_name (GTK_WINDOW (dialog), priv->initial_icon);
		else
			gtk_window_set_icon_name (GTK_WINDOW (dialog), "brasero-00.png");

		brasero_burn_dialog_update_info (dialog,
		                                 &priv->input,
		                                 FALSE);
		brasero_burn_dialog_update_title (dialog, &priv->input);

		if (id)
			g_source_remove (id);

		return BRASERO_BURN_OK;
	}

	if (id)
		g_source_remove (id);

	return BRASERO_BURN_CANCEL;
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
		gdk_window_set_cursor (window, cursor);
		g_object_unref (cursor);
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

	gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (dialog)), NULL);

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
	/* Restore title */
	if (priv->initial_title)
		gtk_window_set_title (GTK_WINDOW (dialog), priv->initial_title);
	else
		brasero_burn_dialog_update_title (dialog, &priv->input);

	/* Restore icon */
	if (priv->initial_icon)
		gtk_window_set_icon_name (GTK_WINDOW (dialog), priv->initial_icon);

	gtk_widget_show (GTK_WIDGET (dialog));
	gtk_window_set_urgency_hint (GTK_WINDOW (dialog), TRUE);
}

static BraseroBurnResult
brasero_burn_dialog_install_missing_cb (BraseroBurn *burn,
					BraseroPluginErrorType type,
					const gchar *detail,
					gpointer user_data)
{
	BraseroBurnDialogPrivate *priv = BRASERO_BURN_DIALOG_PRIVATE (user_data);
	GCancellable *cancel;
	BraseroPK *package;
	GdkWindow *window;
	gboolean res;
	int xid = 0;

	gtk_widget_set_sensitive (GTK_WIDGET (user_data), FALSE);

	/* Get the xid */
	window = gtk_widget_get_window (user_data);
	xid = gdk_x11_window_get_xid (window);

	package = brasero_pk_new ();
	cancel = g_cancellable_new ();
	priv->cancel_plugin = cancel;
	switch (type) {
		case BRASERO_PLUGIN_ERROR_MISSING_APP:
			res = brasero_pk_install_missing_app (package, detail, xid, cancel);
			break;

		case BRASERO_PLUGIN_ERROR_MISSING_LIBRARY:
			res = brasero_pk_install_missing_library (package, detail, xid, cancel);
			break;

		case BRASERO_PLUGIN_ERROR_MISSING_GSTREAMER_PLUGIN:
			res = brasero_pk_install_gstreamer_plugin (package, detail, xid, cancel);
			break;

		default:
			res = FALSE;
			break;
	}

	if (package) {
		g_object_unref (package);
		package = NULL;
	}

	gtk_widget_set_sensitive (GTK_WIDGET (user_data), TRUE);
	if (g_cancellable_is_cancelled (cancel)) {
		g_object_unref (cancel);
		return BRASERO_BURN_CANCEL;
	}

	priv->cancel_plugin = NULL;
	g_object_unref (cancel);

	if (!res)
		return BRASERO_BURN_ERR;

	return BRASERO_BURN_RETRY;
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
			  "install-missing",
			  G_CALLBACK (brasero_burn_dialog_install_missing_cb),
			  dialog);
	g_signal_connect (priv->burn,
			  "insert-media",
			  G_CALLBACK (brasero_burn_dialog_insert_disc_cb),
			  dialog);
	g_signal_connect (priv->burn,
			  "blank-failure",
			  G_CALLBACK (brasero_burn_dialog_blank_failure_cb),
			  dialog);
	g_signal_connect (priv->burn,
			  "eject-failure",
			  G_CALLBACK (brasero_burn_dialog_eject_failure_cb),
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

	brasero_burn_progress_set_action (BRASERO_BURN_PROGRESS (priv->progress),
					  BRASERO_BURN_ACTION_NONE,
					  NULL);

	g_timer_continue (priv->total_time);

	return BRASERO_BURN_OK;
}

static void
brasero_burn_dialog_save_log (BraseroBurnDialog *dialog)
{
	GError *error;
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
	gtk_window_set_icon_name (GTK_WINDOW (chooser), gtk_window_get_icon_name (GTK_WINDOW (dialog)));

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

	error = NULL;
	if (!g_file_get_contents (brasero_burn_session_get_log_path (priv->session),
	                          &contents,
	                          NULL,
	                          &error)) {
		g_warning ("Error while saving log file: %s\n", error? error->message:"none");
		g_error_free (error);
		g_free (path);
		return;
	}
	
	g_file_set_contents (path, contents, -1, NULL);

	g_free (contents);
	g_free (path);
}

static void
brasero_burn_dialog_notify_error (BraseroBurnDialog *dialog,
				  GError *error)
{
	gchar *secondary;
	GtkWidget *button;
	GtkWidget *message;
	GtkResponseType response;
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	/* Restore title */
	if (priv->initial_title)
		gtk_window_set_title (GTK_WINDOW (dialog), priv->initial_title);

	/* Restore icon */
	if (priv->initial_icon)
		gtk_window_set_icon_name (GTK_WINDOW (dialog), priv->initial_icon);

	if (error) {
		secondary =  g_strdup (error->message);
		g_error_free (error);
	}
	else
		secondary = g_strdup (_("An unknown error occurred."));

	if (!gtk_widget_get_visible (GTK_WIDGET (dialog)))
		gtk_widget_show (GTK_WIDGET (dialog));

	message = brasero_burn_dialog_create_message (dialog,
	                                              GTK_MESSAGE_ERROR,
	                                              GTK_BUTTONS_NONE,
	                                              _("Error while burning."));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
						  "%s",
						  secondary);
	g_free (secondary);

	button = gtk_dialog_add_button (GTK_DIALOG (message),
					_("_Save Log"),
					GTK_RESPONSE_OK);
	gtk_button_set_image (GTK_BUTTON (button),
			      gtk_image_new_from_stock (GTK_STOCK_SAVE_AS,
							GTK_ICON_SIZE_BUTTON));

	gtk_dialog_add_button (GTK_DIALOG (message),
			       GTK_STOCK_CLOSE,
			       GTK_RESPONSE_CLOSE);

	brasero_burn_dialog_notify_daemon (dialog, _("Error while burning."));

	response = gtk_dialog_run (GTK_DIALOG (message));
	while (response == GTK_RESPONSE_OK) {
		brasero_burn_dialog_save_log (dialog);
		response = gtk_dialog_run (GTK_DIALOG (message));
	}

	gtk_widget_destroy (message);
}

static gchar *
brasero_burn_dialog_get_success_message (BraseroBurnDialog *dialog)
{
	BraseroBurnDialogPrivate *priv;
	BraseroDrive *drive;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	drive = brasero_burn_session_get_burner (priv->session);

	if (brasero_track_type_get_has_stream (&priv->input)) {
		if (!brasero_drive_is_fake (drive)) {
			if (BRASERO_STREAM_FORMAT_HAS_VIDEO (brasero_track_type_get_stream_format (&priv->input))) {
				if (priv->media & BRASERO_MEDIUM_DVD)
					return g_strdup (_("Video DVD successfully burned"));

				return g_strdup (_("(S)VCD successfully burned"));
			}
			else
				return g_strdup (_("Audio CD successfully burned"));
		}
		return g_strdup (_("Image successfully created"));
	}
	else if (brasero_track_type_get_has_medium (&priv->input)) {
		if (!brasero_drive_is_fake (drive)) {
			if (priv->media & BRASERO_MEDIUM_DVD)
				return g_strdup (_("DVD successfully copied"));
			else
				return g_strdup (_("CD successfully copied"));
		}
		else {
			if (priv->media & BRASERO_MEDIUM_DVD)
				return g_strdup (_("Image of DVD successfully created"));
			else
				return g_strdup (_("Image of CD successfully created"));
		}
	}
	else if (brasero_track_type_get_has_image (&priv->input)) {
		if (!brasero_drive_is_fake (drive)) {
			if (priv->media & BRASERO_MEDIUM_DVD)
				return g_strdup (_("Image successfully burned to DVD"));
			else
				return g_strdup (_("Image successfully burned to CD"));
		}
	}
	else if (brasero_track_type_get_has_data (&priv->input)) {
		if (!brasero_drive_is_fake (drive)) {
			if (priv->media & BRASERO_MEDIUM_DVD)
				return g_strdup (_("Data DVD successfully burned"));
			else
				return g_strdup (_("Data CD successfully burned"));
		}
		return g_strdup (_("Image successfully created"));
	}

	return NULL;
}

static void
brasero_burn_dialog_update_session_info (BraseroBurnDialog *dialog)
{
	gint64 rate;
	gchar *primary = NULL;
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	primary = brasero_burn_dialog_get_success_message (dialog);
	brasero_burn_dialog_activity_stop (dialog, primary);
	g_free (primary);

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
						    priv->media,
						    priv->total_size);
}

static gboolean
brasero_burn_dialog_notify_copy_finished (BraseroBurnDialog *dialog,
                                          GError *error)
{
	gulong added_id;
	BraseroDrive *drive;
	GtkWidget *message;
	gchar *main_message;
	GtkResponseType response;
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	brasero_burn_dialog_update_session_info (dialog);

	if (!gtk_widget_get_visible (GTK_WIDGET (dialog)))
		gtk_widget_show (GTK_WIDGET (dialog));

	main_message = g_strdup_printf (_("Copy #%i has been burned successfully."), priv->num_copies ++);
	message = brasero_burn_dialog_create_message (dialog,
	                                              GTK_MESSAGE_INFO,
	                                              GTK_BUTTONS_CANCEL,
	                                              main_message);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
						  "%s",
						  _("Another copy will start as soon as you insert a new writable disc. If you do not want to burn another copy, press \"Cancel\"."));

	/* connect to signals to be warned when media is inserted */
	drive = brasero_burn_session_get_burner (priv->session);
	added_id = g_signal_connect_after (drive,
					   "medium-added",
					   G_CALLBACK (brasero_burn_dialog_wait_for_insertion_cb),
					   message);

	gtk_widget_show (GTK_WIDGET (message));
	ca_gtk_play_for_widget (GTK_WIDGET (message), 0,
	                        CA_PROP_EVENT_ID, "complete-media-burn",
	                        CA_PROP_EVENT_DESCRIPTION, main_message,
	                        NULL);

	brasero_burn_dialog_notify_daemon (dialog, main_message);
	g_free (main_message);

	response = gtk_dialog_run (GTK_DIALOG (message));

	g_signal_handler_disconnect (drive, added_id);
	gtk_widget_destroy (message);

	return (response == GTK_RESPONSE_OK);
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
		/* This strange hack is a way to workaround #568358.
		 * At one point we'll need to hide the dialog which means it
		 * will anwer with a GTK_RESPONSE_NONE */
		while (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_NONE)
			gtk_widget_show (GTK_WIDGET (dialog));

		gtk_widget_destroy (window);
		return FALSE;
	}

	return (answer == GTK_RESPONSE_OK);
}

static gboolean
brasero_burn_dialog_notify_success (BraseroBurnDialog *dialog)
{
	gboolean res;
	gchar *primary = NULL;
	BraseroBurnDialogPrivate *priv;
	GtkWidget *create_cover = NULL;
	GtkWidget *make_another = NULL;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	brasero_burn_dialog_update_session_info (dialog);

	/* Don't show the "Make _More Copies" button if:
	 * - we wrote to a file
	 * - we wrote a merged session
	 * - we were not already asked for a series of copy */
	if (!priv->num_copies
	&&  !brasero_burn_session_is_dest_file (priv->session)
	&& !(brasero_burn_session_get_flags (priv->session) & BRASERO_BURN_FLAG_MERGE)) {
		/* Useful button but it shouldn't be used for images */
		make_another = gtk_dialog_add_button (GTK_DIALOG (dialog),
						      _("Make _More Copies"),
						      GTK_RESPONSE_OK);
	}

	if (brasero_track_type_get_has_stream (&priv->input)
	|| (brasero_track_type_get_has_medium (&priv->input)
	&& (brasero_track_type_get_medium_type (&priv->input) & BRASERO_MEDIUM_HAS_AUDIO))) {
		/* since we succeed offer the possibility to create cover if that's an audio disc */
		create_cover = gtk_dialog_add_button (GTK_DIALOG (dialog),
						      _("Create Co_ver"),
						      GTK_RESPONSE_CLOSE);
	}

	primary = brasero_burn_dialog_get_success_message (dialog);
	gtk_widget_show(GTK_WIDGET(dialog));
	ca_gtk_play_for_widget(GTK_WIDGET(dialog), 0,
			       CA_PROP_EVENT_ID, "complete-media-burn",
			       CA_PROP_EVENT_DESCRIPTION, primary,
			       NULL);

	brasero_burn_dialog_notify_daemon (dialog, primary);
	g_free (primary);

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

	g_timer_stop (priv->total_time);

	if (result == BRASERO_BURN_CANCEL) {
		/* nothing to do */
	}
	else if (error || result != BRASERO_BURN_OK) {
		brasero_burn_dialog_notify_error (dialog, error);
	}
	else if (priv->num_copies) {
		retry = brasero_burn_dialog_notify_copy_finished (dialog, error);
		if (!retry)
			brasero_burn_dialog_notify_success (dialog);
	}
	else {
		/* see if an image was created. If so, add it to GtkRecent. */
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
		priv->num_copies = retry * 2;
	}

	if (priv->burn) {
		g_object_unref (priv->burn);
		priv->burn = NULL;
	}

	if (priv->rates) {
		g_slist_free (priv->rates);
		priv->rates = NULL;
	}

	return retry;
}

static BraseroBurnResult
brasero_burn_dialog_record_spanned_session (BraseroBurnDialog *dialog,
					    GError **error)
{
	BraseroDrive *burner;
	BraseroTrackType *type;
	gchar *success_message;
	BraseroBurnResult result;
	BraseroBurnDialogPrivate *priv;
	gchar *secondary_message = NULL;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);
	burner = brasero_burn_session_get_burner (priv->session);

	/* Get the messages now as they can change */
	type = brasero_track_type_new ();
	brasero_burn_session_get_input_type (priv->session, type);
	success_message = brasero_burn_dialog_get_success_message (dialog);
	if (brasero_track_type_get_has_data (type)) {
		secondary_message = g_strdup_printf ("%s.\n%s.",
						     success_message,
						     _("There are some files left to burn"));
		g_free (success_message);
	}
	else if (brasero_track_type_get_has_stream (type)) {
		if (BRASERO_STREAM_FORMAT_HAS_VIDEO (brasero_track_type_get_stream_format (type)))
			secondary_message = g_strdup_printf ("%s.\n%s.",
							     success_message,
							     _("There are some more videos left to burn"));
		else
			secondary_message = g_strdup_printf ("%s.\n%s.",
							     success_message,
							     _("There are some more songs left to burn"));
		g_free (success_message);
	}
	else
		secondary_message = success_message;

	brasero_track_type_free (type);

	do {
		gint res;

		result = brasero_burn_record (priv->burn,
					      priv->session,
					      error);
		if (result != BRASERO_BURN_OK) {
			g_free (secondary_message);
			return result;
		}

		/* See if we have more data to burn and ask for a new medium */
		result = brasero_session_span_again (BRASERO_SESSION_SPAN (priv->session));
		if (result == BRASERO_BURN_OK)
			break;

		res = brasero_burn_dialog_wait_for_insertion (dialog,
							      burner,
							      _("Please insert a writable CD or DVD."),
							      secondary_message, 
		                                              TRUE);

		if (res != GTK_RESPONSE_OK) {
			g_free (secondary_message);
			return BRASERO_BURN_CANCEL;
		}

		result = brasero_session_span_next (BRASERO_SESSION_SPAN (priv->session));
		while (result == BRASERO_BURN_ERR) {
			brasero_drive_eject (burner, FALSE, NULL);
			res = brasero_burn_dialog_wait_for_insertion (dialog,
								      burner,
								      _("Please insert a writable CD or DVD."),
								      _("Not enough space available on the disc"),
			                                              FALSE);
			if (res != GTK_RESPONSE_OK) {
				g_free (secondary_message);
				return BRASERO_BURN_CANCEL;
			}
			result = brasero_session_span_next (BRASERO_SESSION_SPAN (priv->session));
		}

	} while (result == BRASERO_BURN_RETRY);

	g_free (secondary_message);
	brasero_session_span_stop (BRASERO_SESSION_SPAN (priv->session));
	return result;
}

static BraseroBurnResult
brasero_burn_dialog_record_session (BraseroBurnDialog *dialog)
{
	gboolean retry;
	GError *error = NULL;
	BraseroBurnResult result;
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	/* Update info */
	brasero_burn_dialog_update_info (dialog,
	                                 &priv->input,
	                                 (brasero_burn_session_get_flags (priv->session) & BRASERO_BURN_FLAG_DUMMY) != 0);
	brasero_burn_dialog_update_title (dialog, &priv->input);

	/* Start the recording session */
	brasero_burn_dialog_activity_start (dialog);
	result = brasero_burn_dialog_setup_session (dialog, &error);
	if (result != BRASERO_BURN_OK)
		return result;

	if (BRASERO_IS_SESSION_SPAN (priv->session))
		result = brasero_burn_dialog_record_spanned_session (dialog, &error);
	else
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
brasero_burn_dialog_wait_for_ready_state_cb (BraseroBurnDialog *dialog)
{
	BraseroBurnDialogPrivate *priv;
	BraseroStatus *status;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	status = brasero_status_new ();
	brasero_burn_session_get_status (priv->session, status);

	if (brasero_status_get_result (status) == BRASERO_BURN_NOT_READY
	||  brasero_status_get_result (status) == BRASERO_BURN_RUNNING) {
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
							   priv->media);
		g_object_unref (status);

		/* Continue */
		return TRUE;
	}

	if (priv->loop)
		g_main_loop_quit (priv->loop);

	priv->wait_ready_state_id = 0;

	g_object_unref (status);
	return FALSE;
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
	if (result == BRASERO_BURN_NOT_READY || result == BRASERO_BURN_RUNNING) {
		GMainLoop *loop;

		loop = g_main_loop_new (NULL, FALSE);
		priv->loop = loop;

		priv->wait_ready_state_id = g_timeout_add_seconds (1,
								   (GSourceFunc) brasero_burn_dialog_wait_for_ready_state_cb,
								   dialog);
		g_main_loop_run (loop);

		priv->loop = NULL;

		if (priv->wait_ready_state_id) {
			g_source_remove (priv->wait_ready_state_id);
			priv->wait_ready_state_id = 0;
		}

		g_main_loop_unref (loop);

		/* Get the final status */
		result = brasero_burn_session_get_status (priv->session, status);
	}

	g_object_unref (status);

	return (result == BRASERO_BURN_OK);
}

static gboolean
brasero_burn_dialog_run_real (BraseroBurnDialog *dialog,
			      BraseroBurnSession *session)
{
	BraseroBurnResult result;
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	g_object_ref (session);
	priv->session = session;

	/* update what we should display */
	brasero_burn_session_get_input_type (session, &priv->input);
	brasero_burn_dialog_update_media (dialog);

	/* show it early */
	gtk_widget_show (GTK_WIDGET (dialog));

	/* wait for ready state */
	if (!brasero_burn_dialog_wait_for_ready_state (dialog))
		return FALSE;

	/* disable autoconfiguration */
	if (BRASERO_IS_SESSION_CFG (priv->session))
		brasero_session_cfg_disable (BRASERO_SESSION_CFG (priv->session));

	priv->total_time = g_timer_new ();
	g_timer_stop (priv->total_time);

	priv->initial_title = g_strdup (gtk_window_get_title (GTK_WINDOW (dialog)));
	priv->initial_icon = g_strdup (gtk_window_get_icon_name (GTK_WINDOW (dialog)));

	do {
		if (!gtk_widget_get_visible (GTK_WIDGET (dialog)))
			gtk_widget_show (GTK_WIDGET (dialog));

		result = brasero_burn_dialog_record_session (dialog);
	} while (result == BRASERO_BURN_RETRY);

	if (priv->initial_title) {
		g_free (priv->initial_title);
		priv->initial_title = NULL;
	}

	if (priv->initial_icon) {
		g_free (priv->initial_icon);
		priv->initial_icon = NULL;
	}

	g_timer_destroy (priv->total_time);
	priv->total_time = NULL;

	priv->session = NULL;
	g_object_unref (session);

	/* re-enable autoconfiguration */
	if (BRASERO_IS_SESSION_CFG (priv->session))
		brasero_session_cfg_enable (BRASERO_SESSION_CFG (priv->session));

	return (result == BRASERO_BURN_OK);
}

/**
 * brasero_burn_dialog_run_multi:
 * @dialog: a #BraseroBurnDialog
 * @session: a #BraseroBurnSession
 *
 * Start burning the contents of @session. Once a disc is burnt, a dialog
 * will be shown to the user and wait for a new insertion before starting to burn
 * again.
 *
 * Return value: a #gboolean. TRUE if the operation was successfully carried out, FALSE otherwise.
 **/
gboolean
brasero_burn_dialog_run_multi (BraseroBurnDialog *dialog,
			       BraseroBurnSession *session)
{
	BraseroBurnResult result;
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);
	priv->num_copies = 1;

	result = brasero_burn_dialog_run_real (dialog, session);
	return (result == BRASERO_BURN_OK);
}


/**
 * brasero_burn_dialog_run:
 * @dialog: a #BraseroBurnDialog
 * @session: a #BraseroBurnSession
 *
 * Start burning the contents of @session.
 *
 * Return value: a #gboolean. TRUE if the operation was successfully carried out, FALSE otherwise.
 **/
gboolean
brasero_burn_dialog_run (BraseroBurnDialog *dialog,
			 BraseroBurnSession *session)
{
	BraseroBurnResult result;
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);
	priv->num_copies = 0;

	result = brasero_burn_dialog_run_real (dialog, session);
	return (result == BRASERO_BURN_OK);
}

static gboolean
brasero_burn_dialog_cancel_dialog (BraseroBurnDialog *toplevel)
{
	gint result;
	GtkWidget *button;
	GtkWidget *message;

	message = brasero_burn_dialog_create_message (toplevel,
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

/**
 * brasero_burn_dialog_cancel:
 * @dialog: a #BraseroBurnDialog
 * @force_cancellation: a #gboolean
 *
 * Cancel the ongoing operation run by @dialog; if @force_cancellation is FALSE then it can
 * happen that the operation won't be cancelled if there is a risk to make a disc unusable.
 *
 * Return value: a #gboolean. TRUE if it was sucessfully cancelled, FALSE otherwise.
 **/
gboolean
brasero_burn_dialog_cancel (BraseroBurnDialog *dialog,
			    gboolean force_cancellation)
{
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (dialog);

	if (priv->loop) {
		g_main_loop_quit (priv->loop);
		return TRUE;
	}

	if (!priv->burn)
		return TRUE;

	if (brasero_burn_cancel (priv->burn, (force_cancellation == TRUE)) == BRASERO_BURN_DANGEROUS) {
		if (!brasero_burn_dialog_cancel_dialog (dialog))
			return FALSE;

		brasero_burn_cancel (priv->burn, FALSE);
	}

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
brasero_burn_dialog_init (BraseroBurnDialog * obj)
{
	GtkWidget *box;
	GtkWidget *vbox;
	GtkWidget *alignment;
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (obj);

	gtk_window_set_default_size (GTK_WINDOW (obj), 500, 0);

	alignment = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
	gtk_widget_show (alignment);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 8, 6, 6);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (obj))),
			    alignment,
			    TRUE,
			    TRUE,
			    0);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (alignment), vbox);

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
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
brasero_burn_dialog_finalize (GObject * object)
{
	BraseroBurnDialogPrivate *priv;

	priv = BRASERO_BURN_DIALOG_PRIVATE (object);

	if (priv->wait_ready_state_id) {
		g_source_remove (priv->wait_ready_state_id);
		priv->wait_ready_state_id = 0;
	}

	if (priv->cancel_plugin) {
		g_cancellable_cancel (priv->cancel_plugin);
		priv->cancel_plugin = NULL;
	}

	if (priv->initial_title) {
		g_free (priv->initial_title);
		priv->initial_title = NULL;
	}

	if (priv->initial_icon) {
		g_free (priv->initial_icon);
		priv->initial_icon = NULL;
	}

	if (priv->burn) {
		brasero_burn_cancel (priv->burn, TRUE);
		g_object_unref (priv->burn);
		priv->burn = NULL;
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

	G_OBJECT_CLASS (brasero_burn_dialog_parent_class)->finalize (object);
}

static void
brasero_burn_dialog_class_init (BraseroBurnDialogClass * klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroBurnDialogPrivate));

	object_class->finalize = brasero_burn_dialog_finalize;
}

/**
 * brasero_burn_dialog_new:
 *
 * Creates a new #BraseroBurnDialog object
 *
 * Return value: a #GtkWidget. Unref when it is not needed anymore.
 **/

GtkWidget *
brasero_burn_dialog_new (void)
{
	BraseroBurnDialog *obj;

	obj = BRASERO_BURN_DIALOG (g_object_new (BRASERO_TYPE_BURN_DIALOG, NULL));

	return GTK_WIDGET (obj);
}
