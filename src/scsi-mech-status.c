/***************************************************************************
 *            scsi-mech-status.c
 *
 *  Mon Mar  3 17:56:00 2008
 *  Copyright  2008  Philippe Rouquier
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

#include <glib.h>

#include "scsi-error.h"
#include "scsi-utils.h"
#include "scsi-base.h"
#include "scsi-command.h"
#include "scsi-opcodes.h"
#include "scsi-mech-status.h"

struct _BraseroScsiMechStatusCDB {
	uchar opcode;

	uchar res1		[7];
	uchar len		[2];
	uchar res2;

	uchar ctl;
};

typedef struct _BraseroScsiMechStatusCDB BraseroScsiMechStatusCDB;

BRASERO_SCSI_COMMAND_DEFINE (BraseroScsiMechStatusCDB,
			     MECH_STATUS,
			     O_RDONLY,
			     BRASERO_SCSI_READ);

BraseroScsiResult
brasero_mmc1_mech_status (BraseroDeviceHandle *handle,
			  BraseroScsiMechStatusHdr *hdr,
			  BraseroScsiErrCode *error)
{
	BraseroScsiMechStatusCDB *cdb;
	BraseroScsiResult res;

	cdb = brasero_scsi_command_new (&info, handle);
	BRASERO_SET_16 (cdb->len, sizeof (BraseroScsiMechStatusHdr));

	memset (hdr, 0, sizeof (BraseroScsiMechStatusHdr));
	res = brasero_scsi_command_issue_sync (cdb,
					       hdr,
					       sizeof (BraseroScsiMechStatusHdr),
					       error);
	brasero_scsi_command_free (cdb);
	return res;
}
