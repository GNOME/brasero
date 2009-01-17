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

#ifndef _BURN_STATUS_PAGE_H
#define _BURN_STATUS_PAGE_H

G_BEGIN_DECLS

/**
 * Status page
 */

typedef struct _BraseroScsiStatusWrSpdDesc BraseroScsiStatusWrSpdDesc;

#if G_BYTE_ORDER == G_LITTLE_ENDIAN

struct _BraseroScsiStatusWrSpdDesc {
	uchar reserved0			:8;

	uchar rot_ctl			:3;
	uchar reserved1			:5;

	uchar speed			[2];
};

struct _BraseroScsiStatusPage {
	uchar code			:6;			/* 0 */
	uchar reserved0			:1;
	uchar ps			:1;

	uchar len			:8;			/* 1 */

	uchar rd_CDR			:1;			/* 2 */
	uchar rd_CDRW			:1;
	uchar method2			:1;
	uchar rd_DVDROM			:1;
	uchar rd_DVDR			:1;
	uchar rd_DVDRAM			:1;
	uchar reserved1			:2;

	uchar wr_CDR			:1;			/* 3 */
	uchar wr_CDRW			:1;
	uchar dummy			:1;
	uchar reserved3			:1;
	uchar wr_DVDR			:1;
	uchar wr_DVDRAM			:1;
	uchar reserved2			:2;

	uchar play_audio		:1;			/* 4 */
	uchar composite			:1;
	uchar digital_port_2		:1;
	uchar digital_port_1		:1;
	uchar mode2_1			:1;
	uchar mode2_2			:1;
	uchar multisession		:1;
	uchar buffer			:1;

	uchar CDDA_support		:1;			/* 5 */
	uchar CDDA_accuracy		:1;
	uchar support_RW		:1;
	uchar RW_interleace		:1;
	uchar c2_pointers		:1;
	uchar isrc			:1;
	uchar upc			:1;
	uchar barcode			:1;

	uchar lock			:1;			/* 6 */
	uchar lock_state		:1;
	uchar jumper			:1;
	uchar eject			:1;
	uchar reserved4			:1;
	uchar load_type			:3;

	uchar separate_vol_level	:1;			/* 7 */
	uchar separate_chnl_mute	:1;
	uchar changer_support		:1;
	uchar slot_selection		:1;
	uchar side_change		:1;
	uchar RW_leadin			:1;
	uchar reserved5			:2;

	uchar rd_current_speed		[2];			/* 8 */
	uchar max_buf_size		[2];
	uchar volume_lvl_num		[2];
	uchar rd_max_speed		[2];

	uchar reserved6			:8;			/* 16 */

	uchar reserved8			:1;			/* 17 */
	uchar bck			:1;
	uchar rck			:1;
	uchar lsbf			:1;
	uchar length			:2;
	uchar reserved7			:2;

	uchar wr_max_speed		[2];			/* 18 */
	uchar wr_current_speed		[2];

	uchar copy_mngt_rev 		[2];			/* 22 */

	uchar reserved9 		[3];

	uchar current_rot_ctl		:2;			/* 27 */
	uchar reserved10		:6;

	uchar wr_selected_speed		[2];
	uchar wr_speed_desc_num		[2];

	BraseroScsiStatusWrSpdDesc wr_spd_desc [0];		/* 32 */
};

#else

struct _BraseroScsiStatusWrSpdDesc {
	uchar reserved0			:8;

	uchar reserved1			:5;
	uchar rot_ctl			:3;

	uchar speed			[2];
};

struct _BraseroScsiStatusPage {
	uchar ps			:1;			/* 0 */
	uchar reserved0			:1;
	uchar code			:6;

	uchar len			:8;			/* 1 */

	uchar reserved1			:2;			/* 2 */
	uchar rd_DVDRAM			:1;
	uchar rd_DVDR			:1;
	uchar rd_DVDROM			:1;
	uchar method2			:1;
	uchar rd_CDRW			:1;
	uchar rd_CDR			:1;

	uchar reserved2			:2;			/* 3 */
	uchar wr_DVDRAM			:1;
	uchar wr_DVDR			:1;
	uchar reserved3			:1;
	uchar dummy			:1;
	uchar wr_CDRW			:1;
	uchar wr_CDR			:1;

	uchar buffer			:1;			/* 4 */
	uchar multisession		:1;
	uchar mode2_2			:1;
	uchar mode2_1			:1;
	uchar digital_port_1		:1;
	uchar digital_port_2		:1;
	uchar composite			:1;
	uchar play_audio		:1;

	uchar barcode			:1;			/* 5 */
	uchar upc			:1;
	uchar isrc			:1;
	uchar c2_pointers		:1;
	uchar RW_interleace		:1;
	uchar support_RW		:1;
	uchar CDDA_accuracy		:1;
	uchar CDDA_support		:1;

	uchar load_type			:3;			/* 6 */
	uchar reserved4			:1;
	uchar eject			:1;
	uchar jumper			:1;
	uchar lock_state		:1;
	uchar lock			:1;

	uchar reserved5			:2;			/* 7 */
	uchar RW_leadin			:1;
	uchar side_change		:1;
	uchar slot_selection		:1;
	uchar changer_support		:1;
	uchar separate_chnl_mute	:1;
	uchar separate_vol_level	:1;

	uchar rd_max_speed		[2];			/* 8 */
	uchar volume_lvl_num		[2];
	uchar max_buf_size		[2];
	uchar rd_current_speed		[2];

	uchar reserved6			:8;			/* 16 */

	uchar reserved7			:2;			/* 17 */
	uchar length			:2;
	uchar lsbf			:1;
	uchar rck			:1;
	uchar bck			:1;
	uchar reserved8			:1;

	uchar wr_max_speed		[2];			/* 18 */
	uchar wr_current_speed		[2];

	uchar copy_mngt_rev 		[2];			/* 22 */

	uchar reserved9 		[3];

	uchar reserved10		:6;			/* 27 */
	uchar current_rot_ctl		:2;

	uchar wr_selected_speed		[2];
	uchar wr_speed_desc_num		[2];

	BraseroScsiStatusWrSpdDesc wr_spd_desc [0];	/* 32 */
};

#endif

typedef struct _BraseroScsiStatusPage BraseroScsiStatusPage;

G_END_DECLS

#endif /* _BURN_STATUS_PAGE_H */

 
