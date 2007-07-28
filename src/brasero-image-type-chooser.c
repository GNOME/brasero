/***************************************************************************
 *            brasero-image-type-chooser.c
 *
 *  mar oct  3 18:40:02 2006
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtklabel.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkcombobox.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtkcelllayout.h>
#include <gtk/gtkcellrenderertext.h>

#include "burn-basics.h"
#include "burn-caps.h"
#include "brasero-image-type-chooser.h"

#define BRASERO_IMAGE_TYPE_CHOOSER_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_IMAGE_TYPE_CHOOSER, BraseroImageTypeChooserPrivate))

G_DEFINE_TYPE (BraseroImageTypeChooser, brasero_image_type_chooser, GTK_TYPE_HBOX);

enum {
	FORMAT_TEXT,
	FORMAT_TYPE,
	FORMAT_LAST
};

enum {
	CHANGED_SIGNAL,
	LAST_SIGNAL
};
static guint brasero_image_type_chooser_signals [LAST_SIGNAL] = { 0 };

struct _BraseroImageTypeChooserPrivate {
	GtkWidget *combo;

	BraseroBurnCaps *caps;
	BraseroImageFormat *formats;
};

static GtkHBoxClass *parent_class = NULL;

void
brasero_image_type_chooser_set_formats (BraseroImageTypeChooser *self,
				        BraseroImageFormat formats)
{
	GtkTreeIter iter;
	GtkTreeModel *store;
	BraseroImageFormat format;
	BraseroImageTypeChooserPrivate *priv;

	priv = BRASERO_IMAGE_TYPE_CHOOSER_PRIVATE (self);

	/* clean */
	store = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->combo));
	gtk_list_store_clear (GTK_LIST_STORE (store));

	/* save the current format to restore it later */
	brasero_image_type_chooser_get_format (self, &format);

	/* now we get the targets available and display them */
	gtk_list_store_prepend (GTK_LIST_STORE (store), &iter);
	gtk_list_store_set (GTK_LIST_STORE (store), &iter,
			    FORMAT_TEXT, _("Let brasero choose (safest)"),
			    FORMAT_TYPE, BRASERO_IMAGE_FORMAT_ANY,
			    -1);

	if (formats & BRASERO_IMAGE_FORMAT_BIN) {
		gtk_list_store_append (GTK_LIST_STORE (store), &iter);
		gtk_list_store_set (GTK_LIST_STORE (store), &iter,
				    FORMAT_TEXT, _("*.iso image"),
				    FORMAT_TYPE, BRASERO_IMAGE_FORMAT_BIN,
				    -1);
	}

	if (formats & BRASERO_IMAGE_FORMAT_CLONE) {
		gtk_list_store_append (GTK_LIST_STORE (store), &iter);
		gtk_list_store_set (GTK_LIST_STORE (store), &iter,
				    FORMAT_TEXT, _("*.raw image"),
				    FORMAT_TYPE, BRASERO_IMAGE_FORMAT_CLONE,
				    -1);
	}

	if (formats & BRASERO_IMAGE_FORMAT_CUE) {
		gtk_list_store_append (GTK_LIST_STORE (store), &iter);
		gtk_list_store_set (GTK_LIST_STORE (store), &iter,
				    FORMAT_TEXT, _("*.cue image"),
				    FORMAT_TYPE, BRASERO_IMAGE_FORMAT_CUE,
				    -1);
	}

	if (formats & BRASERO_IMAGE_FORMAT_CDRDAO) {
		gtk_list_store_append (GTK_LIST_STORE (store), &iter);
		gtk_list_store_set (GTK_LIST_STORE (store), &iter,
				    FORMAT_TEXT, _("*.toc image (cdrdao)"),
				    FORMAT_TYPE, BRASERO_IMAGE_FORMAT_CDRDAO,
				    -1);
	}

	brasero_image_type_chooser_set_format (self, format);
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
	
	if (format == BRASERO_IMAGE_FORMAT_ANY
	&&  format == BRASERO_IMAGE_FORMAT_NONE) {
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
brasero_image_type_chooser_get_format (BraseroImageTypeChooser *self,
				       BraseroImageFormat *format)
{
	GtkTreeIter iter;
	GtkTreeModel *store;
	BraseroImageTypeChooserPrivate *priv;

	priv = BRASERO_IMAGE_TYPE_CHOOSER_PRIVATE (self);

	store = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->combo));

	if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (priv->combo), &iter)) {
		*format = BRASERO_IMAGE_FORMAT_NONE;
		return;
	}

	gtk_tree_model_get (store, &iter,
			    FORMAT_TYPE, format,
			    -1);
}

static void
brasero_image_type_chooser_changed_cb (GtkComboBox *combo,
				       BraseroImageTypeChooser *self)
{
	g_signal_emit (self,
		       brasero_image_type_chooser_signals [CHANGED_SIGNAL],
		       0);
}

static void
brasero_image_type_chooser_init (BraseroImageTypeChooser *obj)
{
	GtkWidget *label;
	GtkListStore *store;
	GtkCellRenderer *renderer;
	BraseroImageTypeChooserPrivate *priv;

	priv = BRASERO_IMAGE_TYPE_CHOOSER_PRIVATE (obj);

	label = gtk_label_new (_("Image type:\t"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (obj), label, FALSE, FALSE, 0);

	store = gtk_list_store_new (FORMAT_LAST,
				    G_TYPE_STRING,
				    G_TYPE_INT);

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

	priv->caps = brasero_burn_caps_get_default ();
}

static void
brasero_image_type_chooser_finalize (GObject *object)
{
	BraseroImageTypeChooserPrivate *priv;

	priv = BRASERO_IMAGE_TYPE_CHOOSER_PRIVATE (object);

	if (priv->caps) {
		g_object_unref (priv->caps);
		priv->caps = NULL;
	}

	if (priv->formats) {
		g_free (priv->formats);
		priv->formats = NULL;
	}

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
