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

#ifndef _BRASERO_TRACK_STREAM_H_
#define _BRASERO_TRACK_STREAM_H_

#include <glib-object.h>

#include "brasero-enums.h"
#include "brasero-track.h"

G_BEGIN_DECLS

#define BRASERO_STREAM_FORMAT_AUDIO(stream_FORMAT)	((stream_FORMAT) & 0x007F)
#define BRASERO_STREAM_FORMAT_VIDEO(stream_FORMAT)	((stream_FORMAT) & 0x0380)

#define	BRASERO_MIN_STREAM_LENGTH			((gint64) 6 * 1000000000LL)
#define BRASERO_STREAM_LENGTH(start_MACRO, end_MACRO)					\
	((end_MACRO) - (start_MACRO) > BRASERO_MIN_STREAM_LENGTH) ?			\
	((end_MACRO) - (start_MACRO)) : BRASERO_MIN_STREAM_LENGTH

#define BRASERO_STREAM_TRACK_HAS_VIDEO(type_MACRO)				\
	(((type_MACRO)->type == BRASERO_TRACK_TYPE_STREAM) &&			\
	 ((type_MACRO)->subtype.audio_format & (BRASERO_VIDEO_FORMAT_UNDEFINED|	\
						BRASERO_VIDEO_FORMAT_VCD|	\
						BRASERO_VIDEO_FORMAT_VIDEO_DVD)))

struct _BraseroStreamInfo {
	gchar *title;
	gchar *artist;
	gchar *composer;
	gint isrc;
};

typedef struct _BraseroStreamInfo BraseroStreamInfo;

void
brasero_stream_info_free (BraseroStreamInfo *info);

BraseroStreamInfo *
brasero_stream_info_copy (BraseroStreamInfo *info);


#define BRASERO_TYPE_TRACK_STREAM             (brasero_track_stream_get_type ())
#define BRASERO_TRACK_STREAM(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_TRACK_STREAM, BraseroTrackStream))
#define BRASERO_TRACK_STREAM_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_TRACK_STREAM, BraseroTrackStreamClass))
#define BRASERO_IS_TRACK_STREAM(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_TRACK_STREAM))
#define BRASERO_IS_TRACK_STREAM_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_TRACK_STREAM))
#define BRASERO_TRACK_STREAM_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_TRACK_STREAM, BraseroTrackStreamClass))

typedef struct _BraseroTrackStreamClass BraseroTrackStreamClass;
typedef struct _BraseroTrackStream BraseroTrackStream;

struct _BraseroTrackStreamClass
{
	GObjectClass parent_class;
};

struct _BraseroTrackStream
{
	GObject parent_instance;
};

GType brasero_track_stream_get_type (void) G_GNUC_CONST;

BraseroTrackStream *
brasero_track_stream_new (void);

BraseroBurnResult
brasero_track_stream_set_source (BraseroTrackStream *track,
				 const gchar *uri);

BraseroBurnResult
brasero_track_stream_set_format (BraseroTrackStream *track,
				 BraseroStreamFormat format);

BraseroBurnResult
brasero_track_stream_set_info (BraseroTrackStream *track,
			       BraseroStreamInfo *info);

BraseroBurnResult
brasero_track_stream_set_boundaries (BraseroTrackStream *track,
				     gint64 start,
				     gint64 end,
				     gint64 gap);

gchar *
brasero_track_stream_get_source (BraseroTrackStream *track,
				 gboolean uri);

BraseroBurnResult
brasero_track_stream_get_length (BraseroTrackStream *track,
				 guint64 *length);

BraseroStreamInfo *
brasero_track_stream_get_info (BraseroTrackStream *track);

guint64
brasero_track_stream_get_start (BraseroTrackStream *track);

guint64
brasero_track_stream_get_end (BraseroTrackStream *track);

guint64
brasero_track_stream_get_gap (BraseroTrackStream *track);

G_END_DECLS

#endif /* _BRASERO_TRACK_STREAM_H_ */
