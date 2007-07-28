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
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtkbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkfilechooser.h>
#include <gtk/gtkfilechooserwidget.h>

#include "burn-basics.h"
#include "brasero-image-properties.h"
#include "brasero-image-type-chooser.h"

typedef struct _BraseroImagePropertiesPrivate BraseroImagePropertiesPrivate;
struct _BraseroImagePropertiesPrivate
{
	GtkWidget *vbox;
	GtkWidget *file;
	GtkWidget *format;
};

#define BRASERO_IMAGE_PROPERTIES_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_IMAGE_PROPERTIES, BraseroImagePropertiesPrivate))


static GtkDialogClass* parent_class = NULL;

G_DEFINE_TYPE (BraseroImageProperties, brasero_image_properties, GTK_TYPE_DIALOG);

BraseroImageFormat
brasero_image_properties_get_format (BraseroImageProperties *self)
{
	BraseroImagePropertiesPrivate *priv;
	BraseroImageFormat format;

	priv = BRASERO_IMAGE_PROPERTIES_PRIVATE (self);

	if (priv->format == NULL)
		return BRASERO_IMAGE_FORMAT_NONE;

	brasero_image_type_chooser_get_format (BRASERO_IMAGE_TYPE_CHOOSER (priv->format),
					       &format);

	return format;
}

gchar *
brasero_image_properties_get_path (BraseroImageProperties *self)
{
	BraseroImagePropertiesPrivate *priv;

	priv = BRASERO_IMAGE_PROPERTIES_PRIVATE (self);
	return gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (priv->file));
}

void
brasero_image_properties_set_path (BraseroImageProperties *self,
				   const gchar *path)
{
	BraseroImagePropertiesPrivate *priv;

	priv = BRASERO_IMAGE_PROPERTIES_PRIVATE (self);

	if (path) {
		gchar *name;

		gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (priv->file), path);

		/* The problem here is that is the file name doesn't exist
		 * in the folder then it won't be displayed so we check that */
		name = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (priv->file));
		if (!name) {
			name = g_path_get_basename (path);
			gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (priv->file), name);
		}

	    	g_free (name);
	}
	else
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (priv->file),
						     g_get_home_dir ());
}

void
brasero_image_properties_set_formats (BraseroImageProperties *self,
				      BraseroImageFormat formats,
				      BraseroImageFormat format)
{
	BraseroImagePropertiesPrivate *priv;

	priv = BRASERO_IMAGE_PROPERTIES_PRIVATE (self);

	/* have a look at the formats and see if it is worth to display a widget */
	if (formats == BRASERO_IMAGE_FORMAT_NONE) {
		if (priv->format) {
			gtk_widget_destroy (priv->format);
			priv->format = NULL;
		}

		return;
	}	

	if (!priv->format) {
		priv->format = brasero_image_type_chooser_new ();
		gtk_box_pack_start (GTK_BOX (priv->vbox),
				    priv->format,
				    FALSE,
				    FALSE,
				    0);
	}

	brasero_image_type_chooser_set_formats (BRASERO_IMAGE_TYPE_CHOOSER (priv->format),
					        formats);
	brasero_image_type_chooser_set_format (BRASERO_IMAGE_TYPE_CHOOSER (priv->format),
					       format);

	gtk_widget_show (priv->format);
}

static void
brasero_image_properties_init (BraseroImageProperties *object)
{
	BraseroImagePropertiesPrivate *priv;

	priv = BRASERO_IMAGE_PROPERTIES_PRIVATE (object);

	gtk_window_set_title (GTK_WINDOW (object), _("Disc image file properties"));
	gtk_dialog_set_has_separator (GTK_DIALOG (object), FALSE);
	gtk_dialog_add_buttons (GTK_DIALOG (object),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_APPLY, GTK_RESPONSE_ACCEPT,
				NULL);

	priv->vbox = gtk_vbox_new (FALSE, 12);
	gtk_widget_show (priv->vbox);
	gtk_container_set_border_width (GTK_CONTAINER (priv->vbox), 10);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (object)->vbox),
			    priv->vbox,
			    TRUE,
			    TRUE,
			    4);

	/* create file chooser */
	priv->file = gtk_file_chooser_widget_new (GTK_FILE_CHOOSER_ACTION_SAVE);
	gtk_widget_show_all (priv->file);

	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (priv->file), TRUE);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (priv->file), TRUE);

	gtk_widget_show_all (priv->vbox);
}

static void
brasero_image_properties_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_image_properties_class_init (BraseroImagePropertiesClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	parent_class = GTK_DIALOG_CLASS (g_type_class_peek_parent (klass));

	g_type_class_add_private (klass, sizeof (BraseroImagePropertiesPrivate));

	object_class->finalize = brasero_image_properties_finalize;
}

GtkWidget *
brasero_image_properties_new ()
{
	return GTK_WIDGET (g_object_new (BRASERO_TYPE_IMAGE_PROPERTIES, NULL));
}
