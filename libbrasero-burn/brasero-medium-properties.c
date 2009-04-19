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

#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtk.h>

#include "brasero-medium-properties.h"
#include "brasero-drive-properties.h"
#include "brasero-image-properties.h"
#include "brasero-session-cfg.h"
#include "brasero-session-helper.h"

#include "burn-basics.h"
#include "brasero-track.h"
#include "brasero-medium.h"
#include "brasero-session.h"
#include "burn-image-format.h"

typedef struct _BraseroMediumPropertiesPrivate BraseroMediumPropertiesPrivate;
struct _BraseroMediumPropertiesPrivate
{
	BraseroBurnSession *session;

	GtkWidget *medium_prop;

	glong valid_sig;

	guint default_format:1;
	guint default_path:1;
	guint default_ext:1;
};

#define BRASERO_MEDIUM_PROPERTIES_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_MEDIUM_PROPERTIES, BraseroMediumPropertiesPrivate))

enum {
	PROP_0,
	PROP_SESSION
};

G_DEFINE_TYPE (BraseroMediumProperties, brasero_medium_properties, GTK_TYPE_BUTTON);

static void
brasero_medium_properties_drive_properties (BraseroMediumProperties *self)
{
	BraseroMediumPropertiesPrivate *priv;
	BraseroBurnFlag compulsory = 0;
	BraseroBurnFlag supported = 0;
	BraseroBurnFlag flags = 0;
	BraseroDrive *drive;
	GtkWidget *toplevel;
	const gchar *path;
	gint result;
	gint64 rate;

	priv = BRASERO_MEDIUM_PROPERTIES_PRIVATE (self);

	/* Build dialog */
	priv->medium_prop = brasero_drive_properties_new ();

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
	gtk_window_set_transient_for (GTK_WINDOW (priv->medium_prop), GTK_WINDOW (toplevel));
	gtk_window_set_destroy_with_parent (GTK_WINDOW (priv->medium_prop), TRUE);
	gtk_window_set_position (GTK_WINDOW (toplevel), GTK_WIN_POS_CENTER_ON_PARENT);

	/* get information */
	drive = brasero_burn_session_get_burner (priv->session);
	rate = brasero_burn_session_get_rate (priv->session);

	brasero_drive_properties_set_drive (BRASERO_DRIVE_PROPERTIES (priv->medium_prop),
					    drive,
					    rate);

	flags = brasero_burn_session_get_flags (priv->session);
	brasero_burn_session_get_burn_flags (priv->session,
					     &supported,
					     &compulsory);

	brasero_drive_properties_set_flags (BRASERO_DRIVE_PROPERTIES (priv->medium_prop),
					    flags,
					    supported,
					    compulsory);

	path = brasero_burn_session_get_tmpdir (priv->session);
	brasero_drive_properties_set_tmpdir (BRASERO_DRIVE_PROPERTIES (priv->medium_prop),
					     path);

	/* launch the dialog */
	gtk_widget_show_all (priv->medium_prop);
	result = gtk_dialog_run (GTK_DIALOG (priv->medium_prop));
	if (result != GTK_RESPONSE_ACCEPT) {
		gtk_widget_destroy (priv->medium_prop);
		priv->medium_prop = NULL;
		return;
	}

	rate = brasero_drive_properties_get_rate (BRASERO_DRIVE_PROPERTIES (priv->medium_prop));
	brasero_burn_session_set_rate (priv->session, rate);

	brasero_burn_session_remove_flag (priv->session, BRASERO_DRIVE_PROPERTIES_FLAGS);
	flags = brasero_drive_properties_get_flags (BRASERO_DRIVE_PROPERTIES (priv->medium_prop));
	brasero_session_cfg_add_flags (BRASERO_SESSION_CFG (priv->session), flags);

	path = brasero_drive_properties_get_tmpdir (BRASERO_DRIVE_PROPERTIES (priv->medium_prop));
	brasero_burn_session_set_tmpdir (priv->session, path);

	gtk_widget_destroy (priv->medium_prop);
	priv->medium_prop = NULL;
}

static gchar *
brasero_medium_properties_get_output_path (BraseroMediumProperties *self)
{
	gchar *path = NULL;
	BraseroImageFormat format;
	BraseroMediumPropertiesPrivate *priv;

	priv = BRASERO_MEDIUM_PROPERTIES_PRIVATE (self);

	format = brasero_burn_session_get_output_format (priv->session);
	switch (format) {
	case BRASERO_IMAGE_FORMAT_BIN:
		brasero_burn_session_get_output (priv->session,
						 &path,
						 NULL,
						 NULL);
		break;

	case BRASERO_IMAGE_FORMAT_CLONE:
		brasero_burn_session_get_output (priv->session,
						 NULL,
						 &path,
						 NULL);
		break;

	case BRASERO_IMAGE_FORMAT_CDRDAO:
		brasero_burn_session_get_output (priv->session,
						 NULL,
						 &path,
						 NULL);
		break;

	case BRASERO_IMAGE_FORMAT_CUE:
		brasero_burn_session_get_output (priv->session,
						 NULL,
						 &path,
						 NULL);
		break;

	default:
		break;
	}

	return path;
}

static void
brasero_medium_properties_set_output_path (BraseroMediumProperties *self,
					   BraseroImageFormat format,
					   const gchar *path)
{
	BraseroMediumPropertiesPrivate *priv;

	priv = BRASERO_MEDIUM_PROPERTIES_PRIVATE (self);

	switch (format) {
	case BRASERO_IMAGE_FORMAT_BIN:
		brasero_burn_session_set_image_output_full (priv->session,
							    format,
							    path,
							    NULL);
		break;

	case BRASERO_IMAGE_FORMAT_CDRDAO:
	case BRASERO_IMAGE_FORMAT_CLONE:
	case BRASERO_IMAGE_FORMAT_CUE:
		brasero_burn_session_set_image_output_full (priv->session,
							    format,
							    NULL,
							    path);
		break;

	default:
		break;
	}
}

static guint
brasero_medium_properties_get_possible_output_formats (BraseroMediumProperties *self,
						       BraseroImageFormat *formats)
{
	guint num = 0;
	BraseroImageFormat format;
	BraseroTrackType *output = NULL;
	BraseroMediumPropertiesPrivate *priv;

	priv = BRASERO_MEDIUM_PROPERTIES_PRIVATE (self);

	/* see how many output format are available */
	format = BRASERO_IMAGE_FORMAT_CDRDAO;
	(*formats) = BRASERO_IMAGE_FORMAT_NONE;

	output = brasero_track_type_new ();
	brasero_track_type_set_has_image (output);

	for (; format > BRASERO_IMAGE_FORMAT_NONE; format >>= 1) {
		BraseroBurnResult result;

		brasero_track_type_set_image_format (output, format);
		result = brasero_burn_session_output_supported (priv->session, output);
		if (result == BRASERO_BURN_OK) {
			(*formats) |= format;
			num ++;
		}
	}

	brasero_track_type_free (output);

	return num;
}

static void
brasero_medium_properties_image_format_changed_cb (BraseroImageProperties *dialog,
						   BraseroMediumProperties *self)
{
	BraseroMediumPropertiesPrivate *priv;
	BraseroImageFormat format;
	gchar *image_path;

	priv = BRASERO_MEDIUM_PROPERTIES_PRIVATE (self);

	/* make sure the extension is still valid */
	image_path = brasero_image_properties_get_path (dialog);
	if (!image_path)
		return;

	format = brasero_image_properties_get_format (dialog);

	if (format == BRASERO_IMAGE_FORMAT_ANY || format == BRASERO_IMAGE_FORMAT_NONE)
		format = brasero_burn_session_get_default_output_format (priv->session);

	if (priv->default_path && !brasero_image_properties_is_path_edited (dialog)) {
		/* not changed: get a new default path */
		g_free (image_path);
		image_path = brasero_image_format_get_default_path (format);
	}
	else if (image_path) {
		gchar *tmp;

		tmp = image_path;
		image_path = brasero_image_format_fix_path_extension (format, FALSE, image_path);
		g_free (tmp);
	}
	else {
		priv->default_path = TRUE;
		image_path = brasero_image_format_get_default_path (format);
	}

	brasero_image_properties_set_path (dialog, image_path);
}

static gboolean
brasero_medium_properties_image_check_extension (BraseroMediumProperties *self,
						 BraseroImageFormat format,
						 const gchar *path)
{
	gchar *dot;
	const gchar *suffixes [] = {".iso",
				    ".toc",
				    ".cue",
				    ".toc",
				    NULL };

	dot = g_utf8_strrchr (path, -1, '.');
	if (dot) {
		if (format & BRASERO_IMAGE_FORMAT_BIN
		&& !strcmp (suffixes [0], dot))
			return TRUE;
		else if (format & BRASERO_IMAGE_FORMAT_CLONE
		     && !strcmp (suffixes [1], dot))
			return TRUE;
		else if (format & BRASERO_IMAGE_FORMAT_CUE
		     && !strcmp (suffixes [2], dot))
			return TRUE;
		else if (format & BRASERO_IMAGE_FORMAT_CDRDAO
		     && !strcmp (suffixes [3], dot))
			return TRUE;
	}

	return FALSE;
}

static gboolean
brasero_medium_properties_image_extension_ask (BraseroMediumProperties *self)
{
	GtkWidget *dialog;
	GtkWidget *toplevel;
	GtkResponseType answer;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
	dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					 GTK_DIALOG_DESTROY_WITH_PARENT |
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_WARNING,
					 GTK_BUTTONS_NONE,
					 _("Do you really want to keep the current extension for the disc image name?"));

		
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  _("If you choose to keep it, programs may not be able to recognize the file type properly."));

	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       _("_Keep Current Extension"),
			       GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       _("Change _Extension"),
			       GTK_RESPONSE_YES);

	answer = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	if (answer == GTK_RESPONSE_YES)
		return TRUE;

	return FALSE;
}

static void
brasero_medium_properties_image_properties (BraseroMediumProperties *self)
{
	BraseroMediumPropertiesPrivate *priv;
	BraseroImageFormat formats;
	BraseroImageFormat format;
	gulong format_changed;
	gchar *original_path;
	GtkWindow *toplevel;
	gchar *image_path;
	gint answer;
	guint num;

	priv = BRASERO_MEDIUM_PROPERTIES_PRIVATE (self);

	priv->medium_prop = brasero_image_properties_new ();

	toplevel = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self)));
	gtk_window_set_transient_for (GTK_WINDOW (priv->medium_prop), GTK_WINDOW (toplevel));
	gtk_window_set_destroy_with_parent (GTK_WINDOW (priv->medium_prop), TRUE);
	gtk_window_set_position (GTK_WINDOW (toplevel), GTK_WIN_POS_CENTER_ON_PARENT);

	/* set all information namely path and format */
	original_path = brasero_medium_properties_get_output_path (self);
	brasero_image_properties_set_path (BRASERO_IMAGE_PROPERTIES (priv->medium_prop), original_path);
	g_free (original_path);

	if (!priv->default_format)
		format = brasero_burn_session_get_output_format (priv->session);
	else
		format = BRASERO_IMAGE_FORMAT_ANY;

	num = brasero_medium_properties_get_possible_output_formats (self, &formats);
	brasero_image_properties_set_formats (BRASERO_IMAGE_PROPERTIES (priv->medium_prop),
					      num > 0 ? formats:BRASERO_IMAGE_FORMAT_NONE,
					      format);

	format_changed = g_signal_connect (priv->medium_prop,
					   "format-changed",
					   G_CALLBACK (brasero_medium_properties_image_format_changed_cb),
					   self);

	/* and here we go ... run the thing */
	gtk_widget_show (priv->medium_prop);
	answer = gtk_dialog_run (GTK_DIALOG (priv->medium_prop));

	g_signal_handler_disconnect (priv->medium_prop, format_changed);

	if (answer != GTK_RESPONSE_OK) {
		gtk_widget_destroy (priv->medium_prop);
		priv->medium_prop = NULL;
		return;
	}

	/* get and check format */
	format = brasero_image_properties_get_format (BRASERO_IMAGE_PROPERTIES (priv->medium_prop));

	/* see if we are to choose the format ourselves */
	if (format == BRASERO_IMAGE_FORMAT_ANY || format == BRASERO_IMAGE_FORMAT_NONE) {
		format = brasero_burn_session_get_default_output_format (priv->session);
		priv->default_format = TRUE;
	}
	else
		priv->default_format = FALSE;

	/* see if the user has changed the path */
	if (brasero_image_properties_is_path_edited (BRASERO_IMAGE_PROPERTIES (priv->medium_prop)))
		priv->default_path = FALSE;

	if (!priv->default_path) {
		/* check the extension */
		image_path = brasero_image_properties_get_path (BRASERO_IMAGE_PROPERTIES (priv->medium_prop));

		/* there is one special case: CLONE image tocs _must_ have a
		 * correct suffix ".toc" so don't ask, fix it */
		if (!brasero_medium_properties_image_check_extension (self, format, image_path)) {
			if (format == BRASERO_IMAGE_FORMAT_CLONE
			||  brasero_medium_properties_image_extension_ask (self)) {
				gchar *tmp;

				priv->default_ext = TRUE;
				tmp = image_path;
				image_path = brasero_image_format_fix_path_extension (format, TRUE, image_path);
				g_free (tmp);
			}
			else
				priv->default_ext = FALSE;
		}
	}
	else
		image_path = brasero_image_format_get_default_path (format);

	gtk_widget_destroy (priv->medium_prop);
	priv->medium_prop = NULL;

	brasero_medium_properties_set_output_path (self,
						format,
						image_path);
	g_free (image_path);
}

static void
brasero_medium_properties_clicked (GtkButton *button)
{
	BraseroMediumPropertiesPrivate *priv;
	BraseroDrive *drive;

	priv = BRASERO_MEDIUM_PROPERTIES_PRIVATE (button);

	drive = brasero_burn_session_get_burner (priv->session);
	if (!drive)
		return;

	if (brasero_drive_is_fake (drive))
		brasero_medium_properties_image_properties (BRASERO_MEDIUM_PROPERTIES (button));
	else
		brasero_medium_properties_drive_properties (BRASERO_MEDIUM_PROPERTIES (button));
}

static void
brasero_medium_properties_update_image_output (BraseroMediumProperties *self,
					       gboolean is_valid)
{
	BraseroMediumPropertiesPrivate *priv;
	BraseroImageFormat valid_format;
	BraseroImageFormat format;
	gchar *path = NULL;

	priv = BRASERO_MEDIUM_PROPERTIES_PRIVATE (self);

	/* Get session current state */
	format = brasero_burn_session_get_output_format (priv->session);
	valid_format = format;

	/* Check current set format if it's invalid */
	if (format != BRASERO_IMAGE_FORMAT_NONE) {
		/* The user set a format. There is nothing to do about it except
		 * checking if the format is still available. If not, then set
		 * default and remove the current one */
		if (!is_valid) {
			priv->default_format = TRUE;
			valid_format = brasero_burn_session_get_default_output_format (priv->session);
		}
		else if (priv->default_format) {
			/* since input, or caps changed, check if there isn't a
			 * better format available. */
			valid_format = brasero_burn_session_get_default_output_format (priv->session);
		}
	}
	else {
		/* This is always invalid; find one */
		priv->default_format = TRUE;
		valid_format = brasero_burn_session_get_default_output_format (priv->session);
	}

	/* see if we have a workable format */
	if (valid_format == BRASERO_IMAGE_FORMAT_NONE) {
		if (priv->medium_prop) {
			gtk_widget_destroy (priv->medium_prop);
			priv->medium_prop = NULL;
		}

		return;
	}

	path = brasero_medium_properties_get_output_path (self);

	/* Now check, fix the output path, _provided__the__format__changed_ */
	if (valid_format == format) {
		g_free (path);
		return;
	}

	if (!path) {
		priv->default_path = TRUE;
		priv->default_ext = TRUE;
		path = brasero_image_format_get_default_path (valid_format);
	}
	else if (priv->default_ext
	     &&  brasero_medium_properties_image_check_extension (self, format, path)) {
		gchar *tmp;

		priv->default_ext = TRUE;

		tmp = path;
		path = brasero_image_format_fix_path_extension (format, TRUE, path);
		g_free (tmp);
	}

	/* we always need to do this */
	brasero_medium_properties_set_output_path (self,
						valid_format,
						path);

	g_free (path);

	if (priv->medium_prop) {
		BraseroImageFormat formats;
		guint num;

		/* update image settings dialog if needed */
		num = brasero_medium_properties_get_possible_output_formats (self, &formats);
		brasero_image_properties_set_formats (BRASERO_IMAGE_PROPERTIES (priv->medium_prop),
						      num > 1 ? formats:BRASERO_IMAGE_FORMAT_NONE,
						      BRASERO_IMAGE_FORMAT_ANY);
	}
}

static void
brasero_medium_properties_valid_session (BraseroSessionCfg *session,
					 BraseroMediumProperties *self)
{
	BraseroMediumPropertiesPrivate *priv;

	priv = BRASERO_MEDIUM_PROPERTIES_PRIVATE (self);

	/* make sure the current displayed path is valid */
	if (brasero_burn_session_is_dest_file (priv->session))
		brasero_medium_properties_update_image_output (self, BRASERO_SESSION_IS_VALID (brasero_session_cfg_get_error (session)));
}

static void
brasero_medium_properties_init (BraseroMediumProperties *object)
{
	BraseroMediumPropertiesPrivate *priv;

	priv = BRASERO_MEDIUM_PROPERTIES_PRIVATE (object);

	gtk_widget_set_tooltip_text (GTK_WIDGET (object), _("Configure recording options"));

	priv->default_ext = TRUE;
	priv->default_path = TRUE;
	priv->default_format = TRUE;
}

static void
brasero_medium_properties_finalize (GObject *object)
{
	BraseroMediumPropertiesPrivate *priv;

	priv = BRASERO_MEDIUM_PROPERTIES_PRIVATE (object);

	if (priv->valid_sig) {
		g_signal_handler_disconnect (priv->session,
					     priv->valid_sig);
		priv->valid_sig = 0;
	}

	if (priv->session) {
		g_object_unref (priv->session);
		priv->session = NULL;
	}

	G_OBJECT_CLASS (brasero_medium_properties_parent_class)->finalize (object);
}

static void
brasero_medium_properties_set_property (GObject *object,
					guint property_id,
					const GValue *value,
					GParamSpec *pspec)
{
	BraseroMediumPropertiesPrivate *priv;
	BraseroBurnSession *session;

	priv = BRASERO_MEDIUM_PROPERTIES_PRIVATE (object);

	switch (property_id) {
	case PROP_SESSION:
		if (priv->session)
			g_object_unref (priv->session);

		session = g_value_get_object (value);

		/* NOTE: no need to unref a potential previous session since
		 * it's only set at construct time */
		priv->session = session;
		g_object_ref (session);

		priv->valid_sig = g_signal_connect (session,
						    "is-valid",
						    G_CALLBACK (brasero_medium_properties_valid_session),
						    object);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
brasero_medium_properties_get_property (GObject *object,
				     guint property_id,
				     GValue *value,
				     GParamSpec *pspec)
{
	BraseroMediumPropertiesPrivate *priv;

	priv = BRASERO_MEDIUM_PROPERTIES_PRIVATE (object);

	switch (property_id) {
	case PROP_SESSION:
		g_value_set_object (value, priv->session);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
brasero_medium_properties_class_init (BraseroMediumPropertiesClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	GtkButtonClass* button_class = GTK_BUTTON_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroMediumPropertiesPrivate));

	object_class->finalize = brasero_medium_properties_finalize;
	object_class->set_property = brasero_medium_properties_set_property;
	object_class->get_property = brasero_medium_properties_get_property;

	button_class->clicked = brasero_medium_properties_clicked;
	g_object_class_install_property (object_class,
					 PROP_SESSION,
					 g_param_spec_object ("session",
							      "The session to work with",
							      "The session to work with",
							      BRASERO_TYPE_BURN_SESSION,
							      G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY));

}

GtkWidget *
brasero_medium_properties_new (BraseroBurnSession *session)
{
	return g_object_new (BRASERO_TYPE_MEDIUM_PROPERTIES,
			     "session", session,
			     "use-stock", TRUE,
			     "label", GTK_STOCK_PROPERTIES,
			     "focus-on-click", FALSE,
			     NULL);
}
