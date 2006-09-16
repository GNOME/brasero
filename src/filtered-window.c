/***************************************************************************
 *            filtered-window.c
 *
 *  dim oct 30 12:25:50 2005
 *  Copyright  2005  Rouquier Philippe
 *  brasero-app@wanadoo.fr
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
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <gtk/gtkdialog.h>
#include <gtk/gtklabel.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtktreeviewcolumn.h>
#include <gtk/gtkcellrenderer.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gtk/gtkcellrenderertoggle.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkmessagedialog.h>

#include <libgnomevfs/gnome-vfs.h>

#include "filtered-window.h"
#include "utils.h"


static void brasero_filtered_dialog_class_init (BraseroFilteredDialogClass *klass);
static void brasero_filtered_dialog_init (BraseroFilteredDialog *sp);
static void brasero_filtered_dialog_finalize (GObject *object);

struct BraseroFilteredDialogPrivate {
	GtkWidget *tree;
	GtkWidget *restore_hidden;
	GtkWidget *restore_broken;

	int broken_state:1;
	int hidden_state:1;
};

enum  {
	STOCK_ID_COL,
	URI_COL,
	TYPE_COL,
	STATUS_COL,
	ACTIVABLE_COL,
	INCLUDED_COL,
	NB_COL,
};

typedef enum {
	REMOVED_SIGNAL,
	RESTORED_SIGNAL,
	LAST_SIGNAL
} BraseroFilteredDialogSignalType;

static guint brasero_filtered_dialog_signals[LAST_SIGNAL] = { 0 };
static GObjectClass *parent_class = NULL;

static void
brasero_filtered_dialog_item_toggled_cb (GtkCellRendererToggle *toggle,
					 const gchar *path,
					 BraseroFilteredDialog *dialog);
static void
brasero_filtered_dialog_restore_hidden_cb (GtkButton *button,
					   BraseroFilteredDialog *dialog);
static void
brasero_filtered_dialog_restore_broken_symlink_cb (GtkButton *button,
						   BraseroFilteredDialog *dialog);
static void
brasero_filtered_dialog_row_activated_cb (GtkTreeView *tree,
                                          GtkTreePath *path,
                                          GtkTreeViewColumn *column,
                                          BraseroFilteredDialog *dialog);

GType
brasero_filtered_dialog_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroFilteredDialogClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_filtered_dialog_class_init,
			NULL,
			NULL,
			sizeof (BraseroFilteredDialog),
			0,
			(GInstanceInitFunc)brasero_filtered_dialog_init,
		};

		type = g_type_register_static(GTK_TYPE_DIALOG, 
			"BraseroFilteredDialog", &our_info, 0);
	}

	return type;
}

static void
brasero_filtered_dialog_class_init (BraseroFilteredDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_filtered_dialog_finalize;
	
	
	brasero_filtered_dialog_signals[REMOVED_SIGNAL] =
	    g_signal_new ("removed",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_ACTION | G_SIGNAL_RUN_FIRST,
			  G_STRUCT_OFFSET (BraseroFilteredDialogClass, removed),
			  NULL, NULL,
			  g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1,
			  G_TYPE_STRING);

	brasero_filtered_dialog_signals[RESTORED_SIGNAL] =
	    g_signal_new ("restored",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_ACTION | G_SIGNAL_RUN_FIRST,
			  G_STRUCT_OFFSET (BraseroFilteredDialogClass, restored),
			  NULL, NULL,
			  g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1,
			  G_TYPE_STRING);

}

static void
brasero_filtered_dialog_init (BraseroFilteredDialog *obj)
{
	GtkWidget *box;
	GtkWidget *label;
	GtkWidget *scroll;
	GtkListStore *model;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	obj->priv = g_new0 (BraseroFilteredDialogPrivate, 1);
	gtk_window_set_title (GTK_WINDOW (obj), _("Removed files"));
	gtk_container_set_border_width (GTK_CONTAINER (GTK_BOX (GTK_DIALOG (obj)->vbox)), 16);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (obj)->vbox), 10);

	label = gtk_label_new (_("<span weight=\"bold\" size=\"larger\">The following files were removed automatically from the project.</span>"));
	g_object_set (G_OBJECT (label), "use-markup", TRUE, NULL);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (obj)->vbox),
			    label,
			    FALSE,
			    TRUE,
			    0);

	label = gtk_label_new (_("Select the files you want to restore:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (obj)->vbox),
			    label,
			    FALSE,
			    TRUE,
			    0);

	model = gtk_list_store_new (NB_COL,
				    G_TYPE_STRING,
				    G_TYPE_STRING,
				    G_TYPE_STRING,
				    G_TYPE_INT,
				    G_TYPE_BOOLEAN,
				    G_TYPE_BOOLEAN);

	obj->priv->tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (obj->priv->tree), TRUE);
	g_object_unref (model);

	g_signal_connect (G_OBJECT (obj->priv->tree), "row-activated",
			  G_CALLBACK (brasero_filtered_dialog_row_activated_cb), obj);

	column = gtk_tree_view_column_new ();
	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "stock-id", STOCK_ID_COL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_end (column, renderer, TRUE);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "text", URI_COL);
	gtk_tree_view_column_set_title (column, _("Files"));
	gtk_tree_view_append_column (GTK_TREE_VIEW (obj->priv->tree), column);
	gtk_tree_view_column_set_sort_column_id (column, URI_COL);
	gtk_tree_view_column_set_clickable (column, TRUE);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_min_width (column, 450);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Type"), renderer,
							   "text", TYPE_COL, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (obj->priv->tree), column);
	gtk_tree_view_column_set_sort_column_id (column, TYPE_COL);
	gtk_tree_view_column_set_clickable (column, TRUE);

	renderer = gtk_cell_renderer_toggle_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Status"), renderer,
							   "inconsistent", ACTIVABLE_COL,
							   "active", INCLUDED_COL, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (obj->priv->tree), column);
	gtk_tree_view_column_set_sort_column_id (column, INCLUDED_COL);
	gtk_tree_view_column_set_clickable (column, TRUE);

	g_signal_connect (G_OBJECT (renderer), "toggled",
			  G_CALLBACK (brasero_filtered_dialog_item_toggled_cb), obj);

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll),
					     GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scroll), obj->priv->tree);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (obj)->vbox),
			    scroll,
			    TRUE,
			    TRUE,
			    0);

	box = gtk_hbox_new (FALSE, 8);
	obj->priv->restore_hidden = gtk_button_new_with_label (_("Restore hidden files"));
	gtk_widget_set_sensitive (obj->priv->restore_hidden, FALSE);
	g_signal_connect (G_OBJECT (obj->priv->restore_hidden),
			  "clicked",
			  G_CALLBACK (brasero_filtered_dialog_restore_hidden_cb),
			  obj);
	gtk_box_pack_start (GTK_BOX (box), obj->priv->restore_hidden, FALSE, FALSE, 0);

	obj->priv->restore_broken = gtk_button_new_with_label (_("Restore broken symlink"));
	gtk_widget_set_sensitive (obj->priv->restore_broken, FALSE);
	g_signal_connect (G_OBJECT (obj->priv->restore_broken),
			  "clicked",
			  G_CALLBACK (brasero_filtered_dialog_restore_broken_symlink_cb),
			  obj);
	gtk_box_pack_start (GTK_BOX (box), obj->priv->restore_broken, FALSE, FALSE, 0);

	gtk_box_pack_end (GTK_BOX (GTK_DIALOG (obj)->vbox),
			  box,
			  FALSE,
			  FALSE,
			  0);

	gtk_dialog_add_button (GTK_DIALOG (obj),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (obj),
				GTK_STOCK_OK, GTK_RESPONSE_OK);

	gtk_dialog_set_has_separator (GTK_DIALOG (obj), TRUE);
}

static void
brasero_filtered_dialog_finalize (GObject *object)
{
	BraseroFilteredDialog *cobj;

	cobj = BRASERO_FILTERED_DIALOG (object);
	g_free (cobj->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkWidget *
brasero_filtered_dialog_new ()
{
	BraseroFilteredDialog *obj;
	
	obj = BRASERO_FILTERED_DIALOG (g_object_new (BRASERO_TYPE_FILTERED_DIALOG, NULL));
	gtk_window_set_default_size (GTK_WINDOW (obj), 640, 500);
	return GTK_WIDGET (obj);
}

void
brasero_filtered_dialog_add (BraseroFilteredDialog *dialog,
			     const char *uri,
			     gboolean restored,
			     BraseroFilterStatus status)
{
	char *labels [] = { N_("hidden file"),
			    N_("unreadable file"),
			    N_("broken symlink"),
			    N_("recursive symlink"),
			    NULL };
	const gchar *stock_id;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *type;

	type = labels [ status - 1 ];
	if (status == BRASERO_FILTER_UNREADABLE)
		stock_id = GTK_STOCK_CANCEL;
	else
		stock_id = NULL;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->tree));
	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			    STOCK_ID_COL, stock_id,
			    URI_COL, uri,
			    TYPE_COL, _(type),
			    STATUS_COL, status,
			    INCLUDED_COL, restored,
			    ACTIVABLE_COL, (status == BRASERO_FILTER_UNREADABLE || status == BRASERO_FILTER_RECURSIVE_SYM),
			    -1);

	if (status == BRASERO_FILTER_HIDDEN)
		gtk_widget_set_sensitive (dialog->priv->restore_hidden, TRUE);

	if (status == BRASERO_FILTER_BROKEN_SYM)
		gtk_widget_set_sensitive (dialog->priv->restore_broken, TRUE);
}

void
brasero_filtered_dialog_get_status (BraseroFilteredDialog *dialog,
				    GSList **restored,
				    GSList **removed)
{
	BraseroFilterStatus status;
	GSList *retval_restored;
	GSList *retval_removed;
	GtkTreeModel *model;
	gboolean included;
	GtkTreeIter iter;
	char *uri;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->tree));
	if (!gtk_tree_model_get_iter_first (model, &iter))
		return;

	retval_restored = NULL;
	retval_removed = NULL;
	do {
		gtk_tree_model_get (model, &iter,
				    URI_COL, &uri,
				    STATUS_COL, &status,
				    INCLUDED_COL, &included, -1);

		if (status == BRASERO_FILTER_UNREADABLE
		||  status == BRASERO_FILTER_RECURSIVE_SYM) {
			g_free (uri);
			continue;
		}

		if (included)
			retval_restored = g_slist_prepend (retval_restored, uri);
		else
			retval_removed = g_slist_prepend (retval_removed, uri);
	} while (gtk_tree_model_iter_next (model, &iter));

	*restored = retval_restored;
	*removed = retval_removed;
}

static void
brasero_filtered_dialog_item_state_changed (BraseroFilteredDialog *dialog,
				            const GtkTreePath *path)
{
	BraseroFilterStatus status;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean active;
	char *uri;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->tree));
	gtk_tree_model_get_iter (model, &iter, (GtkTreePath*) path);

	gtk_tree_model_get (model, &iter,
			    URI_COL, &uri,
			    STATUS_COL, &status,
			    INCLUDED_COL, &active, -1);

	if (active) { /* (RE) EXCLUDE */
		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				    INCLUDED_COL, FALSE, -1);
	}
	else { /* RESTORE */
		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				    INCLUDED_COL, TRUE, -1);
	}

	g_free (uri);
}

static void
brasero_filtered_dialog_item_toggled_cb (GtkCellRendererToggle *toggle,
					 const gchar *path,
					 BraseroFilteredDialog *dialog)
{
	GtkTreePath *treepath;

	treepath = gtk_tree_path_new_from_string (path);
	brasero_filtered_dialog_item_state_changed (dialog, treepath);
	gtk_tree_path_free (treepath);
}

static void
brasero_filtered_dialog_restore_all (BraseroFilteredDialog *dialog,
				     BraseroFilterStatus status)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	BraseroFilterStatus row_status;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->tree));
	if (!gtk_tree_model_get_iter_first (model, &iter))
		return;

	do {
		gtk_tree_model_get (model, &iter,
				    STATUS_COL, &row_status, -1);

		if (status == row_status) {
			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					    INCLUDED_COL, TRUE, -1);
		}

	} while (gtk_tree_model_iter_next (model, &iter));
}

static void
brasero_filtered_dialog_exclude_all (BraseroFilteredDialog *dialog,
				     BraseroFilterStatus status)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	BraseroFilterStatus row_status;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->tree));
	if (!gtk_tree_model_get_iter_first (model, &iter))
		return;

	do {
		gtk_tree_model_get (model, &iter,
				    STATUS_COL, &row_status, -1);

		if (status == row_status)
			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					    INCLUDED_COL, FALSE, -1);

	} while (gtk_tree_model_iter_next (model, &iter));
}

static void
brasero_filtered_dialog_restore_hidden_cb (GtkButton *button,
					   BraseroFilteredDialog *dialog)
{
	if (!dialog->priv->hidden_state) {
		gtk_button_set_label (button, _("Exclude hidden files"));
		brasero_filtered_dialog_restore_all (dialog, BRASERO_FILTER_HIDDEN);
		dialog->priv->hidden_state = 1;
	}
	else {
		gtk_button_set_label (button, _("Restore hidden files"));
		brasero_filtered_dialog_exclude_all (dialog, BRASERO_FILTER_HIDDEN);
		dialog->priv->hidden_state = 0;
	}
}

static void
brasero_filtered_dialog_restore_broken_symlink_cb (GtkButton *button,
						   BraseroFilteredDialog *dialog)
{
	if (!dialog->priv->broken_state) {
		gtk_button_set_label (button, _("Exclude broken symlinks"));
		brasero_filtered_dialog_restore_all (dialog, BRASERO_FILTER_BROKEN_SYM);
		dialog->priv->broken_state = 1;
	}
	else {
		gtk_button_set_label (button, _("Restore broken symlinks"));
		brasero_filtered_dialog_exclude_all (dialog, BRASERO_FILTER_BROKEN_SYM);
		dialog->priv->broken_state = 0;
	}
}

static void
brasero_filtered_dialog_row_activated_cb (GtkTreeView *tree,
                                          GtkTreePath *path,
                                          GtkTreeViewColumn *column,
                                          BraseroFilteredDialog *dialog)
{
	brasero_filtered_dialog_item_state_changed (dialog, path);
}
