/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2005-2008 <bonfire-app@wanadoo.fr>
 * 
 *  Brasero is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 * brasero is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with brasero.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <gio/gio.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <libhal.h>

#include "burn-drive.h"

#include "burn-medium.h"
#include "burn-volume-obj.h"
#include "burn-medium-monitor.h"

typedef struct _BraseroMediumMonitorPrivate BraseroMediumMonitorPrivate;
struct _BraseroMediumMonitorPrivate
{
	GSList *media;
	GSList *drives;

	LibHalContext *ctx;
	GVolumeMonitor *gmonitor;
};

#define BRASERO_MEDIUM_MONITOR_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_MEDIUM_MONITOR, BraseroMediumMonitorPrivate))

enum
{
	MEDIUM_INSERTED,
	MEDIUM_REMOVED,

	LAST_SIGNAL
};


static guint medium_monitor_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (BraseroMediumMonitor, brasero_medium_monitor, G_TYPE_OBJECT);


GSList *
brasero_medium_monitor_get_media (BraseroMediumMonitor *self,
				  BraseroMediaType type)
{
	GSList *iter;
	GSList *list = NULL;
	BraseroMediumMonitorPrivate *priv;

	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (self);

	for (iter = priv->media; iter; iter = iter->next) {
		BraseroMedium *medium;
		BraseroDrive *drive;

		medium = iter->data;
		drive = brasero_medium_get_drive (medium);

		if ((type & BRASERO_MEDIA_TYPE_ANY_IN_BURNER)
		&&  (brasero_drive_can_write (drive))) {
			list = g_slist_prepend (list, medium);
			g_object_ref (medium);
			continue;
		}

		if ((type & BRASERO_MEDIA_TYPE_READABLE)
		&& !(brasero_medium_get_status (medium) & BRASERO_MEDIUM_FILE)
		&&  (brasero_medium_get_status (medium) & (BRASERO_MEDIUM_HAS_AUDIO|BRASERO_MEDIUM_HAS_DATA))) {
			list = g_slist_prepend (list, medium);
			g_object_ref (medium);
			continue;
		}

		if (type & BRASERO_MEDIA_TYPE_WRITABLE) {
			if (brasero_medium_can_be_written (medium)) {
				list = g_slist_prepend (list, medium);
				g_object_ref (medium);
				continue;
			}
		}

		if (type & BRASERO_MEDIA_TYPE_REWRITABLE) {
			if (brasero_medium_can_be_rewritten (medium)) {
				list = g_slist_prepend (list, medium);
				g_object_ref (medium);
				continue;
			}
		}

		if (type & BRASERO_MEDIA_TYPE_FILE) {
			if (brasero_medium_get_status (medium) & BRASERO_MEDIUM_FILE) {
				list = g_slist_prepend (list, medium);
				g_object_ref (medium);
			}
		}
	}

	return list;
}

static void
brasero_medium_monitor_drive_inserted (LibHalContext *ctx,
				       const gchar *udi)
{
	BraseroMediumMonitorPrivate *priv;
	BraseroMediumMonitor *self;
	BraseroDrive *drive = NULL;

	self = libhal_ctx_get_user_data (ctx);
	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (self);

	drive = brasero_drive_new (udi);
	priv->drives = g_slist_prepend (priv->drives, drive);
}

static void
brasero_medium_monitor_medium_inserted (LibHalContext *ctx,
					const gchar *udi)
{
	BraseroMediumMonitorPrivate *priv;
	BraseroMediumMonitor *self;
	BraseroDrive *drive = NULL;
	BraseroMedium *medium;
	gchar *drive_path;
	GSList *iter;

	drive_path = libhal_device_get_property_string (ctx,
							udi,
							"block.device",
							NULL);
	if (!drive_path)
		return;

	self = libhal_ctx_get_user_data (ctx);
	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (self);

	/* Search the drive */
	for (iter = priv->drives; iter; iter = iter->next) {
		BraseroDrive *tmp;

		tmp = iter->data;
		if (!brasero_drive_get_device (tmp))
			continue;

		if (!strcmp (brasero_drive_get_device (tmp), drive_path)) {
			drive = tmp;
			break;
		}
	}
	g_free (drive_path);

	if (!drive)
		return;

	/* Create medium */
	medium = BRASERO_MEDIUM (brasero_volume_new (drive, udi));
	priv->media = g_slist_prepend (priv->media, medium);
	brasero_drive_set_medium (drive, medium);

	g_signal_emit (self,
		       medium_monitor_signals [MEDIUM_INSERTED],
		       0,
		       medium);
}

static void
brasero_medium_monitor_inserted_cb (LibHalContext *ctx,
				    const char *udi)
{
	if (libhal_device_property_exists (ctx, udi, "volume.is_disc", NULL)
	&&  libhal_device_get_property_bool (ctx, udi, "volume.is_disc", NULL))
		brasero_medium_monitor_medium_inserted (ctx, udi);
	else if (libhal_device_property_exists (ctx, udi, "storage.cdrom", NULL)
	     &&  libhal_device_get_property_bool (ctx, udi, "storage.cdrom", NULL))
		brasero_medium_monitor_drive_inserted (ctx, udi);
}

static void
brasero_medium_monitor_removed_cb (LibHalContext *ctx,
				   const char *udi)
{
	BraseroMediumMonitorPrivate *priv;
	BraseroMediumMonitor *self;
	GSList *iter;

	self = libhal_ctx_get_user_data (ctx);
	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (self);

	for (iter = priv->media; iter; iter = iter->next) {
		const gchar *device_udi;
		BraseroMedium *medium;

		medium = iter->data;
		device_udi = brasero_medium_get_udi (medium);
		if (!device_udi)
			continue;

		if (!strcmp (device_udi, udi)) {
			BraseroDrive *drive;

			drive = brasero_medium_get_drive (medium);
			if (drive)
				brasero_drive_set_medium (drive, NULL);

			priv->media = g_slist_remove (priv->media, medium);
			g_signal_emit (self,
				       medium_monitor_signals [MEDIUM_REMOVED],
				       0,
				       medium);

			g_object_unref (medium);
			break;
		}
	}

	for (iter = priv->drives; iter; iter = iter->next) {
		const gchar *device_udi;
		BraseroDrive *drive;

		drive = iter->data;
		device_udi = brasero_drive_get_udi (drive);
		if (!device_udi)
			continue;

		if (!strcmp (device_udi, udi)) {
			BraseroMedium *medium;

			medium = brasero_drive_get_medium (drive);
			brasero_drive_set_medium (drive, NULL);

			if (medium) {
				priv->media = g_slist_remove (priv->media, medium);
				g_signal_emit (self,
					       medium_monitor_signals [MEDIUM_REMOVED],
					       0,
					       medium);
				g_object_unref (medium);
				return;
			}

			priv->drives = g_slist_remove (priv->drives, drive);
			g_object_unref (drive);
			break;
		}
	}
}

static void
brasero_medium_monitor_add_file (BraseroMediumMonitor *self)
{
	BraseroMediumMonitorPrivate *priv;
	BraseroMedium *medium;
	BraseroDrive *drive;

	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (self);

	drive = brasero_drive_new (NULL);
	priv->drives = g_slist_prepend (priv->drives, drive);
	
	medium = g_object_new (BRASERO_TYPE_VOLUME,
			       "drive", drive,
			       NULL);
	priv->media = g_slist_prepend (priv->media, medium);
	brasero_drive_set_medium (drive, medium);
}

static void
brasero_medium_monitor_init (BraseroMediumMonitor *object)
{
	DBusError error;
	int nb_devices, i;
	char **devices = NULL;
	DBusConnection *dbus_connection;
	BraseroMediumMonitorPrivate *priv;

	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (object);

	/* This must done early on. GVolumeMonitor when it relies on HAL (like
	 * us) must be able to update its list of volumes before us so it must
	 * connect to HAL before us. */
	priv->gmonitor = g_volume_monitor_get ();

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
	libhal_ctx_set_device_added (priv->ctx, brasero_medium_monitor_inserted_cb);
	libhal_ctx_set_device_removed (priv->ctx, brasero_medium_monitor_removed_cb);

	if (libhal_ctx_init (priv->ctx, &error) == FALSE) {
		g_warning ("Failed to initialize hal : %s\n", error.message);
		dbus_error_free (&error);
		goto error;
	}

	/* Now we get the list and cache it */
	devices = libhal_find_device_by_capability (priv->ctx,
						    "storage.cdrom", &nb_devices,
						    &error);
	if (dbus_error_is_set (&error)) {
		g_warning ("Hal is not running : %s\n", error.message);
		dbus_error_free (&error);
		goto error;
	}

	for (i = 0; i < nb_devices; i++) {
		int j;
		int nb_volumes;
		BraseroDrive *drive;
		char **volumes = NULL;

		/* create the drive */
		drive = brasero_drive_new (devices [i]);
		priv->drives = g_slist_prepend (priv->drives, drive);

		/* Now search for a possible medium inside */
		volumes = libhal_manager_find_device_string_match (priv->ctx,
								   "info.parent",
								   devices [i],
								   &nb_volumes,
								   &error);
		if (dbus_error_is_set (&error)) {
			g_warning ("Hal connection problem :  %s\n",
				   error.message);
			dbus_error_free (&error);

			if (volumes)
				libhal_free_string_array (volumes);
			goto error;
		}

		for (j = 0; j < nb_volumes; j++) {
			BraseroMedium *medium;

			medium = BRASERO_MEDIUM (brasero_volume_new (drive, volumes [j]));
			priv->media = g_slist_prepend (priv->media, medium);
			brasero_drive_set_medium (drive, medium);
		}

		libhal_free_string_array (volumes);
	}
	libhal_free_string_array (devices);

	brasero_medium_monitor_add_file (object);

	return;

      error:
	libhal_ctx_shutdown (priv->ctx, NULL);
	libhal_ctx_free (priv->ctx);
	priv->ctx = NULL;

	if (devices)
		libhal_free_string_array (devices);

	return;
}

static void
brasero_medium_monitor_finalize (GObject *object)
{
	BraseroMediumMonitorPrivate *priv;

	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (object);

	if (priv->media) {
		g_slist_foreach (priv->media, (GFunc) g_object_unref, NULL);
		g_slist_free (priv->media);
		priv->media = NULL;
	}

	if (priv->drives) {
		g_slist_foreach (priv->drives, (GFunc) g_object_unref, NULL);
		g_slist_free (priv->drives);
		priv->drives = NULL;
	}

	if (priv->ctx) {
		DBusConnection *connection;

		connection = libhal_ctx_get_dbus_connection (priv->ctx);
		dbus_connection_unref (connection);

		libhal_ctx_shutdown (priv->ctx, NULL);
		libhal_ctx_free (priv->ctx);
		priv->ctx = NULL;
	}

	if (priv->gmonitor) {
		g_object_unref (priv->gmonitor);
		priv->gmonitor = NULL;
	}

	G_OBJECT_CLASS (brasero_medium_monitor_parent_class)->finalize (object);
}

static void
brasero_medium_monitor_class_init (BraseroMediumMonitorClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroMediumMonitorPrivate));

	object_class->finalize = brasero_medium_monitor_finalize;

	medium_monitor_signals[MEDIUM_INSERTED] =
		g_signal_new ("medium_added",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_ACTION,
		              0,
		              NULL, NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE, 1,
		              BRASERO_TYPE_MEDIUM);

	medium_monitor_signals[MEDIUM_REMOVED] =
		g_signal_new ("medium_removed",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_ACTION,
		              0,
		              NULL, NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE, 1,
		              BRASERO_TYPE_MEDIUM);
}

static BraseroMediumMonitor *singleton = NULL;

BraseroMediumMonitor *
brasero_medium_monitor_get_default (void)
{
	if (singleton) {
		g_object_ref (singleton);
		return singleton;
	}

	singleton = g_object_new (BRASERO_TYPE_MEDIUM_MONITOR, NULL);
	return singleton;
}
