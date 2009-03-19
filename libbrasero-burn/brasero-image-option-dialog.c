/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-burn
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-burn is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-burn authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-burn. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-burn is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to:
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

#include <gtk/gtk.h>

#include <gconf/gconf-client.h>

#include "burn-basics.h"
#include "brasero-drive.h"

#include "brasero-utils.h"
#include "brasero-image-option-dialog.h"
#include "brasero-src-image.h"
#include "brasero-burn-options.h"

G_DEFINE_TYPE (BraseroImageOptionDialog, brasero_image_option_dialog, BRASERO_TYPE_BURN_OPTIONS);

struct _BraseroImageOptionDialogPrivate {
	GtkWidget *file;
};
typedef struct _BraseroImageOptionDialogPrivate BraseroImageOptionDialogPrivate;

#define BRASERO_IMAGE_OPTION_DIALOG_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_IMAGE_OPTION_DIALOG, BraseroImageOptionDialogPrivate))

static GtkDialogClass *parent_class = NULL;

void
brasero_image_option_dialog_set_image_uri (BraseroImageOptionDialog *dialog,
					   const gchar *uri)
{
	BraseroImageOptionDialogPrivate *priv;

	priv = BRASERO_IMAGE_OPTION_DIALOG_PRIVATE (dialog);

	brasero_src_image_set_uri (BRASERO_SRC_IMAGE (priv->file), uri);
}

static void
brasero_image_option_dialog_init (BraseroImageOptionDialog *obj)
{
	gchar *string;
	BraseroBurnSession *session;
	BraseroImageOptionDialogPrivate *priv;

	priv = BRASERO_IMAGE_OPTION_DIALOG_PRIVATE (obj);

	brasero_burn_options_set_type_shown (BRASERO_BURN_OPTIONS (obj),
					     BRASERO_MEDIA_TYPE_WRITABLE);

	/* Image properties */
	session = brasero_burn_options_get_session (BRASERO_BURN_OPTIONS (obj));
	priv->file = brasero_src_image_new (session);
	g_object_unref (session);

	gtk_widget_show (priv->file);

	/* pack everything */
	string = g_strdup_printf ("<b>%s</b>", _("Select an image to write"));
	brasero_burn_options_add_source (BRASERO_BURN_OPTIONS (obj), 
					 string,
					 priv->file,
					 NULL);
	g_free (string);
}

static void
brasero_image_option_dialog_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_image_option_dialog_class_init (BraseroImageOptionDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroImageOptionDialogPrivate));

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_image_option_dialog_finalize;
}

GtkWidget *
brasero_image_option_dialog_new ()
{
	BraseroImageOptionDialog *obj;
	
	obj = BRASERO_IMAGE_OPTION_DIALOG (g_object_new (BRASERO_TYPE_IMAGE_OPTION_DIALOG,
							"title", _("Image Burning Setup"),
							NULL));
	
	return GTK_WIDGET (obj);
}
