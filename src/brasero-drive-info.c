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

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtkimage.h>
#include <gtk/gtktable.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtknotebook.h>

#include <nautilus-burn-drive.h>

#include <libgnomevfs/gnome-vfs.h>

#include "brasero-drive-info.h"
#include "burn-medium.h"
#include "burn-caps.h"
#include "brasero-ncb.h"

typedef struct _BraseroDriveInfoPrivate BraseroDriveInfoPrivate;
struct _BraseroDriveInfoPrivate
{
	NautilusBurnDrive *drive;

	guint added_sig;
	guint removed_sig;

	GtkWidget *notebook;
	GtkWidget *image;

	GtkWidget *type;
	GtkWidget *capacity;
	GtkWidget *contents;
	GtkWidget *status;

	GtkWidget *image_path;
};

#define BRASERO_DRIVE_INFO_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_DRIVE_INFO, BraseroDriveInfoPrivate))

static GtkHBoxClass* parent_class = NULL;

G_DEFINE_TYPE (BraseroDriveInfo, brasero_drive_info, GTK_TYPE_HBOX);

static void
brasero_drive_info_update_info (BraseroDriveInfo *self,
				NautilusBurnDrive *drive)
{
	BraseroMedia media;
	BraseroBurnCaps *caps;
	BraseroMedia media_status;
	BraseroDriveInfoPrivate *priv;

	priv = BRASERO_DRIVE_INFO_PRIVATE (self);

	if (drive)
		media = NCB_MEDIA_GET_STATUS (drive);
	else
		media = BRASERO_MEDIUM_NONE;

	gtk_label_set_text (GTK_LABEL (priv->type), "");
	gtk_label_set_text (GTK_LABEL (priv->capacity), "");
	gtk_label_set_text (GTK_LABEL (priv->contents), "");
	gtk_label_set_text (GTK_LABEL (priv->status), "");

	/* type */
	if (media == BRASERO_MEDIUM_NONE) {
		gtk_label_set_markup (GTK_LABEL (priv->type),
				      _("<i>no disc</i>"));
		gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
					      "gnome-dev-removable",
					      GTK_ICON_SIZE_DIALOG);
		goto end;
	}
	else if (media == BRASERO_MEDIUM_UNSUPPORTED) {
		gtk_label_set_markup (GTK_LABEL (priv->type),
				      _("<i>unknown type</i>"));
		gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
					      "gnome-dev-removable",
					      GTK_ICON_SIZE_DIALOG);
		goto end;
	}
	else if (media == BRASERO_MEDIUM_BUSY) {
		gtk_label_set_markup (GTK_LABEL (priv->type),
				      _("<i>busy disc</i>"));
		gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
					      "gnome-dev-removable",
					      GTK_ICON_SIZE_DIALOG);
		goto end;
	}

	gtk_label_set_markup (GTK_LABEL (priv->type),
			      NCB_MEDIA_GET_TYPE_STRING (drive));

	/* contents */
	if (media & BRASERO_MEDIUM_BLANK)
		gtk_label_set_markup (GTK_LABEL (priv->contents), _("empty"));
	else if (BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_HAS_AUDIO|BRASERO_MEDIUM_HAS_DATA))
		gtk_label_set_markup (GTK_LABEL (priv->contents), _("audio and data tracks"));
	else if (media & BRASERO_MEDIUM_HAS_AUDIO)
		gtk_label_set_markup (GTK_LABEL (priv->contents), _("audio tracks"));
	else if (media & BRASERO_MEDIUM_HAS_DATA) 
		gtk_label_set_markup (GTK_LABEL (priv->contents), _("data tracks"));

	/* NOTE: we must not rely on the medium advertised info but on what the
	 * library can do */
	caps = brasero_burn_caps_get_default ();
	media_status = brasero_burn_caps_media_capabilities (caps, media);
	g_object_unref (caps);

	if (media_status == BRASERO_MEDIUM_NONE) {
		gchar *data_size_string, *info;
		gint64 data_size;

		NCB_MEDIA_GET_CAPACITY (drive, &data_size, NULL);
		data_size_string = gnome_vfs_format_file_size_for_display (data_size);
		info = g_strdup_printf (_("%s of data"), data_size_string);
		g_free (data_size_string);
	
		gtk_label_set_markup (GTK_LABEL (priv->capacity), info);
		g_free (info);


		if ((media & BRASERO_MEDIUM_CLOSED)
		&& !(media & BRASERO_MEDIUM_REWRITABLE)) {
			/* the media is closed anyway */
			gtk_label_set_markup (GTK_LABEL (priv->status),
					      _("<i>the medium is not writable</i>"));
		}
		else {
			/* maybe with some more plugins it would work since the 
			 * medium is apparently not closed and/or rewritable */
			gtk_label_set_markup (GTK_LABEL (priv->status),
					      _("<i>the medium is not writable with the current set of plugins</i>"));
		}
	}
	else if (media_status == BRASERO_MEDIUM_REWRITABLE) {
		gchar *remaining_string, *info;
		gint64 remaining;

		NCB_MEDIA_GET_CAPACITY (drive, &remaining, NULL);
		remaining_string = gnome_vfs_format_file_size_for_display (remaining);
		info = g_strdup_printf (_("%s free"), remaining_string);
		g_free (remaining_string);
	
		gtk_label_set_markup (GTK_LABEL (priv->capacity), info);
		g_free (info);

		gtk_label_set_markup (GTK_LABEL (priv->status),
				      _("the medium can be recorded (blanking required)"));
	}
	else if (media & BRASERO_MEDIUM_BLANK) {
		gchar *remaining_string, *info;
		gint64 remaining;

		NCB_MEDIA_GET_FREE_SPACE (drive, &remaining, NULL);
		remaining_string = gnome_vfs_format_file_size_for_display (remaining);
		info = g_strdup_printf (_("%s free"), remaining_string);
		g_free (remaining_string);
	
		gtk_label_set_markup (GTK_LABEL (priv->capacity), info);
		g_free (info);

		gtk_label_set_markup (GTK_LABEL (priv->status), _("the medium can be recorded"));
	}
	else if (media_status & BRASERO_MEDIUM_REWRITABLE) {
		gchar *remaining_string, *capacity_string, *info;
		gint64 remaining, capacity;

		NCB_MEDIA_GET_CAPACITY (drive, &capacity, NULL);
		NCB_MEDIA_GET_FREE_SPACE (drive, &remaining, NULL);
		remaining_string = gnome_vfs_format_file_size_for_display (remaining);
		capacity_string = gnome_vfs_format_file_size_for_display (capacity);
		info = g_strdup_printf (_("%s (%s free)"),
					capacity_string,
					remaining_string);
		g_free (remaining_string);
		g_free (capacity_string);
	
		gtk_label_set_markup (GTK_LABEL (priv->capacity), info);
		g_free (info);

		gtk_label_set_markup (GTK_LABEL (priv->status),
				      _("data can be written or appended to the medium"));
	}
	else { /* medium can simply be appended */
		gchar *remaining_string, *info;
		gint64 remaining;

		NCB_MEDIA_GET_FREE_SPACE (drive, &remaining, NULL);
		remaining_string = gnome_vfs_format_file_size_for_display (remaining);
		info = g_strdup_printf (_("%s free"), remaining_string);
		g_free (remaining_string);
	
		gtk_label_set_markup (GTK_LABEL (priv->capacity), info);
		g_free (info);

		gtk_label_set_markup (GTK_LABEL (priv->status),
				      _("data can be appended to the medium"));
	}

end:
	gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
				      NCB_MEDIA_GET_ICON (drive),
				      GTK_ICON_SIZE_DIALOG);
}

void
brasero_drive_info_set_image_path (BraseroDriveInfo *self,
				   const gchar *path)
{
	gchar *info;
	BraseroDriveInfoPrivate *priv;

	priv = BRASERO_DRIVE_INFO_PRIVATE (self);

	info = g_strdup_printf (_("The <b>image</b> will be saved to\n%s"), path ? path:"");
	gtk_label_set_markup (GTK_LABEL (priv->image_path), info);
	g_free (info);

	/* NOTE: we could extend this by checking if the image actually exists and if so
	 * retrieving some information about it like size .... */
}

static void
brasero_drive_info_media_added (NautilusBurnDrive *drive,
				BraseroDriveInfo *self)
{
	brasero_drive_info_update_info (self, drive);
}

static void
brasero_drive_info_media_removed (NautilusBurnDrive *drive,
				  BraseroDriveInfo *self)
{
	brasero_drive_info_update_info (self, drive);
}

void
brasero_drive_info_set_drive (BraseroDriveInfo *self,
			      NautilusBurnDrive *drive)
{
	BraseroDriveInfoPrivate *priv;

	priv = BRASERO_DRIVE_INFO_PRIVATE (self);

	if (priv->drive) {
		if (priv->added_sig) {
			g_signal_handler_disconnect (priv->drive, priv->added_sig);
			priv->added_sig = 0;
		}

		if (priv->removed_sig) {
			g_signal_handler_disconnect (priv->drive, priv->removed_sig);
			priv->removed_sig = 0;
		}

		nautilus_burn_drive_unref (priv->drive);
		priv->drive = NULL;
	}

	if (drive && NCB_DRIVE_GET_TYPE (drive) == NAUTILUS_BURN_DRIVE_TYPE_FILE) {
		gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
					      "iso-image-new",
					      GTK_ICON_SIZE_DIALOG);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), 1);
		return;
	}

	if (drive) {
		priv->added_sig = g_signal_connect (drive,
						    "media-added",
						    G_CALLBACK (brasero_drive_info_media_added),
						    self);
		priv->removed_sig = g_signal_connect (drive,
						      "media-removed",
						      G_CALLBACK (brasero_drive_info_media_removed),
						      self);
		priv->drive = drive;
		nautilus_burn_drive_ref (drive);
	}

	brasero_drive_info_update_info (self, drive);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), 0);
}

static void
brasero_drive_info_init (BraseroDriveInfo *object)
{
	GtkWidget *table;
	GtkWidget *label;
	BraseroDriveInfoPrivate *priv;

	priv = BRASERO_DRIVE_INFO_PRIVATE (object);

	gtk_box_set_spacing (GTK_BOX (object), 12);

	priv->image = gtk_image_new ();
	gtk_box_pack_start (GTK_BOX (object), priv->image, FALSE, FALSE, 0);

	priv->notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (priv->notebook), FALSE);
	gtk_box_pack_start (GTK_BOX (object), priv->notebook, FALSE, FALSE, 0);

	table = gtk_table_new (4, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 4);
	gtk_table_set_col_spacings (GTK_TABLE (table), 8);

	label = gtk_label_new (_("<b>Type:</b>"));
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);

	priv->type = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (priv->type), 0.0, 0.0);
	gtk_table_attach_defaults (GTK_TABLE (table), priv->type, 1, 2, 0, 1);

	label = gtk_label_new (_("<b>Size:</b>"));
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 1, 2);

	priv->capacity = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (priv->capacity), 0.0, 0.0);
	gtk_table_attach (GTK_TABLE (table), priv->capacity, 1, 2, 1, 2,
			  GTK_FILL|GTK_EXPAND, GTK_FILL|GTK_EXPAND, 0, 0);

	label = gtk_label_new (_("<b>Contents:</b>"));
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 2, 3);

	priv->contents = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (priv->contents), 0.0, 0.0);
	gtk_table_attach_defaults (GTK_TABLE (table), priv->contents, 1, 2, 2, 3);

	label = gtk_label_new (_("<b>Status:</b>"));
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 3, 4);

	priv->status = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (priv->status), 0.0, 0.0);
	gtk_table_attach_defaults (GTK_TABLE (table), priv->status, 1, 2, 3, 4);

	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), table, NULL);

	/* that's for the image */
	priv->image_path = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (priv->image_path), 0.0, 0.5);

	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), priv->image_path, NULL);

	gtk_widget_show_all (GTK_WIDGET (object));
}

static void
brasero_drive_info_finalize (GObject *object)
{
	BraseroDriveInfoPrivate *priv;

	priv = BRASERO_DRIVE_INFO_PRIVATE (object);
	if (priv->drive) {
		if (priv->added_sig) {
			g_signal_handler_disconnect (priv->drive, priv->added_sig);
			priv->added_sig = 0;
		}

		if (priv->removed_sig) {
			g_signal_handler_disconnect (priv->drive, priv->removed_sig);
			priv->removed_sig = 0;
		}

		nautilus_burn_drive_unref (priv->drive);
		priv->drive = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_drive_info_class_init (BraseroDriveInfoClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	parent_class = GTK_HBOX_CLASS (g_type_class_peek_parent (klass));

	g_type_class_add_private (klass, sizeof (BraseroDriveInfoPrivate));
	object_class->finalize = brasero_drive_info_finalize;
}

GtkWidget *
brasero_drive_info_new ()
{
	return GTK_WIDGET (g_object_new (BRASERO_TYPE_DRIVE_INFO, NULL));
}
