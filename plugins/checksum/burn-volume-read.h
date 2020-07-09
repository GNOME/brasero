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
 
#ifndef BRASERO_MEDIUM_HANDLE_H
#define BRASERO_MEDIUM_HANDLE_H

#include <glib.h>

#include "burn-basics.h"
#include "burn-volume-source.h"

G_BEGIN_DECLS

typedef struct _BraseroVolFileHandle BraseroVolFileHandle;


BraseroVolFileHandle *
brasero_volume_file_open (BraseroVolSrc *src,
			  BraseroVolFile *file);

void
brasero_volume_file_close (BraseroVolFileHandle *handle);

gboolean
brasero_volume_file_rewind (BraseroVolFileHandle *handle);

gint
brasero_volume_file_read (BraseroVolFileHandle *handle,
			  gchar *buffer,
			  guint len);

BraseroBurnResult
brasero_volume_file_read_line (BraseroVolFileHandle *handle,
			       gchar *buffer,
			       guint len);

BraseroVolFileHandle *
brasero_volume_file_open_direct (BraseroVolSrc *src,
				 BraseroVolFile *file);

gint64
brasero_volume_file_read_direct (BraseroVolFileHandle *handle,
				 guchar *buffer,
				 guint blocks);

G_END_DECLS

#endif /* BRASERO_MEDIUM_HANDLE_H */

 
