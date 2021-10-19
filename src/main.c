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
#include <glib/gi18n-lib.h>

#include <gtk/gtk.h>

#include <gst/gst.h>


#include "brasero-burn-lib.h"
#include "brasero-misc.h"

#include "brasero-multi-dnd.h"
#include "brasero-app.h"
#include "brasero-cli.h"

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
	GApplication *gapp = NULL;
	GOptionContext *context;

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	/* Though we use gtk_get_option_group we nevertheless want gtk+ to be
	 * in a usable state to display our error messages while brasero
	 * specific options are parsed. Otherwise on error that crashes. */
	gtk_init (&argc, &argv);

	memset (&cmd_line_options, 0, sizeof (cmd_line_options));

	context = g_option_context_new (_("[URI] [URI] …"));
	g_option_context_add_main_entries (context,
					   prog_options,
					   GETTEXT_PACKAGE);
	g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);

	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_add_group (context, brasero_media_get_option_group ());
	g_option_context_add_group (context, brasero_burn_library_get_option_group ());
	g_option_context_add_group (context, brasero_utils_get_option_group ());
	g_option_context_add_group (context, gst_init_get_option_group ());
	if (g_option_context_parse (context, &argc, &argv, NULL) == FALSE) {
		g_print (_("Please type \"%s --help\" to see all available options\n"), argv [0]);
		g_option_context_free (context);
		return FALSE;
	}
	g_option_context_free (context);

	if (cmd_line_options.not_unique == FALSE) {
		GError *error = NULL;
		/* Create GApplication and check if there is a process running already */
		gapp = g_application_new ("org.gnome.Brasero", G_APPLICATION_FLAGS_NONE);

		if (!g_application_register (gapp, NULL, &error)) {
			g_warning ("Brasero registered");
			g_error_free (error);
			return 1;
		}

		if (g_application_get_is_remote (gapp)) {
			g_warning ("An instance of Brasero is already running, exiting");
			return 0;
		}
	}

	brasero_burn_library_start (&argc, &argv);
	brasero_enable_multi_DND ();

	current_app = brasero_app_new (gapp);
	if (current_app == NULL)
		return 1;

	brasero_cli_apply_options (current_app);

	g_object_unref (current_app);
	current_app = NULL;

	brasero_burn_library_stop ();

	gst_deinit ();

	return 0;
}
