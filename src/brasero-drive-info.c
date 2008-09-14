/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2007-2008 <bonfire-app@wanadoo.fr>
 * 
 *  Brasero is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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
#include <gtk/gtkalignment.h>

#include "brasero-drive-info.h"
#include "burn-medium.h"
#include "burn-caps.h"

typedef struct _BraseroDriveInfoPrivate BraseroDriveInfoPrivate;
struct _BraseroDriveInfoPrivate
{
	GtkWidget *notebook;
	GtkWidget *image;

	GtkWidget *capacity;
	GtkWidget *status;

	GtkWidget *table;
	GtkWidget *warning;
	GtkWidget *image_path;
};

#define BRASERO_DRIVE_INFO_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_DRIVE_INFO, BraseroDriveInfoPrivate))

static GtkHBoxClass* parent_class = NULL;

G_DEFINE_TYPE (BraseroDriveInfo, brasero_drive_info, GTK_TYPE_HBOX);

static void
brasero_drive_info_update_info (BraseroDriveInfo *self,
				BraseroMedium *medium)
{
	BraseroMedia media;
	BraseroBurnCaps *caps;
	BraseroMedia media_status;
	BraseroDriveInfoPrivate *priv;

	priv = BRASERO_DRIVE_INFO_PRIVATE (self);

	if (medium)
		media = brasero_medium_get_status (medium);
	else
		media = BRASERO_MEDIUM_NONE;

	gtk_label_set_text (GTK_LABEL (priv->capacity), "");
	gtk_label_set_text (GTK_LABEL (priv->status), "");

	/* type */
	if (media == BRASERO_MEDIUM_NONE) {
		gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
					      "gnome-dev-removable",
					      GTK_ICON_SIZE_DIALOG);
		return;
	}

	if (media == BRASERO_MEDIUM_UNSUPPORTED) {
		gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
					      "gnome-dev-removable",
					      GTK_ICON_SIZE_DIALOG);
		return;
	}

	if (media == BRASERO_MEDIUM_BUSY) {
		gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
					      "gnome-dev-removable",
					      GTK_ICON_SIZE_DIALOG);
		return;
	}

	/* NOTE: we must not rely on the medium advertised info but on what the
	 * library can do */
	caps = brasero_burn_caps_get_default ();
	media_status = brasero_burn_caps_media_capabilities (caps, media);
	g_object_unref (caps);

	if (media_status == BRASERO_MEDIUM_NONE) {
		gchar *data_size_string, *info;
		gint64 data_size;

		brasero_medium_get_capacity (medium, &data_size, NULL);
		data_size_string = g_format_size_for_display (data_size);
		info = g_strdup_printf (_("%s of data"), data_size_string);
		g_free (data_size_string);
	
		gtk_label_set_markup (GTK_LABEL (priv->capacity), info);
		g_free (info);


		if ((media & BRASERO_MEDIUM_CLOSED)
		&& !(media & BRASERO_MEDIUM_REWRITABLE)) {
			gchar *string;

			/* the media is closed anyway */
			string = g_strdup_printf ("<i>%s</i>", _("the medium is not writable"));
			gtk_label_set_markup (GTK_LABEL (priv->status), string);
			g_free (string);
		}
		else {
			gchar *string;

			/* maybe with some more plugins it would work since the 
			 * medium is apparently not closed and/or rewritable */
			string = g_strdup_printf ("<i>%s</i>", _("the medium is not writable with the current set of plugins"));
			gtk_label_set_markup (GTK_LABEL (priv->status), string);
			g_free (string);
		}
	}
	else if (media_status == BRASERO_MEDIUM_REWRITABLE) {
		gchar *remaining_string, *info;
		gint64 remaining;

		brasero_medium_get_capacity (medium, &remaining, NULL);
		remaining_string = g_format_size_for_display (remaining);
		info = g_strdup_printf (_("%s free"), remaining_string);
		g_free (remaining_string);
	
		gtk_label_set_markup (GTK_LABEL (priv->capacity), info);
		g_free (info);

		gtk_label_set_markup (GTK_LABEL (priv->status),
				      _("the medium can be recorded (automatic blanking required)"));
	}
	else if (media & BRASERO_MEDIUM_BLANK) {
		gchar *remaining_string, *info;
		gint64 remaining;

		brasero_medium_get_free_space (medium, &remaining, NULL);
		remaining_string = g_format_size_for_display (remaining);
		info = g_strdup_printf (_("%s free"), remaining_string);
		g_free (remaining_string);
	
		gtk_label_set_markup (GTK_LABEL (priv->capacity), info);
		g_free (info);

		gtk_label_set_markup (GTK_LABEL (priv->status), _("the medium can be recorded"));
	}
	else if (media_status & BRASERO_MEDIUM_REWRITABLE) {
		gchar *remaining_string, *capacity_string, *info;
		gint64 remaining, capacity;

		brasero_medium_get_capacity (medium, &capacity, NULL);
		brasero_medium_get_free_space (medium, &remaining, NULL);
		remaining_string = g_format_size_for_display (remaining);
		capacity_string = g_format_size_for_display (capacity);
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

		brasero_medium_get_free_space (medium, &remaining, NULL);
		remaining_string = g_format_size_for_display (remaining);
		info = g_strdup_printf (_("%s free"), remaining_string);
		g_free (remaining_string);
	
		gtk_label_set_markup (GTK_LABEL (priv->capacity), info);
		g_free (info);

		gtk_label_set_markup (GTK_LABEL (priv->status),
				      _("data can be appended to the medium"));
	}

	gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
				      brasero_medium_get_icon (medium),
				      GTK_ICON_SIZE_DIALOG);
}

void
brasero_drive_info_set_image_path (BraseroDriveInfo *self,
				   const gchar *path)
{
	gchar *info;
	BraseroDriveInfoPrivate *priv;

	priv = BRASERO_DRIVE_INFO_PRIVATE (self);

	info = g_strdup_printf (_("The image will be saved to\n%s"), path ? path:"");
	gtk_label_set_markup (GTK_LABEL (priv->image_path), info);
	g_free (info);

	/* NOTE: we could extend this by checking if the image actually exists and if so
	 * retrieving some information about it like size .... */
}

void
brasero_drive_info_set_same_src_dest (BraseroDriveInfo *self,
				      gboolean value)
{
	BraseroDriveInfoPrivate *priv;

	priv = BRASERO_DRIVE_INFO_PRIVATE (self);

	if (value) {
		/* This is to handle a special case when copying a media using
		 * same drive as source and destination */
		gtk_widget_show (priv->warning);
		gtk_widget_hide (priv->image_path);
		gtk_widget_hide (priv->table);
		gtk_widget_hide (priv->image);
	}
	else {
		gtk_widget_hide (priv->warning);
		gtk_widget_show (priv->image_path);
		gtk_widget_show (priv->table);
		gtk_widget_show (priv->image);
	}
}

void
brasero_drive_info_set_medium (BraseroDriveInfo *self,
			       BraseroMedium *medium)
{
	BraseroDriveInfoPrivate *priv;

	priv = BRASERO_DRIVE_INFO_PRIVATE (self);

	gtk_widget_show (priv->image);
	gtk_widget_hide (priv->warning);

	if (medium && (brasero_medium_get_status (medium) & BRASERO_MEDIUM_FILE)) {
		gtk_widget_show (priv->image_path);
		gtk_widget_hide (priv->warning);
		gtk_widget_hide (priv->table);

		gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
					      "iso-image-new",
					      GTK_ICON_SIZE_DIALOG);
		return;
	}

	brasero_drive_info_update_info (self, medium);
	gtk_widget_show (priv->table);
	gtk_widget_hide (priv->warning);
	gtk_widget_hide (priv->image_path);
}

static void
brasero_drive_info_init (BraseroDriveInfo *object)
{
	gchar *string;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *alignment;
	BraseroDriveInfoPrivate *priv;

	priv = BRASERO_DRIVE_INFO_PRIVATE (object);

	gtk_box_set_spacing (GTK_BOX (object), 12);

	priv->image = gtk_image_new ();
	gtk_box_pack_end (GTK_BOX (object), priv->image, FALSE, FALSE, 0);
	gtk_widget_show (priv->image);

	alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
	gtk_widget_show (alignment);
	gtk_box_pack_start (GTK_BOX (object), alignment, FALSE, FALSE, 0);

	priv->notebook = gtk_notebook_new ();
	gtk_widget_show (priv->notebook);
	gtk_container_add (GTK_CONTAINER (alignment), priv->notebook);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (priv->notebook), FALSE);

	string = g_strdup_printf ("<b><i>%s</i></b><i>%s</i>",
				  _("The drive that holds the source media will also be the one used to record.\n"),
				  _("A new recordable media will be required once the one currently loaded has been copied."));

	label = gtk_label_new (string);
	g_free (string);

	priv->warning = label;
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_label_set_line_wrap_mode (GTK_LABEL (label), PANGO_WRAP_WORD);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_notebook_prepend_page (GTK_NOTEBOOK (priv->notebook),
				   priv->warning,
				   NULL);

	table = gtk_table_new (2, 2, FALSE);
	priv->table = table;
	gtk_table_set_row_spacings (GTK_TABLE (table), 4);
	gtk_table_set_col_spacings (GTK_TABLE (table), 8);

	label = gtk_label_new (_("Size:"));
	gtk_widget_show (label);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
			  GTK_FILL, GTK_FILL, 0, 0);

	priv->capacity = gtk_label_new ("");
	gtk_widget_show (priv->capacity);
	gtk_misc_set_alignment (GTK_MISC (priv->capacity), 0.0, 0.0);
	gtk_table_attach (GTK_TABLE (table), priv->capacity, 1, 2, 0, 1,
			  GTK_FILL|GTK_EXPAND, GTK_FILL, 0, 0);

	label = gtk_label_new (_("Status:"));
	gtk_widget_show (label);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
			  GTK_FILL, GTK_FILL, 0, 0);

	priv->status = gtk_label_new ("");
	gtk_widget_show (priv->status);
	gtk_misc_set_alignment (GTK_MISC (priv->status), 0.0, 0.0);
	gtk_table_attach (GTK_TABLE (table), priv->status, 1, 2, 1, 2,
			  GTK_FILL|GTK_EXPAND, GTK_FILL, 0, 0);

	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), table, NULL);

	/* that's for the image */
	priv->image_path = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (priv->image_path), 0.0, 0.5);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), priv->image_path, NULL);
}

static void
brasero_drive_info_finalize (GObject *object)
{
	BraseroDriveInfoPrivate *priv;

	priv = BRASERO_DRIVE_INFO_PRIVATE (object);

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
