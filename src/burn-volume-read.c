/***************************************************************************
 *            brasero-medium-handle.c
 *
 *  Sat Mar 15 17:27:29 2008
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

#include "scsi-device.h"
#include "scsi-mmc1.h"
#include "burn-volume.h"
#include "burn-iso9660.h"
#include "burn-volume-read.h"

struct _BraseroVolFileHandle {
	guchar buffer [2048];
	guint buffer_max;

	/* position in buffer */
	guint offset;

	/* address (in blocks) for current extent */
	guint extent_last;

	/* size in bytes for the current extent */
	guint extent_size;

	BraseroVolSrc *src;
	GSList *extents_backward;
	GSList *extents_forward;
	guint position;
};

void
brasero_volume_file_close (BraseroVolFileHandle *handle)
{
	g_slist_free (handle->extents_forward);
	g_slist_free (handle->extents_backward);
	g_free (handle);
}

BraseroVolFileHandle *
brasero_volume_file_open (BraseroVolSrc *src,
			  BraseroVolFile *file)
{
	BraseroVolFileHandle *handle;
	BraseroVolFileExtent *extent;
	gboolean result;
	GSList *node;

	if (file->isdir)
		return NULL;

	handle = g_new0 (BraseroVolFileHandle, 1);
	handle->src = src;
	brasero_volume_source_ref (src);

	handle->extents_forward = g_slist_copy (file->specific.file.extents);

	node = handle->extents_forward;
	extent = node->data;

	handle->extents_forward = g_slist_remove_link (handle->extents_forward, node);
	node->next = handle->extents_backward;
	handle->extents_backward = node;

	handle->position = extent->block;
	handle->extent_size = extent->size;
	handle->extent_last = BRASERO_SIZE_TO_SECTORS (extent->size, 2048) + extent->block;

	/* start loading first block */
	result = BRASERO_VOL_SRC_SEEK (handle->src, handle->position, SEEK_SET,  NULL);
	if (!result) {
		brasero_volume_file_close (handle);
		return NULL;
	}

	result = BRASERO_VOL_SRC_READ (handle->src, (gchar *) handle->buffer, 1, NULL);
	if (!result) {
		brasero_volume_file_close (handle);
		return NULL;
	}

	handle->offset = 0;
	handle->position ++;

	if (handle->position == handle->extent_last)
		handle->buffer_max = handle->extent_size % 2048;
	else
		handle->buffer_max = sizeof (handle->buffer);

	return handle;
}

BraseroBurnResult
brasero_volume_file_check_state (BraseroVolFileHandle *handle)
{
	gboolean result;

	/* check if we need to load a new block */
	if (handle->offset < handle->buffer_max)
		return BRASERO_BURN_RETRY;

	/* check if we need to change our extent */
	if (handle->position >= handle->extent_last) {
		BraseroVolFileExtent *extent;
		GSList *node;

		/* we are at the end of current extent try to find another */
		if (!handle->extents_forward) {
			/* we reached the end of our file */
			return BRASERO_BURN_OK;
		}

		node = handle->extents_forward;
		extent = node->data;

		handle->extents_forward = g_slist_remove_link (handle->extents_forward, node);
		node->next = handle->extents_backward;
		handle->extents_backward = node;

		handle->position = extent->block;
		handle->extent_size = extent->size;
		handle->extent_last = BRASERO_SIZE_TO_SECTORS (extent->size, 2048) + extent->block;
	}

	result = BRASERO_VOL_SRC_READ (handle->src, (char *) handle->buffer, 1, NULL);
	if (result != BRASERO_SCSI_OK)
		return BRASERO_BURN_ERR;

	handle->offset = 0;
	handle->position ++;

	if (handle->position == handle->extent_last)
		handle->buffer_max = handle->extent_size % 2048;
	else
		handle->buffer_max = sizeof (handle->buffer);

	return BRASERO_BURN_RETRY;
}

gint
brasero_volume_file_read (BraseroVolFileHandle *handle,
			  gchar *buffer,
			  guint len)
{
	guint buffer_offset = 0;
	BraseroBurnResult result;

	while ((len - buffer_offset) > (handle->buffer_max - handle->offset)) {
		/* copy what we already have */
		memcpy (buffer + buffer_offset,
			handle->buffer + handle->offset,
			handle->buffer_max - handle->offset);

		buffer_offset += handle->buffer_max - handle->offset;

		result = brasero_volume_file_check_state (handle);
		if (result == BRASERO_BURN_OK)
			return buffer_offset;

		if (result == BRASERO_BURN_ERR)
			return -1;
	}

	/* we filled the buffer */
	memcpy (buffer + buffer_offset,
		handle->buffer + handle->offset,
		len - buffer_offset);

	handle->offset += len - buffer_offset;

	result = brasero_volume_file_check_state (handle);
	if (result == BRASERO_BURN_ERR)
		return -1;

	return len;
}

static gint
brasero_volume_file_find_line_break (BraseroVolFileHandle *handle,
				     guint buffer_offset,
				     gchar *buffer,
				     guint len)
{
	guchar *break_line;

	/* search the next end of line characher in the buffer */
	break_line = memchr (handle->buffer + handle->offset,
			     '\n',
			     handle->buffer_max - handle->offset);

	if (break_line) {
		guint line_len;

		line_len = break_line - (handle->buffer + handle->offset);
		if (line_len >= len) {
			/* - 1 is to be able to set last character to '\0' */
			memcpy (buffer + buffer_offset,
				handle->buffer + handle->offset,
				len - buffer_offset - 1);

			buffer [len - 1] = '\0';
			handle->offset += len - buffer_offset - 1;
			return TRUE;
		}

		memcpy (buffer, handle->buffer + handle->offset, line_len);
		buffer [line_len] = '\0';

		/* add 1 to skip the line break */
		handle->offset += line_len + 1;
		return TRUE;
	}

	return FALSE;
}

BraseroBurnResult
brasero_volume_file_read_line (BraseroVolFileHandle *handle,
			       gchar *buffer,
			       guint len)
{
	guint buffer_offset = 0;
	gboolean found;

	found = brasero_volume_file_find_line_break (handle,
						     buffer_offset,
						     buffer,
						     len);
	if (found)
		return brasero_volume_file_check_state (handle);

	/* continue while remaining data is too small to fit buffer */
	while ((len - buffer_offset) > (handle->buffer_max - handle->offset)) {
		BraseroScsiResult result;

		/* copy what we already have in the buffer. */
		memcpy (buffer + buffer_offset,
			handle->offset + handle->buffer,
			handle->buffer_max - handle->offset);

		buffer_offset += handle->buffer_max - handle->offset;
		handle->offset = handle->buffer_max;

		/* refill buffer */
		result = brasero_volume_file_check_state (handle);
		if (result == BRASERO_BURN_OK) {
			buffer [len - 1] = '\0';
			return result;
		}

		found = brasero_volume_file_find_line_break (handle,
							     buffer_offset,
							     buffer,
							     len);
		if (found)
			return brasero_volume_file_check_state (handle);
	}

	memcpy (buffer + buffer_offset,
		handle->buffer + handle->offset,
		len - buffer_offset - 1);

	/* we filled the buffer */
	buffer [len - 1] = '\0';
	handle->offset += len - buffer_offset - 1;

	return brasero_volume_file_check_state (handle);
}
