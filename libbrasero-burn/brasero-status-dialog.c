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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtk.h>

#include "brasero-units.h"

#include "brasero-enums.h"
#include "brasero-session.h"
#include "brasero-status-dialog.h"

typedef struct _BraseroStatusDialogPrivate BraseroStatusDialogPrivate;
struct _BraseroStatusDialogPrivate
{
	BraseroBurnSession *session;
	GtkWidget *progress;
	GtkWidget *action;
};

#define BRASERO_STATUS_DIALOG_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_STATUS_DIALOG, BraseroStatusDialogPrivate))

G_DEFINE_TYPE (BraseroStatusDialog, brasero_status_dialog, GTK_TYPE_MESSAGE_DIALOG);


static void
brasero_status_dialog_update (BraseroStatusDialog *self,
			      BraseroStatus *status)
{
	gchar *string;
	gchar *size_str;
	gsize session_bytes;
	gchar *current_action;
	BraseroBurnResult res;
	BraseroTrackType *type;
	BraseroStatusDialogPrivate *priv;

	priv = BRASERO_STATUS_DIALOG_PRIVATE (self);

	current_action = brasero_status_get_current_action (status);
	if (current_action) {
		gchar *string;

		string = g_strdup_printf ("<i>%s</i>", current_action);
		gtk_label_set_markup (GTK_LABEL (priv->action), string);
		g_free (string);
	}
	else
		gtk_label_set_markup (GTK_LABEL (priv->action), "");

	g_free (current_action);

	if (brasero_status_get_progress (status) < 0.0)
		gtk_progress_bar_pulse (GTK_PROGRESS_BAR (priv->progress));
	else
		gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (priv->progress),
					       brasero_status_get_progress (status));

	res = brasero_burn_session_get_size (priv->session,
					     NULL,
					     &session_bytes);
	if (res != BRASERO_BURN_OK)
		return;

	type = brasero_track_type_new ();
	brasero_burn_session_get_input_type (priv->session, type);

	if (brasero_track_type_get_has_stream (type))
		size_str = brasero_units_get_time_string (session_bytes, TRUE, FALSE);
	/* NOTE: this is perfectly fine as brasero_track_type_get_medium_type ()
	 * will return BRASERO_MEDIUM_NONE if this is not a MEDIUM track type */
	else if (brasero_track_type_get_medium_type (type) & BRASERO_MEDIUM_HAS_AUDIO)
		size_str = brasero_units_get_time_string (session_bytes, TRUE, FALSE);
	else
		size_str = g_format_size_for_display (session_bytes);

	brasero_track_type_free (type);

	string = g_strdup_printf (_("Project estimated size: %s"), size_str);
	g_free (size_str);

	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (priv->progress), string);
	g_free (string);
}

static gboolean
brasero_status_dialog_wait_for_ready_state (BraseroStatusDialog *dialog)
{
	BraseroStatusDialogPrivate *priv;
	BraseroBurnResult result;
	BraseroStatus *status;

	priv = BRASERO_STATUS_DIALOG_PRIVATE (dialog);

	status = brasero_status_new ();
	result = brasero_burn_session_get_status (priv->session, status);

	if (result != BRASERO_BURN_NOT_READY) {
		gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
		brasero_status_free (status);
		return FALSE;
	}

	brasero_status_dialog_update (dialog, status);
	brasero_status_free (status);

	return TRUE;
}

BraseroBurnResult
brasero_status_dialog_wait_for_session (BraseroStatusDialog *dialog,
					GtkWidget *toplevel,
					BraseroBurnSession *session)
{
	int id;
	int answer;
	BraseroStatus *status;
	BraseroBurnResult result;
	BraseroStatusDialogPrivate *priv;

	/* Make sure we really need to run this dialog */
	status = brasero_status_new ();
	result = brasero_burn_session_get_status (session, status);

	if (result != BRASERO_BURN_NOT_READY) {
		brasero_status_free (status);
		return result;
	}

	if (toplevel) {
		gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
		gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);
		gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	}

	priv = BRASERO_STATUS_DIALOG_PRIVATE (dialog);
	priv->session = g_object_ref (session);

	/* we are not ready to use the track presumably because
	 * data or audio has not finished to explore a directory
	 * or get the metadata of a song or a film  */

	brasero_status_dialog_update (dialog, status);
	brasero_status_free (status);

	id = g_timeout_add (100,
			    (GSourceFunc) brasero_status_dialog_wait_for_ready_state,
		            dialog);

	answer = gtk_dialog_run (GTK_DIALOG (dialog));

	g_source_remove (id);

	priv->session = NULL;
	g_object_unref (session);

	if (answer == GTK_RESPONSE_OK)
		return BRASERO_BURN_OK;

	return brasero_burn_session_get_status (session, NULL);
}

static void
brasero_status_dialog_init (BraseroStatusDialog *object)
{
	BraseroStatusDialogPrivate *priv;
	GtkWidget *main_box;
	GtkWidget *box;

	priv = BRASERO_STATUS_DIALOG_PRIVATE (object);

	gtk_dialog_add_button (GTK_DIALOG (object),
			       GTK_STOCK_CANCEL,
			       GTK_RESPONSE_CANCEL);

	box = gtk_vbox_new (FALSE, 4);
	gtk_widget_show (box);
	main_box = gtk_dialog_get_content_area (GTK_DIALOG (object));
	gtk_box_pack_end (GTK_BOX (main_box),
			  box,
			  TRUE,
			  TRUE,
			  0);

	priv->progress = gtk_progress_bar_new ();
	gtk_widget_show (priv->progress);
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (priv->progress), " ");
	gtk_box_pack_start (GTK_BOX (box),
			    priv->progress,
			    TRUE,
			    TRUE,
			    0);

	priv->action = gtk_label_new ("");
	gtk_widget_show (priv->action);
	gtk_label_set_use_markup (GTK_LABEL (priv->action), TRUE);
	gtk_misc_set_alignment (GTK_MISC (priv->action), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (box),
			    priv->action,
			    FALSE,
			    TRUE,
			    0);
}

static void
brasero_status_dialog_finalize (GObject *object)
{
	G_OBJECT_CLASS (brasero_status_dialog_parent_class)->finalize (object);
}

static void
brasero_status_dialog_class_init (BraseroStatusDialogClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroStatusDialogPrivate));

	object_class->finalize = brasero_status_dialog_finalize;
}

GtkWidget *
brasero_status_dialog_new (void)
{
	return g_object_new (BRASERO_TYPE_STATUS_DIALOG,
			     "title",  _("Project Size Estimation"),
			     "message-type", GTK_MESSAGE_OTHER,
			     "text", _("Please wait until the estimation of the project size is completed."),
			     "secondary-text", _("All files from the project need to be analysed to complete this operation."),
			     NULL);
}
