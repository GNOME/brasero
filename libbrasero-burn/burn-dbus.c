/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-burn
 * Copyright (C) Luis Medinas 2008 <lmedinas@gmail.com>
 * Copyright (C) Philippe Rouquier 2008 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-burn is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-burn authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-burn. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-burn is distributed in the hope that it will be useful,
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <gtk/gtk.h>
#include "burn-dbus.h"

#define	GS_DBUS_SERVICE			"org.gnome.SessionManager"
#define	GS_DBUS_INHIBIT_PATH		"/org/gnome/SessionManager"
#define	GS_DBUS_INHIBIT_INTERFACE	"org.gnome.SessionManager"

static GDBusConnection *conn;

void
brasero_uninhibit_suspend (guint cookie)
{
	GError		*error = NULL;
	GVariant 	*res;

	if (cookie < 0) {
		g_warning ("Invalid cookie");
		return;
	}

	conn = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

	if (conn == NULL) {
		g_warning ("Couldn't get a DBUS connection: %s",
			    error->message);
		g_error_free (error);
		return;
	}

	res = g_dbus_connection_call_sync (conn,
					   GS_DBUS_SERVICE, 
					   GS_DBUS_INHIBIT_PATH, 
					   GS_DBUS_INHIBIT_INTERFACE,
					   "Uninhibit", 
					   g_variant_new("(u)", 
							 cookie),
					   NULL,
					   G_DBUS_CALL_FLAGS_NONE, 
					   -1,
					   NULL,
					   &error);

	if (res == NULL) {
		g_warning ("Failed to restore the system power manager: %s",
			    error->message);
		g_error_free (error);
	} else {
                g_variant_unref (res);
        }

}

gint
brasero_inhibit_suspend (const char *reason)
{
	guint	         cookie;
	GError		*error	= NULL;
	GVariant 	*res;

	g_return_val_if_fail (reason != NULL, -1);

	conn = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

	if (conn == NULL) {
		g_warning ("Couldn't get a DBUS connection: %s",
			    error->message);
		g_error_free (error);
		return -1;
	}

	res = g_dbus_connection_call_sync (conn,
					   GS_DBUS_SERVICE,
					   GS_DBUS_INHIBIT_PATH, 
					   GS_DBUS_INHIBIT_INTERFACE,
					   "Inhibit",
					   g_variant_new("(susu)",
							 g_get_application_name (),
							 0,
							 reason,
							 1 | 4),
					   G_VARIANT_TYPE ("(u)"),
					   G_DBUS_CALL_FLAGS_NONE, 
					   -1,
					   NULL,
					   &error);

	if (res == NULL) {
		g_warning ("Failed to inhibit the system from suspending: %s",
			    error->message);
		g_error_free (error);
		cookie = -1;
	}
	else {
		g_variant_get (res, "(u)", &cookie);
		g_variant_unref (res);
	}

	return cookie;
}
