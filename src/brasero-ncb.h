/***************************************************************************
 *            brasero_ncb.h
 *
 *  Sun Sep  3 11:03:26 2006
 *  Copyright  2006  philippe
 *  <philippe@Rouquier Philippe.localdomain>
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

#include <nautilus-burn-drive-monitor.h>
#include <nautilus-burn-drive.h>

#include "burn-medium.h"

G_BEGIN_DECLS

#define NCB_DRIVE_GET_TYPE(drive) 	\
nautilus_burn_drive_get_drive_type ((drive))

#define NCB_DRIVE_GET_DEVICE(drive) 	\
nautilus_burn_drive_get_device (drive)

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

BraseroMedium *
NCB_DRIVE_GET_MEDIUM (NautilusBurnDrive *drive);

void
NCB_DRIVE_SET_MEDIUM (NautilusBurnDrive *drive,
		      BraseroMedium *medium);

gboolean
NCB_DRIVE_UNMOUNT (NautilusBurnDrive *drive, GError **error);

gboolean
NCB_DRIVE_MOUNT (NautilusBurnDrive *drive, GError **error);

gchar *
NCB_VOLUME_GET_MOUNT_POINT (NautilusBurnDrive *drive, GError **error);

gboolean
NCB_MEDIA_GET_LAST_DATA_TRACK_ADDRESS (NautilusBurnDrive *drive,
				       gint64 *byte,
				       gint64 *sector);

gboolean
NCB_MEDIA_GET_LAST_DATA_TRACK_SPACE (NautilusBurnDrive *drive,
				     gint64 *size,
				     gint64 *blocks);

guint
NCB_MEDIA_GET_TRACK_NUM (NautilusBurnDrive *drive);

gboolean
NCB_MEDIA_GET_TRACK_ADDRESS (NautilusBurnDrive *drive,
			     guint num,
			     gint64 *byte,
			     gint64 *sector);

gboolean
NCB_MEDIA_GET_TRACK_SPACE (NautilusBurnDrive *drive,
			   guint num,
			   gint64 *size,
			   gint64 *blocks);

gint64
NCB_MEDIA_GET_NEXT_WRITABLE_ADDRESS (NautilusBurnDrive *drive);

gint64
NCB_MEDIA_GET_MAX_WRITE_RATE (NautilusBurnDrive *drive);

void
NCB_MEDIA_GET_DATA_SIZE (NautilusBurnDrive *drive,
			 gint64 *size,
			 gint64 *blocks);

void
NCB_MEDIA_GET_CAPACITY (NautilusBurnDrive *drive,
			gint64 *size,
			gint64 *blocks);

void
NCB_MEDIA_GET_FREE_SPACE (NautilusBurnDrive *drive,
			  gint64 *size,
			  gint64 *blocks);

BraseroMedia
NCB_MEDIA_GET_STATUS (NautilusBurnDrive *drive);

const gchar *
NCB_MEDIA_GET_TYPE_STRING (NautilusBurnDrive *drive);

const gchar *
NCB_MEDIA_GET_ICON (NautilusBurnDrive *drive);

#define NCB_MEDIA_IS(drive, flags)			\
BRASERO_MEDIUM_IS (NCB_MEDIA_GET_STATUS (drive),(flags))

void
NCB_INIT (void);

G_END_DECLS

#endif /* _BRASERO_NCB_H */

 
