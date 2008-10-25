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

#include "brasero-project-manager.h"
#include "brasero-multi-dnd.h"
#include "brasero-session.h"
#include "brasero-utils.h"
#include "brasero-app.h"
#include "burn-debug.h"
#include "burn.h"

gchar *project_uri;
gchar *playlist_uri;
gchar *iso_uri;
gchar **files;
gchar **audio_project;
gchar **data_project;
gint copy_project;
gint empty_project;
gint disc_blank;
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
	GtkWidget *manager;
    	gboolean load_default_project = FALSE;

	manager = brasero_app_get_project_manager (app);

    	if (empty_project) {
		brasero_project_manager_empty (BRASERO_PROJECT_MANAGER (manager));
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

		brasero_project_manager_empty (BRASERO_PROJECT_MANAGER (manager));
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
		brasero_project_manager_set_oneshot (BRASERO_PROJECT_MANAGER (manager), TRUE);
		brasero_project_manager_copy (BRASERO_PROJECT_MANAGER (manager), device);
	}
	else if (iso_uri) {
		brasero_project_manager_set_oneshot (BRASERO_PROJECT_MANAGER (manager), TRUE);
		BRASERO_PROJECT_OPEN_URI (manager, brasero_project_manager_iso, iso_uri);
	}
	else if (project_uri) {
		brasero_project_manager_set_oneshot (BRASERO_PROJECT_MANAGER (manager), TRUE);
		BRASERO_PROJECT_OPEN_URI (manager, brasero_project_manager_open_project, project_uri);
	}

#ifdef BUILD_PLAYLIST

	else if (playlist_uri) {
		brasero_project_manager_set_oneshot (BRASERO_PROJECT_MANAGER (manager), TRUE);
		BRASERO_PROJECT_OPEN_URI (manager, brasero_project_manager_open_playlist, playlist_uri);
	}

#endif

	else if (audio_project) {
		BRASERO_PROJECT_OPEN_LIST (manager, brasero_project_manager_audio, files);
	}
	else if (data_project) {
		BRASERO_PROJECT_OPEN_LIST (manager, brasero_project_manager_data, files);
	}
	else if (disc_blank) {
		brasero_app_blank (app);
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

		/* reverse to keep the order of files */
		list = g_slist_reverse (list);
		brasero_project_manager_set_oneshot (BRASERO_PROJECT_MANAGER (manager), TRUE);
		brasero_project_manager_data (BRASERO_PROJECT_MANAGER (manager), list);
		g_slist_free (list);
	}
	else if (files) {
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
		brasero_project_manager_empty (BRASERO_PROJECT_MANAGER (manager));
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

	GtkWidget *app;
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

	app = brasero_app_new ();
	if (app == NULL)
		return 1;

	gtk_widget_realize (app);

	brasero_app_parse_options (BRASERO_APP (app));

	gtk_widget_show (app);

	gtk_main ();

	brasero_burn_library_shutdown ();

#ifdef BUILD_GNOME2

	g_object_unref (program);

#endif

	gst_deinit ();

	return 0;
}
