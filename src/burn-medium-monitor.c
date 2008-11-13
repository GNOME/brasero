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

#include "burn-debug.h"
#include "burn-drive.h"
#include "burn-medium.h"
#include "burn-hal-watch.h"
#include "burn-medium-monitor.h"

#if defined(HAVE_STRUCT_USCSI_CMD)
#define BLOCK_DEVICE	"block.solaris.raw_device"
#else
#define BLOCK_DEVICE	"block.device"
#endif

typedef struct _BraseroMediumMonitorPrivate BraseroMediumMonitorPrivate;
struct _BraseroMediumMonitorPrivate
{
	GSList *drives;
	GVolumeMonitor *gmonitor;

	gint probing;
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

BraseroDrive *
brasero_medium_monitor_get_drive (BraseroMediumMonitor *self,
				  const gchar *device)
{
	GSList *iter;
	BraseroMediumMonitorPrivate *priv;

	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (self);
	for (iter = priv->drives; iter; iter = iter->next) {
		BraseroDrive *drive;
		const gchar *drive_device;

		drive = iter->data;
		drive_device = brasero_drive_get_device (drive);
		if (drive_device && !strcmp (drive_device, device)) {
			g_object_ref (drive);
			return drive;
		}
	}

	return NULL;
}

gboolean
brasero_medium_monitor_is_probing (BraseroMediumMonitor *self)
{
	GSList *iter;
	BraseroMediumMonitorPrivate *priv;

	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (self);

	for (iter = priv->drives; iter; iter = iter->next) {
		BraseroDrive *drive;

		drive = iter->data;
		if (brasero_drive_probing (drive))
			return TRUE;
	}

	return FALSE;
}

GSList *
brasero_medium_monitor_get_media (BraseroMediumMonitor *self,
				  BraseroMediaType type)
{
	GSList *iter;
	GSList *list = NULL;
	BraseroMediumMonitorPrivate *priv;

	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (self);

	for (iter = priv->drives; iter; iter = iter->next) {
		BraseroMedium *medium;
		BraseroDrive *drive;

		drive = iter->data;

		medium = brasero_drive_get_medium (drive);
		if (!medium)
			continue;

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
brasero_medium_monitor_medium_added_cb (BraseroDrive *drive,
					BraseroMedium *medium,
					BraseroMediumMonitor *self)
{
	g_signal_emit (self,
		       medium_monitor_signals [MEDIUM_INSERTED],
		       0,
		       medium);
}

static void
brasero_medium_monitor_medium_removed_cb (BraseroDrive *drive,
					  BraseroMedium *medium,
					  BraseroMediumMonitor *self)
{
	g_signal_emit (self,
		       medium_monitor_signals [MEDIUM_REMOVED],
		       0,
		       medium);
}

static void
brasero_medium_monitor_inserted_cb (BraseroHALWatch *watch,
				    const char *udi,
				    BraseroMediumMonitor *self)
{
	BraseroMediumMonitorPrivate *priv;
	BraseroDrive *drive = NULL;
	LibHalContext *ctx;

	ctx = brasero_hal_watch_get_ctx (watch);
	if (!libhal_device_query_capability (ctx, udi, "storage.cdrom", NULL))
		return;

	BRASERO_BURN_LOG ("New drive inserted");

	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (self);

	drive = brasero_drive_new (udi);
	priv->drives = g_slist_prepend (priv->drives, drive);

	/* check if a medium is inserted */
	if (brasero_drive_get_medium (drive))
		g_signal_emit (self,
			       medium_monitor_signals [MEDIUM_INSERTED],
			       0,
			       brasero_drive_get_medium (drive));

	/* connect to signals */
	g_signal_connect (drive,
			  "medium-added",
			  G_CALLBACK (brasero_medium_monitor_medium_added_cb),
			  self);
	g_signal_connect (drive,
			  "medium-removed",
			  G_CALLBACK (brasero_medium_monitor_medium_removed_cb),
			  self);
}

static void
brasero_medium_monitor_removed_cb (BraseroHALWatch *watch,
				   const char *udi,
				   BraseroMediumMonitor *self)
{
	BraseroMediumMonitorPrivate *priv;
	LibHalContext *ctx;
	GSList *iter;
	GSList *next;

	ctx = brasero_hal_watch_get_ctx (watch);
	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (self);

	BRASERO_BURN_LOG ("Drive removed");

	for (iter = priv->drives; iter; iter = next) {
		const gchar *device_udi;
		BraseroDrive *drive;

		drive = iter->data;
		next = iter->next;

		device_udi = brasero_drive_get_udi (drive);
		if (!device_udi)
			continue;

		if (!strcmp (device_udi, udi)) {
			BraseroMedium *medium;

			medium = brasero_drive_get_medium (drive);
			if (medium)
				g_signal_emit (self,
					       medium_monitor_signals [MEDIUM_REMOVED],
					       0,
					       medium);

			priv->drives = g_slist_remove (priv->drives, drive);
			g_object_unref (drive);
		}
	}
}

static void
brasero_medium_monitor_init (BraseroMediumMonitor *object)
{
	DBusError error;
	int nb_devices, i;
	LibHalContext *ctx;
	BraseroDrive *drive;
	char **devices = NULL;
	BraseroHALWatch *watch;
	BraseroMediumMonitorPrivate *priv;

	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (object);

	/* This must done early on. GVolumeMonitor when it relies on HAL (like
	 * us) must be able to update its list of volumes before us so it must
	 * connect to HAL before us. */
	priv->gmonitor = g_volume_monitor_get ();

	watch = brasero_hal_watch_get_default ();
	ctx = brasero_hal_watch_get_ctx (watch);

	g_signal_connect (watch,
			  "device-added",
			  G_CALLBACK (brasero_medium_monitor_inserted_cb),
			  object);
	g_signal_connect (watch,
			  "device-removed",
			  G_CALLBACK (brasero_medium_monitor_removed_cb),
			  object);

	/* Now we get the list and cache it */
	dbus_error_init (&error);
	BRASERO_BURN_LOG ("Polling for drives");
	devices = libhal_find_device_by_capability (ctx,
						    "storage.cdrom", &nb_devices,
						    &error);
	if (dbus_error_is_set (&error)) {
		BRASERO_BURN_LOG ("Hal is not running : %s\n", error.message);
		dbus_error_free (&error);
		return;
	}

	BRASERO_BURN_LOG ("Found %d drives", nb_devices);
	for (i = 0; i < nb_devices; i++) {
		/* create the drive */
		BRASERO_BURN_LOG ("Probing %s", devices [i]);
		drive = brasero_drive_new (devices [i]);
		priv->drives = g_slist_prepend (priv->drives, drive);

		g_signal_connect (drive,
				  "medium-added",
				  G_CALLBACK (brasero_medium_monitor_medium_added_cb),
				  object);
		g_signal_connect (drive,
				  "medium-removed",
				  G_CALLBACK (brasero_medium_monitor_medium_removed_cb),
				  object);
	}
	libhal_free_string_array (devices);

	/* add fake/file drive */
	drive = brasero_drive_new (NULL);
	priv->drives = g_slist_prepend (priv->drives, drive);

	return;
}

static void
brasero_medium_monitor_finalize (GObject *object)
{
	BraseroMediumMonitorPrivate *priv;

	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (object);

	if (priv->drives) {
		g_slist_foreach (priv->drives, (GFunc) g_object_unref, NULL);
		g_slist_free (priv->drives);
		priv->drives = NULL;
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
