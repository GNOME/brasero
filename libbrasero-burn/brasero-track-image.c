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

static BraseroBurnResult
brasero_track_image_set_source_real (BraseroTrackImage *track,
				     const gchar *image,
				     const gchar *toc,
				     BraseroImageFormat format)
{
	BraseroTrackImagePrivate *priv;

	priv = BRASERO_TRACK_IMAGE_PRIVATE (track);

	priv->format = format;

	if (priv->image)
		g_free (priv->image);

	if (priv->toc)
		g_free (priv->toc);

	priv->image = g_strdup (image);
	priv->toc = g_strdup (toc);

	return BRASERO_BURN_OK;
}

/**
 * brasero_track_image_set_source:
 * @track: a #BraseroTrackImage
 * @image: a #gchar or NULL
 * @toc: a #gchar or NULL
 * @format: a #BraseroImageFormat
 *
 * Sets the image source path (and its toc if need be)
 * as well as its format.
 *
 * Return value: a #BraseroBurnResult. BRASERO_BURN_OK if it is successful.
 **/

BraseroBurnResult
brasero_track_image_set_source (BraseroTrackImage *track,
				const gchar *image,
				const gchar *toc,
				BraseroImageFormat format)
{
	BraseroTrackImageClass *klass;
	BraseroBurnResult res;

	g_return_val_if_fail (BRASERO_IS_TRACK_IMAGE (track), BRASERO_BURN_ERR);

	/* See if it has changed */
	klass = BRASERO_TRACK_IMAGE_GET_CLASS (track);
	if (!klass->set_source)
		return BRASERO_BURN_ERR;

	res = klass->set_source (track, image, toc, format);
	if (res != BRASERO_BURN_OK)
		return res;

	brasero_track_changed (BRASERO_TRACK (track));
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_track_image_set_block_num_real (BraseroTrackImage *track,
					goffset blocks)
{
	BraseroTrackImagePrivate *priv;

	priv = BRASERO_TRACK_IMAGE_PRIVATE (track);
	priv->blocks = blocks;
	return BRASERO_BURN_OK;
}

/**
 * brasero_track_image_set_block_num:
 * @track: a #BraseroTrackImage
 * @blocks: a #goffset
 *
 * Sets the image size (in sectors).
 *
 * Return value: a #BraseroBurnResult. BRASERO_BURN_OK if it is successful.
 **/

BraseroBurnResult
brasero_track_image_set_block_num (BraseroTrackImage *track,
				   goffset blocks)
{
	BraseroTrackImagePrivate *priv;
	BraseroTrackImageClass *klass;
	BraseroBurnResult res;

	g_return_val_if_fail (BRASERO_IS_TRACK_IMAGE (track), BRASERO_BURN_ERR);

	priv = BRASERO_TRACK_IMAGE_PRIVATE (track);
	if (priv->blocks == blocks)
		return BRASERO_BURN_OK;

	klass = BRASERO_TRACK_IMAGE_GET_CLASS (track);
	if (!klass->set_block_num)
		return BRASERO_BURN_ERR;

	res = klass->set_block_num (track, blocks);
	if (res != BRASERO_BURN_OK)
		return res;

	brasero_track_changed (BRASERO_TRACK (track));
	return BRASERO_BURN_OK;
}

/**
 * brasero_track_image_get_source:
 * @track: a #BraseroTrackImage
 * @uri: a #gboolean
 *
 * This function returns the path or the URI (if @uri is TRUE) of the
 * source image file.
 *
 * Return value: a #gchar
 **/

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

/**
 * brasero_track_image_get_toc_source:
 * @track: a #BraseroTrackImage
 * @uri: a #gboolean
 *
 * This function returns the path or the URI (if @uri is TRUE) of the
 * source toc file.
 *
 * Return value: a #gchar
 **/

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

/**
 * brasero_track_image_get_format:
 * @track: a #BraseroTrackImage
 *
 * This function returns the format of the
 * source image.
 *
 * Return value: a #BraseroImageFormat
 **/

BraseroImageFormat
brasero_track_image_get_format (BraseroTrackImage *track)
{
	BraseroTrackImagePrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK_IMAGE (track), BRASERO_IMAGE_FORMAT_NONE);

	priv = BRASERO_TRACK_IMAGE_PRIVATE (track);
	return priv->format;
}

/**
 * brasero_track_image_need_byte_swap:
 * @track: a #BraseroTrackImage
 *
 * This function returns whether the data bytes need swapping. Some .bin files
 * associated with .cue files are little endian for audio whereas they should
 * be big endian.
 *
 * Return value: a #gboolean
 **/

gboolean
brasero_track_image_need_byte_swap (BraseroTrackImage *track)
{
	BraseroTrackImagePrivate *priv;
	gchar *cueuri;
	gboolean res;

	g_return_val_if_fail (BRASERO_IS_TRACK_IMAGE (track), BRASERO_IMAGE_FORMAT_NONE);

	priv = BRASERO_TRACK_IMAGE_PRIVATE (track);
	if (priv->format != BRASERO_IMAGE_FORMAT_CUE)
		return FALSE;

	cueuri = brasero_string_get_uri (priv->toc);
	res = brasero_image_format_cue_bin_byte_swap (cueuri, NULL, NULL);
	g_free (cueuri);

	return res;
}

static BraseroBurnResult
brasero_track_image_get_track_type (BraseroTrack *track,
				    BraseroTrackType *type)
{
	BraseroTrackImagePrivate *priv;

	priv = BRASERO_TRACK_IMAGE_PRIVATE (track);

	brasero_track_type_set_has_image (type);
	brasero_track_type_set_image_format (type, priv->format);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_track_image_get_size (BraseroTrack *track,
			      goffset *blocks,
			      goffset *block_size)
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

	klass->set_source = brasero_track_image_set_source_real;
	klass->set_block_num = brasero_track_image_set_block_num_real;
}

/**
 * brasero_track_image_new:
 *
 * Creates a new #BraseroTrackImage object.
 *
 * This type of tracks is used to burn disc images.
 *
 * Return value: a #BraseroTrackImage object.
 **/

BraseroTrackImage *
brasero_track_image_new (void)
{
	return g_object_new (BRASERO_TYPE_TRACK_IMAGE, NULL);
}
