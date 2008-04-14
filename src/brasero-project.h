/***************************************************************************
 *            project.h
 *
 *  mar nov 29 09:32:17 2005
 *  Copyright  2005  Rouquier Philippe
 *  brasero-app@wanadoo.fr
 ***************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef PROJECT_H
#define PROJECT_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtkwidget.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkuimanager.h>

#include "brasero-disc.h"
#include "brasero-uri-container.h"
#include "brasero-project-type-chooser.h"
#include "brasero-jacket-edit.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_PROJECT         (brasero_project_get_type ())
#define BRASERO_PROJECT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_PROJECT, BraseroProject))
#define BRASERO_PROJECT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_PROJECT, BraseroProjectClass))
#define BRASERO_IS_PROJECT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_PROJECT))
#define BRASERO_IS_PROJECT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_PROJECT))
#define BRASERO_PROJECT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_PROJECT, BraseroProjectClass))

typedef struct BraseroProjectPrivate BraseroProjectPrivate;

typedef struct {
	GtkVBox parent;
	BraseroProjectPrivate *priv;
} BraseroProject;

typedef struct {
	GtkVBoxClass parent_class;

	void	(*add_pressed)	(BraseroProject *project);
} BraseroProjectClass;

GType brasero_project_get_type ();
GtkWidget *brasero_project_new ();

gboolean
brasero_project_confirm_switch (BraseroProject *project);

void
brasero_project_set_audio (BraseroProject *project, GSList *uris);
void
brasero_project_set_data (BraseroProject *project, GSList *uris);
void
brasero_project_set_none (BraseroProject *project);

void
brasero_project_set_source (BraseroProject *project,
			    BraseroURIContainer *source);

BraseroProjectType
brasero_project_open_project (BraseroProject *project, const gchar *uri);
gboolean
brasero_project_save_project (BraseroProject *project);
gboolean
brasero_project_save_project_as (BraseroProject *project);

BraseroProjectType
brasero_project_load_session (BraseroProject *project, const gchar *uri);
gboolean
brasero_project_save_session (BraseroProject *project,
			      const gchar *uri,
			      gboolean show_cancel);

void
brasero_project_register_ui (BraseroProject *project,
			     GtkUIManager *manager);

void
brasero_project_set_cover_specifics (BraseroProject *project,
				     BraseroJacketEdit *cover);

#endif /* PROJECT_H */
