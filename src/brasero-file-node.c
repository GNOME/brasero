/***************************************************************************
 *            brasero-file-node.c
 *
 *  Sat Dec  1 14:48:46 2007
 *  Copyright  2007  Philippe Rouquier
 *  <bonfire-app@wanadoo.fr>
 ****************************************************************************/

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include <gio/gio.h>

#include "burn-basics.h"

#include "brasero-file-node.h"
#include "brasero-utils.h"

BraseroFileNode *
brasero_file_node_root_new (void)
{
	BraseroFileNode *root;

	root = g_new0 (BraseroFileNode, 1);
	root->is_root = TRUE;
	root->is_imported = TRUE;

	root->union3.stats = g_new0 (BraseroFileTreeStats, 1);
	return root;
}

BraseroFileNode *
brasero_file_node_get_root (BraseroFileNode *node,
			    guint *depth_retval)
{
	BraseroFileNode *parent;
	guint depth = 0;

	parent = node;
	while (parent) {
		if (parent->is_root) {
			if (depth_retval)
				*depth_retval = depth;

			return parent;
		}

		depth ++;
		parent = parent->parent;
	}

	return NULL;
}


guint
brasero_file_node_get_depth (BraseroFileNode *node)
{
	guint depth = 0;

	while (node) {
		if (node->is_root)
			return depth;

		depth ++;
		node = node->parent;
	}

	return 0;
}

BraseroFileTreeStats *
brasero_file_node_get_tree_stats (BraseroFileNode *node,
				  guint *depth)
{
	BraseroFileTreeStats *stats;
	BraseroFileNode *root;

	stats = BRASERO_FILE_NODE_STATS (node);
	if (stats)
		return stats;

	root = brasero_file_node_get_root (node, depth);
	stats = BRASERO_FILE_NODE_STATS (root);

	return stats;
}

gint
brasero_file_node_sort_default_cb (gconstpointer obj_a, gconstpointer obj_b)
{
	const BraseroFileNode *a = obj_a;
	const BraseroFileNode *b = obj_b;

	if (a->is_file == b->is_file)
		return 0;

	if (b->is_file)
		return -1;
	
	return 1;
}

gint
brasero_file_node_sort_name_cb (gconstpointer obj_a, gconstpointer obj_b)
{
	gint res;
	const BraseroFileNode *a = obj_a;
	const BraseroFileNode *b = obj_b;


	res = brasero_file_node_sort_default_cb (a, b);
	if (res)
		return res;

	return strcmp (BRASERO_FILE_NODE_NAME (a), BRASERO_FILE_NODE_NAME (b));
}

gint
brasero_file_node_sort_size_cb (gconstpointer obj_a, gconstpointer obj_b)
{
	gint res;
	gint num_a, num_b;
	const BraseroFileNode *a = obj_a;
	const BraseroFileNode *b = obj_b;


	res = brasero_file_node_sort_default_cb (a, b);
	if (res)
		return res;

	if (a->is_file)
		return BRASERO_FILE_NODE_SECTORS (a) - BRASERO_FILE_NODE_SECTORS (b);

	/* directories */
	num_a = brasero_file_node_get_n_children (a);
	num_b = brasero_file_node_get_n_children (b);
	return num_a - num_b;
}

gint 
brasero_file_node_sort_mime_cb (gconstpointer obj_a, gconstpointer obj_b)
{
	gint res;
	const BraseroFileNode *a = obj_a;
	const BraseroFileNode *b = obj_b;


	res = brasero_file_node_sort_default_cb (a, b);
	if (res)
		return res;

	return strcmp (BRASERO_FILE_NODE_NAME (a), BRASERO_FILE_NODE_NAME (b));
}

static BraseroFileNode *
brasero_file_node_insert (BraseroFileNode *head,
			  BraseroFileNode *node,
			  GCompareFunc sort_func,
			  guint *newpos)
{
	BraseroFileNode *iter;
	guint n = 0;

	/* check for some special cases */
	if (!head) {
		node->next = NULL;
		return node;
	}

	if (sort_func (head, node) > 0) {
		/* head is after node */
		node->next = head;
		if (newpos)
			*newpos = 0;

		return node;
	}

	n = 1;
	for (iter = head; iter->next; iter = iter->next) {
		if (sort_func (iter->next, node) > 0) {
			/* iter->next should be located after node */
			node->next = iter->next;
			iter->next = node;

			if (newpos)
				*newpos = n;

			return head;
		}
		n ++;
	}

	/* append it */
	iter->next = node;
	node->next = NULL;

	if (newpos)
		*newpos = n;

	return head;
}

gint *
brasero_file_node_need_resort (BraseroFileNode *node,
			       GCompareFunc sort_func)
{
	BraseroFileNode *previous;
	BraseroFileNode *parent;
	BraseroFileNode *head;
	gint *array = NULL;
	guint newpos = 0;
	guint oldpos;
	guint size;

	parent = node->parent;
	head = BRASERO_FILE_NODE_CHILDREN (parent);

	/* find previous node and get old position */
	if (head != node) {
		previous = head;
		oldpos = 0;
		while (previous->next != node) {
			previous = previous->next;
			oldpos ++;
		}
		oldpos ++;
	}
	else {
		previous = NULL;
		oldpos = 0;
	}

	/* see where we should start from head or from node->next */
	if (previous && sort_func (previous, node) > 0) {
		gint i;

		/* move on the left */

		previous->next = node->next;

		head = brasero_file_node_insert (head, node, sort_func, &newpos);
		parent->union2.children = head;

		/* create an array to reflect the changes */
		size = brasero_file_node_get_n_children (parent);
		array = g_new0 (gint, size);

		for (i = 0; i < size; i ++) {
			if (i == newpos)
				array [i] = oldpos;
			else if (i > newpos && i <= oldpos)
				array [i] = i - 1;
			else
				array [i] = i;
		}
	}
	else if (node->next && sort_func (node, node->next) > 0) {
		gint i;

		/* move on the right */

		if (previous)
			previous->next = node->next;
		else
			parent->union2.children = node->next;

		/* NOTE: here we're sure head hasn't changed since we checked 
		 * that node should go after node->next (given as head for the
		 * insertion here) */
		brasero_file_node_insert (node->next, node, sort_func, &newpos);

		/* we started from oldpos so newpos needs updating */
		newpos += oldpos;

		/* create an array to reflect the changes */
		size = brasero_file_node_get_n_children (parent);
		array = g_new0 (gint, size);

		for (i = 0; i < size; i ++) {
			if (i == newpos)
				array [i] = oldpos;
			else if (i >= oldpos && i < newpos)
				array [i] = i + 1;
			else
				array [i] = i;
		}
	}

	return array;
}

gint *
brasero_file_node_sort_children (BraseroFileNode *parent,
				 GCompareFunc sort_func)
{
	BraseroFileNode *new_order = NULL;
	BraseroFileNode *iter;
	BraseroFileNode *next;
	gint *array = NULL;
	gint num_children;
	guint oldpos = 1;
	guint newpos;

	new_order = BRASERO_FILE_NODE_CHILDREN (parent);

	/* check for some special cases */
	if (!new_order)
		return NULL;

	if (!new_order->next)
		return NULL;

	/* make the array */
	num_children = brasero_file_node_get_n_children (parent);
	array = g_new (gint, num_children);

	next = new_order->next;
	new_order->next = NULL;
	array [0] = 0;

	for (iter = next; iter; iter = next, oldpos ++) {
		/* unlink iter */
		next = iter->next;
		iter->next = NULL;

		newpos = 0;
		new_order = brasero_file_node_insert (new_order,
						      iter,
						      sort_func,
						      &newpos);

		if (newpos < oldpos)
			memmove (array + newpos + 1, array + newpos, (oldpos - newpos) * sizeof (guint));

		array [newpos] = oldpos;
	}

	/* set the new order */
	parent->union2.children = new_order;

	return array;
}

gint *
brasero_file_node_reverse_children (BraseroFileNode *parent)
{
	BraseroFileNode *previous;
	BraseroFileNode *last;
	BraseroFileNode *iter;
	gint firstfile = 0;
	gint *array;
	gint size;
	gint i;
	
	/* when reversing the list of children the only thing we must pay 
	 * attention to is to keep directories first; so we do it in two passes
	 * first order the directories and then the files */
	last = BRASERO_FILE_NODE_CHILDREN (parent);

	/* special case */
	if (!last || !last->next)
		return NULL;

	previous = last;
	iter = last->next;
	size = 1;

	if (!last->is_file) {
		while (!iter->is_file) {
			BraseroFileNode *next;

			next = iter->next;
			iter->next = previous;

			size ++;
			if (!next) {
				/* No file afterwards */
				parent->union2.children = iter;
				last->next = NULL;
				firstfile = size;
				goto end;
			}

			previous = iter;
			iter = next;
		}

		/* the new head is the last processed node */
		parent->union2.children = previous;
		firstfile = size;

		previous = iter;
		iter = iter->next;
		previous->next = NULL;
	}

	while (iter) {
		BraseroFileNode *next;

		next = iter->next;
		iter->next = previous;

		size ++;

		previous = iter;
		iter = next;
	}

	/* NOTE: iter is NULL here */
	if (last->is_file) {
		last->next = NULL;
		parent->union2.children = previous;
	}
	else
		last->next = previous;

end:

	array = g_new (gint, size);

	for (i = 0; i < firstfile; i ++)
		array [i] = firstfile - i - 1;

	for (i = firstfile; i < size; i ++)
		array [i] = size - i + firstfile - 1;

	return array;
}

BraseroFileNode *
brasero_file_node_nth_child (BraseroFileNode *parent,
			     guint nth)
{
	BraseroFileNode *peers;
	guint pos;

	peers = BRASERO_FILE_NODE_CHILDREN (parent);
	for (pos = 0; pos < nth && peers; pos ++)
		peers = peers->next;

	return peers;
}

guint
brasero_file_node_get_n_children (const BraseroFileNode *node)
{
	BraseroFileNode *children;
	guint num = 0;

	for (children = BRASERO_FILE_NODE_CHILDREN (node); children; children = children->next)
		num ++;

	return num;
}

guint
brasero_file_node_get_pos_as_child (BraseroFileNode *node)
{
	BraseroFileNode *parent;
	BraseroFileNode *peers;
	guint pos = 0;

	parent = node->parent;
	for (peers = BRASERO_FILE_NODE_CHILDREN (parent); peers; peers = peers->next) {
		if (peers == node)
			break;
		pos ++;
	}

	return pos;
}

gboolean
brasero_file_node_is_ancestor (BraseroFileNode *parent,
			       BraseroFileNode *node)
{
	while (node && node != parent)
		node = node->parent;

	if (!node)
		return FALSE;

	return TRUE;
}

BraseroFileNode *
brasero_file_node_check_name_existence (BraseroFileNode *parent,
				        const gchar *name)
{
	BraseroFileNode *iter;

	if (name && name [0] == '\0')
		return NULL;

	iter = BRASERO_FILE_NODE_CHILDREN (parent);
	for (; iter; iter = iter->next) {
		if (!strcmp (name, BRASERO_FILE_NODE_NAME (iter)))
			return iter;
	}

	return NULL;
}

BraseroFileNode *
brasero_file_node_check_imported_sibling (BraseroFileNode *node)
{
	BraseroFileNode *parent;
	BraseroFileNode *iter;
	BraseroImport *import;

	parent = node->parent;

	/* That could happen if a node is moved to a location where another node
	 * (to be removed) has the same name and is a parent of this node */
	if (!parent)
		return NULL;

	/* See if among the imported children of the parent one of them
	 * has the same name as the node being removed. If so, restore
	 * it with all its imported children (provided that's a
	 * directory). */
	import = BRASERO_FILE_NODE_IMPORT (parent);
	if (!import)
		return NULL;

	iter = import->replaced;
	if (!strcmp (BRASERO_FILE_NODE_NAME (iter), BRASERO_FILE_NODE_NAME (node))) {
		/* A match, remove it from the list and return it */
		import->replaced = iter->next;
		if (!import->replaced) {
			/* no more imported saved import structure */
			parent->union1.name = import->name;
			parent->has_import = FALSE;
			g_free (import);
		}

		iter->next = NULL;
		return iter;			
	}

	for (; iter->next; iter = iter->next) {
		if (!strcmp (BRASERO_FILE_NODE_NAME (iter->next), BRASERO_FILE_NODE_NAME (node))) {
			BraseroFileNode *removed;
			/* There is one match, remove it from the list */
			removed = iter->next;
			iter->next = removed->next;
			removed->next = NULL;
			return removed;
		}
	}

	return NULL;
}

gchar *
brasero_file_node_validate_utf8_name (const gchar *name)
{
	gchar *retval, *ptr;
	const gchar *invalid;

	if (!name)
		return NULL;

	if (g_utf8_validate (name, -1, &invalid))
		return NULL;

	retval = g_strdup (name);
	ptr = retval + (invalid - name);
	*ptr = '_';
	ptr++;

	while (!g_utf8_validate (ptr, -1, &invalid)) {
		ptr = (gchar*) invalid;
		*ptr = '?';
		ptr ++;
	}

	return retval;
}

void
brasero_file_node_graft (BraseroFileNode *file_node,
			 BraseroURINode *uri_node)
{
	BraseroGraft *graft;

	if (!file_node->is_grafted) {
		BraseroFileNode *parent;

		graft = g_new (BraseroGraft, 1);
		graft->name = file_node->union1.name;
		file_node->union1.graft = graft;
		file_node->is_grafted = TRUE;

		/* since it wasn't grafted propagate the size change; that is
		 * substract the current node size from the parent nodes until
		 * the parent graft point. */
		for (parent = file_node->parent; parent && !parent->is_root; parent = parent->parent) {
			parent->union3.sectors -= BRASERO_FILE_NODE_SECTORS (file_node);
			if (parent->is_grafted)
				break;
		}
	}
	else {
		BraseroURINode *old_uri_node;

		graft = BRASERO_FILE_NODE_GRAFT (file_node);
		old_uri_node = graft->node;
		if (old_uri_node == uri_node)
			return;

		old_uri_node->nodes = g_slist_remove (old_uri_node->nodes, file_node);
	}

	graft->node = uri_node;
	uri_node->nodes = g_slist_prepend (uri_node->nodes, file_node);
}

void
brasero_file_node_ungraft (BraseroFileNode *node)
{
	BraseroGraft *graft;
	BraseroFileNode *parent;

	if (!node->is_grafted)
		return;

	graft = node->union1.graft;

	/* Remove it from the URINode list of grafts */
	graft->node->nodes = g_slist_remove (graft->node->nodes, node);

	/* The name must be exactly the one of the URI*/
	node->is_grafted = FALSE;
	node->union1.name = graft->name;

	/* Removes the graft */
	g_free (graft);

	/* Propagate the size change up the parents to the next
	 * grafted parent in the tree (if any). */
	for (parent = node->parent; parent && !parent->is_root; parent = parent->parent) {
		parent->union3.sectors += BRASERO_FILE_NODE_SECTORS (node);
		if (parent->is_grafted)
			break;
	}
}

void
brasero_file_node_rename (BraseroFileNode *node,
			  const gchar *name)
{
	g_free (BRASERO_FILE_NODE_NAME (node));
	if (node->is_grafted)
		node->union1.graft->name = g_strdup (name);
	else
		node->union1.name = g_strdup (name);
}

void
brasero_file_node_add (BraseroFileNode *parent,
		       BraseroFileNode *node,
		       GCompareFunc sort_func)
{
	parent->union2.children = brasero_file_node_insert (BRASERO_FILE_NODE_CHILDREN (parent),
							    node,
							    sort_func,
							    NULL);

	node->parent = parent;
	if (!node->is_imported) {
		BraseroFileTreeStats *stats;
		guint depth = 0;

		if (!node->is_grafted) {
			/* propagate the size change*/
			for (; parent && !parent->is_root; parent = parent->parent) {
				parent->union3.sectors += BRASERO_FILE_NODE_SECTORS (node);
				if (parent->is_grafted)
					break;
			}
		}

		stats = brasero_file_node_get_tree_stats (parent, &depth);
		if (node->is_file) {
			/* only count files */
			stats->children ++;
		}

		if (depth > 6)
			node->is_deep = TRUE;
	}
}

void
brasero_file_node_set_from_info (BraseroFileNode *node,
				 BraseroFileTreeStats *stats,
				 GFileInfo *info)
{
	/* NOTE: the name will never be replaced here since that means
	 * we could replace a previously set name (that triggered the
	 * creation of a graft). If someone wants to set a new name,
	 * then rename_node is the function. */

	/* update :
	 * - the mime type
	 * - the size (and possibly the one of his parent)
	 * - the type */
	node->is_file = (g_file_info_get_file_type (info) != G_FILE_TYPE_DIRECTORY);
	node->is_fake = FALSE;
	node->is_loading = FALSE;
	node->is_imported = FALSE;
	node->is_reloading = FALSE;
	node->is_symlink = (g_file_info_get_is_symlink (info));

	if (node->is_file) {
		guint sectors;
		gint sectors_diff;

		/* register mime type string */
		if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE)) {
			const gchar *mime;

			if (BRASERO_FILE_NODE_MIME (node))
				brasero_utils_unregister_string (BRASERO_FILE_NODE_MIME (node));

			mime = g_file_info_get_content_type (info);
			node->union2.mime = brasero_utils_register_string (mime);
		}

		sectors = BRASERO_SIZE_TO_SECTORS (g_file_info_get_size (info), 2048);

		if (sectors > BRASERO_FILE_2G_LIMIT && BRASERO_FILE_NODE_SECTORS (node) <= BRASERO_FILE_2G_LIMIT)
			stats->num_2Gio ++;
		else if (sectors <= BRASERO_FILE_2G_LIMIT && BRASERO_FILE_NODE_SECTORS (node) > BRASERO_FILE_2G_LIMIT)
			stats->num_2Gio --;

		/* The node isn't grafted and it's a file. So we must propagate
		 * its size up to the parent graft node. */
		/* NOTE: we used to accumulate all the directory contents till
		 * the end and process all of entries at once, when it was
		 * finished. We had to do that to calculate the whole size. */
		sectors_diff = sectors - BRASERO_FILE_NODE_SECTORS (node);
		for (; node; node = node->parent) {
			node->union3.sectors += sectors_diff;
			if (node->is_grafted)
				break;
		}
	}
	else	/* since that's directory then it must be explored now */
		node->is_exploring = TRUE;
}

gchar *
brasero_file_node_get_uri_name (const gchar *uri)
{
	gchar *unescaped_name;
	GFile *vfs_uri;
	gchar *name;

	/* g_path_get_basename is not comfortable with uri related
	 * to the root directory so check that before */
	vfs_uri = g_file_new_for_uri (uri);
	name = g_file_get_basename (vfs_uri);
	g_object_unref (vfs_uri);

	unescaped_name = g_uri_unescape_string (name, NULL);
	g_free (name);

	/* NOTE: a graft should be added for non utf8 name since we
	 * modify them; in fact we use this function only in the next
	 * one which creates only grafted nodes. */
	name = brasero_file_node_validate_utf8_name (unescaped_name);
	if (name) {
		g_free (unescaped_name);
		return name;
	}
	return unescaped_name;
}

BraseroFileNode *
brasero_file_node_new_loading (const gchar *name,
			       BraseroFileNode *parent,
			       GCompareFunc sort_func)
{
	BraseroFileNode *node;

	node = g_new0 (BraseroFileNode, 1);
	node->union1.name = g_strdup (name);
	node->is_loading = TRUE;

	brasero_file_node_add (parent, node, sort_func);

	return node;
}

BraseroFileNode *
brasero_file_node_new_from_info (GFileInfo *info,
				 BraseroFileNode *parent,
				 GCompareFunc sort_func)
{
	BraseroFileNode *node;
	BraseroFileTreeStats *stats;

	node = g_new0 (BraseroFileNode, 1);
	node->union1.name = g_strdup (g_file_info_get_name (info));

	stats = brasero_file_node_get_tree_stats (parent, NULL);
	brasero_file_node_set_from_info (node, stats, info);

	/* This must be done after above function */
	brasero_file_node_add (parent, node, sort_func);

	return node;
}

BraseroFileNode *
brasero_file_node_new_imported_session_file (BraseroVolFile *file,
					     BraseroFileNode *parent,
					     GCompareFunc sort_func)
{
	BraseroFileNode *node;

	/* Create the node information */
	node = g_new0 (BraseroFileNode, 1);
	node->union1.name = g_strdup (BRASERO_VOLUME_FILE_NAME (file));
	node->is_file = (file->isdir == FALSE);
	node->is_imported = TRUE;

	if (node->is_file)
		node->union3.sectors = BRASERO_SIZE_TO_SECTORS (BRASERO_VOLUME_FILE_SIZE (file), 2048);

	/* Add it (we must add a graft) */
	brasero_file_node_add (parent, node, sort_func);
	return node;
}

BraseroFileNode *
brasero_file_node_new_empty_folder (const gchar *name,
				    BraseroFileNode *parent,
				    GCompareFunc sort_func)
{
	BraseroFileNode *node;

	/* Create the node information */
	node = g_new0 (BraseroFileNode, 1);
	node->union1.name = g_strdup (name);
	node->is_fake = TRUE;

	/* Add it (we must add a graft) */
	brasero_file_node_add (parent, node, sort_func);
	return node;
}

void
brasero_file_node_unlink (BraseroFileNode *node)
{
	BraseroFileNode *iter;
	BraseroImport *import;

	if (!node->parent)
		return;

	iter = BRASERO_FILE_NODE_CHILDREN (node->parent);

	/* handle the size change for previous parent */
	if (!node->is_grafted && !node->is_imported) {
		BraseroFileNode *parent;

		/* handle the size change if it wasn't grafted */
		for (parent = node->parent; parent && !parent->is_root; parent = parent->parent) {
			parent->union3.sectors -= BRASERO_FILE_NODE_SECTORS (node);
			if (parent->is_grafted)
				break;
		}
	}

	node->is_deep = FALSE;

	if (iter == node) {
		node->parent->union2.children = node->next;
		node->parent = NULL;
		node->next = NULL;
		return;
	}

	for (; iter->next; iter = iter->next) {
		if (iter->next == node) {
			iter->next = node->next;
			node->parent = NULL;
			node->next = NULL;
			return;
		}
	}

	if (!node->is_imported || !node->parent->has_import)
		return;

	/* It wasn't found among the parent children. If parent is imported and
	 * the node is imported as well then check if it isn't among the import
	 * children */
	import = BRASERO_FILE_NODE_IMPORT (node->parent);
	iter = import->replaced;

	if (iter == node) {
		import->replaced = iter->next;
		node->parent = NULL;
		node->next = NULL;
		return;
	}

	for (; iter->next; iter = iter->next) {
		if (iter->next == node) {
			iter->next = node->next;
			node->parent = NULL;
			node->next = NULL;
			return;
		}
	}
}

void
brasero_file_node_move_from (BraseroFileNode *node,
			     BraseroFileTreeStats *stats)
{
	gboolean was_deep;

	/* NOTE: for the time being no backend supports moving imported files */
	if (node->is_imported)
		return;

	was_deep = (brasero_file_node_get_depth (node) > 6);
	if (was_deep)
		stats->num_deep --;

	brasero_file_node_unlink (node);
}

void
brasero_file_node_move_to (BraseroFileNode *node,
			   BraseroFileNode *parent,
			   GCompareFunc sort_func)
{
	BraseroFileTreeStats *stats;
	guint depth;

	/* NOTE: for the time being no backend supports moving imported files */
	if (node->is_imported)
		return;

	/* reinsert it now at the new location */
	parent->union2.children = brasero_file_node_insert (BRASERO_FILE_NODE_CHILDREN (parent),
							    node,
							    sort_func,
							    NULL);
	node->parent = parent;

	if (!node->is_grafted) {
		BraseroFileNode *parent;

		/* propagate the size change for new parent */
		for (parent = node->parent; parent && !parent->is_root; parent = parent->parent) {
			parent->union3.sectors += BRASERO_FILE_NODE_SECTORS (node);
			if (parent->is_grafted)
				break;
		}
	}

	/* NOTE: here stats about the tree can change if the parent has a depth
	 * > 6 and if previous didn't. Other stats remains unmodified. */
	stats = brasero_file_node_get_tree_stats (parent, &depth);
	if (!depth > 6) {
		stats->num_deep ++;
		node->is_deep = TRUE;
	}
}

static void
brasero_file_node_destroy_with_children (BraseroFileNode *node,
					 BraseroFileTreeStats *stats)
{
	BraseroFileNode *child;
	BraseroFileNode *next;
	BraseroImport *import;
	BraseroGraft *graft;

	/* destroy all children recursively */
	for (child = BRASERO_FILE_NODE_CHILDREN (node); child; child = next) {
		next = child->next;
		brasero_file_node_destroy_with_children (child, stats);
	}

	/* update all statistics on tree if any */
	if (stats) {
		/* check if that's a 2 Gio file */
		if (node->is_2Gio)
			stats->num_2Gio --;

		/* check if that's a deep directory file */
		if (node->is_deep)
			stats->num_deep --;

		/* update file number statistics */
		if (!node->is_imported && node->is_file)
			stats->children --;
	}

	/* destruction */
	import = BRASERO_FILE_NODE_IMPORT (node);
	graft = BRASERO_FILE_NODE_GRAFT (node);
	if (graft) {
		BraseroURINode *uri_node;

		uri_node = graft->node;

		/* Handle removal from BraseroURINode struct */
		if (uri_node)
			uri_node->nodes = g_slist_remove (uri_node->nodes, node);

		g_free (graft->name);
		g_free (graft);
	}
	else if (import) {
		/* if imported then destroy the saved children */
		for (child = import->replaced; child; child = next) {
			next = child->next;
			brasero_file_node_destroy_with_children (child, stats);
		}

		g_free (import->name);
		g_free (import);
	}
	else
		g_free (BRASERO_FILE_NODE_NAME (node));

	/* destroy the node */
	if (node->is_file && !node->is_imported && BRASERO_FILE_NODE_MIME (node))
		brasero_utils_unregister_string (BRASERO_FILE_NODE_MIME (node));

	if (node->is_root)
		g_free (BRASERO_FILE_NODE_STATS (node));

	g_free (node);
}

/**
 * Destroy a node and its children updating the tree stats.
 * If it isn't unlinked yet, it does it.
 */
void
brasero_file_node_destroy (BraseroFileNode *node,
			   BraseroFileTreeStats *stats)
{
	/* remove from the parent children list or more probably from the 
	 * import list. */
	if (node->parent)
		brasero_file_node_unlink (node);

	/* traverse the whole tree and free children updating tree stats */
	brasero_file_node_destroy_with_children (node, stats);
}

/**
 * Pre-remove function that unparent a node (before a possible destruction).
 * If node is imported, it saves it in its parent, destroys all child nodes
 * that are not imported and restore children that were imported.
 * NOTE: tree stats are only updated if the node is imported.
 */

static void
brasero_file_node_save_imported_children (BraseroFileNode *node,
					  BraseroFileTreeStats *stats,
					  GCompareFunc sort_func)
{
	BraseroFileNode *iter;
	BraseroImport *import;

	/* clean children */
	for (iter = BRASERO_FILE_NODE_CHILDREN (node); iter; iter = iter->next) {
		if (!iter->is_imported)
			brasero_file_node_destroy_with_children (iter, stats);

		if (!iter->is_file)
			brasero_file_node_save_imported_children (iter, stats, sort_func);
	}

	/* restore all replaced children */
	import = BRASERO_FILE_NODE_IMPORT (node);
	if (!import)
		return;

	for (iter = import->replaced; iter; iter = iter->next)
		brasero_file_node_insert (iter, node, sort_func, NULL);

	/* remove import */
	node->union1.name = import->name;
	node->has_import = FALSE;
	g_free (import);
}

void
brasero_file_node_save_imported (BraseroFileNode *node,
				 BraseroFileTreeStats *stats,
				 BraseroFileNode *parent,
				 GCompareFunc sort_func)
{
	BraseroImport *import;

	/* if it isn't imported return */
	if (!node->is_imported)
		return;

	/* Remove all the children that are not imported. Also restore
	 * all children that were replaced so as to restore the original
	 * order of files. */

	/* that shouldn't happen since root itself is considered imported */
	if (!parent || !parent->is_imported)
		return;

	/* save the node in its parent import structure */
	import = BRASERO_FILE_NODE_IMPORT (parent);
	if (!import) {
		import = g_new0 (BraseroImport, 1);
		import->name = BRASERO_FILE_NODE_NAME (parent);
		parent->union1.import = import;
		parent->has_import = TRUE;
	}

	/* unlink it and add it to the list */
	brasero_file_node_unlink (node);
	node->next = import->replaced;
	import->replaced = node;
	node->parent = parent;

	/* Explore children and remove not imported ones and restore.
	 * Update the tree stats at the same time.
	 * NOTE: here the tree stats are only used for the grafted children that
	 * are not imported in the tree. */
	brasero_file_node_save_imported_children (node, stats, sort_func);
}
