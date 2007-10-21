/*
 * brasero-plugin-manager.c
 * This file is part of brasero
 *
 * Copyright (C) 2007 Philippe Rouquier
 *
 * Based on brasero code (brasero/brasero-plugin-manager.c) by: 
 * 	- Paolo Maggi <paolo@gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, 
 * Boston, MA 02111-1307, USA. 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <glib/gi18n.h>

#include <gconf/gconf-client.h>

#include "brasero-plugin-manager-ui.h"
#include "brasero-utils.h"
#include "burn-plugin.h"
#include "burn-caps.h"
#include "burn-plugin-private.h"
#include "burn-plugin-manager.h"
#include "brasero-plugin-option.h"

typedef enum {
	BRASERO_PLUGIN_BURN_ENGINE			= 0,
	BRASERO_PLUGIN_IMAGE_ENGINE,
	BRASERO_PLUGIN_CONVERT_ENGINE,
	BRASERO_PLUGIN_MISCELLANEOUS,
	BRASERO_PLUGIN_ERROR
} BraseroPluginCategory;

enum
{
	ACTIVE_COLUMN,
	AVAILABLE_COLUMN,
	PLUGIN_COLUMN,
	N_COLUMNS
};

#define PLUGIN_MANAGER_UI_NAME_TITLE _("Plugin")
#define PLUGIN_MANAGER_UI_ACTIVE_TITLE _("Enabled")

#define BRASERO_PLUGIN_MANAGER_UI_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), BRASERO_TYPE_PLUGIN_MANAGER_UI, BraseroPluginManagerUIPrivate))

struct _BraseroPluginManagerUIPrivate
{
	GtkWidget	*description;
	GtkWidget	*label;

	GtkWidget	*tree;

	GtkWidget	*up_button;
	GtkWidget	*down_button;
	GtkWidget	*about_button;
	GtkWidget	*configure_button;

	GtkWidget 	*about;
	
	GtkWidget	*popup_menu;

	BraseroPluginCategory category;
	gulong order_changed_id;
	GSList	*plugins;
};

G_DEFINE_TYPE(BraseroPluginManagerUI, brasero_plugin_manager_ui, GTK_TYPE_VBOX)

enum {
	TREE_MODEL_ROW,
};
static GtkTargetEntry ntables_source [] = {
	{"GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, TREE_MODEL_ROW},
};
static guint nb_targets_source = sizeof (ntables_source) / sizeof (ntables_source[0]);

static void plugin_manager_ui_toggle_active (GtkTreeIter *iter, GtkTreeModel *model); 
static void brasero_plugin_manager_ui_finalize (GObject *object);

static void 
brasero_plugin_manager_ui_class_init (BraseroPluginManagerUIClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = brasero_plugin_manager_ui_finalize;

	g_type_class_add_private (object_class, sizeof (BraseroPluginManagerUIPrivate));
}

static BraseroPlugin *
plugin_manager_ui_get_selected_plugin (BraseroPluginManagerUI *pm)
{
	BraseroPlugin *plugin = NULL;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	BraseroPluginManagerUIPrivate *priv;

	priv = BRASERO_PLUGIN_MANAGER_UI_GET_PRIVATE (pm);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree));
	g_return_val_if_fail (model != NULL, NULL);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));
	g_return_val_if_fail (selection != NULL, NULL);

	if (gtk_tree_selection_get_selected (selection, NULL, &iter))
	{
		gtk_tree_model_get (model, &iter, PLUGIN_COLUMN, &plugin, -1);
	}
	
	return plugin;
}

static void
about_button_cb (GtkWidget          *button,
		 BraseroPluginManagerUI *pm)
{
	GtkWidget *dialog;
	gchar *copyright;
	BraseroPlugin *plugin;
	BraseroPluginManagerUIPrivate *priv;
	const gchar *authors [2] = { NULL };

	priv = BRASERO_PLUGIN_MANAGER_UI_GET_PRIVATE (pm);

	plugin = plugin_manager_ui_get_selected_plugin (pm);

	g_return_if_fail (plugin != NULL);

	/* if there is another about dialog already open destroy it */
	if (priv->about)
		gtk_widget_destroy (priv->about);

	authors [0] = brasero_plugin_get_author (plugin);

	copyright = g_strdup_printf (_("Copyright %s"),
				     brasero_plugin_get_author (plugin));

	dialog = g_object_new (GTK_TYPE_ABOUT_DIALOG,
			       "program-name", brasero_plugin_get_name (plugin),
			       "copyright", copyright,
			       "authors", authors,
			       "comments", brasero_plugin_get_description (plugin),
			       "logo-icon-name", brasero_plugin_get_icon_name (plugin),
			       NULL);

	g_free (copyright);

	gtk_window_set_transient_for (GTK_WINDOW (dialog),
				      GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (pm))));

	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);

	gtk_widget_show_all (dialog);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static void
configure_button_cb (GtkWidget          *button,
		     BraseroPluginManagerUI *pm)
{
	GtkResponseType result;
	BraseroPlugin *plugin;
	GtkWindow *toplevel;
	GtkWidget *dialog;

	plugin = plugin_manager_ui_get_selected_plugin (pm);

	g_return_if_fail (plugin != NULL);

	toplevel = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET(pm)));

	dialog = brasero_plugin_option_new ();

	brasero_plugin_option_set_plugin (BRASERO_PLUGIN_OPTION (dialog), plugin);
	gtk_window_set_transient_for (GTK_WINDOW (dialog),
				      GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (pm))));

	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	
	result = gtk_dialog_run (GTK_DIALOG (dialog));
	if (result == GTK_RESPONSE_OK)
		brasero_plugin_option_save_settings (BRASERO_PLUGIN_OPTION (dialog));

	gtk_widget_destroy (dialog);
}

static void
plugin_manager_ui_view_rank_cell_cb (GtkTreeViewColumn *tree_column,
				     GtkCellRenderer   *cell,
				     GtkTreeModel      *tree_model,
				     GtkTreeIter       *iter,
				     gpointer           data)
{
	g_object_set (G_OBJECT (cell),
		      "visible", FALSE,
		      NULL);
	return;
/*
	May be removed in a near future 
	BraseroPluginManagerUIPrivate *priv;
	GtkTreePath *treepath;
	gint *indices;
	gchar *text;
	
	g_return_if_fail (tree_model != NULL);
	g_return_if_fail (tree_column != NULL);

	priv = BRASERO_PLUGIN_MANAGER_UI_GET_PRIVATE (data);
	if (priv->category == BRASERO_PLUGIN_ERROR
	||  priv->category == BRASERO_PLUGIN_MISCELLANEOUS) {
		g_object_set (G_OBJECT (cell),
			      "markup", NULL,
			      NULL);
		return;
	}

	treepath = gtk_tree_model_get_path (tree_model, iter);
	if (!treepath)
		return;

	indices = gtk_tree_path_get_indices (treepath);
	if (!indices) {
		gtk_tree_path_free (treepath);
		return;
	}

	text = g_markup_printf_escaped ("<b>%i -</b>", indices [0] + 1);
	gtk_tree_path_free (treepath);

	g_object_set (G_OBJECT (cell),
		      "markup", text,
		      NULL);
	g_free (text);
*/
}

static void
plugin_manager_ui_view_info_cell_cb (GtkTreeViewColumn *tree_column,
				     GtkCellRenderer   *cell,
				     GtkTreeModel      *tree_model,
				     GtkTreeIter       *iter,
				     gpointer           data)
{
	BraseroPlugin *plugin;
	gchar *text;
	
	g_return_if_fail (tree_model != NULL);
	g_return_if_fail (tree_column != NULL);

	gtk_tree_model_get (tree_model, iter,
			    PLUGIN_COLUMN, &plugin,
			    -1);

	if (!plugin)
		return;

	if (brasero_plugin_get_error (plugin))
		text = g_markup_printf_escaped (_("<b>%s</b>\n%s\n<i>%s</i>"),
						brasero_plugin_get_name (plugin),
						brasero_plugin_get_description (plugin),
						brasero_plugin_get_error (plugin));
	else
		text = g_markup_printf_escaped ("<b>%s</b>\n%s",
						brasero_plugin_get_name (plugin),
						brasero_plugin_get_description (plugin));

	g_object_set (G_OBJECT (cell),
		      "markup", text,
		      "sensitive", brasero_plugin_get_gtype (plugin) != G_TYPE_NONE,
		      NULL);

	g_free (text);
}

static void
plugin_manager_ui_view_icon_cell_cb (GtkTreeViewColumn *tree_column,
				     GtkCellRenderer   *cell,
				     GtkTreeModel      *tree_model,
				     GtkTreeIter       *iter,
				     gpointer           data)
{
	
	g_return_if_fail (tree_model != NULL);
	g_return_if_fail (tree_column != NULL);

	g_object_set (G_OBJECT (cell),
		      "visible", FALSE,
		      NULL);
	return;
/*
	For the time being don't use it since there is no plugin with icon 
	BraseroPlugin *plugin;

	gtk_tree_model_get (tree_model, iter, PLUGIN_COLUMN, &plugin, -1);

	if (!plugin) {
		g_object_set (G_OBJECT (cell),
			      "visible", FALSE,
			      NULL);
		return;
	}

	g_object_set (G_OBJECT (cell),
		      "visible", TRUE,
		      "icon-name",
		      brasero_plugin_get_icon_name (plugin),
		      "sensitive",
		      brasero_plugin_get_gtype (plugin) != G_TYPE_NONE,
		      NULL);
*/
}


static void
active_toggled_cb (GtkCellRendererToggle *cell,
		   gchar                 *path_str,
		   BraseroPluginManagerUI    *pm)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeModel *model;
	BraseroPluginManagerUIPrivate *priv;

	priv = BRASERO_PLUGIN_MANAGER_UI_GET_PRIVATE (pm);
	path = gtk_tree_path_new_from_string (path_str);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree));
	g_return_if_fail (model != NULL);

	gtk_tree_model_get_iter (model, &iter, path);

	if (&iter != NULL)
		plugin_manager_ui_toggle_active (&iter, model);

	gtk_tree_path_free (path);
}

static void
cursor_changed_cb (GtkTreeView *view,
		   gpointer     data)
{
	BraseroPlugin *plugin;
	BraseroPluginManagerUI *pm = data;
	BraseroPluginManagerUIPrivate *priv;

	priv = BRASERO_PLUGIN_MANAGER_UI_GET_PRIVATE (pm);
	plugin = plugin_manager_ui_get_selected_plugin (pm);

	gtk_widget_set_sensitive (GTK_WIDGET (priv->about_button),
				  plugin != NULL);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->configure_button),
				  (plugin != NULL) && 
				   brasero_plugin_get_next_conf_option (plugin, NULL));
}

static void
row_activated_cb (GtkTreeView       *tree_view,
		  GtkTreePath       *path,
		  GtkTreeViewColumn *column,
		  gpointer           data)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	BraseroPluginManagerUI *pm = data;
	BraseroPluginManagerUIPrivate *priv;

	priv = BRASERO_PLUGIN_MANAGER_UI_GET_PRIVATE (pm);
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree));

	g_return_if_fail (model != NULL);

	gtk_tree_model_get_iter (model, &iter, path);

	g_return_if_fail (&iter != NULL);

	plugin_manager_ui_toggle_active (&iter, model);
}

static gint 
list_priority_sort_func (gconstpointer a, gconstpointer b)
{
	BraseroPlugin *plugin1 = BRASERO_PLUGIN (a), *plugin2 = BRASERO_PLUGIN (b);

	return brasero_plugin_get_priority (plugin2) - brasero_plugin_get_priority (plugin1);
}

static void
brasero_plugin_manager_ui_save_order (BraseroPluginManagerUI *pm)
{
	BraseroPluginManagerUIPrivate *priv;
	GConfClient *client;
	GtkTreeModel *model;
	GtkTreeIter row;
	gint rank;

	priv = BRASERO_PLUGIN_MANAGER_UI_GET_PRIVATE (pm);

	if (priv->category == BRASERO_PLUGIN_ERROR
	||  priv->category == BRASERO_PLUGIN_MISCELLANEOUS)
		return;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree));

	/* The safest way to handle this is to enforce priorities for all plugins */
	if (!gtk_tree_model_get_iter_first (model, &row))
		return;

	rank = gtk_tree_model_iter_n_children (model, NULL);
	client = gconf_client_get_default ();
	do {
		BraseroPlugin *plugin;
		gchar *key;

		gtk_tree_model_get (model, &row,
				    PLUGIN_COLUMN, &plugin,
				    -1);

		if (!plugin) {
			g_warning ("No plugin in row");
			continue;
		}

		key = brasero_plugin_get_gconf_priority_key (plugin);
		gconf_client_set_int (client,
				      key,
				      rank,
				      NULL);
		g_free (key);

		rank --;
	} while (gtk_tree_model_iter_next (model, &row));
	
	g_object_unref (client);
}

static void
brasero_plugin_manager_ui_order_changed_cb (GtkTreeModel *model,
					    GtkTreePath *path,
					    BraseroPluginManagerUI *pm)
{
	brasero_plugin_manager_ui_save_order (pm);
}

static void
brasero_plugin_manager_ui_update_up_down (BraseroPluginManagerUI *pm)
{
	BraseroPluginManagerUIPrivate *priv;
	GtkTreeSelection *selection;
	GtkTreePath *treepath;
	GtkTreeModel *model;
	GtkTreeIter iter;

	priv = BRASERO_PLUGIN_MANAGER_UI_GET_PRIVATE (pm);

	if (priv->category == BRASERO_PLUGIN_ERROR
	||  priv->category == BRASERO_PLUGIN_MISCELLANEOUS) {
		gtk_widget_set_sensitive (priv->up_button, FALSE);
		gtk_widget_set_sensitive (priv->down_button, FALSE);
		return;
	}

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_widget_set_sensitive (priv->up_button, FALSE);
		gtk_widget_set_sensitive (priv->down_button, FALSE);
		return;
	}

	treepath = gtk_tree_model_get_path (model, &iter);
	gtk_tree_path_next (treepath);
	if (gtk_tree_model_get_iter (model, &iter, treepath))
		gtk_widget_set_sensitive (priv->down_button, TRUE);
	else
		gtk_widget_set_sensitive (priv->down_button, FALSE);

	gtk_tree_path_free (treepath);
	if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
		return;

	treepath = gtk_tree_model_get_path (model, &iter);
	if (!gtk_tree_path_prev (treepath)) {
		gtk_widget_set_sensitive (priv->up_button, FALSE);
		gtk_tree_path_free (treepath);
		return;
	}

	if (gtk_tree_model_get_iter (model, &iter, treepath))
		gtk_widget_set_sensitive (priv->up_button, TRUE);
	else
		gtk_widget_set_sensitive (priv->up_button, FALSE);

	gtk_tree_path_free (treepath);
}

static void
up_button_cb (GtkWidget *button,
	      BraseroPluginManagerUI *pm)
{
	BraseroPluginManagerUIPrivate *priv;
	GtkTreeSelection *selection;
	GtkTreePath *treepath;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeIter prev;

	priv = BRASERO_PLUGIN_MANAGER_UI_GET_PRIVATE (pm);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	treepath = gtk_tree_model_get_path (model, &iter);
	if (!gtk_tree_path_prev (treepath)) {
		gtk_tree_path_free (treepath);
		return;
	}

	if (!gtk_tree_model_get_iter (model, &prev, treepath)) {
		gtk_tree_path_free (treepath);
		return;
	}

	gtk_tree_path_free (treepath);
	gtk_list_store_move_before (GTK_LIST_STORE (model), &iter, &prev);
	brasero_plugin_manager_ui_save_order (pm);

	brasero_plugin_manager_ui_update_up_down (pm);
}

static void
down_button_cb (GtkWidget *button,
		BraseroPluginManagerUI *pm)
{
	BraseroPluginManagerUIPrivate *priv;
	GtkTreeSelection *selection;
	GtkTreePath *treepath;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeIter next;

	priv = BRASERO_PLUGIN_MANAGER_UI_GET_PRIVATE (pm);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	treepath = gtk_tree_model_get_path (model, &iter);
	gtk_tree_path_next (treepath);
	if (!gtk_tree_model_get_iter (model, &next, treepath)) {
		gtk_tree_path_free (treepath);
		return;
	}

	gtk_tree_path_free (treepath);
	gtk_list_store_move_after (GTK_LIST_STORE (model), &iter, &next);
	brasero_plugin_manager_ui_save_order (pm);

	brasero_plugin_manager_ui_update_up_down (pm);
}

static void
plugin_manager_ui_populate_lists (BraseroPluginManagerUI *pm)
{
	BraseroPluginManagerUIPrivate *priv;
	BraseroPlugin *plugin;
	BraseroBurnCaps *caps;
	const GSList *plugins;
	GtkListStore *model;
	GtkTreeIter iter;

	priv = BRASERO_PLUGIN_MANAGER_UI_GET_PRIVATE (pm);
	plugins = priv->plugins;

	caps = brasero_burn_caps_get_default ();

	model = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree)));
	if (priv->order_changed_id)
		g_signal_handler_disconnect (model, priv->order_changed_id);

	/* re-sort our private list of plugins in case the user have changed it
	 * through this dialog. We can't do it in order_changed function since
	 * the plugins can't have their priority updated through gconf
	 * notification (it would need us to allow the main loop to run) */
	priv->plugins = g_slist_sort (priv->plugins, list_priority_sort_func);

	for (; plugins; plugins = plugins->next) {
		plugin = plugins->data;

		switch (priv->category) {
		case BRASERO_PLUGIN_ERROR:
			if (brasero_plugin_get_gtype (plugin) != G_TYPE_NONE)
				continue;
			break;

		case BRASERO_PLUGIN_BURN_ENGINE:
			if (brasero_burn_caps_plugin_can_burn (caps, plugin) != BRASERO_BURN_OK)
				continue;
			break;

		case BRASERO_PLUGIN_IMAGE_ENGINE:
			if (brasero_burn_caps_plugin_can_image (caps, plugin) != BRASERO_BURN_OK)
				continue;
			break;

		case BRASERO_PLUGIN_CONVERT_ENGINE:
			if (brasero_burn_caps_plugin_can_convert (caps, plugin) != BRASERO_BURN_OK)
				continue;
			break;

		case BRASERO_PLUGIN_MISCELLANEOUS:
			if (brasero_burn_caps_plugin_can_burn (caps, plugin) == BRASERO_BURN_OK
			||  brasero_burn_caps_plugin_can_convert (caps, plugin) == BRASERO_BURN_OK
			||  brasero_burn_caps_plugin_can_image (caps, plugin) == BRASERO_BURN_OK
			||  brasero_plugin_get_gtype (plugin) == G_TYPE_NONE)
				continue;
			break;
		}

		gtk_list_store_append (model, &iter);
		gtk_list_store_set (model, &iter,
				    ACTIVE_COLUMN, brasero_plugin_get_active (plugin),
				    AVAILABLE_COLUMN, brasero_plugin_get_gtype (plugin) != G_TYPE_NONE,
				    PLUGIN_COLUMN, plugin,
				    -1);
	}
	g_object_unref (caps);

	if (priv->category == BRASERO_PLUGIN_ERROR
	||  priv->category == BRASERO_PLUGIN_MISCELLANEOUS) {
		gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (priv->tree),
							0,
							NULL,
							0,
							0);
		gtk_tree_view_enable_model_drag_dest (GTK_TREE_VIEW (priv->tree),
						      NULL,
						      0,
						      0);
	}
	else {
		gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (priv->tree),
							GDK_BUTTON1_MASK,
							ntables_source,
							nb_targets_source,
							GDK_ACTION_MOVE);
		gtk_tree_view_enable_model_drag_dest (GTK_TREE_VIEW (priv->tree),
						      ntables_source,
						      nb_targets_source,
						      GDK_ACTION_MOVE);
	}

	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (model), &iter))
	{
		GtkTreeSelection *selection;

		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));
		g_return_if_fail (selection != NULL);
		
		gtk_tree_selection_select_iter (selection, &iter);

		gtk_tree_model_get (GTK_TREE_MODEL (model), &iter,
				    PLUGIN_COLUMN, &plugin, -1);

		gtk_widget_set_sensitive (GTK_WIDGET (priv->configure_button),
					  (brasero_plugin_get_next_conf_option (plugin, NULL) != NULL));
	}

	/* update buttons */
	plugin = plugin_manager_ui_get_selected_plugin (pm);

	gtk_widget_set_sensitive (GTK_WIDGET (priv->about_button),
				  plugin != NULL);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->configure_button),
				  (plugin != NULL) && 
				   brasero_plugin_get_next_conf_option (plugin, NULL));

	brasero_plugin_manager_ui_update_up_down (pm);

	priv->order_changed_id = g_signal_connect (model,
						   "row-deleted",
						   G_CALLBACK (brasero_plugin_manager_ui_order_changed_cb),
						   pm);
}

static void
brasero_plugin_manager_ui_combo_changed_cb (GtkComboBox *box,
					    BraseroPluginManagerUI *pm)
{
	BraseroPluginManagerUIPrivate *priv;
	GtkTreeModel *model;

	priv = BRASERO_PLUGIN_MANAGER_UI_GET_PRIVATE (pm);

	/* clear the tree and re-populate it */
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree));

	/* disconnect before clearing */
	if (priv->order_changed_id) {
		g_signal_handler_disconnect (model, priv->order_changed_id);
		priv->order_changed_id = 0;
	}

	gtk_list_store_clear (GTK_LIST_STORE (model));

	priv->category = gtk_combo_box_get_active (box);
	plugin_manager_ui_populate_lists (pm);

	switch (priv->category) {
		case BRASERO_PLUGIN_ERROR:
			gtk_label_set_markup (GTK_LABEL (priv->description),
					     _("<i>The following plugins had errors on loading.</i>"));
			gtk_label_set_text (GTK_LABEL (priv->label), "\n\n");

			break;

		case BRASERO_PLUGIN_BURN_ENGINE:
			gtk_label_set_markup (GTK_LABEL (priv->description),
					     _("<i>Includes plugins that can burn, blank or format discs (CDs and DVDs).</i>"));
			gtk_label_set_text (GTK_LABEL (priv->label),
					    _("The list is sorted according to the use order. Move them up and down if you want to set another order of priority."));

			break;

		case BRASERO_PLUGIN_IMAGE_ENGINE:
			gtk_label_set_markup (GTK_LABEL (priv->description),
					     _("<i>Includes plugins that can create images suitable to be burnt on discs.</i>"));
			gtk_label_set_text (GTK_LABEL (priv->label),
					    _("The list is sorted according to the use order. Move them up and down if you want to set another order of priority."));

			break;

		case BRASERO_PLUGIN_CONVERT_ENGINE:
			gtk_label_set_markup (GTK_LABEL (priv->description),
					     _("<i>Includes plugins that can convert image formats into other formats.</i>"));
			gtk_label_set_text (GTK_LABEL (priv->label),
					    _("The list is sorted according to the use order. Move them up and down if you want to set another order of priority."));

			break;

		case BRASERO_PLUGIN_MISCELLANEOUS:
			gtk_label_set_markup (GTK_LABEL (priv->description),
					     _("<i>Includes plugins that provide additional functionalities.</i>"));
			gtk_label_set_text (GTK_LABEL (priv->label), "\n\n");
			break;
		default:
			break;
	}
}

static gboolean
plugin_manager_ui_set_active (GtkTreeIter  *iter,
			      GtkTreeModel *model,
			      gboolean      active)
{
	BraseroPlugin *plugin;
	GtkTreeIter category;
	gboolean res = TRUE;

	gtk_tree_model_get (model, iter, PLUGIN_COLUMN, &plugin, -1);

	g_return_val_if_fail (plugin != NULL, FALSE);

	brasero_plugin_set_active (plugin, active);
 
	/* set new value */
	gtk_list_store_set (GTK_LIST_STORE (model), 
			    iter, 
			    ACTIVE_COLUMN, brasero_plugin_get_active (plugin),
			    AVAILABLE_COLUMN, brasero_plugin_get_gtype (plugin) != G_TYPE_NONE,
			    -1);

	/* search if this plugin appears under other categories
	 * and deactivate it as well */
	if (!gtk_tree_model_get_iter_first (model, &category))
		return res;

	do {
		GtkTreeIter child;

		if (!gtk_tree_model_iter_children (model, &child, &category))
			continue;

		do {
			BraseroPlugin *tmp_plugin;

			tmp_plugin = NULL;
			gtk_tree_model_get (model, &child,
					    PLUGIN_COLUMN, &tmp_plugin,
					    -1);

			if (plugin == tmp_plugin) {
				gtk_list_store_set (GTK_LIST_STORE (model), &child, 
						    ACTIVE_COLUMN, active,
						    AVAILABLE_COLUMN, brasero_plugin_get_gtype (plugin) != G_TYPE_NONE,
						    -1);
				break;
			}

		} while (gtk_tree_model_iter_next (model, &child));

	} while (gtk_tree_model_iter_next (model, &category));

	return res;
}

static void
plugin_manager_ui_toggle_active (GtkTreeIter  *iter,
				 GtkTreeModel *model)
{
	gboolean active;

	gtk_tree_model_get (model, iter, ACTIVE_COLUMN, &active, -1);

	active ^= 1;

	plugin_manager_ui_set_active (iter, model, active);
}

static void
plugin_manager_ui_set_active_all (BraseroPluginManagerUI *pm,
				  gboolean            active)
{
	BraseroPluginManagerUIPrivate *priv;
	GtkTreeModel *model;
	GtkTreeIter iter;

	priv = BRASERO_PLUGIN_MANAGER_UI_GET_PRIVATE (pm);
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree));

	g_return_if_fail (model != NULL);

	gtk_tree_model_get_iter_first (model, &iter);

	do {
		plugin_manager_ui_set_active (&iter, model, active);		
	}
	while (gtk_tree_model_iter_next (model, &iter));
}

/* Callback used as the interactive search comparison function */
static gboolean
name_search_cb (GtkTreeModel *model,
		gint          column,
		const gchar  *key,
		GtkTreeIter  *iter,
		gpointer      data)
{
	BraseroPlugin *plugin;
	gchar *normalized_string;
	gchar *normalized_key;
	gchar *case_normalized_string;
	gchar *case_normalized_key;
	gint key_len;
	gboolean retval;

	gtk_tree_model_get (model, iter, PLUGIN_COLUMN, &plugin, -1);
	if (!plugin)
		return FALSE;

	normalized_string = g_utf8_normalize (brasero_plugin_get_name (plugin), -1, G_NORMALIZE_ALL);
	normalized_key = g_utf8_normalize (key, -1, G_NORMALIZE_ALL);
	case_normalized_string = g_utf8_casefold (normalized_string, -1);
	case_normalized_key = g_utf8_casefold (normalized_key, -1);

	key_len = strlen (case_normalized_key);

	/* Oddly enough, this callback must return whether to stop the search
	 * because we found a match, not whether we actually matched.
	 */
	retval = (strncmp (case_normalized_key, case_normalized_string, key_len) != 0);

	g_free (normalized_key);
	g_free (normalized_string);
	g_free (case_normalized_key);
	g_free (case_normalized_string);

	return retval;
}

static void
enable_plugin_menu_cb (GtkMenu            *menu,
		       BraseroPluginManagerUI *pm)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	BraseroPluginManagerUIPrivate *priv;

	priv = BRASERO_PLUGIN_MANAGER_UI_GET_PRIVATE (pm);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree));
	g_return_if_fail (model != NULL);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));
	g_return_if_fail (selection != NULL);

	if (gtk_tree_selection_get_selected (selection, NULL, &iter))
		plugin_manager_ui_toggle_active (&iter, model);
}

static void
enable_all_menu_cb (GtkMenu            *menu,
		    BraseroPluginManagerUI *pm)
{
	plugin_manager_ui_set_active_all (pm, TRUE);
}

static void
disable_all_menu_cb (GtkMenu            *menu,
		     BraseroPluginManagerUI *pm)
{
	plugin_manager_ui_set_active_all (pm, FALSE);
}

static GtkWidget *
create_tree_popup_menu (BraseroPluginManagerUI *pm)
{
	GtkWidget *menu;
	GtkWidget *item;
	GtkWidget *image;
	BraseroPlugin *plugin;

	plugin = plugin_manager_ui_get_selected_plugin (pm);

	menu = gtk_menu_new ();

	item = gtk_image_menu_item_new_with_mnemonic (_("_About"));
	image = gtk_image_new_from_stock (GTK_STOCK_ABOUT,
					  GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (item, "activate",
			  G_CALLBACK (about_button_cb), pm);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	item = gtk_image_menu_item_new_with_mnemonic (_("C_onfigure"));
	image = gtk_image_new_from_stock (GTK_STOCK_PREFERENCES,
					  GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (item, "activate",
			  G_CALLBACK (configure_button_cb), pm);
	gtk_widget_set_sensitive (item,
				  (brasero_plugin_get_next_conf_option (plugin, NULL) != NULL));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	item = gtk_check_menu_item_new_with_mnemonic (_("A_ctivate"));
	gtk_widget_set_sensitive (item,
				  brasero_plugin_get_gtype (plugin) != G_TYPE_NONE);	
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item),
					brasero_plugin_get_active (plugin));
	g_signal_connect (item, "toggled",
			  G_CALLBACK (enable_plugin_menu_cb), pm);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);					

	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	item = gtk_menu_item_new_with_mnemonic (_("Ac_tivate All"));
	g_signal_connect (item, "activate",
			  G_CALLBACK (enable_all_menu_cb), pm);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	item = gtk_menu_item_new_with_mnemonic (_("_Deactivate All"));
	g_signal_connect (item, "activate",
			  G_CALLBACK (disable_all_menu_cb), pm);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	
	gtk_widget_show_all (menu);
	
	return menu;
}

static void
tree_popup_menu_detach (BraseroPluginManagerUI *pm,
			GtkMenu            *menu)
{
	BraseroPluginManagerUIPrivate *priv;

	priv = BRASERO_PLUGIN_MANAGER_UI_GET_PRIVATE (pm);
	priv->popup_menu = NULL;
}

static void
menu_position_under_widget (GtkMenu  *menu,
			    gint     *x,
			    gint     *y,
			    gboolean *push_in,
			    gpointer  user_data)
{
	GtkWidget *w = GTK_WIDGET (user_data);
	GtkRequisition requisition;

	gdk_window_get_origin (w->window, x, y);
	gtk_widget_size_request (GTK_WIDGET (menu), &requisition);

	if (gtk_widget_get_direction (w) == GTK_TEXT_DIR_RTL) {
		*x += w->allocation.x + w->allocation.width - requisition.width;
	} else {
		*x += w->allocation.x;
	}

	*y += w->allocation.y + w->allocation.height;

	*push_in = TRUE;
}

static void
menu_position_under_tree_view (GtkMenu  *menu,
			       gint     *x,
			       gint     *y,
			       gboolean *push_in,
			       gpointer  user_data)
{
	GtkTreeView *tree = GTK_TREE_VIEW (user_data);
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;

	model = gtk_tree_view_get_model (tree);

	g_return_if_fail (model != NULL);

	selection = gtk_tree_view_get_selection (tree);

	g_return_if_fail (selection != NULL);

	if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		GtkTreePath *path;
		GdkRectangle rect;

		gdk_window_get_origin (GTK_WIDGET (tree)->window, x, y);
			
		path = gtk_tree_model_get_path (model, &iter);

		gtk_tree_view_get_cell_area (tree, 
					     path,
					     gtk_tree_view_get_column (tree, 0), /* FIXME 0 for RTL ? */
					     &rect);
		gtk_tree_path_free (path);
		
		*x += rect.x;
		*y += rect.y + rect.height;
		
		if (gtk_widget_get_direction (GTK_WIDGET (tree)) == GTK_TEXT_DIR_RTL) {
			GtkRequisition requisition;

			gtk_widget_size_request (GTK_WIDGET (menu), &requisition);

			*x += rect.width - requisition.width;
		}
	} else {
		/* No selection -> regular "under widget" positioning */
		menu_position_under_widget (menu,
					    x, y, push_in,
					    tree);
	}
}

static void
show_tree_popup_menu (GtkTreeView        *tree,
		      BraseroPluginManagerUI *pm,
		      GdkEventButton     *event)
{
	BraseroPluginManagerUIPrivate *priv;

	priv = BRASERO_PLUGIN_MANAGER_UI_GET_PRIVATE (pm);

	if (priv->popup_menu)
		gtk_widget_destroy (priv->popup_menu);

	priv->popup_menu = create_tree_popup_menu (pm);
	
	gtk_menu_attach_to_widget (GTK_MENU (priv->popup_menu),
				   GTK_WIDGET (pm),
				   (GtkMenuDetachFunc) tree_popup_menu_detach);

	if (event != NULL)
	{
		gtk_menu_popup (GTK_MENU (priv->popup_menu), NULL, NULL,
				NULL, NULL,
				event->button, event->time);
	}
	else
	{
		gtk_menu_popup (GTK_MENU (priv->popup_menu), NULL, NULL,
				menu_position_under_tree_view, tree,
				0, gtk_get_current_event_time ());

		gtk_menu_shell_select_first (GTK_MENU_SHELL (priv->popup_menu),
					     FALSE);
	}
}

static gboolean
button_press_event_cb (GtkWidget          *tree,
		       GdkEventButton     *event,
		       BraseroPluginManagerUI *pm)
{
	/* We want the treeview selection to be updated before showing the menu.
	 * This code is evil, thanks to Federico Mena Quintero's black magic.
	 * See: http://mail.gnome.org/archives/gtk-devel-list/2006-February/msg00168.html
	 * FIXME: Let's remove it asap.
	 */

	static gboolean in_press = FALSE;
	gboolean handled;

	if (in_press)
		return FALSE; /* we re-entered */

	if (GDK_BUTTON_PRESS != event->type || 3 != event->button)
		return FALSE; /* let the normal handler run */

	in_press = TRUE;
	handled = gtk_widget_event (tree, (GdkEvent *) event);
	in_press = FALSE;

	if (!handled)
		return FALSE;
		
	/* The selection is fully updated by now */
	show_tree_popup_menu (GTK_TREE_VIEW (tree), pm, event);
	return TRUE;
}

static gboolean
popup_menu_cb (GtkTreeView        *tree,
	       BraseroPluginManagerUI *pm)
{
	show_tree_popup_menu (tree, pm, NULL);
	return TRUE;
}

static void
tree_selection_changed_cb (GtkTreeSelection *selection,
			   BraseroPluginManagerUI *pm)
{
	brasero_plugin_manager_ui_update_up_down (pm);
}

static void
plugin_manager_ui_construct_tree (BraseroPluginManagerUI *pm)
{
	BraseroPluginManagerUIPrivate *priv;
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell;
	GtkListStore *model;

	priv = BRASERO_PLUGIN_MANAGER_UI_GET_PRIVATE (pm);

	model = gtk_list_store_new (N_COLUMNS,
				    G_TYPE_BOOLEAN,
				    G_TYPE_BOOLEAN,
				    G_TYPE_POINTER);

	gtk_tree_view_set_model (GTK_TREE_VIEW (priv->tree),
				 GTK_TREE_MODEL (model));
	g_object_unref (model);

	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (priv->tree), TRUE);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (priv->tree), TRUE);

	/* First column */
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, PLUGIN_MANAGER_UI_NAME_TITLE);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_expand (column, TRUE);

	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (column, cell,
						 plugin_manager_ui_view_rank_cell_cb,
						 pm, NULL);

	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, cell, FALSE);
	g_object_set (cell, "stock-size", GTK_ICON_SIZE_SMALL_TOOLBAR, NULL);
	gtk_tree_view_column_set_cell_data_func (column, cell,
						 plugin_manager_ui_view_icon_cell_cb,
						 pm, NULL);
	
	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, cell, TRUE);
	g_object_set (cell, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_tree_view_column_set_cell_data_func (column, cell,
						 plugin_manager_ui_view_info_cell_cb,
						 pm, NULL);

	gtk_tree_view_column_set_spacing (column, 6);
	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->tree), column);

	/* Last column */
	cell = gtk_cell_renderer_toggle_new ();
	g_object_set (cell, "xpad", 6, NULL);
	g_signal_connect (cell,
			  "toggled",
			  G_CALLBACK (active_toggled_cb),
			  pm);
	column = gtk_tree_view_column_new_with_attributes (PLUGIN_MANAGER_UI_ACTIVE_TITLE,
							   cell,
							   "active", ACTIVE_COLUMN,
							   "activatable", AVAILABLE_COLUMN,
							   "sensitive", AVAILABLE_COLUMN,
							   NULL);

	gtk_tree_view_column_set_title (column, PLUGIN_MANAGER_UI_ACTIVE_TITLE);
	gtk_tree_view_column_set_expand (column, FALSE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->tree), column);

	/* Enable search for our non-string column */
	gtk_tree_view_set_search_column (GTK_TREE_VIEW (priv->tree),
					 PLUGIN_COLUMN);
	gtk_tree_view_set_search_equal_func (GTK_TREE_VIEW (priv->tree),
					     name_search_cb,
					     NULL,
					     NULL);

	g_signal_connect (priv->tree,
			  "cursor_changed",
			  G_CALLBACK (cursor_changed_cb),
			  pm);
	g_signal_connect (priv->tree,
			  "row_activated",
			  G_CALLBACK (row_activated_cb),
			  pm);

	g_signal_connect (priv->tree,
			  "button-press-event",
			  G_CALLBACK (button_press_event_cb),
			  pm);
	g_signal_connect (priv->tree,
			  "popup-menu",
			  G_CALLBACK (popup_menu_cb),
			  pm);
	gtk_widget_show (priv->tree);
}

static void 
brasero_plugin_manager_ui_init (BraseroPluginManagerUI *pm)
{
	gchar *markup;
	GtkWidget *table;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *combo;
	GtkWidget *alignment;
	GtkWidget *viewport;
	GtkWidget *vbuttonbox;
	BraseroPluginManager *manager;
	BraseroPluginManagerUIPrivate *priv;

	priv = BRASERO_PLUGIN_MANAGER_UI_GET_PRIVATE (pm);

	gtk_box_set_spacing (GTK_BOX (pm), 6);
	gtk_container_set_border_width (GTK_CONTAINER (pm), 12);

	label = gtk_label_new (NULL);
	markup = g_markup_printf_escaped ("<span weight=\"bold\">%s</span>",
					  _("Plugins"));
	gtk_label_set_markup (GTK_LABEL (label), markup);
	g_free (markup);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	
	gtk_box_pack_start (GTK_BOX (pm), label, FALSE, TRUE, 0);

	alignment = gtk_alignment_new (0., 0., 1., 1.);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 12, 0);
	gtk_box_pack_start (GTK_BOX (pm), alignment, TRUE, TRUE, 0);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (alignment), vbox);

	/* Combo to choose categories */
	table = gtk_table_new (2, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 3);
	gtk_table_set_col_spacings (GTK_TABLE (table), 6);
	gtk_widget_show (table);
	gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 6);

	label = gtk_label_new (_("Category:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, 0, 0, 0, 0);

	combo = gtk_combo_box_new_text ();
	gtk_combo_box_insert_text (GTK_COMBO_BOX (combo),
				   BRASERO_PLUGIN_BURN_ENGINE,
				   _("Burn engines"));
	gtk_combo_box_insert_text (GTK_COMBO_BOX (combo),
				   BRASERO_PLUGIN_IMAGE_ENGINE,
				   _("Imaging engines"));
	gtk_combo_box_insert_text (GTK_COMBO_BOX (combo),
				   BRASERO_PLUGIN_CONVERT_ENGINE,
				   _("Image type conversion engines"));
	gtk_combo_box_insert_text (GTK_COMBO_BOX (combo),
				   BRASERO_PLUGIN_MISCELLANEOUS,
				   _("Miscellaneous engines"));
	gtk_combo_box_insert_text (GTK_COMBO_BOX (combo),
				   BRASERO_PLUGIN_ERROR,
				   _("Engines with errors on loading"));
	gtk_widget_show (combo);
	g_signal_connect (combo,
			  "changed",
			  G_CALLBACK (brasero_plugin_manager_ui_combo_changed_cb),
			  pm);
	gtk_table_attach_defaults (GTK_TABLE (table), combo, 1, 2, 0, 1);

	priv->description = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (priv->description), 0.0, 0.5);
	gtk_widget_show (priv->description);
	gtk_table_attach_defaults (GTK_TABLE (table), priv->description, 1, 2, 1, 2);

	priv->label = gtk_label_new (NULL);
	gtk_label_set_line_wrap_mode (GTK_LABEL (priv->label), PANGO_WRAP_WORD);
	gtk_label_set_line_wrap (GTK_LABEL (priv->label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (priv->label), 0.0, 0.5);
	gtk_widget_show (priv->label);
	gtk_box_pack_start (GTK_BOX (vbox), priv->label, FALSE, TRUE, 0);

	/* bottom part: tree, buttons */
	hbox = gtk_hbox_new (FALSE, 12);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 6);

	viewport = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (viewport),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (viewport), 
					     GTK_SHADOW_IN);

	gtk_box_pack_start (GTK_BOX (hbox), viewport, TRUE, TRUE, 0);

	priv->tree = gtk_tree_view_new ();
	gtk_container_add (GTK_CONTAINER (viewport), priv->tree);

	g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree)),
			  "changed",
			  G_CALLBACK (tree_selection_changed_cb),
			  pm);

	vbuttonbox = gtk_vbutton_box_new ();
	gtk_box_pack_start (GTK_BOX (hbox), vbuttonbox, FALSE, FALSE, 0);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (vbuttonbox), GTK_BUTTONBOX_START);
	gtk_box_set_spacing (GTK_BOX (vbuttonbox), 8);


	priv->up_button = brasero_utils_make_button (_("Move _Up"),
						     NULL,
						     NULL,
						     GTK_ICON_SIZE_BUTTON);
	gtk_container_add (GTK_CONTAINER (vbuttonbox), priv->up_button);

	priv->down_button = brasero_utils_make_button (_("Move _Down"),
						       NULL,
						       NULL,
						       GTK_ICON_SIZE_BUTTON);
	gtk_container_add (GTK_CONTAINER (vbuttonbox), priv->down_button);

	priv->about_button = brasero_utils_make_button (_("_About"),
							NULL,
							GTK_STOCK_ABOUT,
							GTK_ICON_SIZE_BUTTON);
	gtk_container_add (GTK_CONTAINER (vbuttonbox), priv->about_button);

	priv->configure_button = brasero_utils_make_button (_("C_onfigure"),
							    NULL,
							    GTK_STOCK_PREFERENCES,
							    GTK_ICON_SIZE_BUTTON);
	gtk_container_add (GTK_CONTAINER (vbuttonbox), priv->configure_button);

	/* setup a window of a sane size. */
	gtk_widget_set_size_request (GTK_WIDGET (viewport), 300, 200);

	g_signal_connect (priv->up_button,
			  "clicked",
			  G_CALLBACK (up_button_cb),
			  pm);
	g_signal_connect (priv->down_button,
			  "clicked",
			  G_CALLBACK (down_button_cb),
			  pm);
	g_signal_connect (priv->about_button,
			  "clicked",
			  G_CALLBACK (about_button_cb),
			  pm);
	g_signal_connect (priv->configure_button,
			  "clicked",
			  G_CALLBACK (configure_button_cb),
			  pm);

	plugin_manager_ui_construct_tree (pm);

	/* get the list of available plugins (or installed) */
	manager = brasero_plugin_manager_get_default ();
	priv->plugins = brasero_plugin_manager_get_plugins_list (manager);

	if (!priv->plugins){
		gtk_widget_set_sensitive (priv->about_button, FALSE);
		gtk_widget_set_sensitive (priv->configure_button, FALSE);		
	}
	else
		priv->plugins = g_slist_sort (priv->plugins, list_priority_sort_func);

	/* sets a default */
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), BRASERO_PLUGIN_BURN_ENGINE);
}

static void
brasero_plugin_manager_ui_finalize (GObject *object)
{
	BraseroPluginManagerUI *pm = BRASERO_PLUGIN_MANAGER_UI (object);
	BraseroPluginManagerUIPrivate *priv;

	priv = BRASERO_PLUGIN_MANAGER_UI_GET_PRIVATE (pm);

	if (priv->plugins) {
		g_slist_free (priv->plugins);
		priv->plugins = NULL;
	}

	if (priv->popup_menu)
		gtk_widget_destroy (priv->popup_menu);

	G_OBJECT_CLASS (brasero_plugin_manager_ui_parent_class)->finalize (object);

}

GtkWidget *brasero_plugin_manager_ui_new (void)
{
	return g_object_new (BRASERO_TYPE_PLUGIN_MANAGER_UI,0);
}
