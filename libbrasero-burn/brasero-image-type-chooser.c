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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtk.h>

#include "burn-basics.h"
#include "brasero-image-type-chooser.h"

#define BRASERO_IMAGE_TYPE_CHOOSER_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_IMAGE_TYPE_CHOOSER, BraseroImageTypeChooserPrivate))

G_DEFINE_TYPE (BraseroImageTypeChooser, brasero_image_type_chooser, GTK_TYPE_BOX);

enum {
	FORMAT_TEXT,
	FORMAT_TYPE,
	FORMAT_SVCD,
	FORMAT_LAST
};

enum {
	CHANGED_SIGNAL,
	LAST_SIGNAL
};
static guint brasero_image_type_chooser_signals [LAST_SIGNAL] = { 0 };

struct _BraseroImageTypeChooserPrivate {
	GtkWidget *combo;

	BraseroImageFormat format;
	gboolean is_svcd;

	guint updating:1;
};

static GtkBoxClass *parent_class = NULL;

guint
brasero_image_type_chooser_set_formats (BraseroImageTypeChooser *self,
				        BraseroImageFormat formats,
                                        gboolean show_autodetect,
                                        gboolean is_video)
{
	guint format_num;
	GtkTreeIter iter;
	GtkTreeModel *store;
	BraseroImageTypeChooserPrivate *priv;

	priv = BRASERO_IMAGE_TYPE_CHOOSER_PRIVATE (self);

	priv->updating = TRUE;

	format_num = 0;

	/* clean */
	store = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->combo));
	gtk_list_store_clear (GTK_LIST_STORE (store));

	/* now we get the targets available and display them */
	if (show_autodetect) {
		gtk_list_store_prepend (GTK_LIST_STORE (store), &iter);
		gtk_list_store_set (GTK_LIST_STORE (store), &iter,
				    FORMAT_TEXT, _("Autodetect"),
				    FORMAT_TYPE, BRASERO_IMAGE_FORMAT_NONE,
				    -1);
	}

	if (formats & BRASERO_IMAGE_FORMAT_BIN) {
		format_num ++;
		gtk_list_store_append (GTK_LIST_STORE (store), &iter);
		gtk_list_store_set (GTK_LIST_STORE (store), &iter,
				    FORMAT_TEXT, is_video? _("Video DVD image"):_("ISO9660 image"),
				    FORMAT_TYPE, BRASERO_IMAGE_FORMAT_BIN,
				    -1);
	}

	if (formats & BRASERO_IMAGE_FORMAT_CLONE) {
		format_num ++;
		gtk_list_store_append (GTK_LIST_STORE (store), &iter);
		gtk_list_store_set (GTK_LIST_STORE (store), &iter,
				    FORMAT_TEXT, _("Readcd/Readom image"),
				    FORMAT_TYPE, BRASERO_IMAGE_FORMAT_CLONE,
				    -1);
	}

	if (formats & BRASERO_IMAGE_FORMAT_CUE) {
		format_num ++;
		if (is_video) {
			format_num ++;
	
			gtk_list_store_append (GTK_LIST_STORE (store), &iter);
			gtk_list_store_set (GTK_LIST_STORE (store), &iter,
					    FORMAT_TEXT, _("VCD image"),
					    FORMAT_TYPE, BRASERO_IMAGE_FORMAT_CUE,
					    -1);

			gtk_list_store_append (GTK_LIST_STORE (store), &iter);
			gtk_list_store_set (GTK_LIST_STORE (store), &iter,
					    FORMAT_TEXT, _("SVCD image"),
					    FORMAT_TYPE, BRASERO_IMAGE_FORMAT_CUE,
			                    FORMAT_SVCD, TRUE,
					    -1);
		}
		else {
			gtk_list_store_append (GTK_LIST_STORE (store), &iter);
			gtk_list_store_set (GTK_LIST_STORE (store), &iter,
					    FORMAT_TEXT, _("Cue image"),
					    FORMAT_TYPE, BRASERO_IMAGE_FORMAT_CUE,
					    -1);
		}
	}

	if (formats & BRASERO_IMAGE_FORMAT_CDRDAO) {
		format_num ++;
		gtk_list_store_append (GTK_LIST_STORE (store), &iter);
		gtk_list_store_set (GTK_LIST_STORE (store), &iter,
				    FORMAT_TEXT, _("Cdrdao image"),
				    FORMAT_TYPE, BRASERO_IMAGE_FORMAT_CDRDAO,
				    -1);
	}

	priv->updating = FALSE;

	/* Make sure the selected format is still supported */
	if (priv->format & formats)
		brasero_image_type_chooser_set_format (self, priv->format);
	else
		brasero_image_type_chooser_set_format (self, BRASERO_IMAGE_FORMAT_NONE);

	return format_num;
}

void
brasero_image_type_chooser_set_format (BraseroImageTypeChooser *self,
				       BraseroImageFormat format)
{
	GtkTreeIter iter;
	GtkTreeModel *store;
	BraseroImageTypeChooserPrivate *priv;

	priv = BRASERO_IMAGE_TYPE_CHOOSER_PRIVATE (self);

	store = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->combo));
	
	if (format == BRASERO_IMAGE_FORMAT_NONE) {
		gtk_combo_box_set_active (GTK_COMBO_BOX (priv->combo), 0);
		return;
	}

	if (!gtk_tree_model_get_iter_first (store, &iter))
		return;

	do {
		BraseroImageFormat iter_format;

		gtk_tree_model_get (store, &iter,
				    FORMAT_TYPE, &iter_format,
				    -1);

		if (iter_format == format) {
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (priv->combo), &iter);
			return;
		}

	} while (gtk_tree_model_iter_next (store, &iter));

	/* just to make sure we see if there is a line which is active. It can 
	 * happens that the last time it was a CD and the user chose RAW. If it
	 * is now a DVD it can't be raw any more */
	if (gtk_combo_box_get_active (GTK_COMBO_BOX (priv->combo)) == -1)
		gtk_combo_box_set_active (GTK_COMBO_BOX (priv->combo), 0);
}

void
brasero_image_type_chooser_set_VCD_type (BraseroImageTypeChooser *chooser,
                                         gboolean is_SVCD)
{
	GtkTreeIter iter;
	GtkTreeModel *store;
	BraseroImageTypeChooserPrivate *priv;

	priv = BRASERO_IMAGE_TYPE_CHOOSER_PRIVATE (chooser);

	store = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->combo));
	
	if (!gtk_tree_model_get_iter_first (store, &iter))
		return;

	do {
		BraseroImageFormat iter_format;
		gboolean is_svcd;

		gtk_tree_model_get (store, &iter,
				    FORMAT_TYPE, &iter_format,
		                    FORMAT_SVCD, &is_svcd,
				    -1);

		if (iter_format == BRASERO_IMAGE_FORMAT_CUE && is_SVCD == is_svcd) {
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (priv->combo), &iter);
			return;
		}

	} while (gtk_tree_model_iter_next (store, &iter));

	/* just to make sure we see if there is a line which is active. It can 
	 * happens that the last time it was a CD and the user chose RAW. If it
	 * is now a DVD it can't be raw any more */
	if (gtk_combo_box_get_active (GTK_COMBO_BOX (priv->combo)) == -1)
		gtk_combo_box_set_active (GTK_COMBO_BOX (priv->combo), 0);
}

void
brasero_image_type_chooser_get_format (BraseroImageTypeChooser *self,
				       BraseroImageFormat *format)
{
	BraseroImageTypeChooserPrivate *priv;

	priv = BRASERO_IMAGE_TYPE_CHOOSER_PRIVATE (self);
	*format = priv->format;
}

gboolean
brasero_image_type_chooser_get_VCD_type (BraseroImageTypeChooser *chooser)
{
	BraseroImageTypeChooserPrivate *priv;
	GtkTreeModel *model;
	gboolean value;
	GtkTreeIter iter;

	priv = BRASERO_IMAGE_TYPE_CHOOSER_PRIVATE (chooser);
	if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (priv->combo), &iter))
		return FALSE;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->combo));
	gtk_tree_model_get (model, &iter,
	                    FORMAT_SVCD, &value,
	                    -1);

	return value;
}

static void
brasero_image_type_chooser_changed_cb (GtkComboBox *combo,
				       BraseroImageTypeChooser *self)
{
	GtkTreeIter iter;
	gboolean is_svcd;
	GtkTreeModel *store;
	BraseroImageFormat current;
	BraseroImageTypeChooserPrivate *priv;

	priv = BRASERO_IMAGE_TYPE_CHOOSER_PRIVATE (self);

	if (priv->updating)
		return;

	store = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->combo));
	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (priv->combo), &iter))
		gtk_tree_model_get (store, &iter,
				    FORMAT_TYPE, &current,
		                    FORMAT_SVCD, &is_svcd,
				    -1);
	else
		current = BRASERO_IMAGE_FORMAT_NONE;

	if (current == priv->format
	&& is_svcd == priv->is_svcd)
		return;

	priv->format = current;
	priv->is_svcd = is_svcd;

	g_signal_emit (self,
		       brasero_image_type_chooser_signals [CHANGED_SIGNAL],
		       0);
}

static void
brasero_image_type_chooser_init (BraseroImageTypeChooser *obj)
{
	GtkListStore *store;
	GtkCellRenderer *renderer;
	BraseroImageTypeChooserPrivate *priv;

	priv = BRASERO_IMAGE_TYPE_CHOOSER_PRIVATE (obj);

	store = gtk_list_store_new (FORMAT_LAST,
				    G_TYPE_STRING,
				    G_TYPE_INT,
	                            G_TYPE_BOOLEAN);

	priv->combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
	g_signal_connect (priv->combo,
			  "changed",
			  G_CALLBACK (brasero_image_type_chooser_changed_cb),
			  obj);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (priv->combo), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (priv->combo), renderer,
					"text", FORMAT_TEXT,
					NULL);

	gtk_widget_show (priv->combo);
	gtk_box_pack_end (GTK_BOX (obj), priv->combo, TRUE, TRUE, 0);
}

static void
brasero_image_type_chooser_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_image_type_chooser_class_init (BraseroImageTypeChooserClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	g_type_class_add_private (klass, sizeof (BraseroImageTypeChooserPrivate));

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_image_type_chooser_finalize;

	brasero_image_type_chooser_signals [CHANGED_SIGNAL] = 
	    g_signal_new ("changed",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_FIRST,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__VOID,
			  G_TYPE_NONE,
			  0);
}


GtkWidget *
brasero_image_type_chooser_new ()
{
	BraseroImageTypeChooser *obj;

	obj = BRASERO_IMAGE_TYPE_CHOOSER (g_object_new (BRASERO_TYPE_IMAGE_TYPE_CHOOSER, NULL));
	
	return GTK_WIDGET (obj);
}
