/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Brasero
 * Copyright (C) Philippe Rouquier 2008 <bonfire-app@wanadoo.fr>
 * 
 * Brasero is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Brasero is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _BRASERO_APP_H_
#define _BRASERO_APP_H_

#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_APP             (brasero_app_get_type ())
#define BRASERO_APP(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_APP, BraseroApp))
#define BRASERO_APP_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_APP, BraseroAppClass))
#define BRASERO_IS_APP(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_APP))
#define BRASERO_IS_APP_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_APP))
#define BRASERO_APP_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_APP, BraseroAppClass))

typedef struct _BraseroAppClass BraseroAppClass;
typedef struct _BraseroApp BraseroApp;

struct _BraseroAppClass
{
	GtkWindowClass parent_class;
};

struct _BraseroApp
{
	GtkWindow parent_instance;
};

GType brasero_app_get_type (void) G_GNUC_CONST;

GtkWidget *
brasero_app_new (void);

void
brasero_app_run (BraseroApp *app,
		 gboolean load_default_project);

gboolean
brasero_app_is_running (BraseroApp *app);

GtkWidget *
brasero_app_dialog (BraseroApp *app,
		    const gchar *primary_message,
		    GtkButtonsType button_type,
		    GtkMessageType msg_type);

void
brasero_app_alert (BraseroApp *app,
		   const gchar *primary_message,
		   const gchar *secondary_message,
		   GtkMessageType type);

void
brasero_app_blank (BraseroApp *app,
		   const gchar *device,
		   gboolean wait_and_close);

void
brasero_app_check (BraseroApp *app,
		   const gchar *device,
		   gboolean wait_and_close);

void
brasero_app_get_geometry (BraseroApp *app,
			  gint *width,
			  gint *height,
			  gboolean *maximised);

GtkWidget *
brasero_app_get_statusbar1 (BraseroApp *app);

GtkWidget *
brasero_app_get_statusbar2 (BraseroApp *app);

GtkUIManager *
brasero_app_get_ui_manager (BraseroApp *app);

GtkWidget *
brasero_app_get_project_manager (BraseroApp *app);

G_END_DECLS

#endif /* _BRASERO_APP_H_ */
