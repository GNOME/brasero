/***************************************************************************
 *            burn-track.h
 *
 *  Thu Dec  7 09:51:03 2006
 *  Copyright  2006  algernon
 *  <algernon@localhost.localdomain>
 ****************************************************************************/

/*
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

#include <glib.h>

#include "burn-basics.h"
#include "burn-medium.h"

#ifndef _BURN_TRACK_H
#define _BURN_TRACK_H

G_BEGIN_DECLS

/* NOTE: the order has a meaning here and is used for sorting */
typedef enum {
	BRASERO_TRACK_TYPE_NONE				= 0,
	BRASERO_TRACK_TYPE_AUDIO,
	BRASERO_TRACK_TYPE_DATA,
	BRASERO_TRACK_TYPE_IMAGE,
	BRASERO_TRACK_TYPE_DISC,
} BraseroTrackDataType;

typedef enum {
	BRASERO_IMAGE_FORMAT_NONE		= 0,
	BRASERO_IMAGE_FORMAT_BIN		= 1,
	BRASERO_IMAGE_FORMAT_CUE		= 1 << 1,
	BRASERO_IMAGE_FORMAT_CLONE		= 1 << 2,
	BRASERO_IMAGE_FORMAT_CDRDAO		= 1 << 3,
	BRASERO_IMAGE_FORMAT_ANY		= BRASERO_IMAGE_FORMAT_BIN|
						  BRASERO_IMAGE_FORMAT_CUE|
						  BRASERO_IMAGE_FORMAT_CDRDAO|
						  BRASERO_IMAGE_FORMAT_CLONE,
} BraseroImageFormat;

typedef enum {
	BRASERO_IMAGE_FS_NONE			= 0,
	BRASERO_IMAGE_FS_ISO			= 1,
	BRASERO_IMAGE_FS_UDF			= 1 << 1,
	BRASERO_IMAGE_FS_JOLIET			= 1 << 2,
	BRASERO_IMAGE_FS_VIDEO			= 1 << 3,
	BRASERO_IMAGE_FS_ANY			= BRASERO_IMAGE_FS_ISO|
						  BRASERO_IMAGE_FS_UDF|
						  BRASERO_IMAGE_FS_JOLIET|
						  BRASERO_IMAGE_FS_VIDEO
} BraseroImageFS;

typedef enum {
	BRASERO_AUDIO_FORMAT_NONE		= 0,
	BRASERO_AUDIO_FORMAT_UNDEFINED		= 1,
	BRASERO_AUDIO_FORMAT_4_CHANNEL		= 1 << 1,
	BRASERO_AUDIO_FORMAT_RAW		= 1 << 2
} BraseroAudioFormat;

typedef enum {
	BRASERO_CHECKSUM_NONE			= 0,
	BRASERO_CHECKSUM_MD5			= 1,
	BRASERO_CHECKSUM_MD5_FILE		= 1 << 1,
	BRASERO_CHECKSUM_ANY			= BRASERO_CHECKSUM_MD5
} BraseroChecksumType;

/**
 *
 */

struct _BraseroGraftPt {
	gchar *uri;
	gchar *path;

	/* list of uris (char *) that are to be excluded for this path only */
	GSList *excluded;
};
typedef struct _BraseroGraftPt BraseroGraftPt;

void
brasero_graft_point_free (BraseroGraftPt *graft);

BraseroGraftPt *
brasero_graft_point_copy (BraseroGraftPt *graft);

/**
 *
 */

struct _BraseroSongInfo {
	gchar *title;
	gchar *artist;
	gchar *composer;
	gint isrc;
};

typedef struct _BraseroSongInfo BraseroSongInfo;

void
brasero_song_info_free (BraseroSongInfo *info);

BraseroSongInfo *
brasero_song_info_copy (BraseroSongInfo *info);

/**
 *
 */

typedef struct _BraseroTrack BraseroTrack;

struct _BraseroTrackType {
	BraseroTrackDataType type;
	union {
		BraseroImageFormat img_format;		/* used with IMAGE type */
		BraseroMedia media;		/* used with DISC types */
		BraseroImageFS fs_type;
		BraseroAudioFormat audio_format;
		BraseroChecksumType checksum;
	} subtype;
};
typedef struct _BraseroTrackType BraseroTrackType;

gboolean
brasero_track_type_equal (const BraseroTrackType *type_A,
			  const BraseroTrackType *type_B);

/**
 *
 */

BraseroTrack *
brasero_track_new (BraseroTrackDataType type);

void
brasero_track_ref (BraseroTrack *track);

void
brasero_track_unref (BraseroTrack *track);

BraseroTrackDataType
brasero_track_get_type (BraseroTrack *track,
			BraseroTrackType *type);

BraseroTrack *
brasero_track_copy (BraseroTrack *track);

/**
 * Functions to set the track contents
 */

BraseroBurnResult
brasero_track_set_audio_source (BraseroTrack *track,
				const gchar *uri,
				BraseroAudioFormat format);

BraseroBurnResult
brasero_track_set_audio_info (BraseroTrack *track,
			      BraseroSongInfo *info);

BraseroBurnResult
brasero_track_set_audio_boundaries (BraseroTrack *track,
				    gint64 start,
				    gint64 gap);

BraseroBurnResult
brasero_track_set_data_source (BraseroTrack *track,
			       GSList *grafts,
			       GSList *unreadable);

BraseroBurnResult
brasero_track_set_data_fs (BraseroTrack *track,
			   BraseroImageFS fstype);
BraseroBurnResult
brasero_track_add_data_fs (BraseroTrack *track,
			   BraseroImageFS fstype);
BraseroBurnResult
brasero_track_unset_data_fs (BraseroTrack *track,
			     BraseroImageFS fstype);

BraseroBurnResult
brasero_track_set_drive_source (BraseroTrack *track,
				NautilusBurnDrive *drive);

BraseroBurnResult
brasero_track_set_image_source (BraseroTrack *track,
				const gchar *image,
				const gchar *toc,
				BraseroImageFormat format);


/**
 * Function to get the track contents
 */

gchar *
brasero_track_get_audio_source (BraseroTrack *track, gboolean uri);
gint64
brasero_track_get_audio_gap (BraseroTrack *track);
gint64
brasero_track_get_audio_start (BraseroTrack *track);
BraseroSongInfo *
brasero_track_get_audio_info (BraseroTrack *track);

NautilusBurnDrive *
brasero_track_get_drive_source (BraseroTrack *track);

GSList *
brasero_track_get_data_grafts_source (BraseroTrack *track);
GSList *
brasero_track_get_data_excluded_source (BraseroTrack *track);

BraseroBurnResult
brasero_track_get_data_paths (BraseroTrack *track,
			      const gchar *grafts_path,
			      const gchar *excluded_path,
			      const gchar *emptydir,
			      GError **error);

gchar *
brasero_track_get_image_source (BraseroTrack *track, gboolean uri);
gchar *
brasero_track_get_toc_source (BraseroTrack *track, gboolean uri);

/** 
 * Allow to set and get some information about a track
 */

BraseroBurnResult
brasero_track_set_checksum (BraseroTrack *track,
			    BraseroChecksumType type,
			    const gchar *checksum);

const gchar *
brasero_track_get_checksum (BraseroTrack *track);

BraseroChecksumType
brasero_track_get_checksum_type (BraseroTrack *track);


void
brasero_track_set_estimated_size (BraseroTrack *track,
				  gint64 block_size,
				  gint64 blocks,
				  gint64 size);
BraseroBurnResult
brasero_track_get_estimated_size (BraseroTrack *track,
				  gint64 *block_size,
				  gint64 *blocks,
				  gint64 *size);

G_END_DECLS

#endif /* _BURN_TRACK_H */

 
