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

#include <unistd.h>
#include <string.h>

#ifdef HAVE_CAM_LIB_H
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <camlib.h>
#endif

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include <gio/gio.h>

#include "brasero-media-private.h"
#include "brasero-gio-operation.h"

#include "brasero-medium.h"
#include "brasero-volume.h"
#include "brasero-drive.h"

#include "brasero-drive-priv.h"
#include "scsi-device.h"
#include "scsi-utils.h"
#include "scsi-spc1.h"
#include "scsi-mmc1.h"
#include "scsi-mmc2.h"
#include "scsi-status-page.h"
#include "scsi-mode-pages.h"
#include "scsi-sbc.h"

typedef struct _BraseroDrivePrivate BraseroDrivePrivate;
struct _BraseroDrivePrivate
{
	GDrive *gdrive;

	GThread *probe;
	GMutex *mutex;
	GCond *cond;
	GCond *cond_probe;
	gint probe_id;

	BraseroMedium *medium;
	BraseroDriveCaps caps;

	gchar *udi;

	gchar *name;

	gchar *device;
	gchar *block_device;

	GCancellable *cancel;

	guint initial_probe:1;
	guint initial_probe_cancelled:1;

	guint has_medium:1;
	guint probe_cancelled:1;

	guint locked:1;
	guint ejecting:1;
	guint probe_waiting:1;
};

#define BRASERO_DRIVE_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_DRIVE, BraseroDrivePrivate))

enum {
	MEDIUM_REMOVED,
	MEDIUM_INSERTED,
	LAST_SIGNAL
};
static gulong drive_signals [LAST_SIGNAL] = {0, };

enum {
	PROP_NONE	= 0,
	PROP_DEVICE,
	PROP_GDRIVE,
	PROP_UDI
};

G_DEFINE_TYPE (BraseroDrive, brasero_drive, G_TYPE_OBJECT);

#define BRASERO_DRIVE_OPEN_ATTEMPTS			5

static void
brasero_drive_probe_inside (BraseroDrive *drive);

/**
 * brasero_drive_get_gdrive:
 * @drive: a #BraseroDrive
 *
 * Returns the #GDrive corresponding to this #BraseroDrive
 *
 * Return value: a #GDrive or NULL. Unref after use.
 **/
GDrive *
brasero_drive_get_gdrive (BraseroDrive *drive)
{
	BraseroDrivePrivate *priv;

	g_return_val_if_fail (drive != NULL, NULL);
	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), NULL);

	if (brasero_drive_is_fake (drive))
		return NULL;

	priv = BRASERO_DRIVE_PRIVATE (drive);

	if (!priv->gdrive)
		return NULL;

	return g_object_ref (priv->gdrive);
}

/**
 * brasero_drive_can_eject:
 * @drive: #BraseroDrive
 *
 * Returns whether the drive can eject media.
 *
 * Return value: a #gboolean. TRUE if the media can be ejected, FALSE otherwise.
 *
 **/
gboolean
brasero_drive_can_eject (BraseroDrive *drive)
{
	GVolume *volume;
	gboolean result;
	BraseroDrivePrivate *priv;

	g_return_val_if_fail (drive != NULL, FALSE);
	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), FALSE);

	priv = BRASERO_DRIVE_PRIVATE (drive);

	if (!priv->gdrive) {
		BRASERO_MEDIA_LOG ("No GDrive");
		goto last_resort;
	}

	if (!g_drive_can_eject (priv->gdrive)) {
		BRASERO_MEDIA_LOG ("GDrive can't eject");
		goto last_resort;
	}

	return TRUE;

last_resort:

	if (!priv->medium)
		return FALSE;

	/* last resort */
	volume = brasero_volume_get_gvolume (BRASERO_VOLUME (priv->medium));
	if (!volume)
		return FALSE;

	result = g_volume_can_eject (volume);
	g_object_unref (volume);

	return result;
}

static void
brasero_drive_cancel_probing (BraseroDrive *drive)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (drive);

	priv->probe_waiting = FALSE;

	g_mutex_lock (priv->mutex);
	if (priv->probe) {
		/* This to signal that we are cancelling */
		priv->probe_cancelled = TRUE;
		priv->initial_probe_cancelled = TRUE;

		/* This is to wake up the thread if it
		 * was asleep waiting to retry to get
		 * hold of a handle to probe the drive */
		g_cond_signal (priv->cond_probe);

		g_cond_wait (priv->cond, priv->mutex);
	}
	g_mutex_unlock (priv->mutex);

	if (priv->probe_id) {
		g_source_remove (priv->probe_id);
		priv->probe_id = 0;
	}
}

static void
brasero_drive_wait_probing_thread (BraseroDrive *drive)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (drive);

	g_mutex_lock (priv->mutex);
	if (priv->probe) {
		/* This is to wake up the thread if it
		 * was asleep waiting to retry to get
		 * hold of a handle to probe the drive */
		g_cond_signal (priv->cond_probe);
		g_cond_wait (priv->cond, priv->mutex);
	}
	g_mutex_unlock (priv->mutex);
}

/**
 * brasero_drive_eject:
 * @drive: #BraseroDrive
 * @wait: #gboolean whether to wait for the completion of the operation (with a GMainLoop)
 * @error: #GError
 *
 * Open the drive tray or ejects the media if there is any inside.
 *
 * Return value: a #gboolean. TRUE on success, FALSE otherwise.
 *
 **/
gboolean
brasero_drive_eject (BraseroDrive *drive,
		     gboolean wait,
		     GError **error)
{
	BraseroDrivePrivate *priv;
	GVolume *gvolume;
	gboolean res;

	g_return_val_if_fail (drive != NULL, FALSE);
	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), FALSE);

	priv = BRASERO_DRIVE_PRIVATE (drive);

	/* reset if needed */
	if (g_cancellable_is_cancelled (priv->cancel)) {
		BRASERO_MEDIA_LOG ("Resetting GCancellable object");
		g_cancellable_reset (priv->cancel);
	}

	BRASERO_MEDIA_LOG ("Trying to eject drive");
	if (priv->gdrive) {
		/* Wait for any ongoing probing as it
		 * would prevent the door from being
		 * opened. */
		brasero_drive_wait_probing_thread (drive);

		priv->ejecting = TRUE;
		res = brasero_gio_operation_eject_drive (priv->gdrive,
							 priv->cancel,
							 wait,
							 error);
		priv->ejecting = FALSE;
		if (priv->probe_waiting)
			brasero_drive_probe_inside (drive);

		if (res)
			return TRUE;

		if (g_cancellable_is_cancelled (priv->cancel))
			return FALSE;
	}
	else
		BRASERO_MEDIA_LOG ("No GDrive");

	if (!priv->medium)
		return FALSE;

	/* reset if needed */
	if (g_cancellable_is_cancelled (priv->cancel)) {
		BRASERO_MEDIA_LOG ("Resetting GCancellable object");
		g_cancellable_reset (priv->cancel);
	}

	gvolume = brasero_volume_get_gvolume (BRASERO_VOLUME (priv->medium));
	if (gvolume) {
		BRASERO_MEDIA_LOG ("Trying to eject volume");

		/* Cancel any ongoing probing as it
		 * would prevent the door from being
		 * opened. */
		brasero_drive_wait_probing_thread (drive);

		priv->ejecting = TRUE;
		res = brasero_gio_operation_eject_volume (gvolume,
							  priv->cancel,
							  wait,
							  error);

		priv->ejecting = FALSE;
		if (priv->probe_waiting)
			brasero_drive_probe_inside (drive);

		g_object_unref (gvolume);
	}

	return res;
}

/**
 * brasero_drive_cancel_current_operation:
 * @drive: #BraseroDrive *
 *
 * Cancels all operations currently running for @drive
 *
 **/
void
brasero_drive_cancel_current_operation (BraseroDrive *drive)
{
	BraseroDrivePrivate *priv;

	g_return_if_fail (drive != NULL);
	g_return_if_fail (BRASERO_IS_DRIVE (drive));

	priv = BRASERO_DRIVE_PRIVATE (drive);

	BRASERO_MEDIA_LOG ("Cancelling GIO operation");
	g_cancellable_cancel (priv->cancel);
}

/**
 * brasero_drive_get_bus_target_lun_string:
 * @drive: a #BraseroDrive
 *
 * Returns the bus, target, lun ("{bus},{target},{lun}") as a string which is
 * sometimes needed by some backends like cdrecord.
 *
 * NOTE: that function returns either bus/target/lun or the device path
 * according to OSes. Basically it returns bus/target/lun only for FreeBSD
 * which is the only OS in need for that. For all others it returns the device
 * path. 
 *
 * Return value: a string or NULL. The string must be freed when not needed
 *
 **/
gchar *
brasero_drive_get_bus_target_lun_string (BraseroDrive *drive)
{
	g_return_val_if_fail (drive != NULL, NULL);
	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), NULL);

	return brasero_device_get_bus_target_lun (brasero_drive_get_device (drive));
}

/**
 * brasero_drive_is_fake:
 * @drive: a #BraseroDrive
 *
 * Returns whether or not the drive is a fake one. There is only one and
 * corresponds to a file which is used when the user wants to burn to a file.
 *
 * Return value: %TRUE or %FALSE.
 **/
gboolean
brasero_drive_is_fake (BraseroDrive *drive)
{
	BraseroDrivePrivate *priv;

	g_return_val_if_fail (drive != NULL, FALSE);
	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), FALSE);

	priv = BRASERO_DRIVE_PRIVATE (drive);
	return (priv->device == NULL);
}

/**
 * brasero_drive_is_door_open:
 * @drive: a #BraseroDrive
 *
 * Returns whether or not the drive door is open.
 *
 * Return value: %TRUE or %FALSE.
 **/
gboolean
brasero_drive_is_door_open (BraseroDrive *drive)
{
	const gchar *device;
	BraseroDrivePrivate *priv;
	BraseroDeviceHandle *handle;
	BraseroScsiMechStatusHdr hdr;

	g_return_val_if_fail (drive != NULL, FALSE);
	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), FALSE);

	priv = BRASERO_DRIVE_PRIVATE (drive);
	if (!priv->device)
		return FALSE;

	device = brasero_drive_get_device (drive);
	handle = brasero_device_handle_open (device, FALSE, NULL);
	if (!handle)
		return FALSE;

	brasero_mmc1_mech_status (handle,
				  &hdr,
				  NULL);

	brasero_device_handle_close (handle);

	return hdr.door_open;
}

/**
 * brasero_drive_can_use_exclusively:
 * @drive: a #BraseroDrive
 *
 * Returns whether or not the drive can be used exclusively, that is whether or
 * not it is currently used by another application.
 *
 * Return value: %TRUE or %FALSE.
 **/
gboolean
brasero_drive_can_use_exclusively (BraseroDrive *drive)
{
	BraseroDeviceHandle *handle;
	const gchar *device;

	g_return_val_if_fail (drive != NULL, FALSE);
	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), FALSE);

	device = brasero_drive_get_device (drive);
	handle = brasero_device_handle_open (device, TRUE, NULL);
	if (!handle)
		return FALSE;

	brasero_device_handle_close (handle);
	return TRUE;
}

/**
 * brasero_drive_is_locked:
 * @drive: a #BraseroDrive
 * @reason: a #gchar or NULL. A string to indicate what the drive was locked for if return value is %TRUE
 *
 * Checks whether a #BraseroDrive is currently locked. Manual ejection shouldn't be possible any more.
 *
 * Since 2.29.0
 *
 * Return value: %TRUE if the drive is locked or %FALSE.
 **/
gboolean
brasero_drive_is_locked (BraseroDrive *drive,
                         gchar **reason)
{
	BraseroDrivePrivate *priv;

	g_return_val_if_fail (drive != NULL, FALSE);
	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), FALSE);

	priv = BRASERO_DRIVE_PRIVATE (drive);
	return priv->locked;
}

/**
 * brasero_drive_lock:
 * @drive: a #BraseroDrive
 * @reason: a string to indicate what the drive was locked for
 * @reason_for_failure: a string (or NULL) to hold the reason why the locking failed
 *
 * Locks a #BraseroDrive. Manual ejection shouldn't be possible any more.
 *
 * Return value: %TRUE if the drive was successfully locked or %FALSE.
 **/
gboolean
brasero_drive_lock (BraseroDrive *drive,
		    const gchar *reason,
		    gchar **reason_for_failure)
{
	BraseroDeviceHandle *handle;
	BraseroDrivePrivate *priv;
	const gchar *device;
	gboolean result;

	g_return_val_if_fail (drive != NULL, FALSE);
	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), FALSE);

	priv = BRASERO_DRIVE_PRIVATE (drive);
	if (!priv->device)
		return FALSE;

	device = brasero_drive_get_device (drive);
	handle = brasero_device_handle_open (device, FALSE, NULL);
	if (!handle)
		return FALSE;

	result = (brasero_sbc_medium_removal (handle, 1, NULL) == BRASERO_SCSI_OK);
	if (result) {
		BRASERO_MEDIA_LOG ("Device locked");
		priv->locked = TRUE;
	}
	else
		BRASERO_MEDIA_LOG ("Device failed to lock");

	brasero_device_handle_close (handle);
	return result;
}

/**
 * brasero_drive_unlock:
 * @drive: a #BraseroDrive
 *
 * Unlocks a #BraseroDrive.
 *
 * Return value: %TRUE if the drive was successfully unlocked or %FALSE.
 **/
gboolean
brasero_drive_unlock (BraseroDrive *drive)
{
	BraseroDeviceHandle *handle;
	BraseroDrivePrivate *priv;
	const gchar *device;
	gboolean result;

	g_return_val_if_fail (drive != NULL, FALSE);
	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), FALSE);

	priv = BRASERO_DRIVE_PRIVATE (drive);
	if (!priv->device)
		return FALSE;

	device = brasero_drive_get_device (drive);
	handle = brasero_device_handle_open (device, FALSE, NULL);
	if (!handle)
		return FALSE;

	result = (brasero_sbc_medium_removal (handle, 0, NULL) == BRASERO_SCSI_OK);
	if (result) {
		BRASERO_MEDIA_LOG ("Device unlocked");
		priv->locked = FALSE;

		if (priv->probe_waiting) {
			BRASERO_MEDIA_LOG ("Probe on hold");

			/* A probe was waiting */
			brasero_drive_probe_inside (drive);
		}
	}
	else
		BRASERO_MEDIA_LOG ("Device failed to unlock");

	brasero_device_handle_close (handle);

	return result;
}

/**
 * brasero_drive_get_display_name:
 * @drive: a #BraseroDrive
 *
 * Gets a string holding the name for the drive. That string can be then
 * displayed in a user interface.
 *
 * Return value: a string holding the name
 **/
gchar *
brasero_drive_get_display_name (BraseroDrive *drive)
{
	BraseroDrivePrivate *priv;

	g_return_val_if_fail (drive != NULL, NULL);
	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), NULL);

	priv = BRASERO_DRIVE_PRIVATE (drive);
	if (!priv->device) {
		/* Translators: This is a fake drive, a file, and means that
		 * when we're writing, we're writing to a file and create an
		 * image on the hard drive. */
		return g_strdup (_("Image File"));
	}

	return g_strdup (priv->name);
}

/**
 * brasero_drive_get_device:
 * @drive: a #BraseroDrive
 *
 * Gets a string holding the device path for the drive.
 *
 * Return value: a string holding the device path.
 * On Solaris returns raw device.
 **/
const gchar *
brasero_drive_get_device (BraseroDrive *drive)
{
	BraseroDrivePrivate *priv;

	g_return_val_if_fail (drive != NULL, NULL);
	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), NULL);

	priv = BRASERO_DRIVE_PRIVATE (drive);
	return priv->device;
}

/**
 * brasero_drive_get_block_device:
 * @drive: a #BraseroDrive
 *
 * Gets a string holding the block device path for the drive. This can be used on
 * some other OSes, like Solaris, for GIO operations instead of the device
 * path.
 *
 * Solaris uses block device for GIO operations and
 * uses raw device for system calls and backends
 * like cdrtool.
 *
 * If such a path is not available, it returns the device path.
 *
 * Return value: a string holding the block device path
 **/
const gchar *
brasero_drive_get_block_device (BraseroDrive *drive)
{
	BraseroDrivePrivate *priv;

	g_return_val_if_fail (drive != NULL, NULL);
	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), NULL);

	priv = BRASERO_DRIVE_PRIVATE (drive);
	return priv->block_device? priv->block_device:priv->device;
}

/**
 * brasero_drive_get_udi:
 * @drive: a #BraseroDrive
 *
 * Gets a string holding the HAL udi corresponding to this device. It can be used
 * to uniquely identify the drive.
 *
 * Return value: a string holding the HAL udi or NULL. Not to be freed
 **/
const gchar *
brasero_drive_get_udi (BraseroDrive *drive)
{
	BraseroDrivePrivate *priv;

	if (!drive)
		return NULL;

	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), NULL);

	priv = BRASERO_DRIVE_PRIVATE (drive);
	if (!priv->device || !priv->gdrive)
		return NULL;

	if (priv->udi)
		return priv->udi;

	priv->udi = g_drive_get_identifier (priv->gdrive, G_VOLUME_IDENTIFIER_KIND_HAL_UDI);
	return priv->udi;
}

/**
 * brasero_drive_get_medium:
 * @drive: a #BraseroDrive
 *
 * Gets the medium currently inserted in the drive. If there is no medium or if
 * the medium is not probed yet then it returns NULL.
 *
 * Return value: (transfer none): a #BraseroMedium object or NULL. No need to unref after use.
 **/
BraseroMedium *
brasero_drive_get_medium (BraseroDrive *drive)
{
	BraseroDrivePrivate *priv;

	if (!drive)
		return NULL;

	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), NULL);

	priv = BRASERO_DRIVE_PRIVATE (drive);
	if (brasero_drive_probing (drive))
		return NULL;

	return priv->medium;
}

/**
 * brasero_drive_get_caps:
 * @drive: a #BraseroDrive
 *
 * Returns what type(s) of disc the drive can write to.
 *
 * Return value: a #BraseroDriveCaps.
 **/
BraseroDriveCaps
brasero_drive_get_caps (BraseroDrive *drive)
{
	BraseroDrivePrivate *priv;

	g_return_val_if_fail (drive != NULL, BRASERO_DRIVE_CAPS_NONE);
	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), BRASERO_DRIVE_CAPS_NONE);

	priv = BRASERO_DRIVE_PRIVATE (drive);
	return priv->caps;
}

/**
 * brasero_drive_can_write_media:
 * @drive: a #BraseroDrive
 * @media: a #BraseroMedia
 *
 * Returns whether the disc can burn a specific media type.
 *
 * Since 2.29.0
 *
 * Return value: a #gboolean. TRUE if the drive can write this type of media and FALSE otherwise
 **/
gboolean
brasero_drive_can_write_media (BraseroDrive *drive,
                               BraseroMedia media)
{
	BraseroDrivePrivate *priv;

	g_return_val_if_fail (drive != NULL, FALSE);
	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), FALSE);

	priv = BRASERO_DRIVE_PRIVATE (drive);

	if (!(media & BRASERO_MEDIUM_REWRITABLE)
	&&   (media & BRASERO_MEDIUM_CLOSED))
		return FALSE;

	if (media & BRASERO_MEDIUM_FILE)
		return FALSE;

	if (BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_CDR))
		return (priv->caps & BRASERO_DRIVE_CAPS_CDR) != 0;

	if (BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDR))
		return (priv->caps & BRASERO_DRIVE_CAPS_DVDR) != 0;

	if (BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDR_PLUS))
		return (priv->caps & BRASERO_DRIVE_CAPS_DVDR_PLUS) != 0;

	if (BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_CDRW))
		return (priv->caps & BRASERO_DRIVE_CAPS_CDRW) != 0;

	if (BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDRW))
		return (priv->caps & BRASERO_DRIVE_CAPS_DVDRW) != 0;

	if (BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDRW_RESTRICTED))
		return (priv->caps & BRASERO_DRIVE_CAPS_DVDRW) != 0;

	if (BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDRW_PLUS))
		return (priv->caps & BRASERO_DRIVE_CAPS_DVDRW_PLUS) != 0;

	if (BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDR_PLUS_DL))
		return (priv->caps & BRASERO_DRIVE_CAPS_DVDR_PLUS_DL) != 0;

	if (BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDRW_PLUS_DL))
		return (priv->caps & BRASERO_DRIVE_CAPS_DVDRW_PLUS_DL) != 0;

	if (BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVD_RAM))
		return (priv->caps & BRASERO_DRIVE_CAPS_DVDRAM) != 0;

	/* All types of BD-R */
	if (BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_BD|BRASERO_MEDIUM_WRITABLE))
		return (priv->caps & BRASERO_DRIVE_CAPS_BDR) != 0;

	if (BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_BDRE))
		return (priv->caps & BRASERO_DRIVE_CAPS_BDRW) != 0;

	return FALSE;
}

/**
 * brasero_drive_can_write:
 * @drive: a #BraseroDrive
 *
 * Returns whether the disc can burn any disc at all.
 *
 * Return value: a #gboolean. TRUE if the drive can write a disc and FALSE otherwise
 **/
gboolean
brasero_drive_can_write (BraseroDrive *drive)
{
	BraseroDrivePrivate *priv;

	g_return_val_if_fail (drive != NULL, FALSE);
	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), FALSE);

	priv = BRASERO_DRIVE_PRIVATE (drive);
	return (priv->caps & (BRASERO_DRIVE_CAPS_CDR|
			      BRASERO_DRIVE_CAPS_DVDR|
			      BRASERO_DRIVE_CAPS_DVDR_PLUS|
			      BRASERO_DRIVE_CAPS_CDRW|
			      BRASERO_DRIVE_CAPS_DVDRW|
			      BRASERO_DRIVE_CAPS_DVDRW_PLUS|
			      BRASERO_DRIVE_CAPS_DVDR_PLUS_DL|
			      BRASERO_DRIVE_CAPS_DVDRW_PLUS_DL));
}

static void
brasero_drive_medium_probed (BraseroMedium *medium,
			     BraseroDrive *self)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (self);

	/* only when it is probed */
	/* NOTE: BraseroMedium calls GDK_THREADS_ENTER/LEAVE() around g_signal_emit () */
	if (brasero_medium_get_status (priv->medium) == BRASERO_MEDIUM_NONE) {
		g_object_unref (priv->medium);
		priv->medium = NULL;
		return;
	}

	g_signal_emit (self,
		       drive_signals [MEDIUM_INSERTED],
		       0,
		       priv->medium);
}

/**
 * This is not public API. Defined in brasero-drive-priv.h.
 */
gboolean
brasero_drive_probing (BraseroDrive *drive)
{
	BraseroDrivePrivate *priv;

	g_return_val_if_fail (drive != NULL, FALSE);
	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), FALSE);

	priv = BRASERO_DRIVE_PRIVATE (drive);
	if (priv->probe != NULL)
		return TRUE;

	if (priv->medium)
		return brasero_medium_probing (priv->medium);

	return FALSE;
}

static void
brasero_drive_update_medium (BraseroDrive *drive)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (drive);

	if (priv->has_medium) {
		if (priv->medium) {
			BRASERO_MEDIA_LOG ("Already a medium. Skipping");
			return;
		}

		BRASERO_MEDIA_LOG ("Probing new medium");
		priv->medium = g_object_new (BRASERO_TYPE_VOLUME,
					     "drive", drive,
					     NULL);

		g_signal_connect (priv->medium,
				  "probed",
				  G_CALLBACK (brasero_drive_medium_probed),
				  drive);
	}
	else if (priv->medium) {
		BraseroMedium *medium;

		BRASERO_MEDIA_LOG ("Medium removed");

		medium = priv->medium;
		priv->medium = NULL;

		g_signal_emit (drive,
			       drive_signals [MEDIUM_REMOVED],
			       0,
			       medium);

		g_object_unref (medium);
	}
}

static gboolean
brasero_drive_probed_inside (gpointer data)
{
	BraseroDrive *self;
	BraseroDrivePrivate *priv;

	self = BRASERO_DRIVE (data);
	priv = BRASERO_DRIVE_PRIVATE (self);

	if (!g_mutex_trylock (priv->mutex))
		return TRUE;

	priv->probe_id = 0;
	g_mutex_unlock (priv->mutex);

	brasero_drive_update_medium (self);
	return FALSE;
}

static gpointer
brasero_drive_probe_inside_thread (gpointer data)
{
	gint counter = 0;
	GTimeVal wait_time;
	const gchar *device;
	BraseroScsiErrCode code;
	BraseroDrivePrivate *priv;
	BraseroDeviceHandle *handle = NULL;
	BraseroDrive *drive = BRASERO_DRIVE (data);

	priv = BRASERO_DRIVE_PRIVATE (drive);

	/* the drive might be busy (a burning is going on) so we don't block
	 * but we re-try to open it every second */
	device = brasero_drive_get_device (drive);
	BRASERO_MEDIA_LOG ("Trying to open device %s", device);

	priv->has_medium = FALSE;

	handle = brasero_device_handle_open (device, FALSE, &code);
	while (!handle && counter <= BRASERO_DRIVE_OPEN_ATTEMPTS) {
		sleep (1);

		if (priv->probe_cancelled) {
			BRASERO_MEDIA_LOG ("Open () cancelled");
			goto end;
		}

		counter ++;
		handle = brasero_device_handle_open (device, FALSE, &code);
	}

	if (!handle) {
		BRASERO_MEDIA_LOG ("Open () failed: medium busy");
		goto end;
	}

	if (priv->probe_cancelled) {
		BRASERO_MEDIA_LOG ("Open () cancelled");

		brasero_device_handle_close (handle);
		goto end;
	}

	while (brasero_spc1_test_unit_ready (handle, &code) != BRASERO_SCSI_OK) {
		if (code == BRASERO_SCSI_NO_MEDIUM) {
			BRASERO_MEDIA_LOG ("No medium inserted");

			brasero_device_handle_close (handle);
			goto end;
		}

		if (code != BRASERO_SCSI_NOT_READY) {
			BRASERO_MEDIA_LOG ("Device does not respond");

			brasero_device_handle_close (handle);
			goto end;
		}

		g_get_current_time (&wait_time);
		g_time_val_add (&wait_time, 2000000);

		g_mutex_lock (priv->mutex);
		g_cond_timed_wait (priv->cond_probe,
		                   priv->mutex,
		                   &wait_time);
		g_mutex_unlock (priv->mutex);

		if (priv->probe_cancelled) {
			BRASERO_MEDIA_LOG ("Device probing cancelled");

			brasero_device_handle_close (handle);
			goto end;
		}
	}

	BRASERO_MEDIA_LOG ("Medium inserted");
	brasero_device_handle_close (handle);

	priv->has_medium = TRUE;

end:

	g_mutex_lock (priv->mutex);

	if (!priv->probe_cancelled)
		priv->probe_id = g_idle_add (brasero_drive_probed_inside, drive);

	priv->probe = NULL;
	g_cond_broadcast (priv->cond);
	g_mutex_unlock (priv->mutex);

	g_thread_exit (0);

	return NULL;
}

static void
brasero_drive_probe_inside (BraseroDrive *drive)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (drive);

	if (priv->initial_probe) {
		BRASERO_MEDIA_LOG ("Still initializing the drive properties");
		return;
	}

	/* Check that a probe is not already being performed */
	if (priv->probe) {
		BRASERO_MEDIA_LOG ("Ongoing probe");
		brasero_drive_cancel_probing (drive);
	}

	BRASERO_MEDIA_LOG ("Setting new probe");

	g_mutex_lock (priv->mutex);

	priv->probe_waiting = FALSE;
	priv->probe_cancelled = FALSE;

	priv->probe = g_thread_create (brasero_drive_probe_inside_thread,
	                               drive,
				       FALSE,
				       NULL);

	g_mutex_unlock (priv->mutex);
}

static void
brasero_drive_medium_gdrive_changed_cb (BraseroDrive *gdrive,
					BraseroDrive *drive)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (drive);
	if (priv->locked || priv->ejecting) {
		BRASERO_MEDIA_LOG ("Waiting for next unlocking of the drive to probe");

		/* Since the drive was locked, it should
		 * not be possible that the medium
		 * actually changed.
		 * This allows to avoid probing while
		 * we are burning something.
		 * Delay the probe until brasero_drive_unlock ()
		 * is called.  */
		priv->probe_waiting = TRUE;
		return;
	}

	BRASERO_MEDIA_LOG ("GDrive changed");
	brasero_drive_probe_inside (drive);
}

static void
brasero_drive_update_gdrive (BraseroDrive *drive,
                             GDrive *gdrive)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (drive);
	if (priv->gdrive) {
		g_signal_handlers_disconnect_by_func (priv->gdrive,
						      brasero_drive_medium_gdrive_changed_cb,
						      drive);

		/* Stop any ongoing GIO operation */
		g_cancellable_cancel (priv->cancel);
	
		g_object_unref (priv->gdrive);
		priv->gdrive = NULL;
	}

	BRASERO_MEDIA_LOG ("Setting GDrive %p", gdrive);

	if (gdrive) {
		priv->gdrive = g_object_ref (gdrive);

		/* If it's not a fake drive then connect to signal for any
		 * change and check medium inside */
		g_signal_connect (priv->gdrive,
				  "changed",
				  G_CALLBACK (brasero_drive_medium_gdrive_changed_cb),
				  drive);
	}

	if (priv->locked || priv->ejecting) {
		BRASERO_MEDIA_LOG ("Waiting for next unlocking of the drive to probe");

		/* Since the drive was locked, it should
		 * not be possible that the medium
		 * actually changed.
		 * This allows to avoid probing while
		 * we are burning something.
		 * Delay the probe until brasero_drive_unlock ()
		 * is called.  */
		priv->probe_waiting = TRUE;
		return;
	}

	brasero_drive_probe_inside (drive);
}

/**
 * brasero_drive_reprobe:
 * @drive: a #BraseroDrive
 *
 * Reprobes the drive contents. Useful when an operation has just been performed
 * (blanking, burning, ...) and medium status should be updated.
 *
 * NOTE: This operation does not block.
 *
 **/

void
brasero_drive_reprobe (BraseroDrive *drive)
{
	BraseroDrivePrivate *priv;
	BraseroMedium *medium;

	g_return_if_fail (drive != NULL);
	g_return_if_fail (BRASERO_IS_DRIVE (drive));

	priv = BRASERO_DRIVE_PRIVATE (drive);
	
	if (priv->gdrive) {
		/* reprobe the contents of the drive system wide */
		g_drive_poll_for_media (priv->gdrive, NULL, NULL, NULL);
	}

	priv->probe_waiting = FALSE;

	BRASERO_MEDIA_LOG ("Reprobing inserted medium");
	if (priv->medium) {
		/* remove current medium */
		medium = priv->medium;
		priv->medium = NULL;

		g_signal_emit (drive,
			       drive_signals [MEDIUM_REMOVED],
			       0,
			       medium);
		g_object_unref (medium);
	}

	brasero_drive_probe_inside (drive);
}

static gboolean
brasero_drive_get_caps_profiles (BraseroDrive *self,
                                 BraseroDeviceHandle *handle,
                                 BraseroScsiErrCode *code)
{
	BraseroScsiGetConfigHdr *hdr = NULL;
	BraseroScsiProfileDesc *profiles;
	BraseroScsiFeatureDesc *desc;
	BraseroDrivePrivate *priv;
	BraseroScsiResult result;
	int profiles_num;
	int size;

	priv = BRASERO_DRIVE_PRIVATE (self);

	BRASERO_MEDIA_LOG ("Checking supported profiles");
	result = brasero_mmc2_get_configuration_feature (handle,
	                                                 BRASERO_SCSI_FEAT_PROFILES,
	                                                 &hdr,
	                                                 &size,
	                                                 code);
	if (result != BRASERO_SCSI_OK) {
		BRASERO_MEDIA_LOG ("GET CONFIGURATION failed");
		return FALSE;
	}

	BRASERO_MEDIA_LOG ("Dectected medium is 0x%x", BRASERO_GET_16 (hdr->current_profile));

	/* Go through all features available */
	desc = hdr->desc;
	profiles = (BraseroScsiProfileDesc *) desc->data;
	profiles_num = desc->add_len / sizeof (BraseroScsiProfileDesc);

	while (profiles_num) {
		switch (BRASERO_GET_16 (profiles->number)) {
			case BRASERO_SCSI_PROF_CDR:
				priv->caps |= BRASERO_DRIVE_CAPS_CDR;
				break;
			case BRASERO_SCSI_PROF_CDRW:
				priv->caps |= BRASERO_DRIVE_CAPS_CDRW;
				break;
			case BRASERO_SCSI_PROF_DVD_R: 
				priv->caps |= BRASERO_DRIVE_CAPS_DVDR;
				break;
			case BRASERO_SCSI_PROF_DVD_RW_SEQUENTIAL: 
			case BRASERO_SCSI_PROF_DVD_RW_RESTRICTED: 
				priv->caps |= BRASERO_DRIVE_CAPS_DVDRW;
				break;
			case BRASERO_SCSI_PROF_DVD_RAM: 
				priv->caps |= BRASERO_DRIVE_CAPS_DVDRAM;
				break;
			case BRASERO_SCSI_PROF_DVD_R_PLUS_DL:
				priv->caps |= BRASERO_DRIVE_CAPS_DVDR_PLUS_DL;
				break;
			case BRASERO_SCSI_PROF_DVD_RW_PLUS_DL:
				priv->caps |= BRASERO_DRIVE_CAPS_DVDRW_PLUS_DL;
				break;
			case BRASERO_SCSI_PROF_DVD_R_PLUS:
				priv->caps |= BRASERO_DRIVE_CAPS_DVDR_PLUS;
				break;
			case BRASERO_SCSI_PROF_DVD_RW_PLUS:
				priv->caps |= BRASERO_DRIVE_CAPS_DVDRW_PLUS;
				break;
			case BRASERO_SCSI_PROF_BR_R_SEQUENTIAL:
			case BRASERO_SCSI_PROF_BR_R_RANDOM:
				priv->caps |= BRASERO_DRIVE_CAPS_BDR;
				break;
			case BRASERO_SCSI_PROF_BD_RW:
				priv->caps |= BRASERO_DRIVE_CAPS_BDRW;
				break;
			default:
				break;
		}

		if (priv->initial_probe_cancelled)
			break;

		/* Move the pointer to the next features */
		profiles ++;
		profiles_num --;
	}

	g_free (hdr);
	return TRUE;
}

static void
brasero_drive_get_caps_2A (BraseroDrive *self,
                           BraseroDeviceHandle *handle,
                           BraseroScsiErrCode *code)
{
	BraseroScsiStatusPage *page_2A = NULL;
	BraseroScsiModeData *data = NULL;
	BraseroDrivePrivate *priv;
	BraseroScsiResult result;
	int size = 0;

	priv = BRASERO_DRIVE_PRIVATE (self);

	result = brasero_spc1_mode_sense_get_page (handle,
						   BRASERO_SPC_PAGE_STATUS,
						   &data,
						   &size,
						   code);
	if (result != BRASERO_SCSI_OK) {
		BRASERO_MEDIA_LOG ("MODE SENSE failed");
		return;
	}

	page_2A = (BraseroScsiStatusPage *) &data->page;

	if (page_2A->wr_CDR != 0)
		priv->caps |= BRASERO_DRIVE_CAPS_CDR;
	if (page_2A->wr_CDRW != 0)
		priv->caps |= BRASERO_DRIVE_CAPS_CDRW;
	if (page_2A->wr_DVDR != 0)
		priv->caps |= BRASERO_DRIVE_CAPS_DVDR;
	if (page_2A->wr_DVDRAM != 0)
		priv->caps |= BRASERO_DRIVE_CAPS_DVDRAM;

	g_free (data);
}

static gpointer
brasero_drive_probe_thread (gpointer data)
{
	gint counter = 0;
	GTimeVal wait_time;
	const gchar *device;
	BraseroScsiResult res;
	BraseroScsiInquiry hdr;
	BraseroScsiErrCode code;
	BraseroDrivePrivate *priv;
	BraseroDeviceHandle *handle;
	BraseroDrive *drive = BRASERO_DRIVE (data);

	priv = BRASERO_DRIVE_PRIVATE (drive);

	/* the drive might be busy (a burning is going on) so we don't block
	 * but we re-try to open it every second */
	device = brasero_drive_get_device (drive);
	BRASERO_MEDIA_LOG ("Trying to open device %s", device);

	handle = brasero_device_handle_open (device, FALSE, &code);
	while (!handle && counter <= BRASERO_DRIVE_OPEN_ATTEMPTS) {
		sleep (1);

		if (priv->initial_probe_cancelled) {
			BRASERO_MEDIA_LOG ("Open () cancelled");
			goto end;
		}

		counter ++;
		handle = brasero_device_handle_open (device, FALSE, &code);
	}

	if (priv->initial_probe_cancelled) {
		BRASERO_MEDIA_LOG ("Open () cancelled");
		goto end;
	}

	if (!handle) {
		BRASERO_MEDIA_LOG ("Open () failed: medium busy");
		goto end;
	}

	while (brasero_spc1_test_unit_ready (handle, &code) != BRASERO_SCSI_OK) {
		if (code == BRASERO_SCSI_NO_MEDIUM) {
			BRASERO_MEDIA_LOG ("No medium inserted");
			goto capabilities;
		}

		if (code != BRASERO_SCSI_NOT_READY) {
			brasero_device_handle_close (handle);
			BRASERO_MEDIA_LOG ("Device does not respond");
			goto end;
		}

		g_get_current_time (&wait_time);
		g_time_val_add (&wait_time, 2000000);

		g_mutex_lock (priv->mutex);
		g_cond_timed_wait (priv->cond_probe,
		                   priv->mutex,
		                   &wait_time);
		g_mutex_unlock (priv->mutex);

		if (priv->initial_probe_cancelled) {
			brasero_device_handle_close (handle);
			BRASERO_MEDIA_LOG ("Device probing cancelled");
			goto end;
		}
	}

	BRASERO_MEDIA_LOG ("Device ready");
	priv->has_medium = TRUE;

capabilities:

	/* get additional information like the name */
	res = brasero_spc1_inquiry (handle, &hdr, NULL);
	if (res == BRASERO_SCSI_OK) {
		gchar *name_utf8;
		gchar *vendor;
		gchar *model;
		gchar *name;

		vendor = g_strndup ((gchar *) hdr.vendor, sizeof (hdr.vendor));
		model = g_strndup ((gchar *) hdr.name, sizeof (hdr.name));
		name = g_strdup_printf ("%s %s", g_strstrip (vendor), g_strstrip (model));
		g_free (vendor);
		g_free (model);

		/* make sure that's proper UTF-8 */
		name_utf8 = g_convert_with_fallback (name,
		                                     -1,
		                                     "ASCII",
		                                     "UTF-8",
		                                     "_",
		                                     NULL,
		                                     NULL,
		                                     NULL);
		g_free (name);

		priv->name = name_utf8;
	}

	/* Get supported medium types */
	if (!brasero_drive_get_caps_profiles (drive, handle, &code))
		brasero_drive_get_caps_2A (drive, handle, &code);

	brasero_device_handle_close (handle);

	BRASERO_MEDIA_LOG ("Drive caps are %d", priv->caps);

end:

	g_mutex_lock (priv->mutex);

	brasero_drive_update_medium (drive);

	priv->probe = NULL;
	priv->initial_probe = FALSE;

	g_cond_broadcast (priv->cond);
	g_mutex_unlock (priv->mutex);

	g_thread_exit (0);

	return NULL;
}

static void
brasero_drive_init_real_device (BraseroDrive *drive,
                                const gchar *device)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (drive);

#if defined(HAVE_STRUCT_USCSI_CMD)
	/* On Solaris path points to raw device, block_path points to the block device. */
	g_assert(g_str_has_prefix(device, "/dev/dsk/"));
	priv->device = g_strdup_printf ("/dev/rdsk/%s", device + 9);
	priv->block_device = g_strdup (device);
	BRASERO_MEDIA_LOG ("Initializing block drive %s", priv->block_device);
#else
	priv->device = g_strdup (device);
#endif

	BRASERO_MEDIA_LOG ("Initializing drive %s from device", priv->device);

	/* NOTE: why a thread? Because in case of a damaged medium, brasero can
	 * block on some functions until timeout and if we do this in the main
	 * thread then our whole UI blocks. This medium won't be exported by the
	 * BraseroDrive that exported until it returns PROBED signal.
	 * One (good) side effect is that it also improves start time. */
	g_mutex_lock (priv->mutex);

	priv->initial_probe = TRUE;
	priv->probe = g_thread_create (brasero_drive_probe_thread,
				       drive,
				       FALSE,
				       NULL);

	g_mutex_unlock (priv->mutex);
}

static void
brasero_drive_set_property (GObject *object,
			    guint prop_id,
			    const GValue *value,
			    GParamSpec *pspec)
{
	BraseroDrivePrivate *priv;
	GDrive *gdrive = NULL;

	g_return_if_fail (BRASERO_IS_DRIVE (object));

	priv = BRASERO_DRIVE_PRIVATE (object);

	switch (prop_id)
	{
	case PROP_UDI:
		break;
	case PROP_GDRIVE:
		if (!priv->device)
			break;

		gdrive = g_value_get_object (value);
		brasero_drive_update_gdrive (BRASERO_DRIVE (object), gdrive);
		break;
	case PROP_DEVICE:
		/* The first case is only a fake drive/medium */
		if (!g_value_get_string (value))
			priv->medium = g_object_new (BRASERO_TYPE_VOLUME,
						     "drive", object,
						     NULL);
		else
			brasero_drive_init_real_device (BRASERO_DRIVE (object), g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
brasero_drive_get_property (GObject *object,
			    guint prop_id,
			    GValue *value,
			    GParamSpec *pspec)
{
	BraseroDrivePrivate *priv;

	g_return_if_fail (BRASERO_IS_DRIVE (object));

	priv = BRASERO_DRIVE_PRIVATE (object);

	switch (prop_id)
	{
	case PROP_UDI:
		break;
	case PROP_GDRIVE:
		g_value_set_object (value, priv->gdrive);
		break;
	case PROP_DEVICE:
		g_value_set_string (value, priv->device);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
brasero_drive_init (BraseroDrive *object)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (object);
	priv->cancel = g_cancellable_new ();

	priv->mutex = g_mutex_new ();
	priv->cond = g_cond_new ();
	priv->cond_probe = g_cond_new ();
}

static void
brasero_drive_finalize (GObject *object)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (object);

	BRASERO_MEDIA_LOG ("Finalizing BraseroDrive");

	brasero_drive_cancel_probing (BRASERO_DRIVE (object));

	if (priv->mutex) {
		g_mutex_free (priv->mutex);
		priv->mutex = NULL;
	}

	if (priv->cond) {
		g_cond_free (priv->cond);
		priv->cond = NULL;
	}

	if (priv->cond_probe) {
		g_cond_free (priv->cond_probe);
		priv->cond_probe = NULL;
	}

	if (priv->medium) {
		g_signal_emit (object,
			       drive_signals [MEDIUM_REMOVED],
			       0,
			       priv->medium);
		g_object_unref (priv->medium);
		priv->medium = NULL;
	}

	if (priv->name) {
		g_free (priv->name);
		priv->name = NULL;
	}

	if (priv->device) {
		g_free (priv->device);
		priv->device = NULL;
	}

	if (priv->block_device) {
		g_free (priv->block_device);
		priv->block_device = NULL;
	}

	if (priv->udi) {
		g_free (priv->udi);
		priv->udi = NULL;
	}

	if (priv->gdrive) {
		g_signal_handlers_disconnect_by_func (priv->gdrive,
						      brasero_drive_medium_gdrive_changed_cb,
						      object);
		g_object_unref (priv->gdrive);
		priv->gdrive = NULL;
	}

	if (priv->cancel) {
		g_cancellable_cancel (priv->cancel);
		g_object_unref (priv->cancel);
		priv->cancel = NULL;
	}

	G_OBJECT_CLASS (brasero_drive_parent_class)->finalize (object);
}

static void
brasero_drive_class_init (BraseroDriveClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroDrivePrivate));

	object_class->finalize = brasero_drive_finalize;
	object_class->set_property = brasero_drive_set_property;
	object_class->get_property = brasero_drive_get_property;

	/**
 	* BraseroDrive::medium-added:
 	* @drive: the object which received the signal
  	* @medium: the new medium which was added
	*
 	* This signal gets emitted when a new medium was detected
 	*
 	*/
	drive_signals[MEDIUM_INSERTED] =
		g_signal_new ("medium_added",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
		              G_STRUCT_OFFSET (BraseroDriveClass, medium_added),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE, 1,
		              BRASERO_TYPE_MEDIUM);

	/**
 	* BraseroDrive::medium-removed:
 	* @drive: the object which received the signal
  	* @medium: the medium which was removed
	*
 	* This signal gets emitted when a medium is not longer available
 	*
 	*/
	drive_signals[MEDIUM_REMOVED] =
		g_signal_new ("medium_removed",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
		              G_STRUCT_OFFSET (BraseroDriveClass, medium_removed),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE, 1,
		              BRASERO_TYPE_MEDIUM);

	g_object_class_install_property (object_class,
	                                 PROP_UDI,
	                                 g_param_spec_string("udi",
	                                                     "udi",
	                                                     "HAL udi as a string (Deprecated)",
	                                                     NULL,
	                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class,
	                                 PROP_GDRIVE,
	                                 g_param_spec_object ("gdrive",
	                                                      "GDrive",
	                                                      "A GDrive object for the drive",
	                                                      G_TYPE_DRIVE,
	                                                     G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
	                                 PROP_DEVICE,
	                                 g_param_spec_string ("device",
	                                                      "Device",
	                                                      "Device path for the drive",
	                                                      NULL,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

