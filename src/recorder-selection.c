/***************************************************************************
*            recorder-selection.c
*
*  mer jun 15 12:40:07 2005
*  Copyright  2005  Philippe Rouquier
*  brasero-app@wanadoo.fr
****************************************************************************/

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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtkbutton.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkdialog.h>

#include <nautilus-burn-drive.h>
#include <nautilus-burn-drive-selection.h>
#include <nautilus-burn-recorder.h>
#include <nautilus-burn-drive-monitor.h>

#include "burn-caps.h"
#include "recorder-selection.h"
#include "utils.h"
#include "burn.h"
#include "burn-volume.h"
#include "burn-track.h"
#include "brasero-ncb.h"
#include "brasero-image-type-chooser.h"

static void brasero_recorder_selection_class_init (BraseroRecorderSelectionClass *klass);
static void brasero_recorder_selection_init (BraseroRecorderSelection *sp);
static void brasero_recorder_selection_finalize (GObject * object);

static void brasero_recorder_selection_set_property (GObject *object,
						     guint property_id,
						     const GValue *value,
						     GParamSpec *pspec);
static void brasero_recorder_selection_get_property (GObject *object,
						     guint property_id,
						     GValue *value,
						     GParamSpec *pspec);

static void brasero_recorder_selection_button_cb (GtkWidget *button,
						   BraseroRecorderSelection *selection);

static void brasero_recorder_selection_drive_changed_cb (NautilusBurnDriveSelection *selector, 
							  NautilusBurnDrive *drive,
							  BraseroRecorderSelection *selection);

static void brasero_recorder_selection_update_drive_info (BraseroRecorderSelection *selection);

enum {
	PROP_NONE,
	PROP_SESSION,
};

enum {
	MEDIA_CHANGED_SIGNAL,
	LAST_SIGNAL
};
static guint brasero_recorder_selection_signals [LAST_SIGNAL] = { 0 };

typedef struct {
	gint64 rate;
	BraseroBurnFlag flags;
} BraseroDriveProp;

struct BraseroRecorderSelectionPrivate {
	/* output properties */
	GtkWidget *image_type_widget;
	GtkWidget *selection;
	GHashTable *settings;
	GtkWidget *dialog;

	GtkWidget *notebook;

	GtkWidget *type;
	GtkWidget *capacity;
	GtkWidget *contents;
	GtkWidget *status;
	
	GtkWidget *infos;
	GtkWidget *image;

	GtkWidget *props;
	NautilusBurnDrive *drive;

	gint added_signal;
	gint removed_signal;

	BraseroBurnCaps *caps;
	BraseroBurnSession *session;
	BraseroMediumInfo media;
	BraseroTrackType source;
};

static GObjectClass *parent_class = NULL;



/**
 *
 */

static void
brasero_recorder_selection_update_image_path (BraseroRecorderSelection *selection)
{
	gchar *info;
	gchar *path = NULL;

	path = brasero_recorder_selection_get_image_path (selection);

	info = g_strdup_printf (_("The <b>image</b> will be saved to\n%s"), path ? path:"");
	g_free (path);

	gtk_label_set_markup (GTK_LABEL (selection->priv->infos), info);
	g_free (info);
}

static void
brasero_recorder_selection_set_drive_default_properties (BraseroRecorderSelection *selection,
							 BraseroDriveProp *prop)
{
	/* these are sane defaults */
	BraseroBurnFlag flags = BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE|
				BRASERO_BURN_FLAG_CHECK_SIZE|
				BRASERO_BURN_FLAG_FAST_BLANK|
				BRASERO_BURN_FLAG_BURNPROOF|
				BRASERO_BURN_FLAG_NOGRACE|
				BRASERO_BURN_FLAG_EJECT;

	if (selection->priv->source.type == BRASERO_TRACK_TYPE_DATA
	||  selection->priv->source.type == BRASERO_TRACK_TYPE_DISC
	||  selection->priv->source.type == BRASERO_TRACK_TYPE_IMAGE)
		flags |= BRASERO_BURN_FLAG_NO_TMP_FILES;

	prop->flags = flags;
	prop->rate = NCB_MEDIA_GET_MAX_WRITE_SPEED (selection->priv->drive);
}

/**
 *
 */

static void
brasero_recorder_selection_drive_properties (BraseroRecorderSelection *selection)
{
	NautilusBurnDrive *drive;
	BraseroMediumInfo media;
	BraseroDriveProp *prop;
	BraseroBurnFlag flags = 0;
	BraseroBurnFlag supported = 0;
	BraseroBurnFlag compulsory = 0;
	GtkWidget *combo;
	GtkWindow *toplevel;
	GtkWidget *toggle_otf = NULL;
	GtkWidget *toggle_simulation = NULL;
	GtkWidget *toggle_eject = NULL;
	GtkWidget *toggle_burnproof = NULL;
	GtkWidget *dialog;
	gchar *header, *text;
	gchar *display_name;
	gint result, i;
	gint max_rate, max_speed;
	GSList *list = NULL;

	/* */
	nautilus_burn_drive_ref (selection->priv->drive);
	drive = selection->priv->drive;

	/* dialog */
	display_name = nautilus_burn_drive_get_name_for_display (drive);
	header = g_strdup_printf (_("Properties of %s"), display_name);

	/* search for the main window */
	toplevel = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (selection)));
	dialog = gtk_dialog_new_with_buttons (header,
					      GTK_WINDOW (toplevel),
					      GTK_DIALOG_DESTROY_WITH_PARENT |
					      GTK_DIALOG_MODAL,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					      NULL);
	gtk_window_set_default_size (GTK_WINDOW (dialog), 340, 250);
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	g_free (header);

	/* Speed combo */
	media = NCB_MEDIA_GET_STATUS (drive);
	max_rate = NCB_MEDIA_GET_MAX_WRITE_SPEED (drive);

	combo = gtk_combo_box_new_text ();
	if (media & BRASERO_MEDIUM_DVD)
		max_speed = NAUTILUS_BURN_DRIVE_DVD_SPEED (max_rate);
	else
		max_speed = NAUTILUS_BURN_DRIVE_CD_SPEED (max_rate);

	gtk_combo_box_prepend_text (GTK_COMBO_BOX (combo), _("Max speed"));
	for (i = max_speed; i > 0; i -= 2) {
		text = g_strdup_printf ("%i x (%s)",
					i,
					(media & BRASERO_MEDIUM_DVD) ? _("DVD"):_("CD"));
		gtk_combo_box_append_text (GTK_COMBO_BOX (combo), text);
		g_free (text);
	}

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    brasero_utils_pack_properties (_("<b>Burning speed</b>"),
							   combo, NULL),
			    FALSE, FALSE, 0);

	if (display_name)
		prop = g_hash_table_lookup (selection->priv->settings, display_name); /* FIXME what about drives with the same display names */
	else
		prop = NULL;

	if (!prop) {
		prop = g_new0 (BraseroDriveProp, 1);
		brasero_recorder_selection_set_drive_default_properties (selection, prop);
		g_hash_table_insert (selection->priv->settings,
				     display_name,
				     prop);

		gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
	}
	else {
		gint speed;

		g_free (display_name);

		speed = prop->speed;
		if (!speed || speed >= max_speed)
			gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
		else
			gtk_combo_box_set_active (GTK_COMBO_BOX (combo),
						 (max_speed - speed) / 2 + 1);
	}

	/* properties */
	brasero_burn_caps_get_flags (selection->priv->caps,
				     selection->priv->session,
				     &compulsory,
				     &supported);

	if (supported & BRASERO_BURN_FLAG_DUMMY) {
		toggle_simulation = gtk_check_button_new_with_label (_("Simulate the burning"));
		if (prop->flags & BRASERO_BURN_FLAG_DUMMY)
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle_simulation), TRUE);

		list = g_slist_prepend (list, toggle_simulation);
	}

	if (supported & BRASERO_BURN_FLAG_EJECT) {
		toggle_eject = gtk_check_button_new_with_label (_("Eject after burning"));
		if (prop->flags & BRASERO_BURN_FLAG_EJECT)
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle_eject), TRUE);

		list = g_slist_prepend (list, toggle_eject);
	}

	if (supported & BRASERO_BURN_FLAG_BURNPROOF) {
		toggle_burnproof = gtk_check_button_new_with_label (_("Use burnproof (decrease the risk of failures)"));
		if (prop->flags & BRASERO_BURN_FLAG_BURNPROOF)
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle_burnproof), TRUE);

		list = g_slist_prepend (list, toggle_burnproof);
	}

	if (supported & BRASERO_BURN_FLAG_NO_TMP_FILES) {
		toggle_otf = gtk_check_button_new_with_label (_("Burn the image directly without saving it to disc"));
		if (prop->flags & BRASERO_BURN_FLAG_NO_TMP_FILES)
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle_otf), TRUE);

		if (compulsory & BRASERO_BURN_FLAG_NO_TMP_FILES)
			gtk_widget_set_sensitive (toggle_otf, FALSE);

		list = g_slist_prepend (list, toggle_otf);
	}

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    brasero_utils_pack_properties_list (_("<b>Options</b>"), list),
			    FALSE,
			    FALSE, 0);

	g_slist_free (list);

	gtk_widget_show_all (dialog);
	selection->priv->dialog = dialog;
	result = gtk_dialog_run (GTK_DIALOG (dialog));
	selection->priv->dialog = NULL;

	if (result != GTK_RESPONSE_ACCEPT) {
		nautilus_burn_drive_unref (drive);
		gtk_widget_destroy (dialog);
		return;
	}

	prop->speed = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));
	if (prop->speed == 0)
		prop->speed = max_speed;
	else
		prop->speed = max_speed - (props->speed - 1) * 2;

	flags = prop->flags;

	if (toggle_otf
	&&  gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle_otf)) == TRUE)
		flags |= BRASERO_BURN_FLAG_NO_TMP_FILES;
	else
		flags &= ~BRASERO_BURN_FLAG_NO_TMP_FILES;

	if (toggle_eject
	&&  gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle_eject)) == TRUE)
		flags |= BRASERO_BURN_FLAG_EJECT;
	else
		flags &= ~BRASERO_BURN_FLAG_EJECT;

	if (toggle_simulation
	&&  gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle_simulation)) == TRUE)
		flags |= BRASERO_BURN_FLAG_DUMMY;
	else
		flags &= ~BRASERO_BURN_FLAG_DUMMY;

	if (toggle_burnproof
	&&  gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle_burnproof)) == TRUE)
		flags |= BRASERO_BURN_FLAG_BURNPROOF;
	else
		flags &= ~BRASERO_BURN_FLAG_BURNPROOF;

	prop->flags = flags;

	brasero_burn_session_set_rate (selection->priv->session, rate);
	brasero_burn_session_set_flags (selection->priv->session, prop->flags);

	gtk_widget_destroy (dialog);
	nautilus_burn_drive_unref (drive);
}

static void
brasero_recorder_selection_image_properties (BraseroRecorderSelection *selection)
{
	BraseroTrackType output;
	GtkWindow *toplevel;
	GtkWidget *dialog;
	gchar *image_path;
	gint answer;

	toplevel = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (selection)));
	dialog = gtk_file_chooser_dialog_new (_("Choose a location for the disc image"),
					      GTK_WINDOW (toplevel),
					      GTK_FILE_CHOOSER_ACTION_SAVE,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_SAVE, GTK_RESPONSE_OK,
					      NULL);

	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), TRUE);

	image_path = brasero_recorder_selection_get_image_path (selection);
	if (image_path) {
		gchar *name;

		gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dialog), image_path);

		/* The problem here is that is the file name doesn't exist
		 * in the folder then it won't be displayed so we check that */
		name = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		if (!name) {
			name = g_path_get_basename (image_path);
			gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), name);
		}

		g_free (image_path);
		g_free (name);
	}
	else
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog),
						     g_get_home_dir ());
	
	gtk_widget_show (dialog);
	answer = gtk_dialog_run (GTK_DIALOG (dialog));

	if (answer != GTK_RESPONSE_OK) {
		gtk_widget_destroy (dialog);
		return;
	}

	image_path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

	output.type = BRASERO_TRACK_TYPE_IMAGE;
	output.subtype.img_format = brasero_recorder_selection_get_image_format (selection);
	brasero_burn_session_set_output (selection->priv->session, &output, image_path);
	g_free (image_path);

	gtk_widget_destroy (dialog);
}

static void
brasero_recorder_selection_disc_image_properties (BraseroRecorderSelection *selection)
{
	GtkWidget *format_chooser;
	BraseroTrackType output;
	BraseroTrackType source;
	GtkWindow *toplevel;
	GtkWidget *chooser;
	GtkWidget *dialog;
	gchar *image_path;
	GtkWidget *vbox;
	gint answer;

	toplevel = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (selection)));
	dialog = gtk_dialog_new_with_buttons (_("Disc image file properties"),
					      GTK_WINDOW (toplevel),
					      GTK_DIALOG_MODAL|
					      GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_APPLY, GTK_RESPONSE_OK,
					      NULL);
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

	vbox = gtk_vbox_new (FALSE, 12);
	gtk_widget_show (vbox);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox, TRUE, TRUE, 4);

	chooser = gtk_file_chooser_widget_new (GTK_FILE_CHOOSER_ACTION_SAVE);
	gtk_widget_show_all (chooser);

	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (chooser), TRUE);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), TRUE);

	/* we reset the previous settings */
	image_path = brasero_recorder_selection_get_image_path (selection);
	if (image_path) {
		gchar *name;

		gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (chooser),
					       image_path);

		/* The problem here is that is the file name doesn't exist
		 * in the folder then it won't be displayed so we check that */
		name = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
		if (!name) {
			name = g_path_get_basename (image_path);
			gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (chooser), name);
		}

		g_free (image_path);
	    	g_free (name);
	}
	else
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser),
						     g_get_home_dir ());

	gtk_box_pack_start (GTK_BOX (vbox), chooser, TRUE, TRUE, 0);

	format_chooser = brasero_image_type_chooser_new ();
	gtk_box_pack_start (GTK_BOX (vbox), format_chooser, FALSE, FALSE, 0);
	gtk_widget_show (format_chooser);

	brasero_burn_session_get_output_type (selection->priv->session, &output);
	brasero_burn_session_get_track_type (selection->priv->session, &source);
	brasero_image_type_chooser_set_source (BRASERO_IMAGE_TYPE_CHOOSER (format_chooser),
					       selection->priv->drive,
					       &source,
					       output.subtype.img_format);

	/* and here we go */
	gtk_widget_show (dialog);

	selection->priv->image_type_widget = format_chooser;
	answer = gtk_dialog_run (GTK_DIALOG (dialog));
	selection->priv->image_type_widget = NULL;

	if (answer != GTK_RESPONSE_OK) {
		gtk_widget_destroy (dialog);
		return;
	}

	image_path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
	brasero_image_type_chooser_get_format (BRASERO_IMAGE_TYPE_CHOOSER (format_chooser),
					       &output.subtype.img_format);

	brasero_burn_session_set_output (selection->priv->session, &output, image_path);
	g_free (image_path);

	gtk_widget_destroy (dialog);
}

static void
brasero_recorder_selection_button_cb (GtkWidget *button,
				      BraseroRecorderSelection *selection)
{
	if (NCB_DRIVE_GET_TYPE (selection->priv->drive) == NAUTILUS_BURN_DRIVE_TYPE_FILE) {
		BraseroTrackType source;

		brasero_burn_session_get_track_type (selection->priv->session, &source);
		if (source.type == BRASERO_TRACK_TYPE_DISC)
			brasero_recorder_selection_disc_image_properties (selection);
		else
			brasero_recorder_selection_image_properties (selection);

		/* we update the path of the future image */
		brasero_recorder_selection_update_image_path (selection);
	}
	else
		brasero_recorder_selection_drive_properties (selection);
}

/**
 *
 */

static void
brasero_recorder_selection_create_prop_button (BraseroRecorderSelection *selection)
{
	GtkWidget *parent;

	selection->priv->props = gtk_button_new_from_stock (GTK_STOCK_PROPERTIES);
	g_signal_connect (G_OBJECT (selection->priv->props),
			  "clicked",
			  G_CALLBACK (brasero_recorder_selection_button_cb),
			  selection);

	parent = gtk_widget_get_parent (selection->priv->selection);
	gtk_box_pack_start (GTK_BOX (parent), selection->priv->props, FALSE, FALSE, 0);
}

static void
brasero_recorder_selection_set_source_type (BraseroRecorderSelection *selection)
{
	BraseroTrackType source;
	BraseroTrackType output;

	if (!selection->priv->session)
		return;

	brasero_burn_session_get_track_type (selection->priv->session, &source);
	if (source.type != BRASERO_TRACK_TYPE_DISC
	||  NCB_DRIVE_GET_TYPE (selection->priv->drive) != NAUTILUS_BURN_DRIVE_TYPE_FILE)
		return;

	/* in case the user asked to copy to a file on the hard disk then we 
	 * need to update the name to change the extension if it is now a DVD
	 * an it wasn't before */
	brasero_recorder_selection_update_image_path (selection);

	/* try to see if we need to update the image
	 * type selection when copying a drive */
	brasero_burn_session_get_output_type (selection->priv->session, &output);
	brasero_image_type_chooser_set_source (BRASERO_IMAGE_TYPE_CHOOSER (selection->priv->image_type_widget),
					       selection->priv->drive,
					       &source,
					       output.type == BRASERO_TRACK_TYPE_IMAGE?
					       output.subtype.img_format:BRASERO_TRACK_TYPE_NONE);
}

static void
brasero_recorder_selection_set_property (GObject *object,
					 guint property_id,
					 const GValue *value,
					 GParamSpec *pspec)
{
	BraseroRecorderSelection *selection;
	BraseroBurnSession *session;

	selection = BRASERO_RECORDER_SELECTION (object);
	switch (property_id) {
	case PROP_SESSION:
		session = g_value_get_object (value);
		g_object_unref (selection->priv->session);
		selection->priv->session = session;

		if (session) {
			g_object_ref (session);
			g_object_set (G_OBJECT (selection->priv->selection),
				      "file-image", TRUE,
				      NULL);

			if (!selection->priv->props) {
				brasero_recorder_selection_create_prop_button (selection);
				gtk_widget_show (selection->priv->props);
			}

			g_object_set (G_OBJECT (selection->priv->selection),
				      "show-recorders-only", TRUE,
				      NULL);

			brasero_recorder_selection_set_source_type (selection);
		}
		else {
			g_object_set (G_OBJECT (selection->priv->selection),
				      "file-image", FALSE,
				      NULL);

			if (selection->priv->props) {
				gtk_widget_destroy (selection->priv->props);
				selection->priv->props = NULL;
			}
		
			g_object_set (G_OBJECT (selection->priv->selection),
				      "show-recorders-only", FALSE,
				      NULL);
		}

		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
brasero_recorder_selection_set_image_properties (BraseroRecorderSelection *selection,
						 BraseroBurnSession *session)
{
	BraseroTrackType type;

	type.type = BRASERO_TRACK_TYPE_IMAGE;
	type.subtype.img_format = brasero_recorder_selection_get_image_format (selection);

	if (!selection->priv->image_path) {
		gchar *output;

		output = brasero_recorder_selection_get_image_path (selection);
		brasero_burn_session_set_output (session, &type, output);
		g_free (output);
	}
	else
		brasero_burn_session_set_output (session, 
						 &type,
						 selection->priv->image_path);
}


static void
brasero_recorder_selection_init (BraseroRecorderSelection *obj)
{

}

static void
brasero_recorder_selection_finalize (GObject *object)
{
	BraseroRecorderSelection *cobj;




	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_recorder_selection_get_property (GObject *object,
					 guint property_id,
					 GValue *value,
					 GParamSpec *pspec)
{
	BraseroRecorderSelection *selection;

	selection = BRASERO_RECORDER_SELECTION (object);
	switch (property_id) {
	case PROP_SESSION:
		g_value_set_object (value, selection->priv->session);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

GtkWidget *
brasero_recorder_selection_new (BraseroBurnSession *session)
{
	BraseroRecorderSelection *obj;

	obj = BRASERO_RECORDER_SELECTION (g_object_new (BRASERO_TYPE_RECORDER_SELECTION,
					                "session", session,
							NULL));

	return GTK_WIDGET (obj);
}


void
brasero_recorder_selection_get_media (BraseroRecorderSelection *selection,
				      BraseroMediumInfo *media)
{
	if (!media)
		return;

	if (selection->priv->drive)
		*media = NCB_MEDIA_GET_STATUS (selection->priv->drive);
	else
		*media = BRASERO_MEDIUM_NONE;
}

GType
brasero_recorder_selection_get_type ()
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroRecorderSelectionClass),
			NULL,
			NULL,
			(GClassInitFunc)
			    brasero_recorder_selection_class_init,
			NULL,
			NULL,
			sizeof (BraseroRecorderSelection),
			0,
			(GInstanceInitFunc)
			    brasero_recorder_selection_init,
		};

		type = g_type_register_static (GTK_TYPE_VBOX,
					       "BraseroRecorderSelection",
					       &our_info, 0);
	}

	return type;
}

static void
brasero_recorder_selection_class_init (BraseroRecorderSelectionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_recorder_selection_finalize;
	object_class->set_property = brasero_recorder_selection_set_property;
	object_class->get_property = brasero_recorder_selection_get_property;

	brasero_recorder_selection_signals [MEDIA_CHANGED_SIGNAL] =
	    g_signal_new ("media_changed",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_FIRST,
			  G_STRUCT_OFFSET (BraseroRecorderSelectionClass,
					   media_changed),
			  NULL, NULL,
			  g_cclosure_marshal_VOID__INT,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_INT);

	g_object_class_install_property (object_class,
					 PROP_SESSION,
					 g_param_spec_object ("session", NULL, NULL,
							      BRASERO_TYPE_BURN_SESSION, G_PARAM_READWRITE));
}



