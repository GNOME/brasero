/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
/***************************************************************************
 *            main.c
 *
 *  Sat Jun 11 12:00:29 2005
 *  Copyright  2005  Philippe Rouquier	
 *  <brasero-app@wanadoo.fr>
 ****************************************************************************/

#include <string.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <locale.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include <gst/gst.h>

#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>

#include <nautilus-burn-recorder.h>
#include <nautilus-burn-init.h>


#include <gconf/gconf-client.h>

#ifdef HAVE_LIBNOTIFY
#include <libnotify/notify.h>
#endif

#include "brasero-app.h"
#include "menu.h"
#include "blank-dialog.h"
#include "brasero-sum-dialog.h"
#include "brasero-session.h"
#include "brasero-project-manager.h"

static GConfClient *client;
gchar *project_uri;
gchar *iso_uri;
gchar **files;
gchar **audio_project;
gchar **data_project;
gint copy_project;
gint is_escaped;
gint open_ncb;
gint debug;

static const GOptionEntry options [] = {
	{ "project", 'p', G_OPTION_FLAG_FILENAME, G_OPTION_ARG_STRING, &project_uri,
	  N_("Open the specified project"),
	  N_("PROJECT") },

	{ "audio", 'a', 0, G_OPTION_ARG_NONE, &audio_project,
	  N_("Open an audio project adding the URIs given on the command line"),
	  NULL },

	{ "data", 'd', 0, G_OPTION_ARG_NONE, &data_project,
         N_("Open a data project adding the URIs given on the command line"),
          NULL },

	{ "copy", 'c', 0, G_OPTION_ARG_NONE, &copy_project,
	  N_("Copy a disc"),
	  NULL },

	{ "image", 'i', G_OPTION_FLAG_FILENAME, G_OPTION_ARG_STRING, &iso_uri,
	 N_("Uri of an image file to be burnt (autodetected)"),
          NULL },

	{ "ncb", 'n', 0, G_OPTION_ARG_NONE, &open_ncb,
	  N_("Open a data project with the contents of nautilus-cd-burner"),
          NULL },

	{ "debug", 'g', 0, G_OPTION_ARG_NONE, &debug,
	  N_("Display debug statements on stdout"),
	  NULL },

	{ G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &files,
	  NULL, NULL }, /* collects file arguments */

	{ NULL }
};

#define BRASERO_PROJECT_OPEN_URI(app, function, path)	\
{									\
	gchar *uri;							\
	uri = gnome_vfs_make_uri_from_input (path);			\
	function (BRASERO_PROJECT_MANAGER (app->contents), uri);	\
	return;								\
}

#define BRASERO_PROJECT_OPEN_LIST(app, function, uris)	\
{						\
	GSList *list = NULL;			\
	gchar **iter;				\
	/* convert all names into a GSList * */	\
	for (iter = uris; iter && *iter; iter ++) {				\
		gchar *uri;							\
		uri = gnome_vfs_make_uri_from_input (*iter);			\
		list = g_slist_prepend (list, uri);				\
	}									\
	function (BRASERO_PROJECT_MANAGER (app->contents), list);		\
	g_slist_foreach (list, (GFunc) g_free, NULL);				\
	g_slist_free (list);							\
	return;									\
}

static gboolean
on_delete_cb (GtkWidget *window, GdkEvent *event, BraseroApp *app)
{
	brasero_session_save (app);
	return FALSE;
}

static gboolean
on_destroy_cb (GtkWidget *window, GdkEvent *event, BraseroApp *app)
{
	gtk_main_quit ();
	return FALSE;
}

void
on_exit_cb (GtkAction *action, BraseroApp *app)
{
	brasero_session_save (app);
	gtk_widget_destroy (app->mainwin);
}

void
on_erase_cb (GtkAction *action, BraseroApp *app)
{
	GtkWidget *dialog;
	GtkWidget *toplevel;

	dialog = brasero_blank_dialog_new ();
	toplevel = gtk_widget_get_toplevel (app->mainwin);

	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);

	gtk_widget_show_all (dialog);
}

void
on_integrity_check_cb (GtkAction *action, BraseroApp *app)
{
	GtkWidget *dialog;
	GtkWidget *toplevel;

	dialog = brasero_sum_dialog_new ();
	toplevel = gtk_widget_get_toplevel (app->mainwin);

	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);

	gtk_widget_show_all (dialog);
}

void
on_disc_info_cb (GtkAction *button, BraseroApp *app)
{

}

void
on_about_cb (GtkAction *action, BraseroApp *app)
{
	GtkWidget *dialog;
	const char *authors[] = { "Philippe Rouquier", NULL };
	GdkPixbuf *logo;

	logo = gdk_pixbuf_new_from_file (BRASERO_DATADIR "/icon-final-128x128.png", NULL);

	dialog = gtk_about_dialog_new ();
	gtk_about_dialog_set_name (GTK_ABOUT_DIALOG (dialog), "Brasero");
	gtk_about_dialog_set_copyright (GTK_ABOUT_DIALOG (dialog),
					"Copyright (c) Philippe Rouquier");
	gtk_about_dialog_set_authors (GTK_ABOUT_DIALOG (dialog), authors);
	gtk_about_dialog_set_comments (GTK_ABOUT_DIALOG (dialog),
				       "Disc burning tool");
	gtk_about_dialog_set_logo (GTK_ABOUT_DIALOG (dialog), logo);
	g_object_unref (logo);
	gtk_about_dialog_set_version (GTK_ABOUT_DIALOG (dialog), VERSION);
	gtk_about_dialog_set_translator_credits (GTK_ABOUT_DIALOG (dialog),
						 _("translator-credits"));

	gtk_window_set_transient_for (GTK_WINDOW (dialog),
				      GTK_WINDOW (app->mainwin));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_position (GTK_WINDOW (dialog),
				 GTK_WIN_POS_CENTER_ON_PARENT);

	gtk_widget_show (dialog);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static gboolean
on_window_state_changed_cb (GtkWidget *widget,
			    GdkEventWindowState *event,
			    BraseroApp *app)
{
	if (event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED)
		app->is_maximised = 1;
	else
		app->is_maximised = 0;

	return FALSE;
}

static gboolean
on_configure_event_cb (GtkWidget *widget,
		       GdkEventConfigure *event,
		       BraseroApp *app)
{
	if (!app->is_maximised) {
		app->width = event->width;
		app->height = event->height;
	}

	return FALSE;
}

static void
brasero_app_recent_open (GtkRecentChooser *chooser,
			 BraseroApp *app)
{
    	const gchar *mime;
    	GtkRecentInfo *item;

    	item = gtk_recent_chooser_get_current_item (chooser);
    	mime = gtk_recent_info_get_mime_type (item);

    if (!strcmp (mime, "application/x-brasero")) {

	BRASERO_PROJECT_OPEN_URI (app,
					  brasero_project_manager_open,
					  gtk_recent_info_get_uri (item));
	}
    	else if (!strcmp (mime, "application/x-cd-image")
	     ||  !strcmp (mime, "application/x-cdrdao-toc")
	     ||  !strcmp (mime, "application/x-toc")) {
    		BRASERO_PROJECT_OPEN_URI (app,
					  brasero_project_manager_iso,
					  gtk_recent_info_get_uri (item));
	}
}

static void
brasero_app_add_recent (BraseroApp *app)
{
	GtkRecentManager *recent;
	GtkRecentFilter *filter;
	GtkWidget *submenu;
	GtkWidget *menu;

	/* add recent files */
	recent = gtk_recent_manager_get_default ();
 	gtk_recent_manager_set_limit (recent, 5);

	submenu = gtk_recent_chooser_menu_new_for_manager (recent);
	g_signal_connect (submenu,
                  	  "item_activated",
			  G_CALLBACK (brasero_app_recent_open),
			  app);

	filter = gtk_recent_filter_new ();
	gtk_recent_filter_set_name (filter, _("Brasero projects"));
	gtk_recent_filter_add_mime_type (filter, "application/x-brasero");
	gtk_recent_filter_add_mime_type (filter, "application/x-cd-image");
	gtk_recent_filter_add_mime_type (filter, "application/x-cdrdao-toc");
	gtk_recent_filter_add_mime_type (filter, "application/x-toc");
	gtk_recent_chooser_add_filter (GTK_RECENT_CHOOSER (submenu), filter);
	gtk_recent_chooser_set_local_only (GTK_RECENT_CHOOSER (submenu), TRUE);

	menu = gtk_ui_manager_get_widget (app->manager, "/menubar/ProjectMenu/Recent");
 	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu), submenu);    
}

static BraseroApp *
brasero_app_create_app (void)
{
	BraseroApp *app;
	GtkWidget *menubar;
	GtkActionGroup *action_group;
	GtkAccelGroup *accel_group;
	GError *error = NULL;

	/* New window */
	app = g_new0 (BraseroApp, 1);
	app->mainwin = gnome_app_new ("Brasero", NULL);

	gtk_window_set_default_icon_from_file (BRASERO_DATADIR G_DIR_SEPARATOR_S "brasero.png",
					       NULL);
	gtk_window_set_icon_from_file (GTK_WINDOW (app->mainwin),
				       BRASERO_DATADIR G_DIR_SEPARATOR_S "brasero.png",
				       NULL);

	g_signal_connect (G_OBJECT (app->mainwin), "delete-event",
			  G_CALLBACK (on_delete_cb), app);
	g_signal_connect (G_OBJECT (app->mainwin), "destroy",
			  G_CALLBACK (on_destroy_cb), app);

	/* status bar FIXME: what for ??? */
	app->statusbar = gtk_statusbar_new ();
	gnome_app_set_statusbar (GNOME_APP (app->mainwin), app->statusbar);

	/* window contents */
	app->contents = brasero_project_manager_new ();
	gtk_widget_show (app->contents);
	gnome_app_set_contents (GNOME_APP (app->mainwin), app->contents);

	/* menu and toolbar */
	app->manager = gtk_ui_manager_new ();
	brasero_project_manager_register_menu (BRASERO_PROJECT_MANAGER (app->contents),
					       app->manager);

	action_group = gtk_action_group_new ("MenuActions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (action_group,
				      entries,
				      G_N_ELEMENTS (entries),
				      app);

	gtk_ui_manager_insert_action_group (app->manager, action_group, 0);

	if (!gtk_ui_manager_add_ui_from_string (app->manager, description, -1, &error)) {
		g_message ("building menus failed: %s", error->message);
		g_error_free (error);
	}

	menubar = gtk_ui_manager_get_widget (app->manager, "/menubar");
	gnome_app_set_menus (GNOME_APP (app->mainwin), GTK_MENU_BAR (menubar));

	brasero_app_add_recent (app);
 
	/* add accelerators */
	accel_group = gtk_ui_manager_get_accel_group (app->manager);
	gtk_window_add_accel_group (GTK_WINDOW (app->mainwin), accel_group);

	/* set up the window geometry */
	gtk_window_set_position (GTK_WINDOW (app->mainwin), GTK_WIN_POS_CENTER);

	brasero_session_connect (app);
	brasero_session_load (app);

	g_signal_connect (app->mainwin,
			  "window-state-event",
			  G_CALLBACK (on_window_state_changed_cb),
			  app);
	g_signal_connect (app->mainwin,
			  "configure-event",
			  G_CALLBACK (on_configure_event_cb),
			  app);
	return app;
}

static void
brasero_app_parse_options (BraseroApp *app)
{
	gint nb = 0;

	/* we first check that only one of the options was given
	 * (except for --debug) */
	if (copy_project)
		nb ++;
	if (iso_uri)
		nb ++;
	if (project_uri)
		nb ++;
	if (audio_project)
		nb ++;
	if (data_project)
		nb ++;
	if (open_ncb)
		nb ++;

	if (nb > 1) {
		GtkWidget *message;

		message = gtk_message_dialog_new (NULL,
						  GTK_DIALOG_MODAL |
						  GTK_DIALOG_DESTROY_WITH_PARENT,
						  GTK_MESSAGE_INFO,
						  GTK_BUTTONS_CLOSE,
						  _("Incompatible command line options used:"));

		gtk_window_set_title (GTK_WINDOW (message), _("Incompatible options"));
		
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
							  _("only one option can be given at a time."));
		gtk_dialog_run (GTK_DIALOG (message));
		gtk_widget_destroy (message);

		brasero_project_manager_empty (BRASERO_PROJECT_MANAGER (app->contents));
		return;
	}

	if (copy_project) {
		/* this can't combine with all other options */
		brasero_project_manager_copy (BRASERO_PROJECT_MANAGER (app->contents));
		return;
	}

	if (iso_uri)
		BRASERO_PROJECT_OPEN_URI (app, brasero_project_manager_iso, iso_uri);

	if (project_uri)
		BRASERO_PROJECT_OPEN_URI (app, brasero_project_manager_open, project_uri);

	if (audio_project)
		BRASERO_PROJECT_OPEN_LIST (app, brasero_project_manager_audio, files);

	if (data_project)
		BRASERO_PROJECT_OPEN_LIST (app, brasero_project_manager_data, files);
	
	if (open_ncb) {
		GSList *list = NULL;
		gchar **iter;

		list = g_slist_prepend (NULL, "burn:///");

		/* in this case we can also add the files */
		for (iter = files; iter && *iter; iter ++) {
			gchar *uri;
			uri = gnome_vfs_make_uri_from_input (*iter);
			list = g_slist_prepend (list, uri);
		}

		brasero_project_manager_data (BRASERO_PROJECT_MANAGER (app->contents), list);
		g_slist_foreach (list, (GFunc) g_free, NULL);
		g_slist_free (list);
		return;
	}
	
	if (files) {
		const gchar *mime;
	    	gchar *uri;

	    	if (g_strv_length (files) > 1)
			BRASERO_PROJECT_OPEN_LIST (app, brasero_project_manager_data, files);

		/* we need to determine what type of file it is */
	    	uri = gnome_vfs_make_uri_from_input (files [0]);
		mime = gnome_vfs_get_mime_type (uri);
	    	g_free (uri);

		if (mime) {
			if (!strcmp (mime, "application/x-brasero"))
				BRASERO_PROJECT_OPEN_URI (app, brasero_project_manager_open, files [0]);

			if (!strcmp (mime, "application/x-toc")
			||  !strcmp (mime, "application/x-cdrdao-toc")
			||  !strcmp (mime, "application/x-cd-image")
			||  !strcmp (mime, "application/octet-stream"))
				BRASERO_PROJECT_OPEN_URI (app, brasero_project_manager_iso, files [0]);

			/* open it in a data project */
			BRASERO_PROJECT_OPEN_LIST (app, brasero_project_manager_data, files);
		}
		else
			BRASERO_PROJECT_OPEN_LIST (app, brasero_project_manager_data, files);
	}
	else
		brasero_project_manager_empty (BRASERO_PROJECT_MANAGER (app->contents));
}

int
main (int argc, char **argv)
{
	BraseroApp *app;
	GnomeProgram *program;
	GOptionContext *context;

	context = g_option_context_new (_("[URI] [URI] ..."));
	g_option_context_add_main_entries (context,
					   options,
					   GETTEXT_PACKAGE);

	program = gnome_program_init (PACKAGE, VERSION, LIBGNOMEUI_MODULE,
				      argc, argv,
				      GNOME_PARAM_GOPTION_CONTEXT, context,
				      GNOME_PARAM_HUMAN_READABLE_NAME, _("CD/DVD burning"),
				      NULL);

	brasero_utils_init ();

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	if (!g_thread_supported ())
		g_thread_init (NULL);

	gnome_vfs_init ();
	gst_init (&argc, &argv);

	nautilus_burn_init ();

#ifdef HAVE_LIBNOTIFY
	notify_init ("Brasero");
#endif

	client = gconf_client_get_default ();

	app = brasero_app_create_app ();
	if (app == NULL)
		return 1;

	gtk_widget_realize (app->mainwin);

	brasero_app_parse_options (app);

	gtk_widget_show (app->mainwin);

	gtk_main ();

	brasero_session_disconnect (app);
	g_object_unref (program);
	g_free (app);
	gst_deinit ();

	nautilus_burn_shutdown ();

	g_object_unref (client);
	client = NULL;

	return 0;
}
