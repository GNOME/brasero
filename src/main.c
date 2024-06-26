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

int
main (int argc, char **argv)
{
	int result;
	g_autoptr (GApplication) app = NULL;

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	/* Though we use gtk_get_option_group we nevertheless want gtk+ to be
	 * in a usable state to display our error messages while brasero
	 * specific options are parsed. Otherwise on error that crashes. */
	gtk_init (&argc, &argv);

	brasero_burn_library_start (&argc, &argv);
	brasero_enable_multi_DND ();

	app = brasero_app_new ();
	result = brasero_app_run (BRASERO_APP (app), argc, argv);

	brasero_burn_library_stop ();

	gst_deinit ();

	return result;
}
