/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Brasero
 * Copyright (C) Philippe Rouquier 2005-2010 <bonfire-app@wanadoo.fr>
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

#ifndef _BRASERO_DRIVE_SETTINGS_H_
#define _BRASERO_DRIVE_SETTINGS_H_

#include <glib-object.h>

#include "brasero-session.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_DRIVE_SETTINGS             (brasero_drive_settings_get_type ())
#define BRASERO_DRIVE_SETTINGS(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_DRIVE_SETTINGS, BraseroDriveSettings))
#define BRASERO_DRIVE_SETTINGS_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_DRIVE_SETTINGS, BraseroDriveSettingsClass))
#define BRASERO_IS_DRIVE_SETTINGS(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_DRIVE_SETTINGS))
#define BRASERO_IS_DRIVE_SETTINGS_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_DRIVE_SETTINGS))
#define BRASERO_DRIVE_SETTINGS_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_DRIVE_SETTINGS, BraseroDriveSettingsClass))

typedef struct _BraseroDriveSettingsClass BraseroDriveSettingsClass;
typedef struct _BraseroDriveSettings BraseroDriveSettings;

struct _BraseroDriveSettingsClass
{
	GObjectClass parent_class;
};

struct _BraseroDriveSettings
{
	GObject parent_instance;
};

GType brasero_drive_settings_get_type (void) G_GNUC_CONST;

BraseroDriveSettings *
brasero_drive_settings_new (void);

void
brasero_drive_settings_set_session (BraseroDriveSettings *self,
                                    BraseroBurnSession *session);

G_END_DECLS

#endif /* _BRASERO_DRIVE_SETTINGS_H_ */
