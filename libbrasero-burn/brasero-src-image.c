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
#include <glib/gi18n-lib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

#include "burn-basics.h"

#include "brasero-drive.h"

#include "brasero-misc.h"

#include "brasero-image-type-chooser.h"
#include "brasero-session-cfg.h"
#include "brasero-track-image.h"
#include "brasero-track-image-cfg.h"
#include "brasero-src-image.h"
#include "burn-image-format.h"

typedef struct _BraseroSrcImagePrivate BraseroSrcImagePrivate;
struct _BraseroSrcImagePrivate
{
	BraseroBurnSession *session;
	BraseroTrackImageCfg *track;

	gchar *folder;
	GCancellable *cancel;

	GtkWidget *format;
	GtkWidget *label;
	GtkWidget *file;

	GSettings *settings;
};

#define BRASERO_SRC_IMAGE_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_SRC_IMAGE, BraseroSrcImagePrivate))

G_DEFINE_TYPE (BraseroSrcImage, brasero_src_image, GTK_TYPE_BUTTON);

enum {
	PROP_0,
	PROP_SESSION
};

#define BRASERO_SCHEMA_DISPLAY			"org.gnome.brasero.display"
#define BRASERO_PROPS_ISO_DIRECTORY		"iso-folder"

static const gchar *mimes [] = { "application/x-cd-image",
				 "application/x-cue",
				 "application/x-toc",
				 "application/x-cdrdao-toc" };

static void
brasero_src_image_save (BraseroSrcImage *self)
{
	gchar *uri = NULL;
	GtkRecentManager *recent;
	BraseroImageFormat format;
	gchar *groups [] = { "brasero",
			      NULL };
	GtkRecentData recent_data = { NULL,
				      NULL,
				      NULL,
				      "brasero",
				      "brasero -p %u",
				      groups,
				      FALSE };
	BraseroSrcImagePrivate *priv;

	priv = BRASERO_SRC_IMAGE_PRIVATE (self);

	format = brasero_track_image_get_format (BRASERO_TRACK_IMAGE (priv->track));
	if (format == BRASERO_IMAGE_FORMAT_NONE)
		return;

	/* Add it to recent file manager */
	switch (format) {
	case BRASERO_IMAGE_FORMAT_BIN:
		recent_data.mime_type = (gchar *) mimes [0];
		uri = brasero_track_image_get_source (BRASERO_TRACK_IMAGE (priv->track), TRUE);
		break;

	case BRASERO_IMAGE_FORMAT_CUE:
		recent_data.mime_type = (gchar *) mimes [1];
		uri = brasero_track_image_get_toc_source (BRASERO_TRACK_IMAGE (priv->track), TRUE);
		break;

	case BRASERO_IMAGE_FORMAT_CLONE:
		recent_data.mime_type = (gchar *) mimes [2];
		uri = brasero_track_image_get_toc_source (BRASERO_TRACK_IMAGE (priv->track), TRUE);
		break;

	case BRASERO_IMAGE_FORMAT_CDRDAO:
		recent_data.mime_type = (gchar *) mimes [3];
		uri = brasero_track_image_get_toc_source (BRASERO_TRACK_IMAGE (priv->track), TRUE);
		break;

	default:
		return;
	}

	if (!uri)
		return;

	/* save as recent */
	recent = gtk_recent_manager_get_default ();
	gtk_recent_manager_add_full (recent,
				     uri,
				     &recent_data);
	g_free (uri);
}

static void
brasero_src_image_error (BraseroSrcImage *self,
			 GError *error)
{
	BraseroSrcImagePrivate *priv;
	GtkWidget *toplevel;

	priv = BRASERO_SRC_IMAGE_PRIVATE (self);
	if (priv->file)
		toplevel = priv->file;
	else
		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));

	if (!GTK_IS_WINDOW (toplevel))
		return;

	brasero_utils_message_dialog (toplevel,
				      /* Translators: this is a disc image, not a picture */
				      C_("disc", "Please select another image."),
				      error->message,
				      GTK_MESSAGE_ERROR);
}

static void
brasero_src_image_update (BraseroSrcImage *self)
{
	gchar *uri;
	gchar *name;
	gchar *string;
	goffset bytes = 0;
	GFile *file = NULL;
	GError *error = NULL;
	BraseroStatus *status;
	BraseroBurnResult result;
	BraseroImageFormat format;
	gchar *size_string = NULL;
	BraseroSrcImagePrivate *priv;

	priv = BRASERO_SRC_IMAGE_PRIVATE (self);

	if (!priv->track)
		return;

	/* Retrieve a path or an uri */
	format = brasero_track_image_get_format (BRASERO_TRACK_IMAGE (priv->track));
	switch (format) {
	case BRASERO_IMAGE_FORMAT_NONE:
	case BRASERO_IMAGE_FORMAT_BIN:
		uri = brasero_track_image_get_source (BRASERO_TRACK_IMAGE (priv->track), TRUE);
		break;

	case BRASERO_IMAGE_FORMAT_CLONE:
	case BRASERO_IMAGE_FORMAT_CUE:
	case BRASERO_IMAGE_FORMAT_CDRDAO:
		uri = brasero_track_image_get_toc_source (BRASERO_TRACK_IMAGE (priv->track), TRUE);
		break;

	default:
		uri = NULL;
		break;
	}

	if (!uri)
		return;

	file = g_file_new_for_uri (uri);
	g_free (uri);

	name = g_file_get_basename (file);
	if (!name) {
		if (file)
			g_object_unref (file);
		return;
	}

	/* See if information retrieval went fine and/or is ready */
	status = brasero_status_new ();
	result = brasero_track_get_status (BRASERO_TRACK (priv->track), status);
	if (result == BRASERO_BURN_NOT_READY || result == BRASERO_BURN_RUNNING) {
		/* Translators: %s is a path */
		string = g_strdup_printf (_("\"%s\": loading"), name);
		gtk_widget_set_tooltip_text (GTK_WIDGET (self), NULL);
		g_free (name);
		goto end;
	}
	else if (result != BRASERO_BURN_OK) {
		/* Translators: %s is a path and image refers to a disc image */
		string = g_strdup_printf (_("\"%s\": unknown disc image type"), name);
		g_free (name);

		error = brasero_status_get_error (status);
		if (!error)
			goto end;

		gtk_widget_set_tooltip_text (GTK_WIDGET (self), error->message);
		brasero_src_image_error (self, error);
		g_error_free (error);
		goto end;
	}

	uri = g_file_get_uri (file);
	gtk_widget_set_tooltip_text (GTK_WIDGET (self), uri);

	/* Deal with size */
	brasero_track_get_size (BRASERO_TRACK (priv->track), NULL, &bytes);
	size_string = g_format_size (bytes);

	/* NOTE to translators, the first %s is the path of the image
	 * file and the second its size. */
	string = g_strdup_printf (_("\"%s\": %s"), name, size_string);
	g_free (size_string);
	g_free (name);

end:

	if (file)
		g_object_unref (file);

	g_object_unref (status);
	if (string) {
		/* This is hackish and meant to avoid ellipsization to make the
		 * label too small. */
		if (strlen (string) > strlen (_("Click here to select a disc _image")) + 5)
			gtk_label_set_ellipsize (GTK_LABEL (priv->label), PANGO_ELLIPSIZE_START);
		else
			gtk_label_set_ellipsize (GTK_LABEL (priv->label), PANGO_ELLIPSIZE_NONE);

		gtk_label_set_text (GTK_LABEL (priv->label), string);
		g_free (string);
	}
}

static void
brasero_image_src_track_changed_cb (BraseroTrack *track,
				    BraseroSrcImage *dialog)
{
	brasero_src_image_update (dialog);
}

static void
brasero_src_image_check_parent_directory_cb (GObject *object,
					     GAsyncResult *result,
					     gpointer data)
{
	BraseroSrcImagePrivate *priv;
	GError *error = NULL;
	GFileInfo *info;

	priv = BRASERO_SRC_IMAGE_PRIVATE (data);

	info = g_file_query_info_finish (G_FILE (object), result, &error);
	if (!info) {
		g_error_free (error);
		return;
	}

	if (g_file_info_get_file_type (info) != G_FILE_TYPE_DIRECTORY) {
		g_object_unref (info);
		return;
	}
	g_object_unref (info);

	g_free (priv->folder);
	priv->folder = g_file_get_path (G_FILE (object));

	g_settings_set_string (priv->settings,
	                       BRASERO_PROPS_ISO_DIRECTORY,
	                       priv->folder? priv->folder:"");
}

static void
brasero_src_image_changed (BraseroSrcImage *dialog)
{
	gchar *uri;
	GFile *file;
	GFile *parent;
	BraseroImageFormat format;
	BraseroSrcImagePrivate *priv;

	priv = BRASERO_SRC_IMAGE_PRIVATE (dialog);

	brasero_image_type_chooser_get_format (BRASERO_IMAGE_TYPE_CHOOSER (priv->format), &format);
	uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (priv->file));
	brasero_track_image_cfg_force_format (priv->track, format);
	brasero_track_image_cfg_set_source (priv->track, uri);

	/* Make sure it's still a valid folder */
	file = g_file_new_for_uri (uri);
	parent = g_file_get_parent (file);
	g_object_unref (file);

	g_cancellable_reset (priv->cancel);
	g_file_query_info_async (parent,
				 G_FILE_ATTRIBUTE_STANDARD_TYPE,
				 G_FILE_QUERY_INFO_NONE,
				 0,
				 priv->cancel,
				 brasero_src_image_check_parent_directory_cb,
				 dialog);
	g_object_unref (parent);
	g_free (uri);
}

static void
brasero_src_image_set_formats (BraseroSrcImage *dialog)
{
	BraseroSrcImagePrivate *priv;
	BraseroImageFormat formats;
	BraseroImageFormat format;

	priv = BRASERO_SRC_IMAGE_PRIVATE (dialog);

	/* Show all formats here even if we miss a
	 * plugin to burn or use it */
	formats = BRASERO_IMAGE_FORMAT_BIN|
			 BRASERO_IMAGE_FORMAT_CUE|
			 BRASERO_IMAGE_FORMAT_CDRDAO|
			 BRASERO_IMAGE_FORMAT_CLONE;
	brasero_image_type_chooser_set_formats (BRASERO_IMAGE_TYPE_CHOOSER (priv->format), formats,  TRUE, FALSE);

	format = brasero_track_image_cfg_get_forced_format (priv->track);
	brasero_image_type_chooser_set_format (BRASERO_IMAGE_TYPE_CHOOSER (priv->format), format);
}

static gchar *
brasero_src_image_get_current_uri (BraseroSrcImage *self)
{
	BraseroSrcImagePrivate *priv;
	BraseroImageFormat format;
	gchar *uri = NULL;

	priv = BRASERO_SRC_IMAGE_PRIVATE (self);

	format = brasero_track_image_get_format (BRASERO_TRACK_IMAGE (priv->track));
	switch (format) {
	case BRASERO_IMAGE_FORMAT_NONE:
	case BRASERO_IMAGE_FORMAT_BIN:
		uri = brasero_track_image_get_source (BRASERO_TRACK_IMAGE (priv->track), TRUE);
		break;
	case BRASERO_IMAGE_FORMAT_CLONE:
	case BRASERO_IMAGE_FORMAT_CUE:
	case BRASERO_IMAGE_FORMAT_CDRDAO:
		uri = brasero_track_image_get_toc_source (BRASERO_TRACK_IMAGE (priv->track), TRUE);
		break;

	default:
		break;
	}

	return uri;
}

static void
brasero_src_image_clicked (GtkButton *button)
{
	BraseroSrcImagePrivate *priv;
	GtkResponseType response;
	GtkFileFilter *filter;
	GtkWidget *toplevel;
	GtkWidget *label;
	GtkWidget *box;
	gchar *uri;

	priv = BRASERO_SRC_IMAGE_PRIVATE (button);

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (button));
	priv->file = gtk_file_chooser_dialog_new (_("Select Disc Image"),
						  GTK_WINDOW (toplevel),
						  GTK_FILE_CHOOSER_ACTION_OPEN,
						  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						  GTK_STOCK_OPEN, GTK_RESPONSE_OK,
						  NULL);

	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (priv->file), FALSE);

	/* See if we have a URI already chosen, if so use it */
	uri = brasero_src_image_get_current_uri (BRASERO_SRC_IMAGE (button));
	if (uri) {
		if (!gtk_file_chooser_select_uri (GTK_FILE_CHOOSER (priv->file), uri))
			if (!gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (priv->file), priv->folder))
				gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (priv->file), g_get_home_dir ());

		g_free (uri);
	}
	else if (priv->folder) {
		if (!gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (priv->file), priv->folder))
			gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (priv->file), g_get_home_dir ());
	}
	else {
		/* if we haven't been able to get the saved parent folder type, give up */
		g_cancellable_cancel (priv->cancel);
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (priv->file), g_get_home_dir ());
	}

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("All files"));
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (priv->file), filter);

	filter = gtk_file_filter_new ();
	/* Translators: this a disc image here */
	gtk_file_filter_set_name (filter, C_("disc", "Image files"));
	gtk_file_filter_add_mime_type (filter, mimes [0]);
	gtk_file_filter_add_mime_type (filter, mimes [1]);
	gtk_file_filter_add_mime_type (filter, mimes [2]);
	gtk_file_filter_add_mime_type (filter, mimes [3]);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (priv->file), filter);
	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (priv->file), filter);

	/* add the type chooser to the dialog */
	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_show (box);
	gtk_box_pack_end (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (priv->file))),
			  box,
			  FALSE,
			  FALSE,
			  0);

	label = gtk_label_new (_("Image type:"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
	priv->format = brasero_image_type_chooser_new ();
	gtk_widget_show (priv->format);
	gtk_box_pack_start (GTK_BOX (box),
			    priv->format,
			    TRUE,
			    TRUE,
			    0);

	brasero_src_image_set_formats (BRASERO_SRC_IMAGE (button));

	gtk_widget_show (priv->file);
	response = gtk_dialog_run (GTK_DIALOG (priv->file));

	if (response == GTK_RESPONSE_OK)
		brasero_src_image_changed (BRASERO_SRC_IMAGE (button));

	gtk_widget_destroy (priv->file);
	priv->file = NULL;
	priv->format = NULL;
}

static void
brasero_src_image_set_parent_directory (GObject *object,
					GAsyncResult *result,
					gpointer data)
{
	BraseroSrcImagePrivate *priv;
	GFileInfo *info;

	priv = BRASERO_SRC_IMAGE_PRIVATE (data);

	info = g_file_query_info_finish (G_FILE (object), result, NULL);
	if (info) {
		if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
			g_free (priv->folder);
			priv->folder = g_file_get_path (G_FILE (object));
		}
		g_object_unref (info);
	}

	g_settings_set_string (priv->settings,
	                       BRASERO_PROPS_ISO_DIRECTORY,
	                       priv->folder? priv->folder:"");
	g_object_unref (data);
}

static void
brasero_src_image_init (BraseroSrcImage *object)
{
	BraseroSrcImagePrivate *priv;
	GtkWidget *image;
	GtkWidget *label;
	GtkWidget *box;
	gchar *string;
	gchar *uri;

	priv = BRASERO_SRC_IMAGE_PRIVATE (object);

	priv->cancel = g_cancellable_new ();

	/* Set the parent folder to be used in gtkfilechooser. This has to be 
	 * done now not to delay its creation when it's needed and we need to
	 * know if the location that was saved is still valid */
	priv->settings = g_settings_new (BRASERO_SCHEMA_DISPLAY);
	uri = g_settings_get_string (priv->settings, BRASERO_PROPS_ISO_DIRECTORY);

	if (uri && g_str_has_prefix (uri, G_DIR_SEPARATOR_S)) {
		GFile *file;

		/* Make sure it's still a valid parent folder */
		file = g_file_new_for_commandline_arg (uri);
		g_cancellable_reset (priv->cancel);
		g_file_query_info_async (file,
					 G_FILE_ATTRIBUTE_STANDARD_TYPE,
					 G_FILE_QUERY_INFO_NONE,
					 0,
					 priv->cancel,
					 brasero_src_image_set_parent_directory,
					 g_object_ref (object));
		g_object_unref (file);
	}
	g_free (uri);

	/* Translators: this is a disc image */
	string = g_strdup_printf ("<i>%s</i>", _("Click here to select a disc _image"));
	label = gtk_label_new_with_mnemonic (string);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_NONE);
	gtk_widget_show (label);
	g_free (string);

	priv->label = label;

	image = gtk_image_new_from_icon_name ("iso-image-new", GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (image);

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_show (box);
	gtk_box_pack_start (GTK_BOX (box), image, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

	gtk_container_add (GTK_CONTAINER (object), box);
}

static void
brasero_src_image_finalize (GObject *object)
{
	BraseroSrcImagePrivate *priv;

	priv = BRASERO_SRC_IMAGE_PRIVATE (object);

	brasero_src_image_save (BRASERO_SRC_IMAGE (object));

	if (priv->session) {
		g_object_unref (priv->session);
		priv->session = NULL;
	}

	if (priv->cancel) {
		g_cancellable_cancel (priv->cancel);
		g_object_unref (priv->cancel);
		priv->cancel = NULL;
	}

	if (priv->track) {
		g_object_unref (priv->track);
		priv->track = NULL;
	}

	if (priv->folder) {
		g_free (priv->folder);
		priv->folder = NULL;
	}

	if (priv->settings) {
		g_object_unref (priv->settings);
		priv->settings = NULL;
	}

	G_OBJECT_CLASS (brasero_src_image_parent_class)->finalize (object);
}

static BraseroTrack *
_get_session_image_track (BraseroBurnSession *session)
{
	BraseroTrack *track;
	GSList *tracks;
	guint num;

	tracks = brasero_burn_session_get_tracks (session);
	num = g_slist_length (tracks);

	if (num != 1)
		return NULL;

	track = tracks->data;
	if (BRASERO_IS_TRACK_IMAGE (track))
		return track;

	if (BRASERO_IS_TRACK_IMAGE_CFG (track))
		return track;

	return NULL;
}

static void
brasero_src_image_set_property (GObject *object,
				guint property_id,
				const GValue *value,
				GParamSpec *pspec)
{
	BraseroSrcImagePrivate *priv;
	BraseroBurnSession *session;

	priv = BRASERO_SRC_IMAGE_PRIVATE (object);

	switch (property_id) {
	case PROP_SESSION: {
		BraseroTrack *track;

		if (priv->session)
			g_object_unref (priv->session);

		session = g_value_get_object (value);

		/* NOTE: no need to unref a potential previous session since
		 * it's only set at construct time */
		priv->session = session;
		g_object_ref (session);

		track = _get_session_image_track (session);
		if (track) {
			if (!BRASERO_IS_TRACK_IMAGE_CFG (track)) {
				BraseroImageFormat format;
				goffset blocks = 0;
				gchar *image = NULL;
				gchar *toc = NULL;

				toc = brasero_track_image_get_toc_source (BRASERO_TRACK_IMAGE (track), TRUE);
				image = brasero_track_image_get_source (BRASERO_TRACK_IMAGE (track), TRUE);
				format = brasero_track_image_get_format (BRASERO_TRACK_IMAGE (track));
				brasero_track_get_size (BRASERO_TRACK (track),
							&blocks,
							NULL);

				priv->track = brasero_track_image_cfg_new ();
				if (blocks && format != BRASERO_IMAGE_FORMAT_NONE) {
					/* copy all the information */
					brasero_track_image_set_source (BRASERO_TRACK_IMAGE (priv->track),
									image,
									toc,
									format);

					brasero_track_image_set_block_num (BRASERO_TRACK_IMAGE (priv->track), blocks);
				}
				else {
					brasero_track_image_cfg_force_format (priv->track, format);

					switch (format) {
					case BRASERO_IMAGE_FORMAT_NONE:
					case BRASERO_IMAGE_FORMAT_BIN:
						brasero_track_image_cfg_set_source (priv->track, image);
						break;
					case BRASERO_IMAGE_FORMAT_CLONE:
					case BRASERO_IMAGE_FORMAT_CUE:
					case BRASERO_IMAGE_FORMAT_CDRDAO:
						brasero_track_image_cfg_set_source (priv->track, toc);
						break;

					default:
						break;
					}
				}

				brasero_burn_session_add_track (priv->session,
								BRASERO_TRACK (priv->track),
								NULL);
				g_free (image);
				g_free (toc);
			}
			else {
				g_object_ref (track);
				priv->track = BRASERO_TRACK_IMAGE_CFG (track);
			}
		}
		else {
			/* Add our own track */
			priv->track = brasero_track_image_cfg_new ();
			brasero_burn_session_add_track (priv->session,
							BRASERO_TRACK (priv->track),
							NULL);
		}

		g_signal_connect (priv->track,
				  "changed",
				  G_CALLBACK (brasero_image_src_track_changed_cb),
				  object);

		/* Make sure everything fits (NOTE: no need to set format yet,
		 * since at that point no GtkFileChooser was opened.) */
		brasero_src_image_update (BRASERO_SRC_IMAGE (object));

		break;
	}

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
brasero_src_image_get_property (GObject *object,
				guint property_id,
				GValue *value,
				GParamSpec *pspec)
{
	BraseroSrcImagePrivate *priv;

	priv = BRASERO_SRC_IMAGE_PRIVATE (object);

	switch (property_id) {
	case PROP_SESSION:
		g_value_set_object (value, priv->session);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
brasero_src_image_class_init (BraseroSrcImageClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	GtkButtonClass* parent_class = GTK_BUTTON_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroSrcImagePrivate));

	object_class->finalize = brasero_src_image_finalize;
	object_class->set_property = brasero_src_image_set_property;
	object_class->get_property = brasero_src_image_get_property;

	parent_class->clicked = brasero_src_image_clicked;
	g_object_class_install_property (object_class,
					 PROP_SESSION,
					 g_param_spec_object ("session",
							      "The session to work with",
							      "The session to work with",
							      BRASERO_TYPE_BURN_SESSION,
							      G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY));
}

GtkWidget *
brasero_src_image_new (BraseroBurnSession *session)
{
	return g_object_new (BRASERO_TYPE_SRC_IMAGE,
			     "session", session,
			     NULL);
}
