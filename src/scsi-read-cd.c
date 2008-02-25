/***************************************************************************
 *            scsi-read-cd.c
 *
 *  Sun Jan 27 20:39:40 2008
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
#include "scsi-read-cd.h"

#if G_BYTE_ORDER == G_LITTLE_ENDIAN

struct _BraseroReadCDCDB {
	uchar opcode;

	uchar rel_add		:1;
	uchar reserved1		:1;
	uchar sec_type		:3;
	uchar reserved0		:3;

	uchar start_lba		[4];
	uchar len		[3];

	uchar reserved2		:1;
	uchar error		:2;
	uchar edc		:1;
	uchar user_data		:1;
	uchar header		:2;
	uchar sync		:1;

	uchar subchannel	:3;
	uchar reserved3		:5;

	uchar ctl;
};

#else

struct _BraseroReadCDCDB {
	uchar opcode;

	uchar reserved0		:3;
	uchar sec_type		:3;
	uchar reserved1		:1;
	uchar rel_add		:1;

	uchar start_lba		[4];
	uchar len		[3];

	uchar sync		:1;
	uchar header		:2;
	uchar user_data		:1;
	uchar edc		:1;
	uchar error		:2;
	uchar reserved2		:1;

	uchar reserved3		:5;
	uchar subchannel	:3;

	uchar ctl;
};

#endif

typedef struct _BraseroReadCDCDB BraseroReadCDCDB;

BRASERO_SCSI_COMMAND_DEFINE (BraseroReadCDCDB,
			     READ_CD,
			     O_RDONLY,
			     BRASERO_SCSI_READ);

BraseroScsiResult
brasero_mmc1_read_block (BraseroDeviceHandle *handle,
			 gboolean user_data,
			 BraseroScsiBlockType type,
			 BraseroScsiBlockHeader header,
			 BraseroScsiBlockSubChannel channel,
			 int start,
			 int size,
			 unsigned char *buffer,
			 int buffer_len,
			 BraseroScsiErrCode *error)
{
	BraseroReadCDCDB *cdb;
	BraseroScsiResult res;

	cdb = brasero_scsi_command_new (&info, handle);
	BRASERO_SET_32 (cdb->start_lba, start);

	/* NOTE: if we just want to test if block is readable len can be 0 */
	BRASERO_SET_24 (cdb->len, size);

	/* reladr should be O */
	/* no sync field included */
	cdb->sync = 0;

	/* no filtering */
	cdb->sec_type = type;

	/* header ?*/
	cdb->header = header;

	/* returns user data ?*/
	cdb->user_data = user_data;

	/* no EDC */
	/* no error/C2 error */

	/* subchannel */
	cdb->subchannel = channel;

	if (buffer)
		memset (buffer, 0, buffer_len);

	res = brasero_scsi_command_issue_sync (cdb,
					       buffer,
					       buffer_len,
					       error);
	brasero_scsi_command_free (cdb);
	return res;
}
