/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2007-2008 <bonfire-app@wanadoo.fr>
 * 
 *  Brasero is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 * brasero is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with brasero.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifndef _BRASERO_DRIVE_PROPERTIES_H_
#define _BRASERO_DRIVE_PROPERTIES_H_

#include <glib-object.h>

#include <gtk/gtk.h>

#include "brasero-drive.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_DRIVE_PROPERTIES             (brasero_drive_properties_get_type ())
#define BRASERO_DRIVE_PROPERTIES(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_DRIVE_PROPERTIES, BraseroDriveProperties))
#define BRASERO_DRIVE_PROPERTIES_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_DRIVE_PROPERTIES, BraseroDrivePropertiesClass))
#define BRASERO_IS_DRIVE_PROPERTIES(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_DRIVE_PROPERTIES))
#define BRASERO_IS_DRIVE_PROPERTIES_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_DRIVE_PROPERTIES))
#define BRASERO_DRIVE_PROPERTIES_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_DRIVE_PROPERTIES, BraseroDrivePropertiesClass))

typedef struct _BraseroDrivePropertiesClass BraseroDrivePropertiesClass;
typedef struct _BraseroDriveProperties BraseroDriveProperties;

struct _BraseroDrivePropertiesClass
{
	GtkDialogClass parent_class;
};

struct _BraseroDriveProperties
{
	GtkDialog parent_instance;
};

GType brasero_drive_properties_get_type (void) G_GNUC_CONST;
GtkWidget *brasero_drive_properties_new ();

void
brasero_drive_properties_set_drive (BraseroDriveProperties *props,
				    BraseroDrive *drive,
				    gint64 rate);
void
brasero_drive_properties_set_flags (BraseroDriveProperties *props,
				    BraseroBurnFlag flags,
				    BraseroBurnFlag supported,
				    BraseroBurnFlag compulsory);

void
brasero_drive_properties_set_tmpdir (BraseroDriveProperties *props,
				     const gchar *path);

BraseroBurnFlag
brasero_drive_properties_get_flags (BraseroDriveProperties *props);

gint64
brasero_drive_properties_get_rate (BraseroDriveProperties *props);

gchar *
brasero_drive_properties_get_tmpdir (BraseroDriveProperties *props);

G_END_DECLS

#endif /* _BRASERO_DRIVE_PROPERTIES_H_ */
