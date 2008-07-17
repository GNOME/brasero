/***************************************************************************
 *            burn-mode-sense.c
 *
 *  Thu Oct 19 19:40:03 2006
 *  Copyright  2006  algernon
 *  <algernon@localhost.localdomain>
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

#include <fcntl.h>

#include <glib.h>

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

#define BRASERO_MODE_DATA_SET_BYTE_ORDER(data)	data

#endif

typedef struct _BraseroModeSenseCDB BraseroModeSenseCDB;

BRASERO_SCSI_COMMAND_DEFINE (BraseroModeSenseCDB,
			     MODE_SENSE,
			     O_RDONLY,
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
	 * - the block descriptors are actually disabled
	 */
	request_size = BRASERO_GET_16 (header.hdr.len) +
		       G_STRUCT_OFFSET (BraseroScsiModeHdr, len) +
		       sizeof (header.hdr.len);

	page_size = header.page.len +
		    G_STRUCT_OFFSET (BraseroScsiModePage, len) +
		    sizeof (header.page.len);

	if (BRASERO_GET_16 (header.hdr.bdlen)
	||  request_size != page_size + sizeof (BraseroScsiModeHdr)) {
		BRASERO_SCSI_SET_ERRCODE (error, BRASERO_SCSI_SIZE_MISMATCH);
		res = BRASERO_SCSI_FAILURE;
		goto end;
	}

	/* ... allocate an appropriate buffer ... */
	buffer = (BraseroScsiModeData *) g_new0 (uchar, request_size);

	/* ... re-issue the command */
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
