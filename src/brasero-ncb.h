/***************************************************************************
 *            brasero_ncb.h
 *
 *  Sun Sep  3 11:03:26 2006
 *  Copyright  2006  philippe
 *  <philippe@algernon.localdomain>
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
 
#ifndef _BRASERO_NCB_H
#define _BRASERO_NCB_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <nautilus-burn-drive.h>

#ifdef __cplusplus

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <nautilus-burn-drive-monitor.h>
#include <nautilus-burn-drive.h>

extern "C"
{
#endif

#define NCB_DRIVE_GET_TYPE(drive) 	\
nautilus_burn_drive_get_drive_type ((drive))

#define NCB_DRIVE_GET_DEVICE(drive) 	\
nautilus_burn_drive_get_device (drive)

#define NCB_MEDIA_GET_SIZE(drive)			\
nautilus_burn_drive_get_media_size (drive)

#define NCB_MEDIA_GET_CAPACITY(drive)	\
nautilus_burn_drive_get_media_capacity (drive)

#define NCB_DRIVE_GET_LIST(list, recorders, image)	\
{	\
	NautilusBurnDriveMonitor *monitor;	\
	monitor = nautilus_burn_get_drive_monitor ();	\
	if (recorders) { \
		list = nautilus_burn_drive_monitor_get_recorder_drives (monitor);	\
	} else {	\
		list = nautilus_burn_drive_monitor_get_drives (monitor);	\
	}	\
	if (image)	\
		list = g_list_prepend (list, nautilus_burn_drive_monitor_get_drive_for_image (monitor));	\
}

NautilusBurnMediaType
NCB_DRIVE_MEDIA_GET_TYPE (NautilusBurnDrive *drive,
			  gboolean *is_rewritable,
			  gboolean *is_blank,
			  gboolean *has_data,
			  gboolean *has_audio);
gboolean
NCB_MEDIA_HAS_VALID_FS (NautilusBurnDrive *drive);

gboolean
NCB_DRIVE_UNMOUNT (NautilusBurnDrive *drive, GError **error);

gboolean
NCB_DRIVE_MOUNT (NautilusBurnDrive *drive, GError **error);

typedef gpointer BraseroMountHandle;
typedef void	(*BraseroMountCallback)	(NautilusBurnDrive *drive,
					 const gchar *mount_point,
					 gboolean mounted_by_us,
					 const GError *error,
					 gpointer callback_data);

BraseroMountHandle *
NCB_DRIVE_GET_MOUNT_POINT (NautilusBurnDrive *drive,
			   BraseroMountCallback callback,
			   gpointer callback_data);

void
NCB_DRIVE_GET_MOUNT_POINT_CANCEL (BraseroMountHandle handle);

#ifdef __cplusplus
}
#endif

#endif /* _BRASERO_NCB_H */

 
