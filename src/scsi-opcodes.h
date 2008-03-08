/***************************************************************************
 *            scsi-cmd-opcodes.h
 *
 *  Sun Oct 22 11:07:53 2006
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

#ifndef _SCSI_CMD_OPCODES_H
#define _SCSI_CMD_OPCODES_H

#ifdef __cplusplus
extern "C"
{
#endif

/**
 *	SPC1
 */

#define BRASERO_TEST_UNIT_READY_OPCODE			0x00
#define BRASERO_INQUIRY_OPCODE				0x12
#define BRASERO_MODE_SENSE_OPCODE			0x5a


/**
 *	MMC1
 */

#define BRASERO_MECHANISM_STATUS_OPCODE			0xBD
#define BRASERO_READ_DISC_INFORMATION_OPCODE		0x51
#define BRASERO_READ_TRACK_INFORMATION_OPCODE		0x52
#define BRASERO_READ_TOC_PMA_ATIP_OPCODE		0x43
#define BRASERO_READ_BUFFER_CAPACITY_OPCODE		0x5C
#define BRASERO_READ_HEADER_OPCODE			0x44
#define BRASERO_READ_SUB_CHANNEL_OPCODE			0x42
#define BRASERO_READ_MASTER_CUE_OPCODE			0x59
#define BRASERO_LOAD_CD_OPCODE				0xA6
#define BRASERO_MECH_STATUS_OPCODE			0xBD
#define BRASERO_READ_CD_OPCODE				0xBE

/**
 *	MMC2
 */

#define BRASERO_GET_PERFORMANCE_OPCODE			0xac
#define BRASERO_GET_CONFIGURATION_OPCODE		0x46
#define BRASERO_READ_CAPACITY_OPCODE			0x25
#define BRASERO_READ_FORMAT_CAPACITIES_OPCODE		0x23

/**
 *	MMC3
 */

#define BRASERO_READ_DISC_STRUCTURE_OPCODE		0xAD

#ifdef __cplusplus
}
#endif

#endif /* _SCSI_CMD-OPCODES_H */

 
