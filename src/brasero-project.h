/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/***************************************************************************
 *            project.h
 *
 *  mar nov 29 09:32:17 2005
 *  Copyright  2005  Rouquier Philippe
 *  brasero-app@wanadoo.fr
 ***************************************************************************/

/*
 *  Brasero is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Brasero is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifndef PROJECT_H
#define PROJECT_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

#include "brasero-session-cfg.h"

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
	GtkBox parent;
	BraseroProjectPrivate *priv;
} BraseroProject;

typedef struct {
	GtkBoxClass parent_class;

	void	(*add_pressed)	(BraseroProject *project);
} BraseroProjectClass;

GType brasero_project_get_type (void);
GtkWidget *brasero_project_new (void);

BraseroBurnResult
brasero_project_confirm_switch (BraseroProject *project,
				gboolean keep_files);

void
brasero_project_set_audio (BraseroProject *project);
void
brasero_project_set_data (BraseroProject *project);
void
brasero_project_set_video (BraseroProject *project);
void
brasero_project_set_none (BraseroProject *project);

void
brasero_project_set_source (BraseroProject *project,
			    BraseroURIContainer *source);

BraseroProjectType
brasero_project_convert_to_data (BraseroProject *project);

BraseroProjectType
brasero_project_convert_to_stream (BraseroProject *project,
				   gboolean is_video);

BraseroProjectType
brasero_project_open_session (BraseroProject *project,
			      BraseroSessionCfg *session);

gboolean
brasero_project_save_project (BraseroProject *project);
gboolean
brasero_project_save_project_as (BraseroProject *project);

gboolean
brasero_project_save_session (BraseroProject *project,
			      const gchar *uri,
			      gchar **saved_uri,
			      gboolean show_cancel);

void
brasero_project_register_ui (BraseroProject *project,
			     GtkUIManager *manager);

void
brasero_project_create_audio_cover (BraseroProject *project);

G_END_DECLS

#endif /* PROJECT_H */
