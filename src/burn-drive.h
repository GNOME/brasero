/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * trunk
 * Copyright (C) Philippe Rouquier 2008 <bonfire-app@wanadoo.fr>
 * 
 * trunk is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * trunk is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with trunk.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#include <glib-object.h>

#include <nautilus-burn-drive.h>

#ifndef _BURN_DRIVE_H_
#define _BURN_DRIVE_H_

#include "burn-medium.h"

G_BEGIN_DECLS

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
};

struct _BraseroDrive
{
	GObject parent_instance;
};

GType brasero_drive_get_type (void) G_GNUC_CONST;

BraseroDrive *
brasero_drive_new (NautilusBurnDrive *drive);

NautilusBurnDrive *
brasero_drive_get_nautilus_drive (BraseroDrive *drive);

void
brasero_drive_set_medium (BraseroDrive *drive,
			  BraseroMedium *medium);

BraseroMedium *
brasero_drive_get_medium (BraseroDrive *drive);

gboolean
brasero_drive_is_fake (BraseroDrive *self);

gchar *
brasero_drive_get_display_name (BraseroDrive *self);

gchar *
brasero_drive_get_volume_label (BraseroDrive *self);

const gchar *
brasero_drive_get_device (BraseroDrive *self);

gboolean
brasero_drive_can_write (BraseroDrive *self);

gboolean
brasero_drive_can_rewrite (BraseroDrive *self);

gboolean
brasero_drive_eject (BraseroDrive *drive);

gboolean
brasero_drive_mount (BraseroDrive *drive,
		     GError **error);

gboolean
brasero_drive_unmount (BraseroDrive *drive,
		       GError **error);

gboolean
brasero_drive_unmount_wait (BraseroDrive *drive);

gboolean
brasero_drive_is_mounted (BraseroDrive *self);

gboolean
brasero_drive_is_door_open (BraseroDrive *self);

gchar *
brasero_drive_get_mount_point (BraseroDrive *drive,
			       GError **error);

gboolean
brasero_drive_lock (BraseroDrive *self,
		    const gchar *reason,
		    gchar **reason_for_failure);
gboolean
brasero_drive_unlock (BraseroDrive *self);

G_END_DECLS

#endif /* _BURN_DRIVE_H_ */
