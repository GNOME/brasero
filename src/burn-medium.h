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
	BRASERO_MEDIUM_UNSUPPORTED		= -1,
	BRASERO_MEDIUM_NONE			= 0,
	BRASERO_MEDIUM_FILE			= 1,
	BRASERO_MEDIUM_DVD			= 1 << 1,
	BRASERO_MEDIUM_CD			= 1 << 2,
	BRASERO_MEDIUM_RAM			= 1 << 3,
	BRASERO_MEDIUM_BD			= 1 << 4,
	BRASERO_MEDIUM_BLANK			= 1 << 5,
	BRASERO_MEDIUM_HAS_DATA			= 1 << 6,
	BRASERO_MEDIUM_HAS_AUDIO		= 1 << 7,
	BRASERO_MEDIUM_REWRITABLE		= 1 << 8,
	BRASERO_MEDIUM_WRITABLE			= 1 << 9,
	BRASERO_MEDIUM_APPENDABLE		= 1 << 10,
	BRASERO_MEDIUM_PLUS			= 1 << 11,
	BRASERO_MEDIUM_DL			= 1 << 12,
	BRASERO_MEDIUM_JUMP			= 1 << 13,
	BRASERO_MEDIUM_SEQUENTIAL		= 1 << 14,
	BRASERO_MEDIUM_RESTRICTED		= 1 << 15,
	BRASERO_MEDIUM_PROTECTED		= 1 << 16,
	BRASERO_MEDIUM_RANDOM			= 1 << 17

} BraseroMediumInfo;

#define BRASERO_MEDIUM_CDR		(BRASERO_MEDIUM_CD|		\
					 BRASERO_MEDIUM_WRITABLE)
#define BRASERO_MEDIUM_CDRW		(BRASERO_MEDIUM_CD|		\
					 BRASERO_MEDIUM_WRITABLE|	\
					 BRASERO_MEDIUM_REWRITABLE)
#define BRASERO_MEDIUM_DVD_RAM		(BRASERO_MEDIUM_DVD|		\
					 BRASERO_MEDIUM_RAM)
#define BRASERO_MEDIUM_DVDR		(BRASERO_MEDIUM_DVD|		\
					 BRASERO_MEDIUM_WRITABLE)
#define BRASERO_MEDIUM_DVDRW		(BRASERO_MEDIUM_DVD|		\
					 BRASERO_MEDIUM_WRITABLE|	\
					 BRASERO_MEDIUM_REWRITABLE|	\
					 BRASERO_MEDIUM_SEQUENTIAL)
#define BRASERO_MEDIUM_DVDRW_RESTRICTED	(BRASERO_MEDIUM_DVD|		\
					 BRASERO_MEDIUM_WRITABLE|	\
					 BRASERO_MEDIUM_REWRITABLE|	\
					 BRASERO_MEDIUM_RESTRICTED)
#define BRASERO_MEDIUM_DVDR_DL		(BRASERO_MEDIUM_DVD|		\
					 BRASERO_MEDIUM_WRITABLE|	\
					 BRASERO_MEDIUM_SEQUENTIAL|	\
					 BRASERO_MEDIUM_DL)
#define BRASERO_MEDIUM_DVDR_JUMP_DL	(BRASERO_MEDIUM_DVD|		\
					 BRASERO_MEDIUM_WRITABLE|	\
					 BRASERO_MEDIUM_JUMP|		\
					 BRASERO_MEDIUM_DL)
#define BRASERO_MEDIUM_DVDR_PLUS	(BRASERO_MEDIUM_DVD|		\
					 BRASERO_MEDIUM_WRITABLE|	\
					 BRASERO_MEDIUM_PLUS)
#define BRASERO_MEDIUM_DVDRW_PLUS	(BRASERO_MEDIUM_DVD|		\
					 BRASERO_MEDIUM_WRITABLE|	\
					 BRASERO_MEDIUM_REWRITABLE|	\
					 BRASERO_MEDIUM_PLUS)
#define BRASERO_MEDIUM_DVDR_PLUS_DL	(BRASERO_MEDIUM_DVD|		\
					 BRASERO_MEDIUM_WRITABLE|	\
					 BRASERO_MEDIUM_PLUS|		\
					 BRASERO_MEDIUM_DL)
#define BRASERO_MEDIUM_DVDRW_PLUS_DL	(BRASERO_MEDIUM_DVD|		\
					 BRASERO_MEDIUM_WRITABLE|	\
					 BRASERO_MEDIUM_REWRITABLE|	\
					 BRASERO_MEDIUM_PLUS|		\
					 BRASERO_MEDIUM_DL)
#define BRASERO_MEDIUM_BD_ROM		(BRASERO_MEDIUM_DVD|		\
					 BRASERO_MEDIUM_WRITABLE|	\
					 BRASERO_MEDIUM_BD)
#define BRASERO_MEDIUM_BDR		(BRASERO_MEDIUM_DVD|		\
					 BRASERO_MEDIUM_WRITABLE|	\
					 BRASERO_MEDIUM_BD)
#define BRASERO_MEDIUM_BDR_RANDOM	(BRASERO_MEDIUM_DVD|		\
					 BRASERO_MEDIUM_WRITABLE|	\
					 BRASERO_MEDIUM_RANDOM|		\
					 BRASERO_MEDIUM_BD)
#define BRASERO_MEDIUM_BDRW		(BRASERO_MEDIUM_DVD|		\
					 BRASERO_MEDIUM_WRITABLE|	\
					 BRASERO_MEDIUM_REWRITABLE|	\
					 BRASERO_MEDIUM_PLUS|		\
					 BRASERO_MEDIUM_BD)

#define BRASERO_MEDIUM_IS(media, type)	(((media)&(type))==(type))

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
	guint64 blocks_num;
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

const gchar *
brasero_medium_get_type_string (BraseroMedium *medium);

const gchar *
brasero_medium_get_icon (BraseroMedium *medium);

G_END_DECLS

#endif /* _BURN_MEDIUM_H_ */
