/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2005-2008 <bonfire-app@wanadoo.fr>
 * 
 * brasero is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * brasero is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>

#include <gtk/gtk.h>

#include "brasero-jacket-font.h"

typedef struct _BraseroJacketFontPrivate BraseroJacketFontPrivate;
struct _BraseroJacketFontPrivate
{
	GtkWidget *family;
	GtkWidget *size;

	gint current_size;
	gchar *current_family;
};

#define BRASERO_JACKET_FONT_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_JACKET_FONT, BraseroJacketFontPrivate))

enum
{
	FONT_CHANGED,
	SIZE_CHANGED,

	LAST_SIGNAL
};

enum {
	FAMILY_STRING_COL,
	FAMILY_COL,
	FAMILY_COL_NB
};

enum {
	SIZE_STRING_COL,
	SIZE_COL,
	SIZE_COL_NB	
};

static const guint16 font_sizes[] = {
  6, 7, 8, 9, 10, 11, 12, 13, 14, 16, 18, 20, 22, 24, 26, 28,
  32, 36, 40, 48, 56, 64, 72
};

static guint jacket_font_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (BraseroJacketFont, brasero_jacket_font, GTK_TYPE_HBOX);

static void
brasero_jacket_font_family_changed_cb (GtkComboBox *combo,
				       BraseroJacketFont *self)
{
	g_signal_emit (self,
		       jacket_font_signals [FONT_CHANGED],
		       0);
}

static void
brasero_jacket_font_size_changed_cb (GtkComboBox *combo,
				     BraseroJacketFont *self)
{
	g_signal_emit (self,
		       jacket_font_signals [SIZE_CHANGED],
		       0);	
}

void
brasero_jacket_font_set_name (BraseroJacketFont *self,
			      const gchar *string)
{
	BraseroJacketFontPrivate *priv;
	PangoFontDescription *desc;
	const gchar *family_name;
	const gchar *name = NULL;
	PangoFontFamily *family;
	GtkTreeModel *model;
	GtkTreeIter iter;
	guint font_size;
	guint size;

	priv = BRASERO_JACKET_FONT_PRIVATE (self);

	desc = pango_font_description_from_string (string);
	family_name = pango_font_description_get_family (desc);
	font_size = pango_font_description_get_size (desc);

	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (priv->family), &iter)) {
		family = NULL;

		model = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->family));
		gtk_tree_model_get (model, &iter,
				    FAMILY_COL, &family,
				    -1);
		if (family)
			name = pango_font_family_get_name (family);
	}

	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (priv->size), &iter)) {
		model = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->size));
		gtk_tree_model_get (model, &iter,
				    SIZE_COL, &size,
				    -1);
		if (family_name && name && !strcmp (family_name, name) && size == font_size) {
			pango_font_description_free (desc);
			return;
		}
	}

	g_signal_handlers_block_by_func (priv->family,
					 brasero_jacket_font_family_changed_cb,
					 self);

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->family));
	gtk_tree_model_get_iter_first (model, &iter);
	do {
		gtk_tree_model_get (model, &iter,
				    FAMILY_COL, &family,
				    -1);

		name = pango_font_family_get_name (family);
		if (!strcmp (family_name, name)) {
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (priv->family), &iter);
			break;
		}

	} while (gtk_tree_model_iter_next (model, &iter));

	g_signal_handlers_unblock_by_func (priv->family,
					   brasero_jacket_font_family_changed_cb,
					   self);

	g_signal_handlers_block_by_func (priv->size,
					 brasero_jacket_font_size_changed_cb,
					 self);

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->size));
	gtk_tree_model_get_iter_first (model, &iter);
	do {
		gtk_tree_model_get (model, &iter,
				    SIZE_COL, &size,
				    -1);

		if (size == font_size / PANGO_SCALE) {
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (priv->size), &iter);
			break;
		}

	} while (gtk_tree_model_iter_next (model, &iter));

	g_signal_handlers_unblock_by_func (priv->size,
					   brasero_jacket_font_size_changed_cb,
					   self);

	pango_font_description_free (desc);
}

gchar *
brasero_jacket_font_get_family (BraseroJacketFont *self)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	gchar *family = NULL;
	BraseroJacketFontPrivate *priv;

	priv = BRASERO_JACKET_FONT_PRIVATE (self);

	if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (priv->family), &iter))
		return NULL;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->family));
	gtk_tree_model_get (model, &iter,
			    FAMILY_STRING_COL, &family,
			    -1);
	return family;
}

guint
brasero_jacket_font_get_size (BraseroJacketFont *self)
{
	guint size;
	GtkTreeIter iter;
	GtkTreeModel *model;
	BraseroJacketFontPrivate *priv;

	priv = BRASERO_JACKET_FONT_PRIVATE (self);

	if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (priv->size), &iter))
		return 0;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->size));
	gtk_tree_model_get (model, &iter,
			    SIZE_COL, &size,
			    -1);

	return size * PANGO_SCALE;
}
static void
brasero_jacket_fill_sizes (BraseroJacketFont *self)
{
	BraseroJacketFontPrivate *priv;
	GtkTreeModel *model;
	gint i;

	priv = BRASERO_JACKET_FONT_PRIVATE (self);

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->size));
	for (i = 0; i < G_N_ELEMENTS (font_sizes); i ++) {
		GtkTreeIter iter;
		gchar *string;

		string = g_strdup_printf ("%i", font_sizes [i]);
		gtk_list_store_append (GTK_LIST_STORE (model), &iter);
		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				    SIZE_STRING_COL, string,
				    SIZE_COL, font_sizes [i],
				    -1);
		g_free (string);
	}
}

static void
brasero_jacket_fill_families (BraseroJacketFont *self)
{
	BraseroJacketFontPrivate *priv;
	PangoFontFamily **families;
	GtkTreeModel *model;
	gint num = 0;
	gint i;

	priv = BRASERO_JACKET_FONT_PRIVATE (self);
	pango_context_list_families (gtk_widget_get_pango_context (GTK_WIDGET (self)),
				     &families, &num);

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->family));
	for (i = 0; i < num; i ++) {
		const gchar *name;
		GtkTreeIter iter;

		name = pango_font_family_get_name (families [i]);
		gtk_list_store_append (GTK_LIST_STORE (model), &iter);
		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				    FAMILY_COL, families [i],
				    FAMILY_STRING_COL, name,
				    -1);
	}

	g_free (families);
}

static void
brasero_jacket_font_init (BraseroJacketFont *object)
{
	GtkListStore *store;
	GtkCellRenderer *renderer;
	BraseroJacketFontPrivate *priv;

	priv = BRASERO_JACKET_FONT_PRIVATE (object);

	gtk_box_set_homogeneous (GTK_BOX (object), FALSE);

	store = gtk_list_store_new (FAMILY_COL_NB,
				    G_TYPE_STRING,
				    G_TYPE_POINTER);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store), FAMILY_STRING_COL, GTK_SORT_ASCENDING);
	priv->family = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
	g_object_unref (store);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (priv->family), renderer, FALSE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (priv->family), renderer,
				       "text", FAMILY_STRING_COL);

	gtk_box_pack_start (GTK_BOX (object), priv->family, FALSE, FALSE, 0);
	gtk_widget_show (priv->family);
	gtk_combo_box_set_focus_on_click (GTK_COMBO_BOX (priv->family), FALSE);

	g_signal_connect (priv->family,
			  "changed",
			  G_CALLBACK (brasero_jacket_font_family_changed_cb),
			  object);

	store = gtk_list_store_new (SIZE_COL_NB,
				    G_TYPE_STRING,
				    G_TYPE_UINT);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store), SIZE_COL, GTK_SORT_ASCENDING);
	priv->size = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
	g_object_unref (store);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (priv->size), renderer, FALSE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (priv->size), renderer,
				       "text", SIZE_COL);

	gtk_box_pack_start (GTK_BOX (object), priv->size, FALSE, FALSE, 0);
	gtk_widget_show (priv->size);
	gtk_combo_box_set_focus_on_click (GTK_COMBO_BOX (priv->size), FALSE);

	g_signal_connect (priv->size,
			  "changed",
			  G_CALLBACK (brasero_jacket_font_size_changed_cb),
			  object);

	brasero_jacket_fill_families (object);
	brasero_jacket_fill_sizes (object);
}

static void
brasero_jacket_font_finalize (GObject *object)
{
	G_OBJECT_CLASS (brasero_jacket_font_parent_class)->finalize (object);
}

static void
brasero_jacket_font_class_init (BraseroJacketFontClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroJacketFontPrivate));

	object_class->finalize = brasero_jacket_font_finalize;

	jacket_font_signals[FONT_CHANGED] =
		g_signal_new ("font_changed",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_NO_RECURSE | G_SIGNAL_ACTION | G_SIGNAL_NO_HOOKS,
		              0,
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0,
		              G_TYPE_NONE);

	jacket_font_signals[SIZE_CHANGED] =
		g_signal_new ("size_changed",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_NO_RECURSE | G_SIGNAL_ACTION | G_SIGNAL_NO_HOOKS,
		              0,
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0,
		              G_TYPE_NONE);
}

GtkWidget *
brasero_jacket_font_new (void)
{
	return g_object_new (BRASERO_TYPE_JACKET_FONT, NULL);
}
