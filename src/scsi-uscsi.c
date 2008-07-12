/***************************************************************************
 *            scsi-uscsi.c
 *
 *  Wed Oct 18 14:39:28 2008
 *  Copyright  2008  Lin Ma
 *  <lin.ma@sun.com>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include <sys/scsi/scsi.h>
#include <sys/scsi/impl/uscsi.h>

#include "scsi-command.h"
#include "burn-debug.h"
#include "scsi-utils.h"
#include "scsi-error.h"
#include "scsi-sense-data.h"

struct _BraseroDeviceHandle {
	int fd;
};

struct _BraseroScsiCmd {
	uchar cmd [BRASERO_SCSI_CMD_MAX_LEN];
	BraseroDeviceHandle *handle;

	const BraseroScsiCmdInfo *info;
};
typedef struct _BraseroScsiCmd BraseroScsiCmd;

#define BRASERO_SCSI_CMD_OPCODE_OFF			0
#define BRASERO_SCSI_CMD_SET_OPCODE(command)		(command->cmd [BRASERO_SCSI_CMD_OPCODE_OFF] = command->info->opcode)

#define OPEN_FLAGS			O_RDONLY /*|O_EXCL */|O_NONBLOCK

/**
 * This is to send a command
 */
BraseroScsiResult
brasero_scsi_command_issue_sync (gpointer command,
				 gpointer buffer,
				 int size,
				 BraseroScsiErrCode *error)
{
	uchar sense_buffer [BRASERO_SENSE_DATA_SIZE];
	struct uscsi_cmd transport;
	BraseroScsiResult res;
	BraseroScsiCmd *cmd;
	short timeout = 10;

	memset (&sense_buffer, 0, BRASERO_SENSE_DATA_SIZE);
	memset (&transport, 0, sizeof (struct uscsi_cmd));

	cmd = command;

	if (cmd->info->direction & BRASERO_SCSI_READ)
		transport.uscsi_flags = USCSI_READ;
	else if (cmd->info->direction & BRASERO_SCSI_WRITE)
		transport.uscsi_flags = USCSI_WRITE;

	transport.uscsi_cdb = (caddr_t) cmd->cmd;
	g_debug("cmd: %s\n", transport.uscsi_cdb);
	transport.uscsi_cdblen = (uchar_t) cmd->info->size;
	transport.uscsi_bufaddr = (caddr_t) buffer;
	transport.uscsi_buflen = (size_t) size;
	transport.uscsi_timeout = timeout;

	/* where to output the scsi sense buffer */
	transport.uscsi_flags |= USCSI_RQENABLE;
	transport.uscsi_rqbuf = sense_buffer;
	transport.uscsi_rqlen = BRASERO_SENSE_DATA_SIZE;

	/* NOTE only for TEST UNIT READY, REQUEST/MODE SENSE, INQUIRY,
	 * READ CAPACITY, READ BUFFER, READ and LOG SENSE are allowed with it */
	res = ioctl (cmd->handle->fd, USCSICMD, &transport);
	if (res) {
		BRASERO_SCSI_SET_ERRCODE (error, BRASERO_SCSI_ERRNO);
		g_debug("ioctl ERR: %s\n", g_strerror(errno));
		return BRASERO_SCSI_FAILURE;
	}

	if ((transport.uscsi_status & STATUS_MASK) == STATUS_GOOD)
		return BRASERO_SCSI_OK;

	if ((transport.uscsi_rqstatus & STATUS_MASK == STATUS_CHECK)
	    && transport.uscsi_rqlen)
		return brasero_sense_data_process (sense_buffer, error);

	return BRASERO_SCSI_FAILURE;
}

gpointer
brasero_scsi_command_new (const BraseroScsiCmdInfo *info,
			  BraseroDeviceHandle *handle) 
{
	BraseroScsiCmd *cmd;

	/* make sure we can set the flags of the descriptor */

	/* allocate the command */
	cmd = g_new0 (BraseroScsiCmd, 1);
	cmd->info = info;
	cmd->handle = handle;

	BRASERO_SCSI_CMD_SET_OPCODE (cmd);
	return cmd;
}

BraseroScsiResult
brasero_scsi_command_free (gpointer cmd)
{
	g_free (cmd);
	return BRASERO_SCSI_OK;
}

/**
 * This is to open a device
 */

BraseroDeviceHandle *
brasero_device_handle_open (const gchar *path,
			    BraseroScsiErrCode *code)
{
	int fd;
	BraseroDeviceHandle *handle;
	const gchar *blockdisk = "/dev/dsk/";
	gchar *rawdisk = NULL;

	fd = open (path, OPEN_FLAGS);
	if (fd < 0) {
		if (errno == EAGAIN
		||  errno == EWOULDBLOCK
		||  errno == EBUSY)
			*code = BRASERO_SCSI_NOT_READY;
		else
			*code = BRASERO_SCSI_ERRNO;

		g_debug("open ERR: %s\n", g_strerror(errno));
		return NULL;
	}

	handle = g_new (BraseroDeviceHandle, 1);
	handle->fd = fd;

	return handle;
}

void
brasero_device_handle_close (BraseroDeviceHandle *handle)
{
	close (handle->fd);
	g_free (handle);
}

