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

#include <glib.h>

#include "scsi-base.h"

#ifndef _SCSI_READ_TRACK_INFORMATION_H
#define _SCSI_READ_TRACK_INFORMATION_H

G_BEGIN_DECLS

typedef enum {
BRASERO_SCSI_DATA_MODE_1			= 0x01,
BRASERO_SCSI_DATA_MODE_2_XA			= 0x02,
BRASERO_SCSI_DATA_BLOCK_TYPE			= 0x0F
} BraseroScsiDataMode;

#if G_BYTE_ORDER == G_LITTLE_ENDIAN

struct _BraseroScsiTrackInfo {
	uchar len			[2];

	uchar track_num_low;
	uchar session_num_low;

	uchar reserved0;

	uchar track_mode		:4;
	uchar copy			:1;
	uchar damage			:1;
	uchar layer_jmp_rcd_status	:2;

	uchar data_mode			:4;
	/* the 4 next bits indicate the track status */
	uchar fixed_packet		:1;
	uchar packet			:1;
	uchar blank			:1;
	uchar reserved_track		:1;

	uchar next_wrt_address_valid	:1;
	uchar last_recorded_blk_valid	:1;
	uchar reserved1			:6;

	uchar start_lba			[4];
	uchar next_wrt_address		[4];
	uchar free_blocks		[4];
	uchar packet_size		[4];
	uchar track_size		[4];
	uchar last_recorded_blk		[4];

	uchar track_num_high;
	uchar session_num_high;

	uchar reserved2			[2];		/* 36 bytes MMC1 */

	uchar rd_compat_lba		[4];		/* 40 bytes */
	uchar next_layer_jmp		[4];
	uchar last_layer_jmp		[4];		/* 48 bytes */
};

#else

struct _BraseroScsiTrackInfo {
	uchar len			[2];

	uchar track_num_low;
	uchar session_num_low;

	uchar reserved0;

	uchar layer_jmp_rcd_status	:2;
	uchar damage			:1;
	uchar copy			:1;
	uchar track_mode		:4;

	/* the 4 next bits indicate the track status */
	uchar reserved_track		:1;
	uchar blank			:1;
	uchar packet			:1;
	uchar fixed_packet		:1;
	uchar data_mode			:4;

	uchar reserved1			:6;
	uchar last_recorded_blk_valid	:1;
	uchar next_wrt_address_valid	:1;

	uchar start_lba			[4];
	uchar next_wrt_address		[4];
	uchar free_blocks		[4];
	uchar packet_size		[4];
	uchar track_size		[4];
	uchar last_recorded_blk		[4];

	uchar track_num_high;
	uchar session_num_high;

	uchar reserved2			[2];

	uchar rd_compat_lba		[4];
	uchar next_layer_jmp		[4];
	uchar last_layer_jmp		[4];
};

#endif

typedef struct _BraseroScsiTrackInfo BraseroScsiTrackInfo;

#define BRASERO_SCSI_TRACK_NUM(track)		(((track).track_num_high << 8) + (track).track_num_low)
#define BRASERO_SCSI_SESSION_NUM(track)		(((track).session_num_high << 8) + (track).session_num_low)
#define BRASERO_SCSI_TRACK_NUM_PTR(track)	(((track)->track_num_high << 8) + (track)->track_num_low)
#define BRASERO_SCSI_SESSION_NUM_PTR(track)	(((track)->session_num_high << 8) + (track)->session_num_low)

G_END_DECLS

#endif /* _SCSI_READ_TRACK_INFORMATION_H */

 
