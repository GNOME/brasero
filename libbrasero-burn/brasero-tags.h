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

#ifndef _BRASERO_TAGS_H_
#define _BRASERO_TAGS_H_

#include <glib.h>

G_BEGIN_DECLS

/**
 * Some defined and usable tags for a track
 */

#define BRASERO_TRACK_MEDIUM_ADDRESS_START_TAG		"track::medium::address::start"
#define BRASERO_TRACK_MEDIUM_ADDRESS_END_TAG		"track::medium::address::end"

/**
 * Array of filenames (on medium) which have a wrong checksum value (G_TYPE_STRV)
 */

#define BRASERO_TRACK_MEDIUM_WRONG_CHECKSUM_TAG		"track::medium::error::checksum::list"

/**
 * Strings
 */

#define BRASERO_TRACK_STREAM_TITLE_TAG			"track::stream::info::title"
#define BRASERO_TRACK_STREAM_COMPOSER_TAG		"track::stream::info::composer"
#define BRASERO_TRACK_STREAM_ARTIST_TAG			"track::stream::info::artist"
#define BRASERO_TRACK_STREAM_ALBUM_TAG			"track::stream::info::album"
#define BRASERO_TRACK_STREAM_ISRC_TAG			"track::stream::info::isrc"
#define BRASERO_TRACK_STREAM_THUMBNAIL_TAG		"track::stream::snapshot"
#define BRASERO_TRACK_STREAM_MIME_TAG			"track::stream::mime"

/**
 * Int
 */


/**
 * This tag (for sessions) is used to set an estimated size, used to determine
 * in the burn option dialog if the selected medium is big enough.
 */

#define BRASERO_DATA_TRACK_SIZE_TAG			"track::data::estimated_size"
#define BRASERO_STREAM_TRACK_SIZE_TAG			"track::stream::estimated_size"

/**
 * Some defined and usable tags for a session
 */

/**
 * Gives the uri (gchar *) of the cover
 */
#define BRASERO_COVER_URI			"session::art::cover"

/**
 * Define the audio streams for a DVD
 */
#define BRASERO_DVD_STREAM_FORMAT		"session::DVD::stream::format"			/* Int */
#define BRASERO_SESSION_STREAM_AUDIO_FORMAT	"session::stream::audio::format"	/* Int */

/**
 * Define the format: whether VCD or SVCD
 */
enum {
	BRASERO_VCD_NONE,
	BRASERO_VCD_V1,
	BRASERO_VCD_V2,
	BRASERO_SVCD
};
#define BRASERO_VCD_TYPE			"session::VCD::format"

/**
 * This is the video format that should be used.
 */
enum {
	BRASERO_VIDEO_FRAMERATE_NATIVE,
	BRASERO_VIDEO_FRAMERATE_NTSC,
	BRASERO_VIDEO_FRAMERATE_PAL_SECAM
};
#define BRASERO_VIDEO_OUTPUT_FRAMERATE		"session::video::framerate"

/**
 * Aspect ratio
 */
enum {
	BRASERO_VIDEO_ASPECT_NATIVE,
	BRASERO_VIDEO_ASPECT_4_3,
	BRASERO_VIDEO_ASPECT_16_9
};
#define BRASERO_VIDEO_OUTPUT_ASPECT		"session::video::aspect"

G_END_DECLS

#endif
