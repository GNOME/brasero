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

#include "brasero-vfs.h"
#include "brasero-async-task-manager.h"
#include "metadata.h"

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

struct _BraseroVFSMetaCtx {
	BraseroVFSTaskCtx *ctx;
	GnomeVFSFileInfo *info;
	BraseroMetadata *metadata;
};
typedef struct _BraseroVFSMetaCtx BraseroVFSMetaCtx;

struct _BraseroInfoData {
	GSList *results;
	GSList *uris;
	gint flags;
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

struct _BraseroVFSPlaylistData {
	gchar *uri;
	GList *uris;
	TotemPlParser *parser;
	TotemPlParserResult res;
	GnomeVFSFileInfoOptions flags;
};
typedef struct _BraseroVFSPlaylistData BraseroVFSPlaylistData;

struct _BraseroInfoResult  {
	GnomeVFSFileInfo *info;
	GnomeVFSResult result;
	gchar *uri;
};
typedef struct _BraseroInfoResult BraseroInfoResult;

struct _BraseroMetadataResult {
	GnomeVFSFileInfo *info;
	BraseroMetadata *metadata;
	gulong signal;
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

#define BRASERO_CTX_PLAYLIST_CB(ctx, result, uri, info, metadata)		\
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
	BraseroAsyncTaskTypeID playlist_type;

	/* used for metadata */
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
#define MAX_CONCURENT_META 	1
#define MAX_BUFFERED_META	8
#define STOP_META_TIMEOUT	500

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
		g_slist_foreach (data->uris, (GFunc) gnome_vfs_uri_unref, NULL);
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
		GnomeVFSURI *vfsuri;

		if (ctx->cancelled)
			break;

		next = iter->next;
		vfsuri = iter->data;
		data->uris = g_slist_remove (data->uris, vfsuri);

		result = g_new0 (BraseroInfoResult, 1);
		data->results = g_slist_prepend (data->results, result);

		result->info = gnome_vfs_file_info_new ();
		result->uri = gnome_vfs_uri_to_string (vfsuri, GNOME_VFS_URI_HIDE_NONE);
		result->result = gnome_vfs_get_file_info_uri (vfsuri,
							      result->info,
							      data->flags);

		gnome_vfs_uri_unref (vfsuri);
	}

	data->uris = NULL;
}

gboolean
brasero_vfs_get_info (BraseroVFS *self,
		      GList *uris,
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

	for (iter = uris; iter; iter = iter->next) {
		GnomeVFSURI *vfsuri;
		gchar *uri;

		uri = iter->data;
		vfsuri = gnome_vfs_uri_new (uri);
		data->uris = g_slist_prepend (data->uris, vfsuri);
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
				     GNOME_VFS_OK,
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
		result->uri = g_build_filename (data->root, name, NULL);
		g_free (name);

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
	BraseroMetadata *metadata;
	BraseroVFS *self = BRASERO_VFS (callback_data);

	if (!self->priv->unprocessed_meta) {
		self->priv->meta_process_id = 0;
		return FALSE;
	}

	/* to avoid too much overhead we only fetch
	 * MAX_CONCURENT_META metadata files at a time */
	if (g_slist_length (self->priv->processing_meta) >= MAX_CONCURENT_META) {
		self->priv->meta_process_id = 0;
		return FALSE;
	}

	metadata = self->priv->unprocessed_meta->data;
	self->priv->unprocessed_meta = g_slist_remove (self->priv->unprocessed_meta, metadata);
	self->priv->processing_meta = g_slist_prepend (self->priv->processing_meta, metadata);

	brasero_metadata_get_async (metadata, FALSE);
	return TRUE;
}

static void
brasero_vfs_metadata_processed (BraseroVFS *self,
				BraseroMetadata *metadata)
{
	if (!metadata)
		return;

	self->priv->processing_meta = g_slist_remove (self->priv->processing_meta,
						      metadata);
	if (!self->priv->meta_process_id
	&&  g_slist_length (self->priv->processing_meta) < MAX_CONCURENT_META)
		self->priv->meta_process_id = g_idle_add (brasero_vfs_process_metadata,
							  self);

	if (!metadata->has_audio && !metadata->has_video)
		return;

	if (g_queue_find (self->priv->meta_buffer, metadata))
		return;

	g_object_ref (metadata);
	g_queue_push_head (self->priv->meta_buffer, metadata);

	if (g_queue_get_length (self->priv->meta_buffer) > MAX_BUFFERED_META) {
		metadata = g_queue_pop_tail (self->priv->meta_buffer);
		g_object_unref (metadata);
	}
}

static gboolean
brasero_vfs_metadata_unref_cb (gpointer callback_data)
{
	BraseroMetadata *metadata = BRASERO_METADATA (callback_data);

	brasero_metadata_cancel (metadata);
	g_object_unref (metadata);

	return FALSE;
}

static void
brasero_vfs_metadata_refcount_changed (gpointer data,
				       GObject *object,
				       gboolean is_last_ref)
{
	BraseroVFS *self = BRASERO_VFS (data);

	if (is_last_ref) {
		self->priv->processing_meta = g_slist_remove (self->priv->processing_meta,
							      object);
		self->priv->unprocessed_meta = g_slist_remove (self->priv->unprocessed_meta,
							       object);

		g_object_ref (object);
		g_object_remove_toggle_ref (object,
					    brasero_vfs_metadata_refcount_changed,
				            self);

		/* NOTE: to work around a bug in Gstreamer (appearing with ogg)
		 * we delay the unreffing */
		g_timeout_add (STOP_META_TIMEOUT,
			       brasero_vfs_metadata_unref_cb,
			       object);

		if (!self->priv->meta_process_id
		&&  g_slist_length (self->priv->processing_meta) < MAX_CONCURENT_META)
			self->priv->meta_process_id = g_idle_add (brasero_vfs_process_metadata,
								  self);
	}
}

static void
brasero_vfs_metadata_result_free (BraseroVFS *self,
				  BraseroMetadataResult *result,
				  gboolean cancelled)
{
	if (result->info) {
		gnome_vfs_file_info_unref (result->info);
		result->info = NULL;
	}

	if (result->signal)
		g_signal_handler_disconnect (result->metadata, result->signal);

	if (result->metadata) {
		g_object_unref (result->metadata);
		result->metadata = NULL;
	}

	g_free (result);
}

static void
brasero_vfs_metadata_completed_cb (BraseroMetadata *metadata,
				   GError *error,
				   gpointer user_data)
{
	BraseroVFSTaskCtx *ctx = user_data;
	BraseroMetadataResult *result;
	BraseroMetadataData *data;
	GSList *iter;

	brasero_vfs_metadata_processed (BRASERO_CTX_SELF (ctx),
					metadata);

	data = BRASERO_CTX_TASK_DATA (ctx);

	for (iter = data->results; iter; iter = iter->next) {
		result = iter->data;

		if (result->metadata == metadata)
			break;

		result = NULL;
	}

	if (!result) {
		g_warning ("No corresponding BraseroMetadataResult found.\n");
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
			     metadata->uri,
			     result->info,
			     metadata);

	brasero_vfs_metadata_result_free (BRASERO_CTX_SELF (ctx), result, FALSE);

	if (!data->results || !data->refcount)
		brasero_vfs_task_ctx_free (ctx, FALSE);
}

static gint
brasero_vfs_metadata_lookup_buffer (gconstpointer a, gconstpointer b)
{
	const BraseroMetadata *metadata = a;
	const gchar *uri = b;

	return strcmp (uri, metadata->uri);
}

static gboolean
brasero_vfs_metadata_ctx_new (BraseroVFS *self,
			      BraseroVFSTaskCtx *ctx,
			      const gchar *uri,
			      GnomeVFSFileInfo *info)
{
	BraseroMetadata *metadata = NULL;
	BraseroMetadataData *data;
	BraseroMetadataResult *result;
	GSList *iter;

	/* see if one of the metadata in the queue (unprocessed or processing)
	 * is not about the same uri: if so ref the metadata and add a CB */
	for (iter = self->priv->unprocessed_meta; iter; iter = iter->next) {
		metadata = iter->data;
		if (!strcmp (metadata->uri, uri)) {
			g_object_ref (metadata);
			break;
		}

		metadata = NULL;
	}

	for (iter = self->priv->processing_meta; iter && !metadata; iter = iter->next) {
		metadata = iter->data;
		if (!strcmp (metadata->uri, uri))
			g_object_ref (metadata);
		else
			metadata = NULL;
	}

	if (!metadata) {
		metadata = brasero_metadata_new (uri);

		if (!metadata)
			return FALSE;

		g_object_add_toggle_ref (G_OBJECT (metadata),
					 brasero_vfs_metadata_refcount_changed,
					 self);

		if (!self->priv->meta_process_id
		&&  g_slist_length (self->priv->processing_meta) <= MAX_CONCURENT_META)
			self->priv->meta_process_id = g_idle_add (brasero_vfs_process_metadata,
								  self);

		self->priv->unprocessed_meta = g_slist_append (self->priv->unprocessed_meta, metadata);
	}

	/* add a reference to the parent */
	data = BRASERO_CTX_TASK_DATA (ctx);
	data->refcount ++;

	/* we start with a refcount of 1 since we're going to call a function */
	result = g_new0 (BraseroMetadataResult, 1);
	data->results = g_slist_prepend (data->results, result);

	/* keep info for later: add a reference */
	gnome_vfs_file_info_ref (info);
	result->info = info;

	/* connect to the completed signal and wait for metadata object
	 * to be started and to complete its job */
	result->metadata = metadata;
	result->signal = g_signal_connect (G_OBJECT (metadata),
					   "completed",
					   G_CALLBACK (brasero_vfs_metadata_completed_cb),
					   ctx);
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
		brasero_vfs_metadata_result_free (self, result, cancelled);
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
			BraseroMetadata *metadata;

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
			  GnomeVFSFileInfoOptions flags,
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

	BRASERO_CTX_TASK_DATA (ctx) = data;
	BRASERO_CTX_SET_TASK_METHOD (self, ctx, BRASERO_TASK_TYPE_METADATA);

	BRASERO_CTX_SET_METATASK (ctx);
	result = brasero_vfs_get_info (self,
				       uris,
				       flags|
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
			        BraseroMetadata *metadata,
			        gpointer user_data)
{
	BraseroVFSTaskCtx *ctx = user_data;
	BraseroCountData *data;

	data = BRASERO_CTX_TASK_DATA (ctx);

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
					    GNOME_VFS_FILE_INFO_FOLLOW_LINKS,
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
						 GNOME_VFS_FILE_INFO_FOLLOW_LINKS|
						 GNOME_VFS_FILE_INFO_FORCE_SLOW_MIME_TYPE|
						 GNOME_VFS_FILE_INFO_GET_MIME_TYPE,
						 FALSE,
						 BRASERO_TASK_TYPE_COUNT_SUBTASK_AUDIO,
						 ctx);
	else
		return brasero_vfs_get_info (self,
					     uris,
					     GNOME_VFS_FILE_INFO_FOLLOW_LINKS,
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
				     BraseroMetadata *metadata,
				     gpointer user_data)
{
	BraseroVFSTaskCtx *ctx = user_data;

	BRASERO_CTX_PLAYLIST_CB (ctx,
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
		BRASERO_CTX_PLAYLIST_CB (ctx,
					 GNOME_VFS_ERROR_GENERIC,
					 data->uri,
					 NULL,
					 NULL);
		return;
	}

	BRASERO_CTX_SET_METATASK (ctx);
	data->uris = g_list_reverse (data->uris);
	brasero_vfs_get_metadata (self,
				  data->uris,
				  data->flags,
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
brasero_vfs_playlist_thread (BraseroAsyncTaskManager *manager,
			     gpointer callback_data)
{
	BraseroVFSPlaylistData *data;
	BraseroVFSTaskCtx *ctx = callback_data;

	data = BRASERO_CTX_TASK_DATA (ctx);
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
	obj->priv = g_new0 (BraseroVFSPrivate, 1);

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
	obj->priv->playlist_type =
		brasero_async_task_manager_register_type (BRASERO_ASYNC_TASK_MANAGER (obj),
							  brasero_vfs_playlist_thread,
							  brasero_vfs_playlist_result);

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
	brasero_vfs_register_data_type (obj,
					G_OBJECT (obj),
					NULL,
					brasero_vfs_playlist_destroy);
	brasero_vfs_register_data_type (obj,
					G_OBJECT (obj),
					G_CALLBACK (brasero_vfs_playlist_subtask_result),
					brasero_vfs_playlist_subtask_destroy);
}

static void
brasero_vfs_finalize (GObject *object)
{
	BraseroVFS *cobj;
	
	cobj = BRASERO_VFS (object);

	if (cobj->priv->meta_buffer) {
		BraseroMetadata *metadata;

		while ((metadata = g_queue_pop_head (cobj->priv->meta_buffer)) != NULL)
			g_object_unref (metadata);

		g_queue_free (cobj->priv->meta_buffer);
		cobj->priv->meta_buffer = NULL;
	}

	brasero_vfs_stop_all (cobj);

	if (cobj->priv->types) {
		g_hash_table_destroy (cobj->priv->types);
		cobj->priv->types = NULL;
	}

	g_free (cobj->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}
