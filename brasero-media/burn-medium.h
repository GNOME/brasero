/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2007-2008 <bonfire-app@wanadoo.fr>
 * 
 *  Brasero is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 * brasero is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with brasero.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#include <glib-object.h>

#include "burn-media.h"

#ifndef _BURN_MEDIUM_H_
#define _BURN_MEDIUM_H_

G_BEGIN_DECLS

/* Data Transfer Speeds: rates are in KiB/sec */
/* NOTE: rates for audio and data transfer speeds are different:
 * - Data : 150 KiB/sec
 * - Audio : 172.3 KiB/sec
 * Source Wikipedia.com =)
 * Apparently most drives return rates that should be used with Audio factor
 */

#define CD_RATE 176400 /* bytes by second */
#define DVD_RATE 1387500
#define BD_RATE 4500000

typedef struct _BraseroDrive BraseroDrive;

#define BRASERO_SPEED_TO_RATE_CD(speed)		(guint) ((speed) * CD_RATE)
#define BRASERO_SPEED_TO_RATE_DVD(speed)	(guint) ((speed) * DVD_RATE)
#define BRASERO_RATE_TO_SPEED_CD(rate)		(gdouble) ((gdouble) (rate) / (gdouble) CD_RATE)
#define BRASERO_RATE_TO_SPEED_DVD(rate)		(gdouble) ((gdouble) (rate) / (gdouble) DVD_RATE)
#define BRASERO_RATE_TO_SPEED_BD(rate)		(gdouble) ((gdouble) (rate) / (gdouble) BD_RATE)

typedef enum {
	BRASERO_MEDIUM_TRACK_NONE		= 0,
	BRASERO_MEDIUM_TRACK_DATA		= 1,
	BRASERO_MEDIUM_TRACK_AUDIO		= 1 << 1,
	BRASERO_MEDIUM_TRACK_COPY		= 1 << 2,
	BRASERO_MEDIUM_TRACK_PREEMP		= 1 << 3,
	BRASERO_MEDIUM_TRACK_4_CHANNELS		= 1 << 4,
	BRASERO_MEDIUM_TRACK_INCREMENTAL	= 1 << 5,
	BRASERO_MEDIUM_TRACK_LEADOUT		= 1 << 6
} BraseroMediumTrackType;

struct _BraseroMediumTrack {
	guint session;
	BraseroMediumTrackType type;
	guint64 start;
	guint64 blocks_num;
};
typedef struct _BraseroMediumTrack BraseroMediumTrack;

#define BRASERO_TYPE_MEDIUM             (brasero_medium_get_type ())
#define BRASERO_MEDIUM(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_MEDIUM, BraseroMedium))
#define BRASERO_MEDIUM_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_MEDIUM, BraseroMediumClass))
#define BRASERO_IS_MEDIUM(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_MEDIUM))
#define BRASERO_IS_MEDIUM_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_MEDIUM))
#define BRASERO_MEDIUM_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_MEDIUM, BraseroMediumClass))

typedef struct _BraseroMediumClass BraseroMediumClass;
typedef struct _BraseroMedium BraseroMedium;

struct _BraseroMediumClass
{
	GObjectClass parent_class;
};

struct _BraseroMedium
{
	GObject parent_instance;
};

GType brasero_medium_get_type (void) G_GNUC_CONST;


BraseroMedia
brasero_medium_get_status (BraseroMedium *medium);

GSList *
brasero_medium_get_tracks (BraseroMedium *medium);

gboolean
brasero_medium_get_last_data_track_space (BraseroMedium *medium,
					  gint64 *size,
					  gint64 *blocks);

gboolean
brasero_medium_get_last_data_track_address (BraseroMedium *medium,
					    gint64 *byte,
					    gint64 *sector);
guint
brasero_medium_get_track_num (BraseroMedium *medium);

gboolean
brasero_medium_get_track_space (BraseroMedium *medium,
				guint num,
				gint64 *size,
				gint64 *blocks);

gboolean
brasero_medium_get_track_address (BraseroMedium *medium,
				  guint num,
				  gint64 *byte,
				  gint64 *sector);


gint64
brasero_medium_get_next_writable_address (BraseroMedium *medium);

gint64
brasero_medium_get_max_write_speed (BraseroMedium *medium);

gint64 *
brasero_medium_get_write_speeds (BraseroMedium *medium);

void
brasero_medium_get_free_space (BraseroMedium *medium,
			       gint64 *size,
			       gint64 *blocks);

void
brasero_medium_get_capacity (BraseroMedium *medium,
			     gint64 *size,
			     gint64 *blocks);

void
brasero_medium_get_data_size (BraseroMedium *medium,
			      gint64 *size,
			      gint64 *blocks);

gboolean
brasero_medium_can_be_rewritten (BraseroMedium *medium);

gboolean
brasero_medium_can_be_written (BraseroMedium *medium);

const gchar *
brasero_medium_get_CD_TEXT_title (BraseroMedium *medium);

const gchar *
brasero_medium_get_type_string (BraseroMedium *medium);

gchar *
brasero_medium_get_tooltip (BraseroMedium *medium);

BraseroDrive *
brasero_medium_get_drive (BraseroMedium *self);

G_END_DECLS

#endif /* _BURN_MEDIUM_H_ */
