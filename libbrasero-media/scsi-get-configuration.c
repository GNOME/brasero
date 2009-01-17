/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Brasero
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Brasero is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Brasero authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Brasero. This permission is above and beyond the permissions granted
 * by the GPL license by which Brasero is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Brasero is distributed in the hope that it will be useful,
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

#include "scsi-error.h"
#include "scsi-utils.h"
#include "scsi-base.h"
#include "scsi-command.h"
#include "scsi-opcodes.h"
#include "scsi-get-configuration.h"

#if G_BYTE_ORDER == G_LITTLE_ENDIAN

struct _BraseroGetConfigCDB {
	uchar opcode		:8;

	uchar returned_data	:2;
	uchar reserved0		:6;

	uchar feature_num	[2];

	uchar reserved1		[3];

	uchar alloc_len		[2];

	uchar ctl;
};

#else

struct _BraseroGetConfigCDB {
	uchar opcode		:8;

	uchar reserved1		:6;
	uchar returned_data	:2;

	uchar feature_num	[2];

	uchar reserved0		[3];

	uchar alloc_len		[2];

	uchar ctl;
};

#endif

typedef struct _BraseroGetConfigCDB BraseroGetConfigCDB;

BRASERO_SCSI_COMMAND_DEFINE (BraseroGetConfigCDB,
			     GET_CONFIGURATION,
			     BRASERO_SCSI_READ);

typedef enum {
BRASERO_GET_CONFIG_RETURN_ALL_FEATURES	= 0x00,
BRASERO_GET_CONFIG_RETURN_ALL_CURRENT	= 0x01,
BRASERO_GET_CONFIG_RETURN_ONLY_FEATURE	= 0x02,
} BraseroGetConfigReturnedData;

static BraseroScsiResult
brasero_get_configuration (BraseroGetConfigCDB *cdb,
			   BraseroScsiGetConfigHdr **data,
			   int *size,
			   BraseroScsiErrCode *error)
{
	BraseroScsiGetConfigHdr *buffer;
	BraseroScsiGetConfigHdr hdr;
	BraseroScsiResult res;
	int request_size;
	int buffer_size;

	if (!data || !size) {
		BRASERO_SCSI_SET_ERRCODE (error, BRASERO_SCSI_BAD_ARGUMENT);
		return BRASERO_SCSI_FAILURE;
	}

	/* first issue the command once ... */
	memset (&hdr, 0, sizeof (hdr));
	BRASERO_SET_16 (cdb->alloc_len, sizeof (hdr));
	res = brasero_scsi_command_issue_sync (cdb, &hdr, sizeof (hdr), error);
	if (res)
		return res;

	/* ... check the size returned ... */
	request_size = BRASERO_GET_32 (hdr.len) +
		       G_STRUCT_OFFSET (BraseroScsiGetConfigHdr, len) +
		       sizeof (hdr.len);

	/* NOTE: if size is not valid use the maximum possible size */
	if ((request_size - sizeof (hdr)) % 8) {
		BRASERO_MEDIA_LOG ("Unaligned data (%i) setting to max (65530)", request_size);
		request_size = 65530;
	}
	else if (request_size <= sizeof (hdr)) {
		BRASERO_MEDIA_LOG ("Undersized data (%i) setting to max (65530)", request_size);
		request_size = 65530;
	}

	/* ... allocate a buffer and re-issue the command */
	buffer = (BraseroScsiGetConfigHdr *) g_new0 (uchar, request_size);

	BRASERO_SET_16 (cdb->alloc_len, request_size);
	res = brasero_scsi_command_issue_sync (cdb, buffer, request_size, error);
	if (res) {
		g_free (buffer);
		return res;
	}

	/* make sure the response has the requested size */
	buffer_size = BRASERO_GET_32 (buffer->len) +
		      G_STRUCT_OFFSET (BraseroScsiGetConfigHdr, len) +
		      sizeof (hdr.len);

	if (buffer_size != request_size)
		BRASERO_MEDIA_LOG ("Sizes mismatch asked %i / received %i",
				  request_size,
				  buffer_size);

	*data = buffer;
	*size = MIN (buffer_size, request_size);
	return BRASERO_SCSI_OK;
}

BraseroScsiResult
brasero_mmc2_get_configuration_feature (BraseroDeviceHandle *handle,
					BraseroScsiFeatureType type,
					BraseroScsiGetConfigHdr **data,
					int *size,
					BraseroScsiErrCode *error)
{
	BraseroGetConfigCDB *cdb;
	BraseroScsiResult res;

	g_return_val_if_fail (data != NULL, BRASERO_SCSI_FAILURE);
	g_return_val_if_fail (size != NULL, BRASERO_SCSI_FAILURE);

	cdb = brasero_scsi_command_new (&info, handle);
	BRASERO_SET_16 (cdb->feature_num, type);
	cdb->returned_data = BRASERO_GET_CONFIG_RETURN_ONLY_FEATURE;

	res = brasero_get_configuration (cdb, data, size, error);
	brasero_scsi_command_free (cdb);

	/* make sure the desc is the one we want */
	if ((*data) && BRASERO_GET_16 ((*data)->desc->code) != type) {
		BRASERO_SCSI_SET_ERRCODE (error, BRASERO_SCSI_TYPE_MISMATCH);

		g_free (*data);
		*size = 0;
		return BRASERO_SCSI_FAILURE;
	}

	return res;
}
