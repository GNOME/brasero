/***************************************************************************
 *            brasero-vfs.h
 *
 *  jeu sep 21 16:06:48 2006
 *  Copyright  2006  Philippe Rouquier
 *  bonfire-app@wanadoo.fr
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

#ifndef BRASERO_VFS_H
#define BRASERO_VFS_H

#include <glib.h>
#include <glib-object.h>

#include <libgnomevfs/gnome-vfs.h>

#include "brasero-async-task-manager.h"
#include "metadata.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_VFS         (brasero_vfs_get_type ())
#define BRASERO_VFS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_VFS, BraseroVFS))
#define BRASERO_VFS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_VFS, BraseroVFSClass))
#define BRASERO_IS_VFS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_VFS))
#define BRASERO_IS_VFS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_VFS))
#define BRASERO_VFS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_VFS, BraseroVFSClass))

typedef struct _BraseroVFS BraseroVFS;
typedef struct _BraseroVFSPrivate BraseroVFSPrivate;
typedef struct _BraseroVFSClass BraseroVFSClass;

struct _BraseroVFS {
	BraseroAsyncTaskManager parent;
	BraseroVFSPrivate *priv;
};

struct _BraseroVFSClass {
	BraseroAsyncTaskManagerClass parent_class;
};

GType brasero_vfs_get_type ();
BraseroVFS *brasero_vfs_get_default ();

typedef guint BraseroVFSDataID;

typedef void		(*BraseroVFSInfoCallback)	(BraseroVFS *self,
							 GObject *owner,
							 GnomeVFSResult result,
							 const gchar *uri,
							 GnomeVFSFileInfo *info,
							 gpointer callback_data);

typedef void		(*BraseroVFSMetaCallback)	(BraseroVFS *self,
							 GObject *owner,
							 GnomeVFSResult result,
							 const gchar *uri,
							 GnomeVFSFileInfo *info,
							 BraseroMetadata *meta,
							 gpointer callback_data);

typedef void		(*BraseroVFSPlaylistCallback)	(BraseroVFS *self,
							 GObject *owner,
							 GnomeVFSResult result,
							 const gchar *uri,
							 const gchar *title,
							 gpointer callback_data);

typedef void		(*BraseroVFSCountCallback)	(BraseroVFS *self,
							 GObject *owner,
							 gint files_num,
							 gint invalid_num,
							 gint64 files_size,
							 gpointer callback_data);

typedef void		(*BraseroVFSDestroyCallback)	(GObject *object,
							 gpointer callback_data,
							 gboolean cancelled);

typedef gboolean	(*BraseroVFSCompareFunc)	(gpointer data,
							 gpointer user_data);

BraseroVFSDataID
brasero_vfs_register_data_type (BraseroVFS *self,
				GObject *owner,
				GCallback callback,
				BraseroVFSDestroyCallback destroy_func);

gboolean
brasero_vfs_get_count (BraseroVFS *self,
		       GList *uris,
		       gboolean audio,
		       BraseroVFSDataID type,
		       gpointer callback_data);

gboolean
brasero_vfs_get_info (BraseroVFS *self,
		      GList *uris,
		      gboolean check_parent_sym,
		      GnomeVFSFileInfoOptions flags,
		      BraseroVFSDataID type,
		      gpointer callback_data);

gboolean
brasero_vfs_get_metadata (BraseroVFS *self,
			  GList *uris,
			  GnomeVFSFileInfoOptions flags,
			  gboolean recursive,
			  BraseroVFSDataID id,
			  gpointer callback_data);

gboolean
brasero_vfs_parse_playlist (BraseroVFS *vfs,
			    const gchar *uri,
			    GnomeVFSFileInfoOptions flags,
			    BraseroVFSDataID id,
			    gpointer callback_data);

gboolean
brasero_vfs_load_directory (BraseroVFS *self,
			    const gchar *uri,
			    GnomeVFSFileInfoOptions flags,
			    BraseroVFSDataID type,
			    gpointer callback_data);

void
brasero_vfs_cancel (BraseroVFS *self,
		    gpointer object);

gboolean
brasero_vfs_find_urgent (BraseroVFS *self,
			 BraseroVFSDataID id,
			 BraseroVFSCompareFunc func,
			 gpointer user_data);

G_END_DECLS

#endif /* BRASERO_VFS_H */
