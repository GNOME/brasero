/***************************************************************************
 *            burn-images-format.h
 *
 *  Mon Nov  5 18:49:41 2007
 *  Copyright  2007  Philippe Rouquier
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
 
#ifndef _BURN_IMAGES_FORMAT_H
#define _BURN_IMAGES_FORMAT_H

#include <glib.h>

#include "burn-basics.h"

G_BEGIN_DECLS

typedef enum {
	BRASERO_IMAGE_FORMAT_NONE		= 0,
	BRASERO_IMAGE_FORMAT_BIN		= 1,
	BRASERO_IMAGE_FORMAT_CUE		= 1 << 1,
	BRASERO_IMAGE_FORMAT_CLONE		= 1 << 2,
	BRASERO_IMAGE_FORMAT_CDRDAO		= 1 << 3,
	BRASERO_IMAGE_FORMAT_ANY		= BRASERO_IMAGE_FORMAT_BIN|
						  BRASERO_IMAGE_FORMAT_CUE|
						  BRASERO_IMAGE_FORMAT_CDRDAO|
						  BRASERO_IMAGE_FORMAT_CLONE,
} BraseroImageFormat;

BraseroImageFormat
brasero_image_format_identify_cuesheet (const gchar *path);

gchar *
brasero_image_format_get_default_path (BraseroImageFormat format);

gchar *
brasero_image_format_fix_path_extension (BraseroImageFormat format,
					 gboolean check_existence,
					 gchar *path);
gchar *
brasero_image_format_get_complement (BraseroImageFormat format,
				     const gchar *path);

gboolean
brasero_image_format_get_cdrdao_size (gchar *path,
				      gint64 *sectors,
				      gint64 *size,
				      GError **error);
gboolean
brasero_image_format_get_cue_size (gchar *path,
				   gint64 *size,
				   gint64 *blocks,
				   GError **error);
gboolean
brasero_image_format_get_iso_size (gchar *path,
				   gint64 *blocks,
				   gint64 *size,
				   GError **error);
gboolean
brasero_image_format_get_clone_size (gchar *path,
				     gint64 *blocks,
				     gint64 *size,
				     GError **error);

G_END_DECLS

#endif /* _BURN_IMAGES_FORMAT_H */

 
