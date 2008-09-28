/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2008 <bonfire-app@wanadoo.fr>
 * 
 * brasero is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * brasero is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _BRASERO_MEDIUM_PROPERTIES_H_
#define _BRASERO_MEDIUM_PROPERTIES_H_

#include <glib-object.h>

#include <gtk/gtk.h>

#include "burn-session.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_MEDIUM_PROPERTIES             (brasero_medium_properties_get_type ())
#define BRASERO_MEDIUM_PROPERTIES(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_MEDIUM_PROPERTIES, BraseroMediumProperties))
#define BRASERO_MEDIUM_PROPERTIES_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_MEDIUM_PROPERTIES, BraseroMediumPropertiesClass))
#define BRASERO_IS_MEDIUM_PROPERTIES(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_MEDIUM_PROPERTIES))
#define BRASERO_IS_MEDIUM_PROPERTIES_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_MEDIUM_PROPERTIES))
#define BRASERO_MEDIUM_PROPERTIES_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_MEDIUM_PROPERTIES, BraseroMediumPropertiesClass))

typedef struct _BraseroMediumPropertiesClass BraseroMediumPropertiesClass;
typedef struct _BraseroMediumProperties BraseroMediumProperties;

struct _BraseroMediumPropertiesClass
{
	GtkButtonClass parent_class;
};

struct _BraseroMediumProperties
{
	GtkButton parent_instance;
};

GType brasero_medium_properties_get_type (void) G_GNUC_CONST;

GtkWidget *
brasero_medium_properties_new (BraseroBurnSession *session);

G_END_DECLS

#endif /* _BRASERO_MEDIUM_PROPERTIES_H_ */
