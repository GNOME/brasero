/***************************************************************************
 *            scsi-read-format-capacities.c
 *
 *  Thu Nov  2 12:33:40 2006
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>

#include "scsi-error.h"
#include "scsi-utils.h"
#include "scsi-base.h"
#include "scsi-command.h"
#include "scsi-opcodes.h"
#include "scsi-read-format-capacities.h"

struct _BraseroRdFormatCapacitiesCDB {
	uchar opcode;
	uchar reserved		[6];
	uchar alloc_len		[2];
	uchar ctl;
};

typedef struct _BraseroRdFormatCapacitiesCDB BraseroRdFormatCapacitiesCDB;

BRASERO_SCSI_COMMAND_DEFINE (BraseroRdFormatCapacitiesCDB,
			     READ_FORMAT_CAPACITIES,
			     BRASERO_SCSI_READ);

BraseroScsiResult
brasero_mmc2_read_format_capacities (BraseroDeviceHandle *handle,
				     BraseroScsiFormatCapacitiesHdr **data,
				     int *size,
				     BraseroScsiErrCode *error)
{
	BraseroScsiFormatCapacitiesHdr *buffer;
	BraseroScsiFormatCapacitiesHdr hdr;
	BraseroRdFormatCapacitiesCDB *cdb;
	BraseroScsiResult res;
	int request_size;

	if (!data || !size) {
		BRASERO_SCSI_SET_ERRCODE (error, BRASERO_SCSI_BAD_ARGUMENT);
		return BRASERO_SCSI_FAILURE;
	}

	cdb = brasero_scsi_command_new (&info, handle);
	BRASERO_SET_16 (cdb->alloc_len, sizeof (BraseroScsiFormatCapacitiesHdr));

	memset (&hdr, 0, sizeof (BraseroScsiFormatCapacitiesHdr));
	res = brasero_scsi_command_issue_sync (cdb,
					       &hdr,
					       sizeof (BraseroScsiFormatCapacitiesHdr),
					       error);
	if (res)
		goto end;

	request_size = hdr.len + sizeof (hdr.len) + G_STRUCT_OFFSET (BraseroScsiFormatCapacitiesHdr, len);
	buffer = (BraseroScsiFormatCapacitiesHdr *) g_new0 (uchar, request_size);

	BRASERO_SET_16 (cdb->alloc_len, request_size);
	res = brasero_scsi_command_issue_sync (cdb, buffer, request_size, error);
	if (res) {
		g_free (buffer);
		goto end;
	}

	if (request_size != buffer->len + sizeof (buffer->len) + G_STRUCT_OFFSET (BraseroScsiFormatCapacitiesHdr, len)) {
		BRASERO_SCSI_SET_ERRCODE (error, BRASERO_SCSI_SIZE_MISMATCH);

		res = BRASERO_SCSI_FAILURE;
		g_free (buffer);
		goto end;
	}

	*data = buffer;
	*size = request_size;

end:

	brasero_scsi_command_free (cdb);
	return res;
}
