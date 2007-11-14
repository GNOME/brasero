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

#include <string.h>

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
#include <gtk/gtkfilechooserdialog.h>

#include "burn-basics.h"
#include "brasero-image-properties.h"
#include "brasero-image-type-chooser.h"

typedef struct _BraseroImagePropertiesPrivate BraseroImagePropertiesPrivate;
struct _BraseroImagePropertiesPrivate
{
	GtkWidget *format;
	gchar *original_path;

	guint edited:1;
};

#define BRASERO_IMAGE_PROPERTIES_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_IMAGE_PROPERTIES, BraseroImagePropertiesPrivate))

enum {
	FORMAT_CHANGED_SIGNAL,
	LAST_SIGNAL
};
static guint brasero_image_properties_signals [LAST_SIGNAL] = { 0 };

static GtkDialogClass* parent_class = NULL;

G_DEFINE_TYPE (BraseroImageProperties, brasero_image_properties, GTK_TYPE_FILE_CHOOSER_DIALOG);

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

gboolean
brasero_image_properties_is_path_edited (BraseroImageProperties *self)
{
	BraseroImagePropertiesPrivate *priv;
	gchar *chooser_path;

	priv = BRASERO_IMAGE_PROPERTIES_PRIVATE (self);

	if (priv->edited)
		return TRUE;

	chooser_path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (self));
	if (!strcmp (chooser_path, priv->original_path))
		return FALSE;

	return TRUE;
}

gchar *
brasero_image_properties_get_path (BraseroImageProperties *self)
{
	BraseroImagePropertiesPrivate *priv;

	priv = BRASERO_IMAGE_PROPERTIES_PRIVATE (self);
	return gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (self));
}

void
brasero_image_properties_set_path (BraseroImageProperties *self,
				   const gchar *path)
{
	BraseroImagePropertiesPrivate *priv;

	priv = BRASERO_IMAGE_PROPERTIES_PRIVATE (self);

	if (priv->original_path) {
		if (!priv->edited) {
			gchar *chooser_path;

			/* check if the path was edited since the last time it was set */
			chooser_path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (self));
			priv->edited = strcmp (priv->original_path, chooser_path) != 0;
		}
		g_free (priv->original_path);
	}

	priv->original_path = g_strdup (path);
	
	if (path) {
		gchar *name;

		gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (self), path);

		/* The problem here is that is the file name doesn't exist
		 * in the folder then it won't be displayed so we check that */
		name = g_path_get_basename (path);
		gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (self), name);
	    	g_free (name);
	}
	else
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (self),
						     g_get_home_dir ());
}

static void
brasero_image_properties_format_changed_cb (BraseroImageTypeChooser *chooser,
					    BraseroImageProperties *self)
{
	BraseroImagePropertiesPrivate *priv;

	priv = BRASERO_IMAGE_PROPERTIES_PRIVATE (self);

	/* propagate the signal */
	g_signal_emit (self,
		       brasero_image_properties_signals [FORMAT_CHANGED_SIGNAL],
		       0);
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
		gtk_widget_show (priv->format);
		gtk_box_pack_start (GTK_BOX (GTK_DIALOG (self)->vbox),
				    priv->format,
				    FALSE,
				    FALSE,
				    0);
		g_signal_connect (priv->format,
				  "changed",
				  G_CALLBACK (brasero_image_properties_format_changed_cb),
				  self);
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
				GTK_STOCK_APPLY, GTK_RESPONSE_OK,
				NULL);

	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (object)->vbox), 12);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_BOX (GTK_DIALOG (object)->vbox)), 10);
}

static void
brasero_image_properties_finalize (GObject *object)
{
	BraseroImagePropertiesPrivate *priv;

	priv = BRASERO_IMAGE_PROPERTIES_PRIVATE (object);
	if (priv->original_path) {
		g_free (priv->original_path);
		priv->original_path = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_image_properties_class_init (BraseroImagePropertiesClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	parent_class = GTK_DIALOG_CLASS (g_type_class_peek_parent (klass));

	g_type_class_add_private (klass, sizeof (BraseroImagePropertiesPrivate));

	object_class->finalize = brasero_image_properties_finalize;
	brasero_image_properties_signals [FORMAT_CHANGED_SIGNAL] =
	    g_signal_new ("format_changed",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__VOID,
			  G_TYPE_NONE, 0, G_TYPE_NONE);
}

GtkWidget *
brasero_image_properties_new ()
{
	return GTK_WIDGET (g_object_new (BRASERO_TYPE_IMAGE_PROPERTIES,
					 "action", GTK_FILE_CHOOSER_ACTION_SAVE,
					 "do-overwrite-confirmation", TRUE,
					 "local-only", TRUE,
					 NULL));
}
