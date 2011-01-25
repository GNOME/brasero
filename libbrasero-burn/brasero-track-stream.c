/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-burn
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-burn is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-burn authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-burn. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-burn is distributed in the hope that it will be useful,
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include "burn-debug.h"
#include "burn-basics.h"
#include "brasero-track-stream.h"

typedef struct _BraseroTrackStreamPrivate BraseroTrackStreamPrivate;
struct _BraseroTrackStreamPrivate
{
        GFileMonitor *monitor;
	gchar *uri;

	BraseroStreamFormat format;

	guint64 gap;
	guint64 start;
	guint64 end;
};

#define BRASERO_TRACK_STREAM_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_TRACK_STREAM, BraseroTrackStreamPrivate))

G_DEFINE_TYPE (BraseroTrackStream, brasero_track_stream, BRASERO_TYPE_TRACK);

static BraseroBurnResult
brasero_track_stream_set_source_real (BraseroTrackStream *track,
				      const gchar *uri)
{
	BraseroTrackStreamPrivate *priv;

	priv = BRASERO_TRACK_STREAM_PRIVATE (track);

	if (priv->uri)
		g_free (priv->uri);

	priv->uri = g_strdup (uri);

	/* Since that's a new URI chances are, the end point is different */
	priv->end = 0;

	return BRASERO_BURN_OK;
}

/**
 * brasero_track_stream_set_source:
 * @track: a #BraseroTrackStream
 * @uri: a #gchar
 *
 * Sets the stream (song or video) uri.
 *
 * Note: it resets the end point of the track to 0 but keeps start point and gap
 * unchanged.
 *
 * Return value: a #BraseroBurnResult. BRASERO_BURN_OK if it is successful.
 **/

BraseroBurnResult
brasero_track_stream_set_source (BraseroTrackStream *track,
				 const gchar *uri)
{
	BraseroTrackStreamClass *klass;
	BraseroBurnResult res;

	g_return_val_if_fail (BRASERO_IS_TRACK_STREAM (track), BRASERO_BURN_ERR);

	klass = BRASERO_TRACK_STREAM_GET_CLASS (track);
	if (!klass->set_source)
		return BRASERO_BURN_ERR;

	res = klass->set_source (track, uri);
	if (res != BRASERO_BURN_OK)
		return res;

	brasero_track_changed (BRASERO_TRACK (track));
	return BRASERO_BURN_OK;
}

/**
 * brasero_track_stream_get_format:
 * @track: a #BraseroTrackStream
 *
 * This function returns the format of the stream.
 *
 * Return value: a #BraseroStreamFormat.
 **/

BraseroStreamFormat
brasero_track_stream_get_format (BraseroTrackStream *track)
{
	BraseroTrackStreamPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK_STREAM (track), BRASERO_AUDIO_FORMAT_NONE);

	priv = BRASERO_TRACK_STREAM_PRIVATE (track);

	return priv->format;
}

static BraseroBurnResult
brasero_track_stream_set_format_real (BraseroTrackStream *track,
				      BraseroStreamFormat format)
{
	BraseroTrackStreamPrivate *priv;

	priv = BRASERO_TRACK_STREAM_PRIVATE (track);

	if (format == BRASERO_AUDIO_FORMAT_NONE)
		BRASERO_BURN_LOG ("Setting a NONE audio format with a valid uri");

	priv->format = format;
	return BRASERO_BURN_OK;
}

/**
 * brasero_track_stream_set_format:
 * @track: a #BraseroTrackStream
 * @format: a #BraseroStreamFormat
 *
 * Sets the format of the stream.
 *
 * Return value: a #BraseroBurnResult. BRASERO_BURN_OK if it is successful.
 **/

BraseroBurnResult
brasero_track_stream_set_format (BraseroTrackStream *track,
				 BraseroStreamFormat format)
{
	BraseroTrackStreamClass *klass;
	BraseroBurnResult res;

	g_return_val_if_fail (BRASERO_IS_TRACK_STREAM (track), BRASERO_BURN_ERR);

	klass = BRASERO_TRACK_STREAM_GET_CLASS (track);
	if (!klass->set_format)
		return BRASERO_BURN_ERR;

	res = klass->set_format (track, format);
	if (res != BRASERO_BURN_OK)
		return res;

	brasero_track_changed (BRASERO_TRACK (track));
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_track_stream_set_boundaries_real (BraseroTrackStream *track,
					  gint64 start,
					  gint64 end,
					  gint64 gap)
{
	BraseroTrackStreamPrivate *priv;

	priv = BRASERO_TRACK_STREAM_PRIVATE (track);

	if (gap >= 0)
		priv->gap = gap;

	if (end > 0)
		priv->end = end;

	if (start >= 0)
		priv->start = start;

	return BRASERO_BURN_OK;
}

/**
 * brasero_track_stream_set_boundaries:
 * @track: a #BraseroTrackStream
 * @start: a #gint64 or -1 to ignore
 * @end: a #gint64 or -1 to ignore
 * @gap: a #gint64 or -1 to ignore
 *
 * Sets the boundaries of the stream (where it starts, ends in the file;
 * how long is the gap with the next track) in nano seconds.
 *
 * Return value: a #BraseroBurnResult. BRASERO_BURN_OK if it is successful.
 **/

BraseroBurnResult
brasero_track_stream_set_boundaries (BraseroTrackStream *track,
				     gint64 start,
				     gint64 end,
				     gint64 gap)
{
	BraseroTrackStreamClass *klass;
	BraseroBurnResult res;

	g_return_val_if_fail (BRASERO_IS_TRACK_STREAM (track), BRASERO_BURN_ERR);

	klass = BRASERO_TRACK_STREAM_GET_CLASS (track);
	if (!klass->set_boundaries)
		return BRASERO_BURN_ERR;

	res = klass->set_boundaries (track, start, end, gap);
	if (res != BRASERO_BURN_OK)
		return res;

	brasero_track_changed (BRASERO_TRACK (track));
	return BRASERO_BURN_OK;
}

/**
 * brasero_track_stream_get_source:
 * @track: a #BraseroTrackStream
 * @uri: a #gboolean
 *
 * This function returns the path or the URI (if @uri is TRUE)
 * of the stream (song or video file).
 *
 * Note: this function resets any length previously set to 0.
 * Return value: a #gchar.
 **/

gchar *
brasero_track_stream_get_source (BraseroTrackStream *track,
				 gboolean uri)
{
	BraseroTrackStreamPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK_STREAM (track), NULL);

	priv = BRASERO_TRACK_STREAM_PRIVATE (track);
	if (uri)
		return brasero_string_get_uri (priv->uri);
	else
		return brasero_string_get_localpath (priv->uri);
}

/**
 * brasero_track_stream_get_gap:
 * @track: a #BraseroTrackStream
 *
 * This function returns length of the gap (in nano seconds).
 *
 * Return value: a #guint64.
 **/

guint64
brasero_track_stream_get_gap (BraseroTrackStream *track)
{
	BraseroTrackStreamPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK_STREAM (track), 0);

	priv = BRASERO_TRACK_STREAM_PRIVATE (track);
	return priv->gap;
}

/**
 * brasero_track_stream_get_start:
 * @track: a #BraseroTrackStream
 *
 * This function returns start time in the stream (in nano seconds).
 *
 * Return value: a #guint64.
 **/

guint64
brasero_track_stream_get_start (BraseroTrackStream *track)
{
	BraseroTrackStreamPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK_STREAM (track), 0);

	priv = BRASERO_TRACK_STREAM_PRIVATE (track);
	return priv->start;
}

/**
 * brasero_track_stream_get_end:
 * @track: a #BraseroTrackStream
 *
 * This function returns end time in the stream (in nano seconds).
 *
 * Return value: a #guint64.
 **/

guint64
brasero_track_stream_get_end (BraseroTrackStream *track)
{
	BraseroTrackStreamPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK_STREAM (track), 0);

	priv = BRASERO_TRACK_STREAM_PRIVATE (track);
	return priv->end;
}

/**
 * brasero_track_stream_get_length:
 * @track: a #BraseroTrackStream
 * @length: a #guint64
 *
 * This function returns the length of the stream (in nano seconds)
 * taking into account the start and end time as well as the length
 * of the gap. It stores it in @length.
 *
 * Return value: a #BraseroBurnResult. BRASERO_BURN_OK if @length was set.
 **/

BraseroBurnResult
brasero_track_stream_get_length (BraseroTrackStream *track,
				 guint64 *length)
{
	BraseroTrackStreamPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK_STREAM (track), BRASERO_BURN_ERR);

	priv = BRASERO_TRACK_STREAM_PRIVATE (track);

	if (priv->start < 0 || priv->end <= 0)
		return BRASERO_BURN_ERR;

	*length = BRASERO_STREAM_LENGTH (priv->start, priv->end + priv->gap);
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_track_stream_get_size (BraseroTrack *track,
			       goffset *blocks,
			       goffset *block_size)
{
	BraseroStreamFormat format;

	format = brasero_track_stream_get_format (BRASERO_TRACK_STREAM (track));
	if (!BRASERO_STREAM_FORMAT_HAS_VIDEO (format)) {
		if (blocks) {
			guint64 length = 0;

			brasero_track_stream_get_length (BRASERO_TRACK_STREAM (track), &length);
			*blocks = length * 75LL / 1000000000LL;
		}

		if (block_size)
			*block_size = 2352;
	}
	else {
		if (blocks) {
			guint64 length = 0;

			/* This is based on a simple formula:
			 * 4700000000 bytes means 2 hours */
			brasero_track_stream_get_length (BRASERO_TRACK_STREAM (track), &length);
			*blocks = length * 47LL / 72000LL / 2048LL;
		}

		if (block_size)
			*block_size = 2048;
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_track_stream_get_status (BraseroTrack *track,
				 BraseroStatus *status)
{
	BraseroTrackStreamPrivate *priv;

	priv = BRASERO_TRACK_STREAM_PRIVATE (track);
	if (!priv->uri) {
		if (status)
			brasero_status_set_error (status,
						  g_error_new (BRASERO_BURN_ERROR,
							       BRASERO_BURN_ERROR_EMPTY,
							       _("There are no files to write to disc")));

		return BRASERO_BURN_ERR;
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_track_stream_get_track_type (BraseroTrack *track,
				     BraseroTrackType *type)
{
	BraseroTrackStreamPrivate *priv;

	priv = BRASERO_TRACK_STREAM_PRIVATE (track);

	brasero_track_type_set_has_stream (type);
	brasero_track_type_set_stream_format (type, priv->format);

	return BRASERO_BURN_OK;
}

static void
brasero_track_stream_init (BraseroTrackStream *object)
{ }

static void
brasero_track_stream_finalize (GObject *object)
{
	BraseroTrackStreamPrivate *priv;

	priv = BRASERO_TRACK_STREAM_PRIVATE (object);
	if (priv->uri) {
		g_free (priv->uri);
		priv->uri = NULL;
	}

	G_OBJECT_CLASS (brasero_track_stream_parent_class)->finalize (object);
}

static void
brasero_track_stream_class_init (BraseroTrackStreamClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroTrackClass *track_class = BRASERO_TRACK_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroTrackStreamPrivate));

	object_class->finalize = brasero_track_stream_finalize;

	track_class->get_size = brasero_track_stream_get_size;
	track_class->get_status = brasero_track_stream_get_status;
	track_class->get_type = brasero_track_stream_get_track_type;

	klass->set_source = brasero_track_stream_set_source_real;
	klass->set_format = brasero_track_stream_set_format_real;
	klass->set_boundaries = brasero_track_stream_set_boundaries_real;
}

/**
 * brasero_track_stream_new:
 *
 *  Creates a new #BraseroTrackStream object.
 *
 * This type of tracks is used to burn audio or
 * video files.
 *
 * Return value: a #BraseroTrackStream object.
 **/

BraseroTrackStream *
brasero_track_stream_new (void)
{
	return g_object_new (BRASERO_TYPE_TRACK_STREAM, NULL);
}
