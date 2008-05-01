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

#ifndef _BRASERO_DATA_PROJECT_H_
#define _BRASERO_DATA_PROJECT_H_

#include <glib-object.h>
#include <gio/gio.h>

#include <gtk/gtk.h>

#include "brasero-file-node.h"
#include "burn-session.h"

#ifdef BUILD_INOTIFY
#include "brasero-file-monitor.h"
#endif

G_BEGIN_DECLS

#define BRASERO_TYPE_DATA_PROJECT             (brasero_data_project_get_type ())
#define BRASERO_DATA_PROJECT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_DATA_PROJECT, BraseroDataProject))
#define BRASERO_DATA_PROJECT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_DATA_PROJECT, BraseroDataProjectClass))
#define BRASERO_IS_DATA_PROJECT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_DATA_PROJECT))
#define BRASERO_IS_DATA_PROJECT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_DATA_PROJECT))
#define BRASERO_DATA_PROJECT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_DATA_PROJECT, BraseroDataProjectClass))

typedef struct _BraseroDataProjectClass BraseroDataProjectClass;
typedef struct _BraseroDataProject BraseroDataProject;

struct _BraseroDataProjectClass
{
#ifdef BUILD_INOTIFY
	BraseroFileMonitorClass parent_class;
#else
	GObjectClass parent_class;
#endif

	/* virtual functions */

	/**
	 * num_nodes is the number of nodes that were at the root of the 
	 * project.
	 */
	void		(*reset)		(BraseroDataProject *project,
						 guint num_nodes);

	/* NOTE: node_added is also called when there is a moved node;
	 * in this case a node_removed is first called and then the
	 * following function is called (mostly to match GtkTreeModel
	 * API). To detect such a case look at uri which will then be
	 * set to NULL.
	 * NULL uri can also happen when it's a created directory.
	 * if return value is FALSE, node was invalidated during call */
	gboolean	(*node_added)		(BraseroDataProject *project,
						 BraseroFileNode *node,
						 const gchar *uri);

	/* This is more an unparent signal. It shouldn't be assumed that the
	 * node was destroyed or not destroyed. Like the above function, it is
	 * also called when a node is moved. */
	void		(*node_removed)		(BraseroDataProject *project,
						 BraseroFileNode *former_parent,
						 guint former_position,
						 BraseroFileNode *node);

	void		(*node_changed)		(BraseroDataProject *project,
						 BraseroFileNode *node);
	void		(*node_reordered)	(BraseroDataProject *project,
						 BraseroFileNode *parent,
						 gint *new_order);

	void		(*uri_removed)		(BraseroDataProject *project,
						 const gchar *uri);
};

struct _BraseroDataProject
{
#ifdef BUILD_INOTIFY
	BraseroFileMonitor parent_instance;
#else
	GObject parent_instance;
#endif
};

GType brasero_data_project_get_type (void) G_GNUC_CONST;

void
brasero_data_project_reset (BraseroDataProject *project);

guint
brasero_data_project_get_size (BraseroDataProject *project);

guint
brasero_data_project_get_folder_size (BraseroDataProject *project,
				      BraseroFileNode *node);

gboolean
brasero_data_project_get_contents (BraseroDataProject *project,
				   GSList **grafts,
				   GSList **unreadable,
				   gboolean joliet_compat,
				   gboolean append_slash);

gboolean
brasero_data_project_is_empty (BraseroDataProject *project);

gboolean
brasero_data_project_is_video_project (BraseroDataProject *project);

gboolean
brasero_data_project_is_joliet_compliant (BraseroDataProject *project);

guint
brasero_data_project_load_contents (BraseroDataProject *project,
				    GSList *grafts,
				    GSList *excluded);

BraseroFileNode *
brasero_data_project_add_loading_node (BraseroDataProject *project,
				       const gchar *uri,
				       BraseroFileNode *parent);
BraseroFileNode *
brasero_data_project_add_node_from_info (BraseroDataProject *project,
					 const gchar *uri,
					 GFileInfo *info,
					 BraseroFileNode *parent);
BraseroFileNode *
brasero_data_project_add_empty_directory (BraseroDataProject *project,
					  const gchar *name,
					  BraseroFileNode *parent);
BraseroFileNode *
brasero_data_project_add_imported_session_file (BraseroDataProject *project,
						BraseroVolFile *file,
						BraseroFileNode *parent);

void
brasero_data_project_remove_node (BraseroDataProject *project,
				  BraseroFileNode *node);
void
brasero_data_project_destroy_node (BraseroDataProject *self,
				   BraseroFileNode *node);

void
brasero_data_project_node_loaded (BraseroDataProject *project,
				  BraseroFileNode *node,
				  const gchar *uri,
				  GFileInfo *info);
void
brasero_data_project_node_reloaded (BraseroDataProject *project,
				    BraseroFileNode *node,
				    const gchar *uri,
				    GFileInfo *info);
void
brasero_data_project_directory_node_loaded (BraseroDataProject *project,
					    BraseroFileNode *parent);

gboolean
brasero_data_project_rename_node (BraseroDataProject *project,
				  BraseroFileNode *node,
				  const gchar *name);

gboolean
brasero_data_project_move_node (BraseroDataProject *project,
				BraseroFileNode *node,
				BraseroFileNode *parent);

void
brasero_data_project_restore_uri (BraseroDataProject *project,
				  const gchar *uri);
void
brasero_data_project_exclude_uri (BraseroDataProject *project,
				  const gchar *uri);

guint
brasero_data_project_reference_new (BraseroDataProject *project,
				    BraseroFileNode *node);
void
brasero_data_project_reference_free (BraseroDataProject *project,
				     guint reference);
BraseroFileNode *
brasero_data_project_reference_get (BraseroDataProject *project,
				    guint reference);

BraseroFileNode *
brasero_data_project_get_root (BraseroDataProject *project);

gchar *
brasero_data_project_node_to_uri (BraseroDataProject *project,
				  BraseroFileNode *node);

void
brasero_data_project_set_sort_function (BraseroDataProject *project,
					GtkSortType sort_type,
					GCompareFunc sort_func);

G_END_DECLS

#endif /* _BRASERO_DATA_PROJECT_H_ */
