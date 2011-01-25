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

#include <string.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <gio/gio.h>

#include "brasero-media-private.h"

#include "brasero-drive-priv.h"

#include "scsi-device.h"
#include "scsi-utils.h"
#include "scsi-spc1.h"

#include "brasero-drive.h"
#include "brasero-medium.h"
#include "brasero-medium-monitor.h"

typedef struct _BraseroMediumMonitorPrivate BraseroMediumMonitorPrivate;
struct _BraseroMediumMonitorPrivate
{
	GSList *drives;
	GVolumeMonitor *gmonitor;

	GSList *waiting_removal;
	guint waiting_removal_id;

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
 * brasero_medium_monitor_get_drive:
 * @monitor: a #BraseroMediumMonitor
 * @device: the path of the device
 *
 * Returns the #BraseroDrive object whose path is @path.
 *
 * Return value: a #BraseroDrive or NULL. It should be unreffed when no longer in use.
 **/
BraseroDrive *
brasero_medium_monitor_get_drive (BraseroMediumMonitor *monitor,
				  const gchar *device)
{
	GSList *iter;
	BraseroMediumMonitorPrivate *priv;

	g_return_val_if_fail (monitor != NULL, NULL);
	g_return_val_if_fail (device != NULL, NULL);
	g_return_val_if_fail (BRASERO_IS_MEDIUM_MONITOR (monitor), NULL);

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

	g_return_val_if_fail (monitor != NULL, FALSE);
	g_return_val_if_fail (BRASERO_IS_MEDIUM_MONITOR (monitor), FALSE);

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
 * @type: a #BraseroDriveType to tell what type of drives to include in the list
 *
 * Gets the list of available drives that are of the given type.
 *
 * Return value: (element-type BraseroMedia.Drive) (transfer full): a #GSList of  #BraseroDrive or NULL. The list must be freed and the element unreffed when finished.
 **/
GSList *
brasero_medium_monitor_get_drives (BraseroMediumMonitor *monitor,
				   BraseroDriveType type)
{
	BraseroMediumMonitorPrivate *priv;
	GSList *drives = NULL;
	GSList *iter;

	g_return_val_if_fail (monitor != NULL, NULL);
	g_return_val_if_fail (BRASERO_IS_MEDIUM_MONITOR (monitor), NULL);

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
 * Return value: (element-type BraseroMedia.Medium) (transfer full): a #GSList of  #BraseroMedium or NULL. The list must be freed and the element unreffed when finished.
 **/
GSList *
brasero_medium_monitor_get_media (BraseroMediumMonitor *monitor,
				  BraseroMediaType type)
{
	GSList *iter;
	GSList *list = NULL;
	BraseroMediumMonitorPrivate *priv;

	g_return_val_if_fail (monitor != NULL, NULL);
	g_return_val_if_fail (BRASERO_IS_MEDIUM_MONITOR (monitor), NULL);

	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (monitor);

	for (iter = priv->drives; iter; iter = iter->next) {
		BraseroMedium *medium;
		BraseroDrive *drive;

		drive = iter->data;

		medium = brasero_drive_get_medium (drive);
		if (!medium)
			continue;

		if ((type & BRASERO_MEDIA_TYPE_CD) == type
		&& (brasero_medium_get_status (medium) & BRASERO_MEDIUM_CD)) {
			/* If used alone, returns all CDs */
			list = g_slist_prepend (list, medium);
			g_object_ref (medium);
			continue;
		}

		if ((type & BRASERO_MEDIA_TYPE_ANY_IN_BURNER)
		&&  (brasero_drive_can_write (drive))) {
			if ((type & BRASERO_MEDIA_TYPE_CD)) {
				if ((brasero_medium_get_status (medium) & BRASERO_MEDIUM_CD)) {
					list = g_slist_prepend (list, medium);
					g_object_ref (medium);
					continue;
				}
			}
			else {
				list = g_slist_prepend (list, medium);
				g_object_ref (medium);
				continue;
			}
			continue;
		}

		if ((type & BRASERO_MEDIA_TYPE_AUDIO)
		&& !(brasero_medium_get_status (medium) & BRASERO_MEDIUM_FILE)
		&&  (brasero_medium_get_status (medium) & BRASERO_MEDIUM_HAS_AUDIO)) {
			if ((type & BRASERO_MEDIA_TYPE_CD)) {
				if ((brasero_medium_get_status (medium) & BRASERO_MEDIUM_CD)) {
					list = g_slist_prepend (list, medium);
					g_object_ref (medium);
					continue;
				}
			}
			else {
				list = g_slist_prepend (list, medium);
				g_object_ref (medium);
				continue;
			}
			continue;
		}

		if ((type & BRASERO_MEDIA_TYPE_DATA)
		&& !(brasero_medium_get_status (medium) & BRASERO_MEDIUM_FILE)
		&&  (brasero_medium_get_status (medium) & BRASERO_MEDIUM_HAS_DATA)) {
			if ((type & BRASERO_MEDIA_TYPE_CD)) {
				if ((brasero_medium_get_status (medium) & BRASERO_MEDIUM_CD)) {
					list = g_slist_prepend (list, medium);
					g_object_ref (medium);
					continue;
				}
			}
			else {
				list = g_slist_prepend (list, medium);
				g_object_ref (medium);
				continue;
			}
			continue;
		}

		if (type & BRASERO_MEDIA_TYPE_WRITABLE) {
			if (brasero_medium_can_be_written (medium)) {
				if ((type & BRASERO_MEDIA_TYPE_CD)) {
					if ((brasero_medium_get_status (medium) & BRASERO_MEDIUM_CD)) {
						list = g_slist_prepend (list, medium);
						g_object_ref (medium);
						continue;
					}
				}
				else {
					list = g_slist_prepend (list, medium);
					g_object_ref (medium);
					continue;
				}
			}
		}

		if (type & BRASERO_MEDIA_TYPE_REWRITABLE) {
			if (brasero_medium_can_be_rewritten (medium)) {
				if ((type & BRASERO_MEDIA_TYPE_CD)) {
					if ((brasero_medium_get_status (medium) & BRASERO_MEDIUM_CD)) {
						list = g_slist_prepend (list, medium);
						g_object_ref (medium);
						continue;
					}
				}
				else {
					list = g_slist_prepend (list, medium);
					g_object_ref (medium);
					continue;
				}
			}
		}

		if (type & BRASERO_MEDIA_TYPE_FILE) {
			/* make sure the drive is indeed a fake one
			 * since it can happen that some medium did
			 * not properly carry out their initialization 
			 * and are flagged as BRASERO_MEDIUM_FILE
			 * whereas they are not */
			if (brasero_drive_is_fake (drive)) {
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

static gboolean
brasero_medium_monitor_is_drive (BraseroMediumMonitor *monitor,
                                 const gchar *device)
{
	BraseroDeviceHandle *handle;
	BraseroScsiErrCode code;
	gboolean result;

	BRASERO_MEDIA_LOG ("Testing drive %s", device);

	handle = brasero_device_handle_open (device, FALSE, &code);
	if (!handle)
		return FALSE;

	result = (brasero_spc1_inquiry_is_optical_drive (handle, &code) == BRASERO_SCSI_OK);
	brasero_device_handle_close (handle);

	BRASERO_MEDIA_LOG ("Drive %s", result? "is optical":"is not optical");

	return result;
}

static BraseroDrive *
brasero_medium_monitor_drive_new (BraseroMediumMonitor *self,
                                  const gchar *device,
                                  GDrive *gdrive)
{
	BraseroMediumMonitorPrivate *priv;
	BraseroDrive *drive;

	if (!brasero_medium_monitor_is_drive (self, device))
		return NULL;

	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (self);
	drive = g_object_new (BRASERO_TYPE_DRIVE,
	                      "device", device,
			      "gdrive", gdrive,
			      NULL);

	priv->drives = g_slist_prepend (priv->drives, drive);

	g_signal_connect (drive,
			  "medium-added",
			  G_CALLBACK (brasero_medium_monitor_medium_added_cb),
			  self);
	g_signal_connect (drive,
			  "medium-removed",
			  G_CALLBACK (brasero_medium_monitor_medium_removed_cb),
			  self);

	return drive;
}

static void
brasero_medium_monitor_device_added (BraseroMediumMonitor *self,
                                     const gchar *device,
                                     GDrive *gdrive)
{
	BraseroMediumMonitorPrivate *priv;
	BraseroDrive *drive = NULL;

	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (self);

	/* See if the drive is waiting removal.
	 * This is necessary as GIO behaves strangely sometimes
	 * since it sends the "disconnected" signal when a medium
	 * is removed soon followed by a "connected" signal */
	drive = brasero_medium_monitor_get_drive (self, device);
	if (drive) {
		/* Just in case that drive was waiting removal */
		priv->waiting_removal = g_slist_remove (priv->waiting_removal, drive);

		BRASERO_MEDIA_LOG ("Added signal was emitted but the drive is in the removal list. Updating GDrive associated object.");
		g_object_set (drive,
		              "gdrive", gdrive,
		              NULL);

		g_object_unref (drive);
		return;
	}

	/* Make sure it's an optical drive */
	drive = brasero_medium_monitor_drive_new (self, device, gdrive);
	if (!drive)
		return;

	BRASERO_MEDIA_LOG ("New drive added");
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
}

static void
brasero_medium_monitor_connected_cb (GVolumeMonitor *monitor,
                                     GDrive *gdrive,
                                     BraseroMediumMonitor *self)
{
	gchar *device;

	BRASERO_MEDIA_LOG ("GDrive addition signal");

	device = g_drive_get_identifier (gdrive, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
	brasero_medium_monitor_device_added (self, device, gdrive);
	g_free (device);
}

static void
brasero_medium_monitor_volume_added_cb (GVolumeMonitor *monitor,
                                        GVolume *gvolume,
                                        BraseroMediumMonitor *self)
{
	gchar *device;
	GDrive *gdrive;

	BRASERO_MEDIA_LOG ("GVolume addition signal");

	/* No need to signal that addition if the GVolume
	 * object has an associated GDrive as this is just
	 * meant to trap blank discs which have no GDrive
	 * associated but a GVolume. */
	gdrive = g_volume_get_drive (gvolume);
	if (gdrive) {
		BRASERO_MEDIA_LOG ("Existing GDrive skipping");
		g_object_unref (gdrive);
		return;
	}

	device = g_volume_get_identifier (gvolume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
	if  (!device)
		return;

	brasero_medium_monitor_device_added (self, device, NULL);
	g_free (device);
}

static gboolean
brasero_medium_monitor_disconnected_real (gpointer data)
{
	BraseroMediumMonitor *self = BRASERO_MEDIUM_MONITOR (data);
	BraseroMediumMonitorPrivate *priv;
	BraseroMedium *medium;
	BraseroDrive *drive;

	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (self);

	if (!priv->waiting_removal) {
		priv->waiting_removal_id = 0;
		return FALSE;
	}

	drive = priv->waiting_removal->data;
	priv->waiting_removal = g_slist_remove (priv->waiting_removal, drive);

	BRASERO_MEDIA_LOG ("Drive removed");
	medium = brasero_drive_get_medium (drive);

	/* disconnect the signal handlers to avoid having the "medium-removed" fired twice */
	g_signal_handlers_disconnect_by_func (drive,
	                                      brasero_medium_monitor_medium_added_cb,
	                                      self);
	g_signal_handlers_disconnect_by_func (drive,
	                                      brasero_medium_monitor_medium_removed_cb,
	                                      self);

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

	/* in case there are more */
	return TRUE;
}

static void
brasero_medium_monitor_device_removed (BraseroMediumMonitor *self,
                                       const gchar *device,
                                       GDrive *gdrive)
{
	BraseroMediumMonitorPrivate *priv;
	GDrive *associated_gdrive;
	BraseroDrive *drive;

	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (self);

	/* Make sure it's one already detected */
	/* GIO behaves strangely: every time a medium 
	 * is removed from a drive it emits the disconnected
	 * signal (which IMO it shouldn't) soon followed by
	 * a connected signal.
	 * So delay the removal by one or two seconds. */

	drive = brasero_medium_monitor_get_drive (self, device);
	if (!drive)
		return;

	if (G_UNLIKELY (g_slist_find (priv->waiting_removal, drive) != NULL)) {
		g_object_unref (drive);
		return;
	}

	associated_gdrive = brasero_drive_get_gdrive (drive);
	if (associated_gdrive == gdrive) {
		BRASERO_MEDIA_LOG ("Found device to remove");
		priv->waiting_removal = g_slist_append (priv->waiting_removal, drive);

		if (!priv->waiting_removal_id)
			priv->waiting_removal_id = g_timeout_add_seconds (2,
			                                                  brasero_medium_monitor_disconnected_real, 
			                                                  self);
	}
	/* else do nothing and wait for a "drive-disconnected" signal */

	if (associated_gdrive)
		g_object_unref (associated_gdrive);

	g_object_unref (drive);
}

static void
brasero_medium_monitor_volume_removed_cb (GVolumeMonitor *monitor,
                                          GVolume *gvolume,
                                          BraseroMediumMonitor *self)
{
	gchar *device;
	GDrive *gdrive;

	BRASERO_MEDIA_LOG ("Volume removal signal");

	/* No need to signal that removal if the GVolume
	 * object has an associated GDrive as this is just
	 * meant to trap blank discs which have no GDrive
	 * associated but a GVolume. */
	gdrive = g_volume_get_drive (gvolume);
	if (gdrive) {
		g_object_unref (gdrive);
		return;
	}

	device = g_volume_get_identifier (gvolume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
	if (!device)
		return;

	brasero_medium_monitor_device_removed (self, device, NULL);
	g_free (device);
}

static void
brasero_medium_monitor_disconnected_cb (GVolumeMonitor *monitor,
                                        GDrive *gdrive,
                                        BraseroMediumMonitor *self)
{
	gchar *device;

	BRASERO_MEDIA_LOG ("Drive removal signal");

	device = g_drive_get_identifier (gdrive, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
	brasero_medium_monitor_device_removed (self, device, gdrive);
	g_free (device);
}

static void
brasero_medium_monitor_init (BraseroMediumMonitor *object)
{
	GList *iter;
	GList *drives;
	GList *volumes;
	BraseroDrive *drive;
	BraseroMediumMonitorPrivate *priv;

	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (object);

	BRASERO_MEDIA_LOG ("Probing drives and media");

	priv->gmonitor = g_volume_monitor_get ();

	drives = g_volume_monitor_get_connected_drives (priv->gmonitor);
	BRASERO_MEDIA_LOG ("Found %d drives", g_list_length (drives));

	for (iter = drives; iter; iter = iter->next) {
		GDrive *gdrive;
		gchar *device;

		gdrive = iter->data;

		device = g_drive_get_identifier (gdrive, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
		brasero_medium_monitor_drive_new (object, device, gdrive);
		g_free (device);
	}
	g_list_foreach (drives, (GFunc) g_object_unref, NULL);
	g_list_free (drives);

	volumes = g_volume_monitor_get_volumes (priv->gmonitor);
	BRASERO_MEDIA_LOG ("Found %d volumes", g_list_length (volumes));

	for (iter = volumes; iter; iter = iter->next) {
		GVolume *gvolume;
		gchar *device;

		gvolume = iter->data;
		device = g_volume_get_identifier (gvolume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
		if (!device)
			continue;

		/* make sure it isn't already in our list */
		drive = brasero_medium_monitor_get_drive (object, device);
		if (drive) {
			g_free (device);
			g_object_unref (drive);
			continue;
		}

		brasero_medium_monitor_drive_new (object, device, NULL);
		g_free (device);
	}
	g_list_foreach (volumes, (GFunc) g_object_unref, NULL);
	g_list_free (volumes);

	g_signal_connect (priv->gmonitor,
			  "volume-added",
			  G_CALLBACK (brasero_medium_monitor_volume_added_cb),
			  object);
	g_signal_connect (priv->gmonitor,
			  "volume-removed",
			  G_CALLBACK (brasero_medium_monitor_volume_removed_cb),
			  object);
	g_signal_connect (priv->gmonitor,
			  "drive-connected",
			  G_CALLBACK (brasero_medium_monitor_connected_cb),
			  object);
	g_signal_connect (priv->gmonitor,
			  "drive-disconnected",
			  G_CALLBACK (brasero_medium_monitor_disconnected_cb),
			  object);

	/* add fake/file drive */
	drive = g_object_new (BRASERO_TYPE_DRIVE,
	                      "device", NULL,
	                      NULL);
	priv->drives = g_slist_prepend (priv->drives, drive);

	return;
}

static void
brasero_medium_monitor_finalize (GObject *object)
{
	BraseroMediumMonitorPrivate *priv;

	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (object);

	if (priv->waiting_removal_id) {
		g_source_remove (priv->waiting_removal_id);
		priv->waiting_removal_id = 0;
	}

	if (priv->waiting_removal) {
		g_slist_free (priv->waiting_removal);
		priv->waiting_removal = NULL;
	}

	if (priv->drives) {
		g_slist_foreach (priv->drives, (GFunc) g_object_unref, NULL);
		g_slist_free (priv->drives);
		priv->drives = NULL;
	}

	if (priv->gmonitor) {
		g_signal_handlers_disconnect_by_func (priv->gmonitor,
		                                      brasero_medium_monitor_volume_added_cb,
		                                      object);
		g_signal_handlers_disconnect_by_func (priv->gmonitor,
		                                      brasero_medium_monitor_volume_removed_cb,
		                                      object);
		g_signal_handlers_disconnect_by_func (priv->gmonitor,
		                                      brasero_medium_monitor_connected_cb,
		                                      object);
		g_signal_handlers_disconnect_by_func (priv->gmonitor,
		                                      brasero_medium_monitor_disconnected_cb,
		                                      object);
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
 	* BraseroMediumMonitor::medium-added:
 	* @monitor: the object which received the signal
  	* @medium: the new medium which was added
	*
 	* This signal gets emitted when a new medium was detected
 	**/
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
 	* BraseroMediumMonitor::medium-removed:
 	* @monitor: the object which received the signal
  	* @medium: the medium which was removed
	*
 	* This signal gets emitted when a medium is not longer available
 	**/
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
 	* BraseroMediumMonitor::drive-added:
 	* @monitor: the object which received the signal
  	* @medium: the new medium which was added
	*
 	* This signal gets emitted when a new drive was detected
 	**/
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
 	* BraseroMediumMonitor::drive-removed:
 	* @monitor: the object which received the signal
  	* @medium: the medium which was removed
	*
 	* This signal gets emitted when a drive is not longer available
 	**/
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
