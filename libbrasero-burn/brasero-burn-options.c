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

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "burn-basics.h"

#include "brasero-medium.h"
#include "brasero-medium-selection-priv.h"

#include "burn-debug.h"
#include "brasero-session.h"
#include "brasero-session-helper.h"
#include "brasero-burn-options.h"
#include "brasero-video-options.h"
#include "brasero-src-image.h"
#include "brasero-src-selection.h"
#include "brasero-session-cfg.h"
#include "brasero-dest-selection.h"
#include "brasero-medium-properties.h"
#include "brasero-status-dialog.h"
#include "brasero-track-stream.h"
#include "brasero-track-data-cfg.h"
#include "brasero-track-image-cfg.h"

#include "brasero-notify.h"
#include "brasero-misc.h"
#include "brasero-pk.h"

typedef struct _BraseroBurnOptionsPrivate BraseroBurnOptionsPrivate;
struct _BraseroBurnOptionsPrivate
{
	BraseroSessionCfg *session;

	GtkSizeGroup *size_group;

	GtkWidget *source;
	GtkWidget *source_placeholder;
	GtkWidget *message_input;
	GtkWidget *selection;
	GtkWidget *properties;
	GtkWidget *message_output;
	GtkWidget *options;
	GtkWidget *options_placeholder;
	GtkWidget *button;

	GtkWidget *burn;
	GtkWidget *burn_multi;

	guint not_ready_id;
	GtkWidget *status_dialog;

	GCancellable *cancel;

	guint is_valid:1;

	guint has_image:1;
	guint has_audio:1;
	guint has_video:1;
	guint has_data:1;
	guint has_disc:1;
};

#define BRASERO_BURN_OPTIONS_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_BURN_OPTIONS, BraseroBurnOptionsPrivate))

enum {
	PROP_0,
	PROP_SESSION
};

G_DEFINE_TYPE (BraseroBurnOptions, brasero_burn_options, GTK_TYPE_DIALOG);

static void
brasero_burn_options_add_source (BraseroBurnOptions *self,
				 const gchar *title,
				 ...)
{
	va_list vlist;
	GtkWidget *child;
	GSList *list = NULL;
	BraseroBurnOptionsPrivate *priv;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (self);

	/* create message queue for input */
	priv->message_input = brasero_notify_new ();
	list = g_slist_prepend (list, priv->message_input);

	va_start (vlist, title);
	while ((child = va_arg (vlist, GtkWidget *))) {
		GtkWidget *hbox;
		GtkWidget *alignment;

		hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
		gtk_widget_show (hbox);

		gtk_box_pack_start (GTK_BOX (hbox), child, TRUE, TRUE, 0);

		alignment = gtk_alignment_new (0.0, 0.5, 0., 0.);
		gtk_widget_show (alignment);
		gtk_size_group_add_widget (priv->size_group, alignment);
		gtk_box_pack_start (GTK_BOX (hbox), alignment, FALSE, FALSE, 0);

		list = g_slist_prepend (list, hbox);
	}
	va_end (vlist);

	priv->source = brasero_utils_pack_properties_list (title, list);
	g_slist_free (list);

	gtk_container_add (GTK_CONTAINER (priv->source_placeholder), priv->source);
	gtk_widget_show (priv->source_placeholder);
}

/**
 * brasero_burn_options_add_options:
 * @dialog: a #BraseroBurnOptions
 * @options: a #GtkWidget
 *
 * Adds some new options to be displayed in the dialog.
 **/

void
brasero_burn_options_add_options (BraseroBurnOptions *self,
				  GtkWidget *options)
{
	BraseroBurnOptionsPrivate *priv;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (self);
	gtk_box_pack_start (GTK_BOX (priv->options), options, FALSE, TRUE, 0);
	gtk_widget_show (priv->options);
}

static void
brasero_burn_options_set_type_shown (BraseroBurnOptions *self,
				     BraseroMediaType type)
{
	BraseroBurnOptionsPrivate *priv;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (self);
	brasero_medium_selection_show_media_type (BRASERO_MEDIUM_SELECTION (priv->selection), type);
}

static void
brasero_burn_options_message_response_cb (BraseroDiscMessage *message,
					  GtkResponseType response,
					  BraseroBurnOptions *self)
{
	if (response == GTK_RESPONSE_OK) {
		BraseroBurnOptionsPrivate *priv;

		priv = BRASERO_BURN_OPTIONS_PRIVATE (self);
		brasero_session_cfg_add_flags (priv->session, BRASERO_BURN_FLAG_OVERBURN);
	}
}

static void
brasero_burn_options_message_response_span_cb (BraseroDiscMessage *message,
					       GtkResponseType response,
					       BraseroBurnOptions *self)
{
	if (response == GTK_RESPONSE_OK) {
		BraseroBurnOptionsPrivate *priv;

		priv = BRASERO_BURN_OPTIONS_PRIVATE (self);
		brasero_session_span_start (BRASERO_SESSION_SPAN (priv->session));
		if (brasero_session_span_next (BRASERO_SESSION_SPAN (priv->session)) == BRASERO_BURN_ERR)
			BRASERO_BURN_LOG ("Spanning failed\n");
	}
}

#define BRASERO_BURN_OPTIONS_NO_MEDIUM_WARNING	1000

static void
brasero_burn_options_update_no_medium_warning (BraseroBurnOptions *self)
{
	BraseroBurnOptionsPrivate *priv;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (self);

	if (!priv->is_valid
	||  !brasero_burn_session_is_dest_file (BRASERO_BURN_SESSION (priv->session))
	||   brasero_medium_selection_get_media_num (BRASERO_MEDIUM_SELECTION (priv->selection)) != 1) {
		brasero_notify_message_remove (priv->message_output,
					       BRASERO_BURN_OPTIONS_NO_MEDIUM_WARNING);
		return;
	}

	/* The user may have forgotten to insert a disc so remind him of that if
	 * there aren't any other possibility in the selection */
	brasero_notify_message_add (priv->message_output,
				    _("Please insert a writable CD or DVD if you don't want to write to an image file."),
				    NULL,
				    -1,
				    BRASERO_BURN_OPTIONS_NO_MEDIUM_WARNING);
}

static void
brasero_burn_options_not_ready_dialog_cancel_cb (GtkDialog *dialog,
						 guint response,
						 gpointer data)
{
	BraseroBurnOptionsPrivate *priv;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (data);

	if (priv->not_ready_id) {
		g_source_remove (priv->not_ready_id);
		priv->not_ready_id = 0;
	}
	gtk_widget_set_sensitive (GTK_WIDGET (data), TRUE);

	if (response != GTK_RESPONSE_OK)
		gtk_dialog_response (GTK_DIALOG (data),
				     GTK_RESPONSE_CANCEL);
	else {
		priv->status_dialog = NULL;
		gtk_widget_destroy (GTK_WIDGET (dialog));
	}		
}

static gboolean
brasero_burn_options_not_ready_dialog_show_cb (gpointer data)
{
	BraseroBurnOptionsPrivate *priv;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (data);

	/* icon should be set by now */
	gtk_window_set_icon_name (GTK_WINDOW (priv->status_dialog),
	                          gtk_window_get_icon_name (GTK_WINDOW (data)));

	gtk_widget_show (priv->status_dialog);
	priv->not_ready_id = 0;
	return FALSE;
}

static void
brasero_burn_options_not_ready_dialog_shown_cb (GtkWidget *widget,
                                                gpointer data)
{
	BraseroBurnOptionsPrivate *priv;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (data);
	if (priv->not_ready_id) {
		g_source_remove (priv->not_ready_id);
		priv->not_ready_id = 0;
	}
}

static void
brasero_burn_options_setup_buttons (BraseroBurnOptions *self)
{
	BraseroBurnOptionsPrivate *priv;
	BraseroTrackType *type;
	gchar *label_m = "";
	gchar *label;
	gchar *icon;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (self);

	/* add the new widgets */
	type = brasero_track_type_new ();
	brasero_burn_session_get_input_type (BRASERO_BURN_SESSION (priv->session), type);
	if (brasero_burn_session_is_dest_file (BRASERO_BURN_SESSION (priv->session))) {
		label = _("Create _Image");
		icon = "iso-image-new";
	}
	else if (brasero_track_type_get_has_medium (type)) {
		/* Translators: This is a verb, an action */
		label = _("_Copy");
		icon = "media-optical-copy";
	
		label_m = _("Make _Several Copies");
	}
	else {
		/* Translators: This is a verb, an action */
		label = _("_Burn");
		icon = "media-optical-burn";

		label_m = _("Burn _Several Copies");
	}

	if (priv->burn_multi)
		gtk_button_set_label (GTK_BUTTON (priv->burn_multi), label_m);
	else
		priv->burn_multi = gtk_dialog_add_button (GTK_DIALOG (self),
							  label_m,
							  GTK_RESPONSE_ACCEPT);

	if (brasero_burn_session_is_dest_file (BRASERO_BURN_SESSION (priv->session)))
		gtk_widget_hide (priv->burn_multi);
	else
		gtk_widget_show (priv->burn_multi);

	if (priv->burn)
		gtk_button_set_label (GTK_BUTTON (priv->burn), label);
	else
		priv->burn = gtk_dialog_add_button (GTK_DIALOG (self),
						    label,
						    GTK_RESPONSE_OK);

	gtk_button_set_image (GTK_BUTTON (priv->burn), gtk_image_new_from_icon_name (icon, GTK_ICON_SIZE_BUTTON));

	gtk_widget_set_sensitive (priv->burn, priv->is_valid);
	gtk_widget_set_sensitive (priv->burn_multi, priv->is_valid);

	brasero_track_type_free (type);
}

static void
brasero_burn_options_update_valid (BraseroBurnOptions *self)
{
	BraseroBurnOptionsPrivate *priv;
	BraseroSessionError valid;
	BraseroDrive *drive;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (self);

	valid = brasero_session_cfg_get_error (priv->session);
	priv->is_valid = BRASERO_SESSION_IS_VALID (valid);

	brasero_burn_options_setup_buttons (self);

	gtk_widget_set_sensitive (priv->options, priv->is_valid);
	/* Ensure the user can always change the properties (i.e. file location)
	 * the target is a fake drive */
	drive = brasero_burn_session_get_burner (BRASERO_BURN_SESSION (priv->session));
	if (drive && brasero_drive_is_fake (drive))
		gtk_widget_set_sensitive (priv->properties, TRUE);
	else
		gtk_widget_set_sensitive (priv->properties, priv->is_valid);

	if (priv->message_input) {
		gtk_widget_hide (priv->message_input);
		brasero_notify_message_remove (priv->message_input,
					       BRASERO_NOTIFY_CONTEXT_SIZE);
	}

	brasero_notify_message_remove (priv->message_output,
				       BRASERO_NOTIFY_CONTEXT_SIZE);

	if (valid == BRASERO_SESSION_NOT_READY) {
		if (!priv->not_ready_id && !priv->status_dialog) {
			gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);
			priv->status_dialog = brasero_status_dialog_new (BRASERO_BURN_SESSION (priv->session),  GTK_WIDGET (self));
			g_signal_connect (priv->status_dialog,
					  "response", 
					  G_CALLBACK (brasero_burn_options_not_ready_dialog_cancel_cb),
					  self);

			g_signal_connect (priv->status_dialog,
					  "show", 
					  G_CALLBACK (brasero_burn_options_not_ready_dialog_shown_cb),
					  self);
			g_signal_connect (priv->status_dialog,
					  "user-interaction", 
					  G_CALLBACK (brasero_burn_options_not_ready_dialog_shown_cb),
					  self);

			priv->not_ready_id = g_timeout_add_seconds (1,
								    brasero_burn_options_not_ready_dialog_show_cb,
								    self);
		}
	}
	else {
		gtk_widget_set_sensitive (GTK_WIDGET (self), TRUE);
		if (priv->status_dialog) {
			gtk_widget_destroy (priv->status_dialog);
			priv->status_dialog = NULL;
		}

		if (priv->not_ready_id) {
			g_source_remove (priv->not_ready_id);
			priv->not_ready_id = 0;
		}
	}

	if (valid == BRASERO_SESSION_INSUFFICIENT_SPACE) {
		goffset min_disc_size;
		goffset available_space;

		min_disc_size = brasero_session_span_get_max_space (BRASERO_SESSION_SPAN (priv->session));

		/* One rule should be that the maximum batch size should not exceed the disc size
		 * FIXME: we could change it into a dialog telling the user what is the maximum
		 * size required. */
		available_space = brasero_burn_session_get_available_medium_space (BRASERO_BURN_SESSION (priv->session));

		/* Here there is an alternative: we may be able to span the data
		 * across multiple media. So try that. */
		if (available_space > min_disc_size
		&&  brasero_session_span_possible (BRASERO_SESSION_SPAN (priv->session)) == BRASERO_BURN_RETRY) {
			GtkWidget *message;

			message = brasero_notify_message_add (priv->message_output,
							      _("Would you like to burn the selection of files across several media?"),
							      _("The data size is too large for the disc even with the overburn option."),
							      -1,
							      BRASERO_NOTIFY_CONTEXT_SIZE);

			gtk_widget_set_tooltip_text (gtk_info_bar_add_button (GTK_INFO_BAR (message),
									      _("_Burn Several Discs"),
									      GTK_RESPONSE_OK),
						    _("Burn the selection of files across several media"));

			g_signal_connect (message,
					  "response",
					  G_CALLBACK (brasero_burn_options_message_response_span_cb),
					  self);
		}
		else
			brasero_notify_message_add (priv->message_output,
						    _("Please choose another CD or DVD or insert a new one."),
						    _("The data size is too large for the disc even with the overburn option."),
						    -1,
						    BRASERO_NOTIFY_CONTEXT_SIZE);
	}
	else if (valid == BRASERO_SESSION_NO_OUTPUT) {
		brasero_notify_message_add (priv->message_output,
					    _("Please insert a writable CD or DVD."),
					    NULL,
					    -1,
					    BRASERO_NOTIFY_CONTEXT_SIZE);
	}
	else if (valid == BRASERO_SESSION_NO_CD_TEXT) {
		brasero_notify_message_add (priv->message_output,
					    _("No track information (artist, title, ...) will be written to the disc."),
					    _("This is not supported by the current active burning backend."),
					    -1,
					    BRASERO_NOTIFY_CONTEXT_SIZE);
	}
	else if (valid == BRASERO_SESSION_EMPTY) {
		BraseroTrackType *type;
		
		type = brasero_track_type_new ();
		brasero_burn_session_get_input_type (BRASERO_BURN_SESSION (priv->session), type);

		if (brasero_track_type_get_has_data (type))
			brasero_notify_message_add (priv->message_output,
						    _("Please add files."),
						    _("There are no files to write to disc"),
						    -1,
						    BRASERO_NOTIFY_CONTEXT_SIZE);
		else if (!BRASERO_STREAM_FORMAT_HAS_VIDEO (brasero_track_type_get_stream_format (type)))
			brasero_notify_message_add (priv->message_output,
						    _("Please add songs."),
						    _("There are no songs to write to disc"),
						    -1,
						    BRASERO_NOTIFY_CONTEXT_SIZE);
		else
			brasero_notify_message_add (priv->message_output,
						     _("Please add videos."),
						    _("There are no videos to write to disc"),
						    -1,
						    BRASERO_NOTIFY_CONTEXT_SIZE);
		brasero_track_type_free (type);
		gtk_window_resize (GTK_WINDOW (self), 10, 10);
		return;		      
	}
	else if (valid == BRASERO_SESSION_NO_INPUT_MEDIUM) {
		if (priv->message_input) {
			gtk_widget_show (priv->message_input);
			brasero_notify_message_add (priv->message_input,
			                            _("Please insert a disc holding data."),
			                            _("There is no inserted disc to copy."),
			                            -1,
			                            BRASERO_NOTIFY_CONTEXT_SIZE);
		}
	}
	else if (valid == BRASERO_SESSION_NO_INPUT_IMAGE) {
		if (priv->message_input) {
			gtk_widget_show (priv->message_input);
			brasero_notify_message_add (priv->message_input,
			                            _("Please select a disc image."),
			                            _("There is no selected disc image."),
			                            -1,
			                            BRASERO_NOTIFY_CONTEXT_SIZE);
		}
	}
	else if (valid == BRASERO_SESSION_UNKNOWN_IMAGE) {
		if (priv->message_input) {
			gtk_widget_show (priv->message_input);
			brasero_notify_message_add (priv->message_input,
			                            /* Translators: this is a disc image not a picture */
			                            C_("disc", "Please select another image."),
			                            _("It doesn't appear to be a valid disc image or a valid cue file."),
			                            -1,
			                            BRASERO_NOTIFY_CONTEXT_SIZE);
		}
	}
	else if (valid == BRASERO_SESSION_DISC_PROTECTED) {
		if (priv->message_input) {
			gtk_widget_show (priv->message_input);
			brasero_notify_message_add (priv->message_input,
			                            _("Please insert a disc that is not copy protected."),
			                            _("All required applications and libraries are not installed."),
			                            -1,
			                            BRASERO_NOTIFY_CONTEXT_SIZE);
		}
	}
	else if (valid == BRASERO_SESSION_NOT_SUPPORTED) {
		brasero_notify_message_add (priv->message_output,
		                            _("Please replace the disc with a supported CD or DVD."),
		                            NULL,
		                            -1,
		                            BRASERO_NOTIFY_CONTEXT_SIZE);
	}
	else if (valid == BRASERO_SESSION_OVERBURN_NECESSARY) {
		GtkWidget *message;
		
		message = brasero_notify_message_add (priv->message_output,
						      _("Would you like to burn beyond the disc's reported capacity?"),
						      _("The data size is too large for the disc and you must remove files from the selection otherwise."
							"\nYou may want to use this option if you're using 90 or 100 min CD-R(W) which cannot be properly recognised and therefore need overburn option."
							"\nNOTE: This option might cause failure."),
						      -1,
						      BRASERO_NOTIFY_CONTEXT_SIZE);

		gtk_widget_set_tooltip_text (gtk_info_bar_add_button (GTK_INFO_BAR (message),
								      _("_Overburn"),
								      GTK_RESPONSE_OK),
					     _("Burn beyond the disc's reported capacity"));

		g_signal_connect (message,
				  "response",
				  G_CALLBACK (brasero_burn_options_message_response_cb),
				  self);
	}
	else if (brasero_burn_session_same_src_dest_drive (BRASERO_BURN_SESSION (priv->session))) {
		/* The medium is valid but it's a special case */
		brasero_notify_message_add (priv->message_output,
					    _("The drive that holds the source disc will also be the one used to record."),
					    _("A new writable disc will be required once the currently loaded one has been copied."),
					    -1,
					    BRASERO_NOTIFY_CONTEXT_SIZE);
		gtk_widget_show_all (priv->message_output);
	}

	brasero_burn_options_update_no_medium_warning (self);
	gtk_window_resize (GTK_WINDOW (self), 10, 10);
}

static void
brasero_burn_options_valid_cb (BraseroSessionCfg *session,
			       BraseroBurnOptions *self)
{
	brasero_burn_options_update_valid (self);
}

static void
brasero_burn_options_init (BraseroBurnOptions *object)
{
}

/**
 * To build anything we need to have the session set first
 */

static void
brasero_burn_options_build_contents (BraseroBurnOptions *object)
{
	BraseroBurnOptionsPrivate *priv;
	GtkWidget *content_area;
	GtkWidget *selection;
	GtkWidget *alignment;
	gchar *string;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (object);

	priv->size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	/* Sets default flags for the session */
	brasero_burn_session_add_flag (BRASERO_BURN_SESSION (priv->session),
				       BRASERO_BURN_FLAG_NOGRACE|
				       BRASERO_BURN_FLAG_CHECK_SIZE);

	/* Create a cancel button */
	gtk_dialog_add_button (GTK_DIALOG (object),
			       GTK_STOCK_CANCEL,
			       GTK_RESPONSE_CANCEL);

	/* Create an upper box for sources */
	priv->source_placeholder = gtk_alignment_new (0.0, 0.5, 1.0, 1.0);
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (object));
	gtk_box_pack_start (GTK_BOX (content_area),
			    priv->source_placeholder,
			    FALSE,
			    TRUE,
			    0);

	/* Medium selection box */
	selection = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_widget_show (selection);

	alignment = gtk_alignment_new (0.0, 0.5, 1.0, 1.0);
	gtk_widget_show (alignment);
	gtk_box_pack_start (GTK_BOX (selection),
			    alignment,
			    TRUE,
			    TRUE,
			    0);

	priv->selection = brasero_dest_selection_new (BRASERO_BURN_SESSION (priv->session));
	gtk_widget_show (priv->selection);
	gtk_container_add (GTK_CONTAINER (alignment), priv->selection);

	priv->properties = brasero_medium_properties_new (priv->session);
	gtk_size_group_add_widget (priv->size_group, priv->properties);
	gtk_widget_show (priv->properties);
	gtk_box_pack_start (GTK_BOX (selection),
			    priv->properties,
			    FALSE,
			    FALSE,
			    0);

	/* Box to display warning messages */
	priv->message_output = brasero_notify_new ();
	gtk_widget_show (priv->message_output);

	string = g_strdup_printf ("<b>%s</b>", _("Select a disc to write to"));
	selection = brasero_utils_pack_properties (string,
						   priv->message_output,
						   selection,
						   NULL);
	g_free (string);
	gtk_widget_show (selection);

	gtk_box_pack_start (GTK_BOX (content_area),
			    selection,
			    FALSE,
			    TRUE,
			    0);

	/* Create a lower box for options */
	alignment = gtk_alignment_new (0.0, 0.5, 1.0, 1.0);
	gtk_widget_show (alignment);
	gtk_box_pack_start (GTK_BOX (content_area),
			    alignment,
			    FALSE,
			    TRUE,
			    0);
	priv->options_placeholder = alignment;

	priv->options = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add (GTK_CONTAINER (alignment), priv->options);

	g_signal_connect (priv->session,
			  "is-valid",
			  G_CALLBACK (brasero_burn_options_valid_cb),
			  object);

	brasero_burn_options_update_valid (object);
}

static void
brasero_burn_options_reset (BraseroBurnOptions *self)
{
	BraseroBurnOptionsPrivate *priv;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (self);

	priv->has_image = FALSE;
	priv->has_audio = FALSE;
	priv->has_video = FALSE;
	priv->has_data = FALSE;
	priv->has_disc = FALSE;

	/* reset all the dialog */
	if (priv->message_input) {
		gtk_widget_destroy (priv->message_input);
		priv->message_input = NULL;
	}

	if (priv->options) {
		gtk_widget_destroy (priv->options);
		priv->options = NULL;
	}

	priv->options = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add (GTK_CONTAINER (priv->options_placeholder), priv->options);

	if (priv->source) {
		gtk_widget_destroy (priv->source);
		priv->source = NULL;
	}

	gtk_widget_hide (priv->source_placeholder);
}

static void
brasero_burn_options_setup_audio (BraseroBurnOptions *self)
{
	BraseroBurnOptionsPrivate *priv;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (self);

	brasero_burn_options_reset (self);

	priv->has_audio = TRUE;
	gtk_window_set_title (GTK_WINDOW (self), _("Disc Burning Setup"));
	brasero_burn_options_set_type_shown (BRASERO_BURN_OPTIONS (self),
					     BRASERO_MEDIA_TYPE_WRITABLE|
					     BRASERO_MEDIA_TYPE_FILE);
}

static void
brasero_burn_options_setup_video (BraseroBurnOptions *self)
{
	gchar *title;
	GtkWidget *options;
	BraseroBurnOptionsPrivate *priv;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (self);

	brasero_burn_options_reset (self);

	priv->has_video = TRUE;
	gtk_window_set_title (GTK_WINDOW (self), _("Disc Burning Setup"));
	brasero_burn_options_set_type_shown (BRASERO_BURN_OPTIONS (self),
					     BRASERO_MEDIA_TYPE_WRITABLE|
					     BRASERO_MEDIA_TYPE_FILE);

	/* create the options box */
	options = brasero_video_options_new (BRASERO_BURN_SESSION (priv->session));
	gtk_widget_show (options);

	title = g_strdup_printf ("<b>%s</b>", _("Video Options"));
	options = brasero_utils_pack_properties (title,
	                                           options,
	                                           NULL);
	g_free (title);

	gtk_widget_show (options);
	brasero_burn_options_add_options (self, options);
}

static BraseroBurnResult
brasero_status_dialog_uri_has_image (BraseroTrackDataCfg *track,
				     const gchar *uri,
				     BraseroBurnOptions *self)
{
	gint answer;
	gchar *name;
	GtkWidget *button;
	GtkWidget *dialog;
	gboolean was_visible = FALSE;
	gboolean was_not_ready = FALSE;
	BraseroTrackImageCfg *track_img;
	BraseroBurnOptionsPrivate *priv;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (self);
	dialog = gtk_message_dialog_new (GTK_WINDOW (self),
					 GTK_DIALOG_MODAL |
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_NONE,
					 "%s",
					 _("Do you want to create a disc from the contents of the image or with the image file inside?"));

	gtk_window_set_title (GTK_WINDOW (dialog), "");
	gtk_window_set_icon_name (GTK_WINDOW (dialog),
	                          gtk_window_get_icon_name (GTK_WINDOW (self)));

	name = brasero_utils_get_uri_name (uri);
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
			/* Translators: %s is the name of the image */
			_("There is only one selected file (\"%s\"). "
			  "It is the image of a disc and its contents can be burned."),
			name);
	g_free (name);

	gtk_dialog_add_button (GTK_DIALOG (dialog), _("Burn as _File"), GTK_RESPONSE_NO);

	button = brasero_utils_make_button (_("Burn _Contents…"),
	                                    NULL,
	                                    "media-optical-burn",
	                                    GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (button);
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog),
				      button,
				      GTK_RESPONSE_YES);

	if (!priv->not_ready_id && priv->status_dialog) {
		was_visible = TRUE;
		gtk_widget_hide (GTK_WIDGET (priv->status_dialog));
	}
	else if (priv->not_ready_id) {
		g_source_remove (priv->not_ready_id);
		priv->not_ready_id = 0;
		was_not_ready = TRUE;
	}

	gtk_widget_show_all (dialog);
	answer = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	if (answer != GTK_RESPONSE_YES) {
		if (was_not_ready)
			priv->not_ready_id = g_timeout_add_seconds (1,
								    brasero_burn_options_not_ready_dialog_show_cb,
								    self);
		if (was_visible)
			gtk_widget_show (GTK_WIDGET (priv->status_dialog));

		return BRASERO_BURN_OK;
	}

	/* Setup a new track and add it to session */
	track_img = brasero_track_image_cfg_new ();
	brasero_track_image_cfg_set_source (track_img, uri);
	brasero_burn_session_add_track (BRASERO_BURN_SESSION (priv->session),
					BRASERO_TRACK (track_img),
					NULL);

	return BRASERO_BURN_CANCEL;
}

static void
brasero_burn_options_setup_data (BraseroBurnOptions *self)
{
	GSList *tracks;
	BraseroBurnOptionsPrivate *priv;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (self);

	brasero_burn_options_reset (self);

	/* NOTE: we don't need to keep a record of the signal as the track will
	 * be destroyed if the user agrees to burn the image directly */
	tracks = brasero_burn_session_get_tracks (BRASERO_BURN_SESSION (priv->session));
	if (BRASERO_IS_TRACK_DATA_CFG (tracks->data))
		g_signal_connect (tracks->data,
				  "image-uri",
				  G_CALLBACK (brasero_status_dialog_uri_has_image),
				  self);

	priv->has_data = TRUE;
	gtk_window_set_title (GTK_WINDOW (self), _("Disc Burning Setup"));
	brasero_burn_options_set_type_shown (BRASERO_BURN_OPTIONS (self),
					     BRASERO_MEDIA_TYPE_WRITABLE|
					     BRASERO_MEDIA_TYPE_FILE);
}

static void
brasero_burn_options_setup_image (BraseroBurnOptions *self)
{
	gchar *string;
	GtkWidget *file;
	BraseroBurnOptionsPrivate *priv;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (self);

	brasero_burn_options_reset (self);

	priv->has_image = TRUE;
	gtk_window_set_title (GTK_WINDOW (self), _("Image Burning Setup"));
	brasero_burn_options_set_type_shown (self, BRASERO_MEDIA_TYPE_WRITABLE);

	/* Image properties */
	file = brasero_src_image_new (BRASERO_BURN_SESSION (priv->session));
	gtk_widget_show (file);

	/* pack everything */
	string = g_strdup_printf ("<b>%s</b>", _("Select a disc image to write"));
	brasero_burn_options_add_source (self, 
					 string,
					 file,
					 NULL);
	g_free (string);
}

static void
brasero_burn_options_setup_disc (BraseroBurnOptions *self)
{
	gchar *title_str;
	GtkWidget *source;
	BraseroBurnOptionsPrivate *priv;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (self);

	brasero_burn_options_reset (self);

	priv->has_disc = TRUE;
	gtk_window_set_title (GTK_WINDOW (self), _("Copy CD/DVD"));

	/* take care of source media */
	source = brasero_src_selection_new (BRASERO_BURN_SESSION (priv->session));
	gtk_widget_show (source);

	title_str = g_strdup_printf ("<b>%s</b>", _("Select disc to copy"));
	brasero_burn_options_add_source (self,
					 title_str,
					 source,
					 NULL);
	g_free (title_str);

	/* only show media with something to be read on them */
	brasero_medium_selection_show_media_type (BRASERO_MEDIUM_SELECTION (source),
						  BRASERO_MEDIA_TYPE_AUDIO|
						  BRASERO_MEDIA_TYPE_DATA);

	/* This is a special case. When we're copying, someone may want to read
	 * and burn to the same drive so provided that the drive is a burner
	 * then show its contents. */
	brasero_burn_options_set_type_shown (self,
					     BRASERO_MEDIA_TYPE_ANY_IN_BURNER|
					     BRASERO_MEDIA_TYPE_FILE);
}

static void
brasero_burn_options_setup (BraseroBurnOptions *self)
{
	BraseroBurnOptionsPrivate *priv;
	BraseroTrackType *type;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (self);

	/* add the new widgets */
	type = brasero_track_type_new ();
	brasero_burn_session_get_input_type (BRASERO_BURN_SESSION (priv->session), type);
	if (brasero_track_type_get_has_medium (type)) {
		if (!priv->has_disc)
			brasero_burn_options_setup_disc (self);
	}
	else if (brasero_track_type_get_has_image (type)) {
		if (!priv->has_image)
			brasero_burn_options_setup_image (self);
	}
	else if (brasero_track_type_get_has_data (type)) {
		if (!priv->has_data)
			brasero_burn_options_setup_data (self);
	}
	else if (brasero_track_type_get_has_stream (type)) {
		if (BRASERO_STREAM_FORMAT_HAS_VIDEO (brasero_track_type_get_stream_format (type))) {
			if (!priv->has_video)
				brasero_burn_options_setup_video (self);
		}
		else if (!priv->has_audio)
			brasero_burn_options_setup_audio (self);
	}
	brasero_track_type_free (type);

	brasero_burn_options_setup_buttons (self);
}

static void
brasero_burn_options_track_added (BraseroBurnSession *session,
				  BraseroTrack *track,
				  BraseroBurnOptions *dialog)
{
	brasero_burn_options_setup (dialog);
}

static void
brasero_burn_options_track_removed (BraseroBurnSession *session,
				    BraseroTrack *track,
				    guint former_position,
				    BraseroBurnOptions *dialog)
{
	brasero_burn_options_setup (dialog);
}

static void
brasero_burn_options_track_changed (BraseroBurnSession *session,
                                    BraseroTrack *track,
                                    BraseroBurnOptions *dialog)
{
	brasero_burn_options_setup (dialog);
}

static void
brasero_burn_options_set_property (GObject *object,
				   guint prop_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
	BraseroBurnOptionsPrivate *priv;

	g_return_if_fail (BRASERO_IS_BURN_OPTIONS (object));

	priv = BRASERO_BURN_OPTIONS_PRIVATE (object);

	switch (prop_id)
	{
	case PROP_SESSION: /* Readable and only writable at creation time */
		priv->session = g_object_ref (g_value_get_object (value));

		g_object_notify (object, "session");

		g_signal_connect (priv->session,
				  "track-added",
				  G_CALLBACK (brasero_burn_options_track_added),
				  object);
		g_signal_connect (priv->session,
				  "track-removed",
				  G_CALLBACK (brasero_burn_options_track_removed),
				  object);
		g_signal_connect (priv->session,
				  "track-changed",
				  G_CALLBACK (brasero_burn_options_track_changed),
				  object);
		brasero_burn_options_build_contents (BRASERO_BURN_OPTIONS (object));
		brasero_burn_options_setup (BRASERO_BURN_OPTIONS (object));

		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
brasero_burn_options_get_property (GObject *object,
				   guint prop_id,
				   GValue *value,
				   GParamSpec *pspec)
{
	BraseroBurnOptionsPrivate *priv;

	g_return_if_fail (BRASERO_IS_BURN_OPTIONS (object));

	priv = BRASERO_BURN_OPTIONS_PRIVATE (object);

	switch (prop_id)
	{
	case PROP_SESSION:
		g_value_set_object (value, priv->session);
		g_object_ref (priv->session);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
brasero_burn_options_finalize (GObject *object)
{
	BraseroBurnOptionsPrivate *priv;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (object);

	if (priv->cancel) {
		g_cancellable_cancel (priv->cancel);
		priv->cancel = NULL;
	}

	if (priv->not_ready_id) {
		g_source_remove (priv->not_ready_id);
		priv->not_ready_id = 0;
	}

	if (priv->status_dialog) {
		gtk_widget_destroy (priv->status_dialog);
		priv->status_dialog = NULL;
	}

	if (priv->session) {
		g_signal_handlers_disconnect_by_func (priv->session,
						      brasero_burn_options_track_added,
						      object);
		g_signal_handlers_disconnect_by_func (priv->session,
						      brasero_burn_options_track_removed,
						      object);
		g_signal_handlers_disconnect_by_func (priv->session,
						      brasero_burn_options_track_changed,
						      object);
		g_signal_handlers_disconnect_by_func (priv->session,
						      brasero_burn_options_valid_cb,
						      object);

		g_object_unref (priv->session);
		priv->session = NULL;
	}

	if (priv->size_group) {
		g_object_unref (priv->size_group);
		priv->size_group = NULL;
	}

	G_OBJECT_CLASS (brasero_burn_options_parent_class)->finalize (object);
}

static BraseroBurnResult
brasero_burn_options_install_missing (BraseroPluginErrorType type,
                                      const gchar *detail,
                                      gpointer user_data)
{
	BraseroBurnOptionsPrivate *priv = BRASERO_BURN_OPTIONS_PRIVATE (user_data);
	GCancellable *cancel;
	BraseroPK *package;
	gboolean res;
	int xid = 0;

	/* Get the xid */
	xid = gdk_x11_window_get_xid (gtk_widget_get_window (GTK_WIDGET (user_data)));

	package = brasero_pk_new ();
	cancel = g_cancellable_new ();
	priv->cancel = cancel;
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

	if (g_cancellable_is_cancelled (cancel)) {
		g_object_unref (cancel);
		return BRASERO_BURN_CANCEL;
	}

	priv->cancel = NULL;
	g_object_unref (cancel);

	if (!res)
		return BRASERO_BURN_ERR;

	return BRASERO_BURN_RETRY;
}

static BraseroBurnResult
brasero_burn_options_list_missing (BraseroPluginErrorType type,
                                   const gchar *detail,
                                   gpointer user_data)
{
	GString *string = user_data;

	if (type == BRASERO_PLUGIN_ERROR_MISSING_APP ||
	    type == BRASERO_PLUGIN_ERROR_SYMBOLIC_LINK_APP ||
	    type == BRASERO_PLUGIN_ERROR_WRONG_APP_VERSION) {
		g_string_append_c (string, '\n');
		/* Translators: %s is the name of a missing application */
		g_string_append_printf (string, _("%s (application)"), detail);
	}
	else if (type == BRASERO_PLUGIN_ERROR_MISSING_LIBRARY ||
	         type == BRASERO_PLUGIN_ERROR_LIBRARY_VERSION) {
		g_string_append_c (string, '\n');
		/* Translators: %s is the name of a missing library */
		g_string_append_printf (string, _("%s (library)"), detail);
	}
	else if (type == BRASERO_PLUGIN_ERROR_MISSING_GSTREAMER_PLUGIN) {
		g_string_append_c (string, '\n');
		/* Translators: %s is the name of a missing GStreamer plugin */
		g_string_append_printf (string, _("%s (GStreamer plugin)"), detail);
	}

	return BRASERO_BURN_OK;
}

static void
brasero_burn_options_response (GtkDialog *dialog,
                               GtkResponseType response)
{
	BraseroBurnOptionsPrivate *priv;
	BraseroBurnResult result;
	GString *string;

	if (response != GTK_RESPONSE_OK)
		return;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (dialog);
	gtk_widget_set_sensitive (GTK_WIDGET (dialog), FALSE);

	result = brasero_session_foreach_plugin_error (BRASERO_BURN_SESSION (priv->session),
	                                               brasero_burn_options_install_missing,
	                                               dialog);
	if (result == BRASERO_BURN_CANCEL)
		return;

	gtk_widget_set_sensitive (GTK_WIDGET (dialog), TRUE);

	if (result == BRASERO_BURN_OK)
		return;

	string = g_string_new (_("Please install the following manually and try again:"));
	brasero_session_foreach_plugin_error (BRASERO_BURN_SESSION (priv->session),
	                                      brasero_burn_options_list_missing,
	                                      string);

	brasero_utils_message_dialog (GTK_WIDGET (dialog),
	                              _("All required applications and libraries are not installed."),
	                              string->str,
	                              GTK_MESSAGE_ERROR);
	g_string_free (string, TRUE);

	/* Cancel the rest */
	gtk_dialog_response (dialog, GTK_RESPONSE_CANCEL);
}

static void
brasero_burn_options_class_init (BraseroBurnOptionsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkDialogClass *gtk_dialog_class = GTK_DIALOG_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroBurnOptionsPrivate));

	object_class->finalize = brasero_burn_options_finalize;
	object_class->set_property = brasero_burn_options_set_property;
	object_class->get_property = brasero_burn_options_get_property;

	gtk_dialog_class->response = brasero_burn_options_response;

	g_object_class_install_property (object_class,
					 PROP_SESSION,
					 g_param_spec_object ("session",
							      "The session",
							      "The session to work with",
							      BRASERO_TYPE_BURN_SESSION,
							      G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY));
}

/**
 * brasero_burn_options_new:
 * @session: a #BraseroSessionCfg object
 *
 *  Creates a new #BraseroBurnOptions object.
 *
 * Return value: a #GtkWidget object.
 **/

GtkWidget *
brasero_burn_options_new (BraseroSessionCfg *session)
{
	return g_object_new (BRASERO_TYPE_BURN_OPTIONS, "session", session, NULL);
}
