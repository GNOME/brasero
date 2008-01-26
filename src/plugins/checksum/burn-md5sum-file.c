/***************************************************************************
 *            burn-sum.c
 *
 *  ven aoû  4 19:46:34 2006
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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/param.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <gmodule.h>

#include <libgnomevfs/gnome-vfs.h>

#include "burn-plugin.h"
#include "burn-job.h"
#include "burn-md5.h"
#include "burn-md5sum-file.h"
#include "burn-volume.h"
#include "brasero-ncb.h"

BRASERO_PLUGIN_BOILERPLATE (BraseroMd5sumFile, brasero_md5sum_file, BRASERO_TYPE_JOB, BraseroJob);

struct _BraseroMd5sumFilePrivate {
	BraseroMD5Ctx *ctx;
	BraseroMD5 md5;

	/* the path to read from when we check */
	gchar *sums_path;
	gint64 file_num;

	/* the FILE to write to when we generate */
	FILE *file;

	/* this is for the thread and the end of it */
	GThread *thread;
	gint end_id;

	guint cancel;
};
typedef struct _BraseroMd5sumFilePrivate BraseroMd5sumFilePrivate;

#define BRASERO_MD5SUM_FILE_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_MD5SUM_FILE, BraseroMd5sumFilePrivate))

static BraseroJobClass *parent_class = NULL;

static BraseroBurnResult
brasero_md5sum_file_start_md5 (BraseroMd5sumFile *self,
			       const gchar *path,
			       const gchar *graft_path,
			       GError **error)
{
	BraseroBurnResult result = BRASERO_BURN_OK;
	gchar md5_checksum [MD5_STRING_LEN + 1];
	BraseroMd5sumFilePrivate *priv;
	gint written;

	priv = BRASERO_MD5SUM_FILE_PRIVATE (self);

	/* write to the file */
	result = brasero_md5_file_to_string (priv->ctx,
					     path,
					     md5_checksum,
					     0,
					     -1,
					     error);
	if (result != BRASERO_BURN_OK)
		return result;

	written = fwrite (md5_checksum,
			  strlen (md5_checksum),
			  1,
			  priv->file);

	if (written != 1) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the md5 file couldn't be written to (%s)"),
			     strerror (errno));
			
		return BRASERO_BURN_ERR;
	}

	written = fwrite ("  ",
			  2,
			  1,
			  priv->file);

	/* NOTE: we remove the first "/" from path so the file can be
	 * used with md5sum at the root of the disc once mounted */
	written = fwrite (graft_path + 1,
			  strlen (graft_path + 1),
			  1,
			  priv->file);

	if (written != 1) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the md5 file couldn't be written to (%s)"),
			     strerror (errno));

		return BRASERO_BURN_ERR;
	}

	written = fwrite ("\n",
			  1,
			  1,
			  priv->file);

	return result;
}

static BraseroBurnResult
brasero_md5sum_file_explore_directory (BraseroMd5sumFile *self,
				       gint64 file_nb,
				       const gchar *directory,
				       const gchar *disc_path,
				       GHashTable *excludedH,
				       GError **error)
{
	BraseroBurnResult result = BRASERO_BURN_OK;
	BraseroMd5sumFilePrivate *priv;
	const gchar *name;
	GDir *dir;

	priv = BRASERO_MD5SUM_FILE_PRIVATE (self);

	dir = g_dir_open (directory, 0, error);
	if (!dir || *error)
		return BRASERO_BURN_ERR;

	while ((name = g_dir_read_name (dir))) {
		gchar *path;
		gchar *graft_path;

		if (priv->cancel) {
			result = BRASERO_BURN_CANCEL;
			break;
		}

		path = g_build_path (G_DIR_SEPARATOR_S, directory, name, NULL);
		if (g_hash_table_lookup (excludedH, path)) {
			g_free (path);
			continue;
		}

		graft_path = g_build_path (G_DIR_SEPARATOR_S, disc_path, name, NULL);
		if (g_file_test (path, G_FILE_TEST_IS_DIR)) {
			result = brasero_md5sum_file_explore_directory (self,
									file_nb,
									path,
									graft_path,
									excludedH,
									error);
			g_free (path);
			g_free (graft_path);

			if (result != BRASERO_BURN_OK)
				break;

			continue;
		}

		result = brasero_md5sum_file_start_md5 (self,
							path,
							graft_path,
							error);
		g_free (graft_path);
		g_free (path);

		if (result != BRASERO_BURN_OK)
			break;

		priv->file_num ++;
		brasero_job_set_progress (BRASERO_JOB (self),
					  (gdouble) priv->file_num /
					  (gdouble) file_nb);
	}
	g_dir_close (dir);

	/* NOTE: we don't care if the file is twice or more on the disc,
	 * that would be too much overhead/memory consumption for something
	 * that scarcely happens and that way each file can be checked later*/

	return result;
}

static gboolean
brasero_md5sum_file_clean_excluded_table_cb (gpointer key,
					     gpointer data,
					     gpointer user_data)
{
	if (GPOINTER_TO_INT (data) == 1)
		return TRUE;

	return FALSE;
}

static BraseroBurnResult
brasero_md5sum_file_grafts (BraseroMd5sumFile *self, GError **error)
{
	GSList *iter;
	gint64 file_nb;
	BraseroTrack *track;
	GHashTable *excludedH;
	BraseroMd5sumFilePrivate *priv;
	BraseroBurnResult result = BRASERO_BURN_OK;

	priv = BRASERO_MD5SUM_FILE_PRIVATE (self);

	/* opens a file for the sums */
	result = brasero_job_get_tmp_file (BRASERO_JOB (self),
					   ".md5",
					   &priv->sums_path,
					   error);
	if (result != BRASERO_BURN_OK)
		return result;

	priv->file = fopen (priv->sums_path, "w");
	if (!priv->file) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("md5 file couldn't be opened (%s)"),
			     strerror (errno));

		return BRASERO_BURN_ERR;
	}

	if (brasero_job_get_current_track (BRASERO_JOB (self), &track) != BRASERO_BURN_OK) 
		BRASERO_JOB_NOT_SUPPORTED (self);

	/* we fill a hash table with all the files that are excluded globally */
	excludedH = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	iter = brasero_track_get_data_excluded_source (track, FALSE);
	for (; iter; iter = iter->next) {
		gchar *uri;
		gchar *path;

		/* get the path */
		uri = iter->data;
		path = gnome_vfs_get_local_path_from_uri (uri);

		if (path)
			g_hash_table_insert (excludedH, path, path);
	}

	priv->ctx = brasero_md5_new ();

	/* it's now time to start reporting our progress */
	brasero_job_set_current_action (BRASERO_JOB (self),
				        BRASERO_BURN_ACTION_CHECKSUM,
					_("Creating checksum for image files"),
					TRUE);

	file_nb = -1;
	priv->file_num = 0;
	brasero_track_get_data_file_num (track, &file_nb);
	if (file_nb > 0)
		brasero_job_start_progress (BRASERO_JOB (self), TRUE);
	else
		file_nb = -1;

	iter = brasero_track_get_data_grafts_source (track);
	for (; iter; iter = iter->next) {
		BraseroGraftPt *graft;
		gchar *graft_path;
		GSList *excluded;
		gchar *path;
		gchar *uri;

		if (priv->cancel) {
			result = BRASERO_BURN_CANCEL;
			break;
		}

		graft = iter->data;
		if (!graft->uri)
			continue;

		/* add all excluded in the excluded graft hash */
		for (excluded = graft->excluded; excluded; excluded = excluded->next) {
			uri = excluded->data;
			path = gnome_vfs_get_local_path_from_uri (uri);
			g_hash_table_insert (excludedH, path, GINT_TO_POINTER (1));
		}

		/* get the current and futures paths */
		uri = graft->uri;
		path = gnome_vfs_get_local_path_from_uri (uri);
		graft_path = graft->path;

		if (g_file_test (path, G_FILE_TEST_IS_DIR))
			result = brasero_md5sum_file_explore_directory (self,
								   file_nb,
								   path,
								   graft_path,
								   excludedH,
								   error);
		else {
			result = brasero_md5sum_file_start_md5 (self,
							   path,
							   graft_path,
							   error);
			priv->file_num ++;
			brasero_job_set_progress (BRASERO_JOB (self),
						  (gdouble) priv->file_num /
						  (gdouble) file_nb);
		}

		g_free (path);

		/* clean excluded hash table of all graft point excluded */
		g_hash_table_foreach_remove (excludedH,
					     brasero_md5sum_file_clean_excluded_table_cb,
					     NULL);

		if (result != BRASERO_BURN_OK)
			break;
	}

	brasero_md5_free (priv->ctx);
	priv->ctx = NULL;

	g_hash_table_destroy (excludedH);

	/* that's finished we close the file */
	fclose (priv->file);
	priv->file = NULL;

	return result;
}

static gint
brasero_md5sum_file_get_line_num (BraseroMd5sumFile *self,
			     FILE *file,
			     GError **error)
{
	gint c;
	gint num = 0;

	while ((c = getc (file)) != EOF) {
		if (c == '\n')
			num ++;
	}

	if (!feof (file)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     strerror (errno));
		return -1;
	}

	rewind (file);
	return num;
}

static BraseroBurnResult
brasero_md5sum_file_disc_files (BraseroMd5sumFile *self, GError **error)
{
	gchar *root;
	gchar *path;
	gint root_len;
	guint file_nb;
	guint file_num;
	FILE *file = NULL;
	const gchar *name;
	BraseroTrack *track;
	gboolean has_wrongsums;
	NautilusBurnDrive *drive;
	BraseroMd5sumFilePrivate *priv;
	gchar filename [MAXPATHLEN + 1];
	BraseroBurnResult result = BRASERO_BURN_OK;

	priv = BRASERO_MD5SUM_FILE_PRIVATE (self);

	has_wrongsums = FALSE;

	brasero_job_get_current_track (BRASERO_JOB (self), &track);
	drive = brasero_track_get_drive_source (track);
	root = NCB_VOLUME_GET_MOUNT_POINT (drive, error);
	if (!root)
		return BRASERO_BURN_ERR;

	memcpy (filename, root, sizeof (filename));
	root_len = strlen (root);
	filename [root_len ++ ] = '/';

	name = brasero_track_get_checksum (track);
	path = g_build_path (G_DIR_SEPARATOR_S, root, name, NULL);

	file = fopen (path, "r");

	g_free (root);
	g_free (path);
	if (!file) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     strerror (errno));
		return BRASERO_BURN_ERR;
	}

	/* we need to get the number of files at this time and rewind */
	file_nb = brasero_md5sum_file_get_line_num (self, file, error);
	if (file_nb < 1) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     strerror (errno));
		fclose (file);
		return BRASERO_BURN_ERR;
	}

	file_num = 0;
	priv->ctx = brasero_md5_new ();
	while (1) {
		gchar checksum_file [MD5_STRING_LEN + 1];
		gchar checksum_real [MD5_STRING_LEN + 1];
		gint i;
		int c;

		if (priv->cancel)
			break;

		/* first read the checksum string */
		if (fread (checksum_file, 1, MD5_STRING_LEN, file) != MD5_STRING_LEN) {
			if (!feof (file))
				g_set_error (error,
					     BRASERO_BURN_ERROR,
					     BRASERO_BURN_ERROR_GENERAL,
					     strerror (errno));
			break;
		}

		checksum_file [MD5_STRING_LEN] = '\0';
		if (priv->cancel)
			break;

		/* skip spaces in between */
		while (1) {
			c = fgetc (file);

			if (c == EOF) {
				if (feof (file))
					goto end;

				if (errno == EAGAIN || errno == EINTR)
					continue;

				g_set_error (error,
					     BRASERO_BURN_ERROR,
					     BRASERO_BURN_ERROR_GENERAL,
					     strerror (errno));
				goto end;
			}

			if (!isspace (c)) {
				filename [root_len] = c;
				break;
			}
		}

		/* get the filename */
		i = root_len + 1;
		while (1) {
			c = fgetc (file);
			if (c == EOF) {
				if (feof (file))
					goto end;

				if (errno == EAGAIN || errno == EINTR)
					continue;

				g_set_error (error,
					     BRASERO_BURN_ERROR,
					     BRASERO_BURN_ERROR_GENERAL,
					     strerror (errno));
				goto end;
			}

			if (c == '\n')
				break;

			if (i < MAXPATHLEN)
				filename [i ++] = c;
		}

		if (i > MAXPATHLEN) {
			/* we ignore paths that are too long */
			continue;
		}

		filename [i] = 0;
		result = brasero_md5_file_to_string (priv->ctx,
						     filename,
						     checksum_real,
						     0,
						     -1,
						     error);
		if (result == BRASERO_BURN_RETRY)
			checksum_real [0] = '\0';
		else if (result != BRASERO_BURN_OK)
			break;

		file_num++;
		brasero_job_set_progress (BRASERO_JOB (self), (gdouble) file_num / (gdouble) file_nb);

		BRASERO_JOB_LOG (self,
				 "comparing checksums for file %s : %s (from md5 file) / %s (current)",
				 filename, checksum_file, checksum_real);
		if (strcmp (checksum_file, checksum_real)) {
			has_wrongsums = TRUE;
			brasero_job_add_wrong_checksum (BRASERO_JOB (self), filename);
		}

		if (priv->cancel)
			break;
	}

end:
	if (file)
		fclose (file);

	if (priv->ctx) {
		brasero_md5_free (priv->ctx);
		priv->ctx = NULL;
	}

	if (result != BRASERO_BURN_OK)
		return result;

	if (has_wrongsums) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_BAD_CHECKSUM,
			     _("some files may be corrupted on the disc"));
		return BRASERO_BURN_ERR;
	}

	return BRASERO_BURN_OK;
}

struct _BraseroMd5sumFileThreadCtx {
	BraseroMd5sumFile *sum;
	BraseroBurnResult result;
	GError *error;
};
typedef struct _BraseroMd5sumFileThreadCtx BraseroMd5sumFileThreadCtx;

static gboolean
brasero_md5sum_file_end (gpointer data)
{
	BraseroMd5sumFile *self;
	BraseroTrack *track;
	BraseroTrackType input;
	BraseroJobAction action;
	BraseroBurnResult result;
	BraseroMd5sumFilePrivate *priv;
	BraseroMd5sumFileThreadCtx *ctx;
	gchar checksum [MD5_STRING_LEN + 1];

	ctx = data;
	self = ctx->sum;
	priv = BRASERO_MD5SUM_FILE_PRIVATE (self);

	/* NOTE ctx/data is destroyed in its own callback */
	priv->end_id = 0;

	if (ctx->result != BRASERO_BURN_OK) {
		GError *error;

		error = ctx->error;
		ctx->error = NULL;

		brasero_job_error (BRASERO_JOB (self), error);
		return FALSE;
	}

	brasero_job_get_action (BRASERO_JOB (self), &action);
	if (action == BRASERO_JOB_ACTION_CHECKSUM) {
		BraseroChecksumType type;

		/* we were asked to check the sum of the track so get the type
		 * of the checksum first to see what to do */
		track = NULL;
		brasero_job_get_current_track (BRASERO_JOB (self), &track);
		type = brasero_track_get_checksum_type (track);

		if (type == BRASERO_CHECKSUM_MD5_FILE) {
			/* in this case all was already set in session */
			brasero_job_finished_track (BRASERO_JOB (self));
			return FALSE;
		}

		/* DISC checking. Set the checksum for the track and at the same
		 * time compare it to a potential one */
		brasero_md5_string (&priv->md5, checksum);
		checksum [MD5_STRING_LEN] = '\0';

		BRASERO_JOB_LOG (self,
				 "setting new checksum (type = %i) %s (%s before)",
				 type,
				 checksum,
				 brasero_track_get_checksum (track));

		result = brasero_track_set_checksum (track,
						     BRASERO_CHECKSUM_MD5,
						     checksum);
		if (result != BRASERO_BURN_OK)
			goto error;

		brasero_job_finished_track (BRASERO_JOB (self));
		return FALSE;
	}

	/* we were asked to create a checksum. Its type depends on the input */
	brasero_job_get_input_type (BRASERO_JOB (self), &input);

	/* let's create a new DATA track with the md5 file created */
	if (input.type == BRASERO_TRACK_TYPE_DATA) {
		GSList *grafts;
		GSList *excluded;
		BraseroGraftPt *graft;
		BraseroTrackType type;
		GSList *new_grafts = NULL;

		/* for DATA track we add the file to the track */
		brasero_job_get_current_track (BRASERO_JOB (self), &track);
		brasero_track_get_type (track, &type);
		grafts = brasero_track_get_data_grafts_source (track);
		excluded = brasero_track_get_data_excluded_source (track, TRUE);

		for (; grafts; grafts = grafts->next) {
			graft = grafts->data;
			graft = brasero_graft_point_copy (graft);
			new_grafts = g_slist_prepend (new_grafts, graft);
		}

		graft = g_new0 (BraseroGraftPt, 1);
		graft->uri = g_strconcat ("file://", priv->sums_path, NULL);
		graft->path = g_strdup ("/"BRASERO_CHECKSUM_FILE);
		new_grafts = g_slist_prepend (new_grafts, graft);

		track = brasero_track_new (BRASERO_TRACK_TYPE_DATA);
		brasero_track_add_data_fs (track, type.subtype.fs_type);
		brasero_track_set_data_source (track, new_grafts, excluded);

		brasero_track_set_checksum (track,
					    BRASERO_CHECKSUM_MD5_FILE,
					    graft->uri);

		brasero_job_add_track (BRASERO_JOB (self), track);

		/* It's good practice to unref the track afterwards as we don't
		 * need it anymore. BraseroTaskCtx refs it. */
		brasero_track_unref (track);
		
		brasero_job_finished_track (BRASERO_JOB (self));
		return FALSE;
	}
	else if (input.type == BRASERO_TRACK_TYPE_IMAGE) {
		track = NULL;
		brasero_job_get_current_track (BRASERO_JOB (self), &track);

		brasero_md5_string (&priv->md5, checksum);
		checksum [MD5_STRING_LEN] = '\0';

		BRASERO_JOB_LOG (self,
				 "setting new checksum %s (%s before)",
				 checksum,
				 brasero_track_get_checksum (track));

		result = brasero_track_set_checksum (track,
						     BRASERO_CHECKSUM_MD5,
						     checksum);
		if (result != BRASERO_BURN_OK)
			goto error;

		brasero_job_finished_track (BRASERO_JOB (self));
	}
	else
		goto error;

	return FALSE;

error:
{
	GError *error = NULL;

	error = g_error_new (BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_BAD_CHECKSUM,
			     _("some files may be corrupted on the disc"));
	brasero_job_error (BRASERO_JOB (self), error);
	return FALSE;
}
}

static void
brasero_md5sum_file_destroy (gpointer data)
{
	BraseroMd5sumFileThreadCtx *ctx;

	ctx = data;
	if (ctx->error) {
		g_error_free (ctx->error);
		ctx->error = NULL;
	}

	g_free (ctx);
}

static gpointer
brasero_md5sum_file_thread (gpointer data)
{
	BraseroMd5sumFile *self;
	GError *error = NULL;
	BraseroJobAction action;
	BraseroMd5sumFilePrivate *priv;
	BraseroMd5sumFileThreadCtx *ctx;
	BraseroBurnResult result = BRASERO_BURN_NOT_SUPPORTED;

	self = BRASERO_MD5SUM_FILE (data);
	priv = BRASERO_MD5SUM_FILE_PRIVATE (self);

	/* check DISC types and add checksums for DATA and IMAGE-bin types */
	brasero_job_get_action (BRASERO_JOB (self), &action);

	if (action == BRASERO_JOB_ACTION_CHECKSUM) {
		BraseroChecksumType type;
		BraseroTrack *track;

		brasero_job_get_current_track (BRASERO_JOB (self), &track);
		type = brasero_track_get_checksum_type (track);
		if (type == BRASERO_CHECKSUM_MD5_FILE)
			result = brasero_md5sum_file_disc_files (self, &error);
		else
			result = BRASERO_BURN_ERR;
	}
	else if (action == BRASERO_JOB_ACTION_IMAGE) {
		BraseroTrackType type;

		brasero_job_get_input_type (BRASERO_JOB (self), &type);
		if (type.type == BRASERO_TRACK_TYPE_DATA)
			result = brasero_md5sum_file_grafts (self, &error);
		else
			result = BRASERO_BURN_ERR;
	}

	if (result != BRASERO_BURN_CANCEL) {
		ctx = g_new0 (BraseroMd5sumFileThreadCtx, 1);
		ctx->sum = self;
		ctx->error = error;
		ctx->result = result;
		priv->end_id = g_idle_add_full (G_PRIORITY_HIGH_IDLE,
						brasero_md5sum_file_end,
						ctx,
						brasero_md5sum_file_destroy);
	}

	priv->thread = NULL;
	g_thread_exit (NULL);
	return NULL;
}

static BraseroBurnResult
brasero_md5sum_file_start (BraseroJob *job,
			   GError **error)
{
	BraseroMd5sumFilePrivate *priv;
	BraseroJobAction action;

	brasero_job_get_action (job, &action);
	if (action == BRASERO_JOB_ACTION_SIZE)
		return BRASERO_BURN_NOT_SUPPORTED;

	/* we start a thread for the exploration of the graft points */
	priv = BRASERO_MD5SUM_FILE_PRIVATE (job);
	priv->thread = g_thread_create (brasero_md5sum_file_thread,
					BRASERO_MD5SUM_FILE (job),
					TRUE,
					error);

	if (!priv->thread)
		return BRASERO_BURN_ERR;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_md5sum_file_activate (BraseroJob *job,
			      GError **error)
{
	GSList *grafts;
	BraseroTrackType output;
	BraseroTrack *track = NULL;

	brasero_job_get_output_type (job, &output);
	if (output.type != BRASERO_TRACK_TYPE_DATA)
		return BRASERO_BURN_OK;

	/* see that a file with graft "/BRASERO_CHECKSUM_FILE" doesn't already
	 * exists (possible when doing several copies) */
	brasero_job_get_current_track (job, &track);
	grafts = brasero_track_get_data_grafts_source (track);
	for (; grafts; grafts = grafts->next) {
		BraseroGraftPt *graft;

		graft = grafts->data;
		if (graft->path 
		&& !strcmp (graft->path, "/"BRASERO_CHECKSUM_FILE))
			return BRASERO_BURN_NOT_RUNNING;
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_md5sum_file_clock_tick (BraseroJob *job)
{
	BraseroMd5sumFilePrivate *priv;

	priv = BRASERO_MD5SUM_FILE_PRIVATE (job);
	if (!priv->ctx)
		return BRASERO_BURN_OK;

	/* we'll need that function later. For the moment, when generating a
	 * file we can't know how many files there are. Just when checking it */

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_md5sum_file_stop (BraseroJob *job,
		     GError **error)
{
	BraseroMd5sumFilePrivate *priv;

	priv = BRASERO_MD5SUM_FILE_PRIVATE (job);

	if (priv->ctx)
		brasero_md5_cancel (priv->ctx);

	if (priv->thread) {
		priv->cancel = 1;
		g_thread_join (priv->thread);
		priv->cancel = 0;
		priv->thread = NULL;
	}

	if (priv->end_id) {
		g_source_remove (priv->end_id);
		priv->end_id = 0;
	}

	if (priv->file) {
		fclose (priv->file);
		priv->file = NULL;
	}

	if (priv->sums_path) {
		g_free (priv->sums_path);
		priv->sums_path = NULL;
	}

	return BRASERO_BURN_OK;
}

static void
brasero_md5sum_file_init (BraseroMd5sumFile *obj)
{ }

static void
brasero_md5sum_file_finalize (GObject *object)
{
	BraseroMd5sumFilePrivate *priv;
	
	priv = BRASERO_MD5SUM_FILE_PRIVATE (object);

	if (priv->thread) {
		priv->cancel = 1;
		g_thread_join (priv->thread);
		priv->cancel = 0;
		priv->thread = NULL;
	}

	if (priv->end_id) {
		g_source_remove (priv->end_id);
		priv->end_id = 0;
	}

	if (priv->file) {
		fclose (priv->file);
		priv->file = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_md5sum_file_class_init (BraseroMd5sumFileClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroJobClass *job_class = BRASERO_JOB_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroMd5sumFilePrivate));

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_md5sum_file_finalize;

	job_class->activate = brasero_md5sum_file_activate;
	job_class->start = brasero_md5sum_file_start;
	job_class->stop = brasero_md5sum_file_stop;
	job_class->clock_tick = brasero_md5sum_file_clock_tick;
}

static BraseroBurnResult
brasero_md5sum_file_export_caps (BraseroPlugin *plugin, gchar **error)
{
	GSList *input;

	brasero_plugin_define (plugin,
			       "md5sum-file",
			       _("allows to check file integrities on a disc"),
			       "Philippe Rouquier",
			       0);

	/* we can only generate a file for DATA input */
	input = brasero_caps_data_new (BRASERO_IMAGE_FS_ANY);
	brasero_plugin_process_caps (plugin, input);
	g_slist_free (input);

	brasero_plugin_set_process_flags (plugin,
					  BRASERO_PLUGIN_RUN_FIRST|
					  BRASERO_PLUGIN_RUN_LAST);

	/* For discs, we can only check each files on a disc against an md5sum 
	 * file (provided we managed to mount the disc).
	 * NOTE: we can't generate md5 from discs anymore. There are too many
	 * problems reading straight from the disc dev. So we use readcd or 
	 * equivalent instead */
	input = brasero_caps_disc_new (BRASERO_MEDIUM_CD|
				       BRASERO_MEDIUM_DVD|
				       BRASERO_MEDIUM_PLUS|
				       BRASERO_MEDIUM_RESTRICTED|
				       BRASERO_MEDIUM_SEQUENTIAL|
				       BRASERO_MEDIUM_WRITABLE|
				       BRASERO_MEDIUM_REWRITABLE|
				       BRASERO_MEDIUM_CLOSED|
				       BRASERO_MEDIUM_APPENDABLE|
				       BRASERO_MEDIUM_HAS_DATA);
	brasero_plugin_check_caps (plugin,
				   BRASERO_CHECKSUM_MD5_FILE,
				   input);
	g_slist_free (input);

	return BRASERO_BURN_OK;
}
