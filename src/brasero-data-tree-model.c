/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * trunk
 * Copyright (C) Philippe Rouquier 2007 <bonfire-app@wanadoo.fr>
 * 
 * trunk is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * trunk is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with trunk.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtktreemodel.h>
#include <gtk/gtktreednd.h>
#include <gtk/gtktreesortable.h>
#include <gtk/gtkicontheme.h>

#include <libgnomeui/libgnomeui.h>

#include <libgnomevfs/gnome-vfs-mime-handlers.h>

#include "burn-basics.h"

#include "brasero-data-tree-model.h"
#include "brasero-data-project.h"
#include "brasero-data-vfs.h"
#include "brasero-file-node.h"
#include "brasero-utils.h"

#include "eggtreemultidnd.h"

typedef struct _BraseroDataTreeModelPrivate BraseroDataTreeModelPrivate;
struct _BraseroDataTreeModelPrivate
{
	guint stamp;

	GSList *shown;

	gint sort_column;
	GtkSortType sort_type;
};

#define BRASERO_DATA_TREE_MODEL_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_DATA_TREE_MODEL, BraseroDataTreeModelPrivate))

typedef enum {
	BRASERO_ROW_REGULAR		= 0,
	BRASERO_ROW_BOGUS
} BraseroFileRowType;

static void
brasero_data_tree_model_multi_drag_source_iface_init (gpointer g_iface, gpointer data);
static void
brasero_data_tree_model_drag_source_iface_init (gpointer g_iface, gpointer data);
static void
brasero_data_tree_model_drag_dest_iface_init (gpointer g_iface, gpointer data);
static void
brasero_data_tree_model_sortable_iface_init (gpointer g_iface, gpointer data);
static void
brasero_data_tree_model_iface_init (gpointer g_iface, gpointer data);

G_DEFINE_TYPE_WITH_CODE (BraseroDataTreeModel,
			 brasero_data_tree_model,
			 BRASERO_TYPE_DATA_VFS,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
					        brasero_data_tree_model_iface_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_DEST,
					        brasero_data_tree_model_drag_dest_iface_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE,
					        brasero_data_tree_model_drag_source_iface_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_SORTABLE,
						brasero_data_tree_model_sortable_iface_init)
			 G_IMPLEMENT_INTERFACE (EGG_TYPE_TREE_MULTI_DRAG_SOURCE,
					        brasero_data_tree_model_multi_drag_source_iface_init));


static gboolean
brasero_data_tree_model_iter_parent (GtkTreeModel *model,
				     GtkTreeIter *iter,
				     GtkTreeIter *child)
{
	BraseroDataTreeModelPrivate *priv;
	BraseroFileNode *node;

	priv = BRASERO_DATA_TREE_MODEL_PRIVATE (model);

	/* make sure that iter comes from us */
	g_return_val_if_fail (priv->stamp == child->stamp, FALSE);
	g_return_val_if_fail (child->user_data != NULL, FALSE);

	node = child->user_data;
	if (GPOINTER_TO_INT (child->user_data2) == BRASERO_ROW_BOGUS) {
		/* This is a bogus row intended for empty directories
		 * user_data has the parent empty directory. */
		iter->user_data2 = GINT_TO_POINTER (BRASERO_ROW_REGULAR);
		iter->user_data = child->user_data;
		iter->stamp = priv->stamp;
		return TRUE;
	}

	if (!node->parent) {
		iter->user_data = NULL;
		return FALSE;
	}

	iter->stamp = priv->stamp;
	iter->user_data = node->parent;
	iter->user_data2 = GINT_TO_POINTER (BRASERO_ROW_REGULAR);
	return TRUE;
}

static gboolean
brasero_data_tree_model_iter_nth_child (GtkTreeModel *model,
					GtkTreeIter *iter,
					GtkTreeIter *parent,
					gint n)
{
	BraseroDataTreeModelPrivate *priv;
	BraseroFileNode *node;

	priv = BRASERO_DATA_TREE_MODEL_PRIVATE (model);

	if (parent) {
		/* make sure that iter comes from us */
		g_return_val_if_fail (priv->stamp == parent->stamp, FALSE);
		g_return_val_if_fail (parent->user_data != NULL, FALSE);

		if (GPOINTER_TO_INT (parent->user_data2) == BRASERO_ROW_BOGUS) {
			/* This is a bogus row intended for empty directories,
			 * it hasn't got children. */
			return FALSE;
		}

		node = parent->user_data;
	}
	else
		node = brasero_data_project_get_root (BRASERO_DATA_PROJECT (model));

	iter->user_data = brasero_file_node_nth_child (node, n);
	if (!iter->user_data)
		return FALSE;

	iter->stamp = priv->stamp;
	iter->user_data2 = GINT_TO_POINTER (BRASERO_ROW_REGULAR);
	return TRUE;
}

static gint
brasero_data_tree_model_iter_n_children (GtkTreeModel *model,
					 GtkTreeIter *iter)
{
	BraseroDataTreeModelPrivate *priv;
	BraseroFileNode *node;

	priv = BRASERO_DATA_TREE_MODEL_PRIVATE (model);

	if (iter == NULL) {
		/* special case */
		node = brasero_data_project_get_root (BRASERO_DATA_PROJECT (model));
		return brasero_file_node_get_n_children (node);
	}

	/* make sure that iter comes from us */
	g_return_val_if_fail (priv->stamp == iter->stamp, 0);
	g_return_val_if_fail (iter->user_data != NULL, 0);

	if (GPOINTER_TO_INT (iter->user_data2) == BRASERO_ROW_BOGUS)
		return 0;

	node = iter->user_data;
	if (node->is_file)
		return 0;

	/* return at least one for the bogus row labelled "empty". */
	if (!BRASERO_FILE_NODE_CHILDREN (node))
		return 1;

	return brasero_file_node_get_n_children (node);
}

static gboolean
brasero_data_tree_model_iter_has_child (GtkTreeModel *model,
					GtkTreeIter *iter)
{
	BraseroDataTreeModelPrivate *priv;
	BraseroFileNode *node;

	priv = BRASERO_DATA_TREE_MODEL_PRIVATE (model);

	/* make sure that iter comes from us */
	g_return_val_if_fail (priv->stamp == iter->stamp, FALSE);
	g_return_val_if_fail (iter->user_data != NULL, FALSE);

	if (GPOINTER_TO_INT (iter->user_data2) == BRASERO_ROW_BOGUS) {
		/* This is a bogus row intended for empty directories
		 * it hasn't got children */
		return FALSE;
	}

	node = iter->user_data;
	if (node->is_file)
		return FALSE;

	if (!BRASERO_FILE_NODE_CHILDREN (node)) {
		/* It has children but only a bogus one. */
		return TRUE;
	}

	/* always return TRUE here when it's a directory since even if
	 * it's empty we'll add a row written empty underneath it
	 * anyway. */
	return TRUE;
}

static gboolean
brasero_data_tree_model_iter_children (GtkTreeModel *model,
				       GtkTreeIter *iter,
				       GtkTreeIter *parent)
{
	BraseroDataTreeModelPrivate *priv;
	BraseroFileNode *node;

	priv = BRASERO_DATA_TREE_MODEL_PRIVATE (model);

	/* make sure that iter comes from us */
	g_return_val_if_fail (priv->stamp == parent->stamp, FALSE);
	g_return_val_if_fail (parent->user_data != NULL, FALSE);

	if (GPOINTER_TO_INT (parent->user_data2) == BRASERO_ROW_BOGUS) {
		iter->user_data = NULL;
		return FALSE;
	}

	node = parent->user_data;
	if (node->is_file) {
		iter->user_data = NULL;
		return FALSE;
	}

	iter->stamp = priv->stamp;
	if (!BRASERO_FILE_NODE_CHILDREN (node)) {
		/* This is a directory but it hasn't got any child; yet
		 * we show a row written empty for that. Set bogus in
		 * user_data and put parent in user_data. */
		iter->user_data = parent->user_data;
		iter->user_data2 = GINT_TO_POINTER (BRASERO_ROW_BOGUS);
		return TRUE;
	}

	iter->user_data = BRASERO_FILE_NODE_CHILDREN (node);
	iter->user_data2 = GINT_TO_POINTER (BRASERO_ROW_REGULAR);
	return TRUE;
}

static gboolean
brasero_data_tree_model_iter_next (GtkTreeModel *model,
				   GtkTreeIter *iter)
{
	BraseroDataTreeModelPrivate *priv;
	BraseroFileNode *node;

	priv = BRASERO_DATA_TREE_MODEL_PRIVATE (model);

	/* make sure that iter comes from us */
	g_return_val_if_fail (priv->stamp == iter->stamp, FALSE);
	g_return_val_if_fail (iter->user_data != NULL, FALSE);

	if (GPOINTER_TO_INT (iter->user_data2) == BRASERO_ROW_BOGUS) {
		/* This is a bogus row intended for empty directories
		 * user_data has the parent empty directory. It hasn't
		 * got any peer.*/
		iter->user_data = NULL;
		return FALSE;
	}

	node = iter->user_data;
	iter->user_data = node->next;

	if (!node->next)
		return FALSE;

	return TRUE;
}

static void
brasero_data_tree_model_node_shown (GtkTreeModel *model,
				    GtkTreeIter *iter)
{
	BraseroFileNode *node;
	BraseroDataTreeModelPrivate *priv;

	priv = BRASERO_DATA_TREE_MODEL_PRIVATE (model);
	node = iter->user_data;

	/* Check if that's a BOGUS row. In this case that means the parent was
	 * expanded. Therefore ask vfs to increase its priority if it's loading
	 * its contents. */
	if (GPOINTER_TO_INT (iter->user_data2) == BRASERO_ROW_BOGUS) {
		/* NOTE: this has to be a directory */
		/* NOTE: there is no need to check for is_loading case here
		 * since before showing its BOGUS row the tree will have shown
		 * its parent itself and therefore that's the cases that follow
		 */
		if (node->is_exploring) {
			/* the directory is being explored increase priority */
			brasero_data_vfs_require_directory_contents (BRASERO_DATA_VFS (model), node);
		}

		/* Otherwise, that's simply a BOGUS row and its parent was
		 * loaded but it is empty. Nothing to do. */

		return;
	}

	if (!node)
		return;

	node->is_visible ++;

	if (node->is_imported)
		return;

	if (node->is_visible > 1)
		return;

	/* NOTE: no need to see if that's a directory being explored here. If it
	 * is being explored then it has a BOGUS row and that's the above case 
	 * that is reached. */
	if (node->is_loading) {
		/* in this case have vfs to increase priority for this node */
		brasero_data_vfs_require_node_load (BRASERO_DATA_VFS (model), node);
	}
	else if (!BRASERO_FILE_NODE_MIME (node)) {
		/* that means that file wasn't completly loaded. To save
		 * some time we delayed the detection of the mime type
		 * since that takes a lot of time. */
		brasero_data_vfs_load_mime (BRASERO_DATA_VFS (model), node);
	}

	/* add the node to the visible list that is used to update the disc 
	 * share for the node (we don't want to update the whole tree).
	 * Moreover, we only want files since directories don't have space. */
	priv->shown = g_slist_prepend (priv->shown, node);
}

static void
brasero_data_tree_model_node_hidden (GtkTreeModel *model,
				     GtkTreeIter *iter)
{
	BraseroFileNode *node;
	BraseroDataTreeModelPrivate *priv;

	/* if it's a BOGUS row stop here since they are not added to shown list.
	 * In the same way returns if it is a file. */
	if (GPOINTER_TO_INT (iter->user_data2) == BRASERO_ROW_BOGUS)
		return;

	node = iter->user_data;

	if (!node)
		return;

	node->is_visible --;

	if (node->is_imported)
		return;

	priv = BRASERO_DATA_TREE_MODEL_PRIVATE (model);

	/* update shown list */
	if (!node->is_visible)
		priv->shown = g_slist_remove (priv->shown, node);
}

static void
brasero_data_tree_model_get_value (GtkTreeModel *model,
				   GtkTreeIter *iter,
				   gint column,
				   GValue *value)
{
	BraseroDataTreeModelPrivate *priv;
	BraseroDataTreeModel *self;
	BraseroFileNode *node;

	self = BRASERO_DATA_TREE_MODEL (model);
	priv = BRASERO_DATA_TREE_MODEL_PRIVATE (model);

	/* make sure that iter comes from us */
	g_return_if_fail (priv->stamp == iter->stamp);
	g_return_if_fail (iter->user_data != NULL);

	node = iter->user_data;

	if (GPOINTER_TO_INT (iter->user_data2) == BRASERO_ROW_BOGUS) {
		switch (column) {
		case BRASERO_DATA_TREE_MODEL_NAME:
			g_value_init (value, G_TYPE_STRING);
			if (node->is_exploring)
				g_value_set_string (value, _("(loading ...)"));
			else
				g_value_set_string (value, _("empty"));

			return;

		case BRASERO_DATA_TREE_MODEL_MIME_DESC:
		case BRASERO_DATA_TREE_MODEL_MIME_ICON:
		case BRASERO_DATA_TREE_MODEL_SIZE:
			g_value_init (value, G_TYPE_STRING);
			g_value_set_string (value, NULL);
			return;

		case BRASERO_DATA_TREE_MODEL_SHOW_PERCENT:
			g_value_init (value, G_TYPE_BOOLEAN);
			g_value_set_boolean (value, FALSE);
			return;

		case BRASERO_DATA_TREE_MODEL_PERCENT:
			g_value_init (value, G_TYPE_INT);
			g_value_set_int (value, 0);
			return;

		case BRASERO_DATA_TREE_MODEL_STYLE:
			g_value_init (value, PANGO_TYPE_STYLE);
			g_value_set_enum (value, PANGO_STYLE_ITALIC);
			return;

		case BRASERO_DATA_TREE_MODEL_EDITABLE:
			g_value_init (value, G_TYPE_BOOLEAN);
			g_value_set_boolean (value, FALSE);
			return;

		default:
			return;
		}

		return;
	}

	switch (column) {
	case BRASERO_DATA_TREE_MODEL_EDITABLE:
		g_value_init (value, G_TYPE_BOOLEAN);
		g_value_set_boolean (value, (node->is_imported == FALSE) && node->is_selected);
		return;

	case BRASERO_DATA_TREE_MODEL_NAME:
		g_value_init (value, G_TYPE_STRING);

		/* we may have to set some markup on it */
		if (node->is_imported) {
			gchar *markup;

			/* italics and small difference in colour */
			markup = g_strdup_printf ("<span foreground='grey50'>%s</span>",
						  BRASERO_FILE_NODE_NAME (node));

			g_value_set_string (value, markup);
			g_free (markup);
		}
		else
			g_value_set_string (value, BRASERO_FILE_NODE_NAME (node));

		return;

	case BRASERO_DATA_TREE_MODEL_MIME_DESC:
		g_value_init (value, G_TYPE_STRING);
		if (node->is_loading)
			g_value_set_string (value, _("(loading ...)"));
		else if (!node->is_file)
			g_value_set_string (value, gnome_vfs_mime_get_description ("x-directory/normal"));
		else if (node->is_imported)
			g_value_set_string (value, _("Disc file"));
		else if (!BRASERO_FILE_NODE_MIME (node))
			g_value_set_string (value, _("(loading ...)"));
		else
			g_value_set_string (value, gnome_vfs_mime_get_description (BRASERO_FILE_NODE_MIME (node)));

		return;

	case BRASERO_DATA_TREE_MODEL_MIME_ICON:
		g_value_init (value, G_TYPE_STRING);
		if (node->is_loading)
			g_value_set_string (value, "image-loading");
		else if (!node->is_file) {
			/* Here we have two states collapsed and expanded */
			if (node->is_expanded)
				g_value_set_string (value, "folder-open");
			else if (node->is_imported)
				/* that's for all the imported folders */
				g_value_set_string (value, "folder-visiting");
			else
				g_value_set_string (value, "folder");
		}
		else if (node->is_imported) {
			g_value_set_string (value, "media-cdrom");
		}
		else {
			gchar *icon_string;

			icon_string = gnome_icon_lookup (gtk_icon_theme_get_default (), NULL,
							 NULL, NULL, NULL, BRASERO_FILE_NODE_MIME (node),
							 GNOME_ICON_LOOKUP_FLAGS_NONE, NULL);
			g_value_set_string (value, icon_string);
			g_free (icon_string);
		}

		return;

	case BRASERO_DATA_TREE_MODEL_SIZE:
		g_value_init (value, G_TYPE_STRING);
		if (node->is_loading)
			g_value_set_string (value, _("(loading ...)"));
		else if (!node->is_file) {
			guint nb_items;

			if (node->is_exploring) {
				g_value_set_string (value, _("(loading ...)"));
				return;
			}

			nb_items = brasero_file_node_get_n_children (node);
			if (!nb_items)
				g_value_set_string (value, _("empty"));
			else if (nb_items == 1)
				g_value_set_string (value, _("1 item"));
			else {
				gchar *text;

				text = g_strdup_printf (ngettext ("%d item", "%d items", nb_items), nb_items);
				g_value_set_string (value, text);
				g_free (text);
			}
		}
		else {
			gchar *text;

			text = brasero_utils_get_size_string (BRASERO_FILE_NODE_SECTORS (node) * 2048, TRUE, TRUE);
			g_value_set_string (value, text);
			g_free (text);
		}

		return;

	case BRASERO_DATA_TREE_MODEL_SHOW_PERCENT:
		g_value_init (value, G_TYPE_BOOLEAN);
		if (node->is_imported || node->is_loading)
			g_value_set_boolean (value, FALSE);
		else
			g_value_set_boolean (value, TRUE);

		return;

	case BRASERO_DATA_TREE_MODEL_PERCENT:
		g_value_init (value, G_TYPE_INT);
		if (!node->is_imported && !brasero_data_vfs_is_active (BRASERO_DATA_VFS (self))) {
			gint64 size;
			guint node_size;

			size = brasero_data_project_get_size (BRASERO_DATA_PROJECT (self));

			if (!node->is_file)
				node_size = brasero_data_project_get_folder_size (BRASERO_DATA_PROJECT (self), node);
			else
				node_size = BRASERO_FILE_NODE_SECTORS (node);
			g_value_set_int (value, node_size * 100 / size);
		}
		else
			g_value_set_int (value, 0);

		return;

	case BRASERO_DATA_TREE_MODEL_STYLE:
		g_value_init (value, PANGO_TYPE_STYLE);
		if (node->is_imported)
			g_value_set_enum (value, PANGO_STYLE_ITALIC);

		return;

	default:
		return;
	}

	return;
}

GtkTreePath *
brasero_data_tree_model_node_to_path (BraseroDataTreeModel *self,
				      BraseroFileNode *node)
{
	BraseroDataTreeModelPrivate *priv;
	GtkTreePath *path;

	priv = BRASERO_DATA_TREE_MODEL_PRIVATE (self);

	path = gtk_tree_path_new ();
	for (; node->parent && !node->is_root; node = node->parent) {
		guint nth;

		nth = brasero_file_node_get_pos_as_child (node);
		gtk_tree_path_prepend_index (path, nth);
	}

	return path;
}

static GtkTreePath *
brasero_data_tree_model_get_path (GtkTreeModel *model,
				  GtkTreeIter *iter)
{
	BraseroDataTreeModelPrivate *priv;
	BraseroFileNode *node;
	GtkTreePath *path;

	priv = BRASERO_DATA_TREE_MODEL_PRIVATE (model);

	/* make sure that iter comes from us */
	g_return_val_if_fail (priv->stamp == iter->stamp, NULL);
	g_return_val_if_fail (iter->user_data != NULL, NULL);

	node = iter->user_data;

	/* NOTE: there is only one single node without a name: root */
	path = gtk_tree_path_new ();
	for (; node->parent && BRASERO_FILE_NODE_NAME (node); node = node->parent) {
		guint nth;

		nth = brasero_file_node_get_pos_as_child (node);
		gtk_tree_path_prepend_index (path, nth);
	}

	/* Add index 0 for empty bogus row */
	if (GPOINTER_TO_INT (iter->user_data2) == BRASERO_ROW_BOGUS)
		gtk_tree_path_append_index (path, 0);

	return path;
}

BraseroFileNode *
brasero_data_tree_model_path_to_node (BraseroDataTreeModel *self,
				      GtkTreePath *path)
{
	BraseroDataTreeModelPrivate *priv;
	BraseroFileNode *node;
	gint *indices;
	guint depth;
	guint i;

	priv = BRASERO_DATA_TREE_MODEL_PRIVATE (self);

	indices = gtk_tree_path_get_indices (path);
	depth = gtk_tree_path_get_depth (path);

	node = brasero_data_project_get_root (BRASERO_DATA_PROJECT (self));
	for (i = 0; i < depth; i ++) {
		BraseroFileNode *parent;

		parent = node;
		node = brasero_file_node_nth_child (parent, indices [i]);
		if (!node)
			return NULL;
	}

	return node;
}

static gboolean
brasero_data_tree_model_get_iter (GtkTreeModel *model,
				  GtkTreeIter *iter,
				  GtkTreePath *path)
{
	BraseroDataTreeModelPrivate *priv;
	BraseroFileNode *root;
	BraseroFileNode *node;
	const gint *indices;
	guint depth;
	guint i;

	priv = BRASERO_DATA_TREE_MODEL_PRIVATE (model);

	indices = gtk_tree_path_get_indices (path);
	depth = gtk_tree_path_get_depth (path);

	root = brasero_data_project_get_root (BRASERO_DATA_PROJECT (model));
	node = brasero_file_node_nth_child (root, indices [0]);
	if (!node)
		return FALSE;

	for (i = 1; i < depth; i ++) {
		BraseroFileNode *parent;

		parent = node;
		node = brasero_file_node_nth_child (parent, indices [i]);
		if (!node) {
			/* There is one case where this can happen and
			 * is allowed: that's when the parent is an
			 * empty directory. Then index must be 0. */
			if (!parent->is_file
			&&  !BRASERO_FILE_NODE_CHILDREN (parent)
			&&   indices [i] == 0) {
				iter->stamp = priv->stamp;
				iter->user_data = parent;
				iter->user_data2 = GINT_TO_POINTER (BRASERO_ROW_BOGUS);
				return TRUE;
			}

			iter->user_data = NULL;
			return FALSE;
		}
	}

	iter->user_data2 = GINT_TO_POINTER (BRASERO_ROW_REGULAR);
	iter->stamp = priv->stamp;
	iter->user_data = node;

	return TRUE;
}

static GType
brasero_data_tree_model_get_column_type (GtkTreeModel *model,
					 gint index)
{
	switch (index) {
	case BRASERO_DATA_TREE_MODEL_NAME:
		return G_TYPE_STRING;

	case BRASERO_DATA_TREE_MODEL_MIME_DESC:
		return G_TYPE_STRING;

	case BRASERO_DATA_TREE_MODEL_MIME_ICON:
		return G_TYPE_STRING;

	case BRASERO_DATA_TREE_MODEL_SIZE:
		return G_TYPE_STRING;

	case BRASERO_DATA_TREE_MODEL_SHOW_PERCENT:
		return G_TYPE_BOOLEAN;

	case BRASERO_DATA_TREE_MODEL_PERCENT:
		return G_TYPE_INT;

	case BRASERO_DATA_TREE_MODEL_STYLE:
		return PANGO_TYPE_STYLE;

	case BRASERO_DATA_TREE_MODEL_EDITABLE:
		return G_TYPE_BOOLEAN;

	default:
		break;
	}

	return G_TYPE_INVALID;
}

static gint
brasero_data_tree_model_get_n_columns (GtkTreeModel *model)
{
	return BRASERO_DATA_TREE_MODEL_COL_NUM;
}

static GtkTreeModelFlags
brasero_data_tree_model_get_flags (GtkTreeModel *model)
{
	return 0;
}

static gboolean
brasero_data_tree_model_multi_row_draggable (EggTreeMultiDragSource *drag_source,
					     GList *path_list)
{
	GList *iter;

	for (iter = path_list; iter && iter->data; iter = iter->next) {
		GtkTreeRowReference *reference;
		BraseroFileNode *node;
		GtkTreePath *treepath;

		reference = iter->data;
		treepath = gtk_tree_row_reference_get_path (reference);
		node = brasero_data_tree_model_path_to_node (BRASERO_DATA_TREE_MODEL (drag_source), treepath);
		gtk_tree_path_free (treepath);

		/* at least one row must not be an imported row. */
		if (node && !node->is_imported)
			return TRUE;
	}

	return FALSE;
}

static gboolean
brasero_data_tree_model_multi_drag_data_get (EggTreeMultiDragSource *drag_source,
					     GList *path_list,
					     GtkSelectionData *selection_data)
{
	if (selection_data->target == gdk_atom_intern (BRASERO_DND_TARGET_SELF_FILE_NODES, TRUE)) {
		BraseroDNDDataContext context;

		context.model = GTK_TREE_MODEL (drag_source);
		context.references = path_list;

		gtk_selection_data_set (selection_data,
					gdk_atom_intern_static_string (BRASERO_DND_TARGET_SELF_FILE_NODES),
					8,
					(void *) &context,
					sizeof (context));
	}
	else
		return FALSE;

	return TRUE;
}

static gboolean
brasero_data_tree_model_multi_drag_data_delete (EggTreeMultiDragSource *drag_source,
						GList *path_list)
{
	/* NOTE: it's not the data in the selection_data here that should be
	 * deleted but rather the rows selected when there is a move. FALSE
	 * here means that we didn't delete anything. */
	/* return TRUE to stop other handlers */
	return TRUE;
}

static gboolean
brasero_data_tree_model_drag_data_received (GtkTreeDragDest *drag_dest,
					    GtkTreePath *dest_path,
					    GtkSelectionData *selection_data)
{
	BraseroFileNode *node;
	BraseroFileNode *parent;
	GtkTreePath *dest_parent;
	BraseroDataTreeModel *self;

	self = BRASERO_DATA_TREE_MODEL (drag_dest);

	/* NOTE: dest_path is the path to insert before; so we may not have a 
	 * valid path if it's in an empty directory */

	dest_parent = gtk_tree_path_copy (dest_path);
	gtk_tree_path_up (dest_parent);
	parent = brasero_data_tree_model_path_to_node (self, dest_parent);
	if (!parent) {
		gtk_tree_path_up (dest_parent);
		parent = brasero_data_tree_model_path_to_node (self, dest_parent);
	}
	else if (parent->is_file)
		parent = parent->parent;

	gtk_tree_path_free (dest_parent);

	/* Received data: see where it comes from:
	 * - from us, then that's a simple move
	 * - from another widget then it's going to be URIS and we add
	 *   them to the DataProject */
	if (selection_data->target == gdk_atom_intern (BRASERO_DND_TARGET_SELF_FILE_NODES, TRUE)) {
		BraseroDNDDataContext *context;
		GList *iter;

		context = (BraseroDNDDataContext *) selection_data->data;
		if (context->model != GTK_TREE_MODEL (drag_dest))
			return TRUE;

		/* That's us: move the row and its children. */
		for (iter = context->references; iter; iter = iter->next) {
			GtkTreeRowReference *reference;
			GtkTreePath *treepath;

			reference = iter->data;
			treepath = gtk_tree_row_reference_get_path (reference);

			node = brasero_data_tree_model_path_to_node (BRASERO_DATA_TREE_MODEL (drag_dest), treepath);
			gtk_tree_path_free (treepath);

			brasero_data_project_move_node (BRASERO_DATA_PROJECT (self), node, parent);
		}
	}
	else if (selection_data->target == gdk_atom_intern ("text/uri-list", TRUE)) {
		gint i;
		gchar **uris;
		gboolean success = FALSE;

		/* NOTE: there can be many URIs at the same time. One
		 * success is enough to return TRUE. */
		success = FALSE;
		uris = gtk_selection_data_get_uris (selection_data);
		if (!uris)
			return TRUE;

		for (i = 0; uris [i]; i ++) {
			BraseroFileNode *node;

			/* Add the URIs to the project */
			node = brasero_data_project_add_loading_node (BRASERO_DATA_PROJECT (self),
								      uris [i],
								      parent);
			if (node)
				success = TRUE;
		}
		g_strfreev (uris);
	}
	else
		return FALSE;

	return TRUE;
}

static gboolean
brasero_data_tree_model_row_drop_possible (GtkTreeDragDest *drag_dest,
					   GtkTreePath *dest_path,
					   GtkSelectionData *selection_data)
{
	/* See if we are dropping to ourselves */
	if (selection_data->target == gdk_atom_intern_static_string (BRASERO_DND_TARGET_SELF_FILE_NODES)) {
		BraseroDNDDataContext *context;
		GtkTreePath *dest_parent;
		BraseroFileNode *parent;
		GList *iter;

		context = (BraseroDNDDataContext *) selection_data->data;
		if (context->model != GTK_TREE_MODEL (drag_dest))
			return FALSE;

		/* make sure the parent is a directory.
		 * NOTE: in this case dest_path is the exact path where it
		 * should be inserted. */
		dest_parent = gtk_tree_path_copy (dest_path);
		gtk_tree_path_up (dest_parent);

		parent = brasero_data_tree_model_path_to_node (BRASERO_DATA_TREE_MODEL (drag_dest), dest_parent);

		if (!parent) {
			/* See if that isn't a BOGUS row; if so, try with parent */
			gtk_tree_path_up (dest_parent);
			parent = brasero_data_tree_model_path_to_node (BRASERO_DATA_TREE_MODEL (drag_dest), dest_parent);

			if (!parent) {
				gtk_tree_path_free (dest_parent);
				return FALSE;
			}
		}
		else if (parent->is_file) {
			/* if that's a file try with parent */
			gtk_tree_path_up (dest_parent);
			parent = parent->parent;
		}

		if (parent->is_loading) {
			gtk_tree_path_free (dest_parent);
			return FALSE;
		}

		for (iter = context->references; iter; iter = iter->next) {
			GtkTreePath *src_path;
			GtkTreeRowReference *reference;

			reference = iter->data;
			src_path = gtk_tree_row_reference_get_path (reference);

			/* see if we are not moving a parent to one of its children */
			if (gtk_tree_path_is_ancestor (src_path, dest_path)) {
				gtk_tree_path_free (src_path);
				continue;
			}

			if (gtk_tree_path_up (src_path)) {
				/* check that node was moved to another directory */
				if (!parent->parent) {
					if (gtk_tree_path_get_depth (src_path)) {
						gtk_tree_path_free (src_path);
						gtk_tree_path_free (dest_parent);
						return TRUE;
					}
				}
				else if (!gtk_tree_path_get_depth (src_path)
				     ||   gtk_tree_path_compare (src_path, dest_parent)) {
					gtk_tree_path_free (src_path);
					gtk_tree_path_free (dest_parent);
					return TRUE;
				}
			}

			gtk_tree_path_free (src_path);
		}

		gtk_tree_path_free (dest_parent);
		return FALSE;
	}
	else if (selection_data->target == gdk_atom_intern_static_string ("text/uri-list"))
		return TRUE;

	return FALSE;
}

static gboolean
brasero_data_tree_model_drag_data_delete (GtkTreeDragSource *source,
					  GtkTreePath *treepath)
{
	return TRUE;
}

/**
 * Sorting part
 */
static gboolean
brasero_data_tree_model_get_sort_column_id (GtkTreeSortable *sortable,
					    gint *column,
					    GtkSortType *type)
{
	BraseroDataTreeModelPrivate *priv;

	priv = BRASERO_DATA_TREE_MODEL_PRIVATE (sortable);

	if (column)
		*column = priv->sort_column;

	if (type)
		*type = priv->sort_type;

	return TRUE;
}

static void
brasero_data_tree_model_set_sort_column_id (GtkTreeSortable *sortable,
					    gint column,
					    GtkSortType type)
{
	BraseroDataTreeModelPrivate *priv;

	priv = BRASERO_DATA_TREE_MODEL_PRIVATE (sortable);
	priv->sort_column = column;
	priv->sort_type = type;

	switch (column) {
	case BRASERO_DATA_TREE_MODEL_NAME:
		brasero_data_project_set_sort_function (BRASERO_DATA_PROJECT (sortable),
							type,
							brasero_file_node_sort_name_cb);
		break;
	case BRASERO_DATA_TREE_MODEL_SIZE:
		brasero_data_project_set_sort_function (BRASERO_DATA_PROJECT (sortable),
							type,
							brasero_file_node_sort_size_cb);
		break;
	case BRASERO_DATA_TREE_MODEL_MIME_DESC:
		brasero_data_project_set_sort_function (BRASERO_DATA_PROJECT (sortable),
							type,
							brasero_file_node_sort_mime_cb);
		break;
	default:
		brasero_data_project_set_sort_function (BRASERO_DATA_PROJECT (sortable),
							type,
							brasero_file_node_sort_default_cb);
		break;
	}

	gtk_tree_sortable_sort_column_changed (sortable);
}

static void
brasero_data_tree_model_set_sort_func (GtkTreeSortable *sortable,
				       gint column,
				       GtkTreeIterCompareFunc sort_func,
				       gpointer data,
				       GtkDestroyNotify destroy)
{
	
}

static void
brasero_data_tree_model_set_default_sort_func (GtkTreeSortable *sortable,
					       GtkTreeIterCompareFunc sort_func,
					       gpointer data,
					       GtkDestroyNotify destroy)
{
	
}

static gboolean
brasero_data_tree_model_has_default_sort_func (GtkTreeSortable *sortable)
{
	/* That's always true since we sort files and directories */
	return TRUE;
}

static void
brasero_data_tree_model_clear_children (BraseroDataTreeModel *self,
					BraseroFileNode *parent)
{
	BraseroFileNode *node;
	GtkTreePath *treepath;

	node = BRASERO_FILE_NODE_CHILDREN (parent);
	if (!node)
		return;

	treepath = brasero_data_tree_model_node_to_path (self, node);

	for (; node; node = node->next)
		gtk_tree_model_row_deleted (GTK_TREE_MODEL (self), treepath);

	gtk_tree_path_free (treepath);
}

static void
brasero_data_tree_model_clear (BraseroDataTreeModel *self)
{
	BraseroFileNode *root;
	BraseroDataTreeModelPrivate *priv;

	priv = BRASERO_DATA_TREE_MODEL_PRIVATE (self);
	if (priv->shown) {
		g_slist_free (priv->shown);
		priv->shown = NULL;
	}

	root = brasero_data_project_get_root (BRASERO_DATA_PROJECT (self));
	brasero_data_tree_model_clear_children (self, root);
}

static void
brasero_data_tree_model_reset (BraseroDataProject *project)
{
	brasero_data_tree_model_clear (BRASERO_DATA_TREE_MODEL (project));

	/* chain up this function except if we invalidated the node */
	if (BRASERO_DATA_PROJECT_CLASS (brasero_data_tree_model_parent_class)->reset)
		BRASERO_DATA_PROJECT_CLASS (brasero_data_tree_model_parent_class)->reset (project);
}

static gboolean
brasero_data_tree_model_node_added (BraseroDataProject *project,
				    BraseroFileNode *node,
				    const gchar *uri)
{
	BraseroDataTreeModelPrivate *priv;
	BraseroFileNode *parent;
	GtkTreePath *path;
	GtkTreeIter iter;

	/* see if we really need to tell the treeview we changed */
	if (node->parent
	&& !node->parent->is_root
	&& !node->parent->is_visible)
		goto end;

	priv = BRASERO_DATA_TREE_MODEL_PRIVATE (project);

	iter.stamp = priv->stamp;
	iter.user_data = node;
	iter.user_data2 = GINT_TO_POINTER (BRASERO_ROW_REGULAR);

	path = brasero_data_tree_model_node_to_path (BRASERO_DATA_TREE_MODEL (project), node);

	/* if the node is reloading (because of a file system change or because
	 * it was a node that was a tmp folder) then no need to signal an added
	 * signal but a changed one */
	if (node->is_reloading) {
		gtk_tree_model_row_changed (GTK_TREE_MODEL (project), path, &iter);
		gtk_tree_path_free (path);
		goto end;
	}

	/* Add the row itself */
	gtk_tree_model_row_inserted (GTK_TREE_MODEL (project),
				     path,
				     &iter);
	gtk_tree_path_free (path);

	parent = node->parent;
	if (!parent->is_root) {
		/* Tell the tree that the parent changed (since the number of children
		 * changed as well). */
		iter.user_data = parent;
		path = brasero_data_tree_model_node_to_path (BRASERO_DATA_TREE_MODEL (project), parent);

		gtk_tree_model_row_changed (GTK_TREE_MODEL (project), path, &iter);

		/* Check if the parent of this node is empty if so remove the BOGUS row.
		 * Do it afterwards to prevent the parent row to be collapsed if it was
		 * previously expanded. */
		if (parent && brasero_file_node_get_n_children (parent) == 1) {
			gtk_tree_path_append_index (path, 1);
			gtk_tree_model_row_deleted (GTK_TREE_MODEL (project), path);
		}

		gtk_tree_path_free (path);
	}

	/* Now see if this is a directory which is empty and needs a BOGUS */
	if (!node->is_file && !node->is_loading) {
		/* NOTE: No need to check for the number of children ... */

		/* emit child-toggled. Thanks to bogus rows we only need to emit
		 * this signal once since a directory will always have a child
		 * in the tree */
		path = brasero_data_tree_model_node_to_path (BRASERO_DATA_TREE_MODEL (project), node);
		gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (project), path, &iter);

		/* add the row */
		iter.stamp = priv->stamp;
		iter.user_data = node;
		iter.user_data2 = GINT_TO_POINTER (BRASERO_ROW_BOGUS);

		gtk_tree_path_append_index (path, 0);
		gtk_tree_model_row_inserted (GTK_TREE_MODEL (project),
					     path,
					     &iter);
		gtk_tree_path_free (path);
	}

	/* we also have to set the is_visible property as all nodes added to 
	 * root are always visible but ref_node is not necessarily called on
	 * these nodes. */
	if (parent->is_root)
		node->is_visible = TRUE;

end:
	/* chain up this function */
	if (BRASERO_DATA_PROJECT_CLASS (brasero_data_tree_model_parent_class)->node_added)
		return BRASERO_DATA_PROJECT_CLASS (brasero_data_tree_model_parent_class)->node_added (project, node, uri);

	return TRUE;
}

static void
brasero_data_tree_model_node_removed (BraseroDataProject *project,
				      BraseroFileNode *node)
{
	BraseroDataTreeModelPrivate *priv;
	BraseroFileNode *parent;
	GtkTreePath *path;

	/* see if we really need to tell the treeview we changed */
	if (!node->is_visible
	&&   node->parent
	&&  !node->parent->is_root
	&&  !node->parent->is_visible)
		goto end;

	priv = BRASERO_DATA_TREE_MODEL_PRIVATE (project);

	/* remove it from the shown list */
	priv->shown = g_slist_remove (priv->shown, node);

	/* See if the parent of this node still has children. If not we need to
	 * add a bogus row. If it hasn't got children then it only remains our
	 * node in the list.
	 * NOTE: parent has to be a directory. */
	parent = node->parent;
	if (!parent->is_root && BRASERO_FILE_NODE_CHILDREN (parent) == node && !node->next) {
		GtkTreeIter iter;

		iter.stamp = priv->stamp;
		iter.user_data = parent;
		iter.user_data2 = GINT_TO_POINTER (BRASERO_ROW_BOGUS);

		path = brasero_data_tree_model_node_to_path (BRASERO_DATA_TREE_MODEL (project), parent);
		gtk_tree_path_append_index (path, 1);

		gtk_tree_model_row_inserted (GTK_TREE_MODEL (project), path, &iter);
		gtk_tree_path_free (path);
	}

	/* remove the node. Do it after adding a possible BOGUS row.
	 * NOTE since BOGUS row has been added move row. */
	path = brasero_data_tree_model_node_to_path (BRASERO_DATA_TREE_MODEL (project), node);
	gtk_tree_model_row_deleted (GTK_TREE_MODEL (project), path);
	gtk_tree_path_free (path);

end:
	/* chain up this function */
	if (BRASERO_DATA_PROJECT_CLASS (brasero_data_tree_model_parent_class)->node_removed)
		BRASERO_DATA_PROJECT_CLASS (brasero_data_tree_model_parent_class)->node_removed (project, node);
}

static void
brasero_data_tree_model_node_changed (BraseroDataProject *project,
				      BraseroFileNode *node)
{
	BraseroDataTreeModelPrivate *priv;
	GtkTreePath *path;
	GtkTreeIter iter;

	/* see if we really need to tell the treeview we changed */
	if (node->parent
	&& !node->parent->is_root
	&& !node->parent->is_visible)
		goto end;

	priv = BRASERO_DATA_TREE_MODEL_PRIVATE (project);

	/* Get the iter for the node */
	iter.stamp = priv->stamp;
	iter.user_data = node;
	iter.user_data2 = GINT_TO_POINTER (BRASERO_ROW_REGULAR);

	path = brasero_data_tree_model_node_to_path (BRASERO_DATA_TREE_MODEL (project), node);
	gtk_tree_model_row_changed (GTK_TREE_MODEL (project),
				    path,
				    &iter);

	/* Now see if this is a directory which is empty and needs a BOGUS */
	if (!node->is_file) {
		/* NOTE: No need to check for the number of children ... */

		/* emit child-toggled. Thanks to bogus rows we only need to emit
		 * this signal once since a directory will always have a child
		 * in the tree */
		gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (project), path, &iter);

		/* add the row */
		if (!BRASERO_FILE_NODE_CHILDREN (node))  {
			iter.user_data2 = GINT_TO_POINTER (BRASERO_ROW_BOGUS);
			gtk_tree_path_append_index (path, 0);

			gtk_tree_model_row_inserted (GTK_TREE_MODEL (project),
						     path,
						     &iter);
		}
	}
	gtk_tree_path_free (path);

end:
	/* chain up this function */
	if (BRASERO_DATA_PROJECT_CLASS (brasero_data_tree_model_parent_class)->node_changed)
		BRASERO_DATA_PROJECT_CLASS (brasero_data_tree_model_parent_class)->node_changed (project, node);
}

static void
brasero_data_tree_model_node_reordered (BraseroDataProject *project,
					BraseroFileNode *parent,
					gint *new_order)
{
	GtkTreePath *treepath;
	BraseroDataTreeModelPrivate *priv;

	/* see if we really need to tell the treeview we changed */
	if (!parent->is_root
	&&  !parent->is_visible)
		goto end;

	priv = BRASERO_DATA_TREE_MODEL_PRIVATE (project);

	treepath = brasero_data_tree_model_node_to_path (BRASERO_DATA_TREE_MODEL (project), parent);
	if (parent != brasero_data_project_get_root (project)) {
		GtkTreeIter iter;

		iter.stamp = priv->stamp;
		iter.user_data = parent;
		iter.user_data2 = GINT_TO_POINTER (BRASERO_ROW_REGULAR);

		gtk_tree_model_rows_reordered (GTK_TREE_MODEL (project),
					       treepath,
					       &iter,
					       new_order);
	}
	else
		gtk_tree_model_rows_reordered (GTK_TREE_MODEL (project),
					       treepath,
					       NULL,
					       new_order);
	gtk_tree_path_free (treepath);

end:
	/* chain up this function */
	if (BRASERO_DATA_PROJECT_CLASS (brasero_data_tree_model_parent_class)->node_reordered)
		BRASERO_DATA_PROJECT_CLASS (brasero_data_tree_model_parent_class)->node_reordered (project, parent, new_order);
}

static void
brasero_data_tree_model_activity_changed (BraseroDataVFS *vfs,
					  gboolean active)
{
	GtkTreeIter iter;
	GSList *nodes;
	BraseroDataTreeModelPrivate *priv;

	if (brasero_data_vfs_is_active (vfs))
		return;

	priv = BRASERO_DATA_TREE_MODEL_PRIVATE (vfs);

	iter.stamp = priv->stamp;
	iter.user_data2 = GINT_TO_POINTER (BRASERO_ROW_REGULAR);

	/* NOTE: we shouldn't need to use reference here as unref_node is used */
	for (nodes = priv->shown; nodes; nodes = nodes->next) {
		GtkTreePath *treepath;

		iter.user_data = nodes->data;
		treepath = brasero_data_tree_model_node_to_path (BRASERO_DATA_TREE_MODEL (vfs), nodes->data);

		gtk_tree_model_row_changed (GTK_TREE_MODEL (vfs), treepath, &iter);
		gtk_tree_path_free (treepath);
	}

	/* chain up this function */
	if (BRASERO_DATA_VFS_CLASS (brasero_data_tree_model_parent_class)->activity_changed)
		BRASERO_DATA_VFS_CLASS (brasero_data_tree_model_parent_class)->activity_changed (vfs, active);
}

static void
brasero_data_tree_model_init (BraseroDataTreeModel *object)
{
	BraseroDataTreeModelPrivate *priv;

	priv = BRASERO_DATA_TREE_MODEL_PRIVATE (object);

	priv->sort_column = GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID;
	do {
		priv->stamp = g_random_int ();
	} while (!priv->stamp);
}

static void
brasero_data_tree_model_finalize (GObject *object)
{
	BraseroDataTreeModelPrivate *priv;

	priv = BRASERO_DATA_TREE_MODEL_PRIVATE (object);
	if (priv->shown) {
		g_slist_free (priv->shown);
		priv->shown = NULL;
	}

	G_OBJECT_CLASS (brasero_data_tree_model_parent_class)->finalize (object);
}

static void
brasero_data_tree_model_iface_init (gpointer g_iface, gpointer data)
{
	GtkTreeModelIface *iface = g_iface;
	static gboolean initialized = FALSE;

	if (initialized)
		return;

	initialized = TRUE;

	iface->ref_node = brasero_data_tree_model_node_shown;
	iface->unref_node = brasero_data_tree_model_node_hidden;

	iface->get_flags = brasero_data_tree_model_get_flags;
	iface->get_n_columns = brasero_data_tree_model_get_n_columns;
	iface->get_column_type = brasero_data_tree_model_get_column_type;
	iface->get_iter = brasero_data_tree_model_get_iter;
	iface->get_path = brasero_data_tree_model_get_path;
	iface->get_value = brasero_data_tree_model_get_value;
	iface->iter_next = brasero_data_tree_model_iter_next;
	iface->iter_children = brasero_data_tree_model_iter_children;
	iface->iter_has_child = brasero_data_tree_model_iter_has_child;
	iface->iter_n_children = brasero_data_tree_model_iter_n_children;
	iface->iter_nth_child = brasero_data_tree_model_iter_nth_child;
	iface->iter_parent = brasero_data_tree_model_iter_parent;
}

static void
brasero_data_tree_model_multi_drag_source_iface_init (gpointer g_iface, gpointer data)
{
	EggTreeMultiDragSourceIface *iface = g_iface;
	static gboolean initialized = FALSE;

	if (initialized)
		return;

	initialized = TRUE;

	iface->row_draggable = brasero_data_tree_model_multi_row_draggable;
	iface->drag_data_get = brasero_data_tree_model_multi_drag_data_get;
	iface->drag_data_delete = brasero_data_tree_model_multi_drag_data_delete;
}

static void
brasero_data_tree_model_drag_source_iface_init (gpointer g_iface, gpointer data)
{
	GtkTreeDragSourceIface *iface = g_iface;
	static gboolean initialized = FALSE;

	if (initialized)
		return;

	initialized = TRUE;

	iface->drag_data_delete = brasero_data_tree_model_drag_data_delete;
}

static void
brasero_data_tree_model_drag_dest_iface_init (gpointer g_iface, gpointer data)
{
	GtkTreeDragDestIface *iface = g_iface;
	static gboolean initialized = FALSE;

	if (initialized)
		return;

	initialized = TRUE;

	iface->drag_data_received = brasero_data_tree_model_drag_data_received;
	iface->row_drop_possible = brasero_data_tree_model_row_drop_possible;
}

static void
brasero_data_tree_model_sortable_iface_init (gpointer g_iface, gpointer data)
{
	GtkTreeSortableIface *iface = g_iface;
	static gboolean initialized = FALSE;

	if (initialized)
		return;

	initialized = TRUE;

	iface->set_sort_func = brasero_data_tree_model_set_sort_func;
	iface->get_sort_column_id = brasero_data_tree_model_get_sort_column_id;
	iface->set_sort_column_id = brasero_data_tree_model_set_sort_column_id;
	iface->set_default_sort_func = brasero_data_tree_model_set_default_sort_func;
	iface->has_default_sort_func = brasero_data_tree_model_has_default_sort_func;
}

static void
brasero_data_tree_model_class_init (BraseroDataTreeModelClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	BraseroDataVFSClass *vfs_class = BRASERO_DATA_VFS_CLASS (klass);
	BraseroDataProjectClass *data_project_class = BRASERO_DATA_PROJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroDataTreeModelPrivate));

	object_class->finalize = brasero_data_tree_model_finalize;

	vfs_class->activity_changed = brasero_data_tree_model_activity_changed;

	data_project_class->reset = brasero_data_tree_model_reset;
	data_project_class->node_added = brasero_data_tree_model_node_added;
	data_project_class->node_removed = brasero_data_tree_model_node_removed;
	data_project_class->node_changed = brasero_data_tree_model_node_changed;
	data_project_class->node_reordered = brasero_data_tree_model_node_reordered;
}

BraseroDataTreeModel *
brasero_data_tree_model_new (void)
{
	return g_object_new (BRASERO_TYPE_DATA_TREE_MODEL, NULL);
}
