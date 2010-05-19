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

#include "scsi-mmc2.h"

#include "brasero-media-private.h"

#include "scsi-error.h"
#include "scsi-base.h"
#include "scsi-command.h"
#include "scsi-opcodes.h"
#include "scsi-read-disc-structure.h"

#if G_BYTE_ORDER == G_LITTLE_ENDIAN

struct _BraseroReadDiscStructureCDB {
	uchar opcode;

	uchar media_type		:4;
	uchar reserved0			:4;

	/* for formats 0x83 */
	uchar address			[4];
	uchar layer_num;

	uchar format;

	uchar alloc_len			[2];

	uchar reserved1			:6;

	/* for formats 0x02, 0x06, 0x07, 0x80, 0x82 */
	uchar agid			:2;

	uchar ctl;
};

#else

struct _BraseroReadDiscStructureCDB {
	uchar opcode;

	uchar reserved0			:4;
	uchar media_type		:4;

	uchar address			[4];

	uchar layer_num;
	uchar format;

	uchar alloc_len			[2];

	uchar agid			:2;
	uchar reserved1			:6;

	uchar ctl;
};

#endif

typedef struct _BraseroReadDiscStructureCDB BraseroReadDiscStructureCDB;

BRASERO_SCSI_COMMAND_DEFINE (BraseroReadDiscStructureCDB,
			     READ_DISC_STRUCTURE,
			     BRASERO_SCSI_READ);

typedef enum {
BRASERO_MEDIA_DVD_HD_DVD			= 0x00,
BRASERO_MEDIA_BD				= 0x01
	/* reserved */
} BraseroScsiMediaType;

static BraseroScsiResult
brasero_read_disc_structure (BraseroReadDiscStructureCDB *cdb,
			     BraseroScsiReadDiscStructureHdr **data,
			     int *size,
			     BraseroScsiErrCode *error)
{
	BraseroScsiReadDiscStructureHdr *buffer;
	BraseroScsiReadDiscStructureHdr hdr;
	BraseroScsiResult res;
	int request_size;

	if (!data || !size) {
		BRASERO_SCSI_SET_ERRCODE (error, BRASERO_SCSI_BAD_ARGUMENT);
		return BRASERO_SCSI_FAILURE;
	}

	BRASERO_SET_16 (cdb->alloc_len, sizeof (hdr));

	memset (&hdr, 0, sizeof (hdr));
	res = brasero_scsi_command_issue_sync (cdb, &hdr, sizeof (hdr), error);
	if (res)
		return res;

	request_size = BRASERO_GET_16 (hdr.len) + sizeof (hdr.len);
	buffer = (BraseroScsiReadDiscStructureHdr *) g_new0 (uchar, request_size);

	BRASERO_SET_16 (cdb->alloc_len, request_size);
	res = brasero_scsi_command_issue_sync (cdb, buffer, request_size, error);
	if (res) {
		g_free (buffer);
		return res;
	}

	if (request_size < BRASERO_GET_16 (buffer->len) + sizeof (buffer->len)) {
		BRASERO_SCSI_SET_ERRCODE (error, BRASERO_SCSI_SIZE_MISMATCH);
		g_free (buffer);
		return BRASERO_SCSI_FAILURE;
	}

	*data = buffer;
	*size = request_size;

	return res;
}

BraseroScsiResult
brasero_mmc2_read_generic_structure (BraseroDeviceHandle *handle,
				     BraseroScsiGenericFormatType type,
				     BraseroScsiReadDiscStructureHdr **data,
				     int *size,
				     BraseroScsiErrCode *error)
{
	BraseroReadDiscStructureCDB *cdb;
	BraseroScsiResult res;

	g_return_val_if_fail (handle != NULL, BRASERO_SCSI_FAILURE);

	cdb = brasero_scsi_command_new (&info, handle);
	cdb->format = type;

	res = brasero_read_disc_structure (cdb, data, size, error);
	brasero_scsi_command_free (cdb);
	return res;
}

#if 0

/* So far this function only creates a warning at
 * build time and is not used but may be in the
 * future. */

BraseroScsiResult
brasero_mmc2_read_dvd_structure (BraseroDeviceHandle *handle,
				 int address,
				 BraseroScsiDVDFormatType type,
				 BraseroScsiReadDiscStructureHdr **data,
				 int *size,
				 BraseroScsiErrCode *error)
{
	BraseroReadDiscStructureCDB *cdb;
	BraseroScsiResult res;

	cdb = brasero_scsi_command_new (&info, handle);
	cdb->format = type;
	cdb->media_type = BRASERO_MEDIA_DVD_HD_DVD;
	BRASERO_SET_32 (cdb->address, address);

	res = brasero_read_disc_structure (cdb, data, size, error);
	brasero_scsi_command_free (cdb);
	return res;
}

BraseroScsiResult
brasero_mmc5_read_bd_structure (BraseroDeviceHandle *handle,
				BraseroScsiBDFormatType type,
				BraseroScsiReadDiscStructureHdr **data,
				int *size,
				BraseroScsiErrCode *error)
{
	BraseroReadDiscStructureCDB *cdb;
	BraseroScsiResult res;

	cdb = brasero_scsi_command_new (&info, handle);
	cdb->format = type;
	cdb->media_type = BRASERO_MEDIA_BD;

	res = brasero_read_disc_structure (cdb, data, size, error);
	brasero_scsi_command_free (cdb);
	return res;
}

#endif
