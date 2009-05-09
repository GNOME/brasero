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

#include "burn-basics.h"

#include "brasero-medium.h"
#include "brasero-medium-selection-priv.h"

#include "brasero-session.h"
#include "brasero-session-helper.h"
#include "brasero-burn-options.h"
#include "brasero-disc-copy-dialog.h"
#include "brasero-image-option-dialog.h"
#include "brasero-disc-option-dialog.h"
#include "brasero-session-cfg.h"
#include "brasero-dest-selection.h"
#include "brasero-medium-properties.h"
#include "brasero-status-dialog.h"
#include "brasero-track-stream.h"

#include "brasero-notify.h"
#include "brasero-misc.h"

typedef struct _BraseroBurnOptionsPrivate BraseroBurnOptionsPrivate;
struct _BraseroBurnOptionsPrivate
{
	BraseroSessionCfg *session;

	gulong valid_sig;

	GtkSizeGroup *size_group;

	GtkWidget *source;
	GtkWidget *message_input;
	GtkWidget *selection;
	GtkWidget *properties;
	GtkWidget *message_output;
	GtkWidget *options;
	GtkWidget *button;

	guint not_ready_id;
	GtkWidget *status_dialog;

	guint is_valid:1;
};

#define BRASERO_BURN_OPTIONS_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_BURN_OPTIONS, BraseroBurnOptionsPrivate))

enum {
	PROP_0,
	PROP_SESSION
};

G_DEFINE_TYPE (BraseroBurnOptions, brasero_burn_options, GTK_TYPE_DIALOG);

void
brasero_burn_options_add_source (BraseroBurnOptions *self,
				 const gchar *title,
				 ...)
{
	va_list vlist;
	GtkWidget *child;
	GtkWidget *source;
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

		hbox = gtk_hbox_new (FALSE, 12);
		gtk_widget_show (hbox);

		gtk_box_pack_start (GTK_BOX (hbox), child, TRUE, TRUE, 0);

		alignment = gtk_alignment_new (0.0, 0.5, 0., 0.);
		gtk_widget_show (alignment);
		gtk_size_group_add_widget (priv->size_group, alignment);
		gtk_box_pack_start (GTK_BOX (hbox), alignment, FALSE, FALSE, 0);

		list = g_slist_prepend (list, hbox);
	}
	va_end (vlist);

	source = brasero_utils_pack_properties_list (title, list);
	g_slist_free (list);

	gtk_container_add (GTK_CONTAINER (priv->source), source);
	gtk_widget_show (priv->source);

	brasero_dest_selection_choose_best (BRASERO_DEST_SELECTION (priv->selection));
}

void
brasero_burn_options_add_options (BraseroBurnOptions *self,
				  GtkWidget *options)
{
	BraseroBurnOptionsPrivate *priv;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (self);

	gtk_box_pack_start (GTK_BOX (priv->options), options, FALSE, TRUE, 0);
	gtk_widget_show (priv->options);
}

GtkWidget *
brasero_burn_options_add_burn_button (BraseroBurnOptions *self,
				      const gchar *text,
				      const gchar *icon)
{
	BraseroBurnOptionsPrivate *priv;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (self);

	if (priv->button) {
		gtk_widget_destroy (priv->button);
		priv->button = NULL;
	}

	priv->button = gtk_dialog_add_button (GTK_DIALOG (self),
					      text,
					      GTK_RESPONSE_OK);
	gtk_button_set_image (GTK_BUTTON (priv->button),
			      gtk_image_new_from_icon_name (icon,
							    GTK_ICON_SIZE_BUTTON));

	return priv->button;
}

void
brasero_burn_options_lock_selection (BraseroBurnOptions *self)
{
	BraseroBurnOptionsPrivate *priv;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (self);
	brasero_medium_selection_set_active (BRASERO_MEDIUM_SELECTION (priv->selection),
					     brasero_drive_get_medium (brasero_burn_session_get_burner (BRASERO_BURN_SESSION (priv->session))));
	brasero_dest_selection_lock (BRASERO_DEST_SELECTION (priv->selection), TRUE);
}

void
brasero_burn_options_set_type_shown (BraseroBurnOptions *self,
				     BraseroMediaType type)
{
	BraseroBurnOptionsPrivate *priv;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (self);
	brasero_medium_selection_show_media_type (BRASERO_MEDIUM_SELECTION (priv->selection), type);
}

BraseroBurnSession *
brasero_burn_options_get_session (BraseroBurnOptions *self)
{
	BraseroBurnOptionsPrivate *priv;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (self);
	g_object_ref (priv->session);

	return BRASERO_BURN_SESSION (priv->session);
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

#define BRASERO_BURN_OPTIONS_NO_MEDIUM_WARNING	1000

static void
brasero_burn_options_update_no_medium_warning (BraseroBurnOptions *self)
{
	BraseroBurnOptionsPrivate *priv;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (self);

	if (!priv->is_valid) {
		brasero_notify_message_remove (BRASERO_NOTIFY (priv->message_output),
					       BRASERO_BURN_OPTIONS_NO_MEDIUM_WARNING);
		return;
	}

	if (!brasero_burn_session_is_dest_file (BRASERO_BURN_SESSION (priv->session))) {
		brasero_notify_message_remove (BRASERO_NOTIFY (priv->message_output),
					       BRASERO_BURN_OPTIONS_NO_MEDIUM_WARNING);
		return;
	}

	if (brasero_medium_selection_get_media_num (BRASERO_MEDIUM_SELECTION (priv->selection)) != 1) {
		brasero_notify_message_remove (BRASERO_NOTIFY (priv->message_output),
					       BRASERO_BURN_OPTIONS_NO_MEDIUM_WARNING);
		return;
	}

	/* The user may have forgotten to insert a disc so remind him of that if
	 * there aren't any other possibility in the selection */
	brasero_notify_message_add (BRASERO_NOTIFY (priv->message_output),
				    _("Please insert a recordable CD or DVD if you don't want to write to an image file."),
				    NULL,
				    -1,
				    BRASERO_BURN_OPTIONS_NO_MEDIUM_WARNING);
}

static gboolean
brasero_burn_options_not_ready_dialog_cb (gpointer data)
{
	BraseroBurnOptionsPrivate *priv;
	GtkWidget *status_dialog;
	BraseroBurnResult result;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (data);
	priv->not_ready_id = 0;

	status_dialog = brasero_status_dialog_new ();
	result = brasero_status_dialog_wait_for_session (BRASERO_STATUS_DIALOG (status_dialog),
							 GTK_WIDGET (data),
							 BRASERO_BURN_SESSION (priv->session));
	gtk_widget_destroy (status_dialog);

	if (result != BRASERO_BURN_OK)
		gtk_dialog_response (GTK_DIALOG (data), GTK_RESPONSE_CANCEL);

	gtk_widget_set_sensitive (GTK_WIDGET (data), TRUE);

	return FALSE;
}

static void
brasero_burn_options_update_valid (BraseroBurnOptions *self)
{
	BraseroBurnOptionsPrivate *priv;
	BraseroSessionError valid;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (self);

	valid = brasero_session_cfg_get_error (priv->session);
	priv->is_valid = BRASERO_SESSION_IS_VALID (valid);

	gtk_widget_set_sensitive (priv->button, priv->is_valid);
	gtk_widget_set_sensitive (priv->options, priv->is_valid);
	gtk_widget_set_sensitive (priv->properties, priv->is_valid);

	if (priv->message_input) {
		gtk_widget_hide (priv->message_input);
		brasero_notify_message_remove (BRASERO_NOTIFY (priv->message_input),
					       BRASERO_NOTIFY_CONTEXT_SIZE);
	}

	brasero_notify_message_remove (BRASERO_NOTIFY (priv->message_output),
				       BRASERO_NOTIFY_CONTEXT_SIZE);

	if (valid == BRASERO_SESSION_NOT_READY) {
		if (!priv->not_ready_id && !priv->status_dialog) {
			gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);
			priv->not_ready_id = g_timeout_add_seconds (1,
								    brasero_burn_options_not_ready_dialog_cb,
								    self);
		}
	}
	else {
		gtk_widget_set_sensitive (GTK_WIDGET (self), TRUE);
		if (priv->status_dialog)
			gtk_dialog_response (GTK_DIALOG (priv->status_dialog),
					     GTK_RESPONSE_CANCEL);

		if (priv->not_ready_id) {
			g_source_remove (priv->not_ready_id);
			priv->not_ready_id = 0;
		}
	}

	if (valid == BRASERO_SESSION_INSUFFICIENT_SPACE) {
		brasero_notify_message_add (BRASERO_NOTIFY (priv->message_output),
					    _("Please choose another CD or DVD or insert a new one."),
					    _("The size of the project is too large for the disc even with the overburn option."),
					    -1,
					    BRASERO_NOTIFY_CONTEXT_SIZE);
	}
	else if (valid == BRASERO_SESSION_NO_OUTPUT) {
		brasero_notify_message_add (BRASERO_NOTIFY (priv->message_output),
					    _("Please insert a recordable CD or DVD."),
					    _("There is no recordable disc inserted."),
					    -1,
					    BRASERO_NOTIFY_CONTEXT_SIZE);
	}
	else if (valid == BRASERO_SESSION_NO_CD_TEXT) {
		brasero_notify_message_add (BRASERO_NOTIFY (priv->message_output),
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
			brasero_notify_message_add (BRASERO_NOTIFY (priv->message_output),
						    _("Please add files."),
						    _("The project is empty"),
						    -1,
						    BRASERO_NOTIFY_CONTEXT_SIZE);
		else if (!BRASERO_STREAM_FORMAT_HAS_VIDEO (brasero_track_type_get_stream_format (type)))
			brasero_notify_message_add (BRASERO_NOTIFY (priv->message_output),
						    _("Please add songs."),
						    _("The project is empty"),
						    -1,
						    BRASERO_NOTIFY_CONTEXT_SIZE);
		else
			brasero_notify_message_add (BRASERO_NOTIFY (priv->message_output),
						     _("Please add videos."),
						    _("The project is empty"),
						    -1,
						    BRASERO_NOTIFY_CONTEXT_SIZE);
		brasero_track_type_free (type);
		return;		      
	}
	else if (valid == BRASERO_SESSION_NO_INPUT_MEDIUM) {
		GtkWidget *message;

		if (priv->message_input) {
			gtk_widget_show (priv->message_input);
			message = brasero_notify_message_add (BRASERO_NOTIFY (priv->message_input),
							      _("Please insert a disc holding data."),
							      _("There is no inserted disc to copy."),
							      -1,
							      BRASERO_NOTIFY_CONTEXT_SIZE);
		}
	}
	else if (valid == BRASERO_SESSION_NO_INPUT_IMAGE) {
		GtkWidget *message;

		if (priv->message_input) {
			gtk_widget_show (priv->message_input);
			message = brasero_notify_message_add (BRASERO_NOTIFY (priv->message_input),
							      _("Please select an image."),
							      _("There is no selected image."),
							      -1,
							      BRASERO_NOTIFY_CONTEXT_SIZE);
		}
	}
	else if (valid == BRASERO_SESSION_UNKNOWN_IMAGE) {
		GtkWidget *message;

		if (priv->message_input) {
			gtk_widget_show (priv->message_input);
			message = brasero_notify_message_add (BRASERO_NOTIFY (priv->message_input),
							      _("Please select another image."),
							      _("It doesn't appear to be a valid image or a valid cue file."),
							      -1,
							      BRASERO_NOTIFY_CONTEXT_SIZE);
		}
	}
	else if (valid == BRASERO_SESSION_DISC_PROTECTED) {
		GtkWidget *message;

		if (priv->message_input) {
			gtk_widget_show (priv->message_input);
			message = brasero_notify_message_add (BRASERO_NOTIFY (priv->message_input),
							      _("Please insert a disc that is not copy protected."),
							      _("Such a disc cannot be copied without the proper plugins."),
							      -1,
							      BRASERO_NOTIFY_CONTEXT_SIZE);
		}
	}
	else if (valid == BRASERO_SESSION_NOT_SUPPORTED) {
		brasero_notify_message_add (BRASERO_NOTIFY (priv->message_output),
					    _("Please replace the disc with a supported CD or DVD."),
					    _("It is not possible to write with the current set of plugins."),
					    -1,
					    BRASERO_NOTIFY_CONTEXT_SIZE);
	}
	else if (valid == BRASERO_SESSION_OVERBURN_NECESSARY) {
		GtkWidget *message;

		message = brasero_notify_message_add (BRASERO_NOTIFY (priv->message_output),
						      _("Would you like to burn beyond the disc reported capacity?"),
						      _("The size of the project is too large for the disc and you must remove files from the project otherwise."
							"\nYou may want to use this option if you're using 90 or 100 min CD-R(W) which cannot be properly recognised and therefore need overburn option."
							"\nNOTE: This option might cause failure."),
						      -1,
						      BRASERO_NOTIFY_CONTEXT_SIZE);
		brasero_notify_button_add (BRASERO_NOTIFY (priv->message_output),
					   BRASERO_DISC_MESSAGE (message),
					   _("_Overburn"),
					   _("Burn beyond the disc reported capacity"),
					   GTK_RESPONSE_OK);

		g_signal_connect (message,
				  "response",
				  G_CALLBACK (brasero_burn_options_message_response_cb),
				  self);
	}
	else if (brasero_burn_session_same_src_dest_drive (BRASERO_BURN_SESSION (priv->session))) {
		/* The medium is valid but it's a special case */
		brasero_notify_message_add (BRASERO_NOTIFY (priv->message_output),
					    _("The drive that holds the source disc will also be the one used to record."),
					    _("A new recordable disc will be required once the one currently loaded has been copied."),
					    -1,
					    BRASERO_NOTIFY_CONTEXT_SIZE);
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
	gtk_dialog_set_has_separator (GTK_DIALOG (object), FALSE);
	gtk_window_set_resizable (GTK_WINDOW (object), FALSE);
	gtk_window_set_icon_name (GTK_WINDOW (object), "brasero");
}

/**
 * To build anything we need to have the session set first
 */

static void
brasero_burn_options_build_contents (BraseroBurnOptions *object)
{
	BraseroBurnOptionsPrivate *priv;
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
			       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

	/* Create a default Burn button */
	priv->button = gtk_dialog_add_button (GTK_DIALOG (object),
					      _("_Burn"),
					      GTK_RESPONSE_OK);
	gtk_button_set_image (GTK_BUTTON (priv->button),
			      gtk_image_new_from_icon_name ("media-optical-burn",
							    GTK_ICON_SIZE_BUTTON));

	/* Create an upper box for sources */
	priv->source = gtk_alignment_new (0.0, 0.5, 1.0, 1.0);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (object)->vbox),
			    priv->source,
			    FALSE,
			    TRUE,
			    0);

	/* Medium selection box */
	selection = gtk_hbox_new (FALSE, 12);
	gtk_widget_show (selection);

	alignment = gtk_alignment_new (0.0, 0.5, 1.0, 0.0);
	gtk_widget_show (alignment);
	gtk_box_pack_start (GTK_BOX (selection),
			    alignment,
			    TRUE,
			    TRUE,
			    0);

	priv->selection = brasero_dest_selection_new (BRASERO_BURN_SESSION (priv->session));
	gtk_widget_show (priv->selection);
	gtk_container_add (GTK_CONTAINER (alignment), priv->selection);

	priv->properties = brasero_medium_properties_new (BRASERO_BURN_SESSION (priv->session));
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

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (object)->vbox),
			    selection,
			    FALSE,
			    TRUE,
			    0);

	/* Create a lower box for options */
	alignment = gtk_alignment_new (0.0, 0.5, 1.0, 1.0);
	gtk_widget_show (alignment);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (object)->vbox),
			    alignment,
			    FALSE,
			    TRUE,
			    0);

	priv->options = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (alignment), priv->options);

	priv->valid_sig = g_signal_connect (priv->session,
					    "is-valid",
					    G_CALLBACK (brasero_burn_options_valid_cb),
					    object);

	brasero_burn_options_update_valid (object);
}

static void
brasero_burn_options_finalize (GObject *object)
{
	BraseroBurnOptionsPrivate *priv;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (object);

	if (priv->not_ready_id) {
		g_source_remove (priv->not_ready_id);
		priv->not_ready_id = 0;
	}

	if (priv->valid_sig) {
		g_signal_handler_disconnect (priv->session,
					     priv->valid_sig);
		priv->valid_sig = 0;
	}

	if (priv->session) {
		g_object_unref (priv->session);
		priv->session = NULL;
	}

	if (priv->size_group) {
		g_object_unref (priv->size_group);
		priv->size_group = NULL;
	}

	G_OBJECT_CLASS (brasero_burn_options_parent_class)->finalize (object);
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
		priv->session = BRASERO_SESSION_CFG (g_value_get_object (value));
		g_object_ref (priv->session);
		g_object_notify (object, "session");

		brasero_burn_options_build_contents (BRASERO_BURN_OPTIONS (object));
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
brasero_burn_options_class_init (BraseroBurnOptionsClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroBurnOptionsPrivate));

	object_class->finalize = brasero_burn_options_finalize;
	object_class->set_property = brasero_burn_options_set_property;
	object_class->get_property = brasero_burn_options_get_property;

	g_object_class_install_property (object_class,
					 PROP_SESSION,
					 g_param_spec_object ("session",
							      "The session",
							      "The session to work with",
							      BRASERO_TYPE_BURN_SESSION,
							      G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY));
}

GtkWidget *
brasero_burn_options_new (BraseroSessionCfg *session)
{
	BraseroTrackType *type;
	GtkWidget *options;

	type = brasero_track_type_new ();
	brasero_burn_session_get_input_type (BRASERO_BURN_SESSION (session), type);

	if (brasero_track_type_get_has_data (type)
	||  brasero_track_type_get_has_stream (type)) {
		options = g_object_new (BRASERO_TYPE_DISC_OPTION_DIALOG,
					"session", session,
					NULL);
	}
	else if (brasero_track_type_get_has_medium (type)) {
		options = g_object_new (BRASERO_TYPE_DISC_COPY_DIALOG,
					"session", session,
					NULL);
	}
	else if (brasero_track_type_get_has_image (type)) {
		options = g_object_new (BRASERO_TYPE_IMAGE_OPTION_DIALOG,
					"session", session,
					NULL);
	}
	else
		options = NULL;

	brasero_track_type_free (type);

	return options;
}
