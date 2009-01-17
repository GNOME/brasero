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
 
#ifndef _SCSI_READ_CD_H
#define _SCSI_READ_CD_H

#include <glib.h>

G_BEGIN_DECLS


typedef enum {
	BRASERO_SCSI_BLOCK_HEADER_NONE		= 0,
	BRASERO_SCSI_BLOCK_HEADER_MAIN		= 1,
	BRASERO_SCSI_BLOCK_HEADER_SUB		= 1 << 1
} BraseroScsiBlockHeader;

typedef enum {
	BRASERO_SCSI_BLOCK_TYPE_ANY		= 0,
	BRASERO_SCSI_BLOCK_TYPE_CDDA		= 1,
	BRASERO_SCSI_BLOCK_TYPE_MODE1		= 2,
	BRASERO_SCSI_BLOCK_TYPE_MODE2_FORMLESS	= 3,
	BRASERO_SCSI_BLOCK_TYPE_MODE2_FORM1	= 4,
	BRASERO_SCSI_BLOCK_TYPE_MODE2_FORM2	= 5
} BraseroScsiBlockType;

typedef enum {
	BRASERO_SCSI_BLOCK_NO_SUBCHANNEL	= 0,
	BRASERO_SCSI_BLOCK_SUB_Q		= 2,
	BRASERO_SCSI_BLOCK_SUB_R_W		= 4
} BraseroScsiBlockSubChannel;


G_END_DECLS

#endif /* _SCSI_READ_CD_H */

