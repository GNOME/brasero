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

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include <gio/gio.h>

#include "brasero-media-private.h"
#include "brasero-gio-operation.h"

#include "brasero-medium.h"
#include "brasero-volume.h"
#include "brasero-drive.h"

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
	gint probe_id;

	BraseroMedium *medium;
	BraseroDriveCaps caps;

	gchar *udi;

	gchar *path;
	gchar *block_path;

	GCancellable *cancel;

	guint probed:1;
	guint probe_cancelled:1;
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
	PROP_GDRIVE,
	PROP_UDI
};

G_DEFINE_TYPE (BraseroDrive, brasero_drive, G_TYPE_OBJECT);

#define BRASERO_DRIVE_OPEN_ATTEMPTS			5

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

	res = brasero_gio_operation_eject_drive (priv->gdrive,
						 priv->cancel,
						 wait,
						 error);
	if (res)
		return TRUE;

	if (!priv->medium)
		return FALSE;

	gvolume = brasero_volume_get_gvolume (BRASERO_VOLUME (priv->medium));
	res = brasero_gio_operation_eject_volume (gvolume,
						  priv->cancel,
						  wait,
						  error);
	g_object_unref (gvolume);

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
	g_cancellable_cancel (priv->cancel);
}

/**
 * brasero_drive_get_bus_target_lun_string:
 * @drive: a #BraseroDrive
 *
 * Returns the bus, target, lun ("{bus},{target},{lun}") as a string which is
 * sometimes needed by some backends like cdrecord.
 *
 * Return value: a string or NULL. The string must be freed when not needed
 *
 * Deprecated since 2.27.3
 **/
gchar *
brasero_drive_get_bus_target_lun_string (BraseroDrive *drive)
{
	g_return_val_if_fail (drive != NULL, NULL);
	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), NULL);

	return NULL;
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
	return (priv->gdrive == NULL);
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

	g_return_val_if_fail (drive != NULL, FALSE);
	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), FALSE);

	priv = BRASERO_DRIVE_PRIVATE (drive);
	if (!priv->gdrive)
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
	BraseroDeviceHandle *handle;
	BraseroDrivePrivate *priv;
	const gchar *device;
	gboolean result;

	g_return_val_if_fail (drive != NULL, FALSE);
	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), FALSE);

	priv = BRASERO_DRIVE_PRIVATE (drive);
	if (!priv->gdrive)
		return FALSE;

	device = brasero_drive_get_device (drive);
	handle = brasero_device_handle_open (device, FALSE, NULL);
	if (!handle)
		return FALSE;

	result = (brasero_sbc_medium_removal (handle, 1, NULL) == BRASERO_SCSI_OK);
	if (!result) {
		BRASERO_MEDIA_LOG ("Device failed to lock");
	}
	else
		BRASERO_MEDIA_LOG ("Device locked");

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
	if (!priv->gdrive)
		return FALSE;

	device = brasero_drive_get_device (drive);
	handle = brasero_device_handle_open (device, FALSE, NULL);
	if (!handle)
		return FALSE;

	result = (brasero_sbc_medium_removal (handle, 0, NULL) == BRASERO_SCSI_OK);
	if (!result) {
		BRASERO_MEDIA_LOG ("Device failed to unlock");
	}
	else
		BRASERO_MEDIA_LOG ("Device unlocked");

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
	if (!priv->gdrive) {
		/* Translators: This is a fake drive, a file, and means that
		 * when we're writing, we're writing to a file and create an
		 * image on the hard drive. */
		return g_strdup (_("Image File"));
	}

	return g_drive_get_name (priv->gdrive);
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

	g_return_val_if_fail (drive != NULL, NULL);
	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), NULL);

	priv = BRASERO_DRIVE_PRIVATE (drive);
	return priv->path? priv->path:priv->block_path;
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

	g_return_val_if_fail (drive != NULL, NULL);
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
 * Deprecated since 2.27.3
 **/
const gchar *
brasero_drive_get_udi (BraseroDrive *drive)
{
	BraseroDrivePrivate *priv;

	if (!drive)
		return NULL;

	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), NULL);

	priv = BRASERO_DRIVE_PRIVATE (drive);
	if (!priv->gdrive)
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
 * Return value: a #BraseroMedium object or NULL. No need to unref after use.
 **/
BraseroMedium *
brasero_drive_get_medium (BraseroDrive *drive)
{
	BraseroDrivePrivate *priv;

	if (!drive)
		return NULL;

	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), NULL);

	priv = BRASERO_DRIVE_PRIVATE (drive);

	if (!priv->probed && priv->gdrive)
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
brasero_drive_init (BraseroDrive *object)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (object);
	priv->cancel = g_cancellable_new ();
}

static void
brasero_drive_medium_probed (BraseroMedium *medium,
			     BraseroDrive *self)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (self);

	/* only when it is probed */
	/* NOTE: BraseroMedium calls GDK_THREADS_ENTER/LEAVE() around g_signal_emit () */
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

	g_return_val_if_fail (drive != NULL, FALSE);
	g_return_val_if_fail (BRASERO_IS_DRIVE (drive), FALSE);

	priv = BRASERO_DRIVE_PRIVATE (drive);
	return priv->probed != TRUE;
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

static void
brasero_drive_check_medium_inside_gdrive (BraseroDrive *self)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (self);

	BRASERO_MEDIA_LOG ("Contents changed %i", g_drive_has_media (priv->gdrive));

	if (g_drive_has_media (priv->gdrive)) {
		if (priv->medium)
			return;

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
brasero_drive_medium_gdrive_changed_cb (BraseroDrive *gdrive,
					BraseroDrive *drive)
{
	brasero_drive_check_medium_inside_gdrive (drive);
}

static gboolean
brasero_drive_probed (gpointer data)
{
	BraseroDrive *drive = BRASERO_DRIVE (data);
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (data);

	g_thread_join (priv->probe);
	priv->probe = NULL;

	/* If it's not a fake drive then connect to signal for any
	 * change and check medium inside */
	g_signal_connect (priv->gdrive,
			  "changed",
			  G_CALLBACK (brasero_drive_medium_gdrive_changed_cb),
			  drive);

	brasero_drive_check_medium_inside_gdrive (drive);

	priv->probe_id = 0;
	return FALSE;
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

	BRASERO_MEDIA_LOG ("Dectected media %x", BRASERO_GET_16 (hdr->current_profile));

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

		if (priv->probe_cancelled)
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
	const gchar *path;
	BraseroScsiErrCode code;
	BraseroDrivePrivate *priv;
	BraseroDeviceHandle *handle;
	BraseroDrive *drive = BRASERO_DRIVE (data);

	priv = BRASERO_DRIVE_PRIVATE (drive);
	path = brasero_drive_get_device (drive);
	if (!path)
		path = brasero_drive_get_block_device (drive);

	/* the drive might be busy (a burning is going on) so we don't block
	 * but we re-try to open it every second */
	BRASERO_MEDIA_LOG ("Trying to open device %s", path);

	handle = brasero_device_handle_open (path, FALSE, &code);
	while (!handle && counter <= BRASERO_DRIVE_OPEN_ATTEMPTS) {
		sleep (1);

		if (priv->probe_cancelled) {
			BRASERO_MEDIA_LOG ("Open () cancelled");
			priv->probe = NULL;
			return NULL;
		}

		counter ++;
		handle = brasero_device_handle_open (path, FALSE, &code);
	}

	if (priv->probe_cancelled) {
		BRASERO_MEDIA_LOG ("Open () cancelled");
		priv->probe = NULL;
		return NULL;
	}

	if (handle) {
		BRASERO_MEDIA_LOG ("Open () succeeded");

		if (!brasero_drive_get_caps_profiles (drive, handle, &code))
			brasero_drive_get_caps_2A (drive, handle, &code);

		brasero_device_handle_close (handle);

		BRASERO_MEDIA_LOG ("Drive caps are %d", priv->caps);
	}
	else
		BRASERO_MEDIA_LOG ("Open () failed: medium busy");

	priv->probe_id = g_idle_add (brasero_drive_probed, drive);
	return NULL;
}

static void
brasero_drive_init_real (BraseroDrive *drive,
                         GDrive *gdrive)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (drive);

	priv->gdrive = g_object_ref (gdrive);
	priv->block_path = g_drive_get_identifier (priv->gdrive, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);

	BRASERO_MEDIA_LOG ("Initializing drive %s", priv->block_path);

	/* NOTE: why a thread? Because in case of a damaged medium, brasero can
	 * block on some functions until timeout and if we do this in the main
	 * thread then our whole UI blocks. This medium won't be exported by the
	 * BraseroDrive that exported until it returns PROBED signal.
	 * One (good) side effect is that it also improves start time. */
	priv->probe = g_thread_create (brasero_drive_probe_thread,
				       drive,
				       TRUE,
				       NULL);
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
		gdrive = g_value_get_object (value);
		if (!gdrive) {
			priv->probed = TRUE;
			priv->medium = g_object_new (BRASERO_TYPE_VOLUME,
						     "drive", object,
						     NULL);
		}
		else
			brasero_drive_init_real (BRASERO_DRIVE (object), gdrive);

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
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
brasero_drive_finalize (GObject *object)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (object);

	if (priv->udi) {
		g_free (priv->udi);
		priv->udi = NULL;
	}

	if (priv->probe) {
		priv->probe_cancelled = TRUE;
		g_thread_join (priv->probe);
		priv->probe = 0;
	}

	if (priv->probe_id) {
		g_source_remove (priv->probe_id);
		priv->probe_id = 0;
	}

	if (priv->block_path) {
		g_free (priv->block_path);
		priv->block_path = NULL;
	}

	if (priv->medium) {
		g_object_unref (priv->medium);
		priv->medium = NULL;
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
	                                                     "Deprecated",
	                                                     "HAL udi as a string (Deprecated)",
	                                                     NULL,
	                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class,
	                                 PROP_GDRIVE,
	                                 g_param_spec_object ("gdrive",
	                                                      "GDrive",
	                                                      "A GDrive object for the drive",
	                                                      G_TYPE_DRIVE,
	                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

