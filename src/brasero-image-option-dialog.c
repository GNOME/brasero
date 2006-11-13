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

#include "utils.h"
#include "burn-basics.h"
#include "burn-common.h"
#include "brasero-image-option-dialog.h"
#include "recorder-selection.h"
#include "brasero-ncb.h"
#include "brasero-vfs.h"
#include "brasero-image-type-chooser.h"
 
static void brasero_image_option_dialog_class_init (BraseroImageOptionDialogClass *klass);
static void brasero_image_option_dialog_init (BraseroImageOptionDialog *sp);
static void brasero_image_option_dialog_finalize (GObject *object);

struct _BraseroImageOptionDialogPrivate {
	BraseroTrackSource *track;

	BraseroVFS *vfs;
	BraseroVFSDataID info_type;

	GtkWidget *format_chooser;
	GtkWidget *selection;
	GtkWidget *chooser;

	GtkTooltips *tooltips;
};

static GtkDialogClass *parent_class = NULL;

GType
brasero_image_option_dialog_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroImageOptionDialogClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_image_option_dialog_class_init,
			NULL,
			NULL,
			sizeof (BraseroImageOptionDialog),
			0,
			(GInstanceInitFunc)brasero_image_option_dialog_init,
		};

		type = g_type_register_static (GTK_TYPE_DIALOG, 
					       "BraseroImageOptionDialog",
					       &our_info,
					       0);
	}

	return type;
}

static void
brasero_image_option_dialog_class_init (BraseroImageOptionDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

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
brasero_image_option_dialog_make_track (BraseroImageOptionDialog *dialog,
					BraseroImageFormat format,
					gboolean is_image,
					const gchar *uri)
{
	BraseroTrackSource *track = NULL;
    	GtkRecentManager *recent;
	gchar *complement = NULL;

    	/* Add it to recent file manager */
    	recent = gtk_recent_manager_get_default ();
    	gtk_recent_manager_add_item (recent, uri);

	track = g_new0 (BraseroTrackSource, 1);
	track->type = BRASERO_TRACK_SOURCE_IMAGE;
	track->format = format;

	complement = brasero_get_file_complement (format,
						  is_image,
						  uri);
	if (is_image) {
		track->contents.image.toc = complement;
		track->contents.image.image = g_strdup (uri);
	}
	else {
		track->contents.image.toc = g_strdup (uri);
		track->contents.image.image = complement;
	}

	if (dialog->priv->track)
		brasero_track_source_free (dialog->priv->track);

	dialog->priv->track = track;
	gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
}

static void
brasero_image_option_dialog_image_info_cb (BraseroVFS *vfs,
					   GObject *object,
					   GnomeVFSResult result,
					   const gchar *uri,
					   GnomeVFSFileInfo *info,
					   gpointer null_data)
{
	BraseroImageOptionDialog *dialog = BRASERO_IMAGE_OPTION_DIALOG (object);

	if (result != GNOME_VFS_OK) {
		brasero_image_option_dialog_image_info_error (dialog);
		return;
	}

    	/* Add it to recent file manager */
	if (!strcmp (info->mime_type, "application/x-toc"))
		brasero_image_option_dialog_make_track (dialog,
							BRASERO_IMAGE_FORMAT_CLONE,
							FALSE,
							uri);
	else if (!strcmp (info->mime_type, "application/octet-stream")) {
		/* that could be an image, so here is the deal:
		 * if we can find the type through the extension, fine.
		 * if not default to CLONE */
		if (g_str_has_suffix (uri, ".bin"))
			brasero_image_option_dialog_make_track (dialog,
								BRASERO_IMAGE_FORMAT_CDRDAO,
								TRUE,
								uri);
		else if (g_str_has_suffix (uri, ".raw"))
			brasero_image_option_dialog_make_track (dialog,
								BRASERO_IMAGE_FORMAT_CLONE,
								TRUE,
								uri);
		else
			brasero_image_option_dialog_make_track (dialog,
								BRASERO_IMAGE_FORMAT_NONE,
								TRUE,
								uri);
	}
	else if (!strcmp (info->mime_type, "application/x-cd-image"))
		brasero_image_option_dialog_make_track (dialog,
							BRASERO_IMAGE_FORMAT_ISO,
							TRUE,
							uri);
	else if (!strcmp (info->mime_type, "application/x-cdrdao-toc"))
		brasero_image_option_dialog_make_track (dialog,
							BRASERO_IMAGE_FORMAT_CDRDAO,
							FALSE,
							uri);
	else if (!strcmp (info->mime_type, "application/x-cue"))
		brasero_image_option_dialog_make_track (dialog,
							BRASERO_IMAGE_FORMAT_CUE,
							FALSE,
							uri);
	else {
		brasero_image_option_dialog_image_info_error (dialog);
		return;
	}
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

static void
brasero_image_option_dialog_image_info (BraseroImageOptionDialog *dialog)
{
	gchar *uri;
	GList *uris;

	uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog->priv->chooser));

	if (!uri) {
		brasero_image_option_dialog_image_empty (dialog);
		return;
	}

	if (!dialog->priv->vfs)
		dialog->priv->vfs = brasero_vfs_get_default ();

	if (!dialog->priv->info_type)
		dialog->priv->info_type = brasero_vfs_register_data_type (dialog->priv->vfs,
									  G_OBJECT (dialog),
									  G_CALLBACK (brasero_image_option_dialog_image_info_cb),
									  NULL);

	uris = g_list_prepend (NULL, uri);
	brasero_vfs_get_info (dialog->priv->vfs,
			      uris,
			      GNOME_VFS_FILE_INFO_GET_MIME_TYPE|
			      GNOME_VFS_FILE_INFO_FORCE_SLOW_MIME_TYPE,
			      dialog->priv->info_type,
			      NULL);
	g_list_free (uris);
	g_free (uri);
}

void
brasero_image_option_dialog_set_image_uri (BraseroImageOptionDialog *dialog,
					   const gchar *uri)
{
	BraseroTrackSource track = {0, };
	NautilusBurnDrive *drive;

	/* we need to set up a dummy track */
	track.type = BRASERO_TRACK_SOURCE_IMAGE;
	track.format = BRASERO_IMAGE_FORMAT_ANY;
	brasero_recorder_selection_set_source_track (BRASERO_RECORDER_SELECTION (dialog->priv->selection),
						     &track);

	brasero_recorder_selection_get_drive (BRASERO_RECORDER_SELECTION (dialog->priv->selection),
					      &drive,
					      NULL);

	brasero_image_type_chooser_set_source (BRASERO_IMAGE_TYPE_CHOOSER (dialog->priv->format_chooser),
					       drive,
					       BRASERO_TRACK_SOURCE_IMAGE,
					       BRASERO_IMAGE_FORMAT_ANY);

	nautilus_burn_drive_unref (drive);

	if (uri)
		gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (dialog->priv->chooser), uri);
}

static void
brasero_image_option_dialog_burn_clicked_cb (GtkWidget *button,
					     BraseroImageOptionDialog *dialog)
{
	BraseroImageFormat format = BRASERO_IMAGE_FORMAT_ANY;

	brasero_image_type_chooser_get_format (BRASERO_IMAGE_TYPE_CHOOSER (dialog->priv->format_chooser),
					       &format);

	if (format != BRASERO_IMAGE_FORMAT_ANY) {
		gchar *uri;

		uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog->priv->chooser));
		brasero_image_option_dialog_make_track (dialog,
							format,
							FALSE,
							uri);
		g_free (uri);
	}
	else
		brasero_image_option_dialog_image_info (dialog);
}

gboolean
brasero_image_option_dialog_get_param (BraseroImageOptionDialog *dialog,
				       BraseroBurnFlag *flags,
				       NautilusBurnDrive **drive,
				       gint *speed,
				       BraseroTrackSource **source)
{
	BraseroDriveProp props;

	g_return_val_if_fail (drive != NULL, FALSE);
	g_return_val_if_fail (source != NULL, FALSE);

	*source = dialog->priv->track;
	dialog->priv->track = NULL;

	/* get drive, speed and flags */
	brasero_recorder_selection_get_drive (BRASERO_RECORDER_SELECTION (dialog->priv->selection),
					      drive,
					      &props);

	if (speed)
		*speed = props.props.drive_speed;

	if (flags) {
		BraseroBurnFlag tmp;

		tmp = BRASERO_BURN_FLAG_DONT_OVERWRITE|
		      BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE;

		tmp |= props.flags;
		*flags = tmp;
	}

	return TRUE;
}

static void
brasero_image_option_dialog_cancel_clicked_cb (GtkButton *cancel, GtkWidget *dialog)
{
	gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
}

static void
brasero_image_option_dialog_init (BraseroImageOptionDialog *obj)
{
	GtkWidget *burn;
	GtkWidget *label;
	GtkWidget *cancel;
	GtkWidget *options;
	GtkWidget *box, *box1;

	obj->priv = g_new0 (BraseroImageOptionDialogPrivate, 1);
	gtk_dialog_set_has_separator (GTK_DIALOG (obj), FALSE);

	obj->priv->tooltips = gtk_tooltips_new ();
	g_object_ref_sink (GTK_OBJECT (obj->priv->tooltips));

	/* first box */
	obj->priv->selection = brasero_recorder_selection_new ();
	g_object_set (G_OBJECT (obj->priv->selection),
		      "file-image", TRUE,
		      "show-properties", TRUE,
		      "show-recorders-only", TRUE,
		      NULL);

	options = brasero_utils_pack_properties (_("<b>Select a drive to write to</b>"),
						 obj->priv->selection,
						 NULL);
	gtk_widget_show_all (options);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (obj)->vbox),
			    options,
			    FALSE,
			    FALSE,
			    6);

	brasero_recorder_selection_select_default_drive (BRASERO_RECORDER_SELECTION (obj->priv->selection),
							 BRASERO_MEDIA_WRITABLE);
	gtk_tooltips_set_tip (obj->priv->tooltips,
			      obj->priv->selection,
			      _("Choose which drive holds the disc to write to"),
			      _("Choose which drive holds the disc to write to"));

	g_object_set (obj->priv->selection, "file-image", FALSE, NULL);

	/* Image properties */
	box1 = gtk_hbox_new (FALSE, 0);
	label = gtk_label_new (_("Path:\t\t"));
	gtk_box_pack_start (GTK_BOX (box1), label, FALSE, FALSE, 0);

	obj->priv->chooser = gtk_file_chooser_button_new (_("Open an image"), GTK_FILE_CHOOSER_ACTION_OPEN);
	gtk_box_pack_start (GTK_BOX (box1), obj->priv->chooser, TRUE, TRUE, 0);

	gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (obj->priv->chooser),
						 g_get_home_dir ());
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (obj->priv->chooser), FALSE);

	obj->priv->format_chooser = brasero_image_type_chooser_new ();

	box = brasero_utils_pack_properties (_("<b>Image</b>"),
					     obj->priv->format_chooser,
					     box1,
					     NULL);
	gtk_box_pack_end (GTK_BOX (GTK_DIALOG (obj)->vbox),
			  box,
			  TRUE,
			  FALSE,
			  6);
	gtk_widget_show_all (box);

	/* buttons */
	cancel = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
	gtk_widget_show (cancel);
	g_signal_connect (G_OBJECT (cancel),
			  "clicked",
			  G_CALLBACK (brasero_image_option_dialog_cancel_clicked_cb),
			  obj);
	gtk_dialog_add_action_widget (GTK_DIALOG (obj), cancel, GTK_RESPONSE_CANCEL);

	burn = brasero_utils_make_button (_("Burn"), BRASERO_STOCK_BURN, NULL);
	gtk_widget_show (burn);
	g_signal_connect (G_OBJECT (burn),
			  "clicked",
			  G_CALLBACK (brasero_image_option_dialog_burn_clicked_cb),
			  obj);
	gtk_box_pack_end (GTK_BOX (GTK_DIALOG (obj)->action_area),
			  burn,
			  FALSE,
			  FALSE,
			  0);
}

static void
brasero_image_option_dialog_finalize (GObject *object)
{
	BraseroImageOptionDialog *cobj;

	cobj = BRASERO_IMAGE_OPTION_DIALOG (object);

	if (cobj->priv->vfs) {
		brasero_vfs_cancel (cobj->priv->vfs, object);
		g_object_unref (cobj->priv->vfs);
		cobj->priv->vfs = NULL;
	}

	if (cobj->priv->track) {
		brasero_track_source_free (cobj->priv->track);
		cobj->priv->track = NULL;
	}

	if (cobj->priv->tooltips) {
		g_object_unref (cobj->priv->tooltips);
		cobj->priv->tooltips = NULL;
	}

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}
