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
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include <gtk/gtk.h>

#include <libxml/xmlerror.h>
#include <libxml/xmlwriter.h>
#include <libxml/parser.h>
#include <libxml/xmlstring.h>

#include "brasero-app.h"
#include "brasero-utils.h"
#include "brasero-jacket-edit.h"
#include "brasero-blank-dialog.h"
#include "brasero-sum-dialog.h"
#include "brasero-eject-dialog.h"
#include "brasero-session.h"
#include "brasero-project-manager.h"
#include "brasero-pref.h"
#include "brasero-image-option-dialog.h"
#include "brasero-disc-copy-dialog.h"
#include "brasero-burn-dialog.h"
#include "burn-debug.h"
#include "brasero-drive.h"
#include "burn-caps.h"
#include "burn.h"
#include "burn-plugin-manager.h"

typedef struct _BraseroAppPrivate BraseroAppPrivate;
struct _BraseroAppPrivate
{
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

	gint width;
	gint height;

	gchar *saved_contents;
	guint is_maximised:1;
};

#define BRASERO_APP_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_APP, BraseroAppPrivate))


G_DEFINE_TYPE (BraseroApp, brasero_app, G_TYPE_OBJECT);


#define SESSION_VERSION "0.1"
#define BRASERO_SESSION_TMP_SESSION_PATH	"brasero.session"

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
	 N_("Choose plugins for brasero"), G_CALLBACK (on_prefs_cb)},

	{"Eject", "media-eject", N_("E_ject"), NULL,
	 N_("Eject a disc"), G_CALLBACK (on_eject_cb)},

	{"Erase", "media-optical-blank", N_("_Erase..."), NULL,
	 N_("Erase a disc"), G_CALLBACK (on_erase_cb)},

	{"Check", NULL, N_("_Check Integrity..."), NULL,
	 N_("Check data integrity of disc"), G_CALLBACK (on_integrity_check_cb)},

	{"Exit", GTK_STOCK_QUIT, NULL, NULL,
	 N_("Exit the program"), G_CALLBACK (on_exit_cb)},

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
		"<menuitem action='Exit'/>"
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
		"<menuitem action='Erase'/>"
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
	gchar *height_str = NULL;
	gchar *width_str = NULL;
	gchar *state_str = NULL;
	gchar *version = NULL;
	gint height;
	gint width;
	gint state = 0;

	gchar *session_path;
	xmlNodePtr item;
	xmlDocPtr session = NULL;

	GdkScreen *screen;
	GdkRectangle rect;
	gint monitor;

	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);

	/* Make sure that on first run the window has a default size of at least
	 * 85% of the screen (hardware not GTK+) */
	screen = gtk_window_get_screen (GTK_WINDOW (priv->mainwin));
	monitor = gdk_screen_get_monitor_at_window (screen, GTK_WIDGET (priv->mainwin)->window);
	gdk_screen_get_monitor_geometry (screen, monitor, &rect);
	width = rect.width / 100 * 85;
	height = rect.height / 100 * 85;

	session_path = brasero_app_get_path (BRASERO_SESSION_TMP_SESSION_PATH);
	if (!session_path)
		goto end;

	session = xmlParseFile (session_path);
	g_free (session_path);

	if (!session)
		goto end;

	item = xmlDocGetRootElement (session);
	if (!item)
		goto end;

	if (xmlStrcmp (item->name, (const xmlChar *) "Session") || item->next)
		goto end;

	item = item->children;
	while (item) {
		if (!xmlStrcmp (item->name, (const xmlChar *) "version")) {
			if (version)
				goto end;

			version = (char *) xmlNodeListGetString (session,
								 item->xmlChildrenNode,
								 1);
		}
		else if (!xmlStrcmp (item->name, (const xmlChar *) "width")) {
			if (width_str)
				goto end;

			width_str = (char *) xmlNodeListGetString (session,
								   item->xmlChildrenNode,
								   1);
		}
		else if (!xmlStrcmp (item->name, (const xmlChar *) "height")) {
			if (height_str)
				goto end;

			height_str = (char *) xmlNodeListGetString (session,
								    item->xmlChildrenNode,
								    1);
		}
		else if (!xmlStrcmp (item->name, (const xmlChar *) "state")) {
			if (state_str)
				goto end;

			state_str = (char *) xmlNodeListGetString (session,
								   item->xmlChildrenNode,
								   1);
		}
		else if (item->type == XML_ELEMENT_NODE)
			goto end;

		item = item->next;
	}

	if (!version || strcmp (version, SESSION_VERSION))
		goto end;

	/* restore the window state */
	if (height_str)
		height = (int) g_strtod (height_str, NULL);

	if (width_str)
		width = (int) g_strtod (width_str, NULL);

	if (state_str)
		state = (int) g_strtod (state_str, NULL);

end:
	if (height_str)
		g_free (height_str);

	if (width_str)
		g_free (width_str);

	if (state_str)
		g_free (state_str);

	if (version)
		g_free (version);

	xmlFreeDoc (session);

	if (width && height)
		gtk_window_resize (GTK_WINDOW (priv->mainwin),
				   width,
				   height);

	if (state)
		gtk_window_maximize (GTK_WINDOW (priv->mainwin));

	return TRUE;
}

void
brasero_app_save_window_state (BraseroApp *app)
{
	gint success;
	gchar *session_path;
	xmlTextWriter *session;
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);

	/* now save the state of the window */
	session_path = brasero_app_get_path (BRASERO_SESSION_TMP_SESSION_PATH);
	if (!session_path)
		return;

	/* write information */
	session = xmlNewTextWriterFilename (session_path, 0);
	if (!session) {
		g_free (session_path);
		return;
	}

	xmlTextWriterSetIndent (session, 1);
	xmlTextWriterSetIndentString (session, (xmlChar *) "\t");

	success = xmlTextWriterStartDocument (session,
					      NULL,
					      NULL,
					      NULL);
	if (success < 0)
		goto error;

	success = xmlTextWriterStartElement (session,
					     (xmlChar *) "Session");
	if (success < 0)
		goto error;

	success = xmlTextWriterWriteElement (session,
					     (xmlChar *) "version",
					     (xmlChar *) SESSION_VERSION);
	if (success < 0)
		goto error;

	success = xmlTextWriterWriteFormatElement (session,
						   (xmlChar *) "width",
						   "%i",
						   priv->width);
	if (success < 0)
		goto error;

	success = xmlTextWriterWriteFormatElement (session,
						   (xmlChar *) "height",
						   "%i",
						   priv->height);
	if (success < 0)
		goto error;

	success = xmlTextWriterWriteFormatElement (session,
						   (xmlChar *) "state",
						   "%i",
						   priv->is_maximised);
	if (success < 0)
		goto error;

	success = xmlTextWriterEndElement (session);
	if (success < 0)
		goto error;

	xmlTextWriterEndDocument (session);
	xmlFreeTextWriter (session);
	g_free (session_path);
	return;

error:
	xmlTextWriterEndDocument (session);
	xmlFreeTextWriter (session);
	g_remove (session_path);
	g_free (session_path);
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
			return (brasero_burn_dialog_cancel (BRASERO_BURN_DIALOG (priv->burn_dialog)) == FALSE);

		gtk_widget_destroy (priv->burn_dialog);
		return FALSE;
	}

	if (priv->tool_dialog) {
		if (cancellable)
			return (brasero_tool_dialog_cancel (BRASERO_TOOL_DIALOG (priv->tool_dialog)) == FALSE);

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
brasero_app_toplevel_destroyed_cb (GtkObject *object,
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

	if (priv->mainwin)
		toplevel = GTK_WINDOW (priv->mainwin);
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
					 primary_message);

	if (!toplevel && priv->parent) {
		gtk_widget_realize (GTK_WIDGET (dialog));
		gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
		gdk_window_set_transient_for (GTK_WIDGET (dialog)->window, priv->parent);
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
		gdk_window_set_transient_for (GTK_WIDGET (alert)->window, priv->parent);
	}

	if (is_on_top) {
		gtk_window_set_title (GTK_WINDOW (alert), _("Disc Burner"));
		gtk_window_set_skip_pager_hint (GTK_WINDOW (alert), FALSE);
		gtk_window_set_skip_taskbar_hint (GTK_WINDOW (alert), FALSE);
	}

	gtk_dialog_run (GTK_DIALOG (alert));
	gtk_widget_destroy (alert);
}

GtkUIManager *
brasero_app_get_ui_manager (BraseroApp *app)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);
	return priv->manager;
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

	brasero_app_save_window_state (app);
	return FALSE;
}

static void
on_exit_cb (GtkAction *action, BraseroApp *app)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);

	if (brasero_app_save_contents (app, TRUE))
		return;

	if (priv->mainwin) {
		brasero_app_save_window_state (app);
		gtk_widget_destroy (GTK_WIDGET (priv->mainwin));
	}
}

gboolean
brasero_app_is_running (BraseroApp *app)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);
	return (priv->mainwin != NULL);
}

void
brasero_app_set_parent (BraseroApp *app,
			guint parent_xid)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);

	priv->parent = gdk_window_foreign_new (parent_xid);
}

gboolean
brasero_app_burn (BraseroApp *app,
		  BraseroBurnSession *session)
{
	gboolean success;
	GtkWidget *dialog;
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);

	/* now setup the burn dialog */
	dialog = brasero_burn_dialog_new ();
	priv->burn_dialog = dialog;

	brasero_app_set_toplevel (app, GTK_WINDOW (dialog));
	success = brasero_burn_dialog_run (BRASERO_BURN_DIALOG (dialog), session);

	priv->burn_dialog = NULL;

	/* The destruction of the dialog will bring the main window forward */
	gtk_widget_destroy (dialog);
	return success;
}

void
brasero_app_burn_image (BraseroApp *app,
			const gchar *uri)
{
	BraseroBurnSession *session;
	GtkResponseType result;
	GtkWidget *dialog;

	/* setup, show, and run options dialog */
	dialog = brasero_image_option_dialog_new ();
	brasero_image_option_dialog_set_image_uri (BRASERO_IMAGE_OPTION_DIALOG (dialog), uri);

	brasero_app_set_toplevel (app, GTK_WINDOW (dialog));

	result = gtk_dialog_run (GTK_DIALOG (dialog));
	if (result != GTK_RESPONSE_OK) {
		gtk_widget_destroy (dialog);
		return;
	}

	session = brasero_burn_options_get_session (BRASERO_BURN_OPTIONS (dialog));
	/* The destruction of the dialog will bring the main window forward */
	gtk_widget_destroy (dialog);

	brasero_app_burn (app, session);
	g_object_unref (session);
}

void
brasero_app_copy_disc (BraseroApp *app,
		       const gchar *device,
		       const gchar *cover)
{
	BraseroBurnSession *session;
	GtkResponseType result;
	GtkWidget *dialog;

	dialog = brasero_disc_copy_dialog_new ();
	brasero_app_set_toplevel (app, GTK_WINDOW (dialog));

	/* if a device is specified then get the corresponding medium */
	if (device) {
		BraseroDrive *drive;
		BraseroMediumMonitor *monitor;

		monitor = brasero_medium_monitor_get_default ();
		drive = brasero_medium_monitor_get_drive (monitor, device);
		g_object_unref (monitor);

		brasero_disc_copy_dialog_set_drive (BRASERO_DISC_COPY_DIALOG (dialog), drive);
		g_object_unref (drive);
	}

	result = gtk_dialog_run (GTK_DIALOG (dialog));
	if (result != GTK_RESPONSE_OK) {
		gtk_widget_destroy (dialog);
		return;
	}

	session = brasero_burn_options_get_session (BRASERO_BURN_OPTIONS (dialog));

	/* The destruction of the dialog will bring the main window forward */
	gtk_widget_destroy (dialog);

	/* Set a cover if any. */
	if (cover) {
		GValue *value;

		value = g_new0 (GValue, 1);
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (value, cover);
		brasero_burn_session_tag_add (session,
					      BRASERO_COVER_URI,
					      value);
	}

	brasero_app_burn (app, session);
	g_object_unref (session);
}

void
brasero_app_blank (BraseroApp *app,
		   const gchar *device)
{
	GtkWidget *dialog;
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);
	dialog = brasero_blank_dialog_new ();

	if (device) {
		BraseroDrive *drive;
		BraseroMedium *medium;
		BraseroMediumMonitor *monitor;

		monitor = brasero_medium_monitor_get_default ();
		drive = brasero_medium_monitor_get_drive (monitor, device);
		g_object_unref (monitor);

		medium = brasero_drive_get_medium (drive);

		brasero_tool_dialog_set_medium (BRASERO_TOOL_DIALOG (dialog), medium);
		g_object_unref (drive);
	}

	priv->tool_dialog = dialog;
	if (!priv->mainwin) {
		gtk_widget_realize (dialog);

		if (priv->parent) {
			gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
			gdk_window_set_transient_for (GTK_WIDGET (dialog)->window, priv->parent);
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
	gtk_widget_destroy (dialog);
	priv->tool_dialog = NULL;
}

static void
on_erase_cb (GtkAction *action, BraseroApp *app)
{
	brasero_app_blank (app, NULL);
}

static void
on_eject_cb (GtkAction *action, BraseroApp *app)
{
	GtkWidget *dialog;
	GtkWidget *toplevel;
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);

	dialog = brasero_eject_dialog_new ();

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
		   const gchar *device)
{
	GtkWidget *dialog;
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);

	dialog = brasero_sum_dialog_new ();
	priv->tool_dialog = dialog;

	if (device) {
		BraseroDrive *drive;
		BraseroMedium *medium;
		BraseroMediumMonitor *monitor;

		monitor = brasero_medium_monitor_get_default ();
		drive = brasero_medium_monitor_get_drive (monitor, device);
		g_object_unref (monitor);

		medium = brasero_drive_get_medium (drive);

		brasero_tool_dialog_set_medium (BRASERO_TOOL_DIALOG (dialog), medium);
		g_object_unref (drive);
	}

	if (!priv->mainwin) {
		gtk_widget_realize (dialog);

		if (priv->parent) {
			gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
			gdk_window_set_transient_for (GTK_WIDGET (dialog)->window, priv->parent);
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
	gtk_widget_destroy (dialog);
	priv->tool_dialog = NULL;
}

static void
on_integrity_check_cb (GtkAction *action, BraseroApp *app)
{
	brasero_app_check (app, NULL);
}

static void
brasero_app_current_toplevel_destroyed (GtkWidget *widget,
					BraseroApp *app)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);
	if (priv->mainwin)
		gtk_widget_show (GTK_WIDGET (priv->mainwin));
}

void
brasero_app_set_toplevel (BraseroApp *app, GtkWindow *window)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);

	if (!priv->mainwin) {
		if (priv->parent) {
			gtk_widget_realize (GTK_WIDGET (window));
			gtk_window_set_modal (GTK_WINDOW (window), TRUE);
			gdk_window_set_transient_for (GTK_WIDGET (window)->window, priv->parent);
		}
		else {
			gtk_window_set_skip_taskbar_hint (GTK_WINDOW (window), FALSE);
			gtk_window_set_skip_pager_hint (GTK_WINDOW (window), FALSE);
			gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_NORMAL);
			gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
		}
	}
	else {
		gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (priv->mainwin));
		gtk_window_set_modal (GTK_WINDOW (window), TRUE);

		/* hide main dialog if it is shown */
		gtk_widget_hide (GTK_WIDGET (priv->mainwin));

		gtk_window_set_skip_taskbar_hint (GTK_WINDOW (window), FALSE);
		gtk_window_set_skip_pager_hint (GTK_WINDOW (window), FALSE);
		gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_NORMAL);
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
		NULL
	};

	const gchar *documenters[] = {
		"Phil Bull <philbull@gmail.com>\n"
		"Milo Casagrande <milo_casagrande@yahoo.it>\n"
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
			       "copyright", "Copyright © 2005-2008 Philippe Rouquier",
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

 	gtk_show_uri (NULL, "ghelp:brasero", gtk_get_current_event_time (), &error);
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

	if (event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED)
		priv->is_maximised = 1;
	else
		priv->is_maximised = 0;

	return FALSE;
}

static gboolean
on_configure_event_cb (GtkWidget *widget,
		       GdkEventConfigure *event,
		       BraseroApp *app)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);

	if (!priv->is_maximised) {
		priv->width = event->width;
		priv->height = event->height;
	}

	return FALSE;
}

static void
brasero_app_recent_open (GtkRecentChooser *chooser,
			 BraseroApp *app)
{
	gchar *uri;
    	const gchar *mime;
    	GtkRecentInfo *item;
	BraseroAppPrivate *priv;
	GtkRecentManager *manager;

	priv = BRASERO_APP_PRIVATE (app);
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

	brasero_project_manager_open_by_mime (BRASERO_PROJECT_MANAGER (priv->projects),
					      uri,
					      mime);
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

	action = g_object_get_data (G_OBJECT (proxy),  "gtk-action");
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
	priv->contents = gtk_vbox_new (FALSE, 0);
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
	hbox = gtk_hbox_new (TRUE, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_end (GTK_BOX (priv->contents), hbox, FALSE, TRUE, 0);

	priv->statusbar2 = gtk_statusbar_new ();
	gtk_widget_show (priv->statusbar2);
	priv->tooltip_ctx = gtk_statusbar_get_context_id (GTK_STATUSBAR (priv->statusbar2), "tooltip_info");
	gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR (priv->statusbar2), FALSE);
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
		gdk_window_set_transient_for (GTK_WIDGET (priv->mainwin)->window, priv->parent);
	}

	brasero_app_load_window_state (app);
}

void
brasero_app_run_mainwin (BraseroApp *app)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);

	gtk_widget_show (GTK_WIDGET (priv->mainwin));
	gtk_main ();
}

static void
brasero_app_init (BraseroApp *object)
{
	BraseroAppPrivate *priv;

	/* Connect to session */
	brasero_session_connect (object);

	priv = BRASERO_APP_PRIVATE (object);

	g_set_application_name (_("Brasero Disc Burner"));
	gtk_window_set_default_icon_name ("brasero");
}

static void
brasero_app_finalize (GObject *object)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (object);

	brasero_session_disconnect (BRASERO_APP (object));

	if (priv->saved_contents) {
		g_free (priv->saved_contents);
		priv->saved_contents = NULL;
	}

	G_OBJECT_CLASS (brasero_app_parent_class)->finalize (object);
}

static void
brasero_app_class_init (BraseroAppClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroAppPrivate));

	object_class->finalize = brasero_app_finalize;
}

BraseroApp *
brasero_app_new (void)
{
	return g_object_new (BRASERO_TYPE_APP, NULL);
}
