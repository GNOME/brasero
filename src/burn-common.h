/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 
/***************************************************************************
 *            burn-common.h
 *
 *  Tue Feb 14 15:43:55 2006
 *  Copyright  2006  philippe
 *  <brasero-app@wanadoo.fr>
 ****************************************************************************/

 
#ifndef _BURN_COMMON_H
#define _BURN_COMMON_H

#include <glib.h>

#include <nautilus-burn-drive.h>

#include "burn-basics.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define MAX_VALUE_AVERAGE	16

long
brasero_burn_common_compute_time_remaining (gint64 bytes, gdouble bytes_per_sec);

gboolean
brasero_burn_common_rm (const gchar *uri);

gchar *
brasero_get_file_complement (BraseroImageFormat format,
			     gboolean is_image,
			     const gchar *uri);

BraseroBurnResult
brasero_burn_common_check_output (gchar **output,
				  BraseroImageFormat format,
				  gboolean is_image,
				  gboolean overwrite,
				  gchar **toc,
				  GError **error);
gdouble
brasero_burn_common_get_average (GSList **values, gdouble value);

void
brasero_burn_common_eject_async (NautilusBurnDrive *drive);

BraseroBurnResult
brasero_burn_common_check_local_file (const gchar *uri,
				      GError **error);

BraseroBurnResult
brasero_burn_common_create_tmp_directory (gchar **directory_path,
					  gboolean overwrite,
					  GError **error);

BraseroBurnResult
brasero_common_create_pipe (int pipe [2], GError **error);

#ifdef __cplusplus
}
#endif

#endif /* _BURN-COMMON_H */
