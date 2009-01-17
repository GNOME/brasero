/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Brasero
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Brasero is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Brasero authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Brasero. This permission is above and beyond the permissions granted
 * by the GPL license by which Brasero is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Brasero is distributed in the hope that it will be useful,
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
 
#ifndef _SCSI_MECH_STATUS_H
#define _SCSI_MECH_STATUS_H

#include <glib.h>

G_BEGIN_DECLS

#if G_BYTE_ORDER == G_LITTLE_ENDIAN

struct _BraseroScsiMechStatusHdr {
	uchar current_slot	:5;
	uchar changer_state	:2;
	uchar fault		:1;

	uchar res1		:4;
	uchar door_open		:1;
	uchar mech_state	:3;

	uchar current_lba	[3];

	uchar number_slot	:6;
	uchar res2		:2;

	uchar len		[2];
};

#else

struct _BraseroScsiMechStatusHdr {
	uchar fault		:1;
	uchar changer_state	:2;
	uchar current_slot	:5;

	uchar mech_state	:3;
	uchar door_open		:1;
	uchar res1		:4;

	uchar current_lba	[3];

	uchar res2		:2;
	uchar number_slot	:6;

	uchar len		[2];
};

#endif

typedef struct _BraseroScsiMechStatusHdr BraseroScsiMechStatusHdr;

G_END_DECLS

#endif /* _SCSI_MECH_STATUS_H */

 
