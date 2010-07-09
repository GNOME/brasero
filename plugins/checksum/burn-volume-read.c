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

#include "scsi-device.h"
#include "scsi-mmc1.h"
#include "burn-volume.h"
#include "burn-iso9660.h"
#include "burn-volume-read.h"

struct _BraseroVolFileHandle {
	/* 64 is an empirical value based on one of my drives. */
	guchar buffer [2048 * 64];
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
brasero_volume_file_fill_buffer (BraseroVolFileHandle *handle)
{
	guint blocks;
	gboolean result;

	blocks = MIN (sizeof (handle->buffer) / 2048,
		      handle->extent_last - handle->position);

	result = BRASERO_VOL_SRC_READ (handle->src,
				       (char *) handle->buffer,
				       blocks,
				       NULL);
	if (!result)
		return FALSE;

	handle->offset = 0;
	handle->position += blocks;

	if (handle->position == handle->extent_last)
		handle->buffer_max = (blocks - 1) * 2048 +
				     ((handle->extent_size % 2048) ?
				      (handle->extent_size % 2048) :
				       2048);
	else
		handle->buffer_max = sizeof (handle->buffer);

	return TRUE;
}

static gboolean
brasero_volume_file_next_extent (BraseroVolFileHandle *handle)
{
	BraseroVolFileExtent *extent;
	gint res_seek;
	GSList *node;

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
		return FALSE;

	return TRUE;
}

static gboolean
brasero_volume_file_rewind_real (BraseroVolFileHandle *handle)
{
	if (!brasero_volume_file_next_extent (handle))
		return FALSE;

	return brasero_volume_file_fill_buffer (handle);
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

static BraseroBurnResult
brasero_volume_file_check_state (BraseroVolFileHandle *handle)
{
	/* check if we need to load a new block */
	if (handle->offset < handle->buffer_max)
		return BRASERO_BURN_RETRY;

	/* check if we need to change our extent */
	if (handle->position >= handle->extent_last) {
		/* we are at the end of current extent try to find another */
		if (!handle->extents_forward) {
			/* we reached the end of our file */
			return BRASERO_BURN_OK;
		}

		if (!brasero_volume_file_next_extent (handle))
			return BRASERO_BURN_ERR;
	}

	/* Refill buffer */
	if (!brasero_volume_file_fill_buffer (handle))
		return BRASERO_BURN_ERR;

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
		BraseroBurnResult result;

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

BraseroVolFileHandle *
brasero_volume_file_open_direct (BraseroVolSrc *src,
				 BraseroVolFile *file)
{
	BraseroVolFileHandle *handle;

	if (file->isdir)
		return NULL;

	handle = g_new0 (BraseroVolFileHandle, 1);
	handle->src = src;
	brasero_volume_source_ref (src);

	handle->extents_forward = g_slist_copy (file->specific.file.extents);

	/* Here the buffer stays unused, we copy straight to the buffer passed
	 * in the read direct function. */
	if (!brasero_volume_file_next_extent (handle)) {
		brasero_volume_file_close (handle);
		return NULL;
	}

	return handle;
}

gint64
brasero_volume_file_read_direct (BraseroVolFileHandle *handle,
				 guchar *buffer,
				 guint blocks)
{
	gboolean result;
	guint block2read;
	guint readblocks = 0;

start:

	block2read = MIN (blocks - readblocks, handle->extent_last - handle->position);
	if (!block2read)
		return readblocks * 2048;

	result = BRASERO_VOL_SRC_READ (handle->src,
				       (char *) buffer + readblocks * 2048,
				       block2read,
				       NULL);
	if (!result)
		return -1;

	handle->position += block2read;
	readblocks += block2read;

	if (handle->position == handle->extent_last) {
		/* we are at the end of current extent try to find another */
		if (!handle->extents_forward) {
			/* we reached the end of our file */
			return (readblocks - 1) * 2048 +
			       ((handle->extent_size % 2048) != 0?
			        (handle->extent_size % 2048) :
				 2048);
		}

		if (!brasero_volume_file_next_extent (handle))
			return -1;

		goto start;
	}

	return readblocks * 2048;
}

