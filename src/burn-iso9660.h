/***************************************************************************
 *            burn-iso9660.h
 *
 *  Sat Oct  7 17:10:09 2006
 *  Copyright  2006  algernon
 *  <algernon@localhost.localdomain>
 ****************************************************************************/

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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */
 
#ifndef _BURN_ISO9660_H
#define _BURN_ISO9660_H

#ifdef __cplusplus
extern "C"
{
#endif

gboolean
brasero_iso9660_get_volume_size (const gchar *path,
				 gint32 *nb_blocks,
				 GError **error);

#ifdef __cplusplus
}
#endif

#endif /* _BURN_ISO9660_H */

 
