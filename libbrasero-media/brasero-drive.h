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

#include <glib-object.h>
#include <gio/gio.h>

#ifndef _BURN_DRIVE_H_
#define _BURN_DRIVE_H_

#include <brasero-medium.h>

G_BEGIN_DECLS

typedef enum {
	BRASERO_DRIVE_CAPS_NONE			= 0,
	BRASERO_DRIVE_CAPS_CDR			= 1,
	BRASERO_DRIVE_CAPS_CDRW			= 1 << 1,
	BRASERO_DRIVE_CAPS_DVDR			= 1 << 2,
	BRASERO_DRIVE_CAPS_DVDRW		= 1 << 3,
	BRASERO_DRIVE_CAPS_DVDR_PLUS		= 1 << 4,
	BRASERO_DRIVE_CAPS_DVDRW_PLUS		= 1 << 5,
	BRASERO_DRIVE_CAPS_DVDR_PLUS_DL		= 1 << 6,
	BRASERO_DRIVE_CAPS_DVDRW_PLUS_DL	= 1 << 7,
	BRASERO_DRIVE_CAPS_DVDRAM		= 1 << 10,
	BRASERO_DRIVE_CAPS_BDR			= 1 << 8,
	BRASERO_DRIVE_CAPS_BDRW			= 1 << 9
} BraseroDriveCaps;

#define BRASERO_TYPE_DRIVE             (brasero_drive_get_type ())
#define BRASERO_DRIVE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_DRIVE, BraseroDrive))
#define BRASERO_DRIVE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_DRIVE, BraseroDriveClass))
#define BRASERO_IS_DRIVE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_DRIVE))
#define BRASERO_IS_DRIVE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_DRIVE))
#define BRASERO_DRIVE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_DRIVE, BraseroDriveClass))

typedef struct _BraseroDriveClass BraseroDriveClass;

struct _BraseroDriveClass
{
	GObjectClass parent_class;

	/* Signals */
	void		(* medium_added)	(BraseroDrive *drive,
						 BraseroMedium *medium);

	void		(* medium_removed)	(BraseroDrive *drive,
						 BraseroMedium *medium);
};

struct _BraseroDrive
{
	GObject parent_instance;
};

GType brasero_drive_get_type (void) G_GNUC_CONST;

void
brasero_drive_reprobe (BraseroDrive *drive);

BraseroMedium *
brasero_drive_get_medium (BraseroDrive *drive);

GDrive *
brasero_drive_get_gdrive (BraseroDrive *drive);

const gchar *
brasero_drive_get_udi (BraseroDrive *drive);

gboolean
brasero_drive_is_fake (BraseroDrive *drive);

gchar *
brasero_drive_get_display_name (BraseroDrive *drive);

const gchar *
brasero_drive_get_device (BraseroDrive *drive);

const gchar *
brasero_drive_get_block_device (BraseroDrive *drive);

gchar *
brasero_drive_get_bus_target_lun_string (BraseroDrive *drive);

BraseroDriveCaps
brasero_drive_get_caps (BraseroDrive *drive);

gboolean
brasero_drive_can_write_media (BraseroDrive *drive,
                               BraseroMedia media);

gboolean
brasero_drive_can_write (BraseroDrive *drive);

gboolean
brasero_drive_can_eject (BraseroDrive *drive);

gboolean
brasero_drive_eject (BraseroDrive *drive,
		     gboolean wait,
		     GError **error);

void
brasero_drive_cancel_current_operation (BraseroDrive *drive);

gboolean
brasero_drive_is_door_open (BraseroDrive *drive);

gboolean
brasero_drive_can_use_exclusively (BraseroDrive *drive);

gboolean
brasero_drive_lock (BraseroDrive *drive,
		    const gchar *reason,
		    gchar **reason_for_failure);
gboolean
brasero_drive_unlock (BraseroDrive *drive);

gboolean
brasero_drive_is_locked (BraseroDrive *drive,
                         gchar **reason);

G_END_DECLS

#endif /* _BURN_DRIVE_H_ */
