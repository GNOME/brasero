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

#ifndef _BRASERO_PROJECT_NAME_H_
#define _BRASERO_PROJECT_NAME_H_

#include <glib-object.h>

#include <gtk/gtkentry.h>

#include "brasero-project-type-chooser.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_PROJECT_NAME             (brasero_project_name_get_type ())
#define BRASERO_PROJECT_NAME(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_PROJECT_NAME, BraseroProjectName))
#define BRASERO_PROJECT_NAME_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_PROJECT_NAME, BraseroProjectNameClass))
#define BRASERO_IS_PROJECT_NAME(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_PROJECT_NAME))
#define BRASERO_IS_PROJECT_NAME_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_PROJECT_NAME))
#define BRASERO_PROJECT_NAME_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_PROJECT_NAME, BraseroProjectNameClass))

typedef struct _BraseroProjectNameClass BraseroProjectNameClass;
typedef struct _BraseroProjectName BraseroProjectName;

struct _BraseroProjectNameClass
{
	GtkEntryClass parent_class;
};

struct _BraseroProjectName
{
	GtkEntry parent_instance;
};

GType brasero_project_name_get_type (void) G_GNUC_CONST;

GtkWidget *
brasero_project_name_new (void);

void
brasero_project_name_set_type (BraseroProjectName *self,
			       BraseroProjectType type);
void
brasero_project_name_set_multisession_medium (BraseroProjectName *self,
					      BraseroMedium *medium);

G_END_DECLS

#endif /* _BRASERO_PROJECT_NAME_H_ */
