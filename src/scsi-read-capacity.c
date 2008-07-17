/***************************************************************************
 *            scsi-read-capacity.c
 *
 *  Sat Oct 28 12:01:52 2006
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
#include "scsi-base.h"
#include "scsi-command.h"
#include "scsi-opcodes.h"
#include "scsi-read-capacity.h"

#if G_BYTE_ORDER == G_LITTLE_ENDIAN

struct _BraseroReadCapacityCDB {
	uchar opcode;

	uchar relative_address		:1;
	uchar reserved0			:7;

	uchar lba			[4];
	uchar reserved1			[2];

	uchar pmi			:1;
	uchar reserved2			:7;

	uchar ctl;
};

#else

struct _BraseroReadCapacityCDB {
	uchar opcode;

	uchar reserved0			:7;
	uchar relative_address		:1;

	uchar lba			[4];
	uchar reserved1			[2];

	uchar reserved2			:7;
	uchar pmi			:1;

	uchar ctl;
};

#endif

typedef struct _BraseroReadCapacityCDB BraseroReadCapacityCDB;

BRASERO_SCSI_COMMAND_DEFINE (BraseroReadCapacityCDB,
			     READ_CAPACITY,
			     O_RDONLY,
			     BRASERO_SCSI_READ);

BraseroScsiResult
brasero_mmc2_read_capacity (BraseroDeviceHandle *handle,
			    BraseroScsiReadCapacityData *data,
			    int size,
			    BraseroScsiErrCode *error)
{
	BraseroReadCapacityCDB *cdb;
	BraseroScsiResult res;

	/* NOTE: all the fields are ignored by MM drives */
	cdb = brasero_scsi_command_new (&info, handle);

	memset (data, 0, size);
	res = brasero_scsi_command_issue_sync (cdb, data, size, error);
	brasero_scsi_command_free (cdb);

	return res;
}
