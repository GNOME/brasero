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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 
/***************************************************************************
 *            burn-basics.h
 *
 *  Sat Feb 11 16:49:00 2006
 *  Copyright  2006  philippe
 *  <brasero-app@wanadoo.fr>
 ****************************************************************************/
#ifndef _BURN_BASICS_H
#define _BURN_BASICS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <glib.h>

#include <nautilus-burn-drive.h>

#include <libgnomevfs/gnome-vfs.h>

#define CDR_SPEED 153600
#define DVD_SPEED 1385000

#define BRASERO_GET_BASENAME_FOR_DISPLAY(uri, name)				\
{										\
    	gchar *escaped_basename;						\
	escaped_basename = g_path_get_basename (uri);				\
    	name = gnome_vfs_unescape_string_for_display (escaped_basename);	\
	g_free (escaped_basename);						\
}

GQuark brasero_burn_quark (void);
#define BRASERO_BURN_ERROR brasero_burn_quark()

#define BRASERO_BURN_LOG_DOMAIN				"BraseroBurn"
#define BRASERO_BURN_LOG(format, ...)				\
		g_log (BRASERO_BURN_LOG_DOMAIN,			\
		       G_LOG_LEVEL_DEBUG,			\
		       format,					\
		       ##__VA_ARGS__);
#define BRASERO_BURN_LOGV(format)				\
	{							\
		va_list arg_list;				\
		va_start (arg_list, format);			\
		g_logv (BRASERO_BURN_LOG_DOMAIN,		\
			G_LOG_LEVEL_DEBUG,			\
			format,					\
			arg_list);				\
		va_end (arg_list);				\
	}
	
#define BRASERO_BURN_TMP_FILE_NAME		"brasero_tmp_XXXXXX"

typedef enum {
	BRASERO_BURN_ERROR_NONE,
	BRASERO_BURN_ERROR_GENERAL,

	BRASERO_BURN_ERROR_TMP_DIR,
	BRASERO_BURN_ERROR_FILE_EXIST,
	BRASERO_BURN_ERROR_INVALID_FILE,
	BRASERO_BURN_ERROR_INCOMPATIBLE_FORMAT,
	BRASERO_BURN_ERROR_JOLIET_TREE,

	BRASERO_BURN_ERROR_SCSI_IOCTL,
	BRASERO_BURN_ERROR_SLOW_DMA,
	BRASERO_BURN_ERROR_PERMISSION,
	BRASERO_BURN_ERROR_BUSY_DRIVE,

	BRASERO_BURN_ERROR_DISC_SPACE,

	BRASERO_BURN_ERROR_MEDIA_SPACE,
	BRASERO_BURN_ERROR_RELOAD_MEDIA,
	BRASERO_BURN_ERROR_MEDIA_BLANK,
	BRASERO_BURN_ERROR_MEDIA_BUSY,
	BRASERO_BURN_ERROR_MEDIA_NONE,
	BRASERO_BURN_ERROR_MEDIA_UNSUPPORTED,
	BRASERO_BURN_ERROR_MEDIA_NOT_REWRITABLE,
	BRASERO_BURN_ERROR_MEDIA_NOT_WRITABLE,
	BRASERO_BURN_ERROR_DVD_NOT_SUPPORTED,
	BRASERO_BURN_ERROR_CD_NOT_SUPPORTED,
} BraseroBurnError;

typedef enum {
	BRASERO_BURN_OK,
	BRASERO_BURN_ERR,
	BRASERO_BURN_RETRY,
	BRASERO_BURN_CANCEL,
	BRASERO_BURN_RUNNING,
	BRASERO_BURN_DANGEROUS,
	BRASERO_BURN_NOT_READY,
	BRASERO_BURN_NOT_RUNNING,
	BRASERO_BURN_NEED_RELOAD,
	BRASERO_BURN_NOT_SUPPORTED,
} BraseroBurnResult;

typedef enum {
	BRASERO_BURN_ACTION_NONE		= 0,
	BRASERO_BURN_ACTION_GETTING_SIZE,
	BRASERO_BURN_ACTION_CHECKSUM,
	BRASERO_BURN_ACTION_CREATING_IMAGE,
	BRASERO_BURN_ACTION_DRIVE_COPY,
	BRASERO_BURN_ACTION_FILE_COPY,
	BRASERO_BURN_ACTION_ANALYSING,
	BRASERO_BURN_ACTION_TRANSCODING,
	BRASERO_BURN_ACTION_PREPARING,
	BRASERO_BURN_ACTION_LEADIN,
	BRASERO_BURN_ACTION_WRITING,
	BRASERO_BURN_ACTION_WRITING_CD_TEXT,
	BRASERO_BURN_ACTION_FIXATING,
	BRASERO_BURN_ACTION_LEADOUT,
	BRASERO_BURN_ACTION_ERASING,
	BRASERO_BURN_ACTION_FINISHED,
	BRASERO_BURN_ACTION_LAST
} BraseroBurnAction;

const char *
brasero_burn_action_to_string (BraseroBurnAction action);

typedef enum {
	BRASERO_TRACK_SOURCE_DEFAULT,
	BRASERO_TRACK_SOURCE_UNKNOWN,
	BRASERO_TRACK_SOURCE_SUM,
	BRASERO_TRACK_SOURCE_SONG,		/* imager */
	BRASERO_TRACK_SOURCE_INF,		/* recorder */
	BRASERO_TRACK_SOURCE_AUDIO,		/* recorder */
	BRASERO_TRACK_SOURCE_DATA,		/* imager */
	BRASERO_TRACK_SOURCE_GRAFTS,		/* used internally mostly (must be local files) */
	BRASERO_TRACK_SOURCE_IMAGE,
	BRASERO_TRACK_SOURCE_DISC,		/* imager */
	BRASERO_TRACK_SOURCE_IMAGER,		/* used internally for on the fly burning */
} BraseroTrackSourceType;

typedef enum {
	BRASERO_IMAGE_FORMAT_NONE		= 0,
	BRASERO_IMAGE_FORMAT_JOLIET 		= 1,
	BRASERO_IMAGE_FORMAT_ISO		= 1 << 1,
	BRASERO_IMAGE_FORMAT_CUE		= 1 << 2,
	BRASERO_IMAGE_FORMAT_CLONE		= 1 << 3,
	BRASERO_IMAGE_FORMAT_CDRDAO		= 1 << 4,
	BRASERO_IMAGE_FORMAT_VIDEO		= 1 << 5,
	BRASERO_IMAGE_FORMAT_ANY		= BRASERO_IMAGE_FORMAT_JOLIET|
						  BRASERO_IMAGE_FORMAT_ISO|
						  BRASERO_IMAGE_FORMAT_CUE|
						  BRASERO_IMAGE_FORMAT_CDRDAO|
						  BRASERO_IMAGE_FORMAT_CLONE|
						  BRASERO_IMAGE_FORMAT_VIDEO,
} BraseroImageFormat;

typedef struct _BraseroImager BraseroImager;

struct _BraseroMD5 {
	guint32 A;
	guint32 B;
	guint32 C;
	guint32 D;
};
typedef struct _BraseroMD5 BraseroMD5;

struct _BraseroTrackSource {
	BraseroTrackSourceType type;
	BraseroImageFormat format;

	union {
		struct {
			gchar *label;
			GSList *grafts;			/* BraseroGraftPt *graft */
			GSList *excluded;		/* list of uris (char*) that are to be always excluded */
		} data;
		struct {
			gchar *label;
			gchar *grafts_path;
			gchar *excluded_path;
		} grafts;
		struct {
			gchar *album;
			GSList *files;			/* BraseroSongFile * */
		} songs;
		/* NOTE: _INF and _AUDIO share the same structure but the 
		 * difference is that with _INF the song are not transcoded.
		 * Files must be local. */
		struct {
			gchar *album;
			GSList *infos;			/* BraseroSongInfo * */
		} audio;
		struct {
			NautilusBurnDrive *disc;
		} drive;
		struct {
			gchar *image;
			gchar *toc;
		} image;
		struct {
			BraseroImager *obj;
		} imager;
		struct {
			BraseroMD5 md5;
		} sum;
			
	} contents;
};
typedef struct _BraseroTrackSource BraseroTrackSource;

gchar *
brasero_track_source_get_image_localpath (BraseroTrackSource *track);
gchar *
brasero_track_source_get_raw_localpath (BraseroTrackSource *track);
gchar *
brasero_track_source_get_cue_localpath (BraseroTrackSource *track);
gchar *
brasero_track_source_get_cdrdao_localpath (BraseroTrackSource *track);

void
brasero_track_source_free (BraseroTrackSource *source);
BraseroTrackSource *
brasero_track_source_copy (const BraseroTrackSource *source);

#define BRASERO_TRACK_SOURCE_ALLOW_DVD(source)	\
	(source->type != BRASERO_TRACK_SOURCE_AUDIO && \
	 source->type != BRASERO_TRACK_SOURCE_SONG && \
	 source->type != BRASERO_TRACK_SOURCE_INF && \
	(source->format == BRASERO_IMAGE_FORMAT_NONE ||	\
	(source->format & BRASERO_IMAGE_FORMAT_ISO)))

struct _BraseroGraftPt {
	gchar *uri;
	gchar *path;
	GSList *excluded; /* list of uris (char *) that are to be excluded only for this path */
};
typedef struct _BraseroGraftPt BraseroGraftPt;

void
brasero_graft_point_free (BraseroGraftPt *graft);
BraseroGraftPt *
brasero_graft_point_copy (BraseroGraftPt *graft);

struct _BraseroSongFile {
	gchar *title;
	gchar *artist;
	gchar *composer;
	gint isrc;
	gchar *uri;

	gint64 gap;
};
typedef struct _BraseroSongFile BraseroSongFile;

struct _BraseroSongInfo {
	gchar *path;
	gchar *title;
	gchar *artist;
	gchar *composer;
	gint isrc;

	gint64 duration;
	gint sectors;
};

typedef struct _BraseroSongInfo BraseroSongInfo;

void
brasero_song_info_free (BraseroSongInfo *info);

BraseroSongInfo *
brasero_song_info_copy (BraseroSongInfo *info);

#ifdef __cplusplus
}
#endif

#endif /* _BURN-BASICS_H */
 
