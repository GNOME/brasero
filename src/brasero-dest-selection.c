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

#include <nautilus-burn-drive.h>

#include <gconf/gconf-client.h>

#include "burn-basics.h"
#include "burn-caps.h"
#include "burn-track.h"
#include "burn-medium.h"
#include "burn-session.h"
#include "brasero-ncb.h"
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
};

#define BRASERO_DEST_SELECTION_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_DEST_SELECTION, BraseroDestSelectionPrivate))

enum {
	PROP_0,
	PROP_SESSION
};

static BraseroDriveSelectionClass* parent_class = NULL;

G_DEFINE_TYPE (BraseroDestSelection, brasero_dest_selection, BRASERO_TYPE_DRIVE_SELECTION);

#define BRASERO_DRIVE_PROPERTIES_KEY		"/apps/brasero/drives"

enum {
	VALID_MEDIA_SIGNAL,
	LAST_SIGNAL
};
static guint brasero_dest_selection_signals [LAST_SIGNAL] = { 0 };

static gchar *
brasero_dest_selection_get_config_key (BraseroTrackDataType input,
				       NautilusBurnDrive *drive,
				       const gchar *property)
{
	BraseroMedia media;
	gchar *display_name;
	gchar *disc_type;
	gchar *key = NULL;

	media = NCB_MEDIA_GET_STATUS (drive);

	/* make sure display_name doesn't contain any forbidden characters */
	display_name = nautilus_burn_drive_get_name_for_display (drive);
	g_strdelimit (display_name, " +()", '_');

	disc_type = g_strdup (NCB_MEDIA_GET_TYPE_STRING (drive));
	if (!disc_type) {
		g_free (display_name);
		return NULL;
	}

	g_strdelimit (disc_type, " +()", '_');

	switch (input) {
	case BRASERO_TRACK_TYPE_NONE:
		key = g_strdup_printf ("%s/%s/none_%s/%s",
				       BRASERO_DRIVE_PROPERTIES_KEY,
				       display_name,
				       disc_type,
				       property);
		break;
	case BRASERO_TRACK_TYPE_DISC:
		key = g_strdup_printf ("%s/%s/disc_%s/%s",
				       BRASERO_DRIVE_PROPERTIES_KEY,
				       display_name,
				       disc_type,
				       property);
		break;

	case BRASERO_TRACK_TYPE_DATA:
		key = g_strdup_printf ("%s/%s/data_%s/%s",
				       BRASERO_DRIVE_PROPERTIES_KEY,
				       display_name,
				       disc_type,
				       property);
		break;

	case BRASERO_TRACK_TYPE_IMAGE:
		key = g_strdup_printf ("%s/%s/image_%s/%s",
				       BRASERO_DRIVE_PROPERTIES_KEY,
				       display_name,
				       disc_type,
				       property);
		break;

	case BRASERO_TRACK_TYPE_AUDIO:
		key = g_strdup_printf ("%s/%s/audio_%s/%s",
				       BRASERO_DRIVE_PROPERTIES_KEY,
				       display_name,
				       disc_type,
				       property);
		break;
	default:
		break;
	}

	g_free (display_name);
	g_free (disc_type);
	return key;
}

static void
brasero_dest_selection_save_drive_properties (BraseroDestSelection *self)
{
	BraseroDestSelectionPrivate *priv;
	NautilusBurnDrive *drive;
	BraseroTrackType input;
	BraseroBurnFlag flags;
	GConfClient *client;
	const gchar *path;
	guint64 rate;
	guint speed;
	gchar *key;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

	drive = brasero_burn_session_get_burner (priv->session);
	if (!drive)
		return;

	if (NCB_MEDIA_GET_STATUS (drive) == BRASERO_MEDIUM_NONE)
		return;

	brasero_burn_session_get_input_type (priv->session, &input);

	client = gconf_client_get_default ();

	rate = brasero_burn_session_get_rate (priv->session);
	if (brasero_burn_session_get_dest_media (priv->session) & BRASERO_MEDIUM_DVD)
		speed = BRASERO_RATE_TO_SPEED_DVD (rate);
	else
		speed = BRASERO_RATE_TO_SPEED_CD (rate);

	key = brasero_dest_selection_get_config_key (input.type, drive, "speed");
	gconf_client_set_int (client, key, speed, NULL);
	g_free (key);

	key = brasero_dest_selection_get_config_key (input.type, drive, "flags");
	flags = gconf_client_get_int (client, key, NULL);
	flags &= ~BRASERO_DRIVE_PROPERTIES_FLAGS;
	flags |= (brasero_burn_session_get_flags (priv->session) & BRASERO_DRIVE_PROPERTIES_FLAGS);
	gconf_client_set_int (client, key, flags, NULL);
	g_free (key);

	/* temporary directory */
	path = brasero_burn_session_get_tmpdir (priv->session);
	key = g_strdup_printf ("%s/tmpdir", BRASERO_DRIVE_PROPERTIES_KEY);
	gconf_client_set_string (client, key, path, NULL);
	g_free (key);

	g_object_unref (client);
}

static void
brasero_dest_selection_drive_properties (BraseroDestSelection *self)
{
	BraseroDestSelectionPrivate *priv;
	BraseroBurnFlag compulsory = 0;
	BraseroBurnFlag supported = 0;
	BraseroBurnFlag flags = 0;
	NautilusBurnDrive *drive;
	GtkWidget *toplevel;
	const gchar *path;
	gint result;
	gint64 rate;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

	priv->drive_prop = brasero_drive_properties_new ();

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
	gtk_window_set_transient_for (GTK_WINDOW (priv->drive_prop), GTK_WINDOW (toplevel));
	gtk_window_set_destroy_with_parent (GTK_WINDOW (priv->drive_prop), TRUE);
	gtk_window_set_position (GTK_WINDOW (toplevel), GTK_WIN_POS_CENTER_ON_PARENT);

	drive = brasero_burn_session_get_burner (priv->session);
	rate = brasero_burn_session_get_rate (priv->session);

	brasero_drive_properties_set_drive (BRASERO_DRIVE_PROPERTIES (priv->drive_prop),
					    drive,
					    rate);
	nautilus_burn_drive_unref (drive);

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
		nautilus_burn_drive_unref (drive);
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
	else if (source.type == BRASERO_TRACK_TYPE_AUDIO)
		return;

	if (source.type == BRASERO_TRACK_TYPE_DATA
	||  source.subtype.media & BRASERO_MEDIUM_DVD) {
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
		gboolean result;

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
brasero_dest_selection_image_properties (BraseroDestSelection *self)
{
	BraseroDestSelectionPrivate *priv;
	BraseroImageFormat formats;
	BraseroImageFormat format;
	BraseroTrackType output;
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

	if (!priv->default_format)
		format = brasero_burn_session_get_output_format (priv->session);
	else
		format = BRASERO_IMAGE_FORMAT_ANY;

	num = brasero_dest_selection_get_possible_output_formats (self, &formats);
	brasero_image_properties_set_formats (BRASERO_IMAGE_PROPERTIES (priv->drive_prop),
					      num > 0 ? formats:BRASERO_IMAGE_FORMAT_NONE,
					      format);

	/* and here we go ... run the thing */
	gtk_widget_show (priv->drive_prop);
	answer = gtk_dialog_run (GTK_DIALOG (priv->drive_prop));

	if (answer != GTK_RESPONSE_OK) {
		gtk_widget_destroy (priv->drive_prop);
		priv->drive_prop = NULL;
		g_free (original_path);
		return;
	}

	/* see if the user has changed the path */
	image_path = brasero_image_properties_get_path (BRASERO_IMAGE_PROPERTIES (priv->drive_prop));
	if (strcmp (original_path, image_path)) {
		priv->default_path = FALSE;
		brasero_drive_selection_set_image_path (BRASERO_DRIVE_SELECTION (self), image_path);
	}

	g_free (original_path);

	/* get and check format */
	format = brasero_image_properties_get_format (BRASERO_IMAGE_PROPERTIES (priv->drive_prop));
	if (format != BRASERO_IMAGE_FORMAT_NONE) {
		/* see if we are to choose the format ourselves */
		if (format == BRASERO_IMAGE_FORMAT_ANY) {
			brasero_dest_selection_get_default_output_format (self,
									  &output);
			format = output.subtype.img_format;
		}
	}

	brasero_burn_session_set_image_output (priv->session,
					 format,
					 image_path);

	gtk_widget_destroy (priv->drive_prop);
	priv->drive_prop = NULL;
	g_free (image_path);
}

static void
brasero_dest_selection_properties_button_cb (GtkWidget *button,
					     BraseroDestSelection *self)
{
	BraseroDestSelectionPrivate *priv;
	NautilusBurnDrive *drive;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

	drive = brasero_burn_session_get_burner (priv->session);
	if (!drive)
		return;

	if (NCB_DRIVE_GET_TYPE (drive) == NAUTILUS_BURN_DRIVE_TYPE_FILE)
		brasero_dest_selection_image_properties (self);
	else
		brasero_dest_selection_drive_properties (self);
}

static void
brasero_dest_selection_set_drive_properties (BraseroDestSelection *self)
{
	BraseroDestSelectionPrivate *priv;
	NautilusBurnDrive *drive;
	BraseroTrackType input;
	BraseroBurnFlag flags;
	GError *error = NULL;
	GConfClient *client;
	gchar *path;
	guint64 rate;
	gint speed;
	gchar *key;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

	brasero_burn_session_get_input_type (priv->session, &input);
	if (input.type == BRASERO_TRACK_TYPE_NONE)
		return;
	
	drive = brasero_burn_session_get_burner (priv->session);
	if (!drive)
		return;

	if (NCB_MEDIA_GET_STATUS (drive) == BRASERO_MEDIUM_NONE)
		return;

	/* update/set the rate */
	client = gconf_client_get_default ();

	key = brasero_dest_selection_get_config_key (input.type, drive, "speed");
	speed = gconf_client_get_int (client, key, &error);
	g_free (key);

	if (error) {
		g_error_free (error);
		error = NULL;

		rate = NCB_MEDIA_GET_MAX_WRITE_RATE (drive);
	}
	else if (NCB_MEDIA_GET_STATUS (drive) & BRASERO_MEDIUM_DVD)
		rate = BRASERO_SPEED_TO_RATE_DVD (speed);
	else
		rate = BRASERO_SPEED_TO_RATE_CD (speed);

	brasero_burn_session_set_rate (priv->session, rate);

	/* do the same with the flags */
	key = brasero_dest_selection_get_config_key (input.type, drive, "flags");
	flags = gconf_client_get_int (client, key, &error);
	g_free (key);

	if (error) {
		BraseroTrackType source;

		g_error_free (error);
		error = NULL;

		/* these are sane defaults */
		brasero_burn_session_add_flag (priv->session,
					       BRASERO_BURN_FLAG_EJECT|
					       BRASERO_BURN_FLAG_BURNPROOF);

		brasero_burn_session_remove_flag (priv->session, BRASERO_BURN_FLAG_DUMMY);

		brasero_burn_session_get_input_type (priv->session, &source);
		if (source.type == BRASERO_TRACK_TYPE_DATA
		||  source.type == BRASERO_TRACK_TYPE_DISC
		||  source.type == BRASERO_TRACK_TYPE_IMAGE)
			brasero_burn_session_add_flag (priv->session, BRASERO_BURN_FLAG_NO_TMP_FILES);
		else
			brasero_burn_session_remove_flag (priv->session, BRASERO_BURN_FLAG_NO_TMP_FILES);
	}
	else {
		BraseroBurnFlag supported;
		BraseroBurnFlag compulsory;

		brasero_burn_caps_get_flags (priv->caps,
					     priv->session,
					     &supported,
					     &compulsory);
		flags &= (supported & BRASERO_DRIVE_PROPERTIES_FLAGS);
		flags |= (compulsory & BRASERO_DRIVE_PROPERTIES_FLAGS);

		brasero_burn_session_remove_flag (priv->session, BRASERO_DRIVE_PROPERTIES_FLAGS);
		brasero_burn_session_add_flag (priv->session, flags);
	}

	nautilus_burn_drive_unref (drive);

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
	const gchar *suffixes [] = {".iso",
				    ".toc",
				    ".toc",
				    ".toc",
				    ".bin",
				    NULL };
	BraseroDestSelectionPrivate *priv;
	BraseroTrackType output;
	const gchar *suffix;
	gchar *path;
	gint i = 0;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

	priv->default_format = TRUE;
	priv->default_path = TRUE;

	/* apparently nothing has been set yet so give a default location */
	brasero_dest_selection_get_default_output_format (self, &output);

	if (output.type == BRASERO_TRACK_TYPE_NONE
	||  output.subtype.img_format == BRASERO_IMAGE_FORMAT_NONE) {
		brasero_burn_session_set_image_output (priv->session,
						       BRASERO_IMAGE_FORMAT_NONE,
						       NULL);
		return;
	}

	if (output.subtype.img_format & BRASERO_IMAGE_FORMAT_BIN)
		suffix = suffixes [0];
	else if (output.subtype.img_format & BRASERO_IMAGE_FORMAT_CLONE)
		suffix = suffixes [1];
	else if (output.subtype.img_format & BRASERO_IMAGE_FORMAT_CUE)
		suffix = suffixes [2];
	else if (output.subtype.img_format & BRASERO_IMAGE_FORMAT_CDRDAO)
		suffix = suffixes [3];
	else
		return;

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
	};

	brasero_burn_session_set_image_output (priv->session,
					       output.subtype.img_format,
					       path);
	brasero_drive_selection_set_image_path (BRASERO_DRIVE_SELECTION (self),
						path);
	g_free (path);
}

static void
brasero_dest_selection_check_image_settings (BraseroDestSelection *self)
{
	BraseroDestSelectionPrivate *priv;
	BraseroBurnResult result;
	BraseroTrackType output;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

	/* make sure the current output format is still possible given the new
	 * source. If not, find a better one */
	output.type = BRASERO_TRACK_TYPE_IMAGE;
	output.subtype.img_format = brasero_burn_session_get_output_format (priv->session);
	result = brasero_burn_caps_is_output_supported (priv->caps,
							priv->session,
							&output);

	if (result != BRASERO_BURN_OK) {
		if (priv->button)
			gtk_widget_set_sensitive (priv->button, FALSE);
		return;
	}

	if (priv->button)
		gtk_widget_set_sensitive (priv->button, TRUE);

	if (!priv->default_path) {
		gchar *path;
		BraseroTrackType output;

		path = brasero_dest_selection_get_output_path (self);
		brasero_dest_selection_get_default_output_format (self, &output);
		brasero_burn_session_set_image_output (priv->session,
						       output.subtype.img_format,
						       path);
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
}

static void
brasero_dest_selection_check_drive_settings (BraseroDestSelection *self,
					     NautilusBurnDrive *drive)
{
	BraseroBurnFlag compulsory = BRASERO_BURN_FLAG_NONE;
	BraseroBurnFlag supported = BRASERO_BURN_FLAG_NONE;
	BraseroDestSelectionPrivate *priv;
	BraseroBurnResult result;
	BraseroBurnFlag flags;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

	/* update the flags for the current drive */
	result = brasero_burn_caps_get_flags (priv->caps,
					      priv->session,
					      &supported,
					      &compulsory);

	/* send a signal to tell whether we support this disc or not */
	g_signal_emit (self,
		       brasero_dest_selection_signals [VALID_MEDIA_SIGNAL],
		       0,
		       (result == BRASERO_BURN_OK));

	if (priv->button) {
		if (result != BRASERO_BURN_OK)
			gtk_widget_set_sensitive (priv->button, FALSE);
		else
			gtk_widget_set_sensitive (priv->button, TRUE);
	}

	flags = brasero_burn_session_get_flags (priv->session);
	flags &= (supported & BRASERO_DRIVE_PROPERTIES_FLAGS);
	flags |= (compulsory & BRASERO_DRIVE_PROPERTIES_FLAGS);

	brasero_burn_session_remove_flag (priv->session, BRASERO_DRIVE_PROPERTIES_FLAGS);
	brasero_burn_session_add_flag (priv->session, flags);

	/* save potential changes for the new profile */
	brasero_dest_selection_save_drive_properties (self);

	if (priv->drive_prop) {
		/* the dialog may need to be updated */
		brasero_drive_properties_set_flags (BRASERO_DRIVE_PROPERTIES (priv->drive_prop),
						    flags,
						    supported,
						    compulsory);
	}
}

static void
brasero_dest_selection_check_after_change (BraseroDestSelection *self)
{
	NautilusBurnDrive *drive;
	BraseroDestSelectionPrivate *priv;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

	/* The caps of the library / the source have changed so we must check:
	 * if it's an image that the output type is still possible
	 * if it's a drive that all flags are still supported and that the media is too */
	drive = brasero_burn_session_get_burner (priv->session);
	if (!drive)
		return;

	if (NCB_DRIVE_GET_TYPE (drive) != NAUTILUS_BURN_DRIVE_TYPE_FILE)
		brasero_dest_selection_check_drive_settings (self, drive);
	else
		brasero_dest_selection_check_image_settings (self);

	nautilus_burn_drive_unref (drive);
}

static void
brasero_dest_selection_source_changed (BraseroBurnSession *session,
				       BraseroDestSelection *self)
{
	brasero_dest_selection_set_drive_properties (self);
	brasero_dest_selection_check_after_change (self);
}

static void
brasero_dest_selection_caps_changed (BraseroBurnCaps *caps,
				     BraseroDestSelection *self)
{
	brasero_dest_selection_check_after_change (self);
}

static void
brasero_dest_selection_output_changed (BraseroBurnSession *session,
				       BraseroDestSelection *self)
{
	BraseroDestSelectionPrivate *priv;
	NautilusBurnDrive *burner;
	NautilusBurnDrive *drive;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

	burner = brasero_burn_session_get_burner (priv->session);
	if (priv->drive_prop) {
		/* cancel the drive properties dialog as it's not the same drive */
		gtk_dialog_response (GTK_DIALOG (priv->drive_prop),
				     GTK_RESPONSE_CANCEL);
	}

	/* make sure the current displayed drive reflects that */
	drive = brasero_drive_selection_get_drive (BRASERO_DRIVE_SELECTION (self));
	if (drive && burner && !nautilus_burn_drive_equal (burner, drive)) {
		brasero_drive_selection_set_drive (BRASERO_DRIVE_SELECTION (self), drive);
	}

	if (drive)
		nautilus_burn_drive_unref (drive);

	if (NCB_DRIVE_GET_TYPE (burner) != NAUTILUS_BURN_DRIVE_TYPE_FILE) {
		gint numcopies;

		brasero_dest_selection_set_drive_properties (self);
		brasero_dest_selection_check_drive_settings (self, burner);

		gtk_widget_set_sensitive (priv->copies_box, TRUE);
		gtk_widget_show (priv->copies_box);

		numcopies = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->copies_spin));
		brasero_burn_session_set_num_copies (priv->session, numcopies);
	}
	else {
		gtk_widget_set_sensitive (priv->copies_box, FALSE);
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
				      NautilusBurnDrive *drive)
{
	BraseroDestSelectionPrivate *priv;

	priv = BRASERO_DEST_SELECTION_PRIVATE (selection);
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
	GtkWidget *label;

	priv = BRASERO_DEST_SELECTION_PRIVATE (object);

	priv->caps = brasero_burn_caps_get_default ();
	priv->caps_sig = g_signal_connect (G_OBJECT (priv->caps),
					   "caps-changed",
					   G_CALLBACK (brasero_dest_selection_caps_changed),
					   object);

	priv->button = gtk_button_new_from_stock (GTK_STOCK_PROPERTIES);
	gtk_widget_set_tooltip_text (priv->button, _("Configure some options for the recording"));
	g_signal_connect (G_OBJECT (priv->button),
			  "clicked",
			  G_CALLBACK (brasero_dest_selection_properties_button_cb),
			  object);

	brasero_drive_selection_set_tooltip (BRASERO_DRIVE_SELECTION (object),
					     _("Choose which drive holds the disc to write to"));

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

	priv->default_path = 1;
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
		g_signal_handler_disconnect (priv->caps, priv->caps_sig);
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
	NautilusBurnDrive *drive;

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
		nautilus_burn_drive_unref (drive);

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
