/***************************************************************************
 *            burn-volume.h
 *
 *  mer nov 15 09:44:34 2006
 *  Copyright  2006  Philippe Rouquier
 *  bonfire-app@wanadoo.fr
 ***************************************************************************/

/*
 *  Brasero is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Brasero is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to:
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
	} dir;

	} specific;

	guint isdir:1;

	/* mainly used internally */
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
