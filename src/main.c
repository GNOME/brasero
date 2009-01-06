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
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>

#include <gio/gio.h>

#include <gtk/gtk.h>

#include <gconf/gconf-client.h>

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include "brasero-project-manager.h"
#include "brasero-multi-dnd.h"
#include "brasero-session.h"
#include "brasero-utils.h"
#include "brasero-app.h"
#include "burn-debug.h"
#include "burn.h"

#include "eggsmclient.h"

gchar *burn_project_uri;
gchar *project_uri;
gchar *cover_project;
gchar *playlist_uri;
gchar *iso_uri;
gchar **files;
gint audio_project;
gint data_project;
gint video_project;
gint copy_project;
gint empty_project;
gint disc_blank;
gint disc_check;
gint open_ncb;
gint parent_window;
gint debug;

#define BRASERO_CONF_DIR "/apps/brasero"

static const GOptionEntry options [] = {
	{ "project", 'p', 0, G_OPTION_ARG_STRING, &project_uri,
	  N_("Open the specified project"),
	  N_("PROJECT") },

#ifdef BUILD_PLAYLIST

	 { "playlist", 'l', 0, G_OPTION_ARG_STRING, &playlist_uri,
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
	  N_("PATH TO DEVICE") },

	{ "cover", 'j', 0, G_OPTION_ARG_STRING, &cover_project,
	  N_("Cover to use"),
	  N_("PATH TO COVER") },

	{ "video", 'o', 0, G_OPTION_ARG_NONE, &video_project,
	  N_("Open a video project adding the URIs given on the command line"),
	  NULL },

	{ "image", 'i', 0, G_OPTION_ARG_STRING, &iso_uri,
	 N_("Uri of an image file to be burnt (autodetected)"),
          N_("PATH TO PLAYLIST") },

    	{ "empty", 'e', 0, G_OPTION_ARG_NONE, &empty_project,
         N_("Force brasero to display the project selection page"),
          NULL },

	{ "blank", 'b', 0, G_OPTION_ARG_NONE, &disc_blank,
	  N_("Open the blank disc dialog"),
	  NULL },

	{ "check", 'k', 0, G_OPTION_ARG_NONE, &disc_check,
	  N_("Open the check disc dialog"),
	  NULL },

	{ "ncb", 'n', 0, G_OPTION_ARG_NONE, &open_ncb,
	  N_("Burn the contents of burn:// URI"),
	  NULL },

	{ "burn-and-remove-project", 'r', 0, G_OPTION_ARG_STRING, &burn_project_uri,
	  N_("Burn the specified project and REMOVE it.\nThis option is mainly useful for integration use with other applications."),
	  N_("PATH") },

	{ "transient-for", 'x', 0, G_OPTION_ARG_INT, &parent_window,
	/* Translators: the xid is a number identifying each window in the X11
	 * world (not Windows, MacOS X). The following sentence says that
	 * brasero will be set to be always on top of the window identified by
	 * xid. In other word, the window with the given xid will become brasero
	 * parent as if brasero was a dialog for the parent application */
	  N_("The XID of the parent window"), NULL },

	{ "debug", 'g', 0, G_OPTION_ARG_NONE, &debug,
	  N_("Display debug statements on stdout"),
	  NULL },

	{ G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &files,
	  NULL, NULL }, /* collects file arguments */

	{ NULL }
};

#define BRASERO_PROJECT_OPEN_URI(manager_MACRO, function, path)		\
{									\
	GFile *file;							\
	gchar *uri;							\
	file = g_file_new_for_commandline_arg (path);			\
	uri = g_file_get_uri (file);					\
	g_object_unref (file);						\
	function (BRASERO_PROJECT_MANAGER (manager_MACRO), uri);	\
}

#define BRASERO_PROJECT_OPEN_LIST(manager_MACRO, function, uris)		\
{										\
	GSList *list = NULL;							\
	gchar **iter;								\
	/* convert all names into a GSList * */					\
	for (iter = uris; iter && *iter; iter ++) {				\
		gchar *uri;							\
		GFile *file;							\
		file = g_file_new_for_commandline_arg (*iter);			\
		uri = g_file_get_uri (file);					\
		g_object_unref (file);						\
		list = g_slist_prepend (list, uri);				\
	}									\
	/* reverse to keep the order of files */				\
	list = g_slist_reverse (list);						\
	function (BRASERO_PROJECT_MANAGER (manager_MACRO), list);		\
	g_slist_foreach (list, (GFunc) g_free, NULL);				\
	g_slist_free (list);							\
}

static void
brasero_app_parse_options (BraseroApp *app)
{
	gint nb = 0;
	GtkWidget *manager = NULL;

	if (parent_window)
		brasero_app_set_parent (app, parent_window);

    	if (empty_project) {
	    	brasero_app_create_mainwin (app);
		manager = brasero_app_get_project_manager (app);
		brasero_project_manager_empty (BRASERO_PROJECT_MANAGER (manager));
		return;
	}

	/* we first check that only one of the options was given
	 * (except for --debug and cover argument) */
	if (copy_project)
		nb ++;
	if (iso_uri)
		nb ++;
	if (project_uri)
		nb ++;
	if (burn_project_uri)
		nb ++;
	if (playlist_uri)
		nb ++;
	if (audio_project)
		nb ++;
	if (data_project)
		nb ++;
	if (video_project)
	    	nb ++;
	if (disc_blank)
	  	nb ++;
	if (open_ncb)
		nb ++;

	if (nb > 1) {
		brasero_app_create_mainwin (app);

		brasero_app_alert (app,
				   _("Incompatible command line options used."),
				   _("Only one option can be given at a time"),
				   GTK_MESSAGE_ERROR);

		manager = brasero_app_get_project_manager (app);
		brasero_project_manager_empty (BRASERO_PROJECT_MANAGER (manager));
	}
	else if (copy_project) {
		gchar *device = NULL;

		/* Make sure there is only one file in the remaining list for
		* specifying the source device. It could be extended to let
		* the user specify the destination device as well */
		if (files
		&&  files [0] != NULL
		&&  files [1] == NULL)
			device = files [0]; 

		brasero_app_copy_disc (app, device, cover_project);
		return;
	}
	else if (iso_uri) {
		GFile *file;
		gchar *uri;

		file = g_file_new_for_commandline_arg (iso_uri);
		uri = g_file_get_uri (file);
		g_object_unref (file);

		brasero_app_burn_image (app, uri);
		return;
	}
	else if (project_uri) {
		brasero_app_create_mainwin (app);
		manager = brasero_app_get_project_manager (app);
		brasero_project_manager_set_oneshot (BRASERO_PROJECT_MANAGER (manager), TRUE);
		BRASERO_PROJECT_OPEN_URI (manager, brasero_project_manager_open_project, project_uri);
	}
	else if (burn_project_uri) {
		brasero_app_create_mainwin (app);
		manager = brasero_app_get_project_manager (app);
		brasero_project_manager_set_oneshot (BRASERO_PROJECT_MANAGER (manager), TRUE);
		BRASERO_PROJECT_OPEN_URI (manager, brasero_project_manager_burn_project, burn_project_uri);
		if (g_remove (burn_project_uri) != 0) {
			gchar *path;

			path = g_filename_from_uri (burn_project_uri, NULL, NULL);
			g_remove (path);
			g_free (path);
		}
		return;
	}
	else if (open_ncb) {
		GFileEnumerator *enumerator;
		GFileInfo *info = NULL;
		GError *error = NULL;
		GSList *list = NULL;
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

			if (error)
				string = g_strdup_printf (_("An internal error occured (%s)"), error->message);
			else
				string = g_strdup (_("An internal error occured"));

			brasero_app_alert (app,
					   _("Error while loading the project."),
					   string,
					   GTK_MESSAGE_ERROR);

			g_free (string);
			g_object_unref (file);
			return;
		}

		while ((info = g_file_enumerator_next_file (enumerator, NULL, &error)))
			list = g_slist_prepend (list, g_strconcat ("burn:///", g_file_info_get_name (info), NULL));

		g_object_unref (enumerator);
		g_object_unref (file);

		if (error) {
			gchar *string;

			if (error)
				string = g_strdup_printf (_("An internal error occured (%s)"), error->message);
			else
				string = g_strdup (_("An internal error occured"));

			brasero_app_alert (app,
					   _("Error while loading the project."),
					   string,
					   GTK_MESSAGE_ERROR);

			g_free (string);
			g_object_unref (file);

			g_slist_foreach (list, (GFunc) g_free, NULL);
			g_slist_free (list);
			return;
		}

		if (!list) {
			brasero_app_alert (app,
					   _("Please add files to the project."),
					   _("The project is empty"),
					   GTK_MESSAGE_ERROR);
			return;
		}

		/* reverse to keep the order of files */
		list = g_slist_reverse (list);
		brasero_app_create_mainwin (app);
		manager = brasero_app_get_project_manager (app);
		brasero_project_manager_set_oneshot (BRASERO_PROJECT_MANAGER (manager), TRUE);
		brasero_project_manager_data (BRASERO_PROJECT_MANAGER (manager), list);

		g_slist_foreach (list, (GFunc) g_free, NULL);
		g_slist_free (list);
		return;
	}

#ifdef BUILD_PLAYLIST

	else if (playlist_uri) {
		brasero_app_create_mainwin (app);
		manager = brasero_app_get_project_manager (app);
		BRASERO_PROJECT_OPEN_URI (manager, brasero_project_manager_open_playlist, playlist_uri);
	}

#endif

	else if (audio_project) {
		brasero_app_create_mainwin (app);
		manager = brasero_app_get_project_manager (app);
		BRASERO_PROJECT_OPEN_LIST (manager, brasero_project_manager_audio, files);
	}
	else if (data_project) {
		brasero_app_create_mainwin (app);
		manager = brasero_app_get_project_manager (app);
		BRASERO_PROJECT_OPEN_LIST (manager, brasero_project_manager_data, files);
	}
	else if (video_project) {
		brasero_app_create_mainwin (app);
		manager = brasero_app_get_project_manager (app);
	    	BRASERO_PROJECT_OPEN_LIST (manager, brasero_project_manager_video, files);
	}
	else if (disc_blank) {
		gchar *device = NULL;

		/* make sure there is only one file in the remaining list for
		 * specifying the source device. It could be extended to let
		 * the user specify the destination device as well */
		if (files
		&&  files [0] != NULL
		&&  files [1] == NULL)
			device = files [0];

		brasero_app_blank (app, device);
		return;
	}
	else if (disc_check) {
		gchar *device = NULL;

		/* make sure there is only one file in the remaining list for
		 * specifying the source device. It could be extended to let
		 * the user specify the destination device as well */
		if (files
		&&  files [0] != NULL
		&&  files [1] == NULL)
			device = files [0];

		brasero_app_check (app, device);
		return;
	}
	else if (files) {
		brasero_app_create_mainwin (app);
		manager = brasero_app_get_project_manager (app);

		if (g_strv_length (files) == 1) {
			BraseroProjectType type;

			brasero_project_manager_set_oneshot (BRASERO_PROJECT_MANAGER (manager), TRUE);
			type = brasero_project_manager_open_uri (BRASERO_PROJECT_MANAGER (manager), files [0]);

			/* Fallback if it hasn't got a suitable URI */
			if (type == BRASERO_PROJECT_TYPE_INVALID)
				BRASERO_PROJECT_OPEN_LIST (manager, brasero_project_manager_data, files);
		}
		else {
			brasero_project_manager_set_oneshot (BRASERO_PROJECT_MANAGER (manager), TRUE);
			BRASERO_PROJECT_OPEN_LIST (manager, brasero_project_manager_data, files);
		}
	}
	else {
		brasero_app_create_mainwin (app);
		manager = brasero_app_get_project_manager (app);
		brasero_project_manager_empty (BRASERO_PROJECT_MANAGER (manager));
	}

	brasero_app_run_mainwin (app);
}

static BraseroApp *current_app = NULL;

/**
 * This is actually declared in brasero-app.h
 */

BraseroApp *
brasero_app_get_default (void)
{
	return current_app;
}

int
main (int argc, char **argv)
{
	GConfClient *client;
	GOptionContext *context;


#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	g_thread_init (NULL);
	g_type_init ();

	context = g_option_context_new (_("[URI] [URI] ..."));
	g_option_context_add_main_entries (context,
					   options,
					   GETTEXT_PACKAGE);
	g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);

	g_option_context_add_group (context, egg_sm_client_get_option_group ());
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_add_group (context, gst_init_get_option_group ());
	if (g_option_context_parse (context, &argc, &argv, NULL) == FALSE) {
		g_print (_("Please type %s --help to see all available options\n"), argv [0]);
		g_option_context_free (context);
		exit (1);
	}

	g_option_context_free (context);

	gst_init (&argc, &argv);

	/* This is for missing codec automatic install */
	gst_pb_utils_init ();

	client = gconf_client_get_default ();
	gconf_client_add_dir (client,
			      BRASERO_CONF_DIR,
			      GCONF_CLIENT_PRELOAD_NONE,
			      NULL);

	brasero_burn_set_debug (debug);
	brasero_burn_library_init ();

	brasero_enable_multi_DND ();
	brasero_utils_init ();

	current_app = brasero_app_new ();
	if (current_app == NULL)
		return 1;

	brasero_app_parse_options (current_app);
	current_app = NULL;

	brasero_burn_library_shutdown ();

	gconf_client_remove_dir (client, BRASERO_CONF_DIR, NULL);
	g_object_unref (client);

	gst_deinit ();

	return 0;
}
