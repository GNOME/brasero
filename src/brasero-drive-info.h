/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8-*- */
/*
 * brasero
 * Copyright (C) PhiPhilippe Rouquier 2007-2008nfire-app@wanadoo.fr>
 * 
 * Brasero is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. * 
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

#ifndef _BRASERO_DRIVE_INFO_H_
#define _BRASERO_DRIVE_INFO_H_

#include <glib-object.h>
#include <gtk/gtkhbox.h>

#include "burn-medium.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_DRIVE_INFO            	(brasero_drive_info_get_type ())
#define BRASERO_DRIVE_INFO(obj)            	(G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_DRIVE_INFO, BraseroDriveInfo))
#define BRASERO_DRIVE_INFO_CLASS(klass)    	(G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_DRIVE_INFO, BraseroDriveInfoClass))
#define BRASERO_IS_DRIVE_INFO(obj)         	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_DRIVE_INFO))
#define BRASERO_IS_DRIVE_INFO_CLASS(klass) 	(G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_DRIVE_INFO))
#define BRASERO_DRIVE_INFO_GET_CLASS(obj)   	(G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_DRIVE_INFO, BraseroDriveInfoClass))

typedef struct _BraseroDriveInfoClass BraseroDriveInfoClass;
typedef struct _BraseroDriveInfo BraseroDriveInfo;

struct _BraseroDriveInfoClass
{
	GtkHBoxClass parent_class;
};

struct _BraseroDriveInfo
{
	GtkHBox parent_instance;
};

GType brasero_drive_info_get_type (void) G_GNUC_CONST;

GtkWidget *
brasero_drive_info_new ();

void
brasero_drive_info_set_medium (BraseroDriveInfo *info,
			       BraseroMedium *medium);

void
brasero_drive_info_set_image_path (BraseroDriveInfo *info,
				   const gchar *path);

void
brasero_drive_info_set_same_src_dest (BraseroDriveInfo *info,
				      gboolean same_src_dest);

G_END_DECLS

#endif /* _BRASERO_DRIVE_INFO_H_ */
