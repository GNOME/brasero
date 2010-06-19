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

#ifndef _BRASERO_DATA_VFS_H_
#define _BRASERO_DATA_VFS_H_

#include <glib-object.h>
#include <gtk/gtk.h>

#include "brasero-data-session.h"
#include "brasero-filtered-uri.h"

G_BEGIN_DECLS

#define BRASERO_SCHEMA_FILTER			"org.gnome.brasero.filter"
#define BRASERO_PROPS_FILTER_HIDDEN	        "hidden"
#define BRASERO_PROPS_FILTER_BROKEN	        "broken-sym"
#define BRASERO_PROPS_FILTER_REPLACE_SYMLINK    "replace-sym"

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
brasero_data_vfs_load_mime (BraseroDataVFS *vfs,
			    BraseroFileNode *node);

gboolean
brasero_data_vfs_require_node_load (BraseroDataVFS *vfs,
				    BraseroFileNode *node);

gboolean
brasero_data_vfs_require_directory_contents (BraseroDataVFS *vfs,
					     BraseroFileNode *node);

BraseroFilteredUri *
brasero_data_vfs_get_filtered_model (BraseroDataVFS *vfs);

G_END_DECLS

#endif /* _BRASERO_DATA_VFS_H_ */
