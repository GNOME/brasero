/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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

	BRASERO_BURN_ERROR_TMP_DIR,
	BRASERO_BURN_ERROR_FILE_EXIST,
	BRASERO_BURN_ERROR_INVALID_FILE,
	BRASERO_BURN_ERROR_INCOMPATIBLE_FORMAT,
	BRASERO_BURN_ERROR_JOLIET_TREE,

	BRASERO_BURN_ERROR_SCSI_IOCTL,
	BRASERO_BURN_ERROR_SLOW_DMA,
	BRASERO_BURN_ERROR_PERMISSION,
	BRASERO_BURN_ERROR_BUSY_DRIVE,

	BRASERO_BURN_ERROR_DISK_SPACE,

	BRASERO_BURN_ERROR_MEDIA_SPACE,
	BRASERO_BURN_ERROR_RELOAD_MEDIA,
	BRASERO_BURN_ERROR_MEDIA_BLANK,
	BRASERO_BURN_ERROR_MEDIA_BUSY,
	BRASERO_BURN_ERROR_MEDIA_NONE,
	BRASERO_BURN_ERROR_MEDIA_UNSUPPORTED,
	BRASERO_BURN_ERROR_MEDIA_NOT_REWRITABLE,
	BRASERO_BURN_ERROR_MEDIA_NOT_WRITABLE,
	BRASERO_BURN_ERROR_DVD_NOT_SUPPORTED,
	BRASERO_BURN_ERROR_CD_NOT_SUPPORTED,

	BRASERO_BURN_ERROR_BAD_CHECKSUM,

	/* these are not necessarily error */
	BRASERO_BURN_WARNING_NEXT_COPY,
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
 * exclusive flags: that way MULTI will always win over any other flag if they
 * are exclusive. On the other hand DAO will always lose. */
typedef enum {
	BRASERO_BURN_FLAG_NONE			= 0,

	/* These flags should always be supported */
	BRASERO_BURN_FLAG_EJECT			= 1,
	BRASERO_BURN_FLAG_NOGRACE		= 1 << 1,
	BRASERO_BURN_FLAG_DONT_CLEAN_OUTPUT	= 1 << 2,
	BRASERO_BURN_FLAG_DONT_OVERWRITE	= 1 << 3,
	BRASERO_BURN_FLAG_CHECK_SIZE		= 1 << 4,

	/* These are of great importance for the result */
	BRASERO_BURN_FLAG_MERGE			= 1 << 5,
	BRASERO_BURN_FLAG_MULTI			= 1 << 6,
	BRASERO_BURN_FLAG_APPEND		= 1 << 7,

	BRASERO_BURN_FLAG_BURNPROOF		= 1 << 8,
	BRASERO_BURN_FLAG_NO_TMP_FILES		= 1 << 9,
	BRASERO_BURN_FLAG_DUMMY			= 1 << 10,

	/* FIXME! this flag is more or less linked to OVERBURN one can't we do 
	 * a single one */
	BRASERO_BURN_FLAG_OVERBURN		= 1 << 11,

	BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE	= 1 << 12,
	BRASERO_BURN_FLAG_FAST_BLANK		= 1 << 13,

	BRASERO_BURN_FLAG_DAO			= 1 << 14,

	BRASERO_BURN_FLAG_LAST
} BraseroBurnFlag;

#define BRASERO_BURN_FLAG_ALL			0x7FFF

#define BRASERO_PLUGIN_KEY		"/apps/brasero/config/plugins"

#define BRASERO_DURATION_TO_BYTES(duration)					\
	((gint64) (duration) * 75 * 2352 / 1000000000 +				\
	(((gint64) ((duration) * 75 * 2352) % 1000000000) ? 1:0))
#define BRASERO_DURATION_TO_SECTORS(duration)					\
	((gint64) (duration) * 75 / 1000000000 +				\
	(((gint64) ((duration) * 75) % 1000000000) ? 1:0))
#define BRASERO_SIZE_TO_SECTORS(size, secsize)					\
	(((size) / (secsize)) + (((size) % (secsize)) ? 1:0))
#define BRASERO_BYTES_TO_DURATION(bytes)					\
	(((bytes) * 1000000000) / (2352 * 75) + 				\
	((((bytes) * 1000000000) % (2352 * 75)) ? 1:0))

BraseroBurnResult
brasero_burn_library_init (void);

GSList *
brasero_burn_library_get_plugins_list (void);

void
brasero_burn_library_shutdown (void);

G_END_DECLS

#endif /* _BURN_BASICS_H */
