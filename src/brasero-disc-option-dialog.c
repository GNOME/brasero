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

#include "utils.h"
#include "burn-basics.h"
#include "brasero-disc-option-dialog.h"
#include "recorder-selection.h"
#include "brasero-ncb.h"
#include "disc.h"
 
static void brasero_disc_option_dialog_class_init (BraseroDiscOptionDialogClass *klass);
static void brasero_disc_option_dialog_init (BraseroDiscOptionDialog *sp);
static void brasero_disc_option_dialog_finalize (GObject *object);

struct _BraseroDiscOptionDialogPrivate {
	BraseroBurnCaps *caps;

	BraseroBurnFlag flags;

	BraseroTrackSource *track;
	BraseroDisc *disc;

	GtkWidget *video_toggle;
	GtkWidget *joliet_toggle;
	GtkWidget *checksum_toggle;
	GtkWidget *close_check;

	GtkWidget *selection;
	GtkWidget *label;

	GtkTooltips *tooltips;

	gint label_modified:1;
};

#define KEY_ACTIVATE_CHECKSUM	"/apps/brasero/config/activate_checksum"

static GtkDialogClass *parent_class = NULL;

GType
brasero_disc_option_dialog_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroDiscOptionDialogClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_disc_option_dialog_class_init,
			NULL,
			NULL,
			sizeof (BraseroDiscOptionDialog),
			0,
			(GInstanceInitFunc)brasero_disc_option_dialog_init,
		};

		type = g_type_register_static (GTK_TYPE_DIALOG, 
					       "BraseroDiscOptionDialog",
					       &our_info,
					       0);
	}

	return type;
}

static void
brasero_disc_option_dialog_class_init (BraseroDiscOptionDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

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

static void
brasero_disc_option_dialog_set_state (BraseroDiscOptionDialog *dialog)
{
	gboolean has_video;
	NautilusBurnMediaType media;
	NautilusBurnDrive *drive = NULL;
	BraseroBurnFlag default_flags = BRASERO_BURN_FLAG_NONE;
	BraseroBurnFlag supported_flags = BRASERO_BURN_FLAG_NONE;
	BraseroBurnFlag compulsory_flags = BRASERO_BURN_FLAG_NONE;

	/* get the media type and drive */
	brasero_recorder_selection_get_drive (BRASERO_RECORDER_SELECTION (dialog->priv->selection),
					      &drive,
					      NULL);
	media = nautilus_burn_drive_get_media_type (drive);

	if (dialog->priv->video_toggle) {
		if (NAUTILUS_BURN_DRIVE_MEDIA_TYPE_IS_DVD (media)) {
			has_video = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->video_toggle));
			gtk_widget_set_sensitive (dialog->priv->video_toggle, TRUE);
		}
		else {
			has_video = FALSE;
			gtk_widget_set_sensitive (dialog->priv->video_toggle, FALSE);
		}
	}
	else
		has_video = FALSE;

	if (has_video)
		dialog->priv->track->format |= BRASERO_IMAGE_FORMAT_VIDEO;
	else
		dialog->priv->track->format &= ~BRASERO_IMAGE_FORMAT_VIDEO;

	/* This option is only available if the disc is appendable 
	 * or if it's DVD since these latters are always appendable */
	brasero_burn_caps_get_flags (dialog->priv->caps,
				     dialog->priv->track,
				     drive,
				     &default_flags,
				     &compulsory_flags,
				     &supported_flags);

	if (dialog->priv->close_check) {
		if (has_video) {
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->priv->close_check), FALSE);
			gtk_widget_set_sensitive (dialog->priv->close_check, FALSE);
		}
		else if ((supported_flags & BRASERO_BURN_FLAG_DONT_CLOSE) == 0) {
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->priv->close_check), FALSE);
			gtk_widget_set_sensitive (dialog->priv->close_check, FALSE);
		}
		else if (compulsory_flags & BRASERO_BURN_FLAG_DONT_CLOSE) {
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->priv->close_check), TRUE);
			gtk_widget_set_sensitive (dialog->priv->close_check, FALSE);
		}
		else {
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->priv->close_check),
						     (default_flags & BRASERO_BURN_FLAG_DONT_CLOSE) != 0);
			gtk_widget_set_sensitive (dialog->priv->close_check, TRUE);
		}
	}

	/* if it's a multisession disc we also need to reset the title as the
	 * label of the last session on condition the user didn't modify the 
	 * default title. In the same way if the previous media was multisession
	 * but the new one isn't we reset our default label on condition it 
	 * hasn't been modified. */
	if (dialog->priv->track
	&&  dialog->priv->label
	&& !dialog->priv->label_modified) {
		time_t t;
		gchar buffer [128];
		gchar *title_str = NULL;

		t = time (NULL);
		strftime (buffer, sizeof (buffer), "%d %b %y", localtime (&t));

		if (dialog->priv->track->type == BRASERO_TRACK_SOURCE_DATA) {
			if ((NCB_DRIVE_GET_TYPE (drive) & NAUTILUS_BURN_DRIVE_TYPE_FILE) == 0
			&&  NCB_MEDIA_IS_APPENDABLE (drive))
				title_str = nautilus_burn_drive_get_media_label (drive);

			if (!title_str || title_str [0] == '\0')
				title_str = g_strdup_printf (_("Data disc (%s)"), buffer);
		}
		else if (dialog->priv->track->type == BRASERO_TRACK_SOURCE_AUDIO)
			title_str = g_strdup_printf (_("Audio disc (%s)"), buffer);

		gtk_entry_set_text (GTK_ENTRY (dialog->priv->label), title_str);
		g_free (title_str);
	}

	if (drive)	
		nautilus_burn_drive_unref (drive);
}

static void
brasero_disc_option_dialog_media_changed (BraseroRecorderSelection *selection,
					  NautilusBurnMediaType media,
					  BraseroDiscOptionDialog *dialog)
{
	brasero_disc_option_dialog_set_state (dialog);
}

static void
brasero_disc_option_dialog_video_clicked (GtkToggleButton *video,
					  BraseroDiscOptionDialog *dialog)
{
	brasero_disc_option_dialog_set_state (dialog);
}

static void
brasero_disc_option_dialog_add_multisession (BraseroDiscOptionDialog *dialog,
					     GtkWidget *box)
{
	GtkWidget *dont_close_check;
	GtkWidget *options;

	/* DVD don't need that */
	dont_close_check = gtk_check_button_new_with_label (_("Leave the disc open to add other files later"));
	dialog->priv->close_check = dont_close_check;
	gtk_tooltips_set_tip (dialog->priv->tooltips,
			      dont_close_check,
			      _("Allow to add more data to the disc later"),
			      _("Allow to add more data to the disc later"));

	options = brasero_utils_pack_properties (_("<b>Multisession</b>"),
						 dont_close_check,
						 NULL);
	gtk_box_pack_start (GTK_BOX (box), options, FALSE, FALSE, 0);
}

static void
brasero_disc_option_label_changed (GtkEditable *editable,
				   BraseroDiscOptionDialog *dialog)
{
	dialog->priv->label_modified = 1;
}

static void
brasero_disc_option_set_title_widget (BraseroDiscOptionDialog *dialog,
				      BraseroTrackSourceType type)
{
	gchar *title_str = NULL;
	gchar buffer [128];
	GtkWidget *widget;
	gchar *label;
	time_t t;

	if (!dialog->priv->label) {
		dialog->priv->label = gtk_entry_new ();
		gtk_entry_set_max_length (GTK_ENTRY (dialog->priv->label), 32);
	}

	dialog->priv->label_modified = 0;

	/* Header : This must be less that 32 characters long */
	t = time (NULL);
	strftime (buffer, sizeof (buffer), "%d %b %y", localtime (&t));

	if (type == BRASERO_TRACK_SOURCE_DATA) {
		NautilusBurnDrive *drive = NULL;
		NautilusBurnMediaType media;

		label = g_strdup (_("<b>Label of the disc</b>"));

		/* we need to know if it's multisession disc. In this case
		 * it's better to set the name of the previous session */
		brasero_recorder_selection_get_drive (BRASERO_RECORDER_SELECTION (dialog->priv->selection),
						      &drive,
						      NULL);

		media = nautilus_burn_drive_get_media_type (drive);

		if (drive
		&& (NCB_DRIVE_GET_TYPE (drive) & NAUTILUS_BURN_DRIVE_TYPE_FILE) == 0
		&&  NCB_MEDIA_IS_APPENDABLE (drive))
			title_str = nautilus_burn_drive_get_media_label (drive);

		if (!title_str || title_str [0] == '\0')
			title_str = g_strdup_printf (_("Data disc (%s)"), buffer);
	}
	else if (type == BRASERO_TRACK_SOURCE_AUDIO) {
		label = g_strdup (_("<b>Title</b>"));
		title_str = g_strdup_printf (_("Audio disc (%s)"), buffer);
	}
	else
		return;

	gtk_entry_set_text (GTK_ENTRY (dialog->priv->label), title_str);
	g_free (title_str);

	g_signal_connect (dialog->priv->label,
			  "changed",
			  G_CALLBACK (brasero_disc_option_label_changed),
			  dialog);

	widget = brasero_utils_pack_properties (label, dialog->priv->label, NULL);
	g_free (label);

	gtk_widget_show_all (widget);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    widget,
			    FALSE,
			    FALSE,
			    0);
}

static void
brasero_disc_option_dialog_joliet_toggled_cb (GtkToggleButton *toggle,
					      BraseroDiscOptionDialog *dialog)
{
	GtkResponseType answer;
	GtkWidget *message;
	gboolean hide;

	if (!GTK_WIDGET_VISIBLE (dialog)) {
		gtk_widget_show (GTK_WIDGET (dialog));
		hide = TRUE;
	}

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

	/* to make sure we don't run the dialog twice */
	g_signal_handlers_disconnect_by_func (toggle,
					      brasero_disc_option_dialog_joliet_toggled_cb,
					      dialog);
}

static void
brasero_disc_option_dialog_add_data_options (BraseroDiscOptionDialog *dialog,
					     BraseroImageFormat format)
{
	GtkWidget *widget = NULL;
	GConfClient *client;
	GtkWidget *options;
	gboolean checksum;

	/* create the options */
	widget = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_end (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			  widget,
			  TRUE,
			  FALSE,
			  6);

	/* general options */
	dialog->priv->joliet_toggle = gtk_check_button_new_with_label (_("Increase compatibility with Windows systems"));
	gtk_tooltips_set_tip (dialog->priv->tooltips,
			      dialog->priv->joliet_toggle,
			      _("Improve compatibility with Windows systems by allowing to display long filenames (maximum 64 characters)"),
			      _("Improve compatibility with Windows systems by allowing to display long filenames (maximum 64 characters)"));

	/* NOTE: we take for granted that if the source does not require
	 * to have the joliet extension, it's because it does have some
	 * incompatible filenames inside */
	if (!(format & BRASERO_IMAGE_FORMAT_JOLIET))
		g_signal_connect (dialog->priv->joliet_toggle,
				  "toggled",
				  G_CALLBACK (brasero_disc_option_dialog_joliet_toggled_cb),
				  dialog);
	else
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->priv->joliet_toggle), TRUE);

	dialog->priv->checksum_toggle = gtk_check_button_new_with_label (_("Check data integrity"));
	gtk_tooltips_set_tip (dialog->priv->tooltips,
			      dialog->priv->checksum_toggle,
			      _("Allow to check the integrity of files on the disc"),
			      _("Allow to check the integrity of files on the disc"));

	client = gconf_client_get_default ();
	checksum = gconf_client_get_bool (client, KEY_ACTIVATE_CHECKSUM, NULL);
	g_object_unref (client);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->priv->checksum_toggle), checksum);

	/* the following option is only shown if the source has the 
	 * video format (checked in data-disc). Like above if there 
	 * isn't this flag we assume it's not possible. Now, this 
	 * options isn't checked by default */
	if (format & BRASERO_IMAGE_FORMAT_VIDEO) {
		dialog->priv->video_toggle = gtk_check_button_new_with_label (_("Create a video DVD"));
		g_signal_connect (dialog->priv->video_toggle,
				  "toggled",
				  G_CALLBACK (brasero_disc_option_dialog_video_clicked),
				  dialog);

		gtk_tooltips_set_tip (dialog->priv->tooltips,
				      dialog->priv->video_toggle,
				      _("Create a video DVD that can be played by all DVD readers"),
				      _("Create a video DVD that can be played by all DVD readers"));
	}

	options = brasero_utils_pack_properties (_("<b>Disc options</b>"),
						 dialog->priv->joliet_toggle,
						 dialog->priv->checksum_toggle,
						 dialog->priv->video_toggle,
						 NULL);
	gtk_box_pack_start (GTK_BOX (widget), options, FALSE, FALSE, 0);

	/* multisession options */
	brasero_disc_option_dialog_add_multisession (dialog, widget);
	gtk_widget_show_all (widget);

	brasero_disc_option_dialog_set_state (dialog);
}

void
brasero_disc_option_dialog_add_audio_options (BraseroDiscOptionDialog *dialog)
{
	GtkWidget *widget;
	GtkWidget *options;
	GtkWidget *dont_close_check;

	widget = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_end (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			  widget,
			  TRUE,
			  FALSE,
			  6);

	/* multisession options */
	dont_close_check = gtk_check_button_new_with_label (_("Leave the disc open to add a data session later"));
	dialog->priv->close_check = dont_close_check;
	gtk_tooltips_set_tip (dialog->priv->tooltips,
			      dont_close_check,
			      _("Allow create what is called an enhanced CD or CD+"),
			      _("Allow create what is called an enhanced CD or CD+"));

	options = brasero_utils_pack_properties (_("<b>Multisession</b>"),
						 dont_close_check,
						 NULL);
	gtk_box_pack_start (GTK_BOX (widget), options, FALSE, FALSE, 0);
	gtk_widget_show_all (widget);

	brasero_disc_option_dialog_set_state (dialog);	
}

void
brasero_disc_option_dialog_set_disc (BraseroDiscOptionDialog *dialog,
				     NautilusBurnDrive *drive,
				     BraseroDisc *disc)
{
	BraseroTrackSourceType type = BRASERO_TRACK_SOURCE_UNKNOWN;
	BraseroImageFormat format = BRASERO_IMAGE_FORMAT_NONE;

	brasero_disc_get_track_type (disc, &type, &dialog->priv->flags, &format);

	/* we need to set a dummy track */
	if (dialog->priv->track)
		brasero_track_source_free (dialog->priv->track);

	dialog->priv->track = g_new0 (BraseroTrackSource, 1);
	dialog->priv->track->type = type;
	dialog->priv->track->format = format;

	if (drive && (dialog->priv->flags & (BRASERO_BURN_FLAG_MERGE|BRASERO_BURN_FLAG_APPEND))) {
		brasero_recorder_selection_set_drive (BRASERO_RECORDER_SELECTION (dialog->priv->selection), drive);
		brasero_recorder_selection_lock (BRASERO_RECORDER_SELECTION (dialog->priv->selection), FALSE);
	}

	brasero_recorder_selection_set_source_track (BRASERO_RECORDER_SELECTION (dialog->priv->selection),
						     dialog->priv->track);

	g_object_set (dialog->priv->selection, "file-image", TRUE, NULL);

	/* NOTE: the caller must have ensured the disc is ready */
	dialog->priv->disc = disc;

	brasero_disc_option_set_title_widget (dialog, type);

	if (type == BRASERO_TRACK_SOURCE_DATA)
		brasero_disc_option_dialog_add_data_options (dialog, format);
	else if (type == BRASERO_TRACK_SOURCE_AUDIO)
		brasero_disc_option_dialog_add_audio_options (dialog);
}

static void
brasero_disc_option_dialog_burn_clicked_cb (GtkWidget *button, BraseroDiscOptionDialog *dialog)
{
	if (dialog->priv->checksum_toggle) {
		GConfClient *client;

		client = gconf_client_get_default ();
		gconf_client_set_bool (client,
				       KEY_ACTIVATE_CHECKSUM,
				       gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->checksum_toggle)),
				       NULL);
		g_object_unref (client);
	}
		
	gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
}

gboolean
brasero_disc_option_dialog_get_param (BraseroDiscOptionDialog *dialog,
				      BraseroBurnFlag *flags,
				      NautilusBurnDrive **drive,
				      gint *speed,
				      BraseroTrackSource **source,
				      gchar **output,
				      gboolean *checksum)
{
	BraseroTrackSource *track;
	BraseroDriveProp props;
	BraseroBurnFlag tmp;
	gboolean dvd_video;

	g_return_val_if_fail (source != NULL, FALSE);
	g_return_val_if_fail (drive != NULL, FALSE);

	/* get track */
	if (dialog->priv->joliet_toggle
	&&  gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->joliet_toggle)))
		brasero_disc_get_track_source (dialog->priv->disc,
					       &track,
					       BRASERO_IMAGE_FORMAT_JOLIET);
	else
		brasero_disc_get_track_source (dialog->priv->disc,
					       &track,
					       BRASERO_IMAGE_FORMAT_NONE);			

	if (dialog->priv->label)
		track->contents.data.label = g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->priv->label)));;

	if (!dialog->priv->video_toggle
	|| !GTK_WIDGET_IS_SENSITIVE (dialog->priv->video_toggle)
	|| !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->video_toggle))) {
		dvd_video = FALSE;
		track->format &= ~BRASERO_IMAGE_FORMAT_VIDEO;
	}
	else {
		dvd_video = TRUE;
		track->format |= BRASERO_IMAGE_FORMAT_VIDEO;
	}

	*source = track;

	/* get drive, speed and flags */
	brasero_recorder_selection_get_drive (BRASERO_RECORDER_SELECTION (dialog->priv->selection),
					      drive,
					      &props);

	tmp = BRASERO_BURN_FLAG_NONE;
	if (NCB_DRIVE_GET_TYPE (*drive) == NAUTILUS_BURN_DRIVE_TYPE_FILE) {
		if (output)
			*output = props.output_path;

		if (speed)
			*speed = 0;
	}
	else {
		tmp = BRASERO_BURN_FLAG_DONT_OVERWRITE;
		if (speed)
			*speed = props.props.drive_speed;

		if (output)
			*output = NULL;
	}

	if (flags) {
		tmp |= props.flags;

		if (!dvd_video
		&&  dialog->priv->close_check
		&&  gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->close_check)))
			tmp |= BRASERO_BURN_FLAG_DONT_CLOSE;
		else /* this is because for DVD the flag is compulsory */
			tmp &= ~BRASERO_BURN_FLAG_DONT_CLOSE;

		if ((dialog->priv->flags & (BRASERO_BURN_FLAG_APPEND|BRASERO_BURN_FLAG_MERGE))) {
			BraseroBurnFlag supported_flags = BRASERO_BURN_FLAG_NONE;

			/* see if merge flag is supported if that's an audio
			 * track we can't use merge just append */
			brasero_burn_caps_get_flags (dialog->priv->caps,
						     track,
						     *drive,
						     NULL,
						     NULL,
						     &supported_flags);

			tmp |= (dialog->priv->flags & supported_flags);
		}
		else if (NCB_DRIVE_GET_TYPE (*drive) != NAUTILUS_BURN_DRIVE_TYPE_FILE)
			tmp |= BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE;

		*flags = tmp;
	}

	if (checksum) {
		if (dialog->priv->checksum_toggle
		&&  gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->checksum_toggle)))
			*checksum = TRUE;
		else
			*checksum = FALSE;
	}

	return TRUE;
}

static void
brasero_disc_option_dialog_cancel_clicked_cb (GtkButton *cancel, GtkWidget *dialog)
{
	gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
}

static void
brasero_disc_option_dialog_init (BraseroDiscOptionDialog *obj)
{
	GtkWidget *burn;
	GtkWidget *cancel;
	GtkWidget *options;

	obj->priv = g_new0 (BraseroDiscOptionDialogPrivate, 1);
	gtk_dialog_set_has_separator (GTK_DIALOG (obj), FALSE);

	obj->priv->tooltips = gtk_tooltips_new ();
	g_object_ref_sink (GTK_OBJECT (obj->priv->tooltips));

	obj->priv->caps = brasero_burn_caps_get_default ();

	/* first box */
	obj->priv->selection = brasero_recorder_selection_new ();
	gtk_widget_show (obj->priv->selection);
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

	g_signal_connect (obj->priv->selection,
			  "media-changed",
			  G_CALLBACK (brasero_disc_option_dialog_media_changed),
			  obj);

	/* buttons */
	cancel = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
	gtk_widget_show (cancel);
	g_signal_connect (G_OBJECT (cancel),
			  "clicked",
			  G_CALLBACK (brasero_disc_option_dialog_cancel_clicked_cb),
			  obj);
	gtk_dialog_add_action_widget (GTK_DIALOG (obj), cancel, GTK_RESPONSE_CANCEL);

	burn = brasero_utils_make_button (_("Burn"),
					  NULL,
					  "brasero-action-burn",
					  GTK_ICON_SIZE_LARGE_TOOLBAR);
	gtk_widget_show (burn);
	g_signal_connect (G_OBJECT (burn),
			  "clicked",
			  G_CALLBACK (brasero_disc_option_dialog_burn_clicked_cb),
			  obj);
	gtk_dialog_add_action_widget (GTK_DIALOG (obj), burn, GTK_RESPONSE_OK);
}

static void
brasero_disc_option_dialog_finalize (GObject *object)
{
	BraseroDiscOptionDialog *cobj;

	cobj = BRASERO_DISC_OPTION_DIALOG (object);
	
	if (cobj->priv->caps) {
		g_object_unref (cobj->priv->caps);
		cobj->priv->caps = NULL;
	}

	if (cobj->priv->tooltips) {
		g_object_unref (cobj->priv->tooltips);
		cobj->priv->tooltips = NULL;
	}

	if (cobj->priv->track) {
		brasero_track_source_free (cobj->priv->track);
		cobj->priv->track = NULL;
	}

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}
