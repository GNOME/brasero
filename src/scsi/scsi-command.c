/***************************************************************************
 *            burn-scsi-command.c
 *
 *  Thu Oct 19 18:04:55 2006
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

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <scsi/scsi.h>
#include <scsi/sg.h>

#include "scsi-error.h"
#include "scsi-utils.h"
#include "scsi-base.h"
#include "scsi-command.h"
#include "scsi-sense-data.h"

#define BRASERO_SCSI_CMD_OPCODE_OFF			0
#define BRASERO_SCSI_CMD_SET_OPCODE(command)		(command->cmd [BRASERO_SCSI_CMD_OPCODE_OFF] = command->info->opcode)

struct _BraseroScsiCmd {
	uchar cmd [BRASERO_SCSI_CMD_MAX_LEN];
	int fd;

	const BraseroScsiCmdInfo *info;
};
typedef struct _BraseroScsiCmd BraseroScsiCmd;

gpointer
brasero_scsi_command_new (const BraseroScsiCmdInfo *info, int fd) 
{
	BraseroScsiCmd *cmd;

	/* make sure we can set the flags of the descriptor */

	/* allocate the command */
	cmd = g_new0 (BraseroScsiCmd, 1);
	cmd->info = info;
	cmd->fd = fd;

	BRASERO_SCSI_CMD_SET_OPCODE (cmd);
	return cmd;
}

BraseroScsiResult
brasero_scsi_command_free (gpointer cmd)
{
	g_free (cmd);
	return BRASERO_SCSI_OK;
}

static void
brasero_sg_command_setup (struct sg_io_hdr *transport,
			  uchar *sense_data,
			  BraseroScsiCmd *cmd,
			  uchar *buffer,
			  int size)
{
	memset (sense_data, 0, BRASERO_SENSE_DATA_SIZE);
	memset (transport, 0, sizeof (struct sg_io_hdr));
	
	transport->interface_id = 'S';				/* mandatory */
//	transport->flags = SG_FLAG_LUN_INHIBIT|SG_FLAG_DIRECT_IO;
	transport->cmdp = cmd->cmd;
	transport->cmd_len = cmd->info->size;
	transport->dxferp = buffer;
	transport->dxfer_len = size;

	/* where to output the scsi sense buffer */
	transport->sbp = sense_data;
	transport->mx_sb_len = BRASERO_SENSE_DATA_SIZE;

	if (cmd->info->direction & BRASERO_SCSI_READ)
		transport->dxfer_direction = SG_DXFER_FROM_DEV;
	else if (cmd->info->direction & BRASERO_SCSI_WRITE)
		transport->dxfer_direction = SG_DXFER_TO_DEV;
}

BraseroScsiResult
brasero_scsi_command_issue_sync (gpointer command,
				 gpointer buffer,
				 int size,
				 BraseroScsiErrCode *error)
{
	uchar sense_buffer [BRASERO_SENSE_DATA_SIZE];
	struct sg_io_hdr transport;
	BraseroScsiResult res;
	BraseroScsiCmd *cmd;

	cmd = command;
	brasero_sg_command_setup (&transport,
				  sense_buffer,
				  cmd,
				  buffer,
				  size);

	/* for the time being only sg driver is supported */

	/* NOTE on SG_IO: only for TEST UNIT READY, REQUEST/MODE SENSE, INQUIRY,
	 * READ CAPACITY, READ BUFFER, READ and LOG SENSE are allowed with it */
	res = ioctl (cmd->fd, SG_IO, &transport);
	if (res) {
		BRASERO_SCSI_SET_ERRCODE (error, BRASERO_SCSI_ERRNO);
		return BRASERO_SCSI_FAILURE;
	}

	if ((transport.info & SG_INFO_OK_MASK) == SG_INFO_OK)
		return BRASERO_SCSI_OK;

	if ((transport.masked_status & CHECK_CONDITION) && transport.sb_len_wr)
		return brasero_sense_data_process (sense_buffer, error);

	return BRASERO_SCSI_FAILURE;
}

BraseroScsiResult
brasero_scsi_command_issue_immediate (gpointer command,
				      gpointer buffer,
				      int size,
				      BraseroScsiErrCode *error)
{
	uchar sense_buffer [BRASERO_SENSE_DATA_SIZE];
	struct sg_io_hdr transport;
	BraseroScsiCmd *cmd;
	int count;

	cmd = command;
	brasero_sg_command_setup (&transport,
				  sense_buffer,
				  cmd,
				  buffer,
				  size);
g_print ("eeee %p\n", transport.dxferp);
transport.pack_id = 7;
	transport.timeout = 2000;
	do {
		count = write (cmd->fd, &transport, sizeof (struct sg_io_hdr));
	} while (count != sizeof (struct sg_io_hdr) && errno == EAGAIN);

	if (count != sizeof (struct sg_io_hdr)) {
		BRASERO_SCSI_SET_ERRCODE (error, BRASERO_SCSI_ERRNO);
		return BRASERO_SCSI_FAILURE;
	}

//	memset (&transport, 0, sizeof (struct sg_io_hdr));
	do {
		count = read (cmd->fd, &transport, sizeof (struct sg_io_hdr));
	} while (count != sizeof (struct sg_io_hdr) && errno == EAGAIN);
g_print ("aaaa %p\n", transport.dxferp);

	if (count != sizeof (struct sg_io_hdr)) {
		BRASERO_SCSI_SET_ERRCODE (error, BRASERO_SCSI_ERRNO);
		return BRASERO_SCSI_FAILURE;
	}

	if ((transport.info & SG_INFO_OK_MASK) == SG_INFO_OK)
		return BRASERO_SCSI_OK;

	if ((transport.masked_status & CHECK_CONDITION) && transport.sb_len_wr)
		return brasero_sense_data_process (sense_buffer, error);

	return BRASERO_SCSI_FAILURE;
}
