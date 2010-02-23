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

#ifndef _BRASERO_MEDIUM_SELECTION_H_
#define _BRASERO_MEDIUM_SELECTION_H_

#include <glib-object.h>

#include <gtk/gtk.h>

#include <brasero-medium-monitor.h>
#include <brasero-medium.h>
#include <brasero-drive.h>

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

	/* Signals */
	void		(* medium_changed)		(BraseroMediumSelection *selection,
							 BraseroMedium *medium);

	/* virtual function */
	gchar *		(*format_medium_string)		(BraseroMediumSelection *selection,
							 BraseroMedium *medium);
};

/**
 * BraseroMediumSelection:
 *
 * Rename to: MediumSelection
 */

struct _BraseroMediumSelection
{
	GtkComboBox parent_instance;
};

G_MODULE_EXPORT GType brasero_medium_selection_get_type (void) G_GNUC_CONST;

GtkWidget* brasero_medium_selection_new (void);

BraseroMedium *
brasero_medium_selection_get_active (BraseroMediumSelection *selector);

gboolean
brasero_medium_selection_set_active (BraseroMediumSelection *selector,
				     BraseroMedium *medium);

void
brasero_medium_selection_show_media_type (BraseroMediumSelection *selector,
					  BraseroMediaType type);

G_END_DECLS

#endif /* _BRASERO_MEDIUM_SELECTION_H_ */
