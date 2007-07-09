/***************************************************************************
 *            scsi-read-track-information.c
 *
 *  Fri Oct 27 07:12:07 2006
 *  Copyright  2006  algernon
 *  <algernon@localhost.localdomain>
 ****************************************************************************/

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
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
			     O_RDONLY,
			     BRASERO_SCSI_READ);

typedef enum {
BRASERO_FIELD_LBA			= 0x00,
BRASERO_FIELD_TRACK_NUM			= 0x01,
BRASERO_FIELD_SESSION_NUM		= 0x02,
	/* reserved */
} BraseroFieldType;

#define BRASERO_SCSI_INCOMPLETE_TRACK	0xFF

/**
 * 
 * NOTE: if media is a CD and track_num = 0 then returns leadin
 * but since the other media don't have a leadin they error out.
 * if track_num = 255 returns last incomplete track.
 */
 
BraseroScsiResult
brasero_mmc1_read_track_info (int fd,
			      int track_num,
			      BraseroScsiTrackInfo *track_info,
			      int size,
			      BraseroScsiErrCode *error)
{
	BraseroRdTrackInfoCDB *cdb;
	BraseroScsiResult res;

	cdb = brasero_scsi_command_new (&info, fd);
	cdb->addr_num_type = BRASERO_FIELD_TRACK_NUM;
	BRASERO_SET_32 (cdb->blk_addr_trk_ses_num, track_num);
	BRASERO_SET_16 (cdb->alloc_len, size);

	memset (track_info, 0, sizeof (BraseroScsiTrackInfo));
	res = brasero_scsi_command_issue_sync (cdb, track_info, size, error);
	brasero_scsi_command_free (cdb);
	return res;
}

BraseroScsiResult
brasero_mmc1_read_track_info_for_block (int fd,
					int block,
					BraseroScsiTrackInfo *track_info,
					int size,
					BraseroScsiErrCode *error)
{
	BraseroRdTrackInfoCDB *cdb;
	BraseroScsiResult res;

	cdb = brasero_scsi_command_new (&info, fd);

	cdb->addr_num_type = BRASERO_FIELD_LBA;
	BRASERO_SET_32 (cdb->blk_addr_trk_ses_num, block);
	BRASERO_SET_16 (cdb->alloc_len, size);

	memset (track_info, 0, sizeof (BraseroScsiTrackInfo));
	res = brasero_scsi_command_issue_sync (cdb, track_info, size, error);
	brasero_scsi_command_free (cdb);
	return res;
}

BraseroScsiResult
brasero_mmc1_read_session_first_track_info (int fd,
					    int session,
					    BraseroScsiTrackInfo *track_info,
					    int size,
					    BraseroScsiErrCode *error)
{
	BraseroRdTrackInfoCDB *cdb;
	BraseroScsiResult res;

	cdb = brasero_scsi_command_new (&info, fd);

	BRASERO_SET_32 (cdb->blk_addr_trk_ses_num, session);
	cdb->addr_num_type = BRASERO_FIELD_SESSION_NUM;
	BRASERO_SET_16 (cdb->alloc_len, size);

	memset (track_info, 0, sizeof (BraseroScsiTrackInfo));
	res = brasero_scsi_command_issue_sync (cdb, track_info, size, error);
	brasero_scsi_command_free (cdb);
	return res;
}

BraseroScsiResult
brasero_mmc1_read_first_open_session_track_info (int fd,
						 BraseroScsiTrackInfo *track_info,
						 int size,
						 BraseroScsiErrCode *error)
{
	BraseroRdTrackInfoCDB *cdb;
	BraseroScsiResult res;

	cdb = brasero_scsi_command_new (&info, fd);
	cdb->addr_num_type = BRASERO_FIELD_TRACK_NUM;
	BRASERO_SET_32 (cdb->blk_addr_trk_ses_num, BRASERO_SCSI_INCOMPLETE_TRACK);
	BRASERO_SET_16 (cdb->alloc_len, size);

	memset (track_info, 0, sizeof (BraseroScsiTrackInfo));
	res = brasero_scsi_command_issue_sync (cdb, track_info, size, error);
	brasero_scsi_command_free (cdb);
	return res;
}

BraseroScsiResult
brasero_mmc5_read_first_open_session_track_info (int fd,
						 BraseroScsiTrackInfo *track_info,
						 int size,
						 BraseroScsiErrCode *error)
{
	BraseroRdTrackInfoCDB *cdb;
	BraseroScsiResult res;

	cdb = brasero_scsi_command_new (&info, fd);
	cdb->open = 1;
	cdb->addr_num_type = BRASERO_FIELD_TRACK_NUM;
	BRASERO_SET_32 (cdb->blk_addr_trk_ses_num, 1);
	BRASERO_SET_16 (cdb->alloc_len, size);

	memset (track_info, 0, sizeof (BraseroScsiTrackInfo));
	res = brasero_scsi_command_issue_sync (cdb, track_info, size, error);
	brasero_scsi_command_free (cdb);
	return res;
}
