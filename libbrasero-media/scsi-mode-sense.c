/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-media
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-media is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-media authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-media. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-media is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-media is distributed in the hope that it will be useful,
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

#include <glib.h>

#include "brasero-media-private.h"

#include "scsi-spc1.h"

#include "scsi-error.h"
#include "scsi-utils.h"
#include "scsi-base.h"
#include "scsi-command.h"
#include "scsi-opcodes.h"
#include "scsi-mode-pages.h"

/**
 * MODE SENSE command description (defined in SPC, Scsi Primary Commands) 
 */

#if G_BYTE_ORDER == G_LITTLE_ENDIAN

struct _BraseroModeSenseCDB {
	uchar opcode		:8;

	uchar reserved1		:3;
	uchar dbd		:1;
	uchar llbaa		:1;
	uchar reserved0		:3;

	uchar page_code		:8;
	uchar subpage_code	:8;

	uchar reserved2		[3];

	uchar alloc_len		[2];
	uchar ctl;
};

#else

struct _BraseroModeSenseCDB {
	uchar opcode		:8;

	uchar reserved0		:3;
	uchar llbaa		:1;
	uchar dbd		:1;
	uchar reserved1		:3;

	uchar page_code		:8;
	uchar subpage_code	:8;

	uchar reserved2		[3];

	uchar alloc_len		[2];
	uchar ctl;
};

#endif

typedef struct _BraseroModeSenseCDB BraseroModeSenseCDB;

BRASERO_SCSI_COMMAND_DEFINE (BraseroModeSenseCDB,
			     MODE_SENSE,
			     BRASERO_SCSI_READ);

#define BRASERO_MODE_DATA(data)			((BraseroScsiModeData *) (data))

BraseroScsiResult
brasero_spc1_mode_sense_get_page (BraseroDeviceHandle *handle,
				  BraseroSPCPageType num,
				  BraseroScsiModeData **data,
				  int *data_size,
				  BraseroScsiErrCode *error)
{
	int page_size;
	int buffer_size;
	int request_size;
	BraseroScsiResult res;
	BraseroModeSenseCDB *cdb;
	BraseroScsiModeData header;
	BraseroScsiModeData *buffer;

	g_return_val_if_fail (handle != NULL, BRASERO_SCSI_FAILURE);

	if (!data || !data_size) {
		BRASERO_SCSI_SET_ERRCODE (error, BRASERO_SCSI_BAD_ARGUMENT);
		return BRASERO_SCSI_FAILURE;
	}

	/* issue a first command to get the size of the page ... */
	cdb = brasero_scsi_command_new (&info, handle);
	cdb->dbd = 1;
	cdb->page_code = num;
	BRASERO_SET_16 (cdb->alloc_len, sizeof (header));
	bzero (&header, sizeof (header));

	BRASERO_MEDIA_LOG ("Getting page size");
	res = brasero_scsi_command_issue_sync (cdb, &header, sizeof (header), error);
	if (res)
		goto end;

	if (!header.hdr.len) {
		BRASERO_SCSI_SET_ERRCODE (error, BRASERO_SCSI_SIZE_MISMATCH);
		res = BRASERO_SCSI_FAILURE;
		goto end;
	}

	/* Paranoïa, make sure:
	 * - the size given in header, the one of the page returned are coherent
	 * - the block descriptors are actually disabled */
	if (BRASERO_GET_16 (header.hdr.bdlen)) {
		BRASERO_SCSI_SET_ERRCODE (error, BRASERO_SCSI_BAD_ARGUMENT);
		BRASERO_MEDIA_LOG ("Block descriptors not disabled %i", BRASERO_GET_16 (header.hdr.bdlen));
		res = BRASERO_SCSI_FAILURE;
		goto end;
	}

	request_size = BRASERO_GET_16 (header.hdr.len) +
		       G_STRUCT_OFFSET (BraseroScsiModeHdr, len) +
		       sizeof (header.hdr.len);

	page_size = header.page.len +
		    G_STRUCT_OFFSET (BraseroScsiModePage, len) +
		    sizeof (header.page.len);

	if (request_size != page_size + sizeof (BraseroScsiModeHdr)) {
		BRASERO_SCSI_SET_ERRCODE (error, BRASERO_SCSI_SIZE_MISMATCH);
		BRASERO_MEDIA_LOG ("Incoherent answer sizes: request %i, page %i", request_size, page_size);
		res = BRASERO_SCSI_FAILURE;
		goto end;
	}

	/* ... allocate an appropriate buffer ... */
	buffer = (BraseroScsiModeData *) g_new0 (uchar, request_size);

	/* ... re-issue the command */
	BRASERO_MEDIA_LOG("Getting page (size = %i)", request_size);

	BRASERO_SET_16 (cdb->alloc_len, request_size);
	res = brasero_scsi_command_issue_sync (cdb, buffer, request_size, error);
	if (res) {
		g_free (buffer);
		goto end;
	}

	/* Paranoïa, some more checks:
	 * - the size of the page returned is the one we requested
	 * - block descriptors are actually disabled
	 * - header claimed size == buffer size
	 * - header claimed size == sizeof (header) + sizeof (page) */
	buffer_size = BRASERO_GET_16 (buffer->hdr.len) +
		      G_STRUCT_OFFSET (BraseroScsiModeHdr, len) +
		      sizeof (buffer->hdr.len);

	page_size = buffer->page.len +
		    G_STRUCT_OFFSET (BraseroScsiModePage, len) +
		    sizeof (buffer->page.len);

	if (request_size != buffer_size
	||  BRASERO_GET_16 (buffer->hdr.bdlen)
	||  buffer_size != page_size + sizeof (BraseroScsiModeHdr)) {
		g_free (buffer);

		BRASERO_SCSI_SET_ERRCODE (error, BRASERO_SCSI_SIZE_MISMATCH);
		res = BRASERO_SCSI_FAILURE;
		goto end;
	}

	*data = buffer;
	*data_size = request_size;

end:
	brasero_scsi_command_free (cdb);
	return res;
}
