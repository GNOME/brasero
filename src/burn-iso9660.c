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

#define BLOCK_SIZE 2048

#define ISO9660_VOL_TYPE_OFF			0
#define ISO9660_VOL_IDENTIFIER_OFF		1

#define PRIMARY_VOL_IDENTIFIER_OFF		40
#define PRIMARY_VOL_SPACE_OFF			80

static gint32
brasero_iso9660_get_733_val (gchar *buffer)
{
	gint32 *ptr;

	ptr = (gint32*) buffer;

	return *ptr;
}

gboolean
brasero_iso9660_get_volume_size (const gchar *path,
				 gint32 *nb_blocks,
				 GError **error)
{
	FILE *file;
	int bytes_read;
	gchar buffer [BLOCK_SIZE];

	file = fopen (path, "r");
	if (!file) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     strerror (errno));
		return FALSE;
	}

	/* skip the first 16 blocks */
	if (fseek (file, 16 * BLOCK_SIZE, SEEK_SET) == -1) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     strerror (errno));
		fclose (file);
		return FALSE;
	}

	bytes_read = fread (buffer, 1, BLOCK_SIZE, file);
	if (bytes_read != BLOCK_SIZE) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     strerror (errno));
		fclose (file);
		return FALSE;
	}

	/* make a few checks to ensure this is an 
	 * iso 9660 primary volume descriptor */
	if (buffer [ISO9660_VOL_TYPE_OFF] != 1) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("it does not appear to be a primary volume descriptor"));
		fclose (file);
		return FALSE;
	}

	if (strncmp (buffer + ISO9660_VOL_IDENTIFIER_OFF, "CD001", 5)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("it does not appear to be a valid iso9660 image"));
		fclose (file);
		return FALSE;
	}

	/* read the size of the volume and the identifier */
	*nb_blocks = brasero_iso9660_get_733_val (buffer + PRIMARY_VOL_SPACE_OFF);

	fclose (file);
	return TRUE;
}
