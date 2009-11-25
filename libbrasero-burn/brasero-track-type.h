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

#ifndef _BURN_TRACK_TYPE_H
#define _BURN_TRACK_TYPE_H

#include <glib.h>

#include <brasero-enums.h>
#include <brasero-media.h>

G_BEGIN_DECLS

typedef struct _BraseroTrackType BraseroTrackType;

BraseroTrackType *
brasero_track_type_new (void);

void
brasero_track_type_free (BraseroTrackType *type);

gboolean
brasero_track_type_is_empty (const BraseroTrackType *type);
gboolean
brasero_track_type_get_has_data (const BraseroTrackType *type);
gboolean
brasero_track_type_get_has_image (const BraseroTrackType *type);
gboolean
brasero_track_type_get_has_stream (const BraseroTrackType *type);
gboolean
brasero_track_type_get_has_medium (const BraseroTrackType *type);

void
brasero_track_type_set_has_data (BraseroTrackType *type);
void
brasero_track_type_set_has_image (BraseroTrackType *type);
void
brasero_track_type_set_has_stream (BraseroTrackType *type);
void
brasero_track_type_set_has_medium (BraseroTrackType *type);

BraseroStreamFormat
brasero_track_type_get_stream_format (const BraseroTrackType *type);
BraseroImageFormat
brasero_track_type_get_image_format (const BraseroTrackType *type);
BraseroMedia
brasero_track_type_get_medium_type (const BraseroTrackType *type);
BraseroImageFS
brasero_track_type_get_data_fs (const BraseroTrackType *type);

void
brasero_track_type_set_stream_format (BraseroTrackType *type,
				      BraseroStreamFormat format);
void
brasero_track_type_set_image_format (BraseroTrackType *type,
				     BraseroImageFormat format);
void
brasero_track_type_set_medium_type (BraseroTrackType *type,
				    BraseroMedia media);
void
brasero_track_type_set_data_fs (BraseroTrackType *type,
				BraseroImageFS fs_type);

gboolean
brasero_track_type_equal (const BraseroTrackType *type_A,
			  const BraseroTrackType *type_B);

G_END_DECLS

#endif
