/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2007-2008 <bonfire-app@wanadoo.fr>
 *
 *  Brasero is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 * brasero is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with brasero.  If not, write to:
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

#include <gtk/gtkbutton.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkstock.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkspinbutton.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkmessagedialog.h>

#include <gconf/gconf-client.h>

#include "burn-basics.h"
#include "burn-caps.h"
#include "burn-track.h"
#include "burn-medium.h"
#include "burn-session.h"
#include "burn-plugin-manager.h"
#include "burn-drive.h"
#include "brasero-drive-selection.h"
#include "brasero-drive-properties.h"
#include "brasero-drive-info.h"
#include "brasero-image-properties.h"
#include "brasero-dest-selection.h"


typedef struct _BraseroDestSelectionPrivate BraseroDestSelectionPrivate;
struct _BraseroDestSelectionPrivate
{
	BraseroBurnCaps *caps;
	BraseroBurnSession *session;

	GtkWidget *info;

	GtkWidget *drive_prop;
	GtkWidget *button;

	GtkWidget *copies_box;
	GtkWidget *copies_spin;

	guint default_format:1;
	guint default_path:1;
	guint default_ext:1;
};

#define BRASERO_DEST_SELECTION_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_DEST_SELECTION, BraseroDestSelectionPrivate))

enum {
	PROP_0,
	PROP_SESSION
};

static BraseroDriveSelectionClass* parent_class = NULL;

G_DEFINE_TYPE (BraseroDestSelection, brasero_dest_selection, BRASERO_TYPE_DRIVE_SELECTION);

#define BRASERO_DEST_SAVED_FLAGS	(BRASERO_DRIVE_PROPERTIES_FLAGS|BRASERO_BURN_FLAG_MULTI)

static void
brasero_dest_selection_drive_properties (BraseroDestSelection *self)
{
	BraseroDestSelectionPrivate *priv;
	BraseroBurnFlag compulsory = 0;
	BraseroBurnFlag supported = 0;
	BraseroBurnFlag flags = 0;
	BraseroDrive *drive;
	GtkWidget *toplevel;
	const gchar *path;
	gint result;
	gint64 rate;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

	/* Build dialog */
	priv->drive_prop = brasero_drive_properties_new ();

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
	gtk_window_set_transient_for (GTK_WINDOW (priv->drive_prop), GTK_WINDOW (toplevel));
	gtk_window_set_destroy_with_parent (GTK_WINDOW (priv->drive_prop), TRUE);
	gtk_window_set_position (GTK_WINDOW (toplevel), GTK_WIN_POS_CENTER_ON_PARENT);

	/* get information */
	drive = brasero_burn_session_get_burner (priv->session);
	rate = brasero_burn_session_get_rate (priv->session);

	brasero_drive_properties_set_drive (BRASERO_DRIVE_PROPERTIES (priv->drive_prop),
					    drive,
					    rate);

	flags = brasero_burn_session_get_flags (priv->session);
	brasero_burn_caps_get_flags (priv->caps,
				     priv->session,
				     &supported,
				     &compulsory);

	brasero_drive_properties_set_flags (BRASERO_DRIVE_PROPERTIES (priv->drive_prop),
					    flags,
					    supported,
					    compulsory);

	path = brasero_burn_session_get_tmpdir (priv->session);
	brasero_drive_properties_set_tmpdir (BRASERO_DRIVE_PROPERTIES (priv->drive_prop),
					     path);

	/* launch the dialog */
	gtk_widget_show_all (priv->drive_prop);
	result = gtk_dialog_run (GTK_DIALOG (priv->drive_prop));
	if (result != GTK_RESPONSE_ACCEPT) {
		gtk_widget_destroy (priv->drive_prop);
		priv->drive_prop = NULL;
		return;
	}

	rate = brasero_drive_properties_get_rate (BRASERO_DRIVE_PROPERTIES (priv->drive_prop));
	brasero_burn_session_set_rate (priv->session, rate);

	brasero_burn_session_remove_flag (priv->session, BRASERO_DRIVE_PROPERTIES_FLAGS);
	flags = brasero_drive_properties_get_flags (BRASERO_DRIVE_PROPERTIES (priv->drive_prop));
	brasero_burn_session_add_flag (priv->session, flags);

	path = brasero_drive_properties_get_tmpdir (BRASERO_DRIVE_PROPERTIES (priv->drive_prop));
	brasero_burn_session_set_tmpdir (priv->session, path);

	gtk_widget_destroy (priv->drive_prop);
	priv->drive_prop = NULL;
}

static gchar *
brasero_dest_selection_get_output_path (BraseroDestSelection *self)
{
	gchar *path = NULL;
	BraseroImageFormat format;
	BraseroDestSelectionPrivate *priv;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

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
brasero_dest_selection_set_output_path (BraseroDestSelection *self,
					BraseroImageFormat format,
					const gchar *path)
{
	BraseroDestSelectionPrivate *priv;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

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
brasero_dest_selection_get_possible_output_formats (BraseroDestSelection *self,
						    BraseroImageFormat *formats)
{
	guint num = 0;
	BraseroTrackType output;
	BraseroImageFormat format;
	BraseroDestSelectionPrivate *priv;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

	/* see how many output format are available */
	format = BRASERO_IMAGE_FORMAT_CDRDAO;
	(*formats) = BRASERO_IMAGE_FORMAT_NONE;
	output.type = BRASERO_TRACK_TYPE_IMAGE;

	for (; format > BRASERO_IMAGE_FORMAT_NONE; format >>= 1) {
		BraseroBurnResult result;

		output.subtype.img_format = format;
		result = brasero_burn_caps_is_output_supported (priv->caps,
								priv->session,
								&output);
		if (result == BRASERO_BURN_OK) {
			(*formats) |= format;
			num ++;
		}
	}

	return num;
}

static void
brasero_dest_selection_image_format_changed_cb (BraseroImageProperties *dialog,
						BraseroDestSelection *self)
{
	BraseroDestSelectionPrivate *priv;
	BraseroImageFormat format;
	gchar *image_path;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

	/* make sure the extension is still valid */
	image_path = brasero_image_properties_get_path (dialog);
	if (!image_path)
		return;

	format = brasero_image_properties_get_format (dialog);

	if (format == BRASERO_IMAGE_FORMAT_ANY || format == BRASERO_IMAGE_FORMAT_NONE)
		format = brasero_burn_caps_get_default_output_format (priv->caps, priv->session);

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
brasero_dest_selection_image_check_extension (BraseroDestSelection *self,
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
brasero_dest_selection_image_extension_ask (BraseroDestSelection *self)
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

		
	gtk_window_set_title (GTK_WINDOW (dialog), _("Image Extension"));
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  _("If you choose to keep it programs may not be able to recognize the file type properly."));

	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       _("_Don't change extension"),
			       GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       _("Change _extension"),
			       GTK_RESPONSE_YES);

	answer = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	if (answer == GTK_RESPONSE_YES)
		return TRUE;

	return FALSE;
}

static void
brasero_dest_selection_image_properties (BraseroDestSelection *self)
{
	BraseroDestSelectionPrivate *priv;
	BraseroImageFormat formats;
	BraseroImageFormat format;
	gulong format_changed;
	gchar *original_path;
	GtkWindow *toplevel;
	gchar *image_path;
	gint answer;
	guint num;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

	priv->drive_prop = brasero_image_properties_new ();

	toplevel = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self)));
	gtk_window_set_transient_for (GTK_WINDOW (priv->drive_prop), GTK_WINDOW (toplevel));
	gtk_window_set_destroy_with_parent (GTK_WINDOW (priv->drive_prop), TRUE);
	gtk_window_set_position (GTK_WINDOW (toplevel), GTK_WIN_POS_CENTER_ON_PARENT);

	/* set all information namely path and format */
	original_path = brasero_dest_selection_get_output_path (self);
	brasero_image_properties_set_path (BRASERO_IMAGE_PROPERTIES (priv->drive_prop), original_path);
	g_free (original_path);

	if (!priv->default_format)
		format = brasero_burn_session_get_output_format (priv->session);
	else
		format = BRASERO_IMAGE_FORMAT_ANY;

	num = brasero_dest_selection_get_possible_output_formats (self, &formats);
	brasero_image_properties_set_formats (BRASERO_IMAGE_PROPERTIES (priv->drive_prop),
					      num > 0 ? formats:BRASERO_IMAGE_FORMAT_NONE,
					      format);

	format_changed = g_signal_connect (priv->drive_prop,
					   "format-changed",
					   G_CALLBACK (brasero_dest_selection_image_format_changed_cb),
					   self);

	/* and here we go ... run the thing */
	gtk_widget_show (priv->drive_prop);
	answer = gtk_dialog_run (GTK_DIALOG (priv->drive_prop));

	g_signal_handler_disconnect (priv->drive_prop, format_changed);

	if (answer != GTK_RESPONSE_OK) {
		gtk_widget_destroy (priv->drive_prop);
		priv->drive_prop = NULL;
		return;
	}

	/* get and check format */
	format = brasero_image_properties_get_format (BRASERO_IMAGE_PROPERTIES (priv->drive_prop));

	/* see if we are to choose the format ourselves */
	if (format == BRASERO_IMAGE_FORMAT_ANY || format == BRASERO_IMAGE_FORMAT_NONE) {
		format = brasero_burn_caps_get_default_output_format (priv->caps, priv->session);
		priv->default_format = TRUE;
	}
	else
		priv->default_format = FALSE;

	/* see if the user has changed the path */
	if (brasero_image_properties_is_path_edited (BRASERO_IMAGE_PROPERTIES (priv->drive_prop)))
		priv->default_path = FALSE;

	if (!priv->default_path) {
		/* check the extension */
		image_path = brasero_image_properties_get_path (BRASERO_IMAGE_PROPERTIES (priv->drive_prop));

		/* there is one special case: CLONE image tocs _must_ have a
		 * correct suffix ".toc" so don't ask, fix it */
		if (!brasero_dest_selection_image_check_extension (self, format, image_path)) {
			if (format == BRASERO_IMAGE_FORMAT_CLONE
			||  brasero_dest_selection_image_extension_ask (self)) {
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

	gtk_widget_destroy (priv->drive_prop);
	priv->drive_prop = NULL;

	brasero_drive_info_set_image_path (BRASERO_DRIVE_INFO (priv->info), image_path);
	brasero_dest_selection_set_output_path (self,
						format,
						image_path);
	g_free (image_path);
}

static void
brasero_dest_selection_properties_button_cb (GtkWidget *button,
					     BraseroDestSelection *self)
{
	BraseroDestSelectionPrivate *priv;
	BraseroDrive *drive;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

	drive = brasero_burn_session_get_burner (priv->session);
	if (!drive)
		return;

	if (brasero_drive_is_fake (drive))
		brasero_dest_selection_image_properties (self);
	else
		brasero_dest_selection_drive_properties (self);
}

static void
brasero_dest_selection_update_image_output (BraseroDestSelection *self,
					    gboolean is_valid)
{
	BraseroDestSelectionPrivate *priv;
	BraseroImageFormat valid_format;
	BraseroImageFormat format;
	gchar *path = NULL;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

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
			valid_format = brasero_burn_caps_get_default_output_format (priv->caps, priv->session);
		}
		else if (priv->default_format) {
			/* since input, or caps changed, check if there isn't a
			 * better format available. */
			valid_format = brasero_burn_caps_get_default_output_format (priv->caps, priv->session);
		}
	}
	else {
		/* This is always invalid; find one */
		priv->default_format = TRUE;
		valid_format = brasero_burn_caps_get_default_output_format (priv->caps, priv->session);
	}

	/* see if we have a workable format */
	if (valid_format == BRASERO_IMAGE_FORMAT_NONE) {
		if (priv->drive_prop) {
			gtk_widget_destroy (priv->drive_prop);
			priv->drive_prop = NULL;
		}

		return;
	}

	path = brasero_dest_selection_get_output_path (self);

	/* Now check, fix the output path, _provided__the__format__changed_ */
	if (valid_format == format) {
		brasero_drive_info_set_image_path (BRASERO_DRIVE_INFO (priv->info), path);
		g_free (path);
		return;
	}

	if (!path) {
		priv->default_path = TRUE;
		priv->default_ext = TRUE;
		path = brasero_image_format_get_default_path (valid_format);
	}
	else if (priv->default_ext
	     &&  brasero_dest_selection_image_check_extension (self, format, path)) {
		gchar *tmp;

		priv->default_ext = TRUE;

		tmp = path;
		path = brasero_image_format_fix_path_extension (format, TRUE, path);
		g_free (tmp);
	}

	/* Do it now !!! before a possible nested "is-valid" signal is fired */
	brasero_drive_info_set_image_path (BRASERO_DRIVE_INFO (priv->info), path);

	/* we always need to do this */
	brasero_dest_selection_set_output_path (self,
						valid_format,
						path);

	g_free (path);

	if (priv->drive_prop) {
		BraseroImageFormat formats;
		guint num;

		/* update image settings dialog if needed */
		num = brasero_dest_selection_get_possible_output_formats (self, &formats);
		brasero_image_properties_set_formats (BRASERO_IMAGE_PROPERTIES (priv->drive_prop),
						      num > 1 ? formats:BRASERO_IMAGE_FORMAT_NONE,
						      BRASERO_IMAGE_FORMAT_ANY);
	}
}

static void
brasero_dest_selection_valid_session (BraseroBurnSession *session,
				      gboolean is_valid,
				      BraseroDestSelection *self)
{
	BraseroDestSelectionPrivate *priv;
	BraseroDrive *burner;
	BraseroDrive *drive;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

	/* make sure the current displayed drive reflects that */
	burner = brasero_burn_session_get_burner (priv->session);
	drive = brasero_drive_selection_get_drive (BRASERO_DRIVE_SELECTION (self));
	if (burner != drive) {
		brasero_drive_selection_set_drive (BRASERO_DRIVE_SELECTION (self), drive);

		if (priv->drive_prop) {
			/* cancel the drive properties dialog as it's not the same drive */
			gtk_dialog_response (GTK_DIALOG (priv->drive_prop), GTK_RESPONSE_CANCEL);
		}
	}

	if (drive)
		g_object_unref (drive);

	if (!burner) {
		gtk_widget_set_sensitive (priv->button, is_valid);
		return;
	}

	/* do it now !!! */
	gtk_widget_set_sensitive (priv->button, is_valid);

	if (!brasero_drive_is_fake (burner)) {
		gint numcopies;

		numcopies = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->copies_spin));
		brasero_burn_session_set_num_copies (priv->session, numcopies);
		gtk_widget_set_sensitive (priv->copies_box, is_valid);
		gtk_widget_show (priv->copies_box);

		brasero_drive_info_set_medium (BRASERO_DRIVE_INFO (priv->info),
					       brasero_drive_get_medium (drive));
 		brasero_drive_info_set_same_src_dest (BRASERO_DRIVE_INFO (priv->info),
						      brasero_burn_session_same_src_dest_drive (priv->session));
	}
	else {
		gtk_widget_hide (priv->copies_box);
		brasero_burn_session_set_num_copies (priv->session, 1);

		/* need to update the format and perhaps the path */
		brasero_dest_selection_update_image_output (self, is_valid);
	}
}

static void
brasero_dest_selection_drive_changed (BraseroDriveSelection *selection,
				      BraseroDrive *drive)
{
	BraseroDestSelectionPrivate *priv;

	priv = BRASERO_DEST_SELECTION_PRIVATE (selection);

	if (priv->session)
		brasero_burn_session_set_burner (priv->session, drive);	
}

static void
brasero_dest_selection_copies_num_changed_cb (GtkSpinButton *button,
					      BraseroDestSelection *self)
{
	gint numcopies;
	BraseroDestSelectionPrivate *priv;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);
	numcopies = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->copies_spin));
	brasero_burn_session_set_num_copies (priv->session, numcopies);
}

static void
brasero_dest_selection_init (BraseroDestSelection *object)
{
	BraseroDestSelectionPrivate *priv;
	GtkWidget *label;

	priv = BRASERO_DEST_SELECTION_PRIVATE (object);

	priv->info = brasero_drive_info_new ();
	gtk_widget_show (priv->info);
	gtk_box_pack_start (GTK_BOX (object),
			    priv->info,
			    FALSE,
			    FALSE,
			    0);

	priv->caps = brasero_burn_caps_get_default ();

	priv->button = gtk_button_new_from_stock (GTK_STOCK_PROPERTIES);
	gtk_widget_show (priv->button);
	gtk_widget_set_tooltip_text (priv->button, _("Configure some options for the recording"));
	g_signal_connect (G_OBJECT (priv->button),
			  "clicked",
			  G_CALLBACK (brasero_dest_selection_properties_button_cb),
			  object);

	brasero_drive_selection_set_tooltip (BRASERO_DRIVE_SELECTION (object),
					     _("Choose the disc to write to"));

	brasero_drive_selection_set_button (BRASERO_DRIVE_SELECTION (object),
					    priv->button);

	priv->copies_box = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (priv->copies_box);
	gtk_box_pack_end (GTK_BOX (object), priv->copies_box, FALSE, FALSE, 0);

	label = gtk_label_new (_("Number of copies "));
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (priv->copies_box), label, FALSE, FALSE, 0);

	priv->copies_spin = gtk_spin_button_new_with_range (1.0, 99.0, 1.0);
	gtk_widget_show (priv->copies_spin);
	gtk_box_pack_start (GTK_BOX (priv->copies_box), priv->copies_spin, FALSE, FALSE, 0);
	g_signal_connect (priv->copies_spin,
			  "value-changed",
			  G_CALLBACK (brasero_dest_selection_copies_num_changed_cb),
			  object);

	/* Only show media on which we can write and which are in a burner.
	 * There is one exception though, when we're copying media and when the
	 * burning device is the same as the dest device. */
	brasero_drive_selection_set_type_shown (BRASERO_DRIVE_SELECTION (object),
						BRASERO_MEDIA_TYPE_WRITABLE);

	priv->default_ext = TRUE;
	priv->default_path = TRUE;
	priv->default_format = TRUE;
}

static void
brasero_dest_selection_finalize (GObject *object)
{
	BraseroDestSelectionPrivate *priv;

	priv = BRASERO_DEST_SELECTION_PRIVATE (object);

	if (priv->caps) {
		g_object_unref (priv->caps);
		priv->caps = NULL;
	}

	if (priv->session) {
		g_object_unref (priv->session);
		priv->session = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_dest_selection_set_property (GObject *object,
				     guint property_id,
				     const GValue *value,
				     GParamSpec *pspec)
{
	BraseroDestSelectionPrivate *priv;
	BraseroBurnSession *session;
	BraseroDrive *drive;

	priv = BRASERO_DEST_SELECTION_PRIVATE (object);

	switch (property_id) {
	case PROP_SESSION:
		if (priv->session)
			g_object_unref (priv->session);

		session = g_value_get_object (value);

		/* NOTE: no need to unref a potential previous session since
		 * it's only set at construct time */
		priv->session = session;
		g_object_ref (session);
		g_signal_connect (session,
				  "is-valid",
				  G_CALLBACK (brasero_dest_selection_valid_session),
				  object);

		drive = brasero_drive_selection_get_drive (BRASERO_DRIVE_SELECTION (object));
		brasero_burn_session_set_burner (session, drive);

		if (drive)
			g_object_unref (drive);

		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
brasero_dest_selection_get_property (GObject *object,
				     guint property_id,
				     GValue *value,
				     GParamSpec *pspec)
{
	BraseroDestSelectionPrivate *priv;

	priv = BRASERO_DEST_SELECTION_PRIVATE (object);

	switch (property_id) {
	case PROP_SESSION:
		g_value_set_object (value, priv->session);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
brasero_dest_selection_class_init (BraseroDestSelectionClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	BraseroDriveSelectionClass *select_class = BRASERO_DRIVE_SELECTION_CLASS (klass);

	parent_class = BRASERO_DRIVE_SELECTION_CLASS (g_type_class_peek_parent (klass));

	g_type_class_add_private (klass, sizeof (BraseroDestSelectionPrivate));

	object_class->finalize = brasero_dest_selection_finalize;
	object_class->set_property = brasero_dest_selection_set_property;
	object_class->get_property = brasero_dest_selection_get_property;

	select_class->drive_changed = brasero_dest_selection_drive_changed;

	g_object_class_install_property (object_class,
					 PROP_SESSION,
					 g_param_spec_object ("session",
							      "The session to work with",
							      "The session to work with",
							      BRASERO_TYPE_BURN_SESSION,
							      G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY));
}

GtkWidget *
brasero_dest_selection_new (BraseroBurnSession *session)
{
	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (session), NULL);

	return GTK_WIDGET (g_object_new (BRASERO_TYPE_DEST_SELECTION,
					 "session", session,
					 NULL));
}
