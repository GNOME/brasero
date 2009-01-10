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
	DRIVE_ADDED,
	DRIVE_REMOVED,

	LAST_SIGNAL
};


static guint medium_monitor_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (BraseroMediumMonitor, brasero_medium_monitor, G_TYPE_OBJECT);


/**
 * These definitions go here as they shouldn't be public and they're used only 
 * here.
 */

BraseroDrive *
brasero_drive_new (const gchar *udi);

gboolean
brasero_drive_probing (BraseroDrive *drive);

/**
 * brasero_medium_monitor_get_drive:
 * @monitor: a #BraseroMediumMonitor
 * @device: the path of the device
 *
 * Returns the #BraseroDrive object whose path is @path.
 *
 * Return value: a #BraseroDrive or NULL
 **/

BraseroDrive *
brasero_medium_monitor_get_drive (BraseroMediumMonitor *monitor,
				  const gchar *device)
{
	GSList *iter;
	BraseroMediumMonitorPrivate *priv;

	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (monitor);
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

/**
 * brasero_medium_monitor_is_probing:
 * @monitor: a #BraseroMediumMonitor
 *
 * Returns if the library is still probing some other media.
 *
 * Return value: %TRUE if it is still probing some media
 **/

gboolean
brasero_medium_monitor_is_probing (BraseroMediumMonitor *monitor)
{
	GSList *iter;
	BraseroMediumMonitorPrivate *priv;

	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (monitor);

	for (iter = priv->drives; iter; iter = iter->next) {
		BraseroDrive *drive;

		drive = iter->data;
		if (brasero_drive_is_fake (drive))
			continue;

		if (brasero_drive_probing (drive))
			return TRUE;
	}

	return FALSE;
}

/**
 * brasero_medium_monitor_get_drives:
 * @monitor: a #BraseroMediumMonitor
 * @include_fake: a #BraseroDriveType to tell what type of drives to include in the list
 *
 * Obtains the list of available drives.
 *
 * Return value: a #GSList or NULL
 **/

GSList *
brasero_medium_monitor_get_drives (BraseroMediumMonitor *monitor,
				   BraseroDriveType type)
{
	BraseroMediumMonitorPrivate *priv;
	GSList *drives = NULL;
	GSList *iter;

	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (monitor);

	for (iter = priv->drives; iter; iter = iter->next) {
		BraseroDrive *drive;

		drive = iter->data;
		if (brasero_drive_is_fake (drive)) {
			if (type & BRASERO_DRIVE_TYPE_FILE)
				drives = g_slist_prepend (drives, drive);

			continue;
		}

		if (brasero_drive_can_write (drive)
		&& (type & BRASERO_DRIVE_TYPE_WRITER)) {
			drives = g_slist_prepend (drives, drive);
			continue;
		}

		if (type & BRASERO_DRIVE_TYPE_READER) {
			drives = g_slist_prepend (drives, drive);
			continue;
		}
	}
	g_slist_foreach (drives, (GFunc) g_object_ref, NULL);

	return drives;
}

/**
 * brasero_medium_monitor_get_media:
 * @monitor: a #BraseroMediumMonitor
 * @type: the type of #BraseroMedium that should be in the list
 *
 * Obtains the list of available media that are of the given type.
 *
 * Return value: a #GSList or NULL
 **/

GSList *
brasero_medium_monitor_get_media (BraseroMediumMonitor *monitor,
				  BraseroMediaType type)
{
	GSList *iter;
	GSList *list = NULL;
	BraseroMediumMonitorPrivate *priv;

	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (monitor);

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

		if ((type & BRASERO_MEDIA_TYPE_AUDIO)
		&& !(brasero_medium_get_status (medium) & BRASERO_MEDIUM_FILE)
		&&  (brasero_medium_get_status (medium) & BRASERO_MEDIUM_HAS_AUDIO)) {
			list = g_slist_prepend (list, medium);
			g_object_ref (medium);
			continue;
		}

		if ((type & BRASERO_MEDIA_TYPE_DATA)
		&& !(brasero_medium_get_status (medium) & BRASERO_MEDIUM_FILE)
		&&  (brasero_medium_get_status (medium) & BRASERO_MEDIUM_HAS_DATA)) {
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

	BRASERO_MEDIA_LOG ("New drive added");

	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (self);

	drive = brasero_drive_new (udi);
	priv->drives = g_slist_prepend (priv->drives, drive);
	g_signal_emit (self,
		       medium_monitor_signals [DRIVE_ADDED],
		       0,
		       drive);

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

	BRASERO_MEDIA_LOG ("Drive removed");

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
			g_signal_emit (self,
				       medium_monitor_signals [DRIVE_REMOVED],
				       0,
				       drive);
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
	BRASERO_MEDIA_LOG ("Polling for drives");
	devices = libhal_find_device_by_capability (ctx,
						    "storage.cdrom", &nb_devices,
						    &error);
	if (dbus_error_is_set (&error)) {
		BRASERO_MEDIA_LOG ("Hal is not running : %s\n", error.message);
		dbus_error_free (&error);
		return;
	}

	BRASERO_MEDIA_LOG ("Found %d drives", nb_devices);
	for (i = 0; i < nb_devices; i++) {
		/* create the drive */
		BRASERO_MEDIA_LOG ("Probing %s", devices [i]);
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

	/**
 	* BraseroVolumeMonitor::medium-added:
 	* @monitor: the object which received the signal
  	* @medium: the new medium which was added
	*
 	* This signal gets emitted when a new medium was detected
 	*
 	*/
	medium_monitor_signals[MEDIUM_INSERTED] =
		g_signal_new ("medium_added",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
		              G_STRUCT_OFFSET (BraseroMediumMonitorClass, medium_added),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE, 1,
		              BRASERO_TYPE_MEDIUM);

	/**
 	* BraseroVolumeMonitor::medium-removed:
 	* @monitor: the object which received the signal
  	* @medium: the medium which was removed
	*
 	* This signal gets emitted when a medium is not longer available
 	*
 	*/
	medium_monitor_signals[MEDIUM_REMOVED] =
		g_signal_new ("medium_removed",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
		              G_STRUCT_OFFSET (BraseroMediumMonitorClass, medium_removed),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE, 1,
		              BRASERO_TYPE_MEDIUM);

	/**
 	* BraseroVolumeMonitor::drive-added:
 	* @monitor: the object which received the signal
  	* @medium: the new medium which was added
	*
 	* This signal gets emitted when a new drive was detected
 	*
 	*/
	medium_monitor_signals[DRIVE_ADDED] =
		g_signal_new ("drive_added",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
		              G_STRUCT_OFFSET (BraseroMediumMonitorClass, drive_added),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE, 1,
		              BRASERO_TYPE_DRIVE);

	/**
 	* BraseroVolumeMonitor::drive-removed:
 	* @monitor: the object which received the signal
  	* @medium: the medium which was removed
	*
 	* This signal gets emitted when a drive is not longer available
 	*
 	*/
	medium_monitor_signals[DRIVE_REMOVED] =
		g_signal_new ("drive_removed",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
		              G_STRUCT_OFFSET (BraseroMediumMonitorClass, drive_removed),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE, 1,
		              BRASERO_TYPE_DRIVE);
}

static BraseroMediumMonitor *singleton = NULL;

/**
 * brasero_medium_monitor_get_default:
 *
 * Gets the currently active monitor.
 *
 * Return value: a #BraseroMediumMonitor. Unref when it is not needed anymore.
 **/

BraseroMediumMonitor *
brasero_medium_monitor_get_default (void)
{
	if (singleton) {
		g_object_ref (singleton);
		return singleton;
	}

	singleton = g_object_new (BRASERO_TYPE_MEDIUM_MONITOR, NULL);

	/* keep a reference */
	g_object_ref (singleton);
	return singleton;
}
