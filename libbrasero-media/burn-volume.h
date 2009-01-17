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

#include <glib.h>

#ifndef BURN_VOLUME_H
#define BURN_VOLUME_H

G_BEGIN_DECLS

#include "burn-volume-source.h"

struct _BraseroVolDesc {
	guchar type;
	gchar id			[5];
	guchar version;
};
typedef struct _BraseroVolDesc BraseroVolDesc;

struct _BraseroVolFileExtent {
	guint block;
	guint size;
};
typedef struct _BraseroVolFileExtent BraseroVolFileExtent;

typedef struct _BraseroVolFile BraseroVolFile;
struct _BraseroVolFile {
	BraseroVolFile *parent;

	gchar *name;
	gchar *rr_name;

	union {

	struct {
		GSList *extents;
		guint64 size_bytes;
	} file;

	struct {
		GList *children;
		guint address;
	} dir;

	} specific;

	guint isdir:1;
	guint isdir_loaded:1;

	/* mainly used internally */
	guint has_RR:1;
	guint relocated:1;
};

gboolean
brasero_volume_is_valid (BraseroVolSrc *src,
			 GError **error);

gboolean
brasero_volume_get_size (BraseroVolSrc *src,
			 gint64 block,
			 gint64 *nb_blocks,
			 GError **error);

BraseroVolFile *
brasero_volume_get_files (BraseroVolSrc *src,
			  gint64 block,
			  gchar **label,
			  gint64 *nb_blocks,
			  gint64 *data_blocks,
			  GError **error);

BraseroVolFile *
brasero_volume_get_file (BraseroVolSrc *src,
			 const gchar *path,
			 gint64 volume_start_block,
			 GError **error);

GList *
brasero_volume_load_directory_contents (BraseroVolSrc *vol,
					gint64 session_block,
					gint64 block,
					GError **error);


#define BRASERO_VOLUME_FILE_NAME(file)			((file)->rr_name?(file)->rr_name:(file)->name)
#define BRASERO_VOLUME_FILE_SIZE(file)			((file)->isdir?0:(file)->specific.file.size_bytes)

void
brasero_volume_file_free (BraseroVolFile *file);

gchar *
brasero_volume_file_to_path (BraseroVolFile *file);

BraseroVolFile *
brasero_volume_file_from_path (const gchar *ptr,
			       BraseroVolFile *parent);

gint64
brasero_volume_file_size (BraseroVolFile *file);

BraseroVolFile *
brasero_volume_file_merge (BraseroVolFile *file1,
			   BraseroVolFile *file2);

G_END_DECLS

#endif /* BURN_VOLUME_H */
