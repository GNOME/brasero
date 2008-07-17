/***************************************************************************
 *            burn-iso-field.h
 *
 *  Mon Nov 27 17:32:39 2006
 *  Copyright  2006  Rouquier Philippe
 *  <Rouquier Philippe@localhost.localdomain>
 ****************************************************************************/

/*
 * Brasero is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * Brasero is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */

#include <glib.h>

#ifndef _BURN_ISO_FIELD_H
#define _BURN_ISO_FIELD_H

#ifdef __cplusplus
extern "C"
{
#endif

guint32
brasero_iso9660_get_733_val (guchar *buffer);

#ifdef __cplusplus
}
#endif

#endif /* _BURN_ISO_FIELD_H */

 
