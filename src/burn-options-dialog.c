/***************************************************************************
 *            burn-options-dialog.c
 *
 *  mer mar 29 13:47:56 2006
 *  Copyright  2006  Rouquier Philippe
 *  brasero-app@wanadoo.fr
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
#include "burn-options-dialog.h"
#include "recorder-selection.h"
#include "burn-dialog.h"
#include "brasero-ncb.h"

static void brasero_burn_option_dialog_class_init (BraseroBurnOptionDialogClass *klass);
static void brasero_burn_option_dialog_init (BraseroBurnOptionDialog *sp);
static void brasero_burn_option_dialog_finalize (GObject *object);

static void
brasero_burn_option_dialog_dont_close_toggled_cb (GtkToggleButton *toggle,
						  BraseroBurnOptionDialog *dialog);
static void
brasero_burn_option_dialog_append_toggled_cb (GtkToggleButton *toggle,
					      BraseroBurnOptionDialog *dialog);
static void
brasero_burn_option_dialog_joliet_toggled_cb (GtkToggleButton *toggle,
					      BraseroBurnOptionDialog *dialog);
static void
brasero_burn_option_dialog_cancel_clicked_cb (GtkButton *button,
					      GtkWidget *dialog);
static void
brasero_burn_option_dialog_burn_clicked_cb (GtkWidget *button,
					    BraseroBurnOptionDialog *dialog);
static void
brasero_burn_option_dialog_media_changed (BraseroRecorderSelection *selection,
					  NautilusBurnMediaType media,
					  BraseroBurnOptionDialog *dialog);

struct BraseroBurnOptionDialogPrivate {
	GtkWidget *video_toggle;
	GtkWidget *joliet_toggle;
	GtkWidget *checksum_toggle;
	GtkWidget *append_check;

	GtkWidget *selection;
	GtkWidget *label;

	GtkTooltips *tooltips;

	BraseroBurnFlag flags;
};

static GObjectClass *parent_class = NULL;

#define KEY_ACTIVATE_CHECKSUM	"/apps/brasero/config/activate_checksum"
GType
brasero_burn_option_dialog_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroBurnOptionDialogClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_burn_option_dialog_class_init,
			NULL,
			NULL,
			sizeof (BraseroBurnOptionDialog),
			0,
			(GInstanceInitFunc)brasero_burn_option_dialog_init,
		};

		type = g_type_register_static(GTK_TYPE_DIALOG,
					      "BraseroBurnOptionDialog",
					      &our_info,
					      0);
	}

	return type;
}

static void
brasero_burn_option_dialog_class_init (BraseroBurnOptionDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_burn_option_dialog_finalize;
}

static void
brasero_burn_option_dialog_init (BraseroBurnOptionDialog *obj)
{
	GtkWidget *burn;
	GtkWidget *cancel;

	obj->priv = g_new0 (BraseroBurnOptionDialogPrivate, 1);
	gtk_dialog_set_has_separator (GTK_DIALOG (obj), FALSE);

	obj->priv->tooltips = gtk_tooltips_new ();
	g_object_ref (obj->priv->tooltips);
	g_object_ref_sink (GTK_OBJECT (obj->priv->tooltips));

	/* first box */
	obj->priv->selection = brasero_recorder_selection_new ();
	g_object_set (G_OBJECT (obj->priv->selection),
		      "file-image", TRUE,
		      "show-properties", TRUE,
		      "show-recorders-only", TRUE,
		      NULL);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (obj)->vbox),
			    brasero_utils_pack_properties (_("<b>Select a drive to write to</b>"),
							   obj->priv->selection,
							   NULL),
			    FALSE,
			    FALSE,
			    0);

	brasero_recorder_selection_select_default_drive (BRASERO_RECORDER_SELECTION (obj->priv->selection),
							 BRASERO_MEDIA_WRITABLE);
	gtk_tooltips_set_tip (obj->priv->tooltips,
			      obj->priv->selection,
			      _("Choose which drive holds the disc to write to"),
			      _("Choose which drive holds the disc to write to"));

	g_signal_connect (obj->priv->selection,
			  "media-changed",
			  G_CALLBACK (brasero_burn_option_dialog_media_changed),
			  obj);

	/* buttons */
	cancel = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
	g_signal_connect (G_OBJECT (cancel),
			  "clicked",
			  G_CALLBACK (brasero_burn_option_dialog_cancel_clicked_cb),
			  obj);
	gtk_dialog_add_action_widget (GTK_DIALOG (obj), cancel, GTK_RESPONSE_CANCEL);

	burn = brasero_utils_make_button (_("Burn"), GTK_STOCK_CDROM);
	g_signal_connect (G_OBJECT (burn),
			  "clicked",
			  G_CALLBACK (brasero_burn_option_dialog_burn_clicked_cb),
			  obj);
	gtk_dialog_add_action_widget (GTK_DIALOG (obj), burn, GTK_RESPONSE_OK);
}

static void
brasero_burn_option_dialog_finalize (GObject *object)
{
	BraseroBurnOptionDialog *cobj;

	cobj = BRASERO_BURN_OPTION_DIALOG (object);

	if (cobj->priv->tooltips) {
		g_object_unref (cobj->priv->tooltips);
		cobj->priv->tooltips = NULL;
	}

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_burn_option_set_title_widget (BraseroBurnOptionDialog *dialog,
				      gboolean is_data)
{
	GtkWidget *widget;
	char buffer [128];
	char *title_str;
	char *label;
	time_t t;

	dialog->priv->label = gtk_entry_new ();
	gtk_entry_set_max_length(GTK_ENTRY (dialog->priv->label), 32);

	/* Header : This must be less that 32 characters long */
	t = time (NULL);
	strftime (buffer, sizeof (buffer), "%d %b %y", localtime (&t));

	if (is_data)
		title_str = g_strdup_printf (_("Data disc (%s)"), buffer);
	else
		title_str = g_strdup_printf (_("Audio disc (%s)"), buffer);

	gtk_entry_set_text (GTK_ENTRY (dialog->priv->label), title_str);
	g_free (title_str);

	if (is_data)
		label = g_strdup (_("<b>Label of the disc</b>"));
	else
		label = g_strdup (_("<b>Title</b>"));
	widget = brasero_utils_pack_properties (label,
						dialog->priv->label,
						NULL);
	g_free (label);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    widget,
			    TRUE,
			    TRUE,
			    0);
	gtk_box_reorder_child (GTK_BOX (GTK_DIALOG (dialog)->vbox), widget, 0);
}

GtkWidget *
brasero_burn_option_dialog_new (const BraseroTrackSource *track)
{
	BraseroBurnOptionDialog *obj;
	GtkWidget *widget = NULL;
	
	obj = BRASERO_BURN_OPTION_DIALOG (g_object_new (BRASERO_TYPE_BURN_OPTION_DIALOG,
							"title", _("Disc burning setup"),
							NULL));

	brasero_recorder_selection_set_source_track (BRASERO_RECORDER_SELECTION (obj->priv->selection),
						     track);

	/* keep a copy of the track for later */
	if (track->type == BRASERO_TRACK_SOURCE_DATA) {
		NautilusBurnDrive *drive = NULL;
		GtkWidget *dont_close_check;
		GtkWidget *append_check;
		GConfClient *client;
		GtkWidget *options;
		gboolean checksum;

		brasero_burn_option_set_title_widget (obj, TRUE);

		widget = gtk_vbox_new (FALSE, 0);

		append_check = gtk_check_button_new_with_label (_("Append the files to those already on the disc"));
		gtk_tooltips_set_tip (obj->priv->tooltips,
				      append_check,
				      _("Add these data to those already on the disc instead of replacing them"),
				      _("Add these data to those already on the disc instead of replacing them"));
		g_signal_connect (G_OBJECT (append_check),
				  "toggled",
				  G_CALLBACK (brasero_burn_option_dialog_append_toggled_cb),
				  obj);
		obj->priv->append_check = append_check;

		/* This option is only available if the disc is appendable */
		brasero_recorder_selection_get_drive (BRASERO_RECORDER_SELECTION (obj->priv->selection),
						      &drive,
						      NULL);
		if (drive && nautilus_burn_drive_media_is_appendable (drive))
			gtk_widget_set_sensitive (append_check, TRUE);
		else
			gtk_widget_set_sensitive (append_check, FALSE);

		if (drive)	
			nautilus_burn_drive_unref (drive);

		dont_close_check = gtk_check_button_new_with_label (_("Leave the disc open to add other files later"));
		gtk_tooltips_set_tip (obj->priv->tooltips,
				      dont_close_check,
				      _("Allow to add more data to the disc later"),
				      _("Allow to add more data to the disc later"));
		g_signal_connect (G_OBJECT (dont_close_check),
				  "toggled",
				  G_CALLBACK (brasero_burn_option_dialog_dont_close_toggled_cb),
				  obj);
		options = brasero_utils_pack_properties (_("<b>Multisession</b>"),
							 append_check,
							 dont_close_check,
							 NULL);
		gtk_box_pack_start (GTK_BOX (widget), options, FALSE, FALSE, 0);

		obj->priv->joliet_toggle = gtk_check_button_new_with_label (_("Increase compatibility with Windows systems"));
		gtk_tooltips_set_tip (obj->priv->tooltips,
				      obj->priv->joliet_toggle,
				      _("Improve compatibility with Windows systems by allowing to display long filenames (maximum 64 characters)"),
				      _("Improve compatibility with Windows systems by allowing to display long filenames (maximum 64 characters)"));
		/* NOTE: we take for granted that if the source does not require
		 * to have the joliet extension, it's because it does have some
		 * incompatible filenames inside */
		if (!(track->format & BRASERO_IMAGE_FORMAT_JOLIET))
			g_signal_connect (obj->priv->joliet_toggle,
					  "toggled",
					  G_CALLBACK (brasero_burn_option_dialog_joliet_toggled_cb),
					  obj);
		else
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (obj->priv->joliet_toggle), TRUE);

		obj->priv->checksum_toggle = gtk_check_button_new_with_label (_("Check data integrity"));
		gtk_tooltips_set_tip (obj->priv->tooltips,
				      obj->priv->checksum_toggle,
				      _("Allow to check the integrity of files on the disc"),
				      _("Allow to check the integrity of files on the disc"));

		client = gconf_client_get_default ();
		checksum = gconf_client_get_bool (client, KEY_ACTIVATE_CHECKSUM, NULL);
		g_object_unref (client);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (obj->priv->checksum_toggle), checksum);

		/* the following option is only shown if the source has the 
		 * video format (checked in data-disc). Like above if there 
		 * isn't this flag we assume it's not possible. Now, this 
		 * options isn't checked by default */
		if (track->format & BRASERO_IMAGE_FORMAT_VIDEO) {
			obj->priv->video_toggle = gtk_check_button_new_with_label (_("Create a video DVD"));
			gtk_tooltips_set_tip (obj->priv->tooltips,
					      obj->priv->video_toggle,
					      _("Create a video DVD that can be played by all DVD readers"),
					      _("Create a video DVD that can be played by all DVD readers"));
		}

		options = brasero_utils_pack_properties (_("<b>Disc options</b>"),
							 obj->priv->joliet_toggle,
							 obj->priv->checksum_toggle,
							 obj->priv->video_toggle,
							 NULL);
		gtk_box_pack_start (GTK_BOX (widget), options, FALSE, FALSE, 0);

		g_object_set (obj->priv->selection, "file-image", TRUE, NULL);
	}
	else
		g_object_set (obj->priv->selection, "file-image", FALSE, NULL);


	if (widget) {
		gtk_box_pack_end (GTK_BOX (GTK_DIALOG (obj)->vbox),
				  widget,
				  FALSE,
				  FALSE,
				  0);
		gtk_widget_show_all (widget);
	}

	return GTK_WIDGET (obj);
}

static void
brasero_burn_option_dialog_media_changed (BraseroRecorderSelection *selection,
					  NautilusBurnMediaType media,
					  BraseroBurnOptionDialog *dialog)
{
	if (!dialog->priv->append_check)
		return;
}

static void
brasero_burn_option_dialog_append_toggled_cb (GtkToggleButton *toggle,
					      BraseroBurnOptionDialog *dialog)
{
	if (gtk_toggle_button_get_active (toggle))
		dialog->priv->flags |= BRASERO_BURN_FLAG_MERGE;
	else
		dialog->priv->flags &= ~BRASERO_BURN_FLAG_MERGE;
}

static void
brasero_burn_option_dialog_joliet_toggled_cb (GtkToggleButton *toggle,
					      BraseroBurnOptionDialog *dialog)
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
					      brasero_burn_option_dialog_joliet_toggled_cb,
					      dialog);
}

static void
brasero_burn_option_dialog_dont_close_toggled_cb (GtkToggleButton *toggle,
						  BraseroBurnOptionDialog *dialog)
{
	if (gtk_toggle_button_get_active (toggle))
		dialog->priv->flags |= BRASERO_BURN_FLAG_DONT_CLOSE;
	else
		dialog->priv->flags &= ~BRASERO_BURN_FLAG_DONT_CLOSE;
}

static void
brasero_burn_option_dialog_burn_clicked_cb (GtkWidget *button, BraseroBurnOptionDialog *dialog)
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
brasero_burn_option_dialog_get_session_param (BraseroBurnOptionDialog *dialog,
					      NautilusBurnDrive **drive,
					      gint *speed,
					      gchar **output,
					      BraseroBurnFlag *flags,
					      gchar **label,
					      BraseroImageFormat *format,
					      gboolean *checksum)
{
	BraseroDriveProp props;

	brasero_recorder_selection_get_drive (BRASERO_RECORDER_SELECTION (dialog->priv->selection),
					      drive,
					      &props);

	*flags = BRASERO_BURN_FLAG_NONE;

	if (NCB_DRIVE_GET_TYPE (*drive) == NAUTILUS_BURN_DRIVE_TYPE_FILE) {
		*output = props.output_path;
		*speed = 0;
	}
	else {
		*flags = BRASERO_BURN_FLAG_DONT_OVERWRITE;
		*speed = props.props.drive_speed;
		*output = NULL;
	}

	if (flags)
		*flags |= props.flags | dialog->priv->flags;

	if (format) {
		*format = BRASERO_IMAGE_FORMAT_NONE;

		if (dialog->priv->joliet_toggle
		&&  gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->joliet_toggle)))
			*format |= BRASERO_IMAGE_FORMAT_JOLIET;

		if (dialog->priv->video_toggle
		&&  gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->video_toggle)))
			*format |= BRASERO_IMAGE_FORMAT_VIDEO;
	}

	if (label && dialog->priv->label)
		*label = g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->priv->label)));

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
brasero_burn_option_dialog_cancel_clicked_cb (GtkButton *cancel, GtkWidget *dialog)
{
	gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
}
