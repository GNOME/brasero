/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8-*- */
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

#ifndef _BRASERO_DRIVE_SELECTION_H_
#define _BRASERO_DRIVE_SELECTION_H_

#include <glib-object.h>
#include <gtk/gtkvbox.h>

#include "burn-medium-monitor.h"
#include "burn-medium.h"
#include "burn-drive.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_DRIVE_SELECTION             (brasero_drive_selection_get_type ())
#define BRASERO_DRIVE_SELECTION(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_DRIVE_SELECTION, BraseroDriveSelection))
#define BRASERO_DRIVE_SELECTION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_DRIVE_SELECTION, BraseroDriveSelectionClass))
#define BRASERO_IS_DRIVE_SELECTION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_DRIVE_SELECTION))
#define BRASERO_IS_DRIVE_SELECTION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_DRIVE_SELECTION))
#define BRASERO_DRIVE_SELECTION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_DRIVE_SELECTION, BraseroDriveSelectionClass))

typedef struct _BraseroDriveSelectionClass BraseroDriveSelectionClass;
typedef struct _BraseroDriveSelection BraseroDriveSelection;

struct _BraseroDriveSelectionClass
{
	GtkVBoxClass parent_class;

	void	(*drive_changed)	(BraseroDriveSelection *selection,
					 BraseroDrive *drive);
};

struct _BraseroDriveSelection
{
	GtkVBox parent_instance;
};

GType brasero_drive_selection_get_type (void) G_GNUC_CONST;

GtkWidget *
brasero_drive_selection_new (void);

void
brasero_drive_selection_lock (BraseroDriveSelection *selection,
			      gboolean locked);

void
brasero_drive_selection_set_drive (BraseroDriveSelection *selection,
				   BraseroDrive *drive);

void
brasero_drive_selection_set_tooltip (BraseroDriveSelection *selection,
				     const gchar *tooltip);

BraseroDrive *
brasero_drive_selection_get_drive (BraseroDriveSelection *selection);

BraseroMedium *
brasero_drive_selection_get_medium (BraseroDriveSelection *selection);

void
brasero_drive_selection_set_image_path (BraseroDriveSelection *selection,
					const gchar *path);

void
brasero_drive_selection_set_same_src_dest (BraseroDriveSelection *selection);

void
brasero_drive_selection_set_button (BraseroDriveSelection *selection,
				    GtkWidget *button);

void
brasero_drive_selection_set_type_shown (BraseroDriveSelection *self,
					BraseroMediaType type);

G_END_DECLS

#endif /* _BRASERO_DRIVE_SELECTION_H_ */
