/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/***************************************************************************
 *            brasero-project-manager.h
 *
 *  mer mai 24 14:22:56 2006
 *  Copyright  2006  Rouquier Philippe
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

#ifndef BRASERO_PROJECT_MANAGER_H
#define BRASERO_PROJECT_MANAGER_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

#include "brasero-medium.h"
#include "brasero-project-parse.h"
#include "brasero-project-type-chooser.h"
#include "brasero-session-cfg.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_PROJECT_MANAGER         (brasero_project_manager_get_type ())
#define BRASERO_PROJECT_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_PROJECT_MANAGER, BraseroProjectManager))
#define BRASERO_PROJECT_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_PROJECT_MANAGER, BraseroProjectManagerClass))
#define BRASERO_IS_PROJECT_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_PROJECT_MANAGER))
#define BRASERO_IS_PROJECT_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_PROJECT_MANAGER))
#define BRASERO_PROJECT_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_PROJECT_MANAGER, BraseroProjectManagerClass))

typedef struct BraseroProjectManagerPrivate BraseroProjectManagerPrivate;

typedef struct {
	GtkNotebook parent;
	BraseroProjectManagerPrivate *priv;
} BraseroProjectManager;

typedef struct {
	GtkNotebookClass parent_class;	
} BraseroProjectManagerClass;

GType brasero_project_manager_get_type (void);
GtkWidget *brasero_project_manager_new (void);

gboolean
brasero_project_manager_open_session (BraseroProjectManager *manager,
                                      BraseroSessionCfg *session);

void
brasero_project_manager_empty (BraseroProjectManager *manager);

/**
 * returns the path of the project that was saved. NULL otherwise.
 */

gboolean
brasero_project_manager_save_session (BraseroProjectManager *manager,
				      const gchar *path,
				      gchar **saved_uri,
				      gboolean cancellable);

void
brasero_project_manager_register_ui (BraseroProjectManager *manager,
				     GtkUIManager *ui_manager);

void
brasero_project_manager_switch (BraseroProjectManager *manager,
				BraseroProjectType type,
				gboolean reset);

G_END_DECLS

#endif /* BRASERO_PROJECT_MANAGER_H */
