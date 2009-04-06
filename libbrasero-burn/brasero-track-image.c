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

#include <string.h>

#include "brasero-track-image.h"
#include "brasero-enums.h"
#include "brasero-track.h"

#include "burn-debug.h"
#include "burn-image-format.h"

typedef struct _BraseroTrackImagePrivate BraseroTrackImagePrivate;
struct _BraseroTrackImagePrivate
{
	gchar *image;
	gchar *toc;

	guint64 blocks;

	BraseroImageFormat format;
};

#define BRASERO_TRACK_IMAGE_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_TRACK_IMAGE, BraseroTrackImagePrivate))


G_DEFINE_TYPE (BraseroTrackImage, brasero_track_image, BRASERO_TYPE_TRACK);

BraseroBurnResult
brasero_track_image_set_source (BraseroTrackImage *track,
				const gchar *image,
				const gchar *toc,
				BraseroImageFormat format)
{
	BraseroTrackImagePrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK_IMAGE (track), BRASERO_BURN_ERR);

	priv = BRASERO_TRACK_IMAGE_PRIVATE (track);

	priv->format = format;

	if (priv->image)
		g_free (priv->image);

	if (priv->toc)
		g_free (priv->toc);

	priv->image = g_strdup (image);
	priv->toc = g_strdup (toc);

	brasero_track_changed (BRASERO_TRACK (track));

	return BRASERO_BURN_OK;
}

void
brasero_track_image_set_block_num (BraseroTrackImage *track,
				   guint64 blocks)
{
	BraseroTrackImagePrivate *priv;

	g_return_if_fail (BRASERO_IS_TRACK_IMAGE (track));

	priv = BRASERO_TRACK_IMAGE_PRIVATE (track);

	if (priv->blocks == blocks)
		return;

	priv->blocks = blocks;
	brasero_track_changed (BRASERO_TRACK (track));
}

gchar *
brasero_track_image_get_source (BraseroTrackImage *track,
				gboolean uri)
{
	BraseroTrackImagePrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK_IMAGE (track), NULL);

	priv = BRASERO_TRACK_IMAGE_PRIVATE (track);

	if (!priv->image) {
		gchar *complement;
		gchar *retval;
		gchar *toc;

		if (!priv->toc) {
			BRASERO_BURN_LOG ("Image nor toc were set");
			return NULL;
		}

		toc = brasero_string_get_localpath (priv->toc);
		complement = brasero_image_format_get_complement (priv->format, toc);
		g_free (toc);

		if (!complement) {
			BRASERO_BURN_LOG ("No complement could be retrieved");
			return NULL;
		}

		BRASERO_BURN_LOG ("Complement file retrieved %s", complement);
		if (uri)
			retval = brasero_string_get_uri (complement);
		else
			retval = brasero_string_get_localpath (complement);

		g_free (complement);
		return retval;
	}

	if (uri)
		return brasero_string_get_uri (priv->image);
	else
		return brasero_string_get_localpath (priv->image);
}

gchar *
brasero_track_image_get_toc_source (BraseroTrackImage *track,
				    gboolean uri)
{
	BraseroTrackImagePrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK_IMAGE (track), NULL);

	priv = BRASERO_TRACK_IMAGE_PRIVATE (track);

	/* Don't use file complement retrieval here as it's not possible */
	if (uri)
		return brasero_string_get_uri (priv->toc);
	else
		return brasero_string_get_localpath (priv->toc);
}

BraseroImageFormat
brasero_track_image_get_format (BraseroTrackImage *track)
{
	BraseroTrackImagePrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK_IMAGE (track), BRASERO_IMAGE_FORMAT_NONE);

	priv = BRASERO_TRACK_IMAGE_PRIVATE (track);
	return priv->format;
}

static BraseroTrackDataType
brasero_track_image_get_track_type (BraseroTrack *track,
				    BraseroTrackType *type)
{
	BraseroTrackImagePrivate *priv;

	priv = BRASERO_TRACK_IMAGE_PRIVATE (track);

	if (!type)
		return BRASERO_TRACK_TYPE_IMAGE;

	brasero_track_type_set_has_image (type);
	brasero_track_type_set_image_format (type, priv->format);

	return BRASERO_TRACK_TYPE_IMAGE;
}

static BraseroBurnResult
brasero_track_image_get_size (BraseroTrack *track,
			      guint64 *blocks,
			      guint *block_size)
{
	BraseroTrackImagePrivate *priv;

	priv = BRASERO_TRACK_IMAGE_PRIVATE (track);

	if (priv->format == BRASERO_IMAGE_FORMAT_BIN) {
		if (block_size)
			*block_size = 2048;
	}
	else if (priv->format == BRASERO_IMAGE_FORMAT_CLONE) {
		if (block_size)
			*block_size = 2448;
	}
	else if (priv->format == BRASERO_IMAGE_FORMAT_CDRDAO) {
		if (block_size)
			*block_size = 2352;
	}
	else if (priv->format == BRASERO_IMAGE_FORMAT_CUE) {
		if (block_size)
			*block_size = 2352;
	}
	else if (block_size)
		*block_size = 0;

	if (blocks)
		*blocks = priv->blocks;

	return BRASERO_BURN_OK;
}

static void
brasero_track_image_init (BraseroTrackImage *object)
{ }

static void
brasero_track_image_finalize (GObject *object)
{
	BraseroTrackImagePrivate *priv;

	priv = BRASERO_TRACK_IMAGE_PRIVATE (object);
	if (priv->image) {
		g_free (priv->image);
		priv->image = NULL;
	}

	if (priv->toc) {
		g_free (priv->toc);
		priv->toc = NULL;
	}

	G_OBJECT_CLASS (brasero_track_image_parent_class)->finalize (object);
}

static void
brasero_track_image_class_init (BraseroTrackImageClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	BraseroTrackClass *track_class = BRASERO_TRACK_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroTrackImagePrivate));

	object_class->finalize = brasero_track_image_finalize;

	track_class->get_size = brasero_track_image_get_size;
	track_class->get_type = brasero_track_image_get_track_type;
}

BraseroTrackImage *
brasero_track_image_new (void)
{
	return g_object_new (BRASERO_TYPE_TRACK_IMAGE, NULL);
}
