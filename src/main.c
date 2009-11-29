/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

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

#include "brasero-medium-monitor.h"

#include "brasero-project-manager.h"
#include "brasero-multi-dnd.h"
#include "brasero-utils.h"
#include "brasero-misc.h"
#include "brasero-app.h"

#include "brasero-burn-lib.h"
#include "brasero-session.h"

#include "eggsmclient.h"

BraseroDrive *burner = NULL;

gchar *burn_project_uri;
gchar *project_uri;
gchar *cover_project;
gchar *playlist_uri;
gchar *copy_project_path;
gchar *image_project_uri;

gchar **files;

gint audio_project;
gint data_project;
gint video_project;
gint empty_project;
gint open_ncb;
gint parent_window;
gint burn_immediately;
gint disc_blank;
gint disc_check;

gboolean copy_project;
gboolean image_project;

static gboolean
brasero_main_copy_project (const gchar *option_name,
                           const gchar *value,
                           gpointer data,
                           GError **error);
static gboolean
brasero_main_image_project (const gchar *option_name,
   			    const gchar *value,
                            gpointer data,
                            GError **error);

static gboolean
brasero_main_fake_device (const gchar *option_name,
			  const gchar *value,
			  gpointer data,
			  GError **error);

static gboolean
brasero_main_burning_device (const gchar *option_name,
			     const gchar *value,
			     gpointer data,
			     GError **error);

static const GOptionEntry options [] = {
	{ "project", 'p', 0, G_OPTION_ARG_FILENAME, &project_uri,
	  N_("Open the specified project"),
	  N_("PROJECT") },

#ifdef BUILD_PLAYLIST

	 { "playlist", 'l', 0, G_OPTION_ARG_FILENAME, &playlist_uri,
	  N_("Open the specified playlist as an audio project"),
	  N_("PLAYLIST") },

#endif

	{ "device", 0, G_OPTION_FLAG_FILENAME, G_OPTION_ARG_CALLBACK, brasero_main_burning_device,
	  N_("Set the drive to be used for burning"),
	  N_("DEVICE PATH") },

	{ "image-file", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, brasero_main_fake_device,
	  N_("Create an image file instead of burning"),
	  NULL },

	{ "audio", 'a', 0, G_OPTION_ARG_NONE, &audio_project,
	  N_("Open an audio project adding the URIs given on the command line"),
	  NULL },

	{ "data", 'd', 0, G_OPTION_ARG_NONE, &data_project,
         N_("Open a data project adding the URIs given on the command line"),
          NULL },

	{ "copy", 'c', G_OPTION_FLAG_OPTIONAL_ARG|G_OPTION_FLAG_FILENAME, G_OPTION_ARG_CALLBACK, brasero_main_copy_project,
	  N_("Copy a disc"),
	  N_("PATH TO DEVICE") },

	{ "cover", 'j', 0, G_OPTION_ARG_FILENAME, &cover_project,
	  N_("Cover to use"),
	  N_("PATH TO COVER") },

	{ "video", 'o', 0, G_OPTION_ARG_NONE, &video_project,
	  N_("Open a video project adding the URIs given on the command line"),
	  NULL },

	{ "image", 'i', G_OPTION_FLAG_OPTIONAL_ARG|G_OPTION_FLAG_FILENAME, G_OPTION_ARG_CALLBACK, brasero_main_image_project,
	 N_("URI of an image file to burn (autodetected)"),
          N_("PATH TO IMAGE") },

    	{ "empty", 'e', 0, G_OPTION_ARG_NONE, &empty_project,
         N_("Force brasero to display the project selection page"),
          NULL },

	{ "blank", 'b', 0, G_OPTION_ARG_NONE, &disc_blank,
	  N_("Open the blank disc dialog"),
	  N_("PATH TO DEVICE") },

	{ "check", 'k', 0, G_OPTION_ARG_NONE, &disc_check,
	  N_("Open the check disc dialog"),
	  N_("PATH TO DEVICE") },

	{ "ncb", 'n', 0, G_OPTION_ARG_NONE, &open_ncb,
	  N_("Burn the contents of burn:// URI"),
	  NULL },

	{ "immediately", 0, 0, G_OPTION_ARG_NONE, &burn_immediately,
	  N_("Start burning immediately."),
	  NULL },

	{ "burn-and-remove-project", 'r', 0, G_OPTION_ARG_FILENAME, &burn_project_uri,
	  N_("Burn the specified project and remove it.\nThis option is mainly useful for integration with other applications."),
	  N_("PATH") },

	{ "transient-for", 'x', 0, G_OPTION_ARG_INT, &parent_window,
	/* Translators: the xid is a number identifying each window in the X11
	 * world (not Windows, MacOS X). The following sentence says that
	 * brasero will be set to be always on top of the window identified by
	 * xid. In other word, the window with the given xid will become brasero
	 * parent as if brasero was a dialog for the parent application */
	  N_("The XID of the parent window"), NULL },

	{ G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &files,
	  NULL, NULL }, /* collects file arguments */

	{ NULL }
};

static gboolean
brasero_main_fake_device (const gchar *option_name,
                          const gchar *value,
                          gpointer data,
                          GError **error)
{
	BraseroMediumMonitor *monitor;
	GSList *list;

	/* Wait for the libbrasero-media to be ready */
	monitor = brasero_medium_monitor_get_default ();
	while (brasero_medium_monitor_is_probing (monitor))
		sleep (1);

	list = brasero_medium_monitor_get_drives (monitor, BRASERO_DRIVE_TYPE_FILE);
	if (!list)
		return FALSE;

	burner = list->data;
	g_slist_free (list);

	return TRUE;
}

static gboolean
brasero_main_burning_device (const gchar *option_name,
			     const gchar *value,
			     gpointer data,
			     GError **error)
{
	BraseroDrive *burner;
	BraseroMediumMonitor *monitor;

	if (!value)
		return FALSE;

	/* Wait for the libbrasero-media to be ready */
	monitor = brasero_medium_monitor_get_default ();
	while (brasero_medium_monitor_is_probing (monitor))
		sleep (1);

	burner = brasero_medium_monitor_get_drive (monitor, value);
	g_object_unref (monitor);

	if (burner) {
		if (!brasero_drive_can_write (burner)) {
			gchar *string;

			/* Translators: %s is the path of drive */
			string = g_strdup_printf (_("\"%s\" cannot write."), value);
			brasero_utils_message_dialog (NULL,
						      string,
						      NULL,
						      GTK_MESSAGE_ERROR);

			g_object_unref (burner);
			return FALSE;
		}
	}
	else {
		gchar *string;

		/* Translators: %s is the path of a drive */
		string = g_strdup_printf (_("\"%s\" cannot be found."), value);
		brasero_utils_message_dialog (NULL,
					      string,
					      NULL,
					      GTK_MESSAGE_ERROR);
		g_free (string);
		return FALSE;
	}

	return TRUE;
}

static gboolean
brasero_main_copy_project (const gchar *option_name,
                           const gchar *value,
                           gpointer data,
                           GError **error)
{
	copy_project = TRUE;
	copy_project_path = g_strdup (value);

	return TRUE;
}

static gboolean
brasero_main_image_project (const gchar *option_name,
			    const gchar *value,
        		    gpointer data,
	                    GError **error)
{
	image_project = TRUE;
	image_project_uri = g_strdup (value);

	return TRUE;
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
		brasero_app_run_mainwin (app);
		return;
	}

	/* we first check that only one of the options was given
	 * (except for --debug, cover argument and device) */
	if (copy_project)
		nb ++;
	if (image_project)
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
	else if (project_uri) {
		brasero_app_open_project (app,
					  burner,
					  project_uri,
					  FALSE,
					  TRUE,
					  burn_immediately != 0);
		return;
	}
	else if (burn_project_uri) {
		brasero_app_open_project (app,
					  burner,
					  burn_project_uri,
					  FALSE,
					  TRUE,
					  TRUE);

		if (g_remove (burn_project_uri) != 0) {
			gchar *path;

			path = g_filename_from_uri (burn_project_uri, NULL, NULL);
			g_remove (path);
			g_free (path);
		}
		return;
	}

#ifdef BUILD_PLAYLIST

	else if (playlist_uri) {
		brasero_app_open_project (app,
					  burner,
					  playlist_uri,
					  TRUE,
					  TRUE,
					  burn_immediately != 0);
		return;
	}

#endif
	else if (copy_project) {
		brasero_app_copy_disc (app,
				       burner,
				       copy_project_path,
				       cover_project,
				       burn_immediately != 0);
		return;
	}
	else if (image_project) {
		brasero_app_image (app,
				   burner,
				   image_project_uri,
				   burn_immediately != 0);
		return;
	}
	else if (open_ncb) {
		brasero_app_burn_uri (app, burner, burn_immediately != 0);
		return;
	}
	else if (disc_blank) {
		brasero_app_blank (app, burner, burn_immediately != 0);
		return;
	}
	else if (disc_check) {
		brasero_app_check (app, burner, burn_immediately != 0);
		return;
	}
	else if (audio_project) {
		brasero_app_stream (app, burner, files, FALSE, burn_immediately != 0);
		if (burn_immediately)
			return;
	}
	else if (data_project) {
		brasero_app_data (app, burner, files, burn_immediately != 0);
		if (burn_immediately)
			return;
	}
	else if (video_project) {
		brasero_app_stream (app, burner, files, TRUE, burn_immediately != 0);
		if (burn_immediately)
			return;
	}
	else if (files) {
		if (g_strv_length (files) == 1
		&&  brasero_app_open_uri (app, files [0], FALSE))
			return;

		brasero_app_data (app, burner, files, burn_immediately != 0);
		if (burn_immediately)
			return;
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
	GOptionContext *context;

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	g_thread_init (NULL);
	g_type_init ();

	gtk_init (&argc, &argv);

	brasero_burn_library_start (&argc, &argv);

	context = g_option_context_new (_("[URI] [URI] …"));
	g_option_context_add_main_entries (context,
					   options,
					   GETTEXT_PACKAGE);
	g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);

	g_option_context_add_group (context, egg_sm_client_get_option_group ());
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_add_group (context, brasero_media_get_option_group ());
	g_option_context_add_group (context, brasero_burn_library_get_option_group ());
	g_option_context_add_group (context, gst_init_get_option_group ());
	if (g_option_context_parse (context, &argc, &argv, NULL) == FALSE) {
		g_print (_("Please type \"%s --help\" to see all available options\n"), argv [0]);
		g_option_context_free (context);
		exit (1);
	}
	g_option_context_free (context);

	brasero_enable_multi_DND ();

	current_app = brasero_app_new ();
	if (current_app == NULL)
		return 1;

	brasero_app_parse_options (current_app);

	g_object_unref (current_app);
	current_app = NULL;

	brasero_burn_library_stop ();

	gst_deinit ();

	return 0;
}

	/* REMINDER: this is done in burn library now */
/*	gst_init (&argc, &argv);
	gst_pb_utils_init ();
	client = gconf_client_get_default ();
	gconf_client_add_dir (client,
			      BRASERO_CONF_DIR,
			      GCONF_CLIENT_PRELOAD_NONE,
			      NULL);
*/
