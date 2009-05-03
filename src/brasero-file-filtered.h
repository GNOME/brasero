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

#ifndef _BRASERO_FILE_FILTERED_H_
#define _BRASERO_FILE_FILTERED_H_

#include <glib-object.h>
#include <gtk/gtk.h>

#include "brasero-track-data-cfg.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_FILE_FILTERED             (brasero_file_filtered_get_type ())
#define BRASERO_FILE_FILTERED(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_FILE_FILTERED, BraseroFileFiltered))
#define BRASERO_FILE_FILTERED_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_FILE_FILTERED, BraseroFileFilteredClass))
#define BRASERO_IS_FILE_FILTERED(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_FILE_FILTERED))
#define BRASERO_IS_FILE_FILTERED_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_FILE_FILTERED))
#define BRASERO_FILE_FILTERED_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_FILE_FILTERED, BraseroFileFilteredClass))

typedef struct _BraseroFileFilteredClass BraseroFileFilteredClass;
typedef struct _BraseroFileFiltered BraseroFileFiltered;

struct _BraseroFileFilteredClass
{
	GtkExpanderClass parent_class;
};

struct _BraseroFileFiltered
{
	GtkExpander parent_instance;
};

GType brasero_file_filtered_get_type (void) G_GNUC_CONST;

GtkWidget*
brasero_file_filtered_new (BraseroTrackDataCfg *track);

void
brasero_file_filtered_set_right_button_group (BraseroFileFiltered *self,
					      GtkSizeGroup *group);

G_END_DECLS

#endif /* _BRASERO_FILE_FILTERED_H_ */
