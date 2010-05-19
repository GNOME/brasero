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

#include "scsi-mmc1.h"

#include "scsi-error.h"
#include "scsi-utils.h"
#include "scsi-base.h"
#include "scsi-command.h"
#include "scsi-opcodes.h"
#include "scsi-read-disc-info.h"

#if G_BYTE_ORDER == G_LITTLE_ENDIAN

struct _BraseroRdDiscInfoCDB {
	uchar opcode;

	uchar data_type		:3;
	uchar reserved0		:5;

	uchar reserved1		[5];
	uchar alloc_len		[2];

	uchar ctl;
};

#else

struct _BraseroRdDiscInfoCDB {
	uchar opcode;

	uchar reserved0		:5;
	uchar data_type		:3;

	uchar reserved1		[5];
	uchar alloc_len		[2];

	uchar ctl;
};

#endif

typedef struct _BraseroRdDiscInfoCDB BraseroRdDiscInfoCDB;

BRASERO_SCSI_COMMAND_DEFINE (BraseroRdDiscInfoCDB,
			     READ_DISC_INFORMATION,
			     BRASERO_SCSI_READ);

typedef enum {
BRASERO_DISC_INFO_STD		= 0x00,
BRASERO_DISC_INFO_TRACK_RES	= 0x01,
BRASERO_DISC_INFO_POW_RES	= 0x02,
	/* reserved */
} BraseroDiscInfoType;


BraseroScsiResult
brasero_mmc1_read_disc_information_std (BraseroDeviceHandle *handle,
					BraseroScsiDiscInfoStd **info_return,
					int *size,
					BraseroScsiErrCode *error)
{
	BraseroScsiDiscInfoStd std_info;
	BraseroScsiDiscInfoStd *buffer;
	BraseroRdDiscInfoCDB *cdb;
	BraseroScsiResult res;
	int request_size;
	int buffer_size;

	g_return_val_if_fail (handle != NULL, BRASERO_SCSI_FAILURE);

	if (!info_return || !size) {
		BRASERO_SCSI_SET_ERRCODE (error, BRASERO_SCSI_BAD_ARGUMENT);
		return BRASERO_SCSI_FAILURE;
	}

	cdb = brasero_scsi_command_new (&info, handle);
	cdb->data_type = BRASERO_DISC_INFO_STD;
	BRASERO_SET_16 (cdb->alloc_len, sizeof (BraseroScsiDiscInfoStd));

	memset (&std_info, 0, sizeof (BraseroScsiDiscInfoStd));
	res = brasero_scsi_command_issue_sync (cdb,
					       &std_info,
					       sizeof (BraseroScsiDiscInfoStd),
					       error);
	if (res)
		goto end;

	request_size = BRASERO_GET_16 (std_info.len) + 
		       sizeof (std_info.len);
	
	buffer = (BraseroScsiDiscInfoStd *) g_new0 (uchar, request_size);

	BRASERO_SET_16 (cdb->alloc_len, request_size);
	res = brasero_scsi_command_issue_sync (cdb, buffer, request_size, error);
	if (res) {
		g_free (buffer);
		goto end;
	}

	buffer_size = BRASERO_GET_16 (buffer->len) +
		      sizeof (buffer->len);

	if (request_size != buffer_size)
		BRASERO_MEDIA_LOG ("Sizes mismatch asked %i / received %i",
				  request_size,
				  buffer_size);

	*info_return = buffer;
	*size = MIN (request_size, buffer_size);

end:

	brasero_scsi_command_free (cdb);
	return res;
}

#if 0

/* These functions are not used for now but may
 * be one day. So keep them around but turn 
 * them off to avoid build warnings */
 
BraseroScsiResult
brasero_mmc5_read_disc_information_tracks (BraseroDeviceHandle *handle,
					   BraseroScsiTrackResInfo *info_return,
					   int size,
					   BraseroScsiErrCode *error)
{
	BraseroRdDiscInfoCDB *cdb;
	BraseroScsiResult res;

	cdb = brasero_scsi_command_new (&info, handle);
	cdb->data_type = BRASERO_DISC_INFO_TRACK_RES;
	BRASERO_SET_16 (cdb->alloc_len, size);

	memset (info_return, 0, size);
	res = brasero_scsi_command_issue_sync (cdb, info_return, size, error);
	brasero_scsi_command_free (cdb);
	return res;
}

BraseroScsiResult
brasero_mmc5_read_disc_information_pows (BraseroDeviceHandle *handle,
					 BraseroScsiPOWResInfo *info_return,
					 int size,
					 BraseroScsiErrCode *error)
{
	BraseroRdDiscInfoCDB *cdb;
	BraseroScsiResult res;

	cdb = brasero_scsi_command_new (&info, handle);
	cdb->data_type = BRASERO_DISC_INFO_POW_RES;
	BRASERO_SET_16 (cdb->alloc_len, size);

	memset (info_return, 0, size);
	res = brasero_scsi_command_issue_sync (cdb, info_return, size, error);
	brasero_scsi_command_free (cdb);
	return res;
}

#endif
