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

#include <gconf/gconf-client.h>

#include "burn-basics.h"
#include "burn-caps.h"

#include "brasero-drive.h"

#include "brasero-misc.h"

#include "brasero-image-type-chooser.h"
#include "brasero-session-cfg.h"
#include "brasero-track-image.h"
#include "brasero-src-image.h"
#include "burn-image-format.h"

typedef struct _BraseroSrcImagePrivate BraseroSrcImagePrivate;
struct _BraseroSrcImagePrivate
{
	BraseroBurnSession *session;
	BraseroTrackImage *track;

	BraseroBurnCaps *caps;

	gchar *folder;
	GCancellable *cancel;

	GtkWidget *format;
	GtkWidget *label;
	GtkWidget *file;
};

#define BRASERO_SRC_IMAGE_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_SRC_IMAGE, BraseroSrcImagePrivate))

G_DEFINE_TYPE (BraseroSrcImage, brasero_src_image, GTK_TYPE_BUTTON);

enum {
	PROP_0,
	PROP_SESSION
};

#define BRASERO_KEY_ISO_DIRECTORY		"/apps/brasero/display/iso_folder"

static const gchar *mimes [] = { "application/x-cd-image",
				 "application/x-cue",
				 "application/x-toc",
				 "application/x-cdrdao-toc" };

static void
brasero_src_image_save (BraseroSrcImage *self)
{
	gchar *uri = NULL;
	BraseroTrackType type;
	GtkRecentManager *recent;
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

	brasero_track_get_track_type (BRASERO_TRACK (priv->track), &type);
	if (type.type == BRASERO_TRACK_TYPE_NONE
	||  type.subtype.img_format == BRASERO_IMAGE_FORMAT_NONE)
		return;

	/* Add it to recent file manager */
	switch (type.subtype.img_format) {
	case BRASERO_IMAGE_FORMAT_BIN:
		recent_data.mime_type = (gchar *) mimes [0];
		uri = brasero_track_image_get_source (priv->track, TRUE);
		break;

	case BRASERO_IMAGE_FORMAT_CUE:
		recent_data.mime_type = (gchar *) mimes [1];
		uri = brasero_track_image_get_toc_source (priv->track, TRUE);
		break;

	case BRASERO_IMAGE_FORMAT_CLONE:
		recent_data.mime_type = (gchar *) mimes [2];
		uri = brasero_track_image_get_toc_source (priv->track, TRUE);
		break;

	case BRASERO_IMAGE_FORMAT_CDRDAO:
		recent_data.mime_type = (gchar *) mimes [3];
		uri = brasero_track_image_get_toc_source (priv->track, TRUE);
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
brasero_src_image_update (BraseroSrcImage *dialog)
{
	gchar *uri;
	gchar *path;
	GFile *file;
	gchar *string;
	guint64 size = 0;
	gchar *size_string;
	BraseroSrcImagePrivate *priv;
	BraseroTrackType type = { 0, };

	priv = BRASERO_SRC_IMAGE_PRIVATE (dialog);

	if (!priv->track)
		return;

	/* Deal with size */
	brasero_track_get_size (BRASERO_TRACK (priv->track), NULL, &size);
	size_string = g_format_size_for_display (size);

	/* Retrieve a path or an uri */
	path = NULL;
	brasero_track_get_track_type (BRASERO_TRACK (priv->track), &type);
	switch (type.subtype.img_format) {
	case BRASERO_IMAGE_FORMAT_NONE:
	case BRASERO_IMAGE_FORMAT_BIN:
		uri = brasero_track_image_get_source (priv->track, TRUE);
		break;

	case BRASERO_IMAGE_FORMAT_CUE:
	case BRASERO_IMAGE_FORMAT_CDRDAO:
	case BRASERO_IMAGE_FORMAT_CLONE:
		uri = brasero_track_image_get_source (priv->track, TRUE);
		break;

	default:
		path = NULL;
		break;
	}

	file = g_file_new_for_uri (uri);
	g_free (uri);

	if (g_file_is_native (file)) {
		path = g_file_get_path (file);
		if (!path)
			path = g_file_get_uri (file);
	}
	else
		path = g_file_get_uri (file);

	g_object_unref (file);

	if (!path) {
		g_free (size_string);
		return;
	}

	/* NOTE to translators, the first %s is the path of the image
	 * file and the second its size. */
	string = g_strdup_printf (_("\"%s\": %s"), path, size_string);
	g_free (size_string);
	g_free (path);

	if (string) {
		/* This is hackish and meant to avoid ellipsization to make the
		 * label to small. */
		if (strlen (string) > strlen (_("Click here to select an _image")) + 5)
			gtk_label_set_ellipsize (GTK_LABEL (priv->label), PANGO_ELLIPSIZE_START);
		else
			gtk_label_set_ellipsize (GTK_LABEL (priv->label), PANGO_ELLIPSIZE_NONE);

		gtk_label_set_text (GTK_LABEL (priv->label), string);
		g_free (string);
	}
}

static void
brasero_src_image_set_track (BraseroSrcImage *dialog,
			     BraseroImageFormat format,
			     const gchar *image,
			     const gchar *toc)
{
	BraseroSrcImagePrivate *priv;

	priv = BRASERO_SRC_IMAGE_PRIVATE (dialog);

	/* set image type before adding so that signal has the right type */
	brasero_track_image_set_source (priv->track,
					image,
					toc,
					format);

	if (!toc && !image && format == BRASERO_IMAGE_FORMAT_NONE)
		return;

	brasero_src_image_update (dialog);
}

static void
brasero_src_image_error (BraseroSrcImage *self,
			 GError *error)
{
	brasero_utils_message_dialog (gtk_widget_get_toplevel (GTK_WIDGET (self)),
				      _("Please select another image."),
				      error->message,
				      GTK_MESSAGE_ERROR);
}

static void
brasero_src_image_check_parent_directory_cb (GObject *object,
					     GAsyncResult *result,
					     gpointer data)
{
	BraseroSrcImagePrivate *priv;
	GConfClient *client;
	GFileInfo *info;

	priv = BRASERO_SRC_IMAGE_PRIVATE (data);

	info = g_file_query_info_finish (G_FILE (object), result, NULL);
	if (!info)
		return;

	if (g_file_info_get_file_type (info) != G_FILE_TYPE_DIRECTORY)
		return;

	g_free (priv->folder);
	priv->folder = g_file_get_uri (G_FILE (object));

	client = gconf_client_get_default ();
	gconf_client_set_string (client,
				 BRASERO_KEY_ISO_DIRECTORY,
				 priv->folder? priv->folder:"",
				 NULL);
	g_object_unref (client);

}

static void
brasero_src_image_get_info_cb (GObject *object,
			       GAsyncResult *result,
			       gpointer data)
{
	BraseroSrcImage *dialog = BRASERO_SRC_IMAGE (data);
	BraseroSrcImagePrivate *priv;
	GError *error = NULL;
	const gchar *mime;
	GFileInfo *info;
	gchar *uri;

	info = g_file_query_info_finish (G_FILE (object), result, &error);
	uri = g_file_get_uri (G_FILE (object));

	priv = BRASERO_SRC_IMAGE_PRIVATE (dialog);
	if (error) {
		brasero_src_image_set_track (dialog,
					     BRASERO_IMAGE_FORMAT_NONE,
					     NULL,
					     NULL);

		/* we need to say that image can't be loaded */
		brasero_src_image_error (dialog, error);
		g_error_free (error);
		return;
	}

	mime = g_file_info_get_content_type (info);
	if (mime
	&& (!strcmp (mime, "application/x-toc")
	||  !strcmp (mime, "application/x-cdrdao-toc")
	||  !strcmp (mime, "application/x-cue"))) {
		BraseroImageFormat format;
		gchar *path;

		path = g_filename_from_uri (uri, NULL, NULL);
		format = brasero_image_format_identify_cuesheet (path);
		g_free (path);

		if (format != BRASERO_IMAGE_FORMAT_NONE)
			brasero_src_image_set_track (dialog,
						     format,
						     NULL,
						     uri);
		else if (g_str_has_suffix (path, ".toc"))
			brasero_src_image_set_track (dialog,
						     BRASERO_IMAGE_FORMAT_CLONE,
						     NULL,
						     uri);
		else
			brasero_src_image_set_track (dialog,
						     BRASERO_IMAGE_FORMAT_NONE,
						     NULL,
						     uri);
	}
	else if (mime && !strcmp (mime, "application/octet-stream")) {
		/* that could be an image, so here is the deal:
		 * if we can find the type through the extension, fine.
		 * if not default to CLONE */
		if (g_str_has_suffix (uri, ".bin"))
			brasero_src_image_set_track (dialog,
						     BRASERO_IMAGE_FORMAT_CDRDAO,
						     uri,
						     NULL);
		else if (g_str_has_suffix (uri, ".raw"))
			brasero_src_image_set_track (dialog,
						     BRASERO_IMAGE_FORMAT_CLONE,
						     uri,
						     NULL);
		else
			brasero_src_image_set_track (dialog,
						     BRASERO_IMAGE_FORMAT_BIN,
						     uri,
						     NULL);
	}
	else if (mime && !strcmp (mime, "application/x-cd-image"))
		brasero_src_image_set_track (dialog,
					     BRASERO_IMAGE_FORMAT_BIN,
					     uri,
					     NULL);
	else
		brasero_src_image_set_track (dialog,
					     BRASERO_IMAGE_FORMAT_NONE,
					     uri,
					     NULL);

	g_object_unref (info);
}

static void
brasero_src_image_get_format (BraseroSrcImage *dialog,
			      const gchar *uri)
{
	BraseroSrcImagePrivate *priv;
	GFile *file;

	priv = BRASERO_SRC_IMAGE_PRIVATE (dialog);

	if (!uri) {
		brasero_src_image_set_track (dialog,
					     BRASERO_IMAGE_FORMAT_NONE,
					     NULL,
					     NULL);
		return;
	}

	if (priv->format) {
		BraseroImageFormat format;

		/* NOTE: this is only used when a GtkFileChooser has been
		 * spawned */
		brasero_image_type_chooser_get_format (BRASERO_IMAGE_TYPE_CHOOSER (priv->format), &format);
		switch (format) {
		/* Respect the user's choice regarding format */
		case BRASERO_IMAGE_FORMAT_BIN:
			brasero_src_image_set_track (dialog,
						     format,
						     uri,
						     NULL);
			return;
		case BRASERO_IMAGE_FORMAT_CUE:
			brasero_src_image_set_track (dialog,
						     format,
						     NULL,
						     uri);
			return;
		case BRASERO_IMAGE_FORMAT_CDRDAO:
			brasero_src_image_set_track (dialog,
						     format,
						     NULL,
						     uri);
			return;
		case BRASERO_IMAGE_FORMAT_CLONE:
			brasero_src_image_set_track (dialog,
						     format,
						     NULL,
						     uri);
			return;

		/* handle those cases afterwards */
		default:
			break;
		}
	}

	file = g_file_new_for_uri (uri);
	g_file_query_info_async (file,
				 G_FILE_ATTRIBUTE_STANDARD_TYPE ","
				 G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
				 G_FILE_QUERY_INFO_NONE,
				 0,
				 priv->cancel,
				 brasero_src_image_get_info_cb,
				 dialog);
	g_object_unref (file);
}

static void
brasero_src_image_changed (BraseroSrcImage *dialog)
{
	gchar *uri;
	GFile *file;
	GFile *parent;
	BraseroSrcImagePrivate *priv;

	priv = BRASERO_SRC_IMAGE_PRIVATE (dialog);

	/* Cancel any pending operation */
	g_cancellable_cancel (priv->cancel);
	g_cancellable_reset (priv->cancel);

	uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (priv->file));
	brasero_src_image_get_format (dialog, uri);

	/* Make sure it's still a valid folder */
	file = g_file_new_for_uri (uri);
	parent = g_file_get_parent (file);
	g_object_unref (file);

	g_file_query_info_async (parent,
				 G_FILE_ATTRIBUTE_STANDARD_TYPE,
				 G_FILE_QUERY_INFO_NONE,
				 0,
				 priv->cancel,
				 brasero_src_image_check_parent_directory_cb,
				 dialog);
	g_object_unref (parent);
}

static void
brasero_src_image_set_formats (BraseroSrcImage *dialog)
{
	BraseroSrcImagePrivate *priv;
	BraseroImageFormat formats;
	BraseroImageFormat format;
	BraseroTrackType output;
	BraseroTrackType input;
	BraseroMedium *medium;
	BraseroDrive *drive;

	priv = BRASERO_SRC_IMAGE_PRIVATE (dialog);

	if (!priv->format)
		return;

	/* get the available image types */
	output.type = BRASERO_TRACK_TYPE_DISC;
	drive = brasero_burn_session_get_burner (priv->session);
	medium = brasero_drive_get_medium (drive);
	output.subtype.media = brasero_medium_get_status (medium);

	input.type = BRASERO_TRACK_TYPE_IMAGE;
	formats = BRASERO_IMAGE_FORMAT_NONE;
	format = BRASERO_IMAGE_FORMAT_CDRDAO;

	for (; format != BRASERO_IMAGE_FORMAT_NONE; format >>= 1) {
		BraseroBurnResult result;

		input.subtype.img_format = format;
		result = brasero_burn_caps_is_input_supported (priv->caps,
							       priv->session,
							       &input,
							       FALSE);
		if (result == BRASERO_BURN_OK)
			formats |= format;
	}

	brasero_image_type_chooser_set_formats (BRASERO_IMAGE_TYPE_CHOOSER (priv->format),
					        formats);
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

	priv = BRASERO_SRC_IMAGE_PRIVATE (button);

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (button));
	priv->file = gtk_file_chooser_dialog_new (_("Select Image File"),
						  GTK_WINDOW (toplevel),
						  GTK_FILE_CHOOSER_ACTION_OPEN,
						  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						  GTK_STOCK_OPEN, GTK_RESPONSE_OK,
						  NULL);

	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (priv->file), FALSE);

	if (priv->folder) {
		if (!gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (priv->file), priv->folder))
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
	gtk_file_filter_set_name (filter, C_("disc", "Image files only"));
	gtk_file_filter_add_mime_type (filter, mimes [0]);
	gtk_file_filter_add_mime_type (filter, mimes [1]);
	gtk_file_filter_add_mime_type (filter, mimes [2]);
	gtk_file_filter_add_mime_type (filter, mimes [3]);
	gtk_file_filter_add_mime_type (filter, "image/*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (priv->file), filter);
	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (priv->file), filter);

	/* add the type chooser to the dialog */
	box = gtk_hbox_new (FALSE, 6);
	gtk_widget_show (box);
	gtk_box_pack_end (GTK_BOX (GTK_DIALOG (priv->file)->vbox),
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
	GConfClient *client;
	GFileInfo *info;

	priv = BRASERO_SRC_IMAGE_PRIVATE (data);

	info = g_file_query_info_finish (G_FILE (object), result, NULL);
	if (!info)
		goto update_gconf;

	if (g_file_info_get_file_type (info) != G_FILE_TYPE_DIRECTORY)
		goto update_gconf;

	g_free (priv->folder);
	priv->folder = g_file_get_uri (G_FILE (object));

update_gconf:

	client = gconf_client_get_default ();
	gconf_client_set_string (client,
				 BRASERO_KEY_ISO_DIRECTORY,
				 priv->folder? priv->folder:"",
				 NULL);
	g_object_unref (client);

}

static void
brasero_src_image_init (BraseroSrcImage *object)
{
	BraseroSrcImagePrivate *priv;
	GConfClient *client;
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
	client = gconf_client_get_default ();
	uri = gconf_client_get_string (client, BRASERO_KEY_ISO_DIRECTORY, NULL);
	g_object_unref (client);

	if (uri && uri [0] != '\0') {
		GFile *file;

		/* Make sure it's still a valid folder */
		file = g_file_new_for_commandline_arg (uri);
		g_file_query_info_async (file,
					 G_FILE_ATTRIBUTE_STANDARD_TYPE,
					 G_FILE_QUERY_INFO_NONE,
					 0,
					 priv->cancel,
					 brasero_src_image_set_parent_directory,
					 object);
		g_object_unref (file);
	}
	g_free (uri);
		 
	priv->caps = brasero_burn_caps_get_default ();

	string = g_strdup_printf ("<i>%s</i>", _("Click here to select an _image"));
	label = gtk_label_new_with_mnemonic (string);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_NONE);
	gtk_widget_show (label);
	g_free (string);

	priv->label = label;

	image = gtk_image_new_from_icon_name ("iso-image-new", GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (image);

	box = gtk_hbox_new (FALSE, 6);
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

	if (priv->caps) {
		g_object_unref (priv->caps);
		priv->caps = NULL;
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
		gchar *image;
		gchar *toc;

		if (priv->session)
			g_object_unref (priv->session);

		session = g_value_get_object (value);

		/* NOTE: no need to unref a potential previous session since
		 * it's only set at construct time */
		priv->session = session;
		g_object_ref (session);

		track = _get_session_image_track (session);
		if (track) {
			g_object_ref (track);
			priv->track = BRASERO_TRACK_IMAGE (track);
		}
		else {
			/* Add our own track */
			priv->track = brasero_track_image_new ();
			brasero_burn_session_add_track (priv->session, BRASERO_TRACK (priv->track));
		}

		/* Make sure everything fits (NOTE: no need to set format yet,
		 * since at that point no GtkFileChooser was opened.) */
		toc = brasero_track_image_get_toc_source (priv->track, TRUE);
		image = brasero_track_image_get_source (priv->track, TRUE);
		if (toc || image) {
			BraseroTrackType type = { 0, };

			brasero_track_get_track_type (BRASERO_TRACK (priv->track), &type);
			if (type.subtype.img_format != BRASERO_IMAGE_FORMAT_NONE)
				brasero_src_image_update (BRASERO_SRC_IMAGE (object));
			else
				brasero_src_image_get_format (BRASERO_SRC_IMAGE (object), toc? toc:image);

			g_free (image);
			g_free (toc);
		}

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
