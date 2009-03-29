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
 *            utils.c
 *
 *  Wed May 18 16:58:16 2005
 *  Copyright  2005  Philippe Rouquier
 *  <brasero-app@wanadoo.fr>
 ****************************************************************************/


#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtk.h>

#include "brasero-utils.h"
#include "brasero-app.h"

#define BRASERO_ERROR brasero_error_quark()

static GHashTable *stringsH = NULL;
G_LOCK_DEFINE_STATIC (stringsH);

GQuark
brasero_error_quark (void)
{
	static GQuark quark = 0;

	if (!quark)
		quark = g_quark_from_static_string ("BraSero_error");

	return quark;
}

static gboolean
brasero_utils_clear_strings_cb (gchar *string,
				guint ref,
				gpointer NULL_data)
{
	g_free (string);
	return TRUE;
}

static void
brasero_utils_free (void)
{
	if (stringsH) {
		G_LOCK (stringsH);
		g_hash_table_foreach_remove (stringsH,
					     (GHRFunc) brasero_utils_clear_strings_cb,
					     NULL);
		g_hash_table_destroy (stringsH);
		stringsH = NULL;
		G_UNLOCK (stringsH);
	}
}

void
brasero_utils_init (void)
{
	g_atexit (brasero_utils_free);
}

/**
 * Allows multiple uses of the same string
 */

gchar *
brasero_utils_register_string (const gchar *string)
{
	gboolean success;
	gpointer key, reftmp;
	guint ref;

	if (!string) {
		g_warning ("Null string to be registered");
		return NULL;
	}

	G_LOCK (stringsH);

	if (!stringsH) {
		stringsH = g_hash_table_new (g_str_hash, g_str_equal);
		success = FALSE;
	}
	else
		success = g_hash_table_lookup_extended (stringsH,
							string,
							&key,
							&reftmp);

	if (!success) {
		key = g_strdup (string);
		g_hash_table_insert (stringsH,
				     key,
				     GINT_TO_POINTER (1));
		G_UNLOCK (stringsH);
		return key;
	}

	ref = GPOINTER_TO_INT(reftmp) + 1;
	g_hash_table_insert (stringsH,
			     key,
			     GINT_TO_POINTER (ref));

	G_UNLOCK (stringsH);
	return key;
}

void
brasero_utils_unregister_string (const gchar *string)
{
	gboolean success;
	gpointer key, reftmp;
	guint ref;

	if (!string) {
		g_warning ("Null string to be unregistered");
		return;
	}

	G_LOCK (stringsH);

	if (!stringsH) {
		G_UNLOCK (stringsH);
		return;
	}

	success = g_hash_table_lookup_extended (stringsH,
						string,
						&key,
						&reftmp);
	if (!success) {
		G_UNLOCK (stringsH);
		return;
	}

	ref = GPOINTER_TO_INT(reftmp) - 1;

	if (ref > 0)
		g_hash_table_insert (stringsH, key, GINT_TO_POINTER (ref));
	else if (ref <= 0) {
		g_hash_table_remove (stringsH, string);
		g_free (key);
	}

	G_UNLOCK (stringsH);
}

void
brasero_utils_launch_app (GtkWidget *widget,
			  GSList *list)
{
	GSList *item;

	for (item = list; item; item = item->next) {
		GError *error;
		gchar *uri;

		error = NULL;
		uri = item->data;

		if (!g_app_info_launch_default_for_uri (uri, NULL, &error)) {
			gchar *string;

			string = g_strdup_printf ("\"%s\" could not be opened", uri);
			brasero_app_alert (brasero_app_get_default (),
					   string,
					   error->message,
					   GTK_MESSAGE_ERROR);
			g_free (string);
			g_error_free (error);
			continue;
		}
	}
}
