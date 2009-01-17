/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-media
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-media is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-media authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-media. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-media is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-media is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#include "scsi-base.h"

#ifndef _SCSI_READ_DISC_STRUCTURE_H
#define _SCSI_READ_DISC_STRUCTURE_H

G_BEGIN_DECLS

typedef enum {
	/* structures specific to DVD and HD-DVD */
BRASERO_SCSI_FORMAT_DVD_PHYSICAL_INFO		= 0x00,
BRASERO_SCSI_FORMAT_DVD_COPYRIGHT_INFO		= 0x01,
BRASERO_SCSI_FORMAT_ROM_DISC_KEY		= 0x02,
BRASERO_SCSI_FORMAT_DVD_BURST_CUTTING_AREA	= 0x03,
BRASERO_SCSI_FORMAT_DVD_MANUFACTURING_INFO	= 0x04,
BRASERO_SCSI_FORMAT_DVD_COPYRIGHT_MNGT		= 0x05,

BRASERO_SCSI_FORMAT_ROM_MEDIA_ID		= 0x06, /* Only for ROM medium? */

BRASERO_SCSI_FORMAT_ROM_MEDIA_KEY_BLOCK		= 0x07,
BRASERO_SCSI_FORMAT_RAM_DDS_INFO		= 0x08,
BRASERO_SCSI_FORMAT_RAM_MEDIUM_STATUS		= 0x09,
BRASERO_SCSI_FORMAT_RAM_SPARE_AREA		= 0x0A,
BRASERO_SCSI_FORMAT_RAM_RECORDING_TYPE		= 0x0B,
BRASERO_SCSI_FORMAT_LESS_BORDER_OUT_RMD		= 0x0C,
BRASERO_SCSI_FORMAT_LESS_RAM			= 0x0D,
BRASERO_SCSI_FORMAT_LESS_PRE_PIT_INFO		= 0x0E, /* Only for DVD-R/-RW */

BRASERO_SCSI_FORMAT_LESS_MEDIA_ID_DVD		= 0x0F, /* Only for DVD-R/-RW */

BRASERO_SCSI_FORMAT_LESS_PHYSICAL_FORMAT	= 0x10,
BRASERO_SCSI_FORMAT_PLUS_ADIP			= 0x11,
BRASERO_SCSI_FORMAT_HD_COPYRIGHT_PROTECTION	= 0x12,
	/* reserved */
BRASERO_SCSI_FORMAT_DVD_COPYRIGHT_DATA_SECTION	= 0x15,
	/* reserved */
BRASERO_SCSI_FORMAT_HD_R_MEDIUM_STATUS		= 0x19,
BRASERO_SCSI_FORMAT_HD_R_LAST_RECORDED_RMD	= 0x1A,
	/* reserved */
BRASERO_SCSI_FORMAT_DL_LAYER_CAPACITY		= 0x20,
BRASERO_SCSI_FORMAT_DL_MIDDLE_ZONE_START	= 0x21,
BRASERO_SCSI_FORMAT_DL_JUMP_INTERVAL_SIZE	= 0x22,
BRASERO_SCSI_FORMAT_DL_LBA_START		= 0x23,
BRASERO_SCSI_FORMAT_DL_REMAPPING_ANCHOR		= 0x24,
	/* reserved */
BRASERO_SCSI_FORMAT_PLUS_DISC_CTRL_BLOCK	= 0x30,
BRASERO_SCSI_FORMAT_MRW_MTA_ECC_BLOCK		= 0x31,	/* See Mount Rainer specs */
} BraseroScsiDVDFormatType;

typedef enum {
/* for blu-ray discs */
BRASERO_SCSI_FORMAT_BD_DISC_INFO		= 0x00,
	/*reserved */
BRASERO_SCSI_FORMAT_BD_DISC_DEFINITION		= 0x08,
BRASERO_SCSI_FORMAT_BD_CARTRIDGE_STATUS		= 0x09,
BRASERO_SCSI_FORMAT_BD_SPARE_AREA_STATUS	= 0x0A,
	/* reserved */
BRASERO_SCSI_FORMAT_BD_RAW_DEFECT_LIST		= 0x12,
	/* reserved */
BRASERO_SCSI_FORMAT_BD_PHYSICAL_ACCESS_CTL	= 0x30,
	/* reserved */
} BraseroScsiBDFormatType;

typedef enum {
	/* general structures */
BRASERO_FORMAT_VOLUME_ID_AACS			= 0x80,
BRASERO_FORMAT_MEDIA_SERIAL_AACS		= 0x81,
BRASERO_FORMAT_MEDIA_ID_AACS			= 0x82,
BRASERO_FORMAT_MEDIA_KEY_BLOCK			= 0x83,
	/* reserved */
BRASERO_FORMAT_LAYERS_LIST			= 0x90,
	/* reserved */
BRASERO_FORMAT_WRITE_PROTECTION_STATUS		= 0xC0,
	/* reserved */
BRASERO_FORMAT_READ_DISC_STRUCT_CAPS		= 0xFF,
} BraseroScsiGenericFormatType;

struct _BraseroScsiReadDiscStructureHdr {
	uchar len			[2];
	uchar reserved0;

	/* only with 0x83 (key) */
	uchar key_pack_num;

	uchar data			[0];
};
typedef struct _BraseroScsiReadDiscStructureHdr BraseroScsiReadDiscStructureHdr;

G_END_DECLS

#endif /* _SCSI_READ_DISC_STRUCTURE_H */

 
