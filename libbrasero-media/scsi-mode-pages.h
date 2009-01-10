/***************************************************************************
 *            scsi-mode-pages.h
 *
 *  Sat Oct 21 19:11:53 2006
 *  Copyright  2006  Rouquier Philippe
 *  <bonfire-app@wanadoo.fr>
 ****************************************************************************/

/*
 * Brasero is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * Brasero is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
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

 
