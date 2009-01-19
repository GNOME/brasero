/***************************************************************************
 *            brasero-medium-handle.h
 *
 *  Sat Mar 15 17:28:02 2008
 *  Copyright  2008  Philippe Rouquier
 *  <bonfire-app@wanadoo.fr>
 ****************************************************************************/

/*
 * Libbrasero-media is free software; you can redistribute it and/or modify
fy
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */
 
#ifndef _BRASERO_MEDIUM_HANDLE_H
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

G_END_DECLS

#endif /* BRASERO_MEDIUM_HANDLE_H */

 
