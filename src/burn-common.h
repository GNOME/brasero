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
#include "burn-track.h"

G_BEGIN_DECLS


gchar *
brasero_get_file_complement (BraseroImageFormat format,
			     gboolean is_image,
			     const gchar *uri);

BraseroBurnResult
brasero_burn_common_create_tmp_directory (gchar **directory_path,
					  gboolean overwrite,
					  GError **error);

G_END_DECLS

#endif /* _BURN-COMMON_H */
