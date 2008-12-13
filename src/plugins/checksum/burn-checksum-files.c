/***************************************************************************
 *            burn-sum.c
 *
 *  ven aoû  4 19:46:34 2006
 *  Copyright  2006  Rouquier Philippe
 *  brasero-app@wanadoo.fr
 ***************************************************************************/

/*
 *  Brasero is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Brasero is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
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

#include "scsi-device.h"
#include "burn-plugin.h"
#include "burn-job.h"
#include "burn-checksum-files.h"
#include "burn-volume.h"
#include "burn-drive.h"
#include "burn-volume-obj.h"
#include "burn-volume-read.h"

BRASERO_PLUGIN_BOILERPLATE (BraseroChecksumFiles, brasero_checksum_files, BRASERO_TYPE_JOB, BraseroJob);

struct _BraseroChecksumFilesPrivate {
	/* the path to read from when we check */
	gchar *sums_path;
	BraseroChecksumType checksum_type;

	gint64 file_num;

	/* the FILE to write to when we generate */
	FILE *file;

	/* this is for the thread and the end of it */
	GThread *thread;
	gint end_id;

	guint cancel;
};
typedef struct _BraseroChecksumFilesPrivate BraseroChecksumFilesPrivate;

#define BRASERO_CHECKSUM_FILES_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_CHECKSUM_FILES, BraseroChecksumFilesPrivate))

#define BLOCK_SIZE			64
#define GCONF_KEY_CHECKSUM_TYPE		"/apps/brasero/config/checksum_files"

static BraseroJobClass *parent_class = NULL;

static BraseroBurnResult
brasero_checksum_files_get_file_checksum (BraseroChecksumFiles *self,
					  GChecksumType type,
					  const gchar *path,
					  gchar **checksum_string,
					  GError **error)
{
	BraseroChecksumFilesPrivate *priv;
	guchar buffer [BLOCK_SIZE];
	GChecksum *checksum;
	gint read_bytes;
	FILE *file;

	priv = BRASERO_CHECKSUM_FILES_PRIVATE (self);

	file = fopen (path, "r");
	if (!file) {
                int errsv;
		gchar *name = NULL;

		/* If the file doesn't exist carry on with next */
		if (errno == ENOENT)
			return BRASERO_BURN_RETRY;

		name = g_path_get_basename (path);

                errsv = errno;
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("File \"%s\" could not be opened (%s)"),
			     name,
			     g_strerror (errsv));
		g_free (name);

		return BRASERO_BURN_ERR;
	}

	checksum = g_checksum_new (type);

	read_bytes = fread (buffer, 1, BLOCK_SIZE, file);
	g_checksum_update (checksum, buffer, read_bytes);

	while (read_bytes == BLOCK_SIZE) {
		if (priv->cancel)
			return BRASERO_BURN_CANCEL;

		read_bytes = fread (buffer, 1, BLOCK_SIZE, file);
		g_checksum_update (checksum, buffer, read_bytes);
	}

	*checksum_string = g_strdup (g_checksum_get_string (checksum));
	g_checksum_free (checksum);
	fclose (file);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_checksum_files_add_file_checksum (BraseroChecksumFiles *self,
					  const gchar *path,
					  GChecksumType checksum_type,
					  const gchar *graft_path,
					  GError **error)
{
	BraseroBurnResult result = BRASERO_BURN_OK;
	BraseroChecksumFilesPrivate *priv;
	gchar *checksum_string = NULL;
	gint written;

	priv = BRASERO_CHECKSUM_FILES_PRIVATE (self);

	/* write to the file */
	result = brasero_checksum_files_get_file_checksum (self,
							   checksum_type,
							   path,
							   &checksum_string,
							   error);
	if (result != BRASERO_BURN_OK)
		return BRASERO_BURN_ERR;

	written = fwrite (checksum_string,
			  strlen (checksum_string),
			  1,
			  priv->file);
	g_free (checksum_string);

	if (written != 1) {
                int errsv = errno;
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("Data could not be written (%s)"),
			     g_strerror (errsv));
			
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
                int errsv = errno;
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("Data could not be written (%s)"),
			     g_strerror (errsv));

		return BRASERO_BURN_ERR;
	}

	written = fwrite ("\n",
			  1,
			  1,
			  priv->file);

	return result;
}

static BraseroBurnResult
brasero_checksum_files_explore_directory (BraseroChecksumFiles *self,
					  GChecksumType checksum_type,
					  gint64 file_nb,
					  const gchar *directory,
					  const gchar *disc_path,
					  GHashTable *excludedH,
					  GError **error)
{
	BraseroBurnResult result = BRASERO_BURN_OK;
	BraseroChecksumFilesPrivate *priv;
	const gchar *name;
	GDir *dir;

	priv = BRASERO_CHECKSUM_FILES_PRIVATE (self);

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
			result = brasero_checksum_files_explore_directory (self,
									   checksum_type,
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

		/* Only checksum regular files and avoid fifos, ... */
		if (!g_file_test (path, G_FILE_TEST_IS_REGULAR)) {
			g_free (path);
			g_free (graft_path);
			continue;
		}

		result = brasero_checksum_files_add_file_checksum (self,
								   path,
								   checksum_type,
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

static BraseroBurnResult
brasero_checksum_file_process_former_line (BraseroChecksumFiles *self,
					   BraseroTrack *track,
					   const gchar *line,
					   GError **error)
{
	guint i;
	gchar *path;
	GSList *grafts;
	guint written_bytes;
	BraseroChecksumFilesPrivate *priv;

	priv = BRASERO_CHECKSUM_FILES_PRIVATE (self);

	/* first skip the checksum string */
	i = 0;
	while (!isspace (line [i])) i ++;

	/* skip white spaces */
	while (isspace (line [i])) i ++;

	/* get the path string */
	path = g_strdup (line + i);

	for (grafts = brasero_track_get_data_grafts_source (track); grafts; grafts = grafts->next) {
		BraseroGraftPt *graft;
		guint len;

		/* NOTE: graft->path + 1 is because in the checksum files on the 
		 * disc there is not first "/" so if we want to compare ... */
		graft = grafts->data;
		if (!strcmp (graft->path + 1, path)) {
			g_free (path);
			return BRASERO_BURN_OK;
		}

		len = strlen (graft->path + 1);
		if (!strncmp (graft->path + 1, path, len)
		&&   path [len] == G_DIR_SEPARATOR) {
			g_free (path);
			return BRASERO_BURN_OK;
		}
	}

	g_free (path);

	/* write the whole line in the new file */
	written_bytes = fwrite (line, 1, strlen (line), priv->file);
	if (written_bytes != strlen (line)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     "%s",
			     g_strerror (errno));
		return BRASERO_BURN_ERR;
	}

	if (!fwrite ("\n", 1, 1, priv->file)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     "%s",
			     g_strerror (errno));
		return BRASERO_BURN_ERR;
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_checksum_files_merge_with_former_session (BraseroChecksumFiles *self,
						  GError **error)
{
	BraseroBurnFlag flags = BRASERO_BURN_FLAG_NONE;
	BraseroChecksumFilesPrivate *priv;
	BraseroDeviceHandle *dev_handle;
	BraseroVolFileHandle *handle;
	BraseroBurnResult result;
	BraseroVolFile *file;
	BraseroTrack *track;
	gchar buffer [2048];
	BraseroVolSrc *vol;
	gint64 start_block;
	gchar *device;

	priv = BRASERO_CHECKSUM_FILES_PRIVATE (self);

	/* Now we need to know if we're merging. If so, we need to merge the
	 * former checksum file with the new ones. */
	brasero_job_get_flags (BRASERO_JOB (self), &flags);
	if (!(flags & BRASERO_BURN_FLAG_MERGE))
		return BRASERO_BURN_OK;

	/* get the former file */
	result = brasero_job_get_last_session_address (BRASERO_JOB (self), &start_block);
	if (result != BRASERO_BURN_OK)
		return result;

	/* try every file and make sure they are of the same type */
	brasero_job_get_device (BRASERO_JOB (self), &device);
	dev_handle = brasero_device_handle_open (device, FALSE, NULL);
	g_free (device);

	vol = brasero_volume_source_open_device_handle (dev_handle, error);
	file = brasero_volume_get_file (vol,
					"/"BRASERO_MD5_FILE,
					start_block,
					NULL);

	if (!file) {
		file = brasero_volume_get_file (vol,
						"/"BRASERO_SHA1_FILE,
						start_block,
						NULL);
		if (!file) {
			file = brasero_volume_get_file (vol,
							"/"BRASERO_SHA256_FILE,
							start_block,
							NULL);
			if (!file) {
				brasero_volume_source_close (vol);
				BRASERO_JOB_LOG (self, "no checksum file found");
				return BRASERO_BURN_OK;
			}
			else if (priv->checksum_type != BRASERO_CHECKSUM_SHA256_FILE) {
				brasero_volume_source_close (vol);
				BRASERO_JOB_LOG (self, "checksum type mismatch (%i against %i)",
						 priv->checksum_type,
						 BRASERO_CHECKSUM_SHA256_FILE);
				return BRASERO_BURN_OK;
			}
		}
		else if (priv->checksum_type != BRASERO_CHECKSUM_SHA1_FILE) {
			BRASERO_JOB_LOG (self, "checksum type mismatch (%i against %i)",
					 priv->checksum_type,
					 BRASERO_CHECKSUM_SHA1_FILE);
			brasero_volume_source_close (vol);
			return BRASERO_BURN_OK;
		}
	}
	else if (priv->checksum_type != BRASERO_CHECKSUM_MD5_FILE) {
		brasero_volume_source_close (vol);
		BRASERO_JOB_LOG (self, "checksum type mismatch (%i against %i)",
				 priv->checksum_type,
				 BRASERO_CHECKSUM_MD5_FILE);
		return BRASERO_BURN_OK;
	}

	BRASERO_JOB_LOG (self, "Found file %s", file);
	handle = brasero_volume_file_open (vol, file);
	brasero_volume_source_close (vol);

	if (!handle) {
		BRASERO_JOB_LOG (self, "Failed to open file");
		brasero_device_handle_close (dev_handle);
		brasero_volume_file_free (file);
		return BRASERO_BURN_ERR;
	}

	brasero_job_get_current_track (BRASERO_JOB (self), &track);

	/* Now check the files that have been replaced; to do that check the 
	 * paths of the new image whenever a read path from former file is a
	 * child of one of the new paths, then it must not be included. */
	result = brasero_volume_file_read_line (handle, buffer, sizeof (buffer));
	while (result == BRASERO_BURN_RETRY) {
		if (priv->cancel) {
			brasero_volume_file_close (handle);
			brasero_volume_file_free (file);
			brasero_device_handle_close (dev_handle);
			return BRASERO_BURN_CANCEL;
		}

		result = brasero_checksum_file_process_former_line (self,
								    track,
								    buffer,
								    error);
		if (result != BRASERO_BURN_OK) {
			brasero_volume_file_close (handle);
			brasero_volume_file_free (file);
			brasero_device_handle_close (dev_handle);
			return result;
		}

		result = brasero_volume_file_read_line (handle, buffer, sizeof (buffer));
	}

	result = brasero_checksum_file_process_former_line (self, track, buffer, error);
	brasero_volume_file_close (handle);
	brasero_volume_file_free (file);
	brasero_device_handle_close (dev_handle);

	return result;
}

static BraseroBurnResult
brasero_checksum_files_create_checksum (BraseroChecksumFiles *self,
					GError **error)
{
	GSList *iter;
	gint64 file_nb;
	BraseroTrack *track;
	GConfClient *client;
	GHashTable *excludedH;
	GChecksumType gchecksum_type;
	BraseroChecksumFilesPrivate *priv;
	BraseroChecksumType checksum_type;
	BraseroBurnResult result = BRASERO_BURN_OK;

	priv = BRASERO_CHECKSUM_FILES_PRIVATE (self);

	/* get the checksum type */
	client = gconf_client_get_default ();
	checksum_type = gconf_client_get_int (client, GCONF_KEY_CHECKSUM_TYPE, NULL);
	g_object_unref (client);

	if (checksum_type == BRASERO_CHECKSUM_NONE)
		gchecksum_type = G_CHECKSUM_MD5;
	else if (checksum_type & BRASERO_CHECKSUM_MD5_FILE)
		gchecksum_type = G_CHECKSUM_MD5;
	else if (checksum_type & BRASERO_CHECKSUM_SHA1_FILE)
		gchecksum_type = G_CHECKSUM_SHA1;
	else if (checksum_type & BRASERO_CHECKSUM_SHA256_FILE)
		gchecksum_type = G_CHECKSUM_SHA256;
	else
		gchecksum_type = G_CHECKSUM_MD5;

	/* opens a file for the sums */
	switch (gchecksum_type) {
	case G_CHECKSUM_MD5:
		priv->checksum_type = BRASERO_CHECKSUM_MD5_FILE;
		result = brasero_job_get_tmp_file (BRASERO_JOB (self),
						   ".md5",
						   &priv->sums_path,
						   error);
		break;
	case G_CHECKSUM_SHA1:
		priv->checksum_type = BRASERO_CHECKSUM_SHA1_FILE;
		result = brasero_job_get_tmp_file (BRASERO_JOB (self),
						   ".sha1",
						   &priv->sums_path,
						   error);
		break;
	case G_CHECKSUM_SHA256:
		priv->checksum_type = BRASERO_CHECKSUM_SHA256_FILE;
		result = brasero_job_get_tmp_file (BRASERO_JOB (self),
						   ".sha256",
						   &priv->sums_path,
						   error);
		break;
	default:
		result = BRASERO_BURN_CANCEL;
		break;
	}

	if (result != BRASERO_BURN_OK || !priv->sums_path)
		return result;

	priv->file = fopen (priv->sums_path, "w");
	if (!priv->file) {
                int errsv = errno;

		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("File \"%s\" could not be opened (%s)"),
			     priv->sums_path,
			     g_strerror (errsv));

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
		path = g_filename_from_uri (uri, NULL, NULL);

		if (path)
			g_hash_table_insert (excludedH, path, path);
	}

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
		gchar *path;

		if (priv->cancel) {
			result = BRASERO_BURN_CANCEL;
			break;
		}

		graft = iter->data;
		if (!graft->uri)
			continue;

		/* get the current and future paths */
		/* FIXME: graft->uri can be path or URIs ... This should be
		 * fixed for graft points. */
		if (!graft->uri)
			path = NULL;
		else if (graft->uri [0] == '/')
			path = g_strdup (graft->uri);
		else if (g_str_has_prefix (graft->uri, "file://"))
			path = g_filename_from_uri (graft->uri, NULL, NULL);
		else
			path = NULL;

		graft_path = graft->path;

		if (g_file_test (path, G_FILE_TEST_IS_DIR))
			result = brasero_checksum_files_explore_directory (self,
									   gchecksum_type,
									   file_nb,
									   path,
									   graft_path,
									   excludedH,
									   error);
		else {
			result = brasero_checksum_files_add_file_checksum (self,
									   path,
									   gchecksum_type,
									   graft_path,
									   error);
			priv->file_num ++;
			brasero_job_set_progress (BRASERO_JOB (self),
						  (gdouble) priv->file_num /
						  (gdouble) file_nb);
		}

		g_free (path);
		if (result != BRASERO_BURN_OK)
			break;
	}

	g_hash_table_destroy (excludedH);

	if (result == BRASERO_BURN_OK)
		result = brasero_checksum_files_merge_with_former_session (self, error);

	/* that's finished we close the file */
	fclose (priv->file);
	priv->file = NULL;

	return result;
}

static gint
brasero_checksum_files_get_line_num (BraseroChecksumFiles *self,
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
			     "%s",
			     g_strerror (errno));
		return -1;
	}

	rewind (file);
	return num;
}

static BraseroBurnResult
brasero_checksum_files_check_files (BraseroChecksumFiles *self,
				    BraseroChecksumType checksum_type,
				    GError **error)
{
	gchar *root;
	gchar *path;
	gint root_len;
	guint file_nb;
	guint file_num;
	FILE *file = NULL;
	const gchar *name;
	gint checksum_len;
	BraseroTrack *track;
	GValue *value = NULL;
	BraseroMedium *medium;
	GChecksumType gchecksum_type;
	GArray *wrong_checksums = NULL;
	gchar filename [MAXPATHLEN + 1];
	BraseroChecksumFilesPrivate *priv;
	BraseroBurnResult result = BRASERO_BURN_OK;

	priv = BRASERO_CHECKSUM_FILES_PRIVATE (self);

	brasero_job_get_current_track (BRASERO_JOB (self), &track);
	medium = brasero_track_get_medium_source (track);
	root = brasero_volume_get_mount_point (BRASERO_VOLUME (medium), FALSE);
	if (!root)
		return BRASERO_BURN_ERR;

	root_len = strlen (root);
	memcpy (filename, root, root_len);
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
			     "%s",
			     g_strerror (errno));
		return BRASERO_BURN_ERR;
	}

	/* we need to get the number of files at this time and rewind */
	file_nb = brasero_checksum_files_get_line_num (self, file, error);
	if (file_nb == 0) {
		fclose (file);
		return BRASERO_BURN_OK;
	}

	if (file_nb < 0) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     "%s",
			     g_strerror (errno));
		fclose (file);
		return BRASERO_BURN_ERR;
	}

	file_num = 0;
	brasero_job_set_current_action (BRASERO_JOB (self),
				        BRASERO_BURN_ACTION_CHECKSUM,
					_("Checking file integrity"),
					TRUE);
	brasero_job_start_progress (BRASERO_JOB (self), FALSE);

	/* Get the checksum type */
	switch (checksum_type) {
	case BRASERO_CHECKSUM_MD5_FILE:
		gchecksum_type = G_CHECKSUM_MD5;
		break;
	case BRASERO_CHECKSUM_SHA1_FILE:
		gchecksum_type = G_CHECKSUM_SHA1;
		break;
	case BRASERO_CHECKSUM_SHA256_FILE:
		gchecksum_type = G_CHECKSUM_SHA256;
		break;
	default:
		gchecksum_type = G_CHECKSUM_MD5;
		break;
	}

	checksum_len = g_checksum_type_get_length (gchecksum_type) * 2;
	while (1) {
		gchar checksum_file [512];
		gchar *checksum_real;
		gint i;
		int c;

		if (priv->cancel)
			break;

		/* first read the checksum string */
		if (fread (checksum_file, 1, checksum_len, file) != checksum_len) {
			if (!feof (file))
				g_set_error (error,
					     BRASERO_BURN_ERROR,
					     BRASERO_BURN_ERROR_GENERAL,
					     "%s",
					     g_strerror (errno));
			break;
		}
		checksum_file [checksum_len] = '\0';

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
					     "%s",
					     g_strerror (errno));
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
					     "%s",
					     g_strerror (errno));
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
		checksum_real = NULL;

		/* we certainly don't want to checksum anything but regular file */
		if (!g_file_test (filename, G_FILE_TEST_IS_REGULAR))
			continue;

		result = brasero_checksum_files_get_file_checksum (self,
								   gchecksum_type,
								   filename,
								   &checksum_real,
								   error);
		if (result == BRASERO_BURN_RETRY)
			continue;

		if (result != BRASERO_BURN_OK)
			break;

		file_num++;
		brasero_job_set_progress (BRASERO_JOB (self),
					  (gdouble) file_num /
					  (gdouble) file_nb);
		BRASERO_JOB_LOG (self,
				 "comparing checksums for file %s : %s (from md5 file) / %s (current)",
				 filename, checksum_file, checksum_real);

		if (strcmp (checksum_file, checksum_real)) {
			gchar *string;
			if (!wrong_checksums)
				wrong_checksums = g_array_new (TRUE,
							       TRUE, 
							       sizeof (gchar *));

			string = g_strdup (filename);
			wrong_checksums = g_array_append_val (wrong_checksums,
							      string);
		}

		g_free (checksum_real);
		if (priv->cancel)
			break;
	}

end:
	if (file)
		fclose (file);

	if (result != BRASERO_BURN_OK)
		return result;

	if (!wrong_checksums)
		return BRASERO_BURN_OK;

	/* add the tag */
	value = g_new0 (GValue, 1);
	g_value_init (value, G_TYPE_STRV);
	g_value_take_boxed (value, wrong_checksums->data);
	g_array_free (wrong_checksums, FALSE);

	brasero_track_tag_add (track,
			       BRASERO_TRACK_MEDIUM_WRONG_CHECKSUM_TAG,
			       value);

	g_set_error (error,
		     BRASERO_BURN_ERROR,
		     BRASERO_BURN_ERROR_BAD_CHECKSUM,
		     _("Some files may be corrupted on the disc"));

	return BRASERO_BURN_ERR;
}

struct _BraseroChecksumFilesThreadCtx {
	BraseroChecksumFiles *sum;
	BraseroBurnResult result;
	GError *error;
};
typedef struct _BraseroChecksumFilesThreadCtx BraseroChecksumFilesThreadCtx;

static gboolean
brasero_checksum_files_end (gpointer data)
{
	BraseroTrack *track;
	BraseroTrackType input;
	BraseroChecksumFiles *self;
	BraseroJobAction action;
	BraseroChecksumFilesPrivate *priv;
	BraseroChecksumFilesThreadCtx *ctx;

	ctx = data;
	self = ctx->sum;
	priv = BRASERO_CHECKSUM_FILES_PRIVATE (self);

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
		/* everything was done in thread */
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

		for (; grafts; grafts = grafts->next) {
			graft = grafts->data;
			graft = brasero_graft_point_copy (graft);
			new_grafts = g_slist_prepend (new_grafts, graft);
		}

		graft = g_new0 (BraseroGraftPt, 1);
		graft->uri = g_strconcat ("file://", priv->sums_path, NULL);
		switch (priv->checksum_type) {
		case BRASERO_CHECKSUM_SHA1_FILE:
			graft->path = g_strdup ("/"BRASERO_SHA1_FILE);
			break;
		case BRASERO_CHECKSUM_SHA256_FILE:
			graft->path = g_strdup ("/"BRASERO_SHA256_FILE);
			break;
		case BRASERO_CHECKSUM_MD5_FILE:
		default:
			graft->path = g_strdup ("/"BRASERO_MD5_FILE);
			break;
		}

		BRASERO_JOB_LOG (self,
				 "Adding graft for checksum file %s %s",
				 graft->path,
				 graft->uri);

		new_grafts = g_slist_prepend (new_grafts, graft);
		excluded = brasero_track_get_data_excluded_source (track, TRUE);

		track = brasero_track_new (BRASERO_TRACK_TYPE_DATA);
		brasero_track_add_data_fs (track, type.subtype.fs_type);
		brasero_track_set_data_source (track, new_grafts, excluded);
		brasero_track_set_checksum (track,
					    priv->checksum_type,
					    graft->uri);

		brasero_job_add_track (BRASERO_JOB (self), track);

		/* It's good practice to unref the track afterwards as we don't
		 * need it anymore. BraseroTaskCtx refs it. */
		brasero_track_unref (track);
		
		brasero_job_finished_track (BRASERO_JOB (self));
		return FALSE;
	}
	else
		goto error;

	return FALSE;

error:
{
	GError *error = NULL;

	error = g_error_new (BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_BAD_CHECKSUM,
			     _("Some files may be corrupted on the disc"));
	brasero_job_error (BRASERO_JOB (self), error);
	return FALSE;
}
}

static void
brasero_checksum_files_destroy (gpointer data)
{
	BraseroChecksumFilesThreadCtx *ctx;

	ctx = data;
	if (ctx->error) {
		g_error_free (ctx->error);
		ctx->error = NULL;
	}

	g_free (ctx);
}

static gpointer
brasero_checksum_files_thread (gpointer data)
{
	BraseroChecksumFiles *self;
	GError *error = NULL;
	BraseroJobAction action;
	BraseroChecksumFilesPrivate *priv;
	BraseroChecksumFilesThreadCtx *ctx;
	BraseroBurnResult result = BRASERO_BURN_NOT_SUPPORTED;

	self = BRASERO_CHECKSUM_FILES (data);
	priv = BRASERO_CHECKSUM_FILES_PRIVATE (self);

	/* check DISC types and add checksums for DATA and IMAGE-bin types */
	brasero_job_get_action (BRASERO_JOB (self), &action);

	if (action == BRASERO_JOB_ACTION_CHECKSUM) {
		BraseroChecksumType type;
		BraseroTrack *track;

		brasero_job_get_current_track (BRASERO_JOB (self), &track);
		type = brasero_track_get_checksum_type (track);
		if (type & (BRASERO_CHECKSUM_MD5_FILE|BRASERO_CHECKSUM_SHA1_FILE|BRASERO_CHECKSUM_SHA256))
			result = brasero_checksum_files_check_files (self, type, &error);
		else
			result = BRASERO_BURN_ERR;
	}
	else if (action == BRASERO_JOB_ACTION_IMAGE) {
		BraseroTrackType type;

		brasero_job_get_input_type (BRASERO_JOB (self), &type);
		if (type.type == BRASERO_TRACK_TYPE_DATA)
			result = brasero_checksum_files_create_checksum (self, &error);
		else
			result = BRASERO_BURN_ERR;
	}

	if (result != BRASERO_BURN_CANCEL) {
		ctx = g_new0 (BraseroChecksumFilesThreadCtx, 1);
		ctx->sum = self;
		ctx->error = error;
		ctx->result = result;
		priv->end_id = g_idle_add_full (G_PRIORITY_HIGH_IDLE,
						brasero_checksum_files_end,
						ctx,
						brasero_checksum_files_destroy);
	}

	priv->thread = NULL;
	g_thread_exit (NULL);
	return NULL;
}

static BraseroBurnResult
brasero_checksum_files_start (BraseroJob *job,
			      GError **error)
{
	BraseroChecksumFilesPrivate *priv;
	BraseroJobAction action;

	brasero_job_get_action (job, &action);
	if (action == BRASERO_JOB_ACTION_SIZE) {
		/* say we won't write to disc */
		brasero_job_set_output_size_for_current_track (job, 0, 0);
		return BRASERO_BURN_NOT_RUNNING;
	}

	/* we start a thread for the exploration of the graft points */
	priv = BRASERO_CHECKSUM_FILES_PRIVATE (job);
	priv->thread = g_thread_create (brasero_checksum_files_thread,
					BRASERO_CHECKSUM_FILES (job),
					TRUE,
					error);

	if (!priv->thread)
		return BRASERO_BURN_ERR;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_checksum_files_activate (BraseroJob *job,
				 GError **error)
{
	GSList *grafts;
	BraseroTrackType output;
	BraseroTrack *track = NULL;

	brasero_job_get_output_type (job, &output);
	if (output.type != BRASERO_TRACK_TYPE_DATA)
		return BRASERO_BURN_OK;

	/* see that a file with graft "/BRASERO_CHECKSUM_FILE" doesn't already
	 * exists (possible when doing several copies) or when a simulation 
	 * already took place before. */
	brasero_job_get_current_track (job, &track);
	grafts = brasero_track_get_data_grafts_source (track);
	for (; grafts; grafts = grafts->next) {
		BraseroGraftPt *graft;

		graft = grafts->data;
		if (graft->path) {
			if (!strcmp (graft->path, "/"BRASERO_MD5_FILE))
				return BRASERO_BURN_NOT_RUNNING;
			if (!strcmp (graft->path, "/"BRASERO_SHA1_FILE))
				return BRASERO_BURN_NOT_RUNNING;
			if (!strcmp (graft->path, "/"BRASERO_SHA256_FILE))
				return BRASERO_BURN_NOT_RUNNING;
		}
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_checksum_files_clock_tick (BraseroJob *job)
{
	BraseroChecksumFilesPrivate *priv;

	priv = BRASERO_CHECKSUM_FILES_PRIVATE (job);

	/* we'll need that function later. For the moment, when generating a
	 * file we can't know how many files there are. Just when checking it */

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_checksum_files_stop (BraseroJob *job,
			     GError **error)
{
	BraseroChecksumFilesPrivate *priv;

	priv = BRASERO_CHECKSUM_FILES_PRIVATE (job);

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
brasero_checksum_files_init (BraseroChecksumFiles *obj)
{ }

static void
brasero_checksum_files_finalize (GObject *object)
{
	BraseroChecksumFilesPrivate *priv;
	
	priv = BRASERO_CHECKSUM_FILES_PRIVATE (object);

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
brasero_checksum_files_class_init (BraseroChecksumFilesClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroJobClass *job_class = BRASERO_JOB_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroChecksumFilesPrivate));

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_checksum_files_finalize;

	job_class->activate = brasero_checksum_files_activate;
	job_class->start = brasero_checksum_files_start;
	job_class->stop = brasero_checksum_files_stop;
	job_class->clock_tick = brasero_checksum_files_clock_tick;
}

static BraseroBurnResult
brasero_checksum_files_export_caps (BraseroPlugin *plugin, gchar **error)
{
	GSList *input;
	BraseroPluginConfOption *checksum_type;

	brasero_plugin_define (plugin,
			       "File checksum",
			       _("Allows to check file integrities on a disc"),
			       "Philippe Rouquier",
			       0);

	/* we can only generate a file for DATA input */
	input = brasero_caps_data_new (BRASERO_IMAGE_FS_ANY);
	brasero_plugin_process_caps (plugin, input);
	g_slist_free (input);

	/* we can run on initial track or later for whatever a DATA track */
	brasero_plugin_set_process_flags (plugin,
					  BRASERO_PLUGIN_RUN_PREPROCESSING|
					  BRASERO_PLUGIN_RUN_BEFORE_TARGET);

	/* For discs, we can only check each files on a disc against an md5sum 
	 * file (provided we managed to mount the disc).
	 * NOTE: we can't generate md5 from discs anymore. There are too many
	 * problems reading straight from the disc dev. So we use readcd or 
	 * equivalent instead */
	input = brasero_caps_disc_new (BRASERO_MEDIUM_CD|
				       BRASERO_MEDIUM_DVD|
				       BRASERO_MEDIUM_DUAL_L|
				       BRASERO_MEDIUM_PLUS|
				       BRASERO_MEDIUM_RESTRICTED|
				       BRASERO_MEDIUM_SEQUENTIAL|
				       BRASERO_MEDIUM_WRITABLE|
				       BRASERO_MEDIUM_REWRITABLE|
				       BRASERO_MEDIUM_CLOSED|
				       BRASERO_MEDIUM_APPENDABLE|
				       BRASERO_MEDIUM_HAS_DATA);
	brasero_plugin_check_caps (plugin,
				   BRASERO_CHECKSUM_MD5_FILE|
				   BRASERO_CHECKSUM_SHA1_FILE|
				   BRASERO_CHECKSUM_SHA256_FILE,
				   input);
	g_slist_free (input);

	/* add some configure options */
	checksum_type = brasero_plugin_conf_option_new (GCONF_KEY_CHECKSUM_TYPE,
							_("Hashing algorithm to be used:"),
							BRASERO_PLUGIN_OPTION_CHOICE);
	brasero_plugin_conf_option_choice_add (checksum_type,
					       _("MD5"), BRASERO_CHECKSUM_MD5_FILE);
	brasero_plugin_conf_option_choice_add (checksum_type,
					       _("SHA1"), BRASERO_CHECKSUM_SHA1_FILE);
	brasero_plugin_conf_option_choice_add (checksum_type,
					       _("SHA256"), BRASERO_CHECKSUM_SHA256_FILE);

	brasero_plugin_add_conf_option (plugin, checksum_type);

	return BRASERO_BURN_OK;
}
