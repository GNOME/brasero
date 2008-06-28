/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2007 <bonfire-app@wanadoo.fr>
 *
 * brasero is free software.
 *
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
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
#include "brasero-image-properties.h"
#include "brasero-dest-selection.h"


typedef struct _BraseroDestSelectionPrivate BraseroDestSelectionPrivate;
struct _BraseroDestSelectionPrivate
{
	BraseroBurnCaps *caps;
	BraseroBurnSession *session;

	GtkWidget *drive_prop;
	GtkWidget *button;

	GtkWidget *copies_box;
	GtkWidget *copies_spin;

	guint caps_sig;
	guint input_sig;
	guint output_sig;

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

enum {
	VALID_MEDIA_SIGNAL,
	LAST_SIGNAL
};
static guint brasero_dest_selection_signals [LAST_SIGNAL] = { 0 };

#define BRASERO_DEST_SAVED_FLAGS	(BRASERO_DRIVE_PROPERTIES_FLAGS|BRASERO_BURN_FLAG_MULTI)

static void
brasero_dest_selection_save_drive_properties (BraseroDestSelection *self)
{
	BraseroDestSelectionPrivate *priv;
	BraseroBurnFlag flags;
	GConfClient *client;
	const gchar *path;
	guint64 rate;
	gchar *key;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

	client = gconf_client_get_default ();

	rate = brasero_burn_session_get_rate (priv->session);
	key = brasero_burn_session_get_config_key (priv->session, "speed");
	if (!key) {
		g_object_unref (client);
		return;
	}

	gconf_client_set_int (client, key, rate / 1024, NULL);
	g_free (key);

	key = brasero_burn_session_get_config_key (priv->session, "flags");
	if (!key) {
		g_object_unref (client);
		return;
	}

	flags = gconf_client_get_int (client, key, NULL);
	flags &= ~BRASERO_DRIVE_PROPERTIES_FLAGS;
	flags |= (brasero_burn_session_get_flags (priv->session) & BRASERO_DEST_SAVED_FLAGS);
	gconf_client_set_int (client, key, flags, NULL);
	g_free (key);

	/* temporary directory */
	path = brasero_burn_session_get_tmpdir (priv->session);
	key = g_strdup_printf ("%s/tmpdir", BRASERO_DRIVE_PROPERTIES_KEY);
	gconf_client_set_string (client, key, path, NULL);
	g_free (key);

	g_object_unref (client);
}

static gboolean
brasero_dest_selection_check_same_src_dest (BraseroDestSelection *self)
{
	BraseroDestSelectionPrivate *priv;
	BraseroMedium *medium;
	BraseroDrive *drive;
	BraseroMedia media;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

	/* if we have the same source and destination as drives then we don't 
	 * grey out the properties button as otherwise it would always remain
	 * so. Instead of that we grey it only if there is no medium or if the
	 * medium is blank. */
	if (!brasero_burn_session_same_src_dest_drive (priv->session))
		return FALSE;

	/* grey out button only if the source (and therefore dest drive)
	 * hasn't got any medium inside */
	drive = brasero_burn_session_get_src_drive (priv->session);
	if (!drive)
		return FALSE;

	medium = brasero_drive_get_medium (drive);
	media = brasero_medium_get_status (medium);;

	if (media == BRASERO_MEDIUM_NONE)
		return FALSE;

	if (media & BRASERO_MEDIUM_BLANK
	|| (media & (BRASERO_MEDIUM_HAS_AUDIO|BRASERO_MEDIUM_HAS_DATA)) == 0)
		return FALSE;

	return TRUE;
}

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
	if (!brasero_dest_selection_check_same_src_dest (self)) {
		brasero_burn_caps_get_flags (priv->caps,
					     priv->session,
					     &supported,
					     &compulsory);
	}
	else {
		supported = BRASERO_DRIVE_PROPERTIES_FLAGS;
		supported &= ~BRASERO_BURN_FLAG_NO_TMP_FILES;
		compulsory = BRASERO_BURN_FLAG_NONE;
	}

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

	brasero_dest_selection_save_drive_properties (self);

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
brasero_dest_selection_get_default_output_format (BraseroDestSelection *self,
						  BraseroTrackType *output)
{
	BraseroTrackType source;
	BraseroBurnResult result;
	BraseroDestSelectionPrivate *priv;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

	brasero_burn_session_get_input_type (priv->session, &source);
	if (source.type == BRASERO_TRACK_TYPE_NONE) {
		output->type = BRASERO_TRACK_TYPE_NONE;
		return;
	}

	output->type = BRASERO_TRACK_TYPE_IMAGE;
	output->subtype.img_format = BRASERO_IMAGE_FORMAT_NONE;

	if (source.type == BRASERO_TRACK_TYPE_IMAGE) {
		output->subtype.img_format = source.subtype.img_format;
		return;
	}

	if (source.type == BRASERO_TRACK_TYPE_AUDIO)
		return;

	if (source.type == BRASERO_TRACK_TYPE_DATA
	||  source.subtype.media & (BRASERO_MEDIUM_DVD|BRASERO_MEDIUM_DVD_DL)) {
		output->subtype.img_format = BRASERO_IMAGE_FORMAT_BIN;
		result = brasero_burn_caps_is_output_supported (priv->caps,
								priv->session,
								output);
		if (result != BRASERO_BURN_OK)
			output->subtype.img_format = BRASERO_IMAGE_FORMAT_NONE;

		return;
	}

	/* for the input which are CDs there are lots of possible formats */
	output->subtype.img_format = BRASERO_IMAGE_FORMAT_CDRDAO;
	for (; output->subtype.img_format != BRASERO_IMAGE_FORMAT_NONE;
	       output->subtype.img_format >>= 1) {
	
		result = brasero_burn_caps_is_output_supported (priv->caps,
								priv->session,
								output);
		if (result == BRASERO_BURN_OK)
			return;
	}

	return;
}

static gchar *
brasero_dest_selection_get_default_output_path (BraseroDestSelection *self,
						BraseroImageFormat format)
{
	const gchar *suffixes [] = {".iso",
				    ".toc",
				    ".cue",
				    ".toc",
				    NULL };
	BraseroDestSelectionPrivate *priv;
	const gchar *suffix = NULL;
	gchar *path;
	gint i = 0;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

	if (format & BRASERO_IMAGE_FORMAT_BIN)
		suffix = suffixes [0];
	else if (format & BRASERO_IMAGE_FORMAT_CLONE)
		suffix = suffixes [1];
	else if (format & BRASERO_IMAGE_FORMAT_CUE)
		suffix = suffixes [2];
	else if (format & BRASERO_IMAGE_FORMAT_CDRDAO)
		suffix = suffixes [3];

	path = g_strdup_printf ("%s/brasero%s",
				g_get_home_dir (),
				suffix);

	while (g_file_test (path, G_FILE_TEST_EXISTS)) {
		g_free (path);

		path = g_strdup_printf ("%s/brasero-%i%s",
					g_get_home_dir (),
					i,
					suffix);
		i ++;
	}

	return path;
}

static gchar *
brasero_dest_selection_fix_image_extension (BraseroImageFormat format,
					    gboolean check_existence,
					    gchar *path)
{
	gchar *dot;
	guint i = 0;
	gchar *retval = NULL;
	const gchar *suffix = NULL;;
	const gchar *suffixes [] = {".iso",
				    ".toc",
				    ".cue",
				    ".toc",
				    NULL };

	/* search the last dot to check extension */
	dot = g_utf8_strrchr (path, -1, '.');
	if (dot && strlen (dot) < 5 && strlen (dot) > 1) {
		if (format & BRASERO_IMAGE_FORMAT_BIN
		&&  strcmp (suffixes [0], dot))
			*dot = '\0';
		else if (format & BRASERO_IMAGE_FORMAT_CLONE
		     &&  strcmp (suffixes [1], dot))
			*dot = '\0';
		else if (format & BRASERO_IMAGE_FORMAT_CUE
		     &&  strcmp (suffixes [2], dot))
			*dot = '\0';
		else if (format & BRASERO_IMAGE_FORMAT_CDRDAO
		     &&  strcmp (suffixes [3], dot))
			*dot = '\0';
		else
			return path;
	}

	/* determine the proper suffix */
	if (format & BRASERO_IMAGE_FORMAT_BIN)
		suffix = suffixes [0];
	else if (format & BRASERO_IMAGE_FORMAT_CLONE)
		suffix = suffixes [1];
	else if (format & BRASERO_IMAGE_FORMAT_CUE)
		suffix = suffixes [2];
	else if (format & BRASERO_IMAGE_FORMAT_CDRDAO)
		suffix = suffixes [3];
	else
		return path;

	/* make sure the file doesn't exist */
	retval = g_strdup_printf ("%s%s", path, suffix);
	if (!check_existence) {
		g_free (path);
		return retval;
	}

	while (g_file_test (retval, G_FILE_TEST_EXISTS)) {
		g_free (retval);
		retval = g_strdup_printf ("%s-%i%s", path, i, suffix);
		i ++;
	}

	g_free (path);
	return retval;
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
	format = brasero_image_properties_get_format (dialog);

	if (format == BRASERO_IMAGE_FORMAT_ANY || format == BRASERO_IMAGE_FORMAT_NONE) {
		BraseroTrackType output;

		brasero_dest_selection_get_default_output_format (self, &output);
		format = output.subtype.img_format;
	}

	if (priv->default_path && !brasero_image_properties_is_path_edited (dialog)) {
		/* not changed: get a new default path */
		g_free (image_path);
		image_path = brasero_dest_selection_get_default_output_path (self, format);
	}
	else
		image_path = brasero_dest_selection_fix_image_extension (format, FALSE, image_path);

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
	BraseroTrackType output;
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
		brasero_dest_selection_get_default_output_format (self, &output);
		format = output.subtype.img_format;
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

		if (!brasero_dest_selection_image_check_extension (self, format, image_path)) {
			if (!brasero_dest_selection_image_extension_ask (self)) {
				priv->default_ext = TRUE;
				image_path = brasero_dest_selection_fix_image_extension (format, TRUE, image_path);
			}
			else
				priv->default_ext = FALSE;
		}
	}
	else
		image_path = brasero_dest_selection_get_default_output_path (self, format);

	gtk_widget_destroy (priv->drive_prop);
	priv->drive_prop = NULL;

	brasero_drive_selection_set_image_path (BRASERO_DRIVE_SELECTION (self), image_path);
	brasero_burn_session_set_image_output (priv->session,
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
brasero_dest_selection_add_drive_properties_flags (BraseroDestSelection *self,
						   BraseroBurnFlag flags,
						   BraseroBurnFlag *supported_retval,
						   BraseroBurnFlag *compulsory_retval)
{
	BraseroDestSelectionPrivate *priv;
	BraseroBurnFlag supported = BRASERO_BURN_FLAG_NONE;
	BraseroBurnFlag compulsory = BRASERO_BURN_FLAG_NONE;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

	/* wipe out previous flags */
	brasero_burn_session_remove_flag (priv->session,
					  BRASERO_DRIVE_PROPERTIES_FLAGS|
					  BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE|
					  BRASERO_BURN_FLAG_FAST_BLANK|
					  BRASERO_BURN_FLAG_DAO);

	/* check each flag before re-adding it */
	brasero_burn_caps_get_flags (priv->caps,
				     priv->session,
				     &supported,
				     &compulsory);

	if ((flags & BRASERO_BURN_FLAG_EJECT)
	&&  (supported & BRASERO_BURN_FLAG_EJECT)) {
		brasero_burn_session_add_flag (priv->session, BRASERO_BURN_FLAG_EJECT);
		brasero_burn_caps_get_flags (priv->caps,
					     priv->session,
					     &supported,
					     &compulsory);
	}

	if ((flags & BRASERO_BURN_FLAG_BURNPROOF)
	&&  (supported & BRASERO_BURN_FLAG_BURNPROOF)) {
		brasero_burn_session_add_flag (priv->session, BRASERO_BURN_FLAG_BURNPROOF);
		brasero_burn_caps_get_flags (priv->caps,
					     priv->session,
					     &supported,
					     &compulsory);
	}

	if ((flags & BRASERO_BURN_FLAG_NO_TMP_FILES)
	&&  (supported & BRASERO_BURN_FLAG_NO_TMP_FILES)) {
		brasero_burn_session_add_flag (priv->session, BRASERO_BURN_FLAG_NO_TMP_FILES);
		brasero_burn_caps_get_flags (priv->caps,
					     priv->session,
					     &supported,
					     &compulsory);
	}

	if ((flags & BRASERO_BURN_FLAG_DUMMY)
	&&  (supported & BRASERO_BURN_FLAG_DUMMY)) {
		brasero_burn_session_add_flag (priv->session, BRASERO_BURN_FLAG_DUMMY);
		brasero_burn_caps_get_flags (priv->caps,
					     priv->session,
					     &supported,
					     &compulsory);
	}

	/* check additional flags */
	if (supported & BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE) {
		/* clean up the disc and have more space when possible */
		brasero_burn_session_add_flag (priv->session, BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE);
		brasero_burn_caps_get_flags (priv->caps,
					     priv->session,
					     &supported,
					     &compulsory);

		if (supported & BRASERO_BURN_FLAG_FAST_BLANK) {
			brasero_burn_session_add_flag (priv->session, BRASERO_BURN_FLAG_FAST_BLANK);
			brasero_burn_caps_get_flags (priv->caps,
						     priv->session,
						     &supported,
						     &compulsory);
		}
	}

	/* use DAO whenever it's possible */
	if (supported & BRASERO_BURN_FLAG_DAO) {
		brasero_burn_session_add_flag (priv->session, BRASERO_BURN_FLAG_DAO);
		brasero_burn_caps_get_flags (priv->caps,
					     priv->session,
					     &supported,
					     &compulsory);
	}
	brasero_burn_session_add_flag (priv->session, compulsory);

	if (supported_retval)
		*supported_retval = supported;
	if (compulsory_retval)
		*compulsory_retval = compulsory;
}

static void
brasero_dest_selection_set_drive_properties (BraseroDestSelection *self)
{
	BraseroDestSelectionPrivate *priv;
	BraseroBurnResult is_valid;
	BraseroTrackType source;
	BraseroBurnFlag flags;
	BraseroMedium *medium;
	BraseroDrive *drive;
	GConfClient *client;
	GConfValue *value;
	guint64 rate;
	gchar *path;
	gchar *key;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

	brasero_burn_session_get_input_type (priv->session, &source);
	if (source.type == BRASERO_TRACK_TYPE_NONE) {
		g_signal_emit (self,
			       brasero_dest_selection_signals [VALID_MEDIA_SIGNAL],
			       0,
			       FALSE);
		gtk_widget_set_sensitive (priv->button, FALSE);
		gtk_widget_set_sensitive (priv->copies_box, FALSE);
		return;
	}

	if (brasero_burn_session_is_dest_file (priv->session)) {
		BraseroBurnResult result;

		result = brasero_burn_caps_is_session_supported (priv->caps, priv->session);
		g_signal_emit (self,
			       brasero_dest_selection_signals [VALID_MEDIA_SIGNAL],
			       0,
			       (result == BRASERO_BURN_OK));

		gtk_widget_set_sensitive (priv->button, (result == BRASERO_BURN_OK));
		return;
	}

	drive = brasero_burn_session_get_burner (priv->session);
	if (!drive) {
		g_signal_emit (self,
			       brasero_dest_selection_signals [VALID_MEDIA_SIGNAL],
			       0,
			       FALSE);
		gtk_widget_set_sensitive (priv->button, FALSE);
		gtk_widget_set_sensitive (priv->copies_box, FALSE);
		return;
	}

	medium = brasero_drive_get_medium (drive);
	if (!medium || brasero_medium_get_status (medium) == BRASERO_MEDIUM_NONE) {
		g_signal_emit (self,
			       brasero_dest_selection_signals [VALID_MEDIA_SIGNAL],
			       0,
			       FALSE);
		gtk_widget_set_sensitive (priv->button, FALSE);
		gtk_widget_set_sensitive (priv->copies_box, FALSE);
		return;
	}

	/* update/set the rate */
	client = gconf_client_get_default ();

	key = brasero_burn_session_get_config_key (priv->session, "speed");
	if (!key) {
		g_object_unref (client);
		return;
	}

	value = gconf_client_get_without_default (client, key, NULL);
	g_free (key);

	if (!value)
		rate = brasero_medium_get_max_write_speed (medium);
	else {
		rate = gconf_value_get_int (value) * 1024;
		gconf_value_free (value);
	}

	brasero_burn_session_set_rate (priv->session, rate);

	/* do the same with the flags.
	 * NOTE: every time we add a flag we have to re-ask for supported flags.
	 * Indeed two flags could be mutually exclusive and then adding both at
	 * the same would make the session unusable (MULTI and BLANK_BEFORE_WRITE) */
	key = brasero_burn_session_get_config_key (priv->session, "flags");
	if (!key) {
		g_object_unref (client);
		return;
	}

	value = gconf_client_get_without_default (client, key, NULL);
	g_free (key);

	if (brasero_dest_selection_check_same_src_dest (self)) {
		/* Special case */

		/* wipe out previous flags */
		brasero_burn_session_remove_flag (priv->session,
						  BRASERO_DRIVE_PROPERTIES_FLAGS);

		/* set new ones */
		if (value) {
			flags = gconf_value_get_int (value) & BRASERO_DEST_SAVED_FLAGS;
			gconf_value_free (value);
		}
		else
			flags = BRASERO_BURN_FLAG_EJECT|
				BRASERO_BURN_FLAG_BURNPROOF;

		brasero_burn_session_add_flag (priv->session, flags);

		/* NOTE: of course NO_TMP is not possible; DAO and BLANK_BEFORE
		 * could be yet. The problem here is that we cannot test all
		 * this since we don't know yet what the disc type is going to 
		 * be. So we set DAO and BLANK_BEFORE_WRITE just in case.
		 * Hopefully burn.c will be able to handle that later. */
		brasero_burn_session_add_flag (priv->session,
					       BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE|
					       BRASERO_BURN_FLAG_FAST_BLANK|
					       BRASERO_BURN_FLAG_DAO);
	}
	else if (!value) {
		BraseroBurnFlag supported = BRASERO_BURN_FLAG_NONE;
		BraseroBurnFlag compulsory = BRASERO_BURN_FLAG_NONE;

		flags = BRASERO_BURN_FLAG_EJECT|
			BRASERO_BURN_FLAG_BURNPROOF;

		if (source.type == BRASERO_TRACK_TYPE_DATA
		||  source.type == BRASERO_TRACK_TYPE_DISC
		||  source.type == BRASERO_TRACK_TYPE_IMAGE)
			flags |= BRASERO_BURN_FLAG_NO_TMP_FILES;

		brasero_dest_selection_add_drive_properties_flags (self,
								   flags,
								   &supported,
								   &compulsory);
	}
	else {
		BraseroBurnFlag supported = BRASERO_BURN_FLAG_NONE;
		BraseroBurnFlag compulsory = BRASERO_BURN_FLAG_NONE;

		/* set the saved flags (make sure they are supported) */
		flags = gconf_value_get_int (value) & BRASERO_DEST_SAVED_FLAGS;
		gconf_value_free (value);

		brasero_dest_selection_add_drive_properties_flags (self,
								   flags,
								   &supported,
								   &compulsory);
	}

	/* Now that we updated the session flags see if everything works */
	is_valid = brasero_burn_caps_is_session_supported (priv->caps, priv->session);
	g_signal_emit (self,
		       brasero_dest_selection_signals [VALID_MEDIA_SIGNAL],
		       0,
		       (is_valid == BRASERO_BURN_OK));

	gtk_widget_set_sensitive (priv->copies_box, (is_valid == BRASERO_BURN_OK));
	gtk_widget_set_sensitive (priv->button, (is_valid == BRASERO_BURN_OK));

	key = g_strdup_printf ("%s/tmpdir", BRASERO_DRIVE_PROPERTIES_KEY);
	path = gconf_client_get_string (client, key, NULL);
	brasero_burn_session_set_tmpdir (priv->session, path);
	g_free (path);
	g_free (key);

	g_object_unref (client);
}

static void
brasero_dest_selection_set_image_properties (BraseroDestSelection *self)
{
	BraseroDestSelectionPrivate *priv;
	BraseroTrackType output;
	gchar *path;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);
	priv->default_format = TRUE;
	priv->default_path = TRUE;

	/* apparently nothing has been set yet so give a default location */
	brasero_dest_selection_get_default_output_format (self, &output);

	if (output.type == BRASERO_TRACK_TYPE_NONE
	||  output.subtype.img_format == BRASERO_IMAGE_FORMAT_NONE) {
		/* That means that we've got a problem */
		/* FIXME: we need to display a message nevertheless */
		brasero_burn_session_set_image_output (priv->session,
						       BRASERO_IMAGE_FORMAT_NONE,
						       NULL);
		g_signal_emit (self,
			       brasero_dest_selection_signals [VALID_MEDIA_SIGNAL],
			       0,
			       FALSE);

		gtk_widget_set_sensitive (priv->button, FALSE);
		return;
	}

	path = brasero_dest_selection_get_default_output_path (self, output.subtype.img_format);
	g_signal_emit (self,
		       brasero_dest_selection_signals [VALID_MEDIA_SIGNAL],
		       0,
		       TRUE);
	gtk_widget_set_sensitive (priv->button, TRUE);

	brasero_burn_session_set_image_output (priv->session,
					       output.subtype.img_format,
					       path);

	brasero_drive_selection_set_image_path (BRASERO_DRIVE_SELECTION (self),
						path);
	g_free (path);

	brasero_burn_session_remove_flag (priv->session,
					  BRASERO_BURN_FLAG_DUMMY|
					  BRASERO_BURN_FLAG_NO_TMP_FILES);
}

static void
brasero_dest_selection_check_image_settings (BraseroDestSelection *self)
{
	BraseroDestSelectionPrivate *priv;
	BraseroBurnResult result;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

	if (brasero_burn_session_get_output (priv->session, NULL, NULL, NULL) == BRASERO_BURN_OK) {
		gchar *path;
		BraseroTrackType output;
		BraseroImageFormat format;

		/* we already have an output check its validity */
		output.type = BRASERO_TRACK_TYPE_IMAGE;

		if (!priv->default_format) {
			/* The user set a format */
			output.subtype.img_format = brasero_burn_session_get_output_format (priv->session);

			/* check that the format is still supported. If not then find a good default */
			result = brasero_burn_caps_is_output_supported (priv->caps,
									priv->session,
									&output);
			if (result != BRASERO_BURN_OK) {
				priv->default_format = TRUE;
				brasero_dest_selection_get_default_output_format (self, &output);
			}
		}
		else /* retrieve a possible better default format */
			brasero_dest_selection_get_default_output_format (self, &output);

		format = output.subtype.img_format;
		g_signal_emit (self,
			       brasero_dest_selection_signals [VALID_MEDIA_SIGNAL],
			       0,
			       (format != BRASERO_IMAGE_FORMAT_NONE));

		if (priv->button)
			gtk_widget_set_sensitive (priv->button, (format != BRASERO_IMAGE_FORMAT_NONE));

		if (format == BRASERO_IMAGE_FORMAT_NONE) {
			/* FIXME: we've got a problem and it's not possible,
			 * display a message to say so */
			if (priv->drive_prop) {
				gtk_widget_destroy (priv->drive_prop);
				priv->drive_prop = NULL;
			}

			return;
		}

		if (!priv->default_path) {
			/* check that the extension is ok if not update it */
			path = brasero_dest_selection_get_output_path (self);
			if (priv->default_ext
			&&  brasero_dest_selection_image_check_extension (self, format, path))
				path = brasero_dest_selection_fix_image_extension (format, TRUE, path);
		}
		else
			path = brasero_dest_selection_get_default_output_path (self, format);

		brasero_burn_session_set_image_output (priv->session, format, path);
		brasero_drive_selection_set_image_path (BRASERO_DRIVE_SELECTION (self), path);
		g_free (path);
	}
	else
		brasero_dest_selection_set_image_properties (self);

	if (priv->drive_prop) {
		BraseroImageFormat formats;
		guint num;

		/* update image settings dialog if needed */
		num = brasero_dest_selection_get_possible_output_formats (self, &formats);
		brasero_image_properties_set_formats (BRASERO_IMAGE_PROPERTIES (priv->drive_prop),
						      num > 1 ? formats:BRASERO_IMAGE_FORMAT_NONE,
						      BRASERO_IMAGE_FORMAT_ANY);
	}

	brasero_burn_session_remove_flag (priv->session, BRASERO_BURN_FLAG_DUMMY);
}

static void
brasero_dest_selection_check_drive_settings (BraseroDestSelection *self)
{
	BraseroBurnFlag compulsory = BRASERO_BURN_FLAG_NONE;
	BraseroBurnFlag supported = BRASERO_BURN_FLAG_NONE;
	BraseroDestSelectionPrivate *priv;
	BraseroBurnResult result;
	BraseroBurnFlag flags;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

	/* Update the flags and save them */
	if (brasero_dest_selection_check_same_src_dest (self)) {
		/* These are always set in any case and there is no way to check
		 * the current flags */
		brasero_burn_session_add_flag (priv->session,
					       BRASERO_BURN_FLAG_DAO|
					       BRASERO_BURN_FLAG_FAST_BLANK|
					       BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE);
	}
	else {
		/* Try to properly update the flags for the current drive */
		flags = brasero_burn_session_get_flags (priv->session);

		/* check each flag before re-adding it */
		brasero_dest_selection_add_drive_properties_flags (self,
								   flags,
								   &supported,
								   &compulsory);
	}

	/* NOTE: we save even if result != BRASERO_BURN_OK. That way if a flag
	 * is no longer supported after the removal of a plugin then the 
	 * properties are reset and the user can access them again */

	/* save potential changes for the new profile */
	brasero_dest_selection_save_drive_properties (self);

	if (priv->drive_prop) {
		/* the dialog may need to be updated */
		brasero_drive_properties_set_flags (BRASERO_DRIVE_PROPERTIES (priv->drive_prop),
						    flags,
						    supported,
						    compulsory);
	}

	/* Once we've updated the flags, send a signal to tell whether we
	 * support this disc or not. Update everything. */
	result = brasero_burn_caps_is_session_supported (priv->caps, priv->session);
	g_signal_emit (self,
		       brasero_dest_selection_signals [VALID_MEDIA_SIGNAL],
		       0,
		       (result == BRASERO_BURN_OK));

	if (priv->button)
		gtk_widget_set_sensitive (priv->button, (result == BRASERO_BURN_OK));

	gtk_widget_set_sensitive (priv->copies_box, (result == BRASERO_BURN_OK));
}

static void
brasero_dest_selection_source_changed (BraseroBurnSession *session,
				       BraseroDestSelection *self)
{
	BraseroDestSelectionPrivate *priv;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

	if (brasero_burn_session_is_dest_file (priv->session)) {
		/* check that if a path was set there may be none if there was
		 * no disc inserted when the dialog was created. */
		if (brasero_burn_session_get_output (priv->session, NULL, NULL, NULL) != BRASERO_BURN_OK)
			brasero_dest_selection_set_image_properties (self);

		return;
	}

	/* NOTE: that can't happen if we are going to write to an image since
	 * that would mean we are changing the image format (something we don't
	 * do). So it has to be when we write to a drive */
	brasero_dest_selection_set_drive_properties (self);
}

static void
brasero_dest_selection_caps_changed (BraseroPluginManager *manager,
				     BraseroDestSelection *self)
{
	BraseroDestSelectionPrivate *priv;

	/* In this case we are still in the same context (same src, dest) so we
	 * check that all current flags and such are still valid */

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

	/* The caps of the library / the source have changed so we must check:
	 * if it's an image that the output type is still possible
	 * if it's a drive that all flags are still supported and that the media is too */
	if (brasero_burn_session_is_dest_file (priv->session))
		brasero_dest_selection_check_image_settings (self);
	else
		brasero_dest_selection_check_drive_settings (self);
}

static void
brasero_dest_selection_output_changed (BraseroBurnSession *session,
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
		brasero_dest_selection_set_drive_properties (self);
		return;
	}

	if (!brasero_drive_is_fake (burner)) {
		gint numcopies;

		brasero_dest_selection_set_drive_properties (self);

		numcopies = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->copies_spin));
		brasero_burn_session_set_num_copies (priv->session, numcopies);
		gtk_widget_show (priv->copies_box);
	}
	else {
		gtk_widget_hide (priv->copies_box);
		brasero_burn_session_set_num_copies (priv->session, 1);

		/* Make sure there is an output path/type in case that's an image;
		 * if not, set default ones */
		if (brasero_burn_session_get_output (priv->session, NULL, NULL, NULL) != BRASERO_BURN_OK)
			brasero_dest_selection_set_image_properties (self);
		else
			brasero_dest_selection_check_image_settings (self);
	}
}

static void
brasero_dest_selection_drive_changed (BraseroDriveSelection *selection,
				      BraseroDrive *drive)
{
	BraseroDestSelectionPrivate *priv;

	priv = BRASERO_DEST_SELECTION_PRIVATE (selection);

	if (!priv->session)
		return;

	brasero_burn_session_set_burner (priv->session, drive);

	if (brasero_burn_session_same_src_dest_drive (priv->session))
		brasero_drive_selection_set_same_src_dest (selection);
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
	BraseroPluginManager *manager;
	GtkWidget *label;

	priv = BRASERO_DEST_SELECTION_PRIVATE (object);

	priv->caps = brasero_burn_caps_get_default ();
	manager = brasero_plugin_manager_get_default ();
	priv->caps_sig = g_signal_connect (manager,
					   "caps-changed",
					   G_CALLBACK (brasero_dest_selection_caps_changed),
					   object);

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

	if (priv->output_sig) {
		g_signal_handler_disconnect (priv->session, priv->output_sig);
		priv->output_sig = 0;
	}

	if (priv->input_sig) {
		g_signal_handler_disconnect (priv->session, priv->input_sig);
		priv->input_sig = 0;
	}

	if (priv->caps_sig) {
		BraseroPluginManager *manager;

		manager = brasero_plugin_manager_get_default ();
		g_signal_handler_disconnect (manager, priv->caps_sig);
		priv->caps_sig = 0;
	}

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

		drive = brasero_drive_selection_get_drive (BRASERO_DRIVE_SELECTION (object));
		brasero_burn_session_set_burner (session, drive);

		if (drive)
			g_object_unref (drive);

		if (brasero_burn_session_same_src_dest_drive (priv->session))
			brasero_drive_selection_set_same_src_dest (BRASERO_DRIVE_SELECTION (object));
		else if (brasero_burn_session_is_dest_file (session))
			brasero_dest_selection_set_image_properties (BRASERO_DEST_SELECTION (object));
		else
			brasero_dest_selection_set_drive_properties (BRASERO_DEST_SELECTION (object));

		priv->input_sig = g_signal_connect (session,
						    "input-changed",
						    G_CALLBACK (brasero_dest_selection_source_changed),
						    object);
		priv->output_sig = g_signal_connect (session,
						     "output-changed",
						     G_CALLBACK (brasero_dest_selection_output_changed),
						     object);
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

	brasero_dest_selection_signals [VALID_MEDIA_SIGNAL] =
	    g_signal_new ("valid_media",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__BOOLEAN,
			  G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

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
