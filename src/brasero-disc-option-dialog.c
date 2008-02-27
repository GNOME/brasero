/***************************************************************************
 *            brasero-disc-option-dialog.c
 *
 *  jeu sep 28 17:28:45 2006
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

#include <gconf/gconf-client.h>

#include "brasero-utils.h"
#include "burn-basics.h"
#include "burn-medium.h"
#include "burn-session.h"
#include "burn-caps.h"
#include "burn-plugin-manager.h"
#include "brasero-disc-option-dialog.h"
#include "brasero-dest-selection.h"
#include "brasero-ncb.h"
#include "brasero-disc.h"

G_DEFINE_TYPE (BraseroDiscOptionDialog, brasero_disc_option_dialog, GTK_TYPE_DIALOG);

struct _BraseroDiscOptionDialogPrivate {
	BraseroBurnSession *session;

	BraseroBurnCaps *caps;
	BraseroDisc *disc;

	guint caps_sig;
	guint output_sig;

	GtkWidget *video_toggle;
	GtkWidget *joliet_toggle;
	GtkWidget *multi_toggle;

	GtkWidget *selection;
	GtkWidget *label;

	GtkWidget *button;

	guint label_modified:1;
	guint joliet_warning:1;

	guint checksum_saved:1;
	guint joliet_saved:1;
	guint multi_saved:1;
	guint video_saved:1;
};
typedef struct _BraseroDiscOptionDialogPrivate BraseroDiscOptionDialogPrivate;

#define BRASERO_DISC_OPTION_DIALOG_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_DISC_OPTION_DIALOG, BraseroDiscOptionDialogPrivate))

static GtkDialogClass *parent_class = NULL;

static void
brasero_disc_option_dialog_save_multi_state (BraseroDiscOptionDialog *dialog)
{
	BraseroDiscOptionDialogPrivate *priv;
	GConfClient *client;
	gboolean multi_on;
	gchar *key;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	key = brasero_burn_session_get_config_key (priv->session, "multi");
	if (!key)
		return;

	multi_on = (brasero_burn_session_get_flags (priv->session) & BRASERO_BURN_FLAG_MULTI) != 0;

	client = gconf_client_get_default ();
	gconf_client_set_int (client, key, multi_on, NULL);
	g_object_unref (client);
	g_free (key);
}

static void
brasero_disc_option_dialog_load_multi_state (BraseroDiscOptionDialog *dialog)
{
	BraseroDiscOptionDialogPrivate *priv;
	GConfClient *client;
	gboolean multi_on;
	gchar *key;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	/* That's only provided multi is not compulsory or unsupported */

	key = brasero_burn_session_get_config_key (priv->session, "multi");
	client = gconf_client_get_default ();
	multi_on = gconf_client_get_int (client, key, NULL);
	g_object_unref (client);
	g_free (key);

	/* NOTE: no need to take care of adding/removing MULTI flag to session,
	 * the callback for the button will do it on its own. */
	if (multi_on)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->multi_toggle), TRUE);
	else
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->multi_toggle), FALSE);
}

static gchar *
brasero_disc_option_dialog_get_default_label (BraseroDiscOptionDialog *dialog)
{
	time_t t;
	gchar buffer [128];
	gchar *title_str = NULL;
	BraseroTrackType source;
	BraseroMedia media;
	NautilusBurnDrive *drive;
	BraseroDiscOptionDialogPrivate *priv;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	brasero_burn_session_get_input_type (priv->session, &source);

	drive = brasero_drive_selection_get_drive (BRASERO_DRIVE_SELECTION (priv->selection));
	media = NCB_MEDIA_GET_STATUS (drive);

	t = time (NULL);
	strftime (buffer, sizeof (buffer), "%d %b %y", localtime (&t));

	if (source.type == BRASERO_TRACK_TYPE_DATA) {
		if ((media & BRASERO_MEDIUM_APPENDABLE)
		&&  !brasero_burn_session_is_dest_file (priv->session))
			title_str = nautilus_burn_drive_get_media_label (drive);

		if (!title_str || title_str [0] == '\0')
			title_str = g_strdup_printf (_("Data disc (%s)"), buffer);
	}
	else if (source.type == BRASERO_TRACK_TYPE_AUDIO)
		title_str = g_strdup_printf (_("Audio disc (%s)"), buffer);

	nautilus_burn_drive_unref (drive);
	return title_str;
}

static void
brasero_disc_option_dialog_set_label (BraseroDiscOptionDialog *dialog)
{
	const gchar *label;
	BraseroDiscOptionDialogPrivate *priv;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	label = gtk_entry_get_text (GTK_ENTRY (priv->label));
	brasero_burn_session_set_label (priv->session, label);
}

static gboolean
brasero_disc_option_dialog_update_label (BraseroDiscOptionDialog *dialog)
{
	gchar *label;
	BraseroDiscOptionDialogPrivate *priv;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	if (!priv->label)
		return FALSE;

	label = brasero_disc_option_dialog_get_default_label (dialog);
	gtk_entry_set_text (GTK_ENTRY (priv->label), label);
	g_free (label);

	brasero_disc_option_dialog_set_label (dialog);
	return TRUE;
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
	BraseroDiscOptionDialogPrivate *priv;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);
	if (!priv->joliet_toggle)
		return FALSE;

	/* what we want to check is Joliet support */
	brasero_burn_session_get_input_type (priv->session, &source);

	source.subtype.fs_type |= BRASERO_IMAGE_FS_JOLIET;
	result = brasero_burn_caps_is_input_supported (priv->caps,
						       priv->session,
						       &source);
	if (result == BRASERO_BURN_OK) {
		if (GTK_WIDGET_IS_SENSITIVE (priv->joliet_toggle))
			return FALSE;

		gtk_widget_set_sensitive (priv->joliet_toggle, TRUE);

		if (!priv->joliet_saved)
			return FALSE;

		source.subtype.fs_type |= BRASERO_IMAGE_FS_JOLIET;
		brasero_burn_session_set_input_type (priv->session, &source);

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->joliet_toggle), priv->joliet_saved);
		return TRUE;
	}

	if (!GTK_WIDGET_IS_SENSITIVE (priv->joliet_toggle))
		return FALSE;

	priv->joliet_saved = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->joliet_toggle));
	if (priv->joliet_saved) {
		source.subtype.fs_type &= ~BRASERO_IMAGE_FS_JOLIET;
		brasero_burn_session_set_input_type (priv->session, &source);

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->joliet_toggle), FALSE);
	}

	gtk_widget_set_sensitive (priv->joliet_toggle, FALSE);
	return TRUE;
}

static gboolean
brasero_disc_option_dialog_update_video (BraseroDiscOptionDialog *dialog)
{
	BraseroDiscOptionDialogPrivate *priv;
	BraseroBurnResult result;
	BraseroTrackType source;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	if (!priv->video_toggle)
		return FALSE;

	/* the library must have the proper support and multi mustn't be on */
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->multi_toggle)))
		goto turn_off;

	brasero_burn_session_get_input_type (priv->session, &source);
	source.subtype.fs_type |= BRASERO_IMAGE_FS_VIDEO;
	result = brasero_burn_caps_is_input_supported (priv->caps,
						       priv->session,
						       &source);

	if (result != BRASERO_BURN_OK)
		goto turn_off;

	if (GTK_WIDGET_IS_SENSITIVE (priv->video_toggle))
		return FALSE;

	gtk_widget_set_sensitive (priv->video_toggle, TRUE);

	if (!priv->video_saved)
		return TRUE;

	brasero_burn_session_set_input_type (priv->session, &source);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->video_toggle), TRUE);

	/* multi and video shouldn't be on at the same time
	 * NOTE: in this case multi_toggle cannot be on since we checked
	 * its state earlier on */
	priv->multi_saved = FALSE;
	gtk_widget_set_sensitive (priv->multi_toggle, FALSE);

	return TRUE;

turn_off:

	if (!GTK_WIDGET_IS_SENSITIVE (priv->video_toggle))
		return FALSE;

	priv->video_saved = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->video_toggle));
	if (priv->video_saved) {
		source.subtype.fs_type &= ~BRASERO_IMAGE_FS_VIDEO;
		brasero_burn_session_set_input_type (priv->session, &source);

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->video_toggle), FALSE);
	}

	gtk_widget_set_sensitive (priv->video_toggle, FALSE);
	return TRUE;
}

static void
brasero_disc_option_dialog_update_multi (BraseroDiscOptionDialog *dialog)
{
	BraseroDiscOptionDialogPrivate *priv;
	BraseroBurnFlag supported = BRASERO_BURN_FLAG_NONE;
	BraseroBurnFlag compulsory = BRASERO_BURN_FLAG_NONE;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	if (!priv->multi_toggle)
		return;

	/* Wipe out some flags before trying to see if MULTI is supported:
	 * DAO/BLANK_BEFORE_WRITE don't really get along well with MULTI */
	brasero_burn_session_remove_flag (priv->session,
					  BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE|
					  BRASERO_BURN_FLAG_DAO);

	/* see if multi disc option is supported or compulsory. The returned
	 * value just indicate if the button state can be modified. */
	brasero_burn_caps_get_flags (priv->caps,
				     priv->session,
				     &supported,
				     &compulsory);

	if (!(supported & BRASERO_BURN_FLAG_MULTI)) {
		/* just in case it was already set */
		brasero_burn_session_remove_flag (priv->session, BRASERO_BURN_FLAG_MULTI);

		gtk_widget_set_sensitive (priv->multi_toggle, FALSE);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->multi_toggle), FALSE);
		goto end;
	}

	if (compulsory & BRASERO_BURN_FLAG_MULTI) {
		/* NOTE: in this case video button is updated later see caps_changed and media_changed */
		brasero_burn_session_add_flag (priv->session, BRASERO_BURN_FLAG_MULTI);

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->multi_toggle), TRUE);
		gtk_widget_set_sensitive (priv->multi_toggle, FALSE);
		goto end;
	}

	/* to improve video DVD compatibility we don't allow to leave a disc
	 * open and have a video DVD created */
	if (priv->video_toggle
	&&  gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->video_toggle))) {
		if (GTK_WIDGET_IS_SENSITIVE (priv->multi_toggle))
			priv->multi_saved = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->multi_toggle));

		brasero_burn_session_remove_flag (priv->session, BRASERO_BURN_FLAG_MULTI);

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->multi_toggle), FALSE);
		gtk_widget_set_sensitive (priv->multi_toggle, FALSE);
		goto end;
	}

	/* only load preferences if it is supported and not compulsory */
	gtk_widget_set_sensitive (priv->multi_toggle, TRUE);
	brasero_disc_option_dialog_load_multi_state (dialog);

end:
	/* Try to see if previously wiped out flags can be re-enabled now */
	brasero_burn_caps_get_flags (priv->caps,
				     priv->session,
				     &supported,
				     &compulsory);

	/* we need to do that to override the flags that may be set in
	 * brasero-dest-selection.c. The following doesn't like MULTI. */
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

	/* Likewise DAO and MULTI don't always get along well but use DAO
	 * whenever it's possible */
	if (supported & BRASERO_BURN_FLAG_DAO)
		brasero_burn_session_add_flag (priv->session, BRASERO_BURN_FLAG_DAO);
}

static void
brasero_disc_option_dialog_caps_changed (BraseroPluginManager *manager,
					 BraseroDiscOptionDialog *dialog)
{
	/* update the multi button:
	 * NOTE: order is important here multi then video */
	brasero_disc_option_dialog_update_multi (dialog);
	/* update the joliet button */
	brasero_disc_option_dialog_update_joliet (dialog);
	/* update the video button */
	brasero_disc_option_dialog_update_video (dialog);
}

static void
brasero_disc_option_dialog_output_changed (BraseroBurnSession *session,
					   BraseroDiscOptionDialog *dialog)
{
	BraseroDiscOptionDialogPrivate *priv;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	/* update the multi button:
	 * NOTE: order is important here multi then video */
	brasero_disc_option_dialog_update_multi (dialog);
	/* update the joliet button */
	brasero_disc_option_dialog_update_joliet (dialog);
	/* update the video button */
	brasero_disc_option_dialog_update_video (dialog);

	/* see if we need to update the label */
	if (!priv->label_modified)
		brasero_disc_option_dialog_update_label (dialog);
}

/**
 * These functions are used to update the session according to the states
 * of the buttons and entry 
 */

static void
brasero_disc_option_dialog_set_video (BraseroDiscOptionDialog *dialog)
{
	BraseroDiscOptionDialogPrivate *priv;
	BraseroTrackType source;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	if (!priv->video_toggle)
		return;

	/* NOTE: we don't check for the sensitive property since when
	 * something is compulsory the button is active but insensitive
	 */
	brasero_burn_session_get_input_type (priv->session, &source);
	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->video_toggle))) {
		source.subtype.fs_type &= ~BRASERO_IMAGE_FS_VIDEO;

		brasero_disc_option_dialog_update_multi (dialog);
	}
	else {
		source.subtype.fs_type |= BRASERO_IMAGE_FS_VIDEO;

		priv->multi_saved = FALSE;
		gtk_widget_set_sensitive (priv->multi_toggle, FALSE);
	}

	brasero_burn_session_set_input_type (priv->session, &source);
}

static void
brasero_disc_option_dialog_set_joliet (BraseroDiscOptionDialog *dialog)
{
	BraseroDiscOptionDialogPrivate *priv;
	BraseroTrackType source;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	if (!priv->joliet_toggle)
		return;

	/* NOTE: we don't check for the sensitive property since when
	 * something is compulsory the button is active but insensitive */
	brasero_burn_session_get_input_type (priv->session, &source);
	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->joliet_toggle)))
		source.subtype.fs_type &= ~BRASERO_IMAGE_FS_JOLIET;
	else
		source.subtype.fs_type |= BRASERO_IMAGE_FS_JOLIET;
	brasero_burn_session_set_input_type (priv->session, &source);
}

static void
brasero_disc_option_dialog_set_multi (BraseroDiscOptionDialog *dialog)
{
	BraseroDiscOptionDialogPrivate *priv;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	if (!priv->multi_toggle)
		return;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->multi_toggle))) {
		brasero_burn_session_remove_flag (priv->session, BRASERO_BURN_FLAG_MULTI);
		brasero_disc_option_dialog_update_video (dialog);
		brasero_disc_option_dialog_save_multi_state (dialog);
		return;
	}

	brasero_burn_session_add_flag (priv->session, BRASERO_BURN_FLAG_MULTI);
	brasero_disc_option_dialog_save_multi_state (dialog);

	if (!priv->video_toggle)
		return;

	/* to improve video DVD compatibility we don't allow to leave a disc
	 * open and have a video DVD created.
	 * NOTE: video and multi buttons are antithetic so if the user pressed
	 * this one that means that video wasn't active so no need to set video
	 * to FALSE*/
	priv->video_saved = FALSE;
	gtk_widget_set_sensitive (priv->video_toggle, FALSE);
}

static void
brasero_disc_option_dialog_multi_toggled (GtkToggleButton *multi_toggle,
					  BraseroDiscOptionDialog *dialog)
{
	brasero_disc_option_dialog_set_multi (dialog);
}

static void
brasero_disc_option_dialog_video_toggled (GtkToggleButton *video_toggle,
					  BraseroDiscOptionDialog *dialog)
{
	brasero_disc_option_dialog_set_video (dialog);
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

	brasero_disc_option_dialog_set_joliet (dialog);
	if (!GTK_WIDGET_VISIBLE (dialog)) {
		gtk_widget_show (GTK_WIDGET (dialog));
		hide = TRUE;
	}

	if (priv->joliet_warning)
		return;

	message = gtk_message_dialog_new (GTK_WINDOW (dialog),
					  GTK_DIALOG_DESTROY_WITH_PARENT|
					  GTK_DIALOG_MODAL,
					  GTK_MESSAGE_INFO,
					  GTK_BUTTONS_CLOSE,
					  _("Some files don't have a suitable name for a Windows-compatible CD:"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
						  _("their names will be changed and truncated to 64 characters."));

	gtk_window_set_title (GTK_WINDOW (message), _("Windows compatibility"));
	answer = gtk_dialog_run (GTK_DIALOG (message));
	gtk_widget_destroy (message);
}

static void
brasero_disc_option_label_changed (GtkEditable *editable,
				   BraseroDiscOptionDialog *dialog)
{
	BraseroDiscOptionDialogPrivate *priv;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	brasero_disc_option_dialog_set_label (dialog);
	priv->label_modified = 1;
}

static void
brasero_disc_option_dialog_title_widget (BraseroDiscOptionDialog *dialog)
{
	BraseroDiscOptionDialogPrivate *priv;
	BraseroTrackType type;
	gchar *title_str = NULL;
	gchar *label = NULL;
	GtkWidget *widget;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	if (!priv->label) {
		priv->label = gtk_entry_new ();
		gtk_entry_set_max_length (GTK_ENTRY (priv->label), 32);
	}

	priv->label_modified = 0;
	g_signal_connect (priv->label,
			  "changed",
			  G_CALLBACK (brasero_disc_option_label_changed),
			  dialog);

	title_str = brasero_disc_option_dialog_get_default_label (dialog);
	gtk_entry_set_text (GTK_ENTRY (priv->label), title_str);
	g_free (title_str);

	brasero_disc_option_dialog_set_label (dialog);

	brasero_burn_session_get_input_type (priv->session, &type);
	if (type.type == BRASERO_TRACK_TYPE_DATA)
		label = g_strdup (_("<b>Label of the disc</b>"));
	else if (type.type == BRASERO_TRACK_TYPE_AUDIO)
		label = g_strdup (_("<b>Title</b>"));

	widget = brasero_utils_pack_properties (label, priv->label, NULL);
	g_free (label);

	gtk_widget_show_all (widget);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    widget,
			    FALSE,
			    FALSE,
			    0);
}

static gboolean
brasero_disc_option_dialog_video_widget (BraseroDiscOptionDialog *dialog)
{
	BraseroDiscOptionDialogPrivate *priv;
	BraseroTrackType type;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	/* Two conditions to create this widget:
	 * the track must have the necessary files
	 * the library must have the proper plugin */
	brasero_burn_session_get_input_type (priv->session, &type);
	if (!(type.type & BRASERO_IMAGE_FS_VIDEO))
		return FALSE;

	priv->video_toggle = gtk_check_button_new_with_mnemonic (_("Create a vid_eo DVD"));
	g_signal_connect (priv->video_toggle,
			  "toggled",
			  G_CALLBACK (brasero_disc_option_dialog_video_toggled),
			  dialog);
	gtk_widget_set_tooltip_text (priv->video_toggle,
			      _("Create a video DVD that can be played by all DVD readers"));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->video_toggle), TRUE);
	brasero_disc_option_dialog_update_video (dialog);

	return TRUE;
}

static gboolean
brasero_disc_option_dialog_joliet_widget (BraseroDiscOptionDialog *dialog)
{
	BraseroDiscOptionDialogPrivate *priv;
	BraseroTrackType type;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	priv->joliet_toggle = gtk_check_button_new_with_mnemonic (_("Increase compatibility with _Windows systems"));
	gtk_widget_set_tooltip_text (priv->joliet_toggle,
				     _("Improve compatibility with Windows systems by allowing to display long filenames (maximum 64 characters)"));

	/* NOTE: we take for granted that if the source does not require
	 * to have the joliet extension, it's because it does have some
	 * incompatible filenames inside */
	brasero_burn_session_get_input_type (priv->session, &type);
	if (type.subtype.fs_type & BRASERO_IMAGE_FS_JOLIET) {
		priv->joliet_warning = 1;
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->joliet_toggle), TRUE);
	}

	brasero_disc_option_dialog_update_joliet (dialog);

	g_signal_connect (priv->joliet_toggle,
			  "toggled",
			  G_CALLBACK (brasero_disc_option_dialog_joliet_toggled_cb),
			  dialog);
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
	BraseroTrackType source;
	GtkWidget *options;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	brasero_burn_session_get_input_type (priv->session, &source);

	/* create the options */
	widget = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_end (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			  widget,
			  TRUE,
			  FALSE,
			  6);

	/* multisession options */
	brasero_disc_option_dialog_multi_widget (dialog);

	/* general options */
	brasero_disc_option_dialog_joliet_widget (dialog);

	/* video toggle */
	brasero_disc_option_dialog_video_widget (dialog);

	options = brasero_utils_pack_properties (_("<b>Disc options</b>"),
						 priv->multi_toggle,
						 priv->joliet_toggle,
						 priv->video_toggle,
						 NULL);
	gtk_box_pack_start (GTK_BOX (widget), options, FALSE, FALSE, 0);

	gtk_widget_show_all (widget);
}

static void
brasero_disc_option_dialog_add_audio_options (BraseroDiscOptionDialog *dialog)
{
	GtkWidget *widget;
	GtkWidget *options;
	BraseroDiscOptionDialogPrivate *priv;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	widget = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_end (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			  widget,
			  TRUE,
			  FALSE,
			  6);

	/* multisession options */
	priv->multi_toggle = gtk_check_button_new_with_mnemonic (_("Leave the disc _open to add a data session later"));
	g_signal_connect (priv->multi_toggle,
			  "toggled",
			  G_CALLBACK (brasero_disc_option_dialog_multi_toggled),
			  dialog);
	gtk_widget_set_tooltip_text (priv->multi_toggle,
			      _("Allow create what is called an enhanced CD or CD+"));

	options = brasero_utils_pack_properties (_("<b>Disc options</b>"),
						 priv->multi_toggle,
						 NULL);
	gtk_box_pack_start (GTK_BOX (widget), options, FALSE, FALSE, 0);

	brasero_disc_option_dialog_update_multi (dialog);
	gtk_widget_show_all (widget);
}

void
brasero_disc_option_dialog_set_disc (BraseroDiscOptionDialog *dialog,
				     BraseroDisc *disc)
{
	BraseroDiscOptionDialogPrivate *priv;
	BraseroTrackType type;
	gboolean lock_drive;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	if (priv->disc)
		g_object_unref (priv->disc);

	priv->disc = disc;
	g_object_ref (disc);

	if (priv->output_sig) {
		g_signal_handler_disconnect (priv->session, priv->output_sig);
		priv->output_sig = 0;
	}

	brasero_disc_set_session_param (disc, priv->session);

	/* see if we should lock the drive */
	lock_drive = (brasero_burn_session_get_flags (priv->session) & (BRASERO_BURN_FLAG_APPEND|
									BRASERO_BURN_FLAG_MERGE)) != 0;
	brasero_drive_selection_lock (BRASERO_DRIVE_SELECTION (priv->selection), lock_drive);

	priv->output_sig = g_signal_connect (priv->session,
					     "output-changed",
					     G_CALLBACK (brasero_disc_option_dialog_output_changed),
					     dialog);

	/* NOTE: the caller must have ensured the disc is ready */
	brasero_disc_option_dialog_title_widget (dialog);

	brasero_burn_session_get_input_type (priv->session, &type);
	if (type.type == BRASERO_TRACK_TYPE_DATA) {
	brasero_drive_selection_set_type_shown (BRASERO_DRIVE_SELECTION (priv->selection),
						BRASERO_MEDIA_TYPE_WRITABLE|
						BRASERO_MEDIA_TYPE_FILE);
		brasero_disc_option_dialog_add_data_options (dialog);
	}
	else if (type.type == BRASERO_TRACK_TYPE_AUDIO) {
		brasero_drive_selection_set_type_shown (BRASERO_DRIVE_SELECTION (priv->selection),
							BRASERO_MEDIA_TYPE_WRITABLE);
		brasero_disc_option_dialog_add_audio_options (dialog);
	}
}

static void
brasero_disc_option_dialog_valid_media_cb (BraseroDestSelection *selection,
					   gboolean valid,
					   BraseroDiscOptionDialog *self)
{
	BraseroDiscOptionDialogPrivate *priv;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (self);
	gtk_widget_set_sensitive (priv->button, valid);
}

BraseroBurnSession *
brasero_disc_option_dialog_get_session (BraseroDiscOptionDialog *dialog)
{
	BraseroDiscOptionDialogPrivate *priv;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	brasero_disc_set_session_contents (priv->disc, priv->session);
	g_object_ref (priv->session);

	return priv->session;
}

static void
brasero_disc_option_dialog_init (BraseroDiscOptionDialog *obj)
{
	GtkWidget *button;
	GtkWidget *options;
	BraseroPluginManager *manager;
	BraseroDiscOptionDialogPrivate *priv;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (obj);

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
					   G_CALLBACK (brasero_disc_option_dialog_caps_changed),
					   obj);

	priv->session = brasero_burn_session_new ();
	brasero_burn_session_add_flag (priv->session,
				       BRASERO_BURN_FLAG_EJECT|
				       BRASERO_BURN_FLAG_NOGRACE|
				       BRASERO_BURN_FLAG_BURNPROOF|
				       BRASERO_BURN_FLAG_CHECK_SIZE|
				       BRASERO_BURN_FLAG_DONT_CLEAN_OUTPUT);

	/* first box */
	priv->selection = brasero_dest_selection_new (priv->session);
	g_signal_connect (priv->selection,
			  "valid-media",
			  G_CALLBACK (brasero_disc_option_dialog_valid_media_cb),
			  obj);

	options = brasero_utils_pack_properties (_("<b>Select a drive to write to</b>"),
						 priv->selection,
						 NULL);
	gtk_widget_show_all (options);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (obj)->vbox),
			    options,
			    FALSE,
			    FALSE,
			    6);
}

static void
brasero_disc_option_dialog_finalize (GObject *object)
{
	BraseroDiscOptionDialogPrivate *priv;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (object);

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

	if (priv->output_sig) {
		g_signal_handler_disconnect (priv->session, priv->output_sig);
		priv->output_sig = 0;
	}

	if (priv->session) {
		g_object_unref (priv->session);
		priv->session = NULL;
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
							"title", _("Disc burning setup"),
							NULL));
	
	return GTK_WIDGET (obj);
}
