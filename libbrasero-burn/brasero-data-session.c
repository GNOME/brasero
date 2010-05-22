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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>

#include "brasero-media-private.h"

#include "scsi-device.h"

#include "brasero-drive.h"
#include "brasero-medium.h"
#include "brasero-medium-monitor.h"
#include "burn-volume.h"

#include "brasero-burn-lib.h"

#include "brasero-data-session.h"
#include "brasero-data-project.h"
#include "brasero-file-node.h"
#include "brasero-io.h"

#include "libbrasero-marshal.h"

typedef struct _BraseroDataSessionPrivate BraseroDataSessionPrivate;
struct _BraseroDataSessionPrivate
{
	BraseroIOJobBase *load_dir;

	/* Multisession drives that are inserted */
	GSList *media;

	/* Drive whose session is loaded */
	BraseroMedium *loaded;

	/* Nodes from the loaded session in the tree */
	GSList *nodes;
};

#define BRASERO_DATA_SESSION_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_DATA_SESSION, BraseroDataSessionPrivate))

G_DEFINE_TYPE (BraseroDataSession, brasero_data_session, BRASERO_TYPE_DATA_PROJECT);

enum {
	AVAILABLE_SIGNAL,
	LOADED_SIGNAL,
	LAST_SIGNAL
};

static gulong brasero_data_session_signals [LAST_SIGNAL] = { 0 };

/**
 * to evaluate the contents of a medium or image async
 */
struct _BraseroIOImageContentsData {
	BraseroIOJob job;
	gchar *dev_image;

	gint64 session_block;
	gint64 block;
};
typedef struct _BraseroIOImageContentsData BraseroIOImageContentsData;

static void
brasero_io_image_directory_contents_destroy (BraseroAsyncTaskManager *manager,
					     gboolean cancelled,
					     gpointer callback_data)
{
	BraseroIOImageContentsData *data = callback_data;

	g_free (data->dev_image);
	brasero_io_job_free (cancelled, BRASERO_IO_JOB (data));
}

static BraseroAsyncTaskResult
brasero_io_image_directory_contents_thread (BraseroAsyncTaskManager *manager,
					    GCancellable *cancel,
					    gpointer callback_data)
{
	BraseroIOImageContentsData *data = callback_data;
	BraseroDeviceHandle *handle;
	GList *children, *iter;
	GError *error = NULL;
	BraseroVolSrc *vol;

	handle = brasero_device_handle_open (data->job.uri, FALSE, NULL);
	if (!handle) {
		GError *error;

		error = g_error_new (BRASERO_BURN_ERROR,
		                     BRASERO_BURN_ERROR_GENERAL,
		                     _("The drive is busy"));

		brasero_io_return_result (data->job.base,
					  data->job.uri,
					  NULL,
					  error,
					  data->job.callback_data);
		return BRASERO_ASYNC_TASK_FINISHED;
	}

	vol = brasero_volume_source_open_device_handle (handle, &error);
	if (!vol) {
		brasero_device_handle_close (handle);
		brasero_io_return_result (data->job.base,
					  data->job.uri,
					  NULL,
					  error,
					  data->job.callback_data);
		return BRASERO_ASYNC_TASK_FINISHED;
	}

	children = brasero_volume_load_directory_contents (vol,
							   data->session_block,
							   data->block,
							   &error);
	brasero_volume_source_close (vol);
	brasero_device_handle_close (handle);

	for (iter = children; iter; iter = iter->next) {
		BraseroVolFile *file;
		GFileInfo *info;

		file = iter->data;

		info = g_file_info_new ();
		g_file_info_set_file_type (info, file->isdir? G_FILE_TYPE_DIRECTORY:G_FILE_TYPE_REGULAR);
		g_file_info_set_name (info, BRASERO_VOLUME_FILE_NAME (file));

		if (file->isdir)
			g_file_info_set_attribute_int64 (info,
							 BRASERO_IO_DIR_CONTENTS_ADDR,
							 file->specific.dir.address);
		else
			g_file_info_set_size (info, BRASERO_VOLUME_FILE_SIZE (file));

		brasero_io_return_result (data->job.base,
					  data->job.uri,
					  info,
					  NULL,
					  data->job.callback_data);
	}

	g_list_foreach (children, (GFunc) brasero_volume_file_free, NULL);
	g_list_free (children);

	return BRASERO_ASYNC_TASK_FINISHED;
}

static const BraseroAsyncTaskType image_contents_type = {
	brasero_io_image_directory_contents_thread,
	brasero_io_image_directory_contents_destroy
};

static void
brasero_io_load_image_directory (const gchar *dev_image,
				 gint64 session_block,
				 gint64 block,
				 const BraseroIOJobBase *base,
				 BraseroIOFlags options,
				 gpointer user_data)
{
	BraseroIOImageContentsData *data;
	BraseroIOResultCallbackData *callback_data = NULL;

	if (user_data) {
		callback_data = g_new0 (BraseroIOResultCallbackData, 1);
		callback_data->callback_data = user_data;
	}

	data = g_new0 (BraseroIOImageContentsData, 1);
	data->block = block;
	data->session_block = session_block;

	brasero_io_set_job (BRASERO_IO_JOB (data),
			    base,
			    dev_image,
			    options,
			    callback_data);

	brasero_io_push_job (BRASERO_IO_JOB (data),
			     &image_contents_type);

}

void
brasero_data_session_remove_last (BraseroDataSession *self)
{
	BraseroDataSessionPrivate *priv;
	GSList *iter;

	priv = BRASERO_DATA_SESSION_PRIVATE (self);

	if (!priv->nodes)
		return;

	/* go through the top nodes and remove all the imported nodes */
	for (iter = priv->nodes; iter; iter = iter->next) {
		BraseroFileNode *node;

		node = iter->data;
		brasero_data_project_destroy_node (BRASERO_DATA_PROJECT (self), node);
	}

	g_slist_free (priv->nodes);
	priv->nodes = NULL;

	g_signal_emit (self,
		       brasero_data_session_signals [LOADED_SIGNAL],
		       0,
		       priv->loaded,
		       FALSE);

	if (priv->loaded) {
		g_object_unref (priv->loaded);
		priv->loaded = NULL;
	}
}

static void
brasero_data_session_load_dir_destroy (GObject *object,
				       gboolean cancelled,
				       gpointer data)
{
	gint reference;
	BraseroFileNode *parent;

	/* reference */
	reference = GPOINTER_TO_INT (data);
	if (reference <= 0)
		return;

	parent = brasero_data_project_reference_get (BRASERO_DATA_PROJECT (object), reference);
	if (parent)
		brasero_data_project_directory_node_loaded (BRASERO_DATA_PROJECT (object), parent);

	brasero_data_project_reference_free (BRASERO_DATA_PROJECT (object), reference);
}

static void
brasero_data_session_load_dir_result (GObject *owner,
				      GError *error,
				      const gchar *dev_image,
				      GFileInfo *info,
				      gpointer data)
{
	BraseroDataSessionPrivate *priv;
	BraseroFileNode *parent;
	BraseroFileNode *node;
	gint reference;

	priv = BRASERO_DATA_SESSION_PRIVATE (owner);

	if (!info) {
		g_signal_emit (owner,
			       brasero_data_session_signals [LOADED_SIGNAL],
			       0,
			       priv->loaded,
			       FALSE);

		/* FIXME: tell the user the error message */
		return;
	}

	reference = GPOINTER_TO_INT (data);
	if (reference > 0)
		parent = brasero_data_project_reference_get (BRASERO_DATA_PROJECT (owner),
							     reference);
	else
		parent = NULL;

	/* add all the files/folders at the root of the session */
	node = brasero_data_project_add_imported_session_file (BRASERO_DATA_PROJECT (owner),
							       info,
							       parent);
	if (!node) {
		/* This is not a problem, it could be simply that the user did 
		 * not want to overwrite, so do not do the following (reminder):
		g_signal_emit (owner,
			       brasero_data_session_signals [LOADED_SIGNAL],
			       0,
			       priv->loaded,
			       (priv->nodes != NULL));
		*/
		return;
 	}

	/* Only if we're exploring root directory */
	if (!parent) {
		priv->nodes = g_slist_prepend (priv->nodes, node);

		if (g_slist_length (priv->nodes) == 1) {
			/* Only tell when the first top node is successfully loaded */
			g_signal_emit (owner,
				       brasero_data_session_signals [LOADED_SIGNAL],
				       0,
				       priv->loaded,
				       TRUE);
		}
	}
}

static gboolean
brasero_data_session_load_directory_contents_real (BraseroDataSession *self,
						   BraseroFileNode *node,
						   GError **error)
{
	BraseroDataSessionPrivate *priv;
	goffset session_block;
	const gchar *device;
	gint reference = -1;

	if (node && !node->is_fake)
		return TRUE;

	priv = BRASERO_DATA_SESSION_PRIVATE (self);
	device = brasero_drive_get_device (brasero_medium_get_drive (priv->loaded));
	brasero_medium_get_last_data_track_address (priv->loaded,
						    NULL,
						    &session_block);

	if (!priv->load_dir)
		priv->load_dir = brasero_io_register (G_OBJECT (self),
						      brasero_data_session_load_dir_result,
						      brasero_data_session_load_dir_destroy,
						      NULL);

	/* If there aren't any node then that's root */
	if (node) {
		reference = brasero_data_project_reference_new (BRASERO_DATA_PROJECT (self), node);
		node->is_exploring = TRUE;
	}

	brasero_io_load_image_directory (device,
					 session_block,
					 BRASERO_FILE_NODE_IMPORTED_ADDRESS (node),
					 priv->load_dir,
					 BRASERO_IO_INFO_URGENT,
					 GINT_TO_POINTER (reference));

	if (node)
		node->is_fake = FALSE;

	return TRUE;
}

gboolean
brasero_data_session_load_directory_contents (BraseroDataSession *self,
					      BraseroFileNode *node,
					      GError **error)
{
	if (node == NULL)
		return FALSE;

	return brasero_data_session_load_directory_contents_real (self, node, error);
}

gboolean
brasero_data_session_add_last (BraseroDataSession *self,
			       BraseroMedium *medium,
			       GError **error)
{
	BraseroDataSessionPrivate *priv;

	priv = BRASERO_DATA_SESSION_PRIVATE (self);

	if (priv->nodes)
		return FALSE;

	priv->loaded = medium;
	g_object_ref (medium);

	return brasero_data_session_load_directory_contents_real (self, NULL, error);
}

gboolean
brasero_data_session_has_available_media (BraseroDataSession *self)
{
	BraseroDataSessionPrivate *priv;

	priv = BRASERO_DATA_SESSION_PRIVATE (self);

	return priv->media != NULL;
}

GSList *
brasero_data_session_get_available_media (BraseroDataSession *self)
{
	GSList *retval;
	BraseroDataSessionPrivate *priv;

	priv = BRASERO_DATA_SESSION_PRIVATE (self);

	retval = g_slist_copy (priv->media);
	g_slist_foreach (retval, (GFunc) g_object_ref, NULL);

	return retval;
}

BraseroMedium *
brasero_data_session_get_loaded_medium (BraseroDataSession *self)
{
	BraseroDataSessionPrivate *priv;

	priv = BRASERO_DATA_SESSION_PRIVATE (self);
	if (!priv->media || !priv->nodes)
		return NULL;

	return priv->loaded;
}

static gboolean
brasero_data_session_is_valid_multi (BraseroMedium *medium)
{
	BraseroMedia media;
	BraseroMedia media_status;

	media = brasero_medium_get_status (medium);
	media_status = brasero_burn_library_get_media_capabilities (media);

	return (media_status & BRASERO_MEDIUM_WRITABLE) &&
	       (media & BRASERO_MEDIUM_HAS_DATA) &&
	       (brasero_medium_get_last_data_track_address (medium, NULL, NULL) != -1);
}

static void
brasero_data_session_disc_added_cb (BraseroMediumMonitor *monitor,
				    BraseroMedium *medium,
				    BraseroDataSession *self)
{
	BraseroDataSessionPrivate *priv;

	priv = BRASERO_DATA_SESSION_PRIVATE (self);

	if (!brasero_data_session_is_valid_multi (medium))
		return;

	g_object_ref (medium);
	priv->media = g_slist_prepend (priv->media, medium);

	g_signal_emit (self,
		       brasero_data_session_signals [AVAILABLE_SIGNAL],
		       0,
		       medium,
		       TRUE);
}

static void
brasero_data_session_disc_removed_cb (BraseroMediumMonitor *monitor,
				      BraseroMedium *medium,
				      BraseroDataSession *self)
{
	GSList *iter;
	GSList *next;
	BraseroDataSessionPrivate *priv;

	priv = BRASERO_DATA_SESSION_PRIVATE (self);

	/* see if that's the current loaded one */
	if (priv->loaded && priv->loaded == medium)
		brasero_data_session_remove_last (self);

	/* remove it from our list */
	for (iter = priv->media; iter; iter = next) {
		BraseroMedium *iter_medium;

		iter_medium = iter->data;
		next = iter->next;

		if (medium == iter_medium) {
			g_signal_emit (self,
				       brasero_data_session_signals [AVAILABLE_SIGNAL],
				       0,
				       medium,
				       FALSE);

			priv->media = g_slist_remove (priv->media, iter_medium);
			g_object_unref (iter_medium);
		}
	}
}

static void
brasero_data_session_init (BraseroDataSession *object)
{
	GSList *iter, *list;
	BraseroMediumMonitor *monitor;
	BraseroDataSessionPrivate *priv;

	priv = BRASERO_DATA_SESSION_PRIVATE (object);

	monitor = brasero_medium_monitor_get_default ();
	g_signal_connect (monitor,
			  "medium-added",
			  G_CALLBACK (brasero_data_session_disc_added_cb),
			  object);
	g_signal_connect (monitor,
			  "medium-removed",
			  G_CALLBACK (brasero_data_session_disc_removed_cb),
			  object);

	list = brasero_medium_monitor_get_media (monitor,
						 BRASERO_MEDIA_TYPE_WRITABLE|
						 BRASERO_MEDIA_TYPE_REWRITABLE);
	g_object_unref (monitor);

	/* check for a multisession medium already in */
	for (iter = list; iter; iter = iter->next) {
		BraseroMedium *medium;

		medium = iter->data;
		if (brasero_data_session_is_valid_multi (medium)) {
			g_object_ref (medium);
			priv->media = g_slist_prepend (priv->media, medium);
		}
	}
	g_slist_foreach (list, (GFunc) g_object_unref, NULL);
	g_slist_free (list);
}

static void
brasero_data_session_stop_io (BraseroDataSession *self)
{
	BraseroDataSessionPrivate *priv;

	priv = BRASERO_DATA_SESSION_PRIVATE (self);

	if (priv->load_dir) {
		brasero_io_cancel_by_base (priv->load_dir);
		brasero_io_job_base_free (priv->load_dir);
		priv->load_dir = NULL;
	}
}

static void
brasero_data_session_reset (BraseroDataProject *project,
			    guint num_nodes)
{
	brasero_data_session_stop_io (BRASERO_DATA_SESSION (project));

	/* chain up this function except if we invalidated the node */
	if (BRASERO_DATA_PROJECT_CLASS (brasero_data_session_parent_class)->reset)
		BRASERO_DATA_PROJECT_CLASS (brasero_data_session_parent_class)->reset (project, num_nodes);
}

static void
brasero_data_session_finalize (GObject *object)
{
	BraseroDataSessionPrivate *priv;
	BraseroMediumMonitor *monitor;

	priv = BRASERO_DATA_SESSION_PRIVATE (object);

	monitor = brasero_medium_monitor_get_default ();
	g_signal_handlers_disconnect_by_func (monitor,
	                                      brasero_data_session_disc_added_cb,
	                                      object);
	g_signal_handlers_disconnect_by_func (monitor,
	                                      brasero_data_session_disc_removed_cb,
	                                      object);
	g_object_unref (monitor);

	if (priv->loaded) {
		g_object_unref (priv->loaded);
		priv->loaded = NULL;
	}

	if (priv->media) {
		g_slist_foreach (priv->media, (GFunc) g_object_unref, NULL);
		g_slist_free (priv->media);
		priv->media = NULL;
	}

	if (priv->nodes) {
		g_slist_free (priv->nodes);
		priv->nodes = NULL;
	}

	/* NOTE no need to clean up size_changed_sig since it's connected to 
	 * ourselves. It disappears with use. */

	brasero_data_session_stop_io (BRASERO_DATA_SESSION (object));

	/* don't care about the nodes since they will be automatically
	 * destroyed */

	G_OBJECT_CLASS (brasero_data_session_parent_class)->finalize (object);
}


static void
brasero_data_session_class_init (BraseroDataSessionClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	BraseroDataProjectClass *project_class = BRASERO_DATA_PROJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroDataSessionPrivate));

	object_class->finalize = brasero_data_session_finalize;

	project_class->reset = brasero_data_session_reset;

	brasero_data_session_signals [AVAILABLE_SIGNAL] = 
	    g_signal_new ("session_available",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST,
			  0,
			  NULL, NULL,
			  brasero_marshal_VOID__OBJECT_BOOLEAN,
			  G_TYPE_NONE,
			  2,
			  G_TYPE_OBJECT,
			  G_TYPE_BOOLEAN);
	brasero_data_session_signals [LOADED_SIGNAL] = 
	    g_signal_new ("session_loaded",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST,
			  0,
			  NULL, NULL,
			  brasero_marshal_VOID__OBJECT_BOOLEAN,
			  G_TYPE_NONE,
			  2,
			  G_TYPE_OBJECT,
			  G_TYPE_BOOLEAN);
}
