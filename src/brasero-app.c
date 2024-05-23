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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "brasero-misc.h"
#include "brasero-io.h"

#include "brasero-app.h"
#include "brasero-setting.h"
#include "brasero-blank-dialog.h"
#include "brasero-sum-dialog.h"
#include "brasero-eject-dialog.h"
#include "brasero-project-manager.h"
#include "brasero-pref.h"

#include "brasero-drive.h"
#include "brasero-medium.h"
#include "brasero-volume.h"

#include "brasero-tags.h"
#include "brasero-burn.h"
#include "brasero-track-disc.h"
#include "brasero-track-image.h"
#include "brasero-track-data-cfg.h"
#include "brasero-track-stream-cfg.h"
#include "brasero-track-image-cfg.h"
#include "brasero-session.h"
#include "brasero-burn-lib.h"

#include "brasero-status-dialog.h"
#include "brasero-burn-options.h"
#include "brasero-burn-dialog.h"
#include "brasero-jacket-edit.h"

#include "burn-plugin-manager.h"
#include "brasero-drive-settings.h"

typedef struct _BraseroAppPrivate BraseroAppPrivate;
struct _BraseroAppPrivate
{
	GApplication *gapp;

	BraseroSetting *setting;

	GdkWindow *parent;

	GtkWidget *mainwin;

	GtkWidget *burn_dialog;
	GtkWidget *tool_dialog;

	/* This is the toplevel window currently displayed */
	GtkWidget *toplevel;

	GtkWidget *projects;
	GtkWidget *contents;
	GtkWidget *statusbar1;
	GtkWidget *statusbar2;
	GtkUIManager *manager;

	guint tooltip_ctx;

	gchar *saved_contents;

	guint is_maximized:1;
	guint mainwin_running:1;
};

#define BRASERO_APP_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_APP, BraseroAppPrivate))


G_DEFINE_TYPE (BraseroApp, brasero_app, G_TYPE_OBJECT);

enum {
	PROP_NONE,
	PROP_GAPP
};

/**
 * Menus and toolbar
 */

static void on_prefs_cb (GtkAction *action, BraseroApp *app);
static void on_eject_cb (GtkAction *action, BraseroApp *app);
static void on_erase_cb (GtkAction *action, BraseroApp *app);
static void on_integrity_check_cb (GtkAction *action, BraseroApp *app);

static void on_exit_cb (GtkAction *action, BraseroApp *app);

static void on_about_cb (GtkAction *action, BraseroApp *app);
static void on_help_cb (GtkAction *action, BraseroApp *app);

static GtkActionEntry entries[] = {
	{"ProjectMenu", NULL, N_("_Project")},
	{"ViewMenu", NULL, N_("_View")},
	{"EditMenu", NULL, N_("_Edit")},
	{"ToolMenu", NULL, N_("_Tools")},

	{"HelpMenu", NULL, N_("_Help")},

	{"Plugins", NULL, N_("P_lugins"), NULL,
	 N_("Choose plugins for Brasero"), G_CALLBACK (on_prefs_cb)},

	{"Eject", "media-eject", N_("E_ject"), NULL,
	 N_("Eject a disc"), G_CALLBACK (on_eject_cb)},

	{"Blank", "media-optical-blank", N_("_Blank…"), NULL,
	 N_("Blank a disc"), G_CALLBACK (on_erase_cb)},

	{"Check", NULL, N_("_Check Integrity…"), NULL,
	 N_("Check data integrity of disc"), G_CALLBACK (on_integrity_check_cb)},

	{"Quit", GTK_STOCK_QUIT, NULL, NULL,
	 N_("Quit Brasero"), G_CALLBACK (on_exit_cb)},

	{"Contents", GTK_STOCK_HELP, N_("_Contents"), "F1", N_("Display help"),
	 G_CALLBACK (on_help_cb)}, 

	{"About", GTK_STOCK_ABOUT, NULL, NULL, N_("About"),
	 G_CALLBACK (on_about_cb)},
};


static const gchar *description = {
	"<ui>"
	    "<menubar name='menubar' >"
	    "<menu action='ProjectMenu'>"
		"<placeholder name='ProjectPlaceholder'/>"
		"<separator/>"
		"<menuitem action='Quit'/>"
	    "</menu>"
	    "<menu action='EditMenu'>"
		"<placeholder name='EditPlaceholder'/>"
		"<separator/>"
		"<menuitem action='Plugins'/>"
	    "</menu>"
	    "<menu action='ViewMenu'>"
		"<placeholder name='ViewPlaceholder'/>"
	    "</menu>"
	    "<menu action='ToolMenu'>"
		"<placeholder name='DiscPlaceholder'/>"
		"<menuitem action='Eject'/>"
		"<menuitem action='Blank'/>"
		"<menuitem action='Check'/>"
	    "</menu>"
	    "<menu action='HelpMenu'>"
		"<menuitem action='Contents'/>"
		"<separator/>"
		"<menuitem action='About'/>"
	    "</menu>"
	    "</menubar>"
	"</ui>"
};

static gchar *
brasero_app_get_path (const gchar *name)
{
	gchar *directory;
	gchar *retval;

	directory = g_build_filename (g_get_user_config_dir (),
				      "brasero",
				      NULL);
	if (!g_file_test (directory, G_FILE_TEST_EXISTS))
		g_mkdir_with_parents (directory, S_IRWXU);

	retval = g_build_filename (directory, name, NULL);
	g_free (directory);
	return retval;
}

static gboolean
brasero_app_load_window_state (BraseroApp *app)
{
	gint width;
	gint height;
	gint state = 0;
	gpointer value;

	GdkScreen *screen;
	GdkRectangle rect;
	gint monitor;

	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);

	/* Make sure that on first run the window has a default size of at least
	 * 85% of the screen (hardware not GTK+) */
	screen = gtk_window_get_screen (GTK_WINDOW (priv->mainwin));
	monitor = gdk_screen_get_monitor_at_window (screen, gtk_widget_get_window (GTK_WIDGET (priv->mainwin)));
	gdk_screen_get_monitor_geometry (screen, monitor, &rect);

	brasero_setting_get_value (brasero_setting_get_default (),
	                           BRASERO_SETTING_WIN_WIDTH,
	                           &value);
	width = GPOINTER_TO_INT (value);

	brasero_setting_get_value (brasero_setting_get_default (),
	                           BRASERO_SETTING_WIN_HEIGHT,
	                           &value);
	height = GPOINTER_TO_INT (value);

	brasero_setting_get_value (brasero_setting_get_default (),
	                           BRASERO_SETTING_WIN_MAXIMIZED,
	                           &value);
	state = GPOINTER_TO_INT (value);

	if (width > 0 && height > 0)
		gtk_window_resize (GTK_WINDOW (priv->mainwin),
				   width,
				   height);
	else
		gtk_window_resize (GTK_WINDOW (priv->mainwin),
		                   rect.width / 100 *85,
		                   rect.height / 100 * 85);

	if (state)
		gtk_window_maximize (GTK_WINDOW (priv->mainwin));

	return TRUE;
}

/**
 * returns FALSE when nothing prevents the shutdown
 * returns TRUE when shutdown should be delayed
 */

gboolean
brasero_app_save_contents (BraseroApp *app,
			   gboolean cancellable)
{
	gboolean cancel;
	gchar *project_path;
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);

	if (priv->burn_dialog) {
		if (cancellable)
			return (brasero_burn_dialog_cancel (BRASERO_BURN_DIALOG (priv->burn_dialog), FALSE) == FALSE);

		gtk_widget_destroy (priv->burn_dialog);
		return FALSE;
	}

	if (priv->tool_dialog) {
		if (cancellable) {
			if (BRASERO_IS_TOOL_DIALOG (priv->tool_dialog))
				return (brasero_tool_dialog_cancel (BRASERO_TOOL_DIALOG (priv->tool_dialog)) == FALSE);
			else if (BRASERO_IS_EJECT_DIALOG (priv->tool_dialog))
				return (brasero_eject_dialog_cancel (BRASERO_EJECT_DIALOG (priv->tool_dialog)) == FALSE);
		}

		gtk_widget_destroy (priv->tool_dialog);
		return FALSE;
	}

	/* If we are not having a main window there is no point in going further */
	if (!priv->mainwin)
		return FALSE;

	if (priv->saved_contents) {
		g_free (priv->saved_contents);
		priv->saved_contents = NULL;
	}

	project_path = brasero_app_get_path (BRASERO_SESSION_TMP_PROJECT_PATH);
	cancel = brasero_project_manager_save_session (BRASERO_PROJECT_MANAGER (priv->projects),
						       project_path,
						       &priv->saved_contents,
						       cancellable);
	g_free (project_path);

	return cancel;
}

const gchar *
brasero_app_get_saved_contents (BraseroApp *app)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);
	return priv->saved_contents;
}

/**
 * These functions are only useful because they set the proper toplevel parent
 * for the message dialog. The following one also sets some properties in case
 * there isn't any toplevel parent (like show in taskbar, ...).
 **/

static void
brasero_app_toplevel_destroyed_cb (GtkWidget *object,
				   BraseroApp *app)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);
	priv->toplevel = NULL;
}

GtkWidget *
brasero_app_dialog (BraseroApp *app,
		    const gchar *primary_message,
		    GtkButtonsType button_type,
		    GtkMessageType msg_type)
{
	gboolean is_on_top = FALSE;
	BraseroAppPrivate *priv;
	GtkWindow *toplevel;
	GtkWidget *dialog;

	priv = BRASERO_APP_PRIVATE (app);

	if (priv->mainwin) {
		toplevel = GTK_WINDOW (priv->mainwin);
		gtk_widget_show (priv->mainwin);
	}
	else if (!priv->toplevel) {
		is_on_top = TRUE;
		toplevel = NULL;
	}
	else
		toplevel = GTK_WINDOW (priv->toplevel);

	dialog = gtk_message_dialog_new (toplevel,
					 GTK_DIALOG_DESTROY_WITH_PARENT|
					 GTK_DIALOG_MODAL,
					 msg_type,
					 button_type,
					 "%s",
					 primary_message);

	if (!toplevel && priv->parent) {
		gtk_widget_realize (GTK_WIDGET (dialog));
		gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
		gdk_window_set_transient_for (gtk_widget_get_window (GTK_WIDGET (dialog)), priv->parent);
	}

	if (is_on_top) {
		gtk_window_set_skip_pager_hint (GTK_WINDOW (dialog), FALSE);
		gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), FALSE);

		priv->toplevel = dialog;
		g_signal_connect (dialog,
				  "destroy",
				  G_CALLBACK (brasero_app_toplevel_destroyed_cb),
				  app);
	}

	return dialog;
}

void
brasero_app_alert (BraseroApp *app,
		   const gchar *primary_message,
		   const gchar *secondary_message,
		   GtkMessageType type)
{
	GtkWidget *parent = NULL;
	gboolean is_on_top= TRUE;
	BraseroAppPrivate *priv;
	GtkWidget *alert;

	priv = BRASERO_APP_PRIVATE (app);

	/* Whatever happens, they need a parent or must be in the taskbar */
	if (priv->mainwin) {
		parent = GTK_WIDGET (priv->mainwin);
		is_on_top = FALSE;
	}
	else if (priv->toplevel) {
		parent = priv->toplevel;
		is_on_top = FALSE;
	}

	alert = brasero_utils_create_message_dialog (parent,
						     primary_message,
						     secondary_message,
						     type);
	if (!parent && priv->parent) {
		is_on_top = FALSE;

		gtk_widget_realize (GTK_WIDGET (alert));
		gtk_window_set_modal (GTK_WINDOW (alert), TRUE);
		gdk_window_set_transient_for (gtk_widget_get_window (GTK_WIDGET (alert)), priv->parent);
	}

	if (is_on_top) {
		gtk_window_set_title (GTK_WINDOW (alert), _("Disc Burner"));
		gtk_window_set_skip_pager_hint (GTK_WINDOW (alert), FALSE);
		gtk_window_set_skip_taskbar_hint (GTK_WINDOW (alert), FALSE);
	}

	gtk_dialog_run (GTK_DIALOG (alert));
	gtk_widget_destroy (alert);
}

GtkWidget *
brasero_app_get_statusbar1 (BraseroApp *app)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);

	/* FIXME: change with future changes */
	return priv->statusbar1;
}

GtkWidget *
brasero_app_get_statusbar2 (BraseroApp *app)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);
	return priv->statusbar2;
}

GtkWidget *
brasero_app_get_project_manager (BraseroApp *app)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);
	return priv->projects;
}

static gboolean
on_destroy_cb (GtkWidget *window, BraseroApp *app)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);
	if (priv->mainwin)
		gtk_main_quit ();

	return FALSE;
}

static gboolean
on_delete_cb (GtkWidget *window, GdkEvent *event, BraseroApp *app)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);
	if (!priv->mainwin)
		return FALSE;

	if (brasero_app_save_contents (app, TRUE))
		return TRUE;

	return FALSE;
}

static void
on_exit_cb (GtkAction *action, BraseroApp *app)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);

	if (brasero_app_save_contents (app, TRUE))
		return;

	if (priv->mainwin)
		gtk_widget_destroy (GTK_WIDGET (priv->mainwin));
}

gboolean
brasero_app_is_running (BraseroApp *app)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);
	return priv->mainwin_running;
}

void
brasero_app_set_parent (BraseroApp *app,
			guint parent_xid)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);
#ifdef GDK_WINDOWING_X11
	if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
		priv->parent = gdk_x11_window_foreign_new_for_display (gdk_display_get_default (), parent_xid);
#endif
}

gboolean
brasero_app_burn (BraseroApp *app,
		  BraseroBurnSession *session,
		  gboolean multi)
{
	gboolean success;
	GtkWidget *dialog;
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);

	/* now setup the burn dialog */
	dialog = brasero_burn_dialog_new ();
	gtk_window_set_icon_name (GTK_WINDOW (dialog), "brasero");

	priv->burn_dialog = dialog;

	brasero_app_set_toplevel (app, GTK_WINDOW (dialog));
	if (!multi)
		success = brasero_burn_dialog_run (BRASERO_BURN_DIALOG (dialog),
						   BRASERO_BURN_SESSION (session));
	else
		success = brasero_burn_dialog_run_multi (BRASERO_BURN_DIALOG (dialog),
							 BRASERO_BURN_SESSION (session));
	priv->burn_dialog = NULL;

	/* The destruction of the dialog will bring the main window forward */
	gtk_widget_destroy (dialog);
	return success;
}

static BraseroBurnResult
brasero_app_burn_options (BraseroApp *app,
			  BraseroSessionCfg *session)
{
	GtkWidget *dialog;
	GtkResponseType answer;

	dialog = brasero_burn_options_new (session);
	brasero_app_set_toplevel (app, GTK_WINDOW (dialog));
	gtk_window_set_icon_name (GTK_WINDOW (dialog), "brasero");

	answer = gtk_dialog_run (GTK_DIALOG (dialog));

	/* The destruction of the dialog will bring the main window forward */
	gtk_widget_destroy (dialog);
	if (answer == GTK_RESPONSE_OK)
		return BRASERO_BURN_OK;

	if (answer == GTK_RESPONSE_ACCEPT)
		return BRASERO_BURN_RETRY;

	return BRASERO_BURN_CANCEL;

}

static void
brasero_app_session_burn (BraseroApp *app,
			  BraseroSessionCfg *session,
			  gboolean burn)
{
	BraseroDriveSettings *settings;

	/* Set saved temporary directory for the session.
	 * NOTE: BraseroBurnSession can cope with NULL path */
	settings = brasero_drive_settings_new ();
	brasero_drive_settings_set_session (settings, BRASERO_BURN_SESSION (session));

	/* We need to have a drive to start burning immediately */
	if (burn && brasero_burn_session_get_burner (BRASERO_BURN_SESSION (session))) {
		BraseroStatus *status;
		BraseroBurnResult result;

		status = brasero_status_new ();
		brasero_burn_session_get_status (BRASERO_BURN_SESSION (session), status);

		result = brasero_status_get_result (status);
		if (result == BRASERO_BURN_NOT_READY || result == BRASERO_BURN_RUNNING) {
			GtkWidget *status_dialog;

			status_dialog = brasero_status_dialog_new (BRASERO_BURN_SESSION (session), NULL);
			gtk_dialog_run (GTK_DIALOG (status_dialog));
			gtk_widget_destroy (status_dialog);

			brasero_burn_session_get_status (BRASERO_BURN_SESSION (session), status);
			result = brasero_status_get_result (status);
		}
		g_object_unref (status);

		if (result == BRASERO_BURN_CANCEL) {
			g_object_unref (settings);
			return;
		}

		if (result != BRASERO_BURN_OK) {
			GError *error;

			error = brasero_status_get_error (status);
			brasero_app_alert (app,
					   _("Error while burning."),
					   error? error->message:"",
					   GTK_MESSAGE_ERROR);
			if (error)
				g_error_free (error);

			g_object_unref (settings);
			return;
		}

		result = brasero_app_burn (app,
		                           BRASERO_BURN_SESSION (session),
		                           FALSE);
	}
	else {
		BraseroBurnResult result;

		result = brasero_app_burn_options (app, session);
		if (result == BRASERO_BURN_OK || result == BRASERO_BURN_RETRY) {
			result = brasero_app_burn (app,
			                           BRASERO_BURN_SESSION (session),
			                           (result == BRASERO_BURN_RETRY));
		}
	}

	g_object_unref (settings);
}

void
brasero_app_copy_disc (BraseroApp *app,
		       BraseroDrive *burner,
		       const gchar *device,
		       const gchar *cover,
		       gboolean burn)
{
	BraseroTrackDisc *track = NULL;
	BraseroSessionCfg *session;
	BraseroDrive *drive = NULL;

	session = brasero_session_cfg_new ();
	track = brasero_track_disc_new ();
	brasero_burn_session_add_track (BRASERO_BURN_SESSION (session),
					BRASERO_TRACK (track),
					NULL);
	g_object_unref (track);

	/* if a device is specified then get the corresponding medium */
	if (device) {
		BraseroMediumMonitor *monitor;

		monitor = brasero_medium_monitor_get_default ();
		drive = brasero_medium_monitor_get_drive (monitor, device);
		g_object_unref (monitor);

		if (!drive)
			return;

		brasero_track_disc_set_drive (BRASERO_TRACK_DISC (track), drive);
		g_object_unref (drive);
	}

	/* Set a cover if any. */
	if (cover) {
		GValue *value;

		value = g_new0 (GValue, 1);
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (value, cover);
		brasero_burn_session_tag_add (BRASERO_BURN_SESSION (session),
					      BRASERO_COVER_URI,
					      value);
	}

	brasero_burn_session_set_burner (BRASERO_BURN_SESSION (session), burner);
	brasero_app_session_burn (app, session, burn);
	g_object_unref (session);
}

void
brasero_app_image (BraseroApp *app,
		   BraseroDrive *burner,
		   const gchar *uri_arg,
		   gboolean burn)
{
	BraseroSessionCfg *session;
	BraseroTrackImageCfg *track = NULL;

	/* setup, show, and run options dialog */
	session = brasero_session_cfg_new ();
	track = brasero_track_image_cfg_new ();
	brasero_burn_session_add_track (BRASERO_BURN_SESSION (session),
					BRASERO_TRACK (track),
					NULL);
	g_object_unref (track);

	if (uri_arg) {
		GFile *file;
		gchar *uri;

		file = g_file_new_for_commandline_arg (uri_arg);
		uri = g_file_get_uri (file);
		g_object_unref (file);

		brasero_track_image_cfg_set_source (track, uri);
		g_free (uri);
	}

	brasero_burn_session_set_burner (BRASERO_BURN_SESSION (session), burner);
	brasero_app_session_burn (app, session, burn);
	g_object_unref (session);
}

static void
brasero_app_process_session (BraseroApp *app,
			     BraseroSessionCfg *session,
			     gboolean burn)
{
	if (!burn) {
		GtkWidget *manager;
		BraseroAppPrivate *priv;

		priv = BRASERO_APP_PRIVATE (app);
		if (!priv->mainwin)
			brasero_app_create_mainwin (app);

		manager = brasero_app_get_project_manager (app);
		brasero_project_manager_open_session (BRASERO_PROJECT_MANAGER (manager), session);
	}
	else
		brasero_app_session_burn (app, session, TRUE);
}

void
brasero_app_burn_uri (BraseroApp *app,
		      BraseroDrive *burner,
		      gboolean burn)
{
	GFileEnumerator *enumerator;
	BraseroSessionCfg *session;
	BraseroTrackDataCfg *track;
	GFileInfo *info = NULL;
	GError *error = NULL;
	GFile *file;

	/* Here we get the contents from the burn:// URI and add them
	 * individually to the data project. This is done in case it is
	 * empty no to start the "Getting Project Size" dialog and then
	 * show the "Project is empty" dialog. Do this synchronously as:
	 * - we only want the top nodes which reduces time needed
	 * - it's always local
	 * - windows haven't been shown yet
	 * NOTE: don't use any file specified on the command line. */
	file = g_file_new_for_uri ("burn://");
	enumerator = g_file_enumerate_children (file,
						G_FILE_ATTRIBUTE_STANDARD_NAME,
						G_FILE_QUERY_INFO_NONE,
						NULL,
						&error);
	if (!enumerator) {
		gchar *string;

		if (error) {
			string = g_strdup (error->message);
			g_error_free (error);
		}
		else
			string = g_strdup (_("An internal error occurred"));

		brasero_app_alert (app,
				   _("Error while loading the project"),
				   string,
				   GTK_MESSAGE_ERROR);

		g_free (string);
		g_object_unref (file);
		return;
	}

	session = brasero_session_cfg_new ();

	track = brasero_track_data_cfg_new ();
	brasero_burn_session_add_track (BRASERO_BURN_SESSION (session), BRASERO_TRACK (track), NULL);
	g_object_unref (track);

	while ((info = g_file_enumerator_next_file (enumerator, NULL, &error)) != NULL) {
		gchar *uri;

		uri = g_strconcat ("burn:///", g_file_info_get_name (info), NULL);
		g_object_unref (info);

		brasero_track_data_cfg_add (track, uri, NULL);
		g_free (uri);
	}

	g_object_unref (enumerator);
	g_object_unref (file);

	if (error) {
		g_object_unref (session);

		/* NOTE: this check errors in g_file_enumerator_next_file () */
		brasero_app_alert (app,
				   _("Error while loading the project"),
				   error->message,
				   GTK_MESSAGE_ERROR);
		return;
	}

	if (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (track), NULL) == 0) {
	        g_object_unref (session);
		brasero_app_alert (app,
				   _("Please add files to the project."),
				   _("The project is empty"),
				   GTK_MESSAGE_ERROR);
		return;
	}

	brasero_burn_session_set_burner (BRASERO_BURN_SESSION (session), burner);
	brasero_app_process_session (app, session, burn);
	g_object_unref (session);
}

void
brasero_app_data (BraseroApp *app,
		  BraseroDrive *burner,
		  gchar * const *uris,
		  gboolean burn)
{
	BraseroTrackDataCfg *track;
	BraseroSessionCfg *session;
	BraseroAppPrivate *priv;
	int i, num;

	priv = BRASERO_APP_PRIVATE (app);

	if (!uris) {
		GtkWidget *manager;

		if (burn) {
			brasero_app_alert (app,
					   _("Please add files to the project."),
					   _("The project is empty"),
					   GTK_MESSAGE_ERROR);
			return;
		}

		if (!priv->mainwin)
			brasero_app_create_mainwin (app);

		manager = brasero_app_get_project_manager (app);
		brasero_project_manager_switch (BRASERO_PROJECT_MANAGER (manager),
						BRASERO_PROJECT_TYPE_DATA,
						TRUE);
		return;
	}

	session = brasero_session_cfg_new ();
	track = brasero_track_data_cfg_new ();
	brasero_burn_session_add_track (BRASERO_BURN_SESSION (session), BRASERO_TRACK (track), NULL);
	g_object_unref (track);

	num = g_strv_length ((gchar **) uris);
	for (i = 0; i < num; i ++) {
		GFile *file;
		gchar *uri;

		file = g_file_new_for_commandline_arg (uris [i]);
		uri = g_file_get_uri (file);
		g_object_unref (file);

		/* Ignore the return value */
		brasero_track_data_cfg_add (track, uri, NULL);
		g_free (uri);
	}

	brasero_burn_session_set_burner (BRASERO_BURN_SESSION (session), burner);
	brasero_app_process_session (app, session, burn);
	g_object_unref (session);
}

void
brasero_app_stream (BraseroApp *app,
		    BraseroDrive *burner,
		    gchar * const *uris,
		    gboolean is_video,
		    gboolean burn)
{
	BraseroSessionCfg *session;
	BraseroAppPrivate *priv;
	int i, num;

	priv = BRASERO_APP_PRIVATE (app);

	session = brasero_session_cfg_new ();

	if (!uris) {
		GtkWidget *manager;

		if (burn) {
			brasero_app_alert (app,
					   _("Please add files to the project."),
					   _("The project is empty"),
					   GTK_MESSAGE_ERROR);
			return;
		}

		if (!priv->mainwin)
			brasero_app_create_mainwin (app);

		manager = brasero_app_get_project_manager (app);
		brasero_project_manager_switch (BRASERO_PROJECT_MANAGER (manager),
						is_video? BRASERO_PROJECT_TYPE_VIDEO:BRASERO_PROJECT_TYPE_AUDIO,
						TRUE);
		return;
	}

	num = g_strv_length ((gchar **) uris);
	for (i = 0; i < num; i ++) {
		BraseroTrackStreamCfg *track;
		GFile *file;
		gchar *uri;

		file = g_file_new_for_commandline_arg (uris [i]);
		uri = g_file_get_uri (file);
		g_object_unref (file);

		track = brasero_track_stream_cfg_new ();
		brasero_track_stream_set_source (BRASERO_TRACK_STREAM (track), uri);
		g_free (uri);

		if (is_video)
			brasero_track_stream_set_format (BRASERO_TRACK_STREAM (track),
			                                 BRASERO_VIDEO_FORMAT_UNDEFINED);

		brasero_burn_session_add_track (BRASERO_BURN_SESSION (session), BRASERO_TRACK (track), NULL);
		g_object_unref (track);
	}

	brasero_burn_session_set_burner (BRASERO_BURN_SESSION (session), burner);
	brasero_app_process_session (app, session, burn);
	g_object_unref (session);
}

void
brasero_app_blank (BraseroApp *app,
		   BraseroDrive *burner,
		   gboolean burn)
{
	BraseroBlankDialog *dialog;
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);
	dialog = brasero_blank_dialog_new ();
	gtk_window_set_icon_name (GTK_WINDOW (dialog), "brasero");

	if (burner) {
		BraseroMedium *medium;

		medium = brasero_drive_get_medium (burner);
		brasero_tool_dialog_set_medium (BRASERO_TOOL_DIALOG (dialog), medium);
	}

	priv->tool_dialog = GTK_WIDGET (dialog);
	if (!priv->mainwin) {
		gtk_widget_realize (GTK_WIDGET (dialog));

		if (priv->parent) {
			gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
			gdk_window_set_transient_for (gtk_widget_get_window (GTK_WIDGET (dialog)), priv->parent);
		}
	}
	else {
		GtkWidget *toplevel;

		/* FIXME! This is a bad idea and needs fixing */
		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (priv->mainwin));

		gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
		gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
		gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	}

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (GTK_WIDGET (dialog));
	priv->tool_dialog = NULL;
}

static void
on_erase_cb (GtkAction *action, BraseroApp *app)
{
	brasero_app_blank (app, NULL, FALSE);
}

static void
on_eject_cb (GtkAction *action, BraseroApp *app)
{
	GtkWidget *dialog;
	GtkWidget *toplevel;
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);

	dialog = brasero_eject_dialog_new ();
	gtk_window_set_icon_name (GTK_WINDOW (dialog), "brasero");

	/* FIXME! This is a bad idea and needs fixing */
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (priv->mainwin));

	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);

	priv->tool_dialog = dialog;
	gtk_dialog_run (GTK_DIALOG (dialog));
	priv->tool_dialog = NULL;

	gtk_widget_destroy (dialog);
}

void
brasero_app_check (BraseroApp *app,
		   BraseroDrive *burner,
		   gboolean burn)
{
	BraseroSumDialog *dialog;
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);

	dialog = brasero_sum_dialog_new ();
	gtk_window_set_icon_name (GTK_WINDOW (dialog), "brasero");

	priv->tool_dialog = GTK_WIDGET (dialog);

	if (burner) {
		BraseroMedium *medium;

		medium = brasero_drive_get_medium (burner);
		brasero_tool_dialog_set_medium (BRASERO_TOOL_DIALOG (dialog), medium);
	}

	if (!priv->mainwin) {
		gtk_widget_realize (GTK_WIDGET (dialog));

		if (priv->parent) {
			gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
			gdk_window_set_transient_for (gtk_widget_get_window (GTK_WIDGET (dialog)), priv->parent);
		}
	}
	else {
		GtkWidget *toplevel;

		/* FIXME! This is a bad idea and needs fixing */
		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (priv->mainwin));

		gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
		gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
		gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	}

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (GTK_WIDGET (dialog));
	priv->tool_dialog = NULL;
}

static void
on_integrity_check_cb (GtkAction *action, BraseroApp *app)
{
	brasero_app_check (app, NULL, FALSE);
}

static void
brasero_app_current_toplevel_destroyed (GtkWidget *widget,
					BraseroApp *app)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);
	if (priv->mainwin_running)
		gtk_widget_show (GTK_WIDGET (priv->mainwin));
}

void
brasero_app_set_toplevel (BraseroApp *app, GtkWindow *window)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);

	if (!priv->mainwin_running) {
		if (priv->parent) {
			gtk_widget_realize (GTK_WIDGET (window));
			gdk_window_set_transient_for (gtk_widget_get_window (GTK_WIDGET (window)), priv->parent);
		}
		else {
			gtk_window_set_skip_taskbar_hint (GTK_WINDOW (window), FALSE);
			gtk_window_set_skip_pager_hint (GTK_WINDOW (window), FALSE);
			gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
		}
	}
	else {
		/* hide main dialog if it is shown */
		gtk_widget_hide (GTK_WIDGET (priv->mainwin));

		gtk_window_set_skip_taskbar_hint (GTK_WINDOW (window), FALSE);
		gtk_window_set_skip_pager_hint (GTK_WINDOW (window), FALSE);
		gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
	}

	gtk_widget_show (GTK_WIDGET (window));
	g_signal_connect (window,
			  "destroy",
			  G_CALLBACK (brasero_app_current_toplevel_destroyed),
			  app);
}

static void
on_prefs_cb (GtkAction *action, BraseroApp *app)
{
	GtkWidget *dialog;
	GtkWidget *toplevel;
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);

	dialog = brasero_pref_new ();

	/* FIXME! This is a bad idea and needs fixing */
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (priv->mainwin));

	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);

	gtk_widget_show_all (dialog);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static void
on_about_cb (GtkAction *action, BraseroApp *app)
{
	const gchar *authors[] = {
		"Philippe Rouquier <bonfire-app@wanadoo.fr>",
		"Luis Medinas <lmedinas@gmail.com>",
		NULL
	};

	const gchar *documenters[] = {
		"Phil Bull <philbull@gmail.com>",
		"Milo Casagrande <milo_casagrande@yahoo.it>",
		"Andrew Stabeno <stabeno@gmail.com>",
		NULL
	};

	const gchar *license_part[] = {
		N_("Brasero is free software; you can redistribute "
		   "it and/or modify it under the terms of the GNU "
		   "General Public License as published by the Free "
		   "Software Foundation; either version 2 of the "
		   "License, or (at your option) any later version."),
                N_("Brasero is distributed in the hope that it will "
		   "be useful, but WITHOUT ANY WARRANTY; without even "
		   "the implied warranty of MERCHANTABILITY or FITNESS "
		   "FOR A PARTICULAR PURPOSE.  See the GNU General "
		   "Public License for more details."),
                N_("You should have received a copy of the GNU General "
		   "Public License along with Brasero; if not, write "
		   "to the Free Software Foundation, Inc., "
                   "51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA"),
		NULL
        };

	gchar  *license, *comments;
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);

	comments = g_strdup (_("A simple to use CD/DVD burning application for GNOME"));

	license = g_strjoin ("\n\n",
                             _(license_part[0]),
                             _(license_part[1]),
                             _(license_part[2]),
			     NULL);

	/* This can only be shown from the main window so no need for toplevel */
	gtk_show_about_dialog (GTK_WINDOW (GTK_WIDGET (priv->mainwin)),
			       "program-name", "Brasero",
			       "comments", comments,
			       "version", VERSION,
			       "copyright", "Copyright © 2005-2010 Philippe Rouquier",
			       "authors", authors,
			       "documenters", documenters,
			       "website", "http://www.gnome.org/projects/brasero",
			       "website-label", _("Brasero Homepage"),
			       "license", license,
			       "wrap-license", TRUE,
			       "logo-icon-name", "brasero",
			       /* Translators: This is a special message that shouldn't be translated
                                 * literally. It is used in the about box to give credits to
                                 * the translators.
                                 * Thus, you should translate it to your name and email address.
                                 * You should also include other translators who have contributed to
                                 * this translation; in that case, please write each of them on a separate
                                 * line seperated by newlines (\n).
                                 */
                               "translator-credits", _("translator-credits"),
			       NULL);

	g_free (comments);
	g_free (license);
}

static void
on_help_cb (GtkAction *action, BraseroApp *app)
{
	GError *error = NULL;
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);

	gtk_show_uri (NULL, "help:brasero", gtk_get_current_event_time (), &error);
   	if (error) {
		GtkWidget *d;
        
		d = gtk_message_dialog_new (GTK_WINDOW (priv->mainwin),
					    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					    GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
					    "%s", error->message);
		gtk_dialog_run (GTK_DIALOG(d));
		gtk_widget_destroy (d);
		g_error_free (error);
		error = NULL;
	}
}

static gboolean
on_window_state_changed_cb (GtkWidget *widget,
			    GdkEventWindowState *event,
			    BraseroApp *app)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);

	if (event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) {
		priv->is_maximized = 1;
		brasero_setting_set_value (brasero_setting_get_default (),
		                           BRASERO_SETTING_WIN_MAXIMIZED,
		                           GINT_TO_POINTER (1));
	}
	else {
		priv->is_maximized = 0;
		brasero_setting_set_value (brasero_setting_get_default (),
		                           BRASERO_SETTING_WIN_MAXIMIZED,
		                           GINT_TO_POINTER (0));
	}

	return FALSE;
}

static gboolean
on_configure_event_cb (GtkWidget *widget,
		       GdkEventConfigure *event,
		       BraseroApp *app)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);

	if (!priv->is_maximized) {
		brasero_setting_set_value (brasero_setting_get_default (),
		                           BRASERO_SETTING_WIN_WIDTH,
		                           GINT_TO_POINTER (event->width));
		brasero_setting_set_value (brasero_setting_get_default (),
		                           BRASERO_SETTING_WIN_HEIGHT,
		                           GINT_TO_POINTER (event->height));
	}

	return FALSE;
}

gboolean
brasero_app_open_project (BraseroApp *app,
			  BraseroDrive *burner,
                          const gchar *uri,
                          gboolean is_playlist,
                          gboolean warn_user,
                          gboolean burn)
{
	BraseroSessionCfg *session;

	session = brasero_session_cfg_new ();

#ifdef BUILD_PLAYLIST

	if (is_playlist) {
		if (!brasero_project_open_audio_playlist_project (uri,
								  BRASERO_BURN_SESSION (session),
								  warn_user))
			return FALSE;
	}
	else

#endif
	
	if (!brasero_project_open_project_xml (uri,
					       BRASERO_BURN_SESSION (session),
					       warn_user))
		return FALSE;

	brasero_app_process_session (app, session, burn);
	g_object_unref (session);

	return TRUE;
}

static gboolean
brasero_app_open_by_mime (BraseroApp *app,
                          const gchar *uri,
                          const gchar *mime,
                          gboolean warn_user)
{
	if (!mime) {
		/* that can happen when the URI could not be identified */
		return FALSE;
	}

	/* When our files/description of x-brasero mime type is not properly 
	 * installed, it's returned as application/xml, so check that too. */
	if (!strcmp (mime, "application/x-brasero")
	||  !strcmp (mime, "application/xml"))
		return brasero_app_open_project (app,
						 NULL,
						 uri,
						 FALSE,
						 warn_user,
						 FALSE);

#ifdef BUILD_PLAYLIST

	else if (!strcmp (mime, "audio/x-scpls")
	     ||  !strcmp (mime, "audio/x-ms-asx")
	     ||  !strcmp (mime, "audio/x-mp3-playlist")
	     ||  !strcmp (mime, "audio/x-mpegurl"))
		return brasero_app_open_project (app,
						 NULL,
						 uri,
						 TRUE,
						 warn_user,
						 FALSE);

#endif

	else if (!strcmp (mime, "application/x-cd-image")
	     ||  !strcmp (mime, "application/vnd.efi.iso")
	     ||  !strcmp (mime, "application/x-cdrdao-toc")
	     ||  !strcmp (mime, "application/x-toc")
	     ||  !strcmp (mime, "application/x-cue")) {
		brasero_app_image (app, NULL, uri, FALSE);
		return TRUE;
	}

	return FALSE;
}

static gboolean
brasero_app_open_uri_file (BraseroApp *app,
                           GFile *file,
                           GFileInfo *info,
                           gboolean warn_user)
{
	BraseroProjectType type;
	gchar *uri = NULL;

	/* if that's a symlink, redo it on its target to get the real mime type
	 * that usually also depends on the extension of the target:
	 * ex: an iso file with the extension .iso will be seen as octet-stream
	 * if the symlink hasn't got any extention at all */
	while (g_file_info_get_is_symlink (info)) {
		const gchar *target;
		GFileInfo *tmp_info;
		GFile *tmp_file;
		GError *error = NULL;

		target = g_file_info_get_symlink_target (info);
		if (!g_path_is_absolute (target)) {
			gchar *parent;
			gchar *tmp;

			tmp = g_file_get_path (file);
			parent = g_path_get_dirname (tmp);
			g_free (tmp);

			target = g_build_filename (parent, target, NULL);
			g_free (parent);
		}

		tmp_file = g_file_new_for_commandline_arg (target);
		tmp_info = g_file_query_info (tmp_file,
					      G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
					      G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK ","
					      G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET,
					      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
					      NULL,
					      &error);
		if (!tmp_info) {
			g_object_unref (tmp_file);
			break;
		}

		g_object_unref (info);
		g_object_unref (file);

		info = tmp_info;
		file = tmp_file;
	}

	uri = g_file_get_uri (file);
	if (g_file_query_exists (file, NULL)
	&&  g_file_info_get_content_type (info)) {
		const gchar *mime;

		mime = g_file_info_get_content_type (info);
	  	type = brasero_app_open_by_mime (app, uri, mime, warn_user);
        } 
	else if (warn_user) {
		gchar *string;

		string = g_strdup_printf (_("The project \"%s\" does not exist"), uri);
		brasero_app_alert (app,
				   _("Error while loading the project"),
				   string,
				   GTK_MESSAGE_ERROR);
		g_free (string);

		type = BRASERO_PROJECT_TYPE_INVALID;
	}
	else
		type = BRASERO_PROJECT_TYPE_INVALID;

	g_free (uri);
	return (type != BRASERO_PROJECT_TYPE_INVALID);
}

gboolean
brasero_app_open_uri (BraseroApp *app,
                      const gchar *uri_arg,
                      gboolean warn_user)
{
	GFile *file;
	GFileInfo *info;
	gboolean retval;

	/* FIXME: make that asynchronous */
	/* NOTE: don't follow symlink because we want to identify them */
	file = g_file_new_for_commandline_arg (uri_arg);
	if (!file)
		return FALSE;

	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
				  G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK ","
				  G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET,
				  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				  NULL,
				  NULL);

	if (!info) {
		g_object_unref (file);
		return FALSE;
	}

	retval = brasero_app_open_uri_file (app, file, info, warn_user);

	g_object_unref (file);
	g_object_unref (info);

	return retval;
}

gboolean
brasero_app_open_uri_drive_detection (BraseroApp *app,
                                      BraseroDrive *burner,
                                      const gchar *uri_arg,
                                      const gchar *cover_project,
                                      gboolean burn_immediately)
{
	gchar *uri;
	GFile *file;
	GFileInfo *info;
	gboolean retval = FALSE;

	file = g_file_new_for_commandline_arg (uri_arg);
	if (!file)
		return FALSE;

	/* Note: if the path is the path of a mounted volume the uri returned
	 * will be entirely different like if /path/to/somewhere is where
	 * an audio CD is mounted will return cdda://sr0/ */
	uri = g_file_get_uri (file);
	info = g_file_query_info (file,
	                          G_FILE_ATTRIBUTE_STANDARD_TYPE ","
	                          G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
				  G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK ","
				  G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET,
				  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				  NULL,
				  NULL);
	if (!info) {
		g_object_unref (file);
		g_free (uri);
		return FALSE;
	}

	if (g_file_info_get_file_type (info) == G_FILE_TYPE_SPECIAL) {
		/* It could be a block device, try */
		if (!g_strcmp0 (g_file_info_get_content_type (info), "inode/blockdevice")) {
			gchar *device;
			BraseroMedia media;
			BraseroDrive *drive;
			BraseroMedium *medium;
			BraseroMediumMonitor *monitor;

			g_object_unref (info);
			g_free (uri);

			monitor = brasero_medium_monitor_get_default ();
			while (brasero_medium_monitor_is_probing (monitor))
				sleep (1);

			device = g_file_get_path (file);
			drive = brasero_medium_monitor_get_drive (monitor, device);
			g_object_unref (monitor);

			if (!drive) {
				/* This is not a known optical drive to us. */
				g_object_unref (file);
				return FALSE;
			}

			medium = brasero_drive_get_medium (drive);
			if (!medium) {
				g_object_unref (file);
				g_object_unref (drive);
				return FALSE;
			}

			media = brasero_medium_get_status (medium);
			if (BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_BLANK)) {
				/* This medium is blank so it rules out blanking
				 * copying, checksuming. Open a data project. */
				g_object_unref (file);
				g_object_unref (drive);
				return FALSE;
			}
			g_object_unref (drive);

			/* It seems that we are expected to copy the disc */
			device = g_strdup (g_file_get_path (file));
			g_object_unref (file);
			brasero_app_copy_disc (app,
					       burner,
					       device,
					       cover_project,
					       burn_immediately != 0);
			g_free (device);
			return TRUE;
		}

		/* The rest are unsupported */
	}
	else if (g_str_has_prefix (uri, "cdda:")) {
		GFile *child;
		gchar *device;
		BraseroMediumMonitor *monitor;

		/* Make sure we are talking of the root */
		child = g_file_get_parent (file);
		if (child) {
			g_object_unref (child);
			g_object_unref (info);
			g_object_unref (file);
			g_free (uri);
			return FALSE;
		}

		/* We need to wait for the monitor to be ready */
		monitor = brasero_medium_monitor_get_default ();
		while (brasero_medium_monitor_is_probing (monitor))
			sleep (1);
		g_object_unref (monitor);

		if (g_str_has_suffix (uri, "/"))
			device = g_strdup_printf ("/dev/%.*s",
				                  (int) (strrchr (uri, '/') - uri - 7),
				                  uri + 7);
		else
			device = g_strdup_printf ("/dev/%s", uri + 7);
		brasero_app_copy_disc (app,
				       burner,
				       device,
				       cover_project,
				       burn_immediately != 0);
		g_free (device);

		retval = TRUE;
	}
	else if (g_str_has_prefix (uri, "file:/")
	     &&  g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
		BraseroMediumMonitor *monitor;
		gchar *directory_path;
		GSList *drives;
		GSList *iter;

		/* Try to detect a mounted optical disc */
		monitor = brasero_medium_monitor_get_default ();
		while (brasero_medium_monitor_is_probing (monitor))
			sleep (1);

		/* Check if this is a mount point for an optical disc */
		directory_path = g_file_get_path (file);
		drives = brasero_medium_monitor_get_drives (monitor, BRASERO_DRIVE_TYPE_ALL_BUT_FILE);
		for (iter = drives; iter; iter = iter->next) {
			gchar *mountpoint;
			BraseroDrive *drive;
			BraseroMedium *medium;

			drive = iter->data;
			medium = brasero_drive_get_medium (drive);
			mountpoint = brasero_volume_get_mount_point (BRASERO_VOLUME (medium), NULL);
			if (!mountpoint)
				continue;

			if (!g_strcmp0 (mountpoint, directory_path)) {
				g_free (mountpoint);
				brasero_app_copy_disc (app,
						       burner,
						       brasero_drive_get_device (drive),
						       cover_project,
						       burn_immediately != 0);
				retval = TRUE;
				break;
			}
			g_free (mountpoint);
		}
		g_slist_foreach (drives, (GFunc) g_object_unref, NULL);
		g_slist_free (drives);

		g_free (directory_path);
	}

	g_object_unref (info);
	g_object_unref (file);
	g_free (uri);
	return retval;
}

static void
brasero_app_recent_open (GtkRecentChooser *chooser,
			 BraseroApp *app)
{
	gchar *uri;
    	const gchar *mime;
    	GtkRecentInfo *item;
	GtkRecentManager *manager;

	/* This is a workaround since following code doesn't work */
	/*
    	item = gtk_recent_chooser_get_current_item (GTK_RECENT_CHOOSER (chooser));
	if (!item)
		return;
	*/

	uri = gtk_recent_chooser_get_current_uri (GTK_RECENT_CHOOSER (chooser));
	if (!uri)
		return;

	manager = gtk_recent_manager_get_default ();
	item = gtk_recent_manager_lookup_item (manager, uri, NULL);

	if (!item) {
		g_free (uri);
		return;
	}

	mime = gtk_recent_info_get_mime_type (item);

	if (!mime) {
		g_free (uri);
		g_warning ("Unrecognized mime type");
		return;
	}

	/* Make sure it is no longer one shot */
	brasero_app_open_by_mime (app,
	                          uri,
	                          mime,
	                          TRUE);
	gtk_recent_info_unref (item);
	g_free (uri);
}

static void
brasero_app_add_recent (BraseroApp *app,
			GtkActionGroup *group)
{
	GtkRecentManager *recent;
	GtkRecentFilter *filter;
	GtkAction *action;

	recent = gtk_recent_manager_get_default ();
	action = gtk_recent_action_new_for_manager ("RecentProjects",
						    _("_Recent Projects"),
						    _("Display the projects recently opened"),
						    NULL,
						    recent);
	filter = gtk_recent_filter_new ();

	gtk_recent_filter_set_name (filter, _("_Recent Projects"));
	gtk_recent_filter_add_mime_type (filter, "application/x-brasero");
	gtk_recent_filter_add_mime_type (filter, "application/vnd.efi.iso");
	gtk_recent_filter_add_mime_type (filter, "application/x-cd-image");
	gtk_recent_filter_add_mime_type (filter, "application/x-cdrdao-toc");
	gtk_recent_filter_add_mime_type (filter, "application/x-toc");
	gtk_recent_filter_add_mime_type (filter, "application/x-cue");
	gtk_recent_filter_add_mime_type (filter, "audio/x-scpls");
	gtk_recent_filter_add_mime_type (filter, "audio/x-ms-asx");
	gtk_recent_filter_add_mime_type (filter, "audio/x-mp3-playlist");
	gtk_recent_filter_add_mime_type (filter, "audio/x-mpegurl");

	gtk_recent_chooser_add_filter (GTK_RECENT_CHOOSER (action), filter);
	gtk_recent_chooser_set_filter (GTK_RECENT_CHOOSER (action), filter);

	gtk_recent_chooser_set_local_only (GTK_RECENT_CHOOSER (action), TRUE);

	gtk_recent_chooser_set_limit (GTK_RECENT_CHOOSER (action), 5);

	gtk_recent_chooser_set_show_tips (GTK_RECENT_CHOOSER (action), TRUE);

	gtk_recent_chooser_set_show_icons (GTK_RECENT_CHOOSER (action), TRUE);

	gtk_recent_chooser_set_sort_type (GTK_RECENT_CHOOSER (action), GTK_RECENT_SORT_MRU);

	gtk_action_group_add_action (group, action);
	g_object_unref (action);
	g_signal_connect (action,
			  "item-activated",
			  G_CALLBACK (brasero_app_recent_open),
			  app);
}

static void
brasero_menu_item_selected_cb (GtkMenuItem *proxy,
			       BraseroApp *app)
{
	BraseroAppPrivate *priv;
	GtkAction *action;
	gchar *message;

	priv = BRASERO_APP_PRIVATE (app);

	action = gtk_activatable_get_related_action (GTK_ACTIVATABLE (proxy));
	g_return_if_fail (action != NULL);

	g_object_get (G_OBJECT (action), "tooltip", &message, NULL);
	if (message) {
		gtk_statusbar_push (GTK_STATUSBAR (priv->statusbar2),
				    priv->tooltip_ctx,
				    message);
		g_free (message);

		gtk_statusbar_push (GTK_STATUSBAR (priv->statusbar1),
				    priv->tooltip_ctx,
				    "");
	}
}

static void
brasero_menu_item_deselected_cb (GtkMenuItem *proxy,
				 BraseroApp *app)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);

	gtk_statusbar_pop (GTK_STATUSBAR (priv->statusbar2),
			   priv->tooltip_ctx);
	gtk_statusbar_pop (GTK_STATUSBAR (priv->statusbar1),
			   priv->tooltip_ctx);
}

static void
brasero_connect_ui_manager_proxy_cb (GtkUIManager *manager,
				     GtkAction *action,
				     GtkWidget *proxy,
				     BraseroApp *app)
{
	if (!GTK_IS_MENU_ITEM (proxy))
		return;

	g_signal_connect (proxy,
			  "select",
			  G_CALLBACK (brasero_menu_item_selected_cb),
			  app);
	g_signal_connect (proxy,
			  "deselect",
			  G_CALLBACK (brasero_menu_item_deselected_cb),
			  app);
}

static void
brasero_disconnect_ui_manager_proxy_cb (GtkUIManager *manager,
					GtkAction *action,
					GtkWidget *proxy,
					BraseroApp *app)
{
	if (!GTK_IS_MENU_ITEM (proxy))
		return;

	g_signal_handlers_disconnect_by_func (proxy,
					      G_CALLBACK (brasero_menu_item_selected_cb),
					      app);
	g_signal_handlers_disconnect_by_func (proxy,
					      G_CALLBACK (brasero_menu_item_deselected_cb),
					      app);
}

static void
brasero_caps_changed_cb (BraseroPluginManager *manager,
			 BraseroApp *app)
{
	BraseroAppPrivate *priv;
	GtkWidget *widget;

	priv = BRASERO_APP_PRIVATE (app);

	widget = gtk_ui_manager_get_widget (priv->manager, "/menubar/ToolMenu/Check");

	if (!brasero_burn_library_can_checksum ())
		gtk_widget_set_sensitive (widget, FALSE);
	else
		gtk_widget_set_sensitive (widget, TRUE);
}

void
brasero_app_create_mainwin (BraseroApp *app)
{
	GtkWidget *hbox;
	GtkWidget *menubar;
	GError *error = NULL;
	BraseroAppPrivate *priv;
	GtkAccelGroup *accel_group;
	GtkActionGroup *action_group;
	BraseroPluginManager *plugin_manager;

	priv = BRASERO_APP_PRIVATE (app);

	if (priv->mainwin)
		return;

	/* New window */
	priv->mainwin = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_icon_name (GTK_WINDOW (priv->mainwin), "brasero");

	g_signal_connect (G_OBJECT (priv->mainwin),
			  "delete-event",
			  G_CALLBACK (on_delete_cb),
			  app);
	g_signal_connect (G_OBJECT (priv->mainwin),
			  "destroy",
			  G_CALLBACK (on_destroy_cb),
			  app);

	/* contents */
	priv->contents = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (priv->contents);

	gtk_container_add (GTK_CONTAINER (priv->mainwin), priv->contents);

	/* menu and toolbar */
	priv->manager = gtk_ui_manager_new ();
	g_signal_connect (priv->manager,
			  "connect-proxy",
			  G_CALLBACK (brasero_connect_ui_manager_proxy_cb),
			  app);
	g_signal_connect (priv->manager,
			  "disconnect-proxy",
			  G_CALLBACK (brasero_disconnect_ui_manager_proxy_cb),
			  app);

	action_group = gtk_action_group_new ("MenuActions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (action_group,
				      entries,
				      G_N_ELEMENTS (entries),
				      app);

	gtk_ui_manager_insert_action_group (priv->manager, action_group, 0);

	brasero_app_add_recent (app, action_group);

	if (!gtk_ui_manager_add_ui_from_string (priv->manager, description, -1, &error)) {
		g_message ("building menus failed: %s", error->message);
		g_error_free (error);
	}

	menubar = gtk_ui_manager_get_widget (priv->manager, "/menubar");
	gtk_box_pack_start (GTK_BOX (priv->contents), menubar, FALSE, FALSE, 0);

	/* window contents */
	priv->projects = brasero_project_manager_new ();
	gtk_widget_show (priv->projects);

	gtk_box_pack_start (GTK_BOX (priv->contents), priv->projects, TRUE, TRUE, 0);

	/* status bar to display the size of selected files */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_end (GTK_BOX (priv->contents), hbox, FALSE, TRUE, 0);

	priv->statusbar2 = gtk_statusbar_new ();
	gtk_widget_show (priv->statusbar2);
	priv->tooltip_ctx = gtk_statusbar_get_context_id (GTK_STATUSBAR (priv->statusbar2), "tooltip_info");
	gtk_box_pack_start (GTK_BOX (hbox), priv->statusbar2, FALSE, TRUE, 0);

	priv->statusbar1 = gtk_statusbar_new ();
	gtk_widget_show (priv->statusbar1);
	gtk_box_pack_start (GTK_BOX (hbox), priv->statusbar1, FALSE, TRUE, 0);

	/* Update everything */
	brasero_project_manager_register_ui (BRASERO_PROJECT_MANAGER (priv->projects),
					     priv->manager);

	gtk_ui_manager_ensure_update (priv->manager);

	/* check if we can use checksums (we need plugins enabled) */
	if (!brasero_burn_library_can_checksum ()) {
		GtkWidget *widget;

		widget = gtk_ui_manager_get_widget (priv->manager, "/menubar/ToolMenu/Check");
		gtk_widget_set_sensitive (widget, FALSE);
	}

	plugin_manager = brasero_plugin_manager_get_default ();
	g_signal_connect (plugin_manager,
			  "caps-changed",
			  G_CALLBACK (brasero_caps_changed_cb),
			  app);

	/* add accelerators */
	accel_group = gtk_ui_manager_get_accel_group (priv->manager);
	gtk_window_add_accel_group (GTK_WINDOW (priv->mainwin), accel_group);

	/* set up the window geometry */
	gtk_window_set_position (GTK_WINDOW (priv->mainwin),
				 GTK_WIN_POS_CENTER);

	g_signal_connect (priv->mainwin,
			  "window-state-event",
			  G_CALLBACK (on_window_state_changed_cb),
			  app);
	g_signal_connect (priv->mainwin,
			  "configure-event",
			  G_CALLBACK (on_configure_event_cb),
			  app);

	gtk_widget_realize (GTK_WIDGET (priv->mainwin));

	if (priv->parent) {
		gtk_window_set_modal (GTK_WINDOW (priv->mainwin), TRUE);
		gdk_window_set_transient_for (gtk_widget_get_window (GTK_WIDGET (priv->mainwin)), priv->parent);
	}

	brasero_app_load_window_state (app);
}

static void
brasero_app_activate (GApplication *gapp,
                      BraseroApp *app)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);

	/* Except if we are supposed to quit, show the window */
	if (priv->mainwin_running) {
		gtk_widget_show (priv->mainwin);
		gtk_window_present (GTK_WINDOW (priv->mainwin));
	}
}

gboolean
brasero_app_run_mainwin (BraseroApp *app)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);

	if (!priv->mainwin)
		return FALSE;

	if (priv->mainwin_running)
		return TRUE;

	priv->mainwin_running = 1;
	gtk_widget_show (GTK_WIDGET (priv->mainwin));

	if (priv->gapp)
		g_signal_connect (priv->gapp,
				  "activate",
				  G_CALLBACK (brasero_app_activate),
				  app);
	gtk_main ();
	return TRUE;
}

static GtkWindow *
brasero_app_get_io_parent_window (gpointer user_data)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (user_data);
	if (!priv->mainwin) {
		if (priv->parent)
			return GTK_WINDOW (priv->parent);
	}
	else {
		GtkWidget *toplevel;

		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (priv->mainwin));
		return GTK_WINDOW (toplevel);
	}

	return NULL;
}

static void
brasero_app_init (BraseroApp *object)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (object);

	priv->mainwin = NULL;

	/* Load settings */
	priv->setting = brasero_setting_get_default ();
	brasero_setting_load (priv->setting);

	g_set_application_name (_("Disc Burner"));
	gtk_window_set_default_icon_name ("brasero");

	brasero_io_set_parent_window_callback (brasero_app_get_io_parent_window, object);
}

static void
brasero_app_finalize (GObject *object)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (object);

	brasero_setting_save (priv->setting);
	g_object_unref (priv->setting);
	priv->setting = NULL;

	if (priv->saved_contents) {
		g_free (priv->saved_contents);
		priv->saved_contents = NULL;
	}

	G_OBJECT_CLASS (brasero_app_parent_class)->finalize (object);
}
static void
brasero_app_set_property (GObject *object,
                          guint prop_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
	BraseroAppPrivate *priv;

	g_return_if_fail (BRASERO_IS_APP (object));

	priv = BRASERO_APP_PRIVATE (object);

	switch (prop_id)
	{
	case PROP_GAPP:
		priv->gapp = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
brasero_app_get_property (GObject *object,
			  guint prop_id,
			  GValue *value,
			  GParamSpec *pspec)
{
	BraseroAppPrivate *priv;

	g_return_if_fail (BRASERO_IS_APP (object));

	priv = BRASERO_APP_PRIVATE (object);

	switch (prop_id)
	{
	case PROP_GAPP:
		g_value_set_object (value, priv->gapp);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
brasero_app_class_init (BraseroAppClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroAppPrivate));

	object_class->finalize = brasero_app_finalize;
	object_class->set_property = brasero_app_set_property;
	object_class->get_property = brasero_app_get_property;

	g_object_class_install_property (object_class,
	                                 PROP_GAPP,
	                                 g_param_spec_object("gapp",
	                                                     "GApplication",
	                                                     "The GApplication object",
	                                                     G_TYPE_APPLICATION,
	                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

BraseroApp *
brasero_app_new (GApplication *gapp)
{
	return g_object_new (BRASERO_TYPE_APP,
	                     "gapp", gapp,
	                     NULL);
}
