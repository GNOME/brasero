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

#include <string.h>
#include <stdio.h>
#include <libgen.h>
#include <sys/param.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <gio/gio.h>

#include "brasero-units.h"

#include "brasero-data-project.h"
#include "libbrasero-marshal.h"

#include "brasero-misc.h"
#include "brasero-io.h"

#include "burn-debug.h"
#include "brasero-track-data.h"

typedef struct _BraseroDataProjectPrivate BraseroDataProjectPrivate;
struct _BraseroDataProjectPrivate
{
	BraseroFileNode *root;

	GCompareFunc sort_func;
	GtkSortType sort_type;

	GSList *spanned;

	/**
	 * In this table we record all changes (key = URI, data = list
	 * of nodes) that is:
	 * - files actually grafted (don't have a URI parent in the tree/table)
	 * - name changes for any node (whether it be because of invalid utf8
	 *   or because it was changed by the user.
	 * - files that were removed/moved somewhere else in the tree
	 * - unreadable files
	 * All these URIs/addresses should be excluded first. Then for each node
	 * there is in the list a graft point should be added. If there isn't 
	 * any node then that means that the file/URI will not appear in the 
	 * image */
	GHashTable *grafts;
	GHashTable *reference;

	GHashTable *joliet;

	guint ref_count;

	/* This is a counter for the number of files to be loaded */
	guint loading;

	guint is_loading_contents:1;
};

#define BRASERO_DATA_PROJECT_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_DATA_PROJECT, BraseroDataProjectPrivate))

#ifdef BUILD_INOTIFY

#include "brasero-file-monitor.h"

G_DEFINE_TYPE (BraseroDataProject, brasero_data_project, BRASERO_TYPE_FILE_MONITOR);

#else

G_DEFINE_TYPE (BraseroDataProject, brasero_data_project, G_TYPE_OBJECT);

#endif


enum {
	JOLIET_RENAME_SIGNAL,
	NAME_COLLISION_SIGNAL,
	SIZE_CHANGED_SIGNAL,
	DEEP_DIRECTORY_SIGNAL,
	G2_FILE_SIGNAL,
	PROJECT_LOADED_SIGNAL,
	VIRTUAL_SIBLING_SIGNAL,
	LAST_SIGNAL
};

static guint brasero_data_project_signals [LAST_SIGNAL] = {0};

/**
 * This is used in grafts hash table to identify created directories
 */
static const gchar NEW_FOLDER [] = "NewFolder";


typedef gboolean	(*BraseroDataNodeAddedFunc)	(BraseroDataProject *project,
							 BraseroFileNode *node,
							 const gchar *uri);

static void
brasero_data_project_virtual_sibling (BraseroDataProject *self,
				      BraseroFileNode *node,
				      BraseroFileNode *sibling)
{
	BraseroDataProjectPrivate *priv;
	BraseroFileTreeStats *stats;
	BraseroFileNode *children;
	BraseroFileNode *iter;

	if (sibling == node)
		return;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	g_signal_emit (self,
		       brasero_data_project_signals [VIRTUAL_SIBLING_SIGNAL],
		       0,
		       node,
		       sibling);

	stats = brasero_file_node_get_tree_stats (priv->root, NULL);
	if (node) {
		/* we remove the virtual node, BUT, we keep its
		 * virtual children that will be appended to the
		 * node being moved in replacement. */
		/* NOTE: children MUST all be virtual */
		children = BRASERO_FILE_NODE_CHILDREN (sibling);
		for (iter = children; iter; iter = iter->next)
			brasero_file_node_add (node, iter, NULL);

		sibling->union2.children = NULL;
	}
	else {
		/* Remove the virtual node. This should never happens */
		g_warning ("Virtual nodes could not be transfered");
	}

	/* Just destroy the node as it has no other 
	 * existence nor goal in existence but to create
	 * a collision. */
	brasero_file_node_destroy (sibling, stats);
}

static gboolean
brasero_data_project_node_signal (BraseroDataProject *self,
				  guint signal,
				  BraseroFileNode *node)
{
	GValue instance_and_params [2];
	GValue return_value;
	GValue *params;

	/* object which signalled */
	instance_and_params->g_type = 0;
	g_value_init (instance_and_params, G_TYPE_FROM_INSTANCE (self));
	g_value_set_instance (instance_and_params, self);

	/* arguments of signal (name) */
	params = instance_and_params + 1;
	params->g_type = 0;
	g_value_init (params, G_TYPE_POINTER);
	g_value_set_pointer (params, node);

	/* default to FALSE */
	return_value.g_type = 0;
	g_value_init (&return_value, G_TYPE_BOOLEAN);
	g_value_set_boolean (&return_value, FALSE);

	g_signal_emitv (instance_and_params,
			brasero_data_project_signals [signal],
			0,
			&return_value);

	g_value_unset (instance_and_params);
	g_value_unset (params);

	/* In this case always remove the sibling */
	if (signal == NAME_COLLISION_SIGNAL && BRASERO_FILE_NODE_VIRTUAL (node))
		return FALSE;

	return g_value_get_boolean (&return_value);
}

static gboolean
brasero_data_project_file_signal (BraseroDataProject *self,
				  guint signal,
				  const gchar *name)
{
	GValue instance_and_params [2];
	GValue return_value;
	GValue *params;

	/* object which signalled */
	instance_and_params->g_type = 0;
	g_value_init (instance_and_params, G_TYPE_FROM_INSTANCE (self));
	g_value_set_instance (instance_and_params, self);

	/* arguments of signal (name) */
	params = instance_and_params + 1;
	params->g_type = 0;
	g_value_init (params, G_TYPE_STRING);
	g_value_set_string (params, name);

	/* default to FALSE */
	return_value.g_type = 0;
	g_value_init (&return_value, G_TYPE_BOOLEAN);
	g_value_set_boolean (&return_value, FALSE);

	g_signal_emitv (instance_and_params,
			brasero_data_project_signals [signal],
			0,
			&return_value);

	g_value_unset (instance_and_params);
	g_value_unset (params);

	return g_value_get_boolean (&return_value);
}

/**
 * Manages the references to a node
 */

guint
brasero_data_project_reference_new (BraseroDataProject *self,
				    BraseroFileNode *node)
{
	BraseroDataProjectPrivate *priv;
	guint retval;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	retval = priv->ref_count;
	while (g_hash_table_lookup (priv->reference, GINT_TO_POINTER (retval))) {
		retval ++;

		if (retval == G_MAXINT)
			retval = 1;

		/* this means there is no more room for reference */
		if (retval == priv->ref_count)
			return 0;
	}

	g_hash_table_insert (priv->reference,
			     GINT_TO_POINTER (retval),
			     node);
	priv->ref_count = retval + 1;
	if (priv->ref_count == G_MAXINT)
		priv->ref_count = 1;

	return retval;
}

void
brasero_data_project_reference_free (BraseroDataProject *self,
				     guint reference)
{
	BraseroDataProjectPrivate *priv;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);
	g_hash_table_remove (priv->reference, GINT_TO_POINTER (reference));
}

BraseroFileNode *
brasero_data_project_reference_get (BraseroDataProject *self,
				    guint reference)
{
	BraseroDataProjectPrivate *priv;

	/* if it was invalidated then the node returned is NULL */
	priv = BRASERO_DATA_PROJECT_PRIVATE (self);
	return g_hash_table_lookup (priv->reference, GINT_TO_POINTER (reference));
}

static gboolean
brasero_data_project_reference_remove_children_cb (gpointer key,
						   gpointer data,
						   gpointer callback_data)
{
	BraseroFileNode *node = data;
	BraseroFileNode *parent = callback_data;

	if (brasero_file_node_is_ancestor (parent, node))
		return TRUE;

	return FALSE;
}

static void
brasero_data_project_reference_invalidate (BraseroDataProject *self,
					   BraseroFileNode *node)
{
	BraseroDataProjectPrivate *priv;

	/* used internally to invalidate reference whose node was removed */
	priv = BRASERO_DATA_PROJECT_PRIVATE (self);
	g_hash_table_foreach_remove (priv->reference,
				     (GHRFunc) brasero_data_project_reference_remove_children_cb,
				     node);
}


/**
 * Manages the Joliet incompatible names 
 */
struct _BraseroJolietKey {
	BraseroFileNode *parent;
	gchar name [65];
};
typedef struct _BraseroJolietKey BraseroJolietKey;

static guint
brasero_data_project_joliet_hash (gconstpointer data)
{
	guint hash_node;
	guint hash_name;
	const BraseroJolietKey *key = data;

	hash_node = g_direct_hash (key->parent);	
	hash_name = g_str_hash (key->name);
	return hash_node + hash_name;
}

static gboolean
brasero_data_project_joliet_equal (gconstpointer a, gconstpointer b)
{
	const BraseroJolietKey *key1 = a;
	const BraseroJolietKey *key2 = b;

	if (key1->parent != key2->parent)
		return FALSE;

	if (strcmp (key1->name, key2->name))
		return FALSE;

	return TRUE;
}

static void
brasero_data_project_joliet_set_key (BraseroJolietKey *key,
				     BraseroFileNode *node)
{
	gchar *dot;
	guint extension_len;

	/* key is equal to the parent path and the 64 first characters
	 * (always including the extension) of the name */
	dot = g_utf8_strrchr (BRASERO_FILE_NODE_NAME (node), -1, '.');

	if (dot)
		extension_len = strlen (dot);
	else
		extension_len = 0;

	if (dot && extension_len > 1 && extension_len < 5)
		sprintf (key->name,
			 "%.*s%s",
			 64 - extension_len,
			 BRASERO_FILE_NODE_NAME (node),
			 dot);
	else
		sprintf (key->name,
			 "%.64s",
			 BRASERO_FILE_NODE_NAME (node));

	key->parent = node->parent;
}

static void
brasero_data_project_joliet_add_node (BraseroDataProject *self,
				      BraseroFileNode *node)
{
	BraseroDataProjectPrivate *priv;
	BraseroJolietKey key;
	GSList *list;

	if (!node->parent)
		return;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	if (!priv->joliet)
		priv->joliet = g_hash_table_new (brasero_data_project_joliet_hash,
						 brasero_data_project_joliet_equal);

	brasero_data_project_joliet_set_key (&key, node);
	list = g_hash_table_lookup (priv->joliet, &key);
	if (!list) {
		BraseroJolietKey *table_key;

		/* create the actual key if it isn't in the hash */
		table_key = g_new0 (BraseroJolietKey, 1);
		brasero_data_project_joliet_set_key (table_key, node);
		g_hash_table_insert (priv->joliet,
				     table_key,
				     g_slist_prepend (NULL, node));
	}
	else {
		list = g_slist_prepend (list, node);
		g_hash_table_insert (priv->joliet,
		                     &key,
		                     list);
	}

	/* Signal that we'll have a collision */
	g_signal_emit (self,
		       brasero_data_project_signals [JOLIET_RENAME_SIGNAL],
		       0);
}

static gboolean
brasero_data_project_joliet_remove_node (BraseroDataProject *self,
					 BraseroFileNode *node)
{
	BraseroDataProjectPrivate *priv;
	BraseroJolietKey key;
	gpointer hash_key;
	gboolean success;
	gpointer list;

	if (!node->parent)
		return FALSE;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	brasero_data_project_joliet_set_key (&key, node);
	success = g_hash_table_lookup_extended (priv->joliet,
						&key,
						&hash_key,
						&list);

	/* remove the exact path if it is a joliet non compliant file */
	if (!success)
		return FALSE;

	list = g_slist_remove (list, node);
	if (!list) {
		/* NOTE: we don't free the hash table now if it's empty,
		 * since this function could have been called by move
		 * function and in this case a path could probably be
		 * re-inserted */
		g_hash_table_remove (priv->joliet, &key);
		g_free (hash_key);
	}
	else
		g_hash_table_insert (priv->joliet,
				     &key,
				     list);

	return TRUE;
}

static gboolean
brasero_data_project_joliet_remove_children_node_cb (gpointer data_key,
						     gpointer data,
						     gpointer callback_data)
{
	BraseroFileNode *parent = callback_data;
	BraseroJolietKey *key = data_key;
	GSList *nodes = data;

	if (brasero_file_node_is_ancestor (parent, key->parent) || parent == key->parent) {
		g_slist_free (nodes);
		return TRUE;
	}

	return FALSE;
}

static void
brasero_data_project_joliet_remove_children_node (BraseroDataProject *self,
						  BraseroFileNode *parent)
{
	BraseroDataProjectPrivate *priv;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	if (!parent)
		parent = priv->root;

	g_hash_table_foreach_remove (priv->joliet,
				     brasero_data_project_joliet_remove_children_node_cb,
				     parent);
}

/**
 * Conversion functions
 */
gchar *
brasero_data_project_node_to_uri (BraseroDataProject *self,
				  BraseroFileNode *node)
{
	BraseroDataProjectPrivate *priv;
	GSList *list = NULL;
	gchar *retval;
	guint uri_len;
	GSList *iter;
	gchar *ptr;
	guint len;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	if (node->is_fake || node->is_imported)
		return NULL;

	if (node->is_grafted)
		return g_strdup (BRASERO_FILE_NODE_GRAFT (node)->node->uri);

	/* find the first grafted parent */
	uri_len = 0;
	list = NULL;
	for (; node; node = node->parent) {
		gchar *escaped_name;

		if (node->is_grafted)
			break;

		if (node == priv->root)
			break;

		/* the + 1 is for the separator */
		escaped_name = g_uri_escape_string (BRASERO_FILE_NODE_NAME (node),
						    G_URI_RESERVED_CHARS_ALLOWED_IN_PATH,
						    FALSE);
		uri_len += strlen (escaped_name) + 1;
		list = g_slist_prepend (list, escaped_name);
	}

	/* The node here is the first grafted parent */
	if (!node || node->is_root) {
		g_slist_foreach (list, (GFunc) g_free, NULL);
		g_slist_free (list);
		return NULL;
	}

	/* NOTE: directories URIs shouldn't have a separator at end */
	len = strlen (BRASERO_FILE_NODE_GRAFT (node)->node->uri);
	uri_len += len;

	retval = g_new (gchar, uri_len + 1);

	memcpy (retval, BRASERO_FILE_NODE_GRAFT (node)->node->uri, len);
	ptr = retval + len;

	for (iter = list; iter; iter = iter->next) {
		gchar *escaped_name;

		escaped_name = iter->data;

		ptr [0] = G_DIR_SEPARATOR;
		ptr ++;

		len = strlen (escaped_name);
		memcpy (ptr, escaped_name, len);
		ptr += len;
	}
	g_slist_foreach (list, (GFunc) g_free, NULL);
	g_slist_free (list);

	ptr [0] = '\0';
	return retval;
}
			  
static BraseroFileNode *
brasero_data_project_find_child_node (BraseroFileNode *node,
				      const gchar *path)
{
	gchar *end;
	guint len;

	/* skip the separator if any */
	if (path [0] == G_DIR_SEPARATOR)
		path ++;

	/* find the next separator if any */
	end = g_utf8_strchr (path, -1, G_DIR_SEPARATOR);

	if (end)
		len = end - path;
	else
		len = strlen (path);

	/* go through the children nodes and find the name */
	for (node = BRASERO_FILE_NODE_CHILDREN (node); node; node = node->next) {
		if (node
		&& !strncmp (BRASERO_FILE_NODE_NAME (node), path, len)
		&& (BRASERO_FILE_NODE_NAME (node) [len] == G_DIR_SEPARATOR
		||  BRASERO_FILE_NODE_NAME (node) [len] == '\0')) {
			if (end)
				return brasero_data_project_find_child_node (node, end);

			return node;
		}	
	}

	return NULL;
}

static GSList *
brasero_data_project_uri_to_nodes (BraseroDataProject *self,
				   const gchar *uri)
{
	BraseroDataProjectPrivate *priv;
	BraseroURINode *graft;
	GSList *nodes = NULL;
	gchar *parent;
	GSList *iter;
	gchar *path;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	/* see if it grafted. If so, return the list */
	graft = g_hash_table_lookup (priv->grafts, uri);
	if (graft)
		return g_slist_copy (graft->nodes);

	/* keep going up until we reach root URI in grafts */
	parent = g_path_get_dirname (uri);
	while (strcmp (parent, G_DIR_SEPARATOR_S) && strchr (parent, G_DIR_SEPARATOR)) {
		graft = g_hash_table_lookup (priv->grafts, parent);
		if (graft)
			break;

		parent = dirname (parent);
	}

	if (!graft) {
		/* no graft point was found; there isn't any node */
		g_free (parent);
		return NULL;
	}

	uri += strlen (parent);
	g_free (parent);

	/* unescape URI */
	path = g_uri_unescape_string (uri, NULL);
	for (iter = graft->nodes; iter; iter = iter->next) {
		BraseroFileNode *node;

		node = iter->data;

		/* find the child node starting from the grafted node */
		node = brasero_data_project_find_child_node (node, path);
		if (node)
			nodes = g_slist_prepend (nodes, node);
	}
	g_free (path);

	return nodes;
}

/**
 * Sorting
 * DataProject must be the one to handle that:
 * - BraseroFileNode can't send signal when something was reordered_id
 * - It is the object that adds files as a result of the exploration of a folder
 */

static void
brasero_data_project_node_changed (BraseroDataProject *self,
				   BraseroFileNode *node)
{
	gint *array;
	BraseroDataProjectClass *klass;
	BraseroDataProjectPrivate *priv;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);
	klass = BRASERO_DATA_PROJECT_GET_CLASS (self);

	if (klass->node_changed)
		klass->node_changed (self, node);

	array = brasero_file_node_need_resort (node, priv->sort_func);
	if (!array)
		return;

	if (klass->node_reordered)
		klass->node_reordered (self, node->parent, array);
	g_free (array);
}

static void
brasero_data_project_reorder_children (BraseroDataProject *self,
				       BraseroFileNode *parent)
{
	gint *array;
	BraseroDataProjectClass *klass;
	BraseroDataProjectPrivate *priv;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	array = brasero_file_node_sort_children (parent, priv->sort_func);

	if (!array)
		return;

	klass = BRASERO_DATA_PROJECT_GET_CLASS (self);
	if (klass->node_reordered)
		klass->node_reordered (self, parent, array);
	g_free (array);
}

static void
brasero_data_project_resort_tree (BraseroDataProject *self,
				  BraseroFileNode *parent)
{
	BraseroFileNode *iter;

	for (iter = BRASERO_FILE_NODE_CHILDREN (parent); iter; iter = iter->next) {
		if (iter->is_file)
			continue;

		brasero_data_project_reorder_children (self, iter);
		brasero_data_project_resort_tree (self, iter);
	}	
}

static void
brasero_data_project_reverse_children (BraseroDataProject *self,
				       BraseroFileNode *parent)
{
	gint *array;
	BraseroDataProjectClass *klass;

	array = brasero_file_node_reverse_children (parent);

	if (!array)
		return;

	klass = BRASERO_DATA_PROJECT_GET_CLASS (self);
	if (klass->node_reordered)
		klass->node_reordered (self, parent, array);
	g_free (array);
}

static void
brasero_data_project_reverse_tree (BraseroDataProject *self,
				   BraseroFileNode *parent)
{
	BraseroFileNode *iter;

	for (iter = BRASERO_FILE_NODE_CHILDREN (parent); iter; iter = iter->next) {
		if (iter->is_file)
			continue;

		brasero_data_project_reverse_children (self, iter);
		brasero_data_project_reverse_tree (self, iter);
	}	
}
void
brasero_data_project_set_sort_function (BraseroDataProject *self,
					GtkSortType sort_type,
					GCompareFunc sort_func)
{
	BraseroDataProjectPrivate *priv;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	if (priv->sort_func != sort_func) {
		priv->sort_func = sort_func;
		priv->sort_type = sort_type;

		/* resort all the tree */
		brasero_data_project_reorder_children (self, priv->root);
		brasero_data_project_resort_tree (self, priv->root);
	}
	else if (priv->sort_type != sort_type) {
		priv->sort_type = sort_type;
		brasero_data_project_reverse_children (self, priv->root);
		brasero_data_project_reverse_tree (self, priv->root);
	}
}

/**
 *
 */

static gboolean
brasero_data_project_uri_has_parent (BraseroDataProject *self,
				     const gchar *uri)
{
	BraseroDataProjectPrivate *priv;
	gchar *parent;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	parent = g_path_get_dirname (uri);

	/* keep going up until we reach a root URI */
	while (strcmp (parent, G_DIR_SEPARATOR_S) && strchr (parent, G_DIR_SEPARATOR)) {
		if (g_hash_table_lookup (priv->grafts, parent)) {
			g_free (parent);
			return TRUE;
		}

		parent = dirname (parent);
	}

	g_free (parent);
	return FALSE;
}

static gboolean
brasero_data_project_uri_is_graft_needed (BraseroDataProject *self,
					  const gchar *uri)
{
	BraseroDataProjectPrivate *priv;
	BraseroURINode *graft_parent;
	BraseroURINode *graft;
	gchar *unescaped;
	gchar *parent;
	GSList *iter;
	gchar *name;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	graft = g_hash_table_lookup (priv->grafts, uri);

	/* get the URI name and parent. NOTE: name is unescaped to fit
	 * the names of nodes that are meant for display and therefore
	 * also unescaped. It's not necessary for parent URI. */
	unescaped = g_uri_unescape_string (uri, NULL);
	name = g_path_get_basename (unescaped);
	g_free (unescaped);

	parent = g_path_get_dirname (uri);
	for (iter = graft->nodes; iter; iter = iter->next) {
		BraseroFileNode *node;
		gchar *parent_uri;

		node = iter->data;
		if (node->parent == priv->root) {
			g_free (parent);
			g_free (name);
			return TRUE;
		}

		if (node->parent->is_fake) {
			g_free (parent);
			g_free (name);
			return TRUE;
		}

		/* make sure the node has the right name. */
		if (strcmp (BRASERO_FILE_NODE_NAME (node), name)) {
			g_free (parent);
			g_free (name);
			return TRUE;
		}

		/* make sure the node has the right parent. */
		parent_uri = brasero_data_project_node_to_uri (self, node->parent);
		if (!parent_uri || strcmp (parent_uri, parent)) {
			g_free (parent_uri);
			g_free (parent);
			g_free (name);
			return TRUE;
		}

		g_free (parent_uri);
	}
	g_free (name);

	/* make sure no node is missing/removed. To do this find the 
	 * first parent URI in the hash and see if it has the same 
	 * number of graft point as this one. If not that means one
	 * node is missing. */
	graft_parent = g_hash_table_lookup (priv->grafts, parent);
	while (parent && !graft_parent) {
		parent = dirname (parent);
		graft_parent = g_hash_table_lookup (priv->grafts, parent);
	}
	g_free (parent);

	if (g_slist_length (graft_parent->nodes) != g_slist_length (graft->nodes))
		return TRUE;

	return FALSE;
}

static void
brasero_data_project_uri_remove_graft (BraseroDataProject *self,
				       const gchar *uri)
{
	BraseroDataProjectPrivate *priv;
	BraseroDataProjectClass *klass;
	BraseroURINode *graft = NULL;
	gchar *key = NULL;
	GSList *iter;
	GSList *next;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	g_hash_table_lookup_extended (priv->grafts,
				      uri,
				      (gpointer *) &key,
				      (gpointer *) &graft);

	/* Put the nodes in ungrafted state */
	for (iter = graft->nodes; iter; iter = next) {
		BraseroFileNode *iter_node;

		next = iter->next;
		iter_node = iter->data;
		brasero_file_node_ungraft (iter_node);
	}

	/* we have to free the key and data ourselves */
	g_hash_table_remove (priv->grafts, uri);

	klass = BRASERO_DATA_PROJECT_GET_CLASS (self);
	if (klass->uri_removed)
		klass->uri_removed (self, uri);

	if (key && key != NEW_FOLDER)
		brasero_utils_unregister_string (key);

	if (graft) {
		/* NOTE: no need to free graft->uri since that's the key */
		g_slist_free (graft->nodes);
		g_free (graft);
	}
}

static gboolean
brasero_data_project_graft_is_needed (BraseroDataProject *self,
				      BraseroURINode *uri_node)
{
	if (uri_node->nodes)
		return TRUE;

	/* there aren't any node grafted for this URI. See if we should keep the
	 * URI in the hash; if so, the URI must have parents in the hash */
	if (brasero_data_project_uri_has_parent (self, uri_node->uri)) {
		/* here that means that this URI is nowhere in the tree but has
		 * parent URIs which are grafted. So keep it in the hash to
		 * signal that URI is not in the tree. */
		return TRUE;
	}

	brasero_data_project_uri_remove_graft (self, uri_node->uri);
	return FALSE;
}

static BraseroURINode *
brasero_data_project_uri_add_graft (BraseroDataProject *self,
				    const gchar *uri)
{
	BraseroDataProjectPrivate *priv;
	BraseroURINode *graft;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	graft = g_new0 (BraseroURINode, 1);
	if (uri != NEW_FOLDER)
		graft->uri = brasero_utils_register_string (uri);
	else
		graft->uri = (gchar *) NEW_FOLDER;

	g_hash_table_insert (priv->grafts,
			     graft->uri,
			     graft);

	return graft;
}

static BraseroURINode *
brasero_data_project_uri_ensure_graft (BraseroDataProject *self,
				       const gchar *uri)
{
	BraseroDataProjectPrivate *priv;
	BraseroURINode *graft;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	graft = g_hash_table_lookup (priv->grafts, uri);
	if (graft)
		return graft;

	return brasero_data_project_uri_add_graft (self, uri);
}

static BraseroURINode *
brasero_data_project_uri_graft_nodes (BraseroDataProject *self,
				      const gchar *uri)
{
	BraseroURINode *graft;
	GSList *nodes;
	GSList *iter;

	/* Find all the nodes that should be grafted.
	 * NOTE: this must be done before asking for a new graft */
	nodes = brasero_data_project_uri_to_nodes (self, uri);
	graft = brasero_data_project_uri_add_graft (self, uri);

	/* NOTE: all nodes should have the exact same size. */

	/* Tell the nodes they are all grafted. */
	for (iter = nodes; iter; iter = iter->next) {
		BraseroFileNode *iter_node;

		iter_node = iter->data;
		brasero_file_node_graft (iter_node, graft);
	}
	g_slist_free (nodes);

	return graft;
}

static void
brasero_data_project_add_node_and_children (BraseroDataProject *self,
					    BraseroFileNode *node,
					    BraseroDataNodeAddedFunc klass_node_added_func)
{
	BraseroFileNode *iter;

	klass_node_added_func (self, node, NULL);

	/* now we probably have to call node_added on every single child */
	for (iter = BRASERO_FILE_NODE_CHILDREN (node); iter; iter = iter->next) {
		if (!iter->is_file)
			brasero_data_project_add_node_and_children (self, iter, klass_node_added_func);
		else
			klass_node_added_func (self, iter, NULL);
	}
}

struct _BraseroRemoveChildrenGraftData {
	BraseroFileNode *node;
	BraseroDataProject *project;
};
typedef struct _BraseroRemoveChildrenGraftData BraseroRemoveChildrenGraftData;

static gboolean
brasero_data_project_remove_node_children_graft_cb (const gchar *key,
						    BraseroURINode *graft,
						    BraseroRemoveChildrenGraftData *data)
{
	GSList *iter;
	GSList *next;

	/* Remove all children nodes of node.
	 * NOTE: here there is nothing to do about the size. */
	for (iter = graft->nodes; iter; iter = next) {
		BraseroFileNode *iter_node;

		iter_node = iter->data;
		next = iter->next;

		if (data->node == iter_node)
			continue;

		if (brasero_file_node_is_ancestor (data->node, iter_node))
			brasero_file_node_ungraft (iter_node);
	}

	if (graft->nodes)
		return FALSE;

	/* Check if this graft should be removed. If not, it should 
	 * have a parent URI in the graft. */
	return (brasero_data_project_uri_has_parent (data->project, key) == FALSE);
}

static void
brasero_data_project_remove_node_children_graft (BraseroDataProject *self,
						 BraseroFileNode *node)
{
	BraseroDataProjectPrivate *priv;
	BraseroRemoveChildrenGraftData callback_data;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	callback_data.project = self;
	callback_data.node = node;
	g_hash_table_foreach_remove (priv->grafts,
				     (GHRFunc) brasero_data_project_remove_node_children_graft_cb,
				     &callback_data);
}

#ifdef BUILD_INOTIFY

static gboolean
brasero_data_project_monitor_cancel_foreach_cb (gpointer data,
						gpointer callback_data)
{
	BraseroFileNode *node = data;
	BraseroFileNode *parent = callback_data;

	if (node == parent)
		return TRUE;

	return brasero_file_node_is_ancestor (parent, node);
}

#endif

static void
brasero_data_project_node_removed (BraseroDataProject *self,
				   BraseroFileNode *node)
{
	BraseroDataProjectPrivate *priv;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

#ifdef BUILD_INOTIFY

	/* remove all monitoring */
	if (node->is_monitored)
		brasero_file_monitor_foreach_cancel (BRASERO_FILE_MONITOR (self),
						     brasero_data_project_monitor_cancel_foreach_cb,
						     node);
#endif

	/* invalidate possible references (including for children)*/
	brasero_data_project_reference_invalidate (self, node);

	/* remove all children graft points; do it all at once. */
	brasero_data_project_remove_node_children_graft (self, node);

	/* remove all children nodes + node from the joliet table */
	brasero_data_project_joliet_remove_children_node (self, node);

	if (strlen (BRASERO_FILE_NODE_NAME (node)) > 64)
		brasero_data_project_joliet_remove_node (self, node);

	/* See if this node is grafted; if so remove it from the hash.
	 * If not, get the URI and all the nodes with the same URI and
	 * add the list (less this node) to the hash.
	 * NOTE: imported file case should not be addressed here*/
	if (node->is_grafted) {
		BraseroGraft *graft;
		BraseroURINode *uri_node;

		/* NOTE: in this case there is no size changes to do 
		 * for nodes or grafts. The size change for the whole
		 * project will be made during the addition of all the
		 * graft sizes. If there is no more nodes for this
		 * graft then it won't be taken into account. */

		/* There is already a graft */
		graft = BRASERO_FILE_NODE_GRAFT (node);
		uri_node = graft->node;

		/* NOTE: after this function the graft is invalid */
		brasero_file_node_ungraft (node);

		if (!uri_node->nodes) {
			/* that's the last node grafted for this URI.
			 * There are no more nodes for this URI after.
			 * See if we should keep the URI in the hash;
			 * if so, the URI must have parents in the hash
			 */
			if (!brasero_data_project_uri_has_parent (self, uri_node->uri))
				brasero_data_project_uri_remove_graft (self, uri_node->uri);
		}
	}
	else if (!node->is_imported) {
		gchar *uri;

		/* This URI will need a graft if it hasn't one yet */
		uri = brasero_data_project_node_to_uri (self, node);

		if (!g_hash_table_lookup (priv->grafts, uri))
			brasero_data_project_uri_graft_nodes (self, uri);

		/* NOTE: since the URI wasn't grafted it has to have a
		 * valid parent that's why we don't check the graft 
		 * validity afterwards */
		g_free (uri);
	}
}

static void
brasero_data_project_remove_real (BraseroDataProject *self,
				  BraseroFileNode *node)
{
	BraseroDataProjectPrivate *priv;
	BraseroDataProjectClass *klass;
	BraseroFileNode *former_parent;
	BraseroFileTreeStats *stats;
	guint former_position;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	brasero_data_project_node_removed (self, node);

	/* save parent, unparent it, signal the removal */
	former_parent = node->parent;
	former_position = brasero_file_node_get_pos_as_child (node);

	brasero_file_node_unlink (node);

	klass = BRASERO_DATA_PROJECT_GET_CLASS (self);
	if (klass->node_removed)
		klass->node_removed (self, former_parent, former_position, node);

	/* save imported nodes in their parent structure or destroy it */
	stats = brasero_file_node_get_tree_stats (priv->root, NULL);
	if (!node->is_imported)
		brasero_file_node_destroy (node, stats);
	else
		brasero_file_node_save_imported (node,
						 stats,
						 former_parent,
						 priv->sort_func);

	g_signal_emit (self,
		       brasero_data_project_signals [SIZE_CHANGED_SIGNAL],
		       0);
}

static void
brasero_data_project_convert_to_fake (BraseroDataProject *self,
				      BraseroFileNode *node)
{
	BraseroURINode *graft;
	BraseroDataProjectPrivate *priv;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	/* make it a fake directory not to break order */
	node->is_fake = TRUE;
	node->is_loading = FALSE;
	node->is_tmp_parent = FALSE;

	brasero_file_node_ungraft (node);
	graft = brasero_data_project_uri_ensure_graft (self, NEW_FOLDER);
	brasero_file_node_graft (node, graft);
	brasero_data_project_node_changed (self, node);

	/* Remove 2 since we're not going to load its contents */
	priv->loading -= 2;
	g_signal_emit (self,
		       brasero_data_project_signals [PROJECT_LOADED_SIGNAL],
		       0,
		       priv->loading);
}

void
brasero_data_project_remove_node (BraseroDataProject *self,
				  BraseroFileNode *node)
{
	BraseroFileNode *imported_sibling;
	BraseroDataProjectPrivate *priv;
	BraseroDataProjectClass *klass;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	if (node->is_tmp_parent) {
		/* This node was created as a temporary parent, it doesn't exist
		 * so we replace it with a fake one. */

		/* Don't exclude any URI since it doesn't exist apparently */

		/* No need to check for deep directory since that was in the
		 * project as such. Keep it that way. */

		brasero_data_project_convert_to_fake (self, node);
		return;
	}
	else if (priv->loading && node->is_grafted) {
		/* that means that's a grafted that failed to load */
		brasero_data_project_convert_to_fake (self, node);
		return;
	}

	/* check for a sibling now (before destruction) */
	imported_sibling = brasero_file_node_check_imported_sibling (node);
	brasero_data_project_remove_real (self, node);

	/* add the sibling now (after destruction) */
	if (!imported_sibling)
		return;

	klass = BRASERO_DATA_PROJECT_GET_CLASS (self);
	brasero_file_node_add (imported_sibling->parent, imported_sibling, priv->sort_func);
	brasero_data_project_add_node_and_children (self, imported_sibling, klass->node_added);
}

void
brasero_data_project_destroy_node (BraseroDataProject *self,
				   BraseroFileNode *node)
{
	BraseroDataProjectPrivate *priv;
	BraseroDataProjectClass *klass;
	BraseroFileNode *former_parent;
	BraseroFileTreeStats *stats;
	guint former_position;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	brasero_data_project_node_removed (self, node);

	/* unlink the node and signal the removal */
	former_parent = node->parent;
	former_position = brasero_file_node_get_pos_as_child (node);

	brasero_file_node_unlink (node);

	klass = BRASERO_DATA_PROJECT_GET_CLASS (self);
	if (klass->node_removed)
		klass->node_removed (self, former_parent, former_position, node);

	stats = brasero_file_node_get_tree_stats (priv->root, NULL);
	brasero_file_node_destroy (node, stats);

	g_signal_emit (self,
		       brasero_data_project_signals [SIZE_CHANGED_SIGNAL],
		       0);

	/* NOTE: no need to check for imported_sibling here since this function
	 * actually destroys all nodes including imported ones and is mainly 
	 * used to remove imported nodes. */
}

static gboolean
brasero_data_project_is_deep (BraseroDataProject *self,
			      BraseroFileNode *parent,
			      const gchar *name,
			      gboolean isfile)
{
	gint parent_depth;
	BraseroFileTreeStats *stats;
	BraseroDataProjectPrivate *priv;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	/* if there are already deep files accepts new ones (includes the 
	 * possible imported ones. */
	stats = brasero_file_node_get_tree_stats (priv->root, NULL);
	if (stats->num_deep)
		return TRUE;

	/* This node could have been moved beyond the depth 6 only in one case,
	 * which is with imported directories. Otherwise since we check
	 * directories for a depth of 5, its parent would have already been 
	 * detected. */
	parent_depth = brasero_file_node_get_depth (parent);
	if (!isfile) {
		if (parent_depth < 5)
			return TRUE;
	}
	else {
		if (parent_depth < 6)
			return TRUE;
	}

	if (brasero_data_project_file_signal (self, DEEP_DIRECTORY_SIGNAL, name))
		return FALSE;

	return TRUE;
}

static void
brasero_data_project_remove_sibling (BraseroDataProject *self,
				     BraseroFileNode *sibling,
				     BraseroFileNode *replacement)
{
	BraseroDataProjectPrivate *priv;

	if (sibling != replacement)
		return;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	if (BRASERO_FILE_NODE_VIRTUAL (sibling)) {
		BraseroFileTreeStats *stats;
		BraseroFileNode *children;
		BraseroFileNode *iter;

		stats = brasero_file_node_get_tree_stats (priv->root, NULL);
		if (replacement) {
			/* we remove the virtual node, BUT, we keep its
			 * virtual children that will be appended to the
			 * node being moved in replacement. */
			/* NOTE: children MUST all be virtual */
			children = BRASERO_FILE_NODE_CHILDREN (sibling);
			for (iter = children; iter; iter = iter->next)
				brasero_file_node_add (replacement, iter, NULL);

			sibling->union2.children = NULL;
		}
		else {
			/* Remove the virtual node. This should never happens */
			g_warning ("Virtual nodes could not be transfered");
		}

		/* Just destroy the node as it has no other 
		 * existence nor goal in existence but to create
		 * a collision. */
		brasero_file_node_destroy (sibling, stats);
	}
	else {
		/* The node existed and the user wants the existing to 
		 * be replaced, so we delete that node (since the new
		 * one would have the old one's children otherwise). */
		 brasero_data_project_remove_real (self, sibling);
	 }
}

gboolean
brasero_data_project_move_node (BraseroDataProject *self,
				BraseroFileNode *node,
				BraseroFileNode *parent)
{
	BraseroFileNode *imported_sibling;
	BraseroFileNode *target_sibling;
	BraseroDataProjectPrivate *priv;
	BraseroDataProjectClass *klass;
	BraseroFileNode *former_parent;
	BraseroFileTreeStats *stats;
	guint former_position;
	gboolean check_graft;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	if (!parent)
		parent = priv->root;
	else if (parent->is_file || parent->is_loading)
		return FALSE;

	/* can't be moved to the same directory */
	if (node->parent == parent)
		return FALSE;

	/* see if node is not a parent of parent */
	if (brasero_file_node_is_ancestor (node, parent))
		return FALSE;

	/* see if we won't break the max path depth barrier */
	if (!brasero_data_project_is_deep (self, parent, BRASERO_FILE_NODE_NAME (node), node->is_file))
		return FALSE;

	/* One case could make us fail: if there is the same name in
	 * the directory: in that case return FALSE; check now. */
	target_sibling = brasero_file_node_check_name_existence (parent, BRASERO_FILE_NODE_NAME (node));
	if (target_sibling) {
		if (BRASERO_FILE_NODE_VIRTUAL (target_sibling)) {
			brasero_data_project_virtual_sibling (self, node, target_sibling);
			target_sibling = NULL;
		}
		else if (brasero_data_project_node_signal (self, NAME_COLLISION_SIGNAL, target_sibling))
			return FALSE;
	}

	/* If node was in the joliet incompatible table, remove it */
	brasero_data_project_joliet_remove_node (self, node);

	/* check if this file was hiding an imported file. One exception is if
	 * there is a sibling in the target directory which is the parent of our
	 * node. */
	if (!target_sibling || !brasero_file_node_is_ancestor (target_sibling, node))
		imported_sibling = brasero_file_node_check_imported_sibling (node);
	else
		imported_sibling = NULL;

	if (!node->is_grafted) {
		gchar *uri;

		/* Get the URI and all the nodes with the same URI and 
		 * add the list to the hash => add a graft.
		 * See note underneath: if it wasn't grafted before the
		 * move it should probably be a graft now.
		 * NOTE: we need to do it now before it gets unparented. */
		uri = brasero_data_project_node_to_uri (self, node);
		if (!g_hash_table_lookup (priv->grafts, uri))
			brasero_data_project_uri_graft_nodes (self, uri);
		g_free (uri);

		check_graft = FALSE;
	}
	else
		check_graft = TRUE;

	/* really reparent it; signal:
	 * - old location removal
	 * - new location addition */

	/* unparent node now in case its target sibling is a parent */
	former_parent = node->parent;
	former_position = brasero_file_node_get_pos_as_child (node);
	stats = brasero_file_node_get_tree_stats (priv->root, NULL);
	brasero_file_node_move_from (node, stats);

	klass = BRASERO_DATA_PROJECT_GET_CLASS (self);
	if (former_parent && klass->node_removed)
		klass->node_removed (self, former_parent, former_position, node);

	if (target_sibling)
		brasero_data_project_remove_sibling (self,
						     target_sibling,
						     node);

	brasero_file_node_move_to (node, parent, priv->sort_func);

	if (klass->node_added)
		klass->node_added (self, node, NULL);

	if (check_graft) {
		BraseroGraft *graft;
		BraseroURINode *uri_node;

		graft = BRASERO_FILE_NODE_GRAFT (node);
		uri_node = graft->node;

		/* check if still need a graft point after the location change. */
		if (!brasero_data_project_uri_is_graft_needed (self, uri_node->uri))
			brasero_data_project_uri_remove_graft (self, uri_node->uri);
	}

	/* Check joliet name compatibility; this must be done after move as it
	 * depends on the parent. */
	if (strlen (BRASERO_FILE_NODE_NAME (node)) > 64)
		brasero_data_project_joliet_add_node (self, node);

	if (imported_sibling) {
		BraseroDataProjectClass *klass;

		klass = BRASERO_DATA_PROJECT_GET_CLASS (self);
		brasero_file_node_add (imported_sibling->parent, imported_sibling, priv->sort_func);
		if (klass->node_added)
			brasero_data_project_add_node_and_children (self, imported_sibling, klass->node_added);
	}

	/* NOTE: if it has come back to its original location on the 
	 * file system then it has to be grafted; if it was moved back
	 * to origins and wasn't grafted that means it comes from
	 * another graft of the sames parent, so either it has the same
	 * name as its other copy, which is impossible, or the other
	 * copy was moved and then there are graft points.*/

	return TRUE;
}

gboolean
brasero_data_project_rename_node (BraseroDataProject *self,
				  BraseroFileNode *node,
				  const gchar *name)
{
	BraseroFileNode *imported_sibling;
	BraseroDataProjectPrivate *priv;
	BraseroFileNode *sibling;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	/* Don't allow rename to succeed if name is the empty string */
	if (strlen (name) < 1) {
		return FALSE;
	}

	/* make sure there isn't the same name in the directory: if so, that's 
	 * simply not possible to rename. */
	sibling = brasero_file_node_check_name_existence (node->parent, name);
	if (sibling) {
		if (BRASERO_FILE_NODE_VIRTUAL (sibling))
			brasero_data_project_virtual_sibling (self, node, sibling);
		else if (brasero_data_project_node_signal (self, NAME_COLLISION_SIGNAL, sibling))
			return FALSE;
		else if (sibling != node) {
			/* The node existed and the user wants the existing to 
			 * be replaced, so we delete that node (since the new
			 * one would have the old one's children otherwise). */
			brasero_data_project_remove_real (self, sibling);
		}
	}

	/* If node was in the joliet incompatible table, remove it */
	brasero_data_project_joliet_remove_node (self, node);

	/* see if this node didn't replace an imported one. If so the old 
	 * imported node must re-appear in the tree. */
	imported_sibling = brasero_file_node_check_imported_sibling (node);

	if (!node->is_grafted) {
		gchar *uri;

		/* The node URI doesn't exist in URI hash. That's why
		 * we need to add one with all nodes having the same
		 * URI. */
		uri = brasero_data_project_node_to_uri (self, node);
		if (!g_hash_table_lookup (priv->grafts, uri))
			brasero_data_project_uri_graft_nodes (self, uri);
		g_free (uri);

		/* now we can change the name */
		brasero_file_node_rename (node, name);
	}
	else {
		BraseroURINode *uri_node;
		BraseroGraft *graft;

		/* change the name now so we can check afterwards if a
		 * graft is still needed (the name could have been 
		 * changed back to the original one). */
		graft = BRASERO_FILE_NODE_GRAFT (node);
		uri_node = graft->node;

		brasero_file_node_rename (node, name);
		if (!brasero_data_project_uri_is_graft_needed (self, uri_node->uri))
			brasero_data_project_uri_remove_graft (self, uri_node->uri);
	}

	/* Check joliet name compatibility. This must be done after the
	 * node information have been setup. */
	if (strlen (name) > 64)
		brasero_data_project_joliet_add_node (self, node);

	brasero_data_project_node_changed (self, node);

	if (imported_sibling) {
		BraseroDataProjectClass *klass;

		klass = BRASERO_DATA_PROJECT_GET_CLASS (self);

		brasero_file_node_add (sibling->parent, imported_sibling, priv->sort_func);
		if (klass->node_added)
			brasero_data_project_add_node_and_children (self, imported_sibling, klass->node_added);
	}

	return TRUE;
}

static gboolean
brasero_data_project_add_node_real (BraseroDataProject *self,
				    BraseroFileNode *node,
				    BraseroURINode *graft,
				    const gchar *uri)
{
	BraseroDataProjectPrivate *priv;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	/* See if we should create a graft for the node.
	 * NOTE: if we create a graft we create a graft for all nodes
	 * that have the same URI in the tree too. */
	if (graft) {
		/* If there is already a graft for this URI, then add node */
		brasero_file_node_graft (node, graft);
	}
	else if (node->parent == priv->root) {
		/* The node is at the root of the project; graft it as well as
		 * all the nodes already in the tree with the same URI */
		graft = brasero_data_project_uri_graft_nodes (self, uri);
		brasero_file_node_graft (node, graft);
	}
	else if (node->is_fake) {
		/* The node is a fake directory; graft it as well as all the 
		 * nodes already in the tree with the same URI */
		graft = brasero_data_project_uri_graft_nodes (self, uri);
		brasero_file_node_graft (node, graft);
	}
	else {
		gchar *parent_uri;
		gchar *name_uri;

		parent_uri = brasero_data_project_node_to_uri (self, node->parent);
		name_uri = g_path_get_basename (uri);

		/* NOTE: in here use a special function here since that node 
		 * could already be in the tree but under its rightful parent
		 * and then it won't have any graft yet. That's why these nodes
		 * need to be grafted as well. */ 
		if (parent_uri) {
			guint parent_len;

			parent_len = strlen (parent_uri);
			if (strncmp (parent_uri, uri, parent_len)
			||  uri [parent_len] != G_DIR_SEPARATOR
			|| !name_uri
			|| !BRASERO_FILE_NODE_NAME (node)
			||  strcmp (name_uri, BRASERO_FILE_NODE_NAME (node))) {
				/* The node hasn't been put under its rightful
				 * parent from the original file system. That
				 * means we must add a graft */
				graft = brasero_data_project_uri_graft_nodes (self, uri);
				brasero_file_node_graft (node, graft);
			}
			/* NOTE: we don't need to check if the nodes's name
			 * is the same as the one of the URI. This function is
			 * used by two other functions that pass the URI name
			 * as name to the info so that should always be fine. */

			 /* NOTE: for ungrafted nodes the parent graft size is
			 * updated when setting info on node. */
			g_free (parent_uri);
		}
		else {
			/* its father is probably an fake empty directory */
			graft = brasero_data_project_uri_graft_nodes (self, uri);
			brasero_file_node_graft (node, graft);
		}
		g_free (name_uri);
	}

	if (!priv->is_loading_contents) {
		BraseroDataProjectClass *klass;

		/* Signal that something has changed in the tree */
		klass = BRASERO_DATA_PROJECT_GET_CLASS (self);
		if (klass->node_added
		&& !klass->node_added (self, node, uri != NEW_FOLDER? uri:NULL))
			return FALSE;
	}

	/* check joliet compatibility; do it after node was created. */
	if (strlen (BRASERO_FILE_NODE_NAME (node)) > 64)
		brasero_data_project_joliet_add_node (self, node);

	return TRUE;
}

void
brasero_data_project_restore_uri (BraseroDataProject *self,
				  const gchar *uri)
{
	BraseroDataProjectPrivate *priv;
	BraseroURINode *graft;
	gchar *parent_uri;
	GSList *nodes;
	GSList *iter;
	gchar *name;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	name = brasero_utils_get_uri_name (uri);

	parent_uri = g_path_get_dirname (uri);
	nodes = brasero_data_project_uri_to_nodes (self, parent_uri);
	g_free (parent_uri);

	graft = g_hash_table_lookup (priv->grafts, uri);
	for (iter = nodes; iter; iter = iter->next) {
		BraseroFileNode *parent;
		BraseroFileNode *node;

		parent = iter->data;

		/* restore it if it wasn't already and can (no existing node
		 * with the same name must exist). */
		if (brasero_file_node_check_name_existence (parent, name))
			continue;

		node = brasero_file_node_new_loading (name);
		brasero_file_node_add (parent, node, priv->sort_func);
		brasero_data_project_add_node_real (self, node, graft, uri);
	}
	g_slist_free (nodes);
	g_free (name);

	/* see if we still need a graft after all that */
	if (graft && !brasero_data_project_uri_is_graft_needed (self, uri))
		brasero_data_project_uri_remove_graft (self, uri);
}

void
brasero_data_project_exclude_uri (BraseroDataProject *self,
				  const gchar *uri)
{
	BraseroDataProjectPrivate *priv;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	/* NOTE: we don't remove the existing nodes in case one was
	 * previously restored. In any case remove all loading nodes.
	 * There is one exception if the status is unreadable */

	/* make sure a graft exists to signal that it is excluded */
	if (!g_hash_table_lookup (priv->grafts, uri)) {
		/* NOTE: if the graft point exists it should be empty */
		brasero_data_project_uri_add_graft (self, uri);
	}
}

BraseroFileNode *
brasero_data_project_add_imported_session_file (BraseroDataProject *self,
						GFileInfo *info,
						BraseroFileNode *parent)
{
	BraseroFileNode *node;
	BraseroFileNode *sibling;
	BraseroDataProjectClass *klass;
	BraseroDataProjectPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_DATA_PROJECT (self), NULL);
	g_return_val_if_fail (info != NULL, NULL);

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	if (!parent)
		parent = priv->root;

	sibling = brasero_file_node_check_name_existence (parent, g_file_info_get_name (info));
	if (sibling) {
		/* The node exists but it may be that we've loaded the project
		 * before. Then the necessary directories to hold the grafted
		 * files will have been created as fake directories. We need to
		 * replace those whenever we run into one but not lose their 
		 * children. */
		if (BRASERO_FILE_NODE_VIRTUAL (sibling)) {
			node = brasero_file_node_new_imported_session_file (info);
			brasero_data_project_virtual_sibling (self, node, sibling);
		}
		else if (sibling->is_fake && sibling->is_tmp_parent) {
			BraseroGraft *graft;
			BraseroURINode *uri_node;

			graft = BRASERO_FILE_NODE_GRAFT (sibling);
			uri_node = graft->node;

			/* NOTE after this function graft is invalid */
			brasero_file_node_ungraft (sibling);

			/* see if uri_node is still needed */
			if (!uri_node->nodes
			&&  !brasero_data_project_uri_has_parent (self, uri_node->uri))
				brasero_data_project_uri_remove_graft (self, uri_node->uri);

			if (sibling->is_file)
				sibling->is_fake = FALSE;
			else
				sibling->union3.imported_address = g_file_info_get_attribute_int64 (info, BRASERO_IO_DIR_CONTENTS_ADDR);

			sibling->is_imported = TRUE;
			sibling->is_tmp_parent = FALSE;

			/* Something has changed, tell the tree */
			klass = BRASERO_DATA_PROJECT_GET_CLASS (self);
			if (klass->node_changed)
				klass->node_changed (self, sibling);

			return sibling;
		}
		else if (brasero_data_project_node_signal (self, NAME_COLLISION_SIGNAL, sibling))
			return NULL;
		else {
			/* The node existed and the user wants the existing to 
			 * be replaced, so we delete that node (since the new
			 * one would have the old one's children otherwise). */
			brasero_data_project_remove_real (self, sibling);
			node = brasero_file_node_new_imported_session_file (info);
		}
	}
	else
		node = brasero_file_node_new_imported_session_file (info);

	/* Add it (we must add a graft) */
	brasero_file_node_add (parent, node, priv->sort_func);

	/* In this case, there can be no graft, and furthermore the
	 * lengths of the names are not our problem. Just signal that
	 * something has changed in the tree */
	klass = BRASERO_DATA_PROJECT_GET_CLASS (self);
	if (klass->node_added)
		klass->node_added (self, node, NULL);

	return node;
}

BraseroFileNode *
brasero_data_project_add_empty_directory (BraseroDataProject *self,
					  const gchar *name,
					  BraseroFileNode *parent)
{
	BraseroFileNode *node;
	BraseroURINode *graft;
	BraseroFileNode *sibling;
	BraseroDataProjectPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_DATA_PROJECT (self), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	if (!parent)
		parent = priv->root;

	/* check directory_depth */
	if (!brasero_data_project_is_deep (self, parent, name, FALSE))
		return NULL;

	sibling = brasero_file_node_check_name_existence (parent, name);
	if (sibling) {
		if (BRASERO_FILE_NODE_VIRTUAL (sibling)) {
			node = brasero_file_node_new_empty_folder (name);
			brasero_data_project_virtual_sibling (self, node, sibling);
		}
		else if (brasero_data_project_node_signal (self, NAME_COLLISION_SIGNAL, sibling))
			return NULL;
		else {
			/* The node existed and the user wants the existing to 
			 * be replaced, so we delete that node (since the new
			 * one would have the old one's children otherwise). */
			brasero_data_project_remove_real (self, sibling);
			node = brasero_file_node_new_empty_folder (name);
		}
	}
	else
		node = brasero_file_node_new_empty_folder (name);

	brasero_file_node_add (parent, node, priv->sort_func);

	/* Add it (we must add a graft) */
	graft = g_hash_table_lookup (priv->grafts, NEW_FOLDER);
	if (!brasero_data_project_add_node_real (self, node, graft, NEW_FOLDER))
		return NULL;

	return node;
}

static void
brasero_data_project_update_uri (BraseroDataProject *self,
				 BraseroFileNode *node,
				 const gchar *uri)
{
	gchar *parent_uri;
	BraseroGraft *graft;
	BraseroURINode *uri_node;
	BraseroURINode *former_uri_node;

	graft = BRASERO_FILE_NODE_GRAFT (node);
	former_uri_node = graft->node;

	if (!strcmp (former_uri_node->uri, uri)) {
		/* Nothing needs update */
		return;
	}

	/* different URIS; make sure the node still needs a graft:
	 * - if so, update it
	 * - if not, remove it */
	parent_uri = brasero_data_project_node_to_uri (self, node->parent);
	if (parent_uri) {
		guint parent_len;

		parent_len = strlen (parent_uri);

		if (strncmp (parent_uri, uri, parent_len)
		&&  uri [parent_len] != G_DIR_SEPARATOR) {
			/* The node hasn't been put under its rightful parent
			 * from the original file system. That means we must add
			 * a graft or update the current one. */
			uri_node = brasero_data_project_uri_add_graft (self, uri);
			brasero_file_node_graft (node, uri_node);
		}
		else {
			/* rightful parent: ungraft it */
			brasero_file_node_ungraft (node);
		}

		g_free (parent_uri);
	}
	else {
		uri_node = brasero_data_project_uri_add_graft (self, uri);
		brasero_file_node_graft (node, uri_node);
	}

	/* the node was ungrafted, check if the former graft is still needed */
	brasero_data_project_graft_is_needed (self, former_uri_node);
}

gboolean
brasero_data_project_node_loaded (BraseroDataProject *self,
				  BraseroFileNode *node,
				  const gchar *uri,
				  GFileInfo *info)
{
	guint64 size;
	GFileType type;
	gboolean size_changed;
	BraseroFileTreeStats *stats;
	BraseroDataProjectPrivate *priv;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	type = g_file_info_get_file_type (info);
	if (node->is_tmp_parent) {
		/* we must make sure that this is really a directory */
		if (type != G_FILE_TYPE_DIRECTORY) {
			/* exclude the URI we're replacing */
			brasero_data_project_exclude_uri (self, uri);
			brasero_data_project_convert_to_fake (self, node);
			return TRUE;
		}

		priv->loading --;
		g_signal_emit (self,
			       brasero_data_project_signals [PROJECT_LOADED_SIGNAL],
			       0,
			       priv->loading);

		/* That's indeed a directory. It's going to be loaded. */
	}
	else if (priv->loading && node->is_grafted) {
		priv->loading --;
		if (type != G_FILE_TYPE_DIRECTORY) {
			/* no need to load its contents since it's not a folder */
			priv->loading --;
		}

		g_signal_emit (self,
			       brasero_data_project_signals [PROJECT_LOADED_SIGNAL],
			       0,
			       priv->loading);
	}

	/* If the node is not grafted because it was put under its original 
	 * parent on the file system it comes from, then its parent URI can't
	 * have changed (the parent it was put under had already its URI cleaned
	 * of any symlink). Its URI may be different though if it's a symlink
	 * but that case is treated somewhere else. */
	if (node->is_grafted) {
		/* The URI of the node could be different from the one we gave
		 * earlier as brasero-io looks for parent symlinks and replace
		 * them with their target. So since it's a graft, we need to 
		 * update the graft URI just to make sure. */
		brasero_data_project_update_uri (self, node, uri);
	}

	size = g_file_info_get_size (info);
	if (type != G_FILE_TYPE_DIRECTORY) {
		if (BRASERO_BYTES_TO_SECTORS (size, 2048) > BRASERO_FILE_2G_LIMIT
		&&  BRASERO_FILE_NODE_SECTORS (node) < BRASERO_FILE_2G_LIMIT) {
			if (brasero_data_project_file_signal (self, G2_FILE_SIGNAL, g_file_info_get_name (info))) {
				brasero_data_project_remove_node (self, node);
				return FALSE;
			}
		}
	}

	/* avoid signalling twice for the same directory */
	if (!brasero_data_project_is_deep (self, node->parent,  BRASERO_FILE_NODE_NAME (node), node->is_file)) {
		brasero_data_project_remove_node (self, node);
		return FALSE;
	}

	size_changed = (BRASERO_BYTES_TO_SECTORS (size, 2048) != BRASERO_FILE_NODE_SECTORS (node));
	stats = brasero_file_node_get_tree_stats (priv->root, NULL);
	brasero_file_node_set_from_info (node, stats, info);

	/* Check it that needs a graft: this node has not been moved so we don't
	 * need to check these cases yet it could turn out that it was a symlink
	 * then we need a graft. */
	if (g_file_info_get_is_symlink (info) && g_file_info_get_file_type (info) != G_FILE_TYPE_SYMBOLIC_LINK) {
		BraseroURINode *graft;
		gchar *uri;

		/* first we exclude the symlink, then we graft its target. */
		uri = brasero_data_project_node_to_uri (self, node);
		brasero_file_node_ungraft (node);
		brasero_data_project_exclude_uri (self, uri);
		g_free (uri);

		/* NOTE: info has the uri for the target of the symlink.
		 * NOTE 2: all nodes with target URI become grafted. */
		graft = brasero_data_project_uri_graft_nodes (self, g_file_info_get_symlink_target (info));
		brasero_file_node_graft (node, graft);
	}

	/* at this point we know all we need to know about our node and in 
	 * particular if it's a file or a directory, if it's grafted or not
	 * That's why we can start monitoring it. */
	if (!node->is_monitored) {
#ifdef BUILD_INOTIFY
		if (node->is_grafted)
			brasero_file_monitor_single_file (BRASERO_FILE_MONITOR (self),
							  uri,
							  node);

		if (!node->is_file)
			brasero_file_monitor_directory_contents (BRASERO_FILE_MONITOR (self),
								 uri,
								 node);
		node->is_monitored = TRUE;
#endif
	}

	/* signal the changes */
	brasero_data_project_node_changed (self, node);
	if (size_changed)
		g_signal_emit (self,
			       brasero_data_project_signals [SIZE_CHANGED_SIGNAL],
			       0);

	return TRUE;
}

void
brasero_data_project_node_reloaded (BraseroDataProject *self,
				    BraseroFileNode *node,
				    const gchar *uri,
				    GFileInfo *info)
{
	BraseroDataProjectPrivate *priv;
	BraseroFileTreeStats *stats;
	gboolean size_changed;
	const gchar *name;
	guint64 size;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	node->is_reloading = FALSE;

	/* the only thing that can have changed here is size. Readability was 
	 * checked in data-vfs.c. That's why we're only interested in files
	 * since directories don't have size. */ 
	if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
		return;

	size = g_file_info_get_size (info);
	name = g_file_info_get_name (info);
	if (BRASERO_BYTES_TO_SECTORS (size, 2048) > BRASERO_FILE_2G_LIMIT
	&&  BRASERO_FILE_NODE_SECTORS (node) < BRASERO_FILE_2G_LIMIT) {
		if (brasero_data_project_file_signal (self, G2_FILE_SIGNAL, name)) {
			brasero_data_project_remove_node (self, node);
			return;
		}
	}

	size_changed = (BRASERO_BYTES_TO_SECTORS (size, 2048) == BRASERO_FILE_NODE_SECTORS (node));
	if (BRASERO_FILE_NODE_MIME (node) && !size_changed)
		return;

	stats = brasero_file_node_get_tree_stats (priv->root, NULL);
	brasero_file_node_set_from_info (node, stats, info);

	/* no need to check for graft since it wasn't renamed, it wasn't moved
	 * its type hasn't changed (and therefore it can't be a symlink. For 
	 * these reasons it stays as is (whether grafted or not). */

	/* it's probably already watched (through its parent). */

	brasero_data_project_node_changed (self, node);
	if (size_changed)
		g_signal_emit (self,
			       brasero_data_project_signals [SIZE_CHANGED_SIGNAL],
			       0);
}

static BraseroFileNode *
brasero_data_project_add_loading_node_real (BraseroDataProject *self,
					    const gchar *uri,
					    const gchar *name_arg,
					    gboolean is_hidden,
					    BraseroFileNode *parent)
{
	gchar *name;
	BraseroFileNode *node;
	BraseroURINode *graft;
	BraseroFileNode *sibling;
	BraseroDataProjectPrivate *priv;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	graft = g_hash_table_lookup (priv->grafts, uri);
	if (!parent)
		parent = priv->root;

	if (!name_arg) {
		/* NOTE: find the name of the node through the URI */
		name = brasero_utils_get_uri_name (uri);
	}
	else
		name = g_strdup (name_arg);

	/* make sure that name doesn't exist */
	sibling = brasero_file_node_check_name_existence (parent, name);
	if (sibling) {
		if (BRASERO_FILE_NODE_VIRTUAL (sibling)) {
			node = brasero_file_node_new_loading (name);
			brasero_data_project_virtual_sibling (self, node, sibling);
		}
		else if (brasero_data_project_node_signal (self, NAME_COLLISION_SIGNAL, sibling)) {
			g_free (name);
			return NULL;
		}
		else {
			/* The node existed and the user wants the existing to 
			 * be replaced, so we delete that node (since the new
			 * one would have the old one's children otherwise). */
			brasero_data_project_remove_real (self, sibling);
			node = brasero_file_node_new_loading (name);
			graft = g_hash_table_lookup (priv->grafts, uri);
		}
	}
	else
		node = brasero_file_node_new_loading (name);

	g_free (name);

	brasero_file_node_add (parent, node, priv->sort_func);

	node->is_hidden = is_hidden;
	if (!brasero_data_project_add_node_real (self, node, graft, uri))
		return NULL;

	return node;
}

BraseroFileNode *
brasero_data_project_add_loading_node (BraseroDataProject *self,
				       const gchar *uri,
				       BraseroFileNode *parent)
{
	g_return_val_if_fail (BRASERO_IS_DATA_PROJECT (self), NULL);
	g_return_val_if_fail (uri != NULL, NULL);

	return brasero_data_project_add_loading_node_real (self, uri, NULL, FALSE, parent);
}

BraseroFileNode *
brasero_data_project_add_hidden_node (BraseroDataProject *self,
				      const gchar *uri,
				      const gchar *name,
				      BraseroFileNode *parent)
{
	g_return_val_if_fail (BRASERO_IS_DATA_PROJECT (self), NULL);
	g_return_val_if_fail (uri != NULL, NULL);

	return brasero_data_project_add_loading_node_real (self, uri, name, TRUE, parent);
}

void
brasero_data_project_directory_node_loaded (BraseroDataProject *self,
					    BraseroFileNode *parent)
{
	BraseroDataProjectPrivate *priv;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	if (parent->is_exploring) {
		BraseroDataProjectClass *klass;

		klass = BRASERO_DATA_PROJECT_GET_CLASS (self);

		parent->is_exploring = FALSE;
		/* This is to make sure the directory row is
		 * updated in case it is empty. Otherwise, it
		 * would carry on to be displayed as loading
		 * if it were empty.
		 * Don't use brasero_data_project_node_changed
		 * as we don't reorder the rows. */
		if (klass->node_changed)
			klass->node_changed (self, parent);
	}

	/* Mostly useful at project load time. */
	if (priv->loading) {
		if (parent->is_grafted || parent->is_tmp_parent) {
			priv->loading --;
			g_signal_emit (self,
				       brasero_data_project_signals [PROJECT_LOADED_SIGNAL],
				       0,
				       priv->loading);
		}
	}
}

/**
 * This function is only used by brasero-data-vfs.c to add the contents of a 
 * directory. That's why if a node with the same name is already grafted we 
 * can't add it. It means that the node is probable excluded.
 * NOTE: all the files added through this function are not grafted since they
 * are added due to the exploration of their parent. If they collide with
 * anything it can only be with a grafed node.
 */

BraseroFileNode *
brasero_data_project_add_node_from_info (BraseroDataProject *self,
					 const gchar *uri,
					 GFileInfo *info,
					 BraseroFileNode *parent)
{
	GFileType type;
	const gchar *name;
	BraseroFileNode *node;
	BraseroURINode *graft;
	BraseroFileNode *sibling;
	BraseroDataProjectPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_DATA_PROJECT (self), NULL);
	g_return_val_if_fail (info != NULL, NULL);

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	/* Only useful at project load time. In this case if the URI has already
	 * a graft we ignore it since if it existed it has been added through a
	 * graft point and not through directory exploration. */
	graft = g_hash_table_lookup (priv->grafts, uri);
	if (priv->loading && graft) {
		GSList *iter;

		/* This is either :
		 * - a graft node loaded in the beginning (already loaded then)
		 * - an excluded node (and therefore no need to do anything)
		 * - a temporary parent and therefore we just ungraft it (no
		 *   need to set is_tmp_parent to FALSE since it'll be done when
		 *   it is loaded) */
		for (iter = graft->nodes; iter; iter = iter->next) {
			node = iter->data;

			if (parent == node->parent) {
				if (node->is_tmp_parent) {
					if (!brasero_data_project_uri_is_graft_needed (self, graft->uri))
						brasero_data_project_uri_remove_graft (self, graft->uri);
					return node;
				}
			}
		}

		return NULL;
	}

	if (!parent)
		parent = priv->root;

	name = g_file_info_get_name (info);

	/* Run a few checks */
	type = g_file_info_get_file_type (info);
	if (type != G_FILE_TYPE_DIRECTORY) {
		guint64 size;

		size = g_file_info_get_size (info);
		if (BRASERO_BYTES_TO_SECTORS (size, 2048) > BRASERO_FILE_2G_LIMIT)
			if (brasero_data_project_file_signal (self, G2_FILE_SIGNAL, name)) {
				/* we need to exclude this uri */
				brasero_data_project_exclude_uri (self, uri);
				return NULL;
			}
	}
	/* This is a special case where we won't try all checks for deep nested
	 * files. Since this function is only used by brasero-data-vfs.c to 
	 * add the results of its exploration, we only check directories and
	 * just check for a directory to have a depth of 6 (means parent has a
	 * depth of 5. */
	else if (brasero_file_node_get_depth (parent) == 5) {
		if (brasero_data_project_file_signal (self, DEEP_DIRECTORY_SIGNAL, name)) {
			/* we need to exclude this uri */
			brasero_data_project_exclude_uri (self, uri);
			return NULL;
		}
	} 

	/* make sure that name doesn't exist */
	sibling = brasero_file_node_check_name_existence (parent, name);
	if (sibling) {
		BraseroFileTreeStats *stats;

		stats = brasero_file_node_get_tree_stats (priv->root, NULL);

		if (BRASERO_FILE_NODE_VIRTUAL (sibling)) {
			node = brasero_file_node_new (g_file_info_get_name (info));
			brasero_file_node_set_from_info (node, stats, info);
			brasero_data_project_virtual_sibling (self, node, sibling);
		}
		else if (brasero_data_project_node_signal (self, NAME_COLLISION_SIGNAL, sibling)) {
			/* we need to exclude this uri */
			brasero_data_project_exclude_uri (self, uri);
			return NULL;
		}
		else {
			/* The node existed and the user wants the existing to 
			 * be replaced, so we delete that node (since the new
			 * one would have the old one's children otherwise). */
			node = brasero_file_node_new (g_file_info_get_name (info));
			brasero_file_node_set_from_info (node, stats, info);

			brasero_data_project_remove_real (self, sibling);
			graft = g_hash_table_lookup (priv->grafts, uri);
		}
	}
	else {
		BraseroFileTreeStats *stats;

		node = brasero_file_node_new (g_file_info_get_name (info));
		stats = brasero_file_node_get_tree_stats (priv->root, NULL);
		brasero_file_node_set_from_info (node, stats, info);
	}

	brasero_file_node_add (parent, node, priv->sort_func);

	if (g_file_info_get_is_symlink (info)
	&&  g_file_info_get_file_type (info) != G_FILE_TYPE_SYMBOLIC_LINK) {
		/* first we exclude the symlink, then we graft its target */
		brasero_data_project_exclude_uri (self, uri);

		/* then we add the node */
		if (!brasero_data_project_add_node_real (self,
		                                         node,
		                                         graft,
		                                         g_file_info_get_symlink_target (info)))
			return NULL;
	}
	else {
		if (!brasero_data_project_add_node_real (self,
		                                         node,
		                                         graft,
		                                         uri))
			return NULL;
	}

	if (type != G_FILE_TYPE_DIRECTORY)
		g_signal_emit (self,
			       brasero_data_project_signals [SIZE_CHANGED_SIGNAL],
			       0);

	/* at this point we know all we need to know about our node and in 
	 * particular if it's a file or a directory, if it's grafted or not
	 * That's why we can start monitoring it. */
	if (!node->is_monitored) {

#ifdef BUILD_INOTIFY

		if (node->is_grafted)
			brasero_file_monitor_single_file (BRASERO_FILE_MONITOR (self),
							  uri,
							  node);

		if (!node->is_file)
			brasero_file_monitor_directory_contents (BRASERO_FILE_MONITOR (self),
								 uri,
								 node);
		node->is_monitored = TRUE;

#endif

	}

	return node;
}

/**
 * Export tree internals into a track 
 */
struct _MakeTrackData {
	gboolean append_slash;
	gboolean hidden_nodes;

	GSList *grafts;
	GSList *excluded;

	BraseroDataProject *project;
};
typedef struct _MakeTrackData MakeTrackData;

gchar *
brasero_data_project_node_to_path (BraseroDataProject *self,
				   BraseroFileNode *node)
{
	guint len;
	GSList *list;
	GSList *iter;
	gchar path [MAXPATHLEN] = {0, };

	if (!node || G_NODE_IS_ROOT (node))
		return g_strdup (G_DIR_SEPARATOR_S);

	/* walk the nodes up to the parent and add them to a list */
	list = NULL;
	while (node->parent) {
		list = g_slist_prepend (list, node);
		node = node->parent;
	}

	len = 0;
	for (iter = list; iter; iter = iter->next) {
		gchar *name;
		guint name_len;

		node = iter->data;

		*(path + len) = G_DIR_SEPARATOR;
		len ++;

		if (len > MAXPATHLEN)
			return NULL;

		/* Make sure path length didn't go over MAXPATHLEN. */
		name = BRASERO_FILE_NODE_NAME (node);

		name_len = strlen (name);
		if (len + name_len > MAXPATHLEN)
			return NULL;

		memcpy (path + len, name, name_len);
		len += name_len;
	}
	g_slist_free (list);

	return g_strdup (path);
}

static void
_foreach_grafts_make_list_cb (const gchar *uri,
			      BraseroURINode *uri_node,
			      MakeTrackData *data)
{
	GSList *iter;
	gboolean add_to_excluded = (uri_node->nodes == NULL);

	/* add each node */
	for (iter = uri_node->nodes; iter; iter = iter->next) {
		BraseroFileNode *node;
		BraseroGraftPt *graft;

		node = iter->data;
		if (!data->hidden_nodes && node->is_hidden)
			continue;

		add_to_excluded = TRUE;
		graft = g_new0 (BraseroGraftPt, 1);

		/* if URI is a created directory set URI to NULL */
		if (uri && uri != NEW_FOLDER)
			graft->uri = g_strdup (uri);

		graft->path = brasero_data_project_node_to_path (data->project, node);
		if (!node->is_file && data->append_slash) {
			gchar *tmp;

			/* we need to know if that's a directory or not since if
			 * it is then mkisofs (but not genisoimage) requires the
			 * disc path to end with '/'; if there isn't '/' at the 
			 * end then only the directory contents are added. */
			tmp = graft->path;
			graft->path = g_strconcat (graft->path, "/", NULL);
			g_free (tmp);
		}

		data->grafts = g_slist_prepend (data->grafts, graft);
	}

	/* Each URI in this table must be excluded. Then each node in 
	 * this list will be grafted. That way only those that we are
	 * interested in will be in the tree. */

	/* Add to the unreadable. This could be further improved by 
	 * checking if there is a parent in the hash for this URI. If 
	 * not that's no use adding this URI to unreadable. */
	/* NOTE: if that the created directories URI, then there is no 
	 * need to add it to excluded */
	if (uri != NEW_FOLDER && add_to_excluded)
		data->excluded = g_slist_prepend (data->excluded, g_strdup (uri));
}

static void
_foreach_joliet_incompatible_make_list_cb (BraseroJolietKey *key,
					   GSList *nodes,
					   MakeTrackData *data)
{
	GSList *iter;

	/* now exclude all nodes and graft them with a joliet compatible name */
	for (iter = nodes; iter; iter = iter->next) {
		BraseroFileNode *node;
		BraseroGraftPt *graft;

		node = iter->data;

		/* skip grafted nodes (they were already processed). */
		if (node->is_grafted)
			continue;

		graft = g_new0 (BraseroGraftPt, 1);
		graft->path = brasero_data_project_node_to_path (data->project, node);
		if (!node->is_file && data->append_slash) {
			gchar *tmp;

			/* we need to know if that's a directory or not since if
			 * it is then mkisofs (but not genisoimage) requires the
			 * disc path to end with '/'; if there isn't '/' at the 
			 * end then only the directory contents are added. */
			tmp = graft->path;
			graft->path = g_strconcat (graft->path, "/", NULL);
			g_free (tmp);
		}

		/* NOTE: here it's not possible to get a created folder here 
		 * since it would be grafted */
		graft->uri = brasero_data_project_node_to_uri (data->project, node);
		data->grafts = g_slist_prepend (data->grafts, graft);

		data->excluded = g_slist_prepend (data->excluded, g_strdup (graft->uri));
	}
}

gboolean
brasero_data_project_get_contents (BraseroDataProject *self,
				   GSList **grafts,
				   GSList **unreadable,
				   gboolean hidden_nodes,
				   gboolean joliet_compat,
				   gboolean append_slash)
{
	MakeTrackData callback_data;
	BraseroDataProjectPrivate *priv;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	if (!g_hash_table_size (priv->grafts))
		return FALSE;

	callback_data.project = self;
	callback_data.grafts = NULL;
	callback_data.excluded = NULL;
	callback_data.hidden_nodes = hidden_nodes;
	callback_data.append_slash = append_slash;

	g_hash_table_foreach (priv->grafts,
			      (GHFunc) _foreach_grafts_make_list_cb,
			      &callback_data);

	/* This is possible even if the GHashTable is empty since there could be
	 * only excluded URI inside or hidden nodes like autorun.inf. */
	if (!callback_data.grafts) {
		g_slist_foreach (callback_data.excluded, (GFunc) g_free, NULL);
		g_slist_free (callback_data.excluded);
		return FALSE;
	}

	if (joliet_compat) {
		/* Make sure that all nodes with incompatible joliet names are
		 * added as graft points. */
		g_hash_table_foreach (priv->joliet,
				      (GHFunc) _foreach_joliet_incompatible_make_list_cb,
				      &callback_data);
	}

	if (!grafts) {
		g_slist_foreach (callback_data.grafts, (GFunc) brasero_graft_point_free, NULL);
		g_slist_free (callback_data.grafts);
	}
	else
		*grafts = callback_data.grafts;

	if (!unreadable) {
		g_slist_foreach (callback_data.excluded, (GFunc) g_free, NULL);
		g_slist_free (callback_data.excluded);
	}
	else
		*unreadable = callback_data.excluded;

	return TRUE;
}

typedef struct _MakeTrackDataSpan MakeTrackDataSpan;
struct _MakeTrackDataSpan {
	GSList *grafts;
	GSList *joliet_grafts;

	guint64 files_num;
	guint64 dir_num;
	BraseroImageFS fs_type;
};

static void
brasero_data_project_span_set_fs_type (MakeTrackDataSpan *data,
				       BraseroFileNode *node)
{
	if (node->is_symlink) {
		data->fs_type |= BRASERO_IMAGE_FS_SYMLINK;

		/* UDF won't be possible anymore with symlinks */
		if (data->fs_type & BRASERO_IMAGE_ISO_FS_LEVEL_3)
			data->fs_type &= ~(BRASERO_IMAGE_FS_UDF|
					   BRASERO_IMAGE_FS_JOLIET);
	}

	if (node->is_2GiB) {
		data->fs_type |= BRASERO_IMAGE_ISO_FS_LEVEL_3;
		if (!(data->fs_type & BRASERO_IMAGE_FS_SYMLINK))
			data->fs_type |= BRASERO_IMAGE_FS_UDF;
	}

	if (node->is_deep)
		data->fs_type |= BRASERO_IMAGE_ISO_FS_DEEP_DIRECTORY;
}

static void
brasero_data_project_span_explore_folder_children (MakeTrackDataSpan *data,
						   BraseroFileNode *node)
{
	for (node = BRASERO_FILE_NODE_CHILDREN (node); node; node = node->next) {
		if (node->is_grafted)
			data->grafts = g_slist_prepend (data->grafts, node);

		if (node->is_file) {
			brasero_data_project_span_set_fs_type (data, node);
			data->files_num ++;
		}
		else {
			brasero_data_project_span_explore_folder_children (data, node);
			data->dir_num ++;
		}
	}
}

static void
brasero_data_project_span_generate (BraseroDataProject *self,
				    MakeTrackDataSpan *data,
				    gboolean append_slash,
				    BraseroTrackData *track)
{
	GSList *iter;
	gpointer uri_data;
	GHashTableIter hiter;
	GSList *grafts = NULL;
	GSList *excluded = NULL;
	BraseroDataProjectPrivate *priv;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	for (iter = data->grafts; iter; iter = iter->next) {
		BraseroFileNode *node;
		BraseroGraftPt *graft;

		node = iter->data;

		graft = g_new0 (BraseroGraftPt, 1);

		/* REMINDER for the developper who's forgetful:
		 * The real joliet compliant path names will be generated later
		 * either by the backends (like libisofs) or by the library (see
		 * burn-mkisofs-base.c). So no need to care about that. */
		graft->path = brasero_data_project_node_to_path (self, node);
		if (!node->is_file && append_slash) {
			gchar *tmp;

			/* we need to know if that's a directory or not since if
			 * it is then mkisofs (but not genisoimage) requires the
			 * disc path to end with '/'; if there isn't '/' at the 
			 * end then only the directory contents are added. */
			tmp = graft->path;
			graft->path = g_strconcat (graft->path, "/", NULL);
			g_free (tmp);
		}
		graft->uri = brasero_data_project_node_to_uri (self, node);
		grafts = g_slist_prepend (grafts, graft);
	}

	/* NOTE about excluded file list:
	 * don't try to check for every empty BraseroUriNode whether its URI is
	 * a child of one of the grafted node that would take too much time for
	 * almost no win; better add all of them (which includes the above graft
	 * list as well. */
	g_hash_table_iter_init (&hiter, priv->grafts);
	while (g_hash_table_iter_next (&hiter, &uri_data, NULL)) {
		if (uri_data != NEW_FOLDER)
			excluded = g_slist_prepend (excluded, g_strdup (uri_data));
	}

	if (data->fs_type & BRASERO_IMAGE_FS_JOLIET) {
		/* Add the joliet grafts */
		for (iter = data->joliet_grafts; iter; iter = iter->next) {
			BraseroFileNode *node;
			BraseroGraftPt *graft;

			node = iter->data;

			graft = g_new0 (BraseroGraftPt, 1);

			/* REMINDER for the developper who's forgetful:
			 * The real joliet compliant path names will be generated later
			 * either by the backends (like libisofs) or by the library (see
			 * burn-mkisofs-base.c). So no need to care about that. */
			graft->path = brasero_data_project_node_to_path (self, node);
			if (!node->is_file && append_slash) {
				gchar *tmp;

				/* we need to know if that's a directory or not since if
				 * it is then mkisofs (but not genisoimage) requires the
				 * disc path to end with '/'; if there isn't '/' at the 
				 * end then only the directory contents are added. */
				tmp = graft->path;
				graft->path = g_strconcat (graft->path, "/", NULL);
				g_free (tmp);
			}

			grafts = g_slist_prepend (grafts, graft);

			if (graft->uri)
				excluded = g_slist_prepend (excluded, graft->uri);
		}
	}

	brasero_track_data_set_source (track, grafts, excluded);
}

goffset
brasero_data_project_improve_image_size_accuracy (goffset sectors,
						  guint64 dir_num,
						  BraseroImageFS fs_type)
{
	/* sector number should be increased in the following way to get
	 * a more accurate number:
	 * - the first (empty most of the time) 16 sectors
	 * - primary volume descriptor block
	 * - terminator volume descriptor block
	 * - one sector for root (and one more if there is joliet)
	 * - 4 sectors for the path table (at least!!)
	 * - for every directory add a block (for all entry records)
	 *   and another one if there is joliet on
	 */
	sectors += 23;
	sectors += dir_num * 1;
	
	if (fs_type & BRASERO_IMAGE_FS_JOLIET) {
		/* For joliet :
		 * - 1 sector for the volume descriptor
		 * - 1 sector for the root descriptor
		 * - 4 sectors for the path table (at least!!)
		 */
		sectors += 6;

		/* For joliet 2 sectors per directory (at least!!) */
		sectors += dir_num * 2;
	}

	/* Finally there is a 150 pad block at the end (only with mkisofs !!).
	 * That was probably done to avoid getting an image whose size would be
	 * less than 1 sec??? */
	sectors += 150;

	return sectors;
}

goffset
brasero_data_project_get_max_space (BraseroDataProject *self)
{
	BraseroDataProjectPrivate *priv;
	BraseroFileNode *children;
	goffset max_sectors = 0;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	/* When empty this is an error */
	if (!g_hash_table_size (priv->grafts))
		return 0;

	children = BRASERO_FILE_NODE_CHILDREN (priv->root);
	while (children) {
		goffset child_sectors;

		if (g_slist_find (priv->spanned, children)) {
			children = children->next;
			continue;
		}

		if (children->is_file)
			child_sectors = BRASERO_FILE_NODE_SECTORS (children);
		else
			child_sectors = brasero_data_project_get_folder_sectors (self, children);

		max_sectors = MAX (max_sectors, child_sectors);
		children = children->next;
	}

	return max_sectors;
}

BraseroBurnResult
brasero_data_project_span (BraseroDataProject *self,
			   goffset max_sectors,
			   gboolean append_slash,
			   gboolean joliet,
			   BraseroTrackData *track)
{
	MakeTrackDataSpan callback_data;
	BraseroDataProjectPrivate *priv;
	BraseroFileNode *children;
	goffset total_sectors = 0;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	/* When empty this is an error */
	if (!g_hash_table_size (priv->grafts))
		return BRASERO_BURN_ERR;

	callback_data.dir_num = 0;
	callback_data.files_num = 0;
	callback_data.grafts = NULL;
	callback_data.joliet_grafts = NULL;
	callback_data.fs_type = BRASERO_IMAGE_FS_ISO;
	if (joliet)
		callback_data.fs_type |= BRASERO_IMAGE_FS_JOLIET;

	children = BRASERO_FILE_NODE_CHILDREN (priv->root);
	while (children) {
		goffset child_sectors;

		if (g_slist_find (priv->spanned, children)) {
			children = children->next;
			continue;
		}

		if (children->is_file)
			child_sectors = BRASERO_FILE_NODE_SECTORS (children);
		else
			child_sectors = brasero_data_project_get_folder_sectors (self, children);

		/* if the top directory is too large, continue */
		if (child_sectors + total_sectors > max_sectors) {
			children = children->next;
			continue;
		}

		/* FIXME: we need a better algorithm here that would add first
		 * the biggest top folders/files and that would try to fill as
		 * much as possible the disc. */
		total_sectors += child_sectors;

		/* Take care of joliet non compliant nodes */
		if (callback_data.fs_type & BRASERO_IMAGE_FS_JOLIET) {
			GHashTableIter iter;
			gpointer value_data;
			gpointer key_data;

			/* Problem is we don't know whether there are symlinks */
			g_hash_table_iter_init (&iter, priv->joliet);
			while (g_hash_table_iter_next (&iter, &key_data, &value_data)) {
				GSList *nodes;
				BraseroJolietKey *key;

				/* Is the node a graft a child of a graft */
				key = key_data;
				if (key->parent == children || brasero_file_node_is_ancestor (children, key->parent)) {
					/* Add all the children to the list of
					 * grafts provided they are not already
					 * grafted. */
					for (nodes = value_data; nodes; nodes = nodes->next) {
						BraseroFileNode *node;

						/* skip grafted nodes (they are
						 * already or will be processed)
						 */
						node = nodes->data;
						if (node->is_grafted)
							continue;
						
						callback_data.joliet_grafts = g_slist_prepend (callback_data.joliet_grafts, node);
					}

					break;
				}
			}
		}

		callback_data.grafts = g_slist_prepend (callback_data.grafts, children);
		if (children->is_file) {
			brasero_data_project_span_set_fs_type (&callback_data, children);
			callback_data.files_num ++;
		}
		else {
			brasero_data_project_span_explore_folder_children (&callback_data, children);
			callback_data.dir_num ++;
		}

		priv->spanned = g_slist_prepend (priv->spanned, children);
		children = children->next;
	}

	/* This means it's finished */
	if (!callback_data.grafts) {
		BRASERO_BURN_LOG ("No graft found for spanning");
		return BRASERO_BURN_OK;
	}

	brasero_data_project_span_generate (self,
					    &callback_data,
					    append_slash,
					    track);

	total_sectors = brasero_data_project_improve_image_size_accuracy (total_sectors,
									  callback_data.dir_num,
									  callback_data.fs_type);

	brasero_track_data_set_data_blocks (track, total_sectors);
	brasero_track_data_add_fs (track, callback_data.fs_type);
	brasero_track_data_set_file_num (track, callback_data.files_num);

	BRASERO_BURN_LOG ("Set object (size %" G_GOFFSET_FORMAT ")", total_sectors);

	g_slist_free (callback_data.grafts);
	g_slist_free (callback_data.joliet_grafts);

	return BRASERO_BURN_RETRY;
}

BraseroBurnResult
brasero_data_project_span_possible (BraseroDataProject *self,
				    goffset max_sectors)
{
	BraseroDataProjectPrivate *priv;
	gboolean has_data_left = FALSE;
	BraseroFileNode *children;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	/* When empty this is an error */
	if (!g_hash_table_size (priv->grafts))
		return BRASERO_BURN_ERR;

	children = BRASERO_FILE_NODE_CHILDREN (priv->root);
	while (children) {
		goffset child_sectors;

		if (g_slist_find (priv->spanned, children)) {
			children = children->next;
			continue;
		}

		if (children->is_file)
			child_sectors = BRASERO_FILE_NODE_SECTORS (children);
		else
			child_sectors = brasero_data_project_get_folder_sectors (self, children);

		/* Find at least one file or directory that can be spanned */
		if (child_sectors < max_sectors)
			return BRASERO_BURN_RETRY;

		/* if the top directory is too large, continue */
		children = children->next;
		has_data_left = TRUE;
	}

	if (has_data_left)
		return BRASERO_BURN_ERR;

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_data_project_span_again (BraseroDataProject *self)
{
	BraseroDataProjectPrivate *priv;
	BraseroFileNode *children;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	/* When empty this is an error */
	if (!g_hash_table_size (priv->grafts))
		return BRASERO_BURN_ERR;

	children = BRASERO_FILE_NODE_CHILDREN (priv->root);
	while (children) {
		if (!g_slist_find (priv->spanned, children))
			return BRASERO_BURN_RETRY;

		children = children->next;
	}

	return BRASERO_BURN_OK;
}

void
brasero_data_project_span_stop (BraseroDataProject *self)
{
	BraseroDataProjectPrivate *priv;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);
	g_slist_free (priv->spanned);
	priv->spanned = NULL;
}

gboolean
brasero_data_project_has_symlinks (BraseroDataProject *self)
{
	BraseroDataProjectPrivate *priv;
	BraseroFileTreeStats *stats;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	stats = brasero_file_node_get_tree_stats (priv->root, NULL);
	if (stats->num_sym)
		return TRUE;

	return FALSE;
}

gboolean
brasero_data_project_is_joliet_compliant (BraseroDataProject *self)
{
	BraseroDataProjectPrivate *priv;
	BraseroFileTreeStats *stats;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	stats = brasero_file_node_get_tree_stats (priv->root, NULL);
	if (stats->num_sym)
		return FALSE;

	if (!priv->joliet || !g_hash_table_size (priv->joliet))
		return TRUE;

	return FALSE;
}

gboolean
brasero_data_project_is_video_project (BraseroDataProject *self)
{
	BraseroDataProjectPrivate *priv;
	gboolean has_video, has_audio;
	BraseroFileNode *iter;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	/* here we check that the selection can be burnt as a video DVD.
	 * It must have :
	 * - a VIDEO_TS and AUDIO_TS at its root
	 * - the VIDEO_TS directory must have VIDEO_TS.IFO, VIDEO_TS.VOB
	     and VIDEO_TS.BUP inside */

	has_audio = has_video = FALSE;

	iter = BRASERO_FILE_NODE_CHILDREN (priv->root);
	for (; iter; iter = iter->next) {
		gchar *name;

		name = BRASERO_FILE_NODE_NAME (iter);
		if (!name)
			continue;

		if (!strcmp (name, "VIDEO_TS")) {
			BraseroFileNode *child;
			gboolean has_ifo, has_bup;

			has_ifo = has_bup = FALSE;
			child = BRASERO_FILE_NODE_CHILDREN (iter);

			for (; child; child = child->next) {
				name = BRASERO_FILE_NODE_NAME (child);
				if (!name)
					continue;

				if (!strcmp (name, "VIDEO_TS.IFO"))
					has_ifo = TRUE;
				else if (!strcmp (name, "VIDEO_TS.BUP"))
					has_bup = TRUE;
			}

			if (!has_ifo || !has_bup)
				return FALSE;

			has_video = TRUE;
		}
		else if (!strcmp (name, "AUDIO_TS"))
			has_audio = TRUE;
	}

	if (!has_video || !has_audio)
		return FALSE;

	return TRUE;
}

gboolean
brasero_data_project_is_empty (BraseroDataProject *self)
{
	BraseroDataProjectPrivate *priv;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);
	if (g_hash_table_size (priv->grafts))
		return FALSE;

	return TRUE;
}

/**
 * The following functions are mostly useful to load projects
 */

static BraseroFileNode *
brasero_data_project_create_path (BraseroDataProject *self,
				  BraseroFileNode *parent,
				  const gchar **buffer,
				  GSList **folders)
{
	gchar *end;
	const gchar *path;
	BraseroDataProjectPrivate *priv;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	path = *buffer;
	if (path [0] == G_DIR_SEPARATOR)
		path ++;

	end = g_utf8_strchr (path, -1, G_DIR_SEPARATOR);

	while (end && end [1] != '\0') {
		BraseroFileNode *node;
		gchar *name;
		gint len;

		/* create the path */
		len = end - path;
		name = g_strndup (path, len);

		node = brasero_file_node_new_loading (name);
		brasero_file_node_add (parent, node, priv->sort_func);
		parent = node;
		g_free (name);

		/* check joliet compatibility; do it after node was created. */
		if (strlen (BRASERO_FILE_NODE_NAME (parent)) > 64)
			brasero_data_project_joliet_add_node (self, parent);

		/* Set this directory to be set as a folder if it turns out it 
		 * isn't one. */
		parent->is_tmp_parent = TRUE;

		(*folders) = g_slist_prepend ((*folders), parent);

		/* Go on with the next; skip the separator */
		path += len;
		if (path [0] == G_DIR_SEPARATOR)
			path ++;

		end = g_utf8_strchr (path, -1, G_DIR_SEPARATOR);
	}

	*buffer = path;
	return parent;
}

static BraseroFileNode *
brasero_data_project_skip_existing (BraseroDataProject *self,
				    BraseroFileNode *parent,
				    const gchar **buffer)
{
	gchar *end;
	const gchar *path;

	path = *buffer;

	if (path [0] == G_DIR_SEPARATOR)
		path ++;

	end = g_utf8_strchr (path, -1, G_DIR_SEPARATOR);

	/* first look for the existing nodes */
	while (end && end [1] != '\0') {
		BraseroFileNode *node;
		gboolean found;
		guint len;

		len = end - path;

		/* go through the children nodes and find the name */
		found = FALSE;
		for (node = BRASERO_FILE_NODE_CHILDREN (parent); node; node = node->next) {
			if (node
			&& !strncmp (BRASERO_FILE_NODE_NAME (node), path, len)
			&& (BRASERO_FILE_NODE_NAME (node) [len] == G_DIR_SEPARATOR
			||  BRASERO_FILE_NODE_NAME (node) [len] == '\0')) {
				parent = node;
				found = TRUE;
				break;
			}	
		}

		if (!found)
			break;

		/* skip the separator */
		path += len;
		if (path [0] == G_DIR_SEPARATOR)
			path ++;

		end = g_utf8_strchr (path, -1, G_DIR_SEPARATOR);
	}

	*buffer = path;
	return parent;
}

static GSList *
brasero_data_project_add_path (BraseroDataProject *self,
			       const gchar *path,
			       const gchar *uri,
			       GSList *folders)
{
	BraseroDataProjectPrivate *priv;
	BraseroFileNode *parent;
	BraseroFileNode *node;
	BraseroURINode *graft;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	/* we don't create the last part (after the last separator) of
	 * the node since we're only interested in the existence of the
	 * parent. */

	/* find the last existing node in the path */
	parent = brasero_data_project_skip_existing (self, priv->root, &path);

	/* create the missing parents if needed */
	parent = brasero_data_project_create_path (self,
						   parent,
						   &path,
						   &folders);

	/* Now that we ensured that the parent path exists add the final node */

	/* we're sure that this node needs grafting and there is already
	 * probably a graft in the hash (but no node yet). */
	if (!uri)
		uri = NEW_FOLDER;

	graft = brasero_data_project_uri_ensure_graft (self, uri);
	node = brasero_file_node_check_name_existence (parent, path);
	if (node && node->is_tmp_parent) {
		/* This node doesn't need renaming since it was found by name.
		 * No need for joliet check either (done at creation time). 
		 * There is already a graft since it is a fake so remove it from
		 * previous graft and add the new one (it needs to be grafted). */
		node->is_tmp_parent = FALSE;
		brasero_file_node_graft (node, graft);
		folders = g_slist_remove (folders, node);

		/* NOTE: we can use node_added here since no temporary directory
		 * was explicitely added yet. */
		if (uri == NEW_FOLDER) {
			node->is_fake = TRUE;
			node->is_file = FALSE;
			node->is_loading = FALSE;
			node->is_reloading = FALSE;

			/* Don't signal the node addition yet we'll do it later
			 * when all the nodes are created */
		}
		else {
			node->is_file = FALSE;
			node->is_fake = FALSE;
			node->is_loading = TRUE;
			node->is_reloading = FALSE;

			/* Don't signal the node addition yet we'll do it later
			 * when all the nodes are created */
		}
	}
	else if (node) {
		g_warning ("Already existing node");
		/* error: the path exists twice. That shouldn't happen */
		return folders;
	}
	else {
		/* don't use brasero_data_project_add_loading_node since that way:
		 * - we don't check for sibling
		 * - we set right from the start the right name */
		if (uri != NEW_FOLDER)
			node = brasero_file_node_new_loading (path);
		else
			node = brasero_file_node_new_empty_folder (path);

		brasero_file_node_add (parent, node, priv->sort_func);

		/* the following function checks for joliet, graft it */
		brasero_data_project_add_node_real (self,
		                                    node,
		                                    graft,
		                                    uri);
			
	}

	return folders;
}

static GSList *
brasero_data_project_add_excluded_uri (BraseroDataProject *self,
				       const gchar *uri,
				       GSList *folders)
{
	BraseroDataProjectPrivate *priv;
	BraseroURINode *graft = NULL;
	BraseroURINode *uri_graft;
	gchar *unescaped_uri;
	gint parent_uri_len;
	gchar *parent_uri;
	GSList *parents;
	GSList *iter;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	/* First exclude this URI, that is make sure a graft was created if need
	 * be without node.
	 * NOTE: grafted nodes could already have been added for this URI. Get 
	 * them. */
	brasero_data_project_exclude_uri (self, uri);
	uri_graft = g_hash_table_lookup (priv->grafts, uri);

	/* Then get all the nodes from the first grafted parent URI.
	 * NOTE: here we don't check the graft uri itself since there is a graft
	 * but there are probably no node. */
	parent_uri = g_path_get_dirname (uri);
	while (strcmp (parent_uri, G_DIR_SEPARATOR_S) && strchr (parent_uri, G_DIR_SEPARATOR)) {
		/* keep going up until we reach root URI in grafts */
		graft = g_hash_table_lookup (priv->grafts, parent_uri);
		if (graft)
			break;

		graft = NULL;
		parent_uri = dirname (parent_uri);
	}
	g_free (parent_uri);

	if (!graft)
		return folders;

	/* Remove from the list the parent nodes which have a grafted child node
	 * with this URI */
	parents = g_slist_copy (graft->nodes);
	for (iter = uri_graft->nodes; iter; iter = iter->next) {
		GSList *next;
		GSList *nodes;
		BraseroFileNode *node;

		node = iter->data;
		for (nodes = parents; nodes; nodes = next) {
			BraseroFileNode *parent;

			parent = nodes->data;
			next = nodes->next;
			if (brasero_file_node_is_ancestor (parent, node))
				parents = g_slist_remove (parents, parent);
		}
	}

	if (!parents)
		return folders;

	/* Create the paths starting from these nodes */
	unescaped_uri = g_uri_unescape_string (uri, NULL);

	parent_uri = g_uri_unescape_string (graft->uri, NULL);
	parent_uri_len = strlen (parent_uri);
	g_free (parent_uri);

	for (iter = parents; iter; iter = iter->next) {
		BraseroFileNode *parent;
		const gchar *path;

		parent = iter->data;
		path = unescaped_uri + parent_uri_len;

		/* skip the already existing ones */
		parent = brasero_data_project_skip_existing (self, parent, &path);

		/* check parent needs to be created */
		if (path [0] != G_DIR_SEPARATOR)
			continue;

		/* First create the path */
		brasero_data_project_create_path (self,
						  parent,
						  &path,
						  &folders);
	}
	g_slist_free (parents);
	g_free (unescaped_uri);

	return folders;
}

static gint
brasero_data_project_load_contents_notify_directory (BraseroDataProject *self,
						     BraseroFileNode *parent,
						     BraseroDataNodeAddedFunc func)
{
	BraseroFileNode *child;
	gint num = 0;

	child = BRASERO_FILE_NODE_CHILDREN (parent);
	while (child) {
		gchar *uri;
		gboolean res;
		BraseroFileNode *next;

		/* The child could be removed during the process */
		next = child->next;

		/**
		 * This is to get the number of operations remaining before the
		 * whole project is loaded.
		 * +1 for loading information about a file or a directory (that
		 * means they must not be fake).
		 * +1 for loading the directory contents.
		 */
		if (child->is_fake) {
			/* This is a fake directory, there is no operation */
			res = func (self, child, NULL);
		}
		else {
			uri = brasero_data_project_node_to_uri (self, child);
			res = func (self, child, uri);
			g_free (uri);

			if (res)
				num ++;
		}

		/* for whatever reason the node could have been invalidated */
		if (res && !child->is_file) {
			if (!child->is_fake)
				num ++;

			num += brasero_data_project_load_contents_notify_directory (self,
										    child,
										    func);
		}

		child = next;
	}

	return num;
}

static gint
brasero_data_project_load_contents_notify (BraseroDataProject *self)
{
	gint num;
	BraseroDataProjectClass *klass;
	BraseroDataProjectPrivate *priv;

	klass = BRASERO_DATA_PROJECT_GET_CLASS (self);
	if (!klass->node_added)
		return 0;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	/* we'll notify for every single node in the tree starting from the top.
	 * NOTE: at this point there are only grafted nodes (fake or not) in the
	 * tree. */
	num = brasero_data_project_load_contents_notify_directory (self,
								   priv->root,
								   klass->node_added);
	return num;
}

guint
brasero_data_project_load_contents (BraseroDataProject *self,
				    GSList *grafts,
				    GSList *excluded)
{
	GSList *iter;
	GSList *folders = NULL;
	BraseroDataProjectPrivate *priv;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);
	priv->is_loading_contents = 1;

	for (iter = grafts; iter; iter = iter->next) {
		BraseroGraftPt *graft;
		GFile *file;
		gchar *path;
		gchar *uri;

		graft = iter->data;

		if (graft->uri) {
			file = g_file_new_for_uri (graft->uri);
			uri = g_file_get_uri (file);
			g_object_unref (file);
		}
		else
			uri = NULL;

		if (graft->path) {
			/* This might happen if we are loading brasero projects */
			if (g_str_has_suffix (graft->path, G_DIR_SEPARATOR_S)) {
				int len;

				len = strlen (graft->path);
				path = g_strndup (graft->path, len - 1);
			}
			else
				path = g_strdup (graft->path);
		}
		else
			path = NULL;

		folders = brasero_data_project_add_path (self,
							 path,
							 uri,
							 folders);
		g_free (path);
		g_free (uri);
	}

	for (iter = excluded; iter; iter = iter->next) {
		gchar *uri;
		GFile *file;

		file = g_file_new_for_uri (iter->data);
		uri = g_file_get_uri (file);
		g_object_unref (file);

		folders = brasero_data_project_add_excluded_uri (self,
								 uri,
								 folders);
		g_free (uri);
	}

	/* Now load the temporary folders that were created */
	for (iter = folders; iter; iter = iter->next) {
		BraseroURINode *graft;
		BraseroFileNode *tmp;
		gchar *uri;

		tmp = iter->data;

		/* get the URI for this node. There should be one now that all
		 * graft nodes are in the tree. */
		uri = brasero_data_project_node_to_uri (self, tmp);
		if (!uri) {
			/* This node has been grafted under a node that was
			 * imported or was itself an imported node. Since there
			 * is no imported nodes any more, then it has to become
			 * fake.
			 * NOTE: it has to be a directory */
			tmp->is_fake = TRUE;
			tmp->is_loading = FALSE;
			tmp->is_reloading = FALSE;

			graft = brasero_data_project_uri_ensure_graft (self, NEW_FOLDER);
			brasero_file_node_graft (tmp, graft);

			/* Don't signal the node addition yet we'll do it later
			 * when all the nodes are created */

			continue;
		}

		/* graft it ? */
		graft = brasero_data_project_uri_ensure_graft (self, uri);
		brasero_file_node_graft (tmp, graft);
		g_free (uri);

		/* Don't signal the node addition yet we'll do it later when 
		 * all the nodes are created */
	}
	g_slist_free (folders);

	priv->loading = brasero_data_project_load_contents_notify (self);

	priv->is_loading_contents = 0;
	return priv->loading;
}

/**
 * get the size of the whole tree in sectors 
 */
static void
brasero_data_project_sum_graft_size_cb (gpointer key,
					BraseroURINode *graft,
					guint *sum_value)
{
	BraseroFileNode *node;

	if (!graft->nodes)
		return;

	node = graft->nodes->data;
	*sum_value += BRASERO_FILE_NODE_SECTORS (node);
}

goffset
brasero_data_project_get_sectors (BraseroDataProject *self)
{
	BraseroDataProjectPrivate *priv;
	guint retval = 0;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	/* make the sum of all graft sizes provided they have nodes */
	g_hash_table_foreach (priv->grafts,
			      (GHFunc) brasero_data_project_sum_graft_size_cb,
			      &retval);
	return retval;
}

struct _BraseroFileSize {
	goffset sum;
	BraseroFileNode *node;
};
typedef struct _BraseroFileSize BraseroFileSize;

static void
brasero_data_project_folder_size_cb (const gchar *uri,
				     BraseroURINode *graft,
				     BraseroFileSize *size)
{
	GSList *iter;

	for (iter = graft->nodes; iter; iter = iter->next) {
		BraseroFileNode *node;

		node = iter->data;
		if (node == size->node)
			continue;

		if (brasero_file_node_is_ancestor (size->node, node)) {
			size->sum += BRASERO_FILE_NODE_SECTORS (node);
			return;
		}
	}
}

goffset
brasero_data_project_get_folder_sectors (BraseroDataProject *self,
					 BraseroFileNode *node)
{
	BraseroDataProjectPrivate *priv;
	BraseroFileSize size;

	if (node->is_file)
		return 0;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	size.node = node;
	size.sum = BRASERO_FILE_NODE_SECTORS (node);

	g_hash_table_foreach (priv->grafts,
			      (GHFunc) brasero_data_project_folder_size_cb,
			      &size);
	return size.sum;
}

static void
brasero_data_project_init (BraseroDataProject *object)
{
	BraseroDataProjectPrivate *priv;

	priv = BRASERO_DATA_PROJECT_PRIVATE (object);

	/* create the root */
	priv->root = brasero_file_node_root_new ();

	priv->sort_func = brasero_file_node_sort_default_cb;
	priv->ref_count = 1;

	/* create the necessary hash tables */
	priv->grafts = g_hash_table_new (g_str_hash,
					 g_str_equal);
	priv->joliet = g_hash_table_new (brasero_data_project_joliet_hash,
					 brasero_data_project_joliet_equal);
	priv->reference = g_hash_table_new (g_direct_hash,
					    g_direct_equal);
}

BraseroFileNode *
brasero_data_project_get_root (BraseroDataProject *self)
{
	BraseroDataProjectPrivate *priv;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);
	return priv->root;
}

/**
 * This is to watch a still empty path and get a warning through the collision
 * name signal when the node is created. If a node is already created for this
 * path, then returns NULL.
 */

BraseroFileNode *
brasero_data_project_watch_path (BraseroDataProject *project,
				 const gchar *path)
{
	BraseroDataProjectPrivate *priv;
	BraseroFileNode *parent;
	gchar **array;
	gchar **iter;

	priv = BRASERO_DATA_PROJECT_PRIVATE (project);
	parent = brasero_data_project_skip_existing (project, priv->root, &path);

	if (!path || path [0] == '\0')
		return NULL;

	/* Now add the virtual node */
	if (g_str_has_prefix (path, G_DIR_SEPARATOR_S))
		array = g_strsplit (path + 1, G_DIR_SEPARATOR_S, 0);
	else
		array = g_strsplit (path, G_DIR_SEPARATOR_S, 0);

	for (iter = array; iter && *iter && parent; iter ++) {
		BraseroFileNode *node;

		node = brasero_file_node_new_virtual (*iter);
		brasero_file_node_add (parent, node, NULL);
		parent = node;
	}

	g_strfreev (array);

	/* This function shouldn't fail anyway */
	return parent;
}

static gboolean
brasero_data_project_clear_grafts_cb (gchar *key,
				      BraseroURINode *graft,
				      gpointer NULL_data)
{
	GSList *iter;
	GSList *next;

	/* NOTE: no need to clear the key since it's the same as graft->uri. */
	for (iter = graft->nodes; iter; iter = next) {
		BraseroFileNode *node;

		node = iter->data;
		next = iter->next;
		brasero_file_node_ungraft (node);
	}

	if (graft->uri != NEW_FOLDER)
		brasero_utils_unregister_string (graft->uri);

	g_free (graft);
	return TRUE;
}

static gboolean
brasero_data_project_clear_joliet_cb (BraseroJolietKey *key,
				      GSList *nodes,
				      gpointer NULL_data)
{
	g_free (key);
	g_slist_free (nodes);
	return TRUE;
}

static void
brasero_data_project_clear (BraseroDataProject *self)
{
	BraseroDataProjectPrivate *priv;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	if (priv->spanned) {
		g_slist_free (priv->spanned);
		priv->spanned = NULL;
	}

	/* clear the tables.
	 * NOTE: reference hash doesn't need to be cleared. */
	g_hash_table_foreach_remove (priv->grafts,
				     (GHRFunc) brasero_data_project_clear_grafts_cb,
				     NULL);

	g_hash_table_foreach_remove (priv->joliet,
				     (GHRFunc) brasero_data_project_clear_joliet_cb,
				     NULL);

	g_hash_table_destroy (priv->reference);
	priv->reference = g_hash_table_new (g_direct_hash, g_direct_equal);

	/* no need to give a stats since we're destroying it */
	brasero_file_node_destroy (priv->root, NULL);
	priv->root = NULL;

#ifdef BUILD_INOTIFY

	brasero_file_monitor_reset (BRASERO_FILE_MONITOR (self));

#endif
}

void
brasero_data_project_reset (BraseroDataProject *self)
{
	BraseroDataProjectPrivate *priv;
	BraseroDataProjectClass *klass;
	guint num_nodes;

	priv = BRASERO_DATA_PROJECT_PRIVATE (self);

	/* Do it now */
	num_nodes = brasero_file_node_get_n_children (priv->root);
	brasero_data_project_clear (self);

	klass = BRASERO_DATA_PROJECT_GET_CLASS (self);
	if (klass->reset)
		klass->reset (self, num_nodes);

	priv->loading = 0;
	priv->root = brasero_file_node_root_new ();
}

static void
brasero_data_project_finalize (GObject *object)
{
	BraseroDataProjectPrivate *priv;

	priv = BRASERO_DATA_PROJECT_PRIVATE (object);
	brasero_data_project_clear (BRASERO_DATA_PROJECT (object));

	if (priv->grafts) {
		g_hash_table_destroy (priv->grafts);
		priv->grafts = NULL;
	}

	if (priv->joliet) {
		g_hash_table_destroy (priv->joliet);
		priv->joliet = NULL;
	}

	if (priv->reference) {
		g_hash_table_destroy (priv->reference);
		priv->reference = NULL;
	}

	G_OBJECT_CLASS (brasero_data_project_parent_class)->finalize (object);
}

/**
 * Callbacks for inotify backend
 */

#ifdef BUILD_INOTIFY

static void
brasero_data_project_file_added (BraseroFileMonitor *monitor,
				 gpointer callback_data,
				 const gchar *name)
{
	BraseroFileNode *sibling;
	BraseroFileNode *parent;
	gchar *escaped_name;
	gchar *parent_uri;
	gchar *uri;

	/* Here the name can't be NULL since it means that the event
	 * happened on the callback_data */
	if (!name)
		return;

	/* NOTE: don't use reference as data-project is expected to 
	 * stop monitoring those nodes when they are removed. */
	if (!callback_data)
		return;

	parent = callback_data;

	/* check the node doesn't exist already (in this case it was grafted) */
	sibling = brasero_file_node_check_name_existence (parent, name);

	/* get the new URI */
	parent_uri = brasero_data_project_node_to_uri (BRASERO_DATA_PROJECT (monitor), parent);
	escaped_name = g_uri_escape_string (name,
					    G_URI_RESERVED_CHARS_ALLOWED_IN_PATH,
					    FALSE);
	uri = g_strconcat (parent_uri, G_DIR_SEPARATOR_S, escaped_name, NULL);
	g_free (escaped_name);
	g_free (parent_uri);

	if (!sibling || BRASERO_FILE_NODE_VIRTUAL (sibling)) {
		/* If there is a virtual node, get rid of it */
		brasero_data_project_add_loading_node (BRASERO_DATA_PROJECT (monitor),
						       uri,
						       parent);
	}
	else {
		/* There is no way we can add the node to tree; so exclude it */
		brasero_data_project_exclude_uri (BRASERO_DATA_PROJECT (monitor), uri);
	}

	g_free (uri);
}

static void
brasero_data_project_file_update_URI (BraseroDataProject *self,
				      BraseroFileNode *node,
				      const gchar *parent_uri,
				      const gchar *name)
{
	BraseroURINode *uri_node;
	BraseroGraft *graft;
	gchar *escaped_name;
	gchar *uri;

	if (!node->is_grafted)
		return;

	/* If the node was grafted then update its URI in the hash */

	/* change the graft now so we can check afterwards if a graft is still
	 * needed (the name could have been changed back to the original one). */
	graft = BRASERO_FILE_NODE_GRAFT (node);
	uri_node = graft->node;

	/* get the new uri */
	escaped_name = g_uri_escape_string (name,
					    G_URI_RESERVED_CHARS_ALLOWED_IN_PATH,
					    FALSE);
	uri = g_build_path (G_DIR_SEPARATOR_S, parent_uri, escaped_name, NULL);
	g_free (escaped_name);

	/* ungraft it */
	brasero_file_node_ungraft (node);

	/* regraft it with the new correct URI */
	uri_node = brasero_data_project_uri_ensure_graft (self, uri);
	brasero_file_node_graft (node, uri_node);
	g_free (uri);

	/* make sure we still need it in case it was moved to the right place */
	if (!brasero_data_project_uri_is_graft_needed (self, uri_node->uri))
		brasero_data_project_uri_remove_graft (self, uri_node->uri);
}

static void
brasero_data_project_file_update_name (BraseroDataProject *self,
				       BraseroFileNode *node,
				       const gchar *new_name)
{
	BraseroFileNode *sibling;

	/* see if the old name was correct or if it had been changed.
	 * If it has been changed it'll be at least grafted but grafted
	 * doesn't mean that the name is not the original one since it
	 * could have been moved. 
	 * We don't want to rename files that were renamed. */

	/* If node was in the joliet incompatible table, remove it */
	brasero_data_project_joliet_remove_node (self, node);

	/* see if this node didn't replace an imported one. If so the old 
	 * imported node must re-appear in the tree. */
	sibling = brasero_file_node_check_imported_sibling (node);

	/* the name had not been changed so update it */
	brasero_file_node_rename (node, new_name);

	/* Check joliet name compatibility. This must be done after the
	 * node information have been setup. */
	if (strlen (new_name) > 64)
		brasero_data_project_joliet_add_node (self, node);

	brasero_data_project_node_changed (self, node);

	if (sibling) {
		BraseroDataProjectClass *klass;
		BraseroDataProjectPrivate *priv;

		klass = BRASERO_DATA_PROJECT_GET_CLASS (self);
		priv = BRASERO_DATA_PROJECT_PRIVATE (self);

		/* restore the imported node that has the same name as old_name */
		brasero_file_node_add (sibling->parent, sibling, priv->sort_func);
		if (klass->node_added)
			brasero_data_project_add_node_and_children (self, sibling, klass->node_added);
	}
}

static void
brasero_data_project_file_graft (BraseroDataProject *self,
				 BraseroFileNode *node,
				 const gchar *real_name)
{
	BraseroURINode *uri_node;
	gchar *escaped_name;
	gchar *parent;
	gchar *uri;

	/* get the (new) URI */
	uri = brasero_data_project_node_to_uri (self, node);
	parent = g_path_get_dirname (uri);
	g_free (uri);

	escaped_name = g_uri_escape_string (real_name,
					    G_URI_RESERVED_CHARS_ALLOWED_IN_PATH,
					    FALSE);
	uri = g_strconcat (parent, G_DIR_SEPARATOR_S, escaped_name, NULL);
	g_free (escaped_name);
	g_free (parent);

	/* create new node and graft */
	uri_node = brasero_data_project_uri_ensure_graft (self, uri);
	brasero_file_node_graft (node, uri_node);
	g_free (uri);
}

static void
brasero_data_project_file_renamed (BraseroFileMonitor *monitor,
				   BraseroFileMonitorType type,
				   gpointer callback_data,
				   const gchar *old_name,
				   const gchar *new_name)
{
	BraseroFileNode *sibling;
	BraseroFileNode *node;

	/* If old_name is NULL then it means the event is against callback.
	 * Otherwise that's against one of the children of callback. */
	if (type == BRASERO_FILE_MONITOR_FOLDER)
		node = brasero_file_node_check_name_existence (callback_data, old_name);
	else
		node = callback_data;

	if (!node)
		return;

	/* make sure there isn't the same name in the directory: if so, that's 
	 * simply not possible to rename. So if node is grafted it keeps its
	 * name if not, it's grafted with the old name. */
	sibling = brasero_file_node_check_name_existence (node->parent, new_name);
	if (sibling && !BRASERO_FILE_NODE_VIRTUAL (sibling)) {
		if (!node->is_grafted) {
			brasero_data_project_file_graft (BRASERO_DATA_PROJECT (monitor), node, new_name);
			return;
		}

		/* if that's a grafted, just keep its name (but update URI). */
	}
	else if (!node->is_grafted || !strcmp (old_name, BRASERO_FILE_NODE_NAME (node))) {
		/* see if the old name was correct or if it had been changed.
		 * If it has been changed it'll be at least grafted but grafted
		 * doesn't mean that the name is not the original one since it
		 * could have been moved. 
		 * We don't want to rename files that were renamed. */
		brasero_data_project_file_update_name (BRASERO_DATA_PROJECT (monitor), node, new_name);
	}

	if (sibling && BRASERO_FILE_NODE_VIRTUAL (sibling)) {
		/* Signal collision and remove virtual node but ignore result */
		brasero_data_project_virtual_sibling (BRASERO_DATA_PROJECT (monitor), node, sibling);
	}

	if (node->is_grafted) {
		BraseroURINode *uri_node;
		BraseroGraft *graft;
		gchar *parent;

		graft = BRASERO_FILE_NODE_GRAFT (node);
		uri_node = graft->node;

		/* If the node was grafted then update its URI in the hash */
		parent = g_path_get_dirname (uri_node->uri);
		brasero_data_project_file_update_URI (BRASERO_DATA_PROJECT (monitor),
						      node,
						      parent,
						      new_name);
		g_free (parent);
	}
}

static void
brasero_data_project_file_moved (BraseroFileMonitor *monitor,
				 BraseroFileMonitorType type,
				 gpointer callback_src,
				 const gchar *name_src,
				 gpointer callback_dest,
				 const gchar *name_dest)
{
	BraseroFileNode *node, *parent;

	if (type == BRASERO_FILE_MONITOR_FOLDER)
		node = brasero_file_node_check_name_existence (callback_src, name_src);
	else
		node = callback_src;

	if (!node)
		return;

	/* callback_dest has to be the new parent from the tree. If 
	 * that node has been moved to a fake node directory then it
	 * won't be returned; besides we wouldn't know where to put it
	 * in the tree even if it were returned. */
	parent = callback_dest;

	if (node->is_grafted) {
		gchar *parent_uri;

		/* simply update its URI in the hash. It was moved here on 
		 * purpose by the user so we don't move it. Remove graft is it
		 * isn't needed.
		 * Also, update the node name if it wasn't changed. */
		if (!strcmp (name_src, BRASERO_FILE_NODE_NAME (node))) {
			/* we also need to rename it since the user didn't
			 * change its name. Make sure though that in the new
			 * parent contents a file doesn't exist witht the same
			 * name. If so, then keep the old name. */
			if (!brasero_file_node_check_name_existence (parent, name_dest))
				brasero_data_project_file_update_name (BRASERO_DATA_PROJECT (monitor), node, name_dest);
		}

		/* update graft URI */
		parent_uri = brasero_data_project_node_to_uri (BRASERO_DATA_PROJECT (monitor), parent);
		brasero_data_project_file_update_URI (BRASERO_DATA_PROJECT (monitor),
						      node,
						      parent_uri,
						      name_dest);
		g_free (parent_uri);
	}
	else {
		guint former_position;
		BraseroFileNode *sibling;
		BraseroFileTreeStats *stats;
		BraseroFileNode *former_parent;
		BraseroDataProjectClass *klass;
		BraseroDataProjectPrivate *priv;

		klass = BRASERO_DATA_PROJECT_GET_CLASS (monitor);
		priv = BRASERO_DATA_PROJECT_PRIVATE (monitor);

		/* make sure there isn't the same name in the directory: if so,
		 * that's simply not possible to rename. So if node is grafted
		 * it keeps its name; if not, it's grafted with the old name. */
		sibling = brasero_file_node_check_name_existence (parent, name_dest);
		if (sibling && !BRASERO_FILE_NODE_VIRTUAL (sibling)) {
			brasero_data_project_file_graft (BRASERO_DATA_PROJECT (monitor), node, name_dest);
			return;
		}

		if (sibling && BRASERO_FILE_NODE_VIRTUAL (sibling)) {
			/* Signal collision and remove virtual node but ignore result */
			brasero_data_project_virtual_sibling (BRASERO_DATA_PROJECT (monitor), node, sibling);
		}

		/* If node was in the joliet incompatible table, remove it */
		brasero_data_project_joliet_remove_node (BRASERO_DATA_PROJECT (monitor), node);

		/* see if we won't break the max path depth barrier */
		if (!brasero_data_project_is_deep (BRASERO_DATA_PROJECT (monitor), parent,  BRASERO_FILE_NODE_NAME (node), node->is_file)) {
			brasero_data_project_remove_node (BRASERO_DATA_PROJECT (monitor), node);
			return;
		}

		/* see if this node didn't replace an imported one. If so the old 
		 * imported node must re-appear in the tree after the move. */
		sibling = brasero_file_node_check_imported_sibling (node);

		/* move it */
		former_parent = node->parent;
		former_position = brasero_file_node_get_pos_as_child (node);

		stats = brasero_file_node_get_tree_stats (priv->root, NULL);
		brasero_file_node_move_from (node, stats);
		if (klass->node_removed)
			klass->node_removed (BRASERO_DATA_PROJECT (monitor),
					     former_parent,
					     former_position,
					     node);

		if (name_dest && strcmp (name_dest, name_src)) {
			/* the name has been changed so update it */
			brasero_file_node_rename (node, name_dest);
		}

		/* Check joliet name compatibility. This must be done after the
		 * node information have been setup. */
		if (strlen (name_dest) > 64)
			brasero_data_project_joliet_add_node (BRASERO_DATA_PROJECT (monitor), node);

		brasero_file_node_move_to (node, parent, priv->sort_func);

		if (klass->node_added)
			klass->node_added (BRASERO_DATA_PROJECT (monitor), node, NULL);

		if (sibling) {
			BraseroDataProjectClass *klass;

			klass = BRASERO_DATA_PROJECT_GET_CLASS (monitor);

			/* restore the imported node that has the same name as old_name */
			brasero_file_node_add (sibling->parent, sibling, priv->sort_func);
			if (klass->node_added)
				brasero_data_project_add_node_and_children (BRASERO_DATA_PROJECT (monitor), sibling, klass->node_added);
		}
	}
}

static void
brasero_data_project_file_removed (BraseroFileMonitor *monitor,
				   BraseroFileMonitorType type,
				   gpointer callback_data,
				   const gchar *name)
{
	BraseroDataProjectPrivate *priv;
	BraseroURINode *uri_node;
	BraseroFileNode *node;
	gchar *uri;

	priv = BRASERO_DATA_PROJECT_PRIVATE (monitor);

	/* If name is NULL then it means the event is against callback.
	 * Otherwise that's against one of the children of callback. */
	if (type == BRASERO_FILE_MONITOR_FOLDER)
		node = brasero_file_node_check_name_existence (callback_data, name);
	else
		node = callback_data;

	if (!node)
		return;

	uri = brasero_data_project_node_to_uri (BRASERO_DATA_PROJECT (monitor), node);
	brasero_data_project_remove_node (BRASERO_DATA_PROJECT (monitor), node);

	/* a graft must have been created or already existed. */
	uri_node = g_hash_table_lookup (priv->grafts, uri);
	g_free (uri);

	/* check if we can remove it (no more nodes) */
	if (!uri_node || uri_node->nodes)
		return;

	g_hash_table_remove (priv->grafts, uri_node->uri);
	brasero_utils_unregister_string (uri_node->uri);
	g_free (uri_node);
}

static void
brasero_data_project_file_modified (BraseroFileMonitor *monitor,
				    gpointer callback_data,
				    const gchar *name)
{
	BraseroFileNode *node = callback_data;
	BraseroDataProjectClass *klass;
	gchar *uri;

	if (node->is_loading)
		return;

	/* If that's a directory we don't need to reload it since we're watching
	 * it. That event is sent because the inode was written (a new file has
	 * been added to it). Now since we monitor the directory we'd rather
	 * process the added event (or the removed one). */
	if (!node->is_file)
		return;

	/* This is a call for a rescan of the node; flag it as loading */
	node->is_reloading = TRUE;

	/* Signal that something has changed in the tree */
	klass = BRASERO_DATA_PROJECT_GET_CLASS (BRASERO_DATA_PROJECT (monitor));
	uri = brasero_data_project_node_to_uri (BRASERO_DATA_PROJECT (monitor), node);
	if (klass->node_added)
		klass->node_added (BRASERO_DATA_PROJECT (monitor), node, uri);
	g_free (uri);
}

#endif

static void
brasero_data_project_class_init (BraseroDataProjectClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroDataProjectPrivate));

	object_class->finalize = brasero_data_project_finalize;

	brasero_data_project_signals [JOLIET_RENAME_SIGNAL] = 
	    g_signal_new ("joliet-rename",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__VOID,
			  G_TYPE_NONE,
			  0,
			  G_TYPE_NONE);
	brasero_data_project_signals [NAME_COLLISION_SIGNAL] = 
	    g_signal_new ("name_collision",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL, NULL,
			  brasero_marshal_BOOLEAN__POINTER,
			  G_TYPE_BOOLEAN,
			  1,
			  G_TYPE_POINTER);
	brasero_data_project_signals [SIZE_CHANGED_SIGNAL] = 
	    g_signal_new ("size_changed",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__VOID,
			  G_TYPE_NONE,
			  0,
			  G_TYPE_NONE);
	brasero_data_project_signals [DEEP_DIRECTORY_SIGNAL] = 
	    g_signal_new ("deep_directory",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL, NULL,
			  brasero_marshal_BOOLEAN__STRING,
			  G_TYPE_BOOLEAN,
			  1,
			  G_TYPE_STRING);
	brasero_data_project_signals [G2_FILE_SIGNAL] = 
	    g_signal_new ("G2_file",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL, NULL,
			  brasero_marshal_BOOLEAN__STRING,
			  G_TYPE_BOOLEAN,
			  1,
			  G_TYPE_STRING);
	brasero_data_project_signals [PROJECT_LOADED_SIGNAL] = 
	    g_signal_new ("project-loaded",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__INT,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_INT);

	brasero_data_project_signals [VIRTUAL_SIBLING_SIGNAL] = 
	    g_signal_new ("virtual-sibling",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL, NULL,
			  brasero_marshal_VOID__POINTER_POINTER,
			  G_TYPE_NONE,
			  2,
			  G_TYPE_POINTER,
			  G_TYPE_POINTER);

#ifdef BUILD_INOTIFY

	BraseroFileMonitorClass *monitor_class = BRASERO_FILE_MONITOR_CLASS (klass);

	monitor_class->file_added = brasero_data_project_file_added;
	monitor_class->file_moved = brasero_data_project_file_moved;
	monitor_class->file_removed = brasero_data_project_file_removed;
	monitor_class->file_renamed = brasero_data_project_file_renamed;
	monitor_class->file_modified = brasero_data_project_file_modified;

#endif
}
