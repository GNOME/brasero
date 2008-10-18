/***************************************************************************
 *            brasero-disc-option-dialog.c
 *
 *  jeu sep 28 17:28:45 2006
 *  Copyright  2006  Philippe Rouquier
 *  bonfire-app@wanadoo.fr
 ***************************************************************************/

/*
 *  Brasero is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Brasero is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to:
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

#include <gtk/gtkdialog.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkbutton.h>

#include "burn-basics.h"
#include "burn-drive.h"
#include "burn-medium.h"
#include "burn-volume-obj.h"
#include "burn-session.h"
#include "burn-caps.h"
#include "burn-plugin-manager.h"
#include "brasero-disc-option-dialog.h"
#include "brasero-dest-selection.h"
#include "brasero-session-cfg.h"
#include "brasero-disc.h"
#include "brasero-utils.h"
#include "brasero-burn-options.h"


G_DEFINE_TYPE (BraseroDiscOptionDialog, brasero_disc_option_dialog, BRASERO_TYPE_BURN_OPTIONS);

struct _BraseroDiscOptionDialogPrivate {
	BraseroBurnCaps *caps;
	BraseroDisc *disc;

	GtkWidget *joliet_toggle;
	GtkWidget *multi_toggle;

	GtkWidget *video_options;
	GtkWidget *dvd_audio;
	GtkWidget *vcd_label;
	GtkWidget *vcd_button;
	GtkWidget *svcd_button;

	guint label_modified:1;
	guint joliet_warning:1;

	gulong valid_sig;

	guint checksum_saved:1;
	guint joliet_saved:1;
	guint multi_saved:1;
};
typedef struct _BraseroDiscOptionDialogPrivate BraseroDiscOptionDialogPrivate;

#define BRASERO_DISC_OPTION_DIALOG_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_DISC_OPTION_DIALOG, BraseroDiscOptionDialogPrivate))

static GtkDialogClass *parent_class = NULL;

static void
brasero_disc_option_dialog_load_multi_state (BraseroDiscOptionDialog *dialog)
{
	BraseroDiscOptionDialogPrivate *priv;
	BraseroBurnSession *session;
	gboolean value;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);
g_print ("KKLS\n");
	session = brasero_burn_options_get_session (BRASERO_BURN_OPTIONS (dialog));

	if (!brasero_session_cfg_is_supported (BRASERO_SESSION_CFG (session), BRASERO_BURN_FLAG_MULTI)) {
g_print ("RRKE\n");
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->multi_toggle), FALSE);
		gtk_widget_set_sensitive (priv->multi_toggle, FALSE);
		g_object_unref (session);
		return;
	}

	value = (brasero_burn_session_get_flags (session) & BRASERO_BURN_FLAG_MULTI) != 0;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->multi_toggle), value);

	if (!value) {
		g_object_unref (session);
		return;
	}
g_print ("jlskjd\n");
	/* set sensitivity */
	value = brasero_session_cfg_is_compulsory (BRASERO_SESSION_CFG (session),
						   BRASERO_BURN_FLAG_MULTI);
	gtk_widget_set_sensitive (priv->multi_toggle, value != TRUE);
	g_object_unref (session);
}

/**
 * These functions are used when caps-changed event or drive-changed event
 * are generated. They are used to check that flags or fs are valid.
 */

static gboolean
brasero_disc_option_dialog_update_joliet (BraseroDiscOptionDialog *dialog)
{
	BraseroTrackType source;
	BraseroBurnResult result;
	BraseroBurnSession *session;
	BraseroDiscOptionDialogPrivate *priv;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);
	if (!priv->joliet_toggle)
		return FALSE;

	/* what we want to check Joliet support */
	session = brasero_burn_options_get_session (BRASERO_BURN_OPTIONS (dialog));
	brasero_burn_session_get_input_type (session, &source);

	source.subtype.fs_type |= BRASERO_IMAGE_FS_JOLIET;
	result = brasero_burn_caps_is_input_supported (priv->caps,
						       session,
						       &source,
						       FALSE);
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

		source.subtype.fs_type |= BRASERO_IMAGE_FS_JOLIET;
		brasero_burn_session_set_input_type (session, &source);

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
		source.subtype.fs_type &= ~BRASERO_IMAGE_FS_JOLIET;
		brasero_burn_session_set_input_type (session, &source);

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->joliet_toggle), FALSE);
	}

	gtk_widget_set_sensitive (priv->joliet_toggle, FALSE);
	g_object_unref (session);

	return TRUE;
}

static void
brasero_disc_option_dialog_update_multi (BraseroDiscOptionDialog *dialog)
{
	BraseroTrackType input;
	BraseroBurnSession *session;
	BraseroDiscOptionDialogPrivate *priv;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	if (!priv->multi_toggle)
		return;

	session = brasero_burn_options_get_session (BRASERO_BURN_OPTIONS (dialog));
	brasero_burn_session_get_input_type (session, &input);
g_print ("REACHED\n");
	/* MULTI and Video projects don't get along */
	if (input.type == BRASERO_TRACK_TYPE_DATA
	&& (input.subtype.fs_type & BRASERO_IMAGE_FS_VIDEO)
	&& (brasero_burn_session_get_dest_media (session) & BRASERO_MEDIUM_DVD)) {
		brasero_session_cfg_remove_flags (BRASERO_SESSION_CFG (session), BRASERO_BURN_FLAG_MULTI);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->multi_toggle), FALSE);
		gtk_widget_set_sensitive (priv->multi_toggle, FALSE);
		g_object_unref (session);
		return;
	}

	brasero_disc_option_dialog_load_multi_state (dialog);
	g_object_unref (session);
}

static void
brasero_disc_option_dialog_update_video (BraseroDiscOptionDialog *dialog)
{
	BraseroDiscOptionDialogPrivate *priv;
	BraseroBurnSession *session;
	BraseroMedia media;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	session = brasero_burn_options_get_session (BRASERO_BURN_OPTIONS (dialog));
	media = brasero_burn_session_get_dest_media (session);

	if (media & BRASERO_MEDIUM_DVD) {
		gtk_widget_show (priv->dvd_audio);
		gtk_widget_hide (priv->vcd_label);
		gtk_widget_hide (priv->vcd_button);
		gtk_widget_hide (priv->svcd_button);
	}
	else if (media & BRASERO_MEDIUM_CD) {
		gtk_widget_hide (priv->dvd_audio);
		gtk_widget_show (priv->vcd_label);
		gtk_widget_show (priv->vcd_button);
		gtk_widget_show (priv->svcd_button);
	}
	else if (media & BRASERO_MEDIUM_FILE) {
		BraseroImageFormat format;

		/* if we create a CUE file then that's a SVCD */
		format = brasero_burn_session_get_output_format (session);
		if (format == BRASERO_IMAGE_FORMAT_NONE) {
			g_object_unref (session);
			return;
		}

		if (format == BRASERO_IMAGE_FORMAT_CUE) {
			gtk_widget_hide (priv->dvd_audio);
			gtk_widget_show (priv->vcd_label);
			gtk_widget_show (priv->vcd_button);
			gtk_widget_show (priv->svcd_button);
		}
		else if (format == BRASERO_IMAGE_FORMAT_BIN) {
			gtk_widget_show (priv->dvd_audio);
			gtk_widget_hide (priv->vcd_label);
			gtk_widget_hide (priv->vcd_button);
			gtk_widget_hide (priv->svcd_button);
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
	BraseroBurnSession *session;
	BraseroTrackType source;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	if (!priv->joliet_toggle)
		return;

	session = brasero_burn_options_get_session (BRASERO_BURN_OPTIONS (dialog));

	/* NOTE: we don't check for the sensitive property since when
	 * something is compulsory the button is active but insensitive */
	brasero_burn_session_get_input_type (session, &source);
	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->joliet_toggle)))
		source.subtype.fs_type &= ~BRASERO_IMAGE_FS_JOLIET;
	else
		source.subtype.fs_type |= BRASERO_IMAGE_FS_JOLIET;
	brasero_burn_session_set_input_type (session, &source);

	g_object_unref (session);
}

static void
brasero_disc_option_dialog_set_multi (BraseroDiscOptionDialog *dialog)
{
	BraseroDiscOptionDialogPrivate *priv;
	BraseroBurnSession *session;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	if (!priv->multi_toggle)
		return;

	session = brasero_burn_options_get_session (BRASERO_BURN_OPTIONS (dialog));

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->multi_toggle)))
		brasero_session_cfg_remove_flags (BRASERO_SESSION_CFG (session),
						  BRASERO_BURN_FLAG_MULTI);
	else
		brasero_session_cfg_add_flags (BRASERO_SESSION_CFG (session),
					       BRASERO_BURN_FLAG_MULTI);

	g_object_unref (session);
}

static void
brasero_disc_option_dialog_multi_toggled (GtkToggleButton *multi_toggle,
					  BraseroDiscOptionDialog *dialog)
{
	brasero_disc_option_dialog_set_multi (dialog);
}

static void
brasero_disc_option_dialog_joliet_toggled_cb (GtkToggleButton *toggle,
					      BraseroDiscOptionDialog *dialog)
{
	BraseroDiscOptionDialogPrivate *priv;
	GtkResponseType answer;
	GtkWidget *message;
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
					  _("Should files be renamed to be windows-compatible?"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
						  _("Some files don't have a suitable name for a Windows-compatible CD. Those names will be changed and truncated to 64 characters."));

	gtk_window_set_title (GTK_WINDOW (message), _("Windows Compatibility"));

	gtk_dialog_add_button (GTK_DIALOG (message),
			       _("_Don't rename"),
			       GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (message),
			       _("_Rename"),
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
	BraseroBurnSession *session;
	BraseroTrackType type;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	priv->joliet_toggle = gtk_check_button_new_with_mnemonic (_("Increase compatibility with _Windows systems"));
	gtk_widget_set_tooltip_text (priv->joliet_toggle,
				     _("Improve compatibility with Windows systems by allowing to display long filenames (maximum 64 characters)"));

	/* NOTE: we take for granted that if the source does not require
	 * to have the joliet extension, it's because it does have some
	 * incompatible filenames inside */
	session = brasero_burn_options_get_session (BRASERO_BURN_OPTIONS (dialog));
	brasero_burn_session_get_input_type (session, &type);
	if (type.subtype.fs_type & BRASERO_IMAGE_FS_JOLIET) {
		priv->joliet_warning = 1;
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->joliet_toggle), TRUE);
	}

	brasero_disc_option_dialog_update_joliet (dialog);

	g_signal_connect (priv->joliet_toggle,
			  "toggled",
			  G_CALLBACK (brasero_disc_option_dialog_joliet_toggled_cb),
			  dialog);

	g_object_unref (session);
	return TRUE;
}

static gboolean
brasero_disc_option_dialog_multi_widget (BraseroDiscOptionDialog *dialog)
{
	BraseroDiscOptionDialogPrivate *priv;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);
	priv->multi_toggle = gtk_check_button_new_with_mnemonic (_("Leave the disc _open to add other files later"));

	g_signal_connect (priv->multi_toggle,
			  "toggled",
			  G_CALLBACK (brasero_disc_option_dialog_multi_toggled),
			  dialog);
	gtk_widget_set_tooltip_text (priv->multi_toggle,
			      _("Allow to add more data to the disc later"));

	brasero_disc_option_dialog_update_multi (dialog);
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


	/* create the options */
	widget = gtk_vbox_new (FALSE, 0);
	brasero_burn_options_add_options (BRASERO_BURN_OPTIONS (dialog), widget);

	/* multisession options */
	brasero_disc_option_dialog_multi_widget (dialog);

	/* general options */
	brasero_disc_option_dialog_joliet_widget (dialog);

	string = g_strdup_printf ("<b>%s</b>", _("Disc options"));
	options = brasero_utils_pack_properties (string,
						 priv->multi_toggle,
						 priv->joliet_toggle,
						 NULL);
	g_free (string);

	gtk_box_pack_start (GTK_BOX (widget), options, FALSE, FALSE, 0);

	gtk_widget_show_all (widget);
}

static void
brasero_disc_option_dialog_add_audio_options (BraseroDiscOptionDialog *dialog)
{
	gchar *string;
	GtkWidget *widget;
	GtkWidget *options;
	BraseroDiscOptionDialogPrivate *priv;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	widget = gtk_vbox_new (FALSE, 0);
	brasero_burn_options_add_options (BRASERO_BURN_OPTIONS (dialog), widget);

	/* multisession options */
	priv->multi_toggle = gtk_check_button_new_with_mnemonic (_("Leave the disc _open to add a data session later"));
	g_signal_connect (priv->multi_toggle,
			  "toggled",
			  G_CALLBACK (brasero_disc_option_dialog_multi_toggled),
			  dialog);
	gtk_widget_set_tooltip_text (priv->multi_toggle,
				     _("Allow create what is called an enhanced CD or CD+"));

	string = g_strdup_printf ("<b>%s</b>", _("Disc options"));
	options = brasero_utils_pack_properties (string,
						 priv->multi_toggle,
						 NULL);
	g_free (string);

	gtk_box_pack_start (GTK_BOX (widget), options, FALSE, FALSE, 0);

	brasero_disc_option_dialog_update_multi (dialog);
	gtk_widget_show_all (widget);
}

static void
brasero_disc_option_dialog_AC3 (GtkToggleButton *button,
				BraseroDiscOptionDialog *dialog)
{
	BraseroBurnSession *session;
	BraseroAudioFormat format;
	GValue *value = NULL;

	session = brasero_burn_options_get_session (BRASERO_BURN_OPTIONS (dialog));

	brasero_burn_session_tag_lookup (session,
					 BRASERO_DVD_AUDIO_STREAMS,
					 &value);

	if (value)
		format = g_value_get_int (value);
	else
		format = BRASERO_AUDIO_FORMAT_NONE;

	if (gtk_toggle_button_get_active (button))
		format |= BRASERO_AUDIO_FORMAT_AC3;
	else
		format &= ~BRASERO_AUDIO_FORMAT_AC3;

	value = g_new0 (GValue, 1);
	g_value_init (value, G_TYPE_INT);
	g_value_set_int (value, format);
	brasero_burn_session_tag_add (session,
				      BRASERO_DVD_AUDIO_STREAMS,
				      value);

	g_object_unref (session);
}

static void
brasero_disc_option_dialog_MP2 (GtkToggleButton *button,
				BraseroDiscOptionDialog *dialog)
{
	BraseroBurnSession *session;
	BraseroAudioFormat format;
	GValue *value = NULL;

	session = brasero_burn_options_get_session (BRASERO_BURN_OPTIONS (dialog));

	brasero_burn_session_tag_lookup (session,
					 BRASERO_DVD_AUDIO_STREAMS,
					 &value);

	if (value)
		format = g_value_get_int (value);
	else
		format = BRASERO_AUDIO_FORMAT_NONE;

	if (gtk_toggle_button_get_active (button))
		format |= BRASERO_AUDIO_FORMAT_MP2;
	else
		format &= ~BRASERO_AUDIO_FORMAT_MP2;

	value = g_new0 (GValue, 1);
	g_value_init (value, G_TYPE_INT);
	g_value_set_int (value, format);
	brasero_burn_session_tag_add (session,
				      BRASERO_DVD_AUDIO_STREAMS,
				      value);

	g_object_unref (session);
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
	if (!gtk_toggle_button_get_active (button))
		return;

	brasero_disc_option_dialog_set_tag (dialog,
					    BRASERO_VCD_TYPE,
					    BRASERO_SVCD);
}

static void
brasero_disc_option_dialog_VCD (GtkToggleButton *button,
				BraseroDiscOptionDialog *dialog)
{
	if (!gtk_toggle_button_get_active (button))
		return;

	brasero_disc_option_dialog_set_tag (dialog,
					    BRASERO_VCD_TYPE,
					    BRASERO_VCD_V2);
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
brasero_disc_option_dialog_native_aspect (GtkToggleButton *button,
					  BraseroDiscOptionDialog *dialog)
{
	BraseroBurnSession *session;

	if (!gtk_toggle_button_get_active (button))
		return;

	session = brasero_burn_options_get_session (BRASERO_BURN_OPTIONS (dialog));
	brasero_burn_session_tag_remove (session, BRASERO_VIDEO_OUTPUT_ASPECT);
	g_object_unref (session);
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
			  3, 4,
			  1, 2,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);

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

	button3 = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (button1),
								  _("Native aspect _ratio"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button3), TRUE);
	g_signal_connect (button3,
			  "toggled",
			  G_CALLBACK (brasero_disc_option_dialog_native_aspect),
			  dialog);
	gtk_table_attach (GTK_TABLE (table),
			  button3,
			  1, 2,
			  1, 2,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);

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

	/* Audio options for DVDs */
	button1 = gtk_check_button_new_with_mnemonic (_("Add _AC3 audio stream"));
	button2 = gtk_check_button_new_with_mnemonic (_("Add _MP2 audio stream"));

	string = g_strdup_printf ("<b>%s</b>", _("Audio Options"));
	options = brasero_utils_pack_properties (string,
						 button1,
						 button2,
						 NULL);
	g_free (string);

	g_signal_connect (button1,
			  "clicked",
			  G_CALLBACK (brasero_disc_option_dialog_AC3),
			  dialog);
	g_signal_connect (button2,
			  "clicked",
			  G_CALLBACK (brasero_disc_option_dialog_MP2),
			  dialog);

	gtk_box_pack_start (GTK_BOX (widget), options, FALSE, FALSE, 0);
	priv->dvd_audio = options;

	gtk_widget_show_all (widget);
	brasero_disc_option_dialog_update_video (dialog);

	priv->video_options = widget;
}

void
brasero_disc_option_dialog_set_disc (BraseroDiscOptionDialog *dialog,
				     BraseroDisc *disc)
{
	BraseroDiscOptionDialogPrivate *priv;
	BraseroBurnSession *session;
	BraseroTrackType type;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	if (priv->disc)
		g_object_unref (priv->disc);

	priv->disc = disc;
	g_object_ref (disc);

	session = brasero_burn_options_get_session (BRASERO_BURN_OPTIONS (dialog));
	brasero_disc_set_session_param (disc, session);

	brasero_burn_session_get_input_type (session, &type);
	if (type.type == BRASERO_TRACK_TYPE_DATA) {
		brasero_burn_options_set_type_shown (BRASERO_BURN_OPTIONS (dialog),
						     BRASERO_MEDIA_TYPE_WRITABLE|
						     BRASERO_MEDIA_TYPE_FILE);
		brasero_disc_option_dialog_add_data_options (dialog);
	}
	else if (type.type == BRASERO_TRACK_TYPE_AUDIO) {
		if (type.subtype.audio_format & (BRASERO_VIDEO_FORMAT_UNDEFINED|BRASERO_VIDEO_FORMAT_VCD|BRASERO_VIDEO_FORMAT_VIDEO_DVD)) {
			brasero_burn_options_set_type_shown (BRASERO_BURN_OPTIONS (dialog),
							     BRASERO_MEDIA_TYPE_WRITABLE|
							     BRASERO_MEDIA_TYPE_FILE);
			brasero_disc_option_dialog_add_video_options (dialog);
		}
		else {
			brasero_burn_options_set_type_shown (BRASERO_BURN_OPTIONS (dialog),
							     BRASERO_MEDIA_TYPE_WRITABLE);
			brasero_disc_option_dialog_add_audio_options (dialog);
		}
	}

	/* see if we should lock the drive only with MERGE */
	if (brasero_burn_session_get_flags (session) & BRASERO_BURN_FLAG_MERGE)
		brasero_burn_options_lock_selection (BRASERO_BURN_OPTIONS (dialog));

	g_object_unref (session);
}

BraseroBurnSession *
brasero_disc_option_dialog_get_session (BraseroDiscOptionDialog *self)
{
	BraseroBurnSession *session;
	BraseroDiscOptionDialogPrivate *priv;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (self);

	session = brasero_burn_options_get_session (BRASERO_BURN_OPTIONS (self));
	brasero_disc_set_session_contents (priv->disc, session);
	return session;
}

static void
brasero_disc_option_dialog_valid_media_cb (BraseroSessionCfg *session,
					   BraseroDiscOptionDialog *self)
{
	BraseroDiscOptionDialogPrivate *priv;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (self);

	if (priv->video_options)
		gtk_widget_set_sensitive (priv->video_options, brasero_session_cfg_get_error (session) == BRASERO_SESSION_VALID);

	/* update the joliet button */
	brasero_disc_option_dialog_update_joliet (self);

	/* for video disc see what's the output : CD or DVD */
	if (priv->dvd_audio)
		brasero_disc_option_dialog_update_video (self);

	/* flags could have changed so make sure multi gets updated */
	brasero_disc_option_dialog_update_multi (self);
}

static void
brasero_disc_option_dialog_init (BraseroDiscOptionDialog *obj)
{
	BraseroDiscOptionDialogPrivate *priv;
	BraseroBurnSession *session;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (obj);

	priv->caps = brasero_burn_caps_get_default ();

	session = brasero_burn_options_get_session (BRASERO_BURN_OPTIONS (obj));
	priv->valid_sig = g_signal_connect (session,
					    "is-valid",
					    G_CALLBACK (brasero_disc_option_dialog_valid_media_cb),
					    obj);
	g_object_unref (session);
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

	if (priv->caps) {
		g_object_unref (priv->caps);
		priv->caps = NULL;
	}

	if (priv->disc) {
		g_object_unref (priv->disc);
		priv->disc = NULL;
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

GtkWidget *
brasero_disc_option_dialog_new ()
{
	BraseroDiscOptionDialog *obj;

	obj = BRASERO_DISC_OPTION_DIALOG (g_object_new (BRASERO_TYPE_DISC_OPTION_DIALOG,
							"title", _("Disc Burning Setup"),
							NULL));

	return GTK_WIDGET (obj);
}
