/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-media
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-media is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-media authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-media. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-media is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-media is distributed in the hope that it will be useful,
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
#include <glib-object.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <libhal.h>

#include "brasero-media.h"
#include "brasero-media-private.h"

#include "burn-hal-watch.h"
#include "libbrasero-marshal.h"

typedef struct _BraseroHALWatchPrivate BraseroHALWatchPrivate;
struct _BraseroHALWatchPrivate
{
	LibHalContext * ctx;
};

#define BRASERO_HAL_WATCH_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_HAL_WATCH, BraseroHALWatchPrivate))

enum
{
	PROPERTY_CHANGED,
	DEVICE_ADDED,
	DEVICE_REMOVED,
	LAST_SIGNAL
};


static guint hal_watch_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (BraseroHALWatch, brasero_hal_watch, G_TYPE_OBJECT);

LibHalContext *
brasero_hal_watch_get_ctx (BraseroHALWatch *self)
{
	BraseroHALWatchPrivate *priv;

	priv = BRASERO_HAL_WATCH_PRIVATE (self);
	if (!priv->ctx)
		BRASERO_MEDIA_LOG ("HAL context is NULL");

	return priv->ctx;
}

static void
brasero_hal_watch_property_changed_cb (LibHalContext *ctx,
				       const char *udi,
				       const char *key,
				       dbus_bool_t is_removed,
				       dbus_bool_t is_added)
{
	BraseroHALWatch *self;

	self = libhal_ctx_get_user_data (ctx);
	g_signal_emit (self,
		       hal_watch_signals [PROPERTY_CHANGED],
		       0,
		       udi,
		       key);
}

static void
brasero_hal_watch_device_added_cb (LibHalContext *ctx,
				   const char *udi)
{
	BraseroHALWatch *self;

	self = libhal_ctx_get_user_data (ctx);
	g_signal_emit (self,
		       hal_watch_signals [DEVICE_ADDED],
		       0,
		       udi);
}

static void
brasero_hal_watch_device_removed_cb (LibHalContext *ctx,
				     const char *udi)
{
	BraseroHALWatch *self;

	self = libhal_ctx_get_user_data (ctx);
	g_signal_emit (self,
		       hal_watch_signals [DEVICE_REMOVED],
		       0,
		       udi);
}

static void
brasero_hal_watch_init (BraseroHALWatch *object)
{
	DBusError error;
	BraseroHALWatchPrivate *priv;
	DBusConnection *dbus_connection = NULL;

	priv = BRASERO_HAL_WATCH_PRIVATE (object);

	/* initialize the connection with hal */
	priv->ctx = libhal_ctx_new ();
	if (priv->ctx == NULL) {
		g_warning ("Cannot initialize hal library\n");
		goto error;
	}

	dbus_error_init (&error);
	dbus_connection = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
	if (dbus_error_is_set (&error)) {
		g_warning ("Cannot connect to DBus %s\n", error.message);
		dbus_error_free (&error);
		goto error;
	}

	dbus_connection_setup_with_g_main (dbus_connection, NULL);
	libhal_ctx_set_dbus_connection (priv->ctx, dbus_connection);

	libhal_ctx_set_user_data (priv->ctx, object);
	libhal_ctx_set_cache (priv->ctx, FALSE);

	/* monitor devices addition and removal */
	libhal_ctx_set_device_added (priv->ctx, brasero_hal_watch_device_added_cb);
	libhal_ctx_set_device_removed (priv->ctx, brasero_hal_watch_device_removed_cb);
	libhal_ctx_set_device_property_modified (priv->ctx, brasero_hal_watch_property_changed_cb);

	if (libhal_ctx_init (priv->ctx, &error))
		return;

	g_warning ("Failed to initialize hal : %s\n", error.message);
	dbus_error_free (&error);

error:

	libhal_ctx_shutdown (priv->ctx, NULL);
	libhal_ctx_free (priv->ctx);
	priv->ctx = NULL;

	if (dbus_connection)
		dbus_connection_unref (dbus_connection);
}

static void
brasero_hal_watch_finalize (GObject *object)
{
	BraseroHALWatchPrivate *priv;

	priv = BRASERO_HAL_WATCH_PRIVATE (object);

	if (priv->ctx) {
		DBusConnection *connection;

		connection = libhal_ctx_get_dbus_connection (priv->ctx);
		dbus_connection_unref (connection);

		libhal_ctx_shutdown (priv->ctx, NULL);
		libhal_ctx_free (priv->ctx);
		priv->ctx = NULL;
	}

	G_OBJECT_CLASS (brasero_hal_watch_parent_class)->finalize (object);
}

static void
brasero_hal_watch_class_init (BraseroHALWatchClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroHALWatchPrivate));

	object_class->finalize = brasero_hal_watch_finalize;

	hal_watch_signals[PROPERTY_CHANGED] =
		g_signal_new ("property_changed",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
		              0,
		              NULL, NULL,
		              brasero_marshal_VOID__STRING_STRING,
		              G_TYPE_NONE, 2,
		              G_TYPE_STRING,
			      G_TYPE_STRING);
	hal_watch_signals[DEVICE_ADDED] =
		g_signal_new ("device_added",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
		              0,
		              NULL, NULL,
		              g_cclosure_marshal_VOID__STRING,
		              G_TYPE_NONE, 1,
		              G_TYPE_STRING);
	hal_watch_signals[DEVICE_REMOVED] =
		g_signal_new ("device_removed",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
		              0,
		              NULL, NULL,
		              g_cclosure_marshal_VOID__STRING,
		              G_TYPE_NONE, 1,
		              G_TYPE_STRING);
}

static BraseroHALWatch *singleton = NULL;

BraseroHALWatch *
brasero_hal_watch_get_default (void)
{
	if (singleton)
		return singleton;

	singleton = g_object_new (BRASERO_TYPE_HAL_WATCH, NULL);
	return singleton;
}

void
brasero_hal_watch_destroy (void)
{
	if (singleton) {
		g_object_unref (singleton);
		singleton = NULL;
	}
}
