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

#ifndef _SCSI_READ_TOC_PMA_ATIP_H
#define _SCSI_READ_TOC_PMA_ATIP_H

G_BEGIN_DECLS

#if G_BYTE_ORDER == G_LITTLE_ENDIAN

struct _BraseroScsiTocPmaAtipHdr {
	uchar len			[2];

	uchar first_track_session;
	uchar last_track_session;
};

struct _BraseroScsiTocDesc {
	uchar reserved0;

	uchar control			:4; /* BraseroScsiTrackMode 		*/
	uchar adr			:4; /* BraseroScsiQSubChannelProgAreaMode 	*/

	uchar track_num;
	uchar reserved1;

	uchar track_start		[4];
};

struct _BraseroScsiRawTocDesc {
	uchar session_num;

	uchar control			:4; /* BraseroScsiTrackMode 		*/
	uchar adr			:4; /* BraseroScsiQSubChannelLeadinMode	*/

	/* Q sub-channel data */
	uchar track_num;

	uchar point;			/* BraseroScsiQSubChannelLeadinMode5 or BraseroScsiQSubChannelLeadinMode1 */
	uchar min;
	uchar sec;
	uchar frame;

	uchar zero;
	uchar p_min;
	uchar p_sec;
	uchar p_frame;
};

struct _BraseroScsiPmaDesc {
	uchar reserved0;

	uchar control			:4; /* BraseroScsiTrackMode 		*/
	uchar adr			:4; /* BraseroScsiQSubChannelPmaMode 	*/

	uchar track_num;			/* always 0 */

	uchar point;				/* see BraseroScsiQSubChannelPmaMode */
	uchar min;
	uchar sec;
	uchar frame;

	uchar zero;
	uchar p_min;
	uchar p_sec;
	uchar p_frame;
};

struct _BraseroScsiAtipDesc {
	uchar reference_speed		:3;	/* 1 */
	uchar reserved0			:1;
	uchar indicative_target_wrt_pwr	:4;

	uchar reserved1			:6;	/* 2 */
	uchar unrestricted_use		:1;
	uchar bit0			:1;

	uchar A3_valid			:1;	/* 3 */
	uchar A2_valid			:1;
	uchar A1_valid			:1;
	uchar disc_sub_type		:3;
	uchar erasable			:1;
	uchar bit1			:1;

	uchar reserved2;			/* 4 */

	uchar leadin_mn;
	uchar leadin_sec;
	uchar leadin_frame;
	uchar reserved3;			/* 8 */

	/* Additional capacity for high capacity CD-R,
	 * otherwise last possible leadout */
	uchar leadout_mn;
	uchar leadout_sec;
	uchar leadout_frame;
	uchar reserved4;			/* 12 */

	/* Write strategy recording parameters.
	 * See MMC1 and MMC2 for a description. */
	uchar A1_data			[3];
	uchar reserved5;
	
	uchar A2_data			[3];
	uchar reserved6;

	uchar A3_data			[3];
	uchar reserved7;

	/* Be careful here since the following is only true for MMC3. That means
	 * if we use this size with a MMC1/2 drives it returns an error (invalid
	 * field). The following value is not really useful anyway. */
	uchar S4_data			[3];
	uchar reserved8;
};

#else

struct _BraseroScsiTocPmaAtipHdr {
	uchar len			[2];

	uchar first_track_session;
	uchar last_track_session;
};

struct _BraseroScsiTocDesc {
	uchar reserved0;

	uchar adr			:4;
	uchar control			:4;

	uchar track_num;
	uchar reserved1;

	uchar track_start		[4];
};

struct _BraseroScsiRawTocDesc {
	uchar session_num;

	uchar adr			:4;
	uchar control			:4;

	uchar track_num;

	uchar point;
	uchar min;
	uchar sec;
	uchar frame;

	uchar zero;
	uchar p_min;
	uchar p_sec;
	uchar p_frame;
};

struct _BraseroScsiPmaDesc {
	uchar reserved0;

	uchar adr			:4;
	uchar control			:4;

	uchar track_num;

	uchar point;
	uchar min;
	uchar sec;
	uchar frame;

	uchar zero;
	uchar p_min;
	uchar p_sec;
	uchar p_frame;
};

struct _BraseroScsiAtipDesc {
	uchar indicative_target_wrt_pwr	:4;
	uchar reserved0			:1;
	uchar reference_speed		:3;

	uchar bit0			:1;
	uchar unrestricted_use		:1;
	uchar reserved1			:6;

	uchar bit1			:1;
	uchar erasable			:1;
	uchar disc_sub_type		:3;
	uchar A1_valid			:1;
	uchar A2_valid			:1;
	uchar A3_valid			:1;

	uchar reserved2;

	uchar leadin_start_time_mn;
	uchar leadin_start_time_sec;
	uchar leadin_start_time_frame;
	uchar reserved3;

	/* Additional capacity for high capacity CD-R,
	 * otherwise last possible leadout */
	uchar leadout_mn;
	uchar leadout_sec;
	uchar leadout_frame;
	uchar reserved4;

	/* write strategy recording parameters */
	uchar A1_data			[3];
	uchar reserved5;
	
	uchar A2_data			[3];
	uchar reserved6;

	uchar A3_data			[3];
	uchar reserved7;

	uchar S4_data			[3];
	uchar reserved8;
};

#endif

typedef struct _BraseroScsiTocDesc BraseroScsiTocDesc;
typedef struct _BraseroScsiRawTocDesc BraseroScsiRawTocDesc;
typedef struct _BraseroScsiPmaDesc BraseroScsiPmaDesc;
typedef struct _BraseroScsiAtipDesc BraseroScsiAtipDesc;

typedef struct _BraseroScsiTocPmaAtipHdr BraseroScsiTocPmaAtipHdr;

/* multiple toc descriptors may be returned */
struct _BraseroScsiFormattedTocData {
	BraseroScsiTocPmaAtipHdr hdr	[1];
	BraseroScsiTocDesc desc		[0];
};
typedef struct _BraseroScsiFormattedTocData BraseroScsiFormattedTocData;

/* multiple toc descriptors may be returned */
struct _BraseroScsiRawTocData {
	BraseroScsiTocPmaAtipHdr hdr	[1];
	BraseroScsiRawTocDesc desc	[0];
};
typedef struct _BraseroScsiRawTocData BraseroScsiRawTocData;

/* multiple pma descriptors may be returned */
struct _BraseroScsiPmaData {
	BraseroScsiTocPmaAtipHdr hdr	[1];
	BraseroScsiPmaDesc desc		[0];	
};
typedef struct _BraseroScsiPmaData BraseroScsiPmaData;

struct _BraseroScsiAtipData {
	BraseroScsiTocPmaAtipHdr hdr	[1];
	BraseroScsiAtipDesc desc	[1];
};
typedef struct _BraseroScsiAtipData BraseroScsiAtipData;

struct _BraseroScsiMultisessionData {
	BraseroScsiTocPmaAtipHdr hdr	[1];
	BraseroScsiTocDesc desc		[1];
};
typedef struct _BraseroScsiMultisessionData BraseroScsiMultisessionData;

/* Inside a language block, packs must be recorded in that order */
typedef enum {
BRASERO_SCSI_CD_TEXT_ALBUM_TITLE	= 0x80,
BRASERO_SCSI_CD_TEXT_PERFORMER_NAME	= 0x81,
BRASERO_SCSI_CD_TEXT_SONGWRITER_NAME	= 0x82,
BRASERO_SCSI_CD_TEXT_COMPOSER_NAME	= 0x83,
BRASERO_SCSI_CD_TEXT_ARRANGER_NAME	= 0x84,
BRASERO_SCSI_CD_TEXT_ARTIST_NAME	= 0x85,
BRASERO_SCSI_CD_TEXT_DISC_ID_INFO	= 0x86,
BRASERO_SCSI_CD_TEXT_GENRE_ID_INFO	= 0x87,
BRASERO_SCSI_CD_TEXT_TOC_1		= 0x88,
BRASERO_SCSI_CD_TEXT_TOC_2		= 0x89,
BRASERO_SCSI_CD_TEXT_RESERVED_1		= 0x8A,
BRASERO_SCSI_CD_TEXT_RESERVED_2		= 0x8B,
BRASERO_SCSI_CD_TEXT_RESERVED_3		= 0x8C,
BRASERO_SCSI_CD_TEXT_RESERVED_CONTENT	= 0x8D,
BRASERO_SCSI_CD_TEXT_UPC_EAN_ISRC	= 0x8E,
BRASERO_SCSI_CD_TEXT_BLOCK_SIZE		= 0x8F,
} BraseroScsiCDTextPackType;

typedef enum {
	BRASERO_CD_TEXT_8859_1		= 0x00,
	BRASERO_CD_TEXT_ASCII		= 0x01,	/* (7 bit)	*/

	/* Reserved */

	BRASERO_CD_TEXT_KANJI		= 0x80,
	BRASERO_CD_TEXT_KOREAN		= 0x81,
	BRASERO_CD_TEXT_CHINESE		= 0x82	/* Mandarin */
} BraseroScsiCDTextCharset;

struct _BraseroScsiCDTextPackData {
	uchar type;
	uchar track_num;
	uchar pack_num;

	uchar char_pos			:4;	/* byte not used for type 0x8F */
	uchar block_num			:3;
	uchar double_byte		:1;

	uchar text			[12];
	uchar crc			[2];
};
typedef struct _BraseroScsiCDTextPackData BraseroScsiCDTextPackData;

/* Takes two BraseroScsiCDTextPackData (18 bytes) 3 x 12 = 36 bytes */
struct _BraseroScsiCDTextPackCharset {
	char charset;
	char first_track;
	char last_track;
	char copyr_flags;
	char pack_count [16];
	char last_seqnum [8];
	char language_code [8];
};
typedef struct _BraseroScsiCDTextPackCharset BraseroScsiCDTextPackCharset;

struct _BraseroScsiCDTextData {
	BraseroScsiTocPmaAtipHdr hdr	[1];
	BraseroScsiCDTextPackData pack	[0];
};
typedef struct _BraseroScsiCDTextData BraseroScsiCDTextData;

#define BRASERO_SCSI_TRACK_LEADOUT_START	0xAA

G_END_DECLS

#endif /* _SCSI_READ_TOC_PMA_ATIP_H */
