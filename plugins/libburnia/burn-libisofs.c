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

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <gmodule.h>

#include <libisofs/libisofs.h>
#include <libburn/libburn.h>

#include "burn-libburnia.h"
#include "burn-job.h"
#include "brasero-units.h"
#include "brasero-plugin-registration.h"
#include "burn-libburn-common.h"
#include "brasero-track-data.h"
#include "brasero-track-image.h"


#define BRASERO_TYPE_LIBISOFS         (brasero_libisofs_get_type ())
#define BRASERO_LIBISOFS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_LIBISOFS, BraseroLibisofs))
#define BRASERO_LIBISOFS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_LIBISOFS, BraseroLibisofsClass))
#define BRASERO_IS_LIBISOFS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_LIBISOFS))
#define BRASERO_IS_LIBISOFS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_LIBISOFS))
#define BRASERO_LIBISOFS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_LIBISOFS, BraseroLibisofsClass))

BRASERO_PLUGIN_BOILERPLATE (BraseroLibisofs, brasero_libisofs, BRASERO_TYPE_JOB, BraseroJob);

struct _BraseroLibisofsPrivate {
	struct burn_source *libburn_src;

	/* that's for multisession */
	BraseroLibburnCtx *ctx;

	GError *error;
	GThread *thread;
	GMutex *mutex;
	GCond *cond;
	guint thread_id;

	guint cancel:1;
};
typedef struct _BraseroLibisofsPrivate BraseroLibisofsPrivate;

#define BRASERO_LIBISOFS_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_LIBISOFS, BraseroLibisofsPrivate))

static GObjectClass *parent_class = NULL;

static gboolean
brasero_libisofs_thread_finished (gpointer data)
{
	BraseroLibisofs *self = data;
	BraseroLibisofsPrivate *priv;

	priv = BRASERO_LIBISOFS_PRIVATE (self);

	priv->thread_id = 0;
	if (priv->error) {
		GError *error;

		error = priv->error;
		priv->error = NULL;
		brasero_job_error (BRASERO_JOB (self), error);
		return FALSE;
	}

	if (brasero_job_get_fd_out (BRASERO_JOB (self), NULL) != BRASERO_BURN_OK) {
		BraseroTrackImage *track = NULL;
		gchar *output = NULL;
		goffset blocks = 0;

		/* Let's make a track */
		track = brasero_track_image_new ();
		brasero_job_get_image_output (BRASERO_JOB (self),
					      &output,
					      NULL);
		brasero_track_image_set_source (track,
						output,
						NULL,
						BRASERO_IMAGE_FORMAT_BIN);

		brasero_job_get_session_output_size (BRASERO_JOB (self), &blocks, NULL);
		brasero_track_image_set_block_num (track, blocks);

		brasero_job_add_track (BRASERO_JOB (self), BRASERO_TRACK (track));
		g_object_unref (track);
	}

	brasero_job_finished_track (BRASERO_JOB (self));
	return FALSE;
}

static BraseroBurnResult
brasero_libisofs_write_sector_to_fd (BraseroLibisofs *self,
				     int fd,
				     gpointer buffer,
				     gint bytes_remaining)
{
	gint bytes_written = 0;
	BraseroLibisofsPrivate *priv;

	priv = BRASERO_LIBISOFS_PRIVATE (self);

	while (bytes_remaining) {
		gint written;

		written = write (fd,
				 ((gchar *) buffer) + bytes_written,
				 bytes_remaining);

		if (priv->cancel)
			break;

		if (written != bytes_remaining) {
			if (errno != EINTR && errno != EAGAIN) {
                                int errsv = errno;

				/* unrecoverable error */
				priv->error = g_error_new (BRASERO_BURN_ERROR,
							   BRASERO_BURN_ERROR_GENERAL,
							   _("Data could not be written (%s)"),
							   g_strerror (errsv));
				return BRASERO_BURN_ERR;
			}

			g_thread_yield ();
		}

		if (written > 0) {
			bytes_remaining -= written;
			bytes_written += written;
		}
	}

	return BRASERO_BURN_OK;
}

static void
brasero_libisofs_write_image_to_fd_thread (BraseroLibisofs *self)
{
	const gint sector_size = 2048;
	BraseroLibisofsPrivate *priv;
	gint64 written_sectors = 0;
	BraseroBurnResult result;
	guchar buf [sector_size];
	int read_bytes;
	int fd = -1;

	priv = BRASERO_LIBISOFS_PRIVATE (self);

	brasero_job_set_nonblocking (BRASERO_JOB (self), NULL);

	brasero_job_set_current_action (BRASERO_JOB (self),
					BRASERO_BURN_ACTION_CREATING_IMAGE,
					NULL,
					FALSE);

	brasero_job_start_progress (BRASERO_JOB (self), FALSE);
	brasero_job_get_fd_out (BRASERO_JOB (self), &fd);

	BRASERO_JOB_LOG (self, "Writing to pipe");
	read_bytes = priv->libburn_src->read_xt (priv->libburn_src, buf, sector_size);
	while (read_bytes == sector_size) {
		if (priv->cancel)
			break;

		result = brasero_libisofs_write_sector_to_fd (self,
							      fd,
							      buf,
							      sector_size);
		if (result != BRASERO_BURN_OK)
			break;

		written_sectors ++;
		brasero_job_set_written_track (BRASERO_JOB (self), written_sectors << 11);

		read_bytes = priv->libburn_src->read_xt (priv->libburn_src, buf, sector_size);
	}

	if (read_bytes == -1 && !priv->error)
		priv->error = g_error_new (BRASERO_BURN_ERROR,
					   BRASERO_BURN_ERROR_GENERAL,
					   "%s", _("Volume could not be created"));
}

static void
brasero_libisofs_write_image_to_file_thread (BraseroLibisofs *self)
{
	const gint sector_size = 2048;
	BraseroLibisofsPrivate *priv;
	gint64 written_sectors = 0;
	guchar buf [sector_size];
	int read_bytes;
	gchar *output;
	FILE *file;

	priv = BRASERO_LIBISOFS_PRIVATE (self);

	brasero_job_get_image_output (BRASERO_JOB (self), &output, NULL);
	file = fopen (output, "w");
	if (!file) {
		int errnum = errno;

		if (errno == EACCES)
			priv->error = g_error_new_literal (BRASERO_BURN_ERROR,
							   BRASERO_BURN_ERROR_PERMISSION,
							   _("You do not have the required permission to write at this location"));
		else
			priv->error = g_error_new_literal (BRASERO_BURN_ERROR,
							   BRASERO_BURN_ERROR_GENERAL,
							   g_strerror (errnum));
		return;
	}

	BRASERO_JOB_LOG (self, "writing to file %s", output);

	brasero_job_set_current_action (BRASERO_JOB (self),
					BRASERO_BURN_ACTION_CREATING_IMAGE,
					NULL,
					FALSE);

	priv = BRASERO_LIBISOFS_PRIVATE (self);
	brasero_job_start_progress (BRASERO_JOB (self), FALSE);

	read_bytes = priv->libburn_src->read_xt (priv->libburn_src, buf, sector_size);
	while (read_bytes == sector_size) {
		if (priv->cancel)
			break;

		if (fwrite (buf, 1, sector_size, file) != sector_size) {
                        int errsv = errno;

			priv->error = g_error_new (BRASERO_BURN_ERROR,
						   BRASERO_BURN_ERROR_GENERAL,
						   _("Data could not be written (%s)"),
						   g_strerror (errsv));
			break;
		}

		if (priv->cancel)
			break;

		written_sectors ++;
		brasero_job_set_written_track (BRASERO_JOB (self), written_sectors << 11);

		read_bytes = priv->libburn_src->read_xt (priv->libburn_src, buf, sector_size);
	}

	if (read_bytes == -1 && !priv->error)
		priv->error = g_error_new (BRASERO_BURN_ERROR,
					   BRASERO_BURN_ERROR_GENERAL,
					   _("Volume could not be created"));

	fclose (file);
	file = NULL;
}

static gpointer
brasero_libisofs_thread_started (gpointer data)
{
	BraseroLibisofsPrivate *priv;
	BraseroLibisofs *self;

	self = BRASERO_LIBISOFS (data);
	priv = BRASERO_LIBISOFS_PRIVATE (self);

	BRASERO_JOB_LOG (self, "Entering thread");
	if (brasero_job_get_fd_out (BRASERO_JOB (self), NULL) == BRASERO_BURN_OK)
		brasero_libisofs_write_image_to_fd_thread (self);
	else
		brasero_libisofs_write_image_to_file_thread (self);

	BRASERO_JOB_LOG (self, "Getting out thread");

	/* End thread */
	g_mutex_lock (priv->mutex);

	if (!priv->cancel)
		priv->thread_id = g_idle_add (brasero_libisofs_thread_finished, self);

	priv->thread = NULL;
	g_cond_signal (priv->cond);
	g_mutex_unlock (priv->mutex);

	g_thread_exit (NULL);

	return NULL;
}

static BraseroBurnResult
brasero_libisofs_create_image (BraseroLibisofs *self,
			       GError **error)
{
	BraseroLibisofsPrivate *priv;
	GError *thread_error = NULL;

	priv = BRASERO_LIBISOFS_PRIVATE (self);

	if (priv->thread)
		return BRASERO_BURN_RUNNING;

	if (iso_init () < 0) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("libisofs could not be initialized."));
		return BRASERO_BURN_ERR;
	}

	iso_set_msgs_severities ("NEVER", "ALL", "brasero (libisofs)");

	g_mutex_lock (priv->mutex);
	priv->thread = g_thread_create (brasero_libisofs_thread_started,
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
brasero_libisofs_create_volume_thread_finished (gpointer data)
{
	BraseroLibisofs *self = data;
	BraseroLibisofsPrivate *priv;
	BraseroJobAction action;

	priv = BRASERO_LIBISOFS_PRIVATE (self);

	priv->thread_id = 0;
	if (priv->error) {
		GError *error;

		error = priv->error;
		priv->error = NULL;
		brasero_job_error (BRASERO_JOB (self), error);
		return FALSE;
	}

	brasero_job_get_action (BRASERO_JOB (self), &action);
	if (action == BRASERO_JOB_ACTION_IMAGE) {
		GError *error = NULL;

		brasero_libisofs_create_image (self, &error);
		if (error)
			brasero_job_error (BRASERO_JOB (self), error);
		else
			return FALSE;
	}

	brasero_job_finished_track (BRASERO_JOB (self));
	return FALSE;
}

static gint
brasero_libisofs_sort_graft_points (gconstpointer a, gconstpointer b)
{
	const BraseroGraftPt *graft_a, *graft_b;
	gint len_a, len_b;

	graft_a = a;
	graft_b = b;

	/* we only want to know if:
	 * - a is a parent of b (a > b, retval < 0) 
	 * - b is a parent of a (b > a, retval > 0). */
	len_a = strlen (graft_a->path);
	len_b = strlen (graft_b->path);

	return len_a - len_b;
}

static int 
brasero_libisofs_import_read (IsoDataSource *src, uint32_t lba, uint8_t *buffer)
{
	struct burn_drive *d;
	off_t data_count;
	gint result;

	d = (struct burn_drive*)src->data;

	result = burn_read_data(d,
				(off_t) lba * (off_t) 2048,
				(char*)buffer, 
				2048,
				&data_count,
				0);
	if (result < 0 )
		return -1; /* error */

	return 1;
}

static int
brasero_libisofs_import_open (IsoDataSource *src)
{
	return 1;
}

static int
brasero_libisofs_import_close (IsoDataSource *src)
{
	return 1;
}
    
static void 
brasero_libisofs_import_free (IsoDataSource *src)
{ }

static BraseroBurnResult
brasero_libisofs_import_last_session (BraseroLibisofs *self,
				      IsoImage *image,
				      IsoWriteOpts *wopts,
				      GError **error)
{
	int result;
	IsoReadOpts *opts;
	BraseroMedia media;
	IsoDataSource *src;
	goffset start_block;
	goffset session_block;
	BraseroLibisofsPrivate *priv;

	priv = BRASERO_LIBISOFS_PRIVATE (self);

	priv->ctx = brasero_libburn_common_ctx_new (BRASERO_JOB (self), FALSE, error);
	if (!priv->ctx)
		return BRASERO_BURN_ERR;

	result = iso_read_opts_new (&opts, 0);
	if (result < 0) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("Read options could not be created"));
		return BRASERO_BURN_ERR;
	}

	src = g_new0 (IsoDataSource, 1);
	src->version = 0;
	src->refcount = 1;
	src->read_block = brasero_libisofs_import_read;
	src->open = brasero_libisofs_import_open;
	src->close = brasero_libisofs_import_close;
	src->free_data = brasero_libisofs_import_free;
	src->data = priv->ctx->drive;

	brasero_job_get_last_session_address (BRASERO_JOB (self), &session_block);
	iso_read_opts_set_start_block (opts, session_block);

	/* import image */
	result = iso_image_import (image, src, opts, NULL);
	iso_data_source_unref (src);
	iso_read_opts_free (opts);

	/* release the drive */
	if (priv->ctx) {
		/* This may not be a good idea ...*/
		brasero_libburn_common_ctx_free (priv->ctx);
		priv->ctx = NULL;
	}

	if (result < 0) {
		BRASERO_JOB_LOG (self, "Import failed 0x%x", result);
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_IMAGE_LAST_SESSION,
			     _("Last session import failed"));	
		return BRASERO_BURN_ERR;
	}

	/* check is this is a DVD+RW */
	brasero_job_get_next_writable_address (BRASERO_JOB (self), &start_block);

	brasero_job_get_media (BRASERO_JOB (self), &media);
	if (BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDRW_PLUS)
	||  BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDRW_RESTRICTED)
	||  BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDRW_PLUS_DL)) {
		/* This is specific to overwrite media; the start address is the
		 * size of all the previous data written */
		BRASERO_JOB_LOG (self, "Growing image (start %i)", start_block);
	}

	/* set the start block for the multisession image */
	iso_write_opts_set_ms_block (wopts, start_block);
	iso_write_opts_set_appendable (wopts, 1);

	iso_tree_set_replace_mode (image, ISO_REPLACE_ALWAYS);
	return BRASERO_BURN_OK;
}

static gpointer
brasero_libisofs_create_volume_thread (gpointer data)
{
	BraseroLibisofs *self = BRASERO_LIBISOFS (data);
	BraseroLibisofsPrivate *priv;
	BraseroTrack *track = NULL;
	IsoWriteOpts *opts = NULL;
	IsoImage *image = NULL;
	BraseroBurnFlag flags;
	GSList *grafts = NULL;
	gchar *label = NULL;
	gchar *publisher;
	GSList *excluded;
	GSList *iter;

	priv = BRASERO_LIBISOFS_PRIVATE (self);

	if (priv->libburn_src) {
		burn_source_free (priv->libburn_src);
		priv->libburn_src = NULL;
	}

	BRASERO_JOB_LOG (self, "creating volume");

	/* create volume */
	brasero_job_get_data_label (BRASERO_JOB (self), &label);
	if (!iso_image_new (label, &image)) {
		priv->error = g_error_new (BRASERO_BURN_ERROR,
					   BRASERO_BURN_ERROR_GENERAL, "%s",
					   _("Volume could not be created"));
		g_free (label);
		goto end;
	}

	iso_write_opts_new (&opts, 2);
	iso_write_opts_set_relaxed_vol_atts(opts, 1);

	brasero_job_get_flags (BRASERO_JOB (self), &flags);
	if (flags & BRASERO_BURN_FLAG_MERGE) {
		BraseroBurnResult result;

		result = brasero_libisofs_import_last_session (self,
							       image,
							       opts,
							       &priv->error);
		if (result != BRASERO_BURN_OK) {
			g_free (label);
			goto end;
		}
	}
	else if (flags & BRASERO_BURN_FLAG_APPEND) {
		goffset start_block;

		brasero_job_get_next_writable_address (BRASERO_JOB (self), &start_block);
		iso_write_opts_set_ms_block (opts, start_block);
	}

	/* set label but set it after merging so the
	 * new does not get replaced by the former */
	publisher = g_strdup_printf ("Brasero-%i.%i.%i",
				     BRASERO_MAJOR_VERSION,
				     BRASERO_MINOR_VERSION,
				     BRASERO_SUB);

	if (label)
		iso_image_set_volume_id (image, label);

	iso_image_set_publisher_id (image, publisher);
	iso_image_set_data_preparer_id (image, g_get_real_name ());

	g_free (publisher);
	g_free (label);

	brasero_job_start_progress (BRASERO_JOB (self), FALSE);

	/* copy the list as we're going to reorder it */
	brasero_job_get_current_track (BRASERO_JOB (self), &track);
	grafts = brasero_track_data_get_grafts (BRASERO_TRACK_DATA (track));
	grafts = g_slist_copy (grafts);
	grafts = g_slist_sort (grafts, brasero_libisofs_sort_graft_points);

	/* add global exclusions */
	for (excluded = brasero_track_data_get_excluded_list (BRASERO_TRACK_DATA (track));
	     excluded; excluded = excluded->next) {
		gchar *uri, *local;

		uri = excluded->data;
		local = g_filename_from_uri (uri, NULL, NULL);
		iso_tree_add_exclude (image, local);
		g_free (local);
	}

	for (iter = grafts; iter; iter = iter->next) {
		BraseroGraftPt *graft;
		gboolean is_directory;
		gchar *path_parent;
		gchar *path_name;
		IsoNode *parent;

		if (priv->cancel)
			goto end;

		graft = iter->data;

		BRASERO_JOB_LOG (self,
				 "Adding graft disc path = %s, URI = %s",
				 graft->path,
				 graft->uri);

		/* search for parent node.
		 * NOTE: because of mkisofs/genisoimage, we add a "/" at the end
		 * directories. So make sure there isn't one when getting the 
		 * parent path or g_path_get_dirname () will return the same
		 * exact name */
		if (g_str_has_suffix (graft->path, G_DIR_SEPARATOR_S)) {
			gchar *tmp;

			/* remove trailing "/" */
			tmp = g_strdup (graft->path);
			tmp [strlen (tmp) - 1] = '\0';
			path_parent = g_path_get_dirname (tmp);
			path_name = g_path_get_basename (tmp);
			g_free (tmp);

			is_directory = TRUE;
		}
		else {
			path_parent = g_path_get_dirname (graft->path);
			path_name = g_path_get_basename (graft->path);
			is_directory = FALSE;
		}

		iso_tree_path_to_node (image, path_parent, &parent);
		g_free (path_parent);

		if (!parent) {
			/* an error has occurred, possibly libisofs hasn't been
			 * able to find a parent for this node */
			g_free (path_name);
			priv->error = g_error_new (BRASERO_BURN_ERROR,
						   BRASERO_BURN_ERROR_GENERAL,
						   /* Translators: %s is the path */
						   _("No parent could be found in the tree for the path \"%s\""),
						   graft->path);
			goto end;
		}

		BRASERO_JOB_LOG (self, "Found parent");

		/* add the file/directory to the volume */
		if (graft->uri) {
			gchar *local_path;
			IsoDirIter *sibling;

			/* graft->uri can be a path or a URI */
			if (graft->uri [0] == '/')
				local_path = g_strdup (graft->uri);
			else if (g_str_has_prefix (graft->uri, "file://"))
				local_path = g_filename_from_uri (graft->uri, NULL, NULL);
			else
				local_path = NULL;

			if (!local_path){
				priv->error = g_error_new (BRASERO_BURN_ERROR,
							   BRASERO_BURN_ERROR_FILE_NOT_LOCAL,
							   _("The file is not stored locally"));
				g_free (path_name);
				goto end;
			}

			/* see if the node exists with the same name among the 
			 * children of the parent directory. If there is a
			 * sibling destroy it. */
			sibling = NULL;
			iso_dir_get_children (ISO_DIR (parent), &sibling);

			IsoNode *node;
			while (iso_dir_iter_next (sibling, &node) == 1) {
				const gchar *iso_name;

				/* check if it has the same name */
				iso_name = iso_node_get_name (node);
				if (iso_name && !strcmp (iso_name, path_name))
					BRASERO_JOB_LOG (self,
							 "Found sibling for %s: removing %x",
							 path_name,
							 iso_dir_iter_remove (sibling));
			}

			if  (is_directory) {
				int result;
				IsoDir *directory;

				/* add directory node */
				result = iso_tree_add_new_dir (ISO_DIR (parent), path_name, &directory);
				if (result < 0) {
					BRASERO_JOB_LOG (self,
							 "ERROR %s %x",
							 path_name,
							 result);
					priv->error = g_error_new (BRASERO_BURN_ERROR,
								   BRASERO_BURN_ERROR_GENERAL,
								   _("libisofs reported an error while creating directory \"%s\""),
								   graft->path);
					g_free (path_name);
					goto end;
				}

				/* add contents */
				result = iso_tree_add_dir_rec (image, directory, local_path);
				if (result < 0) {
					BRASERO_JOB_LOG (self,
							 "ERROR %s %x",
							 path_name,
							 result);
					priv->error = g_error_new (BRASERO_BURN_ERROR,
								   BRASERO_BURN_ERROR_GENERAL,
								   _("libisofs reported an error while adding contents to directory \"%s\" (%x)"),
								   graft->path,
								   result);
					g_free (path_name);
					goto end;
				}
			}
			else {
				IsoNode *node;
				int err;

				err = iso_tree_add_new_node (image,
							 ISO_DIR (parent),
				                         path_name,
							 local_path,
							 &node);
				if (err < 0) {
					BRASERO_JOB_LOG (self,
							 "ERROR %s %x",
							 path_name,
							 err);
					priv->error = g_error_new (BRASERO_BURN_ERROR,
								   BRASERO_BURN_ERROR_GENERAL,
								   _("libisofs reported an error while adding file at path \"%s\""),
								   graft->path);
					g_free (path_name);
					goto end;
				}

				if (iso_node_get_name (node)
				&&  strcmp (iso_node_get_name (node), path_name)) {
					err = iso_node_set_name (node, path_name);
					if (err < 0) {
						BRASERO_JOB_LOG (self,
								 "ERROR %s %x",
								 path_name,
								 err);
						priv->error = g_error_new (BRASERO_BURN_ERROR,
									   BRASERO_BURN_ERROR_GENERAL,
									   _("libisofs reported an error while adding file at path \"%s\""),
									   graft->path);
						g_free (path_name);
						goto end;
					}
				}
			}

			g_free (local_path);
		}
		else if (iso_tree_add_new_dir (ISO_DIR (parent), path_name, NULL) < 0) {
			priv->error = g_error_new (BRASERO_BURN_ERROR,
						   BRASERO_BURN_ERROR_GENERAL,
						   _("libisofs reported an error while creating directory \"%s\""),
						   graft->path);
			g_free (path_name);
			goto end;

		}

		g_free (path_name);
	}


end:

	if (grafts)
		g_slist_free (grafts);

	if (!priv->error && !priv->cancel) {
		gint64 size;
		BraseroImageFS image_fs;

		image_fs = brasero_track_data_get_fs (BRASERO_TRACK_DATA (track));

		if ((image_fs & BRASERO_IMAGE_FS_ISO)
		&&  (image_fs & BRASERO_IMAGE_ISO_FS_LEVEL_3))
			iso_write_opts_set_iso_level (opts, 3);
		else
			iso_write_opts_set_iso_level (opts, 2);

		iso_write_opts_set_rockridge (opts, 1);
		iso_write_opts_set_joliet (opts, (image_fs & BRASERO_IMAGE_FS_JOLIET) != 0);
		iso_write_opts_set_allow_deep_paths (opts, (image_fs & BRASERO_IMAGE_ISO_FS_DEEP_DIRECTORY) != 0);

		if (iso_image_create_burn_source (image, opts, &priv->libburn_src) >= 0) {
			size = priv->libburn_src->get_size (priv->libburn_src);
			brasero_job_set_output_size_for_current_track (BRASERO_JOB (self),
								       BRASERO_BYTES_TO_SECTORS (size, 2048),
								       size);
		}
	}

	if (opts)
		iso_write_opts_free (opts);

	if (image)
		iso_image_unref (image);

	/* End thread */
	g_mutex_lock (priv->mutex);

	/* It is important that the following is done inside the lock; indeed,
	 * if the main loop is idle then that brasero_libisofs_stop_real () can
	 * be called immediatly to stop the plugin while priv->thread is not
	 * NULL.
	 * As in this callback we check whether the thread is running (which
	 * means that we were cancelled) in some cases it would mean that we
	 * would cancel the libburn_src object and create crippled images. */
	if (!priv->cancel)
		priv->thread_id = g_idle_add (brasero_libisofs_create_volume_thread_finished, self);

	priv->thread = NULL;
	g_cond_signal (priv->cond);
	g_mutex_unlock (priv->mutex);

	g_thread_exit (NULL);

	return NULL;
}

static BraseroBurnResult
brasero_libisofs_create_volume (BraseroLibisofs *self, GError **error)
{
	BraseroLibisofsPrivate *priv;
	GError *thread_error = NULL;

	priv = BRASERO_LIBISOFS_PRIVATE (self);
	if (priv->thread)
		return BRASERO_BURN_RUNNING;

	if (iso_init () < 0) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("libisofs could not be initialized."));
		return BRASERO_BURN_ERR;
	}

	iso_set_msgs_severities ("NEVER", "ALL", "brasero (libisofs)");
	g_mutex_lock (priv->mutex);
	priv->thread = g_thread_create (brasero_libisofs_create_volume_thread,
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

static void
brasero_libisofs_clean_output (BraseroLibisofs *self)
{
	BraseroLibisofsPrivate *priv;

	priv = BRASERO_LIBISOFS_PRIVATE (self);

	if (priv->libburn_src) {
		burn_source_free (priv->libburn_src);
		priv->libburn_src = NULL;
	}

	if (priv->error) {
		g_error_free (priv->error);
		priv->error = NULL;
	}
}

static BraseroBurnResult
brasero_libisofs_start (BraseroJob *job,
			GError **error)
{
	BraseroLibisofs *self;
	BraseroJobAction action;
	BraseroLibisofsPrivate *priv;

	self = BRASERO_LIBISOFS (job);
	priv = BRASERO_LIBISOFS_PRIVATE (self);

	brasero_job_get_action (job, &action);
	if (action == BRASERO_JOB_ACTION_SIZE) {
		/* do this to avoid a problem when using
		 * DUMMY flag. libisofs would not generate
		 * a second time. */
		brasero_libisofs_clean_output (BRASERO_LIBISOFS (job));
		brasero_job_set_current_action (BRASERO_JOB (self),
						BRASERO_BURN_ACTION_GETTING_SIZE,
						NULL,
						FALSE);
		return brasero_libisofs_create_volume (self, error);
	}

	if (priv->error) {
		g_error_free (priv->error);
		priv->error = NULL;
	}

	/* we need the source before starting anything */
	if (!priv->libburn_src)
		return brasero_libisofs_create_volume (self, error);

	return brasero_libisofs_create_image (self, error);
}

static void
brasero_libisofs_stop_real (BraseroLibisofs *self)
{
	BraseroLibisofsPrivate *priv;

	priv = BRASERO_LIBISOFS_PRIVATE (self);

	/* Check whether we properly shut down or if we were cancelled */
	g_mutex_lock (priv->mutex);
	if (priv->thread) {
		/* NOTE: this can only happen when we're preparing the volumes
		 * for a multi session disc. At this point we're only running
		 * to get the size of the future volume and we can't race with
		 * libburn plugin that isn't operating at this stage. */
		if (priv->ctx) {
			brasero_libburn_common_ctx_free (priv->ctx);
			priv->ctx = NULL;
		}

		/* A thread is running. In this context we are probably cancelling */
		if (priv->libburn_src)
			priv->libburn_src->cancel (priv->libburn_src);

		priv->cancel = 1;
		g_cond_wait (priv->cond, priv->mutex);
		priv->cancel = 0;
	}
	g_mutex_unlock (priv->mutex);

	if (priv->thread_id) {
		g_source_remove (priv->thread_id);
		priv->thread_id = 0;
	}
}

static BraseroBurnResult
brasero_libisofs_stop (BraseroJob *job,
		       GError **error)
{
	BraseroLibisofs *self;

	self = BRASERO_LIBISOFS (job);
	brasero_libisofs_stop_real (self);
	return BRASERO_BURN_OK;
}

static void
brasero_libisofs_class_init (BraseroLibisofsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroJobClass *job_class = BRASERO_JOB_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroLibisofsPrivate));

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_libisofs_finalize;

	job_class->start = brasero_libisofs_start;
	job_class->stop = brasero_libisofs_stop;
}

static void
brasero_libisofs_init (BraseroLibisofs *obj)
{
	BraseroLibisofsPrivate *priv;

	priv = BRASERO_LIBISOFS_PRIVATE (obj);
	priv->mutex = g_mutex_new ();
	priv->cond = g_cond_new ();
}

static void
brasero_libisofs_finalize (GObject *object)
{
	BraseroLibisofs *cobj;
	BraseroLibisofsPrivate *priv;

	cobj = BRASERO_LIBISOFS (object);
	priv = BRASERO_LIBISOFS_PRIVATE (object);

	brasero_libisofs_stop_real (cobj);
	brasero_libisofs_clean_output (cobj);

	if (priv->mutex) {
		g_mutex_free (priv->mutex);
		priv->mutex = NULL;
	}

	if (priv->cond) {
		g_cond_free (priv->cond);
		priv->cond = NULL;
	}

	/* close libisofs library */
	iso_finish ();

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_libisofs_export_caps (BraseroPlugin *plugin)
{
	GSList *output;
	GSList *input;

	brasero_plugin_define (plugin,
			       "libisofs",
	                       NULL,
			       _("Creates disc images from a file selection"),
			       "Philippe Rouquier",
			       3);

	brasero_plugin_set_flags (plugin,
				  BRASERO_MEDIUM_CDR|
				  BRASERO_MEDIUM_CDRW|
				  BRASERO_MEDIUM_DVDR|
				  BRASERO_MEDIUM_DVDRW|
				  BRASERO_MEDIUM_DUAL_L|
				  BRASERO_MEDIUM_APPENDABLE|
				  BRASERO_MEDIUM_HAS_AUDIO|
				  BRASERO_MEDIUM_HAS_DATA,
				  BRASERO_BURN_FLAG_APPEND|
				  BRASERO_BURN_FLAG_MERGE,
				  BRASERO_BURN_FLAG_NONE);

	brasero_plugin_set_flags (plugin,
				  BRASERO_MEDIUM_DVDRW_PLUS|
				  BRASERO_MEDIUM_RESTRICTED|
				  BRASERO_MEDIUM_DUAL_L|
				  BRASERO_MEDIUM_APPENDABLE|
				  BRASERO_MEDIUM_CLOSED|
				  BRASERO_MEDIUM_HAS_DATA,
				  BRASERO_BURN_FLAG_APPEND|
				  BRASERO_BURN_FLAG_MERGE,
				  BRASERO_BURN_FLAG_NONE);

	output = brasero_caps_image_new (BRASERO_PLUGIN_IO_ACCEPT_FILE|
					 BRASERO_PLUGIN_IO_ACCEPT_PIPE,
					 BRASERO_IMAGE_FORMAT_BIN);

	input = brasero_caps_data_new (BRASERO_IMAGE_FS_ISO|
				       BRASERO_IMAGE_ISO_FS_DEEP_DIRECTORY|
				       BRASERO_IMAGE_ISO_FS_LEVEL_3|
				       BRASERO_IMAGE_FS_JOLIET);
	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (input);

	input = brasero_caps_data_new (BRASERO_IMAGE_FS_ISO|
				       BRASERO_IMAGE_ISO_FS_DEEP_DIRECTORY|
				       BRASERO_IMAGE_ISO_FS_LEVEL_3|
				       BRASERO_IMAGE_FS_SYMLINK);
	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (input);

	g_slist_free (output);

	brasero_plugin_register_group (plugin, _(LIBBURNIA_DESCRIPTION));
}
