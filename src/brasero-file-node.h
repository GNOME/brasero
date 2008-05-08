/***************************************************************************
 *            brasero-file-node.h
 *
 *  Sat Dec  1 14:50:56 2007
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
 
#ifndef _BRASERO_FILE_NODE_H
#define _BRASERO_FILE_NODE_H

#include <glib.h>

#include <gio/gio.h>

#include "burn-volume.h"

G_BEGIN_DECLS

typedef struct _BraseroFileNode BraseroFileNode;

struct _BraseroURINode {
	/* List of all nodes that share the same URI */
	GSList *nodes;

	/* NOTE: uris are always escaped */
	gchar *uri;
};
typedef struct _BraseroURINode BraseroURINode;

struct _BraseroGraft {
	/* The name on CD (which could be different) from the original */
	gchar *name;
	BraseroURINode *node;
};
typedef struct _BraseroGraft BraseroGraft;

struct _BraseroImport {
	gchar *name;
	BraseroFileNode *replaced;
};
typedef struct _BraseroImport BraseroImport;

/**
 * NOTE: The root object keeps some statistics about its tree like
 * - number of children (files+directories)
 * - number of deep directories
 * - number of files over 2 Gio
 */

struct _BraseroFileTreeStats {
	guint children;
	guint num_deep;
	guint num_2Gio;
};
typedef struct _BraseroFileTreeStats BraseroFileTreeStats;

struct _BraseroFileNode {
	BraseroFileNode *parent;
	BraseroFileNode *next;

	/**
	 * This union can be:
	 * - the name of the file on file system and on disc
	 * - a pointer to the graft node for all grafted nodes
	 * - a pointer to saved imported session files
	 * Check is_grafted to determine.
	 */

	/* NOTE: names are unescaped (for display). */

	union {
		BraseroImport *import;
		BraseroGraft *graft;
		gchar *name;
	} union1;

	union {
		gchar *mime;
		BraseroFileNode *children;
	} union2;

	/* NOTE: overflow for sectors will probably not be hit
	 * before a few years given that we store the size in
	 * sectors of 2048. For the time being DVD are usually
	 * 4.3 Gio. To overflow the following member (provided
	 * we are on a 32 architecture) they would have to be
	 * over 8192 Gio. I think it's reasonable to think we
	 * have time. And even then, by this time most of the 
	 * computers will have switched to 64 architecture (in
	 * 2099) and I'll be dead anyway as well as optical
	 * discs. */
	union {
		guint sectors;
		BraseroFileTreeStats *stats;
	} union3;

	/* type of node */
	guint is_root:1;
	guint is_fake:1;
	guint is_file:1;
	guint is_symlink:1;
	guint is_imported:1;
	guint is_monitored:1;

	/* status of union1 */
	guint is_grafted:1;
	guint has_import:1;

	/* VFS status of node */
	guint is_loading:1;
	guint is_reloading:1;
	guint is_exploring:1;

	/* that's for some special nodes (usually counted in statistics) */
	guint is_2Gio:1;
	guint is_deep:1;

	/* This is for nodes created at project load time. This means
	 * that they can be replaced if a real directory is under the
	 * parent */
	guint is_tmp_parent:1;

	/* this should be set by BraseroDataDisc */
	guint is_expanded:1;
	guint is_selected:1;

	/* this is a ref count a max of 255 should be enough */
	guint is_visible:8;
};

/** Returns a const gchar* (it shouldn't be freed). */
#define BRASERO_FILE_NODE_NAME(MACRO_node)					\
	((MACRO_node)->is_grafted?(MACRO_node)->union1.graft->name:		\
	 (MACRO_node)->has_import?(MACRO_node)->union1.import->name:		\
	 (MACRO_node)->union1.name)

#define BRASERO_FILE_NODE_GRAFT(MACRO_node)					\
	((MACRO_node)->is_grafted?(MACRO_node)->union1.graft:NULL)

#define BRASERO_FILE_NODE_IMPORT(MACRO_node)					\
	((MACRO_node)->has_import?(MACRO_node)->union1.import:NULL)

#define BRASERO_FILE_NODE_CHILDREN(MACRO_node)					\
	((MACRO_node)->is_file?NULL:(MACRO_node)->union2.children)

#define BRASERO_FILE_NODE_MIME(MACRO_node)					\
	((MACRO_node)->is_file?(MACRO_node)->union2.mime:"x-directory/normal")

#define BRASERO_FILE_NODE_SECTORS(MACRO_node)					\
	((guint64) ((MACRO_node)->is_root?0:(MACRO_node)->union3.sectors))

#define BRASERO_FILE_NODE_STATS(MACRO_root)					\
	((MACRO_root)->is_root?(MACRO_root)->union3.stats:NULL)

#define BRASERO_FILE_2G_LIMIT		1048576

BraseroFileNode *
brasero_file_node_root_new (void);

BraseroFileNode *
brasero_file_node_get_root (BraseroFileNode *node,
			    guint *depth);

BraseroFileTreeStats *
brasero_file_node_get_tree_stats (BraseroFileNode *node,
				  guint *depth);

BraseroFileNode *
brasero_file_node_nth_child (BraseroFileNode *parent,
			     guint nth);

guint
brasero_file_node_get_depth (BraseroFileNode *node);

guint
brasero_file_node_get_pos_as_child (BraseroFileNode *node);

guint
brasero_file_node_get_n_children (const BraseroFileNode *node);

guint
brasero_file_node_get_pos_as_child (BraseroFileNode *node);

gboolean
brasero_file_node_is_ancestor (BraseroFileNode *parent,
			       BraseroFileNode *node);
BraseroFileNode *
brasero_file_node_check_name_existence (BraseroFileNode *parent,
				        const gchar *name);
BraseroFileNode *
brasero_file_node_check_imported_sibling (BraseroFileNode *node);

/**
 * Nodes are strictly organised so there to be sort function all the time
 */

void
brasero_file_node_add (BraseroFileNode *parent,
		       BraseroFileNode *child,
		       GCompareFunc sort_func);

BraseroFileNode *
brasero_file_node_new_loading (const gchar *name,
			       BraseroFileNode *parent,
			       GCompareFunc sort_func);
BraseroFileNode *
brasero_file_node_new_from_info (GFileInfo *info,
				 BraseroFileNode *parent,
				 GCompareFunc sort_func);
BraseroFileNode *
brasero_file_node_new_empty_folder (const gchar *name,
				    BraseroFileNode *parent,
				    GCompareFunc sort_func);
BraseroFileNode *
brasero_file_node_new_imported_session_file (BraseroVolFile *file,
					     BraseroFileNode *parent,
					     GCompareFunc sort_func);

/**
 * If there are any change in the order it cannot be handled in these functions
 * A call to resort function must be made.
 */
void
brasero_file_node_rename (BraseroFileNode *node,
			  const gchar *name);
void
brasero_file_node_set_from_info (BraseroFileNode *node,
				 GFileInfo *info);

void
brasero_file_node_graft (BraseroFileNode *file_node,
			 BraseroURINode *uri_node);
void
brasero_file_node_ungraft (BraseroFileNode *node);

void
brasero_file_node_move_from (BraseroFileNode *node,
			     BraseroFileTreeStats *stats);
void
brasero_file_node_move_to (BraseroFileNode *node,
			   BraseroFileNode *parent,
			   GCompareFunc sort_func);

void
brasero_file_node_unlink (BraseroFileNode *node);

void
brasero_file_node_destroy (BraseroFileNode *node,
			   BraseroFileTreeStats *stats);

void
brasero_file_node_save_imported (BraseroFileNode *node,
				 BraseroFileTreeStats *stats,
				 BraseroFileNode *parent,
				 GCompareFunc sort_func);

gchar *
brasero_file_node_get_uri_name (const gchar *uri);


gint
brasero_file_node_sort_name_cb (gconstpointer obj_a, gconstpointer obj_b);
gint
brasero_file_node_sort_size_cb (gconstpointer obj_a, gconstpointer obj_b);
gint 
brasero_file_node_sort_mime_cb (gconstpointer obj_a, gconstpointer obj_b);
gint
brasero_file_node_sort_default_cb (gconstpointer obj_a, gconstpointer obj_b);
gint *
brasero_file_node_sort_children (BraseroFileNode *parent,
				 GCompareFunc sort_func);
gint *
brasero_file_node_need_resort (BraseroFileNode *node,
			       GCompareFunc sort_func);
gint *
brasero_file_node_reverse_children (BraseroFileNode *parent);


G_END_DECLS

#endif /* _BRASERO_FILE_NODE_H */
