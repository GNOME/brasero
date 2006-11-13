/***************************************************************************
 *            burn-iso9660.h
 *
 *  sam oct 7 17:32:17 2006
 *  Copyright  2006  Rouquier Philippe
 *  brasero-app@wanadoo.fr
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

#include "burn-basics.h"
#include "burn-iso9660.h"

struct _BraseroVolDesc {
	guchar type;
	gchar id			[5];
	guchar version;
};
typedef struct _BraseroVolDesc BraseroVolDesc;

struct _BraseroIsoPrimary {
	guchar type;
	gchar id			[5];
	guchar version;

	gchar system_id			[32];
	gchar vol_id			[32];

	guchar unused			[8];

	guchar vol_size			[8];

};
typedef struct _BraseroIsoPrimary BraseroIsoPrimary;

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

#define FORBIDDEN_AREA_SECTORS		16
#define ANCHOR_AREA_SECTORS		256

static gboolean
brasero_volume_get_primary (const gchar *path,
			    gchar *primary_vol,
			    GError **error)
{
	BraseroVolDesc *vol;
	int bytes_read;
	FILE *file;

	file = fopen (path, "r");
	if (!file) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     strerror (errno));
		return FALSE;
	}

	/* skip the first 16 blocks */
	if (fseek (file, FORBIDDEN_AREA_SECTORS * ISO9660_BLOCK_SIZE, SEEK_SET) == -1) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     strerror (errno));
		fclose (file);
		return FALSE;
	}

	bytes_read = fread (primary_vol, 1, ISO9660_BLOCK_SIZE, file);
	if (bytes_read != ISO9660_BLOCK_SIZE) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     strerror (errno));
		fclose (file);
		return FALSE;
	}

	fclose (file);

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
			     _("it does not appear to be a valid iso9660 image"));
		return FALSE;
	}

	return TRUE;
}

static gint32
brasero_iso9660_get_733_val (guchar *buffer)
{
	guint32 *ptr;

	ptr = (guint32*) buffer;

	return GUINT32_FROM_LE (*ptr);
}

static gboolean
brasero_volume_is_iso9660_primary_real (const char *buffer,
					GError **error)
{
	BraseroVolDesc *vol;

	/* must be CD001 */
	vol = (BraseroVolDesc *) buffer;
	if (memcmp (vol->id, "CD001", 5)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("it does not appear to be a primary volume descriptor"));
		return FALSE;
	}

	/* must be "1" for primary volume */
	if (vol->type != 1) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("it does not appear to be a primary volume descriptor"));
		return FALSE;
	}

	return TRUE;
}

gboolean
brasero_volume_get_size (const gchar *path,
			 gint32 *nb_blocks,
			 GError **error)
{
	gchar buffer [ISO9660_BLOCK_SIZE];
	BraseroIsoPrimary *vol;

	if (!brasero_volume_get_primary (path, buffer, error))
		return FALSE;

	if (!brasero_volume_is_iso9660_primary_real (buffer, error))
		return FALSE;

	/* read the size of the volume */
	vol = (BraseroIsoPrimary *) buffer;
	*nb_blocks = brasero_iso9660_get_733_val (vol->vol_size);

	return TRUE;
}

gboolean
brasero_volume_get_label (const gchar *path,
			  gchar **label,
			  GError **error)
{
	gchar buffer [ISO9660_BLOCK_SIZE];
	BraseroIsoPrimary *vol;

	if (!brasero_volume_get_primary (path, buffer, error))
		return FALSE;

	if (!brasero_volume_is_iso9660_primary_real (buffer, error))
		return FALSE;

	/* read the identifier */
	vol = (BraseroIsoPrimary *) buffer;
	*label = g_strndup (vol->vol_id, sizeof (vol->vol_id));

	return TRUE;	
}

gboolean
brasero_volume_is_iso9660 (const gchar *path, GError **error)
{
	gchar buffer [ISO9660_BLOCK_SIZE];

	if (!brasero_volume_get_primary (path, buffer, error))
		return FALSE;

	if (!brasero_volume_is_iso9660_primary_real (buffer, error))
		return FALSE;
	
	/* udf has an anchor descriptor at logical sector 256,
	 * then we have an iso9660/UDF bridge format */

	return TRUE;
}

gboolean
brasero_volume_is_valid (const gchar *path, GError **error)
{
	gchar buffer [ISO9660_BLOCK_SIZE];

	if (!brasero_volume_get_primary (path, buffer, error))
		return FALSE;

	return TRUE;	
}
