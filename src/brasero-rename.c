/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Brasero
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
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

#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n.h>

#include <gtk/gtk.h>

#include "brasero-rename.h"

typedef struct _BraseroRenamePrivate BraseroRenamePrivate;
struct _BraseroRenamePrivate
{
	GtkWidget *notebook;
	GtkWidget *combo;

	GtkWidget *insert_entry;
	GtkWidget *insert_combo;

	GtkWidget *delete_entry;

	GtkWidget *substitute_entry;
	GtkWidget *joker_entry;

	GtkWidget *number_left_entry;
	GtkWidget *number_right_entry;
	guint number;

	guint show_default:1;
};

#define BRASERO_RENAME_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_RENAME, BraseroRenamePrivate))



G_DEFINE_TYPE (BraseroRename, brasero_rename, GTK_TYPE_BOX);

void
brasero_rename_set_show_keep_default (BraseroRename *self,
				      gboolean show)
{
	BraseroRenamePrivate *priv;

	priv = BRASERO_RENAME_PRIVATE (self);

	if (!show) {
		if (!priv->show_default)
			return;

		gtk_combo_box_text_remove (GTK_COMBO_BOX_TEXT (priv->combo), 0);

		/* make sure there is one item active */
		if (gtk_combo_box_get_active (GTK_COMBO_BOX (priv->combo)) == -1) {
			gtk_combo_box_set_active (GTK_COMBO_BOX (priv->combo), 0);
			gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), 0);
			gtk_widget_show (priv->notebook);
		}
	}
	else {
		if (priv->show_default)
			return;

		gtk_combo_box_text_prepend_text  (GTK_COMBO_BOX_TEXT (priv->combo),
						  _("<Keep current values>"));
	}

	priv->show_default = show;
}

static gchar *
brasero_rename_insert_string (BraseroRename *self,
			      const gchar *name)
{
	BraseroRenamePrivate *priv;
	gboolean is_at_end;
	const gchar *text;

	priv = BRASERO_RENAME_PRIVATE (self);

	text = gtk_entry_get_text (GTK_ENTRY (priv->insert_entry));
	is_at_end = gtk_combo_box_get_active (GTK_COMBO_BOX (priv->insert_combo));

	if (is_at_end)
		return g_strconcat (name, text, NULL);

	return g_strconcat (text, name, NULL);
}

static gchar *
brasero_rename_delete_string (BraseroRename *self,
			      const gchar *name)
{
	BraseroRenamePrivate *priv;
	const gchar *text;
	gchar *occurrence;

	priv = BRASERO_RENAME_PRIVATE (self);

	text = gtk_entry_get_text (GTK_ENTRY (priv->delete_entry));
	occurrence = g_strstr_len (name, -1, text);

	if (!occurrence)
		return NULL;

	return g_strdup_printf ("%.*s%s", (int) (occurrence - name), name, occurrence + strlen (text));
}

static gchar *
brasero_rename_substitute_string (BraseroRename *self,
			 	  const gchar *name)
{
	BraseroRenamePrivate *priv;
	const gchar *joker;
	const gchar *text;
	gchar *occurrence;

	priv = BRASERO_RENAME_PRIVATE (self);

	text = gtk_entry_get_text (GTK_ENTRY (priv->substitute_entry));
	occurrence = g_strstr_len (name, -1, text);

	if (!occurrence)
		return NULL;

	joker = gtk_entry_get_text (GTK_ENTRY (priv->joker_entry));
	return g_strdup_printf ("%.*s%s%s", (int) (occurrence - name), name, joker, occurrence + strlen (text));
}

static gchar *
brasero_rename_number_string (BraseroRename *self,
			      const gchar *name)
{
	BraseroRenamePrivate *priv;
	const gchar *right;
	const gchar *left;

	priv = BRASERO_RENAME_PRIVATE (self);

	left = gtk_entry_get_text (GTK_ENTRY (priv->number_left_entry));
	right = gtk_entry_get_text (GTK_ENTRY (priv->number_right_entry));
	return g_strdup_printf ("%s%i%s", left, priv->number ++, right);
}

static gchar *
brasero_rename_sequence_string (BraseroRename *self,
                                const gchar *name,
                                guint item_num,
                                guint nb_items)
{
	BraseroRenamePrivate *priv;

	priv = BRASERO_RENAME_PRIVATE (self);

	if (!gtk_combo_box_get_active (GTK_COMBO_BOX (priv->insert_combo)))
		return NULL;

	if (nb_items < 10){
		return g_strdup_printf ("%i%s", item_num, name);
	} else if (nb_items < 100){
		return g_strdup_printf ("%02i%s", item_num, name);       
	} else if (nb_items < 1000){
		return g_strdup_printf ("%03i%s", item_num, name);       
	} else {
		return g_strdup_printf ("%04i%s", item_num, name); 
	}
}

gboolean
brasero_rename_do (BraseroRename *self,
		   GtkTreeSelection *selection,
		   guint column_num,
		   BraseroRenameCallback callback)
{
	BraseroRenamePrivate *priv;
	GtkTreeModel *model;
	GList *selected;
	guint item_num;
	guint nb_items;
	GList *item;
	gint mode;

	priv = BRASERO_RENAME_PRIVATE (self);
	mode = gtk_combo_box_get_active (GTK_COMBO_BOX (priv->combo));

	mode -= priv->show_default;
	if (mode < 0)
		return TRUE;

	selected = gtk_tree_selection_get_selected_rows (selection, &model);

	nb_items = g_list_length (selected);
	item_num = 0;

	for (item = selected; item; item = item->next, item_num ++) {
		GtkTreePath *treepath;
		GtkTreeIter iter;
		gboolean result;
		gchar *new_name;
		gchar *name;

		treepath = item->data;
		gtk_tree_model_get_iter (model, &iter, treepath);

		gtk_tree_model_get (model, &iter,
				    column_num, &name,
				    -1);

redo:
		switch (mode) {
		case 0:
			new_name = brasero_rename_insert_string (self, name);
			break;
		case 1:
			new_name = brasero_rename_delete_string (self, name);
			break;
		case 2:
			new_name = brasero_rename_substitute_string (self, name);
			break;
		case 3:
			new_name = brasero_rename_number_string (self, name);
			break;
		case 4:
			new_name = brasero_rename_sequence_string (self, name, item_num, nb_items);
			break;
		default:
			new_name = NULL;
			break;
		}

		if (!new_name) {
			g_free (name);
			continue;
		}

		result = callback (model, &iter, treepath, name, new_name);

		if (!result) {
			if (mode == 3)
				goto redo;

			g_free (name);
			break;
		}
		g_free (name);
	}

	g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (selected);
	return TRUE;
}

static void
brasero_rename_type_changed (GtkComboBox *combo,
			     BraseroRename *self)
{
	BraseroRenamePrivate *priv;

	priv = BRASERO_RENAME_PRIVATE (self);

	if (gtk_combo_box_get_active (combo) - priv->show_default < 0
	||  gtk_combo_box_get_active (combo) - priv->show_default == 4) {
		gtk_widget_hide (priv->notebook);
		return;
	}

	gtk_widget_show (priv->notebook);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook),
				       gtk_combo_box_get_active (combo) - priv->show_default);
}

static void
brasero_rename_init (BraseroRename *object)
{
	BraseroRenamePrivate *priv;
	GtkWidget *combo;
	GtkWidget *entry;
	GtkWidget *label;
	GtkWidget *hbox;

	priv = BRASERO_RENAME_PRIVATE (object);

	gtk_orientable_set_orientation (GTK_ORIENTABLE (object), GTK_ORIENTATION_VERTICAL);

	priv->notebook = gtk_notebook_new ();
	gtk_widget_show (priv->notebook);
	gtk_box_pack_end (GTK_BOX (object), priv->notebook, FALSE, FALSE, 4);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (priv->notebook), FALSE);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), FALSE);

	priv->combo = gtk_combo_box_text_new ();
	gtk_widget_show (priv->combo);
	gtk_box_pack_start (GTK_BOX (object), priv->combo, FALSE, FALSE, 0);

	priv->show_default = 1;
	gtk_combo_box_text_prepend_text (GTK_COMBO_BOX_TEXT (priv->combo), _("<Keep current values>"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (priv->combo), _("Insert text"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (priv->combo), _("Delete text"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (priv->combo), _("Substitute text"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (priv->combo), _("Number files according to a pattern"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (priv->combo), _("Insert number sequence at beginning"));

	g_signal_connect (priv->combo,
			  "changed",
			  G_CALLBACK (brasero_rename_type_changed),
			  object);

	gtk_combo_box_set_active (GTK_COMBO_BOX (priv->combo), 0);

	/* Insert */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_show (hbox);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), hbox, NULL);

	/* Translators: This is a verb. This is completed later */
	label = gtk_label_new (_("Insert"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	entry = gtk_entry_new ();
	gtk_widget_show (entry);
	gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
	priv->insert_entry = entry;

	combo = gtk_combo_box_text_new ();
	gtk_widget_show (combo);

	/* Translators: This finishes previous action "Insert". It goes like
	 * this: "Insert" [Entry] "at the beginning". */
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("at the beginning"));

	/* Translators: This finishes previous action "Insert". It goes like
	 * this: "Insert" [Entry] "at the end". */
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("at the end"));

	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
	gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, FALSE, 0);
	priv->insert_combo = combo;

	/* Delete */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_show (hbox);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), hbox, NULL);

	label = gtk_label_new (_("Delete every occurrence of"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	entry = gtk_entry_new ();
	gtk_widget_show (entry);
	gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
	priv->delete_entry = entry;

	/* Substitution */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_show (hbox);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), hbox, NULL);

	/* Translators: this is a verb */
	label = gtk_label_new_with_mnemonic (_("_Replace"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	entry = gtk_entry_new ();
	gtk_widget_show (entry);
	gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
	priv->substitute_entry = entry;

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);

	/* Reminder: if this string happens to be used somewhere else in brasero
	 * we'll need a context with C_() macro */
	/* Translators: this goes with above verb to say "_Replace" [Entry]
	 * "with" [Entry]. */
	label = gtk_label_new (_("with"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	entry = gtk_entry_new ();
	gtk_widget_show (entry);
	gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
	priv->joker_entry = entry;

	/* Pattern */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_show (hbox);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), hbox, NULL);

	label = gtk_label_new (_("Rename to"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	entry = gtk_entry_new ();
	gtk_widget_show (entry);
	gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
	priv->number_left_entry = entry;

	label = gtk_label_new (_("{number}"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	entry = gtk_entry_new ();
	gtk_widget_show (entry);
	gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
	priv->number_right_entry = entry;
}

static void
brasero_rename_finalize (GObject *object)
{
	G_OBJECT_CLASS (brasero_rename_parent_class)->finalize (object);
}

static void
brasero_rename_class_init (BraseroRenameClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroRenamePrivate));

	object_class->finalize = brasero_rename_finalize;
}

GtkWidget *
brasero_rename_new (void)
{
	return g_object_new (BRASERO_TYPE_RENAME, NULL);
}
