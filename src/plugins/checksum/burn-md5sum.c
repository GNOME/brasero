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

#include <gconf/gconf-client.h>
#include <libgnomevfs/gnome-vfs.h>

#include "burn-plugin.h"
#include "burn-job.h"
#include "burn-md5.h"
#include "burn-md5sum.h"
#include "burn-volume.h"
#include "brasero-ncb.h"

#define GCONF_DONT_ADD_MD5SUM_FILE_TO_DATA	"/apps/brasero/config/dont_add_md5"

BRASERO_PLUGIN_BOILERPLATE (BraseroMd5sum, brasero_md5sum, BRASERO_TYPE_JOB, BraseroJob);

struct _BraseroMd5sumPrivate {
	BraseroMD5Ctx *ctx;
	BraseroMD5 md5;

	/* the path and fd for the file containing the md5 of files */
	gchar *sums_path;
	FILE *file;

	gint64 total;

	gint64 file_num;

	/* this is for the thread and the end of it */
	GThread *thread;
	gint end_id;

	guint cancel;
};
typedef struct _BraseroMd5sumPrivate BraseroMd5sumPrivate;

#define BRASERO_MD5SUM_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_MD5SUM, BraseroMd5sumPrivate))

static BraseroJobClass *parent_class = NULL;

static gint
brasero_md5sum_live_read (BraseroMd5sum *self,
			  int fd,
			  guchar *buffer,
			  gint bytes,
			  GError **error)
{
	gint total = 0;
	gint read_bytes;
	BraseroMd5sumPrivate *priv;

	priv = BRASERO_MD5SUM_PRIVATE (self);

	while (1) {
		read_bytes = read (fd, buffer + total, (bytes - total));

		/* maybe that's the end of the stream ... */
		if (!read_bytes)
			return total;

		if (priv->cancel)
			return -2;

		/* ... or an error =( */
		if (read_bytes == -1) {
			if (errno != EAGAIN && errno != EINTR) {
				g_set_error (error,
					     BRASERO_BURN_ERROR,
					     BRASERO_BURN_ERROR_GENERAL,
					     _("data could not be read from the pipe (%i: %s)"),
					     errno,
					     strerror (errno));
				return -1;
			}
		}
		else {
			total += read_bytes;

			if (total == bytes)
				return total;
		}

		g_usleep (500);
	}

	return total;
}

static BraseroBurnResult
brasero_md5sum_live_write (BraseroMd5sum *self,
			   int fd,
			   guchar *buffer,
			   gint bytes,
			   GError **error)
{
	gint bytes_remaining;
	gint bytes_written = 0;
	BraseroMd5sumPrivate *priv;

	priv = BRASERO_MD5SUM_PRIVATE (self);

	bytes_remaining = bytes;
	while (bytes_remaining) {
		gint written;

		written = write (fd,
				 buffer + bytes_written,
				 bytes_remaining);

		if (priv->cancel)
			return BRASERO_BURN_CANCEL;

		if (written != bytes_remaining) {
			if (errno != EINTR && errno != EAGAIN) {
				/* unrecoverable error */
				g_set_error (error,
					     BRASERO_BURN_ERROR,
					     BRASERO_BURN_ERROR_GENERAL,
					     _("the data couldn't be written to the pipe (%i: %s)"),
					     errno,
					     strerror (errno));
				return BRASERO_BURN_ERR;
			}
		}

		g_usleep (500);

		if (written > 0) {
			bytes_remaining -= written;
			bytes_written += written;
		}
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_md5sum_live (BraseroMd5sum *self,
		     GError **error)
{
	int fd_in = -1;
	int fd_out = -1;
	guint sum_bytes;
	gint read_bytes;
	guchar buffer [2048];
	BraseroBurnResult result;
	BraseroMd5sumPrivate *priv;

	priv = BRASERO_MD5SUM_PRIVATE (self);

	BRASERO_JOB_LOG (self, "starting md5 generation live");
	result = brasero_job_set_nonblocking (BRASERO_JOB (self), error);
	if (result != BRASERO_BURN_OK)
		return result;

	brasero_job_get_fd_in (BRASERO_JOB (self), &fd_in);
	brasero_job_get_fd_out (BRASERO_JOB (self), &fd_out);

	priv->ctx = brasero_md5_new ();
	brasero_md5_init (priv->ctx, &priv->md5);

	result = BRASERO_BURN_OK;
	while (1) {
		sum_bytes = 0;

		read_bytes = brasero_md5sum_live_read (self,
						       fd_in,
						       buffer,
						       sizeof (buffer),
						       error);
		if (read_bytes == -2) {
			result = BRASERO_BURN_CANCEL;
			goto end;
		}

		if (read_bytes == -1) {
			result = BRASERO_BURN_ERR;
			goto end;
		}

		if (!read_bytes)
			break;

		if (brasero_md5sum_live_write (self, fd_out, buffer, read_bytes, error) != BRASERO_BURN_OK)
			goto end;

		sum_bytes = brasero_md5_sum (priv->ctx,
					     &priv->md5,
					     buffer,
					     read_bytes);
		if (sum_bytes == -1) {
			result = BRASERO_BURN_CANCEL;
			goto end;
		}

		/* this could be a problem, disc recording is more important */
		if (sum_bytes)
			break;
	}

	brasero_md5_end (priv->ctx, &priv->md5, buffer + (read_bytes - sum_bytes), sum_bytes);

end:

	brasero_md5_free (priv->ctx);
	priv->ctx = NULL;

	return result;
}

static BraseroBurnResult
brasero_md5sum_start_md5 (BraseroMd5sum *self,
			  const gchar *path,
			  const gchar *graft_path,
			  GError **error)
{
	BraseroBurnResult result = BRASERO_BURN_OK;
	gchar md5_checksum [MD5_STRING_LEN + 1];
	BraseroMd5sumPrivate *priv;
	gint written;

	priv = BRASERO_MD5SUM_PRIVATE (self);

	/* write to the file */
	result = brasero_md5_file_to_string (priv->ctx,
					     path,
					     md5_checksum,
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
brasero_md5sum_explore_directory (BraseroMd5sum *self,
				  gint64 file_nb,
				  const gchar *directory,
				  const gchar *disc_path,
				  GHashTable *excludedH,
				  GError **error)
{
	BraseroBurnResult result = BRASERO_BURN_OK;
	BraseroMd5sumPrivate *priv;
	const gchar *name;
	GDir *dir;

	priv = BRASERO_MD5SUM_PRIVATE (self);

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
			result = brasero_md5sum_explore_directory (self,
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

		result = brasero_md5sum_start_md5 (self,
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
brasero_md5sum_clean_excluded_table_cb (gpointer key,
					gpointer data,
					gpointer user_data)
{
	if (GPOINTER_TO_INT (data) == 1)
		return TRUE;

	return FALSE;
}

static BraseroBurnResult
brasero_md5sum_grafts (BraseroMd5sum *self, GError **error)
{
	GSList *iter;
	gint64 file_nb;
	BraseroTrack *track;
	GHashTable *excludedH;
	BraseroMd5sumPrivate *priv;
	BraseroBurnResult result = BRASERO_BURN_OK;

	priv = BRASERO_MD5SUM_PRIVATE (self);

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

	file_nb = -1;
	priv->file_num = 0;
	brasero_track_get_data_file_num (track, &file_nb);
	if (!file_nb)
		file_nb = -1;

	/* we fill a hash table with all the files that are excluded globally */
	excludedH = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	iter = brasero_track_get_data_excluded_source (track);
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

	brasero_job_start_progress (BRASERO_JOB (self), TRUE);

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
			result = brasero_md5sum_explore_directory (self,
								   file_nb,
								   path,
								   graft_path,
								   excludedH,
								   error);
		else {
			result = brasero_md5sum_start_md5 (self,
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
					     brasero_md5sum_clean_excluded_table_cb,
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

static BraseroBurnResult
brasero_md5sum_image (BraseroMd5sum *self, GError **error)
{
	BraseroMd5sumPrivate *priv;
	BraseroBurnResult result;
	BraseroTrack *track;
	gchar *path;

	priv = BRASERO_MD5SUM_PRIVATE (self);

	if (brasero_job_get_fd_in (BRASERO_JOB (self), NULL) == BRASERO_BURN_OK)
		return brasero_md5sum_live (self, error);

	brasero_job_get_current_track (BRASERO_JOB (self), &track);
	path = brasero_track_get_image_source (track, FALSE);
	if (!path) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the image is not local"));
		return BRASERO_BURN_ERR;
	}

	result = brasero_track_get_image_size (track, NULL, NULL, &priv->total, error);
	if (result != BRASERO_BURN_OK)
		return result;

	brasero_job_set_current_action (BRASERO_JOB (self),
					BRASERO_BURN_ACTION_CHECKSUM,
					_("Creating image checksum"),
					FALSE);

	brasero_job_start_progress (BRASERO_JOB (self), FALSE);

	priv->ctx = brasero_md5_new ();
	result = brasero_md5_file (priv->ctx,
				   path,
				   &priv->md5,
				   -1,
				   error);
	g_free (path);
	brasero_md5_free (priv->ctx);
	priv->ctx = NULL;

	return result;
}

static BraseroBurnResult
brasero_md5sum_disc (BraseroMd5sum *self, GError **error)
{
	const gchar *device;
	BraseroTrack *track;
	BraseroBurnResult result;
	NautilusBurnDrive *drive;
	BraseroMd5sumPrivate *priv;

	priv = BRASERO_MD5SUM_PRIVATE (self);

	brasero_job_get_current_track (BRASERO_JOB (self), &track);
	drive = brasero_track_get_drive_source (track);

	/* we get the size of the image 
	 * NOTE: media was already checked in burn.c */
	priv->total = 0;
	result = brasero_track_get_disc_data_size (track, NULL, &priv->total);

	device = NCB_DRIVE_GET_DEVICE (drive);
	brasero_job_set_current_action (BRASERO_JOB (self),
					BRASERO_BURN_ACTION_CHECKSUM,
					_("Creating disc checksum"),
					FALSE);
	brasero_job_start_progress (BRASERO_JOB (self), FALSE);

	priv->ctx = brasero_md5_new ();
	result = brasero_md5_file (priv->ctx,
				   device,
				   &priv->md5,
				   priv->total,
				   error);
	brasero_md5_free (priv->ctx);
	priv->ctx = NULL;

	return result;
}

static gint
brasero_md5sum_get_line_num (BraseroMd5sum *self,
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
brasero_md5sum_disc_files (BraseroMd5sum *self, GError **error)
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
	BraseroMd5sumPrivate *priv;
	gchar filename [MAXPATHLEN + 1];
	BraseroBurnResult result = BRASERO_BURN_OK;

	priv = BRASERO_MD5SUM_PRIVATE (self);

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
	file_nb = brasero_md5sum_get_line_num (self, file, error);
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

struct _BraseroMd5sumThreadCtx {
	BraseroMd5sum *sum;
	BraseroBurnResult result;
	GError *error;
};
typedef struct _BraseroMd5sumThreadCtx BraseroMd5sumThreadCtx;

static gboolean
brasero_md5sum_end (gpointer data)
{
	BraseroMd5sum *self;
	BraseroTrack *track;
	BraseroTrackType input;
	BraseroJobAction action;
	BraseroBurnResult result;
	BraseroMd5sumPrivate *priv;
	BraseroMd5sumThreadCtx *ctx;
	gchar checksum [MD5_STRING_LEN + 1];

	ctx = data;
	self = ctx->sum;
	priv = BRASERO_MD5SUM_PRIVATE (self);

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
		excluded = brasero_track_get_data_excluded_source (track);

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
		brasero_track_set_data_source (track,
					       new_grafts,
					       g_slist_copy (excluded));

		brasero_track_set_checksum (track,
					    BRASERO_CHECKSUM_MD5_FILE,
					    graft->uri);

		brasero_job_add_track (BRASERO_JOB (self), track);
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
brasero_md5sum_destroy (gpointer data)
{
	BraseroMd5sumThreadCtx *ctx;

	ctx = data;
	if (ctx->error) {
		g_error_free (ctx->error);
		ctx->error = NULL;
	}

	g_free (ctx);
}

static gpointer
brasero_md5sum_thread (gpointer data)
{
	BraseroMd5sum *self;
	GError *error = NULL;
	BraseroJobAction action;
	BraseroMd5sumPrivate *priv;
	BraseroMd5sumThreadCtx *ctx;
	BraseroBurnResult result = BRASERO_BURN_NOT_SUPPORTED;

	self = BRASERO_MD5SUM (data);
	priv = BRASERO_MD5SUM_PRIVATE (self);

	/* check DISC types and add checksums for DATA and IMAGE-bin types */
	brasero_job_get_action (BRASERO_JOB (self), &action);

	if (action == BRASERO_JOB_ACTION_CHECKSUM) {
		BraseroChecksumType type;
		BraseroTrack *track;

		brasero_job_get_current_track (BRASERO_JOB (self), &track);
		type = brasero_track_get_checksum_type (track);
		if (type == BRASERO_CHECKSUM_MD5)
			result = brasero_md5sum_disc (self, &error);
		else if (type == BRASERO_CHECKSUM_MD5_FILE)
			result = brasero_md5sum_disc_files (self, &error);
		else
			result = BRASERO_BURN_ERR;
	}
	else if (action == BRASERO_JOB_ACTION_IMAGE) {
		BraseroTrackType type;

		brasero_job_get_input_type (BRASERO_JOB (self), &type);
		if (type.type == BRASERO_TRACK_TYPE_DATA)
			result = brasero_md5sum_grafts (self, &error);
		else if (type.type == BRASERO_TRACK_TYPE_IMAGE)
			result = brasero_md5sum_image (self, &error);
		else
			result = BRASERO_BURN_ERR;
	}

	if (result != BRASERO_BURN_CANCEL) {
		ctx = g_new0 (BraseroMd5sumThreadCtx, 1);
		ctx->sum = self;
		ctx->error = error;
		ctx->result = result;
		priv->end_id = g_idle_add_full (G_PRIORITY_HIGH_IDLE,
						brasero_md5sum_end,
						ctx,
						brasero_md5sum_destroy);
	}

	priv->thread = NULL;
	g_thread_exit (NULL);
	return NULL;
}

static BraseroBurnResult
brasero_md5sum_init_real (BraseroJob *self,
			  GError **error)
{
	BraseroJobAction action;

	brasero_job_get_action (self, &action);
	if (action == BRASERO_JOB_ACTION_SIZE) {
		/* we're not outputting anything so we don't have to set an 
		 * output size. On the other hand we'll set a progress */
		return BRASERO_BURN_NOT_RUNNING;
	}
	else if (action == BRASERO_JOB_ACTION_IMAGE) {
		BraseroTrackType output;
		GConfClient *client;

		/* See if we need to perform this operation */
		client = gconf_client_get_default ();
		if (gconf_client_get_bool (client, GCONF_DONT_ADD_MD5SUM_FILE_TO_DATA, NULL)) {
			g_object_unref (client);
			return BRASERO_BURN_NOT_RUNNING;
		}
		g_object_unref (client);

		brasero_job_get_output_type (self, &output);
		if (output.type == BRASERO_TRACK_TYPE_DATA) {
			BraseroTrack *track = NULL;
			GSList *grafts;

			/* see that a file with graft "/BRASERO_CHECKSUM_FILE"
			 * doesn't already exists (possible when doint several
			 * copies */
			brasero_job_get_current_track (self, &track);
			grafts = brasero_track_get_data_grafts_source (track);
			for (; grafts; grafts = grafts->next) {
				BraseroGraftPt *graft;

				graft = grafts->data;
				if (graft->path 
				&& !strcmp (graft->path, "/"BRASERO_CHECKSUM_FILE))
					return BRASERO_BURN_NOT_RUNNING;
			}
		}
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_md5sum_start (BraseroJob *job,
		      GError **error)
{
	BraseroMd5sumPrivate *priv;

	priv = BRASERO_MD5SUM_PRIVATE (job);

	/* we start a thread for the exploration of the graft points */
	priv->thread = g_thread_create (brasero_md5sum_thread,
					BRASERO_MD5SUM (job),
					TRUE,
					error);

	if (!priv->thread)
		return BRASERO_BURN_ERR;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_md5sum_clock_tick (BraseroJob *job)
{
	BraseroMd5sumPrivate *priv;

	priv = BRASERO_MD5SUM_PRIVATE (job);
	if (!priv->ctx)
		return BRASERO_BURN_OK;

	if (priv->total) {
		gint64 written = 0;

		written = brasero_md5_get_written (priv->ctx);
		brasero_job_set_progress (job,
					  (gdouble) written /
					  (gdouble) priv->total);
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_md5sum_stop (BraseroJob *job,
		     GError **error)
{
	BraseroMd5sumPrivate *priv;

	priv = BRASERO_MD5SUM_PRIVATE (job);

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
brasero_md5sum_init (BraseroMd5sum *obj)
{ }

static void
brasero_md5sum_finalize (GObject *object)
{
	BraseroMd5sumPrivate *priv;
	
	priv = BRASERO_MD5SUM_PRIVATE (object);

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
brasero_md5sum_class_init (BraseroMd5sumClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroJobClass *job_class = BRASERO_JOB_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroMd5sumPrivate));

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_md5sum_finalize;

	job_class->init = brasero_md5sum_init_real;
	job_class->start = brasero_md5sum_start;
	job_class->stop = brasero_md5sum_stop;
	job_class->clock_tick = brasero_md5sum_clock_tick;
}

static BraseroBurnResult
brasero_md5sum_export_caps (BraseroPlugin *plugin, gchar **error)
{
	GSList *input;
	BraseroPluginConfOption *option;

	/* FIXME: all this could be fine-tuned a little bit more thanks to some
	 * GConf key specific to this plugin (like for cdrecord) to allow the 
	 * user to tell whether he only wants us to provide checksum for DATA
	 * or BIN images */
	brasero_plugin_define (plugin,
			       "md5sum",
			       _("md5 checksum allows to verify disc/data integrity"),
			       "Philippe Rouquier",
			       0);

	input = brasero_caps_image_new (BRASERO_PLUGIN_IO_ACCEPT_FILE|
					BRASERO_PLUGIN_IO_ACCEPT_PIPE,
					BRASERO_IMAGE_FORMAT_BIN);
	brasero_plugin_process_caps (plugin, input);
	g_slist_free (input);

	input = brasero_caps_data_new (BRASERO_IMAGE_FS_ANY);
	brasero_plugin_process_caps (plugin, input);
	g_slist_free (input);

	brasero_plugin_set_process_flags (plugin,
					  BRASERO_PLUGIN_RUN_FIRST|
					  BRASERO_PLUGIN_RUN_LAST);

	/* FIXME: we might want to add a DISC process as well when using cdrdao for example */
	input = brasero_caps_disc_new (BRASERO_MEDIUM_CD|
				       BRASERO_MEDIUM_DVD|
				       BRASERO_MEDIUM_PLUS|
				       BRASERO_MEDIUM_RESTRICTED|
				       BRASERO_MEDIUM_SEQUENTIAL|
				       BRASERO_MEDIUM_ROM|
				       BRASERO_MEDIUM_WRITABLE|
				       BRASERO_MEDIUM_REWRITABLE|
				       BRASERO_MEDIUM_CLOSED|
				       BRASERO_MEDIUM_APPENDABLE|
				       BRASERO_MEDIUM_HAS_DATA);
	brasero_plugin_check_caps (plugin,
				   BRASERO_CHECKSUM_MD5,
				   input);
	g_slist_free (input);

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

	/* configure options */
	option = brasero_plugin_conf_option_new (GCONF_DONT_ADD_MD5SUM_FILE_TO_DATA,
						 _("don't add a md5sum file to all data projects"),
						 BRASERO_PLUGIN_OPTION_BOOL);

	brasero_plugin_add_conf_option (plugin, option);

	return BRASERO_BURN_OK;
}
