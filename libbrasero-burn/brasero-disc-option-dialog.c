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
#include <string.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

#include "brasero-drive.h"
#include "brasero-medium.h"
#include "brasero-volume.h"

#include "brasero-misc.h"

#include "burn-basics.h"
#include "brasero-tags.h"
#include "brasero-session.h"
#include "brasero-track-data.h"
#include "burn-plugin-manager.h"
#include "brasero-disc-option-dialog.h"
#include "brasero-dest-selection.h"
#include "brasero-session-cfg.h"
#include "brasero-burn-options.h"
#include "brasero-burn-options-private.h"

G_DEFINE_TYPE (BraseroDiscOptionDialog, brasero_disc_option_dialog, BRASERO_TYPE_BURN_OPTIONS);

struct _BraseroDiscOptionDialogPrivate {
	GtkWidget *joliet_toggle;

	GtkWidget *video_options;
	GtkWidget *vcd_label;
	GtkWidget *vcd_button;
	GtkWidget *svcd_button;

	GtkWidget *button_4_3;
	GtkWidget *button_16_9;

	gulong valid_sig;

	guint joliet_warning:1;
	guint joliet_saved:1;
};
typedef struct _BraseroDiscOptionDialogPrivate BraseroDiscOptionDialogPrivate;

#define BRASERO_DISC_OPTION_DIALOG_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_DISC_OPTION_DIALOG, BraseroDiscOptionDialogPrivate))

static GtkDialogClass *parent_class = NULL;


static void
brasero_disc_option_audio_AC3 (BraseroDiscOptionDialog *dialog)
{
	BraseroBurnSession *session;
	GValue *value = NULL;

	session = brasero_burn_options_get_session (BRASERO_BURN_OPTIONS (dialog));

	value = g_new0 (GValue, 1);
	g_value_init (value, G_TYPE_INT);
	g_value_set_int (value, BRASERO_AUDIO_FORMAT_AC3);
	brasero_burn_session_tag_add (session,
				      BRASERO_DVD_STREAM_FORMAT,
				      value);

	g_object_unref (session);
}

static void
brasero_disc_option_audio_MP2 (BraseroDiscOptionDialog *dialog)
{
	BraseroBurnSession *session;
	GValue *value = NULL;

	session = brasero_burn_options_get_session (BRASERO_BURN_OPTIONS (dialog));

	value = g_new0 (GValue, 1);
	g_value_init (value, G_TYPE_INT);
	g_value_set_int (value, BRASERO_AUDIO_FORMAT_MP2);
	brasero_burn_session_tag_add (session,
				      BRASERO_DVD_STREAM_FORMAT,
				      value);

	g_object_unref (session);
}

/**
 * These functions are used when caps-changed event or drive-changed event
 * are generated. They are used to check that flags or fs are valid.
 */

static void
brasero_disc_option_dialog_set_tracks_image_fs (BraseroBurnSession *session,
						BraseroImageFS fs_type)
{
	GSList *tracks;
	GSList *iter;

	tracks = brasero_burn_session_get_tracks (session);
	for (iter = tracks; iter; iter = iter->next) {
		BraseroTrack *track;

		track = iter->data;
		if (!BRASERO_IS_TRACK_DATA (track))
			continue;

		brasero_track_data_add_fs (BRASERO_TRACK_DATA (track), fs_type);
	}
}

static gboolean
brasero_disc_option_dialog_update_joliet (BraseroDiscOptionDialog *dialog)
{
	BraseroImageFS fs_type;
	BraseroBurnResult result;
	BraseroBurnSession *session;
	BraseroTrackType *source = NULL;
	BraseroDiscOptionDialogPrivate *priv;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);
	if (!priv->joliet_toggle)
		return FALSE;

	/* what we want to check Joliet support */
	session = brasero_burn_options_get_session (BRASERO_BURN_OPTIONS (dialog));

	source = brasero_track_type_new ();
	brasero_burn_session_get_input_type (session, source);
	fs_type = brasero_track_type_get_data_fs (source);

	brasero_track_type_set_data_fs (source,
					fs_type|
					BRASERO_IMAGE_FS_JOLIET);

	result = brasero_burn_session_input_supported (session,
						       source,
						       FALSE);
	brasero_track_type_free (source);

	if (result == BRASERO_BURN_OK) {
		if (GTK_WIDGET_IS_SENSITIVE (priv->joliet_toggle)) {
			g_object_unref (session);
			return FALSE;
		}

		gtk_widget_set_sensitive (priv->joliet_toggle, TRUE);

		if (!priv->joliet_saved) {
			g_object_unref (session);
			return FALSE;
		}

		brasero_disc_option_dialog_set_tracks_image_fs (session, fs_type);

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->joliet_toggle), priv->joliet_saved);
		g_object_unref (session);
		return TRUE;
	}

	if (!GTK_WIDGET_IS_SENSITIVE (priv->joliet_toggle)) {
		g_object_unref (session);
		return FALSE;
	}

	priv->joliet_saved = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->joliet_toggle));
	if (priv->joliet_saved) {
		fs_type &= (~BRASERO_IMAGE_FS_JOLIET);
		brasero_disc_option_dialog_set_tracks_image_fs (session, fs_type);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->joliet_toggle), FALSE);
	}

	gtk_widget_set_sensitive (priv->joliet_toggle, FALSE);
	g_object_unref (session);

	return TRUE;
}

static void
brasero_disc_option_dialog_update_video (BraseroDiscOptionDialog *dialog)
{
	BraseroDiscOptionDialogPrivate *priv;
	BraseroBurnSession *session;
	BraseroMedia media;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	/* means we haven't initialized yet */
	if (!priv->vcd_label)
		return;

	session = brasero_burn_options_get_session (BRASERO_BURN_OPTIONS (dialog));
	media = brasero_burn_session_get_dest_media (session);

	if (media & BRASERO_MEDIUM_DVD) {
		brasero_disc_option_audio_AC3 (dialog);
		gtk_widget_hide (priv->vcd_label);
		gtk_widget_hide (priv->vcd_button);
		gtk_widget_hide (priv->svcd_button);

		gtk_widget_set_sensitive (priv->button_4_3, TRUE);
		gtk_widget_set_sensitive (priv->button_16_9, TRUE);
	}
	else if (media & BRASERO_MEDIUM_CD) {
		brasero_disc_option_audio_MP2 (dialog);
		gtk_widget_show (priv->vcd_label);
		gtk_widget_show (priv->vcd_button);
		gtk_widget_show (priv->svcd_button);

		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->vcd_button))) {
			gtk_widget_set_sensitive (priv->button_4_3, FALSE);
			gtk_widget_set_sensitive (priv->button_16_9, FALSE);
		}
		else {
			gtk_widget_set_sensitive (priv->button_4_3, TRUE);
			gtk_widget_set_sensitive (priv->button_16_9, TRUE);
		}
	}
	else if (media & BRASERO_MEDIUM_FILE) {
		BraseroImageFormat format;

		/* if we create a CUE file then that's a (S)VCD */
		format = brasero_burn_session_get_output_format (session);
		if (format == BRASERO_IMAGE_FORMAT_NONE) {
			g_object_unref (session);
			return;
		}

		if (format == BRASERO_IMAGE_FORMAT_CUE) {
			brasero_disc_option_audio_MP2 (dialog);
			gtk_widget_show (priv->vcd_label);
			gtk_widget_show (priv->vcd_button);
			gtk_widget_show (priv->svcd_button);

			if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->vcd_button))) {
				gtk_widget_set_sensitive (priv->button_4_3, FALSE);
				gtk_widget_set_sensitive (priv->button_16_9, FALSE);
			}
			else {
				gtk_widget_set_sensitive (priv->button_4_3, TRUE);
				gtk_widget_set_sensitive (priv->button_16_9, TRUE);
			}
		}
		else if (format == BRASERO_IMAGE_FORMAT_BIN) {
			brasero_disc_option_audio_AC3 (dialog);
			gtk_widget_hide (priv->vcd_label);
			gtk_widget_hide (priv->vcd_button);
			gtk_widget_hide (priv->svcd_button);

			gtk_widget_set_sensitive (priv->button_4_3, TRUE);
			gtk_widget_set_sensitive (priv->button_16_9, TRUE);
		}
	}

	g_object_unref (session);
}

/**
 * These functions are used to update the session according to the states
 * of the buttons and entry
 */

static void
brasero_disc_option_dialog_set_joliet (BraseroDiscOptionDialog *dialog)
{
	BraseroDiscOptionDialogPrivate *priv;
	BraseroTrackType *source = NULL;
	BraseroBurnSession *session;
	BraseroImageFS fs_type;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	if (!priv->joliet_toggle)
		return;

	session = brasero_burn_options_get_session (BRASERO_BURN_OPTIONS (dialog));

	/* NOTE: we don't check for the sensitive property since when
	 * something is compulsory the button is active but insensitive */
	source = brasero_track_type_new ();
	brasero_burn_session_get_input_type (session, source);
	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->joliet_toggle)))
		fs_type = (~BRASERO_IMAGE_FS_JOLIET) & brasero_track_type_get_data_fs (source);
	else
		fs_type = BRASERO_IMAGE_FS_JOLIET|brasero_track_type_get_data_fs (source);
	brasero_track_type_free (source);

	brasero_disc_option_dialog_set_tracks_image_fs (session, fs_type);
	g_object_unref (session);
}

static void
brasero_disc_option_dialog_joliet_toggled_cb (GtkToggleButton *toggle,
					      BraseroDiscOptionDialog *dialog)
{
	BraseroDiscOptionDialogPrivate *priv;
	GtkResponseType answer;
	GtkWidget *message;
	gchar *secondary;
	gboolean hide;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	if (!GTK_WIDGET_VISIBLE (dialog)) {
		gtk_widget_show (GTK_WIDGET (dialog));
		hide = TRUE;
	}

	if (priv->joliet_warning) {
		brasero_disc_option_dialog_set_joliet (dialog);
		return;
	}

	priv->joliet_warning = TRUE;

	message = gtk_message_dialog_new (GTK_WINDOW (dialog),
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
		brasero_disc_option_dialog_set_joliet (dialog);

	priv->joliet_warning = FALSE;
}

static gboolean
brasero_disc_option_dialog_joliet_widget (BraseroDiscOptionDialog *dialog)
{
	BraseroDiscOptionDialogPrivate *priv;
	BraseroTrackType *type = NULL;
	BraseroBurnSession *session;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	priv->joliet_toggle = gtk_check_button_new_with_mnemonic (_("Increase compatibility with _Windows systems"));
	gtk_widget_set_tooltip_text (priv->joliet_toggle,
				     _("Improve compatibility with Windows systems by allowing to display long filenames (maximum 64 characters)"));

	/* NOTE: we take for granted that if the source does not require
	 * to have the joliet extension, it's because it does have some
	 * incompatible filenames inside */
	session = brasero_burn_options_get_session (BRASERO_BURN_OPTIONS (dialog));

	type = brasero_track_type_new ();
	brasero_burn_session_get_input_type (session, type);
	if (brasero_track_type_get_data_fs (type) & BRASERO_IMAGE_FS_JOLIET) {
		priv->joliet_warning = 1;
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->joliet_toggle), TRUE);
	}
	brasero_track_type_free (type);

	brasero_disc_option_dialog_update_joliet (dialog);

	g_signal_connect (priv->joliet_toggle,
			  "toggled",
			  G_CALLBACK (brasero_disc_option_dialog_joliet_toggled_cb),
			  dialog);

	g_object_unref (session);
	return TRUE;
}

static void
brasero_disc_option_dialog_add_data_options (BraseroDiscOptionDialog *dialog)
{
	BraseroDiscOptionDialogPrivate *priv;
	GtkWidget *widget = NULL;
	GtkWidget *options;
	gchar *string;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	/* create the options box */
	widget = gtk_vbox_new (FALSE, 0);
	brasero_burn_options_add_options (BRASERO_BURN_OPTIONS (dialog), widget);

	/* general options */
	brasero_disc_option_dialog_joliet_widget (dialog);
	string = g_strdup_printf ("<b>%s</b>", _("Disc options"));
	options = brasero_utils_pack_properties (string,
						 priv->joliet_toggle,
						 NULL);
	g_free (string);

	gtk_box_pack_start (GTK_BOX (widget), options, FALSE, FALSE, 0);

	gtk_widget_show_all (widget);
}

static void
brasero_disc_option_dialog_set_tag (BraseroDiscOptionDialog *dialog,
				    const gchar *tag,
				    gint contents)
{
	BraseroBurnSession *session;
	GValue *value;

	session = brasero_burn_options_get_session (BRASERO_BURN_OPTIONS (dialog));

	value = g_new0 (GValue, 1);
	g_value_init (value, G_TYPE_INT);
	g_value_set_int (value, contents);
	brasero_burn_session_tag_add (session,
				      tag,
				      value);

	g_object_unref (session);
}

static void
brasero_disc_option_dialog_SVCD (GtkToggleButton *button,
				 BraseroDiscOptionDialog *dialog)
{
	BraseroDiscOptionDialogPrivate *priv;

	if (!gtk_toggle_button_get_active (button))
		return;

	brasero_disc_option_dialog_set_tag (dialog,
					    BRASERO_VCD_TYPE,
					    BRASERO_SVCD);

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	gtk_widget_set_sensitive (priv->button_4_3, TRUE);
	gtk_widget_set_sensitive (priv->button_16_9, TRUE);
}

static void
brasero_disc_option_dialog_VCD (GtkToggleButton *button,
				BraseroDiscOptionDialog *dialog)
{
	BraseroDiscOptionDialogPrivate *priv;

	if (!gtk_toggle_button_get_active (button))
		return;

	brasero_disc_option_dialog_set_tag (dialog,
					    BRASERO_VCD_TYPE,
					    BRASERO_VCD_V2);

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);
	gtk_widget_set_sensitive (priv->button_4_3, FALSE);
	gtk_widget_set_sensitive (priv->button_16_9, FALSE);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->button_4_3), TRUE);
}

static void
brasero_disc_option_dialog_NTSC (GtkToggleButton *button,
				 BraseroDiscOptionDialog *dialog)
{
	if (!gtk_toggle_button_get_active (button))
		return;

	brasero_disc_option_dialog_set_tag (dialog,
					    BRASERO_VIDEO_OUTPUT_FRAMERATE,
					    BRASERO_VIDEO_FRAMERATE_NTSC);
}

static void
brasero_disc_option_dialog_PAL_SECAM (GtkToggleButton *button,
				      BraseroDiscOptionDialog *dialog)
{
	if (!gtk_toggle_button_get_active (button))
		return;

	brasero_disc_option_dialog_set_tag (dialog,
					    BRASERO_VIDEO_OUTPUT_FRAMERATE,
					    BRASERO_VIDEO_FRAMERATE_PAL_SECAM);
}

static void
brasero_disc_option_dialog_native_framerate (GtkToggleButton *button,
					     BraseroDiscOptionDialog *dialog)
{
	BraseroBurnSession *session;

	if (!gtk_toggle_button_get_active (button))
		return;

	session = brasero_burn_options_get_session (BRASERO_BURN_OPTIONS (dialog));
	brasero_burn_session_tag_remove (session, BRASERO_VIDEO_OUTPUT_FRAMERATE);
	g_object_unref (session);
}

static void
brasero_disc_option_dialog_16_9 (GtkToggleButton *button,
				 BraseroDiscOptionDialog *dialog)
{
	if (!gtk_toggle_button_get_active (button))
		return;

	brasero_disc_option_dialog_set_tag (dialog,
					    BRASERO_VIDEO_OUTPUT_ASPECT,
					    BRASERO_VIDEO_ASPECT_16_9);
}

static void
brasero_disc_option_dialog_4_3 (GtkToggleButton *button,
				BraseroDiscOptionDialog *dialog)
{
	if (!gtk_toggle_button_get_active (button))
		return;

	brasero_disc_option_dialog_set_tag (dialog,
					    BRASERO_VIDEO_OUTPUT_ASPECT,
					    BRASERO_VIDEO_ASPECT_4_3);
}

static void
brasero_disc_option_dialog_add_video_options (BraseroDiscOptionDialog *dialog)
{
	gchar *string;
	GtkWidget *label;
	GtkWidget *table;
	GtkWidget *widget;
	GtkWidget *button1;
	GtkWidget *button2;
	GtkWidget *button3;
	GtkWidget *options;
	BraseroDiscOptionDialogPrivate *priv;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	widget = gtk_vbox_new (FALSE, 0);
	brasero_burn_options_add_options (BRASERO_BURN_OPTIONS (dialog), widget);

	table = gtk_table_new (3, 4, FALSE);
	gtk_table_set_col_spacings (GTK_TABLE (table), 8);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_widget_show (table);

	label = gtk_label_new (_("Video format:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table),
			  label,
			  0, 1,
			  0, 1,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);

	button1 = gtk_radio_button_new_with_mnemonic (NULL,
						      _("_NTSC"));
	gtk_widget_set_tooltip_text (button1, _("Format used mostly on the North American Continent"));
	g_signal_connect (button1,
			  "toggled",
			  G_CALLBACK (brasero_disc_option_dialog_NTSC),
			  dialog);
	gtk_table_attach (GTK_TABLE (table),
			  button1,
			  3, 4,
			  0, 1,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);

	button2 = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (button1),
								  _("_PAL/SECAM"));
	gtk_widget_set_tooltip_text (button2, _("Format used mostly in Europe"));
	g_signal_connect (button2,
			  "toggled",
			  G_CALLBACK (brasero_disc_option_dialog_PAL_SECAM),
			  dialog);
	gtk_table_attach (GTK_TABLE (table),
			  button2,
			  2, 3,
			  0, 1,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);

	button3 = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (button1),
								  _("Native _format"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button3), TRUE);
	g_signal_connect (button3,
			  "toggled",
			  G_CALLBACK (brasero_disc_option_dialog_native_framerate),
			  dialog);
	gtk_table_attach (GTK_TABLE (table),
			  button3,
			  1, 2,
			  0, 1,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);

	label = gtk_label_new (_("Aspect ratio:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table),
			  label,
			  0, 1,
			  1, 2,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);

	button1 = gtk_radio_button_new_with_mnemonic (NULL,
						      _("_4:3"));
	g_signal_connect (button1,
			  "toggled",
			  G_CALLBACK (brasero_disc_option_dialog_4_3),
			  dialog);
	gtk_table_attach (GTK_TABLE (table),
			  button1,
			  1, 2,
			  1, 2,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);
	priv->button_4_3 = button1;

	button2 = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (button1),
								  _("_16:9"));
	g_signal_connect (button2,
			  "toggled",
			  G_CALLBACK (brasero_disc_option_dialog_16_9),
			  dialog);
	gtk_table_attach (GTK_TABLE (table),
			  button2,
			  2, 3,
			  1, 2,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);
	priv->button_16_9 = button2;

	/* Video options for (S)VCD */
	label = gtk_label_new (_("VCD type:"));
	priv->vcd_label = label;

	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table),
			  label,
			  0, 1,
			  2, 3,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);

	button1 = gtk_radio_button_new_with_mnemonic_from_widget (NULL, _("Create a SVCD"));
	priv->svcd_button = button1;
	gtk_table_attach (GTK_TABLE (table),
			  button1,
			  1, 2,
			  2, 3,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);

	g_signal_connect (button1,
			  "clicked",
			  G_CALLBACK (brasero_disc_option_dialog_SVCD),
			  dialog);

	button2 = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (button1), _("Create a VCD"));
	priv->vcd_button = button2;
	gtk_table_attach (GTK_TABLE (table),
			  button2,
			  2, 3,
			  2, 3,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);

	g_signal_connect (button2,
			  "clicked",
			  G_CALLBACK (brasero_disc_option_dialog_VCD),
			  dialog);

	string = g_strdup_printf ("<b>%s</b>", _("Video Options"));
	options = brasero_utils_pack_properties (string,
						 table,
						 NULL);
	g_free (string);

	gtk_box_pack_start (GTK_BOX (widget), options, FALSE, FALSE, 0);

	/* NOTE: audio options for DVDs were removed. For SVCD that is MP2 and
	 * for Video DVD even if we have a choice AC3 is the most widespread
	 * audio format. So use AC3 by default. */

	gtk_widget_show_all (widget);
	brasero_disc_option_dialog_update_video (dialog);

	priv->video_options = widget;

	/* Just to make sure our tags are correct in BraseroBurnSession */
	brasero_disc_option_dialog_set_tag (dialog,
					    BRASERO_VCD_TYPE,
					    BRASERO_SVCD);
	brasero_disc_option_dialog_set_tag (dialog,
					    BRASERO_VIDEO_OUTPUT_ASPECT,
					    BRASERO_VIDEO_ASPECT_4_3);
}

static void
brasero_disc_option_dialog_valid_media_cb (BraseroSessionCfg *session,
					   BraseroDiscOptionDialog *self)
{
	BraseroDiscOptionDialogPrivate *priv;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (self);

	if (priv->video_options)
		gtk_widget_set_sensitive (priv->video_options, BRASERO_SESSION_IS_VALID (brasero_session_cfg_get_error (session)));

	/* update the joliet button */
	brasero_disc_option_dialog_update_joliet (self);

	/* for video disc see what's the output: CD or DVD */
	brasero_disc_option_dialog_update_video (self);
}

static void
brasero_disc_option_dialog_set_session (GObject *dialog,
					GParamSpec *pspec,
					gpointer NULL_data)
{
	BraseroDiscOptionDialogPrivate *priv;
	BraseroTrackType *type = NULL;
	BraseroBurnSession *session;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	session = brasero_burn_options_get_session (BRASERO_BURN_OPTIONS (dialog));

	type = brasero_track_type_new ();
	brasero_burn_session_get_input_type (session, type);

	if (brasero_track_type_get_has_data (type)) {
		brasero_burn_options_set_type_shown (BRASERO_BURN_OPTIONS (dialog),
						     BRASERO_MEDIA_TYPE_WRITABLE|
						     BRASERO_MEDIA_TYPE_FILE);
		brasero_disc_option_dialog_add_data_options (BRASERO_DISC_OPTION_DIALOG (dialog));
	}
	else if (brasero_track_type_get_has_stream (type)) {
		if (brasero_track_type_get_stream_format (type) & (BRASERO_VIDEO_FORMAT_UNDEFINED|BRASERO_VIDEO_FORMAT_VCD|BRASERO_VIDEO_FORMAT_VIDEO_DVD)) {
			brasero_burn_options_set_type_shown (BRASERO_BURN_OPTIONS (dialog),
							     BRASERO_MEDIA_TYPE_WRITABLE|
							     BRASERO_MEDIA_TYPE_FILE);
			brasero_disc_option_dialog_add_video_options (BRASERO_DISC_OPTION_DIALOG (dialog));
		}
		else {
			/* No other specific options for audio */
			brasero_burn_options_set_type_shown (BRASERO_BURN_OPTIONS (dialog),
							     BRASERO_MEDIA_TYPE_WRITABLE);
		}
	}
	brasero_track_type_free (type);

	/* see if we should lock the drive only with MERGE */
	if (brasero_burn_session_get_flags (session) & BRASERO_BURN_FLAG_MERGE)
		brasero_burn_options_lock_selection (BRASERO_BURN_OPTIONS (dialog));

	priv->valid_sig = g_signal_connect (session,
					    "is-valid",
					    G_CALLBACK (brasero_disc_option_dialog_valid_media_cb),
					    dialog);
	g_object_unref (session);
}

static void
brasero_disc_option_dialog_init (BraseroDiscOptionDialog *obj)
{
	BraseroDiscOptionDialogPrivate *priv;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (obj);

	gtk_window_set_title (GTK_WINDOW (obj), _("Disc Burning Setup"));
	g_signal_connect (obj,
			  "notify::session",
			  G_CALLBACK (brasero_disc_option_dialog_set_session),
			  NULL);
}

static void
brasero_disc_option_dialog_finalize (GObject *object)
{
	BraseroDiscOptionDialogPrivate *priv;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (object);

	if (priv->valid_sig) {
		BraseroBurnSession *session;

		session = brasero_burn_options_get_session (BRASERO_BURN_OPTIONS (object));
		g_signal_handler_disconnect (session, priv->valid_sig);
		g_object_unref (session);

		priv->valid_sig = 0;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_disc_option_dialog_class_init (BraseroDiscOptionDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroDiscOptionDialogPrivate));

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_disc_option_dialog_finalize;
}

