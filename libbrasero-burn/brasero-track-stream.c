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

#include "burn-debug.h"
#include "burn-basics.h"
#include "brasero-track-stream.h"

typedef struct _BraseroTrackStreamPrivate BraseroTrackStreamPrivate;
struct _BraseroTrackStreamPrivate
{
	gchar *uri;

	BraseroStreamFormat format;

	guint64 gap;
	guint64 start;
	guint64 end;

	BraseroStreamInfo *info;
};

#define BRASERO_TRACK_STREAM_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_TRACK_STREAM, BraseroTrackStreamPrivate))

G_DEFINE_TYPE (BraseroTrackStream, brasero_track_stream, G_TYPE_OBJECT);

void
brasero_stream_info_free (BraseroStreamInfo *info)
{
	if (!info)
		return;

	g_free (info->title);
	g_free (info->artist);
	g_free (info->composer);
	g_free (info);
}

BraseroStreamInfo *
brasero_stream_info_copy (BraseroStreamInfo *info)
{
	BraseroStreamInfo *copy;

	if (!info)
		return NULL;

	copy = g_new0 (BraseroStreamInfo, 1);

	copy->title = g_strdup (info->title);
	copy->artist = g_strdup (info->artist);
	copy->composer = g_strdup (info->composer);
	copy->isrc = info->isrc;

	return copy;
}

BraseroBurnResult
brasero_track_stream_set_source (BraseroTrackStream *track,
				 const gchar *uri)
{
	BraseroTrackStreamPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK_STREAM (track), BRASERO_BURN_ERR);

	priv = BRASERO_TRACK_STREAM_PRIVATE (track);

	if (priv->uri)
		g_free (priv->uri);

	priv->uri = g_strdup (uri);
	brasero_track_changed (BRASERO_TRACK (track));

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_track_stream_set_format (BraseroTrackStream *track,
				 BraseroStreamFormat format)
{
	BraseroTrackStreamPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK_STREAM (track), BRASERO_BURN_ERR);

	priv = BRASERO_TRACK_STREAM_PRIVATE (track);

	if (format == BRASERO_AUDIO_FORMAT_NONE)
		BRASERO_BURN_LOG ("Setting a NONE audio format with a valid uri");

	priv->format = format;
	brasero_track_changed (BRASERO_TRACK (track));

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_track_stream_set_info (BraseroTrackStream *track,
			       BraseroStreamInfo *info)
{
	BraseroTrackStreamPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK_STREAM (track), BRASERO_BURN_ERR);

	priv = BRASERO_TRACK_STREAM_PRIVATE (track);

	if (priv->info)
		brasero_stream_info_free (priv->info);

	priv->info = info;
	brasero_track_changed (BRASERO_TRACK (track));

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_track_stream_set_boundaries (BraseroTrackStream *track,
				     gint64 start,
				     gint64 end,
				     gint64 gap)
{
	BraseroTrackStreamPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK_STREAM (track), BRASERO_BURN_ERR);

	priv = BRASERO_TRACK_STREAM_PRIVATE (track);

	if (gap >= 0)
		priv->gap = gap;

	if (end > 0)
		priv->end = end;

	if (start >= 0)
		priv->start = start;

	brasero_track_changed (BRASERO_TRACK (track));

	return BRASERO_BURN_OK;
}

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

guint64
brasero_track_stream_get_gap (BraseroTrackStream *track)
{
	BraseroTrackStreamPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK_STREAM (track), 0);

	priv = BRASERO_TRACK_STREAM_PRIVATE (track);
	return priv->gap;
}

guint64
brasero_track_stream_get_start (BraseroTrackStream *track)
{
	BraseroTrackStreamPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK_STREAM (track), 0);

	priv = BRASERO_TRACK_STREAM_PRIVATE (track);
	return priv->start;
}

guint64
brasero_track_stream_get_end (BraseroTrackStream *track)
{
	BraseroTrackStreamPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK_STREAM (track), 0);

	priv = BRASERO_TRACK_STREAM_PRIVATE (track);
	return priv->end;
}

/* FIXME: This is bad */
BraseroStreamInfo *
brasero_track_stream_get_info (BraseroTrackStream *track)
{
	BraseroTrackStreamPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK_STREAM (track), 0);

	priv = BRASERO_TRACK_STREAM_PRIVATE (track);
	return priv->info;
}

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
			       guint64 *blocks,
			       guint *block_size)
{
	BraseroTrackStreamPrivate *priv;

	priv = BRASERO_TRACK_STREAM_PRIVATE (track);

	if (blocks) {
		guint64 length = 0;

		brasero_track_stream_get_length (BRASERO_TRACK_STREAM (track), &length);
		*blocks = length * 75LL / 1000000000LL;
	}

	if (block_size)
		*block_size = 2352;

	return BRASERO_BURN_OK;
}

static BraseroTrackDataType
brasero_track_stream_get_track_type (BraseroTrack *track,
				     BraseroTrackType *type)
{
	BraseroTrackStreamPrivate *priv;

	priv = BRASERO_TRACK_STREAM_PRIVATE (track);

	if (!type)
		return BRASERO_TRACK_TYPE_STREAM;

	type->type = BRASERO_TRACK_TYPE_STREAM;
	type->subtype.audio_format = priv->format;

	return BRASERO_TRACK_TYPE_STREAM;
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

	if (priv->info) {
		brasero_stream_info_free (priv->info);
		priv->info = NULL;
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
	track_class->get_type = brasero_track_stream_get_track_type;
}

BraseroTrackStream *
brasero_track_stream_new (void)
{
	return g_object_new (BRASERO_TYPE_TRACK_STREAM, NULL);
}
