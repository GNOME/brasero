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
#include "brasero-ncb.h"
#include "brasero-image-type-chooser.h"

#define ICON_SIZE 48

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
	PROP_IMAGE,
	PROP_SHOW_PROPS,
	PROP_SHOW_RECORDERS_ONLY
};

enum {
	MEDIA_CHANGED_SIGNAL,
	LAST_SIGNAL
};
static guint brasero_recorder_selection_signals [LAST_SIGNAL] = { 0 };


struct BraseroRecorderSelectionPrivate {
	/* output properties */
	gchar *image_path;
	BraseroImageFormat image_format;

	GtkWidget *image_type_widget;
	GtkWidget *selection;
	GHashTable *settings;
	GtkWidget *dialog;
	GtkWidget *infos;
	GtkWidget *image;
	GtkWidget *props;
	NautilusBurnDrive *drive;

	gint added_signal;
	gint removed_signal;

	BraseroBurnCaps *caps;
	NautilusBurnMediaType media_type;
	BraseroTrackSource *track_source;
};

static GObjectClass *parent_class = NULL;

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
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (BraseroRecorderSelectionClass,
					   media_changed),
			  NULL, NULL,
			  g_cclosure_marshal_VOID__INT,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_INT);

	g_object_class_install_property (object_class,
					 PROP_IMAGE,
					 g_param_spec_boolean ("file-image", NULL, NULL,
							       FALSE, G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_SHOW_PROPS,
					 g_param_spec_boolean ("show-properties", NULL, NULL,
							       TRUE, G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_SHOW_RECORDERS_ONLY,
					 g_param_spec_boolean ("show-recorders-only", NULL, NULL,
							       TRUE, G_PARAM_READWRITE));
}

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
brasero_recorder_selection_set_property (GObject *object,
					 guint property_id,
					 const GValue *value,
					 GParamSpec *pspec)
{
	BraseroRecorderSelection *selection;

	selection = BRASERO_RECORDER_SELECTION (object);
	switch (property_id) {
	case PROP_IMAGE:
		g_object_set_property (G_OBJECT (selection->priv->selection),
				       "file-image",
				       value);
		break;
	case PROP_SHOW_PROPS:
		if (g_value_get_boolean (value))  {
			if (!selection->priv->props) {
				brasero_recorder_selection_create_prop_button (selection);
				gtk_widget_show (selection->priv->props);
			}
		}
		else {
			gtk_widget_destroy (selection->priv->props);
			selection->priv->props = NULL;
		}
		break;
	case PROP_SHOW_RECORDERS_ONLY:
		g_object_set_property (G_OBJECT (selection->priv->selection),
				       "show-recorders-only",
				       value);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
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
	case PROP_IMAGE:
		g_object_get_property (G_OBJECT (selection->priv->selection),
				       "file-image",
				       value);
		break;
	case PROP_SHOW_PROPS:
		g_value_set_boolean (value, GTK_WIDGET_VISIBLE (selection->priv->props));
		break;
	case PROP_SHOW_RECORDERS_ONLY:
		g_object_get_property (G_OBJECT (selection->priv->selection),
				       "show-recorders-only",
				       value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
brasero_recorder_selection_init (BraseroRecorderSelection *obj)
{
	GtkWidget *box;

	obj->priv = g_new0 (BraseroRecorderSelectionPrivate, 1);
	gtk_box_set_spacing (GTK_BOX (obj), 12);

	obj->priv->caps = brasero_burn_caps_get_default ();
	obj->priv->settings = g_hash_table_new_full (g_str_hash,
						     g_str_equal,
						     g_free,
						     g_free);

	box = gtk_hbox_new (FALSE, 12);
	obj->priv->selection = nautilus_burn_drive_selection_new ();
	gtk_box_pack_start (GTK_BOX (box),
			    obj->priv->selection,
			    FALSE,
			    FALSE,
			    0);

	brasero_recorder_selection_create_prop_button (obj);
	gtk_box_pack_start (GTK_BOX (obj), box, FALSE, FALSE, 0);
	gtk_widget_show_all (box);

	box = gtk_hbox_new (FALSE, 12);
	obj->priv->image = gtk_image_new ();
	obj->priv->infos = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (obj->priv->infos), 0.0, 0.0);

	gtk_box_pack_start (GTK_BOX (box), obj->priv->image, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), obj->priv->infos, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (obj), box, FALSE, FALSE, 0);

	g_signal_connect (obj->priv->selection,
			  "drive-changed",
			  G_CALLBACK (brasero_recorder_selection_drive_changed_cb),
			  obj);

	obj->priv->image_format = BRASERO_IMAGE_FORMAT_ANY;
	gtk_widget_show_all (box);
}

static void
brasero_recorder_selection_finalize (GObject *object)
{
	BraseroRecorderSelection *cobj;
	NautilusBurnDriveMonitor *monitor;

	cobj = BRASERO_RECORDER_SELECTION (object);
	monitor = nautilus_burn_get_drive_monitor ();

	if (cobj->priv->image_path) {
		g_free (cobj->priv->image_path);
		cobj->priv->image_path = NULL;
	}

	if (cobj->priv->added_signal) {
		g_signal_handler_disconnect (monitor,
					     cobj->priv->added_signal);
		cobj->priv->added_signal = 0;
	}

	if (cobj->priv->removed_signal) {
		g_signal_handler_disconnect (monitor,
					     cobj->priv->removed_signal);
		cobj->priv->removed_signal = 0;
	}

	if (cobj->priv->caps) {
		g_object_unref (cobj->priv->caps);
		cobj->priv->caps = NULL;
	}

	if (cobj->priv->track_source) {
		brasero_track_source_free (cobj->priv->track_source);
		cobj->priv->track_source = NULL;
	}

	g_hash_table_destroy (cobj->priv->settings);

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gchar *
brasero_recorder_selection_get_new_image_path (BraseroRecorderSelection *selection)
{
	BraseroImageFormat image_format;
	const gchar *suffixes [] = {".iso",
				    ".raw",
				    ".cue",
				    ".toc",
				    NULL };
	const gchar *suffix;
	gchar *path;
	gint i = 0;

	image_format = selection->priv->image_format;
	if (image_format == BRASERO_IMAGE_FORMAT_ANY)
		image_format = brasero_burn_caps_get_imager_default_format (selection->priv->caps,
									    selection->priv->track_source);

	if (image_format & BRASERO_IMAGE_FORMAT_ISO)
		suffix = suffixes [0];
	else if (image_format & BRASERO_IMAGE_FORMAT_CLONE)
		suffix = suffixes [1];
	else if (image_format & BRASERO_IMAGE_FORMAT_CUE)
		suffix = suffixes [2];
	else if (image_format & BRASERO_IMAGE_FORMAT_CDRDAO)
		suffix = suffixes [3];
	else
		return NULL;
	
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

	return path;
}

static void
brasero_recorder_selection_set_image_properties (BraseroRecorderSelection *selection,
						 BraseroDriveProp *props)
{
	if (!selection->priv->image_path)
		props->output_path = brasero_recorder_selection_get_new_image_path (selection);
	else
		props->output_path = g_strdup (selection->priv->image_path);

	props->props.image_format = selection->priv->image_format;
	brasero_burn_caps_get_flags (selection->priv->caps,
				     selection->priv->track_source,
				     selection->priv->drive,
				     &props->flags,
				     NULL,
				     NULL);
}

static gboolean
brasero_recorder_selection_set_drive_default_properties (BraseroRecorderSelection *selection,
							 BraseroDriveProp *props)
{
	BraseroBurnFlag default_flags;
	gint max_rate;

	if (brasero_burn_caps_get_flags (selection->priv->caps,
					 selection->priv->track_source,
					 selection->priv->drive,
					 &default_flags,
					 NULL,
					 NULL) != BRASERO_BURN_OK)
		return FALSE;

	props->props.image_format = BRASERO_IMAGE_FORMAT_ANY;

	max_rate = nautilus_burn_drive_get_max_speed_write (selection->priv->drive);
	if (NAUTILUS_BURN_DRIVE_MEDIA_TYPE_IS_DVD (nautilus_burn_drive_get_media_type (selection->priv->drive)))
		props->props.drive_speed = NAUTILUS_BURN_DRIVE_DVD_SPEED (max_rate);
	else
		props->props.drive_speed = NAUTILUS_BURN_DRIVE_CD_SPEED (max_rate);
	
	props->flags = default_flags;
	return TRUE;
}

static gboolean
brasero_recorder_selection_update_info (BraseroRecorderSelection *selection,
					NautilusBurnDrive *drive)
{
	gchar *info;
	GdkPixbuf *pixbuf = NULL;
	gboolean can_record = FALSE;
	gchar *types [] = { 	NULL,
				NULL,
				NULL,
				"gnome-dev-cdrom",
				"gnome-dev-disc-cdr",
				"gnome-dev-disc-cdrw",
				"gnome-dev-disc-dvdrom",
				"gnome-dev-disc-dvdr",
				"gnome-dev-disc-dvdrw",
				"gnome-dev-disc-dvdram",
				"gnome-dev-disc-dvdr-plus",
				"gnome-dev-disc-dvdrw", /* FIXME */
				"gnome-dev-disc-dvdr-plus" /* FIXME */,
				NULL };
	gboolean is_appendable;
	NautilusBurnMediaType type;
	gboolean is_rewritable, has_audio, has_data, is_blank;

	if (drive) {
		type = nautilus_burn_drive_get_media_type_full (drive,
								&is_blank,
								&is_rewritable,
								&has_data,
								&has_audio);

		is_appendable = nautilus_burn_drive_media_is_appendable (drive);
	}
	else {
		has_audio = has_data = is_blank = is_appendable = is_rewritable = FALSE;
		type = NAUTILUS_BURN_MEDIA_TYPE_ERROR;
	}

	selection->priv->media_type = type;

	if (type == NAUTILUS_BURN_MEDIA_TYPE_BUSY) {
		info = g_strdup (_("<i>The disc is busy.</i>"));
		can_record = TRUE;
	}
	else if (type == NAUTILUS_BURN_MEDIA_TYPE_ERROR) {
		info = g_strdup (_("<i>There is no disc in the drive.</i>"));
	}
	else if (type == NAUTILUS_BURN_MEDIA_TYPE_UNKNOWN) {
		info = g_strdup (_("<i>Unknown type of disc.</i>"));
	}
	else if ((!is_appendable && !is_blank && !is_rewritable)
	     || ((is_appendable && has_audio) && !is_blank && !is_rewritable)) {
		info = g_strdup_printf (_("The <b>%s</b> is not writable."),
					nautilus_burn_drive_media_type_get_string (type));
		pixbuf = brasero_utils_get_icon (types [type], ICON_SIZE);
	}
	else if (has_audio) {
		if (is_appendable) {
			gchar *size;
			GnomeVFSFileSize remaining;

			remaining = NCB_MEDIA_GET_CAPACITY (drive) - NCB_MEDIA_GET_SIZE (drive);
			size = gnome_vfs_format_file_size_for_display (remaining);
			info = g_strdup_printf (_("The <b>%s</b> is ready.\nIt contains audio tracks.\nA data session can be added (%s free)."),
						nautilus_burn_drive_media_type_get_string (type),
						size);
			g_free (size);
		}
		else
			info = g_strdup_printf (_("The <b>%s</b> is ready.\nIt contains audio tracks."),
						nautilus_burn_drive_media_type_get_string (type));

		pixbuf = brasero_utils_get_icon (types [type], ICON_SIZE);
		can_record = TRUE;
	}
	else if (has_data) {
		if (is_appendable) {
			gchar *size;
			GnomeVFSFileSize remaining;

			remaining = NCB_MEDIA_GET_CAPACITY (drive) - NCB_MEDIA_GET_SIZE (drive);
			size = gnome_vfs_format_file_size_for_display (remaining);
			info = g_strdup_printf (_("The <b>%s</b> is ready.\nIt contains data.\nMore data can be added (%s free)."),
						nautilus_burn_drive_media_type_get_string (type),
						size);
			g_free (size);
		}
		else
			info = g_strdup_printf (_("The <b>%s</b> is ready.\nIt contains data."),
						nautilus_burn_drive_media_type_get_string (type));

		pixbuf = brasero_utils_get_icon (types [type], ICON_SIZE);
		can_record = TRUE;
	}
	else {
		info = g_strdup_printf (_("The <b>%s</b> is ready.\nIt is empty."),
					nautilus_burn_drive_media_type_get_string (type));
		pixbuf = brasero_utils_get_icon (types [type], ICON_SIZE);
		can_record = TRUE;
	}

	if (selection->priv->props) {
		if (can_record)
			gtk_widget_set_sensitive (selection->priv->props, TRUE);
		else
			gtk_widget_set_sensitive (selection->priv->props, FALSE);
	}

	gtk_label_set_markup (GTK_LABEL (selection->priv->infos), info);
	if (!pixbuf)
		pixbuf = brasero_utils_get_icon ("gnome-dev-removable", ICON_SIZE);

	gtk_image_set_from_pixbuf (GTK_IMAGE (selection->priv->image), pixbuf);
	g_object_unref (pixbuf);
	g_free (info);

	return can_record;
}

static void
brasero_recorder_selection_drive_media_added_cb (NautilusBurnDriveMonitor *monitor,
						 NautilusBurnDrive *drive,
						 BraseroRecorderSelection *selection)
{
	NautilusBurnMediaType type;
	NautilusBurnDrive *selected_drive = NULL;

	/* we must make sure that the change was triggered
	 * by the current selected drive */
	selected_drive =  
		nautilus_burn_drive_selection_get_active (NAUTILUS_BURN_DRIVE_SELECTION (selection->priv->selection));

	if (!selected_drive)
		return;

	if (!nautilus_burn_drive_equal (drive, selected_drive)) {
		nautilus_burn_drive_unref (selected_drive);
		return;
	}

	nautilus_burn_drive_unref (selected_drive);

	brasero_recorder_selection_update_info (selection, drive);

	type = nautilus_burn_drive_get_media_type (drive);
	g_signal_emit (selection,
		       brasero_recorder_selection_signals [MEDIA_CHANGED_SIGNAL],
		       0,
		       type);
}

static void
brasero_recorder_selection_drive_media_removed_cb (NautilusBurnDriveMonitor *monitor,
						   NautilusBurnDrive *drive,
						   BraseroRecorderSelection *selection)
{
	NautilusBurnDrive *selected_drive = NULL;

	/* we must make sure that the change was triggered
	 * by the current selected drive */
	selected_drive =  
		nautilus_burn_drive_selection_get_active (NAUTILUS_BURN_DRIVE_SELECTION (selection->priv->selection));

	if (!selected_drive)
		return;

	if (!nautilus_burn_drive_equal (drive, selected_drive)) {
		nautilus_burn_drive_unref (selected_drive);
		return;
	}

	nautilus_burn_drive_unref (selected_drive);

	if (selection->priv->dialog)
		gtk_dialog_response (GTK_DIALOG (selection->priv->dialog),
				     GTK_RESPONSE_CANCEL);

	/* we don't look at the drive contents since we already
	 * know that there is nothing inside. If we did, it could
	 * force the drive to reload the disc */
	brasero_recorder_selection_update_info (selection, NULL);
	g_signal_emit (selection,
		       brasero_recorder_selection_signals [MEDIA_CHANGED_SIGNAL],
		       0,
		       NAUTILUS_BURN_MEDIA_TYPE_ERROR);
}

static void
brasero_recorder_selection_update_image_path (BraseroRecorderSelection *selection)
{
	gchar *info;

	if (!selection->priv->image_path) {
		gchar *path;

		path = brasero_recorder_selection_get_new_image_path (selection);
		info = g_strdup_printf (_("The <b>image</b> will be saved to\n%s"),
					path);
		g_free (path);
	}
	else
		info = g_strdup_printf (_("The <b>image</b> will be saved to\n%s"),
					selection->priv->image_path);
	gtk_label_set_markup (GTK_LABEL (selection->priv->infos), info);
	g_free (info);
}

static void
brasero_recorder_selection_update_drive_info (BraseroRecorderSelection *selection)
{
	guint added_signal = 0;
	guint removed_signal = 0;
	NautilusBurnDrive *drive;
	NautilusBurnMediaType type;
	gboolean can_record = FALSE;
	NautilusBurnDriveMonitor *monitor;

	monitor = nautilus_burn_get_drive_monitor ();
	drive = nautilus_burn_drive_selection_get_active (NAUTILUS_BURN_DRIVE_SELECTION (selection->priv->selection));

	if (drive == NULL) {
		GdkPixbuf *pixbuf;

	    	gtk_widget_set_sensitive (selection->priv->selection, FALSE);
		gtk_label_set_markup (GTK_LABEL (selection->priv->infos),
				      _("<b>There is no available drive.</b>"));

		type = NAUTILUS_BURN_MEDIA_TYPE_ERROR;
		pixbuf = brasero_utils_get_icon ("gnome-dev-removable", ICON_SIZE);
		gtk_image_set_from_pixbuf (GTK_IMAGE (selection->priv->image), pixbuf);
		g_object_unref (pixbuf);
		goto end;
	}

	if (selection->priv->drive
	&&  nautilus_burn_drive_equal (selection->priv->drive, drive)) {
		nautilus_burn_drive_unref (drive);
		return;
	}

	if (NCB_DRIVE_GET_TYPE (drive) == NAUTILUS_BURN_DRIVE_TYPE_FILE) {
		GdkPixbuf *pixbuf;

		brasero_recorder_selection_update_image_path (selection);

		type = NAUTILUS_BURN_MEDIA_TYPE_ERROR;
		pixbuf = brasero_utils_get_icon_for_mime ("application/x-cd-image", ICON_SIZE);
		gtk_image_set_from_pixbuf (GTK_IMAGE (selection->priv->image), pixbuf);
		g_object_unref (pixbuf);

		gtk_widget_set_sensitive (selection->priv->props, TRUE);
		goto end;
	}

	can_record = brasero_recorder_selection_update_info (selection, drive);
	added_signal = g_signal_connect (G_OBJECT (monitor),
					 "media-added",
					 G_CALLBACK (brasero_recorder_selection_drive_media_added_cb),
					 selection);
	removed_signal = g_signal_connect (G_OBJECT (monitor),
					   "media-removed",
					   G_CALLBACK (brasero_recorder_selection_drive_media_removed_cb),
					   selection);

end:

	if (selection->priv->added_signal) {
		g_signal_handler_disconnect (monitor,
					     selection->priv->added_signal);
		selection->priv->added_signal = 0;
	}

	if (selection->priv->removed_signal) {
		g_signal_handler_disconnect (monitor,
					     selection->priv->removed_signal);
		selection->priv->removed_signal = 0;
	}

	if (selection->priv->drive)
		nautilus_burn_drive_unref (selection->priv->drive);

	selection->priv->drive = drive;
	selection->priv->added_signal = added_signal;
	selection->priv->removed_signal = removed_signal;
	g_signal_emit (selection,
		       brasero_recorder_selection_signals [MEDIA_CHANGED_SIGNAL],
		       0,
		       type);
}

static void
brasero_recorder_selection_drive_changed_cb (NautilusBurnDriveSelection *selector,
					     NautilusBurnDrive *drive,
					     BraseroRecorderSelection *selection)
{
	brasero_recorder_selection_update_drive_info (selection);
}

GtkWidget *
brasero_recorder_selection_new (void)
{
	BraseroRecorderSelection *obj;

	obj = BRASERO_RECORDER_SELECTION (g_object_new (BRASERO_TYPE_RECORDER_SELECTION,
					                NULL));

	return GTK_WIDGET (obj);
}

void
brasero_recorder_selection_set_source_track (BraseroRecorderSelection *selection,
					     const BraseroTrackSource *source)
{
	if (selection->priv->track_source)
		brasero_track_source_free (selection->priv->track_source);

	selection->priv->track_source = brasero_track_source_copy (source);
	if (source->type == BRASERO_TRACK_SOURCE_DISC) {
		if (NCB_DRIVE_GET_TYPE (selection->priv->drive) == NAUTILUS_BURN_DRIVE_TYPE_FILE) {
			/* in case the user asked to copy to a file on the hard disk
			 * then we need to update the name to change the extension
			 * if it is now a DVD an it wasn't before */
			brasero_recorder_selection_update_image_path (selection);
		}

		/* try to see if we need to update the image
		 * type selection when copying a drive */
		if (!selection->priv->image_type_widget)
			return;

		brasero_image_type_chooser_set_source (BRASERO_IMAGE_TYPE_CHOOSER (selection->priv->image_type_widget),
						       selection->priv->drive,
						       source->type,
						       selection->priv->image_format);
	}
}

static void
brasero_recorder_selection_drive_properties (BraseroRecorderSelection *selection)
{
	NautilusBurnDrive *drive;
	NautilusBurnMediaType media;
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
	media = nautilus_burn_drive_get_media_type (drive);
	max_rate = nautilus_burn_drive_get_max_speed_write (drive);

	combo = gtk_combo_box_new_text ();
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Max speed"));
	
	if (NAUTILUS_BURN_DRIVE_MEDIA_TYPE_IS_DVD (media))
		max_speed = NAUTILUS_BURN_DRIVE_DVD_SPEED (max_rate);
	else
		max_speed = NAUTILUS_BURN_DRIVE_CD_SPEED (max_rate);

	for (i = 2; i <= max_speed; i += 2) {
		text = g_strdup_printf ("%i x (%s)",
					i,
					NAUTILUS_BURN_DRIVE_MEDIA_TYPE_IS_DVD (media) ? _("DVD"):_("CD"));
		gtk_combo_box_append_text (GTK_COMBO_BOX (combo), text);
		g_free (text);
	}

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    brasero_utils_pack_properties (_("<b>Burning speed</b>"),
							   combo, NULL),
			    FALSE, FALSE, 0);

	prop = g_hash_table_lookup (selection->priv->settings,
				    display_name); /* FIXME what about drives with the same display names */

	if (!prop) {
		prop = g_new0 (BraseroDriveProp, 1);
		brasero_recorder_selection_set_drive_default_properties (selection, prop);
		g_hash_table_insert (selection->priv->settings,
				     display_name,
				     prop);
	}
	else
		g_free (display_name);

	if (prop->props.drive_speed == 0
	||  prop->props.drive_speed >= nautilus_burn_drive_get_max_speed_write (drive))
		gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
	else
		gtk_combo_box_set_active (GTK_COMBO_BOX (combo), prop->props.drive_speed / 2);

	/* properties */
	brasero_burn_caps_get_flags (selection->priv->caps,
				     selection->priv->track_source,
				     selection->priv->drive,
				     NULL,
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

	if (supported & BRASERO_BURN_FLAG_ON_THE_FLY) {
		toggle_otf = gtk_check_button_new_with_label (_("Burn the image directly without saving it to disc"));
		if (prop->flags & BRASERO_BURN_FLAG_ON_THE_FLY)
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle_otf), TRUE);

		if (compulsory & BRASERO_BURN_FLAG_ON_THE_FLY)
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

	prop->props.drive_speed = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));
	if (prop->props.drive_speed == 0) {
		if (NAUTILUS_BURN_DRIVE_MEDIA_TYPE_IS_DVD (media))
			prop->props.drive_speed = NAUTILUS_BURN_DRIVE_DVD_SPEED (max_rate);
		else
			prop->props.drive_speed = NAUTILUS_BURN_DRIVE_CD_SPEED (max_rate);
	}
	else
		prop->props.drive_speed = prop->props.drive_speed * 2;

	flags = prop->flags;

	if (toggle_otf
	&&  gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle_otf)) == TRUE)
		flags |= BRASERO_BURN_FLAG_ON_THE_FLY;
	else
		flags &= ~BRASERO_BURN_FLAG_ON_THE_FLY;

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

	gtk_widget_destroy (dialog);
	nautilus_burn_drive_unref (drive);
}

static void
brasero_recorder_selection_image_properties (BraseroRecorderSelection *selection)
{
	GtkWindow *toplevel;
	GtkWidget *dialog;
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

	if (selection->priv->image_path) {
		gchar *name;

		gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dialog),
					       selection->priv->image_path);

		/* The problem here is that is the file name doesn't exist
		 * in the folder then it won't be displayed so we check that */
		name = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		if (!name) {
			name = g_path_get_basename (selection->priv->image_path);
			gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), name);
		}

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

	if (selection->priv->image_path)
		g_free (selection->priv->image_path);

	selection->priv->image_path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
	gtk_widget_destroy (dialog);
}

static void
brasero_recorder_selection_disc_image_properties (BraseroRecorderSelection *selection)
{
	GtkWidget *format_chooser;
	GtkWindow *toplevel;
	GtkWidget *chooser;
	GtkWidget *dialog;
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
	if (selection->priv->image_path) {
		gchar *name;

		gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (chooser),
					       selection->priv->image_path);

		/* The problem here is that is the file name doesn't exist
		 * in the folder then it won't be displayed so we check that */
		name = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
		if (!name) {
			name = g_path_get_basename (selection->priv->image_path);
			gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (chooser), name);
		}
	    	g_free (name);
	}
	else
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser),
						     g_get_home_dir ());

	gtk_box_pack_start (GTK_BOX (vbox), chooser, TRUE, TRUE, 0);

	format_chooser = brasero_image_type_chooser_new ();
	gtk_box_pack_start (GTK_BOX (vbox), format_chooser, FALSE, FALSE, 0);
	gtk_widget_show (format_chooser);

	brasero_image_type_chooser_set_source (BRASERO_IMAGE_TYPE_CHOOSER (format_chooser),
					       selection->priv->drive,
					       selection->priv->track_source->type,
					       selection->priv->image_format);

	/* and here we go */
	gtk_widget_show (dialog);

	selection->priv->image_type_widget = format_chooser;
	answer = gtk_dialog_run (GTK_DIALOG (dialog));
	selection->priv->image_type_widget = NULL;

	if (answer != GTK_RESPONSE_OK)
		goto end;

	if (selection->priv->image_path)
		g_free (selection->priv->image_path);

	selection->priv->image_path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));

	brasero_image_type_chooser_get_format (BRASERO_IMAGE_TYPE_CHOOSER (format_chooser),
					       &selection->priv->image_format);

end:
	gtk_widget_destroy (dialog);
}

static void
brasero_recorder_selection_button_cb (GtkWidget *button,
				      BraseroRecorderSelection *selection)
{
	if (NCB_DRIVE_GET_TYPE (selection->priv->drive) == NAUTILUS_BURN_DRIVE_TYPE_FILE) {
		if (selection->priv->track_source
		&&  selection->priv->track_source->type == BRASERO_TRACK_SOURCE_DISC)
			brasero_recorder_selection_disc_image_properties (selection);
		else
			brasero_recorder_selection_image_properties (selection);

		/* we update the path of the future image */
		brasero_recorder_selection_update_image_path (selection);
	}
	else
		brasero_recorder_selection_drive_properties (selection);
}

void
brasero_recorder_selection_get_drive (BraseroRecorderSelection *selection,
				      NautilusBurnDrive **drive,
				      BraseroDriveProp *props)
{
	BraseroDriveProp *setting = { 0, };
	gchar *display_name;

	g_return_if_fail (drive != NULL);

	if (!selection->priv->drive) {
		*drive = NULL;
		if (props) {
			props->flags = BRASERO_BURN_FLAG_NONE;
			props->props.drive_speed = 0;
		}
		return;
	}

	nautilus_burn_drive_ref (selection->priv->drive);
	*drive = selection->priv->drive;

	if (!props)
		return;

	if (NCB_DRIVE_GET_TYPE (selection->priv->drive) == NAUTILUS_BURN_DRIVE_TYPE_FILE) {
		brasero_recorder_selection_set_image_properties (selection, props);
		return;
	}

	display_name = nautilus_burn_drive_get_name_for_display (selection->priv->drive);
	setting = g_hash_table_lookup (selection->priv->settings,
				       display_name);
	g_free (display_name);

	if (!setting) {
		brasero_recorder_selection_set_drive_default_properties (selection, props);
		return;
	}

	props->props.drive_speed = setting->props.drive_speed;
	props->flags = setting->flags;
}

void
brasero_recorder_selection_get_media (BraseroRecorderSelection *selection,
				      NautilusBurnMediaType *media)
{
	if (!media)
		return;

	if (selection->priv->drive)
		*media = nautilus_burn_drive_get_media_type (selection->priv->drive);
	else
		*media = NAUTILUS_BURN_MEDIA_TYPE_UNKNOWN;
}

void
brasero_recorder_selection_set_drive (BraseroRecorderSelection *selection,
				      NautilusBurnDrive *drive)
{
	nautilus_burn_drive_selection_set_active (NAUTILUS_BURN_DRIVE_SELECTION (selection->priv->selection),
						  drive);
}

void
brasero_recorder_selection_select_default_drive (BraseroRecorderSelection *selection,
						 BraseroMediaType type)
{
	GList *iter;
	GList *drives;
	gboolean image;
	gboolean is_blank;
	gboolean has_data;
	gboolean has_audio;
	gboolean recorders;
	gboolean is_rewritable;
	NautilusBurnDrive *drive;
	NautilusBurnMediaType media_type;
	NautilusBurnDrive *candidate = NULL;

	g_object_get (selection->priv->selection,
		      "show-recorders-only",
		      &recorders,
		      NULL);
	g_object_get (selection->priv->selection,
		      "file-image",
		      &image,
		      NULL);

	NCB_DRIVE_GET_LIST (drives, recorders, image);

	for (iter = drives; iter; iter = iter->next) {
		drive = iter->data;

		if (!drive || NCB_DRIVE_GET_TYPE (drive) == NAUTILUS_BURN_DRIVE_TYPE_FILE)
			continue;

		media_type = nautilus_burn_drive_get_media_type_full (drive,
								      &is_rewritable,
								      &is_blank,
								      &has_data,
								      &has_audio);

		if ((type & BRASERO_MEDIA_WRITABLE) &&  nautilus_burn_drive_media_type_is_writable (media_type, is_blank)) {
			/* the perfect candidate would be blank; if not keep for later and see if no better media comes up */
			if (is_blank) {
				nautilus_burn_drive_selection_set_active (NAUTILUS_BURN_DRIVE_SELECTION (selection->priv->selection), drive);
				goto end;
			}

			candidate = drive;
		}
		else if ((type & BRASERO_MEDIA_REWRITABLE) && is_rewritable) {
			/* the perfect candidate would have data; if not keep it for later and see if no better media comes up */
			if (has_data || has_audio) {
				nautilus_burn_drive_selection_set_active (NAUTILUS_BURN_DRIVE_SELECTION (selection->priv->selection), drive);
				goto end;
			}

			candidate = drive;
		}
		else if ((type & BRASERO_MEDIA_WITH_DATA) && (has_data || has_audio)) {
			/* the perfect candidate would not be rewritable; if not keep it for later and see if no better media comes up */
			if (!is_rewritable) {
				nautilus_burn_drive_selection_set_active (NAUTILUS_BURN_DRIVE_SELECTION (selection->priv->selection), drive);
				goto end;
			}

			candidate = drive;
		}
	}

	if (candidate)
		nautilus_burn_drive_selection_set_active (NAUTILUS_BURN_DRIVE_SELECTION (selection->priv->selection), candidate);

end:
	g_list_foreach (drives, (GFunc) nautilus_burn_drive_unref, NULL);
	g_list_free (drives);
}
