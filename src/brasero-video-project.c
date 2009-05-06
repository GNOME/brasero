/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2005-2008 <bonfire-app@wanadoo.fr>
 * 
 * brasero is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * brasero is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>

#include "brasero-tags.h"

#include "brasero-video-project.h"
#include "brasero-file-monitor.h"
#include "brasero-io.h"
#include "brasero-marshal.h"

typedef struct _BraseroVideoProjectPrivate BraseroVideoProjectPrivate;
struct _BraseroVideoProjectPrivate
{
	guint ref_count;
	GHashTable *references;

	BraseroIOJobBase *load_uri;
	BraseroIOJobBase *load_dir;

	BraseroVideoFile *first;

	guint loading;
};

#define BRASERO_VIDEO_PROJECT_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_VIDEO_PROJECT, BraseroVideoProjectPrivate))

#ifdef BUILD_INOTIFY

#include "brasero-file-monitor.h"

G_DEFINE_TYPE (BraseroVideoProject, brasero_video_project, BRASERO_TYPE_FILE_MONITOR);

#else

G_DEFINE_TYPE (BraseroVideoProject, brasero_video_project, G_TYPE_OBJECT);

#endif

enum {
	PROJECT_LOADED_SIGNAL,
	SIZE_CHANGED_SIGNAL,
	DIRECTORY_URI_SIGNAL,
	UNREADABLE_SIGNAL,
	NOT_VIDEO_SIGNAL,
	ACTIVITY_SIGNAL,
	LAST_SIGNAL
};

static guint brasero_video_project_signals [LAST_SIGNAL] = {0};

/**
 * Used to send signals with a default answer
 */

static gboolean
brasero_video_project_file_signal (BraseroVideoProject *self,
				  guint signal,
				  const gchar *name)
{
	GValue instance_and_params [2];
	GValue return_value;
	GValue *params;

	/* object which signalled */
	instance_and_params->g_type = 0;
	g_value_init (instance_and_params, G_TYPE_FROM_INSTANCE (self));
	g_value_set_instance (instance_and_params, self);

	/* arguments of signal (name) */
	params = instance_and_params + 1;
	params->g_type = 0;
	g_value_init (params, G_TYPE_STRING);
	g_value_set_string (params, name);

	/* default to FALSE */
	return_value.g_type = 0;
	g_value_init (&return_value, G_TYPE_BOOLEAN);
	g_value_set_boolean (&return_value, FALSE);

	g_signal_emitv (instance_and_params,
			brasero_video_project_signals [signal],
			0,
			&return_value);

	g_value_unset (instance_and_params);
	g_value_unset (params);

	return g_value_get_boolean (&return_value);
}

/**
 * Manages the references to a node
 */

guint
brasero_video_project_reference_new (BraseroVideoProject *self,
				    BraseroVideoFile *node)
{
	BraseroVideoProjectPrivate *priv;
	guint retval;

	priv = BRASERO_VIDEO_PROJECT_PRIVATE (self);

	if (!priv->references)
		priv->references = g_hash_table_new (g_direct_hash, g_direct_equal);

	retval = priv->ref_count;
	while (g_hash_table_lookup (priv->references, GINT_TO_POINTER (retval))) {
		retval ++;

		if (retval == G_MAXINT)
			retval = 1;

		/* this means there is no more room for reference */
		if (retval == priv->ref_count)
			return 0;
	}

	g_hash_table_insert (priv->references,
			     GINT_TO_POINTER (retval),
			     node);
	priv->ref_count = retval + 1;
	if (priv->ref_count == G_MAXINT)
		priv->ref_count = 1;

	return retval;
}

void
brasero_video_project_reference_free (BraseroVideoProject *self,
				     guint reference)
{
	BraseroVideoProjectPrivate *priv;

	priv = BRASERO_VIDEO_PROJECT_PRIVATE (self);
	g_hash_table_remove (priv->references, GINT_TO_POINTER (reference));
}

BraseroVideoFile *
brasero_video_project_reference_get (BraseroVideoProject *self,
				    guint reference)
{
	BraseroVideoProjectPrivate *priv;

	/* if it was invalidated then the node returned is NULL */
	priv = BRASERO_VIDEO_PROJECT_PRIVATE (self);
	return g_hash_table_lookup (priv->references, GINT_TO_POINTER (reference));
}

static gboolean
brasero_video_project_reference_remove_children_cb (gpointer key,
						   gpointer data,
						   gpointer callback_data)
{
	BraseroVideoFile *node = data;
	BraseroVideoFile *removable = callback_data;

	if (node == removable)
		return TRUE;

	return FALSE;
}

static void
brasero_video_project_reference_invalidate (BraseroVideoProject *self,
					   BraseroVideoFile *node)
{
	BraseroVideoProjectPrivate *priv;

	/* used internally to invalidate reference whose node was removed */
	priv = BRASERO_VIDEO_PROJECT_PRIVATE (self);
	g_hash_table_foreach_remove (priv->references,
				     (GHRFunc) brasero_video_project_reference_remove_children_cb,
				     node);
}

/**
 * Move functions
 */

void
brasero_video_project_rename (BraseroVideoProject *self,
			      BraseroVideoFile *file,
			      const gchar *name)
{
	gchar *tmp;
	BraseroVideoProjectClass *klass;

	tmp = file->info->title;
	file->info->title = g_strdup (name);
	g_free (tmp);

	file->title_set = TRUE;

	klass = BRASERO_VIDEO_PROJECT_GET_CLASS (self);
	if (klass->node_changed)
		klass->node_changed (self, file);
}

void
brasero_video_project_move (BraseroVideoProject *self,
			    BraseroVideoFile *file,
			    BraseroVideoFile *next_file)
{
	BraseroVideoFile *prev, *next;
	BraseroVideoProjectClass *klass;
	BraseroVideoProjectPrivate *priv;

	priv = BRASERO_VIDEO_PROJECT_PRIVATE (self);
	if (!file)
		return;

	if (file == next_file)
		return;

	/* unlink it */
	prev = file->prev;
	next = file->next;

	if (next)
		next->prev = prev;

	if (prev)
		prev->next = next;
	else
		priv->first = next;

	/* tell the model */
	klass = BRASERO_VIDEO_PROJECT_GET_CLASS (self);
	if (klass->node_removed)
		klass->node_removed (self, file);

	/* relink it */
	if (next_file) {
		file->next = next_file;
		file->prev = next_file->prev;
		next_file->prev = file;

		if (file->prev)
			file->prev->next = file;
		else
			priv->first = file;
	}
	else if (priv->first) {
		BraseroVideoFile *last;

		/* Put it at the end */
		last = priv->first;
		while (last->next) last = last->next;

		file->next = NULL;
		file->prev = last;
		last->next = file;
	}
	else {
		priv->first = file;
		file->next = NULL;
		file->prev = NULL;
	}

	/* tell the model */
	if (klass->node_added)
		klass->node_added (self, file);
}

/**
 * Remove functions
 */

void
brasero_video_file_free (BraseroVideoFile *file)
{
	if (file->uri)
		g_free (file->uri);

	if (file->snapshot)
		g_object_unref (file->snapshot);

	if (file->info)
		brasero_stream_info_free (file->info);

	g_free (file);
}

static gboolean
brasero_video_project_foreach_monitor_cancel_cb (gpointer data,
						 gpointer user_data)
{
	BraseroVideoFile *node = data;
	BraseroVideoFile *file = user_data;

	if (node == file)
		return TRUE;

	return FALSE;
}

void
brasero_video_project_remove_file (BraseroVideoProject *self,
				   BraseroVideoFile *file)
{
	BraseroVideoFile *prev, *next;
	BraseroVideoProjectClass *klass;
	BraseroVideoProjectPrivate *priv;

	priv = BRASERO_VIDEO_PROJECT_PRIVATE (self);

	if (!file)
		return;

	/* Unlink it */
	prev = file->prev;
	next = file->next;

	if (next)
		next->prev = prev;

	if (prev)
		prev->next = next;
	else
		priv->first = next;

	klass = BRASERO_VIDEO_PROJECT_GET_CLASS (self);
	if (klass->node_removed)
		klass->node_removed (self, file);

	brasero_video_project_reference_invalidate (self, file);

#ifdef BUILD_INOTIFY

	/* Stop monitoring */
	if (file->is_monitored)
		brasero_file_monitor_foreach_cancel (BRASERO_FILE_MONITOR (self),
						     brasero_video_project_foreach_monitor_cancel_cb,
						     file);

#endif

	/* Free data */
	brasero_video_file_free (file);

	g_signal_emit (self,
		       brasero_video_project_signals [SIZE_CHANGED_SIGNAL],
		       0);
}

void
brasero_video_project_reset (BraseroVideoProject *self)
{
	BraseroVideoProjectPrivate *priv;
	BraseroVideoProjectClass *klass;
	BraseroVideoFile *iter, *next;
	guint num_nodes = 0;

	priv = BRASERO_VIDEO_PROJECT_PRIVATE (self);

	/* cancel all VFS operations */
	if (priv->load_uri) {
		brasero_io_cancel_by_base (priv->load_uri);
		g_free (priv->load_uri);
		priv->load_uri = NULL;
	}

	if (priv->load_dir) {
		brasero_io_cancel_by_base (priv->load_dir);
		g_free (priv->load_dir);
		priv->load_dir = NULL;
	}

	/* destroy all references */
	if (priv->references) {
		g_hash_table_destroy (priv->references);
		priv->references = g_hash_table_new (g_direct_hash, g_direct_equal);
	}

#ifdef BUILD_INOTIFY

	brasero_file_monitor_reset (BRASERO_FILE_MONITOR (self));

#endif

	/* empty tree */
	for (iter = priv->first; iter; iter = next) {
		next = iter->next;
		brasero_video_project_remove_file (self, iter);
	}
	priv->first = NULL;

	priv->loading = 0;

	klass = BRASERO_VIDEO_PROJECT_GET_CLASS (self);
	if (klass->reset)
		klass->reset (self, num_nodes);
}

/**
 * Add functions
 */

static BraseroVideoFile *
brasero_video_project_add_video_file (BraseroVideoProject *self,
				      const gchar *uri,
				      BraseroVideoFile *sibling,
				      guint64 start,
				      guint64 end)
{
	BraseroVideoProjectPrivate *priv;
	BraseroVideoFile *file;

	priv = BRASERO_VIDEO_PROJECT_PRIVATE (self);

	/* create new file and insert it */
	file = g_new0 (BraseroVideoFile, 1);
	file->uri = g_strdup (uri);

	if (start > -1)
		file->start = start;

	if (end > -1)
		file->end = end;

	if (sibling) {
		file->next = sibling;
		file->prev = sibling->prev;

		if (sibling->prev)
			sibling->prev->next = file;
		else
			priv->first = file;

		sibling->prev = file;
	}
	else if (priv->first) {
		BraseroVideoFile *last;

		/* Put it at the end */
		last = priv->first;
		while (last->next) last = last->next;

		file->prev = last;
		file->next = NULL;
		last->next = file;
	}
	else {
		priv->first = file;
		file->next = NULL;
		file->prev = NULL;
	}

	return file;
}

static void
brasero_video_project_set_file_information (BraseroVideoProject *self,
					    BraseroVideoFile *file,
					    GFileInfo *info)
{
	guint64 len;
	GdkPixbuf *snapshot;
	BraseroVideoProjectPrivate *priv;

	priv = BRASERO_VIDEO_PROJECT_PRIVATE (self);

	/* For reloading files no need to go further, we just want to check that
	 * they are still readable and still holds video. */
	if (file->is_reloading) {
		file->is_reloading = FALSE;
		return;
	}

	file->is_loading = FALSE;

	if (g_file_info_get_is_symlink (info)) {
		gchar *sym_uri;

		sym_uri = g_strconcat ("file://", g_file_info_get_symlink_target (info), NULL);
		g_free (file->uri);

		file->uri = sym_uri;
	}

	/* Set the snapshot */
	snapshot = GDK_PIXBUF (g_file_info_get_attribute_object (info, BRASERO_IO_THUMBNAIL));
	if (snapshot) {
		GdkPixbuf *scaled;

		scaled = gdk_pixbuf_scale_simple (snapshot,
						  48 * gdk_pixbuf_get_width (snapshot) / gdk_pixbuf_get_height (snapshot),
						  48,
						  GDK_INTERP_BILINEAR);
		file->snapshot = scaled;
	}

	/* size */
	if (!file->len_set) {
		len = g_file_info_get_attribute_uint64 (info, BRASERO_IO_LEN);
		if (file->end > len)
			file->end = len;
		else if (file->end <= 0)
			file->end = len;
	}

	/* Get the song info */
	if (!file->info)
		file->info = g_new0 (BraseroStreamInfo, 1);

	if (!file->title_set) {
		if (file->info->title)
			g_free (file->info->title);

		file->info->title = g_strdup (g_file_info_get_attribute_string (info, BRASERO_IO_TITLE));
	}

	if (!file->artist_set) {
		if (file->info->artist)
			g_free (file->info->artist);

		file->info->artist = g_strdup (g_file_info_get_attribute_string (info, BRASERO_IO_ARTIST));
	}

	if (!file->composer_set) {
		if (file->info->composer)
			g_free (file->info->composer);

		file->info->composer = g_strdup (g_file_info_get_attribute_string (info, BRASERO_IO_COMPOSER));
	}

	if (!file->isrc_set)
		file->info->isrc = g_file_info_get_attribute_int32 (info, BRASERO_IO_ISRC);

#ifdef BUILD_INOTIFY

	/* Start monitoring */
	file->is_monitored = TRUE;
	brasero_file_monitor_single_file (BRASERO_FILE_MONITOR (self),
					  file->uri,
					  file);

#endif
}

static void
brasero_video_project_vfs_operation_finished (GObject *object,
					      gboolean cancelled,
					      gpointer null_data)
{
	BraseroVideoProjectPrivate *priv;

	priv = BRASERO_VIDEO_PROJECT_PRIVATE (object);

	priv->loading --;
	g_signal_emit (object,
		       brasero_video_project_signals [ACTIVITY_SIGNAL],
		       0,
		       priv->loading > 0);
}

static void
brasero_video_project_add_directory_contents_result (GObject *obj,
						     GError *error,
						     const gchar *uri,
						     GFileInfo *info,
						     gpointer user_data)
{
	BraseroVideoFile *file;
	BraseroVideoFile *sibling;
	BraseroVideoProjectClass *klass;
	guint ref = GPOINTER_TO_INT (user_data);

	/* Check the return status for this file */
	if (error)
		return;

	if (g_file_info_get_file_type (info) != G_FILE_TYPE_REGULAR
	|| !g_file_info_get_attribute_boolean (info, BRASERO_IO_HAS_VIDEO))
		return;

	sibling = brasero_video_project_reference_get (BRASERO_VIDEO_PROJECT (obj), ref);

	/* Add a video file and set all information */
	file = brasero_video_project_add_video_file (BRASERO_VIDEO_PROJECT (obj),
						     uri,
						     sibling,
						     -1,
						     -1);
						     
	brasero_video_project_set_file_information (BRASERO_VIDEO_PROJECT (obj),
						    file,
						    info);

	/* Tell model we added a node */
	klass = BRASERO_VIDEO_PROJECT_GET_CLASS (obj);
	if (klass->node_added)
		klass->node_added (BRASERO_VIDEO_PROJECT (obj), file);

	/* update size */
	g_signal_emit (BRASERO_VIDEO_PROJECT (obj),
		       brasero_video_project_signals [SIZE_CHANGED_SIGNAL],
		       0);
}

static void
brasero_video_project_add_directory_contents (BraseroVideoProject *self,
					      const gchar *uri,
					      BraseroVideoFile *sibling)
{
	BraseroVideoProjectPrivate *priv;
	guint ref;

	priv = BRASERO_VIDEO_PROJECT_PRIVATE (self);

	if (!priv->load_dir)
		priv->load_dir = brasero_io_register (G_OBJECT (self),
						      brasero_video_project_add_directory_contents_result,
						      brasero_video_project_vfs_operation_finished,
						      NULL);

	priv->loading ++;
	g_signal_emit (self,
		       brasero_video_project_signals [ACTIVITY_SIGNAL],
		       0,
		       priv->loading != 0);

	ref = brasero_video_project_reference_new (self, sibling);

	brasero_io_load_directory (uri,
				   priv->load_dir,
				   BRASERO_IO_INFO_MIME|
				   BRASERO_IO_INFO_PERM|
				   BRASERO_IO_INFO_METADATA|
				   BRASERO_IO_INFO_METADATA_MISSING_CODEC|
				   BRASERO_IO_INFO_RECURSIVE|
				   BRASERO_IO_INFO_METADATA_THUMBNAIL,
				   GINT_TO_POINTER (ref));
}

static void
brasero_video_project_result_cb (GObject *obj,
				 GError *error,
				 const gchar *uri,
				 GFileInfo *info,
				 gpointer user_data)
{
	BraseroVideoFile *file;
	BraseroVideoProject *self;
	BraseroVideoProjectClass *klass;
	BraseroVideoProjectPrivate *priv;
	guint ref = GPOINTER_TO_INT (user_data);

	self = BRASERO_VIDEO_PROJECT (obj);
	priv = BRASERO_VIDEO_PROJECT_PRIVATE (obj);

	/* Get the reference for the node */
	file = brasero_video_project_reference_get (self, ref);
	if (!file)
		return;

	/* Check the return status for this file */
	if (error) {
		g_signal_emit (self,
			       brasero_video_project_signals [UNREADABLE_SIGNAL],
			       0,
			       error,
			       uri);

		brasero_video_project_remove_file (self, file);
		return;
	}

	if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
		gboolean result;

		/* Ask the user */
		result = brasero_video_project_file_signal (self,
							    DIRECTORY_URI_SIGNAL,
							    uri);

		/* NOTE: we need to pass a sibling here even if that the file
		 * that's going to be deleted just after. */
		if (result)
			brasero_video_project_add_directory_contents (self,
								      uri,
								      file->next?file->next:file);

		/* remove the file */
		brasero_video_project_remove_file (self, file);
		return;
	}

	if (g_file_info_get_file_type (info) != G_FILE_TYPE_REGULAR
	|| !g_file_info_get_attribute_boolean (info, BRASERO_IO_HAS_VIDEO)) {
		g_signal_emit (self,
			       brasero_video_project_signals [NOT_VIDEO_SIGNAL],
			       0,
			       uri);

		brasero_video_project_remove_file (self, file);
		return;
	}

	brasero_video_project_set_file_information (BRASERO_VIDEO_PROJECT (obj),
						    file,
						    info);

	/* Tell upper object that the node status and information changed */
	klass = BRASERO_VIDEO_PROJECT_GET_CLASS (self);
	if (klass->node_changed)
		klass->node_changed (self, file);

	/* update size */
	g_signal_emit (self,
		       brasero_video_project_signals [SIZE_CHANGED_SIGNAL],
		       0);
}

BraseroVideoFile *
brasero_video_project_add_uri (BraseroVideoProject *self,
			       const gchar *uri,
			       BraseroStreamInfo *info,
			       BraseroVideoFile *sibling,
			       gint64 start,
			       gint64 end)
{
	BraseroVideoProjectPrivate *priv;
	BraseroVideoProjectClass *klass;
	BraseroVideoFile *file;
	guint ref;

	g_return_val_if_fail (uri != NULL, NULL);

	priv = BRASERO_VIDEO_PROJECT_PRIVATE (self);

	/* create new file and insert it */
	file = g_new0 (BraseroVideoFile, 1);
	file->uri = g_strdup (uri);

	if (info) {
		file->info = brasero_stream_info_copy (info);

		if (info->isrc)
			file->isrc_set = TRUE;
		if (info->title)
			file->title_set = TRUE;
		if (info->artist)
			file->artist_set = TRUE;
		if (info->composer)
			file->composer_set = TRUE;
	}
	else
		file->info = g_new0 (BraseroStreamInfo, 1);

	if (start > -1)
		file->start = start;

	if (end > -1) {
		file->end = end;
		file->len_set = TRUE;
	}

	if (sibling) {
		file->next = sibling;
		file->prev = sibling->prev;

		if (sibling->prev)
			sibling->prev->next = file;
		else
			priv->first = file;

		sibling->prev = file;
	}
	else if (priv->first) {
		BraseroVideoFile *last;

		/* Put it at the end */
		last = priv->first;
		while (last->next) last = last->next;

		file->prev = last;
		file->next = NULL;
		last->next = file;
	}
	else {
		priv->first = file;
		file->next = NULL;
		file->prev = NULL;
	}

	/* Tell model we added a node */
	klass = BRASERO_VIDEO_PROJECT_GET_CLASS (self);
	if (klass->node_added)
		klass->node_added (self, file);

	/* get info async for the file */
	if (!priv->load_uri)
		priv->load_uri = brasero_io_register (G_OBJECT (self),
						      brasero_video_project_result_cb,
						      brasero_video_project_vfs_operation_finished,
						      NULL);

	file->is_loading = 1;
	priv->loading ++;

	ref = brasero_video_project_reference_new (self, file);
	brasero_io_get_file_info (uri,
				  priv->load_uri,
				  BRASERO_IO_INFO_PERM|
				  BRASERO_IO_INFO_MIME|
				  BRASERO_IO_INFO_URGENT|
				  BRASERO_IO_INFO_METADATA|
				  BRASERO_IO_INFO_METADATA_MISSING_CODEC|
				  BRASERO_IO_INFO_METADATA_THUMBNAIL,
				  GINT_TO_POINTER (ref));

	g_signal_emit (self,
		       brasero_video_project_signals [ACTIVITY_SIGNAL],
		       0,
		       (priv->loading > 0));

	return file;
}

void
brasero_video_project_resize_file (BraseroVideoProject *self,
				   BraseroVideoFile *file,
				   gint64 start,
				   gint64 end)
{
	BraseroVideoProjectPrivate *priv;
	BraseroVideoProjectClass *klass;

	priv = BRASERO_VIDEO_PROJECT_PRIVATE (self);

	file->start = start;
	file->end = end;

	klass = BRASERO_VIDEO_PROJECT_GET_CLASS (self);
	if (klass->node_changed)
		klass->node_changed (self, file);

	/* update size */
	g_signal_emit (self,
		       brasero_video_project_signals [SIZE_CHANGED_SIGNAL],
		       0);
}

guint64
brasero_video_project_get_size (BraseroVideoProject *self)
{
	BraseroVideoProjectPrivate *priv;
	BraseroVideoFile *iter;
	guint size = 0;

	priv = BRASERO_VIDEO_PROJECT_PRIVATE (self);

	/* FIXME: duration to sectors is not correct here, that's not audio... */
	for (iter = priv->first; iter; iter = iter->next)
		size += BRASERO_DURATION_TO_SECTORS (iter->end - iter->start);

	return size;
}

guint
brasero_video_project_get_file_num (BraseroVideoProject *self)
{
	BraseroVideoProjectPrivate *priv;
	BraseroVideoFile *item;
	guint num = 0;

	priv = BRASERO_VIDEO_PROJECT_PRIVATE (self);
	for (item = priv->first; item; item = item->next)
		num ++;

	return num;
}

BraseroVideoFile *
brasero_video_project_get_nth_item (BraseroVideoProject *self,
				    guint nth)
{
	BraseroVideoFile *item;
	BraseroVideoProjectPrivate *priv;

	priv = BRASERO_VIDEO_PROJECT_PRIVATE (self);
	if (!nth)
		return priv->first;

	priv = BRASERO_VIDEO_PROJECT_PRIVATE (self);
	for (item = priv->first; item; item = item->next) {
		if (nth <= 0)
			return item;

		nth --;
	}

	return NULL;
}

guint
brasero_video_project_get_item_index (BraseroVideoProject *self,
				      BraseroVideoFile *file)
{
	guint nth = 0;
	BraseroVideoFile *item;
	BraseroVideoProjectPrivate *priv;

	priv = BRASERO_VIDEO_PROJECT_PRIVATE (self);

	for (item = priv->first; item; item = item->next) {
		if (item == file)
			return nth;

		nth ++;
	}

	return nth;
}

BraseroDiscResult
brasero_video_project_get_status (BraseroVideoProject *self,
				  gint *remaining,
				  gchar **current_task)
{
	BraseroVideoProjectPrivate *priv;

	priv = BRASERO_VIDEO_PROJECT_PRIVATE (self);

	if (priv->loading) {
		if (remaining)
			*remaining = priv->loading;

		if (current_task)
			*current_task = g_strdup (_("Analysing video files"));

		return BRASERO_DISC_NOT_READY;
	}

	if (!priv->first)
		return BRASERO_DISC_ERROR_EMPTY_SELECTION;

	return BRASERO_DISC_OK;
}

GSList *
brasero_video_project_get_contents (BraseroVideoProject *self,
				    gboolean values_set)
{
	GSList *tracks = NULL;
	BraseroVideoFile *file;
	BraseroVideoProjectPrivate *priv;

	priv = BRASERO_VIDEO_PROJECT_PRIVATE (self);
	if (!priv->first)
		return NULL;

	for (file = priv->first; file; file = file->next) {
		BraseroTrackStream *track;

		track = brasero_track_stream_new ();
		brasero_track_stream_set_source (track, file->uri);
		brasero_track_stream_set_format (track,
						 BRASERO_AUDIO_FORMAT_UNDEFINED|
						 BRASERO_VIDEO_FORMAT_UNDEFINED);

		if (!values_set || file->len_set)
			brasero_track_stream_set_boundaries (track,
							     file->start,
							     file->end,
							     -1);
		else
			brasero_track_stream_set_boundaries (track,
							     file->start,
							     0,
							     -1);

		if (file->info && values_set) {
			if (!file->title_set)
				brasero_track_tag_add_string (BRASERO_TRACK (track),
							      BRASERO_TRACK_STREAM_TITLE_TAG,
							      file->info->title);

			if (!file->artist_set)
				brasero_track_tag_add_string (BRASERO_TRACK (track),
							      BRASERO_TRACK_STREAM_ARTIST_TAG,
							      file->info->artist);

			if (!file->composer_set)
				brasero_track_tag_add_string (BRASERO_TRACK (track),
							      BRASERO_TRACK_STREAM_COMPOSER_TAG,
							      file->info->composer);
			if (!file->isrc_set)
				brasero_track_tag_add_int (BRASERO_TRACK (track),
							   BRASERO_TRACK_STREAM_ISRC_TAG,
							   file->info->isrc);
		}

		tracks = g_slist_prepend (tracks, BRASERO_TRACK (track));
	}

	tracks = g_slist_reverse (tracks);
	return tracks;
}

static void
brasero_video_project_init (BraseroVideoProject *object)
{
	BraseroVideoProjectPrivate *priv;

	priv = BRASERO_VIDEO_PROJECT_PRIVATE (object);
	priv->ref_count = 1;
}

static void
brasero_video_project_finalize (GObject *object)
{
	BraseroVideoProjectPrivate *priv;
	BraseroVideoFile *iter, *next;

	priv = BRASERO_VIDEO_PROJECT_PRIVATE (object);

	for (iter = priv->first; iter; iter = next) {
		next = iter->next;
		g_free (iter->uri);
		brasero_stream_info_free (iter->info);
		g_free (iter);
	}

	if (priv->references) {
		g_hash_table_destroy (priv->references);
		priv->references = NULL;
	}

	G_OBJECT_CLASS (brasero_video_project_parent_class)->finalize (object);
}
/**
 * Callbacks for inotify backend
 */

#ifdef BUILD_INOTIFY

static void
brasero_video_project_file_renamed (BraseroFileMonitor *monitor,
				    BraseroFileMonitorType type,
				    gpointer callback_data,
				    const gchar *old_name,
				    const gchar *new_name)
{
	brasero_video_project_rename (BRASERO_VIDEO_PROJECT (monitor),
				      callback_data,
				      new_name);
}

static void
brasero_video_project_file_removed (BraseroFileMonitor *monitor,
				    BraseroFileMonitorType type,
				    gpointer callback_data,
				    const gchar *name)
{
	brasero_video_project_remove_file (BRASERO_VIDEO_PROJECT (monitor),
					   callback_data);
}

static void
brasero_video_project_file_modified (BraseroFileMonitor *monitor,
				     gpointer callback_data,
				     const gchar *name)
{
	BraseroVideoProjectPrivate *priv;
	BraseroVideoFile *file;
	guint ref;

	priv = BRASERO_VIDEO_PROJECT_PRIVATE (monitor);

	/* priv->load_uri has already been initialized otherwise the tree would
	 * be empty. But who knows... */
	if (!priv->load_uri)
		return;

	file = callback_data;
	file->is_reloading = TRUE;

	ref = brasero_video_project_reference_new (BRASERO_VIDEO_PROJECT (monitor), file);
	brasero_io_get_file_info (file->uri,
				  priv->load_uri,
				  BRASERO_IO_INFO_PERM|
				  BRASERO_IO_INFO_MIME|
				  BRASERO_IO_INFO_URGENT|
				  BRASERO_IO_INFO_METADATA|
				  BRASERO_IO_INFO_METADATA_MISSING_CODEC|
				  BRASERO_IO_INFO_METADATA_THUMBNAIL,
				  GINT_TO_POINTER (ref));
}

static void
brasero_video_project_file_moved (BraseroFileMonitor *monitor,
				  BraseroFileMonitorType type,
				  gpointer callback_src,
				  const gchar *name_src,
				  gpointer callback_dest,
				  const gchar *name_dest)
{
	/* This is a file removed since we won't monitor all folders to get its
	 * new path */
	/* FIXME: what about files moved to one of the URI in the list ? */
	brasero_video_project_remove_file (BRASERO_VIDEO_PROJECT (monitor),
					   callback_src);
}

#endif

static void
brasero_video_project_class_init (BraseroVideoProjectClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroVideoProjectPrivate));

	object_class->finalize = brasero_video_project_finalize;

	brasero_video_project_signals [SIZE_CHANGED_SIGNAL] = 
	    g_signal_new ("size_changed",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__VOID,
			  G_TYPE_NONE,
			  0,
			  G_TYPE_NONE);

	brasero_video_project_signals [PROJECT_LOADED_SIGNAL] = 
	    g_signal_new ("project-loaded",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__INT,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_INT);

	brasero_video_project_signals [UNREADABLE_SIGNAL] = 
	    g_signal_new ("unreadable_uri",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_FIRST,
			  0,
			  NULL, NULL,
			  brasero_marshal_VOID__POINTER_STRING,
			  G_TYPE_NONE,
			  2,
			  G_TYPE_POINTER,
			  G_TYPE_STRING);

	brasero_video_project_signals [NOT_VIDEO_SIGNAL] = 
	    g_signal_new ("not_video_uri",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_FIRST,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__STRING,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_STRING);

	brasero_video_project_signals [DIRECTORY_URI_SIGNAL] = 
	    g_signal_new ("directory_uri",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL, NULL,
			  brasero_marshal_BOOLEAN__STRING,
			  G_TYPE_BOOLEAN,
			  1,
			  G_TYPE_STRING);

	brasero_video_project_signals [ACTIVITY_SIGNAL] = 
	    g_signal_new ("vfs_activity",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_FIRST|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__BOOLEAN,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_BOOLEAN);

#ifdef BUILD_INOTIFY

	BraseroFileMonitorClass *monitor_class = BRASERO_FILE_MONITOR_CLASS (klass);

	/* NOTE: file_added is not needed here since there aren't any directory */
	monitor_class->file_moved = brasero_video_project_file_moved;
	monitor_class->file_removed = brasero_video_project_file_removed;
	monitor_class->file_renamed = brasero_video_project_file_renamed;
	monitor_class->file_modified = brasero_video_project_file_modified;

#endif
}
