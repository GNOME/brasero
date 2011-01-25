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

#include "scsi-device.h"
#include "brasero-plugin-registration.h"
#include "burn-job.h"

#include "brasero-tags.h"
#include "brasero-track-data.h"
#include "brasero-track-disc.h"

#include "burn-volume.h"
#include "brasero-drive.h"
#include "brasero-volume.h"

#include "burn-volume-read.h"


#define BRASERO_TYPE_CHECKSUM_FILES		(brasero_checksum_files_get_type ())
#define BRASERO_CHECKSUM_FILES(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_CHECKSUM_FILES, BraseroChecksumFiles))
#define BRASERO_CHECKSUM_FILES_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_CHECKSUM_FILES, BraseroChecksumFilesClass))
#define BRASERO_IS_CHECKSUM_FILES(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_CHECKSUM_FILES))
#define BRASERO_IS_CHECKSUM_FILES_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_CHECKSUM_FILES))
#define BRASERO_CHECKSUM_FILES_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_CHECKSUM_FILES, BraseroChecksumFilesClass))

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
	GMutex *mutex;
	GCond *cond;
	gint end_id;

	guint cancel;
};
typedef struct _BraseroChecksumFilesPrivate BraseroChecksumFilesPrivate;

#define BRASERO_CHECKSUM_FILES_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_CHECKSUM_FILES, BraseroChecksumFilesPrivate))

#define BLOCK_SIZE			64

#define BRASERO_SCHEMA_CONFIG		"org.gnome.brasero.config"
#define BRASERO_PROPS_CHECKSUM_FILES	"checksum-files"

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
		if (priv->cancel) {
			fclose (file);
			g_checksum_free (checksum);
			return BRASERO_BURN_CANCEL;
		}

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

	for (grafts = brasero_track_data_get_grafts (BRASERO_TRACK_DATA (track)); grafts; grafts = grafts->next) {
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
	BraseroDrive *burner;
	BraseroMedium *medium;
	BraseroVolFile *file;
	BraseroTrack *track;
	gchar buffer [2048];
	BraseroVolSrc *vol;
	goffset start_block;
	const gchar *device;

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
	medium = NULL;
	brasero_job_get_medium (BRASERO_JOB (self), &medium);
	burner = brasero_medium_get_drive (medium);
	device = brasero_drive_get_device (burner);
	dev_handle = brasero_device_handle_open (device, FALSE, NULL);
	if (!dev_handle)
		return BRASERO_BURN_ERR;

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

	BRASERO_JOB_LOG (self, "Found file %p", file);
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
	guint64 file_nb;
	BraseroTrack *track;
	GSettings *settings;
	GHashTable *excludedH;
	GChecksumType gchecksum_type;
	BraseroChecksumFilesPrivate *priv;
	BraseroChecksumType checksum_type;
	BraseroBurnResult result = BRASERO_BURN_OK;

	priv = BRASERO_CHECKSUM_FILES_PRIVATE (self);

	/* get the checksum type */
	settings = g_settings_new (BRASERO_SCHEMA_CONFIG);
	checksum_type = g_settings_get_int (settings, BRASERO_PROPS_CHECKSUM_FILES);
	g_object_unref (settings);

	if (checksum_type & BRASERO_CHECKSUM_MD5_FILE)
		gchecksum_type = G_CHECKSUM_MD5;
	else if (checksum_type & BRASERO_CHECKSUM_SHA1_FILE)
		gchecksum_type = G_CHECKSUM_SHA1;
	else if (checksum_type & BRASERO_CHECKSUM_SHA256_FILE)
		gchecksum_type = G_CHECKSUM_SHA256;
	else {
		checksum_type = BRASERO_CHECKSUM_MD5_FILE;
		gchecksum_type = G_CHECKSUM_MD5;
	}

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
	iter = brasero_track_data_get_excluded_list (BRASERO_TRACK_DATA (track));
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
	brasero_track_data_get_file_num (BRASERO_TRACK_DATA (track), &file_nb);
	if (file_nb > 0)
		brasero_job_start_progress (BRASERO_JOB (self), TRUE);
	else
		file_nb = -1;

	iter = brasero_track_data_get_grafts (BRASERO_TRACK_DATA (track));
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

static BraseroBurnResult
brasero_checksum_files_sum_on_disc_file (BraseroChecksumFiles *self,
					 GChecksumType type,
					 BraseroVolSrc *src,
					 BraseroVolFile *file,
					 gchar **checksum_string,
					 GError **error)
{
	guchar buffer [64 * 2048];
	BraseroChecksumFilesPrivate *priv;
	BraseroVolFileHandle *handle;
	GChecksum *checksum;
	gint read_bytes;

	priv = BRASERO_CHECKSUM_FILES_PRIVATE (self);

	handle = brasero_volume_file_open_direct (src, file);
	if (!handle)
		return BRASERO_BURN_ERR;

	checksum = g_checksum_new (type);

	read_bytes = brasero_volume_file_read_direct (handle,
						      buffer,
						      64);
	g_checksum_update (checksum, buffer, read_bytes);

	while (read_bytes == sizeof (buffer)) {
		if (priv->cancel) {
			brasero_volume_file_close (handle);
			return BRASERO_BURN_CANCEL;
		}

		read_bytes = brasero_volume_file_read_direct (handle,
							      buffer,
							      64);
		g_checksum_update (checksum, buffer, read_bytes);
	}

	*checksum_string = g_strdup (g_checksum_get_string (checksum));
	g_checksum_free (checksum);

	brasero_volume_file_close (handle);

	return BRASERO_BURN_OK;
}

static BraseroVolFile *
brasero_checksum_files_get_on_disc_checksum_type (BraseroChecksumFiles *self,
						  BraseroVolSrc *vol,
						  guint start_block)
{
	BraseroVolFile *file;
	BraseroChecksumFilesPrivate *priv;

	priv = BRASERO_CHECKSUM_FILES_PRIVATE (self);


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
			if (!file || !(priv->checksum_type & (BRASERO_CHECKSUM_SHA256_FILE|BRASERO_CHECKSUM_DETECT))) {
				BRASERO_JOB_LOG (self, "no checksum file found");
				if (file)
					brasero_volume_file_free (file);

				return NULL;
			}
			priv->checksum_type = BRASERO_CHECKSUM_SHA256_FILE;
		}
		else if (priv->checksum_type & (BRASERO_CHECKSUM_SHA1_FILE|BRASERO_CHECKSUM_DETECT))
			priv->checksum_type = BRASERO_CHECKSUM_SHA1_FILE;
		else {
			brasero_volume_file_free (file);
			file = NULL;
		}
	}
	else if (priv->checksum_type & (BRASERO_CHECKSUM_MD5_FILE|BRASERO_CHECKSUM_DETECT))
		priv->checksum_type = BRASERO_CHECKSUM_MD5_FILE;
	else {
		brasero_volume_file_free (file);
		file = NULL;
	}

	BRASERO_JOB_LOG (self, "Found file %p", file);
	return file;
}

static gint
brasero_checksum_files_get_line_num (BraseroChecksumFiles *self,
				     BraseroVolFileHandle *handle)
{
	BraseroBurnResult result;
	int num = 0;

	while ((result = brasero_volume_file_read_line (handle, NULL, 0)) == BRASERO_BURN_RETRY)
		num ++;

	if (result == BRASERO_BURN_ERR)
		return -1;

	brasero_volume_file_rewind (handle);
	return num;
}

static BraseroBurnResult
brasero_checksum_files_check_files (BraseroChecksumFiles *self,
				    GError **error)
{
	GValue *value;
	guint file_nb;
	guint file_num;
	gint checksum_len;
	BraseroVolSrc *vol;
	goffset start_block;
	BraseroTrack *track;
	const gchar *device;
	BraseroVolFile *file;
	BraseroDrive *drive;
	BraseroMedium *medium;
	GChecksumType gchecksum_type;
	GArray *wrong_checksums = NULL;
	BraseroDeviceHandle *dev_handle;
	BraseroChecksumFilesPrivate *priv;
	BraseroVolFileHandle *handle = NULL;
	BraseroBurnResult result = BRASERO_BURN_OK;

	priv = BRASERO_CHECKSUM_FILES_PRIVATE (self);

	/* get medium */
	brasero_job_get_current_track (BRASERO_JOB (self), &track);
	drive = brasero_track_disc_get_drive (BRASERO_TRACK_DISC (track));
	medium = brasero_drive_get_medium (drive);

	/* open volume */
	if (!brasero_medium_get_last_data_track_address (medium, NULL, &start_block))
		return BRASERO_BURN_ERR;

	device = brasero_drive_get_device (brasero_medium_get_drive (medium));
	dev_handle = brasero_device_handle_open (device, FALSE, NULL);
	if (!dev_handle)
		return BRASERO_BURN_ERROR;

	vol = brasero_volume_source_open_device_handle (dev_handle, error);

	/* open checksum file */
	file = brasero_checksum_files_get_on_disc_checksum_type (self,
								 vol,
								 start_block);
	if (!file) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("No checksum file could be found on the disc"));

		BRASERO_JOB_LOG (self, "No checksum file");
		result = BRASERO_BURN_ERR;
		goto end;
	}

	handle = brasero_volume_file_open (vol, file);
	if (!handle) {
		BRASERO_JOB_LOG (self, "Cannot open checksum file");
		/* FIXME: error here ? */
		result = BRASERO_BURN_ERR;
		goto end;
	}

	/* get the number of files at this time and rewind */
	file_nb = brasero_checksum_files_get_line_num (self, handle);
	if (file_nb == 0) {
		BRASERO_JOB_LOG (self, "Empty checksum file");
		result = BRASERO_BURN_OK;
		goto end;
	}

	if (file_nb < 0) {
		/* An error here */
		BRASERO_JOB_LOG (self, "Failed to retrieve the number of lines");
		result = BRASERO_BURN_ERR;
		goto end;
	}

	/* signal we're ready to start */
	file_num = 0;
	brasero_job_set_current_action (BRASERO_JOB (self),
				        BRASERO_BURN_ACTION_CHECKSUM,
					_("Checking file integrity"),
					TRUE);
	brasero_job_start_progress (BRASERO_JOB (self), FALSE);

	/* Get the checksum type */
	switch (priv->checksum_type) {
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
		gchar file_path [MAXPATHLEN + 1];
		gchar checksum_file [512 + 1];
		BraseroVolFile *disc_file;
		gchar *checksum_real;
		gint read_bytes;

		if (priv->cancel)
			break;

		/* first read the checksum */
		read_bytes = brasero_volume_file_read (handle,
						       checksum_file,
						       checksum_len);
		if (read_bytes == 0)
			break;

		if (read_bytes != checksum_len) {
			/* FIXME: an error here */
			BRASERO_JOB_LOG (self, "Impossible to read the checksum from file");
			result = BRASERO_BURN_ERR;
			break;
		}
		checksum_file [checksum_len] = '\0';

		if (priv->cancel)
			break;

		/* skip spaces in between */
		while (1) {
			gchar c [2];

			read_bytes = brasero_volume_file_read (handle, c, 1);
			if (read_bytes == 0) {
				result = BRASERO_BURN_OK;
				goto end;
			}

			if (read_bytes < 0) {
				/* FIXME: an error here */
				BRASERO_JOB_LOG (self, "Impossible to read checksum file");
				result = BRASERO_BURN_ERR;
				goto end;
			}

			if (!isspace (c [0])) {
				file_path [0] = '/';
				file_path [1] = c [0];
				break;
			}
		}

		/* get the filename */
		result = brasero_volume_file_read_line (handle, file_path + 2, sizeof (file_path) - 2);

		/* FIXME: an error here */
		if (result == BRASERO_BURN_ERR) {
			BRASERO_JOB_LOG (self, "Impossible to read checksum file");
			break;
		}

		checksum_real = NULL;

		/* get the file handle itself */
		BRASERO_JOB_LOG (self, "Getting file %s", file_path);
		disc_file = brasero_volume_get_file (vol,
						     file_path,
						     start_block,
						     NULL);
		if (!disc_file) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("File \"%s\" could not be opened"),
				     file_path);
			result = BRASERO_BURN_ERR;
			break;
		}

		/* we certainly don't want to checksum anything but regular file
		 * if (!g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
		 *	brasero_volume_file_free (disc_file);
		 *	continue;
		 * }
		 */

		/* checksum the file */
		result = brasero_checksum_files_sum_on_disc_file (self,
								  gchecksum_type,
								  vol,
								  disc_file,
								  &checksum_real,
								  error);
		brasero_volume_file_free (disc_file);
		if (result == BRASERO_BURN_ERR) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("File \"%s\" could not be opened"),
				     file_path);
			break;
		}

		if (result != BRASERO_BURN_OK)
			break;

		file_num++;
		brasero_job_set_progress (BRASERO_JOB (self),
					  (gdouble) file_num /
					  (gdouble) file_nb);
		BRASERO_JOB_LOG (self,
				 "comparing checksums for file %s : %s (from md5 file) / %s (current)",
				 file_path, checksum_file, checksum_real);

		if (strcmp (checksum_file, checksum_real)) {
			gchar *string;

			BRASERO_JOB_LOG (self, "Wrong checksum");
			if (!wrong_checksums)
				wrong_checksums = g_array_new (TRUE,
							       TRUE, 
							       sizeof (gchar *));

			string = g_strdup (file_path);
			wrong_checksums = g_array_append_val (wrong_checksums, string);
		}

		g_free (checksum_real);
		if (priv->cancel)
			break;
	}

end:

	if (handle)
		brasero_volume_file_close (handle);

	if (file)
		brasero_volume_file_free (file);

	if (vol)
		brasero_volume_source_close (vol);

	if (dev_handle)
		brasero_device_handle_close (dev_handle);

	if (result != BRASERO_BURN_OK) {
		BRASERO_JOB_LOG (self, "Ended with an error");
		if (wrong_checksums) {
			g_strfreev ((gchar **) wrong_checksums->data);
			g_array_free (wrong_checksums, FALSE);
		}
		return result;
	}

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
	BraseroJobAction action;
	BraseroChecksumFiles *self;
	BraseroTrack *current = NULL;
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
	brasero_job_get_current_track (BRASERO_JOB (self), &current);

	/* let's create a new DATA track with the md5 file created */
	if (BRASERO_IS_TRACK_DATA (current)) {
		GSList *iter;
		GSList *grafts;
		GSList *excluded;
		BraseroGraftPt *graft;
		GSList *new_grafts = NULL;
		BraseroTrackData *track = NULL;

		/* for DATA track we add the file to the track */
		grafts = brasero_track_data_get_grafts (BRASERO_TRACK_DATA (current));
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
		excluded = brasero_track_data_get_excluded_list (BRASERO_TRACK_DATA (current));

		/* Duplicate the list since brasero_track_data_set_source ()
		 * takes ownership afterwards */
		excluded = g_slist_copy (excluded);
		for (iter = excluded; iter; iter = iter->next)
			iter->data = g_strdup (iter->data);

		track = brasero_track_data_new ();
		brasero_track_data_add_fs (track, brasero_track_data_get_fs (BRASERO_TRACK_DATA (current)));
		brasero_track_data_set_source (track, new_grafts, excluded);
		brasero_track_set_checksum (BRASERO_TRACK (track),
					    priv->checksum_type,
					    graft->uri);

		brasero_job_add_track (BRASERO_JOB (self), BRASERO_TRACK (track));

		/* It's good practice to unref the track afterwards as we don't
		 * need it anymore. BraseroTaskCtx refs it. */
		g_object_unref (track);
		
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
	GError *error = NULL;
	BraseroJobAction action;
	BraseroChecksumFiles *self;
	BraseroTrack *current = NULL;
	BraseroChecksumFilesPrivate *priv;
	BraseroChecksumFilesThreadCtx *ctx;
	BraseroBurnResult result = BRASERO_BURN_NOT_SUPPORTED;

	self = BRASERO_CHECKSUM_FILES (data);
	priv = BRASERO_CHECKSUM_FILES_PRIVATE (self);

	/* check DISC types and add checksums for DATA and IMAGE-bin types */
	brasero_job_get_action (BRASERO_JOB (self), &action);
	brasero_job_get_current_track (BRASERO_JOB (self), &current);
	if (action == BRASERO_JOB_ACTION_CHECKSUM) {
		priv->checksum_type = brasero_track_get_checksum_type (current);
		if (priv->checksum_type & (BRASERO_CHECKSUM_MD5_FILE|BRASERO_CHECKSUM_SHA1_FILE|BRASERO_CHECKSUM_SHA256_FILE|BRASERO_CHECKSUM_DETECT))
			result = brasero_checksum_files_check_files (self, &error);
		else
			result = BRASERO_BURN_ERR;
	}
	else if (action == BRASERO_JOB_ACTION_IMAGE) {
		if (BRASERO_IS_TRACK_DATA (current))
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

	/* End thread */
	g_mutex_lock (priv->mutex);
	priv->thread = NULL;
	g_cond_signal (priv->cond);
	g_mutex_unlock (priv->mutex);

	g_thread_exit (NULL);
	return NULL;
}

static BraseroBurnResult
brasero_checksum_files_start (BraseroJob *job,
			      GError **error)
{
	BraseroChecksumFilesPrivate *priv;
	GError *thread_error = NULL;
	BraseroJobAction action;

	brasero_job_get_action (job, &action);
	if (action == BRASERO_JOB_ACTION_SIZE) {
		/* say we won't write to disc */
		brasero_job_set_output_size_for_current_track (job, 0, 0);
		return BRASERO_BURN_NOT_RUNNING;
	}

	/* we start a thread for the exploration of the graft points */
	priv = BRASERO_CHECKSUM_FILES_PRIVATE (job);
	g_mutex_lock (priv->mutex);
	priv->thread = g_thread_create (brasero_checksum_files_thread,
					BRASERO_CHECKSUM_FILES (job),
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
brasero_checksum_files_activate (BraseroJob *job,
				 GError **error)
{
	GSList *grafts;
	BraseroTrack *track = NULL;
	BraseroTrackType *output = NULL;

	output = brasero_track_type_new ();
	brasero_job_get_output_type (job, output);

	if (!brasero_track_type_get_has_data (output)) {
		brasero_track_type_free (output);
		return BRASERO_BURN_OK;
	}

	brasero_track_type_free (output);

	/* see that a file with graft "/BRASERO_CHECKSUM_FILE" doesn't already
	 * exists (possible when doing several copies) or when a simulation 
	 * already took place before. */
	brasero_job_get_current_track (job, &track);
	grafts = brasero_track_data_get_grafts (BRASERO_TRACK_DATA (track));
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
brasero_checksum_files_stop (BraseroJob *job,
			     GError **error)
{
	BraseroChecksumFilesPrivate *priv;

	priv = BRASERO_CHECKSUM_FILES_PRIVATE (job);

	g_mutex_lock (priv->mutex);
	if (priv->thread) {
		priv->cancel = 1;
		g_cond_wait (priv->cond, priv->mutex);
		priv->cancel = 0;
		priv->thread = NULL;
	}
	g_mutex_unlock (priv->mutex);

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
{
	BraseroChecksumFilesPrivate *priv;

	priv = BRASERO_CHECKSUM_FILES_PRIVATE (obj);

	priv->mutex = g_mutex_new ();
	priv->cond = g_cond_new ();
}

static void
brasero_checksum_files_finalize (GObject *object)
{
	BraseroChecksumFilesPrivate *priv;
	
	priv = BRASERO_CHECKSUM_FILES_PRIVATE (object);

	g_mutex_lock (priv->mutex);
	if (priv->thread) {
		priv->cancel = 1;
		g_cond_wait (priv->cond, priv->mutex);
		priv->cancel = 0;
		priv->thread = NULL;
	}
	g_mutex_unlock (priv->mutex);

	if (priv->end_id) {
		g_source_remove (priv->end_id);
		priv->end_id = 0;
	}

	if (priv->file) {
		fclose (priv->file);
		priv->file = NULL;
	}

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
}

static void
brasero_checksum_files_export_caps (BraseroPlugin *plugin)
{
	GSList *input;
	BraseroPluginConfOption *checksum_type;

	brasero_plugin_define (plugin,
	                       "file-checksum",
			       /* Translators: this is the name of the plugin
				* which will be translated only when it needs
				* displaying. */
			       N_("File Checksum"),
			       _("Checks file integrities on a disc"),
			       "Philippe Rouquier",
			       0);

	/* only generate a file for DATA input */
	input = brasero_caps_data_new (BRASERO_IMAGE_FS_ANY);
	brasero_plugin_process_caps (plugin, input);
	g_slist_free (input);

	/* run on initial track for whatever a DATA track */
	brasero_plugin_set_process_flags (plugin, BRASERO_PLUGIN_RUN_PREPROCESSING);

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
				   BRASERO_CHECKSUM_DETECT|				   
				   BRASERO_CHECKSUM_MD5_FILE|
				   BRASERO_CHECKSUM_SHA1_FILE|
				   BRASERO_CHECKSUM_SHA256_FILE,
				   input);
	g_slist_free (input);

	/* add some configure options */
	checksum_type = brasero_plugin_conf_option_new (BRASERO_PROPS_CHECKSUM_FILES,
							_("Hashing algorithm to be used:"),
							BRASERO_PLUGIN_OPTION_CHOICE);
	brasero_plugin_conf_option_choice_add (checksum_type,
					       _("MD5"), BRASERO_CHECKSUM_MD5_FILE);
	brasero_plugin_conf_option_choice_add (checksum_type,
					       _("SHA1"), BRASERO_CHECKSUM_SHA1_FILE);
	brasero_plugin_conf_option_choice_add (checksum_type,
					       _("SHA256"), BRASERO_CHECKSUM_SHA256_FILE);

	brasero_plugin_add_conf_option (plugin, checksum_type);

	brasero_plugin_set_compulsory (plugin, FALSE);
}
