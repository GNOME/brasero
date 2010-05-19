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

#include "scsi-mmc3.h"

#include "scsi-error.h"
#include "scsi-utils.h"
#include "scsi-command.h"
#include "scsi-opcodes.h"
#include "scsi-get-performance.h"

/**
 * GET PERMORMANCE command description	(MMC2)
 */

#if G_BYTE_ORDER == G_LITTLE_ENDIAN

struct _BraseroGetPerformanceCDB {
	uchar opcode		:8;

	uchar except		:2;
	uchar write		:1;
	uchar tolerance		:2;
	uchar reserved0		:3;

	uchar start_lba		[4];

	uchar reserved1		[2];

	uchar max_desc		[2];

	uchar type;
	uchar ctl;
};

#else

struct _BraseroGetPerformanceCDB {
	uchar opcode		:8;

	uchar reserved0		:3;
	uchar tolerance		:2;
	uchar write		:1;
	uchar except		:2;

	uchar start_lba		[4];

	uchar reserved1		[2];

	uchar max_desc		[2];

	uchar type;
	uchar ctl;
};

#endif

typedef struct _BraseroGetPerformanceCDB BraseroGetPerformanceCDB;

BRASERO_SCSI_COMMAND_DEFINE (BraseroGetPerformanceCDB,
			     GET_PERFORMANCE,
			     BRASERO_SCSI_READ);

/* used to choose which GET PERFORMANCE response we want */
#define BRASERO_GET_PERFORMANCE_PERF_TYPE		0x00
#define BRASERO_GET_PERFORMANCE_UNUSABLE_AREA_TYPE	0x01
#define BRASERO_GET_PERFORMANCE_DEFECT_STATUS_TYPE	0x02
#define BRASERO_GET_PERFORMANCE_WR_SPEED_TYPE		0x03
#define BRASERO_GET_PERFORMANCE_DBI_TYPE		0x04
#define BRASERO_GET_PERFORMANCE_DBI_CACHE_TYPE		0x05

static BraseroScsiGetPerfData *
brasero_get_performance_get_buffer (BraseroGetPerformanceCDB *cdb,
				    gint sizeof_descriptors,
				    BraseroScsiGetPerfHdr *hdr,
				    BraseroScsiErrCode *error)
{
	BraseroScsiGetPerfData *buffer;
	BraseroScsiResult res;
	int request_size;
	int desc_num;

	/* ... check the request size ... */
	request_size = BRASERO_GET_32 (hdr->len) +
		       G_STRUCT_OFFSET (BraseroScsiGetPerfHdr, len) +
		       sizeof (hdr->len);

	/* ... check the request size ... */
	if (request_size > 2048) {
		BRASERO_MEDIA_LOG ("Oversized data (%i) setting to max (2048)", request_size);
		request_size = 2048;
	}
	else if ((request_size - sizeof (hdr)) % sizeof_descriptors) {
		BRASERO_MEDIA_LOG ("Unaligned data (%i) setting to max (2048)", request_size);
		request_size = 2048;
	}
	else if (request_size < sizeof (hdr)) {
		BRASERO_MEDIA_LOG ("Undersized data (%i) setting to max (2048)", request_size);
		request_size = 2048;
	}

	desc_num = (request_size - sizeof (BraseroScsiGetPerfHdr)) / sizeof_descriptors;

	/* ... allocate a buffer and re-issue the command */
	buffer = (BraseroScsiGetPerfData *) g_new0 (uchar, request_size);

	BRASERO_SET_16 (cdb->max_desc, desc_num);
	res = brasero_scsi_command_issue_sync (cdb, buffer, request_size, error);
	if (res) {
		g_free (buffer);
		return NULL;
	}

	return buffer;
}

static BraseroScsiResult
brasero_get_performance (BraseroGetPerformanceCDB *cdb,
			 gint sizeof_descriptors,
			 BraseroScsiGetPerfData **data,
			 int *data_size,
			 BraseroScsiErrCode *error)
{
	BraseroScsiGetPerfData *buffer;
	BraseroScsiGetPerfHdr hdr;
	BraseroScsiResult res;
	int request_size;
	int buffer_size;

	if (!data || !data_size) {
		BRASERO_SCSI_SET_ERRCODE (error, BRASERO_SCSI_BAD_ARGUMENT);
		return BRASERO_SCSI_FAILURE;
	}

	/* Issue the command once to get the size ... */
	memset (&hdr, 0, sizeof (hdr));
	BRASERO_SET_16 (cdb->max_desc, 0);
	res = brasero_scsi_command_issue_sync (cdb, &hdr, sizeof (hdr), error);
	if (res != BRASERO_SCSI_OK)
		return res;

	/* ... get the request size ... */
	request_size = BRASERO_GET_32 (hdr.len) +
		       G_STRUCT_OFFSET (BraseroScsiGetPerfHdr, len) +
		       sizeof (hdr.len);

	/* ... get the answer itself. */
	buffer = brasero_get_performance_get_buffer (cdb,
						     sizeof_descriptors,
						     &hdr,
						     error);
	if (!buffer)
		return BRASERO_SCSI_FAILURE;

	/* make sure the response has the requested size */
	buffer_size = BRASERO_GET_32 (buffer->hdr.len) +
		      G_STRUCT_OFFSET (BraseroScsiGetPerfHdr, len) +
		      sizeof (buffer->hdr.len);

	if (request_size < buffer_size) {
		BraseroScsiGetPerfData *new_buffer;

		/* Strangely some drives returns a buffer size that is bigger
		 * than the one they returned on the first time. So redo whole
		 * operation again but this time with the new size we got */
		BRASERO_MEDIA_LOG ("Sizes mismatch asked %i / received %i\n"
				   "Re-issuing the command with received size",
				   request_size,
				   buffer_size);

		/* Try to get a new buffer of the new size */
		memcpy (&hdr, &buffer->hdr, sizeof (hdr));
		new_buffer = brasero_get_performance_get_buffer (cdb,
		                                         	 sizeof_descriptors,
		                                                 &hdr,
		                                                 error);
		if (new_buffer) {
			g_free (buffer);
			buffer = new_buffer;

			request_size = buffer_size;
			buffer_size = BRASERO_GET_32 (buffer->hdr.len) +
				      G_STRUCT_OFFSET (BraseroScsiGetPerfHdr, len) +
				      sizeof (buffer->hdr.len);
		}
	}
	else if (request_size > buffer_size)
		BRASERO_MEDIA_LOG ("Sizes mismatch asked %i / received %i",
				   request_size,
				   buffer_size);
	*data = buffer;
	*data_size = MIN (buffer_size, request_size);

	return res;
}

/**
 * MMC3 command extension
 */

BraseroScsiResult
brasero_mmc3_get_performance_wrt_spd_desc (BraseroDeviceHandle *handle,
					   BraseroScsiGetPerfData **data,
					   int *size,
					   BraseroScsiErrCode *error)
{
	BraseroGetPerformanceCDB *cdb;
	BraseroScsiResult res;

	g_return_val_if_fail (handle != NULL, BRASERO_SCSI_FAILURE);

	cdb = brasero_scsi_command_new (&info, handle);
	cdb->type = BRASERO_GET_PERFORMANCE_WR_SPEED_TYPE;

	res = brasero_get_performance (cdb, sizeof (BraseroScsiWrtSpdDesc), data, size, error);
	brasero_scsi_command_free (cdb);
	return res;
}

