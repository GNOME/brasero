/***************************************************************************
 *            scsi-device.h
 *
 *  Mon Feb 11 16:55:05 2008
 *  Copyright  2008  Philippe Rouquier
 *  <bonfire-app@wanadoo.fr>
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
 
#ifndef _SCSI_DEVICE_H
#define _SCSI_DEVICE_H

#include <glib.h>

#include "scsi-error.h"

G_BEGIN_DECLS

typedef struct _BraseroDeviceHandle BraseroDeviceHandle;

BraseroDeviceHandle *
brasero_device_handle_open (const gchar *path, BraseroScsiErrCode *error);

void
brasero_device_handle_close (BraseroDeviceHandle *handle);

G_END_DECLS

#endif /* _SCSI_DEVICE_H */

 
