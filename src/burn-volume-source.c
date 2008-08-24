/***************************************************************************
 *            burn-volume-source.c
 *
 *  Sun May 18 09:48:14 2008
 *  Copyright  2008  Philippe Rouquier
 *  <bonfire-app@wanadoo.fr>
 ****************************************************************************/

/*
 * Brasero is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * Brasero is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "burn-debug.h"
#include "burn-volume-source.h"
#include "burn-iso9660.h"

#include "scsi-mmc1.h"

static gint64
brasero_volume_source_seek_device_handle (BraseroVolSrc *src,
					  guint block,
					  gint whence,
					  GError **error)
{
	gint64 oldpos;

	oldpos = src->position;

	if (whence == SEEK_CUR)
		src->position += block;
	else if (whence == SEEK_SET)
		src->position = block;

	return oldpos;
}

static gint64
brasero_volume_source_seek_fd (BraseroVolSrc *src,
			       guint block,
			       int whence,
			       GError **error)
{
	gint64 oldpos;

	oldpos = ftello (src->data);
	if (fseeko (src->data, (guint64) (block * ISO9660_BLOCK_SIZE), whence) == -1) {
		BRASERO_BURN_LOG ("fseeko () failed at block %i (= %lli bytes) (%s)",
				  block,
				  (guint64) (block * ISO9660_BLOCK_SIZE),
				  strerror (errno));
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     strerror (errno));
		return -1;
	}

	return oldpos / ISO9660_BLOCK_SIZE;
}

static gboolean
brasero_volume_source_read_fd (BraseroVolSrc *src,
			       gchar *buffer,
			       guint blocks,
			       GError **error)
{
	guint64 bytes_read;

	bytes_read = fread (buffer, 1, ISO9660_BLOCK_SIZE * blocks, src->data);
	if (bytes_read != ISO9660_BLOCK_SIZE * blocks) {
		BRASERO_BURN_LOG ("fread () failed (%s)", strerror (errno));
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     strerror (errno));
		return FALSE;
	}

	return TRUE;
}

static gboolean
brasero_volume_source_read_device_handle (BraseroVolSrc *src,
					  gchar *buffer,
					  guint blocks,
					  GError **error)
{
	BraseroScsiResult result;
	BraseroScsiErrCode code;

	BRASERO_BURN_LOG ("Reading with track mode %i", src->data_mode);
	result = brasero_mmc1_read_block (src->data,
					  TRUE,
					  src->data_mode,
					  BRASERO_SCSI_BLOCK_HEADER_NONE,
					  BRASERO_SCSI_BLOCK_NO_SUBCHANNEL,
					  src->position,
					  blocks,
					  (unsigned char *) buffer,
					  blocks * ISO9660_BLOCK_SIZE,
					  &code);
	if (result == BRASERO_SCSI_OK) {
		src->position += blocks;
		return TRUE;
	}

	/* Give it a last chance if the code is BRASERO_SCSI_INVALID_TRACK_MODE */
	if (code == BRASERO_SCSI_INVALID_TRACK_MODE) {
		BRASERO_BURN_LOG ("Wrong track mode autodetecting mode for block %i",
				  src->position);

		for (src->data_mode = BRASERO_SCSI_BLOCK_TYPE_CDDA;
		     src->data_mode <= BRASERO_SCSI_BLOCK_TYPE_MODE2_FORM2;
		     src->data_mode ++) {
			BRASERO_BURN_LOG ("Re-trying with track mode %i", src->data_mode);
			result = brasero_mmc1_read_block (src->data,
							  TRUE,
							  src->data_mode,
							  BRASERO_SCSI_BLOCK_HEADER_NONE,
							  BRASERO_SCSI_BLOCK_NO_SUBCHANNEL,
							  src->position,
							  blocks,
							  (unsigned char *) buffer,
							  blocks * ISO9660_BLOCK_SIZE,
							  &code);

			if (result == BRASERO_SCSI_OK) {
				src->position += blocks;
				return TRUE;
			}

			if (code != BRASERO_SCSI_INVALID_TRACK_MODE) {
				BRASERO_BURN_LOG ("Failed with error code %i", code);
				src->data_mode = BRASERO_SCSI_BLOCK_TYPE_ANY;
				break;
			}
		}
	}

	g_set_error (error,
		     BRASERO_BURN_ERROR,
		     BRASERO_BURN_ERROR_GENERAL,
		     brasero_scsi_strerror (code));

	return FALSE;
}

void
brasero_volume_source_close (BraseroVolSrc *src)
{
	src->ref --;
	if (src->ref > 0)
		return;

	if (src->seek == brasero_volume_source_seek_fd)
		fclose (src->data);

	g_free (src);
}

BraseroVolSrc *
brasero_volume_source_open_file (const gchar *path,
				 GError **error)
{
	BraseroVolSrc *src;
	FILE *file;

	file = fopen (path, "r");
	if (!file) {
		BRASERO_BURN_LOG ("open () failed (%s)", strerror (errno));
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     strerror (errno));
		return FALSE;
	}

	src = g_new0 (BraseroVolSrc, 1);
	src->ref = 1;
	src->data = file;
	src->seek = brasero_volume_source_seek_fd;
	src->read = brasero_volume_source_read_fd;
	return src;
}

BraseroVolSrc *
brasero_volume_source_open_fd (int fd,
			       GError **error)
{
	BraseroVolSrc *src;
	int dup_fd;
	FILE *file;

	dup_fd = dup (fd);
	if (dup_fd == -1) {
		BRASERO_BURN_LOG ("dup () failed (%s)", strerror (errno));
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     strerror (errno));
		return FALSE;
	}

	file = fdopen (dup_fd, "r");
	if (!file) {
		close (dup_fd);

		BRASERO_BURN_LOG ("fdopen () failed (%s)", strerror (errno));
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     strerror (errno));
		return FALSE;
	}

	src = g_new0 (BraseroVolSrc, 1);
	src->ref = 1;
	src->data = file;
	src->seek = brasero_volume_source_seek_fd;
	src->read = brasero_volume_source_read_fd;
	return src;
}

BraseroVolSrc *
brasero_volume_source_open_device_handle (BraseroDeviceHandle *handle,
					  GError **error)
{
	BraseroVolSrc *src;

	src = g_new0 (BraseroVolSrc, 1);
	src->ref = 1;
	src->data = handle;
	src->seek = brasero_volume_source_seek_device_handle;
	src->read = brasero_volume_source_read_device_handle;
	return src;
}

BraseroVolSrc *
brasero_volume_source_open_device_path (const gchar *path,
					GError **error)
{
	BraseroScsiErrCode err;
	BraseroDeviceHandle *handle;

	handle = brasero_device_handle_open (path, &err);
	if (!handle) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     brasero_scsi_strerror (err));
		return NULL;
	}

	return brasero_volume_source_open_device_handle (handle, error);
}

void
brasero_volume_source_ref (BraseroVolSrc *vol)
{
	vol->ref ++;
}

