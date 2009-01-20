/***************************************************************************
 *            brasero-medium-handle.c
 *
 *  Sat Mar 15 17:27:29 2008
 *  Copyright  2008  Philippe Rouquier
 *  <bonfire-app@wanadoo.fr>
 ****************************************************************************/

/*
 * Libbrasero-media is free software; you can redistribute it and/or modify
fy
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
	brasero_volume_source_close (handle->src);
	g_free (handle);
}

static gboolean
brasero_volume_file_rewind_real (BraseroVolFileHandle *handle)
{
	GSList *node;
	gint res_seek;
	gboolean result;
	BraseroVolFileExtent *extent;

	node = handle->extents_forward;
	extent = node->data;

	handle->extents_forward = g_slist_remove_link (handle->extents_forward, node);
	node->next = handle->extents_backward;
	handle->extents_backward = node;

	handle->position = extent->block;
	handle->extent_size = extent->size;
	handle->extent_last = BRASERO_BYTES_TO_SECTORS (extent->size, 2048) + extent->block;

	/* start loading first block */
	res_seek = BRASERO_VOL_SRC_SEEK (handle->src, handle->position, SEEK_SET,  NULL);
	if (res_seek == -1)
		return FALSE;

	result = BRASERO_VOL_SRC_READ (handle->src, (gchar *) handle->buffer, 1, NULL);
	if (!result)
		return FALSE;

	handle->offset = 0;
	handle->position ++;

	if (handle->position == handle->extent_last)
		handle->buffer_max = handle->extent_size % 2048;
	else
		handle->buffer_max = sizeof (handle->buffer);

	return TRUE;
}

BraseroVolFileHandle *
brasero_volume_file_open (BraseroVolSrc *src,
			  BraseroVolFile *file)
{
	BraseroVolFileHandle *handle;

	if (file->isdir)
		return NULL;

	handle = g_new0 (BraseroVolFileHandle, 1);
	handle->src = src;
	brasero_volume_source_ref (src);

	handle->extents_forward = g_slist_copy (file->specific.file.extents);
	if (!brasero_volume_file_rewind_real (handle)) {
		brasero_volume_file_close (handle);
		return NULL;
	}

	return handle;
}

gboolean
brasero_volume_file_rewind (BraseroVolFileHandle *handle)
{
	GSList *node, *next;

	/* Put back all extents in the unread list */
	for (node = handle->extents_backward; node; node = next) {
		next = node->next;
		handle->extents_backward = g_slist_remove_link (handle->extents_backward, node);

		node->next = handle->extents_forward;
		handle->extents_forward = node;
	}
	return brasero_volume_file_rewind_real (handle);
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
		gint res_seek;
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
		handle->extent_last = BRASERO_BYTES_TO_SECTORS (extent->size, 2048) + extent->block;

		res_seek = BRASERO_VOL_SRC_SEEK (handle->src, handle->position, SEEK_SET,  NULL);
		if (res_seek == -1)
			return BRASERO_BURN_ERR;
	}

	result = BRASERO_VOL_SRC_READ (handle->src, (char *) handle->buffer, 1, NULL);
	if (!result)
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
		/* copy what is already in the buffer and refill the latter */
		memcpy (buffer + buffer_offset,
			handle->buffer + handle->offset,
			handle->buffer_max - handle->offset);

		buffer_offset += handle->buffer_max - handle->offset;
		handle->offset = handle->buffer_max;

		result = brasero_volume_file_check_state (handle);
		if (result == BRASERO_BURN_OK)
			return buffer_offset;

		if (result == BRASERO_BURN_ERR)
			return -1;
	}

	/* we filled the buffer and put len bytes in it */
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
	guint line_len;

	/* search the next end of line characher in the buffer */
	break_line = memchr (handle->buffer + handle->offset,
			     '\n',
			     handle->buffer_max - handle->offset);

	if (!break_line)
		return FALSE;

	line_len = break_line - (handle->buffer + handle->offset);
	if (len && line_len >= len) {
		/* - 1 is to be able to set last character to '\0' */
		if (buffer) {
			memcpy (buffer + buffer_offset,
				handle->buffer + handle->offset,
				len - buffer_offset - 1);

			buffer [len - 1] = '\0';
		}

		handle->offset += len - buffer_offset - 1;
		return TRUE;
	}

	if (buffer) {
		memcpy (buffer, handle->buffer + handle->offset, line_len);
		buffer [line_len] = '\0';
	}

	/* add 1 to skip the line break */
	handle->offset += line_len + 1;
	return TRUE;
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
	while (!len || (len - buffer_offset) > (handle->buffer_max - handle->offset)) {
		BraseroScsiResult result;

		/* copy what we already have in the buffer. */
		if (buffer)
			memcpy (buffer + buffer_offset,
				handle->offset + handle->buffer,
				handle->buffer_max - handle->offset);

		buffer_offset += handle->buffer_max - handle->offset;
		handle->offset = handle->buffer_max;

		/* refill buffer */
		result = brasero_volume_file_check_state (handle);
		if (result == BRASERO_BURN_OK) {
			if (buffer)
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

	/* we filled the buffer */
	if (buffer) {
		memcpy (buffer + buffer_offset,
			handle->buffer + handle->offset,
			len - buffer_offset - 1);
		buffer [len - 1] = '\0';
	}

	/* NOTE: when len == 0 we never reach this part */
	handle->offset += len - buffer_offset - 1;

	return brasero_volume_file_check_state (handle);
}
