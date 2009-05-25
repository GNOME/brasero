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
#include <glib/gi18n-lib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

#include "brasero-misc.h"
#include "brasero-track-data.h"
#include "brasero-session.h"
#include "brasero-data-options.h"

typedef struct _BraseroDataOptionsPrivate BraseroDataOptionsPrivate;
struct _BraseroDataOptionsPrivate
{
	BraseroBurnSession *session;

	GtkWidget *joliet_toggle;

	guint joliet_warning:1;
	guint joliet_saved:1;
};

/* FIXME: we need to react to a valid signal so that if joliet is on and the
 * session is invalid we can try to see if deactivating it can make things
 * workable again. */

#define BRASERO_DATA_OPTIONS_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_DATA_OPTIONS, BraseroDataOptionsPrivate))

enum {
	PROP_0,
	PROP_SESSION
};

G_DEFINE_TYPE (BraseroDataOptions, brasero_data_options, GTK_TYPE_ALIGNMENT);

static void
brasero_data_options_set_tracks_image_fs (BraseroBurnSession *session,
					  BraseroImageFS fs_type_add,
					  BraseroImageFS fs_type_rm)
{
	GSList *tracks;
	GSList *iter;

	tracks = brasero_burn_session_get_tracks (session);
	for (iter = tracks; iter; iter = iter->next) {
		BraseroTrack *track;

		track = iter->data;
		if (!BRASERO_IS_TRACK_DATA (track))
			continue;

		if (fs_type_add != BRASERO_IMAGE_FS_NONE)
			brasero_track_data_add_fs (BRASERO_TRACK_DATA (track), fs_type_add);

		if (fs_type_rm != BRASERO_IMAGE_FS_NONE)
			brasero_track_data_rm_fs (BRASERO_TRACK_DATA (track), fs_type_rm);
	}
}

static void
brasero_data_options_set_joliet (BraseroDataOptions *dialog)
{
	BraseroDataOptionsPrivate *priv;
	BraseroTrackType *source = NULL;

	priv = BRASERO_DATA_OPTIONS_PRIVATE (dialog);

	if (!priv->joliet_toggle)
		return;

	/* NOTE: we don't check for the sensitive property since when
	 * something is compulsory the button is active but insensitive */
	source = brasero_track_type_new ();
	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->joliet_toggle)))
		brasero_data_options_set_tracks_image_fs (priv->session,
							  BRASERO_IMAGE_FS_NONE,
							  BRASERO_IMAGE_FS_JOLIET);
	else
		brasero_data_options_set_tracks_image_fs (priv->session,
							  BRASERO_IMAGE_FS_JOLIET,
							  BRASERO_IMAGE_FS_NONE);

	brasero_track_type_free (source);
}

static void
brasero_data_options_joliet_toggled_cb (GtkToggleButton *toggle,
					BraseroDataOptions *options)
{
	BraseroDataOptionsPrivate *priv;
	GtkResponseType answer;
	GtkWidget *message;
	gchar *secondary;

	priv = BRASERO_DATA_OPTIONS_PRIVATE (options);

	/* If the toggle button was active it means that either the user got the
	 * warning dialog or that it was on because no file required any change
	 * in their names so no need for the warning; especially when we turn it
	 * off. */
	if (!gtk_toggle_button_get_active (toggle))
		priv->joliet_warning = TRUE;

	if (priv->joliet_warning) {
		brasero_data_options_set_joliet (options);
		return;
	}

	message = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (options))),
					  GTK_DIALOG_DESTROY_WITH_PARENT|
					  GTK_DIALOG_MODAL,
					  GTK_MESSAGE_INFO,
					  GTK_BUTTONS_NONE,
					  _("Should files be renamed to be fully Windows-compatible?"));

	secondary = g_strdup_printf ("%s\n%s",
				     _("Some files don't have a suitable name for a fully Windows-compatible CD."),
				     _("Those names should be changed and truncated to 64 characters."));
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message), "%s", secondary);
	g_free (secondary);

	gtk_dialog_add_button (GTK_DIALOG (message),
			       _("_Disable Full Windows Compatibility"),
			       GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (message),
			       _("_Rename for Full Windows Compatibility"),
			       GTK_RESPONSE_YES);

	answer = gtk_dialog_run (GTK_DIALOG (message));
	gtk_widget_destroy (message);

	if (answer != GTK_RESPONSE_YES)
		gtk_toggle_button_set_active (toggle, FALSE);
	else
		brasero_data_options_set_joliet (options);

	priv->joliet_warning = TRUE;
}

#if 0

static gboolean
brasero_data_options_update_joliet (BraseroDataOptions *dialog)
{
	BraseroImageFS fs_type;
	BraseroBurnResult result;
	BraseroTrackType *source = NULL;
	BraseroDataOptionsPrivate *priv;

	priv = BRASERO_DATA_OPTIONS_PRIVATE (dialog);
	if (!priv->joliet_toggle)
		return FALSE;

	/* what we want to check Joliet support */
	source = brasero_track_type_new ();
	brasero_burn_session_get_input_type (priv->session, source);
	fs_type = brasero_track_type_get_data_fs (source);

	/* see if it's supported */
	brasero_track_type_set_data_fs (source,
					fs_type|
					BRASERO_IMAGE_FS_JOLIET);

	result = brasero_burn_session_input_supported (priv->session,
						       source,
						       FALSE);

	if (result != BRASERO_BURN_OK) {
		/* Not supported */
		priv->joliet_saved = (fs_type & BRASERO_IMAGE_FS_JOLIET);
		brasero_data_options_set_tracks_image_fs (priv->session,
							  BRASERO_IMAGE_FS_NONE,
							  BRASERO_IMAGE_FS_JOLIET);

		gtk_widget_set_sensitive (priv->joliet_toggle, FALSE);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->joliet_toggle), FALSE);
	}
	else if (!GTK_WIDGET_IS_SENSITIVE (priv->joliet_toggle)) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->joliet_toggle), priv->joliet_saved);

		if (priv->joliet_saved || (fs_type & BRASERO_IMAGE_FS_JOLIET))
			brasero_data_options_set_tracks_image_fs (priv->session,
								  BRASERO_IMAGE_FS_JOLIET,
								  BRASERO_IMAGE_FS_NONE);

		gtk_widget_set_sensitive (priv->joliet_toggle, TRUE);
	}
}

#endif

static void
brasero_data_options_update_joliet_start (BraseroDataOptions *dialog)
{
	BraseroImageFS fs_type;
	BraseroBurnResult result;
	BraseroTrackType *source = NULL;
	BraseroDataOptionsPrivate *priv;

	priv = BRASERO_DATA_OPTIONS_PRIVATE (dialog);
	if (!priv->joliet_toggle)
		return;

	/* what we want to check Joliet support */
	source = brasero_track_type_new ();
	brasero_burn_session_get_input_type (priv->session, source);
	fs_type = brasero_track_type_get_data_fs (source);

	brasero_track_type_set_data_fs (source,
					fs_type|
					BRASERO_IMAGE_FS_JOLIET);
	result = brasero_burn_session_input_supported (priv->session,
						       source,
						       FALSE);

	g_signal_handlers_block_by_func (priv->joliet_toggle,
					 brasero_data_options_joliet_toggled_cb,
					 dialog);

	if (result != BRASERO_BURN_OK) {
		/* Not supported */
		brasero_data_options_set_tracks_image_fs (priv->session,
							  BRASERO_IMAGE_FS_NONE,
							  BRASERO_IMAGE_FS_JOLIET);

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->joliet_toggle), FALSE);
		gtk_widget_set_sensitive (priv->joliet_toggle, FALSE);
	}
	else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->joliet_toggle),
					      (fs_type & BRASERO_IMAGE_FS_JOLIET));
		gtk_widget_set_sensitive (priv->joliet_toggle, TRUE);
	}

	g_signal_handlers_unblock_by_func (priv->joliet_toggle,
					   brasero_data_options_joliet_toggled_cb,
					   dialog);
}

static void
brasero_data_options_set_property (GObject *object,
				   guint prop_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
	BraseroDataOptionsPrivate *priv;

	g_return_if_fail (BRASERO_IS_DATA_OPTIONS (object));

	priv = BRASERO_DATA_OPTIONS_PRIVATE (object);

	switch (prop_id)
	{
	case PROP_SESSION: /* Readable and only writable at creation time */
		priv->session = BRASERO_BURN_SESSION (g_value_get_object (value));
		g_object_ref (priv->session);
		brasero_data_options_update_joliet_start (BRASERO_DATA_OPTIONS (object));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
brasero_data_options_get_property (GObject *object,
				   guint prop_id,
				   GValue *value,
				   GParamSpec *pspec)
{
	BraseroDataOptionsPrivate *priv;

	g_return_if_fail (BRASERO_IS_DATA_OPTIONS (object));

	priv = BRASERO_DATA_OPTIONS_PRIVATE (object);

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
brasero_data_options_init (BraseroDataOptions *object)
{
	BraseroDataOptionsPrivate *priv;
	GtkWidget *options;
	gchar *string;

	priv = BRASERO_DATA_OPTIONS_PRIVATE (object);

	/* general options */
	priv->joliet_toggle = gtk_check_button_new_with_mnemonic (_("Increase compatibility with _Windows systems"));
	gtk_widget_set_tooltip_text (priv->joliet_toggle,
				     _("Improve compatibility with Windows systems by allowing to display long filenames (maximum 64 characters)"));

	g_signal_connect (priv->joliet_toggle,
			  "toggled",
			  G_CALLBACK (brasero_data_options_joliet_toggled_cb),
			  object);

	string = g_strdup_printf ("<b>%s</b>", _("Disc options"));
	options = brasero_utils_pack_properties (string,
						 priv->joliet_toggle,
						 NULL);
	g_free (string);

	gtk_widget_show_all (options);
	gtk_container_add (GTK_CONTAINER (object), options);
}

static void
brasero_data_options_finalize (GObject *object)
{
	BraseroDataOptionsPrivate *priv;

	priv = BRASERO_DATA_OPTIONS_PRIVATE (object);
	if (priv->session) {
		g_object_unref (priv->session);
		priv->session = NULL;
	}

	G_OBJECT_CLASS (brasero_data_options_parent_class)->finalize (object);
}

static void
brasero_data_options_class_init (BraseroDataOptionsClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroDataOptionsPrivate));

	object_class->finalize = brasero_data_options_finalize;
	object_class->set_property = brasero_data_options_set_property;
	object_class->get_property = brasero_data_options_get_property;

	g_object_class_install_property (object_class,
					 PROP_SESSION,
					 g_param_spec_object ("session",
							      "The session",
							      "The session to work with",
							      BRASERO_TYPE_BURN_SESSION,
							      G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY));
}

GtkWidget *
brasero_data_options_new (BraseroBurnSession *session)
{
	return g_object_new (BRASERO_TYPE_DATA_OPTIONS, "session", session, NULL);
}

