/***************************************************************************
 *            brasero-vfs.c
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

#include <string.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>

#include <libgnomevfs/gnome-vfs.h>

#ifdef BUILD_PLAYLIST
#include <totem-pl-parser.h>
#endif

#include "burn-debug.h"
#include "brasero-vfs.h"
#include "brasero-async-task-manager.h"
#include "brasero-metadata.h"

static void brasero_vfs_class_init (BraseroVFSClass *klass);
static void brasero_vfs_init (BraseroVFS *sp);
static void brasero_vfs_finalize (GObject *object);

typedef enum {
	BRASERO_TASK_TYPE_NONE,
	BRASERO_TASK_TYPE_INFO,
	BRASERO_TASK_TYPE_DIRECTORY,	
	BRASERO_TASK_TYPE_COUNT,
	BRASERO_TASK_TYPE_COUNT_SUBTASK_AUDIO,
	BRASERO_TASK_TYPE_COUNT_SUBTASK_DATA,
	BRASERO_TASK_TYPE_COUNT_SUBTASK_PLAYLIST,
	BRASERO_TASK_TYPE_METADATA,
	BRASERO_TASK_TYPE_METADATA_SUBTASK,
	BRASERO_TASK_TYPE_PLAYLIST,
	BRASERO_TASK_TYPE_PLAYLIST_SUBTASK,
	BRASERO_TASK_TYPE_NUM
} BraseroJobType;

struct _BraseroVFSDataType {
	GObject *owner;
	GCallback callback;
	BraseroVFSDestroyCallback destroy;
};
typedef struct _BraseroVFSDataType BraseroVFSDataType;

struct _BraseroVFSTaskCtx {
	BraseroVFSDataType *user_method;
	gpointer user_data;

	BraseroVFSDataType *task_method;
	gpointer task_data;

	gboolean cancelled;
};
typedef struct _BraseroVFSTaskCtx BraseroVFSTaskCtx;

struct _BraseroVFSMetadataTask {
	gchar *uri;
	GSList *ctx;
	BraseroMetadata *metadata;
};
typedef struct _BraseroVFSMetadataTask BraseroVFSMetadataTask;

struct _BraseroInfoData {
	GSList *results;
	GSList *uris;
	gint flags;

	guint check_parent_sym:1;
};
typedef struct _BraseroInfoData BraseroInfoData;

struct _BraseroLoadData {
	gint flags;
	gchar *root;
	GSList *results;
	GnomeVFSResult result;
};
typedef struct _BraseroLoadData BraseroLoadData;

struct _BraseroMetadataData {
	gint refcount;
	gboolean recursive;
	BraseroMetadataFlag flags;
	GSList *results;
};
typedef struct _BraseroMetadataData BraseroMetadataData;

struct _BraseroCountData {
	gint refcount;
	gint files_num;
	gint invalid_num;
	gint64 files_size;
};
typedef struct _BraseroCountData BraseroCountData;

#ifdef BUILD_PLAYLIST

struct _BraseroVFSPlaylistData {
	gchar *uri;
	gchar *title;

	GList *uris;
	TotemPlParser *parser;
	TotemPlParserResult res;
	GnomeVFSFileInfoOptions flags;
};
typedef struct _BraseroVFSPlaylistData BraseroVFSPlaylistData;

#endif

struct _BraseroInfoResult  {
	GnomeVFSFileInfo *info;
	GnomeVFSResult result;
	gchar *uri;
};
typedef struct _BraseroInfoResult BraseroInfoResult;

struct _BraseroMetadataResult {
	GnomeVFSFileInfo *info;
	BraseroVFSMetadataTask *item;
};
typedef struct _BraseroMetadataResult BraseroMetadataResult;


#define BRASERO_CTX_OWNER(ctx)			((ctx)->user_method->owner)
#define BRASERO_CTX_USER_DATA(ctx)		((ctx)->user_data)

#define BRASERO_CTX_SELF(ctx)			(BRASERO_VFS ((ctx)->task_method->owner))
#define BRASERO_CTX_SELF_OBJ(ctx)		(G_OBJECT ((ctx)->task_method->owner))
#define BRASERO_CTX_TASK_DATA(ctx)		((ctx)->task_data)

#define BRASERO_CTX_CALLBACK(ctx)		((ctx)->user_method->callback)
#define BRASERO_CTX_INFO_CALLBACK(ctx)		((BraseroVFSInfoCallback) BRASERO_CTX_CALLBACK (ctx))
#define BRASERO_CTX_COUNT_CALLBACK(ctx)		((BraseroVFSCountCallback) BRASERO_CTX_CALLBACK (ctx))
#define BRASERO_CTX_META_CALLBACK(ctx)		((BraseroVFSMetaCallback) BRASERO_CTX_CALLBACK (ctx))
#define BRASERO_CTX_PLAYLIST_CALLBACK(ctx)	((BraseroVFSPlaylistCallback) BRASERO_CTX_CALLBACK (ctx))

#define BRASERO_CTX_INFO_CB(ctx, result, uri, info)				\
	if (BRASERO_CTX_CALLBACK (ctx))						\
		(BRASERO_CTX_INFO_CALLBACK (ctx)) (BRASERO_CTX_SELF (ctx),	\
						   BRASERO_CTX_OWNER (ctx),	\
						   result,			\
						   uri,				\
						   info,			\
						   BRASERO_CTX_USER_DATA (ctx))

#define BRASERO_CTX_LOAD_CB(ctx, result, uri, info)				\
	if (BRASERO_CTX_CALLBACK (ctx))						\
		(BRASERO_CTX_INFO_CALLBACK (ctx)) (BRASERO_CTX_SELF (ctx),	\
						   BRASERO_CTX_OWNER (ctx),	\
						   result,			\
						   uri,				\
						   info,			\
						   BRASERO_CTX_USER_DATA (ctx))

#define BRASERO_CTX_COUNT_CB(ctx, files_num, invalid_num, size)			\
	if (BRASERO_CTX_CALLBACK (ctx))						\
		(BRASERO_CTX_COUNT_CALLBACK (ctx)) (BRASERO_CTX_SELF (ctx),	\
						    BRASERO_CTX_OWNER (ctx),	\
						    files_num,			\
						    invalid_num,		\
						    size,			\
						    BRASERO_CTX_USER_DATA (ctx))

#define BRASERO_CTX_META_CB(ctx, result, uri, info, metadata)			\
	if (BRASERO_CTX_CALLBACK (ctx))						\
		(BRASERO_CTX_META_CALLBACK (ctx)) (BRASERO_CTX_SELF (ctx),	\
						   BRASERO_CTX_OWNER (ctx),	\
						   result,			\
						   uri,				\
						   info,			\
						   metadata,			\
						   BRASERO_CTX_USER_DATA (ctx))


#define BRASERO_CTX_SET_TASK_METHOD(self, ctx, id) 				\
	((ctx)->task_method = g_hash_table_lookup (self->priv->types, GINT_TO_POINTER (id)))

#define BRASERO_CTX_SET_METATASK(ctx)						\
	(BRASERO_CTX_SELF (ctx)->priv->metatasks = g_slist_prepend (BRASERO_CTX_SELF (ctx)->priv->metatasks, ctx))

struct _BraseroVFSPrivate {
	GSList *metatasks;

	GHashTable *types;
	gint type_num;

	BraseroAsyncTaskTypeID info_type;
	BraseroAsyncTaskTypeID load_type;

#ifdef BUILD_PLAYLIST

	BraseroAsyncTaskTypeID playlist_type;

#endif

	/* used for metadata */
	GSList *metadatas;
	GSList *processing_meta;
	GSList *unprocessed_meta;
	gint meta_process_id;

	/* used to "buffer" some results returned by metadata.
	 * It takes time to return metadata and it's not unusual
	 * to fetch metadata three times in a row, once for size
	 * preview, once for preview, once adding to selection */
	 GQueue *meta_buffer;
};

/* so far one metadata at a time has shown to be the best for performance */
#define MAX_CONCURENT_META 	2
#define MAX_BUFFERED_META	20

static GObjectClass *parent_class = NULL;
static BraseroVFS *singleton = NULL;

GType
brasero_vfs_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroVFSClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_vfs_class_init,
			NULL,
			NULL,
			sizeof (BraseroVFS),
			0,
			(GInstanceInitFunc)brasero_vfs_init,
		};

		type = g_type_register_static (BRASERO_TYPE_ASYNC_TASK_MANAGER, 
					       "BraseroVFS",
					       &our_info,
					       0);
	}

	return type;
}

static void
brasero_vfs_class_init (BraseroVFSClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_vfs_finalize;
}

static void
brasero_vfs_last_reference_cb (gpointer null_data,
			       GObject *object,
			       gboolean is_last_ref)
{
	if (is_last_ref) {
		singleton = NULL;
		g_object_remove_toggle_ref (object,
					    brasero_vfs_last_reference_cb,
					    null_data);
	}
}

BraseroVFS *
brasero_vfs_get_default ()
{
	if (singleton) {
		g_object_ref (singleton);
		return singleton;
	}

	singleton = BRASERO_VFS (g_object_new (BRASERO_TYPE_VFS, NULL));
	g_object_add_toggle_ref (G_OBJECT (singleton),
				 brasero_vfs_last_reference_cb,
				 NULL);
	return singleton;
}

/**
 * This part deals with symlinks, that allows to get unique filenames by
 * replacing any parent symlink by its target and check for recursive
 * symlinks
 */

static gchar *
brasero_vfs_check_for_parent_symlink (const gchar *escaped_uri)
{
	GnomeVFSFileInfo *info;
	GnomeVFSURI *parent;
    	gchar *uri;

    	parent = gnome_vfs_uri_new (escaped_uri);
  	info = gnome_vfs_file_info_new ();
    	uri = gnome_vfs_uri_to_string (parent, GNOME_VFS_URI_HIDE_NONE);

	while (gnome_vfs_uri_has_parent (parent)) {
	    	GnomeVFSURI *tmp;
		GnomeVFSResult result;

		result = gnome_vfs_get_file_info_uri (parent,
						      info,
					              GNOME_VFS_FILE_INFO_FOLLOW_LINKS);

		if (result != GNOME_VFS_OK)
			/* we shouldn't reached this point but who knows */
		    	break;

		/* NOTE: no need to check for broken symlinks since
		 * we wouldn't have reached this point otherwise */
		if (GNOME_VFS_FILE_INFO_SYMLINK (info)) {
		    	gchar *parent_uri;
		    	gchar *new_root;
			gchar *newuri;

		    	parent_uri = gnome_vfs_uri_to_string (parent, GNOME_VFS_URI_HIDE_NONE);
			new_root = gnome_vfs_make_uri_from_input (info->symlink_name);

			newuri = g_strconcat (new_root,
					      uri + strlen (parent_uri),
					      NULL);

		    	g_free (uri);
		    	uri = newuri;	

		    	gnome_vfs_uri_unref (parent);
		    	g_free (parent_uri);

		    	parent = gnome_vfs_uri_new (new_root);
			g_free (new_root);
		}

		tmp = parent;
		parent = gnome_vfs_uri_get_parent (parent);
		gnome_vfs_uri_unref (tmp);

		gnome_vfs_file_info_clear (info);
	}
	gnome_vfs_file_info_unref (info);
	gnome_vfs_uri_unref (parent);

	return uri;
}

static gboolean
brasero_utils_get_symlink_target (const gchar *escaped_uri,
				  GnomeVFSFileInfo *info,
				  GnomeVFSFileInfoOptions flags)
{
	gint size;
	GnomeVFSResult result;

	result = gnome_vfs_get_file_info (escaped_uri,
					  info,
					  flags|
					  GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
	if (result)
		return FALSE;

    	if (info->symlink_name) {
		gchar *target;

		target = gnome_vfs_make_uri_from_input (info->symlink_name);

		g_free (info->symlink_name);
		info->symlink_name = brasero_vfs_check_for_parent_symlink (target);
		g_free (target);
	}

    	if (!info->symlink_name)
		return FALSE;

	/* we check for circular dependency here :
	 * if the target is one of the parent of symlink */
	size = strlen (info->symlink_name);
	if (!strncmp (info->symlink_name, escaped_uri, size)
	&& (*(escaped_uri + size) == '/' || *(escaped_uri + size) == '\0'))
		return FALSE;
	
	return TRUE;
}

static void
brasero_vfs_task_ctx_free (BraseroVFSTaskCtx *ctx, gboolean cancelled)
{
	BRASERO_CTX_SELF (ctx)->priv->metatasks = g_slist_remove (BRASERO_CTX_SELF (ctx)->priv->metatasks, ctx);

	if (ctx->task_method->destroy)
		(ctx->task_method->destroy) (BRASERO_CTX_SELF_OBJ (ctx),
					     ctx,
					     cancelled);

	if (ctx->user_method->destroy)
		(ctx->user_method->destroy) (BRASERO_CTX_OWNER (ctx),
					     BRASERO_CTX_USER_DATA (ctx),
					     cancelled);
	g_free (ctx);
}

static BraseroVFSTaskCtx *
brasero_vfs_ctx_new (BraseroVFS *self,
		     BraseroVFSDataID id,
		     gpointer callback_data)
{
	BraseroVFSTaskCtx *ctx;
	BraseroVFSDataType *method;

	g_return_val_if_fail (self != NULL, NULL);
	g_return_val_if_fail (id > 0, NULL);

	method = g_hash_table_lookup (self->priv->types, GINT_TO_POINTER (id));
	if (!method)
		return NULL;

	ctx = g_new0 (BraseroVFSTaskCtx, 1);
	ctx->user_method = method;
	ctx->user_data = callback_data;

	return ctx;
}

/**
 * used to get info about files
 */

static void
brasero_info_result_free (BraseroInfoResult *result)
{
	gnome_vfs_file_info_unref (result->info);
	g_free (result->uri);
	g_free (result);
}

static void
brasero_vfs_info_destroy (GObject *obj, gpointer user_data, gboolean cancelled)
{
	BraseroInfoData *data;
	BraseroVFSTaskCtx *ctx = user_data;

	data = BRASERO_CTX_TASK_DATA (ctx);

	if (data->results) {
		g_slist_foreach (data->results, (GFunc) brasero_info_result_free, NULL);
		g_slist_free (data->results);
	}

	if (data->uris) {
		g_slist_foreach (data->uris, (GFunc) g_free, NULL);
		g_slist_free (data->uris);
	}

	g_free (data);
}

static void
brasero_vfs_info_result (BraseroAsyncTaskManager *manager,
			 gpointer callback_data)
{
	BraseroVFSTaskCtx *ctx = callback_data;
	BraseroInfoData *data;
	GSList *iter;

	if (ctx->cancelled) {
		brasero_vfs_task_ctx_free (ctx, TRUE);
		return;
	}

	data = BRASERO_CTX_TASK_DATA (ctx);
    	for (iter = data->results; iter; iter = iter->next) {
		BraseroInfoResult *result;

		result = iter->data;
		BRASERO_CTX_INFO_CB (ctx,
				     result->result,
				     result->uri,
				     result->info);
	}

	brasero_vfs_task_ctx_free (ctx, FALSE);
}

static void
brasero_vfs_info_thread (BraseroAsyncTaskManager *manager,
			 gpointer callback_data)
{
	BraseroVFSTaskCtx *ctx = callback_data;
	BraseroInfoData *data;
	GSList *iter, *next;

	data = BRASERO_CTX_TASK_DATA (ctx);
	for (iter = data->uris; iter; iter = next) {
		BraseroInfoResult *result;
		GnomeVFSFileInfo *info;
		GnomeVFSURI *vfsuri;
		gchar *uri;

		if (ctx->cancelled)
			break;

		next = iter->next;
		uri = iter->data;
		data->uris = g_slist_remove (data->uris, uri);

		result = g_new0 (BraseroInfoResult, 1);
		data->results = g_slist_prepend (data->results, result);

		if (data->check_parent_sym) {
			gchar *tmp;

			/* If we want to make sure a directory is not added twice we have to make sure
			 * that it doesn't have a symlink as parent otherwise "/home/Foo/Bar" with Foo
			 * as a symlink pointing to /tmp would be seen as a different file from /tmp/Bar 
			 * It would be much better if we could use the inode numbers provided by gnome_vfs
			 * unfortunately they are guint64 and can't be used in hash tables as keys.
			 * Therefore we check parents up to root to see if there are symlinks and if so
			 * we get a path without symlinks in it. This is done only for local file */
			tmp = uri;
			uri = brasero_vfs_check_for_parent_symlink (uri);
			g_free (tmp);
		}

		result->uri = uri;
		info = gnome_vfs_file_info_new ();
		vfsuri = gnome_vfs_uri_new (uri);
		result->result = gnome_vfs_get_file_info_uri (vfsuri,
							      info,
							      data->flags);
		gnome_vfs_uri_unref (vfsuri);

		if (result->result != GNOME_VFS_OK) {
			result->info = info;
			continue;
		}

		if (info->type == GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK) {
			gnome_vfs_file_info_clear (info);
			if (!brasero_utils_get_symlink_target (uri,
							       info,
							       data->flags)) {
				/* since we checked for the existence of the file
				 * an error means a looping symbolic link */
				if (info->symlink_name)
					result->result = GNOME_VFS_ERROR_LOOP;
			}
		}
		result->info = info;
	}

	data->uris = NULL;
}

gboolean
brasero_vfs_get_info (BraseroVFS *self,
		      GList *uris,
		      gboolean check_parent_sym,
		      GnomeVFSFileInfoOptions flags,
		      BraseroVFSDataID id,
		      gpointer callback_data)
{
	GList *iter;
	BraseroInfoData *data;
	BraseroVFSTaskCtx *ctx;

	ctx = brasero_vfs_ctx_new (self, id, callback_data);
	if (!ctx)
		return FALSE;

	data = g_new0 (BraseroInfoData, 1);
	data->flags = flags;
	data->check_parent_sym = check_parent_sym;

	for (iter = uris; iter; iter = iter->next) {
		gchar *uri;

		uri = iter->data;
		data->uris = g_slist_prepend (data->uris, g_strdup (uri));
	}

	BRASERO_CTX_TASK_DATA (ctx) = data;
	BRASERO_CTX_SET_TASK_METHOD (self, ctx, BRASERO_TASK_TYPE_INFO);

	return brasero_async_task_manager_queue (BRASERO_ASYNC_TASK_MANAGER (self),
						 self->priv->info_type,
						 ctx);
}

/**
 * used to explore directories
 */

static void
brasero_vfs_load_directory_destroy (GObject *obj,
				    gpointer user_data,
				    gboolean cancelled)
{
	BraseroLoadData *data;
	BraseroVFSTaskCtx *ctx = user_data;

	data = BRASERO_CTX_TASK_DATA (ctx);

	if (data->results) {
		g_slist_foreach (data->results, (GFunc) brasero_info_result_free, NULL);
		g_slist_free (data->results);
	}

	g_free (data->root);
	g_free (data);
}

static void
brasero_vfs_load_result (BraseroAsyncTaskManager *manager,
			 gpointer callback_data)
{
	BraseroVFSTaskCtx *ctx = callback_data;
	BraseroLoadData *data;
	GSList *iter;

	data = BRASERO_CTX_TASK_DATA (ctx);

	if (ctx->cancelled) {
		brasero_vfs_task_ctx_free (ctx, TRUE);
		return;	
	}

    	if (data->result != GNOME_VFS_OK) {
		/* directory opening failed */
		BRASERO_CTX_LOAD_CB (ctx,
				     data->result,
				     data->root,
				     NULL);

		brasero_vfs_task_ctx_free (ctx, FALSE);
		return;
	}

	for (iter = data->results; iter; iter = iter->next) {
		BraseroInfoResult *result;

		result = iter->data;
		BRASERO_CTX_LOAD_CB (ctx,
				     result->result,
				     result->uri,
				     result->info);
	}
	
	brasero_vfs_task_ctx_free (ctx, FALSE);
}

static void
brasero_vfs_load_thread (BraseroAsyncTaskManager *manager,
			 gpointer callback_data)
{
	BraseroVFSTaskCtx *ctx = callback_data;
	GnomeVFSDirectoryHandle *handle;
	GnomeVFSFileInfo *info;
	BraseroLoadData *data;
	GnomeVFSResult res;

	data = BRASERO_CTX_TASK_DATA (ctx);
	res = gnome_vfs_directory_open (&handle,
					data->root,
					data->flags);
	data->result = res;
	if (res != GNOME_VFS_OK)
		return;

	info = gnome_vfs_file_info_new ();
	while (gnome_vfs_directory_read_next (handle, info) == GNOME_VFS_OK) {
		BraseroInfoResult *result;
		gchar *name;
		gchar *uri;

		if (ctx->cancelled)
			break;

		if (info->name [0] == '.'
		&& (info->name [1] == '\0'
		|| (info->name [1] == '.' && info->name [2] == '\0'))) {
			gnome_vfs_file_info_clear (info);
			continue;
		}

		result = g_new0 (BraseroInfoResult, 1);
		data->results = g_slist_prepend (data->results, result);

		name = gnome_vfs_escape_string (info->name);
		uri = g_build_filename (data->root, name, NULL);
		g_free (name);

		/* special case for symlinks */
		result->result = GNOME_VFS_OK;
		if (info->type == GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK) {
			gnome_vfs_file_info_clear (info);
			if (!brasero_utils_get_symlink_target (uri,
							       info,
							       data->flags)) {
				/* since we checked for the existence of the file
				 * an error means a looping symbolic link */
				if (info->symlink_name)
					result->result = GNOME_VFS_ERROR_LOOP;
			}
		}

		result->uri = uri;
		result->info = info;

		info = gnome_vfs_file_info_new ();
	}

	gnome_vfs_file_info_unref (info);
	gnome_vfs_directory_close (handle);
}

gboolean
brasero_vfs_load_directory (BraseroVFS *self,
			    const gchar *uri,
			    GnomeVFSFileInfoOptions flags,
			    BraseroVFSDataID id,
			    gpointer callback_data)
{
	BraseroLoadData *data;
	BraseroVFSTaskCtx *ctx;

	if (!uri)
		return FALSE;

	ctx = brasero_vfs_ctx_new (self, id, callback_data);
	if (!ctx)
		return FALSE;

	data = g_new0 (BraseroLoadData, 1);
	data->root = g_strdup (uri);
	data->flags = flags;

	BRASERO_CTX_TASK_DATA (ctx) = data;
	BRASERO_CTX_SET_TASK_METHOD (self, ctx, BRASERO_TASK_TYPE_DIRECTORY);

	return brasero_async_task_manager_queue (BRASERO_ASYNC_TASK_MANAGER (self),
						 self->priv->load_type,
						 ctx);
}

/**
 * used to get the metadata for a multimedia file
 */

static gboolean
brasero_vfs_process_metadata (gpointer callback_data)
{
	BraseroMetadataFlag flags = BRASERO_METADATA_FLAG_NONE;
	BraseroVFS *self = BRASERO_VFS (callback_data);
	BraseroVFSMetadataTask *item;
	GSList *iter;

	if (!self->priv->unprocessed_meta) {
		self->priv->meta_process_id = 0;
		return FALSE;
	}

	if (!self->priv->metadatas) {
		/* Once one metadata object is free, this function will be
		 * called again */
		self->priv->meta_process_id = 0;
		return FALSE;
	}

	item = self->priv->unprocessed_meta->data;
	item->metadata = self->priv->metadatas->data;
	self->priv->metadatas = g_slist_remove (self->priv->metadatas,
						item->metadata);

	BRASERO_BURN_LOG ("starting analysis of %s", item->uri);

	self->priv->unprocessed_meta = g_slist_remove (self->priv->unprocessed_meta, item);
	self->priv->processing_meta = g_slist_prepend (self->priv->processing_meta, item);

	/* merge the flags of all contexts. NOTE: since the search for silences
	 * implies and forces a thorough/long search, no need to care about fast
	 * flag */
	for (iter = item->ctx; iter; iter = iter->next) {
		BraseroVFSTaskCtx *ctx;
		BraseroMetadataData *data;

		ctx = iter->data;
		data = BRASERO_CTX_TASK_DATA (ctx);
		flags |= data->flags;
	}

	brasero_metadata_get_info_async (item->metadata,
					 item->uri,
					 flags);
	return TRUE;
}

static void
brasero_vfs_metadata_task_free (BraseroVFS *self,
				BraseroVFSMetadataTask *item)
{
	if (!item)
		return;

	if (item->ctx)
		return;

	/* there are no more context waiting, remove from the queue */
	self->priv->unprocessed_meta = g_slist_remove (self->priv->unprocessed_meta, item);
	self->priv->processing_meta = g_slist_remove (self->priv->processing_meta, item);
	g_free (item->uri);

	/* if there is a metadata (if it is processing) then:
	 * cancel operation, put metadata in metadatas queue
	 * and schedule a brasero_vfs_process_metadata */
	if (item->metadata) {
		brasero_metadata_cancel (item->metadata);
		self->priv->metadatas = g_slist_prepend (self->priv->metadatas, item->metadata);

		if (!self->priv->meta_process_id && !self->priv->processing_meta)
			self->priv->meta_process_id = g_idle_add (brasero_vfs_process_metadata, self);
	}
	g_free (item);
}

static void
brasero_vfs_metadata_result_free (BraseroVFS *self,
				  BraseroVFSTaskCtx *ctx,
				  BraseroMetadataResult *result,
				  gboolean cancelled)
{
	if (result->info) {
		gnome_vfs_file_info_unref (result->info);
		result->info = NULL;
	}

	if (result->item) {
		result->item->ctx = g_slist_remove (result->item->ctx, ctx);
		brasero_vfs_metadata_task_free (self, result->item);
	}

	g_free (result);
}

static gint
brasero_vfs_metadata_lookup_buffer (gconstpointer a, gconstpointer b)
{
	const BraseroMetadataInfo *metadata = a;
	const gchar *uri = b;

	return strcmp (uri, metadata->uri);
}

static void
brasero_vfs_metadata_ctx_completed (BraseroVFSTaskCtx *ctx,
				    BraseroMetadataInfo *info)
{
	GSList *iter;
	BraseroMetadataData *data;
	BraseroMetadataResult *result;

	data = BRASERO_CTX_TASK_DATA (ctx);

	result = NULL;
	for (iter = data->results; iter; iter = iter->next) {
		result = iter->data;

		if (result->item
		&& !strcmp (result->item->uri, info->uri))
			break;

		result = NULL;
	}

	if (!result) {
		BRASERO_BURN_LOG ("no corresponding BraseroMetadataResult found");
		brasero_vfs_task_ctx_free (ctx, FALSE);
		return;
	}

	data->results = g_slist_remove (data->results, result);
	data->refcount --;

	/* NOTE: the error is useless most of the time (except for debug).
	 * The best way to determine if the file is supported by gstreamer
	 * is to look at has_audio/video */
	BRASERO_CTX_META_CB (ctx,
			     GNOME_VFS_OK, /* the result is OK since we searched for metadata */
			     info->uri,
			     result->info,
			     info);

	brasero_vfs_metadata_result_free (BRASERO_CTX_SELF (ctx),
					  ctx,
					  result,
					  FALSE);

	if (!data->results || !data->refcount)
		brasero_vfs_task_ctx_free (ctx, FALSE);
}

static void
brasero_vfs_metadata_completed_cb (BraseroMetadata *metadata,
				   GError *error,
				   gpointer user_data)
{
	BraseroVFS *self = BRASERO_VFS (user_data);
	BraseroVFSMetadataTask *item = NULL;
	BraseroMetadataInfo *info;
	GSList *iter;

	info = g_new0 (BraseroMetadataInfo, 1);
	brasero_metadata_set_info (metadata, info);

	BRASERO_BURN_LOG ("analysis finished for %s", info->uri);

	/* removed the currently processing metadata from the queue and possibly
	 * add it to the buffer in case it is required again */
	for (iter = self->priv->processing_meta; iter; iter = iter->next) {
		item = iter->data;
		if (!strcmp (item->uri, info->uri))
			break;

		item = NULL;
	}

	if (item) {
		GSList *next;

		self->priv->processing_meta = g_slist_remove (self->priv->processing_meta, item);

		/* process results */
		for (iter = item->ctx; iter; iter = next) {
			BraseroVFSTaskCtx *ctx;

			ctx = iter->data;
			next = iter->next;
			brasero_vfs_metadata_ctx_completed (ctx, info);
	}

		/* NOTE: no need to free item here it is freed once all contexts
		 * have been removed from its queue by the above function */
	}
	else
		BRASERO_BURN_LOG ("couldn't find waiting item in queue (cancelled?)");

	/* remove it and check for the next file to process */
	if (!self->priv->meta_process_id && !self->priv->processing_meta)
		self->priv->meta_process_id = g_idle_add (brasero_vfs_process_metadata, self);

	/* see if info is already in cache buffer */
	if (g_queue_find_custom (self->priv->meta_buffer, info->uri, brasero_vfs_metadata_lookup_buffer))
		return;

	if (!info->has_audio && !info->has_video)
		return;

	g_queue_push_head (self->priv->meta_buffer, info);

	if (g_queue_get_length (self->priv->meta_buffer) > MAX_BUFFERED_META) {
		info = g_queue_pop_tail (self->priv->meta_buffer);
		brasero_metadata_info_free (info);
	}
}

static gboolean
brasero_vfs_metadata_ctx_new (BraseroVFS *self,
			      BraseroVFSTaskCtx *ctx,
			      const gchar *uri,
			      GnomeVFSFileInfo *info)
{
	BraseroVFSMetadataTask *item = NULL;
	BraseroMetadataResult *result;
	BraseroMetadataData *data;
	GSList *iter;

	/* make sure the URI is going to be processed. see if self isn't already
	 * processing the same URI or if the URI is not in the unprocessed queue
	 */
	for (iter = self->priv->unprocessed_meta; iter; iter = iter->next) {
		item = iter->data;
		if (!strcmp (item->uri, uri))
			break;

		item = NULL;
	}

	/* see if it is currently being processed */
	for (iter = self->priv->processing_meta; iter; iter = iter->next) {
		item = iter->data;
		if (!strcmp (item->uri, uri))
			break;

		item = NULL;
	}
	
	if (!item) {
		/* there was nothing to process. Add it */
		item = g_new0 (BraseroVFSMetadataTask, 1);
		item->uri = g_strdup (uri);

		if (!self->priv->meta_process_id && !self->priv->processing_meta)
			self->priv->meta_process_id = g_idle_add (brasero_vfs_process_metadata, self);

		self->priv->unprocessed_meta = g_slist_append (self->priv->unprocessed_meta, item);
	}

	/* add a reference to the parent */
	data = BRASERO_CTX_TASK_DATA (ctx);
	data->refcount ++;

	result = g_new0 (BraseroMetadataResult, 1);
	data->results = g_slist_prepend (data->results, result);

	/* keep info for later: add a reference */
	gnome_vfs_file_info_ref (info);
	result->info = info;
	result->item = item;

	item->ctx = g_slist_prepend (item->ctx, ctx);
	return TRUE;
}

static void
brasero_vfs_metadata_destroy (GObject *object,
			      gpointer user_data,
			      gboolean cancelled)
{
	BraseroVFS *self = BRASERO_VFS (object);
	BraseroVFSTaskCtx *ctx = user_data;
	BraseroMetadataData *data;
	GSList *iter;

	data = BRASERO_CTX_TASK_DATA (ctx);

	/* do it only if the destruction results from cancelling */
	if (cancelled) {
		data->refcount ++;
		brasero_vfs_cancel (self, ctx);
	}

	for (iter = data->results; iter; iter = iter->next) {
		BraseroMetadataResult *result;

		result = iter->data;
		brasero_vfs_metadata_result_free (self,
						  ctx,
						  result,
						  cancelled);
	}

	g_slist_free (data->results);
	data->results = NULL;

	g_free (data);
}

static void
brasero_vfs_metadata_subtask_destroy (GObject *object,
				      gpointer user_data,
				      gboolean cancelled)
{
	BraseroMetadataData *data;
	BraseroVFSTaskCtx *ctx = user_data;

	data = BRASERO_CTX_TASK_DATA (ctx);

	data->refcount --;
	if (data->refcount <= 0)
		brasero_vfs_task_ctx_free (ctx, FALSE);
}

static void
brasero_vfs_metadata_result (BraseroVFS *self,
			     GObject *owner,
			     GnomeVFSResult result,
			     const gchar *uri,
			     GnomeVFSFileInfo *info,
			     gpointer user_data)
{
	BraseroVFSTaskCtx *ctx = user_data;
	BraseroMetadataData *data;

	data = BRASERO_CTX_TASK_DATA (ctx);

	if (result != GNOME_VFS_OK) {
		BRASERO_CTX_META_CB (ctx,
				     result,
				     uri,
				     NULL,
				     NULL);
		return;
	}

	if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
		if (data->recursive) {
			data->refcount ++;
			brasero_vfs_load_directory (BRASERO_CTX_SELF (ctx),
						    uri,
						    GNOME_VFS_FILE_INFO_FOLLOW_LINKS,
						    BRASERO_TASK_TYPE_METADATA_SUBTASK,
						    ctx);
		}
		else {
			BRASERO_CTX_META_CB (ctx,
					     result,
					     uri,
					     info,
					     NULL);
		}
	}
	else if (info->type == GNOME_VFS_FILE_TYPE_REGULAR) {
		GList *node;

		/* avoid launching metadata for some well known common mime types */
		if (info->mime_type
		&& (!strncmp (info->mime_type, "image/", 6)
		||  !strcmp (info->mime_type, "text/plain")
		||  !strcmp (info->mime_type, "application/x-cue") /* this one make gstreamer crash */
		||  !strcmp (info->mime_type, "application/x-cd-image")
		||  !strcmp (info->mime_type, "application/octet-stream"))) {
			BRASERO_CTX_META_CB (ctx,
					     GNOME_VFS_OK,
					     uri,
					     info,
					     NULL);
			return;
		}			

		/* seek in the buffer if we have already explored these metadata */
		node = g_queue_find_custom (self->priv->meta_buffer,
					    uri,
					    brasero_vfs_metadata_lookup_buffer);

		if (node) {
			BraseroMetadataInfo *metadata;

			metadata = node->data;
			BRASERO_CTX_META_CB (ctx,
					     GNOME_VFS_OK,
					     uri,
					     info,
					     metadata);
		}
		else
			brasero_vfs_metadata_ctx_new (self, ctx, uri, info);
	}
}

gboolean
brasero_vfs_get_metadata (BraseroVFS *self,
			  GList *uris,
			  GnomeVFSFileInfoOptions vfs_flags,
			  BraseroMetadataFlag meta_flags,
			  gboolean recursive,
			  BraseroVFSDataID id,
			  gpointer callback_data)
{
	BraseroMetadataData *data;
	BraseroVFSTaskCtx *ctx;
	gboolean result;

	ctx = brasero_vfs_ctx_new (self, id, callback_data);
	if (!ctx)
		return FALSE;

	/* we start with a refcount of 1 since we're going to call a function */
	data = g_new0 (BraseroMetadataData, 1);
	data->refcount = 1;
	data->recursive = recursive;
	data->flags = meta_flags;

	BRASERO_CTX_TASK_DATA (ctx) = data;
	BRASERO_CTX_SET_TASK_METHOD (self, ctx, BRASERO_TASK_TYPE_METADATA);

	BRASERO_CTX_SET_METATASK (ctx);
	result = brasero_vfs_get_info (self,
				       uris,
				       FALSE,
				       vfs_flags|
				       GNOME_VFS_FILE_INFO_FOLLOW_LINKS|
				       GNOME_VFS_FILE_INFO_GET_MIME_TYPE|
				       GNOME_VFS_FILE_INFO_FORCE_SLOW_MIME_TYPE,
				       BRASERO_TASK_TYPE_METADATA_SUBTASK,
				       ctx);

	return result;
}

/**
 * used to count the number of files and the global size
 */

static void
brasero_vfs_count_destroy (GObject *obj, gpointer user_data, gboolean cancelled)
{
	BraseroCountData *data;
	BraseroVFSTaskCtx *ctx = user_data;
	BraseroVFS *self = BRASERO_VFS (obj);

	/* refcount the data before to avoid reaching 0 when cancelling
	 * all the sub-tasks */
	data = BRASERO_CTX_TASK_DATA (ctx);

	/* do it only if the destruction results from cancelling */
	if (cancelled) {
		data->refcount ++;
		brasero_vfs_cancel (self, ctx);
	}
			    
	g_free (data);
}

static void
brasero_vfs_count_subtask_destroy (GObject *obj,
				   gpointer user_data,
				   gboolean cancelled)
{
	BraseroCountData *data;
	BraseroVFSTaskCtx *ctx = user_data;

	data = BRASERO_CTX_TASK_DATA (ctx);

	if (!cancelled)
		BRASERO_CTX_COUNT_CB (ctx,
				      data->files_num,
				      data->invalid_num,
				      data->files_size);

	data->refcount --;
	if (data->refcount <= 0)
		brasero_vfs_task_ctx_free (ctx, FALSE);
}

static void
brasero_vfs_count_result_audio (BraseroVFS *self,
			        GObject *owner,
			        GnomeVFSResult result,
			        const gchar *uri,
			        GnomeVFSFileInfo *info,
			        BraseroMetadataInfo *metadata,
			        gpointer user_data)
{
	BraseroVFSTaskCtx *ctx = user_data;
	BraseroCountData *data;

	data = BRASERO_CTX_TASK_DATA (ctx);

#ifdef BUILD_PLAYLIST

	if (info
	&&  info->mime_type
	&&  (!strcmp (info->mime_type, "audio/x-scpls")
	||   !strcmp (info->mime_type, "audio/x-ms-asx")
	||   !strcmp (info->mime_type, "audio/x-mp3-playlist")
	||   !strcmp (info->mime_type, "audio/x-mpegurl"))) {
		/* it's a playlist so let's parse it */
		data->refcount ++;

		brasero_vfs_parse_playlist (self,
					    uri,
					    GNOME_VFS_FILE_INFO_FOLLOW_LINKS,
					    BRASERO_TASK_TYPE_COUNT_SUBTASK_PLAYLIST,
					    ctx);

		return;
	}

#endif

	data->files_num ++;
	
	if (result != GNOME_VFS_OK
	|| !metadata
	|| !metadata->has_audio)
		data->invalid_num ++;
	else
		data->files_size += metadata->len;

	BRASERO_CTX_COUNT_CB (ctx,
			      data->files_num,
			      data->invalid_num,
			      data->files_size);
}

static void
brasero_vfs_count_result_data (BraseroVFS *self,
			       GObject *owner,
			       GnomeVFSResult result,
			       const gchar *uri,
			       GnomeVFSFileInfo *info,
			       gpointer user_data)
{
	BraseroVFSTaskCtx *ctx = user_data;
	BraseroCountData *data;

	data = BRASERO_CTX_TASK_DATA (ctx);

	data->files_num ++;
	if (result != GNOME_VFS_OK) {
		data->invalid_num ++;
		return;
	}

	if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
		data->refcount ++;
		brasero_vfs_load_directory (BRASERO_CTX_SELF (ctx),
					    uri,
					    GNOME_VFS_FILE_INFO_DEFAULT,//GNOME_VFS_FILE_INFO_FOLLOW_LINKS,
					    BRASERO_TASK_TYPE_COUNT_SUBTASK_DATA,
					    ctx);
	}
	else if (info->type == GNOME_VFS_FILE_TYPE_REGULAR)
		data->files_size += info->size;
}

gboolean
brasero_vfs_get_count (BraseroVFS *self,
		       GList *uris,
		       gboolean audio,
		       BraseroVFSDataID id,
		       gpointer callback_data)
{
	BraseroCountData *data;
	BraseroVFSTaskCtx *ctx;

	ctx = brasero_vfs_ctx_new (self, id, callback_data);
	if (!ctx)
		return FALSE;

	/* we start with a refcount of 1 since we're going to call a function */
	data = g_new0 (BraseroCountData, 1);
	data->refcount = 1;

	BRASERO_CTX_TASK_DATA (ctx) = data;
	BRASERO_CTX_SET_TASK_METHOD (self, ctx, BRASERO_TASK_TYPE_COUNT);

	BRASERO_CTX_SET_METATASK (ctx);

	if (audio)
		return brasero_vfs_get_metadata (self,
						 uris,
						// GNOME_VFS_FILE_INFO_FOLLOW_LINKS|
						 GNOME_VFS_FILE_INFO_FORCE_SLOW_MIME_TYPE|
						 GNOME_VFS_FILE_INFO_GET_MIME_TYPE,
						 BRASERO_METADATA_FLAG_FAST,
						 FALSE,
						 BRASERO_TASK_TYPE_COUNT_SUBTASK_AUDIO,
						 ctx);
	else
		return brasero_vfs_get_info (self,
					     uris,
					     TRUE,
					     GNOME_VFS_FILE_INFO_DEFAULT,//GNOME_VFS_FILE_INFO_FOLLOW_LINKS,
					     BRASERO_TASK_TYPE_COUNT_SUBTASK_DATA,
					     ctx);
}

/**
 * used to parse playlists
 */

#ifdef BUILD_PLAYLIST

static void
brasero_vfs_playlist_destroy (GObject *object,
			      gpointer user_data,
			      gboolean cancelled)
{
	BraseroVFS *self = BRASERO_VFS (object);
	BraseroVFSTaskCtx *ctx = user_data;
	BraseroVFSPlaylistData *data;

	if (cancelled)
		brasero_vfs_cancel (self, ctx);

	data = BRASERO_CTX_TASK_DATA (ctx);

	if (data->uri) {
		g_free (data->uri);
		data->uri = NULL;
	}

	if (data->title) {
		g_free (data->title);
		data->title = NULL;
	}

	if (data->parser) {
		g_object_unref (data->parser);
		data->parser = NULL;
	}

	g_list_foreach (data->uris, (GFunc) g_free, NULL);
	g_list_free (data->uris);
	data->uris = NULL;

	g_free (data);
}

static void
brasero_vfs_playlist_subtask_destroy (GObject *object,
				      gpointer user_data,
				      gboolean cancelled)
{
	BraseroVFSTaskCtx *ctx = user_data;

	/* all the metadata for the job have been
	 * fetched or we've been cancelled */
	if (!cancelled)
		brasero_vfs_task_ctx_free (ctx, FALSE);
}

static void
brasero_vfs_playlist_subtask_result (BraseroVFS *self,
				     GObject *object,
				     GnomeVFSResult result,
				     const gchar *uri,
				     GnomeVFSFileInfo *info,
				     BraseroMetadataInfo *metadata,
				     gpointer user_data)
{
	BraseroVFSTaskCtx *ctx = user_data;
	BraseroVFSPlaylistData *data;

	data = BRASERO_CTX_TASK_DATA (ctx);
	BRASERO_CTX_META_CB (ctx,
			     result,
			     uri,
			     info,
			     metadata);
}

static void
brasero_vfs_playlist_result (BraseroAsyncTaskManager *manager,
			     gpointer callback_data)
{
	BraseroVFSPlaylistData *data;
	BraseroVFSTaskCtx *ctx = callback_data;
	BraseroVFS *self = BRASERO_VFS (manager);

	data = BRASERO_CTX_TASK_DATA (ctx);

	g_object_unref (data->parser);
	data->parser = NULL;

	if (data->res != TOTEM_PL_PARSER_RESULT_SUCCESS) {
		BRASERO_CTX_META_CB (ctx,
				     GNOME_VFS_ERROR_GENERIC,
				     data->uri,
				     NULL,
				     NULL);
		return;
	}
	
	BRASERO_CTX_META_CB (ctx,
			     GNOME_VFS_OK,
			     data->title,
			     NULL,
			     NULL);

	BRASERO_CTX_SET_METATASK (ctx);
	data->uris = g_list_reverse (data->uris);
	brasero_vfs_get_metadata (self,
				  data->uris,
				  data->flags,
				  BRASERO_METADATA_FLAG_NONE,
				  FALSE,
				  BRASERO_TASK_TYPE_PLAYLIST_SUBTASK,
				  ctx);				       
}

static void
brasero_vfs_add_playlist_entry_cb (TotemPlParser *parser,
				   const gchar *uri,
				   const gchar *title,
				   const gchar *genre,
				   BraseroVFSPlaylistData *data)
{
	data->uris = g_list_prepend (data->uris, g_strdup (uri));
}

static void
brasero_vfs_start_end_playlist_cb (TotemPlParser *parser,
				   const gchar *title,
				   BraseroVFSPlaylistData *data)
{
	if (!title)
		return;

	if (!data->title)
		data->title = g_strdup (title);}

static void
brasero_vfs_playlist_thread (BraseroAsyncTaskManager *manager,
			     gpointer callback_data)
{
	BraseroVFSPlaylistData *data;
	BraseroVFSTaskCtx *ctx = callback_data;

	data = BRASERO_CTX_TASK_DATA (ctx);
	g_signal_connect (G_OBJECT (data->parser),
			  "playlist-start",
			  G_CALLBACK (brasero_vfs_start_end_playlist_cb),
			  data);
	g_signal_connect (G_OBJECT (data->parser),
			  "playlist-end",
			  G_CALLBACK (brasero_vfs_start_end_playlist_cb),
			  data);
	g_signal_connect (G_OBJECT (data->parser),
			  "entry",
			  G_CALLBACK (brasero_vfs_add_playlist_entry_cb),
			  data);

	if (g_object_class_find_property (G_OBJECT_GET_CLASS (data->parser), "recurse"))
		g_object_set (G_OBJECT (data->parser), "recurse", FALSE, NULL);

	data->res = totem_pl_parser_parse (data->parser, data->uri, TRUE);
}

gboolean
brasero_vfs_parse_playlist (BraseroVFS *self,
			    const gchar *uri,
			    GnomeVFSFileInfoOptions flags,
			    BraseroVFSDataID id,
			    gpointer callback_data)
{
	BraseroVFSPlaylistData *data;
	BraseroVFSTaskCtx *ctx;

	ctx = brasero_vfs_ctx_new (self, id, callback_data);
	if (!ctx)
		return FALSE;

	data = g_new0 (BraseroVFSPlaylistData, 1);
	data->uri = g_strdup (uri);
	data->flags = flags;
	data->parser = totem_pl_parser_new ();

	BRASERO_CTX_TASK_DATA (ctx) = data;
	BRASERO_CTX_SET_TASK_METHOD (self, ctx, BRASERO_TASK_TYPE_PLAYLIST);

	return brasero_async_task_manager_queue (BRASERO_ASYNC_TASK_MANAGER (self),
						 self->priv->playlist_type,
						 ctx);
}

#endif /* BUILD_PLAYLIST */

/**
 * used to stop/cancel tasks
 */

static gboolean
brasero_vfs_async_lookup_object_queues (BraseroAsyncTaskManager *manager,
					gpointer task,
					gpointer user_data)
{
	BraseroVFSTaskCtx *ctx = task;

	if (BRASERO_CTX_OWNER (ctx) == user_data
	|| (BRASERO_CTX_OWNER (ctx) == G_OBJECT (manager)
	&&  BRASERO_CTX_USER_DATA (ctx) == user_data)) {
		brasero_vfs_task_ctx_free (ctx, TRUE);
		return TRUE;
	}

	return FALSE;
}

static gboolean
brasero_vfs_async_lookup_object_active (BraseroAsyncTaskManager *manager,
					gpointer task,
					gpointer user_data)
{
	BraseroVFSTaskCtx *ctx = task;

	if (BRASERO_CTX_OWNER (ctx) == user_data
	|| (BRASERO_CTX_OWNER (ctx) == G_OBJECT (manager)
	&&  BRASERO_CTX_USER_DATA (ctx) == user_data)) {
		ctx->cancelled = TRUE;
		return TRUE;
	}

	return FALSE;
}

void
brasero_vfs_cancel (BraseroVFS *self, gpointer owner)
{
	GSList *iter, *next;

	/* stop waiting jobs first */
	brasero_async_task_manager_foreach_unprocessed_remove (BRASERO_ASYNC_TASK_MANAGER (self),
							       brasero_vfs_async_lookup_object_queues,
							       owner);

	/* wait for the active jobs to finish */
	brasero_async_task_manager_foreach_active (BRASERO_ASYNC_TASK_MANAGER (self),
						   brasero_vfs_async_lookup_object_active,
						   owner);

	/* clean the result queue */
	brasero_async_task_manager_foreach_processed_remove (BRASERO_ASYNC_TASK_MANAGER (self),
							     brasero_vfs_async_lookup_object_queues,
							     owner);

	for (iter = self->priv->metatasks; iter; iter = next) {
		BraseroVFSTaskCtx *ctx;

		ctx = iter->data;
		next = iter->next;

		if (BRASERO_CTX_OWNER (ctx) == owner
		|| (BRASERO_CTX_OWNER (ctx) == G_OBJECT (self)
		&&  BRASERO_CTX_USER_DATA (ctx) == owner))
			brasero_vfs_task_ctx_free (ctx, TRUE);
	}
}

static void
brasero_vfs_stop_all (BraseroVFS *self)
{
    	GSList *iter, *next;

    	for (iter = self->priv->metatasks; iter; iter = next) {
		BraseroVFSTaskCtx *ctx;

		ctx = iter->data;
		next = iter->next;
		brasero_vfs_task_ctx_free (ctx, TRUE);
	}
}

struct _BraseroVFSTaskCompareData {
	BraseroVFSCompareFunc func;
	BraseroVFSDataType *type;
	gpointer user_data;
};
typedef struct _BraseroVFSTaskCompareData BraseroVFSTaskCompareData;

static gboolean
brasero_vfs_compare_unprocessed_task (BraseroAsyncTaskManager *manager,
				      gpointer task,
				      gpointer callback_data)
{
	BraseroVFSTaskCtx *ctx = task;
	BraseroVFSTaskCompareData *data = callback_data;

	if (ctx->user_method != data->type)
		return FALSE;

	return data->func (ctx->user_data, data->user_data);
}

gboolean
brasero_vfs_find_urgent (BraseroVFS *self,
			 BraseroVFSDataID id,
			 BraseroVFSCompareFunc func,
			 gpointer user_data)
{
	BraseroVFSTaskCompareData callback_data;

	g_return_val_if_fail (self != NULL, FALSE);
	g_return_val_if_fail (func != NULL, FALSE);

	callback_data.func = func;
	callback_data.user_data = user_data;
	callback_data.type = g_hash_table_lookup (self->priv->types,
						  GINT_TO_POINTER (id));

	return brasero_async_task_manager_find_urgent_task (BRASERO_ASYNC_TASK_MANAGER (self),
							    brasero_vfs_compare_unprocessed_task,
							    &callback_data);
}

/**
 * used to register new data type
 */

BraseroVFSDataID
brasero_vfs_register_data_type (BraseroVFS *self,
				GObject *owner,
				GCallback callback,
				BraseroVFSDestroyCallback destroy_func)
{
	BraseroVFSDataType *task;

	/* determine the identifier */
	self->priv->type_num ++;
	if (self->priv->type_num == G_MAXINT) {
		self->priv->type_num = 1;

		while (g_hash_table_lookup (self->priv->types, GINT_TO_POINTER (self->priv->type_num))) {
			self->priv->type_num ++;

			if (self->priv->type_num == G_MAXINT) {
				g_warning ("ERROR: reached the max number of types\n");
				return 0;
			}
		}
	}

	/* create the task and insert it */
	task = g_new0 (BraseroVFSDataType, 1);
	task->owner = owner;
	task->callback = callback;
	task->destroy = destroy_func;

	g_hash_table_insert (self->priv->types,
			     GINT_TO_POINTER (self->priv->type_num),
			     task);

	return self->priv->type_num;
}

static void
brasero_vfs_init (BraseroVFS *obj)
{
	int i;

	obj->priv = g_new0 (BraseroVFSPrivate, 1);

	for (i = 0; i < MAX_CONCURENT_META; i ++) {
		BraseroMetadata *metadata;

		metadata = brasero_metadata_new ();
		g_signal_connect (G_OBJECT (metadata),
				  "completed",
				  G_CALLBACK (brasero_vfs_metadata_completed_cb),
				  obj);
		obj->priv->metadatas = g_slist_prepend (obj->priv->metadatas, metadata);
	}

	obj->priv->meta_buffer = g_queue_new ();

	obj->priv->types = g_hash_table_new_full (g_direct_hash,
						  g_direct_equal,
						  NULL,
						  g_free);

	obj->priv->info_type = 
		brasero_async_task_manager_register_type (BRASERO_ASYNC_TASK_MANAGER (obj),
							  brasero_vfs_info_thread,
							  brasero_vfs_info_result);
	obj->priv->load_type = 
		brasero_async_task_manager_register_type (BRASERO_ASYNC_TASK_MANAGER (obj),
							  brasero_vfs_load_thread,
							  brasero_vfs_load_result);

#ifdef BUILD_PLAYLIST

	obj->priv->playlist_type =
		brasero_async_task_manager_register_type (BRASERO_ASYNC_TASK_MANAGER (obj),
							  brasero_vfs_playlist_thread,
							  brasero_vfs_playlist_result);

#endif

	brasero_vfs_register_data_type (obj,
					G_OBJECT (obj),
					NULL,
					brasero_vfs_info_destroy);
	brasero_vfs_register_data_type (obj,
					G_OBJECT (obj),
					NULL,
					brasero_vfs_load_directory_destroy);
	brasero_vfs_register_data_type (obj,
					G_OBJECT (obj),
					NULL,
					brasero_vfs_count_destroy);
	brasero_vfs_register_data_type (obj,
					G_OBJECT (obj),
					G_CALLBACK (brasero_vfs_count_result_audio),
					brasero_vfs_count_subtask_destroy);
	brasero_vfs_register_data_type (obj,
					G_OBJECT (obj),
					G_CALLBACK (brasero_vfs_count_result_data),
					brasero_vfs_count_subtask_destroy);
	brasero_vfs_register_data_type (obj,
					G_OBJECT (obj),
					G_CALLBACK (brasero_vfs_count_result_audio),
					brasero_vfs_count_subtask_destroy);
	brasero_vfs_register_data_type (obj,
					G_OBJECT (obj),
					NULL,
					brasero_vfs_metadata_destroy);
	brasero_vfs_register_data_type (obj,
					G_OBJECT (obj),
					G_CALLBACK (brasero_vfs_metadata_result),
					brasero_vfs_metadata_subtask_destroy);

#ifdef BUILD_PLAYLIST

	brasero_vfs_register_data_type (obj,
					G_OBJECT (obj),
					NULL,
					brasero_vfs_playlist_destroy);

	brasero_vfs_register_data_type (obj,
					G_OBJECT (obj),
					G_CALLBACK (brasero_vfs_playlist_subtask_result),
					brasero_vfs_playlist_subtask_destroy);

#endif

}

static void
brasero_vfs_finalize (GObject *object)
{
	BraseroVFS *cobj;
	
	cobj = BRASERO_VFS (object);

	if (cobj->priv->meta_buffer) {
		BraseroMetadataInfo *metadata;

		while ((metadata = g_queue_pop_head (cobj->priv->meta_buffer)) != NULL)
			brasero_metadata_info_free (metadata);

		g_queue_free (cobj->priv->meta_buffer);
		cobj->priv->meta_buffer = NULL;
	}

	brasero_vfs_stop_all (cobj);

	if (cobj->priv->types) {
		g_hash_table_destroy (cobj->priv->types);
		cobj->priv->types = NULL;
	}

	g_slist_foreach (cobj->priv->unprocessed_meta, (GFunc) brasero_vfs_metadata_task_free, NULL);
	g_slist_free (cobj->priv->unprocessed_meta);
	cobj->priv->unprocessed_meta = NULL;

	g_slist_foreach (cobj->priv->processing_meta, (GFunc) brasero_vfs_metadata_task_free, NULL);
	g_slist_free (cobj->priv->processing_meta);
	cobj->priv->processing_meta = NULL;

	g_slist_foreach (cobj->priv->metadatas, (GFunc) g_object_unref, NULL);
	g_slist_free (cobj->priv->metadatas);
	cobj->priv->metadatas = NULL;

	g_free (cobj->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}
