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

/* This is for getline() and isblank() */
#define _GNU_SOURCE

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <gio/gio.h>

#include <gmodule.h>

#include "burn-job.h"
#include "brasero-plugin-registration.h"
#include "brasero-xfer.h"
#include "brasero-track-image.h"


#define BRASERO_TYPE_LOCAL_TRACK         (brasero_local_track_get_type ())
#define BRASERO_LOCAL_TRACK(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_LOCAL_TRACK, BraseroLocalTrack))
#define BRASERO_LOCAL_TRACK_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_LOCAL_TRACK, BraseroLocalTrackClass))
#define BRASERO_IS_LOCAL_TRACK(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_LOCAL_TRACK))
#define BRASERO_IS_LOCAL_TRACK_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_LOCAL_TRACK))
#define BRASERO_LOCAL_TRACK_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_LOCAL_TRACK, BraseroLocalTrackClass))

BRASERO_PLUGIN_BOILERPLATE (BraseroLocalTrack, brasero_local_track, BRASERO_TYPE_JOB, BraseroJob);

struct _BraseroLocalTrackPrivate {
	GCancellable *cancel;
	BraseroXferCtx *xfer_ctx;

	gchar *checksum;
	gchar *checksum_path;
	GChecksumType gchecksum_type;
	BraseroChecksumType checksum_type;

	GHashTable *nonlocals;

	guint thread_id;
	GThread *thread;
	GMutex *mutex;
	GCond *cond;

	GSList *src_list;
	GSList *dest_list;

	GError *error;

	guint download_checksum:1;
};
typedef struct _BraseroLocalTrackPrivate BraseroLocalTrackPrivate;

#define BRASERO_LOCAL_TRACK_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_LOCAL_TRACK, BraseroLocalTrackPrivate))

static GObjectClass *parent_class = NULL;

static BraseroBurnResult
brasero_local_track_clock_tick (BraseroJob *job)
{
	BraseroLocalTrackPrivate *priv;
	goffset written = 0;
	goffset total = 0;

	priv = BRASERO_LOCAL_TRACK_PRIVATE (job);

	if (!priv->xfer_ctx)
		return BRASERO_BURN_OK;

	brasero_job_start_progress (job, FALSE);

	brasero_xfer_get_progress (priv->xfer_ctx,
				   &written,
				   &total);
	brasero_job_set_progress (job, (gdouble) written / (gdouble) total);

	return BRASERO_BURN_OK;
}

/**
 * That's for URI translation ...
 */

static gchar *
brasero_local_track_translate_uri (BraseroLocalTrack *self,
				   const gchar *uri)
{
	gchar *newuri;
	gchar *parent;
	BraseroLocalTrackPrivate *priv;

	priv = BRASERO_LOCAL_TRACK_PRIVATE (self);

	if (!uri)
		return NULL;

	/* see if it is a local file */
	if (g_str_has_prefix (uri, "file://"))
		return g_strdup (uri);

	/* see if it was downloaded itself */
	newuri = g_hash_table_lookup (priv->nonlocals, uri);
	if (newuri) {
		/* we copy this string as it will be freed when freeing 
		 * downloaded GSList */
		return g_strdup (newuri);
	}

	/* see if one of its parent will be downloaded */
	parent = g_path_get_dirname (uri);
	while (parent [1] != '\0') {
		gchar *tmp;

		tmp = g_hash_table_lookup (priv->nonlocals, parent);
		if (tmp) {
			newuri = g_build_path (G_DIR_SEPARATOR_S,
					       tmp,
					       uri + strlen (parent),
					       NULL);
			g_free (parent);
			return newuri;
		}

		tmp = parent;
		parent = g_path_get_dirname (tmp);
		g_free (tmp);
	}

	/* that should not happen */
	BRASERO_JOB_LOG (self, "Can't find a downloaded parent for %s", uri);

	g_free (parent);
	return NULL;
}

static BraseroBurnResult
brasero_local_track_read_checksum (BraseroLocalTrack *self)
{
	gint size;
	FILE *file;
	gchar *name;
	gchar *source;
	gchar *line = NULL;
	size_t line_len = 0;
	BraseroTrack *track = NULL;
	BraseroLocalTrackPrivate *priv;

	priv = BRASERO_LOCAL_TRACK_PRIVATE (self);

	/* get the file_checksum from the checksum file */
	file = fopen (priv->checksum_path, "r");

	/* get the name of the file that was downloaded */
	brasero_job_get_current_track (BRASERO_JOB (self), &track);
	source = brasero_track_image_get_source (BRASERO_TRACK_IMAGE (track), TRUE);
	name = g_path_get_basename (source);
	g_free (source);

	if (!file) {
		g_free (name);
		BRASERO_JOB_LOG (self, "Impossible to open the downloaded checksum file");
		return BRASERO_BURN_ERR;
	}

	/* find a line with the name of our file (there could be several ones) */
	BRASERO_JOB_LOG (self, "Searching %s in file", name);
	while ((size = getline (&line, &line_len, file)) > 0) {
		if (strstr (line, name)) {
			gchar *ptr;

			/* Skip blanks */
			ptr = line;
			while (isblank (*ptr)) { ptr ++; size --; }

			if (g_checksum_type_get_length (priv->checksum_type) * 2 > size) {
				g_free (line);
				line = NULL;
				continue;
			}
	
			ptr [g_checksum_type_get_length (priv->gchecksum_type) * 2] = '\0';
			priv->checksum = g_strdup (ptr);
			g_free (line);
			BRASERO_JOB_LOG (self, "Found checksum %s", priv->checksum);
			break;
		}

		g_free (line);
		line = NULL;
	}
	fclose (file);

	return (priv->checksum? BRASERO_BURN_OK:BRASERO_BURN_ERR);
}

static BraseroBurnResult
brasero_local_track_download_checksum (BraseroLocalTrack *self)
{
	BraseroLocalTrackPrivate *priv;
	BraseroBurnResult result;
	BraseroTrack *track;
	gchar *checksum_src;
	GFile *src, *dest;
	GFile *tmp;
	gchar *uri;

	priv = BRASERO_LOCAL_TRACK_PRIVATE (self);

	BRASERO_JOB_LOG (self, "Copying checksum file");

	/* generate a unique name for destination */
	result = brasero_job_get_tmp_file (BRASERO_JOB (self),
					   NULL,
					   &priv->checksum_path,
					   NULL);
	if (result != BRASERO_BURN_OK)
		goto error;

	brasero_job_set_current_action (BRASERO_JOB (self),
					BRASERO_BURN_ACTION_FILE_COPY,
					_("Copying checksum file"),
					TRUE);

	/* This is an image. See if there is any checksum sitting in the same
	 * directory to check our download integrity.
	 * Try all types of checksum. */
	brasero_job_get_current_track (BRASERO_JOB (self), &track);
	uri = brasero_track_image_get_source (BRASERO_TRACK_IMAGE (track), TRUE);
	dest = g_file_new_for_path (priv->checksum_path);

	/* Try with three difference sources */
	checksum_src = g_strdup_printf ("%s.md5", uri);
	src = g_file_new_for_uri (checksum_src);
	g_free (checksum_src);

	result = brasero_xfer_start (priv->xfer_ctx,
				     src,
				     dest,
				     priv->cancel,
				     NULL);
	g_object_unref (src);

	if (result == BRASERO_BURN_OK) {
		priv->gchecksum_type = G_CHECKSUM_MD5;
		priv->checksum_type = BRASERO_CHECKSUM_MD5;
		goto end;
	}

	checksum_src = g_strdup_printf ("%s.sha1", uri);
	src = g_file_new_for_uri (checksum_src);
	g_free (checksum_src);

	result = brasero_xfer_start (priv->xfer_ctx,
				     src,
				     dest,
				     priv->cancel,
				     NULL);
	g_object_unref (src);

	if (result == BRASERO_BURN_OK) {
		priv->gchecksum_type = G_CHECKSUM_SHA1;
		priv->checksum_type = BRASERO_CHECKSUM_SHA1;
		goto end;
	}

	checksum_src = g_strdup_printf ("%s.sha256", uri);
	src = g_file_new_for_uri (checksum_src);
	g_free (checksum_src);

	result = brasero_xfer_start (priv->xfer_ctx,
				     src,
				     dest,
				     priv->cancel,
				     NULL);
	g_object_unref (src);

	if (result == BRASERO_BURN_OK) {
		priv->gchecksum_type = G_CHECKSUM_SHA256;
		priv->checksum_type = BRASERO_CHECKSUM_SHA256;
		goto end;
	}

	/* There are also ftp sites that includes all images checksum in one big
	 * SHA1 file. */
	tmp = g_file_new_for_uri (uri);
	src = g_file_get_parent (tmp);
	g_object_unref (tmp);

	tmp = src;
	src = g_file_get_child_for_display_name (tmp, "SHA1SUM", NULL);
	g_object_unref (tmp);

	result = brasero_xfer_start (priv->xfer_ctx,
				     src,
				     dest,
				     priv->cancel,
				     NULL);
	g_object_unref (src);

	if (result != BRASERO_BURN_OK) {
		g_free (uri);
		g_object_unref (dest);
		goto error;
	}

	priv->gchecksum_type = G_CHECKSUM_SHA1;
	priv->checksum_type = BRASERO_CHECKSUM_SHA1;

end:

	g_object_unref (dest);
	g_free (uri);

	return result;

error:

	/* we give up */
	BRASERO_JOB_LOG (self, "No checksum file available");
	g_free (priv->checksum_path);
	priv->checksum_path = NULL;
	return result;
}

static BraseroBurnResult
brasero_local_track_update_track (BraseroLocalTrack *self)
{
	BraseroTrack *track = NULL;
	BraseroTrack *current = NULL;
	BraseroLocalTrackPrivate *priv;

	priv = BRASERO_LOCAL_TRACK_PRIVATE (self);

	/* now we update all the track with the local uris in retval */
	brasero_job_get_current_track (BRASERO_JOB (self), &current);

	/* make a copy of the tracks instead of modifying them */
	if (BRASERO_IS_TRACK_DATA (current)) {
		GSList *next;
		GSList *grafts;
		GSList *unreadable;
		guint64 file_num = 0;

		track = BRASERO_TRACK (brasero_track_data_new ());
		brasero_track_tag_copy_missing (BRASERO_TRACK (track), current);

		brasero_track_data_add_fs (BRASERO_TRACK_DATA (track), brasero_track_data_get_fs (BRASERO_TRACK_DATA (current)));

		brasero_track_data_get_file_num (BRASERO_TRACK_DATA (current), &file_num);
		brasero_track_data_set_file_num (BRASERO_TRACK_DATA (track), file_num);

		grafts = brasero_track_data_get_grafts (BRASERO_TRACK_DATA (current));
		for (; grafts; grafts = grafts->next) {
			BraseroGraftPt *graft;
			gchar *uri;

			graft = grafts->data;
			uri = brasero_local_track_translate_uri (self, graft->uri);
			if (uri) {
				g_free (graft->uri);
				graft->uri = uri;
			}
		}

		BRASERO_JOB_LOG (self, "Translating unreadable");

		/* Translate the globally excluded.
		 * NOTE: if we can't find a parent for an excluded URI that
		 * means it shouldn't be included. */
		unreadable = brasero_track_data_get_excluded_list (BRASERO_TRACK_DATA (current));
		for (; unreadable; unreadable = next) {
			gchar *new_uri;

			next = unreadable->next;
			new_uri = brasero_local_track_translate_uri (self, unreadable->data);
			g_free (unreadable->data);

			if (new_uri)
				unreadable->data = new_uri;
			else
				unreadable = g_slist_remove (unreadable, unreadable->data);
		}
	}
	else if (BRASERO_IS_TRACK_STREAM (current)) {
		gchar *uri;
		gchar *newuri;

		uri = brasero_track_stream_get_source (BRASERO_TRACK_STREAM (current), TRUE);
		newuri = brasero_local_track_translate_uri (self, uri);

		track = BRASERO_TRACK (brasero_track_stream_new ());
		brasero_track_tag_copy_missing (BRASERO_TRACK (track), current);
		brasero_track_stream_set_source (BRASERO_TRACK_STREAM (track), newuri);
		brasero_track_stream_set_format (BRASERO_TRACK_STREAM (track), brasero_track_stream_get_format (BRASERO_TRACK_STREAM (current)));
		brasero_track_stream_set_boundaries (BRASERO_TRACK_STREAM (track),
						     brasero_track_stream_get_start (BRASERO_TRACK_STREAM (current)),
						     brasero_track_stream_get_end (BRASERO_TRACK_STREAM (current)),
						     brasero_track_stream_get_gap (BRASERO_TRACK_STREAM (current)));
		g_free (uri);
	}
	else if (BRASERO_IS_TRACK_IMAGE (current)) {
		gchar *uri;
		gchar *newtoc;
		gchar *newimage;
		goffset blocks = 0;

		uri = brasero_track_image_get_source (BRASERO_TRACK_IMAGE (current), TRUE);
		newimage = brasero_local_track_translate_uri (self, uri);
		g_free (uri);

		uri = brasero_track_image_get_toc_source (BRASERO_TRACK_IMAGE (current), TRUE);
		newtoc = brasero_local_track_translate_uri (self, uri);
		g_free (uri);

		brasero_track_get_size (current, &blocks, NULL);

		track = BRASERO_TRACK (brasero_track_image_new ());
		brasero_track_tag_copy_missing (BRASERO_TRACK (track), current);
		brasero_track_image_set_source (BRASERO_TRACK_IMAGE (track),
						newimage,
						newtoc,
						brasero_track_image_get_format (BRASERO_TRACK_IMAGE (current)));
		brasero_track_image_set_block_num (BRASERO_TRACK_IMAGE (track), blocks);
	}
	else
		BRASERO_JOB_NOT_SUPPORTED (self);

	if (priv->download_checksum)
		brasero_track_set_checksum (track,
					    priv->checksum_type,
					    priv->checksum);

	brasero_job_add_track (BRASERO_JOB (self), track);

	/* It's good practice to unref the track afterwards as we don't need it
	 * anymore. BraseroTaskCtx refs it. */
	g_object_unref (track);

	return BRASERO_BURN_OK;
}

static gboolean
brasero_local_track_thread_finished (BraseroLocalTrack *self)
{
	BraseroLocalTrackPrivate *priv;

	priv = BRASERO_LOCAL_TRACK_PRIVATE (self);

	priv->thread_id = 0;

	if (priv->xfer_ctx) {
		brasero_xfer_free (priv->xfer_ctx);
		priv->xfer_ctx = NULL;
	}

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

	brasero_local_track_update_track (self);

	brasero_job_finished_track (BRASERO_JOB (self));
	return FALSE;
}

static gpointer
brasero_local_track_thread (gpointer data)
{
	BraseroLocalTrack *self = BRASERO_LOCAL_TRACK (data);
	BraseroLocalTrackPrivate *priv;
	GSList *src, *dest;

	priv = BRASERO_LOCAL_TRACK_PRIVATE (self);
	brasero_job_set_current_action (BRASERO_JOB (self),
					BRASERO_BURN_ACTION_FILE_COPY,
					_("Copying files locally"),
					TRUE);

	for (src = priv->src_list, dest = priv->dest_list;
	     src && dest;
	     src = src->next, dest = dest->next) {
		gchar *name;
		GFile *src_file;
		GFile *dest_file;
		BraseroBurnResult result;

		src_file = src->data;
		dest_file = dest->data;

		name = g_file_get_basename (src_file);
		BRASERO_JOB_LOG (self, "Downloading %s", name);
		g_free (name);

		result = brasero_xfer_start (priv->xfer_ctx,
					     src_file,
					     dest_file,
					     priv->cancel,
					     &priv->error);

		if (g_cancellable_is_cancelled (priv->cancel))
			goto end;

		if (result != BRASERO_BURN_OK)
			goto end;
	}

	/* successfully downloaded files, get a checksum if we can. */
	if (priv->download_checksum
	&& !priv->checksum_path
	&&  brasero_local_track_download_checksum (self) == BRASERO_BURN_OK)
		brasero_local_track_read_checksum (self);

end:

	if (!g_cancellable_is_cancelled (priv->cancel))
		priv->thread_id = g_idle_add ((GSourceFunc) brasero_local_track_thread_finished, self);

	/* End thread */
	g_mutex_lock (priv->mutex);
	priv->thread = NULL;
	g_cond_signal (priv->cond);
	g_mutex_unlock (priv->mutex);

	g_thread_exit (NULL);

	return NULL;
}

static BraseroBurnResult
brasero_local_track_start_thread (BraseroLocalTrack *self,
				  GError **error)
{
	BraseroLocalTrackPrivate *priv;
	GError *thread_error = NULL;

	priv = BRASERO_LOCAL_TRACK_PRIVATE (self);

	if (priv->thread)
		return BRASERO_BURN_RUNNING;

	priv->cancel = g_cancellable_new ();
	priv->xfer_ctx = brasero_xfer_new ();

	g_mutex_lock (priv->mutex);
	priv->thread = g_thread_create (brasero_local_track_thread,
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

static gboolean
_foreach_non_local_cb (const gchar *uri,
		       const gchar *localuri,
		       gpointer *data)
{
	BraseroLocalTrack *self = BRASERO_LOCAL_TRACK (data);
	BraseroLocalTrackPrivate *priv;
	GFile *file, *tmpfile;
	gchar *parent;
	gchar *tmp;

	priv = BRASERO_LOCAL_TRACK_PRIVATE (data);

	/* check that is hasn't any parent in the hash */
	parent = g_path_get_dirname (uri);
	while (parent [1] != '\0') {
		gchar *uri_local;

		uri_local = g_hash_table_lookup (priv->nonlocals, parent);
		if (uri_local) {
			BRASERO_JOB_LOG (self, "Parent for %s was found %s", uri, parent);
			g_free (parent);
			return TRUE;
		}

		tmp = parent;
		parent = g_path_get_dirname (tmp);
		g_free (tmp);
	}
	g_free (parent);

	file = g_file_new_for_uri (uri);
	priv->src_list = g_slist_append (priv->src_list, file);

	tmpfile = g_file_new_for_uri (localuri);
	priv->dest_list = g_slist_append (priv->dest_list, tmpfile);

	BRASERO_JOB_LOG (self, "%s set to be downloaded to %s", uri, localuri);
	return FALSE;
}

static BraseroBurnResult
brasero_local_track_add_if_non_local (BraseroLocalTrack *self,
				      const gchar *uri,
				      GError **error)
{
	BraseroLocalTrackPrivate *priv;
	BraseroBurnResult result;
	gchar *localuri = NULL;

	priv = BRASERO_LOCAL_TRACK_PRIVATE (self);

	if (!uri
	||   uri [0] == '\0'
	||   uri [0] == '/'
	||   g_str_has_prefix (uri, "file://")
	||   g_str_has_prefix (uri, "burn://"))
		return BRASERO_BURN_OK;

	/* add it to the list or uris to download */
	if (!priv->nonlocals)
		priv->nonlocals = g_hash_table_new_full (g_str_hash,
							 g_str_equal,
							 NULL,
							 g_free);

	/* generate a unique name */
	result = brasero_job_get_tmp_file (BRASERO_JOB (self),
					   NULL,
					   &localuri,
					   error);
	if (result != BRASERO_BURN_OK)
		return result;

	if (!g_str_has_prefix (localuri, "file://")) {
		gchar *tmp;

		tmp = localuri;
		localuri = g_strconcat ("file://", tmp, NULL);
		g_free (tmp);
	}

	/* we don't want to replace it if it has already been downloaded */
	if (!g_hash_table_lookup (priv->nonlocals, uri))
		g_hash_table_insert (priv->nonlocals, g_strdup (uri), localuri);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_local_track_start (BraseroJob *job,
			   GError **error)
{
	BraseroLocalTrackPrivate *priv;
	BraseroBurnResult result;
	BraseroJobAction action;
	BraseroLocalTrack *self;
	BraseroTrack *track;
	GSList *grafts;
	gchar *uri;

	self = BRASERO_LOCAL_TRACK (job);
	priv = BRASERO_LOCAL_TRACK_PRIVATE (self);

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

	result = BRASERO_BURN_OK;

	/* make a list of all non local uris to be downloaded and put them in a
	 * list to avoid to download the same file twice. */
	if (BRASERO_IS_TRACK_DATA (track)) {
		/* we put all the non local graft point uris in the hash */
		grafts = brasero_track_data_get_grafts (BRASERO_TRACK_DATA (track));
		for (; grafts; grafts = grafts->next) {
			BraseroGraftPt *graft;

			graft = grafts->data;
			result = brasero_local_track_add_if_non_local (self, graft->uri, error);
			if (result != BRASERO_BURN_OK)
				break;
		}
	}
	else if (BRASERO_IS_TRACK_STREAM (track)) {
		/* NOTE: don't delete URI as they will be inserted in hash */
		uri = brasero_track_stream_get_source (BRASERO_TRACK_STREAM (track), TRUE);
		result = brasero_local_track_add_if_non_local (self, uri, error);
		g_free (uri);
	}
	else if (BRASERO_IS_TRACK_IMAGE (track)) {
		/* NOTE: don't delete URI as they will be inserted in hash */
		uri = brasero_track_image_get_source (BRASERO_TRACK_IMAGE (track), TRUE);
		result = brasero_local_track_add_if_non_local (self, uri, error);
		g_free (uri);

		if (result == BRASERO_BURN_OK) {
			priv->download_checksum = TRUE;

			uri = brasero_track_image_get_toc_source (BRASERO_TRACK_IMAGE (track), TRUE);
			result = brasero_local_track_add_if_non_local (self, uri, error);
			g_free (uri);
		}
	}
	else
		BRASERO_JOB_NOT_SUPPORTED (self);

	if (result != BRASERO_BURN_OK)
		return result;

	/* see if there is anything to download */
	if (!priv->nonlocals) {
		BRASERO_JOB_LOG (self, "no remote URIs");
		return BRASERO_BURN_NOT_RUNNING;
	}

	/* first we create a list of all the non local files that need to be
	 * downloaded. To be elligible a file must not have one of his parent
	 * in the hash. */
	g_hash_table_foreach_remove (priv->nonlocals,
				     (GHRFunc) _foreach_non_local_cb,
				     job);

	return brasero_local_track_start_thread (self, error);
}

static BraseroBurnResult
brasero_local_track_stop (BraseroJob *job,
			  GError **error)
{
	BraseroLocalTrackPrivate *priv = BRASERO_LOCAL_TRACK_PRIVATE (job);

	if (priv->cancel) {
		/* signal that we've been cancelled */
		g_cancellable_cancel (priv->cancel);
	}

	g_mutex_lock (priv->mutex);
	if (priv->thread)
		g_cond_wait (priv->cond, priv->mutex);
	g_mutex_unlock (priv->mutex);

	if (priv->xfer_ctx) {
		brasero_xfer_free (priv->xfer_ctx);
		priv->xfer_ctx = NULL;
	}

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

	if (priv->src_list) {
		g_slist_foreach (priv->src_list, (GFunc) g_object_unref, NULL);
		g_slist_free (priv->src_list);
		priv->src_list = NULL;
	}

	if (priv->dest_list) {
		g_slist_foreach (priv->dest_list, (GFunc) g_object_unref, NULL);
		g_slist_free (priv->dest_list);
		priv->dest_list = NULL;
	}

	if (priv->nonlocals) {
		g_hash_table_destroy (priv->nonlocals);
		priv->nonlocals = NULL;
	}

	if (priv->checksum_path) {
		g_free (priv->checksum_path);
		priv->checksum_path = NULL;
	}

	if (priv->checksum) {
		g_free (priv->checksum);
		priv->checksum = NULL;
	}

	return BRASERO_BURN_OK;
}

static void
brasero_local_track_finalize (GObject *object)
{
	BraseroLocalTrackPrivate *priv = BRASERO_LOCAL_TRACK_PRIVATE (object);

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
brasero_local_track_class_init (BraseroLocalTrackClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroJobClass *job_class = BRASERO_JOB_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroLocalTrackPrivate));

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_local_track_finalize;

	job_class->start = brasero_local_track_start;
	job_class->stop = brasero_local_track_stop;
	job_class->clock_tick = brasero_local_track_clock_tick;
}

static void
brasero_local_track_init (BraseroLocalTrack *obj)
{
	BraseroLocalTrackPrivate *priv = BRASERO_LOCAL_TRACK_PRIVATE (obj);

	priv->mutex = g_mutex_new ();
	priv->cond = g_cond_new ();
}

static void
brasero_local_track_export_caps (BraseroPlugin *plugin)
{
	GSList *caps;

	brasero_plugin_define (plugin,
	                       "file-downloader",
			       /* Translators: this is the name of the plugin
				* which will be translated only when it needs
				* displaying. */
			       N_("File Downloader"),
			       _("Allows files not stored locally to be burned"),
			       "Philippe Rouquier",
			       10);

	caps = brasero_caps_image_new (BRASERO_PLUGIN_IO_ACCEPT_FILE,
				       BRASERO_IMAGE_FORMAT_ANY);
	brasero_plugin_process_caps (plugin, caps);
	g_slist_free (caps);

	caps = brasero_caps_audio_new (BRASERO_PLUGIN_IO_ACCEPT_FILE,
				       BRASERO_AUDIO_FORMAT_UNDEFINED|
	                               BRASERO_AUDIO_FORMAT_DTS|
				       BRASERO_AUDIO_FORMAT_RAW|
				       BRASERO_AUDIO_FORMAT_RAW_LITTLE_ENDIAN|
				       BRASERO_VIDEO_FORMAT_UNDEFINED|
				       BRASERO_VIDEO_FORMAT_VCD|
				       BRASERO_VIDEO_FORMAT_VIDEO_DVD|
				       BRASERO_AUDIO_FORMAT_AC3|
				       BRASERO_AUDIO_FORMAT_MP2|
				       BRASERO_METADATA_INFO);
	brasero_plugin_process_caps (plugin, caps);
	g_slist_free (caps);

	caps = brasero_caps_audio_new (BRASERO_PLUGIN_IO_ACCEPT_FILE,
				       BRASERO_AUDIO_FORMAT_UNDEFINED|
	                               BRASERO_AUDIO_FORMAT_DTS|
				       BRASERO_AUDIO_FORMAT_RAW|
				       BRASERO_AUDIO_FORMAT_RAW_LITTLE_ENDIAN|
				       BRASERO_VIDEO_FORMAT_UNDEFINED|
				       BRASERO_VIDEO_FORMAT_VCD|
				       BRASERO_VIDEO_FORMAT_VIDEO_DVD|
				       BRASERO_AUDIO_FORMAT_AC3|
				       BRASERO_AUDIO_FORMAT_MP2);
	brasero_plugin_process_caps (plugin, caps);
	g_slist_free (caps);
	caps = brasero_caps_data_new (BRASERO_IMAGE_FS_ANY);
	brasero_plugin_process_caps (plugin, caps);
	g_slist_free (caps);

	brasero_plugin_set_process_flags (plugin, BRASERO_PLUGIN_RUN_PREPROCESSING);

	brasero_plugin_set_compulsory (plugin, FALSE);
}
