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

#ifndef _SCSI_READ_DISC_INFO_H
#define _SCSI_READ_DISC_INFO_H

G_BEGIN_DECLS

typedef enum {
BRASERO_SCSI_SESSION_EMPTY		= 0x00,
BRASERO_SCSI_SESSION_INCOMPLETE		= 0x01,
BRASERO_SCSI_SESSION_DAMAGED		= 0x02,
BRASERO_SCSI_SESSION_COMPLETE		= 0x03
} BraseroScsiSessionState;

typedef enum {
BRASERO_SCSI_DISC_EMPTY			= 0x00,
BRASERO_SCSI_DISC_INCOMPLETE		= 0x01,
BRASERO_SCSI_DISC_FINALIZED		= 0x02,
BRASERO_SCSI_DISC_OTHERS		= 0x03
} BraseroScsiDiscStatus;

typedef enum {
BRASERO_SCSI_BG_FORMAT_BLANK_OR_NOT_RW	= 0x00,
BRASERO_SCSI_BG_FORMAT_STARTED		= 0x01,
BRASERO_SCSI_BG_FORMAT_IN_PROGESS	= 0x02,
BRASERO_SCSI_BG_FORMAT_COMPLETED	= 0x03
} BraseroScsiBgFormatStatus;

typedef enum {
BRASERO_SCSI_DISC_CDDA_CDROM		= 0x00,
BRASERO_SCSI_DISC_CD_I			= 0x10,
BRASERO_SCSI_DISC_XA			= 0x20,
BRASERO_SCSI_DISC_UNDEFINED		= 0xFF
	/* reserved */
} BraseroScsiTrackDataFormat;


struct _BraseroScsiOPCEntry {
	uchar speed			[2];
	uchar opc			[6];
};
typedef struct _BraseroScsiOPCEntry BraseroScsiOPCEntry;

#if G_BYTE_ORDER == G_LITTLE_ENDIAN

struct _BraseroScsiDiscInfoStd {
	uchar len			[2];

	uchar status			:2;
	uchar last_session_state	:2;
	uchar erasable			:1;
	uchar info_type			:3;

	uchar first_track_num;
	uchar sessions_num_low;
	uchar first_track_nb_lastses_low;
	uchar last_track_nb_lastses_low;

	uchar bg_format_status		:2;
	uchar dbit			:1;
	uchar reserved0			:1;
	uchar disc_app_code_valid	:1;
	uchar unrestricted_use		:1;
	uchar disc_barcode_valid	:1;
	uchar disc_id_valid		:1;

	uchar disc_type;

	uchar sessions_num_high;
	uchar first_track_nb_lastses_high;
	uchar last_track_nb_lastses_high;

	uchar disc_id			[4];
	uchar last_session_leadin	[4];

	uchar last_possible_leadout_res;
	uchar last_possible_leadout_mn;
	uchar last_possible_leadout_sec;
	uchar last_possible_leadout_frame;

	uchar disc_barcode		[8];

	uchar reserved1;

	uchar OPC_table_num;
	BraseroScsiOPCEntry opc_entry	[0];
};

struct _BraseroScsiTrackResInfo {
	uchar len			[2];

	uchar reserved0			:5;
	uchar disc_info			:3;

	uchar reserved1;

	uchar max_track_num		[2];
	uchar track_num			[2];
	uchar max_appendable_track	[2];
	uchar appendable_track_num	[2];
};

struct _BraseroScsiPOWResInfo {
	uchar len			[2];

	uchar reserved0			:5;
	uchar disc_info			:3;

	uchar reserved1;

	uchar remaining_pow_replacements[4];
	uchar remaining_pow_realloc	[4];
	uchar remaining_pow_update	[4];
};

#else

struct _BraseroScsiDiscInfoStd {
	uchar len			[2];

	uchar info_type			:3;
	uchar erasable			:1;
	uchar last_session_state	:2;
	uchar status			:2;

	uchar first_track_num;
	uchar sessions_num_low;
	uchar first_track_nb_lastses_low;
	uchar last_track_nb_lastses_low;

	uchar disc_id_valid		:1;
	uchar disc_barcode_valid	:1;
	uchar unrestricted_use		:1;
	uchar disc_app_code_valid	:1;
	uchar reserved0			:1;
	uchar dbit			:1;
	uchar bg_format_status		:2;

	uchar disc_type;

	uchar sessions_num_high;
	uchar first_track_nb_lastses_high;
	uchar last_track_nb_lastses_high;

	uchar disc_id			[4];
	uchar last_session_leadin	[4];

	uchar last_possible_leadout_res;
	uchar last_possible_leadout_mn;
	uchar last_possible_leadout_sec;
	uchar last_possible_leadout_frame;

	uchar disc_barcode		[8];

	uchar reserved1;

	uchar OPC_table_num;
	BraseroScsiOPCEntry opc_entry	[0];
};

struct _BraseroScsiTrackResInfo {
	uchar len			[2];

	uchar disc_info			:3;
	uchar reserved0			:5;

	uchar reserved1;

	uchar max_track_num		[2];
	uchar tracks_num		[2];
	uchar max_appendable_tracks	[2];
	uchar appendable_track_num	[2];
};

struct _BraseroScsiPOWResInfo {
	uchar len			[2];

	uchar disc_info			:3;
	uchar reserved0			:5;

	uchar reserved1;

	uchar remaining_pow_replacements[4];
	uchar remaining_pow_realloc	[4];
	uchar remaining_pow_update	[4];
};

#endif

#define BRASERO_FIRST_TRACK_IN_LAST_SESSION(info) (((info)->first_track_nb_lastses_high << 8) + (info)->first_track_nb_lastses_low)
#define BRASERO_LAST_TRACK_IN_LAST_SESSION(info) (((info)->last_track_nb_lastses_high << 8) + (info)->last_track_nb_lastses_low)
#define BRASERO_SESSION_NUM(info) (((info)->sessions_num_high << 8) + (info)->sessions_num_low)

typedef struct _BraseroScsiDiscInfoStd BraseroScsiDiscInfoStd;
typedef struct _BraseroScsiTrackResInfo BraseroScsiTrackResInfo;
typedef struct _BraseroScsiPOWResInfo BraseroScsiPOWResInfo;

G_END_DECLS

#endif /* _SCSI_READ_DISC_INFO_H */

 
