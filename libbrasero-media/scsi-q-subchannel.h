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

#ifndef _SCSI_Q_SUBCHANNEL_H
#define _SCSI_Q_SUBCHANNEL_H

G_BEGIN_DECLS

/**
 * This is for the program area (1, 2, 3) and the leadout (1, 2)
 */

typedef enum {
BRASERO_SCSI_Q_SUB_CHANNEL_NO_MODE		= 0x00,
BRASERO_SCSI_Q_SUB_CHANNEL_CURRENT_POSITION	= 0x01,

/* Media Catalog Number */
BRASERO_SCSI_Q_SUB_CHANNEL_MCN			= 0x02,

/* International Standard Recording Code */ 
BRASERO_SCSI_Q_SUB_CHANNEL_ISRC			= 0x03,
} BraseroScsiQSubChannelMode;


/**
 * This is for Q-sub-channel in leadin.
 * Indicate how to read the fields after POINT
 */

typedef enum {
/* MIN, SEC, FRAME = time in leadin
 * PMIN, PSEC, PFRAME = track start time (BCD)
 * NOTE: In fact it's from O to 99
 */
BRASERO_SCSI_Q_SUB_CHANNEL_TRACK_START			= 0x63,

/* MIN, SEC, FRAME = time in leadin
 * PMIN = track num of first track (BCD)
 * PSEC = program area format
 * PFRAME = 0
 */
BRASERO_SCSI_Q_SUB_CHANNEL_FIRST_TRACK_NUM_DATA_FORMAT	= 0xA0,

/* MIN, SEC, FRAME = time in leadin
 * PMIN = track num of last track in program area (BCD)
 * PSEC, PFRAME = 0
 */
BRASERO_SCSI_Q_SUB_CHANNEL_LAST_TRACK_NUM		= 0xA1,

/* MIN, SEC, FRAME = time in leadin
 * PMIN, PSEC, PFRAME = start time of leadout (BCD)
 */
BRASERO_SCSI_Q_SUB_CHANNEL_LEADOUT_START		= 0xA2,
} BraseroScsiQSubChannelLeadinMode1;

typedef enum {
/* Multisession discs */

/* MIN, SEC, FRAME = start time for next possible session
 * ZERO = number of different mode-5 pointers
 * PMIN, PSEC, PFRAME = the max possible start time for last leadout
 */
BRASERO_SCSI_Q_SUB_CHANNEL_MULTI_NEXT_SESSION		= 0xB0,

/* MIN, SEC, FRAME = ATIP special informations
 * ZERO = 0
 * PMIN, PSEC, PFRAME = start time of the first leadin
 */
BRASERO_SCSI_Q_SUB_CHANNEL_MULTI_FIRST_LEADIN		= 0xC0,

/* Audio only */
BRASERO_SCSI_Q_SUB_CHANNEL_PLAYBACK_SKIPS_1_40		= 0x28,

BRASERO_SCSI_Q_SUB_CHANNEL_SKIP_INTERVALS		= 0xB1,
BRASERO_SCSI_Q_SUB_CHANNEL_SKIP_TRACK_1			= 0xB2,
BRASERO_SCSI_Q_SUB_CHANNEL_SKIP_TRACK_2			= 0xB3,
BRASERO_SCSI_Q_SUB_CHANNEL_SKIP_TRACK_3			= 0xB4
} BraseroScsiQSubChannelLeadinMode5;


/* NOTE: the real meaning of the following values is
 * determined by point member see above */
typedef enum {
BRASERO_SCSI_Q_SUB_CHANNEL_LEADIN_NO_MODE	= 0x00,
BRASERO_SCSI_Q_SUB_CHANNEL_LEADIN_MODE1		= 0x01,
BRASERO_SCSI_Q_SUB_CHANNEL_LEADIN_MODE2		= 0x01,
BRASERO_SCSI_Q_SUB_CHANNEL_LEADIN_MODE5		= 0x05
} BraseroScsiQSubChannelLeadinMode;


/**
 * This is for subchannels in pma
 */

typedef enum {
/* TNO                = 00
 * POINT              = Track number encoded as two BCD digits.
 * ZERO               = 00-09bcd is a label of the frame number in the PMA unity
 * MIN, SEC, FRAME    = Track stop time in 6 BCD digits.
 * PMIN, PSEC, PFRAME = Track start time in 6 BCD digits
 */
BRASERO_SCSI_Q_SUB_CHANNEL_PMA_TOC		= 0x01,

/* TNO             = 00
 * POINT           = 00
 * ZERO            = 00-09bcd is a label of the frame number in the PMA unity
 * MIN, SEC, FRAME = Disc identification as a 6 BCD digit number.
 * PMIN            = 00
 * PSEC            = Sessions format: 00 – CD-DA or CD-ROM, 10 – CD-I, 20 – CD-ROM-XA
 * PFRAME          = 00
 */
BRASERO_SCSI_Q_SUB_CHANNEL_PMA_DISC_ID		= 0x02,

/* TNO                    = 00
 * POINT                  = 01-21bcd is the mode-3 index of this item
 * ZERO                   = 00-09bcd is a label of the frame number in the PMA unity
 * MIN                    = 01-99bcd track number to skip upon playback
 * Each of the following: = 00 if no skip track is specified
 * SEC, FRAME             = 01-99bcd (each byte) track number to skip upon playback
 * PMIN, PSEC, PFRAME
 */
BRASERO_SCSI_Q_SUB_CHANNEL_PMA_SKIP_TRACK	= 0x03,

/* TNO                    = 00
 * POINT                  = 01-21bcd is the mode-4 index of this item
 * ZERO                   = 00-09bcd is a label of the frame number in the PMA unity
 * MIN                    = 01-99bcd track number to unskip upon playback
 * Each of the following: = 00 if no unskip track is specified
 * SEC, FRAME             = 01-99bcd (each byte) track number to unskip upon playback
 * PMIN, PSEC, PFRAME
 */
BRASERO_SCSI_Q_SUB_CHANNEL_PMA_UNSKIP		= 0x04,

/* TNO                = 00
 * POINT              = 01-40bcd is the mode-5 index of this item
 * ZERO               = 00-09bcd is a label of the frame number in the PMA unity
 * MIN, SEC, FRAME    = Skip interval stop time in 6 BCD digits.
 * PMIN, PSEC, PFRAME = Skip interval start time in 6 BCD digits.
 */
BRASERO_SCSI_Q_SUB_CHANNEL_PMA_SKIP_INTERVAL	= 0x05,

/* TNO                = 00
 * POINT              = 01-40bcd is the mode-6 index of this item
 * ZERO               = 00-09bcd is a label of the frame number in the PMA unity
 * MIN, SEC, FRAME    = Unskip interval stop time in 6 BCD digits.
 * PMIN, PSEC, PFRAME = Unskip interval start time in 6 BCD digits.
 */
BRASERO_SCSI_Q_SUB_CHANNEL_PMA_UNSKIP_INTERVAL	= 0x06,
} BraseroScsiQSubChannelPmaMode;

/* This is usually the control field in structures related to Q Sub-Channel */
typedef enum {
BRASERO_SCSI_TRACK_AUDIO			= 0x00,
BRASERO_SCSI_TRACK_PREEMP			= 0x01,
BRASERO_SCSI_TRACK_4_CHANNELS			= 0x08,

BRASERO_SCSI_TRACK_DATA				= 0x04,
BRASERO_SCSI_TRACK_DATA_INCREMENTAL		= 0x01,

BRASERO_SCSI_TRACK_COPY				= 0x02,

} BraseroScsiTrackMode;

G_END_DECLS

#endif /* _SCSI_Q_SUBCHANNEL_H */

 
