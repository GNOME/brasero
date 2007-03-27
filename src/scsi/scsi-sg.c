/***************************************************************************
 *            burn-sg.c
 *
 *  Wed Oct 18 14:39:28 2006
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

#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <scsi/scsi.h>
#include <scsi/sg.h>

#include "scsi-sg.h"
#include "scsi-utils.h"
#include "scsi-error.h"
#include "scsi-sense-data.h"

BraseroScsiResult
brasero_sg_send_command (int fd, struct sg_io_hdr *command, BraseroScsiErrCode *error)
{
	unsigned char sense_buffer [32];
	BraseroScsiErrCode res;

	/* NOTE: the sense buffer is the result of the first write command
	 * (REQUEST SENSE usually) since SG_IO is both write and read. */
	memset (&sense_buffer, sizeof (sense_buffer), 0);

	command->interface_id = 'S';				/* mandatory */
	command->flags = SG_FLAG_LUN_INHIBIT|SG_FLAG_DIRECT_IO;

	/* where to output the scsi sense buffer */
	command->sbp = sense_buffer;
	command->mx_sb_len = sizeof (sense_buffer);

	/* NOTE on SG_IO: only for TEST UNIT READY, REQUEST/MODE SENSE, INQUIRY,
	 * READ CAPACITY, READ BUFFER, READ and LOG SENSE are allowed with it */
	res = ioctl (fd, SG_IO, command);
	if (res) {
		BRASERO_SCSI_SET_ERRCODE (error, BRASERO_SCSI_ERRNO);
		return BRASERO_SCSI_FAILURE;
	}

	if ((command->info & SG_INFO_OK_MASK) == SG_INFO_OK)
		return BRASERO_SCSI_OK;

	if ((command->masked_status & CHECK_CONDITION) && command->sb_len_wr)
		return brasero_sense_data_process (sense_buffer, &res);

	return BRASERO_SCSI_FAILURE;
}
