/*
 * brasero-plugin-manager.c
 * This file is part of brasero
 *
 * Copyright (C) 2007 Philippe Rouquier
 *
 * Based on gedit code (gedit/gedit-plugin-manager.c) by: 
 * 	- Paolo Maggi <paolo@gnome.org>
 *
 * Libbrasero-media is free software; you can redistribute it and/or modify
fy
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Brasero is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <glib/gi18n.h>

#include "brasero-misc.h"

#include "brasero-plugin-manager-ui.h"

#include "brasero-plugin.h"
#include "brasero-plugin-private.h"
#include "brasero-plugin-information.h"
#include "brasero-burn-lib.h"
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
	GtkWidget	*tree;

	GtkWidget	*about_button;
	GtkWidget	*configure_button;

	GtkWidget 	*about;
	
	GtkWidget	*popup_menu;

	GSList		*plugins;
};

G_DEFINE_TYPE (BraseroPluginManagerUI, brasero_plugin_manager_ui, GTK_TYPE_BOX)

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
			       "program-name", _(brasero_plugin_get_display_name (plugin)),
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
	BraseroPlugin *plugin;
	GtkWidget *dialog;

	plugin = plugin_manager_ui_get_selected_plugin (pm);

	g_return_if_fail (plugin != NULL);

	dialog = brasero_plugin_option_new ();

	brasero_plugin_option_set_plugin (BRASERO_PLUGIN_OPTION (dialog), plugin);
	gtk_window_set_transient_for (GTK_WINDOW (dialog),
				      GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (pm))));

	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static void
plugin_manager_ui_view_info_cell_cb (GtkTreeViewColumn *tree_column,
				     GtkCellRenderer   *cell,
				     GtkTreeModel      *tree_model,
				     GtkTreeIter       *iter,
				     gpointer           data)
{
	BraseroPlugin *plugin;
	gchar *error_string;
	gchar *text;
	
	g_return_if_fail (tree_model != NULL);
	g_return_if_fail (tree_column != NULL);

	gtk_tree_model_get (tree_model, iter,
			    PLUGIN_COLUMN, &plugin,
			    -1);

	if (!plugin)
		return;

	error_string = brasero_plugin_get_error_string (plugin);
	if (error_string) {
		text = g_markup_printf_escaped ("<b>%s</b>\n%s\n<i>%s</i>",
						/* Use the translated name of 
						 * the plugin. */
						_(brasero_plugin_get_display_name (plugin)),
						brasero_plugin_get_description (plugin),
						error_string);
		g_free (error_string);
	}
	else
		text = g_markup_printf_escaped ("<b>%s</b>\n%s",
						/* Use the translated name of 
						 * the plugin. */
						_(brasero_plugin_get_display_name (plugin)),
						brasero_plugin_get_description (plugin));

	g_object_set (G_OBJECT (cell),
		      "markup", text,
		      "sensitive", brasero_plugin_get_gtype (plugin) != G_TYPE_NONE && !brasero_plugin_get_compulsory (plugin),
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
		      brasero_plugin_get_gtype (plugin) != G_TYPE_NONE && !brasero_plugin_get_compulsory (plugin),
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

	if (gtk_tree_model_get_iter (model, &iter, path))
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
	g_return_if_fail (gtk_tree_model_get_iter (model, &iter, path));

	plugin_manager_ui_toggle_active (&iter, model);
}

static void
plugin_manager_ui_populate_lists (BraseroPluginManagerUI *pm)
{
	BraseroPluginManagerUIPrivate *priv;
	BraseroPlugin *plugin;
	const GSList *plugins;
	GtkListStore *model;
	GtkTreeIter iter;

	priv = BRASERO_PLUGIN_MANAGER_UI_GET_PRIVATE (pm);
	plugins = priv->plugins;

	model = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree)));

	for (; plugins; plugins = plugins->next) {
		plugin = plugins->data;

		gtk_list_store_append (model, &iter);
		gtk_list_store_set (model, &iter,
				    ACTIVE_COLUMN, brasero_plugin_get_active (plugin, 0),
				    AVAILABLE_COLUMN, brasero_plugin_get_gtype (plugin) != G_TYPE_NONE && !brasero_plugin_get_compulsory (plugin),
				    PLUGIN_COLUMN, plugin,
				    -1);
	}

	plugin = NULL;
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
	gtk_widget_set_sensitive (GTK_WIDGET (priv->about_button),
				  plugin != NULL);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->configure_button),
				  (plugin != NULL) && 
				   brasero_plugin_get_next_conf_option (plugin, NULL));
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

	if (brasero_plugin_get_gtype (plugin) == G_TYPE_NONE
	||  brasero_plugin_get_compulsory (plugin))
		return FALSE;

	brasero_plugin_set_active (plugin, active);
 
	/* set new value */
	gtk_list_store_set (GTK_LIST_STORE (model), 
			    iter, 
			    ACTIVE_COLUMN, brasero_plugin_get_active (plugin, 0),
			    AVAILABLE_COLUMN, brasero_plugin_get_gtype (plugin) != G_TYPE_NONE && !brasero_plugin_get_compulsory (plugin),
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
						    AVAILABLE_COLUMN, brasero_plugin_get_gtype (plugin) != G_TYPE_NONE && !brasero_plugin_get_compulsory (plugin),
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

	if (!plugin_manager_ui_set_active (iter, model, active))
		return;
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

	/* Use translated name for the plugin */
	normalized_string = g_utf8_normalize (_(brasero_plugin_get_display_name (plugin)),
					      -1,
					      G_NORMALIZE_ALL);
	normalized_key = g_utf8_normalize (key,
					   -1,
					   G_NORMALIZE_ALL);
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

	if (!plugin)
		return NULL;

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
				  brasero_plugin_get_gtype (plugin) != G_TYPE_NONE && !brasero_plugin_get_compulsory (plugin));	
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item),
					brasero_plugin_get_active (plugin, 0));
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
	GtkAllocation allocation;

	gdk_window_get_origin (gtk_widget_get_window (w), x, y);
	gtk_widget_get_preferred_size (GTK_WIDGET (menu), &requisition, NULL);

	gtk_widget_get_allocation (w, &allocation);
	if (gtk_widget_get_direction (w) == GTK_TEXT_DIR_RTL) {
		*x += allocation.x + allocation.width - requisition.width;
	} else {
		*x += allocation.x;
	}

	*y += allocation.y + allocation.height;

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

		gdk_window_get_origin (gtk_widget_get_window (GTK_WIDGET (tree)), x, y);
			
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

			gtk_widget_get_preferred_size (GTK_WIDGET (menu), &requisition, NULL);

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
	if (!priv->popup_menu)
		return;
	
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

static gint 
model_name_sort_func (GtkTreeModel *model,
		      GtkTreeIter  *iter1,
		      GtkTreeIter  *iter2,
		      gpointer      user_data)
{
	BraseroPlugin *plugin1, *plugin2;
	gboolean active1, active2;
	
	gtk_tree_model_get (model, iter1, PLUGIN_COLUMN, &plugin1, -1);
	gtk_tree_model_get (model, iter2, PLUGIN_COLUMN, &plugin2, -1);

	active1 = brasero_plugin_get_gtype (plugin1) != G_TYPE_NONE && !brasero_plugin_get_compulsory (plugin1);
	active2 = brasero_plugin_get_gtype (plugin2) != G_TYPE_NONE && !brasero_plugin_get_compulsory (plugin2);

	if (active1 && !active2)
		return -1;

	if (active2 && !active1)
		return 1;

	/* Use the translated name for the plugins */
	return g_utf8_collate (_(brasero_plugin_get_display_name (plugin1)),
			       _(brasero_plugin_get_display_name (plugin2)));
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

	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, cell, FALSE);
	g_object_set (cell, "stock-size", GTK_ICON_SIZE_SMALL_TOOLBAR, NULL);
	gtk_tree_view_column_add_attribute (column,
	                                    cell,
	                                    "sensitive", AVAILABLE_COLUMN);
	gtk_tree_view_column_set_cell_data_func (column, cell,
						 plugin_manager_ui_view_icon_cell_cb,
						 pm, NULL);
	
	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, cell, TRUE);
	gtk_tree_view_column_add_attribute (column,
	                                    cell,
	                                    "sensitive", AVAILABLE_COLUMN);
	g_object_set (cell, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_tree_view_column_set_cell_data_func (column, cell,
						 plugin_manager_ui_view_info_cell_cb,
						 pm, NULL);

	gtk_tree_view_column_set_spacing (column, 6);
	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->tree), column);

	/* Last column */
	cell = gtk_cell_renderer_toggle_new ();
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
	g_object_set (cell, "xpad", 6, NULL);

	/* Sort on the plugin names */
	gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (model),
	                                         model_name_sort_func,
        	                                 NULL,
                	                         NULL);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model),
					      GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
					      GTK_SORT_ASCENDING);

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
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *viewport;
	GtkWidget *alignment;
	GtkWidget *vbuttonbox;
	BraseroPluginManagerUIPrivate *priv;

	priv = BRASERO_PLUGIN_MANAGER_UI_GET_PRIVATE (pm);

	gtk_orientable_set_orientation (GTK_ORIENTABLE (pm), GTK_ORIENTATION_VERTICAL);
	gtk_box_set_spacing (GTK_BOX (pm), 6);
	gtk_container_set_border_width (GTK_CONTAINER (pm), 12);

	alignment = gtk_alignment_new (0., 0., 1., 1.);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 12, 0);
	gtk_box_pack_start (GTK_BOX (pm), alignment, TRUE, TRUE, 0);
 	
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (alignment), vbox);

	/* bottom part: tree, buttons */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
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

	vbuttonbox = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
	gtk_box_pack_start (GTK_BOX (hbox), vbuttonbox, FALSE, FALSE, 0);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (vbuttonbox), GTK_BUTTONBOX_START);
	gtk_box_set_spacing (GTK_BOX (vbuttonbox), 8);

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
	priv->plugins = brasero_burn_library_get_plugins_list ();

	if (!priv->plugins){
		gtk_widget_set_sensitive (priv->about_button, FALSE);
		gtk_widget_set_sensitive (priv->configure_button, FALSE);		
	}
	else
		plugin_manager_ui_populate_lists (pm);
}

static void
brasero_plugin_manager_ui_finalize (GObject *object)
{
	BraseroPluginManagerUI *pm = BRASERO_PLUGIN_MANAGER_UI (object);
	BraseroPluginManagerUIPrivate *priv;

	priv = BRASERO_PLUGIN_MANAGER_UI_GET_PRIVATE (pm);

	if (priv->plugins) {
		g_slist_foreach (priv->plugins, (GFunc) g_object_unref, NULL);
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
