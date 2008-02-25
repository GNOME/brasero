/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2008 <bonfire-app@wanadoo.fr>
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
#include <glib/gstdio.h>

#include <gtk/gtklabel.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtktreeviewcolumn.h>
#include <gtk/gtkcellrenderer.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtksizegroup.h>


#include "brasero-file-filtered.h"
#include "brasero-filter-option.h"
#include "brasero-utils.h"
#include "brasero-data-vfs.h"

enum  {
	STOCK_ID_COL,
	UNESCAPED_URI_COL,
	URI_COL,
	TYPE_COL,
	STATUS_COL,
	ACTIVABLE_COL,
	NB_COL,
};

typedef struct _BraseroFileFilteredPrivate BraseroFileFilteredPrivate;
struct _BraseroFileFilteredPrivate
{
	GtkWidget *tree;
	GtkWidget *restore;
	GtkWidget *options;
	guint num;
};

#define BRASERO_FILE_FILTERED_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_FILE_FILTERED, BraseroFileFilteredPrivate))

enum
{
	FILTERED_SIGNAL,
	RESTORED_SIGNAL,

	LAST_SIGNAL
};


static guint file_filtered_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (BraseroFileFiltered, brasero_file_filtered, GTK_TYPE_EXPANDER);


static gchar *
brasero_file_filtered_get_label_text (guint num, gboolean expanded)
{
	gchar *markup;
	gchar *label;

	if (expanded) {
		if (!num)
			label = g_strdup (_("Hide the _filtered file list (no file)"));
		else
			label = g_strdup_printf (ngettext ("Hide the _filtered file list (%d file)", "Hide the _filtered file list (%d files)", num), num);
	}
	else {
		if (!num)
			label = g_strdup (_("Show the _filtered file list (no file)"));
		else
			label = g_strdup_printf (ngettext ("Show the _filtered file list (%d file)", "Show the _filtered file list (%d files)", num), num);
	}

	markup = g_strdup_printf ("<span weight=\"bold\" size=\"medium\">%s</span>", label);
	g_free (label);

	return markup;
}

static void
brasero_file_filtered_update (BraseroFileFiltered *self)
{
	BraseroFileFilteredPrivate *priv;
	GtkWidget *widget;
	gchar *markup;

	priv = BRASERO_FILE_FILTERED_PRIVATE (self);

	markup = brasero_file_filtered_get_label_text (priv->num, gtk_expander_get_expanded (GTK_EXPANDER (self)));
	widget = gtk_expander_get_label_widget (GTK_EXPANDER (self));
	gtk_label_set_markup_with_mnemonic (GTK_LABEL (widget), markup);
	g_free (markup);
}

static void
brasero_file_filtered_activate (GtkExpander *self)
{
	GTK_EXPANDER_CLASS (brasero_file_filtered_parent_class)->activate (self);
	brasero_file_filtered_update (BRASERO_FILE_FILTERED (self));
}

void
brasero_file_filtered_add (BraseroFileFiltered *self,
			   const gchar *uri,
			   BraseroFilterStatus status)
{
	gchar *labels [] = { N_("hidden file"),
			     N_("unreadable file"),
			     N_("broken symlink"),
			     N_("recursive symlink"),
			     NULL };
	BraseroFileFilteredPrivate *priv;
	const gchar *stock_id;
	gchar *unescaped_uri;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *type;

	priv = BRASERO_FILE_FILTERED_PRIVATE (self);

	type = labels [ status - 1 ];
	if (status == BRASERO_FILTER_UNREADABLE)
		stock_id = GTK_STOCK_CANCEL;
	else
		stock_id = NULL;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree));
	gtk_list_store_append (GTK_LIST_STORE (model), &iter);

	unescaped_uri = g_uri_unescape_string (uri, NULL);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			    STOCK_ID_COL, stock_id,
			    UNESCAPED_URI_COL, unescaped_uri,
			    URI_COL, uri,
			    TYPE_COL, _(type),
			    STATUS_COL, status,
		    ACTIVABLE_COL, (status != BRASERO_FILTER_UNREADABLE && status != BRASERO_FILTER_RECURSIVE_SYM),
			    -1);

	g_free (unescaped_uri);

	/* update label */
	priv->num ++;
	brasero_file_filtered_update (self);
}

static void
brasero_file_filtered_option_pressed_cb (GtkButton *button,
					 BraseroFileFiltered *self)
{
	GtkWidget *option;
	GtkWidget *dialog;
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
	dialog = gtk_dialog_new_with_buttons (_("Filter Options"),
					      GTK_WINDOW (toplevel),
					      GTK_DIALOG_DESTROY_WITH_PARENT |
					      GTK_DIALOG_MODAL,
					      GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
					      NULL);
	option = brasero_filter_option_new ();
	gtk_widget_show (option);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), option, FALSE, FALSE, 0);
	gtk_widget_show (dialog);

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);	
}

static void
brasero_file_filtered_restore_pressed_cb (GtkButton *button,
					  BraseroFileFiltered *self)
{
	BraseroFileFilteredPrivate *priv;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GList *selected;
	GList *iter;

	priv = BRASERO_FILE_FILTERED_PRIVATE (self);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));
	selected = gtk_tree_selection_get_selected_rows (selection, &model);
	selected = g_list_reverse (selected);

	for (iter = selected; iter; iter = iter->next) {
		GtkTreePath *treepath;
		GtkTreeIter treeiter;
		gchar *uri;

		treepath = iter->data;
		gtk_tree_model_get_iter (model, &treeiter, treepath);
		gtk_tree_path_free (treepath);

		gtk_tree_model_get (model, &treeiter,
				    URI_COL, &uri, 
				    -1);
		g_signal_emit (self, file_filtered_signals [RESTORED_SIGNAL], 0, uri);
		gtk_list_store_remove (GTK_LIST_STORE (model), &treeiter);
		priv->num --;
	}
	g_list_free (selected);

	/* update label */
	brasero_file_filtered_update (self);
}

static void
brasero_file_filtered_selection_changed_cb (GtkTreeSelection *selection,
					    BraseroFileFiltered *self)
{
	BraseroFileFilteredPrivate *priv;

	priv = BRASERO_FILE_FILTERED_PRIVATE (self);

	if (gtk_tree_selection_count_selected_rows (selection))
		gtk_widget_set_sensitive (priv->restore, TRUE);
	else
		gtk_widget_set_sensitive (priv->restore, FALSE);
}

void
brasero_file_filtered_clear (BraseroFileFiltered *self)
{
	BraseroFileFilteredPrivate *priv;
	GtkTreeModel *model;

	priv = BRASERO_FILE_FILTERED_PRIVATE (self);

	priv->num = 0;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree));
	gtk_list_store_clear (GTK_LIST_STORE (model));

	brasero_file_filtered_update (self);
}

void
brasero_file_filtered_set_right_button_group (BraseroFileFiltered *self,
					      GtkSizeGroup *group)
{
	BraseroFileFilteredPrivate *priv;

	priv = BRASERO_FILE_FILTERED_PRIVATE (self);
	gtk_size_group_add_widget (group, priv->restore);
	gtk_size_group_add_widget (group, priv->options);
}

static void
brasero_file_filtered_init (BraseroFileFiltered *object)
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *button;
	GtkWidget *scroll;
	GtkWidget *mainbox;
	GtkListStore *model;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	BraseroFileFilteredPrivate *priv;

	priv = BRASERO_FILE_FILTERED_PRIVATE (object);

	gtk_widget_set_tooltip_text (GTK_WIDGET (object), _("Select the files you want to restore and click on the \"Restore\" button"));

	mainbox = gtk_vbox_new (FALSE, 10);
	gtk_widget_show (mainbox);
	gtk_container_add (GTK_CONTAINER (object), mainbox);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_widget_show (hbox);

	model = gtk_list_store_new (NB_COL,
				    G_TYPE_STRING,
				    G_TYPE_STRING,
				    G_TYPE_STRING,
				    G_TYPE_STRING,
				    G_TYPE_INT,
				    G_TYPE_BOOLEAN);

	priv->tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
	gtk_widget_show (priv->tree);
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (priv->tree), TRUE);
	gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree)),
				     GTK_SELECTION_MULTIPLE);
	gtk_tree_view_set_rubber_banding (GTK_TREE_VIEW (priv->tree), TRUE);
	g_object_unref (model);

	g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree)),
						       "changed",
						       G_CALLBACK (brasero_file_filtered_selection_changed_cb),
						       object);
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_expand (column, TRUE);

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "stock-id", STOCK_ID_COL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_end (column, renderer, TRUE);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "text", URI_COL);
	gtk_tree_view_column_set_title (column, _("Files"));
	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->tree), column);
	gtk_tree_view_column_set_sort_column_id (column, URI_COL);
	gtk_tree_view_column_set_clickable (column, TRUE);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_min_width (column, 450);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Type"), renderer,
							   "text", TYPE_COL,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->tree), column);
	gtk_tree_view_column_set_sort_column_id (column, TYPE_COL);
	gtk_tree_view_column_set_clickable (column, TRUE);

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scroll);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll),
					     GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scroll), priv->tree);

	gtk_box_pack_start (GTK_BOX (hbox),
			    scroll,
			    TRUE,
			    TRUE,
			    0);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

	button = gtk_button_new_with_mnemonic (_("_Restore"));
	gtk_widget_show (button);
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
	g_signal_connect (button,
			  "clicked",
			  G_CALLBACK (brasero_file_filtered_restore_pressed_cb),
			  object);
	priv->restore = button;
	gtk_widget_set_sensitive (priv->restore, FALSE);
	gtk_widget_set_tooltip_text (priv->restore, _("Restore the selected files"));

	button = gtk_button_new_with_mnemonic (_("_Options..."));
	gtk_widget_show (button);
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
	g_signal_connect (button,
			  "clicked",
			  G_CALLBACK (brasero_file_filtered_option_pressed_cb),
			  object);
	priv->options = button;
	gtk_widget_set_tooltip_text (priv->options, _("Set the options for file filtering"));

	gtk_box_pack_start (GTK_BOX (mainbox),
			    hbox,
			    TRUE,
			    TRUE,
			    0);
}

static void
brasero_file_filtered_finalize (GObject *object)
{
	G_OBJECT_CLASS (brasero_file_filtered_parent_class)->finalize (object);
}

static void
brasero_file_filtered_class_init (BraseroFileFilteredClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkExpanderClass *expander_class = GTK_EXPANDER_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroFileFilteredPrivate));

	object_class->finalize = brasero_file_filtered_finalize;

	expander_class->activate = brasero_file_filtered_activate;

	file_filtered_signals[FILTERED_SIGNAL] =
		g_signal_new ("filtered",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		              G_STRUCT_OFFSET (BraseroFileFilteredClass, filtered),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__STRING,
		              G_TYPE_NONE, 1,
		              G_TYPE_STRING);

	file_filtered_signals[RESTORED_SIGNAL] =
		g_signal_new ("restored",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		              G_STRUCT_OFFSET (BraseroFileFilteredClass, restored),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__STRING,
		              G_TYPE_NONE, 1,
		              G_TYPE_STRING);
}

GtkWidget*
brasero_file_filtered_new (void)
{
	gchar *markup;
	GtkWidget *object;

	markup = brasero_file_filtered_get_label_text (0, FALSE);
	object = g_object_new (BRASERO_TYPE_FILE_FILTERED,
			       "label", markup,
			       "use-markup", TRUE,
			       "use-underline", TRUE,
			       NULL);
	g_free (markup);

	return object;
}
