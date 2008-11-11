/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2005-2008 <bonfire-app@wanadoo.fr>
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

#ifndef _BRASERO_MEDIUM_SELECTION_H_
#define _BRASERO_MEDIUM_SELECTION_H_

#include <glib-object.h>

#include <gtk/gtk.h>

#include "burn-medium-monitor.h"
#include "burn-medium.h"
#include "burn-drive.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_MEDIUM_SELECTION             (brasero_medium_selection_get_type ())
#define BRASERO_MEDIUM_SELECTION(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_MEDIUM_SELECTION, BraseroMediumSelection))
#define BRASERO_MEDIUM_SELECTION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_MEDIUM_SELECTION, BraseroMediumSelectionClass))
#define BRASERO_IS_MEDIUM_SELECTION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_MEDIUM_SELECTION))
#define BRASERO_IS_MEDIUM_SELECTION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_MEDIUM_SELECTION))
#define BRASERO_MEDIUM_SELECTION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_MEDIUM_SELECTION, BraseroMediumSelectionClass))

typedef struct _BraseroMediumSelectionClass BraseroMediumSelectionClass;
typedef struct _BraseroMediumSelection BraseroMediumSelection;

struct _BraseroMediumSelectionClass
{
	GtkComboBoxClass parent_class;

	/* virtual function */
	gchar *		(*format_medium_string)		(BraseroMediumSelection *selection,
							 BraseroMedium *medium);
};

struct _BraseroMediumSelection
{
	GtkComboBox parent_instance;
};

GType brasero_medium_selection_get_type (void) G_GNUC_CONST;
GtkWidget* brasero_medium_selection_new (void);

typedef gboolean (*BraseroMediumSelectionFunc) (BraseroMedium *medium, gpointer callback_data);

BraseroMedium *
brasero_medium_selection_get_active (BraseroMediumSelection *selection);

BraseroDrive *
brasero_medium_selection_get_active_drive (BraseroMediumSelection *selection);

gboolean
brasero_medium_selection_set_active (BraseroMediumSelection *selection,
				     BraseroMedium *medium);

void
brasero_medium_selection_foreach (BraseroMediumSelection *selection,
				  BraseroMediumSelectionFunc function,
				  gpointer callback_data);

void
brasero_medium_selection_show_type (BraseroMediumSelection *selection,
				    BraseroMediaType type);

guint
brasero_medium_selection_get_drive_num (BraseroMediumSelection *selection);

void
brasero_medium_selection_update_media_string (BraseroMediumSelection *selection);

G_END_DECLS

#endif /* _BRASERO_MEDIUM_SELECTION_H_ */
