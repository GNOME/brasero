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

#include <gconf/gconf-client.h>

#include "brasero-utils.h"
#include "burn-basics.h"
#include "burn-medium.h"
#include "burn-volume-obj.h"
#include "burn-session.h"
#include "burn-caps.h"
#include "burn-plugin-manager.h"
#include "brasero-disc-option-dialog.h"
#include "brasero-dest-selection.h"
#include "burn-drive.h"
#include "brasero-disc.h"

G_DEFINE_TYPE (BraseroDiscOptionDialog, brasero_disc_option_dialog, GTK_TYPE_DIALOG);

struct _BraseroDiscOptionDialogPrivate {
	BraseroBurnSession *session;

	BraseroBurnCaps *caps;
	BraseroDisc *disc;

	guint caps_sig;
	guint output_sig;

	GtkWidget *joliet_toggle;
	GtkWidget *multi_toggle;

	GtkWidget *selection;
	GtkWidget *label;

	GtkWidget *button;

	GtkWidget *video_options;
	GtkWidget *dvd_audio;
	GtkWidget *vcd_label;
	GtkWidget *vcd_button;
	GtkWidget *svcd_button;

	guint label_modified:1;
	guint joliet_warning:1;

	guint checksum_saved:1;
	guint joliet_saved:1;
	guint multi_saved:1;
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
	if (!key)
		return;

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
brasero_disc_option_dialog_truncate_label (const gchar *label)
{
	const gchar *delim;
	gchar *next_char;

	/* find last possible character. We can't just do a tmp + 32 
	 * since we don't know if we are at the start of a character */
	delim = label;
	while ((next_char = g_utf8_find_next_char (delim, NULL))) {
		if (next_char - label > 32)
			break;

		delim = next_char;
	}

	return g_strndup (label, delim - label);
}

static gchar *
brasero_disc_option_dialog_get_default_label (BraseroDiscOptionDialog *dialog)
{
	time_t t;
	gchar buffer [128];
	BraseroDrive *drive;
	gchar *title_str = NULL;
	BraseroTrackType source;
	BraseroDiscOptionDialogPrivate *priv;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	brasero_burn_session_get_input_type (priv->session, &source);
	drive = brasero_drive_selection_get_drive (BRASERO_DRIVE_SELECTION (priv->selection));

	t = time (NULL);
	strftime (buffer, sizeof (buffer), "%d %b %y", localtime (&t));

	if (source.type == BRASERO_TRACK_TYPE_DATA) {
		BraseroBurnFlag flags;

		flags = brasero_burn_session_get_flags (priv->session);
		if (flags & BRASERO_BURN_FLAG_MERGE) {
			BraseroMedium *medium;

			medium = brasero_drive_get_medium (drive);
			title_str = brasero_volume_get_name (BRASERO_VOLUME (medium));
		}

		if (!title_str || title_str [0] == '\0') {
			/* NOTE to translators: the final string must not be over
			 * 32 _bytes_ otherwise it gets truncated. */
			title_str = g_strdup_printf (_("Data disc (%s)"), buffer);

			if (strlen (title_str) > 32) {
				g_free (title_str);
				strftime (buffer, sizeof (buffer), "%F", localtime (&t));
				title_str = g_strdup_printf ("Data disc %s", buffer);
			}
		}
	}
	else if (source.type == BRASERO_TRACK_TYPE_AUDIO) {
		if (source.subtype.audio_format & (BRASERO_VIDEO_FORMAT_UNDEFINED|BRASERO_VIDEO_FORMAT_VCD|BRASERO_VIDEO_FORMAT_VIDEO_DVD))
			/* NOTE to translators: the final string must not be over
			 * 32 _bytes_ */
			title_str = g_strdup_printf (_("Video disc (%s)"), buffer);
		else
			/* NOTE to translators: the final string must not be over
			 * 32 _bytes_ */
			title_str = g_strdup_printf (_("Audio disc (%s)"), buffer);

		if (strlen (title_str) > 32) {
			g_free (title_str);
			strftime (buffer, sizeof (buffer), "%F", localtime (&t));
			title_str = g_strdup_printf ("Audio disc %s", buffer);
		}
	}

	if (drive)
		g_object_unref (drive);

	if (strlen (title_str) > 32) {
		gchar *tmp;

		tmp = brasero_disc_option_dialog_truncate_label (title_str);
		g_free (title_str);

		title_str = tmp;
	}

	return title_str;
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

	brasero_burn_session_set_label (priv->session, label);
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

	/* what we want to check Joliet support */
	brasero_burn_session_get_input_type (priv->session, &source);

	source.subtype.fs_type |= BRASERO_IMAGE_FS_JOLIET;
	result = brasero_burn_caps_is_input_supported (priv->caps,
						       priv->session,
						       &source,
						       FALSE);
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

static void
brasero_disc_option_dialog_update_multi (BraseroDiscOptionDialog *dialog)
{
	BraseroTrackType input;
	BraseroDiscOptionDialogPrivate *priv;
	BraseroBurnFlag supported = BRASERO_BURN_FLAG_NONE;
	BraseroBurnFlag compulsory = BRASERO_BURN_FLAG_NONE;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	if (!priv->multi_toggle)
		return;

	brasero_burn_session_get_input_type (priv->session, &input);
	if (input.type == BRASERO_TRACK_TYPE_DATA
	&& (input.subtype.fs_type & BRASERO_IMAGE_FS_VIDEO)
	&& (brasero_burn_session_get_dest_media (priv->session) & (BRASERO_MEDIUM_DVD|BRASERO_MEDIUM_DVD_DL))) {
		gtk_widget_set_sensitive (priv->multi_toggle, FALSE);
		goto end;
	}

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
}

static void
brasero_disc_option_dialog_update_video (BraseroDiscOptionDialog *dialog)
{
	BraseroDiscOptionDialogPrivate *priv;
	BraseroMedia media;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	media = brasero_burn_session_get_dest_media (priv->session);

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
		format = brasero_burn_session_get_output_format (priv->session);
		if (format == BRASERO_IMAGE_FORMAT_NONE)
			return;

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

	/* see if we need to update the label */
	if (!priv->label_modified)
		brasero_disc_option_dialog_update_label (dialog);

	/* for video disc see what's the output : CD or DVD */
	if (priv->dvd_audio)
		brasero_disc_option_dialog_update_video (dialog);
}

/**
 * These functions are used to update the session according to the states
 * of the buttons and entry
 */

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
		brasero_disc_option_dialog_save_multi_state (dialog);
		return;
	}

	brasero_burn_session_add_flag (priv->session, BRASERO_BURN_FLAG_MULTI);
	brasero_disc_option_dialog_save_multi_state (dialog);
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

static void
brasero_disc_option_label_insert_text (GtkEditable *editable,
				       const gchar *text,
				       gint length,
				       gint *position,
				       gpointer data)
{
	BraseroDiscOptionDialogPrivate *priv;
	const gchar *label;
	gchar *new_text;
	gint new_length;
	gchar *current;
	gint max_len;
	gchar *prev;
	gchar *next;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (data);	

	/* check if this new text will fit in 32 _bytes_ long buffer */
	label = gtk_entry_get_text (GTK_ENTRY (priv->label));
	max_len = 32 - strlen (label) - length;
	if (max_len >= 0)
		return;

	gdk_beep ();

	/* get the last character '\0' of the text to be inserted */
	new_length = length;
	new_text = g_strdup (text);
	current = g_utf8_offset_to_pointer (new_text, g_utf8_strlen (new_text, -1));

	/* don't just remove one character in case there was many more
	 * that were inserted at the same time through DND, paste, ... */
	prev = g_utf8_find_prev_char (new_text, current);
	if (!prev) {
		/* no more characters so no insertion */
		g_signal_stop_emission_by_name (editable, "insert_text"); 
		g_free (new_text);
		return;
	}

	do {
		next = current;
		current = prev;

		prev = g_utf8_find_prev_char (new_text, current);
		if (!prev) {
			/* no more characters so no insertion */
			g_signal_stop_emission_by_name (editable, "insert_text"); 
			g_free (new_text);
			return;
		}

		new_length -= next - current;
		max_len += next - current;
	} while (max_len < 0 && new_length > 0);

	*current = '\0';
	g_signal_handlers_block_by_func (editable,
					 (gpointer) brasero_disc_option_label_insert_text,
					 data);
	gtk_editable_insert_text (editable, new_text, new_length, position);
	g_signal_handlers_unblock_by_func (editable,
					   (gpointer) brasero_disc_option_label_insert_text,
					   data);

	g_signal_stop_emission_by_name (editable, "insert_text");
	g_free (new_text);
}

static void
brasero_disc_option_label_changed (GtkEditable *editable,
				   BraseroDiscOptionDialog *dialog)
{
	const gchar *label;
	BraseroDiscOptionDialogPrivate *priv;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	label = gtk_entry_get_text (GTK_ENTRY (priv->label));
	brasero_burn_session_set_label (priv->session, label);
	priv->label_modified = 1;
}

static void
brasero_disc_option_dialog_title_widget (BraseroDiscOptionDialog *dialog)
{
	BraseroDiscOptionDialogPrivate *priv;
	gchar *title_str = NULL;
	BraseroTrackType type;
	gchar *label = NULL;
	GtkWidget *widget;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	if (!priv->label)
		priv->label = gtk_entry_new ();

	priv->label_modified = 0;
	g_signal_connect (priv->label,
			  "insert_text",
			  G_CALLBACK (brasero_disc_option_label_insert_text),
			  dialog);
	g_signal_connect (priv->label,
			  "changed",
			  G_CALLBACK (brasero_disc_option_label_changed),
			  dialog);

	title_str = brasero_disc_option_dialog_get_default_label (dialog);
	gtk_entry_set_text (GTK_ENTRY (priv->label), title_str);
	brasero_burn_session_set_label (priv->session, title_str);
	g_free (title_str);

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
	GtkWidget *options;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);


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

	options = brasero_utils_pack_properties (_("<b>Disc options</b>"),
						 priv->multi_toggle,
						 priv->joliet_toggle,
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

static void
brasero_disc_option_dialog_AC3 (GtkToggleButton *button,
				BraseroDiscOptionDialog *dialog)
{
	BraseroDiscOptionDialogPrivate *priv;
	BraseroAudioFormat format;
	GValue *value = NULL;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	brasero_burn_session_tag_lookup (priv->session,
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
	brasero_burn_session_tag_add (priv->session,
				      BRASERO_DVD_AUDIO_STREAMS,
				      value);
}

static void
brasero_disc_option_dialog_MP2 (GtkToggleButton *button,
				BraseroDiscOptionDialog *dialog)
{
	BraseroDiscOptionDialogPrivate *priv;
	BraseroAudioFormat format;
	GValue *value = NULL;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	brasero_burn_session_tag_lookup (priv->session,
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
	brasero_burn_session_tag_add (priv->session,
				      BRASERO_DVD_AUDIO_STREAMS,
				      value);
}

static void
brasero_disc_option_dialog_set_tag (BraseroDiscOptionDialog *dialog,
				    const gchar *tag,
				    gint contents)
{
	BraseroDiscOptionDialogPrivate *priv;
	GValue *value;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);

	value = g_new0 (GValue, 1);
	g_value_init (value, G_TYPE_INT);
	g_value_set_int (value, contents);
	brasero_burn_session_tag_add (priv->session,
				      tag,
				      value);
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
	BraseroDiscOptionDialogPrivate *priv;

	if (!gtk_toggle_button_get_active (button))
		return;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);
	brasero_burn_session_tag_remove (priv->session,
					 BRASERO_VIDEO_OUTPUT_FRAMERATE);
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
	BraseroDiscOptionDialogPrivate *priv;

	if (!gtk_toggle_button_get_active (button))
		return;

	priv = BRASERO_DISC_OPTION_DIALOG_PRIVATE (dialog);
	brasero_burn_session_tag_remove (priv->session,
					 BRASERO_VIDEO_OUTPUT_ASPECT);
}

static void
brasero_disc_option_dialog_add_video_options (BraseroDiscOptionDialog *dialog)
{
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
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    widget,
			    FALSE,
			    FALSE,
			    6);

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

	options = brasero_utils_pack_properties (_("<b>Video Options</b>"),
						 table,
						 NULL);
	gtk_box_pack_start (GTK_BOX (widget), options, FALSE, FALSE, 0);

	/* Audio options for DVDs */
	button1 = gtk_check_button_new_with_mnemonic (_("Add _AC3 audio stream"));
	button2 = gtk_check_button_new_with_mnemonic (_("Add _MP2 audio stream"));
	options = brasero_utils_pack_properties (_("<b>Audio Options</b>"),
						 button1,
						 button2,
						 NULL);
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

	/* see if we should lock the drive only with MERGE */
	lock_drive = (brasero_burn_session_get_flags (priv->session) & BRASERO_BURN_FLAG_MERGE) != 0;
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
		if (type.subtype.audio_format & (BRASERO_VIDEO_FORMAT_UNDEFINED|BRASERO_VIDEO_FORMAT_VCD|BRASERO_VIDEO_FORMAT_VIDEO_DVD)) {
			brasero_drive_selection_set_type_shown (BRASERO_DRIVE_SELECTION (priv->selection),
								BRASERO_MEDIA_TYPE_WRITABLE|
								BRASERO_MEDIA_TYPE_FILE);
			brasero_disc_option_dialog_add_video_options (dialog);
		}
		else {
			brasero_drive_selection_set_type_shown (BRASERO_DRIVE_SELECTION (priv->selection),
								BRASERO_MEDIA_TYPE_WRITABLE);
			brasero_disc_option_dialog_add_audio_options (dialog);
		}
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

	if (priv->video_options)
		gtk_widget_set_sensitive (priv->video_options, valid);
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

	options = brasero_utils_pack_properties (_("<b>Select a disc to write to</b>"),
						 priv->selection,
						 NULL);
	gtk_widget_show (options);

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
							"title", _("Disc Burning Setup"),
							NULL));

	return GTK_WIDGET (obj);
}
