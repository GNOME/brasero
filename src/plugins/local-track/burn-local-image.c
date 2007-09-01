/***************************************************************************
 *            burn-local-image.c
 *
 *  dim jui  9 10:54:14 2006
 *  Copyright  2006  Rouquier Philippe
 *  brasero-app@wanadoo.fr
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <gmodule.h>

#include <libgnomevfs/gnome-vfs.h>

#include "burn-basics.h"
#include "burn-job.h"
#include "burn-plugin.h"
#include "burn-local-image.h"

BRASERO_PLUGIN_BOILERPLATE (BraseroLocalTrack, brasero_local_track, BRASERO_TYPE_JOB, BraseroJob);

struct _BraseroLocalTrackPrivate {
	gchar *checksum_src;
	gchar *checksum_dest;

	GHashTable *nonlocals;
	GnomeVFSAsyncHandle *xfer_handle;
};
typedef struct _BraseroLocalTrackPrivate BraseroLocalTrackPrivate;

#define BRASERO_LOCAL_TRACK_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_LOCAL_TRACK, BraseroLocalTrackPrivate))

static GObjectClass *parent_class = NULL;

static gint
brasero_local_track_xfer_async_cb (GnomeVFSAsyncHandle *handle,
				   GnomeVFSXferProgressInfo *info,
				   BraseroLocalTrack *self);


static gchar *
brasero_local_track_translate_uri (BraseroLocalTrack *self,
				   gchar *uri)
{
	gchar *newuri;
	gchar *parent;
	BraseroLocalTrackPrivate *priv;

	priv = BRASERO_LOCAL_TRACK_PRIVATE (self);

	if (uri == NULL)
		return NULL;

	/* see if it is a local file */
	if (g_str_has_prefix (uri, "file://"))
		return uri;

	/* see if it was downloaded itself */
	newuri = g_hash_table_lookup (priv->nonlocals, uri);
	if (newuri) {
		g_free (uri);

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
			newuri = g_strconcat (tmp,
					      uri + strlen (parent),
					      NULL);
			g_free (parent);
			g_free (uri);
			return newuri;
		}

		tmp = parent;
		parent = g_path_get_dirname (tmp);
		g_free (tmp);
	}

	/* that should not happen */
	g_warning ("Can't find a downloaded parent for this non local uri.\n");

	g_free (parent);
	g_free (uri);
	return NULL;
}

static BraseroBurnResult
brasero_local_track_read_checksum (BraseroLocalTrack *self)
{
	gint bytes;
	FILE *file;
	gchar *path;
	gchar checksum [33];
	BraseroTrack *track;
	BraseroLocalTrackPrivate *priv;

	priv = BRASERO_LOCAL_TRACK_PRIVATE (self);

	/* get the file_checksum from the md5 file */
	path = gnome_vfs_get_local_path_from_uri (priv->checksum_dest);
	file = fopen (path, "r");
	g_free (path);

	if (!file)
		return BRASERO_BURN_ERR;

	bytes = fread (checksum, 1, sizeof (checksum) - 1, file);
	fclose (file);

	if (bytes != sizeof (checksum) - 1)
		return BRASERO_BURN_ERR;

	checksum [sizeof (checksum)] = '\0';

	brasero_job_get_current_track (BRASERO_JOB (self), &track);
	brasero_track_set_checksum (track,
				    BRASERO_CHECKSUM_MD5,
				    checksum);
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_local_track_download_checksum (BraseroLocalTrack *self)
{
	BraseroLocalTrackPrivate *priv;
	BraseroBurnResult result;
	GnomeVFSResult res;
	GnomeVFSURI *dest;
	GList *dest_list;
	GnomeVFSURI *src;
	GList *src_list;

	priv = BRASERO_LOCAL_TRACK_PRIVATE (self);

	src = gnome_vfs_uri_new (priv->checksum_src);
	src_list = g_list_prepend (NULL, src);

	/* generate a unique name for dest */
	result = brasero_job_get_tmp_file (BRASERO_JOB (self),
					   NULL,
					   &priv->checksum_dest,
					   NULL);
	if (result != BRASERO_BURN_OK)
		goto error;

	if (!g_str_has_prefix (priv->checksum_dest, "file://")) {
		gchar *tmp;

		tmp = priv->checksum_dest;
		priv->checksum_dest = g_strconcat ("file://", tmp, NULL);
		g_free (tmp);
	}

	dest = gnome_vfs_uri_new (priv->checksum_dest);
	dest_list = g_list_prepend (NULL, dest);

	res = gnome_vfs_async_xfer (&priv->xfer_handle,
				    src_list,
				    dest_list,
				    GNOME_VFS_XFER_DEFAULT|
				    GNOME_VFS_XFER_USE_UNIQUE_NAMES|
				    GNOME_VFS_XFER_RECURSIVE,
				    GNOME_VFS_XFER_ERROR_MODE_ABORT,
				    GNOME_VFS_XFER_OVERWRITE_MODE_ABORT,
				    GNOME_VFS_PRIORITY_DEFAULT,
				    (GnomeVFSAsyncXferProgressCallback) brasero_local_track_xfer_async_cb,
				    self,
				    NULL, NULL);
	
	g_list_foreach (src_list, (GFunc) gnome_vfs_uri_unref, NULL);
	g_list_foreach (dest_list, (GFunc) gnome_vfs_uri_unref, NULL);
	g_list_free (src_list);
	g_list_free (dest_list);

	if (res != GNOME_VFS_OK) {
		result = BRASERO_BURN_ERR;
		goto error;
	}

	brasero_job_set_current_action (BRASERO_JOB (self),
					BRASERO_BURN_ACTION_FILE_COPY,
					_("Copying files md5sum file"),
					TRUE);
	return BRASERO_BURN_OK;

error:
	/* we give up */
	g_free (priv->checksum_src);
	priv->checksum_src = NULL;
	g_free (priv->checksum_dest);
	priv->checksum_dest = NULL;
	return result;
}

static BraseroBurnResult
brasero_local_track_finished (BraseroLocalTrack *self)
{
	BraseroTrack *track;
	BraseroTrackType input;
	BraseroLocalTrackPrivate *priv;

	priv = BRASERO_LOCAL_TRACK_PRIVATE (self);

	/* successfully downloaded files */

	if (priv->checksum_src
	&& !priv->checksum_dest
	&&  brasero_local_track_download_checksum (self) == BRASERO_BURN_OK)
		return BRASERO_BURN_OK;

	if (priv->checksum_dest)
		brasero_local_track_read_checksum (self);

	/* now we update all the track with the local uris in retval */
	brasero_job_get_current_track (BRASERO_JOB (self), &track);
	track = brasero_track_copy (track);
	brasero_track_get_type (track, &input);

	/* FIXME: we'd better make a copy of the tracks instead of modifying
	 * them */
	switch (input.type) {
	case BRASERO_TRACK_TYPE_DATA: {
		GSList *grafts;
		GSList *unreadable;

		grafts = brasero_track_get_data_grafts_source (track);
		for (; grafts; grafts = grafts->next) {
			BraseroGraftPt *graft;
			GSList *excluded;

			graft = grafts->data;
			graft->uri = brasero_local_track_translate_uri (self, graft->uri);

			for (excluded = graft->excluded; excluded; excluded = excluded->next)
				excluded->data = brasero_local_track_translate_uri (self, excluded->data);
		}

		/* translate the globally excluded */
		unreadable = brasero_track_get_data_excluded_source (track);
		for (; unreadable; unreadable = unreadable->next)
			unreadable->data = brasero_local_track_translate_uri (self, unreadable->data);
	}
	break;

	case BRASERO_TRACK_TYPE_AUDIO: {
		gchar *uri;

		uri = brasero_track_get_audio_source (track, TRUE);
		uri = brasero_local_track_translate_uri (self, uri);
		brasero_track_set_audio_source (track, uri, input.subtype.audio_format); 
	}
	break;

	case BRASERO_TRACK_TYPE_IMAGE: {
		gchar *uri;

		uri = brasero_track_get_image_source (track, TRUE);
		uri = brasero_local_track_translate_uri (self, uri);
		brasero_track_set_image_source (track, uri, NULL, input.subtype.img_format);

		uri = brasero_track_get_toc_source (track, TRUE);
		uri = brasero_local_track_translate_uri (self, uri);
		brasero_track_set_image_source (track, NULL, uri, input.subtype.img_format);
	}
	break;

	default:
		BRASERO_JOB_NOT_SUPPORTED (self);
	}

	brasero_job_add_track (BRASERO_JOB (self), track);
	brasero_job_finished_track (BRASERO_JOB (self));
	return BRASERO_BURN_OK;
}

/* This one is for error reporting */
static gint
brasero_local_track_xfer_async_cb (GnomeVFSAsyncHandle *handle,
				   GnomeVFSXferProgressInfo *info,
				   BraseroLocalTrack *self)
{
	BraseroLocalTrackPrivate *priv;

	priv = BRASERO_LOCAL_TRACK_PRIVATE (self);

	if (!priv->xfer_handle)
		return FALSE;

	if (info->source_name) {
		gchar *name, *string;

		BRASERO_GET_BASENAME_FOR_DISPLAY (info->source_name, name);
		string = g_strdup_printf (_("Copying `%s` locally"), name);
		g_free (name);

		/* get the source name from the info */
		brasero_job_set_current_action (BRASERO_JOB (self),
						BRASERO_BURN_ACTION_FILE_COPY,
						string,
						TRUE);
		g_free (string);
	}

	/* that where we'll treat the errors and decide to retry or not */
	brasero_job_start_progress (BRASERO_JOB (self), FALSE);
	brasero_job_set_progress (BRASERO_JOB (self),
				  (gdouble) info->total_bytes_copied /
				  (gdouble) info->bytes_total);

	if (info->phase == GNOME_VFS_XFER_PHASE_COMPLETED) {
		brasero_local_track_finished (self);
		return FALSE;
	}
	else if (info->status != GNOME_VFS_XFER_PROGRESS_STATUS_OK) {
		/* here there is an exception which is if the failure comes
		 * from the .md5 file that may or may not exist */
		if (priv->checksum_dest) {
			g_free (priv->checksum_src);
			priv->checksum_src = NULL;

			g_free (priv->checksum_dest);
			priv->checksum_dest = NULL;

			brasero_local_track_finished (self);
			return FALSE;
		}

		brasero_job_error (BRASERO_JOB (self),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_GENERAL,
						gnome_vfs_result_to_string (info->vfs_status)));
		return GNOME_VFS_XFER_ERROR_ACTION_ABORT;
	}

	return TRUE;
}

struct _BraseroDownloadableListData {
	GHashTable *nonlocals;
	GList *dest_list;
	GList *src_list;
	GError **error;
};
typedef struct _BraseroDownloadableListData BraseroDownloadableListData;

static gboolean
_foreach_non_local_cb (const gchar *uri,
		       const gchar *localuri,
		       BraseroDownloadableListData *data)
{
	GnomeVFSURI *vfsuri, *tmpuri;
	gchar *parent;
	gchar *tmp;

	/* check that is hasn't any parent in the hash */
	parent = g_path_get_dirname (uri);
	while (parent [1] != '\0') {
		localuri = g_hash_table_lookup (data->nonlocals, parent);
		if (localuri) {
			g_free (parent);
			return TRUE;
		}

		tmp = parent;
		parent = g_path_get_dirname (tmp);
		g_free (tmp);
	}
	g_free (parent);

	vfsuri = gnome_vfs_uri_new (uri);
	data->src_list = g_list_append (data->src_list, vfsuri);

	tmpuri = gnome_vfs_uri_new (localuri);
	data->dest_list = g_list_append (data->dest_list, tmpuri);

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

	if (!uri || g_str_has_prefix (uri, "file://"))
		return BRASERO_BURN_OK;

	/* add it to the list or uris to download */
	if (!priv->nonlocals)
		priv->nonlocals = g_hash_table_new_full (g_str_hash,
							 g_str_equal,
							 NULL,
							 g_free);

	if (g_str_has_prefix (uri, "burn://")) {
		GnomeVFSHandle *handle = NULL;
		GnomeVFSResult res;

		/* this is a special case for burn:// uris */
		res = gnome_vfs_open (&handle, uri, GNOME_VFS_OPEN_READ);
		if (res != GNOME_VFS_OK || !handle) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     gnome_vfs_result_to_string (res));
			return BRASERO_BURN_ERR;;
		}

		res = gnome_vfs_file_control (handle,
					      "mapping:get_mapping",
					      &localuri);
		gnome_vfs_close (handle);

		if (res != GNOME_VFS_OK
		|| !localuri
		|| !g_str_has_prefix (localuri, "file://")) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     gnome_vfs_result_to_string (res));
			return BRASERO_BURN_ERR;
		}

		g_hash_table_insert (priv->nonlocals, (gpointer) uri, localuri);
		return BRASERO_BURN_OK;
	}

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
		g_hash_table_insert (priv->nonlocals, (gpointer) uri, localuri);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_local_track_start (BraseroJob *job,
			   GError **error)
{
	BraseroDownloadableListData callback_data;
	BraseroLocalTrackPrivate *priv;
	BraseroJobAction action;
	BraseroLocalTrack *self;
	BraseroTrackType input;
	BraseroTrack *track;
	GnomeVFSResult res;
	GSList *grafts;
	gchar *uri;

	self = BRASERO_LOCAL_TRACK (job);
	priv = BRASERO_LOCAL_TRACK_PRIVATE (self);

	brasero_job_get_action (job, &action);

	/* skip that part */
	if (action == BRASERO_JOB_ACTION_SIZE)
		return BRASERO_BURN_NOT_RUNNING;

	if (action != BRASERO_JOB_ACTION_IMAGE)
		return BRASERO_BURN_NOT_SUPPORTED;

	/* can't be piped so brasero_job_get_current_track will work */
	brasero_job_get_current_track (job, &track);
	brasero_job_get_input_type (job, &input);

	/* make a list of all non local uris to be downloaded and put them in a
	 * list to avoid to download the same file twice. */
	switch (input.type) {
	case BRASERO_TRACK_TYPE_DATA:
		/* we put all the non local graft point uris in the hash */
		grafts = brasero_track_get_data_grafts_source (track);
		for (; grafts; grafts = grafts->next) {
			BraseroGraftPt *graft;

			graft = grafts->data;
			brasero_local_track_add_if_non_local (self, graft->uri, error);
		}
		break;

	case BRASERO_TRACK_TYPE_AUDIO:
		uri = brasero_track_get_audio_source (track, TRUE);
		brasero_local_track_add_if_non_local (self, uri, error);
		g_free (uri);
		break;

	case BRASERO_TRACK_TYPE_IMAGE:
		uri = brasero_track_get_image_source (track, TRUE);
		brasero_local_track_add_if_non_local (self, uri, error);

		/* This is an image. See if there is any md5 sum sitting in the
		 * same directory to check our download integrity */
		priv->checksum_src = g_strdup_printf ("%s.md5", uri);
		g_free (uri);

		uri = brasero_track_get_toc_source (track, TRUE);
		brasero_local_track_add_if_non_local (self, uri, error);
		g_free (uri);
		break;

	default:
		BRASERO_JOB_NOT_SUPPORTED (self);
	}

	/* see if there is anything to download */
	if (!priv->nonlocals) {
		BRASERO_JOB_LOG (self, "no foreign URIs");
		return BRASERO_BURN_NOT_RUNNING;
	}

	/* first we create a list of all the non local files that need to be
	 * downloaded. To be elligible a file must not have one of his parent
	 * in the hash. */
	callback_data.nonlocals = priv->nonlocals;
	callback_data.dest_list = NULL;
	callback_data.src_list = NULL;

	g_hash_table_foreach_remove (priv->nonlocals,
				     (GHRFunc) _foreach_non_local_cb,
				     &callback_data);

	/* if there are files in list then download them otherwise stop */
	if (!callback_data.src_list) {
		/* that means there were only burn:// uris in nonlocals */
		BRASERO_JOB_LOG (self, "no foreign URIs");
		return BRASERO_BURN_NOT_RUNNING;
	}

	res = gnome_vfs_async_xfer (&priv->xfer_handle,
				    callback_data.src_list,
				    callback_data.dest_list,
				    GNOME_VFS_XFER_DEFAULT|
				    GNOME_VFS_XFER_USE_UNIQUE_NAMES|
				    GNOME_VFS_XFER_RECURSIVE,
				    GNOME_VFS_XFER_ERROR_MODE_ABORT,
				    GNOME_VFS_XFER_OVERWRITE_MODE_ABORT,
				    GNOME_VFS_PRIORITY_DEFAULT,
				    (GnomeVFSAsyncXferProgressCallback) brasero_local_track_xfer_async_cb,
				    self,
				    NULL, NULL);

	g_list_foreach (callback_data.src_list, (GFunc) gnome_vfs_uri_unref, NULL);
	g_list_foreach (callback_data.dest_list, (GFunc) gnome_vfs_uri_unref, NULL);
	g_list_free (callback_data.src_list);
	g_list_free (callback_data.dest_list);

	if (res != GNOME_VFS_OK) {
		if (error)
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     gnome_vfs_result_to_string (res));
		return BRASERO_BURN_ERR;
	}

	brasero_job_set_current_action (BRASERO_JOB (self),
					BRASERO_BURN_ACTION_FILE_COPY,
					_("Copying files locally"),
					TRUE);
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_local_track_stop (BraseroJob *job,
			  GError **error)
{
	BraseroLocalTrackPrivate *priv = BRASERO_LOCAL_TRACK_PRIVATE (job);

	if (priv->xfer_handle) {
		gnome_vfs_async_cancel (priv->xfer_handle);
		priv->xfer_handle = NULL;
	}

	if (priv->nonlocals) {
		g_hash_table_destroy (priv->nonlocals);
		priv->nonlocals = NULL;
	}

	if (priv->checksum_src) {
		g_free (priv->checksum_src);
		priv->checksum_src = NULL;
	}

	if (priv->checksum_dest) {
		g_free (priv->checksum_dest);
		priv->checksum_dest = NULL;
	}

	return BRASERO_BURN_OK;
}

static void
brasero_local_track_finalize (GObject *object)
{
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
}

static void
brasero_local_track_init (BraseroLocalTrack *obj)
{ }

static BraseroBurnResult
brasero_local_track_export_caps (BraseroPlugin *plugin, gchar **error)
{
	GSList *caps;

	brasero_plugin_define (plugin,
			       "local-track",
			       _("local-track allows to burn files not stored locally"),
			       "Philippe Rouquier",
			       10);

	caps = brasero_caps_image_new (BRASERO_PLUGIN_IO_ACCEPT_FILE,
				       BRASERO_IMAGE_FORMAT_ANY);
	brasero_plugin_process_caps (plugin, caps);
	g_slist_free (caps);

	caps = brasero_caps_audio_new (BRASERO_PLUGIN_IO_ACCEPT_FILE,
				       BRASERO_AUDIO_FORMAT_UNDEFINED|
				       BRASERO_AUDIO_FORMAT_4_CHANNEL|
				       BRASERO_AUDIO_FORMAT_RAW);
	brasero_plugin_process_caps (plugin, caps);
	g_slist_free (caps);

	caps = brasero_caps_data_new (BRASERO_IMAGE_FS_ANY);
	brasero_plugin_process_caps (plugin, caps);
	g_slist_free (caps);

	brasero_plugin_set_process_flags (plugin, BRASERO_PLUGIN_RUN_FIRST);

	return BRASERO_BURN_OK;
}
