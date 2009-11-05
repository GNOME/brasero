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

#include "brasero-medium.h"
#include "brasero-drive.h"
#include "brasero-track-type.h"
#include "brasero-track-type-private.h"

/**
 * brasero_track_type_new:
 *
 * Creates a new #BraseroTrackType structure.
 * Free it with brasero_track_type_free ().
 *
 * Return value: a #BraseroTrackType pointer.
 **/

BraseroTrackType *
brasero_track_type_new (void)
{
	return g_new0 (BraseroTrackType, 1);
}

/**
 * brasero_track_type_free:
 * @type: a #BraseroTrackType.
 *
 * Frees #BraseroTrackType structure.
 *
 **/

void
brasero_track_type_free (BraseroTrackType *type)
{
	if (!type)
		return;

	g_free (type);
}

/**
 * brasero_track_type_get_image_format:
 * @type: a #BraseroTrackType.
 *
 * Returns the format of an image when
 * brasero_track_type_get_has_image () returned
 * TRUE.
 *
 * Return value: a #BraseroImageFormat
 **/

BraseroImageFormat
brasero_track_type_get_image_format (const BraseroTrackType *type) 
{
	g_return_val_if_fail (type != NULL, BRASERO_IMAGE_FORMAT_NONE);

	if (type->type != BRASERO_TRACK_TYPE_IMAGE)
		return BRASERO_IMAGE_FORMAT_NONE;

	return type->subtype.img_format;
}

/**
 * brasero_track_type_get_data_fs:
 * @type: a #BraseroTrackType.
 *
 * Returns the parameters for the image generation
 * when brasero_track_type_get_has_data () returned
 * TRUE.
 *
 * Return value: a #BraseroImageFS
 **/

BraseroImageFS
brasero_track_type_get_data_fs (const BraseroTrackType *type) 
{
	g_return_val_if_fail (type != NULL, BRASERO_IMAGE_FS_NONE);

	if (type->type != BRASERO_TRACK_TYPE_DATA)
		return BRASERO_IMAGE_FS_NONE;

	return type->subtype.fs_type;
}

/**
 * brasero_track_type_get_stream_format:
 * @type: a #BraseroTrackType.
 *
 * Returns the format for a stream (song or video)
 * when brasero_track_type_get_has_stream () returned
 * TRUE.
 *
 * Return value: a #BraseroStreamFormat
 **/

BraseroStreamFormat
brasero_track_type_get_stream_format (const BraseroTrackType *type) 
{
	g_return_val_if_fail (type != NULL, BRASERO_AUDIO_FORMAT_NONE);

	if (type->type != BRASERO_TRACK_TYPE_STREAM)
		return BRASERO_AUDIO_FORMAT_NONE;

	return type->subtype.stream_format;
}

/**
 * brasero_track_type_get_medium_type:
 * @type: a #BraseroTrackType.
 *
 * Returns the medium type
 * when brasero_track_type_get_has_medium () returned
 * TRUE.
 *
 * Return value: a #BraseroMedia
 **/

BraseroMedia
brasero_track_type_get_medium_type (const BraseroTrackType *type) 
{
	g_return_val_if_fail (type != NULL, BRASERO_MEDIUM_NONE);

	if (type->type != BRASERO_TRACK_TYPE_DISC)
		return BRASERO_MEDIUM_NONE;

	return type->subtype.media;
}

/**
 * brasero_track_type_set_image_format:
 * @type: a #BraseroTrackType.
 * @format: a #BraseroImageFormat
 *
 * Sets the #BraseroImageFormat. Must be called
 * after brasero_track_type_set_has_image ().
 *
 **/

void
brasero_track_type_set_image_format (BraseroTrackType *type,
				     BraseroImageFormat format) 
{
	g_return_if_fail (type != NULL);

	if (type->type != BRASERO_TRACK_TYPE_IMAGE)
		return;

	type->subtype.img_format = format;
}

/**
 * brasero_track_type_set_data_fs:
 * @type: a #BraseroTrackType.
 * @fs_type: a #BraseroImageFS
 *
 * Sets the #BraseroImageFS. Must be called
 * after brasero_track_type_set_has_data ().
 *
 **/

void
brasero_track_type_set_data_fs (BraseroTrackType *type,
				BraseroImageFS fs_type) 
{
	g_return_if_fail (type != NULL);

	if (type->type != BRASERO_TRACK_TYPE_DATA)
		return;

	type->subtype.fs_type = fs_type;
}

/**
 * brasero_track_type_set_stream_format:
 * @type: a #BraseroTrackType.
 * @format: a #BraseroImageFormat
 *
 * Sets the #BraseroStreamFormat. Must be called
 * after brasero_track_type_set_has_stream ().
 *
 **/

void
brasero_track_type_set_stream_format (BraseroTrackType *type,
				      BraseroStreamFormat format) 
{
	g_return_if_fail (type != NULL);

	if (type->type != BRASERO_TRACK_TYPE_STREAM)
		return;

	type->subtype.stream_format = format;
}

/**
 * brasero_track_type_set_medium_type:
 * @type: a #BraseroTrackType.
 * @media: a #BraseroMedia
 *
 * Sets the #BraseroMedia. Must be called
 * after brasero_track_type_set_has_medium ().
 *
 **/

void
brasero_track_type_set_medium_type (BraseroTrackType *type,
				    BraseroMedia media) 
{
	g_return_if_fail (type != NULL);

	if (type->type != BRASERO_TRACK_TYPE_DISC)
		return;

	type->subtype.media = media;
}

/**
 * brasero_track_type_is_empty:
 * @type: a #BraseroTrackType.
 *
 * Returns TRUE if no type was set.
 *
 * Return value: a #gboolean
 **/

gboolean
brasero_track_type_is_empty (const BraseroTrackType *type)
{
	g_return_val_if_fail (type != NULL, FALSE);

	return (type->type == BRASERO_TRACK_TYPE_NONE);
}

/**
 * brasero_track_type_get_has_data:
 * @type: a #BraseroTrackType.
 *
 * Returns TRUE if DATA type (see brasero_track_data_new ()) was set.
 *
 * Return value: a #gboolean
 **/

gboolean
brasero_track_type_get_has_data (const BraseroTrackType *type)
{
	g_return_val_if_fail (type != NULL, FALSE);

	return type->type == BRASERO_TRACK_TYPE_DATA;
}

/**
 * brasero_track_type_get_has_image:
 * @type: a #BraseroTrackType.
 *
 * Returns TRUE if IMAGE type (see brasero_track_image_new ()) was set.
 *
 * Return value: a #gboolean
 **/

gboolean
brasero_track_type_get_has_image (const BraseroTrackType *type)
{
	g_return_val_if_fail (type != NULL, FALSE);

	return type->type == BRASERO_TRACK_TYPE_IMAGE;
}

/**
 * brasero_track_type_get_has_stream:
 * @type: a #BraseroTrackType.
 *
 * This function returns %TRUE if IMAGE type (see brasero_track_stream_new ()) was set.
 *
 * Return value: a #gboolean
 **/

gboolean
brasero_track_type_get_has_stream (const BraseroTrackType *type)
{
	g_return_val_if_fail (type != NULL, FALSE);

	return type->type == BRASERO_TRACK_TYPE_STREAM;
}

/**
 * brasero_track_type_get_has_medium:
 * @type: a #BraseroTrackType.
 *
 * Returns TRUE if MEDIUM type (see brasero_track_disc_new ()) was set.
 *
 * Return value: a #gboolean
 **/

gboolean
brasero_track_type_get_has_medium (const BraseroTrackType *type)
{
	g_return_val_if_fail (type != NULL, FALSE);

	return type->type == BRASERO_TRACK_TYPE_DISC;
}

/**
 * brasero_track_type_set_has_data:
 * @type: a #BraseroTrackType.
 *
 * Set DATA type for @type.
 *
 **/

void
brasero_track_type_set_has_data (BraseroTrackType *type)
{
	g_return_if_fail (type != NULL);

	type->type = BRASERO_TRACK_TYPE_DATA;
}

/**
 * brasero_track_type_set_has_image:
 * @type: a #BraseroTrackType.
 *
 * Set IMAGE type for @type.
 *
 **/

void
brasero_track_type_set_has_image (BraseroTrackType *type)
{
	g_return_if_fail (type != NULL);

	type->type = BRASERO_TRACK_TYPE_IMAGE;
}

/**
 * brasero_track_type_set_has_stream:
 * @type: a #BraseroTrackType.
 *
 * Set STREAM type for @type
 *
 **/

void
brasero_track_type_set_has_stream (BraseroTrackType *type)
{
	g_return_if_fail (type != NULL);

	type->type = BRASERO_TRACK_TYPE_STREAM;
}

/**
 * brasero_track_type_set_has_medium:
 * @type: a #BraseroTrackType.
 *
 * Set MEDIUM type for @type.
 *
 **/

void
brasero_track_type_set_has_medium (BraseroTrackType *type)
{
	g_return_if_fail (type != NULL);

	type->type = BRASERO_TRACK_TYPE_DISC;
}

/**
 * brasero_track_type_equal:
 * @type_A: a #BraseroTrackType.
 * @type_B: a #BraseroTrackType.
 *
 * Returns TRUE if @type_A and @type_B represents
 * the same type and subtype.
 *
 * Return value: a #gboolean
 **/

gboolean
brasero_track_type_equal (const BraseroTrackType *type_A,
			  const BraseroTrackType *type_B)
{
	g_return_val_if_fail (type_A != NULL, FALSE);
	g_return_val_if_fail (type_B != NULL, FALSE);

	if (type_A->type != type_B->type)
		return FALSE;

	switch (type_A->type) {
	case BRASERO_TRACK_TYPE_DATA:
		if (type_A->subtype.fs_type != type_B->subtype.fs_type)
			return FALSE;
		break;
	
	case BRASERO_TRACK_TYPE_DISC:
		if (type_B->subtype.media != type_A->subtype.media)
			return FALSE;
		break;
	
	case BRASERO_TRACK_TYPE_IMAGE:
		if (type_A->subtype.img_format != type_B->subtype.img_format)
			return FALSE;
		break;

	case BRASERO_TRACK_TYPE_STREAM:
		if (type_A->subtype.stream_format != type_B->subtype.stream_format)
			return FALSE;
		break;

	default:
		break;
	}

	return TRUE;
}

#if 0
/**
 * brasero_track_type_match:
 * @type_A: a #BraseroTrackType.
 * @type_B: a #BraseroTrackType.
 *
 * Returns TRUE if @type_A and @type_B match.
 *
 * (Used internally)
 *
 * Return value: a #gboolean
 **/

gboolean
brasero_track_type_match (const BraseroTrackType *type_A,
			  const BraseroTrackType *type_B)
{
	g_return_val_if_fail (type_A != NULL, FALSE);
	g_return_val_if_fail (type_B != NULL, FALSE);

	if (type_A->type != type_B->type)
		return FALSE;

	switch (type_A->type) {
	case BRASERO_TRACK_TYPE_DATA:
		if (!(type_A->subtype.fs_type & type_B->subtype.fs_type))
			return FALSE;
		break;
	
	case BRASERO_TRACK_TYPE_DISC:
		if (!(type_A->subtype.media & type_B->subtype.media))
			return FALSE;
		break;
	
	case BRASERO_TRACK_TYPE_IMAGE:
		if (!(type_A->subtype.img_format & type_B->subtype.img_format))
			return FALSE;
		break;

	case BRASERO_TRACK_TYPE_STREAM:
		if (!(type_A->subtype.stream_format & type_B->subtype.stream_format))
			return FALSE;
		break;

	default:
		break;
	}

	return TRUE;
}

#endif
