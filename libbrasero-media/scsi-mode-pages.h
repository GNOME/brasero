/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-media
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-burn is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-burn authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-burn. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-burn is distributed in the hope that it will be useful,
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

#ifndef _SCSI_MODE_PAGES_H
#define _SCSI_MODE_PAGES_H

G_BEGIN_DECLS

#if G_BYTE_ORDER == G_LITTLE_ENDIAN

struct _BraseroScsiModePage {
	uchar code			:6;
	uchar reserved			:1;
	uchar ps			:1;

	uchar len;
};

#else

struct _BraseroScsiModePage {
	uchar ps			:1;
	uchar reserved			:1;
	uchar code			:6;

	uchar len;
};

#endif

typedef struct _BraseroScsiModePage BraseroScsiModePage;

struct _BraseroScsiModeHdr {
	uchar len			[2];
	uchar medium_type		:8;
	uchar device_param		:8;
	uchar reserved			[2];
	uchar bdlen			[2];
};
typedef struct _BraseroScsiModeHdr BraseroScsiModeHdr;

struct _BraseroScsiModeData {
	BraseroScsiModeHdr hdr;
	BraseroScsiModePage page;
};
typedef struct _BraseroScsiModeData BraseroScsiModeData;

/**
 * Pages codes
 */

typedef enum {
	BRASERO_SPC_PAGE_NULL		= 0x00,
	BRASERO_SPC_PAGE_WRITE		= 0x05,
	BRASERO_SPC_PAGE_STATUS		= 0x2a,
} BraseroSPCPageType;

G_END_DECLS

#endif /* _SCSI_MODE-PAGES_H */

 
