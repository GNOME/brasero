/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-media
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-media is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-media authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-media. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-media is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-media is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifndef _BRASERO_MEDIUM_MONITOR_H_
#define _BRASERO_MEDIUM_MONITOR_H_

#include <glib-object.h>

#include <brasero-medium.h>
#include <brasero-drive.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_MEDIUM_MONITOR             (brasero_medium_monitor_get_type ())
#define BRASERO_MEDIUM_MONITOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_MEDIUM_MONITOR, BraseroMediumMonitor))
#define BRASERO_MEDIUM_MONITOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_MEDIUM_MONITOR, BraseroMediumMonitorClass))
#define BRASERO_IS_MEDIUM_MONITOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_MEDIUM_MONITOR))
#define BRASERO_IS_MEDIUM_MONITOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_MEDIUM_MONITOR))
#define BRASERO_MEDIUM_MONITOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_MEDIUM_MONITOR, BraseroMediumMonitorClass))

typedef struct _BraseroMediumMonitorClass BraseroMediumMonitorClass;
typedef struct _BraseroMediumMonitor BraseroMediumMonitor;


struct _BraseroMediumMonitorClass
{
	GObjectClass parent_class;

	/* Signals */
	void		(* drive_added)		(BraseroMediumMonitor *monitor,
						 BraseroDrive *drive);

	void		(* drive_removed)	(BraseroMediumMonitor *monitor,
						 BraseroDrive *drive);

	void		(* medium_added)	(BraseroMediumMonitor *monitor,
						 BraseroMedium *medium);

	void		(* medium_removed)	(BraseroMediumMonitor *monitor,
						 BraseroMedium *medium);
};

struct _BraseroMediumMonitor
{
	GObject parent_instance;
};

GType brasero_medium_monitor_get_type (void) G_GNUC_CONST;

BraseroMediumMonitor *
brasero_medium_monitor_get_default (void);

typedef enum {
	BRASERO_MEDIA_TYPE_NONE				= 0,
	BRASERO_MEDIA_TYPE_FILE				= 1,
	BRASERO_MEDIA_TYPE_DATA				= 1 << 1,
	BRASERO_MEDIA_TYPE_AUDIO			= 1 << 2,
	BRASERO_MEDIA_TYPE_WRITABLE			= 1 << 3,
	BRASERO_MEDIA_TYPE_REWRITABLE			= 1 << 4,
	BRASERO_MEDIA_TYPE_ANY_IN_BURNER		= 1 << 5,

	/* If combined with other flags it will filter.
	 * if alone all CDs are returned.
	 * It can't be combined with FILE type. */
	BRASERO_MEDIA_TYPE_CD					= 1 << 6,

	BRASERO_MEDIA_TYPE_ALL_BUT_FILE			= 0xFE,
	BRASERO_MEDIA_TYPE_ALL				= 0xFF
} BraseroMediaType;

typedef enum {
	BRASERO_DRIVE_TYPE_NONE				= 0,
	BRASERO_DRIVE_TYPE_FILE				= 1,
	BRASERO_DRIVE_TYPE_WRITER			= 1 << 1,
	BRASERO_DRIVE_TYPE_READER			= 1 << 2,
	BRASERO_DRIVE_TYPE_ALL_BUT_FILE			= 0xFE,
	BRASERO_DRIVE_TYPE_ALL				= 0xFF
} BraseroDriveType;

GSList *
brasero_medium_monitor_get_media (BraseroMediumMonitor *monitor,
				  BraseroMediaType type);

GSList *
brasero_medium_monitor_get_drives (BraseroMediumMonitor *monitor,
				   BraseroDriveType type);

BraseroDrive *
brasero_medium_monitor_get_drive (BraseroMediumMonitor *monitor,
				  const gchar *device);

gboolean
brasero_medium_monitor_is_probing (BraseroMediumMonitor *monitor);

G_END_DECLS

#endif /* _BRASERO_MEDIUM_MONITOR_H_ */
