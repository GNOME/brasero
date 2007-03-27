/***************************************************************************
 *            burn-volume.c
 *
 *  mer nov 15 09:44:34 2006
 *  Copyright  2006  Philippe Rouquier
 *  bonfire-app@wanadoo.fr
 ***************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <errno.h>
#include <string.h>
#include <stdio.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#include "burn-volume.h"
#include "burn-iso9660.h"
#include "burn-basics.h"

struct _BraseroTagDesc {
	guint16 id;
	guint16 version;
	guchar checksum;
	guchar reserved;
	guint16 serial;
	guint16 crc;
	guint16 crc_len;
	guint32 location;
};
typedef struct _BraseroTagDesc BraseroTagDesc;

struct _BraseroAnchorDesc {
	BraseroTagDesc tag;

	guchar main_extent		[8];
	guchar reserve_extent		[8];
};
typedef struct _BraseroAnchorDesc BraseroAnchorDesc;

#define SYSTEM_AREA_SECTORS		16
#define ANCHOR_AREA_SECTORS		256


void
brasero_volume_file_free (BraseroVolFile *file)
{
	if (!file)
		return;

	if (file->isdir) {
		GList *iter;

		for (iter = file->specific.dir.children; iter; iter = iter->next)
			brasero_volume_file_free (iter->data);

		g_list_free (file->specific.dir.children);
	}

	g_free (file->rr_name);
	g_free (file->name);
	g_free (file);
}

static gboolean
brasero_volume_get_primary_from_file (FILE *file,
				      gchar *primary_vol,
				      GError **error)
{
	BraseroVolDesc *vol;
	int bytes_read;

	/* skip the first 16 blocks */
	if (fseek (file, SYSTEM_AREA_SECTORS * ISO9660_BLOCK_SIZE, SEEK_CUR) == -1) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     strerror (errno));
		return FALSE;
	}

	bytes_read = fread (primary_vol, 1, ISO9660_BLOCK_SIZE, file);
	if (bytes_read != ISO9660_BLOCK_SIZE) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     strerror (errno));
		return FALSE;
	}

	/* make a few checks to ensure this is an ECMA volume */
	vol = (BraseroVolDesc *) primary_vol;
	if (memcmp (vol->id, "CD001", 5)
	&&  memcmp (vol->id, "BEA01", 5)
	&&  memcmp (vol->id, "BOOT2", 5)
	&&  memcmp (vol->id, "CDW02", 5)
	&&  memcmp (vol->id, "NSR02", 5)	/* usually UDF */
	&&  memcmp (vol->id, "NSR03", 5)	/* usually UDF */
	&&  memcmp (vol->id, "TEA01", 5)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("there isn't a valid volume descriptor"));
		return FALSE;
	}

	return TRUE;
}

static gboolean
brasero_volume_get_primary (const gchar *path,
			    gchar *primary_vol,
			    GError **error)
{
	FILE *file;
	gboolean result;

	file = fopen (path, "r");
	if (!file) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     strerror (errno));
		return FALSE;
	}

	result = brasero_volume_get_primary_from_file (file, primary_vol, error);
	fclose (file);

	return result;
}

gboolean
brasero_volume_is_valid (const gchar *path, GError **error)
{
	gchar buffer [ISO9660_BLOCK_SIZE];

	if (!brasero_volume_get_primary (path, buffer, error))
		return FALSE;

	return TRUE;	
}

gboolean
brasero_volume_is_iso9660 (const gchar *path, GError **error)
{
	gchar buffer [ISO9660_BLOCK_SIZE];

	if (!brasero_volume_get_primary (path, buffer, error))
		return FALSE;

	if (!brasero_iso9660_is_primary_descriptor (buffer, error))
		return FALSE;

	return TRUE;
}

gboolean
brasero_volume_get_label (const gchar *path,
			  gchar **label,
			  GError **error)
{
	gchar buffer [ISO9660_BLOCK_SIZE];

	if (!brasero_volume_get_primary (path, buffer, error))
		return FALSE;

	if (!brasero_iso9660_is_primary_descriptor (buffer, error))
		return FALSE;

	return brasero_iso9660_get_label (buffer, label, error);
}

gboolean
brasero_volume_get_size (const gchar *path,
			 gint64 *nb_blocks,
			 GError **error)
{
	gchar buffer [ISO9660_BLOCK_SIZE];

	if (!brasero_volume_get_primary (path, buffer, error))
		return FALSE;

	if (!brasero_iso9660_is_primary_descriptor (buffer, error))
		return FALSE;

	return brasero_iso9660_get_size (buffer, nb_blocks, error);
}

BraseroVolFile *
brasero_volume_get_files (const gchar *path,
			  gint64 block,
			  gchar **label,
			  gint64 *nb_blocks,
			  gint64 *data_blocks,
			  GError **error)
{
	gchar buffer [ISO9660_BLOCK_SIZE];
	BraseroVolFile *volroot;
	FILE *file;

	file = fopen (path, "r");
	if (!file) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     strerror (errno));
		return NULL;
	}

	if (fseek (file, block * ISO9660_BLOCK_SIZE, SEEK_SET) == -1) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     strerror (errno));
		return NULL;
	}

	if (!brasero_volume_get_primary_from_file (file, buffer, error)) {
		fclose (file);
		return NULL;
	}

	if (!brasero_iso9660_is_primary_descriptor (buffer, error)) {
		fclose (file);
		return NULL;
	}

	if (label
	&& !brasero_iso9660_get_label (buffer, label, error)) {
		fclose (file);
		return NULL;
	}

	if (nb_blocks
	&& !brasero_iso9660_get_size (buffer, nb_blocks, error)) {
		fclose (file);
		return NULL;
	}

	volroot = brasero_iso9660_get_contents (file, buffer, data_blocks, error);
	fclose (file);

	return volroot;
}

gchar *
brasero_volume_file_to_path (BraseroVolFile *file)
{
	GString *path;
	BraseroVolFile *parent;
	GSList *components = NULL, *iter, *next;

	if (!file)
		return NULL;

	/* make a list of all the components of the path by going up to root */
	parent = file->parent;
	while (parent && parent->name) {
		components = g_slist_prepend (components, BRASERO_VOLUME_FILE_NAME (parent));
		parent = parent->parent;
	}

	if (!components)
		return NULL;

	path = g_string_new (NULL);
	for (iter = components; iter; iter = next) {
		gchar *name;

		name = iter->data;
		next = iter->next;
		components = g_slist_remove (components, name);

		g_string_append_c (path, G_DIR_SEPARATOR);
		g_string_append (path, name);
	}

	g_slist_free (components);
	return g_string_free (path, FALSE);
}

BraseroVolFile *
brasero_volume_file_from_path (const gchar *ptr,
			       BraseroVolFile *parent)
{
	GList *iter;
	gchar *next;
	gint len;

	/* first determine the name of the directory / file to look for */
	if (!ptr || ptr [0] != '/' || !parent)
		return NULL;

	ptr ++;
	next = g_utf8_strchr (ptr, -1, G_DIR_SEPARATOR);
	if (!next)
		len = strlen (ptr);
	else
		len = next - ptr;

	for (iter = parent->specific.dir.children; iter; iter = iter->next) {
		BraseroVolFile *file;

		file = iter->data;
		if (!strncmp (ptr, BRASERO_VOLUME_FILE_NAME (file), len)) {
			/* we've found it seek for the next if any */
			if (!next)
				return file;

			ptr = next;
			return brasero_volume_file_from_path (ptr, file);
		}
	}

	return NULL;
}

gint64
brasero_volume_file_size (BraseroVolFile *file)
{
	GList *iter;
	gint64 size = 0;

	if (!file->isdir)
		return BRASERO_BYTES_TO_BLOCKS (file->specific.file.size_bytes, 2048);

	for (iter = file->specific.dir.children; iter; iter = iter->next) {
		file = iter->data;

		if (file->isdir)
			size += brasero_volume_file_size (file);
		else
			size += BRASERO_BYTES_TO_BLOCKS (file->specific.file.size_bytes, 2048);
	}

	return size;
}
