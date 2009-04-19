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

BraseroTrackType *
brasero_track_type_new (void)
{
	return g_new0 (BraseroTrackType, 1);
}

void
brasero_track_type_free (BraseroTrackType *type)
{
	g_free (type);
}

BraseroImageFormat
brasero_track_type_get_image_format (const BraseroTrackType *type) 
{
	g_return_val_if_fail (type != NULL, BRASERO_IMAGE_FORMAT_NONE);

	if (type->type != BRASERO_TRACK_TYPE_IMAGE)
		return BRASERO_IMAGE_FORMAT_NONE;

	return type->subtype.img_format;
}

BraseroImageFS
brasero_track_type_get_data_fs (const BraseroTrackType *type) 
{
	g_return_val_if_fail (type != NULL, BRASERO_IMAGE_FS_NONE);

	if (type->type != BRASERO_TRACK_TYPE_DATA)
		return BRASERO_IMAGE_FS_NONE;

	return type->subtype.fs_type;
}

BraseroStreamFormat
brasero_track_type_get_stream_format (const BraseroTrackType *type) 
{
	g_return_val_if_fail (type != NULL, BRASERO_AUDIO_FORMAT_NONE);

	if (type->type != BRASERO_TRACK_TYPE_STREAM)
		return BRASERO_AUDIO_FORMAT_NONE;

	return type->subtype.stream_format;
}

BraseroMedia
brasero_track_type_get_medium_type (const BraseroTrackType *type) 
{
	g_return_val_if_fail (type != NULL, BRASERO_MEDIUM_NONE);

	if (type->type != BRASERO_TRACK_TYPE_DISC)
		return BRASERO_MEDIUM_NONE;

	return type->subtype.media;
}

void
brasero_track_type_set_image_format (BraseroTrackType *type,
				     BraseroImageFormat format) 
{
	g_return_if_fail (type != NULL);

	if (type->type != BRASERO_TRACK_TYPE_IMAGE)
		return;

	type->subtype.img_format = format;
}

void
brasero_track_type_set_data_fs (BraseroTrackType *type,
				BraseroImageFS fs_type) 
{
	g_return_if_fail (type != NULL);

	if (type->type != BRASERO_TRACK_TYPE_DATA)
		return;

	type->subtype.fs_type = fs_type;
}

void
brasero_track_type_set_stream_format (BraseroTrackType *type,
				      BraseroImageFormat format) 
{
	g_return_if_fail (type != NULL);

	if (type->type != BRASERO_TRACK_TYPE_STREAM)
		return;

	type->subtype.stream_format = format;
}

void
brasero_track_type_set_medium_type (BraseroTrackType *type,
				    BraseroMedia media) 
{
	g_return_if_fail (type != NULL);

	if (type->type != BRASERO_TRACK_TYPE_DISC)
		return;

	type->subtype.media = media;
}

gboolean
brasero_track_type_is_empty (const BraseroTrackType *type)
{
	g_return_val_if_fail (type != NULL, FALSE);

	return (type->type == BRASERO_TRACK_TYPE_NONE);
}

gboolean
brasero_track_type_get_has_data (const BraseroTrackType *type)
{
	g_return_val_if_fail (type != NULL, FALSE);

	return type->type == BRASERO_TRACK_TYPE_DATA;
}

gboolean
brasero_track_type_get_has_image (const BraseroTrackType *type)
{
	g_return_val_if_fail (type != NULL, FALSE);

	return type->type == BRASERO_TRACK_TYPE_IMAGE;
}

gboolean
brasero_track_type_get_has_stream (const BraseroTrackType *type)
{
	g_return_val_if_fail (type != NULL, FALSE);

	return type->type == BRASERO_TRACK_TYPE_STREAM;
}

gboolean
brasero_track_type_get_has_medium (const BraseroTrackType *type)
{
	g_return_val_if_fail (type != NULL, FALSE);

	return type->type == BRASERO_TRACK_TYPE_DISC;
}

void
brasero_track_type_set_has_data (BraseroTrackType *type)
{
	g_return_if_fail (type != NULL);

	type->type = BRASERO_TRACK_TYPE_DATA;
}

void
brasero_track_type_set_has_image (BraseroTrackType *type)
{
	g_return_if_fail (type != NULL);

	type->type = BRASERO_TRACK_TYPE_IMAGE;
}

void
brasero_track_type_set_has_stream (BraseroTrackType *type)
{
	g_return_if_fail (type != NULL);

	type->type = BRASERO_TRACK_TYPE_STREAM;
}

void
brasero_track_type_set_has_medium (BraseroTrackType *type)
{
	g_return_if_fail (type != NULL);

	type->type = BRASERO_TRACK_TYPE_DISC;
}

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

