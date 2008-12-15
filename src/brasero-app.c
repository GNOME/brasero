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

#include "brasero-app.h"
#include "brasero-utils.h"
#include "brasero-jacket-edit.h"
#include "brasero-blank-dialog.h"
#include "brasero-sum-dialog.h"
#include "brasero-eject-dialog.h"
#include "brasero-session.h"
#include "brasero-project-manager.h"
#include "brasero-pref.h"
#include "burn-debug.h"
#include "burn-drive.h"
#include "burn-caps.h"
#include "burn.h"
#include "burn-plugin-manager.h"

typedef struct _BraseroAppPrivate BraseroAppPrivate;
struct _BraseroAppPrivate
{
	GtkWidget *toplevel;

	GtkWidget *projects;
	GtkWidget *contents;
	GtkWidget *statusbar1;
	GtkWidget *statusbar2;
	GtkUIManager *manager;

	guint tooltip_ctx;

	gint width;
	gint height;

	guint is_maximised:1;
	guint is_running:1;
};

#define BRASERO_APP_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_APP, BraseroAppPrivate))


G_DEFINE_TYPE (BraseroApp, brasero_app, GTK_TYPE_WINDOW);

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

	if (priv->is_running)
		toplevel = GTK_WINDOW (app);
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
	BraseroAppPrivate *priv;
	GtkWidget *alert;

	priv = BRASERO_APP_PRIVATE (app);

	/* Whatever happens, they need a parent or must be in the taskbar */
	if (priv->is_running)
		parent = GTK_WIDGET (app);
	else if (priv->toplevel)
		parent = priv->toplevel;

	alert = brasero_utils_create_message_dialog (parent,
						     primary_message,
						     secondary_message,
						     type);
	gtk_window_set_title (GTK_WINDOW (alert), _("Disc Burner"));

	if (!parent) {
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

void
brasero_app_get_geometry (BraseroApp *app,
			  gint *width,
			  gint *height,
			  gboolean *maximised)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);

	*width = priv->width;
	*height = priv->height;
	*maximised = priv->is_maximised;
}

static gboolean
on_delete_cb (GtkWidget *window, GdkEvent *event, BraseroApp *app)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);
	if (!priv->is_running)
		return FALSE;

	return brasero_session_save (app, TRUE, TRUE);
}

static gboolean
on_destroy_cb (GtkWidget *window, BraseroApp *app)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);
	if (priv->is_running)
		gtk_main_quit ();

	return FALSE;
}

static void
on_exit_cb (GtkAction *action, BraseroApp *app)
{
	if (brasero_session_save (app, TRUE, TRUE))
		return;

	gtk_widget_destroy (GTK_WIDGET (app));
}

gboolean
brasero_app_is_running (BraseroApp *app)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);
	return priv->is_running;
}

void
brasero_app_run (BraseroApp *app)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);

	priv->is_running = TRUE;
	gtk_widget_realize (GTK_WIDGET (app));
	brasero_session_load (app);
}

void
brasero_app_blank (BraseroApp *app,
		   const gchar *device)
{
	GtkWidget *dialog;
	GtkWidget *toplevel;
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);
	dialog = brasero_blank_dialog_new ();
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (app));

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

	if (!priv->is_running) {
		gtk_widget_show (dialog);
		gtk_dialog_run (GTK_DIALOG (dialog));

		/* brasero-tool-dialog auto destroys itself */
		// gtk_widget_destroy (dialog);
	}
	else {
		gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
		gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
		gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);
		
		gtk_widget_show (dialog);
	}
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

	dialog = brasero_eject_dialog_new();
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (app));

	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);

	gtk_widget_show (dialog);
}

void
brasero_app_check (BraseroApp *app,
		   const gchar *device)
{
	GtkWidget *dialog;
	GtkWidget *toplevel;
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (app);

	dialog = brasero_sum_dialog_new ();
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (app));

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

	if (!priv->is_running) {
		gtk_widget_show (dialog);
		gtk_dialog_run (GTK_DIALOG (dialog));

		/* brasero-tool-dialog auto destroys itself */
		// gtk_widget_destroy (dialog);
	}
	else {
		gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
		gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
		gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);
		
		gtk_widget_show (dialog);
	}
}

static void
on_integrity_check_cb (GtkAction *action, BraseroApp *app)
{
	brasero_app_check (app, NULL);
}

static void
on_prefs_cb (GtkAction *action, BraseroApp *app)
{
	GtkWidget *dialog;
	GtkWidget *toplevel;

	dialog = brasero_pref_new ();
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (app));

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
		"Phil Bull <philbull@gmail.com>"
		"Milo Casagrande <milo_casagrande@yahoo.it>"
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

	comments = g_strdup (_("A simple to use CD/DVD burning application for GNOME"));

	license = g_strjoin ("\n\n",
                             _(license_part[0]),
                             _(license_part[1]),
                             _(license_part[2]),
			     NULL);

	gtk_show_about_dialog (GTK_WINDOW (GTK_WIDGET (app)),
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

 	gtk_show_uri (NULL, "ghelp:brasero", gtk_get_current_event_time (), &error);
   	if (error) {
		GtkWidget *d;
        
		d = gtk_message_dialog_new (GTK_WINDOW (app),
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
			    gpointer *NULL_data)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (widget);

	if (event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED)
		priv->is_maximised = 1;
	else
		priv->is_maximised = 0;

	return FALSE;
}

static gboolean
on_configure_event_cb (GtkWidget *widget,
		       GdkEventConfigure *event,
		       gpointer NULL_data)
{
	BraseroAppPrivate *priv;

	priv = BRASERO_APP_PRIVATE (widget);

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

static void
brasero_app_init (BraseroApp *object)
{
	GtkWidget *hbox;
	GtkWidget *menubar;
	GError *error = NULL;
	BraseroAppPrivate *priv;
	GtkAccelGroup *accel_group;
	GtkActionGroup *action_group;
	BraseroPluginManager *plugin_manager;

	/* New window */
	priv = BRASERO_APP_PRIVATE (object);
	g_set_application_name (_("Brasero Disc Burner"));

	gtk_window_set_default_icon_name ("brasero");
	gtk_window_set_icon_name (GTK_WINDOW (object), "brasero");

	g_signal_connect (G_OBJECT (object), "delete-event",
			  G_CALLBACK (on_delete_cb), object);
	g_signal_connect (G_OBJECT (object), "destroy",
			  G_CALLBACK (on_destroy_cb), object);

	/* contents */
	priv->contents = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (priv->contents);

	gtk_container_add (GTK_CONTAINER (object), priv->contents);

	/* menu and toolbar */
	priv->manager = gtk_ui_manager_new ();
	g_signal_connect (priv->manager,
			  "connect-proxy",
			  G_CALLBACK (brasero_connect_ui_manager_proxy_cb),
			  object);
	g_signal_connect (priv->manager,
			  "disconnect-proxy",
			  G_CALLBACK (brasero_disconnect_ui_manager_proxy_cb),
			  object);

	action_group = gtk_action_group_new ("MenuActions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (action_group,
				      entries,
				      G_N_ELEMENTS (entries),
				      object);

	gtk_ui_manager_insert_action_group (priv->manager, action_group, 0);

	brasero_app_add_recent (object, action_group);

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
			  object);

	/* add accelerators */
	accel_group = gtk_ui_manager_get_accel_group (priv->manager);
	gtk_window_add_accel_group (GTK_WINDOW (object), accel_group);

	/* set up the window geometry */
	gtk_window_set_position (GTK_WINDOW (object), GTK_WIN_POS_CENTER);

#ifdef BUILD_GNOME2

	brasero_session_connect (object);

#endif

	g_signal_connect (object,
			  "window-state-event",
			  G_CALLBACK (on_window_state_changed_cb),
			  NULL);
	g_signal_connect (object,
			  "configure-event",
			  G_CALLBACK (on_configure_event_cb),
			  NULL);
}

static void
brasero_app_finalize (GObject *object)
{

#ifdef BUILD_GNOME2

       brasero_session_disconnect (BRASERO_APP (object));
#endif

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
	return g_object_new (BRASERO_TYPE_APP,
			     "type", GTK_WINDOW_TOPLEVEL,
			     NULL);
}
