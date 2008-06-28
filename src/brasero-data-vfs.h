/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2007-2008 <bonfire-app@wanadoo.fr>
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

#ifndef _BRASERO_DATA_VFS_H_
#define _BRASERO_DATA_VFS_H_

#include <glib-object.h>

#include "brasero-data-session.h"

G_BEGIN_DECLS

#define BRASERO_FILTER_HIDDEN_KEY		"/apps/brasero/filter/hidden"
#define BRASERO_FILTER_BROKEN_SYM_KEY		"/apps/brasero/filter/broken_sym"

typedef enum {
	/* Following means it has been removed */
	BRASERO_FILTER_NONE			= 0,
	BRASERO_FILTER_HIDDEN			= 1,
	BRASERO_FILTER_UNREADABLE,
	BRASERO_FILTER_BROKEN_SYM,
	BRASERO_FILTER_RECURSIVE_SYM,
	BRASERO_FILTER_UNKNOWN
} BraseroFilterStatus;

#define BRASERO_TYPE_DATA_VFS             (brasero_data_vfs_get_type ())
#define BRASERO_DATA_VFS(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_DATA_VFS, BraseroDataVFS))
#define BRASERO_DATA_VFS_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_DATA_VFS, BraseroDataVFSClass))
#define BRASERO_IS_DATA_VFS(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_DATA_VFS))
#define BRASERO_IS_DATA_VFS_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_DATA_VFS))
#define BRASERO_DATA_VFS_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_DATA_VFS, BraseroDataVFSClass))

typedef struct _BraseroDataVFSClass BraseroDataVFSClass;
typedef struct _BraseroDataVFS BraseroDataVFS;

struct _BraseroDataVFSClass
{
	BraseroDataSessionClass parent_class;

	void	(*activity_changed)	(BraseroDataVFS *vfs,
					 gboolean active);
};

struct _BraseroDataVFS
{
	BraseroDataSession parent_instance;
};

GType brasero_data_vfs_get_type (void) G_GNUC_CONST;

gboolean
brasero_data_vfs_is_active (BraseroDataVFS *vfs);

gboolean
brasero_data_vfs_is_loading_uri (BraseroDataVFS *vfs);

gboolean
brasero_data_vfs_get_restored (BraseroDataVFS *vfs,
			       GSList **restored);
void
brasero_data_vfs_add_restored (BraseroDataVFS *vfs,
			       const gchar *restored);
void
brasero_data_vfs_remove_restored (BraseroDataVFS *vfs,
				  const gchar *restored);

gboolean
brasero_data_vfs_load_mime (BraseroDataVFS *vfs,
			    BraseroFileNode *node);

gboolean
brasero_data_vfs_require_node_load (BraseroDataVFS *vfs,
				    BraseroFileNode *node);

gboolean
brasero_data_vfs_require_directory_contents (BraseroDataVFS *vfs,
					     BraseroFileNode *node);

G_END_DECLS

#endif /* _BRASERO_DATA_VFS_H_ */
