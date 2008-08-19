/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2007-2008 <bonfire-app@wanadoo.fr>
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
#include <glib/gi18n-lib.h>

#include <gdk/gdkkeysyms.h>

#include <gtk/gtkvbox.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkaction.h>
#include <gtk/gtktoggleaction.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkcellrenderer.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtksizegroup.h>

#include "eggtreemultidnd.h"
#include "baobab-cell-renderer-progress.h"

#include "brasero-data-disc.h"
#include "brasero-file-node.h"
#include "brasero-data-project.h"
#include "brasero-data-vfs.h"
#include "brasero-data-session.h"
#include "brasero-data-tree-model.h"
#include "brasero-file-filtered.h"
#include "brasero-disc.h"
#include "brasero-utils.h"
#include "brasero-disc-message.h"
#include "brasero-rename.h"
#include "brasero-notify.h"

#include "burn-debug.h"
#include "burn-basics.h"
#include "burn-track.h"
#include "burn-session.h"


typedef struct _BraseroDataDiscPrivate BraseroDataDiscPrivate;
struct _BraseroDataDiscPrivate
{
	GtkWidget *tree;
	GtkWidget *filter;
	BraseroDataProject *project;
	GtkWidget *notebook;

	GtkWidget *message;

	GtkUIManager *manager;
	GtkActionGroup *disc_group;

	gint press_start_x;
	gint press_start_y;

	BraseroFileNode *selected;

	GSList *load_errors;

	guint loading;

	guint editing:1;
	guint reject_files:1;

	guint G2_files:1;
	guint deep_directory:1;
};

#define BRASERO_DATA_DISC_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_DATA_DISC, BraseroDataDiscPrivate))

static void
brasero_data_disc_import_session_cb (GtkToggleAction *action,
				     BraseroDataDisc *disc);
static void
brasero_data_disc_new_folder_clicked_cb (GtkButton *button,
					 BraseroDataDisc *disc);
static void
brasero_data_disc_open_activated_cb (GtkAction *action,
				     BraseroDataDisc *disc);
static void
brasero_data_disc_rename_activated_cb (GtkAction *action,
				       BraseroDataDisc *disc);
static void
brasero_data_disc_delete_activated_cb (GtkAction *action,
				       BraseroDataDisc *disc);
static void
brasero_data_disc_paste_activated_cb (GtkAction *action,
				      BraseroDataDisc *disc);

static GtkActionEntry entries [] = {
	{"ContextualMenu", NULL, N_("Menu")},
	{"OpenFile", GTK_STOCK_OPEN, NULL, NULL, N_("Open the selected files"),
	 G_CALLBACK (brasero_data_disc_open_activated_cb)},
	{"RenameData", NULL, N_("R_ename..."), NULL, N_("Rename the selected file"),
	 G_CALLBACK (brasero_data_disc_rename_activated_cb)},
	{"DeleteData", GTK_STOCK_REMOVE, NULL, NULL, N_("Remove the selected files from the project"),
	 G_CALLBACK (brasero_data_disc_delete_activated_cb)},
	{"PasteData", GTK_STOCK_PASTE, NULL, NULL, N_("Add the files stored in the clipboard"),
	 G_CALLBACK (brasero_data_disc_paste_activated_cb)},
	{"NewFolder", "folder-new", N_("New folder"), NULL, N_("Create a new empty folder"),
	 G_CALLBACK (brasero_data_disc_new_folder_clicked_cb)},
};

static GtkToggleActionEntry toggle_entries [] = {
	{"ImportSession", "drive-optical", N_("Import Session"), NULL, N_("Import session"),
	 G_CALLBACK (brasero_data_disc_import_session_cb), FALSE},
};

static const gchar *description = {
	"<ui>"
	"<menubar name='menubar' >"
		"<menu action='EditMenu'>"
		"<placeholder name='EditPlaceholder'>"
			"<menuitem action='NewFolder'/>"
			"<menuitem action='ImportSession'/>"
		"</placeholder>"
		"</menu>"
	"</menubar>"
	"<popup action='ContextMenu'>"
		"<menuitem action='OpenFile'/>"
		"<menuitem action='DeleteData'/>"
		"<menuitem action='RenameData'/>"
		"<separator/>"
		"<menuitem action='PasteData'/>"
	"</popup>"
	"<toolbar name='Toolbar'>"
		"<placeholder name='DiscButtonPlaceholder'>"
			"<separator/>"
			"<toolitem action='NewFolder'/>"
			"<toolitem action='ImportSession'/>"
		"</placeholder>"
	"</toolbar>"
	"</ui>"
};

enum {
	TREE_MODEL_ROW		= 150,
	TARGET_URIS_LIST,
};

static GtkTargetEntry ntables_cd [] = {
	{BRASERO_DND_TARGET_SELF_FILE_NODES, GTK_TARGET_SAME_WIDGET, TREE_MODEL_ROW},
	{"text/uri-list", 0, TARGET_URIS_LIST}
};
static guint nb_targets_cd = sizeof (ntables_cd) / sizeof (ntables_cd[0]);

static GtkTargetEntry ntables_source [] = {
	{BRASERO_DND_TARGET_SELF_FILE_NODES, GTK_TARGET_SAME_WIDGET, TREE_MODEL_ROW},
};

static guint nb_targets_source = sizeof (ntables_source) / sizeof (ntables_source[0]);

enum {
	PROP_NONE,
	PROP_REJECT_FILE,
};

static void
brasero_data_disc_iface_disc_init (BraseroDiscIface *iface);

G_DEFINE_TYPE_WITH_CODE (BraseroDataDisc,
			 brasero_data_disc,
			 GTK_TYPE_VBOX,
			 G_IMPLEMENT_INTERFACE (BRASERO_TYPE_DISC,
					        brasero_data_disc_iface_disc_init));

/**
 * Actions callbacks
 */

static void
brasero_data_disc_import_failure_dialog (BraseroDataDisc *disc,
					 GError *error)
{
	GtkWidget *dialog;
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (disc));
	dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					 GTK_DIALOG_DESTROY_WITH_PARENT |
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_WARNING,
					 GTK_BUTTONS_CLOSE,
					 _("The session couldn't be imported:"));

	gtk_window_set_title (GTK_WINDOW (dialog), _("Session Import Error"));
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  error?error->message:_("unknown error"));

	gtk_widget_show_all (dialog);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static gboolean
brasero_data_disc_import_session (BraseroDataDisc *disc,
				  gboolean import)
{
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);

	if (import) {
		GError *error = NULL;

		if (!brasero_data_session_add_last (BRASERO_DATA_SESSION (priv->project), &error)) {
			brasero_data_disc_import_failure_dialog (disc, error);
			return FALSE;
		}

		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), 1);
		return TRUE;
	}

	brasero_data_session_remove_last (BRASERO_DATA_SESSION (priv->project));
	return FALSE;
}

static void
brasero_data_disc_import_session_cb (GtkToggleAction *action,
				     BraseroDataDisc *disc)
{
	BraseroDataDiscPrivate *priv;
	gboolean res;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);

	brasero_notify_message_remove (BRASERO_NOTIFY (priv->message), BRASERO_NOTIFY_CONTEXT_MULTISESSION);

	res = brasero_data_disc_import_session (disc, gtk_toggle_action_get_active (action));

	/* make sure the button reflects the current state */
	if (gtk_toggle_action_get_active (action) != res) {
		g_signal_handlers_block_by_func (action, brasero_data_disc_import_session_cb, disc);
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), res);
		g_signal_handlers_unblock_by_func (action, brasero_data_disc_import_session_cb, disc);
	}
}

static BraseroFileNode *
brasero_data_disc_get_parent (BraseroDataDisc *self)
{
	BraseroDataDiscPrivate *priv;
	GtkTreeSelection *selection;
	BraseroFileNode *parent;
	GtkTreePath *treepath;
	GtkTreeModel *sort;
	GList *list;

	priv = BRASERO_DATA_DISC_PRIVATE (self);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));
	list = gtk_tree_selection_get_selected_rows (selection, &sort);

	if (g_list_length (list) > 1) {
		g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
		g_list_free (list);
		return brasero_data_project_get_root (priv->project);
	}

	if (!list)
		return brasero_data_project_get_root (priv->project);

	treepath = list->data;
	g_list_free (list);

	parent = brasero_data_tree_model_path_to_node (BRASERO_DATA_TREE_MODEL (priv->project), treepath);
	gtk_tree_path_free (treepath);

	if (parent->is_loading)
		return brasero_data_project_get_root (priv->project);

	if (parent->is_file)
		parent = parent->parent;

	return parent;
}

static void
brasero_data_disc_new_folder_clicked_cb (GtkButton *button,
					 BraseroDataDisc *disc)
{
	BraseroDataDiscPrivate *priv;
	BraseroFileNode *parent;
	BraseroFileNode *node;
	gchar *name;
	gint nb;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);
	if (priv->loading || priv->reject_files)
		return;

	parent = brasero_data_disc_get_parent (disc);
	name = g_strdup_printf (_("New folder"));
	nb = 1;

newname:

	if (brasero_file_node_check_name_existence (parent, name)) {
		g_free (name);
		name = g_strdup_printf (_("New folder %i"), nb);
		nb++;
		goto newname;
	}

	/* just to make sure that tree is not hidden behind info */
	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), 1);
	node = brasero_data_project_add_empty_directory (priv->project, name, parent);
	if (node) {
		GtkTreePath *treepath;
		GtkTreeViewColumn *column;

		/* grab focus must be called before next function to avoid
		 * triggering a bug where if pointer is not in the widget 
		 * any more and enter is pressed the cell will remain editable */
		column = gtk_tree_view_get_column (GTK_TREE_VIEW (priv->tree), 0);
		gtk_widget_grab_focus (priv->tree);

		treepath = brasero_data_tree_model_node_to_path (BRASERO_DATA_TREE_MODEL (priv->project), node);
		gtk_tree_view_set_cursor (GTK_TREE_VIEW (priv->tree),
					  treepath,
					  column,
					  TRUE);
		gtk_tree_path_free (treepath);
	}

	g_free (name);
}

struct _BraseroClipData {
	BraseroDataDisc *disc;
	guint reference;
};
typedef struct _BraseroClipData BraseroClipData;

static void
brasero_data_disc_clipboard_text_cb (GtkClipboard *clipboard,
				     const char *text,
				     BraseroClipData *data)
{
	BraseroFileNode *parent = NULL;
	BraseroDataDiscPrivate *priv;
	gchar **array;
	gchar **item;

	priv = BRASERO_DATA_DISC_PRIVATE (data->disc);

	if (data->reference)
		parent = brasero_data_project_reference_get (priv->project, data->reference);

	array = g_strsplit_set (text, "\n\r", 0);
	item = array;
	while (*item) {
		if (**item != '\0') {
			gchar *uri;
			GFile *file;

			file = g_file_new_for_commandline_arg (*item);
			uri = g_file_get_uri (file);
			g_object_unref (file);

			brasero_data_project_add_loading_node (priv->project,
							       uri,
							       parent);

			/* NOTE: no need to care about the notebook page since 
			 * to reach this part the tree should be displayed first
			 * to have the menu. */
		}

		item++;
	}
	g_strfreev (array);

	if (data->reference)
		brasero_data_project_reference_free (priv->project, data->reference);

	g_free (data);
}

static void
brasero_data_disc_clipboard_targets_cb (GtkClipboard *clipboard,
					GdkAtom *atoms,
					gint n_atoms,
					BraseroClipData *data)
{
	BraseroDataDiscPrivate *priv;
	GdkAtom *iter;
	gchar *target;

	priv = BRASERO_DATA_DISC_PRIVATE (data->disc);

	iter = atoms;
	while (n_atoms) {
		target = gdk_atom_name (*iter);

		if (!strcmp (target, "x-special/gnome-copied-files")
		||  !strcmp (target, "UTF8_STRING")) {
			gtk_clipboard_request_text (clipboard,
						    (GtkClipboardTextReceivedFunc)
						    brasero_data_disc_clipboard_text_cb,
						    data);
			g_free (target);
			return;
		}

		g_free (target);
		iter++;
		n_atoms--;
	}

	if (data->reference)
		brasero_data_project_reference_free (priv->project, data->reference);

	g_free (data);
}

static void
brasero_data_disc_paste_activated_cb (GtkAction *action,
				      BraseroDataDisc *disc)
{
	BraseroDataDiscPrivate *priv;
	GtkClipboard *clipboard;
	BraseroFileNode *parent;
	BraseroClipData *data;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);

	data = g_new0 (BraseroClipData, 1);
	data->disc = disc;

	parent = brasero_data_disc_get_parent (disc);
	if (parent)
		data->reference = brasero_data_project_reference_new (priv->project, parent);

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_request_targets (clipboard,
				       (GtkClipboardTargetsReceivedFunc) brasero_data_disc_clipboard_targets_cb,
				       data);
}

/**
 * Row name edition
 */

static void
brasero_data_disc_name_editing_started_cb (GtkCellRenderer *renderer,
					   GtkCellEditable *editable,
					   gchar *path,
					   BraseroDataDisc *disc)
{
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);
	priv->editing = 1;
}

static void
brasero_data_disc_name_editing_canceled_cb (GtkCellRenderer *renderer,
					    BraseroDataDisc *disc)
{
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);
	priv->editing = 0;
}

static void
brasero_data_disc_name_edited_cb (GtkCellRendererText *cellrenderertext,
				  gchar *path_string,
				  gchar *text,
				  BraseroDataDisc *self)
{
	BraseroDataDiscPrivate *priv;
	BraseroFileNode *node;
	GtkTreePath *path;
	GtkTreeIter row;

	priv = BRASERO_DATA_DISC_PRIVATE (self);

	priv->editing = 0;

	path = gtk_tree_path_new_from_string (path_string);

	/* see if this is still a valid path. It can happen a user removes it
	 * while the name of the row is being edited */
	if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->project), &row, path)) {
		gtk_tree_path_free (path);
		return;
	}

	node = brasero_data_tree_model_path_to_node (BRASERO_DATA_TREE_MODEL (priv->project), path);
	gtk_tree_path_free (path);

	/* make sure it actually changed */
	if (!strcmp (BRASERO_FILE_NODE_NAME (node), text))
		return;

	/* NOTE: BraseroDataProject is where we handle name collisions,
	 * UTF-8 validity, ...
	 * Here if there is a name collision then rename gets aborted. */
	brasero_data_project_rename_node (priv->project, node, text);
}

/**
 * miscellaneous callbacks
 */

static void
brasero_data_disc_set_expand_state (BraseroDataDisc *self,
				    GtkTreePath *treepath,
				    gboolean expanded)
{
	BraseroDataDiscPrivate *priv;
	BraseroFileNode *node;

	priv = BRASERO_DATA_DISC_PRIVATE (self);

	/* only directories can be collapsed */
	node = brasero_data_tree_model_path_to_node (BRASERO_DATA_TREE_MODEL (priv->project), treepath);

	if (node)
		node->is_expanded = expanded;
}

static void
brasero_data_disc_row_collapsed_cb (GtkTreeView *tree,
				    GtkTreeIter *sortparent,
				    GtkTreePath *sortpath,
				    BraseroDataDisc *self)
{
	brasero_data_disc_set_expand_state (self, sortpath, FALSE);
}

static void
brasero_data_disc_row_expanded_cb (GtkTreeView *tree,
				   GtkTreeIter *parent,
				   GtkTreePath *treepath,
				   BraseroDataDisc *self)
{
	brasero_data_disc_set_expand_state (self, treepath, TRUE);
}

static void
brasero_data_disc_project_loaded_cb (BraseroDataProject *project,
				     gint loading,
				     BraseroDataDisc *self)
{
	BraseroDataDiscPrivate *priv;
	GtkWidget *message;

	priv = BRASERO_DATA_DISC_PRIVATE (self);

	message = brasero_notify_get_message_by_context_id (BRASERO_NOTIFY (priv->message), BRASERO_NOTIFY_CONTEXT_LOADING);
	if (!message)
		return;

	if (loading > 0) {
		/* we're not done yet update progress. */
		brasero_disc_message_set_progress (BRASERO_DISC_MESSAGE (message),
						   (gdouble) (priv->loading - loading) / (gdouble) priv->loading);
		return;
	}

	priv->loading = 0;
	if (priv->load_errors) {
		brasero_disc_message_remove_buttons (BRASERO_DISC_MESSAGE (message));

		brasero_disc_message_set_primary (BRASERO_DISC_MESSAGE (message),
						  _("The contents of the project changed since it was saved."));
		brasero_disc_message_set_secondary (BRASERO_DISC_MESSAGE (message),
						    _("Do you want to continue or discard the old project layout?"));

		brasero_disc_message_set_image (BRASERO_DISC_MESSAGE (message),GTK_STOCK_DIALOG_WARNING);
		brasero_disc_message_set_progress_active (BRASERO_DISC_MESSAGE (message), FALSE);
		brasero_notify_button_add (BRASERO_NOTIFY (priv->message),
					   BRASERO_DISC_MESSAGE (message),
					   _("_Discard"),
					   _("Press if you want to discard the current modified project"),
					   GTK_RESPONSE_CANCEL);
		brasero_notify_button_add (BRASERO_NOTIFY (priv->message),
					   BRASERO_DISC_MESSAGE (message),
					   _("_Continue"),
					   _("Press if you want to continue with the current modified project"),
					   GTK_RESPONSE_OK);

		brasero_disc_message_add_errors (BRASERO_DISC_MESSAGE (message),
						 priv->load_errors);
		g_slist_foreach (priv->load_errors, (GFunc) g_free , NULL);
		g_slist_free (priv->load_errors);
		priv->load_errors = NULL;
	}
	else {
		gtk_widget_set_sensitive (GTK_WIDGET (priv->tree), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->filter), TRUE);

		gtk_widget_destroy (message);
	}

	priv->loading = FALSE;
}

static void
brasero_data_disc_activity_changed_cb (BraseroDataVFS *vfs,
				       gboolean active,
				       BraseroDataDisc *self)
{
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (self);

	if (!GTK_WIDGET (self)->window)
		return;

	if (active) {
		GdkCursor *cursor;

		cursor = gdk_cursor_new (GDK_WATCH);
		gdk_window_set_cursor (GTK_WIDGET (self)->window, cursor);
		gdk_cursor_unref (cursor);
	}
	else
		gdk_window_set_cursor (GTK_WIDGET (self)->window, NULL);
}

static void
brasero_data_disc_filtered_uri_cb (BraseroDataVFS *vfs,
				   BraseroFilterStatus status,
				   const gchar *uri,
				   BraseroDataDisc *self)
{
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (self);
	if (status != BRASERO_FILTER_NONE)
		brasero_file_filtered_add (BRASERO_FILE_FILTERED (priv->filter), uri, status);
	else
		brasero_file_filtered_remove (BRASERO_FILE_FILTERED (priv->filter), uri);
}

static void
brasero_data_disc_filter_expanded_cb (GtkExpander *expander,
				      BraseroDataDisc *self)
{
	GtkWidget *parent;

	parent = gtk_widget_get_parent (GTK_WIDGET (expander));

	if (!gtk_expander_get_expanded (expander))
		gtk_box_set_child_packing (GTK_BOX (parent), GTK_WIDGET (expander), TRUE, TRUE, 0, GTK_PACK_END);
	else
		gtk_box_set_child_packing (GTK_BOX (parent), GTK_WIDGET (expander), FALSE, TRUE, 0, GTK_PACK_END);
}

static void
brasero_data_disc_filtered_file_cb (BraseroFileFiltered *filter,
				    const gchar *uri,
				    BraseroDataDisc *self)
{
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (self);
	brasero_data_vfs_remove_restored (BRASERO_DATA_VFS (priv->project), uri);
	brasero_data_project_exclude_uri (BRASERO_DATA_PROJECT (priv->project), uri);
}

static void
brasero_data_disc_restored_file_cb (BraseroFileFiltered *filter,
				    const gchar *uri,
				    BraseroDataDisc *self)
{
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (self);

	brasero_data_vfs_add_restored (BRASERO_DATA_VFS (priv->project), uri);
	brasero_data_project_restore_uri (BRASERO_DATA_PROJECT (priv->project), uri);
}

static void
brasero_data_disc_unreadable_uri_cb (BraseroDataVFS *vfs,
				     const GError *error,
				     const gchar *uri,
				     BraseroDataDisc *self)
{
	gchar *name;
	GtkWidget *dialog;
	GtkWidget *toplevel;
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (self);

	name = brasero_file_node_get_uri_name (uri);
	if (priv->loading) {
		priv->load_errors = g_slist_prepend (priv->load_errors,
						     g_strdup (error->message));

		return;
	}

 	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
	dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					 GTK_DIALOG_DESTROY_WITH_PARENT |
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_CLOSE,
					 _("\"%s\" cannot be added to the selection:"),
					 name);
	g_free (name);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Unreadable File"));
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  "%s.",
						  error->message);

	gtk_widget_show_all (dialog);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static void
brasero_data_disc_recursive_uri_cb (BraseroDataVFS *vfs,
				    const gchar *uri,
				    BraseroDataDisc *self)
{
	gchar *name;
	GtkWidget *dialog;
	GtkWidget *toplevel;
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (self);

	name = brasero_file_node_get_uri_name (uri);
	if (priv->loading) {
		gchar *message;

		message = g_strdup_printf (_("\"%s\" is a recursive symlink."), name);
		priv->load_errors = g_slist_prepend (priv->load_errors, message);
		g_free (name);

		return;
	}

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
	dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					 GTK_DIALOG_DESTROY_WITH_PARENT |
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_CLOSE,
					 _("\"%s\" cannot be added to the selection:"),
					 name);
	g_free (name);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Recursive Symlink"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  _("it is a recursive symlink."));

	gtk_widget_show_all (dialog);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static void
brasero_data_disc_unknown_uri_cb (BraseroDataVFS *vfs,
				  const gchar *uri,
				  BraseroDataDisc *self)
{
	gchar *name;
	GtkWidget *dialog;
	GtkWidget *toplevel;
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (self);

	name = brasero_file_node_get_uri_name (uri);
	if (priv->loading) {
		gchar *message;

		message = g_strdup_printf (_("\"%s\" cannot be found."), name);
		priv->load_errors = g_slist_prepend (priv->load_errors, message);
		g_free (name);

		return;
	}

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
	dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					 GTK_DIALOG_DESTROY_WITH_PARENT |
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_CLOSE,
					 _("\"%s\" cannot be added to the selection:"),
					 name);
	g_free (name);

	gtk_window_set_title (GTK_WINDOW (dialog), _("File Not Found"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  _("it doesn't exist at the specified location."));

	gtk_widget_show_all (dialog);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static gboolean
brasero_data_disc_name_collision_cb (BraseroDataProject *project,
				     const gchar *name,
				     BraseroDataDisc *self)
{
	gint answer;
	GtkWidget *dialog;
	GtkWidget *toplevel;
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (self);

	if (priv->loading) {
		/* don't do anything accept replacement */
		return FALSE;
	}

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
	dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					 GTK_DIALOG_DESTROY_WITH_PARENT |
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_WARNING,
					 GTK_BUTTONS_NONE,
					 _("\"%s\" already exists in the directory:"),
					 name);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Already Existing File"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  _("do you really want to replace it?"));

	gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Don't replace"), GTK_RESPONSE_NO);
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Replace"), GTK_RESPONSE_YES);

	gtk_widget_show_all (dialog);
	answer = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	return (answer != GTK_RESPONSE_YES);
}

static gboolean
brasero_data_disc_2G_file_cb (BraseroDataProject *project,
			      const gchar *name,
			      BraseroDataDisc *self)
{
	gint answer;
	GtkWidget *dialog;
	GtkWidget *toplevel;
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (self);

	if (priv->G2_files)
		return FALSE;

	if (priv->loading) {
		/* don't do anything just accept these files from now on */
		priv->G2_files = TRUE;
		return FALSE;
	}

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
	dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					 GTK_DIALOG_DESTROY_WITH_PARENT |
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_WARNING,
					 GTK_BUTTONS_NONE,
					 _("Do you really want to add \"%s\" to the selection and use the third version of ISO9660 standard to support it?"),
					 name);

	gtk_window_set_title (GTK_WINDOW (dialog), _("File Over 2 GiB"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  _("The size of the file is over 2 GiB. This isn't supported by ISO9660 standard in his first and second versions (the most widespread ones)."
						    "\nIt is recommended to use the third version of ISO9660 standard which is supported by most of the operating systems including Linux and all versions of Windows ©."
						    "\nA known exception is MacOS X that can't read images created with version 3 of ISO9660 standard."));

	gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Don't add"), GTK_RESPONSE_NO);
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Add"), GTK_RESPONSE_YES);

	gtk_widget_show_all (dialog);
	answer = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	priv->G2_files = (answer == GTK_RESPONSE_YES);
	return (answer != GTK_RESPONSE_YES);
}

static gboolean
brasero_data_disc_deep_directory_cb (BraseroDataProject *project,
				     const gchar *name,
				     BraseroDataDisc *self)
{
	gint answer;
	GtkWidget *dialog;
	GtkWidget *toplevel;
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (self);

	if (priv->deep_directory)
		return FALSE;

	if (priv->loading) {
		/* don't do anything just accept these directories from now on */
		priv->deep_directory = TRUE;
		return FALSE;
	}

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
	dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					 GTK_DIALOG_DESTROY_WITH_PARENT |
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_WARNING,
					 GTK_BUTTONS_NONE,
					 _("Do you really want to add \"%s\" to the selection?"),
					 name);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Deep Directory"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  _("The children of this directory will have 6 parent directories. This is a violation of the ISO9660 standard which only allows 6."
						    "\nBrasero can create an image of such a file hierarchy and burn it; but the media may not be readable on all operating systems."
						    "\nNOTE: such a file hierarchy is known to work on linux."));

	gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Don't add"), GTK_RESPONSE_NO);
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Add"), GTK_RESPONSE_YES);

	gtk_widget_show_all (dialog);
	answer = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	priv->deep_directory = (answer == GTK_RESPONSE_YES);
	return (answer != GTK_RESPONSE_YES);
}

static void
brasero_data_disc_size_changed_cb (BraseroDataProject *project,
				   BraseroDataDisc *self)
{
	gint64 size;

	size = brasero_data_project_get_size (project);
	brasero_disc_size_changed (BRASERO_DISC (self), size);
}

static void
brasero_disc_disc_session_import_response_cb (GtkButton *button,
					      GtkResponseType response,
					      BraseroDataDisc *self)
{
	gboolean res;
	GtkAction *action;
	BraseroDataDiscPrivate *priv;

	if (response != GTK_RESPONSE_OK)
		return;

	priv = BRASERO_DATA_DISC_PRIVATE (self);
	res = brasero_data_disc_import_session (self, TRUE);

	action = gtk_action_group_get_action (priv->disc_group, "ImportSession");

	g_signal_handlers_block_by_func (action, brasero_data_disc_import_session_cb, self);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), res);
	g_signal_handlers_unblock_by_func (action, brasero_data_disc_import_session_cb, self);
}

static void
brasero_data_disc_session_available_cb (BraseroDataSession *session,
					gboolean multisession,
					BraseroDataDisc *self)
{
	GtkAction *action;
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (self);
	action = gtk_action_group_get_action (priv->disc_group, "ImportSession");

	if (multisession) {
		GtkWidget *message;

		gtk_action_set_sensitive (action, TRUE);
		message = brasero_notify_message_add (BRASERO_NOTIFY (priv->message),
						      _("A multisession disc is inserted:"),
						      _("Do you want to import its contents?"),
						      10000,
						      BRASERO_NOTIFY_CONTEXT_MULTISESSION);

		brasero_disc_message_set_image (BRASERO_DISC_MESSAGE (message), GTK_STOCK_DIALOG_INFO);

		brasero_notify_button_add (BRASERO_NOTIFY (priv->message),
					   BRASERO_DISC_MESSAGE (message),
					   _("_Import Session"),
					   _("Click here to import its contents"),
					   GTK_RESPONSE_OK);
		g_signal_connect (BRASERO_DISC_MESSAGE (message),
				  "response",
				  G_CALLBACK (brasero_disc_disc_session_import_response_cb),
				  self);
	}
	else {
		brasero_notify_message_remove (BRASERO_NOTIFY (priv->message), BRASERO_NOTIFY_CONTEXT_MULTISESSION);
		gtk_action_set_sensitive (action, FALSE);
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), FALSE);
	}
}

static void
brasero_data_disc_session_loaded_cb (BraseroDataSession *session,
				     gboolean loaded,
				     BraseroDataDisc *self)
{
	if (loaded)
		brasero_disc_flags_changed (BRASERO_DISC (self), BRASERO_BURN_FLAG_MERGE);
	else
		brasero_disc_flags_changed (BRASERO_DISC (self), BRASERO_BURN_FLAG_NONE);
}

/**
 * BraseroDisc interface implementation
 */

static void
brasero_data_disc_clear (BraseroDisc *disc)
{
	BraseroDataDiscPrivate *priv;
	GtkAction *action;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);

	if (priv->loading)
		return;

	action = gtk_action_group_get_action (priv->disc_group, "ImportSession");
	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)))
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), FALSE);

	if (priv->load_errors) {
		g_slist_foreach (priv->load_errors, (GFunc) g_free , NULL);
		g_slist_free (priv->load_errors);
		priv->load_errors = NULL;
	}

	priv->G2_files = FALSE;
	priv->deep_directory = FALSE;

 	brasero_notify_message_remove (BRASERO_NOTIFY (priv->message), BRASERO_NOTIFY_CONTEXT_SIZE);
	brasero_notify_message_remove (BRASERO_NOTIFY (priv->message), BRASERO_NOTIFY_CONTEXT_LOADING);
	brasero_notify_message_remove (BRASERO_NOTIFY (priv->message), BRASERO_NOTIFY_CONTEXT_MULTISESSION);

	brasero_data_project_reset (priv->project);
	brasero_file_filtered_clear (BRASERO_FILE_FILTERED (priv->filter));
	brasero_disc_size_changed (disc, 0);

	gdk_window_set_cursor (GTK_WIDGET (disc)->window, NULL);
}

static void
brasero_data_disc_reset (BraseroDisc *disc)
{
	BraseroDataDiscPrivate *priv;
	GtkAction *action;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);

	action = gtk_action_group_get_action (priv->disc_group, "ImportSession");
	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)))
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), FALSE);

	if (priv->load_errors) {
		g_slist_foreach (priv->load_errors, (GFunc) g_free , NULL);
		g_slist_free (priv->load_errors);
		priv->load_errors = NULL;
	}

	brasero_data_project_reset (priv->project);

	priv->loading = FALSE;
	priv->G2_files = FALSE;
	priv->deep_directory = FALSE;

 	brasero_notify_message_remove (BRASERO_NOTIFY (priv->message), BRASERO_NOTIFY_CONTEXT_SIZE);
	brasero_notify_message_remove (BRASERO_NOTIFY (priv->message), BRASERO_NOTIFY_CONTEXT_LOADING);
	brasero_notify_message_remove (BRASERO_NOTIFY (priv->message), BRASERO_NOTIFY_CONTEXT_MULTISESSION);

	brasero_file_filtered_clear (BRASERO_FILE_FILTERED (priv->filter));
	brasero_disc_size_changed (disc, 0);

	gdk_window_set_cursor (GTK_WIDGET (disc)->window, NULL);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), 0);
}

static void
brasero_data_disc_delete_selected (BraseroDisc *disc)
{
	BraseroDataDiscPrivate *priv;
	GtkTreeSelection *selection;
	GtkTreePath *cursorpath;
	GList *list, *iter;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);

	if (priv->loading)
		return;

	/* we must start by the end for the treepaths to point to valid rows */
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));
	list = gtk_tree_selection_get_selected_rows (selection, NULL);
	list = g_list_reverse (list);

	gtk_tree_view_get_cursor (GTK_TREE_VIEW (priv->tree),
				  &cursorpath,
				  NULL);

	for (iter = list; iter; iter = iter->next) {
		GtkTreePath *treepath;
		BraseroFileNode *node;

		treepath = iter->data;
		if (cursorpath && !gtk_tree_path_compare (cursorpath, treepath)) {
			GtkTreePath *tmp_path;

			/* this is to silence a warning with SortModel when
			 * removing a row being edited. We can only hope that
			 * there won't be G_MAXINT rows =) */
			tmp_path = gtk_tree_path_new_from_indices (G_MAXINT, -1);
			gtk_tree_view_set_cursor (GTK_TREE_VIEW (priv->tree),
						  tmp_path,
						  NULL,
						  FALSE);
			gtk_tree_path_free (tmp_path);
		}

		node = brasero_data_tree_model_path_to_node (BRASERO_DATA_TREE_MODEL (priv->project), treepath);
 		gtk_tree_path_free (treepath);

		brasero_data_project_remove_node (priv->project, node);
	}
	g_list_free (list);

	if (cursorpath)
		gtk_tree_path_free (cursorpath);

	/* warn that the selection changed (there are no more selected paths) */
	if (priv->selected)
		priv->selected = NULL;

	brasero_disc_selection_changed (disc);
}

static BraseroDiscResult
brasero_data_disc_add_uri (BraseroDisc *disc, const gchar *uri)
{
	BraseroDataDiscPrivate *priv;
	BraseroFileNode *parent = NULL;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);

	if (priv->loading || priv->reject_files)
		return BRASERO_DISC_LOADING;

	parent = brasero_data_disc_get_parent (BRASERO_DATA_DISC (disc));
	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), 1);
	if (brasero_data_project_add_loading_node (priv->project, uri, parent))
		return BRASERO_DISC_OK;

	return BRASERO_DISC_ERROR_UNKNOWN;
}

static BraseroDiscResult
brasero_data_disc_get_track (BraseroDisc *disc,
			     BraseroDiscTrack *track)
{
	GSList *grafts = NULL;
	GSList *restored = NULL;
	GSList *unreadable = NULL;
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);

	brasero_data_project_get_contents (priv->project,
					   &grafts,
					   &unreadable,
					   FALSE,
					   FALSE);
	if (!grafts)
		return BRASERO_DISC_ERROR_EMPTY_SELECTION;

	track->type = BRASERO_DISC_TRACK_DATA;
	track->contents.data.grafts = grafts;
	track->contents.data.excluded = unreadable;

	/* get restored */
	brasero_data_vfs_get_restored (BRASERO_DATA_VFS (priv->project), &restored);
	track->contents.data.restored = restored;

	return BRASERO_DISC_OK;
}

static BraseroDiscResult
brasero_data_disc_set_session_param (BraseroDisc *self,
				     BraseroBurnSession *session)
{
	BraseroFileNode *root;
	BraseroTrackType type;
	BraseroImageFS fs_type;
	BraseroFileTreeStats *stats;
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (self);

	root = brasero_data_project_get_root (priv->project);
	stats = BRASERO_FILE_NODE_STATS (root);

	fs_type = BRASERO_IMAGE_FS_ISO;
	if (brasero_data_project_is_joliet_compliant (priv->project))
		fs_type |= BRASERO_IMAGE_FS_JOLIET;

	if (stats->num_2GiB != 0)
		fs_type |= BRASERO_IMAGE_ISO_FS_LEVEL_3|BRASERO_IMAGE_FS_UDF;

	if (stats->num_deep != 0)
		fs_type |= BRASERO_IMAGE_ISO_FS_DEEP_DIRECTORY;

	if (brasero_data_project_is_video_project (priv->project))
		fs_type |= BRASERO_IMAGE_FS_VIDEO;

	type.type = BRASERO_TRACK_TYPE_DATA;
	type.subtype.fs_type = fs_type;
	brasero_burn_session_set_input_type (session, &type);

	/* set multisession options */
	if (brasero_data_session_get_loaded_medium (BRASERO_DATA_SESSION (priv->project))) {
		/* remove the following flag just in case */
		brasero_burn_session_remove_flag (session,
						  BRASERO_BURN_FLAG_FAST_BLANK|
						  BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE);
		brasero_burn_session_add_flag (session, BRASERO_BURN_FLAG_MERGE);
		brasero_burn_session_set_burner (session, brasero_data_session_get_loaded_medium (BRASERO_DATA_SESSION (priv->project)));
	}

	return BRASERO_DISC_OK;
}

static BraseroDiscResult
brasero_data_disc_set_session_contents (BraseroDisc *self,
					BraseroBurnSession *session)
{
	BraseroTrack *track;
	BraseroFileNode *root;
	BraseroTrackType type;
	GSList *grafts = NULL;
	gboolean joliet_compat;
	GSList *unreadable = NULL;
	BraseroFileTreeStats *stats;
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (self);

	/* there should be only one data track */
	brasero_burn_session_get_input_type (session, &type);
	track = brasero_track_new (BRASERO_TRACK_TYPE_DATA);

	/* Set the number of files in the tree */
	root = brasero_data_project_get_root (priv->project);
	stats = BRASERO_FILE_NODE_STATS (root);
	if (stats)
		brasero_track_set_data_file_num (track, stats->children);

	joliet_compat = (type.subtype.fs_type & BRASERO_IMAGE_FS_JOLIET);
	brasero_track_add_data_fs (track, type.subtype.fs_type);

	/* append a slash for mkisofs */
	brasero_data_project_get_contents (priv->project,
					   &grafts,
					   &unreadable,
					   joliet_compat,
					   TRUE); 

	if (!grafts)
		return BRASERO_DISC_ERROR_EMPTY_SELECTION;

	brasero_track_set_data_source (track, grafts, unreadable);
	brasero_burn_session_add_track (session, track);

	/* It's good practice to unref the track afterwards as we don't need it
	 * anymore. BraseroBurnSession refs it. */
	brasero_track_unref (track);

	return BRASERO_DISC_OK;
}

static void
brasero_data_disc_message_response_cb (BraseroDiscMessage *message,
				       GtkResponseType response,
				       BraseroDataDisc *self)
{
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (self);

	gtk_widget_set_sensitive (GTK_WIDGET (priv->tree), TRUE);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->filter), TRUE);

	if (response != GTK_RESPONSE_CANCEL)
		return;

	priv->loading = FALSE;
	brasero_data_disc_clear (BRASERO_DISC (self));
}

static BraseroDiscResult
brasero_data_disc_load_track (BraseroDisc *disc,
			      BraseroDiscTrack *track)
{
	BraseroDataDiscPrivate *priv;
	GtkWidget *message;
	GSList *iter;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);

	/* Firstadd the restored files */
	for (iter = track->contents.data.restored; iter; iter = iter->next) {
		gchar *uri;

		uri = iter->data;
		brasero_data_vfs_add_restored (BRASERO_DATA_VFS (priv->project), uri);
	}

	priv->loading = brasero_data_project_load_contents (priv->project,
							    track->contents.data.grafts,
							    track->contents.data.excluded);

	if (!priv->loading) {
		gtk_widget_set_sensitive (GTK_WIDGET (priv->tree), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->filter), TRUE);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), 1);
		return BRASERO_DISC_OK;
	}

	message = brasero_notify_message_add (BRASERO_NOTIFY (priv->message),
					      _("Please, wait while the project is loading."),
					      NULL,
					      -1,
					      BRASERO_NOTIFY_CONTEXT_LOADING);

	brasero_disc_message_set_image (BRASERO_DISC_MESSAGE (message),GTK_STOCK_DIALOG_INFO);
	brasero_disc_message_set_progress (BRASERO_DISC_MESSAGE (message), 0.0);

	brasero_notify_button_add (BRASERO_NOTIFY (priv->message),
				   BRASERO_DISC_MESSAGE (message),
				   _("_Cancel Loading"),
				   _("Press if you want to cancel current project loading"),
				   GTK_RESPONSE_CANCEL);
	g_signal_connect (message,
			  "response",
			  G_CALLBACK (brasero_data_disc_message_response_cb),
			  disc);

	gtk_widget_set_sensitive (GTK_WIDGET (priv->tree), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->filter), FALSE);

	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), 1);
	return BRASERO_DISC_OK;
}

static BraseroDiscResult
brasero_data_disc_get_status (BraseroDisc *disc)
{
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);
	if (priv->loading)
		return BRASERO_DISC_LOADING;

	if (brasero_data_project_is_empty (priv->project))
		return BRASERO_DISC_ERROR_EMPTY_SELECTION;
	
	if (brasero_data_vfs_is_active (BRASERO_DATA_VFS (priv->project)))
		return BRASERO_DISC_NOT_READY;

	return BRASERO_DISC_OK;
}

static gboolean
brasero_data_disc_get_selected_uri (BraseroDisc *disc,
				    gchar **uri)
{
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);

	if (!priv->selected)
		return FALSE;

	if (!uri)
		return TRUE;

	*uri = brasero_data_project_node_to_uri (priv->project, priv->selected);
	return TRUE;
}

static void
brasero_data_disc_set_drive (BraseroDisc *disc, BraseroDrive *drive)
{
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);

	brasero_data_session_set_drive (BRASERO_DATA_SESSION (priv->project), drive);
}

static guint
brasero_data_disc_add_ui (BraseroDisc *disc,
			  GtkUIManager *manager,
			  GtkWidget *message)
{
	BraseroDataDiscPrivate *priv;
	GError *error = NULL;
	GtkAction *action;
	guint merge_id;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);

	if (priv->message) {
		g_object_unref (priv->message);
		priv->message = NULL;
	}

	priv->message = message;
	g_object_ref (message);

	if (!priv->disc_group) {
		priv->disc_group = gtk_action_group_new (BRASERO_DISC_ACTION);
		gtk_action_group_set_translation_domain (priv->disc_group, GETTEXT_PACKAGE);
		gtk_action_group_add_actions (priv->disc_group,
					      entries,
					      G_N_ELEMENTS (entries),
					      disc);
		gtk_action_group_add_toggle_actions (priv->disc_group,
						     toggle_entries,
						     G_N_ELEMENTS (toggle_entries),
						     disc);
		gtk_ui_manager_insert_action_group (manager,
						    priv->disc_group,
						    0);
	}

	merge_id = gtk_ui_manager_add_ui_from_string (manager,
						      description,
						      -1,
						      &error);
	if (!merge_id) {
		BRASERO_BURN_LOG ("Adding ui elements failed: %s", error->message);
		g_error_free (error);
		return 0;
	}

	action = gtk_action_group_get_action (priv->disc_group, "ImportSession");
	gtk_action_set_sensitive (action, FALSE);
	g_object_set (action,
		      "short-label", _("Import"), /* for toolbar buttons */
		      NULL);

	action = gtk_action_group_get_action (priv->disc_group, "NewFolder");
	g_object_set (action,
		      "short-label", _("New Folder"), /* for toolbar buttons */
		      NULL);

	priv->manager = manager;
	g_object_ref (manager);
	return merge_id;
}

/**
 * Contextual menu callbacks
 */

static void
brasero_data_disc_open_file (BraseroDataDisc *disc, GList *list)
{
	gchar *uri;
	GList *item;
	GSList *uris;
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);

	uris = NULL;
	for (item = list; item; item = item->next) {
		GtkTreePath *treepath;
		BraseroFileNode *node;

		treepath = item->data;
		if (!treepath)
			continue;

		node = brasero_data_tree_model_path_to_node (BRASERO_DATA_TREE_MODEL (priv->project), treepath);
		if (!node || node->is_imported)
			continue;

		uri = brasero_data_project_node_to_uri (priv->project, node);
		if (uri)
			uris = g_slist_prepend (uris, uri);

	}

	if (!uris)
		return;

	brasero_utils_launch_app (GTK_WIDGET (disc), uris);
	g_slist_foreach (uris, (GFunc) g_free, NULL);
	g_slist_free (uris);
}

static void
brasero_data_disc_open_activated_cb (GtkAction *action,
				     BraseroDataDisc *disc)
{
	GList *list;
	GtkTreeSelection *selection;
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));
	list = gtk_tree_selection_get_selected_rows (selection, NULL);
	brasero_data_disc_open_file (disc, list);

	g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (list);
}

static gboolean
brasero_data_disc_mass_rename_cb (GtkTreeModel *model,
				  GtkTreeIter *iter,
				  GtkTreePath *treepath,
				  const gchar *old_name,
				  const gchar *new_name)
{
	BraseroFileNode *node;

	node = brasero_data_tree_model_path_to_node (BRASERO_DATA_TREE_MODEL (model), treepath);
	return brasero_data_project_rename_node (BRASERO_DATA_PROJECT (model),
						 node,
						 new_name);
}

static void
brasero_data_disc_rename_activated (BraseroDataDisc *disc)
{
	BraseroDataDiscPrivate *priv;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkTreePath *treepath;
	GList *list;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));

	list = gtk_tree_selection_get_selected_rows (selection, NULL);
	if (g_list_length (list) == 1) {
		BraseroFileNode *node;

		treepath = list->data;
		g_list_free (list);

		node = brasero_data_tree_model_path_to_node (BRASERO_DATA_TREE_MODEL (priv->project), treepath);
		if (!node || node->is_imported) {
			gtk_tree_path_free (treepath);
			return;
		}

		column = gtk_tree_view_get_column (GTK_TREE_VIEW (priv->tree), 0);

		/* grab focus must be called before next function to avoid
		 * triggering a bug where if pointer is not in the widget 
		 * any more and enter is pressed the cell will remain editable */
		gtk_widget_grab_focus (priv->tree);
		gtk_tree_view_set_cursor_on_cell (GTK_TREE_VIEW (priv->tree),
						  treepath,
						  column,
						  NULL,
						  TRUE);
		gtk_tree_path_free (treepath);
	}
	else {
		GtkWidget *frame;
		GtkWidget *dialog;
		GtkWidget *rename;
		GtkResponseType answer;

		dialog = gtk_dialog_new_with_buttons (_("File Renaming"),
						      GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (disc))),
						      GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
						      _("_Don't rename"), GTK_RESPONSE_CANCEL,
						      _("_Rename"), GTK_RESPONSE_APPLY,
						      NULL);
		gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

		rename = brasero_rename_new ();
		brasero_rename_set_show_keep_default (BRASERO_RENAME (rename), FALSE);
		gtk_widget_show (rename);

		frame = brasero_utils_pack_properties (_("<b>Renaming mode</b>"), rename, NULL);
		gtk_widget_show (frame);

		gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), frame, TRUE, TRUE, 0);
		gtk_widget_show (dialog);

		answer = gtk_dialog_run (GTK_DIALOG (dialog));
		if (answer != GTK_RESPONSE_APPLY) {
			gtk_widget_destroy (dialog);
			return;
		}

		brasero_rename_do (BRASERO_RENAME (rename),
				   gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree)),
				   BRASERO_DATA_TREE_MODEL_NAME,
				   brasero_data_disc_mass_rename_cb);

		gtk_widget_destroy (dialog);
	}
}

static void
brasero_data_disc_rename_activated_cb (GtkAction *action,
				       BraseroDataDisc *disc)
{
	brasero_data_disc_rename_activated (disc);
}

static void
brasero_data_disc_delete_activated_cb (GtkAction *action,
				       BraseroDataDisc *disc)
{
	brasero_data_disc_delete_selected (BRASERO_DISC (disc));
}

/**
 * key/button press handling
 */

static void
brasero_data_disc_selection_changed_cb (GtkTreeSelection *selection,
					BraseroDataDisc *self)
{
	BraseroDataDiscPrivate *priv;
	GtkTreeModel *model;
	GList *selected;

	priv = BRASERO_DATA_DISC_PRIVATE (self);
	priv->selected = NULL;

	selected = gtk_tree_selection_get_selected_rows (selection, &model);
	if (selected) {
		GtkTreePath *treepath;
		BraseroFileNode *node;

		treepath = selected->data;

		/* we need to make sure that this is not a bogus row */
		node = brasero_data_tree_model_path_to_node (BRASERO_DATA_TREE_MODEL (priv->project), treepath);
		if (node && !node->is_imported)
			priv->selected = node;

		g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
		g_list_free (selected);
	}

	brasero_disc_selection_changed (BRASERO_DISC (self));
}

static gboolean
brasero_data_disc_tree_select_function (GtkTreeSelection *selection,
					GtkTreeModel *model,
					GtkTreePath *treepath,
					gboolean is_selected,
					gpointer null_data)
{
	BraseroFileNode *node;

	node = brasero_data_tree_model_path_to_node (BRASERO_DATA_TREE_MODEL (model), treepath);
	if (!node || node->is_imported) {
		if (is_selected)
			return TRUE;

		return FALSE;
	}

	if (is_selected)
		node->is_selected = FALSE;
	else
		node->is_selected = TRUE;

	return TRUE;
}

static void
brasero_data_disc_show_menu (int nb_selected,
			     GtkUIManager *manager,
			     GdkEventButton *event)
{
	GtkWidget *item;

	if (nb_selected == 1) {
		item = gtk_ui_manager_get_widget (manager, "/ContextMenu/OpenFile");
		if (item)
			gtk_widget_set_sensitive (item, TRUE);
		item = gtk_ui_manager_get_widget (manager, "/ContextMenu/RenameData");
		if (item)
			gtk_widget_set_sensitive (item, TRUE);
		item = gtk_ui_manager_get_widget (manager, "/ContextMenu/DeleteData");
		if (item)
			gtk_widget_set_sensitive (item, TRUE);
	}
	else if (!nb_selected) {
		item = gtk_ui_manager_get_widget (manager, "/ContextMenu/OpenFile");
		if (item)
			gtk_widget_set_sensitive (item, FALSE);

		item = gtk_ui_manager_get_widget (manager, "/ContextMenu/RenameData");
		if (item)
			gtk_widget_set_sensitive (item, FALSE);
		item = gtk_ui_manager_get_widget (manager, "/ContextMenu/DeleteData");
		if (item)
			gtk_widget_set_sensitive (item, FALSE);
	}
	else {
		item = gtk_ui_manager_get_widget (manager, "/ContextMenu/OpenFile");
		if (item)
			gtk_widget_set_sensitive (item, TRUE);
		item = gtk_ui_manager_get_widget (manager, "/ContextMenu/RenameData");
		if (item)
			gtk_widget_set_sensitive (item, TRUE);
		item = gtk_ui_manager_get_widget (manager, "/ContextMenu/DeleteData");
		if (item)
			gtk_widget_set_sensitive (item, TRUE);
	}

	item = gtk_ui_manager_get_widget (manager, "/ContextMenu/PasteData");
	if (item) {
		if (gtk_clipboard_wait_is_text_available (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD)))
			gtk_widget_set_sensitive (item, TRUE);
		else
			gtk_widget_set_sensitive (item, FALSE);
	}

	item = gtk_ui_manager_get_widget (manager,"/ContextMenu");
	gtk_menu_popup (GTK_MENU (item),
		        NULL,
			NULL,
			NULL,
			NULL,
			event->button,
			event->time);
}

static gboolean
brasero_data_disc_button_pressed_cb (GtkTreeView *tree,
				     GdkEventButton *event,
				     BraseroDataDisc *self)
{
	gboolean result;
	BraseroFileNode *node = NULL;
	GtkTreePath *treepath = NULL;
	GtkWidgetClass *widget_class;
	BraseroDataDiscPrivate *priv;
	gboolean keep_selection = FALSE;

	priv = BRASERO_DATA_DISC_PRIVATE (self);

	if (GTK_WIDGET_REALIZED (priv->tree)) {
		result = gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (priv->tree),
							event->x,
							event->y,
							&treepath,
							NULL,
							NULL,
							NULL);

		if (treepath)
			node = brasero_data_tree_model_path_to_node (BRASERO_DATA_TREE_MODEL (priv->project),
								     treepath);

		if (node) {
			GtkTreeSelection *selection;
			selection = gtk_tree_view_get_selection (tree);
			keep_selection = gtk_tree_selection_path_is_selected (selection, treepath);
		}

		if (!node && treepath) {
			/* That may be a BOGUS row */
			gtk_tree_path_free (treepath);
			treepath = NULL;
		}
	}
	else
		result = FALSE;

	/* we call the default handler for the treeview before everything else
	 * so it can update itself (particularly its selection) before we use it
	 * NOTE: since the event has been processed here we need to return TRUE
	 * to avoid having the treeview processing this event a second time. */
	widget_class = GTK_WIDGET_GET_CLASS (tree);

	if (priv->loading) {
		widget_class->button_press_event (GTK_WIDGET (tree), event);
		gtk_tree_path_free (treepath);
		return TRUE;
	}

	if ((event->state & (GDK_CONTROL_MASK|GDK_SHIFT_MASK)) == 0) {
		if (node && !node->is_imported)
			priv->selected = node;
		else if ((event->state & GDK_SHIFT_MASK) == 0)
			priv->selected = node;
		else
			priv->selected = NULL;

		brasero_disc_selection_changed (BRASERO_DISC (self));
	}

	if (event->button == 1) {
		widget_class->button_press_event (GTK_WIDGET (tree), event);

		priv->press_start_x = event->x;
		priv->press_start_y = event->y;

		if (event->type == GDK_2BUTTON_PRESS) {
			if (treepath) {
				GList *list;

				list = g_list_prepend (NULL, gtk_tree_path_copy (treepath));
				brasero_data_disc_open_file (self, list);
				g_list_free (list);
			}
		}
		else  if (!node) {
			GtkTreeSelection *selection;

			/* This is to deselect any row when selecting a row that cannot
			 * be selected or in an empty part */
			selection = gtk_tree_view_get_selection (tree);
			gtk_tree_selection_unselect_all (selection);
		}
	}
	else if (event->button == 3) {
		GtkTreeSelection *selection;

		/* Don't update the selection if the right click was on one of
		 * the already selected rows */
		if (!keep_selection) {
			widget_class->button_press_event (GTK_WIDGET (tree), event);

			if (!node) {
				GtkTreeSelection *selection;

				/* This is to deselect any row when selecting a row that cannot
				 * be selected or in an empty part */
				selection = gtk_tree_view_get_selection (tree);
				gtk_tree_selection_unselect_all (selection);
			}
		}

		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));
		brasero_data_disc_show_menu (gtk_tree_selection_count_selected_rows (selection),
					     priv->manager,
					     event);
	}

	gtk_tree_path_free (treepath);

	return TRUE;
}

static gboolean
brasero_data_disc_key_released_cb (GtkTreeView *tree,
				   GdkEventKey *event,
				   BraseroDataDisc *self)
{
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (self);
	
	if (priv->loading)
		return FALSE;

	if (priv->editing)
		return FALSE;

	if (event->keyval == GDK_KP_Delete || event->keyval == GDK_Delete)
		brasero_data_disc_delete_selected (BRASERO_DISC (self));
	else if (event->keyval == GDK_F2)
		brasero_data_disc_rename_activated (self);

	return FALSE;
}

/**
 *
 */

static void
brasero_data_disc_contents_added_cb (GtkTreeModel *model,
				     GtkTreePath *treepath,
				     GtkTreeIter *iter,
				     BraseroDataDisc *self)
{
	brasero_disc_contents_changed (BRASERO_DISC (self), TRUE);
}

static void
brasero_data_disc_contents_removed_cb (GtkTreeModel *model,
				       GtkTreePath *treepath,
				       BraseroDataDisc *self)
{
	BraseroFileNode *root;
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (self);

	root = brasero_data_project_get_root (priv->project);
	brasero_disc_contents_changed (BRASERO_DISC (self), (root && BRASERO_FILE_NODE_CHILDREN (root) != NULL));
}

/**
 * Object creation/destruction
 */
void
brasero_data_disc_set_right_button_group (BraseroDataDisc *self,
					  GtkSizeGroup *size_group)
{
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (self);
	brasero_file_filtered_set_right_button_group (BRASERO_FILE_FILTERED (priv->filter), size_group);
}

static void
brasero_data_disc_init (BraseroDataDisc *object)
{
	BraseroDataDiscPrivate *priv;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeModel *model;
	GtkWidget *mainbox;
	GtkWidget *scroll;

	priv = BRASERO_DATA_DISC_PRIVATE (object);

	gtk_box_set_spacing (GTK_BOX (object), 0);

	/* the information displayed about how to use this tree */
	priv->notebook = brasero_disc_get_use_info_notebook ();
	gtk_widget_show (priv->notebook);
	gtk_box_pack_start (GTK_BOX (object), priv->notebook, TRUE, TRUE, 0);

	mainbox = gtk_vbox_new (FALSE, 12);
	gtk_widget_show (mainbox);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), mainbox, NULL);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), 0);

	priv->project = BRASERO_DATA_PROJECT (brasero_data_tree_model_new ());
	model = GTK_TREE_MODEL (priv->project);

	g_signal_connect (priv->project,
			  "name-collision",
			  G_CALLBACK (brasero_data_disc_name_collision_cb),
			  object);
	g_signal_connect (priv->project,
			  "2G-file",
			  G_CALLBACK (brasero_data_disc_2G_file_cb),
			  object);
	g_signal_connect (priv->project,
			  "deep-directory",
			  G_CALLBACK (brasero_data_disc_deep_directory_cb),
			  object);
	g_signal_connect (priv->project,
			  "size-changed",
			  G_CALLBACK (brasero_data_disc_size_changed_cb),
			  object);
	g_signal_connect (priv->project,
			  "project-loaded",
			  G_CALLBACK (brasero_data_disc_project_loaded_cb),
			  object);
	g_signal_connect (priv->project,
			  "row-inserted",
			  G_CALLBACK (brasero_data_disc_contents_added_cb),
			  object);
	g_signal_connect (priv->project,
			  "row-deleted",
			  G_CALLBACK (brasero_data_disc_contents_removed_cb),
			  object);

	g_signal_connect (priv->project,
			  "vfs-activity",
			  G_CALLBACK (brasero_data_disc_activity_changed_cb),
			  object);
	g_signal_connect (priv->project,
			  "filtered-uri",
			  G_CALLBACK (brasero_data_disc_filtered_uri_cb),
			  object);
	g_signal_connect (priv->project,
			  "unreadable-uri",
			  G_CALLBACK (brasero_data_disc_unreadable_uri_cb),
			  object);
	g_signal_connect (priv->project,
			  "recursive-sym",
			  G_CALLBACK (brasero_data_disc_recursive_uri_cb),
			  object);
	g_signal_connect (priv->project,
			  "unknown-uri",
			  G_CALLBACK (brasero_data_disc_unknown_uri_cb),
			  object);

	g_signal_connect (priv->project,
			  "session-available",
			  G_CALLBACK (brasero_data_disc_session_available_cb),
			  object);

	g_signal_connect (priv->project,
			  "session-loaded",
			  G_CALLBACK (brasero_data_disc_session_loaded_cb),
			  object);

	model = GTK_TREE_MODEL (priv->project);

	/* Tree */
	priv->tree = gtk_tree_view_new_with_model (model);
	g_object_unref (G_OBJECT (model));
	gtk_tree_view_set_rubber_banding (GTK_TREE_VIEW (priv->tree), TRUE);

	/* This must be before connecting to button press event */
	egg_tree_multi_drag_add_drag_support (GTK_TREE_VIEW (priv->tree));
	gtk_widget_show (priv->tree);
	g_signal_connect (priv->tree,
			  "button-press-event",
			  G_CALLBACK (brasero_data_disc_button_pressed_cb),
			  object);
	
	g_signal_connect (priv->tree,
			  "key-release-event",
			  G_CALLBACK (brasero_data_disc_key_released_cb),
			  object);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));
	g_signal_connect (selection,
			  "changed",
			  G_CALLBACK (brasero_data_disc_selection_changed_cb),
			  object);

	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	gtk_tree_selection_set_select_function (selection,
						brasero_data_disc_tree_select_function,
						NULL,
						NULL);

	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (priv->tree), TRUE);

	column = gtk_tree_view_column_new ();

	gtk_tree_view_column_set_resizable (column, TRUE);

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "icon-name", BRASERO_DATA_TREE_MODEL_MIME_ICON);

	renderer = gtk_cell_renderer_text_new ();
	g_signal_connect (G_OBJECT (renderer), "edited",
			  G_CALLBACK (brasero_data_disc_name_edited_cb), object);
	g_signal_connect (G_OBJECT (renderer), "editing-started",
			  G_CALLBACK (brasero_data_disc_name_editing_started_cb), object);
	g_signal_connect (G_OBJECT (renderer), "editing-canceled",
			  G_CALLBACK (brasero_data_disc_name_editing_canceled_cb), object);

	gtk_tree_view_column_pack_end (column, renderer, TRUE);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "text", BRASERO_DATA_TREE_MODEL_NAME);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "style", BRASERO_DATA_TREE_MODEL_STYLE);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "foreground", BRASERO_DATA_TREE_MODEL_COLOR);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "editable", BRASERO_DATA_TREE_MODEL_EDITABLE);

	g_object_set (G_OBJECT (renderer),
		      "ellipsize-set", TRUE,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      NULL);

	gtk_tree_view_column_set_title (column, _("Files"));
	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_column_set_spacing (column, 4);
	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->tree), column);
	gtk_tree_view_column_set_sort_column_id (column, BRASERO_DATA_TREE_MODEL_NAME);
	gtk_tree_view_set_expander_column (GTK_TREE_VIEW (priv->tree), column);

	/* Size column */
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);

	gtk_tree_view_column_add_attribute (column, renderer,
					    "text", BRASERO_DATA_TREE_MODEL_SIZE);
	gtk_tree_view_column_set_title (column, _("Size"));

	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->tree), column);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_expand (column, FALSE);
	gtk_tree_view_column_set_sort_column_id (column, BRASERO_DATA_TREE_MODEL_SIZE);

	/* Description */
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);

	gtk_tree_view_column_add_attribute (column, renderer,
					    "text", BRASERO_DATA_TREE_MODEL_MIME_DESC);
	gtk_tree_view_column_set_title (column, _("Description"));

	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->tree), column);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_expand (column, FALSE);
	gtk_tree_view_column_set_sort_column_id (column, BRASERO_DATA_TREE_MODEL_MIME_DESC);

	/* Space column */
	renderer = baobab_cell_renderer_progress_new ();
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);

	gtk_tree_view_column_add_attribute (column, renderer,
					    "visible", BRASERO_DATA_TREE_MODEL_SHOW_PERCENT);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "perc", BRASERO_DATA_TREE_MODEL_PERCENT);
	gtk_tree_view_column_set_title (column, _("Space"));

	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->tree), column);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_expand (column, FALSE);

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scroll);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll),
					     GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scroll), priv->tree);
	gtk_box_pack_start (GTK_BOX (mainbox), scroll, TRUE, TRUE, 0);

	/* dnd */
	gtk_tree_view_enable_model_drag_dest (GTK_TREE_VIEW
					      (priv->tree),
					      ntables_cd, nb_targets_cd,
					      GDK_ACTION_COPY |
					      GDK_ACTION_MOVE);

	gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (priv->tree),
						GDK_BUTTON1_MASK,
						ntables_source,
						nb_targets_source,
						GDK_ACTION_MOVE);

	/* FIXME: ..... */
	g_signal_connect (G_OBJECT (priv->tree),
			  "row-expanded",
			  G_CALLBACK (brasero_data_disc_row_expanded_cb),
			  object);
	g_signal_connect (G_OBJECT (priv->tree),
			  "row-collapsed",
			  G_CALLBACK (brasero_data_disc_row_collapsed_cb),
			  object);

	/* filtered files */
	priv->filter = brasero_file_filtered_new ();
	g_signal_connect (priv->filter,
			  "activate",
			  G_CALLBACK (brasero_data_disc_filter_expanded_cb),
			  object);
	gtk_widget_show (priv->filter);
	gtk_box_pack_end (GTK_BOX (mainbox), priv->filter, FALSE, TRUE, 0);

	g_signal_connect (priv->filter,
			  "filtered",
			  G_CALLBACK (brasero_data_disc_filtered_file_cb),
			  object);
	g_signal_connect (priv->filter,
			  "restored",
			  G_CALLBACK (brasero_data_disc_restored_file_cb),
			  object);
}

static void
brasero_data_disc_finalize (GObject *object)
{
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (object);

	if (priv->message) {
		g_object_unref (priv->message);
		priv->message = NULL;
	}

	if (priv->load_errors) {
		g_slist_foreach (priv->load_errors, (GFunc) g_free , NULL);
		g_slist_free (priv->load_errors);
		priv->load_errors = NULL;
	}

	G_OBJECT_CLASS (brasero_data_disc_parent_class)->finalize (object);
}

static void
brasero_data_disc_iface_disc_init (BraseroDiscIface *iface)
{
	iface->add_uri = brasero_data_disc_add_uri;
	iface->delete_selected = brasero_data_disc_delete_selected;
	iface->clear = brasero_data_disc_clear;
	iface->reset = brasero_data_disc_reset;
	iface->get_track = brasero_data_disc_get_track;
	iface->set_session_param = brasero_data_disc_set_session_param;
	iface->set_session_contents = brasero_data_disc_set_session_contents;
	iface->load_track = brasero_data_disc_load_track;
	iface->get_status = brasero_data_disc_get_status;
	iface->get_selected_uri = brasero_data_disc_get_selected_uri;
	iface->add_ui = brasero_data_disc_add_ui;
	iface->set_drive = brasero_data_disc_set_drive;
}

static void
brasero_data_disc_get_property (GObject * object,
				guint prop_id,
				GValue * value,
				GParamSpec * pspec)
{
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (object);

	switch (prop_id) {
	case PROP_REJECT_FILE:
		g_value_set_boolean (value, priv->reject_files);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
brasero_data_disc_set_property (GObject * object,
				guint prop_id,
				const GValue * value,
				GParamSpec * pspec)
{
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (object);

	switch (prop_id) {
	case PROP_REJECT_FILE:
		priv->reject_files = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
brasero_data_disc_class_init (BraseroDataDiscClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	g_type_class_add_private (klass, sizeof (BraseroDataDiscPrivate));

	object_class->finalize = brasero_data_disc_finalize;
	object_class->set_property = brasero_data_disc_set_property;
	object_class->get_property = brasero_data_disc_get_property;

	g_object_class_install_property (object_class,
					 PROP_REJECT_FILE,
					 g_param_spec_boolean
					 ("reject-file",
					  "Whether it accepts files",
					  "Whether it accepts files",
					  FALSE,
					  G_PARAM_READWRITE));
}

GtkWidget *
brasero_data_disc_new (void)
{
	return GTK_WIDGET (g_object_new (BRASERO_TYPE_DATA_DISC, NULL));
}
