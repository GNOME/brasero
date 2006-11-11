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

#define ISO9660_VOL_TYPE_OFF			0

#define PRIMARY_VOL_STANDARD_ID_OFF		1
#define PRIMARY_VOL_IDENTIFIER_OFF		40
#define PRIMARY_VOL_IDENTIFIER_SIZE		32
#define PRIMARY_VOL_SPACE_OFF			80

static gboolean
brasero_volume_get_primary (FILE *file,
			    gchar *primary_vol,
			    GError **error)
{
	int bytes_read;

	/* skip the first 16 blocks */
	if (fseek (file, 16 * ISO9660_BLOCK_SIZE, SEEK_SET) == -1) {
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

	/* make a few checks to ensure this is an 
	 * iso 9660 primary volume descriptor */
	if (primary_vol [ISO9660_VOL_TYPE_OFF] != 1) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("it does not appear to be a primary volume descriptor"));
		return FALSE;
	}

	if (strncmp (primary_vol + PRIMARY_VOL_STANDARD_ID_OFF, "CD001", 5)
	&&  strncmp (primary_vol + PRIMARY_VOL_STANDARD_ID_OFF, "BEA01", 5)
	&&  strncmp (primary_vol + PRIMARY_VOL_STANDARD_ID_OFF, "BOOT2", 5)
	&&  strncmp (primary_vol + PRIMARY_VOL_STANDARD_ID_OFF, "CDW02", 5)
	&&  strncmp (primary_vol + PRIMARY_VOL_STANDARD_ID_OFF, "NSR02", 5)	/* usually UDF */
	&&  strncmp (primary_vol + PRIMARY_VOL_STANDARD_ID_OFF, "NSR03", 5)	/* usually UDF */
	&&  strncmp (primary_vol + PRIMARY_VOL_STANDARD_ID_OFF, "TEA01", 5)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("it does not appear to be a valid iso9660 image"));
		return FALSE;
	}

	return TRUE;
}

static gint32
brasero_iso9660_get_733_val (gchar *buffer)
{
	gint32 *ptr;

	ptr = (gint32*) buffer;

	return *ptr;
}

gboolean
brasero_volume_get_size (const gchar *path,
			 gint32 *nb_blocks,
			 GError **error)
{
	FILE *file;
	gchar buffer [ISO9660_BLOCK_SIZE];

	file = fopen (path, "r");
	if (!file) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     strerror (errno));
		return FALSE;
	}

	if (!brasero_volume_get_primary (file, buffer, error)) {
		fclose (file);
		return FALSE;
	}

	/* read the size of the volume and the identifier */
	*nb_blocks = brasero_iso9660_get_733_val (buffer + PRIMARY_VOL_SPACE_OFF);

	fclose (file);
	return TRUE;
}

gboolean
brasero_volume_get_label (const gchar *path,
			  gchar **label,
			  GError **error)
{
	FILE *file;
	gchar buffer [ISO9660_BLOCK_SIZE];

	file = fopen (path, "r");
	if (!file) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     strerror (errno));
		return FALSE;
	}

	if (!brasero_volume_get_primary (file, buffer, error)) {
		fclose (file);
		return FALSE;
	}

	/* read the size of the volume and the identifier */
	*label = g_strndup (buffer + PRIMARY_VOL_IDENTIFIER_OFF,
			    PRIMARY_VOL_IDENTIFIER_SIZE);

	fclose (file);
	return TRUE;	
}
