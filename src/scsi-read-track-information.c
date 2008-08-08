/***************************************************************************
 *            scsi-read-track-information.c
 *
 *  Fri Oct 27 07:12:07 2006
 *  Copyright  2006  Rouquier Philippe
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

#include <glib.h>

#include "burn-debug.h"

#include "scsi-error.h"
#include "scsi-utils.h"
#include "scsi-base.h"
#include "scsi-command.h"
#include "scsi-opcodes.h"
#include "scsi-read-track-information.h"

#if G_BYTE_ORDER == G_LITTLE_ENDIAN

struct _BraseroRdTrackInfoCDB {
	uchar opcode;

	uchar addr_num_type		:2;
	uchar open			:1;	/* MMC5 field only */
	uchar reserved0			:5;

	uchar blk_addr_trk_ses_num	[4];

	uchar reserved1;

	uchar alloc_len			[2];
	uchar ctl;
};

#else

struct _BraseroRdTrackInfoCDB {
	uchar opcode;

	uchar reserved0			:5;
	uchar open			:1;
	uchar addr_num_type		:2;

	uchar blk_addr_trk_ses_num	[4];

	uchar reserved1;

	uchar alloc_len			[2];
	uchar ctl;
};

#endif

typedef struct _BraseroRdTrackInfoCDB BraseroRdTrackInfoCDB;

BRASERO_SCSI_COMMAND_DEFINE (BraseroRdTrackInfoCDB,
			     READ_TRACK_INFORMATION,
			     BRASERO_SCSI_READ);

typedef enum {
BRASERO_FIELD_LBA			= 0x00,
BRASERO_FIELD_TRACK_NUM			= 0x01,
BRASERO_FIELD_SESSION_NUM		= 0x02,
	/* reserved */
} BraseroFieldType;

#define BRASERO_SCSI_INCOMPLETE_TRACK	0xFF

static BraseroScsiResult
brasero_read_track_info (BraseroRdTrackInfoCDB *cdb,
			 BraseroScsiTrackInfo *info,
			 int *size,
			 BraseroScsiErrCode *error)
{
	BraseroScsiTrackInfo hdr;
	BraseroScsiResult res;
	int datasize;

	if (!info || !size) {
		BRASERO_SCSI_SET_ERRCODE (error, BRASERO_SCSI_BAD_ARGUMENT);
		return BRASERO_SCSI_FAILURE;
	}

	/* first ask the drive how long should the data be and then ... */
	datasize = 4;
	memset (&hdr, 0, sizeof (info));
	BRASERO_SET_16 (cdb->alloc_len, datasize);
	res = brasero_scsi_command_issue_sync (cdb, &hdr, datasize, error);
	if (res)
		return res;

	/* ... check the size in case of a buggy firmware ... */
	if (BRASERO_GET_16 (hdr.len) + sizeof (hdr.len) >= datasize) {
		datasize = BRASERO_GET_16 (hdr.len) + sizeof (hdr.len);

		if (datasize > *size) {
			BRASERO_BURN_LOG ("Oversized data received (%i) setting to %i", datasize, *size);
			datasize = *size;
		}
		else if (*size < datasize) {
			BRASERO_BURN_LOG ("Oversized data required (%i) setting to %i", *size, datasize);
			*size = datasize;
		}
	}
	else {
		BRASERO_BURN_LOG ("Undersized data received (%i) setting to %i", datasize, *size);
		datasize = *size;
	}

	/* ... and re-issue the command */
	memset (info, 0, sizeof (BraseroScsiTrackInfo));
	BRASERO_SET_16 (cdb->alloc_len, datasize);
	res = brasero_scsi_command_issue_sync (cdb, info, datasize, error);

	if (!res) {
		if (datasize != BRASERO_GET_16 (info->len) + sizeof (info->len))
			BRASERO_BURN_LOG ("Sizes mismatch asked %i / received %i",
					  datasize,
					  BRASERO_GET_16 (info->len) + sizeof (info->len));

		*size = MIN (datasize, BRASERO_GET_16 (info->len) + sizeof (info->len));
	}

	return res;
}

/**
 * 
 * NOTE: if media is a CD and track_num = 0 then returns leadin
 * but since the other media don't have a leadin they error out.
 * if track_num = 255 returns last incomplete track.
 */
 
BraseroScsiResult
brasero_mmc1_read_track_info (BraseroDeviceHandle *handle,
			      int track_num,
			      BraseroScsiTrackInfo *track_info,
			      int *size,
			      BraseroScsiErrCode *error)
{
	BraseroRdTrackInfoCDB *cdb;
	BraseroScsiResult res;

	cdb = brasero_scsi_command_new (&info, handle);
	cdb->addr_num_type = BRASERO_FIELD_TRACK_NUM;
	BRASERO_SET_32 (cdb->blk_addr_trk_ses_num, track_num);

	res = brasero_read_track_info (cdb, track_info, size, error);
	brasero_scsi_command_free (cdb);

	return res;
}
