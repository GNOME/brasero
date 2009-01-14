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

#include <unistd.h>
#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include <libhal.h>
#include <gio/gio.h>

#include "brasero-media-private.h"

#include "brasero-medium.h"
#include "brasero-volume.h"
#include "brasero-drive.h"
#include "burn-hal-watch.h"

#include "scsi-mmc1.h"

#if defined(HAVE_STRUCT_USCSI_CMD)
#define DEVICE_MODEL	"info.product"
#define BLOCK_DEVICE	"block.solaris.raw_device"
#else
#define DEVICE_MODEL	"storage.model"
#define BLOCK_DEVICE	"block.device"
#endif

typedef struct _BraseroDrivePrivate BraseroDrivePrivate;
struct _BraseroDrivePrivate
{
	BraseroMedium *medium;
	BraseroDriveCaps caps;
	gchar *path;
	gchar *block_path;
	gchar *udi;

	gint bus;
	gint target;
	gint lun;

	gulong hal_sig;

	guint probed:1;
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
	PROP_UDI
};

G_DEFINE_TYPE (BraseroDrive, brasero_drive, G_TYPE_OBJECT);

/**
 * This is private API. The function is defined in brasero-volume.c
 */
BraseroVolume *
brasero_volume_new (BraseroDrive *drive, const gchar *udi);

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
	const gchar *volume_path = NULL;
	GVolumeMonitor *monitor;
	GDrive *gdrive = NULL;
	GList *drives;
	GList *iter;

	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), NULL);

	if (brasero_drive_is_fake (drive))
		return NULL;

#if defined(HAVE_STRUCT_USCSI_CMD)
	volume_path = brasero_drive_get_block_device (drive);
#else
	volume_path = brasero_drive_get_device (drive);
#endif

	/* NOTE: medium-monitor already holds a reference for GVolumeMonitor */
	monitor = g_volume_monitor_get ();
	drives = g_volume_monitor_get_connected_drives (monitor);
	g_object_unref (monitor);

	for (iter = drives; iter; iter = iter->next) {
		gchar *device_path;
		GDrive *tmp;

		tmp = iter->data;
		device_path = g_drive_get_identifier (tmp, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
		if (!device_path)
			continue;

		BRASERO_MEDIA_LOG ("Found drive %s", device_path);
		if (!strcmp (device_path, volume_path)) {
			gdrive = tmp;
			g_free (device_path);
			g_object_ref (gdrive);
			break;
		}

		g_free (device_path);
	}
	g_list_foreach (drives, (GFunc) g_object_unref, NULL);
	g_list_free (drives);

	if (!drive)
		BRASERO_MEDIA_LOG ("No drive found for medium");

	return gdrive;
}

/**
 * brasero_drive_get_bus_target_lun_string:
 * @drive: a #BraseroDrive
 *
 * Returns the bus, target, lun ("{bus},{target},{lun}") as a string which is
 * sometimes needed by some backends like cdrecord.
 *
 * Return value: a string or NULL. The string must be freed when not needed
 **/
gchar *
brasero_drive_get_bus_target_lun_string (BraseroDrive *drive)
{
	BraseroDrivePrivate *priv;

	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), NULL);

	priv = BRASERO_DRIVE_PRIVATE (drive);
	if (!priv->udi)
		return NULL;

	if (priv->bus < 0)
		return NULL;

	return g_strdup_printf ("%i,%i,%i", priv->bus, priv->target, priv->lun);
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

	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), FALSE);

	priv = BRASERO_DRIVE_PRIVATE (drive);
	return (priv->path == NULL);
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
	BraseroDrivePrivate *priv;
	BraseroDeviceHandle *handle;
	BraseroScsiMechStatusHdr hdr;

	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), FALSE);

	priv = BRASERO_DRIVE_PRIVATE (drive);
	if (!priv->udi)
		return FALSE;

	handle = brasero_device_handle_open (priv->path, FALSE, NULL);
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

	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), FALSE);

#if defined(HAVE_STRUCT_USCSI_CMD)
	device = brasero_drive_get_block_device (drive);
#else
	device = brasero_drive_get_device (drive);
#endif

	handle = brasero_device_handle_open (device, TRUE, NULL);
	if (!handle)
		return FALSE;

	brasero_device_handle_close (handle);
	return TRUE;
}

/**
 * brasero_drive_lock:
 * @drive: a #BraseroDrive
 * @reason: a string to indicate what the drive was locked for
 * @reason_for_failure: a string to hold the reason why the locking failed
 *
 * Locks a #BraseroDrive. Ejection shouldn't be possible any more.
 *
 * Return value: %TRUE if the drive was successfully locked or %FALSE.
 **/
gboolean
brasero_drive_lock (BraseroDrive *drive,
		    const gchar *reason,
		    gchar **reason_for_failure)
{
	BraseroDrivePrivate *priv;
	BraseroHALWatch *watch;
	LibHalContext *ctx;
	DBusError error;
	gboolean result;
	gchar *failure;

	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), FALSE);

	priv = BRASERO_DRIVE_PRIVATE (drive);
	if (!priv->udi)
		return FALSE;

	watch = brasero_hal_watch_get_default ();
	ctx = brasero_hal_watch_get_ctx (watch);

	dbus_error_init (&error);
	result = libhal_device_lock (ctx,
				     priv->udi,
				     reason,
				     &failure,
				     &error);

	if (dbus_error_is_set (&error))
		dbus_error_free (&error);

	if (reason_for_failure)
		*reason_for_failure = g_strdup (failure);

	if (failure)
		dbus_free (failure);

	if (result) {
		BRASERO_MEDIA_LOG ("Device locked");
	}
	else {
		BRASERO_MEDIA_LOG ("Device failed to lock");
	}

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
	BraseroDrivePrivate *priv;
	BraseroHALWatch *watch;
	LibHalContext *ctx;
	DBusError error;
	gboolean result;

	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), FALSE);

	priv = BRASERO_DRIVE_PRIVATE (drive);
	if (!priv->udi)
		return FALSE;

	watch = brasero_hal_watch_get_default ();
	ctx = brasero_hal_watch_get_ctx (watch);

	dbus_error_init (&error);
	result = libhal_device_unlock (ctx,
				       priv->udi,
				       &error);

	if (dbus_error_is_set (&error))
		dbus_error_free (&error);

	BRASERO_MEDIA_LOG ("Device unlocked");
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
	BraseroHALWatch *watch;
	LibHalContext *ctx;

	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), NULL);

	priv = BRASERO_DRIVE_PRIVATE (drive);
	if (!priv->udi) {
		/* Translators: This is a fake drive, a file, and means that
		 * when we're writing, we're writing to a file and create an
		 * image on the hard drive. */
		return g_strdup (_("Image File"));
	}

	watch = brasero_hal_watch_get_default ();
	ctx = brasero_hal_watch_get_ctx (watch);
	return libhal_device_get_property_string (ctx,
						  priv->udi,
	  					  DEVICE_MODEL,
						  NULL);
}

/**
 * brasero_drive_get_device:
 * @drive: a #BraseroDrive
 *
 * Gets a string holding the device path for the drive.
 *
 * Return value: a string holding the device path
 **/
const gchar *
brasero_drive_get_device (BraseroDrive *drive)
{
	BraseroDrivePrivate *priv;

	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), NULL);

	priv = BRASERO_DRIVE_PRIVATE (drive);
	return priv->path;
}

/**
 * brasero_drive_get_block_device:
 * @drive: a #BraseroDrive
 *
 * Gets a string holding the block device path for the drive. This can be used on
 * some other OS, like Solaris, for burning operations instead of the device
 * path.
 *
 * Return value: a string holding the block device path
 **/
const gchar *
brasero_drive_get_block_device (BraseroDrive *drive)
{
	BraseroDrivePrivate *priv;

	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), NULL);

	priv = BRASERO_DRIVE_PRIVATE (drive);
	return priv->block_path;
}

/**
 * brasero_drive_get_udi:
 * @drive: a #BraseroDrive
 *
 * Gets a string holding the HAL udi corresponding to this device. It can be used
 * to uniquely identify the drive.
 *
 * Return value: a string holding the HAL udi. Not to be freed
 **/
const gchar *
brasero_drive_get_udi (BraseroDrive *drive)
{
	BraseroDrivePrivate *priv;

	if (!drive)
		return NULL;

	priv = BRASERO_DRIVE_PRIVATE (drive);
	return priv->udi;
}

/**
 * brasero_drive_get_medium:
 * @drive: a #BraseroDrive
 *
 * Gets the medium currently inserted in the drive. If there is no medium or if
 * the medium is not probed yet then it returns NULL.
 *
 * Return value: a #BraseroMedium object or NULL
 **/
BraseroMedium *
brasero_drive_get_medium (BraseroDrive *drive)
{
	BraseroDrivePrivate *priv;

	if (!drive)
		return NULL;

	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), NULL);

	priv = BRASERO_DRIVE_PRIVATE (drive);

	if (!priv->probed && priv->udi)
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

	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), BRASERO_DRIVE_CAPS_NONE);

	priv = BRASERO_DRIVE_PRIVATE (drive);
	return priv->caps;
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
brasero_drive_init (BraseroDrive *object)
{ }

static void
brasero_drive_finalize (GObject *object)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (object);
	if (priv->path) {
		libhal_free_string (priv->path);
		priv->path = NULL;
	}

	if (priv->block_path) {
		libhal_free_string (priv->block_path);
		priv->block_path = NULL;
	}

	if (priv->medium) {
		g_object_unref (priv->medium);
		priv->medium = NULL;
	}

	if (priv->hal_sig) {
		BraseroHALWatch *watch;
		LibHalContext *ctx;
		DBusError error;

		watch = brasero_hal_watch_get_default ();
		ctx = brasero_hal_watch_get_ctx (watch);

		dbus_error_init (&error);
		libhal_device_remove_property_watch (ctx, priv->udi, &error);

		g_signal_handler_disconnect (watch, priv->hal_sig);
		priv->hal_sig = 0;
	}

	if (priv->udi) {
		g_free (priv->udi);
		priv->udi = NULL;
	}

	G_OBJECT_CLASS (brasero_drive_parent_class)->finalize (object);
}

static void
brasero_drive_medium_probed (BraseroMedium *medium,
			     BraseroDrive *self)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (self);

	/* only when it is probed */
	priv->probed = TRUE;
	g_signal_emit (self,
		       drive_signals [MEDIUM_INSERTED],
		       0,
		       priv->medium);
}

/**
 * This is not public API. Defined in burn-monitor.h.
 */
gboolean
brasero_drive_probing (BraseroDrive *drive)
{
	BraseroDrivePrivate *priv;

	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), FALSE);

	priv = BRASERO_DRIVE_PRIVATE (drive);
	return priv->probed != TRUE;
}

/**
 * brasero_drive_reprobe:
 * @monitor: a #BraseroDrive
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

	g_return_if_fail (BRASERO_IS_DRIVE (drive));

	priv = BRASERO_DRIVE_PRIVATE (drive);
	if (!priv->medium)
		return;

	BRASERO_MEDIA_LOG ("Reprobing inserted medium");

	/* remove current medium */
	medium = priv->medium;
	priv->medium = NULL;

	g_signal_emit (drive,
		       drive_signals [MEDIUM_REMOVED],
		       0,
		       medium);
	g_object_unref (medium);
	priv->probed = FALSE;

	/* try to get a new one */
	priv->medium = g_object_new (BRASERO_TYPE_VOLUME,
				     "drive", drive,
				     NULL);
	g_signal_connect (priv->medium,
			  "probed",
			  G_CALLBACK (brasero_drive_medium_probed),
			  drive);
}

#if 0

void
brasero_drive_hal_reprobe (BraseroDrive *self)
{
	BraseroDrivePrivate *priv;
	BraseroHALWatch *watch;
	LibHalContext *ctx;
	DBusError error;

	priv = BRASERO_DRIVE_PRIVATE (self);

	watch = brasero_hal_watch_get_default ();
	ctx = brasero_hal_watch_get_ctx (watch);

	dbus_error_init (&error);
	if (!libhal_device_reprobe (ctx, priv->udi, &error)) {
		BRASERO_MEDIA_LOG ("libhal_device_reprobe () failed %s",
				  error.message);
		dbus_error_free (&error);
	}			      
}

#endif

static void
brasero_drive_check_medium_inside (BraseroDrive *self)
{
	BraseroDrivePrivate *priv;
	BraseroHALWatch *watch;
	gboolean has_medium;
	LibHalContext *ctx;
	DBusError error;

	priv = BRASERO_DRIVE_PRIVATE (self);

	watch = brasero_hal_watch_get_default ();
	ctx = brasero_hal_watch_get_ctx (watch);

	BRASERO_MEDIA_LOG ("Contents changed");

	dbus_error_init (&error);
	has_medium = libhal_device_get_property_bool (ctx,
						      priv->udi,
						      "storage.removable.media_available",
						      &error);
	if (dbus_error_is_set (&error)) {
		g_warning ("Hal connection problem :  %s\n",
			   error.message);
		dbus_error_free (&error);
		return;
	}

	if (has_medium) {
		BRASERO_MEDIA_LOG ("Medium inserted");

		priv->probed = FALSE;
		priv->medium = g_object_new (BRASERO_TYPE_VOLUME,
					     "drive", self,
					     NULL);

		g_signal_connect (priv->medium,
				  "probed",
				  G_CALLBACK (brasero_drive_medium_probed),
				  self);
	}
	else if (priv->medium) {
		BraseroMedium *medium;

		BRASERO_MEDIA_LOG ("Medium removed");

		medium = priv->medium;
		priv->medium = NULL;

		g_signal_emit (self,
			       drive_signals [MEDIUM_REMOVED],
			       0,
			       medium);
		g_object_unref (medium);
		priv->probed = TRUE;
	}
	else
		priv->probed = TRUE;
}

static void
brasero_drive_medium_inside_property_changed_cb (BraseroHALWatch *watch,
						 const char *udi,
						 const char *key,
						 BraseroDrive *drive)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (drive);

	if (key && strcmp (key, "storage.removable.media_available")) {
		priv->probed = TRUE;
		return;
	}

	if (udi && strcmp (udi, priv->udi)) {
		priv->probed = TRUE;
		return;
	}

	brasero_drive_check_medium_inside (drive);
}

static void
brasero_drive_init_real (BraseroDrive *drive)
{
	BraseroDrivePrivate *priv;
	BraseroHALWatch *watch;
	LibHalContext *ctx;
	DBusError error;
	char *parent;

	priv = BRASERO_DRIVE_PRIVATE (drive);

	watch = brasero_hal_watch_get_default ();
	ctx = brasero_hal_watch_get_ctx (watch);

	priv->path = libhal_device_get_property_string (ctx,
							priv->udi,
							BLOCK_DEVICE,
							NULL);
	if (priv->path && priv->path [0] == '\0') {
		g_free (priv->path);
		priv->path = NULL;
	}

	priv->block_path = libhal_device_get_property_string (ctx,
							      priv->udi,
							      "block.device",
							      NULL);
	if (priv->block_path && priv->block_path [0] == '\0') {
		g_free (priv->block_path);
		priv->block_path = NULL;
	}

	if (libhal_device_get_property_bool (ctx, priv->udi, "storage.cdrom.cdr", NULL))
		priv->caps |= BRASERO_DRIVE_CAPS_CDR;
	if (libhal_device_get_property_bool (ctx, priv->udi, "storage.cdrom.cdrw", NULL))
		priv->caps |= BRASERO_DRIVE_CAPS_CDRW;
	if (libhal_device_get_property_bool (ctx, priv->udi, "storage.cdrom.dvdr", NULL))
		priv->caps |= BRASERO_DRIVE_CAPS_DVDR;
	if (libhal_device_get_property_bool (ctx, priv->udi, "storage.cdrom.dvdrw", NULL))
		priv->caps |= BRASERO_DRIVE_CAPS_DVDRW;
	if (libhal_device_get_property_bool (ctx, priv->udi, "storage.cdrom.dvdplusr", NULL))
		priv->caps |= BRASERO_DRIVE_CAPS_DVDR_PLUS;
	if (libhal_device_get_property_bool (ctx, priv->udi, "storage.cdrom.dvdplusrw", NULL))
		priv->caps |= BRASERO_DRIVE_CAPS_DVDRW_PLUS;
	if (libhal_device_get_property_bool (ctx, priv->udi, "storage.cdrom.dvdplusrdl", NULL))
		priv->caps |= BRASERO_DRIVE_CAPS_DVDR_PLUS_DL;
	if (libhal_device_get_property_bool (ctx, priv->udi, "storage.cdrom.dvdplusrwdl", NULL))
		priv->caps |= BRASERO_DRIVE_CAPS_DVDRW_PLUS_DL;

	/* Also get its parent to retrieve the bus, host, lun values */
	priv->bus = -1;
	priv->lun = -1;
	priv->target = -1;
	parent = libhal_device_get_property_string (ctx, priv->udi, "info.parent", NULL);
	if (parent) {
		/* Check it is a SCSI interface */
		if (libhal_device_property_exists (ctx, parent, "scsi.host", NULL)
		&&  libhal_device_property_exists (ctx, parent, "scsi.lun", NULL)
		&&  libhal_device_property_exists (ctx, parent, "scsi.target", NULL)) {
			priv->bus = libhal_device_get_property_int (ctx, parent, "scsi.host", NULL);
			priv->lun = libhal_device_get_property_int (ctx, parent, "scsi.lun", NULL);
			priv->target = libhal_device_get_property_int (ctx, parent, "scsi.target", NULL);
		}

		BRASERO_MEDIA_LOG ("Drive %s has bus,target,lun = %i %i %i",
				  priv->path,
				  priv->bus,
				  priv->target,
				  priv->lun);
		libhal_free_string (parent);
	}

	/* Now check for the medium */
	brasero_drive_check_medium_inside (drive);

	dbus_error_init (&error);
	libhal_device_add_property_watch (ctx, priv->udi, &error);
	if (dbus_error_is_set (&error)) {
		g_warning ("Failed to watch property : %s\n", error.message);
		dbus_error_free (&error);
	}

	priv->hal_sig = g_signal_connect (watch,
					  "property-changed",
					  G_CALLBACK (brasero_drive_medium_inside_property_changed_cb),
					  drive);
}

static void
brasero_drive_set_property (GObject *object,
			    guint prop_id,
			    const GValue *value,
			    GParamSpec *pspec)
{
	BraseroDrivePrivate *priv;

	g_return_if_fail (BRASERO_IS_DRIVE (object));

	priv = BRASERO_DRIVE_PRIVATE (object);

	switch (prop_id)
	{
	case PROP_UDI:
		priv->udi = g_strdup (g_value_get_string (value));
		if (!priv->udi) {
			priv->probed = TRUE;
			priv->medium = g_object_new (BRASERO_TYPE_VOLUME,
						     "drive", object,
						     NULL);
		}
		else
			brasero_drive_init_real (BRASERO_DRIVE (object));
			
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
		g_value_set_string (value, g_strdup (priv->udi));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
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
 	* BraseroVolumeMonitor::medium-added:
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
 	* BraseroVolumeMonitor::medium-removed:
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
	                                                     "HAL udi",
	                                                     "HAL udi as a string",
	                                                     NULL,
	                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

/**
 * This is not public API. Declared in burn-monitor.h.
 */

BraseroDrive *
brasero_drive_new (const gchar *udi)
{
	return g_object_new (BRASERO_TYPE_DRIVE,
			     "udi", udi,
			     NULL);
}
