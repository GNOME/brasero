/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Brasero
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
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

#ifndef _BRASERO_APP_H_
#define _BRASERO_APP_H_

#include <glib-object.h>
#include <gtk/gtk.h>

#include "brasero-session-cfg.h"

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

BraseroApp *
brasero_app_new (GApplication *gapp);

BraseroApp *
brasero_app_get_default (void);

void
brasero_app_set_parent (BraseroApp *app,
			guint xid);

void
brasero_app_set_toplevel (BraseroApp *app, GtkWindow *window);

void
brasero_app_create_mainwin (BraseroApp *app);

gboolean
brasero_app_run_mainwin (BraseroApp *app);

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

gboolean
brasero_app_burn (BraseroApp *app,
		  BraseroBurnSession *session,
		  gboolean multi);

void
brasero_app_burn_uri (BraseroApp *app,
		      BraseroDrive *burner,
		      gboolean burn);

void
brasero_app_data (BraseroApp *app,
		  BraseroDrive *burner,
		  gchar * const *uris,
		  gboolean burn);

void
brasero_app_stream (BraseroApp *app,
		    BraseroDrive *burner,
		    gchar * const *uris,
		    gboolean is_video,
		    gboolean burn);

void
brasero_app_image (BraseroApp *app,
		   BraseroDrive *burner,
		   const gchar *uri,
		   gboolean burn);

void
brasero_app_copy_disc (BraseroApp *app,
		       BraseroDrive *burner,
		       const gchar *device,
		       const gchar *cover,
		       gboolean burn);

void
brasero_app_blank (BraseroApp *app,
		   BraseroDrive *burner,
		   gboolean burn);

void
brasero_app_check (BraseroApp *app,
		   BraseroDrive *burner,
		   gboolean burn);

gboolean
brasero_app_open_project (BraseroApp *app,
			  BraseroDrive *burner,
                          const gchar *uri,
                          gboolean is_playlist,
                          gboolean warn_user,
                          gboolean burn);

gboolean
brasero_app_open_uri (BraseroApp *app,
                      const gchar *uri_arg,
                      gboolean warn_user);

gboolean
brasero_app_open_uri_drive_detection (BraseroApp *app,
                                      BraseroDrive *burner,
                                      const gchar *uri,
                                      const gchar *cover_project,
                                      gboolean burn);
GtkWidget *
brasero_app_get_statusbar1 (BraseroApp *app);

GtkWidget *
brasero_app_get_statusbar2 (BraseroApp *app);

GtkWidget *
brasero_app_get_project_manager (BraseroApp *app);

/**
 * Session management
 */

#define BRASERO_SESSION_TMP_PROJECT_PATH	"brasero-tmp-project"

const gchar *
brasero_app_get_saved_contents (BraseroApp *app);

gboolean
brasero_app_save_contents (BraseroApp *app,
			   gboolean cancellable);

G_END_DECLS

#endif /* _BRASERO_APP_H_ */
