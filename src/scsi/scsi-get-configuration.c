/***************************************************************************
 *            scsi-get-configuration.c
 *
 *  Wed Oct 25 21:34:34 2006
 *  Copyright  2006  Rouquier Philippe
 *  <Rouquier Philippe@localhost.localdomain>
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
			     O_RDONLY,
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

	memset (&hdr, 0, sizeof (hdr));

	BRASERO_SET_16 (cdb->alloc_len, sizeof (hdr));
	res = brasero_scsi_command_issue_sync (cdb, &hdr, sizeof (hdr), error);
	if (res)
		return res;

	/* ... allocate a buffer and re-issue the command */
	request_size = BRASERO_GET_32 (hdr.len) +
		       G_STRUCT_OFFSET (BraseroScsiGetConfigHdr, len) +
		       sizeof (hdr.len);

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

	if (buffer_size != request_size) {
		g_free (buffer);

		BRASERO_SCSI_SET_ERRCODE (error, BRASERO_SCSI_SIZE_MISMATCH);
		return BRASERO_SCSI_FAILURE;
	}

	*data = buffer;
	*size = request_size;

	return BRASERO_SCSI_OK;
}

BraseroScsiResult
brasero_mmc2_get_configuration_feature (int fd,
					BraseroScsiFeatureType type,
					BraseroScsiGetConfigHdr **data,
					int *size,
					BraseroScsiErrCode *error)
{
	BraseroGetConfigCDB *cdb;
	BraseroScsiResult res;

	cdb = brasero_scsi_command_new (&info, fd);
	BRASERO_SET_16 (cdb->feature_num, type);
	cdb->returned_data = BRASERO_GET_CONFIG_RETURN_ONLY_FEATURE;

	res = brasero_get_configuration (cdb, data, size, error);
	brasero_scsi_command_free (cdb);
	return res;
}

BraseroScsiResult
brasero_mmc2_get_configuration_all_features (int fd,
					     int only_current,
					     BraseroScsiGetConfigHdr **data,
					     int *size,
					     BraseroScsiErrCode *error)
{
	BraseroGetConfigCDB *cdb;
	BraseroScsiResult res;

	cdb = brasero_scsi_command_new (&info, fd);
	BRASERO_SET_16 (cdb->feature_num, BRASERO_SCSI_FEAT_PROFILES);

	if (only_current)
		cdb->returned_data = BRASERO_GET_CONFIG_RETURN_ALL_CURRENT;
	else
		cdb->returned_data = BRASERO_GET_CONFIG_RETURN_ALL_FEATURES;

	res = brasero_get_configuration (cdb, data, size, error);
	brasero_scsi_command_free (cdb);
	return res;
}
