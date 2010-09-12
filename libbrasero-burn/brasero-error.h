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

#ifndef _BRASERO_ERROR_H_
#define _BRASERO_ERROR_H_

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
	BRASERO_BURN_ERROR_NONE,
	BRASERO_BURN_ERROR_GENERAL,

	BRASERO_BURN_ERROR_PLUGIN_MISBEHAVIOR,

	BRASERO_BURN_ERROR_SLOW_DMA,
	BRASERO_BURN_ERROR_PERMISSION,
	BRASERO_BURN_ERROR_DRIVE_BUSY,
	BRASERO_BURN_ERROR_DISK_SPACE,

	BRASERO_BURN_ERROR_EMPTY,
	BRASERO_BURN_ERROR_INPUT_INVALID,

	BRASERO_BURN_ERROR_OUTPUT_NONE,

	BRASERO_BURN_ERROR_FILE_INVALID,
	BRASERO_BURN_ERROR_FILE_FOLDER,
	BRASERO_BURN_ERROR_FILE_PLAYLIST,
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

	BRASERO_BURN_ERROR_MISSING_APP_AND_PLUGIN,

	/* these are not necessarily error */
	BRASERO_BURN_WARNING_CHECKSUM,
	BRASERO_BURN_WARNING_INSERT_AFTER_COPY,

	BRASERO_BURN_ERROR_TMP_DIRECTORY,
	BRASERO_BURN_ERROR_ENCRYPTION_KEY
} BraseroBurnError;

/**
 * Error handling and defined return values
 */

GQuark brasero_burn_quark (void);

#define BRASERO_BURN_ERROR				\
	brasero_burn_quark()

G_END_DECLS

#endif /* _BRASERO_ERROR_H_ */

