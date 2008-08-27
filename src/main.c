/*
 * Brasero is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * Brasero is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */
/***************************************************************************
 *            main.c
 *
 *  Sat Jun 11 12:00:29 2005
 *  Copyright  2005  Philippe Rouquier	
 *  <brasero-app@wanadoo.fr>
 ****************************************************************************/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <locale.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <gio/gio.h>

#include <gtk/gtk.h>

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#ifdef BUILD_GNOME2

#include <libgnome/gnome-help.h>
#include <libgnomeui/libgnomeui.h>

#endif

#include "brasero-app.h"
#include "brasero-menu.h"
#include "brasero-jacket-edit.h"
#include "brasero-multi-dnd.h"
#include "brasero-blank-dialog.h"
#include "brasero-sum-dialog.h"
#include "brasero-eject-dialog.h"
#include "brasero-session.h"
#include "brasero-project-manager.h"
#include "burn-drive.h"
#include "brasero-pref.h"
#include "burn-debug.h"
#include "burn.h"
#include "burn-caps.h"
#include "burn-plugin-manager.h"

gchar *project_uri;
gchar *playlist_uri;
gchar *iso_uri;
gchar **files;
gchar **audio_project;
gchar **data_project;
gint copy_project;
gint empty_project;
gint disc_blank;
gint is_escaped;
gint open_ncb;
gint debug;

static const GOptionEntry options [] = {
	{ "project", 'p', G_OPTION_FLAG_FILENAME, G_OPTION_ARG_STRING, &project_uri,
	  N_("Open the specified project"),
	  N_("PROJECT") },

#ifdef BUILD_PLAYLIST

	 { "playlist", 'l', G_OPTION_FLAG_FILENAME, G_OPTION_ARG_STRING, &playlist_uri,
	  N_("Open the specified playlist as an audio project"),
	  N_("PLAYLIST") },

#endif

	{ "audio", 'a', 0, G_OPTION_ARG_NONE, &audio_project,
	  N_("Open an audio project adding the URIs given on the command line"),
	  NULL },

	{ "data", 'd', 0, G_OPTION_ARG_NONE, &data_project,
         N_("Open a data project adding the URIs given on the command line"),
          NULL },

	{ "copy", 'c', 0, G_OPTION_ARG_NONE, &copy_project,
	  N_("Copy a disc"),
	  NULL },

	/* FIXME: last argument should be defined */
	{ "image", 'i', G_OPTION_FLAG_FILENAME, G_OPTION_ARG_STRING, &iso_uri,
	 N_("Uri of an image file to be burnt (autodetected)"),
          NULL },

    	{ "empty", 'e', 0, G_OPTION_ARG_NONE, &empty_project,
         N_("Force brasero to display the project selection page"),
          NULL },

	{ "blank", 'b', 0, G_OPTION_ARG_NONE, &disc_blank,
	  N_("Open the blank disc dialog"),
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
	GFile *file;							\
	gchar *uri;							\
	file = g_file_new_for_commandline_arg (path);			\
	uri = g_file_get_uri (file);					\
	g_object_unref (file);						\
	function (BRASERO_PROJECT_MANAGER (app->contents), uri);	\
}

#define BRASERO_PROJECT_OPEN_LIST(app, function, uris)	\
{						\
	GSList *list = NULL;			\
	gchar **iter;				\
	/* convert all names into a GSList * */	\
	for (iter = uris; iter && *iter; iter ++) {				\
		gchar *uri;							\
		GFile *file;							\
		file = g_file_new_for_commandline_arg (*iter);			\
		uri = g_file_get_uri (file);					\
		g_object_unref (file);						\
		list = g_slist_prepend (list, uri);				\
	}									\
	function (BRASERO_PROJECT_MANAGER (app->contents), list);		\
	g_slist_foreach (list, (GFunc) g_free, NULL);				\
	g_slist_free (list);							\
}

static gboolean
on_delete_cb (GtkWidget *window, GdkEvent *event, BraseroApp *app)
{
	return brasero_session_save (app, TRUE, TRUE);
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
	if (brasero_session_save (app, TRUE, TRUE))
		return;

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

	gtk_widget_show (dialog);
}

void
on_eject_cb (GtkAction *action, BraseroApp *app)
{
	GtkWidget *dialog;
	GtkWidget *toplevel;

	dialog = brasero_eject_dialog_new();
	toplevel = gtk_widget_get_toplevel (app->mainwin);

	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);

	gtk_widget_show (dialog);
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

	gtk_widget_show (dialog);
}

void
on_prefs_cb (GtkAction *action, BraseroApp *app)
{
	GtkWidget *dialog;
	GtkWidget *toplevel;

	dialog = brasero_pref_new ();
	toplevel = gtk_widget_get_toplevel (app->mainwin);

	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);

	gtk_widget_show_all (dialog);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

void
on_disc_info_cb (GtkAction *button, BraseroApp *app)
{

}

void
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

	gtk_show_about_dialog (GTK_WINDOW (app->mainwin),
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

#ifdef BUILD_GNOME2

void
on_help_cb (GtkAction *action, BraseroApp *app)
{
	GError *error = NULL;

 	gnome_help_display ("brasero.xml",
			     NULL,
			     &error);
   	if (error) {
		GtkWidget *d;
        
		d = gtk_message_dialog_new (GTK_WINDOW (app->mainwin),
					    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					    GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
					    error->message);
		gtk_dialog_run (GTK_DIALOG(d));
		gtk_widget_destroy (d);
		g_error_free (error);
		error = NULL;
	}
}

#endif

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

	brasero_project_manager_open_by_mime (BRASERO_PROJECT_MANAGER (app->contents), uri, mime);
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

	gtk_recent_filter_set_name (filter, _("Brasero projects"));
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
	GtkAction *action;
	gchar *message;

	action = g_object_get_data (G_OBJECT (proxy),  "gtk-action");
	g_return_if_fail (action != NULL);

	g_object_get (G_OBJECT (action), "tooltip", &message, NULL);
	if (message) {
		gtk_statusbar_push (GTK_STATUSBAR (app->statusbar),
				    app->tooltip_ctx,
				    message);
		g_free (message);
	}
}

static void
brasero_menu_item_deselected_cb (GtkMenuItem *proxy,
				 BraseroApp *app)
{
	gtk_statusbar_pop (GTK_STATUSBAR (app->statusbar),
			   app->tooltip_ctx);
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
	BraseroBurnCaps *caps;
	GtkWidget *widget;

	caps = brasero_burn_caps_get_default ();
	widget = gtk_ui_manager_get_widget (app->manager, "/menubar/ToolMenu/Check");

	if (!brasero_burn_caps_can_checksum (caps))
		gtk_widget_set_sensitive (widget, FALSE);
	else
		gtk_widget_set_sensitive (widget, TRUE);

	g_object_unref (caps);
}

static BraseroApp *
brasero_app_create_app (void)
{
	BraseroApp *app;
	GtkWidget *menubar;
	GtkWidget *contents;
	GError *error = NULL;
	BraseroBurnCaps *caps;
	GtkAccelGroup *accel_group;
	GtkActionGroup *action_group;
	BraseroPluginManager *plugin_manager;

	/* New window */
	app = g_new0 (BraseroApp, 1);
	app->mainwin = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	g_set_application_name (_("Brasero Disc Burning"));

	gtk_window_set_default_icon_name ("brasero");
	gtk_window_set_icon_name (GTK_WINDOW (app->mainwin), "brasero");

	g_signal_connect (G_OBJECT (app->mainwin), "delete-event",
			  G_CALLBACK (on_delete_cb), app);
	g_signal_connect (G_OBJECT (app->mainwin), "destroy",
			  G_CALLBACK (on_destroy_cb), app);

	/* contents */
	contents = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (contents);

	gtk_container_add (GTK_CONTAINER (app->mainwin), contents);

	/* menu and toolbar */
	app->manager = gtk_ui_manager_new ();
	g_signal_connect (app->manager,
			  "connect-proxy",
			  G_CALLBACK (brasero_connect_ui_manager_proxy_cb),
			  app);
	g_signal_connect (app->manager,
			  "disconnect-proxy",
			  G_CALLBACK (brasero_disconnect_ui_manager_proxy_cb),
			  app);

	action_group = gtk_action_group_new ("MenuActions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (action_group,
				      entries,
				      G_N_ELEMENTS (entries),
				      app);

	gtk_ui_manager_insert_action_group (app->manager, action_group, 0);

	brasero_app_add_recent (app, action_group);

	if (!gtk_ui_manager_add_ui_from_string (app->manager, description, -1, &error)) {
		g_message ("building menus failed: %s", error->message);
		g_error_free (error);
	}

	menubar = gtk_ui_manager_get_widget (app->manager, "/menubar");
	gtk_box_pack_start (GTK_BOX (contents), menubar, FALSE, FALSE, 0);

	/* window contents */
	app->contents = brasero_project_manager_new ();
	gtk_widget_show (app->contents);

	gtk_box_pack_start (GTK_BOX (contents), app->contents, TRUE, TRUE, 0);

	/* status bar to display the size of selected files */
	app->statusbar = gtk_statusbar_new ();
	gtk_widget_show (app->statusbar);
	app->tooltip_ctx = gtk_statusbar_get_context_id (GTK_STATUSBAR (app->statusbar), "tooltip_info");
	gtk_box_pack_end (GTK_BOX (contents), app->statusbar, FALSE, FALSE, 0);

	/* Update everything */
	brasero_project_manager_set_status (BRASERO_PROJECT_MANAGER (app->contents),
					    app->statusbar);
	brasero_project_manager_register_ui (BRASERO_PROJECT_MANAGER (app->contents),
					     app->manager);

	gtk_ui_manager_ensure_update (app->manager);

	/* check if we can use checksums (we need plugins enabled) */
	caps = brasero_burn_caps_get_default ();
	if (!brasero_burn_caps_can_checksum (caps)) {
		GtkWidget *widget;

		widget = gtk_ui_manager_get_widget (app->manager, "/menubar/ToolMenu/Check");
		gtk_widget_set_sensitive (widget, FALSE);
	}
	g_object_unref (caps);

	plugin_manager = brasero_plugin_manager_get_default ();
	g_signal_connect (plugin_manager,
			  "caps-changed",
			  G_CALLBACK (brasero_caps_changed_cb),
			  app);

	/* add accelerators */
	accel_group = gtk_ui_manager_get_accel_group (app->manager);
	gtk_window_add_accel_group (GTK_WINDOW (app->mainwin), accel_group);

	/* set up the window geometry */
	gtk_window_set_position (GTK_WINDOW (app->mainwin), GTK_WIN_POS_CENTER);

#ifdef BUILD_GNOME2

	brasero_session_connect (app);

#endif

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
    	gboolean load_default_project = FALSE;

    	if (empty_project) {
		brasero_project_manager_empty (BRASERO_PROJECT_MANAGER (app->contents));
	    	brasero_session_load (app, FALSE);
		return;
	}

	/* we first check that only one of the options was given
	 * (except for --debug) */
	if (copy_project)
		nb ++;
	if (iso_uri)
		nb ++;
	if (project_uri)
		nb ++;
	if (playlist_uri)
		nb ++;
	if (audio_project)
		nb ++;
	if (data_project)
		nb ++;
	if (disc_blank)
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

		gtk_window_set_title (GTK_WINDOW (message), _("Incompatible Options"));
		
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
							  _("only one option can be given at a time."));
		gtk_dialog_run (GTK_DIALOG (message));
		gtk_widget_destroy (message);

		brasero_project_manager_empty (BRASERO_PROJECT_MANAGER (app->contents));
	}
	else if (copy_project) {
		gchar *device = NULL;

		/* make sure there is only one file in the remaining list for
		 * specifying the source device. It could be extended to let
		 * the user specify the destination device as well */
		if (files
		&&  files [0] != NULL
		&&  files [1] == NULL)
			device = files [0];
		
		/* this can't combine with any other options */
		brasero_project_manager_copy (BRASERO_PROJECT_MANAGER (app->contents), device);
	}
	else if (iso_uri) {
		BRASERO_PROJECT_OPEN_URI (app, brasero_project_manager_iso, iso_uri);
	}
	else if (project_uri) {
		BRASERO_PROJECT_OPEN_URI (app, brasero_project_manager_open_project, project_uri);
	}

#ifdef BUILD_PLAYLIST

	else if (playlist_uri) {
		BRASERO_PROJECT_OPEN_URI (app, brasero_project_manager_open_playlist, playlist_uri);
	}

#endif

	else if (audio_project) {
		BRASERO_PROJECT_OPEN_LIST (app, brasero_project_manager_audio, files);
	}
	else if (data_project) {
		BRASERO_PROJECT_OPEN_LIST (app, brasero_project_manager_data, files);
	}
	else if (disc_blank) {
		on_erase_cb (NULL, app);
	}
	else if (open_ncb) {
		GSList *list = NULL;
		gchar **iter;

		list = g_slist_prepend (NULL, "burn:///");

		/* in this case we can also add the files */
		for (iter = files; iter && *iter; iter ++) {
			GFile *file;
			gchar *uri;

			file = g_file_new_for_commandline_arg (*iter);
			uri = g_file_get_uri (file);
			g_object_unref (file);

			list = g_slist_prepend (list, file);
		}

		brasero_project_manager_data (BRASERO_PROJECT_MANAGER (app->contents), list);
		g_slist_free (list);
	}
	else if (files) {
		BraseroProjectType type;

	    	if (g_strv_length (files) > 1)
			BRASERO_PROJECT_OPEN_LIST (app, brasero_project_manager_data, files);

		type = brasero_project_manager_open_uri (BRASERO_PROJECT_MANAGER (app->contents), files [0]);
		if (type == BRASERO_PROJECT_TYPE_INVALID)
			BRASERO_PROJECT_OPEN_LIST (app, brasero_project_manager_data, files);
	}
	else {
		brasero_project_manager_empty (BRASERO_PROJECT_MANAGER (app->contents));
	    	load_default_project = TRUE;
	}

    	brasero_session_load (app, load_default_project);
}

int
main (int argc, char **argv)
{

#ifdef BUILD_GNOME2
	GnomeProgram *program;
#endif

	BraseroApp *app;
	GOptionContext *context;

	context = g_option_context_new (_("[URI] [URI] ..."));
	g_option_context_add_main_entries (context,
					   options,
					   GETTEXT_PACKAGE);

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	if (!g_thread_supported ())
		g_thread_init (NULL);

	g_type_init ();

#ifdef BUILD_GNOME2

	program = gnome_program_init (PACKAGE, VERSION, LIBGNOMEUI_MODULE,
				      argc, argv,
				      GNOME_PARAM_GOPTION_CONTEXT, context,
				      GNOME_PARAM_APP_DATADIR, PACKAGE_DATA_DIR,
				      GNOME_PARAM_HUMAN_READABLE_NAME, _("CD/DVD burning"),
				      NULL);

#else

	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	if (g_option_context_parse (context, &argc, &argv, NULL) == FALSE) {
		g_print (_("Please type %s --help to see all available options\n"), argv [0]);
		exit (1);
	}

#endif

	gst_init (&argc, &argv);

	/* This is for missing codec automatic install */
	gst_pb_utils_init ();

	brasero_burn_set_debug (debug);
	brasero_burn_library_init ();

	brasero_enable_multi_DND ();
	brasero_utils_init ();
	
	app = brasero_app_create_app ();
	if (app == NULL)
		return 1;

	gtk_widget_realize (app->mainwin);

	brasero_app_parse_options (app);

	gtk_widget_show (app->mainwin);

	gtk_main ();

	brasero_burn_library_shutdown ();

#ifdef BUILD_GNOME2

	brasero_session_disconnect (app);
	g_object_unref (program);

#endif

	g_free (app);
	gst_deinit ();

	return 0;
}
