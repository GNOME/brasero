/***************************************************************************
 *            brasero-image-option-dialog.c
 *
 *  jeu sep 28 17:28:10 2006
 *  Copyright  2006  Philippe Rouquier
 *  bonfire-app@wanadoo.fr
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

#include <string.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib-object.h>

#include <gtk/gtkdialog.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkbutton.h>

#include "brasero-utils.h"
#include "burn-basics.h"
#include "burn-plugin-manager.h"
#include "brasero-image-option-dialog.h"
#include "brasero-image-type-chooser.h"
#include "brasero-dest-selection.h"
#include "burn-drive.h"
#include "brasero-io.h"
 
G_DEFINE_TYPE (BraseroImageOptionDialog, brasero_image_option_dialog, GTK_TYPE_DIALOG);

struct _BraseroImageOptionDialogPrivate {
	BraseroBurnSession *session;
	BraseroTrack *track;

	BraseroBurnCaps *caps;

	gulong caps_sig;
	gulong session_sig;

	BraseroIO *io;
	BraseroIOJobBase *info_type;

	GtkWidget *selection;
	GtkWidget *format;
	GtkWidget *file;
	GtkWidget *button;
};
typedef struct _BraseroImageOptionDialogPrivate BraseroImageOptionDialogPrivate;

#define BRASERO_IMAGE_OPTION_DIALOG_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_IMAGE_OPTION_DIALOG, BraseroImageOptionDialogPrivate))

static GtkDialogClass *parent_class = NULL;

static const gchar *mimes [] = { "application/x-cd-image",
				 "application/x-cue",
				 "application/x-toc",
				 "application/x-cdrdao-toc" };

static void
brasero_image_option_dialog_set_track (BraseroImageOptionDialog *dialog,
				       BraseroImageFormat format,
				       const gchar *image,
				       const gchar *toc)
{
	BraseroImageOptionDialogPrivate *priv;

	priv = BRASERO_IMAGE_OPTION_DIALOG_PRIVATE (dialog);

	if (!priv->track) {
		priv->track = brasero_track_new (BRASERO_TRACK_TYPE_IMAGE);
		brasero_burn_session_add_track (priv->session, priv->track);
	}

	brasero_track_set_image_source (priv->track,
					image,
					toc,
					format);
}

static void
brasero_image_option_dialog_image_info_cb (GObject *object,
					   GError *error,
					   const gchar *uri,
					   GFileInfo *info,
					   gpointer null_data)
{
	BraseroImageOptionDialog *dialog = BRASERO_IMAGE_OPTION_DIALOG (object);
	BraseroImageOptionDialogPrivate *priv;

	priv = BRASERO_IMAGE_OPTION_DIALOG_PRIVATE (dialog);

	if (error) {
		brasero_image_option_dialog_set_track (dialog,
						       BRASERO_IMAGE_FORMAT_NONE,
						       NULL,
						       NULL);
		return;
	}

    	/* Add it to recent file manager */
	if (!strcmp (g_file_info_get_content_type (info), "application/x-toc"))
		brasero_image_option_dialog_set_track (dialog,
						       BRASERO_IMAGE_FORMAT_CLONE,
						       NULL,
						       uri);
	else if (!strcmp (g_file_info_get_content_type (info), "application/octet-stream")) {
		/* that could be an image, so here is the deal:
		 * if we can find the type through the extension, fine.
		 * if not default to CLONE */
		if (g_str_has_suffix (uri, ".bin"))
			brasero_image_option_dialog_set_track (dialog,
							       BRASERO_IMAGE_FORMAT_CDRDAO,
							       uri,
							       NULL);
		else if (g_str_has_suffix (uri, ".raw"))
			brasero_image_option_dialog_set_track (dialog,
							       BRASERO_IMAGE_FORMAT_CLONE,
							       uri,
							       NULL);
		else
			brasero_image_option_dialog_set_track (dialog,
							       BRASERO_IMAGE_FORMAT_BIN,
							       uri,
							       NULL);
	}
	else if (!strcmp (g_file_info_get_content_type (info), "application/x-cd-image"))
		brasero_image_option_dialog_set_track (dialog,
						       BRASERO_IMAGE_FORMAT_BIN,
						       uri,
						       NULL);
	else if (!strcmp (g_file_info_get_content_type (info), "application/x-cdrdao-toc"))
		brasero_image_option_dialog_set_track (dialog,
						       BRASERO_IMAGE_FORMAT_CDRDAO,
						       NULL,
						       uri);
	else if (!strcmp (g_file_info_get_content_type (info), "application/x-cue"))
		brasero_image_option_dialog_set_track (dialog,
						       BRASERO_IMAGE_FORMAT_CUE,
						       NULL,
						       uri);
	else
		brasero_image_option_dialog_set_track (dialog,
						       BRASERO_IMAGE_FORMAT_NONE,
						       NULL,
						       uri);
}

static void
brasero_image_option_dialog_get_format (BraseroImageOptionDialog *dialog,
					gchar *uri)
{
	BraseroImageOptionDialogPrivate *priv;

	priv = BRASERO_IMAGE_OPTION_DIALOG_PRIVATE (dialog);

	if (!uri) {
		brasero_image_option_dialog_set_track (dialog,
						       BRASERO_IMAGE_FORMAT_NONE,
						       NULL,
						       NULL);
		return;
	}

	if (!priv->io)
		priv->io = brasero_io_get_default ();

	if (!priv->info_type)
		priv->info_type = brasero_io_register (G_OBJECT (dialog),
						       brasero_image_option_dialog_image_info_cb,
						       NULL,
						       NULL);

	brasero_io_get_file_info (priv->io,
				  uri,
				  priv->info_type,
				  BRASERO_IO_INFO_MIME,
				  NULL);
}

static void
brasero_image_option_dialog_changed (BraseroImageOptionDialog *dialog)
{
	gchar *uri;
	BraseroImageFormat format;
	BraseroImageOptionDialogPrivate *priv;

	priv = BRASERO_IMAGE_OPTION_DIALOG_PRIVATE (dialog);

	uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (priv->file));
	brasero_image_type_chooser_get_format (BRASERO_IMAGE_TYPE_CHOOSER (priv->format),
					       &format);

	switch (format) {
	case BRASERO_IMAGE_FORMAT_NONE:
		brasero_image_option_dialog_get_format (dialog, uri);
		break;
	case BRASERO_IMAGE_FORMAT_BIN:
		brasero_image_option_dialog_set_track (dialog,
						       format,
						       uri,
						       NULL);
		break;
	case BRASERO_IMAGE_FORMAT_CUE:
		brasero_image_option_dialog_set_track (dialog,
						       format,
						       NULL,
						       uri);
		break;
	case BRASERO_IMAGE_FORMAT_CDRDAO:
		brasero_image_option_dialog_set_track (dialog,
						       format,
						       NULL,
						       uri);
		break;
	case BRASERO_IMAGE_FORMAT_CLONE:
		brasero_image_option_dialog_set_track (dialog,
						       format,
						       NULL,
						       uri);
		break;
	default:
		break;
	}
	g_free (uri);	
}

static void
brasero_image_option_dialog_set_formats (BraseroImageOptionDialog *dialog)
{
	BraseroImageOptionDialogPrivate *priv;
	BraseroImageFormat formats;
	BraseroImageFormat format;
	BraseroTrackType output;
	BraseroTrackType input;
	BraseroMedium *medium;
	BraseroDrive *drive;

	priv = BRASERO_IMAGE_OPTION_DIALOG_PRIVATE (dialog);

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
							       &input);
		if (result == BRASERO_BURN_OK)
			formats |= format;
	}
	brasero_image_type_chooser_set_formats (BRASERO_IMAGE_TYPE_CHOOSER (priv->format),
					        formats);
}

static void
brasero_image_option_dialog_format_changed (BraseroImageTypeChooser *format,
					    BraseroImageOptionDialog *dialog)
{
	brasero_image_option_dialog_changed (dialog);
}

static void
brasero_image_option_dialog_file_changed (GtkFileChooser *chooser,
					  BraseroImageOptionDialog *dialog)
{
	brasero_image_option_dialog_changed (dialog);
}

static void
brasero_image_option_dialog_output_changed_cb (BraseroBurnSession *session,
					       BraseroImageOptionDialog *dialog)
{
	brasero_image_option_dialog_set_formats (dialog);
}

static void
brasero_image_option_dialog_caps_changed (BraseroPluginManager *manager,
					  BraseroImageOptionDialog *dialog)
{
	brasero_image_option_dialog_set_formats (dialog);
}

void
brasero_image_option_dialog_set_image_uri (BraseroImageOptionDialog *dialog,
					   const gchar *uri)
{
	BraseroImageOptionDialogPrivate *priv;

	priv = BRASERO_IMAGE_OPTION_DIALOG_PRIVATE (dialog);

	brasero_image_option_dialog_set_formats (dialog);

	if (uri) {
		gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (priv->file), uri);
		brasero_image_option_dialog_changed (dialog);
	}
	else
		brasero_image_option_dialog_set_track (dialog,
						       BRASERO_IMAGE_FORMAT_NONE,
						       NULL,
						       NULL);
}

static void
brasero_image_option_dialog_image_info_error (BraseroImageOptionDialog *dialog)
{
	GtkWidget *message;
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (dialog));
	message = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					  GTK_DIALOG_DESTROY_WITH_PARENT |
					  GTK_DIALOG_MODAL,
					  GTK_MESSAGE_ERROR,
					  GTK_BUTTONS_CLOSE,
					  _("This image can't be burnt:"));

	gtk_window_set_title (GTK_WINDOW (message), _("Invalid image"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
						  _("it doesn't appear to be a valid image or a valid cue file."));

	gtk_dialog_run (GTK_DIALOG (message));
	gtk_widget_destroy (message);
}

static void
brasero_image_option_dialog_image_empty (BraseroImageOptionDialog *dialog)
{
	GtkWidget *message;
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (dialog));
	message = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					  GTK_DIALOG_DESTROY_WITH_PARENT |
					  GTK_DIALOG_MODAL,
					  GTK_MESSAGE_ERROR,
					  GTK_BUTTONS_CLOSE,
					  _("There is no specified image:"));

	gtk_window_set_title (GTK_WINDOW (message), _("No image"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
						  _("Please, choose an image and retry."));

	gtk_dialog_run (GTK_DIALOG (message));
	gtk_widget_destroy (message);
}

BraseroBurnSession *
brasero_image_option_dialog_get_session (BraseroImageOptionDialog *dialog)
{
	gchar *uri = NULL;
	gchar *groups [] = { "brasero",
			      NULL };
	GtkRecentData recent_data = { NULL,
				      NULL,
				      NULL,
				      "brasero",
				      "brasero -p %u",
				      groups,
				      FALSE };
	BraseroImageOptionDialogPrivate *priv;
	BraseroTrackType type;
	gchar *image;

	priv = BRASERO_IMAGE_OPTION_DIALOG_PRIVATE (dialog);

	/* check that all could be set for the session */
	image = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (priv->file));
	if (!image) {
		brasero_image_option_dialog_image_empty (dialog);
		return NULL;
	}
	g_free (image);

	brasero_track_get_type (priv->track, &type);
	if (type.type == BRASERO_TRACK_TYPE_NONE
	||  type.subtype.img_format == BRASERO_IMAGE_FORMAT_NONE) {
		brasero_image_option_dialog_image_info_error (dialog);
		return NULL;
	}

	/* Add it to recent file manager */
	switch (type.subtype.img_format) {
	case BRASERO_IMAGE_FORMAT_BIN:
		recent_data.mime_type = (gchar *) mimes [0];
		uri = brasero_track_get_image_source (priv->track, TRUE);
		break;

	case BRASERO_IMAGE_FORMAT_CUE:
		recent_data.mime_type = (gchar *) mimes [1];
		uri = brasero_track_get_toc_source (priv->track, TRUE);
		break;

	case BRASERO_IMAGE_FORMAT_CLONE:
		recent_data.mime_type = (gchar *) mimes [2];
		uri = brasero_track_get_toc_source (priv->track, TRUE);
		break;

	case BRASERO_IMAGE_FORMAT_CDRDAO:
		recent_data.mime_type = (gchar *) mimes [3];
		uri = brasero_track_get_toc_source (priv->track, TRUE);
		break;

	default:
		break;
	}

	if (uri) {
		GtkRecentManager *recent;

		recent = gtk_recent_manager_get_default ();
		gtk_recent_manager_add_full (recent,
					     uri,
					     &recent_data);
		g_free (uri);
	}

	g_object_ref (priv->session);
	return priv->session;
}

static void
brasero_image_option_dialog_valid_media_cb (BraseroDestSelection *selection,
					    gboolean valid,
					    BraseroImageOptionDialog *self)
{
	BraseroImageOptionDialogPrivate *priv;

	priv = BRASERO_IMAGE_OPTION_DIALOG_PRIVATE (self);
	gtk_widget_set_sensitive (priv->button, valid);
}

static void
brasero_image_option_dialog_init (BraseroImageOptionDialog *obj)
{
	GtkWidget *label;
	GtkWidget *button;
	GtkWidget *options;
	GtkWidget *box, *box1;
	GtkFileFilter *filter;
	BraseroPluginManager *manager;
	BraseroImageOptionDialogPrivate *priv;

	priv = BRASERO_IMAGE_OPTION_DIALOG_PRIVATE (obj);

	gtk_dialog_set_has_separator (GTK_DIALOG (obj), FALSE);

	button = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
	gtk_widget_show (button);
	gtk_dialog_add_action_widget (GTK_DIALOG (obj),
				      button,
				      GTK_RESPONSE_CANCEL);

	priv->button = brasero_utils_make_button (_("_Burn"),
						  NULL,
						  "media-optical-burn",
						  GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (priv->button);
	gtk_dialog_add_action_widget (GTK_DIALOG (obj),
				      priv->button,
				      GTK_RESPONSE_OK);

	priv->caps = brasero_burn_caps_get_default ();
	manager = brasero_plugin_manager_get_default ();
	priv->caps_sig = g_signal_connect (manager,
					   "caps-changed",
					   G_CALLBACK (brasero_image_option_dialog_caps_changed),
					   obj);

	priv->session = brasero_burn_session_new ();
	brasero_burn_session_add_flag (priv->session,
				       BRASERO_BURN_FLAG_EJECT|
				       BRASERO_BURN_FLAG_NOGRACE|
				       BRASERO_BURN_FLAG_BURNPROOF|
				       BRASERO_BURN_FLAG_CHECK_SIZE|
				       BRASERO_BURN_FLAG_DONT_CLEAN_OUTPUT|
				       BRASERO_BURN_FLAG_FAST_BLANK);
	priv->session_sig = g_signal_connect (priv->session,
					      "output-changed",
					      G_CALLBACK (brasero_image_option_dialog_output_changed_cb),
					      obj);

	/* first box */
	priv->selection = brasero_dest_selection_new (priv->session);
	g_signal_connect (priv->selection,
			  "valid-media",
			  G_CALLBACK (brasero_image_option_dialog_valid_media_cb),
			  obj);

	options = brasero_utils_pack_properties (_("<b>Select a disc to write to</b>"),
						 priv->selection,
						 NULL);

	gtk_widget_show (options);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (obj)->vbox),
			    options,
			    FALSE,
			    FALSE,
			    6);

	brasero_drive_selection_set_type_shown (BRASERO_DRIVE_SELECTION (priv->selection),
						BRASERO_MEDIA_TYPE_WRITABLE);

	/* Image properties */
	box1 = gtk_table_new (2, 2, FALSE);
	gtk_table_set_col_spacings (GTK_TABLE (box1), 6);
	gtk_widget_show (box1);

	label = gtk_label_new (_("Path:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (box1),
			  label,
			  0,
			  1,
			  0,
			  1,
			  GTK_FILL,
			  GTK_FILL,
			  0,
			  0);

	priv->file = gtk_file_chooser_button_new (_("Open an image"), GTK_FILE_CHOOSER_ACTION_OPEN);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("All files"));
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (priv->file), filter);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("Image files only"));
	gtk_file_filter_add_mime_type (filter, mimes [0]);
	gtk_file_filter_add_mime_type (filter, mimes [1]);
	gtk_file_filter_add_mime_type (filter, mimes [2]);
	gtk_file_filter_add_mime_type (filter, mimes [3]);
	gtk_file_filter_add_mime_type (filter, "image/*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (priv->file), filter);
	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (priv->file), filter);

	gtk_table_attach (GTK_TABLE (box1),
			  priv->file,
			  1,
			  2,
			  0,
			  1,
			  GTK_EXPAND|GTK_FILL,
			  GTK_EXPAND|GTK_FILL,
			  0,
			  0);
	g_signal_connect (priv->file,
			  "selection-changed",
			  G_CALLBACK (brasero_image_option_dialog_file_changed),
			  obj);

	gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (priv->file), g_get_home_dir ());
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (priv->file), FALSE);

	label = gtk_label_new (_("Image type:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (box1),
			  label,
			  0,
			  1,
			  1,
			  2,
			  GTK_FILL,
			  GTK_FILL,
			  0,
			  0);

	priv->format = brasero_image_type_chooser_new ();
	gtk_table_attach (GTK_TABLE (box1),
			  priv->format,
			  1,
			  2,
			  1,
			  2,
			  GTK_EXPAND|GTK_FILL,
			  GTK_EXPAND|GTK_FILL,
			  0,
			  0);
	g_signal_connect (priv->format,
			  "changed",
			  G_CALLBACK (brasero_image_option_dialog_format_changed),
			  obj);

	box = brasero_utils_pack_properties (_("<b>Image</b>"),
					     box1,
					     NULL);

	gtk_box_pack_end (GTK_BOX (GTK_DIALOG (obj)->vbox),
			  box,
			  TRUE,
			  FALSE,
			  6);

	gtk_widget_show_all (box);
	brasero_image_option_dialog_set_formats (obj);
}

static void
brasero_image_option_dialog_finalize (GObject *object)
{
	BraseroImageOptionDialogPrivate *priv;

	priv = BRASERO_IMAGE_OPTION_DIALOG_PRIVATE (object);

	if (priv->io) {
		brasero_io_cancel_by_base (priv->io, priv->info_type);

		g_free (priv->info_type);
		priv->info_type = NULL;

		g_object_unref (priv->io);
		priv->io = NULL;
	}

	if (priv->track) {
		brasero_track_unref (priv->track);
		priv->track = NULL;
	}

	if (priv->caps_sig) {
		BraseroPluginManager *manager;

		manager = brasero_plugin_manager_get_default ();
		g_signal_handler_disconnect (manager, priv->caps_sig);
		priv->caps_sig = 0;
	}

	if (priv->session_sig) {
		g_signal_handler_disconnect (priv->session, priv->session_sig);
		priv->session_sig = 0;
	}

	if (priv->session) {
		g_object_unref (priv->session);
		priv->session = NULL;
	}

	if (priv->caps) {
		g_object_unref (priv->caps);
		priv->caps = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_image_option_dialog_class_init (BraseroImageOptionDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroImageOptionDialogPrivate));

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_image_option_dialog_finalize;
}

GtkWidget *
brasero_image_option_dialog_new ()
{
	BraseroImageOptionDialog *obj;
	
	obj = BRASERO_IMAGE_OPTION_DIALOG (g_object_new (BRASERO_TYPE_IMAGE_OPTION_DIALOG,
							"title", _("Image burning setup"),
							NULL));
	
	return GTK_WIDGET (obj);
}
