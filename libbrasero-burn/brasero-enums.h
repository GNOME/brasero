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

#ifndef _BRASERO_ENUM_H_
#define _BRASERO_ENUM_H_

#include <glib.h>

G_BEGIN_DECLS

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

/* These flags are sorted by importance. That's done to solve the problem of
 * exclusive flags: that way MERGE will always win over any other flag if they
 * are exclusive. On the other hand DAO will always lose. */
typedef enum {
	BRASERO_BURN_FLAG_NONE			= 0,

	/* These flags should always be supported */
	BRASERO_BURN_FLAG_CHECK_SIZE		= 1,
	BRASERO_BURN_FLAG_NOGRACE		= 1 << 1,
	BRASERO_BURN_FLAG_EJECT			= 1 << 2,

	/* These are of great importance for the result */
	BRASERO_BURN_FLAG_MERGE			= 1 << 3,
	BRASERO_BURN_FLAG_MULTI			= 1 << 4,
	BRASERO_BURN_FLAG_APPEND		= 1 << 5,

	BRASERO_BURN_FLAG_BURNPROOF		= 1 << 6,
	BRASERO_BURN_FLAG_NO_TMP_FILES		= 1 << 7,
	BRASERO_BURN_FLAG_DUMMY			= 1 << 8,

	BRASERO_BURN_FLAG_OVERBURN		= 1 << 9,

	BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE	= 1 << 10,
	BRASERO_BURN_FLAG_FAST_BLANK		= 1 << 11,

	/* NOTE: these two are contradictory? */
	BRASERO_BURN_FLAG_DAO			= 1 << 13,
	BRASERO_BURN_FLAG_RAW			= 1 << 14,

	BRASERO_BURN_FLAG_LAST
} BraseroBurnFlag;

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
	BRASERO_BURN_ACTION_EJECTING,
	BRASERO_BURN_ACTION_LAST
} BraseroBurnAction;

typedef enum {
	BRASERO_IMAGE_FS_NONE			= 0,
	BRASERO_IMAGE_FS_ISO			= 1,
	BRASERO_IMAGE_FS_UDF			= 1 << 1,
	BRASERO_IMAGE_FS_JOLIET			= 1 << 2,
	BRASERO_IMAGE_FS_VIDEO			= 1 << 3,

	/* The following one conflict with UDF and JOLIET */
	BRASERO_IMAGE_FS_SYMLINK		= 1 << 4,

	BRASERO_IMAGE_ISO_FS_LEVEL_3		= 1 << 5,
	BRASERO_IMAGE_ISO_FS_DEEP_DIRECTORY	= 1 << 6,
	BRASERO_IMAGE_FS_ANY			= BRASERO_IMAGE_FS_ISO|
						  BRASERO_IMAGE_FS_UDF|
						  BRASERO_IMAGE_FS_JOLIET|
						  BRASERO_IMAGE_FS_SYMLINK|
						  BRASERO_IMAGE_ISO_FS_LEVEL_3|
						  BRASERO_IMAGE_FS_VIDEO|
						  BRASERO_IMAGE_ISO_FS_DEEP_DIRECTORY
} BraseroImageFS;

typedef enum {
	BRASERO_AUDIO_FORMAT_NONE		= 0,
	BRASERO_AUDIO_FORMAT_UNDEFINED		= 1,
	BRASERO_AUDIO_FORMAT_DTS		= 1 << 1,
	BRASERO_AUDIO_FORMAT_RAW		= 1 << 2,
	BRASERO_AUDIO_FORMAT_AC3		= 1 << 3,
	BRASERO_AUDIO_FORMAT_MP2		= 1 << 4,

	BRASERO_AUDIO_FORMAT_44100		= 1 << 5,
	BRASERO_AUDIO_FORMAT_48000		= 1 << 6,


	BRASERO_VIDEO_FORMAT_UNDEFINED		= 1 << 7,
	BRASERO_VIDEO_FORMAT_VCD		= 1 << 8,
	BRASERO_VIDEO_FORMAT_VIDEO_DVD		= 1 << 9,

	BRASERO_METADATA_INFO			= 1 << 10,

	BRASERO_AUDIO_FORMAT_RAW_LITTLE_ENDIAN  = 1 << 11,
} BraseroStreamFormat;

#define BRASERO_STREAM_FORMAT_AUDIO(stream_FORMAT)	((stream_FORMAT) & 0x087F)
#define BRASERO_STREAM_FORMAT_VIDEO(stream_FORMAT)	((stream_FORMAT) & 0x0380)

#define	BRASERO_MIN_STREAM_LENGTH			((gint64) 6 * 1000000000LL)
#define BRASERO_STREAM_LENGTH(start_MACRO, end_MACRO)					\
	((end_MACRO) - (start_MACRO) > BRASERO_MIN_STREAM_LENGTH) ?			\
	((end_MACRO) - (start_MACRO)) : BRASERO_MIN_STREAM_LENGTH

#define BRASERO_STREAM_FORMAT_HAS_VIDEO(format_MACRO)				\
	 ((format_MACRO) & (BRASERO_VIDEO_FORMAT_UNDEFINED|			\
	 		    BRASERO_VIDEO_FORMAT_VCD|				\
	 		    BRASERO_VIDEO_FORMAT_VIDEO_DVD))

typedef enum {
	BRASERO_IMAGE_FORMAT_NONE = 0,
	BRASERO_IMAGE_FORMAT_BIN = 1,
	BRASERO_IMAGE_FORMAT_CUE = 1 << 1,
	BRASERO_IMAGE_FORMAT_CLONE = 1 << 2,
	BRASERO_IMAGE_FORMAT_CDRDAO = 1 << 3,
	BRASERO_IMAGE_FORMAT_ANY = BRASERO_IMAGE_FORMAT_BIN|
	BRASERO_IMAGE_FORMAT_CUE|
	BRASERO_IMAGE_FORMAT_CDRDAO|
	BRASERO_IMAGE_FORMAT_CLONE,
} BraseroImageFormat; 

typedef enum {
	BRASERO_PLUGIN_ERROR_NONE					= 0,
	BRASERO_PLUGIN_ERROR_MODULE,
	BRASERO_PLUGIN_ERROR_MISSING_APP,
	BRASERO_PLUGIN_ERROR_WRONG_APP_VERSION,
	BRASERO_PLUGIN_ERROR_SYMBOLIC_LINK_APP,
	BRASERO_PLUGIN_ERROR_MISSING_LIBRARY,
	BRASERO_PLUGIN_ERROR_LIBRARY_VERSION,
	BRASERO_PLUGIN_ERROR_MISSING_GSTREAMER_PLUGIN,
} BraseroPluginErrorType;

G_END_DECLS

#endif

