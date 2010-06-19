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
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <gio/gio.h>

#include <gmodule.h>

#include "brasero-units.h"
#include "burn-job.h"
#include "brasero-plugin-registration.h"

#include "brasero-track.h"
#include "brasero-track-data.h"
#include "brasero-track-image.h"


#define BRASERO_TYPE_BURN_URI         (brasero_burn_uri_get_type ())
#define BRASERO_BURN_URI(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_BURN_URI, BraseroBurnURI))
#define BRASERO_BURN_URI_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_BURN_URI, BraseroBurnURIClass))
#define BRASERO_IS_BURN_URI(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_BURN_URI))
#define BRASERO_IS_BURN_URI_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_BURN_URI))
#define BRASERO_BURN_URI_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_BURN_URI, BraseroBurnURIClass))

BRASERO_PLUGIN_BOILERPLATE (BraseroBurnURI, brasero_burn_uri, BRASERO_TYPE_JOB, BraseroJob);

struct _BraseroBurnURIPrivate {
	GCancellable *cancel;

	BraseroTrack *track;

	guint thread_id;
	GThread *thread;
	GMutex *mutex;
	GCond *cond;

	GError *error;
};
typedef struct _BraseroBurnURIPrivate BraseroBurnURIPrivate;

#define BRASERO_BURN_URI_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_BURN_URI, BraseroBurnURIPrivate))

static GObjectClass *parent_class = NULL;


static gboolean
brasero_burn_uri_thread_finished (BraseroBurnURI *self)
{
	BraseroBurnURIPrivate *priv;

	priv = BRASERO_BURN_URI_PRIVATE (self);

	priv->thread_id = 0;

	if (priv->cancel) {
		g_object_unref (priv->cancel);
		priv->cancel = NULL;
		if (g_cancellable_is_cancelled (priv->cancel))
			return FALSE;
	}

	if (priv->error) {
		GError *error;

		error = priv->error;
		priv->error = NULL;
		brasero_job_error (BRASERO_JOB (self), error);
		return FALSE;
	}

	brasero_job_add_track (BRASERO_JOB (self), priv->track);
	brasero_job_finished_track (BRASERO_JOB (self));

	return FALSE;
}

static gint
brasero_burn_uri_find_graft (gconstpointer A, gconstpointer B)
{
	const BraseroGraftPt *graft = A;

	if (graft && graft->path)
		return strcmp (graft->path, B);

	return 1;
}

static GSList *
brasero_burn_uri_explore_directory (BraseroBurnURI *self,
				    GSList *grafts,
				    GFile *file,
				    const gchar *path,
				    GCancellable *cancel,
				    GError **error)
{
	BraseroTrack *current = NULL;
	GFileEnumerator *enumerator;
	GSList *current_grafts;
	GFileInfo *info;

	enumerator = g_file_enumerate_children (file,
						G_FILE_ATTRIBUTE_STANDARD_NAME ","
						G_FILE_ATTRIBUTE_STANDARD_TYPE ","
						"burn::backing-file",
						G_FILE_QUERY_INFO_NONE,
						cancel,
						error);

	if (!enumerator) {
		g_slist_foreach (grafts, (GFunc) brasero_graft_point_free, NULL);
		g_slist_free (grafts);
		return NULL;
	}

	brasero_job_get_current_track (BRASERO_JOB (self), &current);
	current_grafts = brasero_track_data_get_grafts (BRASERO_TRACK_DATA (current));

	while ((info = g_file_enumerator_next_file (enumerator, cancel, error))) {
		if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
			gchar *disc_path;
			GFile *directory;
			BraseroGraftPt *graft;

			/* Make sure it's not one of the original grafts */
			/* we need to know if that's a directory or not since if
			 * it is then mkisofs (but not genisoimage) requires the
			 * disc path to end with '/'; if there isn't '/' at the 
			 * end then only the directory contents are added. */
			disc_path = g_build_filename (path, g_file_info_get_name (info), G_DIR_SEPARATOR_S, NULL);
			if (g_slist_find_custom (current_grafts, disc_path, (GCompareFunc) brasero_burn_uri_find_graft)) {
				BRASERO_JOB_LOG (self, "Graft already in list %s", disc_path);
				g_object_unref (info);
				g_free (disc_path);
				continue;
			}

			/* we need a dummy directory */
			graft = g_new0 (BraseroGraftPt, 1);
			graft->uri = NULL;
			graft->path = disc_path;
			grafts = g_slist_prepend (grafts, graft);

			BRASERO_JOB_LOG (self, "Adding directory %s at %s", graft->uri, graft->path);

			directory = g_file_get_child (file, g_file_info_get_name (info));
			grafts = brasero_burn_uri_explore_directory (self,
								     grafts,
								     directory,
								     graft->path,
								     cancel,
								     error);
			g_object_unref (directory);

			if (!grafts) {
				g_object_unref (info);
				g_object_unref (enumerator);
				return NULL;
			}
		}
		else if (g_file_info_get_file_type (info) == G_FILE_TYPE_REGULAR
		     /* NOTE: burn:// URI allows symlink */
		     ||  g_file_info_get_file_type (info) == G_FILE_TYPE_SYMBOLIC_LINK) {
			const gchar *real_path;
			BraseroGraftPt *graft;
			gchar *disc_path;

			real_path = g_file_info_get_attribute_byte_string (info, "burn::backing-file");
			if (!real_path) {
				g_set_error (error,
					     BRASERO_BURN_ERROR,
					     BRASERO_BURN_ERROR_GENERAL,
					     _("Impossible to retrieve local file path"));

				g_slist_foreach (grafts, (GFunc) brasero_graft_point_free, NULL);
				g_slist_free (grafts);
				g_object_unref (info);
				g_object_unref (file);
				return NULL;
			}

			/* Make sure it's not one of the original grafts */
			disc_path = g_build_filename (path, g_file_info_get_name (info), NULL);
			if (g_slist_find_custom (current_grafts, disc_path, (GCompareFunc) brasero_burn_uri_find_graft)) {
				BRASERO_JOB_LOG (self, "Graft already in list %s", disc_path);
				g_object_unref (info);
				g_free (disc_path);
				continue;
			}

			graft = g_new0 (BraseroGraftPt, 1);
			graft->path = disc_path;
			graft->uri = g_strdup (real_path);
			/* FIXME: maybe one day, graft->uri will always be an URI */
			/* graft->uri = g_filename_to_uri (real_path, NULL, NULL); */

			/* Make sure it's not one of the original grafts */
			
			grafts = g_slist_prepend (grafts, graft);

			BRASERO_JOB_LOG (self, "Added file %s at %s", graft->uri, graft->path);
		}

		g_object_unref (info);
	}
	g_object_unref (enumerator);

	return grafts;
}

static gboolean
brasero_burn_uri_retrieve_path (BraseroBurnURI *self,
				const gchar *uri,
				gchar **path)
{
	GFile *file;
	GFileInfo *info;
	BraseroBurnURIPrivate *priv;

	priv = BRASERO_BURN_URI_PRIVATE (self);

	if (!uri)
		return FALSE;

	file = g_file_new_for_uri (uri);
	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_STANDARD_NAME ","
				  G_FILE_ATTRIBUTE_STANDARD_TYPE ","
				  "burn::backing-file",
				  G_FILE_QUERY_INFO_NONE,
				  priv->cancel,
				  &priv->error);

	if (priv->error) {
		g_object_unref (file);
		return FALSE;
	}

	if (g_cancellable_is_cancelled (priv->cancel)) {
		g_object_unref (file);
		return FALSE;
	}

	if (!info) {
		/* Error */
		g_object_unref (file);
		g_object_unref (info);
		return FALSE;
	}
		
	if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
		*path = NULL;
	}
	else if (g_file_info_get_file_type (info) == G_FILE_TYPE_REGULAR
	     /* NOTE: burn:// URI allows symlink */
	     ||  g_file_info_get_file_type (info) == G_FILE_TYPE_SYMBOLIC_LINK) {
		const gchar *real_path;

		real_path = g_file_info_get_attribute_byte_string (info, "burn::backing-file");
		if (!real_path) {
			priv->error = g_error_new (BRASERO_BURN_ERROR,
						   BRASERO_BURN_ERROR_GENERAL,
						   _("Impossible to retrieve local file path"));
			g_object_unref (info);
			g_object_unref (file);
			return FALSE;
		}

		*path = g_strdup (real_path);
	}

	g_object_unref (file);
	g_object_unref (info);
	return TRUE;
}

static gpointer
brasero_burn_uri_thread (gpointer data)
{
	BraseroBurnURI *self = BRASERO_BURN_URI (data);
	BraseroTrack *current = NULL;
	BraseroBurnURIPrivate *priv;
	BraseroTrackData *track;
	GSList *excluded = NULL;
	GSList *grafts = NULL;
	guint64 num = 0;
	GSList *src;

	priv = BRASERO_BURN_URI_PRIVATE (self);
	brasero_job_set_current_action (BRASERO_JOB (self),
					BRASERO_BURN_ACTION_FILE_COPY,
					_("Copying files locally"),
					TRUE);

	brasero_job_get_current_track (BRASERO_JOB (self), &current);

	/* This is for IMAGE tracks */
	if (BRASERO_IS_TRACK_IMAGE (current)) {
		gchar *uri;
		gchar *path_toc;
		gchar *path_image;
		goffset blocks = 0;
		BraseroTrackImage *image;

		path_image = NULL;
		uri = brasero_track_image_get_source (BRASERO_TRACK_IMAGE (current), TRUE);
		if (!brasero_burn_uri_retrieve_path (self, uri, &path_image)) {
			g_free (uri);
			goto end;
		}
		g_free (uri);

		path_toc = NULL;
		uri = brasero_track_image_get_toc_source (BRASERO_TRACK_IMAGE (current), TRUE);
		if (uri) {
			/* NOTE: if it's a .bin image there is not .toc file */
			if (!brasero_burn_uri_retrieve_path (self, uri, &path_toc)) {
				g_free (path_image);
				g_free (uri);
				goto end;
			}
			g_free (uri);
		}

		brasero_track_get_size (current, &blocks, NULL);

		image = brasero_track_image_new ();
		brasero_track_tag_copy_missing (BRASERO_TRACK (image), current);
		brasero_track_image_set_source (image,
						path_image,
						path_toc,
						brasero_track_image_get_format (BRASERO_TRACK_IMAGE (current)));
		brasero_track_image_set_block_num (image, blocks);

		priv->track = BRASERO_TRACK (image);

		g_free (path_toc);
		g_free (path_image);
		goto end;
	}

	/* This is for DATA tracks */
	for (src = brasero_track_data_get_grafts (BRASERO_TRACK_DATA (current)); src; src = src->next) {
		GFile *file;
		GFileInfo *info;
		BraseroGraftPt *graft;

		graft = src->data;

		if (!graft->uri) {
			grafts = g_slist_prepend (grafts, brasero_graft_point_copy (graft));
			continue;
		}

		if (!g_str_has_prefix (graft->uri, "burn://")) {
			grafts = g_slist_prepend (grafts, brasero_graft_point_copy (graft));
			continue;
		}

		BRASERO_JOB_LOG (self, "Information retrieval for %s", graft->uri);

		file = g_file_new_for_uri (graft->uri);
		info = g_file_query_info (file,
					  G_FILE_ATTRIBUTE_STANDARD_NAME ","
					  G_FILE_ATTRIBUTE_STANDARD_TYPE ","
					  "burn::backing-file",
					  G_FILE_QUERY_INFO_NONE,
					  priv->cancel,
					  &priv->error);

		if (priv->error) {
			g_object_unref (file);
			goto end;
		}

		if (g_cancellable_is_cancelled (priv->cancel)) {
			g_object_unref (file);
			goto end;
		}

		if (!info) {
			/* Error */
			g_object_unref (file);
			g_object_unref (info);
			goto end;
		}

		/* See if we were passed the burn:/// uri itself (the root).
		 * Then skip graft point addition */
		if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
			if (g_file_info_get_name (info)
			&&  strcmp (g_file_info_get_name (info), "/")) {
				BraseroGraftPt *newgraft;

				/* we need a dummy directory */
				newgraft = g_new0 (BraseroGraftPt, 1);
				newgraft->uri = NULL;
				newgraft->path = g_strdup (graft->path);
				grafts = g_slist_prepend (grafts, newgraft);

				BRASERO_JOB_LOG (self,
						 "Adding directory %s at %s",
						 newgraft->uri,
						 newgraft->path);
				grafts = brasero_burn_uri_explore_directory (self,
									     grafts,
									     file,
									     newgraft->path,
									     priv->cancel,
									     &priv->error);
			}
			else {
				BRASERO_JOB_LOG (self, "Directory is root");
				grafts = brasero_burn_uri_explore_directory (self,
									     grafts,
									     file,
									     "/",
									     priv->cancel,
									     &priv->error);
			}

			if (!grafts) {
				g_object_unref (info);
				g_object_unref (file);
				goto end;
			}
		}
		else if (g_file_info_get_file_type (info) == G_FILE_TYPE_REGULAR
		     /* NOTE: burn:// URI allows symlink */
		     ||  g_file_info_get_file_type (info) == G_FILE_TYPE_SYMBOLIC_LINK) {
			const gchar *real_path;
			BraseroGraftPt *newgraft;

			real_path = g_file_info_get_attribute_byte_string (info, "burn::backing-file");
			if (!real_path) {
				priv->error = g_error_new (BRASERO_BURN_ERROR,
							   BRASERO_BURN_ERROR_GENERAL,
							   _("Impossible to retrieve local file path"));

				g_slist_foreach (grafts, (GFunc) brasero_graft_point_free, NULL);
				g_slist_free (grafts);
				g_object_unref (info);
				g_object_unref (file);
				goto end;
			}

			newgraft = brasero_graft_point_copy (graft);
			g_free (newgraft->uri);

			newgraft->uri = g_strdup (real_path);
			/* FIXME: maybe one day, graft->uri will always be an URI */
			/* newgraft->uri = g_filename_to_uri (real_path, NULL, NULL); */

			BRASERO_JOB_LOG (self,
					 "Added file %s at %s",
					 newgraft->uri,
					 newgraft->path);
			grafts = g_slist_prepend (grafts, newgraft);
		}

		g_object_unref (info);
		g_object_unref (file);
	}
	grafts = g_slist_reverse (grafts);

	/* remove all excluded starting by burn:// from the list */
	for (src = brasero_track_data_get_excluded_list (BRASERO_TRACK_DATA (current)); src; src = src->next) {
		gchar *uri;

		uri = src->data;

		if (uri && g_str_has_prefix (uri, "burn://"))
			continue;

		uri = g_strdup (uri);
		excluded = g_slist_prepend (excluded, uri);

		BRASERO_JOB_LOG (self, "Added excluded file %s", uri);
	}
	excluded = g_slist_reverse (excluded);

	track = brasero_track_data_new ();
	brasero_track_tag_copy_missing (BRASERO_TRACK (track), current);
	
	brasero_track_data_add_fs (track, brasero_track_data_get_fs (BRASERO_TRACK_DATA (current)));

	brasero_track_data_get_file_num (BRASERO_TRACK_DATA (current), &num);
	brasero_track_data_set_file_num (track, num);

	brasero_track_data_set_source (track,
				       grafts,
				       excluded);
	priv->track = BRASERO_TRACK (track);

end:

	if (!g_cancellable_is_cancelled (priv->cancel))
		priv->thread_id = g_idle_add ((GSourceFunc) brasero_burn_uri_thread_finished, self);

	/* End thread */
	g_mutex_lock (priv->mutex);
	g_atomic_pointer_set (&priv->thread, NULL);
	g_cond_signal (priv->cond);
	g_mutex_unlock (priv->mutex);

	g_thread_exit (NULL);

	return NULL;
}

static BraseroBurnResult
brasero_burn_uri_start_thread (BraseroBurnURI *self,
			       GError **error)
{
	BraseroBurnURIPrivate *priv;
	GError *thread_error = NULL;

	priv = BRASERO_BURN_URI_PRIVATE (self);

	if (priv->thread)
		return BRASERO_BURN_RUNNING;

	priv->cancel = g_cancellable_new ();

	g_mutex_lock (priv->mutex);
	priv->thread = g_thread_create (brasero_burn_uri_thread,
					self,
					FALSE,
					&thread_error);
	g_mutex_unlock (priv->mutex);

	/* Reminder: this is not necessarily an error as the thread may have finished */
	//if (!priv->thread)
	//	return BRASERO_BURN_ERR;
	if (thread_error) {
		g_propagate_error (error, thread_error);
		return BRASERO_BURN_ERR;
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_uri_start_if_found (BraseroBurnURI *self,
				 const gchar *uri,
				 GError **error)
{
	if (!uri)
		return BRASERO_BURN_NOT_RUNNING;

	/* Find any graft point with burn:// URI */
	if (!g_str_has_prefix (uri, "burn://"))
		return BRASERO_BURN_NOT_RUNNING;

	BRASERO_JOB_LOG (self, "burn:// URI found %s", uri);
	brasero_burn_uri_start_thread (self, error);
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_uri_start (BraseroJob *job,
			GError **error)
{
	BraseroBurnURIPrivate *priv;
	BraseroBurnResult result;
	BraseroJobAction action;
	BraseroBurnURI *self;
	BraseroTrack *track;
	GSList *grafts;
	gchar *uri;

	self = BRASERO_BURN_URI (job);
	priv = BRASERO_BURN_URI_PRIVATE (self);

	/* skip that part */
	brasero_job_get_action (job, &action);
	if (action == BRASERO_JOB_ACTION_SIZE) {
		/* say we won't write to disc */
		brasero_job_set_output_size_for_current_track (job, 0, 0);
		return BRASERO_BURN_NOT_RUNNING;
	}

	if (action != BRASERO_JOB_ACTION_IMAGE)
		return BRASERO_BURN_NOT_SUPPORTED;

	/* can't be piped so brasero_job_get_current_track will work */
	brasero_job_get_current_track (job, &track);

	result = BRASERO_BURN_NOT_RUNNING;

	/* make a list of all non local uris to be downloaded and put them in a
	 * list to avoid to download the same file twice. */
	if (BRASERO_IS_TRACK_DATA (track)) {
		/* we put all the non local graft point uris in the hash */
		grafts = brasero_track_data_get_grafts (BRASERO_TRACK_DATA (track));
		for (; grafts; grafts = grafts->next) {
			BraseroGraftPt *graft;

			graft = grafts->data;
			result = brasero_burn_uri_start_if_found (self, graft->uri, error);
			if (result != BRASERO_BURN_NOT_RUNNING)
				break;
		}
	}
	else if (BRASERO_IS_TRACK_IMAGE (track)) {
		/* NOTE: don't delete URI as they will be inserted in hash */
		uri = brasero_track_image_get_source (BRASERO_TRACK_IMAGE (track), TRUE);
		result = brasero_burn_uri_start_if_found (self, uri, error);
		g_free (uri);

		if (result == BRASERO_BURN_NOT_RUNNING) {
			uri = brasero_track_image_get_toc_source (BRASERO_TRACK_IMAGE (track), TRUE);
			result = brasero_burn_uri_start_if_found (self, uri, error);
			g_free (uri);
		}
	}
	else
		BRASERO_JOB_NOT_SUPPORTED (self);

	if (!priv->thread)
		BRASERO_JOB_LOG (self, "no burn:// URI found");

	return result;
}

static BraseroBurnResult
brasero_burn_uri_stop (BraseroJob *job,
		       GError **error)
{
	BraseroBurnURIPrivate *priv = BRASERO_BURN_URI_PRIVATE (job);

	if (priv->cancel) {
		/* signal that we've been cancelled */
		g_cancellable_cancel (priv->cancel);
	}

	g_mutex_lock (priv->mutex);
	if (priv->thread)
		g_cond_wait (priv->cond, priv->mutex);
	g_mutex_unlock (priv->mutex);

	if (priv->cancel) {
		/* unref it after the thread has stopped */
		g_object_unref (priv->cancel);
		priv->cancel = NULL;
	}

	if (priv->thread_id) {
		g_source_remove (priv->thread_id);
		priv->thread_id = 0;
	}

	if (priv->error) {
		g_error_free (priv->error);
		priv->error = NULL;
	}

	return BRASERO_BURN_OK;
}

static void
brasero_burn_uri_finalize (GObject *object)
{
	BraseroBurnURIPrivate *priv = BRASERO_BURN_URI_PRIVATE (object);

	if (priv->mutex) {
		g_mutex_free (priv->mutex);
		priv->mutex = NULL;
	}

	if (priv->cond) {
		g_cond_free (priv->cond);
		priv->cond = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_burn_uri_class_init (BraseroBurnURIClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroJobClass *job_class = BRASERO_JOB_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroBurnURIPrivate));

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_burn_uri_finalize;

	job_class->start = brasero_burn_uri_start;
	job_class->stop = brasero_burn_uri_stop;
}

static void
brasero_burn_uri_init (BraseroBurnURI *obj)
{
	BraseroBurnURIPrivate *priv = BRASERO_BURN_URI_PRIVATE (obj);

	priv->mutex = g_mutex_new ();
	priv->cond = g_cond_new ();
}

static void
brasero_burn_uri_export_caps (BraseroPlugin *plugin)
{
	GSList *caps;

	brasero_plugin_define (plugin,
	                       "burn-uri",
			       /* Translators: this is the name of the plugin
				* which will be translated only when it needs
				* displaying. */
			       N_("CD/DVD Creator Folder"),
			       _("Allows files added to the \"CD/DVD Creator Folder\" in Nautilus to be burned"),
			       "Philippe Rouquier",
			       11);

	caps = brasero_caps_image_new (BRASERO_PLUGIN_IO_ACCEPT_FILE,
				       BRASERO_IMAGE_FORMAT_ANY);
	brasero_plugin_process_caps (plugin, caps);
	g_slist_free (caps);

	caps = brasero_caps_data_new (BRASERO_IMAGE_FS_ANY);
	brasero_plugin_process_caps (plugin, caps);
	g_slist_free (caps);

	brasero_plugin_set_process_flags (plugin, BRASERO_PLUGIN_RUN_PREPROCESSING);
}
