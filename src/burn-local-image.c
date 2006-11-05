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

#include <libgnomevfs/gnome-vfs.h>

#include "burn-job.h"
#include "burn-basics.h"
#include "burn-imager.h"
#include "burn-local-image.h"
#include "burn-common.h"
#include "burn-sum.h"
#include "burn-md5.h"

static void brasero_local_image_class_init (BraseroLocalImageClass *klass);
static void brasero_local_image_init (BraseroLocalImage *sp);
static void brasero_local_image_finalize (GObject *object);

static void brasero_local_image_iface_init_image (BraseroImagerIFace *iface);

static BraseroBurnResult
brasero_local_image_set_source (BraseroJob *job,
				const BraseroTrackSource *source,
				GError **error);

static BraseroBurnResult
brasero_local_image_start (BraseroJob *job,
			   int in_fd,
			   int *out_fd,
			   GError **error);

static BraseroBurnResult
brasero_local_image_stop (BraseroJob *job,
			  BraseroBurnResult retval,
			  GError **error);

static BraseroBurnResult
brasero_local_image_get_track (BraseroImager *imager,
			       BraseroTrackSource **source,
			       GError **error);
static BraseroBurnResult
brasero_local_image_set_output (BraseroImager *imager,
				const gchar *output,
				gboolean overwrite,
				gboolean clean,
				GError **error);

struct _BraseroLocalImagePrivate {
	BraseroTrackSource *source;
	gchar *tmpdir;

	const gchar *current_download;
	GHashTable *nonlocals;

	GnomeVFSAsyncHandle *xfer_handle;
	GSList *downloaded;

	gint check_integrity:1;
	gint overwrite:1;
	gint clean:1;
};

static GObjectClass *parent_class = NULL;

static const gchar *NOT_DOWNLOADED = "NOT_DOWNLOADED";

#define ADD_IF_NON_LOCAL(image, uri) \
	if (uri && !g_str_has_prefix (uri, "file://")) { \
		if (!image->priv->nonlocals) \
			image->priv->nonlocals = g_hash_table_new (g_str_hash, g_str_equal); \
		/* we don't want to replace it if it has already been downloaded */ \
		if (!g_hash_table_lookup (image->priv->nonlocals, uri)) \
			g_hash_table_insert (image->priv->nonlocals, uri, (gpointer) NOT_DOWNLOADED); \
	}

GType
brasero_local_image_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroLocalImageClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_local_image_class_init,
			NULL,
			NULL,
			sizeof (BraseroLocalImage),
			0,
			(GInstanceInitFunc) brasero_local_image_init,
		};

		static const GInterfaceInfo imager_info = {
			(GInterfaceInitFunc) brasero_local_image_iface_init_image,
			NULL,
			NULL
		};

		type = g_type_register_static (BRASERO_TYPE_JOB,
					       "BraseroLocalImage",
					       &our_info,
					       0);

		g_type_add_interface_static (type,
					     BRASERO_TYPE_IMAGER,
					     &imager_info);
	}

	return type;
}

static void
brasero_local_image_iface_init_image (BraseroImagerIFace *iface)
{
	iface->get_track = brasero_local_image_get_track;
	iface->set_output = brasero_local_image_set_output;
}

static void
brasero_local_image_class_init (BraseroLocalImageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroJobClass *job_class = BRASERO_JOB_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_local_image_finalize;

	job_class->set_source = brasero_local_image_set_source;
	job_class->start = brasero_local_image_start;
	job_class->stop = brasero_local_image_stop;
}

static void
brasero_local_image_init (BraseroLocalImage *obj)
{
	obj->priv = g_new0 (BraseroLocalImagePrivate, 1);
}

static void
brasero_local_image_clean_tmpdir (BraseroLocalImage *image)
{
	if (image->priv->clean) {
		if (image->priv->downloaded)
			g_slist_foreach (image->priv->downloaded,
					 (GFunc) brasero_burn_common_rm,
					 NULL);
	}

	if (image->priv->downloaded) {
		g_slist_foreach (image->priv->downloaded, (GFunc) g_free, NULL);
		g_slist_free (image->priv->downloaded);
		image->priv->downloaded = NULL;
	}

	if (image->priv->nonlocals) {
		g_hash_table_destroy (image->priv->nonlocals);
		image->priv->nonlocals = NULL;
	}
}

static void
brasero_local_image_finalize (GObject *object)
{
	BraseroLocalImage *cobj;

	cobj = BRASERO_LOCAL_IMAGE (object);

	brasero_local_image_clean_tmpdir (cobj);

	if (cobj->priv->clean) {
		/* since we remove only our files if a file created
		 * by user remains directory won't be removed */
		if (cobj->priv->tmpdir)
			g_remove (cobj->priv->tmpdir);
	}

	if (cobj->priv->tmpdir) {
		g_free (cobj->priv->tmpdir);
		cobj->priv->tmpdir = NULL;
	}

	if (cobj->priv->source) {
		brasero_track_source_free (cobj->priv->source);
		cobj->priv->source = NULL;
	}

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static BraseroBurnResult
brasero_local_image_set_source (BraseroJob *job,
				const BraseroTrackSource *source,
				GError **error)
{
	BraseroLocalImage *image = BRASERO_LOCAL_IMAGE (job);

	if (image->priv->source) {
		brasero_track_source_free (image->priv->source);
		image->priv->source = NULL;
	}

	if (source->type == BRASERO_TRACK_SOURCE_DEFAULT
	||  source->type == BRASERO_TRACK_SOURCE_UNKNOWN
	||  source->type == BRASERO_TRACK_SOURCE_IMAGER
	||  source->type == BRASERO_TRACK_SOURCE_DISC
	||  source->type == BRASERO_TRACK_SOURCE_INF)
		BRASERO_JOB_NOT_SUPPORTED (image);

	/* we accept whatever source */
	image->priv->source = brasero_track_source_copy (source);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_local_image_start (BraseroJob *job,
			   int fd_in,
			   int *fd_out,
			   GError **error)
{
	BraseroLocalImage *image;

	/* we can't pipe or be piped */
	image = BRASERO_LOCAL_IMAGE (job);

	g_return_val_if_fail (BRASERO_IS_LOCAL_IMAGE (job), BRASERO_BURN_ERR);
	
	if (fd_in != -1 || fd_out)
		BRASERO_JOB_NOT_SUPPORTED (image);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_local_image_stop (BraseroJob *job,
			  BraseroBurnResult retval,
			  GError **error)
{
	BraseroLocalImage *image = BRASERO_LOCAL_IMAGE (job);

	if (image->priv->xfer_handle) {
		gnome_vfs_async_cancel (image->priv->xfer_handle);
		image->priv->xfer_handle = NULL;
	}

	return BRASERO_BURN_OK;
}

/* This one is for error reporting */
static int
brasero_local_image_xfer_async_cb (GnomeVFSAsyncHandle *handle,
				   GnomeVFSXferProgressInfo *info,
				   BraseroLocalImage *image)
{
	if (!image->priv->xfer_handle)
		return FALSE;

	if (info->phase == GNOME_VFS_XFER_PHASE_COMPLETED) {
		brasero_job_finished (BRASERO_JOB (image));
		return FALSE;
	}
	else if (info->status != GNOME_VFS_XFER_PROGRESS_STATUS_OK) {
		brasero_job_error (BRASERO_JOB (image),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_GENERAL,
						gnome_vfs_result_to_string (info->vfs_status)));
		return GNOME_VFS_XFER_ERROR_ACTION_ABORT;
	}

	return TRUE;
}

/* This one is for progress reporting */
static gint
brasero_local_image_xfer_cb (GnomeVFSXferProgressInfo *info,
			     BraseroLocalImage *image)
{
	if (!image->priv->xfer_handle)
		return FALSE;

	BRASERO_JOB_TASK_START_PROGRESS (image, FALSE);

	if (info->file_size)
		BRASERO_JOB_TASK_SET_TOTAL (image, info->bytes_total);

	if (info->bytes_copied)
		BRASERO_JOB_TASK_SET_WRITTEN (image, info->total_bytes_copied);

	return TRUE;
}

static BraseroBurnResult
brasero_local_image_download_uri (BraseroLocalImage *image,
				  const gchar *uri_src,
				  gchar **localuri,
				  GError **error)
{
	GList *src_list, *dest_list;
	BraseroBurnResult result;
	GnomeVFSURI *tmpuri;
	GnomeVFSURI *vfsuri;
	GnomeVFSResult res;
	gchar *localpath_tmp;
	gchar *uri_dest;
	gchar *string;
	gchar *name;

	/* generate a unique name */
	localpath_tmp = g_strconcat (image->priv->tmpdir, "/"BRASERO_BURN_TMP_FILE_NAME, NULL);
	if (mkstemp (localpath_tmp) == -1) {
		g_free (localpath_tmp);
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_TMP_DIR,
			     _("a temporary file can't be created: %s"),
			     strerror(errno));
		return BRASERO_BURN_ERR;
	}
	g_remove (localpath_tmp);
	uri_dest = g_strconcat ("file://", localpath_tmp, NULL);
	g_free (localpath_tmp);

	/* start the thing */
	vfsuri = gnome_vfs_uri_new (uri_src);
	src_list = g_list_append (NULL, vfsuri);

	tmpuri = gnome_vfs_uri_new (uri_dest);
	dest_list = g_list_append (NULL, tmpuri);
	
	res = gnome_vfs_async_xfer (&image->priv->xfer_handle,
				    src_list,
				    dest_list,
				    GNOME_VFS_XFER_DEFAULT |
				    GNOME_VFS_XFER_USE_UNIQUE_NAMES |
				    GNOME_VFS_XFER_RECURSIVE,
				    GNOME_VFS_XFER_ERROR_MODE_ABORT,
				    GNOME_VFS_XFER_OVERWRITE_MODE_ABORT,
				    GNOME_VFS_PRIORITY_DEFAULT,
				    (GnomeVFSAsyncXferProgressCallback) brasero_local_image_xfer_async_cb,
				    image,
				    (GnomeVFSXferProgressCallback) brasero_local_image_xfer_cb,
				    image);

	g_list_free (src_list);
	g_list_free (dest_list);
	gnome_vfs_uri_unref (vfsuri);
	gnome_vfs_uri_unref (tmpuri);

	if (res != GNOME_VFS_OK) {
		g_free (uri_dest);
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     gnome_vfs_result_to_string (res));
		return BRASERO_BURN_ERR;
	}

	/* start the job */
	image->priv->current_download = uri_src;

	BRASERO_GET_BASENAME_FOR_DISPLAY (image->priv->current_download, name);
	string = g_strdup_printf (_("Copying \"%s\""), name);
	g_free (name);

	BRASERO_JOB_TASK_SET_ACTION (BRASERO_JOB (image),
				     BRASERO_BURN_ACTION_FILE_COPY,
				     string,
				     TRUE);
	g_free (string);

	result = brasero_job_run (BRASERO_JOB (image), error);

	/* finished: even if it fails we return the path so that we can clean it */
	*localuri = uri_dest;
	if (result != BRASERO_BURN_OK)
		return result;

	return BRASERO_BURN_OK;
}

struct _BraseroDownloadableListData {
	GHashTable *nonlocals;
	GSList *list;
};
typedef struct _BraseroDownloadableListData BraseroDownloadableListData;

static void
_foreach_non_local_cb (const gchar *uri,
		       const gchar *localpath,
		       BraseroDownloadableListData *data)
{
	gchar *localuri;
	gchar *parent;
	gchar *tmp;

	/* FIXME: does it work with all uris (like burn ?) */
	parent = g_path_get_dirname (uri);
	while (parent [1] != '\0') {
		localuri = g_hash_table_lookup (data->nonlocals, parent);
		if (localuri && localuri != NOT_DOWNLOADED) {
			g_free (parent);
			return;
		}

		tmp = parent;
		parent = g_path_get_dirname (tmp);
		g_free (tmp);
	}
	g_free (parent);
	data->list = g_slist_prepend (data->list, (gchar *) uri);
}

static BraseroBurnResult
brasero_local_image_check_image_integrity (BraseroLocalImage *image,
					   const gchar *image_local_uri,
					   const gchar *image_uri)
{
	gint read;
	FILE *file;
	gchar *sumuri;
	gchar *localuri;
	gchar *localpath;
	BraseroJob *slave;
	BraseroBurnResult result;
	BraseroTrackSource source;
	BraseroTrackSource *md5_track;
	gchar file_checksum [MD5_STRING_LEN + 1];
	gchar image_checksum [MD5_STRING_LEN + 1];
	
	/* we try *.md5 *.md5.asc */
	sumuri = g_strdup_printf ("%s.md5", image_uri);
	result = brasero_local_image_download_uri (image,
						   sumuri,
						   &localuri,
						   NULL);
	g_free (sumuri);

	if (result != BRASERO_BURN_OK)
		return BRASERO_BURN_OK;

	localpath = gnome_vfs_get_local_path_from_uri (localuri);
	g_free (localuri);

	/* get the file_checksum from the md5 file */
	file = fopen (localpath, "r");

	/* that way the file will be removed when fclose is called if it 
	 * succeeded and remove it anyway if fopen failed */
	g_remove (localpath);

	if (!file)
		return BRASERO_BURN_OK;

	read = fread (file_checksum, 1, 32, file);
	fclose (file);

	if (read != 32)
		return BRASERO_BURN_OK;

	file_checksum [32] = '\0';

	/* generate a checksum for the image we just downloaded */
	source.type = BRASERO_TRACK_SOURCE_IMAGE;
	source.format = BRASERO_IMAGE_FORMAT_ANY;
	source.contents.image.image = (gchar*) image_local_uri;
	source.contents.image.toc = NULL;

	slave = BRASERO_JOB (g_object_new (BRASERO_TYPE_BURN_SUM, NULL));

	brasero_job_set_slave (BRASERO_JOB (image), slave);
	g_object_unref (slave);

	result = brasero_job_set_source (slave, &source, NULL);
	if (result != BRASERO_BURN_OK) {
		brasero_job_set_slave (BRASERO_JOB (image), NULL);
		return BRASERO_BURN_OK;
	}

	result = brasero_imager_set_output_type (BRASERO_IMAGER (slave),
						 BRASERO_TRACK_SOURCE_SUM,
						 BRASERO_IMAGE_FORMAT_NONE,
						 NULL);
	if (result != BRASERO_BURN_OK) {
		brasero_job_set_slave (BRASERO_JOB (image), NULL);
		return BRASERO_BURN_OK;
	}

	brasero_job_set_relay_slave_signals (BRASERO_JOB (image), TRUE);
	result = brasero_imager_get_track (BRASERO_IMAGER (slave),
					   &md5_track,
					   NULL);

	brasero_job_set_slave (BRASERO_JOB (image), NULL);

	if (result != BRASERO_BURN_OK)
		return BRASERO_BURN_OK;

	brasero_md5_string (&md5_track->contents.sum.md5, image_checksum);
	brasero_track_source_free (md5_track);

	if (strcmp (image_checksum, file_checksum))
		return BRASERO_BURN_RETRY;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_local_image_download_non_local (BraseroLocalImage *image,
					GError **error)
{
	GSList *iter;
	BraseroBurnResult result;
	BraseroDownloadableListData callback_data;

	if (!image->priv->nonlocals)
		return BRASERO_BURN_OK;

	/* first we establish a list of the non local files that need to be
	 * downloaded. To be elligible a file must not have one of his parent
	 * in the hash. */
	callback_data.nonlocals = image->priv->nonlocals;
	callback_data.list = NULL;
	g_hash_table_foreach (image->priv->nonlocals,
			      (GHFunc) _foreach_non_local_cb,
			      &callback_data);

	if (callback_data.list == NULL)
		return BRASERO_BURN_OK;

	/* create a temporary directory */
	result = brasero_burn_common_create_tmp_directory (&image->priv->tmpdir,
							   image->priv->overwrite,
							   error);
	if (result != BRASERO_BURN_OK)
		return result;

	for (iter = callback_data.list; iter; iter = iter->next) {
		gchar *uri;
		gchar *localuri;
		BraseroBurnResult result;

		uri = iter->data;
		localuri = NULL;
		result = BRASERO_BURN_OK;

		/* this is a special case for burn:// uris */
		if (g_str_has_prefix (uri, "burn://")) {
			GnomeVFSHandle *handle = NULL;
			GnomeVFSResult res;

			/* these files are local files so we can check their
			 * readability and that they are not symlinks nor
			 * sockets (since the gnome_vfs_file_open might hang
			 * with the latters) */
			/* NOTE: the following code is taken from ncb */
			result = brasero_burn_common_check_local_file (uri, error);
			if (result != BRASERO_BURN_OK)
				return result;

			res = gnome_vfs_open (&handle, uri, GNOME_VFS_OPEN_READ);
			if (res != GNOME_VFS_OK || !handle)
				return BRASERO_BURN_ERR;

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
					     gnome_vfs_result_to_string (result));
				return BRASERO_BURN_ERR;
			}
		}
		else {
			gint retries = 0;

			localuri = NULL;
			result = BRASERO_BURN_RETRY;

			while (result == BRASERO_BURN_RETRY && retries < 2) {
				if (localuri) {
					g_remove (localuri);
					g_free (localuri);
					retries ++;	
				}

				result = brasero_local_image_download_uri (image,
									   uri,
									   &localuri,
									   error);

				if (result == BRASERO_BURN_OK && image->priv->check_integrity)
					result = brasero_local_image_check_image_integrity (image,
											    localuri,
											    uri);
			}

			/* add newuri to a list to clean it at finalize time 
			 * env if it's false, just to make sure. */
			image->priv->downloaded = g_slist_prepend (image->priv->downloaded,
								   localuri);
		}

		if (result != BRASERO_BURN_OK)
			return result;

		/* now we insert it again in the hash with its local path */
		g_hash_table_insert (image->priv->nonlocals, uri, localuri);
	}

	return BRASERO_BURN_OK;
}

static gchar *
brasero_local_image_translate_uri (BraseroLocalImage *image,
				   gchar *uri)
{
	gchar *newuri;
	gchar *parent;

	if (uri == NULL)
		return NULL;

	/* see if it is a local file */
	if (g_str_has_prefix (uri, "file://"))
		return uri;

	/* see if it was downloaded itself */
	newuri = g_hash_table_lookup (image->priv->nonlocals, uri);
	if (newuri) {
		g_free (uri);

		/* we copy this string as it will be freed when freeing 
		 * downloaded GSList */
		return g_strdup (newuri);
	}

	/* see if one of its parent was downloaded */
	parent = g_path_get_dirname (uri);
	while (parent [1] != '\0') {
		gchar *tmp;

		tmp = g_hash_table_lookup (image->priv->nonlocals, parent);
		if (tmp && tmp != NOT_DOWNLOADED) {
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
brasero_local_image_get_track (BraseroImager *imager,
			       BraseroTrackSource **source,
			       GError **error)
{
	BraseroLocalImage *image = BRASERO_LOCAL_IMAGE (imager);
	BraseroTrackSource *retval;
	BraseroBurnResult result;
	GSList *iter;

	g_return_val_if_fail (image->priv->source != NULL, BRASERO_BURN_NOT_READY);

	retval = image->priv->source;

	/* make a list of all non local uris to be downloaded and put them in a
	 * to avoid to download the same file twice. */
	switch (retval->type) {
	case BRASERO_TRACK_SOURCE_DATA:
		/* we put all the non local graft point uris in the hash */
		for (iter = retval->contents.data.grafts; iter; iter = iter->next) {
			BraseroGraftPt *graft;

			graft = iter->data;
			ADD_IF_NON_LOCAL (image, graft->uri);
		}
		break;

	case BRASERO_TRACK_SOURCE_SONG:
		for (iter = retval->contents.songs.files; iter; iter = iter->next) {
			BraseroSongFile * song;

			song = iter->data;
			ADD_IF_NON_LOCAL (image, song->uri);
		}
		break;

	case BRASERO_TRACK_SOURCE_GRAFTS:
		ADD_IF_NON_LOCAL (image, retval->contents.grafts.excluded_path);
		ADD_IF_NON_LOCAL (image, retval->contents.grafts.grafts_path);
		break;

	case BRASERO_TRACK_SOURCE_IMAGE:
		/* This is an image. See if there is any md5 sum sitting in the
		 * same directory to check our download integrity */
		ADD_IF_NON_LOCAL (image, retval->contents.image.image);
		ADD_IF_NON_LOCAL (image, retval->contents.image.toc);
		image->priv->check_integrity = 1;
		break;

	default:
		BRASERO_JOB_NOT_SUPPORTED (image);
	}

	/* see if there is anything to download */
	if (!image->priv->nonlocals) {
		*source = brasero_track_source_copy (retval);
		return BRASERO_BURN_OK;
	}

	/* start to download the files which needs that */
	result = brasero_local_image_download_non_local (image, error);
	if (result != BRASERO_BURN_OK)
		return result;

	/* now we update all the track with the local uris in retval */
	retval = brasero_track_source_copy (retval);

	switch (retval->type) {
	case BRASERO_TRACK_SOURCE_DATA:
		for (iter = retval->contents.data.grafts; iter; iter = iter->next) {
			BraseroGraftPt *graft;
			GSList *excluded;

			graft = iter->data;
			graft->uri = brasero_local_image_translate_uri (image,
									graft->uri);

			for (excluded = graft->excluded; excluded; excluded = excluded->next)
				excluded->data = brasero_local_image_translate_uri (image,
										    excluded->data);
		}

		/* translate the globally excluded */
		for (iter = retval->contents.data.excluded; iter; iter = iter->next)
			iter->data = brasero_local_image_translate_uri (image,
									iter->data);
		break;

	case BRASERO_TRACK_SOURCE_SONG:
		for (iter = retval->contents.songs.files; iter; iter = iter->next)
			iter->data = brasero_local_image_translate_uri (image,
									iter->data);
		break;

	case BRASERO_TRACK_SOURCE_GRAFTS:
		retval->contents.grafts.excluded_path = brasero_local_image_translate_uri (image,
											   retval->contents.grafts.excluded_path);
		retval->contents.grafts.grafts_path = brasero_local_image_translate_uri (image,
											 retval->contents.grafts.grafts_path);
		break;

	case BRASERO_TRACK_SOURCE_IMAGE:
		retval->contents.image.image = brasero_local_image_translate_uri (image,
										  retval->contents.image.image);
		retval->contents.image.toc = brasero_local_image_translate_uri (image,
										retval->contents.image.toc);
		break;

	default:
		BRASERO_JOB_NOT_SUPPORTED (image);
	}

	*source = retval;
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_local_image_set_output (BraseroImager *imager,
				const gchar *output,
				gboolean overwrite,
				gboolean clean,
				GError **error)
{
	BraseroLocalImage *image = BRASERO_LOCAL_IMAGE (imager);

	brasero_local_image_clean_tmpdir (image);

	/* NOTE: we are supposed to create it so we own this directory */
	if (output)
		image->priv->tmpdir = g_strdup (output);

	image->priv->clean = clean;
	image->priv->overwrite = overwrite;
	return BRASERO_BURN_OK;
}
