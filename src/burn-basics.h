/*
 * Brasero is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * Brasero is distributed in the hope that it will be useful,
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
 
/***************************************************************************
 *            burn-basics.h
 *
 *  Sat Feb 11 16:49:00 2006
 *  Copyright  2006  philippe Rouquier
 *  <brasero-app@wanadoo.fr>
 ****************************************************************************/

#ifndef _BURN_BASICS_H
#define _BURN_BASICS_H

#include <glib.h>

G_BEGIN_DECLS

#include "burn-units.h"

#define BRASERO_GET_BASENAME_FOR_DISPLAY(uri, name)				\
{										\
    	gchar *escaped_basename;						\
	escaped_basename = g_path_get_basename (uri);				\
    	name = g_uri_unescape_string (escaped_basename, NULL);			\
	g_free (escaped_basename);						\
}

GQuark brasero_burn_quark (void);
#define BRASERO_BURN_ERROR brasero_burn_quark()

#define BRASERO_PLUGIN_DIRECTORY		BRASERO_LIBDIR "/brasero/plugins"

#define BRASERO_BURN_TMP_FILE_NAME		"brasero_tmp_XXXXXX"

#define BRASERO_MD5_FILE			".checksum.md5"
#define BRASERO_SHA1_FILE			".checksum.sha1"
#define BRASERO_SHA256_FILE			".checksum.sha256"

typedef enum {
	BRASERO_BURN_ERROR_NONE,
	BRASERO_BURN_ERROR_GENERAL,

	BRASERO_BURN_ERROR_PLUGIN_MISBEHAVIOR,

	BRASERO_BURN_ERROR_SLOW_DMA,
	BRASERO_BURN_ERROR_PERMISSION,
	BRASERO_BURN_ERROR_DRIVE_BUSY,
	BRASERO_BURN_ERROR_DISK_SPACE,

	BRASERO_BURN_ERROR_INPUT_INVALID,

	BRASERO_BURN_ERROR_OUTPUT_NONE,

	BRASERO_BURN_ERROR_FILE_INVALID,
	BRASERO_BURN_ERROR_FILE_NOT_FOUND,
	BRASERO_BURN_ERROR_FILE_NOT_LOCAL,

	BRASERO_BURN_ERROR_WRITE_MEDIUM,
	BRASERO_BURN_ERROR_WRITE_IMAGE,

	BRASERO_BURN_ERROR_IMAGE_INVALID,
	BRASERO_BURN_ERROR_IMAGE_JOLIET,
	BRASERO_BURN_ERROR_IMAGE_LAST_SESSION,

	BRASERO_BURN_ERROR_MEDIUM_NONE,
	BRASERO_BURN_ERROR_MEDIUM_INVALID,
	BRASERO_BURN_ERROR_MEDIUM_SPACE,
	BRASERO_BURN_ERROR_MEDIUM_NO_DATA,
	BRASERO_BURN_ERROR_MEDIUM_NOT_WRITABLE,
	BRASERO_BURN_ERROR_MEDIUM_NOT_REWRITABLE,
	BRASERO_BURN_ERROR_MEDIUM_NEED_RELOADING,

	BRASERO_BURN_ERROR_BAD_CHECKSUM,

	/* these are not necessarily error */
	BRASERO_BURN_WARNING_CHECKSUM,
	BRASERO_BURN_WARNING_INSERT_AFTER_COPY
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
	BRASERO_BURN_ACTION_CREATING_IMAGE,
	BRASERO_BURN_ACTION_RECORDING,
	BRASERO_BURN_ACTION_BLANKING,
	BRASERO_BURN_ACTION_CHECKSUM,
	BRASERO_BURN_ACTION_DRIVE_COPY,
	BRASERO_BURN_ACTION_FILE_COPY,
	BRASERO_BURN_ACTION_ANALYSING,
	BRASERO_BURN_ACTION_TRANSCODING,
	BRASERO_BURN_ACTION_PREPARING,
	BRASERO_BURN_ACTION_LEADIN,
	BRASERO_BURN_ACTION_RECORDING_CD_TEXT,
	BRASERO_BURN_ACTION_FIXATING,
	BRASERO_BURN_ACTION_LEADOUT,
	BRASERO_BURN_ACTION_START_RECORDING,
	BRASERO_BURN_ACTION_FINISHED,
	BRASERO_BURN_ACTION_LAST
} BraseroBurnAction;

const gchar *
brasero_burn_action_to_string (BraseroBurnAction action);

/* These flags are sorted by importance. That's done to solve the problem of
 * exclusive flags: that way MERGE will always win over any other flag if they
 * are exclusive. On the other hand DAO will always lose. */
typedef enum {
	BRASERO_BURN_FLAG_NONE			= 0,

	/* These flags should always be supported */
	BRASERO_BURN_FLAG_EJECT			= 1,
	BRASERO_BURN_FLAG_NOGRACE		= 1 << 1,
	BRASERO_BURN_FLAG_DONT_OVERWRITE	= 1 << 2,
	BRASERO_BURN_FLAG_CHECK_SIZE		= 1 << 3,

	/* These are of great importance for the result */
	BRASERO_BURN_FLAG_MERGE			= 1 << 4,
	BRASERO_BURN_FLAG_MULTI			= 1 << 5,
	BRASERO_BURN_FLAG_APPEND		= 1 << 6,

	BRASERO_BURN_FLAG_BURNPROOF		= 1 << 7,
	BRASERO_BURN_FLAG_NO_TMP_FILES		= 1 << 8,
	BRASERO_BURN_FLAG_DUMMY			= 1 << 9,

	/* FIXME! this flag is more or less linked to OVERBURN one can't we do 
	 * a single one */
	BRASERO_BURN_FLAG_OVERBURN		= 1 << 10,

	BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE	= 1 << 11,
	BRASERO_BURN_FLAG_FAST_BLANK		= 1 << 12,

	/* NOTE: these two are contradictory? */
	BRASERO_BURN_FLAG_DAO			= 1 << 13,
	BRASERO_BURN_FLAG_RAW			= 1 << 14,

	BRASERO_BURN_FLAG_LAST
} BraseroBurnFlag;

#define BRASERO_BURN_FLAG_ALL			0xFFFF

#define BRASERO_PLUGIN_KEY		"/apps/brasero/config/plugins"

BraseroBurnResult
brasero_burn_library_init (void);

GSList *
brasero_burn_library_get_plugins_list (void);

gboolean
brasero_burn_library_can_checksum (void);

void
brasero_burn_library_shutdown (void);

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
#define BRASERO_DVD_AUDIO_STREAMS		"session::DVD::audio::format"

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

#endif /* _BURN_BASICS_H */
