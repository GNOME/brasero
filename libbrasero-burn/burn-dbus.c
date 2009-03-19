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
#include <dbus/dbus-glib.h>
#include "burn-dbus.h"

#define	GPM_DBUS_SERVICE		"org.freedesktop.PowerManagement"
#define	GPM_DBUS_INHIBIT_PATH		"/org/freedesktop/PowerManagement/Inhibit"
#define	GPM_DBUS_INHIBIT_INTERFACE	"org.freedesktop.PowerManagement.Inhibit"

void 
brasero_uninhibit_suspend (guint cookie)
{
	DBusGProxy	*proxy;
	gboolean	res;
	GError		*error = NULL;
	DBusGConnection *conn	= NULL;

	if (cookie < 0) {
		g_warning ("Invalid cookie");
		return;
	}

	conn = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (!conn) {
		g_warning ("Couldn't get a DBUS connection: %s",
			    error->message);
		g_error_free (error);
		return;
	}

	proxy = dbus_g_proxy_new_for_name (conn,
					   GPM_DBUS_SERVICE,
					   GPM_DBUS_INHIBIT_PATH,
					   GPM_DBUS_INHIBIT_INTERFACE);
	if (proxy == NULL) {
		g_warning ("Could not get DBUS proxy: %s", GPM_DBUS_SERVICE);
		dbus_g_connection_unref (conn);
		return;
	}

	res = dbus_g_proxy_call (proxy,
				 "UnInhibit", &error,
				 G_TYPE_UINT, cookie,
				 G_TYPE_INVALID,
				 G_TYPE_INVALID);
	if (!res) {
		g_warning ("Failed to restore the system power manager: %s",
			    error->message);
		g_error_free (error);
	}

	g_object_unref (G_OBJECT (proxy));
	dbus_g_connection_unref (conn);
}

gint
brasero_inhibit_suspend (const char *reason)
{
	DBusGProxy	*proxy;
	guint	         cookie;
	gboolean	 res;
	GError		*error	= NULL;
	DBusGConnection *conn	= NULL;

	g_return_val_if_fail (reason != NULL, -1);

	conn = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (!conn) {
		g_warning ("Couldn't get a DBUS connection: %s",
			    error->message);
		g_error_free (error);
		return -1;
	}

	proxy = dbus_g_proxy_new_for_name (conn,
					   GPM_DBUS_SERVICE,
					   GPM_DBUS_INHIBIT_PATH,
					   GPM_DBUS_INHIBIT_INTERFACE);
	
	if (proxy == NULL) {
		g_warning ("Could not get DBUS proxy: %s", GPM_DBUS_SERVICE);
		return -1;
	}

	res = dbus_g_proxy_call (proxy,
				 "Inhibit", &error,
				 G_TYPE_STRING, "Brasero",
				 G_TYPE_STRING, reason,
				 G_TYPE_INVALID,
				 G_TYPE_UINT, &cookie,
				 G_TYPE_INVALID);
	if (!res) {
		g_warning ("Failed to inhibit the system from suspending: %s",
			    error->message);
		g_error_free (error);
		cookie = -1;
	}

	g_object_unref (G_OBJECT (proxy));
	dbus_g_connection_unref (conn);

	return cookie;
}
