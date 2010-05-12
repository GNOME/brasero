/***************************************************************************
*            serach-entry.c
*
*  jeu mai 19 20:10:07 2005
*  Copyright  2005  Philippe Rouquier
*  brasero-app@wanadoo.fr
****************************************************************************/

/*
 *  Brasero is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Brasero is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

#include "brasero-search-entry.h"
#include "brasero-layout.h"
#include "brasero-setting.h"

static void brasero_search_entry_class_init (BraseroSearchEntryClass *klass);
static void brasero_search_entry_init (BraseroSearchEntry *sp);
static void brasero_search_entry_finalize (GObject *object);
static void brasero_search_entry_destroy (GtkObject *gtk_object);

struct BraseroSearchEntryPrivate {
	GtkWidget *button;
	GtkWidget *combo;
	gint search_id;

	BraseroLayoutType ctx;

	gchar *keywords;

	GtkWidget *documents;
	GtkWidget *pictures;
	GtkWidget *music;
	GtkWidget *video;

};

enum {
	BRASERO_SEARCH_ENTRY_DISPLAY_COL,
	BRASERO_SEARCH_ENTRY_BACKGRD_COL,
	BRASERO_SEARCH_ENTRY_NB_COL
};

static GObjectClass *parent_class = NULL;

#define BRASERO_SEARCH_ENTRY_MAX_HISTORY_ITEMS	10

static void brasero_search_entry_button_clicked_cb (GtkButton *button,
						      BraseroSearchEntry *entry);
static void brasero_search_entry_entry_activated_cb (GtkComboBox *combo,
						      BraseroSearchEntry *entry);
static void brasero_search_entry_activated (BraseroSearchEntry *entry, gboolean query_now);
static void brasero_search_entry_set_history (BraseroSearchEntry *entry,
                                              const gchar * const *history);
static void brasero_search_entry_save_history (BraseroSearchEntry *entry);
static void brasero_search_entry_add_current_keyword_to_history (BraseroSearchEntry *entry);
static void brasero_search_entry_category_clicked_cb (GtkWidget *button, BraseroSearchEntry *entry);

typedef enum {
	ACTIVATED_SIGNAL,
	LAST_SIGNAL
} BraseroSearchEntrySignalType;

static guint brasero_search_entry_signals[LAST_SIGNAL] = { 0 };

GType
brasero_search_entry_get_type ()
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroSearchEntryClass),
			NULL,
			NULL,
			(GClassInitFunc) brasero_search_entry_class_init,
			NULL,
			NULL,
			sizeof (BraseroSearchEntry),
			0,
			(GInstanceInitFunc) brasero_search_entry_init,
		};

		type = g_type_register_static (GTK_TYPE_TABLE,
					       "BraseroSearchEntry",
					       &our_info,
					       0);
	}

	return type;
}

static void
brasero_search_entry_class_init (BraseroSearchEntryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_search_entry_finalize;
	gtk_object_class->destroy = brasero_search_entry_destroy;

	brasero_search_entry_signals[ACTIVATED_SIGNAL] =
	    g_signal_new ("activated", G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
			  G_STRUCT_OFFSET (BraseroSearchEntryClass,
					   activated), NULL, NULL,
			  g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

static gboolean
brasero_search_entry_separator_func (GtkTreeModel *model,
				     GtkTreeIter *iter,
				     gpointer data)
{
	gchar *display;
	gboolean result;

	gtk_tree_model_get (model, iter,
			    BRASERO_SEARCH_ENTRY_DISPLAY_COL, &display,
			    -1);

	if (display) {
		g_free (display);
		result = FALSE;
	}
	else
		result = TRUE;

	return result;
}

static void
brasero_search_entry_init (BraseroSearchEntry *obj)
{
	gchar *string;
	gpointer value;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *entry;
	GtkListStore *store;
	GtkCellRenderer *renderer;
	GtkEntryCompletion *completion;

	gtk_table_set_row_spacings (GTK_TABLE (obj), 6);
	gtk_table_set_col_spacings (GTK_TABLE (obj), 6);
	gtk_table_resize (GTK_TABLE (obj), 2, 3);
	obj->priv = g_new0 (BraseroSearchEntryPrivate, 1);

	string = g_strdup_printf ("<b>%s</b>", _("Search:"));
	label = gtk_label_new (string);
	g_free (string);

	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_table_attach (GTK_TABLE (obj),
			  label,
			  0, 1,
			  0, 1,
			  GTK_FILL,
			  GTK_FILL,
			  2, 2);

	obj->priv->button = gtk_button_new_from_stock (GTK_STOCK_FIND);
	gtk_table_attach (GTK_TABLE (obj),
			  obj->priv->button,
			  2, 3,
			  0, 1,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);

	g_signal_connect (G_OBJECT (obj->priv->button),
			  "clicked",
			  G_CALLBACK (brasero_search_entry_button_clicked_cb),
			  obj);

	/* Set up the combo box */
	store = gtk_list_store_new (BRASERO_SEARCH_ENTRY_NB_COL,
				    G_TYPE_STRING,
				    G_TYPE_STRING);

	obj->priv->combo = gtk_combo_box_entry_new_with_model (GTK_TREE_MODEL (store),
							       BRASERO_SEARCH_ENTRY_DISPLAY_COL);
	g_object_unref (store);
	g_signal_connect (G_OBJECT (obj->priv->combo),
			  "changed",
			  G_CALLBACK (brasero_search_entry_entry_activated_cb),
			  obj);

	gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (obj->priv->combo),
					      brasero_search_entry_separator_func,
					      obj,
					      NULL);

	gtk_cell_layout_clear (GTK_CELL_LAYOUT (obj->priv->combo));
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (obj->priv->combo),
				    renderer,
				    TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (obj->priv->combo),
					renderer,
					"text", BRASERO_SEARCH_ENTRY_DISPLAY_COL,
					"background", BRASERO_SEARCH_ENTRY_BACKGRD_COL,
					NULL);

	/* set auto completion */
	entry = gtk_bin_get_child (GTK_BIN (obj->priv->combo));
	completion = gtk_entry_completion_new ();
	gtk_entry_completion_set_model (completion, GTK_TREE_MODEL (store));
	gtk_entry_completion_set_text_column (completion, BRASERO_SEARCH_ENTRY_DISPLAY_COL);
	gtk_entry_set_completion (GTK_ENTRY (entry), completion);

	gtk_table_attach (GTK_TABLE (obj),
			  obj->priv->combo,
			  1, 2,
			  0, 1,
			  GTK_FILL|GTK_EXPAND,
			  GTK_FILL,
			  0, 0);

	/* Table with filtering options */
	table = gtk_table_new (2, 2, FALSE);
	gtk_table_set_col_spacings (GTK_TABLE (table), 10);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);

	gtk_table_attach (GTK_TABLE (obj),
			  table,
			  1, 2,
			  1, 2,
			  GTK_FILL,
			  GTK_FILL,
			  0, 0);

	obj->priv->documents = gtk_check_button_new_with_mnemonic (_("In _text documents"));
	gtk_button_set_alignment (GTK_BUTTON (obj->priv->documents), 0.0, 0.0);
	gtk_table_attach (GTK_TABLE (table),
			  obj->priv->documents,
			  1, 2,
			  0, 1,
			  GTK_FILL,
			  GTK_FILL,
			  0,
			  0);
	g_signal_connect (obj->priv->documents,
			  "clicked",
			  G_CALLBACK (brasero_search_entry_category_clicked_cb),
			  obj);

	obj->priv->pictures = gtk_check_button_new_with_mnemonic (_("In _pictures"));
	gtk_button_set_alignment (GTK_BUTTON (obj->priv->pictures), 0.0, 0.0);
	gtk_table_attach (GTK_TABLE (table),
			  obj->priv->pictures,
			  1, 2,
			  1, 2,
			  GTK_FILL,
			  GTK_FILL,
			  0,
			  0);
	g_signal_connect (obj->priv->pictures,
			  "clicked",
			  G_CALLBACK (brasero_search_entry_category_clicked_cb),
			  obj);

	obj->priv->music = gtk_check_button_new_with_mnemonic (_("In _music"));
	gtk_button_set_alignment (GTK_BUTTON (obj->priv->music), 0.0, 0.0);
	gtk_table_attach (GTK_TABLE (table),
			  obj->priv->music,
			  2, 3,
			  0, 1,
			  GTK_FILL,
			  GTK_FILL,
			  0,
			  0);
	g_signal_connect (obj->priv->music,
			  "clicked",
			  G_CALLBACK (brasero_search_entry_category_clicked_cb),
			  obj);

	obj->priv->video = gtk_check_button_new_with_mnemonic (_("In _videos"));
	gtk_button_set_alignment (GTK_BUTTON (obj->priv->video), 0.0, 0.0);
	gtk_table_attach (GTK_TABLE (table),
			  obj->priv->video,
			  2, 3,
			  1, 2,
			  GTK_FILL,
			  GTK_FILL,
			  0,
			  0);
	g_signal_connect (obj->priv->video,
			  "clicked",
			  G_CALLBACK (brasero_search_entry_category_clicked_cb),
			  obj);

	/* add tooltips */
	gtk_widget_set_tooltip_text (gtk_bin_get_child (GTK_BIN (obj->priv->combo)),
	                             _("Type your keywords or choose 'All files' from the menu"));

	/* Translators: this is an image, a picture, not a "Disc Image" */
	gtk_widget_set_tooltip_text (obj->priv->pictures,
				     _("Select if you want to search among image files only"));
	gtk_widget_set_tooltip_text (obj->priv->video,
				     _("Select if you want to search among video files only"));
	gtk_widget_set_tooltip_text (obj->priv->music,
				     _("Select if you want to search among audio files only"));
	gtk_widget_set_tooltip_text (obj->priv->documents,
				     _("Select if you want to search among your text documents only"));
	gtk_widget_set_tooltip_text (obj->priv->button,
				     _("Click to start the search"));

	brasero_setting_get_value (brasero_setting_get_default (),
	                           BRASERO_SETTING_SEARCH_ENTRY_HISTORY,
	                           &value);

	brasero_search_entry_set_history (obj, value);
	g_strfreev (value);
}

static void
brasero_search_entry_destroy (GtkObject *gtk_object)
{
	GTK_OBJECT_CLASS (parent_class)->destroy (gtk_object);
}

static void
brasero_search_entry_finalize (GObject *object)
{
	BraseroSearchEntry *cobj;

	cobj = BRASERO_SEARCH_ENTRY (object);

	if (cobj->priv->search_id) {
		g_source_remove (cobj->priv->search_id);
		cobj->priv->search_id = 0;
	}
	if (cobj->priv->keywords) {
		g_free (cobj->priv->keywords);
		cobj->priv->keywords = NULL;
	}
	g_free (cobj->priv);
	cobj->priv = NULL;

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkWidget *
brasero_search_entry_new ()
{
	BraseroSearchEntry *obj;

	obj = BRASERO_SEARCH_ENTRY (g_object_new (BRASERO_TYPE_SEARCH_ENTRY, NULL));

	return GTK_WIDGET (obj);
}

static void
brasero_search_entry_category_clicked_cb (GtkWidget *button,
					  BraseroSearchEntry *entry)
{
	if (entry->priv->keywords) {
		g_free (entry->priv->keywords);
		entry->priv->keywords = NULL;
	}
}

static void
brasero_search_entry_button_clicked_cb (GtkButton *button,
					BraseroSearchEntry *entry)
{
	brasero_search_entry_activated (entry, TRUE);
}

static void
brasero_search_entry_entry_activated_cb (GtkComboBox *combo,
					 BraseroSearchEntry *entry)
{
	if (gtk_combo_box_get_active (GTK_COMBO_BOX (entry->priv->combo)) != -1)
		brasero_search_entry_activated (entry, TRUE);
	else
		brasero_search_entry_activated (entry, FALSE);
}

static void
brasero_search_entry_check_keywords (BraseroSearchEntry *entry) 
{
	const char *keywords;
	char *tmp;

	/* NOTE: there is a strange thing done in beagle (a BUG ?) in
	 * beagle_query_part_human_set_string the string arg is const
	 * but instead of copying the string for internal use it just
	 * sets a pointer to the string; that's why we can't free the
	 * keywords until the query has been sent and even then ... */

	keywords = gtk_entry_get_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (entry->priv->combo))));
	if (!keywords)
		return;

	/* we make sure the search is not empty */
	tmp = g_strdup (keywords);
	tmp = g_strchug (tmp);
	if (*tmp == '\0') {
		g_free (tmp);
		return;
	}

	/* make sure it's has effectively changed */
	if (entry->priv->keywords && !strcmp (tmp, entry->priv->keywords)) {
		g_free (tmp);
		return;
	}

	g_free (entry->priv->keywords);
	entry->priv->keywords = tmp;

	g_signal_emit (entry,
		       brasero_search_entry_signals [ACTIVATED_SIGNAL],
		       0);
	brasero_search_entry_add_current_keyword_to_history (entry);
/*	gtk_widget_set_sensitive (entry->priv->button, FALSE); */
}

/* to avoid sending a query every time the user types something
 * we simply check that the keywords haven't changed for 2 secs */
static gboolean
brasero_search_entry_activated_real (BraseroSearchEntry *entry)
{
	entry->priv->search_id = 0;
	brasero_search_entry_check_keywords (entry);
	return FALSE;
}

static void
brasero_search_entry_activated (BraseroSearchEntry *entry,
				gboolean search_now)
{
	/* we reset the timer */
	if (entry->priv->search_id) {
		g_source_remove (entry->priv->search_id);
		entry->priv->search_id = 0;
	}

	if (!search_now) {
		/* gtk_widget_set_sensitive (entry->priv->button, TRUE); */
		entry->priv->search_id = g_timeout_add_seconds (2,
								(GSourceFunc) brasero_search_entry_activated_real,
								entry);
	}
	else
		brasero_search_entry_check_keywords (entry);
}

static void
brasero_search_entry_set_history (BraseroSearchEntry *entry,
                                  const gchar * const *history)
{
	int i;
	GtkTreeIter row;
	GtkListStore *store;

	store = GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (entry->priv->combo)));
	gtk_list_store_clear (GTK_LIST_STORE (store));

	if (history) {
		for (i = 0; history [i] && i < BRASERO_SEARCH_ENTRY_MAX_HISTORY_ITEMS; i ++) {
			gtk_list_store_append (store, &row);
			gtk_list_store_set (store, &row,
					    BRASERO_SEARCH_ENTRY_DISPLAY_COL, history [i],
					    BRASERO_SEARCH_ENTRY_BACKGRD_COL, NULL,
					    -1);
		}
	}

	/* separator */
	gtk_list_store_append (store, &row);
	gtk_list_store_set (store, &row,
			    BRASERO_SEARCH_ENTRY_DISPLAY_COL, NULL,
			    BRASERO_SEARCH_ENTRY_BACKGRD_COL, NULL,
			    -1);

	/* all files entry */
	gtk_list_store_append (store, &row);
	gtk_list_store_set (store, &row,
			    BRASERO_SEARCH_ENTRY_DISPLAY_COL, _("All files"),
			    BRASERO_SEARCH_ENTRY_BACKGRD_COL, NULL,
			    //BRASERO_SEARCH_ENTRY_BACKGRD_COL, "grey90",
			    -1);
}

static void
brasero_search_entry_save_history (BraseroSearchEntry *entry)
{
	GtkTreeModel *model;
	int num_children;
	GtkTreeIter iter;
	gchar **array;
	int i = 0;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (entry->priv->combo));
	if (!gtk_tree_model_get_iter_first (model, &iter)) {
		brasero_setting_set_value (brasero_setting_get_default (),
					   BRASERO_SETTING_SEARCH_ENTRY_HISTORY,
					   NULL);
		return;
	}

	/* NOTE: the last item is not to be included nor
	 * the blank line should be included. Only
	 * substract one to have an empty row in the
	 * array. */
	num_children = gtk_tree_model_iter_n_children (model, NULL);
	array = g_new0 (gchar *, -- num_children);

	do {
		gchar *string;

		string = NULL;
		gtk_tree_model_get (model, &iter,
		                    BRASERO_SEARCH_ENTRY_DISPLAY_COL, &string,
		                    -1);

		/* break on the blank line */
		if (string == NULL)
			break;

		array [i++] = string;
	} while (gtk_tree_model_iter_next (model, &iter));

	brasero_setting_set_value (brasero_setting_get_default (),
	                           BRASERO_SETTING_SEARCH_ENTRY_HISTORY,
	                           array);
	g_strfreev (array);
}

static void
brasero_search_entry_add_current_keyword_to_history (BraseroSearchEntry *entry)
{
	const char *keywords = NULL;
	GtkTreeModel *model;
	GtkTreeIter iter;

	/* we don't want to add static entry */
	keywords =  gtk_entry_get_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (entry->priv->combo))));
	if (!keywords || !strcmp (keywords, _("All files")))
		return;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (entry->priv->combo));
	if (!gtk_tree_model_get_iter_first (model, &iter))
		return;

	/* make sure the item is not already in the list
	 * otherwise just move it up in first position */
	do {
		gchar *string;

		string = NULL;
		gtk_tree_model_get (model, &iter,
		                    BRASERO_SEARCH_ENTRY_DISPLAY_COL, &string,
		                    -1);

		/* break when we reach the blank line */
		if (!string)
			break;

		if (!strcmp (keywords, string)) {
			gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
			gtk_list_store_prepend (GTK_LIST_STORE (model), &iter);
			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			                    BRASERO_SEARCH_ENTRY_DISPLAY_COL, string,
			                    -1);
			g_free (string);
			goto end;
		}

		g_free (string);
	} while (gtk_tree_model_iter_next (model, &iter));

	/* Remember that in model we have the 10 items
	 * a NULL one and the "All Files" one */
	if (gtk_tree_model_iter_n_children (model, NULL) == BRASERO_SEARCH_ENTRY_MAX_HISTORY_ITEMS + 2) {
		gtk_tree_model_iter_nth_child (model,
		                               &iter,
		                               NULL, 
		                               BRASERO_SEARCH_ENTRY_MAX_HISTORY_ITEMS - 1);

		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
	}

	gtk_list_store_prepend (GTK_LIST_STORE (model), &iter);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
	                    BRASERO_SEARCH_ENTRY_DISPLAY_COL, keywords,
	                    -1);

end:

	brasero_search_entry_save_history (entry);
}

gboolean
brasero_search_entry_set_query (BraseroSearchEntry *entry,
                                BraseroSearchEngine *search)
{
	BraseroSearchScope scope = BRASERO_SEARCH_SCOPE_ANY;
	const gchar *keywords = NULL;

	if (strcmp (entry->priv->keywords, _("All files")))
		keywords = entry->priv->keywords;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (entry->priv->documents)))
		scope |= BRASERO_SEARCH_SCOPE_DOCUMENTS;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (entry->priv->pictures)))
		scope |= BRASERO_SEARCH_SCOPE_PICTURES;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (entry->priv->music)))
		scope |= BRASERO_SEARCH_SCOPE_MUSIC;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (entry->priv->video)))
		scope |= BRASERO_SEARCH_SCOPE_VIDEO;

	brasero_search_engine_new_query (search, keywords);
	brasero_search_engine_set_query_scope (search, scope);

	return TRUE;
}

void
brasero_search_entry_set_context (BraseroSearchEntry *self,
				  BraseroLayoutType type)
{
	if (self->priv->ctx == type)
		return;

	self->priv->ctx = type;
	if (type == BRASERO_LAYOUT_AUDIO) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->priv->video), FALSE);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->priv->music), TRUE);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->priv->pictures), FALSE);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->priv->documents), FALSE);
	}
	else if (type == BRASERO_LAYOUT_VIDEO) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->priv->video), TRUE);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->priv->music), FALSE);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->priv->pictures), FALSE);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->priv->documents), FALSE);
	}
}

