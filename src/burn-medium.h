/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2007 <bonfire-app@wanadoo.fr>
 * 
 * brasero is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
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

#ifndef _BURN_MEDIUM_H_
#define _BURN_MEDIUM_H_

#include <glib-object.h>

#include <nautilus-burn-drive.h>

G_BEGIN_DECLS

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

BraseroMedium *
brasero_medium_new (NautilusBurnDrive *drive);

typedef enum {
	BRASERO_MEDIUM_NONE			= 0,
	BRASERO_MEDIUM_DVD			= 1,
	BRASERO_MEDIUM_CD			= 1 << 1,
	BRASERO_MEDIUM_BLANK			= 1 << 2,
	BRASERO_MEDIUM_HAS_DATA			= 1 << 3,
	BRASERO_MEDIUM_HAS_AUDIO		= 1 << 4,
	BRASERO_MEDIUM_REWRITABLE		= 1 << 5,
	BRASERO_MEDIUM_WRITABLE			= 1 << 6,
	BRASERO_MEDIUM_PLUS			= 1 << 7,
	BRASERO_MEDIUM_DL			= 1 << 8,
	BRASERO_MEDIUM_JUMP			= 1 << 9,
	BRASERO_MEDIUM_SEQUENTIAL		= 1 << 10,
	BRASERO_MEDIUM_RESTRICTED		= 1 << 11,
	BRASERO_MEDIUM_APPENDABLE		= 1 << 12,
	BRASERO_MEDIUM_PROTECTED		= 1 << 13
} BraseroMediumInfo;

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
	BraseroMediumTrackType type;
	guint64 start;
};
typedef struct _BraseroMediumTrack BraseroMediumTrack;

BraseroMediumInfo
brasero_medium_get_status (BraseroMedium *medium);

GSList *
brasero_medium_get_tracks (BraseroMedium *medium);

gint64
brasero_medium_get_last_data_track_address (BraseroMedium *medium);

gint64
brasero_medium_get_next_writable_address (BraseroMedium *medium);

gint
brasero_medium_get_max_write_speed (BraseroMedium *medium);

G_END_DECLS

#endif /* _BURN_MEDIUM_H_ */
