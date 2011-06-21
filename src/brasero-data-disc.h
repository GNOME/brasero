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

#ifndef _BRASERO_DATA_DISC_H_
#define _BRASERO_DATA_DISC_H_

#include <glib-object.h>

#include <gtk/gtk.h>

#include "brasero-medium.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_DATA_DISC             (brasero_data_disc_get_type ())
#define BRASERO_DATA_DISC(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_DATA_DISC, BraseroDataDisc))
#define BRASERO_DATA_DISC_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_DATA_DISC, BraseroDataDiscClass))
#define BRASERO_IS_DATA_DISC(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_DATA_DISC))
#define BRASERO_IS_DATA_DISC_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_DATA_DISC))
#define BRASERO_DATA_DISC_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_DATA_DISC, BraseroDataDiscClass))

typedef struct _BraseroDataDiscClass BraseroDataDiscClass;
typedef struct _BraseroDataDisc BraseroDataDisc;

struct _BraseroDataDiscClass
{
	GtkBoxClass parent_class;
};

struct _BraseroDataDisc
{
	GtkBox parent_instance;
};

GType brasero_data_disc_get_type (void) G_GNUC_CONST;

GtkWidget *
brasero_data_disc_new (void);

void
brasero_data_disc_set_right_button_group (BraseroDataDisc *disc,
					  GtkSizeGroup *size_group);

BraseroMedium *
brasero_data_disc_get_loaded_medium (BraseroDataDisc *disc);

G_END_DECLS

#endif /* _BRASERO_DATA_DISC_H_ */
