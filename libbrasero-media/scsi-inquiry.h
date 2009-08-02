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

#ifndef _SCSI_INQUIRY_H
#define _SCSI_INQUIRY_H

#include <glib.h>

G_BEGIN_DECLS

#if G_BYTE_ORDER == G_LITTLE_ENDIAN

struct _BraseroScsiInquiry {
	uchar type			:5;
	uchar qualifier			:3;

	uchar reserved0		:7;
	uchar rmb				:1;

	uchar ansi_ver			:3;
	uchar ecma_ver		:3;
	uchar iso_ver			:2;

	uchar response_format	:4;
	uchar reserved1		:1;
	uchar norm_aca			:1;
	uchar trmtsk			:1;
	uchar aerc			:1;

	uchar add_len;

	uchar reserved2;

	uchar addr16			:1;
	uchar addr32			:1;
	uchar ack_req			:1;
	uchar mchngr			:1;
	uchar multiP			:1;
	uchar vs1				:1;
	uchar enc_serv			:1;
	uchar reserved3		:1;

	uchar vs2				:1;
	uchar cmd_queue		:1;
	uchar transdis			:1;
	uchar linked			:1;
	uchar sync			:1;
	uchar wbus16			:1;
	uchar wbus32			:1;
	uchar rel_addr			:1;

	uchar vendor			[8];
	uchar name			[16];
	uchar revision			[4];
};

#else

struct _BraseroScsiInquiry {
	uchar qualifier			:3;
	uchar type			:5;

	uchar rmb				:1;
	uchar reserved0		:7;

	uchar iso_ver			:2;
	uchar ecma_ver		:3;
	uchar ansi_ver			:3;

	uchar aerc			:1;
	uchar trmtsk			:1;
	uchar norm_aca			:1;
	uchar reserved1		:1;
	uchar response_format	:4;

	uchar add_len;

	uchar reserved2;

	uchar reserved3		:1;
	uchar enc_serv			:1;
	uchar vs1				:1;
	uchar multiP			:1;
	uchar mchngr			:1;
	uchar ack_req			:1;
	uchar addr32			:1;
	uchar addr16			:1;

	uchar rel_addr			:1;
	uchar wbus32			:1;
	uchar wbus16			:1;
	uchar sync			:1;
	uchar linked			:1;
	uchar transdis			:1;
	uchar cmd_queue		:1;
	uchar vs2				:1;

	uchar vendor			[8];
	uchar name			[16];
	uchar revision			[4];
};

#endif

typedef struct _BraseroScsiInquiry BraseroScsiInquiry;

G_END_DECLS

#endif
