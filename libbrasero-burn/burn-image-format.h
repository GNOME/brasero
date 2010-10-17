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
 
#ifndef _BURN_IMAGES_FORMAT_H
#define _BURN_IMAGES_FORMAT_H

#include <glib.h>

#include "burn-basics.h"

G_BEGIN_DECLS

BraseroImageFormat
brasero_image_format_identify_cuesheet (const gchar *path,
					GCancellable *cancel,
					GError **error);

gchar *
brasero_image_format_get_default_path (BraseroImageFormat format,
				       const gchar *name);

gchar *
brasero_image_format_fix_path_extension (BraseroImageFormat format,
					 gboolean check_existence,
					 const gchar *path);
gchar *
brasero_image_format_get_complement (BraseroImageFormat format,
				     const gchar *path);

gboolean
brasero_image_format_get_cdrdao_size (gchar *uri,
				      guint64 *sectors,
				      guint64 *size_img,
				      GCancellable *cancel,				      
				      GError **error);
gboolean
brasero_image_format_get_cue_size (gchar *uri,
				   guint64 *blocks,
				   guint64 *size_img,
				   GCancellable *cancel,
				   GError **error);
gboolean
brasero_image_format_get_iso_size (gchar *uri,
				   guint64 *blocks,
				   guint64 *size_img,
				   GCancellable *cancel,
				   GError **error);
gboolean
brasero_image_format_get_clone_size (gchar *uri,
				     guint64 *blocks,
				     guint64 *size_img,
				     GCancellable *cancel,
				     GError **error);

gboolean
brasero_image_format_cue_bin_byte_swap (gchar *uri,
					GCancellable *cancel,
					GError **error);
G_END_DECLS

#endif /* _BURN_IMAGES_FORMAT_H */

 
