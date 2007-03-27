/***************************************************************************
 *            scsi-read-disc-info.c
 *
 *  Thu Oct 26 16:51:36 2006
 *  Copyright  2006  Rouquier Philippe
 *  <bonfire-app@wanadoo.fr>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>

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
			     O_RDONLY,
			     BRASERO_SCSI_READ);

typedef enum {
BRASERO_DISC_INFO_STD		= 0x00,
BRASERO_DISC_INFO_TRACK_RES	= 0x01,
BRASERO_DISC_INFO_POW_RES	= 0x02,
	/* reserved */
} BraseroDiscInfoType;


BraseroScsiResult
brasero_mmc1_read_disc_information_std (int fd,
					BraseroScsiDiscInfoStd **info_return,
					int *size,
					BraseroScsiErrCode *error)
{
	BraseroScsiDiscInfoStd std_info;
	BraseroScsiDiscInfoStd *buffer;
	BraseroRdDiscInfoCDB *cdb;
	BraseroScsiResult res;
	int request_size;

	if (!info_return || !size) {
		BRASERO_SCSI_SET_ERRCODE (error, BRASERO_SCSI_BAD_ARGUMENT);
		return BRASERO_SCSI_FAILURE;
	}

	cdb = brasero_scsi_command_new (&info, fd);
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

	if (request_size != BRASERO_GET_16 (buffer->len) + sizeof (buffer->len)) {
		BRASERO_SCSI_SET_ERRCODE (error, BRASERO_SCSI_SIZE_MISMATCH);

		res = BRASERO_SCSI_FAILURE;
		g_free (buffer);
		goto end;
	}

	*info_return = buffer;
	*size = request_size;

end:

	brasero_scsi_command_free (cdb);
	return res;
}

BraseroScsiResult
brasero_mmc5_read_disc_information_tracks (int fd,
					   BraseroScsiTrackResInfo *info_return,
					   int size,
					   BraseroScsiErrCode *error)
{
	BraseroRdDiscInfoCDB *cdb;
	BraseroScsiResult res;

	cdb = brasero_scsi_command_new (&info, fd);
	cdb->data_type = BRASERO_DISC_INFO_TRACK_RES;
	BRASERO_SET_16 (cdb->alloc_len, size);

	memset (info_return, 0, size);
	res = brasero_scsi_command_issue_sync (cdb, info_return, size, error);
	brasero_scsi_command_free (cdb);
	return res;
}

BraseroScsiResult
brasero_mmc5_read_disc_information_pows (int fd,
					 BraseroScsiPOWResInfo *info_return,
					 int size,
					 BraseroScsiErrCode *error)
{
	BraseroRdDiscInfoCDB *cdb;
	BraseroScsiResult res;

	cdb = brasero_scsi_command_new (&info, fd);
	cdb->data_type = BRASERO_DISC_INFO_POW_RES;
	BRASERO_SET_16 (cdb->alloc_len, size);

	memset (info_return, 0, size);
	res = brasero_scsi_command_issue_sync (cdb, info_return, size, error);
	brasero_scsi_command_free (cdb);
	return res;
}
