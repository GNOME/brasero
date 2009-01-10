/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2007-2008 <bonfire-app@wanadoo.fr>
 *
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
 
#ifndef _SCSI_WRITE_PAGE_H
#define _SCSI_WRITE_PAGE_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * Write Parameters Page
 * This mode page is useful for CD-R, CD-RW (not MRW formatted), DVD-R, and
 * DVD-RW media. Not for DVD+R(W)/DVD-RAM.
 */

#if G_BYTE_ORDER == G_LITTLE_ENDIAN

struct _BraseroScsiWritePage {
	uchar code		:6;
	uchar reserved		:1;
	uchar ps		:1;

	uchar len;

	uchar write_type	:4;
	uchar testwrite		:1;
	uchar LS_V		:1;
	uchar BUFE		:1;
	uchar reserved1		:1;

	uchar track_mode	:4;
	uchar copy		:1;
	uchar FP		:1;
	uchar multisession	:2;

	uchar data_block_type	:4;
	uchar reserved2		:4;

	uchar link_size;

	uchar reserved3;

	uchar app_code		:6;
	uchar reserved4		:2;

	uchar session_format;

	uchar reserved5;

	uchar packet_size	[4];

	uchar pause_len		[2];

	uchar MCN		[16];

	uchar ISRC_COL		[16];

	uchar sub_hdr0;
	uchar sub_hdr1;
	uchar sub_hdr2;
	uchar sub_hdr3;

	uchar vendor		[4];
};

#else

struct _BraseroScsiWritePage {
	uchar ps		:1;
	uchar reserved		:1;
	uchar code		:6;

	uchar len;

	uchar reserved1		:1;
	uchar BUFE		:1;
	uchar LS_V		:1;
	uchar testwrite		:1;
	uchar write_type	:4;

	uchar multisession	:2;
	uchar FP		:1;
	uchar copy		:1;
	uchar track_mode	:4;

	uchar reserved2		:4;
	uchar data_block_type	:4;

	uchar link_size;

	uchar reserved3;

	uchar reserved4		:2;
	uchar app_code		:6;

	uchar session_format;

	uchar reserved5;

	uchar packet_size	[4];

	uchar pause_len		[2];

	uchar MCN		[16];

	uchar ISRC_COL		[16];

	uchar sub_hdr0;
	uchar sub_hdr1;
	uchar sub_hdr2;
	uchar sub_hdr3;

	uchar vendor		[4];
};

#endif

typedef struct _BraseroScsiWritePage BraseroScsiWritePage;

typedef enum {
	BRASERO_SCSI_WRITE_PACKET	= 0x00,
	BRASERO_SCSI_WRITE_TAO		= 0x01,
	BRASERO_SCSI_WRITE_SAO		= 0x02,
	BRASERO_SCSI_WRITE_RAW		= 0x03

	/* Reserved */
} BraseroScsiWriteMode;

G_END_DECLS

#endif /* _SCSI_WRITE_PAGE_H */

 
