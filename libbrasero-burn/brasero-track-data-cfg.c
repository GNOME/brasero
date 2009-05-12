/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-burn
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-burn is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-burn authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-burn. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-burn is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtk.h>

#include "brasero-units.h"

#include "brasero-track-data-cfg.h"

#include "libbrasero-marshal.h"

#include "brasero-filtered-uri.h"

#include "brasero-misc.h"
#include "brasero-data-project.h"
#include "brasero-data-tree-model.h"

typedef struct _BraseroTrackDataCfgPrivate BraseroTrackDataCfgPrivate;
struct _BraseroTrackDataCfgPrivate
{
	BraseroDataTreeModel *tree;
	guint stamp;

	guint loading;
	guint loading_remaining;
	GSList *load_errors;

	GtkIconTheme *theme;

	GSList *shown;

	gint sort_column;
	GtkSortType sort_type;

	guint deep_directory:1;
	guint G2_files:1;
};

#define BRASERO_TRACK_DATA_CFG_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_TRACK_DATA_CFG, BraseroTrackDataCfgPrivate))


static void
brasero_track_data_cfg_drag_source_iface_init (gpointer g_iface, gpointer data);
static void
brasero_track_data_cfg_drag_dest_iface_init (gpointer g_iface, gpointer data);
static void
brasero_track_data_cfg_sortable_iface_init (gpointer g_iface, gpointer data);
static void
brasero_track_data_cfg_iface_init (gpointer g_iface, gpointer data);

G_DEFINE_TYPE_WITH_CODE (BraseroTrackDataCfg,
			 brasero_track_data_cfg,
			 BRASERO_TYPE_TRACK_DATA,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
					        brasero_track_data_cfg_iface_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_DEST,
					        brasero_track_data_cfg_drag_dest_iface_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE,
					        brasero_track_data_cfg_drag_source_iface_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_SORTABLE,
						brasero_track_data_cfg_sortable_iface_init));

typedef enum {
	BRASERO_ROW_REGULAR		= 0,
	BRASERO_ROW_BOGUS
} BraseroFileRowType;

enum {
	AVAILABLE,
	LOADED,
	OVERSIZE,
	ACTIVITY,
	IMAGE,
	UNREADABLE,
	RECURSIVE,
	UNKNOWN,
	G2_FILE,
	NAME_COLLISION,
	DEEP_DIRECTORY,
	SOURCE_LOADED, 
	SOURCE_LOADING, 
	LAST_SIGNAL
};

static gulong brasero_track_data_cfg_signals [LAST_SIGNAL] = { 0 };


/**
 * GtkTreeModel part
 */

static GtkTreePath *
brasero_track_data_cfg_node_to_path (BraseroTrackDataCfg *self,
				      BraseroFileNode *node)
{
	BraseroTrackDataCfgPrivate *priv;
	GtkTreePath *path;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (self);

	path = gtk_tree_path_new ();
	for (; node->parent && !node->is_root; node = node->parent) {
		guint nth;

		nth = brasero_file_node_get_pos_as_child (node);
		gtk_tree_path_prepend_index (path, nth);
	}

	return path;
}

static gboolean
brasero_track_data_cfg_iter_parent (GtkTreeModel *model,
				     GtkTreeIter *iter,
				     GtkTreeIter *child)
{
	BraseroTrackDataCfgPrivate *priv;
	BraseroFileNode *node;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (model);

	/* make sure that iter comes from us */
	g_return_val_if_fail (priv->stamp == child->stamp, FALSE);
	g_return_val_if_fail (child->user_data != NULL, FALSE);

	node = child->user_data;
	if (child->user_data2 == GINT_TO_POINTER (BRASERO_ROW_BOGUS)) {
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
brasero_track_data_cfg_iter_nth_child (GtkTreeModel *model,
					GtkTreeIter *iter,
					GtkTreeIter *parent,
					gint n)
{
	BraseroTrackDataCfgPrivate *priv;
	BraseroFileNode *node;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (model);

	if (parent) {
		/* make sure that iter comes from us */
		g_return_val_if_fail (priv->stamp == parent->stamp, FALSE);
		g_return_val_if_fail (parent->user_data != NULL, FALSE);

		if (parent->user_data2 == GINT_TO_POINTER (BRASERO_ROW_BOGUS)) {
			/* This is a bogus row intended for empty directories,
			 * it hasn't got children. */
			return FALSE;
		}

		node = parent->user_data;
	}
	else
		node = brasero_data_project_get_root (BRASERO_DATA_PROJECT (priv->tree));

	iter->user_data = brasero_file_node_nth_child (node, n);
	if (!iter->user_data)
		return FALSE;

	iter->stamp = priv->stamp;
	iter->user_data2 = GINT_TO_POINTER (BRASERO_ROW_REGULAR);
	return TRUE;
}

static gint
brasero_track_data_cfg_iter_n_children (GtkTreeModel *model,
					 GtkTreeIter *iter)
{
	BraseroTrackDataCfgPrivate *priv;
	BraseroFileNode *node;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (model);

	if (iter == NULL) {
		/* special case */
		node = brasero_data_project_get_root (BRASERO_DATA_PROJECT (priv->tree));
		return brasero_file_node_get_n_children (node);
	}

	/* make sure that iter comes from us */
	g_return_val_if_fail (priv->stamp == iter->stamp, 0);
	g_return_val_if_fail (iter->user_data != NULL, 0);

	if (iter->user_data2 == GINT_TO_POINTER (BRASERO_ROW_BOGUS))
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
brasero_track_data_cfg_iter_has_child (GtkTreeModel *model,
					GtkTreeIter *iter)
{
	BraseroTrackDataCfgPrivate *priv;
	BraseroFileNode *node;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (model);

	/* make sure that iter comes from us */
	g_return_val_if_fail (priv->stamp == iter->stamp, FALSE);
	g_return_val_if_fail (iter->user_data != NULL, FALSE);

	if (iter->user_data2 == GINT_TO_POINTER (BRASERO_ROW_BOGUS)) {
		/* This is a bogus row intended for empty directories
		 * it hasn't got children */
		return FALSE;
	}

	node = iter->user_data;

	/* This is a workaround for a warning in gailtreeview.c line 2946 where
	 * gail uses the GtkTreePath and not a copy which if the node inserted
	 * declares to have children and is not expanded leads to the path being
	 * upped and therefore wrong. */
	if (node->is_inserting)
		return FALSE;

	if (node->is_file)
		return FALSE;

	/* always return TRUE here when it's a directory since even if
	 * it's empty we'll add a row written empty underneath it
	 * anyway. */
	return TRUE;
}

static gboolean
brasero_track_data_cfg_iter_children (GtkTreeModel *model,
				       GtkTreeIter *iter,
				       GtkTreeIter *parent)
{
	BraseroTrackDataCfgPrivate *priv;
	BraseroFileNode *node;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (model);

	if (!parent) {
		BraseroFileNode *root;

		/* This is for the top directory */
		root = brasero_data_project_get_root (BRASERO_DATA_PROJECT (priv->tree));
		if (!root || !BRASERO_FILE_NODE_CHILDREN (root))
			return FALSE;

		iter->stamp = priv->stamp;
		iter->user_data = BRASERO_FILE_NODE_CHILDREN (root);
		iter->user_data2 = GINT_TO_POINTER (BRASERO_ROW_REGULAR);
		return TRUE;
	}

	/* make sure that iter comes from us */
	g_return_val_if_fail (priv->stamp == parent->stamp, FALSE);
	g_return_val_if_fail (parent->user_data != NULL, FALSE);

	if (parent->user_data2 == GINT_TO_POINTER (BRASERO_ROW_BOGUS)) {
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
brasero_track_data_cfg_iter_next (GtkTreeModel *model,
				   GtkTreeIter *iter)
{
	BraseroTrackDataCfgPrivate *priv;
	BraseroFileNode *node;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (model);

	/* make sure that iter comes from us */
	g_return_val_if_fail (priv->stamp == iter->stamp, FALSE);
	g_return_val_if_fail (iter->user_data != NULL, FALSE);

	if (iter->user_data2 == GINT_TO_POINTER (BRASERO_ROW_BOGUS)) {
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
brasero_track_data_cfg_node_shown (GtkTreeModel *model,
				   GtkTreeIter *iter)
{
	BraseroFileNode *node;
	BraseroTrackDataCfgPrivate *priv;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (model);
	node = iter->user_data;

	/* Check if that's a BOGUS row. In this case that means the parent was
	 * expanded. Therefore ask vfs to increase its priority if it's loading
	 * its contents. */
	if (iter->user_data2 == GINT_TO_POINTER (BRASERO_ROW_BOGUS)) {
		/* NOTE: this has to be a directory */
		/* NOTE: there is no need to check for is_loading case here
		 * since before showing its BOGUS row the tree will have shown
		 * its parent itself and therefore that's the cases that follow
		 */
		if (node->is_exploring) {
			/* the directory is being explored increase priority */
			brasero_data_vfs_require_directory_contents (BRASERO_DATA_VFS (priv->tree), node);
		}

		/* Otherwise, that's simply a BOGUS row and its parent was
		 * loaded but it is empty. Nothing to do. */
		node->is_expanded = TRUE;
		return;
	}

	if (!node)
		return;

	node->is_visible ++;

	if (node->parent && !node->parent->is_root) {
		if (!node->parent->is_expanded && node->is_visible > 0) {
			GtkTreePath *treepath;

			node->parent->is_expanded = TRUE;
			treepath = gtk_tree_model_get_path (model, iter);
			gtk_tree_model_row_changed (model,
						    treepath,
						    iter);
			gtk_tree_path_free (treepath);
		}
	}

	if (node->is_imported) {
		if (node->is_fake && !node->is_file) {
			/* we don't load all nodes when importing a session do it now */
			brasero_data_session_load_directory_contents (BRASERO_DATA_SESSION (priv->tree), node, NULL);
		}

		return;
	}

	if (node->is_visible > 1)
		return;

	/* NOTE: no need to see if that's a directory being explored here. If it
	 * is being explored then it has a BOGUS row and that's the above case 
	 * that is reached. */
	if (node->is_loading) {
		/* in this case have vfs to increase priority for this node */
		brasero_data_vfs_require_node_load (BRASERO_DATA_VFS (priv->tree), node);
	}
	else if (!BRASERO_FILE_NODE_MIME (node)) {
		/* that means that file wasn't completly loaded. To save
		 * some time we delayed the detection of the mime type
		 * since that takes a lot of time. */
		brasero_data_vfs_load_mime (BRASERO_DATA_VFS (priv->tree), node);
	}

	/* add the node to the visible list that is used to update the disc 
	 * share for the node (we don't want to update the whole tree).
	 * Moreover, we only want files since directories don't have space. */
	priv->shown = g_slist_prepend (priv->shown, node);
}

static void
brasero_track_data_cfg_node_hidden (GtkTreeModel *model,
				    GtkTreeIter *iter)
{
	BraseroFileNode *node;
	BraseroTrackDataCfgPrivate *priv;

	/* if it's a BOGUS row stop here since they are not added to shown list.
	 * In the same way returns if it is a file. */
	node = iter->user_data;
	if (iter->user_data2 == GINT_TO_POINTER (BRASERO_ROW_BOGUS)) {
		node->is_expanded = FALSE;
		return;
	}

	if (!node)
		return;

	node->is_visible --;
	if (node->parent && !node->parent->is_root) {
		if (node->parent->is_expanded && node->is_visible == 0) {
			GtkTreePath *treepath;
			GtkTreeIter parent_iter;

			node->parent->is_expanded = FALSE;
			treepath = brasero_track_data_cfg_node_to_path (BRASERO_TRACK_DATA_CFG (model), node->parent);
			gtk_tree_model_get_iter (model, &parent_iter, treepath);
			gtk_tree_model_row_changed (model,
						    treepath,
						    &parent_iter);
			gtk_tree_path_free (treepath);
		}
	}

	if (node->is_imported)
		return;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (model);

	/* update shown list */
	if (!node->is_visible)
		priv->shown = g_slist_remove (priv->shown, node);
}

static void
brasero_track_data_cfg_get_value (GtkTreeModel *model,
				   GtkTreeIter *iter,
				   gint column,
				   GValue *value)
{
	BraseroTrackDataCfgPrivate *priv;
	BraseroTrackDataCfg *self;
	BraseroFileNode *node;

	self = BRASERO_TRACK_DATA_CFG (model);
	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (model);

	/* make sure that iter comes from us */
	g_return_if_fail (priv->stamp == iter->stamp);
	g_return_if_fail (iter->user_data != NULL);

	node = iter->user_data;

	if (iter->user_data2 == GINT_TO_POINTER (BRASERO_ROW_BOGUS)) {
		switch (column) {
		case BRASERO_DATA_TREE_MODEL_NAME:
			g_value_init (value, G_TYPE_STRING);
			if (node->is_exploring)
				g_value_set_string (value, _("(loading ...)"));
			else
				g_value_set_string (value, _("Empty"));

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
	
		case BRASERO_DATA_TREE_MODEL_COLOR:
			g_value_init (value, G_TYPE_STRING);
			g_value_set_string (value, NULL);
			return;

		case BRASERO_DATA_TREE_MODEL_IS_FILE:
			g_value_init (value, G_TYPE_BOOLEAN);
			g_value_set_boolean (value, FALSE);
			return;

		case BRASERO_DATA_TREE_MODEL_IS_LOADING:
			g_value_init (value, G_TYPE_BOOLEAN);
			g_value_set_boolean (value, FALSE);
			return;

		case BRASERO_DATA_TREE_MODEL_IS_IMPORTED:
			g_value_init (value, G_TYPE_BOOLEAN);
			g_value_set_boolean (value, FALSE);
			return;

		case BRASERO_DATA_TREE_MODEL_URI:
			g_value_init (value, G_TYPE_STRING);
			g_value_set_string (value, NULL);
			return;

		default:
			return;
		}

		return;
	}

	switch (column) {
	case BRASERO_DATA_TREE_MODEL_EDITABLE:
		g_value_init (value, G_TYPE_BOOLEAN);
		g_value_set_boolean (value, (node->is_imported == FALSE)/* && node->is_selected*/);
		return;

	case BRASERO_DATA_TREE_MODEL_NAME: {
		gchar *filename;

		g_value_init (value, G_TYPE_STRING);
		filename = g_filename_to_utf8 (BRASERO_FILE_NODE_NAME (node),
					       -1,
					       NULL,
					       NULL,
					       NULL);
		if (!filename)
			filename = brasero_utils_validate_utf8 (BRASERO_FILE_NODE_NAME (node));

		if (filename)
			g_value_set_string (value, filename);
		else	/* Glib section on g_convert advise to use a string like
			 * "Invalid Filename". */
			g_value_set_string (value, BRASERO_FILE_NODE_NAME (node));

		g_free (filename);
		return;
	}

	case BRASERO_DATA_TREE_MODEL_MIME_DESC:
		g_value_init (value, G_TYPE_STRING);
		if (node->is_loading)
			g_value_set_string (value, _("(loading ...)"));
		else if (!node->is_file) {
			gchar *description;

			description = g_content_type_get_description ("inode/directory");
			g_value_set_string (value, description);
			g_free (description);
		}
		else if (node->is_imported)
			g_value_set_string (value, _("Disc file"));
		else if (!BRASERO_FILE_NODE_MIME (node))
			g_value_set_string (value, _("(loading ...)"));
		else {
			gchar *description;

			description = g_content_type_get_description (BRASERO_FILE_NODE_MIME (node));
			g_value_set_string (value, description);
			g_free (description);
		}

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
		else if (BRASERO_FILE_NODE_MIME (node)) {
			const gchar *icon_string = "text-x-preview";
			GIcon *icon;

			/* NOTE: implemented in glib 2.15.6 (not for windows though) */
			icon = g_content_type_get_icon (BRASERO_FILE_NODE_MIME (node));
			if (G_IS_THEMED_ICON (icon)) {
				const gchar * const *names = NULL;

				names = g_themed_icon_get_names (G_THEMED_ICON (icon));
				if (names) {
					gint i;

					for (i = 0; names [i]; i++) {
						if (gtk_icon_theme_has_icon (priv->theme, names [i])) {
							icon_string = names [i];
							break;
						}
					}
				}
			}

			g_value_set_string (value, icon_string);
			g_object_unref (icon);
		}
		else
			g_value_set_string (value, "image-loading");

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
				g_value_set_string (value, _("Empty"));
			else {
				gchar *text;

				text = g_strdup_printf (ngettext ("%d item", "%d items", nb_items), nb_items);
				g_value_set_string (value, text);
				g_free (text);
			}
		}
		else {
			gchar *text;

			text = g_format_size_for_display (BRASERO_FILE_NODE_SECTORS (node) * 2048);
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
		if (!node->is_imported && !brasero_data_vfs_is_active (BRASERO_DATA_VFS (priv->tree))) {
			gint64 sectors;
			goffset node_sectors;

			sectors = brasero_data_project_get_sectors (BRASERO_DATA_PROJECT (priv->tree));

			if (!node->is_file)
				node_sectors = brasero_data_project_get_folder_sectors (BRASERO_DATA_PROJECT (priv->tree), node);
			else
				node_sectors = BRASERO_FILE_NODE_SECTORS (node);

			if (sectors)
				g_value_set_int (value, MAX (0, MIN (node_sectors * 100 / sectors, 100)));
			else
				g_value_set_int (value, 0);
		}
		else
			g_value_set_int (value, 0);

		return;

	case BRASERO_DATA_TREE_MODEL_STYLE:
		g_value_init (value, PANGO_TYPE_STYLE);
		if (node->is_imported)
			g_value_set_enum (value, PANGO_STYLE_ITALIC);

		return;

	case BRASERO_DATA_TREE_MODEL_COLOR:
		g_value_init (value, G_TYPE_STRING);
		if (node->is_imported)
			g_value_set_string (value, "grey50");

		return;

	case BRASERO_DATA_TREE_MODEL_URI: {
		gchar *uri;

		g_value_init (value, G_TYPE_STRING);
		uri = brasero_data_project_node_to_uri (BRASERO_DATA_PROJECT (priv->tree), node);
		g_value_set_string (value, uri);
		g_free (uri);
		return;
	}

	case BRASERO_DATA_TREE_MODEL_IS_FILE:
		g_value_init (value, G_TYPE_BOOLEAN);
		g_value_set_boolean (value, node->is_file);
		return;

	case BRASERO_DATA_TREE_MODEL_IS_LOADING:
		g_value_init (value, G_TYPE_BOOLEAN);
		g_value_set_boolean (value, node->is_loading);
		return;

	case BRASERO_DATA_TREE_MODEL_IS_IMPORTED:
		g_value_init (value, G_TYPE_BOOLEAN);
		g_value_set_boolean (value, node->is_imported);
		return;

	default:
		return;
	}

	return;
}

static GtkTreePath *
brasero_track_data_cfg_get_path (GtkTreeModel *model,
				  GtkTreeIter *iter)
{
	BraseroTrackDataCfgPrivate *priv;
	BraseroFileNode *node;
	GtkTreePath *path;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (model);

	/* make sure that iter comes from us */
	g_return_val_if_fail (priv->stamp == iter->stamp, NULL);
	g_return_val_if_fail (iter->user_data != NULL, NULL);

	node = iter->user_data;

	/* NOTE: there is only one single node without a name: root */
	path = brasero_track_data_cfg_node_to_path (BRASERO_TRACK_DATA_CFG (model), node);

	/* Add index 0 for empty bogus row */
	if (iter->user_data2 == GINT_TO_POINTER (BRASERO_ROW_BOGUS))
		gtk_tree_path_append_index (path, 0);

	return path;
}

BraseroFileNode *
brasero_track_data_cfg_path_to_node (BraseroTrackDataCfg *self,
				     GtkTreePath *path)
{
	BraseroTrackDataCfgPrivate *priv;
	BraseroFileNode *node;
	gint *indices;
	guint depth;
	guint i;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (self);

	indices = gtk_tree_path_get_indices (path);
	depth = gtk_tree_path_get_depth (path);

	node = brasero_data_project_get_root (BRASERO_DATA_PROJECT (priv->tree));
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
brasero_track_data_cfg_get_iter (GtkTreeModel *model,
				 GtkTreeIter *iter,
				 GtkTreePath *path)
{
	BraseroTrackDataCfgPrivate *priv;
	BraseroFileNode *root;
	BraseroFileNode *node;
	const gint *indices;
	guint depth;
	guint i;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (model);

	indices = gtk_tree_path_get_indices (path);
	depth = gtk_tree_path_get_depth (path);

	root = brasero_data_project_get_root (BRASERO_DATA_PROJECT (priv->tree));
	/* NOTE: if we're in reset, then root won't exist anymore */
	if (!root)
		return FALSE;
		
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
brasero_track_data_cfg_get_column_type (GtkTreeModel *model,
					 gint index)
{
	switch (index) {
	case BRASERO_DATA_TREE_MODEL_NAME:
		return G_TYPE_STRING;

	case BRASERO_DATA_TREE_MODEL_URI:
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

	case BRASERO_DATA_TREE_MODEL_COLOR:
		return G_TYPE_STRING;

	case BRASERO_DATA_TREE_MODEL_EDITABLE:
		return G_TYPE_BOOLEAN;

	case BRASERO_DATA_TREE_MODEL_IS_FILE:
		return G_TYPE_BOOLEAN;

	case BRASERO_DATA_TREE_MODEL_IS_LOADING:
		return G_TYPE_BOOLEAN;

	case BRASERO_DATA_TREE_MODEL_IS_IMPORTED:
		return G_TYPE_BOOLEAN;

	default:
		break;
	}

	return G_TYPE_INVALID;
}

static gint
brasero_track_data_cfg_get_n_columns (GtkTreeModel *model)
{
	return BRASERO_DATA_TREE_MODEL_COL_NUM;
}

static GtkTreeModelFlags
brasero_track_data_cfg_get_flags (GtkTreeModel *model)
{
	return 0;
}

static gboolean
brasero_track_data_cfg_row_draggable (GtkTreeDragSource *drag_source,
				      GtkTreePath *treepath)
{
	BraseroFileNode *node;

	node = brasero_track_data_cfg_path_to_node (BRASERO_TRACK_DATA_CFG (drag_source), treepath);
	if (node && !node->is_imported)
		return TRUE;

	return FALSE;
}

static gboolean
brasero_track_data_cfg_drag_data_get (GtkTreeDragSource *drag_source,
				      GtkTreePath *treepath,
				      GtkSelectionData *selection_data)
{
	if (selection_data->target == gdk_atom_intern (BRASERO_DND_TARGET_DATA_TRACK_REFERENCE_LIST, TRUE)) {
		GtkTreeRowReference *reference;

		reference = gtk_tree_row_reference_new (GTK_TREE_MODEL (drag_source), treepath);
		gtk_selection_data_set (selection_data,
					gdk_atom_intern_static_string (BRASERO_DND_TARGET_DATA_TRACK_REFERENCE_LIST),
					8,
					(void *) g_list_prepend (NULL, reference),
					sizeof (GList));
	}
	else
		return FALSE;

	return TRUE;
}

static gboolean
brasero_track_data_cfg_drag_data_delete (GtkTreeDragSource *drag_source,
					 GtkTreePath *path)
{
	/* NOTE: it's not the data in the selection_data here that should be
	 * deleted but rather the rows selected when there is a move. FALSE
	 * here means that we didn't delete anything. */
	/* return TRUE to stop other handlers */
	return TRUE;
}

static gboolean
brasero_track_data_cfg_drag_data_received (GtkTreeDragDest *drag_dest,
					   GtkTreePath *dest_path,
					   GtkSelectionData *selection_data)
{
	BraseroFileNode *node;
	BraseroFileNode *parent;
	GtkTreePath *dest_parent;
	BraseroTrackDataCfgPrivate *priv;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (drag_dest);

	/* NOTE: dest_path is the path to insert before; so we may not have a 
	 * valid path if it's in an empty directory */

	dest_parent = gtk_tree_path_copy (dest_path);
	gtk_tree_path_up (dest_parent);
	parent = brasero_track_data_cfg_path_to_node (BRASERO_TRACK_DATA_CFG (drag_dest), dest_parent);
	if (!parent) {
		gtk_tree_path_up (dest_parent);
		parent = brasero_track_data_cfg_path_to_node (BRASERO_TRACK_DATA_CFG (drag_dest), dest_parent);
	}
	else if (parent->is_file)
		parent = parent->parent;

	gtk_tree_path_free (dest_parent);

	/* Received data: see where it comes from:
	 * - from us, then that's a simple move
	 * - from another widget then it's going to be URIS and we add
	 *   them to the DataProject */
	if (selection_data->target == gdk_atom_intern (BRASERO_DND_TARGET_DATA_TRACK_REFERENCE_LIST, TRUE)) {
		GList *iter;

		iter = (GList *) selection_data->data;

		/* That's us: move the row and its children. */
		for (; iter; iter = iter->next) {
			GtkTreeRowReference *reference;
			GtkTreeModel *src_model;
			GtkTreePath *treepath;

			reference = iter->data;
			src_model = gtk_tree_row_reference_get_model (reference);
			if (src_model != GTK_TREE_MODEL (drag_dest))
				continue;

			treepath = gtk_tree_row_reference_get_path (reference);

			node = brasero_track_data_cfg_path_to_node (BRASERO_TRACK_DATA_CFG (drag_dest), treepath);
			gtk_tree_path_free (treepath);

			brasero_data_project_move_node (BRASERO_DATA_PROJECT (priv->tree), node, parent);
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
			node = brasero_data_project_add_loading_node (BRASERO_DATA_PROJECT (priv->tree),
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
brasero_track_data_cfg_row_drop_possible (GtkTreeDragDest *drag_dest,
					  GtkTreePath *dest_path,
					  GtkSelectionData *selection_data)
{
	/* See if we are dropping to ourselves */
	if (selection_data->target == gdk_atom_intern_static_string (BRASERO_DND_TARGET_DATA_TRACK_REFERENCE_LIST)) {
		GtkTreePath *dest_parent;
		BraseroFileNode *parent;
		GList *iter;

		iter = (GList *) selection_data->data;

		/* make sure the parent is a directory.
		 * NOTE: in this case dest_path is the exact path where it
		 * should be inserted. */
		dest_parent = gtk_tree_path_copy (dest_path);
		gtk_tree_path_up (dest_parent);

		parent = brasero_track_data_cfg_path_to_node (BRASERO_TRACK_DATA_CFG (drag_dest), dest_parent);

		if (!parent) {
			/* See if that isn't a BOGUS row; if so, try with parent */
			gtk_tree_path_up (dest_parent);
			parent = brasero_track_data_cfg_path_to_node (BRASERO_TRACK_DATA_CFG (drag_dest), dest_parent);

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

		for (; iter; iter = iter->next) {
			GtkTreePath *src_path;
			GtkTreeModel *src_model;
			GtkTreeRowReference *reference;

			reference = iter->data;
			src_model = gtk_tree_row_reference_get_model (reference);
			if (src_model != GTK_TREE_MODEL (drag_dest))
				continue;

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

/**
 * Sorting part
 */

static gboolean
brasero_track_data_cfg_get_sort_column_id (GtkTreeSortable *sortable,
					    gint *column,
					    GtkSortType *type)
{
	BraseroTrackDataCfgPrivate *priv;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (sortable);

	if (column)
		*column = priv->sort_column;

	if (type)
		*type = priv->sort_type;

	return TRUE;
}

static void
brasero_track_data_cfg_set_sort_column_id (GtkTreeSortable *sortable,
					    gint column,
					    GtkSortType type)
{
	BraseroTrackDataCfgPrivate *priv;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (sortable);
	priv->sort_column = column;
	priv->sort_type = type;

	switch (column) {
	case BRASERO_DATA_TREE_MODEL_NAME:
		brasero_data_project_set_sort_function (BRASERO_DATA_PROJECT (priv->tree),
							type,
							brasero_file_node_sort_name_cb);
		break;
	case BRASERO_DATA_TREE_MODEL_SIZE:
		brasero_data_project_set_sort_function (BRASERO_DATA_PROJECT (priv->tree),
							type,
							brasero_file_node_sort_size_cb);
		break;
	case BRASERO_DATA_TREE_MODEL_MIME_DESC:
		brasero_data_project_set_sort_function (BRASERO_DATA_PROJECT (priv->tree),
							type,
							brasero_file_node_sort_mime_cb);
		break;
	default:
		brasero_data_project_set_sort_function (BRASERO_DATA_PROJECT (priv->tree),
							type,
							brasero_file_node_sort_default_cb);
		break;
	}

	gtk_tree_sortable_sort_column_changed (sortable);
}

static gboolean
brasero_track_data_cfg_has_default_sort_func (GtkTreeSortable *sortable)
{
	/* That's always true since we sort files and directories */
	return TRUE;
}

static void
brasero_track_data_cfg_node_added (BraseroDataProject *project,
				   BraseroFileNode *node,
				   BraseroTrackDataCfg *self)
{
	BraseroTrackDataCfgPrivate *priv;
	BraseroFileNode *parent;
	GtkTreePath *path;
	GtkTreeIter iter;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (self);

	iter.stamp = priv->stamp;
	iter.user_data = node;
	iter.user_data2 = GINT_TO_POINTER (BRASERO_ROW_REGULAR);

	path = brasero_track_data_cfg_node_to_path (self, node);

	/* if the node is reloading (because of a file system change or because
	 * it was a node that was a tmp folder) then no need to signal an added
	 * signal but a changed one */
	if (node->is_reloading) {
		gtk_tree_model_row_changed (GTK_TREE_MODEL (self), path, &iter);
		gtk_tree_path_free (path);
		return;
	}

	/* Add the row itself */
	/* This is a workaround for a warning in gailtreeview.c line 2946 where
	 * gail uses the GtkTreePath and not a copy which if the node inserted
	 * declares to have children and is not expanded leads to the path being
	 * upped and therefore wrong. */
	node->is_inserting = 1;
	gtk_tree_model_row_inserted (GTK_TREE_MODEL (self),
				     path,
				     &iter);
	node->is_inserting = 0;
	gtk_tree_path_free (path);

	parent = node->parent;
	if (!parent->is_root) {
		/* Tell the tree that the parent changed (since the number of children
		 * changed as well). */
		iter.user_data = parent;
		path = brasero_track_data_cfg_node_to_path (self, parent);

		gtk_tree_model_row_changed (GTK_TREE_MODEL (self), path, &iter);

		/* Check if the parent of this node is empty if so remove the BOGUS row.
		 * Do it afterwards to prevent the parent row to be collapsed if it was
		 * previously expanded. */
		if (parent && brasero_file_node_get_n_children (parent) == 1) {
			gtk_tree_path_append_index (path, 1);
			gtk_tree_model_row_deleted (GTK_TREE_MODEL (self), path);
		}

		gtk_tree_path_free (path);
	}

	/* Now see if this is a directory which is empty and needs a BOGUS */
	if (!node->is_file && !node->is_loading) {
		/* emit child-toggled. Thanks to bogus rows we only need to emit
		 * this signal once since a directory will always have a child
		 * in the tree */
		path = brasero_track_data_cfg_node_to_path (self, node);
		gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (self), path, &iter);
		gtk_tree_path_free (path);
	}

	/* we also have to set the is_visible property as all nodes added to 
	 * root are always visible but ref_node is not necessarily called on
	 * these nodes. */
//	if (parent->is_root)
//		node->is_visible = TRUE;
}

static void
brasero_track_data_cfg_node_removed (BraseroDataProject *project,
				     BraseroFileNode *former_parent,
				     guint former_position,
				     BraseroFileNode *node,
				     BraseroTrackDataCfg *self)
{
	BraseroTrackDataCfgPrivate *priv;
	GSList *iter, *next;
	GtkTreePath *path;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (self);

	/* remove it from the shown list and all its children as well */
	priv->shown = g_slist_remove (priv->shown, node);
	for (iter = priv->shown; iter; iter = next) {
		BraseroFileNode *tmp;

		tmp = iter->data;
		next = iter->next;
		if (brasero_file_node_is_ancestor (node, tmp))
			priv->shown = g_slist_remove (priv->shown, tmp);
	}

	/* See if the parent of this node still has children. If not we need to
	 * add a bogus row. If it hasn't got children then it only remains our
	 * node in the list.
	 * NOTE: parent has to be a directory. */
	if (!former_parent->is_root && !BRASERO_FILE_NODE_CHILDREN (former_parent)) {
		GtkTreeIter iter;

		iter.stamp = priv->stamp;
		iter.user_data = former_parent;
		iter.user_data2 = GINT_TO_POINTER (BRASERO_ROW_BOGUS);

		path = brasero_track_data_cfg_node_to_path (self, former_parent);
		gtk_tree_path_append_index (path, 1);

		gtk_tree_model_row_inserted (GTK_TREE_MODEL (self), path, &iter);
		gtk_tree_path_free (path);
	}

	/* remove the node. Do it after adding a possible BOGUS row.
	 * NOTE since BOGUS row has been added move row. */
	path = brasero_track_data_cfg_node_to_path (self, former_parent);
	gtk_tree_path_append_index (path, former_position);

	gtk_tree_model_row_deleted (GTK_TREE_MODEL (self), path);
	gtk_tree_path_free (path);
}

static void
brasero_track_data_cfg_node_changed (BraseroDataProject *project,
				     BraseroFileNode *node,
				     BraseroTrackDataCfg *self)
{
	BraseroTrackDataCfgPrivate *priv;
	GtkTreePath *path;
	GtkTreeIter iter;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (self);

	/* Get the iter for the node */
	iter.stamp = priv->stamp;
	iter.user_data = node;
	iter.user_data2 = GINT_TO_POINTER (BRASERO_ROW_REGULAR);

	path = brasero_track_data_cfg_node_to_path (self, node);
	gtk_tree_model_row_changed (GTK_TREE_MODEL (self),
				    path,
				    &iter);

	/* Now see if this is a directory which is empty and needs a BOGUS */
	if (!node->is_file) {
		/* NOTE: No need to check for the number of children ... */

		/* emit child-toggled. Thanks to bogus rows we only need to emit
		 * this signal once since a directory will always have a child
		 * in the tree */
		gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (self),
						      path,
						      &iter);

		/* The problem is that without that, the folder contents on disc
		 * won't be added to the tree if the node it replaced was
		 * already visible. */
		if (node->is_imported
		&&  node->is_visible
		&&  node->is_fake)
			brasero_data_session_load_directory_contents (BRASERO_DATA_SESSION (project),
								      node,
								      NULL);

		/* add the row */
		if (!BRASERO_FILE_NODE_CHILDREN (node))  {
			iter.user_data2 = GINT_TO_POINTER (BRASERO_ROW_BOGUS);
			gtk_tree_path_append_index (path, 0);

			gtk_tree_model_row_inserted (GTK_TREE_MODEL (self),
						     path,
						     &iter);
		}
	}
	gtk_tree_path_free (path);
}

static void
brasero_track_data_cfg_node_reordered (BraseroDataProject *project,
				       BraseroFileNode *parent,
				       gint *new_order,
				       BraseroTrackDataCfg *self)
{
	GtkTreePath *treepath;
	BraseroTrackDataCfgPrivate *priv;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (self);

	treepath = brasero_track_data_cfg_node_to_path (self, parent);
	if (parent != brasero_data_project_get_root (project)) {
		GtkTreeIter iter;

		iter.stamp = priv->stamp;
		iter.user_data = parent;
		iter.user_data2 = GINT_TO_POINTER (BRASERO_ROW_REGULAR);

		gtk_tree_model_rows_reordered (GTK_TREE_MODEL (self),
					       treepath,
					       &iter,
					       new_order);
	}
	else
		gtk_tree_model_rows_reordered (GTK_TREE_MODEL (self),
					       treepath,
					       NULL,
					       new_order);

	gtk_tree_path_free (treepath);
}

static void
brasero_track_data_cfg_finalize (GObject *object)
{
	BraseroTrackDataCfgPrivate *priv;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (object);

	if (priv->shown) {
		g_slist_free (priv->shown);
		priv->shown = NULL;
	}

	if (priv->tree) {
		g_object_unref (priv->tree);
		priv->tree = NULL;
	}

	G_OBJECT_CLASS (brasero_track_data_cfg_parent_class)->finalize (object);
}

static void
brasero_track_data_cfg_iface_init (gpointer g_iface, gpointer data)
{
	GtkTreeModelIface *iface = g_iface;
	static gboolean initialized = FALSE;

	if (initialized)
		return;

	initialized = TRUE;

	iface->ref_node = brasero_track_data_cfg_node_shown;
	iface->unref_node = brasero_track_data_cfg_node_hidden;

	iface->get_flags = brasero_track_data_cfg_get_flags;
	iface->get_n_columns = brasero_track_data_cfg_get_n_columns;
	iface->get_column_type = brasero_track_data_cfg_get_column_type;
	iface->get_iter = brasero_track_data_cfg_get_iter;
	iface->get_path = brasero_track_data_cfg_get_path;
	iface->get_value = brasero_track_data_cfg_get_value;
	iface->iter_next = brasero_track_data_cfg_iter_next;
	iface->iter_children = brasero_track_data_cfg_iter_children;
	iface->iter_has_child = brasero_track_data_cfg_iter_has_child;
	iface->iter_n_children = brasero_track_data_cfg_iter_n_children;
	iface->iter_nth_child = brasero_track_data_cfg_iter_nth_child;
	iface->iter_parent = brasero_track_data_cfg_iter_parent;
}

static void
brasero_track_data_cfg_drag_source_iface_init (gpointer g_iface, gpointer data)
{
	GtkTreeDragSourceIface *iface = g_iface;
	static gboolean initialized = FALSE;

	if (initialized)
		return;

	initialized = TRUE;

	iface->row_draggable = brasero_track_data_cfg_row_draggable;
	iface->drag_data_get = brasero_track_data_cfg_drag_data_get;
	iface->drag_data_delete = brasero_track_data_cfg_drag_data_delete;
}

static void
brasero_track_data_cfg_drag_dest_iface_init (gpointer g_iface, gpointer data)
{
	GtkTreeDragDestIface *iface = g_iface;
	static gboolean initialized = FALSE;

	if (initialized)
		return;

	initialized = TRUE;

	iface->drag_data_received = brasero_track_data_cfg_drag_data_received;
	iface->row_drop_possible = brasero_track_data_cfg_row_drop_possible;
}

static void
brasero_track_data_cfg_sortable_iface_init (gpointer g_iface, gpointer data)
{
	GtkTreeSortableIface *iface = g_iface;
	static gboolean initialized = FALSE;

	if (initialized)
		return;

	initialized = TRUE;

	iface->get_sort_column_id = brasero_track_data_cfg_get_sort_column_id;
	iface->set_sort_column_id = brasero_track_data_cfg_set_sort_column_id;
	iface->has_default_sort_func = brasero_track_data_cfg_has_default_sort_func;
}

/**
 * Track part
 */

gboolean
brasero_track_data_cfg_add (BraseroTrackDataCfg *track,
			    const gchar *uri,
			    GtkTreePath *parent)
{
	BraseroTrackDataCfgPrivate *priv;
	BraseroFileNode *parent_node;

	g_return_val_if_fail (BRASERO_TRACK_DATA_CFG (track), FALSE);
	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);

	if (priv->loading)
		return FALSE;

	if (parent) {
		parent_node = brasero_track_data_cfg_path_to_node (track, parent);
		if (parent_node && (parent_node->is_file || parent_node->is_loading))
			parent_node = parent_node->parent;
	}
	else
		parent_node = brasero_data_project_get_root (BRASERO_DATA_PROJECT (priv->tree));

	return (brasero_data_project_add_loading_node (BRASERO_DATA_PROJECT (BRASERO_DATA_PROJECT (priv->tree)), uri, parent_node) != NULL);
}

GtkTreePath *
brasero_track_data_cfg_add_empty_directory (BraseroTrackDataCfg *track,
					    const gchar *name,
					    GtkTreePath *parent)
{
	BraseroTrackDataCfgPrivate *priv;
	BraseroFileNode *parent_node;
	gchar *default_name = NULL;
	BraseroFileNode *node;

	g_return_val_if_fail (BRASERO_TRACK_DATA_CFG (track), FALSE);

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);
	if (priv->loading)
		return NULL;

	if (parent) {
		parent_node = brasero_track_data_cfg_path_to_node (track, parent);
		if (parent_node && (parent_node->is_file || parent_node->is_loading))
			parent_node = parent_node->parent;
	}
	else
		parent_node = brasero_data_project_get_root (BRASERO_DATA_PROJECT (priv->tree));

	if (!name) {
		guint nb = 1;

		default_name = g_strdup_printf (_("New folder"));
		while (brasero_file_node_check_name_existence (parent_node, default_name)) {
			g_free (default_name);
			default_name = g_strdup_printf (_("New folder %i"), nb);
			nb++;
		}

	}

	node = brasero_data_project_add_empty_directory (BRASERO_DATA_PROJECT (priv->tree),
							 name? name:default_name,
							 parent_node);
	if (default_name)
		g_free (default_name);

	if (!node)
		return NULL;

	return brasero_track_data_cfg_node_to_path (track, node);
}

gboolean
brasero_track_data_cfg_remove (BraseroTrackDataCfg *track,
			       GtkTreePath *treepath)
{
	BraseroTrackDataCfgPrivate *priv;
	BraseroFileNode *node;

	g_return_val_if_fail (BRASERO_TRACK_DATA_CFG (track), FALSE);
	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);
	if (priv->loading)
		return FALSE;

	node = brasero_track_data_cfg_path_to_node (track, treepath);
	brasero_data_project_remove_node (BRASERO_DATA_PROJECT (priv->tree), node);
	return TRUE;
}

gboolean
brasero_track_data_cfg_rename (BraseroTrackDataCfg *track,
			       const gchar *newname,
			       GtkTreePath *treepath)
{
	BraseroTrackDataCfgPrivate *priv;
	BraseroFileNode *node;

	g_return_val_if_fail (BRASERO_TRACK_DATA_CFG (track), FALSE);
	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);
	node = brasero_track_data_cfg_path_to_node (track, treepath);
	return brasero_data_project_rename_node (BRASERO_DATA_PROJECT (priv->tree),
						 node,
						 newname);
}

gboolean
brasero_track_data_cfg_reset (BraseroTrackDataCfg *track)
{
	BraseroTrackDataCfgPrivate *priv;
	BraseroFileNode *root;
	GtkTreePath *treepath;
	guint num;
	guint i;

	g_return_val_if_fail (BRASERO_TRACK_DATA_CFG (track), FALSE);
	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);
	if (priv->loading)
		return FALSE;

	root = brasero_data_project_get_root (BRASERO_DATA_PROJECT (priv->tree));
	num = brasero_file_node_get_n_children (root);

	brasero_data_project_reset (BRASERO_DATA_PROJECT (priv->tree));

	treepath = gtk_tree_path_new_first ();
	for (i = 0; i < num; i++)
		gtk_tree_model_row_deleted (GTK_TREE_MODEL (track), treepath);
	gtk_tree_path_free (treepath);

	g_slist_free (priv->shown);
	priv->shown = NULL;

	priv->G2_files = FALSE;
	priv->deep_directory = FALSE;

	return TRUE;
}

GtkTreeModel *
brasero_track_data_cfg_get_filtered_model (BraseroTrackDataCfg *track)
{
	BraseroTrackDataCfgPrivate *priv;
	GtkTreeModel *model;

	g_return_val_if_fail (BRASERO_TRACK_DATA_CFG (track), NULL);
	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);
	model = GTK_TREE_MODEL (brasero_data_vfs_get_filtered_model (BRASERO_DATA_VFS (priv->tree)));
	g_object_ref (model);
	return model;
}

void
brasero_track_data_cfg_restore (BraseroTrackDataCfg *track,
				GtkTreePath *treepath)
{
	BraseroTrackDataCfgPrivate *priv;
	BraseroFilteredUri *filtered;
	gchar *uri;

	g_return_if_fail (BRASERO_TRACK_DATA_CFG (track));
	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);

	filtered = brasero_data_vfs_get_filtered_model (BRASERO_DATA_VFS (priv->tree));
	uri = brasero_filtered_uri_restore (filtered, treepath);

	brasero_data_project_restore_uri (BRASERO_DATA_PROJECT (priv->tree), uri);
	g_free (uri);
}

void
brasero_track_data_cfg_dont_filter_uri (BraseroTrackDataCfg *track,
					const gchar *uri)
{
	BraseroTrackDataCfgPrivate *priv;
	BraseroFilteredUri *filtered;

	g_return_if_fail (BRASERO_TRACK_DATA_CFG (track));
	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);

	filtered = brasero_data_vfs_get_filtered_model (BRASERO_DATA_VFS (priv->tree));
	brasero_filtered_uri_dont_filter (filtered, uri);
	brasero_data_project_restore_uri (BRASERO_DATA_PROJECT (priv->tree), uri);
}

GSList *
brasero_track_data_cfg_get_restored_list (BraseroTrackDataCfg *track)
{
	BraseroTrackDataCfgPrivate *priv;
	BraseroFilteredUri *filtered;

	g_return_val_if_fail (BRASERO_TRACK_DATA_CFG (track), NULL);
	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);

	filtered = brasero_data_vfs_get_filtered_model (BRASERO_DATA_VFS (priv->tree));
	return brasero_filtered_uri_get_restored_list (filtered);
}

gboolean
brasero_track_data_cfg_load_medium (BraseroTrackDataCfg *track,
				    BraseroMedium *medium,
				    GError **error)
{
	BraseroTrackDataCfgPrivate *priv;

	g_return_val_if_fail (BRASERO_TRACK_DATA_CFG (track), FALSE);
	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);
	return brasero_data_session_add_last (BRASERO_DATA_SESSION (priv->tree),
					      medium,
					      error);
}

void
brasero_track_data_cfg_unload_current_medium (BraseroTrackDataCfg *track)
{
	BraseroTrackDataCfgPrivate *priv;

	g_return_if_fail (BRASERO_TRACK_DATA_CFG (track));
	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);
	brasero_data_session_remove_last (BRASERO_DATA_SESSION (priv->tree));
}

BraseroMedium *
brasero_track_data_cfg_get_current_medium (BraseroTrackDataCfg *track)
{
	BraseroTrackDataCfgPrivate *priv;

	g_return_val_if_fail (BRASERO_TRACK_DATA_CFG (track), NULL);
	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);
	return brasero_data_session_get_loaded_medium (BRASERO_DATA_SESSION (priv->tree));
}

GSList *
brasero_track_data_cfg_get_available_media (BraseroTrackDataCfg *track)
{
	BraseroTrackDataCfgPrivate *priv;

	g_return_val_if_fail (BRASERO_TRACK_DATA_CFG (track), NULL);
	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);
	return brasero_data_session_get_available_media (BRASERO_DATA_SESSION (priv->tree));
}

static BraseroBurnResult
brasero_track_data_cfg_set_source (BraseroTrackData *track,
				   GSList *grafts,
				   GSList *excluded)
{
	BraseroTrackDataCfgPrivate *priv;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);
	if (!grafts)
		return BRASERO_BURN_ERR;

	priv->loading = brasero_data_project_load_contents (BRASERO_DATA_PROJECT (priv->tree),
							    grafts,
							    excluded);

	if (!priv->loading)
		return BRASERO_BURN_OK;

	return BRASERO_BURN_NOT_READY;
}

static BraseroImageFS
brasero_track_data_cfg_get_fs (BraseroTrackData *track)
{
	BraseroTrackDataCfgPrivate *priv;
	BraseroFileTreeStats *stats;
	BraseroImageFS fs_type;
	BraseroFileNode *root;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);

	root = brasero_data_project_get_root (BRASERO_DATA_PROJECT (priv->tree));
	stats = BRASERO_FILE_NODE_STATS (root);

	fs_type = BRASERO_IMAGE_FS_ISO;
	if (brasero_data_project_has_symlinks (BRASERO_DATA_PROJECT (priv->tree)))
		fs_type |= BRASERO_IMAGE_FS_SYMLINK;
	else {
		/* These two are incompatible with symlinks */
		if (brasero_data_project_is_joliet_compliant (BRASERO_DATA_PROJECT (priv->tree)))
			fs_type |= BRASERO_IMAGE_FS_JOLIET;

		if (brasero_data_project_is_video_project (BRASERO_DATA_PROJECT (priv->tree)))
			fs_type |= BRASERO_IMAGE_FS_VIDEO;
	}

	if (stats->num_2GiB != 0) {
		fs_type |= BRASERO_IMAGE_ISO_FS_LEVEL_3;
		if (!(fs_type & BRASERO_IMAGE_FS_SYMLINK))
			fs_type |= BRASERO_IMAGE_FS_UDF;
	}

	if (stats->num_deep != 0)
		fs_type |= BRASERO_IMAGE_ISO_FS_DEEP_DIRECTORY;

	return fs_type;
}

static GSList *
brasero_track_data_cfg_get_grafts (BraseroTrackData *track)
{
	BraseroTrackDataCfgPrivate *priv;
	BraseroImageFS fs_type;
	GSList *grafts = NULL;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);

	/* append a slash for mkisofs */
	fs_type = brasero_track_data_cfg_get_fs (track);
	brasero_data_project_get_contents (BRASERO_DATA_PROJECT (priv->tree),
					   &grafts,
					   NULL,
					   (fs_type & BRASERO_IMAGE_FS_JOLIET) != 0,
					   TRUE);
	return grafts;
}

static GSList *
brasero_track_data_cfg_get_excluded (BraseroTrackData *track)
{
	BraseroTrackDataCfgPrivate *priv;
	GSList *unreadable = NULL;
	BraseroImageFS fs_type;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);

	/* append a slash for mkisofs */
	fs_type = brasero_track_data_cfg_get_fs (track);
	brasero_data_project_get_contents (BRASERO_DATA_PROJECT (priv->tree),
					   NULL,
					   &unreadable,
					   (fs_type & BRASERO_IMAGE_FS_JOLIET) != 0,
					   TRUE);
	return unreadable;
}

static guint64
brasero_track_data_cfg_get_file_num (BraseroTrackData *track)
{
	BraseroTrackDataCfgPrivate *priv;
	BraseroFileTreeStats *stats;
	BraseroFileNode *root;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);

	root = brasero_data_project_get_root (BRASERO_DATA_PROJECT (priv->tree));
	stats = BRASERO_FILE_NODE_STATS (root);

	return stats->children;
}

static BraseroTrackDataType
brasero_track_data_cfg_get_track_type (BraseroTrack *track,
				       BraseroTrackType *type)
{
	BraseroTrackDataCfgPrivate *priv;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);

	if (!type)
		return BRASERO_TRACK_TYPE_DATA;

	brasero_track_type_set_has_data (type);
	brasero_track_type_set_data_fs (type, brasero_track_data_cfg_get_fs (BRASERO_TRACK_DATA (track)));

	return BRASERO_TRACK_TYPE_DATA;
}

static BraseroBurnResult
brasero_track_data_cfg_get_status (BraseroTrack *track,
				   BraseroStatus *status)
{
	BraseroTrackDataCfgPrivate *priv;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);

	if (priv->loading) {
		brasero_status_set_not_ready (status,
					      (gdouble) (priv->loading - priv->loading_remaining) / (gdouble) priv->loading,
					      _("Analysing files"));
		return BRASERO_BURN_NOT_READY;
	}

	/* This one goes before the next since a node may be loading but not
	 * yet in the project and therefore project will look empty */
	if (brasero_data_vfs_is_active (BRASERO_DATA_VFS (priv->tree))) {
		if (status)
			brasero_status_set_not_ready (status,
						      -1.0,
						      _("Analysing files"));

		return BRASERO_BURN_NOT_READY;
	}

	if (priv->load_errors) {
		g_slist_foreach (priv->load_errors, (GFunc) g_free, NULL);
		g_slist_free (priv->load_errors);
		priv->load_errors = NULL;
		return BRASERO_BURN_ERR;
	}

	if (brasero_data_project_is_empty (BRASERO_DATA_PROJECT (priv->tree)))
		return BRASERO_BURN_ERR;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_track_data_cfg_get_size (BraseroTrack *track,
				 goffset *blocks,
				 goffset *block_size)
{
	BraseroTrackDataCfgPrivate *priv;
	goffset sectors = 0;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);

	sectors = brasero_data_project_get_sectors (BRASERO_DATA_PROJECT (priv->tree));
	if (blocks)
		*blocks = sectors;

	if (block_size)
		*block_size = 2048;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_track_data_cfg_image_uri_cb (BraseroDataVFS *vfs,
				     const gchar *uri,
				     BraseroTrackDataCfg *self)
{
	BraseroTrackDataCfgPrivate *priv;
	GValue instance_and_params [2];
	GValue return_value;
	GValue *params;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (self);
	if (priv->loading)
		return BRASERO_BURN_OK;

	/* object which signalled */
	instance_and_params->g_type = 0;
	g_value_init (instance_and_params, G_TYPE_FROM_INSTANCE (self));
	g_value_set_instance (instance_and_params, self);

	/* arguments of signal (name) */
	params = instance_and_params + 1;
	params->g_type = 0;
	g_value_init (params, G_TYPE_STRING);
	g_value_set_string (params, uri);

	/* default to CANCEL */
	return_value.g_type = 0;
	g_value_init (&return_value, G_TYPE_INT);
	g_value_set_int (&return_value, BRASERO_BURN_CANCEL);

	g_signal_emitv (instance_and_params,
			brasero_track_data_cfg_signals [IMAGE],
			0,
			&return_value);

	g_value_unset (instance_and_params);
	g_value_unset (params);

	return g_value_get_int (&return_value);
}

static void
brasero_track_data_cfg_unreadable_uri_cb (BraseroDataVFS *vfs,
					  const GError *error,
					  const gchar *uri,
					  BraseroTrackDataCfg *self)
{
	BraseroTrackDataCfgPrivate *priv;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (self);

	if (priv->loading) {
		priv->load_errors = g_slist_prepend (priv->load_errors,
						     g_strdup (error->message));

		return;
	}

	g_signal_emit (self,
		       brasero_track_data_cfg_signals [UNREADABLE],
		       0,
		       error,
		       uri);
}

static void
brasero_track_data_cfg_recursive_uri_cb (BraseroDataVFS *vfs,
					 const gchar *uri,
					 BraseroTrackDataCfg *self)
{
	BraseroTrackDataCfgPrivate *priv;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (self);

	if (priv->loading) {
		gchar *message;
		gchar *name;

		name = brasero_utils_get_uri_name (uri);
		message = g_strdup_printf (_("\"%s\" is a recursive symlink."), name);
		priv->load_errors = g_slist_prepend (priv->load_errors, message);
		g_free (name);

		return;
	}
	g_signal_emit (self,
		       brasero_track_data_cfg_signals [RECURSIVE],
		       0,
		       uri);
}

static void
brasero_track_data_cfg_unknown_uri_cb (BraseroDataVFS *vfs,
				       const gchar *uri,
				       BraseroTrackDataCfg *self)
{
	BraseroTrackDataCfgPrivate *priv;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (self);

	if (priv->loading) {
		gchar *message;
		gchar *name;

		name = brasero_utils_get_uri_name (uri);
		message = g_strdup_printf (_("\"%s\" cannot be found."), name);
		priv->load_errors = g_slist_prepend (priv->load_errors, message);
		g_free (name);

		return;
	}
	g_signal_emit (self,
		       brasero_track_data_cfg_signals [UNKNOWN],
		       0,
		       uri);
}

static gboolean
brasero_track_data_cfg_name_collision_cb (BraseroDataProject *project,
					  const gchar *name,
					  BraseroTrackDataCfg *self)
{
	BraseroTrackDataCfgPrivate *priv;
	gboolean result;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (self);

	if (priv->loading) {
		/* don't do anything accept replacement */
		return FALSE;
	}

	g_signal_emit (self,
		       brasero_track_data_cfg_signals [NAME_COLLISION],
		       0,
		       name,
		       &result);
	return result;
}

static gboolean
brasero_track_data_cfg_deep_directory (BraseroDataProject *project,
				       const gchar *string,
				       BraseroTrackDataCfg *self)
{
	BraseroTrackDataCfgPrivate *priv;
	gboolean result;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (self);

	if (priv->deep_directory)
		return FALSE;

	if (priv->loading) {
		/* don't do anything just accept these directories from now on */
		priv->deep_directory = TRUE;
		return FALSE;
	}

	g_signal_emit (self,
		       brasero_track_data_cfg_signals [DEEP_DIRECTORY],
		       0,
		       string,
		       &result);

	priv->deep_directory = result;
	return result;
}

static gboolean
brasero_track_data_cfg_2G_file (BraseroDataProject *project,
				const gchar *string,
				BraseroTrackDataCfg *self)
{
	BraseroTrackDataCfgPrivate *priv;
	gboolean result;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (self);

	/* This is to make sure we asked once */
	if (priv->G2_files)
		return FALSE;

	if (priv->loading) {
		/* don't do anything just accept these files from now on */
		priv->G2_files = TRUE;
		return FALSE;
	}

	g_signal_emit (self,
		       brasero_track_data_cfg_signals [G2_FILE],
		       0,
		       string,
		       &result);
	priv->G2_files = result;
	return result;
}

static void
brasero_track_data_cfg_project_loaded (BraseroDataProject *project,
				       gint loading,
				       BraseroTrackDataCfg *self)
{
	BraseroTrackDataCfgPrivate *priv;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (self);

	priv->loading_remaining = loading;
	if (loading > 0) {
		gdouble progress;

		progress = (gdouble) (priv->loading - priv->loading_remaining) / (gdouble) priv->loading;
		g_signal_emit (self,
			       brasero_track_data_cfg_signals [SOURCE_LOADING],
			       0,
			       progress);
		return;
	}

	priv->loading = 0;
	g_signal_emit (self,
		       brasero_track_data_cfg_signals [SOURCE_LOADED],
		       0,
		       priv->load_errors);
}

static void
brasero_track_data_cfg_activity_changed (BraseroDataVFS *vfs,
					 gboolean active,
					 BraseroTrackDataCfg *self)
{
	GSList *nodes;
	GtkTreeIter iter;
	BraseroTrackDataCfgPrivate *priv;

	if (brasero_data_vfs_is_active (vfs))
		goto emit_signal;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (self);

	/* This is used when we finished exploring so we can update BRASERO_DATA_TREE_MODEL_PERCENT */
	iter.stamp = priv->stamp;
	iter.user_data2 = GINT_TO_POINTER (BRASERO_ROW_REGULAR);

	/* NOTE: we shouldn't need to use reference here as unref_node is used */
	for (nodes = priv->shown; nodes; nodes = nodes->next) {
		GtkTreePath *treepath;

		iter.user_data = nodes->data;
		treepath = brasero_track_data_cfg_node_to_path (self, nodes->data);
		gtk_tree_model_row_changed (GTK_TREE_MODEL (self), treepath, &iter);
		gtk_tree_path_free (treepath);
	}

emit_signal:

	g_signal_emit (self,
		       brasero_track_data_cfg_signals [ACTIVITY],
		       0,
		       active);

	brasero_track_changed (BRASERO_TRACK (self));
}

static void
brasero_track_data_cfg_oversized_cb (BraseroDataProject *project,
				     gboolean is_oversized,
				     gboolean overburn_possible,
				     BraseroTrackDataCfg *self)
{
	g_signal_emit (self,
		       brasero_track_data_cfg_signals [OVERSIZE],
		       0,
		       is_oversized,
		       overburn_possible);
}

static void
brasero_track_data_cfg_size_changed_cb (BraseroDataProject *project,
					BraseroTrackDataCfg *self)
{
	brasero_track_changed (BRASERO_TRACK (self));
}

static void
brasero_track_data_cfg_session_available_cb (BraseroDataSession *session,
					     BraseroMedium *medium,
					     gboolean available,
					     BraseroTrackDataCfg *self)
{
	g_signal_emit (self,
		       brasero_track_data_cfg_signals [OVERSIZE],
		       0,
		       medium,
		       available);
}

static void
brasero_track_data_cfg_session_loaded_cb (BraseroDataSession *session,
					  BraseroMedium *medium,
					  gboolean loaded,
					  BraseroTrackDataCfg *self)
{
	g_signal_emit (self,
		       brasero_track_data_cfg_signals [OVERSIZE],
		       0,
		       medium,
		       loaded);
}

static void
brasero_track_data_cfg_init (BraseroTrackDataCfg *object)
{
	BraseroTrackDataCfgPrivate *priv;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (object);

	priv->sort_column = GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID;
	do {
		priv->stamp = g_random_int ();
	} while (!priv->stamp);

	priv->theme = gtk_icon_theme_get_default ();
	priv->tree = brasero_data_tree_model_new ();

	g_signal_connect (priv->tree,
			  "row-added",
			  G_CALLBACK (brasero_track_data_cfg_node_added),
			  object);
	g_signal_connect (priv->tree,
			  "row-changed",
			  G_CALLBACK (brasero_track_data_cfg_node_changed),
			  object);
	g_signal_connect (priv->tree,
			  "row-removed",
			  G_CALLBACK (brasero_track_data_cfg_node_removed),
			  object);
	g_signal_connect (priv->tree,
			  "rows-reordered",
			  G_CALLBACK (brasero_track_data_cfg_node_reordered),
			  object);

	g_signal_connect (priv->tree,
			  "oversize",
			  G_CALLBACK (brasero_track_data_cfg_oversized_cb),
			  object);
	g_signal_connect (priv->tree,
			  "size-changed",
			  G_CALLBACK (brasero_track_data_cfg_size_changed_cb),
			  object);

	g_signal_connect (priv->tree,
			  "session-available",
			  G_CALLBACK (brasero_track_data_cfg_session_available_cb),
			  object);
	g_signal_connect (priv->tree,
			  "session-loaded",
			  G_CALLBACK (brasero_track_data_cfg_session_loaded_cb),
			  object);
	
	g_signal_connect (priv->tree,
			  "project-loaded",
			  G_CALLBACK (brasero_track_data_cfg_project_loaded),
			  object);
	g_signal_connect (priv->tree,
			  "vfs-activity",
			  G_CALLBACK (brasero_track_data_cfg_activity_changed),
			  object);
	g_signal_connect (priv->tree,
			  "deep-directory",
			  G_CALLBACK (brasero_track_data_cfg_deep_directory),
			  object);
	g_signal_connect (priv->tree,
			  "2G-file",
			  G_CALLBACK (brasero_track_data_cfg_2G_file),
			  object);
	g_signal_connect (priv->tree,
			  "unreadable-uri",
			  G_CALLBACK (brasero_track_data_cfg_unreadable_uri_cb),
			  object);
	g_signal_connect (priv->tree,
			  "unknown-uri",
			  G_CALLBACK (brasero_track_data_cfg_unknown_uri_cb),
			  object);
	g_signal_connect (priv->tree,
			  "recursive-sym",
			  G_CALLBACK (brasero_track_data_cfg_recursive_uri_cb),
			  object);
	g_signal_connect (priv->tree,
			  "image-uri",
			  G_CALLBACK (brasero_track_data_cfg_image_uri_cb),
			  object);
	g_signal_connect (priv->tree,
			  "name-collision",
			  G_CALLBACK (brasero_track_data_cfg_name_collision_cb),
			  object);
}

static void
brasero_track_data_cfg_class_init (BraseroTrackDataCfgClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroTrackClass *track_class = BRASERO_TRACK_CLASS (klass);
	BraseroTrackDataClass *parent_class = BRASERO_TRACK_DATA_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroTrackDataCfgPrivate));

	object_class->finalize = brasero_track_data_cfg_finalize;

	track_class->get_size = brasero_track_data_cfg_get_size;
	track_class->get_type = brasero_track_data_cfg_get_track_type;
	track_class->get_status = brasero_track_data_cfg_get_status;

	parent_class->set_source = brasero_track_data_cfg_set_source;

	parent_class->get_fs = brasero_track_data_cfg_get_fs;
	parent_class->get_grafts = brasero_track_data_cfg_get_grafts;
	parent_class->get_excluded = brasero_track_data_cfg_get_excluded;
	parent_class->get_file_num = brasero_track_data_cfg_get_file_num;

	brasero_track_data_cfg_signals [AVAILABLE] = 
	    g_signal_new ("session_available",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST,
			  0,
			  NULL, NULL,
			  brasero_marshal_VOID__OBJECT_BOOLEAN,
			  G_TYPE_NONE,
			  2,
			  G_TYPE_OBJECT,
			  G_TYPE_BOOLEAN);
	brasero_track_data_cfg_signals [LOADED] = 
	    g_signal_new ("session_loaded",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST,
			  0,
			  NULL, NULL,
			  brasero_marshal_VOID__OBJECT_BOOLEAN,
			  G_TYPE_NONE,
			  2,
			  G_TYPE_OBJECT,
			  G_TYPE_BOOLEAN);
	brasero_track_data_cfg_signals [OVERSIZE] = 
	    g_signal_new ("session_oversized",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST,
			  0,
			  NULL, NULL,
			  brasero_marshal_VOID__BOOLEAN_BOOLEAN,
			  G_TYPE_NONE,
			  2,
			  G_TYPE_BOOLEAN,
			  G_TYPE_BOOLEAN);

	brasero_track_data_cfg_signals [ACTIVITY] = 
	    g_signal_new ("vfs_activity",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_FIRST|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__BOOLEAN,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_BOOLEAN);

	brasero_track_data_cfg_signals [IMAGE] = 
	    g_signal_new ("image_uri",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL, NULL,
			  brasero_marshal_INT__STRING,
			  G_TYPE_INT,
			  1,
			  G_TYPE_STRING);

	brasero_track_data_cfg_signals [UNREADABLE] = 
	    g_signal_new ("unreadable_uri",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_FIRST,
			  0,
			  NULL, NULL,
			  brasero_marshal_VOID__POINTER_STRING,
			  G_TYPE_NONE,
			  2,
			  G_TYPE_POINTER,
			  G_TYPE_STRING);

	brasero_track_data_cfg_signals [RECURSIVE] = 
	    g_signal_new ("recursive_sym",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_FIRST,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__STRING,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_STRING);

	brasero_track_data_cfg_signals [UNKNOWN] = 
	    g_signal_new ("unknown_uri",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_FIRST,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__STRING,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_STRING);
	brasero_track_data_cfg_signals [G2_FILE] = 
	    g_signal_new ("2G_file",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL, NULL,
			  brasero_marshal_BOOLEAN__STRING,
			  G_TYPE_BOOLEAN,
			  1,
			  G_TYPE_STRING);
	brasero_track_data_cfg_signals [NAME_COLLISION] = 
	    g_signal_new ("name_collision",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL, NULL,
			  brasero_marshal_BOOLEAN__STRING,
			  G_TYPE_BOOLEAN,
			  1,
			  G_TYPE_STRING);
	brasero_track_data_cfg_signals [DEEP_DIRECTORY] = 
	    g_signal_new ("deep_directory",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL, NULL,
			  brasero_marshal_BOOLEAN__STRING,
			  G_TYPE_BOOLEAN,
			  1,
			  G_TYPE_STRING);

	brasero_track_data_cfg_signals [SOURCE_LOADING] = 
	    g_signal_new ("source_loading",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__DOUBLE,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_DOUBLE);
	brasero_track_data_cfg_signals [SOURCE_LOADED] = 
	    g_signal_new ("source_loaded",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__POINTER,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_POINTER);
}

BraseroTrackDataCfg *
brasero_track_data_cfg_new (void)
{
	return g_object_new (BRASERO_TYPE_TRACK_DATA_CFG, NULL);
}
