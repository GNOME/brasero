/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2007 <bonfire-app@wanadoo.fr>
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

#include <string.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <gconf/gconf-client.h>

#include "brasero-data-vfs.h"
#include "brasero-data-project.h"
#include "brasero-file-node.h"
#include "brasero-io.h"
#include "brasero-utils.h"
#include "brasero-marshal.h"
#include "brasero-utils.h"

#include "burn-debug.h"

typedef struct _BraseroDataVFSPrivate BraseroDataVFSPrivate;
struct _BraseroDataVFSPrivate
{
	/* In this hash there are all URIs currently loading. Every
	 * time we want to load a new URI, we should ask this table if
	 * that URI is currently loading; if so, add the associated
	 * node to the list in the hash table. */
	GHashTable *loading;
	GHashTable *directories;

	/* This keeps a record of all URIs that have been restored by
	 * the user despite the filtering rules. */
	GHashTable *filtered;

	BraseroIO *io;
	BraseroIOJobBase *load_uri;
	BraseroIOJobBase *load_contents;

	guint filter_hidden:1;
	guint filter_broken_sym:1;
};

#define BRASERO_DATA_VFS_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_DATA_VFS, BraseroDataVFSPrivate))

enum {
	UNREADABLE_SIGNAL,
	RECURSIVE_SIGNAL,
	FILTERED_SIGNAL,
	ACTIVITY_SIGNAL,
	UNKNOWN_SIGNAL,
	LAST_SIGNAL
};

static gulong brasero_data_vfs_signals [LAST_SIGNAL] = { 0 };

typedef enum {
	BRASERO_DATA_VFS_NONE		= 0,
	BRASERO_DATA_VFS_RESTORED,
	BRASERO_DATA_VFS_FILTERED
} BraseroDataVFSFilterStatus;

G_DEFINE_TYPE (BraseroDataVFS, brasero_data_vfs, BRASERO_TYPE_DATA_SESSION);

static void
brasero_data_vfs_restored_list_cb (gpointer key,
				   gpointer data,
				   gpointer callback_data)
{
	GSList **list = callback_data;

	if (GPOINTER_TO_INT (data) == BRASERO_DATA_VFS_RESTORED)
		*list = g_slist_prepend (*list, g_strdup (key));
}

gboolean
brasero_data_vfs_get_restored (BraseroDataVFS *self,
			       GSList **restored)
{
	BraseroDataVFSPrivate *priv;

	priv = BRASERO_DATA_VFS_PRIVATE (self);

	*restored = NULL;
	g_hash_table_foreach (priv->filtered,
			      brasero_data_vfs_restored_list_cb,
			      restored);
	return TRUE;
}

void
brasero_data_vfs_add_restored (BraseroDataVFS *self,
			       const gchar *restored)
{
	BraseroDataVFSPrivate *priv;
	guint value;

	priv = BRASERO_DATA_VFS_PRIVATE (self);

	value = GPOINTER_TO_INT (g_hash_table_lookup (priv->filtered, restored));
	if (value) {
		if (GPOINTER_TO_INT (value) != BRASERO_DATA_VFS_RESTORED)
			g_hash_table_insert (priv->filtered,
					     (gchar *) restored,
					     GINT_TO_POINTER (BRASERO_DATA_VFS_RESTORED));
	}
	else
		g_hash_table_insert (priv->filtered,
				     (gchar *) brasero_utils_register_string (restored),
				     GINT_TO_POINTER (BRASERO_DATA_VFS_RESTORED));
}

void
brasero_data_vfs_remove_restored (BraseroDataVFS *self,
				  const gchar *restored)
{
	BraseroDataVFSPrivate *priv;
	guint value;

	priv = BRASERO_DATA_VFS_PRIVATE (self);

	value = GPOINTER_TO_INT (g_hash_table_lookup (priv->filtered, restored));
	if (value) {
		if (GPOINTER_TO_INT (value) != BRASERO_DATA_VFS_FILTERED)
			g_hash_table_insert (priv->filtered,
					     (gchar *) restored,
					     GINT_TO_POINTER (BRASERO_DATA_VFS_FILTERED));
	}
	else
		g_hash_table_insert (priv->filtered,
				     (gchar *) brasero_utils_register_string (restored),
				     GINT_TO_POINTER (BRASERO_DATA_VFS_FILTERED));
}

gboolean
brasero_data_vfs_is_active (BraseroDataVFS *self)
{
	BraseroDataVFSPrivate *priv;

	priv = BRASERO_DATA_VFS_PRIVATE (self);
	return (g_hash_table_size (priv->loading) != 0 || g_hash_table_size (priv->directories) != 0);
}

gboolean
brasero_data_vfs_is_loading_uri (BraseroDataVFS *self)
{
	BraseroDataVFSPrivate *priv;

	priv = BRASERO_DATA_VFS_PRIVATE (self);
	return (g_hash_table_size (priv->loading) != 0);
}

static gboolean
brasero_data_vfs_check_uri_result (BraseroDataVFS *self,
				   const gchar *uri,
				   GError *error,
				   GFileInfo *info)
{
	BraseroDataVFSPrivate *priv;

	priv = BRASERO_DATA_VFS_PRIVATE (self);

	/* Only signal errors if the node was specifically added by the user
	 * that is if it is loading. So check the loading GHashTable to know 
	 * that. Otherwise this URI comes from directory exploration. */

	if (error) {
		if (error->domain == G_IO_ERROR && error->code == G_IO_ERROR_NOT_FOUND) {
			if (g_hash_table_lookup (priv->loading, uri))
				g_signal_emit (self,
					       brasero_data_vfs_signals [UNKNOWN_SIGNAL],
					       0,
					       uri);
		}
		else if (error->domain == BRASERO_ERROR && error->code == BRASERO_ERROR_SYMLINK_LOOP) {
			brasero_data_project_exclude_uri (BRASERO_DATA_PROJECT (self),
							  uri);

			if (g_hash_table_lookup (priv->loading, uri))
				g_signal_emit (self,
					       brasero_data_vfs_signals [RECURSIVE_SIGNAL],
					       0,
					       uri);
		}
		else {
			brasero_data_project_exclude_uri (BRASERO_DATA_PROJECT (self),
							  uri);

			if (g_hash_table_lookup (priv->loading, uri))
				g_signal_emit (self,
					       brasero_data_vfs_signals [UNREADABLE_SIGNAL],
					       0,
					       error,
					       uri);
		}

		BRASERO_BURN_LOG ("VFS information retrieval error %s : %s\n",
				  uri,
				  error->message);

		return FALSE;
	}

	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_READ)
	&& !g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_READ)) {
		brasero_data_project_exclude_uri (BRASERO_DATA_PROJECT (self), uri);

		if (g_hash_table_lookup (priv->loading, uri)) {
			GError *error;

			error = g_error_new (BRASERO_ERROR,
					     BRASERO_ERROR_GENERAL,
					     _("\"%s\" cannot be read"),
					     g_file_info_get_name (info));
			g_signal_emit (self,
				       brasero_data_vfs_signals [UNREADABLE_SIGNAL],
				       0,
				       error,
				       uri);
		}
		return FALSE;
	}

	return TRUE;
}

static void
brasero_data_vfs_remove_from_hash (BraseroDataVFS *self,
				   GHashTable *h_table,
				   const gchar *uri)
{
	GSList *nodes;
	GSList *iter;

	/* data is the reference to the node explored */
	nodes = g_hash_table_lookup (h_table, uri);
	for (iter = nodes; iter; iter = iter->next) {
		guint reference;

		reference = GPOINTER_TO_INT (iter->data);
		brasero_data_project_reference_free (BRASERO_DATA_PROJECT (self), reference);
	}
	g_slist_free (nodes);
	g_hash_table_remove (h_table, uri);
}

/**
 * Explore and add the contents of a directory already loaded
 */
static void
brasero_data_vfs_directory_load_end (GObject *object,
				     gboolean cancelled,
				     gpointer data)
{
	BraseroDataVFSPrivate *priv = BRASERO_DATA_VFS_PRIVATE (object);
	BraseroDataVFS *self = BRASERO_DATA_VFS (object);
	gchar *uri = data;
	GSList *nodes;

	priv = BRASERO_DATA_VFS_PRIVATE (self);

	nodes = g_hash_table_lookup (priv->directories, uri);
	for (; nodes; nodes = nodes->next) {
		BraseroFileNode *parent;
		guint reference;

		reference = GPOINTER_TO_INT (nodes->data);
		parent = brasero_data_project_reference_get (BRASERO_DATA_PROJECT (self), reference);
		if (!parent)
			continue;

		parent->is_exploring = FALSE;
		brasero_data_project_directory_node_loaded (BRASERO_DATA_PROJECT (self), parent);
	}

	brasero_data_vfs_remove_from_hash (self, priv->directories, uri);
	brasero_utils_unregister_string (uri);

	if (cancelled)
		return;

	/* Only emit a signal if state changed. Some widgets need to know if 
	 * either directories loading or uri loading state has changed to signal
	 * it even if there were some uri loading. */
	if (!g_hash_table_size (priv->directories))
		g_signal_emit (self,
			       brasero_data_vfs_signals [ACTIVITY_SIGNAL],
			       0,
			       g_hash_table_size (priv->loading));
}

static gboolean
brasero_data_vfs_directory_check_symlink_loop (BraseroDataVFS *self,
					       BraseroFileNode *parent,
					       const gchar *uri,
					       GFileInfo *info)
{
	BraseroDataVFSPrivate *priv;
	const gchar *target_uri;
	guint target_len;
	guint uri_len;

	priv = BRASERO_DATA_VFS_PRIVATE (self);

	target_uri = g_file_info_get_symlink_target (info);
	if (!target_uri)
		return FALSE;

	/* if target points to a child that's OK */
	uri_len = strlen (uri);
	if (!strncmp (target_uri, uri, uri_len)
	&&   target_uri [uri_len] == G_DIR_SEPARATOR)
		return FALSE;

	target_len = strlen (target_uri);
	while (parent && !parent->is_root) {
		BraseroFileNode *next;
		gchar *parent_uri;
		guint parent_len;
		gchar *next_uri;
		guint next_len;

		/* if the file is not grafted carry on */
		if (!parent->is_grafted) {
			parent = parent->parent;
			continue;
		}

		/* if the file is a symlink, carry on since that's why it was
		 * grafted. It can't have been added by the user since in this
		 * case we replace the symlink by the target. */
		if (parent->is_symlink) {
			parent = parent->parent;
			continue;
		}

		parent_uri = brasero_data_project_node_to_uri (BRASERO_DATA_PROJECT (self), parent);
		parent_len = strlen (parent_uri);

		/* see if target is a parent of that file */
		if (!strncmp (target_uri, parent_uri, target_len)
		&&   parent_uri [target_len] == G_DIR_SEPARATOR) {
			g_free (parent_uri);
			return TRUE;
		}

		/* see if the graft is also the parent of the target */
		if (!strncmp (parent_uri, target_uri, parent_len)
		&&   target_uri [parent_len] == G_DIR_SEPARATOR) {
			g_free (parent_uri);
			return TRUE;
		}

		/* The next graft point must be the natural parent of this one */
		next = parent->parent;
		if (!next || next->is_root) {
			g_free (parent_uri);
			break;
		}

		next_uri = brasero_data_project_node_to_uri (BRASERO_DATA_PROJECT (self), next);
		next_len = strlen (next_uri);

		if (!strncmp (next_uri, parent_uri, next_len)
		&&   parent_uri [next_len] == G_DIR_SEPARATOR) {
			/* It's not the natural parent. We're done */
			g_free (parent_uri);
			break;
		}

		/* retry with the next parent graft point */
		g_free (parent_uri);
		parent = next;
	}

	return FALSE;
}

static void
brasero_data_vfs_directory_load_result (GObject *owner,
					GError *error,
					const gchar *uri,
					GFileInfo *info,
					gpointer data)
{
	BraseroDataVFS *self = BRASERO_DATA_VFS (owner);
	BraseroDataVFSPrivate *priv;
	gchar *parent_uri = data;
	const gchar *name;
	GSList *nodes;
	GSList *iter;

	priv = BRASERO_DATA_VFS_PRIVATE (self);

	/* check the status of the operation.
	 * NOTE: no need to remove the nodes. */
	if (!brasero_data_vfs_check_uri_result (self, uri, error, info))
		return;

	/* Filtering part */

	name = g_file_info_get_name (info);

	/* See if it's a broken symlink */
	if (g_file_info_get_is_symlink (info)
	&& !g_file_info_get_symlink_target (info)) {
		BraseroDataVFSFilterStatus status;

		/* See if this file is already in filtered */
		status = GPOINTER_TO_INT (g_hash_table_lookup (priv->filtered, uri));
		if (status == BRASERO_DATA_VFS_NONE) {
			uri = brasero_utils_register_string (uri);
			g_hash_table_insert (priv->filtered,
					     (gchar *) uri,
					     GINT_TO_POINTER (BRASERO_DATA_VFS_FILTERED));
		}

		/* See if we are supposed to keep them */
		if (status != BRASERO_DATA_VFS_RESTORED && priv->filter_broken_sym) {
			brasero_data_project_exclude_uri (BRASERO_DATA_PROJECT (self),
							  uri);

			if (status == BRASERO_DATA_VFS_NONE) {
				/* Advertise only once this filtered URI */
				g_signal_emit (self,
					       brasero_data_vfs_signals [FILTERED_SIGNAL],
					       0,
					       BRASERO_FILTER_BROKEN_SYM,
					       uri);
			}

			return;
		}
	}

	/* A new hidden file ? */
	else if (name [0] == '.') {
		BraseroDataVFSFilterStatus status;

		/* See if this file is already in restored */
		status = GPOINTER_TO_INT (g_hash_table_lookup (priv->filtered, uri));
		if (status == BRASERO_DATA_VFS_NONE) {
			uri = brasero_utils_register_string (uri);
			g_hash_table_insert (priv->filtered,
					     (gchar *) uri,
					     GINT_TO_POINTER (BRASERO_DATA_VFS_FILTERED));
		}

		/* See if we are supposed to keep them */
		if (status != BRASERO_DATA_VFS_RESTORED && priv->filter_hidden) {
			brasero_data_project_exclude_uri (BRASERO_DATA_PROJECT (self),
							  uri);

			if (status == BRASERO_DATA_VFS_NONE) {
				/* Advertise only once this filtered URI */
				g_signal_emit (self,
					       brasero_data_vfs_signals [FILTERED_SIGNAL],
					       0,
					       BRASERO_FILTER_HIDDEN,
					       uri);
			}

			return;
		}
	}

	/* add node for all parents */
	nodes = g_hash_table_lookup (priv->directories, parent_uri);
	for (iter = nodes; iter; iter = iter->next) {
		guint reference;
		BraseroFileNode *parent;

		reference = GPOINTER_TO_INT (iter->data);
		parent = brasero_data_project_reference_get (BRASERO_DATA_PROJECT (self), reference);
		if (!parent)
			continue;

		if (g_file_info_get_is_symlink (info)) {
			if (brasero_data_vfs_directory_check_symlink_loop (self, parent, uri, info)) {
				brasero_data_project_exclude_uri (BRASERO_DATA_PROJECT (self), uri);
				if (g_hash_table_lookup (priv->loading, uri))
					g_signal_emit (self,
						       brasero_data_vfs_signals [RECURSIVE_SIGNAL],
						       0,
						       uri);
				return;
			}
		}

		brasero_data_project_add_node_from_info (BRASERO_DATA_PROJECT (self),
							 uri,
							 info,
							 parent);
	}
}

static gboolean
brasero_data_vfs_load_directory (BraseroDataVFS *self,
				 BraseroFileNode *node,
				 const gchar *uri)
{
	BraseroDataVFSPrivate *priv;
	gchar *registered;
	guint reference;
	GSList *nodes;

	priv = BRASERO_DATA_VFS_PRIVATE (self);

	/* Start exploration of directory*/
	reference = brasero_data_project_reference_new (BRASERO_DATA_PROJECT (self), node);

	nodes = g_hash_table_lookup (priv->directories, uri);
	if (nodes) {
		/* It's loading, wait for the results */
		nodes = g_slist_prepend (nodes, GINT_TO_POINTER (reference));
		g_hash_table_insert (priv->directories, (gchar *) uri, nodes);
		return TRUE;
	}

	registered = brasero_utils_register_string (uri);
	g_hash_table_insert (priv->directories,
			     registered,
			     g_slist_prepend (NULL, GINT_TO_POINTER (reference)));

	if (!priv->load_contents)
		priv->load_contents = brasero_io_register (G_OBJECT (self),
							   brasero_data_vfs_directory_load_result,
							   brasero_data_vfs_directory_load_end,
							   NULL);

	/* no need to require mime types here as these rows won't be visible */
	brasero_io_load_directory (priv->io,
				   uri,
				   priv->load_contents,
				   BRASERO_IO_INFO_PERM,
				   registered);

	/* Only emit a signal if state changed. Some widgets need to know if 
	 * either directories loading or uri loading state has changed to signal
	 * it even if there were some uri loading. */
	if (g_hash_table_size (priv->directories) == 1)
		g_signal_emit (self,
			       brasero_data_vfs_signals [ACTIVITY_SIGNAL],
			       0,
			       TRUE);

	return TRUE;
}

/**
 * Update a node already in the tree
 */
static void
brasero_data_vfs_loading_node_end (GObject *object,
				   gboolean cancelled,
				   gpointer data)
{
	BraseroDataVFSPrivate *priv = BRASERO_DATA_VFS_PRIVATE (object);
	BraseroDataVFS *self = BRASERO_DATA_VFS (object);
	gchar *uri = data;

	priv = BRASERO_DATA_VFS_PRIVATE (self);
	brasero_data_vfs_remove_from_hash (self, priv->loading, uri);
	brasero_utils_unregister_string (uri);

	/* Only emit a signal if state changed. Some widgets need to know if 
	 * either directories loading or uri loading state has changed to signal
	 * it even if there were some directories loading.
	 * NOTE: we only cancel when we're stopping. That's why there is no need
	 * to emit any signal in this case (cancellation). */
	if (!g_hash_table_size (priv->loading) && !cancelled)
		g_signal_emit (self,
			       brasero_data_vfs_signals [ACTIVITY_SIGNAL],
			       0,
			       g_hash_table_size (priv->directories));
}

static void
brasero_data_vfs_loading_node_result (GObject *owner,
				      GError *error,
				      const gchar *uri,
				      GFileInfo *info,
				      gpointer NULL_data)
{
	GSList *iter;
	GSList *nodes;
	BraseroDataVFS *self = BRASERO_DATA_VFS (owner);
	BraseroDataVFSPrivate *priv = BRASERO_DATA_VFS_PRIVATE (self);

	nodes = g_hash_table_lookup (priv->loading, uri);

	/* check the status of the operation */
	if (!brasero_data_vfs_check_uri_result (self, uri, error, info)) {
		/* we need to remove the loading node that is waiting */
		for (iter = nodes; iter; iter = iter->next) {
			BraseroFileNode *node;
			guint reference;

			reference = GPOINTER_TO_INT (iter->data);
			node = brasero_data_project_reference_get (BRASERO_DATA_PROJECT (self), reference);
			brasero_data_project_remove_node (BRASERO_DATA_PROJECT (self), node);
		}

		return;
	}

	/* NOTE: we don't check for a broken symlink here since the
	 * user chose to add it. So even if it were we would have to 
	 * add it. The same for hidden files. */
	for (iter = nodes; iter; iter = iter->next) {
		guint reference;
		BraseroFileNode *node;

		reference = GPOINTER_TO_INT (iter->data);

		/* check if the node still exists */
		node = brasero_data_project_reference_get (BRASERO_DATA_PROJECT (self), reference);
		brasero_data_project_reference_free (BRASERO_DATA_PROJECT (self), reference);
		if (!node)
			continue;

		/* if the node is reloading but not loading that means we just
		 * want to know if it is still readable and or if its size (if
		 * it's a file) changed; if we reached this point no need to go
		 * further.
		 * Yet, another case is when we need their mime type. They are
		 * also set as reloading. */

		/* NOTE: check is loading here on purpose. Otherwise directories
		 * that replace a temp parent wouldn't load since they are also
		 * reloading. */
		if (!node->is_loading) {
			brasero_data_project_node_reloaded (BRASERO_DATA_PROJECT (self), node, uri, info);
			continue;
		}

		/* update node */
		brasero_data_project_node_loaded (BRASERO_DATA_PROJECT (self), node, uri, info);

		/* See what type of file it is. If that's a directory then 
		 * explore it right away */
		if (node->is_file)
			continue;

		/* starts exploring its contents */
		brasero_data_vfs_load_directory (self, node, uri);
	}
}

static gboolean
brasero_data_vfs_load_node (BraseroDataVFS *self,
			    BraseroIOFlags flags,
			    guint reference,
			    const gchar *uri)
{
	BraseroDataVFSPrivate *priv;
	gchar *registered;
	gboolean result;

	priv = BRASERO_DATA_VFS_PRIVATE (self);

	registered = brasero_utils_register_string (uri);
	g_hash_table_insert (priv->loading,
			     registered,
			     g_slist_prepend (NULL, GINT_TO_POINTER (reference)));

	if (!priv->load_uri)
		priv->load_uri = brasero_io_register (G_OBJECT (self),
						      brasero_data_vfs_loading_node_result,
						      brasero_data_vfs_loading_node_end,
						      NULL);

	brasero_io_get_file_info (priv->io,
				  uri,
				  priv->load_uri,
				  flags,
				  registered);

	/* Only emit a signal if state changed. Some widgets need to know if 
	 * either directories loading or uri loading state has changed to signal
	 * it even if there were some directories loading. */
	if (g_hash_table_size (priv->loading) == 1)
		g_signal_emit (self,
			       brasero_data_vfs_signals [ACTIVITY_SIGNAL],
			       0,
			       TRUE);

	return result;
}

static gboolean
brasero_data_vfs_loading_node (BraseroDataVFS *self,
			       BraseroFileNode *node,
			       const gchar *uri)
{
	BraseroDataVFSPrivate *priv;
	guint reference;
	GSList *nodes;

	/* NOTE: this function receives URIs only from utf8 origins (not from
	 * gnome-vfs for example) so we can assume that this is safe */

	priv = BRASERO_DATA_VFS_PRIVATE (self);

	if (!node->is_reloading) {
		gchar *name;
		GFile *vfs_uri;
		gchar *unescaped_name;

		/* g_path_get_basename is not comfortable with uri related
		 * to the root directory so check that before */
		vfs_uri = g_file_new_for_uri (uri);
		name = g_file_get_basename (vfs_uri);
		g_object_unref (vfs_uri);

		unescaped_name = g_uri_unescape_string (name, NULL);
		g_free (name);
		name = unescaped_name;

		if (!name)
			return TRUE;

		if (!strcmp (name, G_DIR_SEPARATOR_S)) {
			g_free (name);

			/* This is a root directory: we don't add it since a
			 * child of the root directory can't be a root itself.
			 * So we add all its contents under its parent. Remove
			 * the loading node as well. 
			 * Be careful in the next functions not to use node. */
			brasero_data_vfs_load_directory (self, node->parent, uri);

			/* node was invalidated: return FALSE */
			brasero_data_project_remove_node (BRASERO_DATA_PROJECT (self), node);
			return FALSE;
		}
		g_free (name);
	}

	/* FIXME: we could know right from the start if that node is is_loading */
	/* add a reference on the node to update it when we have all information */
	reference = brasero_data_project_reference_new (BRASERO_DATA_PROJECT (self), node);
	nodes = g_hash_table_lookup (priv->loading, uri);
	if (nodes) {
		/* It's loading, wait for the results */
		nodes = g_slist_prepend (nodes, GINT_TO_POINTER (reference));
		g_hash_table_insert (priv->loading, (gchar *) uri, nodes);
		return TRUE;
	}

	/* loading nodes are almost always visible already so get mime type */
	return brasero_data_vfs_load_node (self,
					   BRASERO_IO_INFO_PERM|
					   BRASERO_IO_INFO_MIME|
					   BRASERO_IO_INFO_CHECK_PARENT_SYMLINK,
					   reference,
					   uri);
}

static gboolean
brasero_data_vfs_increase_priority_cb (gpointer data, gpointer user_data)
{
	if (data == user_data)
		return TRUE;

	return FALSE;
}

static gboolean
brasero_data_vfs_require_higher_priority (BraseroDataVFS *self,
					  BraseroFileNode *node,
					  BraseroIO *io,
					  BraseroIOJobBase *type)
{
	gchar *registered;
	gchar *uri;

	uri = brasero_data_project_node_to_uri (BRASERO_DATA_PROJECT (self), node);
	registered = brasero_utils_register_string (uri);
	g_free (uri);

	brasero_io_find_urgent (io,
				type,
				brasero_data_vfs_increase_priority_cb,
				registered);

	brasero_utils_unregister_string (registered);
	return TRUE;
}

gboolean
brasero_data_vfs_require_directory_contents (BraseroDataVFS *self,
					     BraseroFileNode *node)
{
	BraseroDataVFSPrivate *priv;

	priv = BRASERO_DATA_VFS_PRIVATE (self);
	return brasero_data_vfs_require_higher_priority (self,
							 node,
							 priv->io,
							 priv->load_contents);
}

gboolean
brasero_data_vfs_require_node_load (BraseroDataVFS *self,
				    BraseroFileNode *node)
{
	BraseroDataVFSPrivate *priv;

	priv = BRASERO_DATA_VFS_PRIVATE (self);
	return brasero_data_vfs_require_higher_priority (self,
							 node,
							 priv->io,
							 priv->load_uri);
}

gboolean
brasero_data_vfs_load_mime (BraseroDataVFS *self,
			    BraseroFileNode *node)
{
	BraseroDataVFSPrivate *priv;
	guint reference;
	gboolean result;
	GSList *nodes;
	gchar *uri;

	priv = BRASERO_DATA_VFS_PRIVATE (self);

	if (node->is_loading || node->is_reloading) {
		brasero_data_vfs_require_node_load (self, node);
		return TRUE;
	}

	uri = brasero_data_project_node_to_uri (BRASERO_DATA_PROJECT (self), node);
	node->is_reloading = TRUE;

	/* make sure the node is not already in the loading table */
	nodes = g_hash_table_lookup (priv->loading, uri);
	if (nodes) {
		gchar *registered;

		registered = brasero_utils_register_string (uri);
		g_free (uri);

		for (; nodes; nodes = nodes->next) {
			guint reference;
			BraseroFileNode *ref_node;

			reference = GPOINTER_TO_INT (nodes->data);
			ref_node = brasero_data_project_reference_get (BRASERO_DATA_PROJECT (self), reference);
			if (ref_node == node) {
				/* 1sk for a higher priority */
				brasero_io_find_urgent (priv->io,
							priv->load_uri,
							brasero_data_vfs_increase_priority_cb,
							registered);
				brasero_utils_unregister_string (registered);
				return TRUE;
			}
		}

		/* It's loading, wait for the results */
		reference = brasero_data_project_reference_new (BRASERO_DATA_PROJECT (self), node);
		nodes = g_slist_prepend (nodes, GINT_TO_POINTER (reference));
		g_hash_table_insert (priv->loading, (gchar *) uri, nodes);

		/* Yet, ask for a higher priority */
		brasero_io_find_urgent (priv->io,
					priv->load_uri,
					brasero_data_vfs_increase_priority_cb,
					registered);
		brasero_utils_unregister_string (registered);
		return TRUE;
	}

	reference = brasero_data_project_reference_new (BRASERO_DATA_PROJECT (self), node);
	result = brasero_data_vfs_load_node (self,
					     BRASERO_IO_INFO_MIME|
					     BRASERO_IO_INFO_URGENT,
					     reference,
					     uri);
	g_free (uri);

	return result;
}

/**
 * This function implements the virtual function from data-project
 * It checks the node type and if it is a directory, it explores it
 */
static gboolean
brasero_data_vfs_node_added (BraseroDataProject *project,
			     BraseroFileNode *node,
			     const gchar *uri)
{
	BraseroDataVFSPrivate *priv;
	BraseroDataVFS *self;

	self = BRASERO_DATA_VFS (project);
	priv = BRASERO_DATA_VFS_PRIVATE (self);

	/* URI can be NULL if it's a created directory or if the node
	 * has just been moved to another location in the tree. */
	if (!uri)
		goto chain;

	/* Is it loading or reloading? if not, only explore directories. */
	if (node->is_loading || node->is_reloading) {
		if (brasero_data_vfs_loading_node (self, node, uri))
			goto chain;

		return FALSE;
	}

	/* NOTE: a symlink pointing to a directory will return TRUE. */
	if (node->is_file)
		goto chain;

	brasero_data_vfs_load_directory (self, node, uri);

chain:
	/* chain up this function except if we invalidated the node */
	if (BRASERO_DATA_PROJECT_CLASS (brasero_data_vfs_parent_class)->node_added)
		return BRASERO_DATA_PROJECT_CLASS (brasero_data_vfs_parent_class)->node_added (project, node, uri);

	return TRUE;
}

static gboolean
brasero_data_vfs_empty_loading_cb (gpointer key,
				   gpointer data,
				   gpointer callback_data)
{
	BraseroDataProject *project = BRASERO_DATA_PROJECT (callback_data);
	GSList *nodes = data;
	GSList *iter;

	brasero_utils_unregister_string (key);
	for (iter = nodes; iter; iter = iter->next) {
		guint reference;

		reference = GPOINTER_TO_INT (iter->data);
		brasero_data_project_reference_free (project, reference);
	}
	g_slist_free (nodes);
	return TRUE;
}

static gboolean
brasero_data_vfs_empty_filtered_cb (gpointer key,
				    gpointer data,
				    gpointer callback_data)
{
	brasero_utils_unregister_string (key);
	return TRUE;
}

static void
brasero_data_vfs_clear (BraseroDataVFS *self)
{
	BraseroDataVFSPrivate *priv;

	priv = BRASERO_DATA_VFS_PRIVATE (self);

	/* Stop all VFS operations */
	if (priv->io) {
		brasero_io_cancel_by_base (priv->io, priv->load_uri);
		brasero_io_cancel_by_base (priv->io, priv->load_contents);

		g_free (priv->load_uri);
		priv->load_uri = NULL;

		g_free (priv->load_contents);
		priv->load_contents = NULL;
	}

	/* Empty the hash tables */
	g_hash_table_foreach_remove (priv->loading,
				     brasero_data_vfs_empty_loading_cb,
				     self);
	g_hash_table_foreach_remove (priv->directories,
				     brasero_data_vfs_empty_loading_cb,
				     self);
	g_hash_table_foreach_remove (priv->filtered,
				     brasero_data_vfs_empty_filtered_cb,
				     self);
}

static void
brasero_data_vfs_reset (BraseroDataProject *project)
{
	brasero_data_vfs_clear (BRASERO_DATA_VFS (project));

	/* chain up this function except if we invalidated the node */
	if (BRASERO_DATA_PROJECT_CLASS (brasero_data_vfs_parent_class)->reset)
		BRASERO_DATA_PROJECT_CLASS (brasero_data_vfs_parent_class)->reset (project);
}

static void
brasero_data_vfs_init (BraseroDataVFS *object)
{
	GConfClient *client;
	BraseroDataVFSPrivate *priv;

	priv = BRASERO_DATA_VFS_PRIVATE (object);

	/* load the fitering rules */
	client = gconf_client_get_default ();
	priv->filter_hidden = gconf_client_get_bool (client,
						     BRASERO_FILTER_HIDDEN_KEY,
						     NULL);
	priv->filter_broken_sym = gconf_client_get_bool (client,
							 BRASERO_FILTER_BROKEN_SYM_KEY,
							 NULL);
	g_object_unref (client);

	/* create the hash tables */
	priv->loading = g_hash_table_new (g_str_hash, g_str_equal);
	priv->directories = g_hash_table_new (g_str_hash, g_str_equal);
	priv->filtered = g_hash_table_new (g_str_hash, g_str_equal);

	/* get the vfs object */
	priv->io = brasero_io_get_default ();
}

static void
brasero_data_vfs_finalize (GObject *object)
{
	BraseroDataVFSPrivate *priv;

	brasero_data_vfs_clear (BRASERO_DATA_VFS (object));

	priv = BRASERO_DATA_VFS_PRIVATE (object);
	if (priv->loading) {
		g_hash_table_destroy (priv->loading);
		priv->loading = NULL;
	}

	if (priv->directories) {
		g_hash_table_destroy (priv->directories);
		priv->directories = NULL;
	}

	if (priv->filtered) {
		g_hash_table_destroy (priv->filtered);
		priv->filtered = NULL;
	}

	if (priv->io) {
		g_object_unref (priv->io);
		priv->io = NULL;
	}

	G_OBJECT_CLASS (brasero_data_vfs_parent_class)->finalize (object);
}

static void
brasero_data_vfs_class_init (BraseroDataVFSClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	BraseroDataProjectClass *data_project_class = BRASERO_DATA_PROJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroDataVFSPrivate));

	object_class->finalize = brasero_data_vfs_finalize;

	data_project_class->reset = brasero_data_vfs_reset;
	data_project_class->node_added = brasero_data_vfs_node_added;

	/* There is no need to implement the other virtual functions.
	 * For example, even if we were notified of a node removal it 
	 * would take a lot of time to remove it from the hashes. */

	brasero_data_vfs_signals [ACTIVITY_SIGNAL] = 
	    g_signal_new ("vfs_activity",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_FIRST|G_SIGNAL_NO_RECURSE,
			  G_STRUCT_OFFSET (BraseroDataVFSClass,
					   activity_changed),
			  NULL, NULL,
			  g_cclosure_marshal_VOID__BOOLEAN,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_BOOLEAN);

	brasero_data_vfs_signals [FILTERED_SIGNAL] = 
	    g_signal_new ("filtered_uri",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_FIRST,
			  0,
			  NULL, NULL,
			  brasero_marshal_VOID__INT_STRING,
			  G_TYPE_NONE,
			  2,
			  G_TYPE_INT,
			  G_TYPE_STRING);

	brasero_data_vfs_signals [UNREADABLE_SIGNAL] = 
	    g_signal_new ("unreadable_uri",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_FIRST,
			  0,
			  NULL, NULL,
			  brasero_marshal_VOID__INT_STRING,
			  G_TYPE_NONE,
			  2,
			  G_TYPE_POINTER,
			  G_TYPE_STRING);

	brasero_data_vfs_signals [RECURSIVE_SIGNAL] = 
	    g_signal_new ("recursive_sym",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_FIRST,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__STRING,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_STRING);

	brasero_data_vfs_signals [UNKNOWN_SIGNAL] = 
	    g_signal_new ("unknown_uri",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_FIRST,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__STRING,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_STRING);
}
