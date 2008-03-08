/***************************************************************************
 *            scsi-eject.c
 *
 *  Mon Mar  3 17:40:24 2008
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

#if G_BYTE_ORDER == G_LITTLE_ENDIAN

struct _BraseroLoadCDCDB {
	uchar opcode;

	uchar immed		:1;
	uchar res2		:4;
	uchar res1		:3;

	uchar res3		[2];

	uchar start		:1;
	uchar load		:1;
	uchar res4		:6;

	uchar res5		[3];
	uchar slot;
	uchar res6		[2];

	uchar ctl;
};

#else

struct _BraseroLoadCDCDB {
	uchar opcode;

	uchar res1		:3;
	uchar res2		:4;
	uchar immed		:1;

	uchar res3		[2];

	uchar res4		:6;
	uchar load		:1;
	uchar start		:1;

	uchar res5		[3];
	uchar slot;
	uchar res6		[2];

	uchar ctl;
};

#endif

typedef struct _BraseroLoadCDCDB BraseroLoadCDCDB;

BRASERO_SCSI_COMMAND_DEFINE (BraseroLoadCDCDB,
			     LOAD_CD,
			     O_RDONLY,
			     BRASERO_SCSI_READ);

BraseroScsiResult
brasero_mmc1_load_cd (BraseroDeviceHandle *handle,
		      BraseroScsiErrCode *error)
{
	BraseroLoadCDCDB *cdb;
	BraseroScsiResult res;

	cdb = brasero_scsi_command_new (&info, handle);
	res = brasero_scsi_command_issue_sync (cdb,
					       NULL,
					       0,
					       error);
	brasero_scsi_command_free (cdb);
	return res;
}
