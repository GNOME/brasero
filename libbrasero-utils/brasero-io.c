/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-misc
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-misc is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-misc authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-misc. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-misc is distributed in the hope that it will be useful,
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
#include <errno.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <gio/gio.h>

#include <gdk/gdkx.h>

#include <gtk/gtk.h>

#ifdef BUILD_PLAYLIST
#include <totem-pl-parser.h>
#endif

#include "brasero-misc.h"
#include "brasero-io.h"
#include "brasero-metadata.h"
#include "brasero-async-task-manager.h"

#define BRASERO_TYPE_IO             (brasero_io_get_type ())
#define BRASERO_IO(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_IO, BraseroIO))
#define BRASERO_IO_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_IO, BraseroIOClass))
#define BRASERO_IS_IO(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_IO))
#define BRASERO_IS_IO_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_IO))
#define BRASERO_IO_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_IO, BraseroIOClass))

typedef struct _BraseroIOClass BraseroIOClass;
typedef struct _BraseroIO BraseroIO;

struct _BraseroIOClass
{
	BraseroAsyncTaskManagerClass parent_class;
};

struct _BraseroIO
{
	BraseroAsyncTaskManager parent_instance;
};

GType brasero_io_get_type (void) G_GNUC_CONST;

typedef struct _BraseroIOPrivate BraseroIOPrivate;
struct _BraseroIOPrivate
{
	GMutex *lock;

	GSList *mounted;

	/* used for returning results */
	GSList *results;
	gint results_id;

	/* used for metadata */
	GMutex *lock_metadata;

	GSList *metadatas;
	GSList *metadata_running;

	/* used to "buffer" some results returned by metadata.
	 * It takes time to return metadata and it's not unusual
	 * to fetch metadata three times in a row, once for size
	 * preview, once for preview, once adding to selection */
	GQueue *meta_buffer;

	guint progress_id;
	GSList *progress;

	BraseroIOGetParentWinCb win_callback;
	gpointer win_user_data;
};

#define BRASERO_IO_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_IO, BraseroIOPrivate))

/* so far 2 metadata at a time has shown to be the best for performance */
#define MAX_CONCURENT_META 	2
#define MAX_BUFFERED_META	20

struct _BraseroIOJobResult {
	const BraseroIOJobBase *base;
	BraseroIOResultCallbackData *callback_data;

	GFileInfo *info;
	GError *error;
	gchar *uri;
};
typedef struct _BraseroIOJobResult BraseroIOJobResult;


typedef void	(*BraseroIOJobProgressCallback)	(BraseroIOJob *job,
						 BraseroIOJobProgress *progress);

struct _BraseroIOJobProgress {
	BraseroIOJob *job;
	BraseroIOJobProgressCallback progress;

	BraseroIOPhase phase;

	guint files_num;
	guint files_invalid;

	guint64 read_b;
	guint64 total_b;

	guint64 current_read_b;
	guint64 current_total_b;

	gchar *current;
};

G_DEFINE_TYPE (BraseroIO, brasero_io, BRASERO_TYPE_ASYNC_TASK_MANAGER);

static BraseroIO *singleton = NULL;

static BraseroIO *
brasero_io_get_default ()
{
	if (singleton) {
		g_object_ref (singleton);
		return singleton;
	}

	singleton = g_object_new (BRASERO_TYPE_IO, NULL);
	g_object_ref (singleton);
	return singleton;
}

/**
 * That's the structure to pass the progress on
 */

static gboolean
brasero_io_job_progress_report_cb (gpointer callback_data)
{
	BraseroIOPrivate *priv;
	GSList *iter;

	priv = BRASERO_IO_PRIVATE (callback_data);

	g_mutex_lock (priv->lock);
	for (iter = priv->progress; iter; iter = iter->next) {
		BraseroIOJobProgress *progress;
		gpointer callback_data;

		progress = iter->data;

		callback_data = progress->job->callback_data?
				progress->job->callback_data->callback_data:
				NULL;

		/* update our progress */
		progress->progress (progress->job, progress);
		progress->job->base->methods->progress (progress->job->base->object,
		                                        progress,
		                                        callback_data);
	}
	g_mutex_unlock (priv->lock);

	return TRUE;
}

static void
brasero_io_job_progress_report_start (BraseroIO *self,
				      BraseroIOJob *job,
				      BraseroIOJobProgressCallback callback)
{
	BraseroIOJobProgress *progress;
	BraseroIOPrivate *priv;

	priv = BRASERO_IO_PRIVATE (self);

	if (!job->base->methods->progress)
		return;

	progress = g_new0 (BraseroIOJobProgress, 1);
	progress->job = job;
	progress->progress = callback;

	g_mutex_lock (priv->lock);
	priv->progress = g_slist_prepend (priv->progress, progress);
	if (!priv->progress_id)
		priv->progress_id = g_timeout_add (500, brasero_io_job_progress_report_cb, self);
	g_mutex_unlock (priv->lock);
}

static void
brasero_io_job_progress_report_stop (BraseroIO *self,
				     BraseroIOJob *job)
{
	BraseroIOPrivate *priv;
	GSList *iter;

	priv = BRASERO_IO_PRIVATE (self);
	g_mutex_lock (priv->lock);
	for (iter = priv->progress; iter; iter = iter->next) {
		BraseroIOJobProgress *progress;

		progress = iter->data;
		if (progress->job == job) {
			priv->progress = g_slist_remove (priv->progress, progress);
			if (progress->current)
				g_free (progress->current);

			g_free (progress);
			break;
		}
	}

	if (!priv->progress) {
		if (priv->progress_id) {
			g_source_remove (priv->progress_id);
			priv->progress_id = 0;
		}
	}

	g_mutex_unlock (priv->lock);
}

guint
brasero_io_job_progress_get_file_processed (BraseroIOJobProgress *progress)
{
	return progress->files_num;
}

guint64
brasero_io_job_progress_get_read (BraseroIOJobProgress *progress)
{
	return progress->current_read_b + progress->read_b;
}

guint64
brasero_io_job_progress_get_total (BraseroIOJobProgress *progress)
{
	return progress->total_b;
}

BraseroIOPhase
brasero_io_job_progress_get_phase (BraseroIOJobProgress *progress)
{
	return progress->phase;
}

static void
brasero_io_unref_result_callback_data (BraseroIOResultCallbackData *data,
				       GObject *object,
				       BraseroIODestroyCallback destroy,
				       gboolean cancelled)
{
	if (!data)
		return;

	/* see atomically if we are the last to hold a lock */
	if (!g_atomic_int_dec_and_test (&data->ref))
		return;

	if (destroy)
		destroy (object,
			 cancelled,
			 data->callback_data);
	g_free (data);
}

static void
brasero_io_job_result_free (BraseroIOJobResult *result)
{
	if (result->info)
		g_object_unref (result->info);

	if (result->error)
		g_error_free (result->error);

	if (result->uri)
		g_free (result->uri);

	g_free (result);
}

/**
 * Used to return the results
 */

#define NUMBER_OF_RESULTS	25

static gboolean
brasero_io_return_result_idle (gpointer callback_data)
{
	BraseroIO *self = BRASERO_IO (callback_data);
	BraseroIOResultCallbackData *data;
	BraseroIOJobResult *result;
	BraseroIOPrivate *priv;
	guint results_id;
	int i;

	priv = BRASERO_IO_PRIVATE (self);

	g_mutex_lock (priv->lock);

	/* Put that to 0 for now so that a new idle call will be scheduled while
	 * we are in the loop. That way if we block the other one will be able 
	 * to deliver results. */
	results_id = priv->results_id;
	priv->results_id = 0;

	/* Return several results at a time that can be a huge speed gain.
	 * What should be the value that provides speed and responsiveness? */
	for (i = 0; priv->results && i < NUMBER_OF_RESULTS;) {
		BraseroIOJobBase *base;
		GSList *iter;

		/* Find the next result that can be returned */
		result = NULL;
		for (iter = priv->results; iter; iter = iter->next) {
			BraseroIOJobResult *tmp_result;

			tmp_result = iter->data;
			if (!tmp_result->base->methods->in_use) {
				result = tmp_result;
				break;
			}
		}

		if (!result)
			break;

		/* Make sure another result is not returned for this base. This 
		 * is to avoid BraseroDataDisc showing multiple dialogs for 
		 * various problems; like one dialog for joliet, one for deep,
		 * and one for name collision. */
		base = (BraseroIOJobBase *) result->base;
		base->methods->in_use = TRUE;

		priv->results = g_slist_remove (priv->results, result);

		/* This is to make sure the object
		 *  lives as long as we need it. */
		g_object_ref (base->object);

		g_mutex_unlock (priv->lock);

		data = result->callback_data;

		if (result->uri || result->info || result->error)
			base->methods->callback (base->object,
			                          result->error,
			                          result->uri,
			                          result->info,
			                          data? data->callback_data:NULL);

		/* call destroy () for callback data */
		brasero_io_unref_result_callback_data (data,
						       base->object,
						       base->methods->destroy,
						       FALSE);

		brasero_io_job_result_free (result);

		g_mutex_lock (priv->lock);

		i ++;

		g_object_unref (base->object);
		base->methods->in_use = FALSE;
	}

	if (!priv->results_id && priv->results && i >= NUMBER_OF_RESULTS) {
		/* There are still results and no idle call is scheduled so we
		 * have to restart ourselves to make sure we empty the queue */
		priv->results_id = results_id;
		g_mutex_unlock (priv->lock);
		return TRUE;
	}

	g_mutex_unlock (priv->lock);
	return FALSE;
}

static void
brasero_io_queue_result (BraseroIO *self,
			 BraseroIOJobResult *result)
{
	BraseroIOPrivate *priv;

	priv = BRASERO_IO_PRIVATE (self);

	/* insert the task in the results queue */
	g_mutex_lock (priv->lock);
	priv->results = g_slist_append (priv->results, result);
	if (!priv->results_id)
		priv->results_id = g_idle_add ((GSourceFunc) brasero_io_return_result_idle, self);
	g_mutex_unlock (priv->lock);
}

void
brasero_io_return_result (const BraseroIOJobBase *base,
			  const gchar *uri,
			  GFileInfo *info,
			  GError *error,
			  BraseroIOResultCallbackData *callback_data)
{
	BraseroIO *self = brasero_io_get_default ();
	BraseroIOJobResult *result;

	/* even if it is cancelled we let the result go through to be able to 
	 * call its destroy callback in the main thread. */

	result = g_new0 (BraseroIOJobResult, 1);
	result->base = base;
	result->info = info;
	result->error = error;
	result->uri = g_strdup (uri);

	if (callback_data) {
		g_atomic_int_inc (&callback_data->ref);
		result->callback_data = callback_data;
	}

	brasero_io_queue_result (self, result);
	g_object_unref (self);
}

/**
 * Used to push a job
 */

void
brasero_io_set_job (BraseroIOJob *job,
		    const BraseroIOJobBase *base,
		    const gchar *uri,
		    BraseroIOFlags options,
		    BraseroIOResultCallbackData *callback_data)
{
	job->base = base;
	job->uri = g_strdup (uri);
	job->options = options;
	job->callback_data = callback_data;

	if (callback_data)
		g_atomic_int_inc (&job->callback_data->ref);
}

void
brasero_io_push_job (BraseroIOJob *job,
		     const BraseroAsyncTaskType *type)
{
	BraseroIO *self = brasero_io_get_default ();

	if (job->options & BRASERO_IO_INFO_URGENT)
		brasero_async_task_manager_queue (BRASERO_ASYNC_TASK_MANAGER (self),
						  BRASERO_ASYNC_URGENT,
						  type,
						  job);
	else if (job->options & BRASERO_IO_INFO_IDLE)
		brasero_async_task_manager_queue (BRASERO_ASYNC_TASK_MANAGER (self),
						  BRASERO_ASYNC_IDLE,
						  type,
						  job);
	else
		brasero_async_task_manager_queue (BRASERO_ASYNC_TASK_MANAGER (self),
						  BRASERO_ASYNC_NORMAL,
						  type,
						  job);
	g_object_unref (self);
}

/**
 * Job destruction
 */

void
brasero_io_job_free (gboolean cancelled,
		     BraseroIOJob *job)
{
	/* NOTE: the callback_data member is never destroyed here since it would
	 * be destroyed in a thread different from the main loop.
	 * Either it's destroyed in the thread that called brasero_io_cancel ()
	 * or after all results are returned (and therefore in main loop).
	 * As a special case, some jobs like read directory contents have to
	 * return a dummy result to destroy the callback_data if the directory
	 * is empty.
	 * If the job happens to be the last to carry a reference then destroy
	 * it but do it in the main loop by sending a dummy result. */

	/* NOTE2: that's also used for directory loading when there aren't any
	 * result to notify the caller that the operation has finished but that
	 * there weren't any result to report. */
	if (job->callback_data) {
		/* see atomically if we are the last to hold a lock:
		 * If so, and if cancelled is TRUE destroy it now since we are
		 * always cancelled (and destroyed in the main loop). Otherwise
		 * add a dummy result to destroy callback_data. */
		if (g_atomic_int_dec_and_test (&job->callback_data->ref)) {
			if (cancelled) {
				if (job->base->methods->destroy)
					job->base->methods->destroy (job->base->object,
					                              TRUE,
					                              job->callback_data->callback_data);

				g_free (job->callback_data);
			}
			else {
				BraseroIO *self = brasero_io_get_default ();

				brasero_io_return_result (job->base,
							  NULL,
							  NULL,
							  NULL,
							  job->callback_data);
				g_object_unref (self);
			}
		}
	}

	g_free (job->uri);
	g_free (job);
}

static void
brasero_io_job_destroy (BraseroAsyncTaskManager *manager,
			gboolean cancelled,
			gpointer callback_data)
{
	BraseroIOJob *job = callback_data;

	/* If a job is destroyed we don't call the destroy callback since it
	 * otherwise it would be called in a different thread. All object that
	 * cancel io ops are doing it either in destroy () and therefore handle
	 * all destruction for callback_data or if they don't they usually don't
	 * pass any callback data anyway. */
	/* NOTE: usually threads are cancelled from the main thread/loop and
	 * block until the active task is removed which means that if we called
	 * the destroy () then the destruction would be done in the main loop */
	brasero_io_job_free (cancelled, job);
}

/**
 * That's when we need to mount a remote volume
 */

struct _BraseroIOMount {
	GError *error;
	gboolean result;
	gboolean finished;
};
typedef struct _BraseroIOMount BraseroIOMount;

static void
brasero_io_mount_enclosing_volume_cb (GObject *source,
				      GAsyncResult *result,
				      gpointer callback_data)
{
	BraseroIOMount *mount = callback_data;

	mount->result = g_file_mount_enclosing_volume_finish (G_FILE (source),
							      result,
							      &mount->error);
	mount->finished = TRUE;
}

static gboolean
brasero_io_mount_enclosing_volume (BraseroIO *self,
				   GFile *file,
				   GCancellable *cancel,
				   GError **error)
{
	GMount *mounted;
	GtkWindow *parent;
	BraseroIOPrivate *priv;
	BraseroIOMount mount = { NULL, };
	GMountOperation *operation = NULL;

	priv = BRASERO_IO_PRIVATE (self);

	if (priv->win_callback) {
		parent = priv->win_callback (priv->win_user_data);

		if (parent)
			operation = gtk_mount_operation_new (parent);
	}

	g_file_mount_enclosing_volume (file,
				       G_MOUNT_MOUNT_NONE,
				       operation,
				       cancel,
				       brasero_io_mount_enclosing_volume_cb,
				       &mount);
	if (operation)
		g_object_unref (operation);

	/* sleep and wait operation end */
	while (!mount.finished && !g_cancellable_is_cancelled (cancel))
		sleep (1);

	mounted = g_file_find_enclosing_mount (file, cancel, NULL);
	if (mounted) {
		BraseroIOPrivate *priv;

		priv = BRASERO_IO_PRIVATE (self);

		/* Keep these for later to unmount them */
		if (mount.result) {
			g_mutex_lock (priv->lock);
			priv->mounted = g_slist_prepend (priv->mounted, mounted);
			g_mutex_unlock (priv->lock);
		}
		else
			g_object_unref (mounted);
	}

	if (!mounted
	&&   mount.error
	&&  !g_cancellable_is_cancelled (cancel))
		g_propagate_error (error, mount.error);
	else if (mount.error)
		g_error_free (mount.error);

	BRASERO_UTILS_LOG ("Parent volume is %s",
			   (mounted != NULL && !g_cancellable_is_cancelled (cancel))?
			   "mounted":"not mounted");

	return (mounted != NULL && !g_cancellable_is_cancelled (cancel));
}

/**
 * This part deals with symlinks, that allows to get unique filenames by
 * replacing any parent symlink by its target and check for recursive
 * symlinks
 */

static gchar *
brasero_io_check_for_parent_symlink (const gchar *escaped_uri,
				     GCancellable *cancel)
{
	GFile *parent;
	GFile *file;
    	gchar *uri;

	/* don't check if the node itself is a symlink since that'll be done */
	file = g_file_new_for_uri (escaped_uri);
    	uri = g_file_get_uri (file);
	parent = g_file_get_parent (file);
	g_object_unref (file);

	while (parent) {
	    	GFile *tmp;
		GFileInfo *info;

		info = g_file_query_info (parent,
					  G_FILE_ATTRIBUTE_STANDARD_TYPE ","
					  G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK ","
					  G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET,
					  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,	/* don't follow symlinks */
					  NULL,
					  NULL);
		if (!info)
		    	break;

		/* NOTE: no need to check for broken symlinks since
		 * we wouldn't have reached this point otherwise */
		if (g_file_info_get_is_symlink (info)) {
			const gchar *target_path;
		    	gchar *parent_uri;
		    	gchar *new_root;
			gchar *newuri;

		    	parent_uri = g_file_get_uri (parent);
			target_path = g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET);

			/* check if this is not a relative path */
			 if (!g_path_is_absolute (target_path)) {
				gchar *tmp;

				tmp = g_path_get_dirname (parent_uri);
				new_root = g_build_path (G_DIR_SEPARATOR_S,
							 tmp,
							 target_path,
							 NULL);
				g_free (tmp);
			}
			else
				new_root = g_filename_to_uri (target_path, NULL, NULL);

			newuri = g_build_path (G_DIR_SEPARATOR_S,
					       new_root,
					       uri + strlen (parent_uri),
					       NULL);
		    	g_free (uri);
		    	uri = newuri;	

		    	g_object_unref (parent);
		    	g_free (parent_uri);

		    	parent = g_file_new_for_uri (new_root);
			g_free (new_root);
		}

		tmp = parent;
		parent = g_file_get_parent (parent);
		g_object_unref (tmp);

		g_object_unref (info);
	}

	if (parent)
		g_object_unref (parent);

	return uri;
}

static gchar *
brasero_io_get_uri_from_path (GFile *file,
			      const gchar *path)
{
	gchar *uri;

	if (!g_path_is_absolute (path))
		file = g_file_resolve_relative_path (file, path);
	else
		file = g_file_new_for_path (path);

	if (!file)
		return NULL;

	uri = g_file_get_uri (file);
	g_object_unref (file);
	return uri;
}

static gboolean
brasero_io_check_symlink_target (GFile *parent,
				 GFileInfo *info)
{
	const gchar *target;
	gchar *target_uri;
	guint size;
	gchar *uri;

	target = g_file_info_get_symlink_target (info);
    	if (!target)
		return FALSE;

	target_uri = brasero_io_get_uri_from_path (parent, target);
	if (!target_uri)
		return FALSE;

	/* we check for circular dependency here :
	 * if the target is one of the parent of symlink */
	size = strlen (target_uri);
	uri = g_file_get_uri (parent);

	if (!strncmp (target_uri, uri, size)
	&& (*(uri + size) == '/' || *(uri + size) == '\0')) {
		g_free (target_uri);
		g_free (uri);
		return FALSE;
	}
	g_free (uri);

	g_file_info_set_symlink_target (info, target_uri);
	g_free (target_uri);

	return TRUE;
}

/**
 * Used to retrieve metadata for audio files
 */

struct _BraserIOMetadataTask {
	gchar *uri;
	GSList *results;
};
typedef struct _BraseroIOMetadataTask BraseroIOMetadataTask;

struct _BraseroIOMetadataCached {
	guint64 last_modified;
	BraseroMetadataInfo *info;

	guint missing_codec_used:1;
};
typedef struct _BraseroIOMetadataCached BraseroIOMetadataCached;

static gint
brasero_io_metadata_lookup_buffer (gconstpointer a, gconstpointer b)
{
	const BraseroIOMetadataCached *cached = a;
	const gchar *uri = b;

	return strcmp (uri, cached->info->uri);
}

static void
brasero_io_metadata_cached_free (BraseroIOMetadataCached *cached)
{
	brasero_metadata_info_free (cached->info);
	g_free (cached);
}

static void
brasero_io_set_metadata_attributes (GFileInfo *info,
				    BraseroMetadataInfo *metadata)
{
	g_file_info_set_attribute_uint64 (info, BRASERO_IO_LEN, metadata->len);

	if (metadata->type)
		g_file_info_set_content_type (info, metadata->type);

	if (metadata->artist)
		g_file_info_set_attribute_string (info, BRASERO_IO_ARTIST, metadata->artist);

	if (metadata->title)
		g_file_info_set_attribute_string (info, BRASERO_IO_TITLE, metadata->title);

	if (metadata->album)
		g_file_info_set_attribute_string (info, BRASERO_IO_ALBUM, metadata->album);

	if (metadata->genre)
		g_file_info_set_attribute_string (info, BRASERO_IO_GENRE, metadata->genre);

	if (metadata->composer)
		g_file_info_set_attribute_string (info, BRASERO_IO_COMPOSER, metadata->composer);

	if (metadata->isrc)
		g_file_info_set_attribute_string (info, BRASERO_IO_ISRC, metadata->isrc);

	g_file_info_set_attribute_boolean (info, BRASERO_IO_HAS_AUDIO, metadata->has_audio);
	if (metadata->has_audio) {
		if (metadata->channels)
			g_file_info_set_attribute_int32 (info, BRASERO_IO_CHANNELS, metadata->channels);

		if (metadata->rate)
			g_file_info_set_attribute_int32 (info, BRASERO_IO_RATE, metadata->rate);

		if (metadata->has_dts)
			g_file_info_set_attribute_boolean (info, BRASERO_IO_HAS_DTS, TRUE);
	}

	g_file_info_set_attribute_boolean (info, BRASERO_IO_HAS_VIDEO, metadata->has_video);
	g_file_info_set_attribute_boolean (info, BRASERO_IO_IS_SEEKABLE, metadata->is_seekable);
	if (metadata->snapshot)
		g_file_info_set_attribute_object (info, BRASERO_IO_THUMBNAIL, G_OBJECT (metadata->snapshot));

	/* FIXME: what about silences */
}

static BraseroMetadata *
brasero_io_find_metadata (BraseroIO *self,
			  GCancellable *cancel,
			  const gchar *uri,
			  BraseroMetadataFlag flags,
			  GError **error)
{
	GSList *iter;
	BraseroIOPrivate *priv;
	BraseroMetadata *metadata;

	priv = BRASERO_IO_PRIVATE (self);

	BRASERO_UTILS_LOG ("Retrieving available metadata %s", uri);

	/* First see if a metadata is running with the same uri and the same
	 * flags as us. In this case, acquire the lock and wait for the lock
	 * to become available which will mean that it has finished
	 * NOTE: since we will hold the lock another thread waiting to get 
	 * an available metadata won't be able to have this one. */
	for (iter = priv->metadata_running; iter; iter = iter->next) {
		const gchar *metadata_uri;
		BraseroMetadataFlag metadata_flags;

		metadata = iter->data;
		metadata_uri = brasero_metadata_get_uri (metadata);
		if (!metadata_uri) {
			/* It could a metadata that was running but failed to
			 * retrieve anything and is waiting to be inserted back
			 * in the available list. Ignore it. */
			continue;
		}

		metadata_flags = brasero_metadata_get_flags (metadata);

		if (((flags & metadata_flags) == flags)
		&&  !strcmp (uri, metadata_uri)) {
			/* Found one: release the IO lock to let other threads
			 * do what they need to do then lock the metadata lock
			 * Let the thread that got the lock first move it back
			 * to waiting queue */
			BRASERO_UTILS_LOG ("Already ongoing search for %s", uri);
			brasero_metadata_increase_listener_number (metadata);
			return metadata;
		}
	}

	/* Grab an available metadata (NOTE: there should always be at least one
	 * since we run 2 threads at max and have two metadatas available) */
	while (!priv->metadatas) {
		if (g_cancellable_is_cancelled (cancel))
			return NULL;

		g_mutex_unlock (priv->lock_metadata);
		g_usleep (250);
		g_mutex_lock (priv->lock_metadata);
	}

	/* One metadata is finally available */
	metadata = priv->metadatas->data;

	/* Try to set it up for running */
	if (!brasero_metadata_set_uri (metadata, flags, uri, error))
		return NULL;

	/* The metadata is ready for running put it in right queue */
	brasero_metadata_increase_listener_number (metadata);
	priv->metadatas = g_slist_remove (priv->metadatas, metadata);
	priv->metadata_running = g_slist_prepend (priv->metadata_running, metadata);

	return metadata;
}

static gboolean
brasero_io_wait_for_metadata (BraseroIO *self,
			      GCancellable *cancel,
			      GFileInfo *info,
			      BraseroMetadata *metadata,
			      BraseroMetadataFlag flags,
			      BraseroMetadataInfo *meta_info)
{
	gboolean result;
	gboolean is_last;
	BraseroIOPrivate *priv;

	priv = BRASERO_IO_PRIVATE (self);

	brasero_metadata_wait (metadata, cancel);
	g_mutex_lock (priv->lock_metadata);

	is_last = brasero_metadata_decrease_listener_number (metadata);

	if (!g_cancellable_is_cancelled (cancel))
		result = brasero_metadata_get_result (metadata, meta_info, NULL);
	else
		result = FALSE;

	/* Only the last thread waiting for the result will put metadata back
	 * into the available queue and cache the results. */
	if (!is_last) {
		g_mutex_unlock (priv->lock_metadata);
		return result;
	}

	if (result) {
		/* see if we should add it to the buffer */
		if (meta_info->has_audio || meta_info->has_video) {
			BraseroIOMetadataCached *cached;

			cached = g_new0 (BraseroIOMetadataCached, 1);
			cached->last_modified = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_STANDARD_SIZE);

			cached->info = g_new0 (BraseroMetadataInfo, 1);
			brasero_metadata_get_result (metadata, cached->info, NULL);

			cached->missing_codec_used = (flags & BRASERO_METADATA_FLAG_MISSING) != 0;

			g_queue_push_head (priv->meta_buffer, cached);
			if (g_queue_get_length (priv->meta_buffer) > MAX_BUFFERED_META) {
				cached = g_queue_pop_tail (priv->meta_buffer);
				brasero_io_metadata_cached_free (cached);
			}
		}
	}

	/* Make sure it is stopped */
	BRASERO_UTILS_LOG ("Stopping metadata information retrieval (%p)", metadata);
	brasero_metadata_cancel (metadata);

	priv->metadata_running = g_slist_remove (priv->metadata_running, metadata);
	priv->metadatas = g_slist_append (priv->metadatas, metadata);

	g_mutex_unlock (priv->lock_metadata);

	return result;
}

static gboolean
brasero_io_get_metadata_info (BraseroIO *self,
			      GCancellable *cancel,
			      const gchar *uri,
			      GFileInfo *info,
			      BraseroMetadataFlag flags,
			      BraseroMetadataInfo *meta_info)
{
	BraseroMetadata *metadata = NULL;
	BraseroIOPrivate *priv;
	const gchar *mime;
	GList *node;

	if (g_cancellable_is_cancelled (cancel))
		return FALSE;

	priv = BRASERO_IO_PRIVATE (self);

	mime = g_file_info_get_content_type (info);
	BRASERO_UTILS_LOG ("Found file with type %s", mime);

	if (mime
	&& (!strncmp (mime, "image/", 6)
	||  !strcmp (mime, "text/plain")
	||  !strcmp (mime, "application/x-cue") /* this one make gstreamer crash */
	||  !strcmp (mime, "application/vnd.efi.iso")
	||  !strcmp (mime, "application/x-cd-image")))
		return FALSE;

	BRASERO_UTILS_LOG ("Retrieving metadata info");
	g_mutex_lock (priv->lock_metadata);

	/* Seek in the buffer if we have already explored these metadata. Check 
	 * the info last modified time in case a result should be updated. */
	node = g_queue_find_custom (priv->meta_buffer,
				    uri,
				    brasero_io_metadata_lookup_buffer);
	if (node) {
		guint64 last_modified;
		BraseroIOMetadataCached *cached;

		cached = node->data;
		last_modified = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_STANDARD_SIZE);
		if (last_modified == cached->last_modified) {
			gboolean refresh_cache = FALSE;

			if (flags & BRASERO_METADATA_FLAG_MISSING) {
				/* This cached result may indicate an error and
				 * this error could be related to the fact that
				 * it was not first looked for with missing
				 * codec detection. */
				if (!cached->missing_codec_used)
					refresh_cache = TRUE;
			}

			if (flags & BRASERO_METADATA_FLAG_THUMBNAIL) {
				/* If there isn't any snapshot retry */
				if (!cached->info->snapshot)
					refresh_cache = TRUE;
			}

			if (!refresh_cache) {
				brasero_metadata_info_copy (meta_info, cached->info);
				g_mutex_unlock (priv->lock_metadata);
				return TRUE;
			}
		}

		/* Found the same URI but it didn't have all required flags so
		 * we'll get another metadata information; Remove it from the
		 * queue => no same URI twice */
		g_queue_remove (priv->meta_buffer, cached);
		brasero_io_metadata_cached_free (cached);

		BRASERO_UTILS_LOG ("Updating cache information for %s", uri);
	}

	/* Find a metadata */
	metadata = brasero_io_find_metadata (self, cancel, uri, flags, NULL);
	g_mutex_unlock (priv->lock_metadata);

	if (!metadata)
		return FALSE;

	return brasero_io_wait_for_metadata (self,
					     cancel,
					     info,
					     metadata,
					     flags,
					     meta_info);
}

/**
 * Used to get information about files
 */

static GFileInfo *
brasero_io_get_file_info_thread_real (BraseroAsyncTaskManager *manager,
				      GCancellable *cancel,
				      GFile *file,
				      BraseroIOFlags options,
				      GError **error)
{
	gchar attributes [256] = {G_FILE_ATTRIBUTE_STANDARD_NAME ","
				  G_FILE_ATTRIBUTE_STANDARD_SIZE ","
				  G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK ","
				  G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET ","
				  G_FILE_ATTRIBUTE_STANDARD_TYPE};
	GError *local_error = NULL;
	gboolean should_thumbnail;
	GFileInfo *info;

	if (g_cancellable_is_cancelled (cancel))
		return NULL;

	if (options & BRASERO_IO_INFO_PERM)
		strcat (attributes, "," G_FILE_ATTRIBUTE_ACCESS_CAN_READ);
	if (options & BRASERO_IO_INFO_MIME)
		strcat (attributes, "," G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
	if (options & BRASERO_IO_INFO_ICON)
		strcat (attributes, "," G_FILE_ATTRIBUTE_STANDARD_ICON);
	if (options & BRASERO_IO_INFO_METADATA_THUMBNAIL)
		strcat (attributes, "," G_FILE_ATTRIBUTE_THUMBNAIL_PATH);

	/* if retrieving metadata we need this one to check if a possible result
	 * in cache should be updated or used */
	if (options & BRASERO_IO_INFO_METADATA)
		strcat (attributes, "," G_FILE_ATTRIBUTE_STANDARD_SIZE);

	info = g_file_query_info (file,
				  attributes,
				  (options & BRASERO_IO_INFO_FOLLOW_SYMLINK)?G_FILE_QUERY_INFO_NONE:G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,	/* follow symlinks by default*/
				  cancel,
				  &local_error);
	if (!info) {
		if (local_error && local_error->code == G_IO_ERROR_NOT_MOUNTED) {
			gboolean res;

			BRASERO_UTILS_LOG ("Starting to mount parent volume");
			g_error_free (local_error);
			local_error = NULL;

			/* try to mount whatever has to be mounted */
			/* NOTE: of course, we block a thread but one advantage
			 * is that we won't have many queries to mount the same
			 * remote volume at the same time. */
			res = brasero_io_mount_enclosing_volume (BRASERO_IO (manager),
								 file,
								 cancel,
								 error);
			if (!res)
				return NULL;

			return brasero_io_get_file_info_thread_real (manager,
								     cancel,
								     file,
								     options,
								     error);
		}

		g_propagate_error (error, local_error);
		return NULL;
	}

	if (g_file_info_get_is_symlink (info)) {
		GFile *parent;

		parent = g_file_get_parent (file);
		if (!brasero_io_check_symlink_target (parent, info)) {
			g_set_error (error,
				     BRASERO_UTILS_ERROR,
				     BRASERO_UTILS_ERROR_SYMLINK_LOOP,
				     _("Recursive symbolic link"));

			g_object_unref (info);
			g_object_unref (file);
			g_object_unref (parent);
			return NULL;
		}
		g_object_unref (parent);
	}

	should_thumbnail = FALSE;
	if (options & BRASERO_IO_INFO_METADATA_THUMBNAIL) {
		const gchar *path;

		path = g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);
		if (path) {
			GdkPixbuf *pixbuf;

			pixbuf = gdk_pixbuf_new_from_file (path, NULL);
			if (pixbuf) {
				g_file_info_set_attribute_object (info,
								  BRASERO_IO_THUMBNAIL,
								  G_OBJECT (pixbuf));
				g_object_unref (pixbuf);
			}
			else
				should_thumbnail = TRUE;
		}
		else if (!g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_THUMBNAILING_FAILED))
			should_thumbnail = TRUE;
	}

	/* see if we are supposed to get metadata for this file (provided it's
	 * an audio file of course). */
	if ((g_file_info_get_file_type (info) == G_FILE_TYPE_REGULAR
	||   g_file_info_get_file_type (info) == G_FILE_TYPE_SYMBOLIC_LINK)
	&&  (options & BRASERO_IO_INFO_METADATA)) {
		BraseroMetadataInfo metadata = { NULL };
		BraseroMetadataFlag flags;
		gboolean result;
		gchar *uri;

		flags = ((options & BRASERO_IO_INFO_METADATA_MISSING_CODEC) ? BRASERO_METADATA_FLAG_MISSING : 0)|
			((should_thumbnail) ? BRASERO_METADATA_FLAG_THUMBNAIL : 0);

		uri = g_file_get_uri (file);
		result = brasero_io_get_metadata_info (BRASERO_IO (manager),
						       cancel,
						       uri,
						       info,
						       flags,
						       &metadata);
		g_free (uri);

		if (result)
			brasero_io_set_metadata_attributes (info, &metadata);

		brasero_metadata_info_clear (&metadata);
	}

	return info;
}

static BraseroAsyncTaskResult
brasero_io_get_file_info_thread (BraseroAsyncTaskManager *manager,
				 GCancellable *cancel,
				 gpointer callback_data)
{
	BraseroIOJob *job = callback_data;
	gchar *file_uri = NULL;
	GError *error = NULL;
	GFileInfo *info;
	GFile *file;

	if (job->options & BRASERO_IO_INFO_CHECK_PARENT_SYMLINK) {
		/* If we want to make sure a directory is not added twice we have to make sure
		 * that it doesn't have a symlink as parent otherwise "/home/Foo/Bar" with Foo
		 * as a symlink pointing to /tmp would be seen as a different file from /tmp/Bar 
		 * It would be much better if we could use the inode numbers provided by gnome_vfs
		 * unfortunately they are guint64 and can't be used in hash tables as keys.
		 * Therefore we check parents up to root to see if there are symlinks and if so
		 * we get a path without symlinks in it. This is done only for local file */
		file_uri = brasero_io_check_for_parent_symlink (job->uri, cancel);
	}

	if (g_cancellable_is_cancelled (cancel)) {
		g_free (file_uri);
		return BRASERO_ASYNC_TASK_FINISHED;
	}

	file = g_file_new_for_uri (file_uri?file_uri:job->uri);
	info = brasero_io_get_file_info_thread_real (manager,
						     cancel,
						     file,
						     job->options,
						     &error);

	/* do this to have a very nice URI:
	 * for example: file://pouet instead of file://../directory/pouet */
	g_free (file_uri);
	file_uri = g_file_get_uri (file);
	g_object_unref (file);

	brasero_io_return_result (job->base,
				  file_uri,
				  info,
				  error,
				  job->callback_data);

	g_free (file_uri);
	return BRASERO_ASYNC_TASK_FINISHED;
}

static const BraseroAsyncTaskType info_type = {
	brasero_io_get_file_info_thread,
	brasero_io_job_destroy
};

static void
brasero_io_new_file_info_job (const gchar *uri,
			      const BraseroIOJobBase *base,
			      BraseroIOFlags options,
			      BraseroIOResultCallbackData *callback_data)
{
	BraseroIOJob *job;

	job = g_new0 (BraseroIOJob, 1);
	brasero_io_set_job (job,
			    base,
			    uri,
			    options,
			    callback_data);

	brasero_io_push_job (job, &info_type);
}

void
brasero_io_get_file_info (const gchar *uri,
			  const BraseroIOJobBase *base,
			  BraseroIOFlags options,
			  gpointer user_data)
{
	BraseroIOResultCallbackData *callback_data = NULL;
	BraseroIO *self = brasero_io_get_default ();

	if (user_data) {
		callback_data = g_new0 (BraseroIOResultCallbackData, 1);
		callback_data->callback_data = user_data;
	}

	brasero_io_new_file_info_job (uri, base, options, callback_data);
	g_object_unref (self);
}

/**
 * Used to parse playlists
 */

#ifdef BUILD_PLAYLIST

struct _BraseroIOPlaylist {
	gchar *title;
	GSList *uris;
};
typedef struct _BraseroIOPlaylist BraseroIOPlaylist;

static void
brasero_io_playlist_clear (BraseroIOPlaylist *data)
{
	g_slist_foreach (data->uris, (GFunc) g_free, NULL);
	g_slist_free (data->uris);

	g_free (data->title);
}

static void
brasero_io_add_playlist_entry_parsed_cb (TotemPlParser *parser,
					 const gchar *uri,
					 GHashTable *metadata,
					 BraseroIOPlaylist *data)
{
	data->uris = g_slist_prepend (data->uris, g_strdup (uri));
}

static void
brasero_io_start_playlist_cb (TotemPlParser *parser,
				  const gchar *uri,
				  GHashTable *metadata,
				  BraseroIOPlaylist *data)
{
	const gchar *title;

	title = g_hash_table_lookup (metadata, TOTEM_PL_PARSER_FIELD_TITLE);
	if (!title)
		return;

	if (!data->title)
		data->title = g_strdup (title);
}

static gboolean
brasero_io_parse_playlist_get_uris (const gchar *uri,
				    BraseroIOPlaylist *playlist,
				    GError **error)
{
	gboolean result;
	TotemPlParser *parser;

	parser = totem_pl_parser_new ();
	g_signal_connect (parser,
			  "playlist-started",
			  G_CALLBACK (brasero_io_start_playlist_cb),
			  playlist);
	g_signal_connect (parser,
			  "entry-parsed",
			  G_CALLBACK (brasero_io_add_playlist_entry_parsed_cb),
			  playlist);

	if (g_object_class_find_property (G_OBJECT_GET_CLASS (parser), "recurse"))
		g_object_set (G_OBJECT (parser), "recurse", FALSE, NULL);

	result = totem_pl_parser_parse (parser, uri, TRUE);
	g_object_unref (parser);

	if (!result) {
		g_set_error (error,
			     BRASERO_UTILS_ERROR,
			     BRASERO_UTILS_ERROR_GENERAL,
			     _("The file does not appear to be a playlist"));

		return FALSE;
	}

	return TRUE;
}

static BraseroAsyncTaskResult
brasero_io_parse_playlist_thread (BraseroAsyncTaskManager *manager,
				  GCancellable *cancel,
				  gpointer callback_data)
{
	GSList *iter;
	gboolean result;
	GFileInfo *info;
	GError *error = NULL;
	BraseroIOJob *job = callback_data;
	BraseroIOPlaylist data = { NULL, };

	result = brasero_io_parse_playlist_get_uris (job->uri, &data, &error);
	if (!result) {
		brasero_io_return_result (job->base,
					  job->uri,
					  NULL,
					  error,
					  job->callback_data);
		return BRASERO_ASYNC_TASK_FINISHED;
	}

	if (g_cancellable_is_cancelled (cancel))
		return BRASERO_ASYNC_TASK_FINISHED;

	/* that's finished; Send the title */
	info = g_file_info_new ();
	g_file_info_set_attribute_boolean (info,
					   BRASERO_IO_IS_PLAYLIST,
					   TRUE);
	g_file_info_set_attribute_uint32 (info,
					  BRASERO_IO_PLAYLIST_ENTRIES_NUM,
					  g_slist_length (data.uris));
	if (data.title)
		g_file_info_set_attribute_string (info,
						  BRASERO_IO_PLAYLIST_TITLE,
						  data.title);

	brasero_io_return_result (job->base,
				  job->uri,
				  info,
				  NULL,
				  job->callback_data);

	/* Now get information about each file in the list. 
	 * Reverse order of list to get a correct order for entries. */
	data.uris = g_slist_reverse (data.uris);
	for (iter = data.uris; iter; iter = iter->next) {
		GFile *file;
		gchar *child;
		GFileInfo *child_info;

		child = iter->data;
		if (g_cancellable_is_cancelled (cancel))
			break;

		file = g_file_new_for_uri (child);
		child_info = brasero_io_get_file_info_thread_real (manager,
								   cancel,
								   file,
								   job->options,
								   NULL);
		g_object_unref (file);

		if (!child_info)
			continue;

		brasero_io_return_result (job->base,
					  child,
					  child_info,
					  NULL,
					  job->callback_data);
	}

	brasero_io_playlist_clear (&data);
	return BRASERO_ASYNC_TASK_FINISHED;
}

static const BraseroAsyncTaskType playlist_type = {
	brasero_io_parse_playlist_thread,
	brasero_io_job_destroy
};

void
brasero_io_parse_playlist (const gchar *uri,
			   const BraseroIOJobBase *base,
			   BraseroIOFlags options,
			   gpointer user_data)
{
	BraseroIOJob *job;
	BraseroIO *self = brasero_io_get_default ();
	BraseroIOResultCallbackData *callback_data = NULL;

	if (user_data) {
		callback_data = g_new0 (BraseroIOResultCallbackData, 1);
		callback_data->callback_data = user_data;
	}

	job = g_new0 (BraseroIOJob, 1);
	brasero_io_set_job (job,
			    base,
			    uri,
			    options,
			    callback_data);

	brasero_io_push_job (job, &playlist_type);
	g_object_unref (self);
}

#endif

/**
 * Used to count the number of files under a directory and the children size
 */

struct _BraseroIOCountData {
	BraseroIOJob job;

	GSList *uris;
	GSList *children;

	guint files_num;
	guint files_invalid;

	guint64 total_b;
	gboolean progress_started;
};
typedef struct _BraseroIOCountData BraseroIOCountData;

static void
brasero_io_get_file_count_destroy (BraseroAsyncTaskManager *manager,
				   gboolean cancelled,
				   gpointer callback_data)
{
	BraseroIOCountData *data = callback_data;

	g_slist_foreach (data->uris, (GFunc) g_free, NULL);
	g_slist_free (data->uris);

	g_slist_foreach (data->children, (GFunc) g_object_unref, NULL);
	g_slist_free (data->children);

	brasero_io_job_progress_report_stop (BRASERO_IO (manager), callback_data);

	brasero_io_job_free (cancelled, callback_data);
}

#ifdef BUILD_PLAYLIST

static gboolean
brasero_io_get_file_count_process_playlist (BraseroIO *self,
					    GCancellable *cancel,
					    BraseroIOCountData *data,
					    const gchar *uri)
{
	BraseroIOPlaylist playlist = {NULL, };
	GSList *iter;

	if (!brasero_io_parse_playlist_get_uris (uri, &playlist, NULL))
		return FALSE;

	for (iter = playlist.uris; iter; iter = iter->next) {
		gboolean result;
		GFileInfo *info;
		gchar *child_uri;
		BraseroMetadataInfo metadata = { NULL, };

		child_uri = iter->data;
		data->files_num ++;

		info = g_file_info_new ();
		result = brasero_io_get_metadata_info (self,
						       cancel,
						       child_uri,
						       info,
						       ((data->job.options & BRASERO_IO_INFO_METADATA_MISSING_CODEC) ? BRASERO_METADATA_FLAG_MISSING : 0) |
						       ((data->job.options & BRASERO_IO_INFO_METADATA_THUMBNAIL) ? BRASERO_METADATA_FLAG_THUMBNAIL : 0),
						       &metadata);

		if (result)
			data->total_b += metadata.len;
		else
			data->files_invalid ++;

		brasero_metadata_info_clear (&metadata);
		g_object_unref (info);
	}

	brasero_io_playlist_clear (&playlist);
	return TRUE;
}

#endif 

static void
brasero_io_get_file_count_process_file (BraseroIO *self,
					GCancellable *cancel,
					BraseroIOCountData *data,
					GFile *file,
					GFileInfo *info)
{
	if (data->job.options & BRASERO_IO_INFO_METADATA) {
		BraseroMetadataInfo metadata = { NULL, };
		gboolean result = FALSE;
		gchar *child_uri;

		child_uri = g_file_get_uri (file);
		result = brasero_io_get_metadata_info (self,
						       cancel,
						       child_uri,
						       info,
						       ((data->job.options & BRASERO_IO_INFO_METADATA_MISSING_CODEC) ? BRASERO_METADATA_FLAG_MISSING : 0) |
						       ((data->job.options & BRASERO_IO_INFO_METADATA_THUMBNAIL) ? BRASERO_METADATA_FLAG_THUMBNAIL : 0),
						       &metadata);
		if (result)
			data->total_b += metadata.len;

#ifdef BUILD_PLAYLIST

		/* see if that's a playlist (and if we have recursive on). */
		else if (data->job.options & BRASERO_IO_INFO_RECURSIVE) {
			const gchar *mime;

			mime = g_file_info_get_content_type (info);
			if (mime
			&& (!strcmp (mime, "audio/x-scpls")
			||  !strcmp (mime, "audio/x-ms-asx")
			||  !strcmp (mime, "audio/x-mp3-playlist")
			||  !strcmp (mime, "audio/x-mpegurl"))) {
				if (!brasero_io_get_file_count_process_playlist (self, cancel, data, child_uri))
					data->files_invalid ++;
			}
			else
				data->files_invalid ++;
		}

#endif

		else
			data->files_invalid ++;

		brasero_metadata_info_clear (&metadata);
		g_free (child_uri);
		return;
	}

	data->total_b += g_file_info_get_size (info);
}

static void
brasero_io_get_file_count_process_directory (BraseroIO *self,
					     GCancellable *cancel,
					     BraseroIOCountData *data)
{
	GFile *file;
	GFileInfo *info;
	GError *error = NULL;
	GFileEnumerator *enumerator;
	gchar attributes [512] = {G_FILE_ATTRIBUTE_STANDARD_NAME "," 
				  G_FILE_ATTRIBUTE_STANDARD_SIZE "," 
				  G_FILE_ATTRIBUTE_STANDARD_TYPE };

	if ((data->job.options & BRASERO_IO_INFO_METADATA)
	&&  (data->job.options & BRASERO_IO_INFO_RECURSIVE))
		strcat (attributes, "," G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);

	file = data->children->data;
	data->children = g_slist_remove (data->children, file);

	enumerator = g_file_enumerate_children (file,
						attributes,
						(data->job.options & BRASERO_IO_INFO_FOLLOW_SYMLINK)?G_FILE_QUERY_INFO_NONE:G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,	/* follow symlinks by default*/
						cancel,
						NULL);
	if (!enumerator) {
		g_object_unref (file);
		return;
	}

	while ((info = g_file_enumerator_next_file (enumerator, cancel, &error)) || error) {
		GFile *child;

		if (g_cancellable_is_cancelled (cancel)) {
			g_object_unref (info);
			break;
		}

		data->files_num ++;

		if (error) {
			g_error_free (error);
			error = NULL;

			data->files_invalid ++;
			continue;
		}

		child = g_file_get_child (file, g_file_info_get_name (info));

		if (g_file_info_get_file_type (info) == G_FILE_TYPE_REGULAR
		||  g_file_info_get_file_type (info) == G_FILE_TYPE_SYMBOLIC_LINK) {
			brasero_io_get_file_count_process_file (self, cancel, data, child, info);
			g_object_unref (child);
		}
		else if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
			data->children = g_slist_prepend (data->children, child);
		else
			g_object_unref (child);

		g_object_unref (info);
	}

	g_file_enumerator_close (enumerator, cancel, NULL);
	g_object_unref (enumerator);
	g_object_unref (file);
}

static gboolean
brasero_io_get_file_count_start (BraseroIO *self,
				 GCancellable *cancel,
				 BraseroIOCountData *data,
				 const gchar *uri)
{
	GFile *file;
	GFileInfo *info;
	gchar attributes [512] = {G_FILE_ATTRIBUTE_STANDARD_NAME "," 
				  G_FILE_ATTRIBUTE_STANDARD_SIZE "," 
				  G_FILE_ATTRIBUTE_STANDARD_TYPE };

	if ((data->job.options & BRASERO_IO_INFO_METADATA)
	&&  (data->job.options & BRASERO_IO_INFO_RECURSIVE))
		strcat (attributes, "," G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);

	file = g_file_new_for_uri (uri);
	info = g_file_query_info (file,
				  attributes,
				  (data->job.options & BRASERO_IO_INFO_FOLLOW_SYMLINK)?G_FILE_QUERY_INFO_NONE:G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,	/* follow symlinks by default*/
				  cancel,
				  NULL);
	data->files_num ++;

	if (!info) {
		g_object_unref (file);
		data->files_invalid ++;
		return FALSE;
	}

	if (g_file_info_get_file_type (info) == G_FILE_TYPE_REGULAR
	||  g_file_info_get_file_type (info) == G_FILE_TYPE_SYMBOLIC_LINK) {
		brasero_io_get_file_count_process_file (self, cancel, data, file, info);
		g_object_unref (file);
	}
	else if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
		if (data->job.options & BRASERO_IO_INFO_RECURSIVE)
			data->children = g_slist_prepend (data->children, file);
		else
			g_object_unref (file);
	}
	else
		g_object_unref (file);

	g_object_unref (info);
	return TRUE;
}

static void
brasero_io_get_file_count_progress_cb (BraseroIOJob *job,
				       BraseroIOJobProgress *progress)
{
	BraseroIOCountData *data = (BraseroIOCountData *) job;

	progress->read_b = data->total_b;
	progress->total_b = data->total_b;
	progress->files_num = data->files_num;
	progress->files_invalid = data->files_invalid;
}

static BraseroAsyncTaskResult
brasero_io_get_file_count_thread (BraseroAsyncTaskManager *manager,
				  GCancellable *cancel,
				  gpointer callback_data)
{
	BraseroIOCountData *data = callback_data;
	GFileInfo *info;
	gchar *uri;

	if (data->children) {
		brasero_io_get_file_count_process_directory (BRASERO_IO (manager), cancel, data);
		return BRASERO_ASYNC_TASK_RESCHEDULE;
	}

	if (!data->uris) {
		info = g_file_info_new ();

		/* set GFileInfo information */
		g_file_info_set_attribute_uint32 (info, BRASERO_IO_COUNT_INVALID, data->files_invalid);
		g_file_info_set_attribute_uint64 (info, BRASERO_IO_COUNT_SIZE, data->total_b);
		g_file_info_set_attribute_uint32 (info, BRASERO_IO_COUNT_NUM, data->files_num);

		brasero_io_return_result (data->job.base,
					  NULL,
					  info,
					  NULL,
					  data->job.callback_data);

		return BRASERO_ASYNC_TASK_FINISHED;
	}

	if (!data->progress_started) {
		brasero_io_job_progress_report_start (BRASERO_IO (manager),
						      &data->job,
						      brasero_io_get_file_count_progress_cb);
		data->progress_started = 1;
	}

	uri = data->uris->data;
	data->uris = g_slist_remove (data->uris, uri);

	brasero_io_get_file_count_start (BRASERO_IO (manager), cancel, data, uri);
	g_free (uri);

	return BRASERO_ASYNC_TASK_RESCHEDULE;
}

static const BraseroAsyncTaskType count_type = {
	brasero_io_get_file_count_thread,
	brasero_io_get_file_count_destroy
};

void
brasero_io_get_file_count (GSList *uris,
			   const BraseroIOJobBase *base,
			   BraseroIOFlags options,
			   gpointer user_data)
{
	BraseroIOCountData *data;
	BraseroIO *self = brasero_io_get_default ();
	BraseroIOResultCallbackData *callback_data = NULL;

	if (user_data) {
		callback_data = g_new0 (BraseroIOResultCallbackData, 1);
		callback_data->callback_data = user_data;
	}

	data = g_new0 (BraseroIOCountData, 1);

	for (; uris; uris = uris->next)
		data->uris = g_slist_prepend (data->uris, g_strdup (uris->data));

	brasero_io_set_job (BRASERO_IO_JOB (data),
			    base,
			    NULL,
			    options,
			    callback_data);

	brasero_io_push_job (BRASERO_IO_JOB (data), &count_type);
	g_object_unref (self);
}

/**
 * Used to explore directories
 */

struct _BraseroIOContentsData {
	BraseroIOJob job;
	GSList *children;
};
typedef struct _BraseroIOContentsData BraseroIOContentsData;

static void
brasero_io_load_directory_destroy (BraseroAsyncTaskManager *manager,
				   gboolean cancelled,
				   gpointer callback_data)
{
	BraseroIOContentsData *data = callback_data;

	g_slist_foreach (data->children, (GFunc) g_object_unref, NULL);
	g_slist_free (data->children);

	brasero_io_job_free (cancelled, BRASERO_IO_JOB (data));
}

#ifdef BUILD_PLAYLIST

static gboolean
brasero_io_load_directory_playlist (BraseroIO *self,
				    GCancellable *cancel,
				    BraseroIOContentsData *data,
				    const gchar *uri,
				    const gchar *attributes)
{
	BraseroIOPlaylist playlist = {NULL, };
	GSList *iter;

	if (!brasero_io_parse_playlist_get_uris (uri, &playlist, NULL))
		return FALSE;

	for (iter = playlist.uris; iter; iter = iter->next) {
		GFile *file;
		gboolean result;
		GFileInfo *info;
		gchar *child_uri;
		BraseroMetadataInfo metadata = { NULL, };

		child_uri = iter->data;

		file = g_file_new_for_uri (child_uri);
		info = g_file_query_info (file,
					  attributes,
					  G_FILE_QUERY_INFO_NONE,		/* follow symlinks */
					  cancel,
					  NULL);
		if (!info) {
			g_object_unref (file);
			continue;
		}

		result = brasero_io_get_metadata_info (self,
						       cancel,
						       child_uri,
						       info,
						       ((data->job.options & BRASERO_IO_INFO_METADATA_MISSING_CODEC) ? BRASERO_METADATA_FLAG_MISSING : 0) |
						       ((data->job.options & BRASERO_IO_INFO_METADATA_THUMBNAIL) ? BRASERO_METADATA_FLAG_THUMBNAIL : 0),
						       &metadata);

		if (result) {
			brasero_io_set_metadata_attributes (info, &metadata);
			brasero_io_return_result (data->job.base,
						  child_uri,
						  info,
						  NULL,
						  data->job.callback_data);
		}
		else
			g_object_unref (info);

		brasero_metadata_info_clear (&metadata);

		g_object_unref (file);
	}

	brasero_io_playlist_clear (&playlist);
	return TRUE;
}

#endif

static BraseroAsyncTaskResult
brasero_io_load_directory_thread (BraseroAsyncTaskManager *manager,
				  GCancellable *cancel,
				  gpointer callback_data)
{
	gchar attributes [512] = {G_FILE_ATTRIBUTE_STANDARD_NAME "," 
				  G_FILE_ATTRIBUTE_STANDARD_SIZE ","
				  G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK ","
				  G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET ","
				  G_FILE_ATTRIBUTE_STANDARD_TYPE };
	BraseroIOContentsData *data = callback_data;
	GFileEnumerator *enumerator;
	GError *error = NULL;
	GFileInfo *info;
	GFile *file;

	if (data->job.options & BRASERO_IO_INFO_PERM)
		strcat (attributes, "," G_FILE_ATTRIBUTE_ACCESS_CAN_READ);

	if (data->job.options & BRASERO_IO_INFO_MIME)
		strcat (attributes, "," G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
	else if ((data->job.options & BRASERO_IO_INFO_METADATA)
	     &&  (data->job.options & BRASERO_IO_INFO_RECURSIVE))
		strcat (attributes, "," G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);

	if (data->job.options & BRASERO_IO_INFO_ICON)
		strcat (attributes, "," G_FILE_ATTRIBUTE_STANDARD_ICON);

	if (data->children) {
		file = data->children->data;
		data->children = g_slist_remove (data->children, file);
	}
	else
		file = g_file_new_for_uri (data->job.uri);

	enumerator = g_file_enumerate_children (file,
						attributes,
						(data->job.options & BRASERO_IO_INFO_FOLLOW_SYMLINK)?G_FILE_QUERY_INFO_NONE:G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,	/* follow symlinks by default*/
						cancel,
						&error);

	if (!enumerator) {
		gchar *directory_uri;

		directory_uri = g_file_get_uri (file);
		brasero_io_return_result (data->job.base,
					  directory_uri,
					  NULL,
					  error,
					  data->job.callback_data);
		g_free (directory_uri);
		g_object_unref (file);

		if (data->children)
			return BRASERO_ASYNC_TASK_RESCHEDULE;

		return BRASERO_ASYNC_TASK_FINISHED;
	}

	while ((info = g_file_enumerator_next_file (enumerator, cancel, NULL))) {
		const gchar *name;
		gchar *child_uri;
		GFile *child;

		name = g_file_info_get_name (info);
		if (g_cancellable_is_cancelled (cancel)) {
			g_object_unref (info);
			break;
		}

		if (name [0] == '.'
		&& (name [1] == '\0'
		|| (name [1] == '.' && name [2] == '\0'))) {
			g_object_unref (info);
			continue;
		}

		child = g_file_get_child (file, name);
		if (!child)
			continue;

		child_uri = g_file_get_uri (child);

		/* special case for symlinks */
		if (g_file_info_get_is_symlink (info)) {
			if (!brasero_io_check_symlink_target (file, info)) {
				error = g_error_new (BRASERO_UTILS_ERROR,
						     BRASERO_UTILS_ERROR_SYMLINK_LOOP,
						     _("Recursive symbolic link"));

				/* since we checked for the existence of the file
				 * an error means a looping symbolic link */
				brasero_io_return_result (data->job.base,
							  child_uri,
							  NULL,
							  error,
							  data->job.callback_data);

				g_free (child_uri);
				g_object_unref (info);
				g_object_unref (child);
				continue;
			}
		}

		if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
			brasero_io_return_result (data->job.base,
						  child_uri,
						  info,
						  NULL,
						  data->job.callback_data);

			if (data->job.options & BRASERO_IO_INFO_RECURSIVE)
				data->children = g_slist_prepend (data->children, child);
			else
				g_object_unref (child);

			g_free (child_uri);
			continue;
		}

		if (data->job.options & BRASERO_IO_INFO_METADATA) {
			BraseroMetadataInfo metadata = {NULL, };
			gboolean result;

			/* add metadata information to this file */
			result = brasero_io_get_metadata_info (BRASERO_IO (manager),
							       cancel,
							       child_uri,
							       info,
							       ((data->job.options & BRASERO_IO_INFO_METADATA_MISSING_CODEC) ? BRASERO_METADATA_FLAG_MISSING : 0) |
							       ((data->job.options & BRASERO_IO_INFO_METADATA_THUMBNAIL) ? BRASERO_METADATA_FLAG_THUMBNAIL : 0),
							       &metadata);

			if (result)
				brasero_io_set_metadata_attributes (info, &metadata);

#ifdef BUILD_PLAYLIST

			else if (data->job.options & BRASERO_IO_INFO_RECURSIVE) {
				const gchar *mime;

				mime = g_file_info_get_content_type (info);
				if (mime
				&& (!strcmp (mime, "audio/x-scpls")
				||  !strcmp (mime, "audio/x-ms-asx")
				||  !strcmp (mime, "audio/x-mp3-playlist")
				||  !strcmp (mime, "audio/x-mpegurl")))
					brasero_io_load_directory_playlist (BRASERO_IO (manager),
									    cancel,
									    data,
									    child_uri,
									    attributes);
			}

#endif

			brasero_metadata_info_clear (&metadata);
		}

		brasero_io_return_result (data->job.base,
					  child_uri,
					  info,
					  NULL,
					  data->job.callback_data);
		g_free (child_uri);
		g_object_unref (child);
	}

	g_file_enumerator_close (enumerator, NULL, NULL);
	g_object_unref (enumerator);
	g_object_unref (file);

	if (data->children)
		return BRASERO_ASYNC_TASK_RESCHEDULE;

	return BRASERO_ASYNC_TASK_FINISHED;
}

static const BraseroAsyncTaskType contents_type = {
	brasero_io_load_directory_thread,
	brasero_io_load_directory_destroy
};

void
brasero_io_load_directory (const gchar *uri,
			   const BraseroIOJobBase *base,
			   BraseroIOFlags options,
			   gpointer user_data)
{
	BraseroIOContentsData *data;
	BraseroIO *self = brasero_io_get_default ();
	BraseroIOResultCallbackData *callback_data = NULL;

	if (user_data) {
		callback_data = g_new0 (BraseroIOResultCallbackData, 1);
		callback_data->callback_data = user_data;
	}

	data = g_new0 (BraseroIOContentsData, 1);
	brasero_io_set_job (BRASERO_IO_JOB (data),
			    base,
			    uri,
			    options,
			    callback_data);

	brasero_io_push_job (BRASERO_IO_JOB (data), &contents_type);
	g_object_unref (self);
}

static void
brasero_io_cancel_result (BraseroIO *self,
			  BraseroIOJobResult *result)
{
	BraseroIOResultCallbackData *data;
	BraseroIOPrivate *priv;

	priv = BRASERO_IO_PRIVATE (self);

	g_mutex_lock (priv->lock);
	priv->results = g_slist_remove (priv->results, result);
	g_mutex_unlock (priv->lock);

	data = result->callback_data;
	brasero_io_unref_result_callback_data (data,
					       result->base->object,
					       result->base->methods->destroy,
					       TRUE);
	brasero_io_job_result_free (result);
}

static gboolean
brasero_io_cancel_tasks_by_base_cb (BraseroAsyncTaskManager *manager,
				    gpointer callback_data,
				    gpointer user_data)
{
	BraseroIOJob *job = callback_data;
	BraseroIOJobBase *base = user_data;

	if (job->base != base)
		return FALSE;

	return TRUE;
}

void
brasero_io_cancel_by_base (BraseroIOJobBase *base)
{
	GSList *iter;
	GSList *next;
	BraseroIOPrivate *priv;
	BraseroIO *self = brasero_io_get_default ();

	priv = BRASERO_IO_PRIVATE (self);

	brasero_async_task_manager_foreach_unprocessed_remove (BRASERO_ASYNC_TASK_MANAGER (self),
							       brasero_io_cancel_tasks_by_base_cb,
							       base);

	brasero_async_task_manager_foreach_active_remove (BRASERO_ASYNC_TASK_MANAGER (self),
							  brasero_io_cancel_tasks_by_base_cb,
							  base);

	/* do it afterwards in case some results slipped through */
	for (iter = priv->results; iter; iter = next) {
		BraseroIOJobResult *result;

		result = iter->data;
		next = iter->next;

		if (result->base != base)
			continue;

		brasero_io_cancel_result (self, result);
	}

	g_object_unref (self);
}

struct _BraseroIOJobCompareData {
	BraseroIOCompareCallback func;
	const BraseroIOJobBase *base;
	gpointer user_data;
};
typedef struct _BraseroIOJobCompareData BraseroIOJobCompareData;

static gboolean
brasero_io_compare_unprocessed_task (BraseroAsyncTaskManager *manager,
				     gpointer task,
				     gpointer callback_data)
{
	BraseroIOJob *job = task;
	BraseroIOJobCompareData *data = callback_data;

	if (job->base == data->base)
		return FALSE;

	if (!job->callback_data)
		return FALSE;

	return data->func (job->callback_data->callback_data, data->user_data);
}

void
brasero_io_find_urgent (const BraseroIOJobBase *base,
			BraseroIOCompareCallback callback,
			gpointer user_data)
{
	BraseroIOJobCompareData callback_data;
	BraseroIO *self = brasero_io_get_default ();

	callback_data.func = callback;
	callback_data.base = base;
	callback_data.user_data = user_data;

	brasero_async_task_manager_find_urgent_task (BRASERO_ASYNC_TASK_MANAGER (self),
						     brasero_io_compare_unprocessed_task,
						     &callback_data);
	g_object_unref (self);
						     
}

BraseroIOJobCallbacks *
brasero_io_register_job_methods (BraseroIOResultCallback callback,
                                 BraseroIODestroyCallback destroy,
                                 BraseroIOProgressCallback progress)
{
	BraseroIOJobCallbacks *methods;

	methods = g_new0 (BraseroIOJobCallbacks, 1);
	methods->callback = callback;
	methods->destroy = destroy;
	methods->progress = progress;

	return methods;
}

BraseroIOJobBase *
brasero_io_register_with_methods (GObject *object,
                                  BraseroIOJobCallbacks *methods)
{
	BraseroIOJobBase *base;

	base = g_new0 (BraseroIOJobBase, 1);
	base->object = object;
	base->methods = methods;
	methods->ref ++;

	return base;
}

BraseroIOJobBase *
brasero_io_register (GObject *object,
		     BraseroIOResultCallback callback,
		     BraseroIODestroyCallback destroy,
		     BraseroIOProgressCallback progress)
{
	return brasero_io_register_with_methods (object, brasero_io_register_job_methods (callback, destroy, progress));
}

void
brasero_io_job_base_free (BraseroIOJobBase *base)
{
	BraseroIOJobCallbacks *methods;

	if (!base)
		return;

	methods = base->methods;
	g_free (base);

	methods->ref --;
	if (methods->ref <= 0)
		g_free (methods);
}

static int
 brasero_io_xid_for_metadata (gpointer user_data)
{
	BraseroIOPrivate *priv;

	priv = BRASERO_IO_PRIVATE (user_data);
	if (priv->win_callback) {
		int xid;
		GtkWindow *parent;

		parent = priv->win_callback (priv->win_user_data);
		xid = gdk_x11_window_get_xid (gtk_widget_get_window(GTK_WIDGET (parent)));
		return xid;
	}

	return 0;
}

static void
brasero_io_init (BraseroIO *object)
{
	BraseroIOPrivate *priv;
	BraseroMetadata *metadata;
	priv = BRASERO_IO_PRIVATE (object);

	priv->lock = g_mutex_new ();
	priv->lock_metadata = g_mutex_new ();

	priv->meta_buffer = g_queue_new ();

	/* create metadatas now since it doesn't work well when it's created in 
	 * a thread. */
	metadata = brasero_metadata_new ();
	priv->metadatas = g_slist_prepend (priv->metadatas, metadata);
	brasero_metadata_set_get_xid_callback (metadata, brasero_io_xid_for_metadata, object);
	metadata = brasero_metadata_new ();
	priv->metadatas = g_slist_prepend (priv->metadatas, metadata);
	brasero_metadata_set_get_xid_callback (metadata, brasero_io_xid_for_metadata, object);
}

static gboolean
brasero_io_free_async_queue (BraseroAsyncTaskManager *manager,
			     gpointer callback_data,
			     gpointer NULL_data)
{
	/* don't do anything here, the async task manager
	 * will destroy the job anyway.
	 */
	return TRUE;
}

static void
brasero_io_finalize (GObject *object)
{
	BraseroIOPrivate *priv;
	GSList *iter;

	priv = BRASERO_IO_PRIVATE (object);

	brasero_async_task_manager_foreach_unprocessed_remove (BRASERO_ASYNC_TASK_MANAGER (object),
							       brasero_io_free_async_queue,
							       NULL);

	brasero_async_task_manager_foreach_active_remove (BRASERO_ASYNC_TASK_MANAGER (object),
							  brasero_io_free_async_queue,
							  NULL);

	g_slist_foreach (priv->metadatas, (GFunc) g_object_unref, NULL);
	g_slist_free (priv->metadatas);
	priv->metadatas = NULL;

	if (priv->meta_buffer) {
		BraseroIOMetadataCached *cached;

		while ((cached = g_queue_pop_head (priv->meta_buffer)) != NULL)
			brasero_io_metadata_cached_free (cached);

		g_queue_free (priv->meta_buffer);
		priv->meta_buffer = NULL;
	}

	if (priv->results_id) {
		g_source_remove (priv->results_id);
		priv->results_id = 0;
	}

	for (iter = priv->results; iter; iter = iter->next) {
		BraseroIOJobResult *result;

		result = iter->data;
		brasero_io_job_result_free (result);
	}
	g_slist_free (priv->results);
	priv->results = NULL;

	if (priv->progress_id) {
		g_source_remove (priv->progress_id);
		priv->progress_id = 0;
	}

	if (priv->progress) {
		g_slist_foreach (priv->progress, (GFunc) g_free, NULL);
		g_slist_free (priv->progress);
		priv->progress = NULL;
	}

	if (priv->lock) {
		g_mutex_free (priv->lock);
		priv->lock = NULL;
	}

	if (priv->lock_metadata) {
		g_mutex_free (priv->lock_metadata);
		priv->lock_metadata = NULL;
	}

	if (priv->mounted) {
		GSList *iter;

		/* unmount all volumes we mounted ourselves */
		for (iter = priv->mounted; iter; iter = iter->next) {
			GMount *mount;

			mount = iter->data;

			BRASERO_UTILS_LOG ("Unmountin volume");
			g_mount_unmount_with_operation (mount,
					 		G_MOUNT_UNMOUNT_NONE,
							NULL,
					 		NULL,
					 		NULL,
					 		NULL);
			g_object_unref (mount);
		}
	}

	G_OBJECT_CLASS (brasero_io_parent_class)->finalize (object);
}

static void
brasero_io_class_init (BraseroIOClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroIOPrivate));

	object_class->finalize = brasero_io_finalize;
}

static gboolean
brasero_io_cancel (BraseroAsyncTaskManager *manager,
                   gpointer callback_data,
                   gpointer user_data)
{
	return TRUE;
}

void
brasero_io_shutdown (void)
{
	GSList *iter, *next;
	BraseroIOPrivate *priv;

	priv = BRASERO_IO_PRIVATE (singleton);

	brasero_async_task_manager_foreach_unprocessed_remove (BRASERO_ASYNC_TASK_MANAGER (singleton),
							       brasero_io_cancel,
							       NULL);

	brasero_async_task_manager_foreach_active_remove (BRASERO_ASYNC_TASK_MANAGER (singleton),
							  brasero_io_cancel,
							  NULL);

	/* do it afterwards in case some results slipped through */
	for (iter = priv->results; iter; iter = next) {
		BraseroIOJobResult *result;

		result = iter->data;
		next = iter->next;
		brasero_io_cancel_result (singleton, result);
	}

	if (singleton) {
		g_object_unref (singleton);
		singleton = NULL;
	}
}

void
brasero_io_set_parent_window_callback (BraseroIOGetParentWinCb callback,
                                       gpointer user_data)
{
	BraseroIOPrivate *priv;
	BraseroIO *self;

	self = brasero_io_get_default ();
	priv = BRASERO_IO_PRIVATE (self);
	priv->win_callback = callback;
	priv->win_user_data = user_data;
	g_object_unref (self);
}
