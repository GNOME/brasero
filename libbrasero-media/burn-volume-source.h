/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-media
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-media is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-media authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-media. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-media is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-media is distributed in the hope that it will be useful,
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

#ifndef _BURN_VOLUME_SOURCE_H
#define _BURN_VOLUME_SOURCE_H

#include <glib.h>

#include "scsi-device.h"

G_BEGIN_DECLS

typedef struct _BraseroVolSrc BraseroVolSrc;

typedef gboolean (*BraseroVolSrcReadFunc)	(BraseroVolSrc *src,
						 gchar *buffer,
						 guint size,
						 GError **error);
typedef gint64	 (*BraseroVolSrcSeekFunc)	(BraseroVolSrc *src,
						 guint block,
						 gint whence,
						 GError **error);
struct _BraseroVolSrc {
	BraseroVolSrcReadFunc read;
	BraseroVolSrcSeekFunc seek;
	guint64 position;
	gpointer data;
	guint data_mode;
	guint ref;
};

#define BRASERO_VOL_SRC_SEEK(vol_MACRO, block_MACRO, whence_MACRO, error_MACRO)	\
	vol_MACRO->seek (vol_MACRO, block_MACRO, whence_MACRO, error_MACRO)

#define BRASERO_VOL_SRC_READ(vol_MACRO, buffer_MACRO, num_MACRO, error_MACRO)	\
	vol_MACRO->read (vol_MACRO, buffer_MACRO, num_MACRO, error_MACRO)


BraseroVolSrc *
brasero_volume_source_open_device_handle (BraseroDeviceHandle *handle,
					  GError **error);
BraseroVolSrc *
brasero_volume_source_open_file (const gchar *path,
				 GError **error);
BraseroVolSrc *
brasero_volume_source_open_fd (int fd,
			       GError **error);

void
brasero_volume_source_ref (BraseroVolSrc *vol);

void
brasero_volume_source_close (BraseroVolSrc *src);

G_END_DECLS

#endif /* BURN_VOLUME_SOURCE_H */

 
