/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8 -*-
 *
 * brasero-drive-selection.h
 *
 * Copyright (C) 2002-2004 Bastien Nocera <hadess@hadess.net>
 * Copyright (C) 2005-2006 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2009      Philippe Rouquier <bonfire-app@wanadoo.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors: Bastien Nocera <hadess@hadess.net>
 *          William Jon McCann <mccann@jhu.edu>
 */

#ifndef _BRASERO_DRIVE_SELECTION_H_
#define _BRASERO_DRIVE_SELECTION_H_

#include <gtk/gtk.h>

#include <burn-drive.h>
#include <burn-medium-monitor.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_DRIVE_SELECTION              (brasero_drive_selection_get_type ())
#define BRASERO_DRIVE_SELECTION(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_DRIVE_SELECTION, BraseroDriveSelection))
#define BRASERO_DRIVE_SELECTION_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_DRIVE_SELECTION, BraseroDriveSelectionClass))
#define BRASERO_IS_DRIVE_SELECTION(obj)           (G_TYPE_CHECK_INSTANCE_TYPE (obj, BRASERO_TYPE_DRIVE_SELECTION))
#define BRASERO_IS_DRIVE_SELECTION_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_DRIVE_SELECTION))


typedef struct {
        GtkComboBox                        widget;
} BraseroDriveSelection;

typedef struct {
        GtkComboBoxClass parent_class;

        void (* drive_changed)  (GtkWidget         *selection,
                                 BraseroDrive      *drive);
} BraseroDriveSelectionClass;

GtkType                  brasero_drive_selection_get_type           (void);
GtkWidget               *brasero_drive_selection_new                (void);

void                     brasero_drive_selection_set_active         (BraseroDriveSelection *selection,
                                                                     BraseroDrive          *drive);
BraseroDrive            *brasero_drive_selection_get_active         (BraseroDriveSelection *selection);

void                     brasero_drive_selection_show_type          (BraseroDriveSelection *drive,
                                                                     BraseroDriveType       type);

G_END_DECLS

#endif /* _BRASERO_DRIVE_SELECTION_H_ */
