/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Brasero
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
#include "brasero-volume.h"
#include "brasero-gio-operation.h"

typedef struct _BraseroVolumePrivate BraseroVolumePrivate;
struct _BraseroVolumePrivate
{
	GCancellable *cancel;
};

#define BRASERO_VOLUME_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_VOLUME, BraseroVolumePrivate))

G_DEFINE_TYPE (BraseroVolume, brasero_volume, BRASERO_TYPE_MEDIUM);

/**
 * brasero_volume_get_gvolume:
 * @volume: #BraseroVolume
 *
 * Gets the corresponding #GVolume for @volume.
 *
 * Return value: a #GVolume *.
 *
 **/
GVolume *
brasero_volume_get_gvolume (BraseroVolume *volume)
{
	const gchar *volume_path = NULL;
	GVolumeMonitor *monitor;
	GVolume *gvolume = NULL;
	BraseroDrive *drive;
	GList *volumes;
	GList *iter;

	g_return_val_if_fail (volume != NULL, NULL);
	g_return_val_if_fail (BRASERO_IS_VOLUME (volume), NULL);

	drive = brasero_medium_get_drive (BRASERO_MEDIUM (volume));

	/* This returns the block device which is the
	 * same as the device for all OSes except
	 * Solaris where the device is the raw device. */
	volume_path = brasero_drive_get_block_device (drive);

	/* NOTE: medium-monitor already holds a reference for GVolumeMonitor */
	monitor = g_volume_monitor_get ();
	volumes = g_volume_monitor_get_volumes (monitor);
	g_object_unref (monitor);

	for (iter = volumes; iter; iter = iter->next) {
		gchar *device_path;
		GVolume *tmp;

		tmp = iter->data;
		device_path = g_volume_get_identifier (tmp, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
		if (!device_path)
			continue;

		BRASERO_MEDIA_LOG ("Found volume %s", device_path);
		if (!strcmp (device_path, volume_path)) {
			gvolume = tmp;
			g_free (device_path);
			g_object_ref (gvolume);
			break;
		}

		g_free (device_path);
	}
	g_list_foreach (volumes, (GFunc) g_object_unref, NULL);
	g_list_free (volumes);

	if (!gvolume)
		BRASERO_MEDIA_LOG ("No volume found for medium");

	return gvolume;
}

/**
 * brasero_volume_is_mounted:
 * @volume: #BraseroVolume
 *
 * Returns whether the volume is currently mounted.
 *
 * Return value: a #gboolean. TRUE if it is mounted.
 *
 **/
gboolean
brasero_volume_is_mounted (BraseroVolume *volume)
{
	gchar *path;

	g_return_val_if_fail (volume != NULL, FALSE);
	g_return_val_if_fail (BRASERO_IS_VOLUME (volume), FALSE);

	/* NOTE: that's the surest way to know if a drive is really mounted. For
	 * GIO a blank medium can be mounted to burn:/// which is not really 
	 * what we're interested in. So the mount path must be also local. */
	path = brasero_volume_get_mount_point (volume, NULL);
	if (path) {
		g_free (path);
		return TRUE;
	}

	return FALSE;
}

/**
 * brasero_volume_get_mount_point:
 * @volume: #BraseroVolume
 * @error: #GError **
 *
 * Returns the path for mount point for @volume.
 *
 * Return value: a #gchar *
 *
 **/
gchar *
brasero_volume_get_mount_point (BraseroVolume *volume,
				GError **error)
{
	gchar *local_path = NULL;
	GVolume *gvolume;
	GMount *mount;
	GFile *root;

	g_return_val_if_fail (volume != NULL, NULL);
	g_return_val_if_fail (BRASERO_IS_VOLUME (volume), NULL);

	gvolume = brasero_volume_get_gvolume (volume);
	if (!gvolume)
		return NULL;

	/* get the uri for the mount point */
	mount = g_volume_get_mount (gvolume);
	g_object_unref (gvolume);
	if (!mount)
		return NULL;

	root = g_mount_get_root (mount);
	g_object_unref (mount);

	if (!root) {
		g_set_error (error,
			     BRASERO_MEDIA_ERROR,
			     BRASERO_MEDIA_ERROR_GENERAL,
			     _("The disc mount point could not be retrieved"));
	}
	else {
		local_path = g_file_get_path (root);
		g_object_unref (root);
		BRASERO_MEDIA_LOG ("Mount point is %s", local_path);
	}

	return local_path;
}

/**
 * brasero_volume_umount:
 * @volume: #BraseroVolume
 * @wait: #gboolean
 * @error: #GError **
 *
 * Unmount @volume. If wait is set to TRUE, then block (in a GMainLoop) until
 * the operation finishes.
 *
 * Return value: a #gboolean. TRUE if the operation succeeded.
 *
 **/
gboolean
brasero_volume_umount (BraseroVolume *volume,
		       gboolean wait,
		       GError **error)
{
	gboolean result;
	GVolume *gvolume;
	BraseroVolumePrivate *priv;

	if (!volume)
		return TRUE;

	g_return_val_if_fail (BRASERO_IS_VOLUME (volume), FALSE);

	priv = BRASERO_VOLUME_PRIVATE (volume);

	gvolume = brasero_volume_get_gvolume (volume);
	if (!gvolume)
		return TRUE;

	if (g_cancellable_is_cancelled (priv->cancel)) {
		BRASERO_MEDIA_LOG ("Resetting GCancellable object");
		g_cancellable_reset (priv->cancel);
	}

	result = brasero_gio_operation_umount (gvolume,
					       priv->cancel,
					       wait,
					       error);
	g_object_unref (gvolume);

	return result;
}

/**
 * brasero_volume_mount:
 * @volume: #BraseroVolume *
 * @parent_window: #GtkWindow *
 * @wait: #gboolean
 * @error: #GError **
 *
 * Mount @volume. If wait is set to TRUE, then block (in a GMainLoop) until
 * the operation finishes.
 * @parent_window is used if an authentification is needed. Then the authentification
 * dialog will be set modal.
 *
 * Return value: a #gboolean. TRUE if the operation succeeded.
 *
 **/
gboolean
brasero_volume_mount (BraseroVolume *volume,
		      GtkWindow *parent_window,
		      gboolean wait,
		      GError **error)
{
	gboolean result;
	GVolume *gvolume;
	BraseroVolumePrivate *priv;

	if (!volume)
		return TRUE;

	g_return_val_if_fail (BRASERO_IS_VOLUME (volume), FALSE);

	priv = BRASERO_VOLUME_PRIVATE (volume);

	gvolume = brasero_volume_get_gvolume (volume);
	if (!gvolume)
		return TRUE;

	if (g_cancellable_is_cancelled (priv->cancel)) {
		BRASERO_MEDIA_LOG ("Resetting GCancellable object");
		g_cancellable_reset (priv->cancel);
	}

	result = brasero_gio_operation_mount (gvolume,
					      parent_window,
					      priv->cancel,
					      wait,
					      error);
	g_object_unref (gvolume);

	return result;
}

/**
 * brasero_volume_cancel_current_operation:
 * @volume: #BraseroVolume *
 *
 * Cancels all operations currently running for @volume
 *
 **/
void
brasero_volume_cancel_current_operation (BraseroVolume *volume)
{
	BraseroVolumePrivate *priv;

	g_return_if_fail (volume != NULL);
	g_return_if_fail (BRASERO_IS_VOLUME (volume));

	priv = BRASERO_VOLUME_PRIVATE (volume);

	BRASERO_MEDIA_LOG ("Cancelling volume operation");

	g_cancellable_cancel (priv->cancel);
}

/**
 * brasero_volume_get_icon:
 * @volume: #BraseroVolume *
 *
 * Returns a GIcon pointer for the volume.
 *
 * Return value: a #GIcon*
 *
 **/
GIcon *
brasero_volume_get_icon (BraseroVolume *volume)
{
	GVolume *gvolume;
	GMount *mount;
	GIcon *icon;

	if (!volume)
		return g_themed_icon_new_with_default_fallbacks ("drive-optical");

	g_return_val_if_fail (BRASERO_IS_VOLUME (volume), NULL);

	if (brasero_medium_get_status (BRASERO_MEDIUM (volume)) == BRASERO_MEDIUM_FILE)
		return g_themed_icon_new_with_default_fallbacks ("iso-image-new");

	gvolume = brasero_volume_get_gvolume (volume);
	if (!gvolume)
		return g_themed_icon_new_with_default_fallbacks ("drive-optical");

	mount = g_volume_get_mount (gvolume);
	if (mount) {
		icon = g_mount_get_icon (mount);
		g_object_unref (mount);
	}
	else
		icon = g_volume_get_icon (gvolume);

	g_object_unref (gvolume);

	return icon;
}

/**
 * brasero_volume_get_name:
 * @volume: #BraseroVolume *
 *
 * Returns a string that can be displayed to represent the volume²
 *
 * Return value: a #gchar *. Free when not needed anymore.
 *
 **/
gchar *
brasero_volume_get_name (BraseroVolume *volume)
{
	BraseroMedia media;
	const gchar *type;
	GVolume *gvolume;
	gchar *name;

	g_return_val_if_fail (volume != NULL, NULL);
	g_return_val_if_fail (BRASERO_IS_VOLUME (volume), NULL);

	media = brasero_medium_get_status (BRASERO_MEDIUM (volume));
	if (media & BRASERO_MEDIUM_FILE) {
		/* Translators: This is a fake drive, a file, and means that
		 * when we're writing, we're writing to a file and create an
		 * image on the hard drive. */
		return g_strdup (_("Image File"));
	}

	if (media & BRASERO_MEDIUM_HAS_AUDIO) {
		const gchar *audio_name;

		audio_name = brasero_medium_get_CD_TEXT_title (BRASERO_MEDIUM (volume));
		if (audio_name)
			return g_strdup (audio_name);
	}

	gvolume = brasero_volume_get_gvolume (volume);
	if (!gvolume)
		goto last_chance;

	name = g_volume_get_name (gvolume);
	g_object_unref (gvolume);

	if (name)
		return name;

last_chance:

	type = brasero_medium_get_type_string (BRASERO_MEDIUM (volume));
	name = NULL;
	if (media & BRASERO_MEDIUM_BLANK) {
		/* NOTE for translators: the first %s is the disc type and Blank is an adjective. */
		name = g_strdup_printf (_("Blank disc (%s)"), type);
	}
	else if (BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_HAS_AUDIO|BRASERO_MEDIUM_HAS_DATA)) {
		/* NOTE for translators: the first %s is the disc type. */
		name = g_strdup_printf (_("Audio and data disc (%s)"), type);
	}
	else if (media & BRASERO_MEDIUM_HAS_AUDIO) {
		/* NOTE for translators: the first %s is the disc type. */
		name = g_strdup_printf (_("Audio disc (%s)"), type);
	}
	else if (media & BRASERO_MEDIUM_HAS_DATA) {
		/* NOTE for translators: the first %s is the disc type. */
		name = g_strdup_printf (_("Data disc (%s)"), type);
	}
	else {
		name = g_strdup (type);
	}

	return name;
}

static void
brasero_volume_init (BraseroVolume *object)
{
	BraseroVolumePrivate *priv;

	priv = BRASERO_VOLUME_PRIVATE (object);
	priv->cancel = g_cancellable_new ();
}

static void
brasero_volume_finalize (GObject *object)
{
	BraseroVolumePrivate *priv;

	priv = BRASERO_VOLUME_PRIVATE (object);

	BRASERO_MEDIA_LOG ("Finalizing Volume object");
	if (priv->cancel) {
		g_cancellable_cancel (priv->cancel);
		g_object_unref (priv->cancel);
		priv->cancel = NULL;
	}

	G_OBJECT_CLASS (brasero_volume_parent_class)->finalize (object);
}

static void
brasero_volume_class_init (BraseroVolumeClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroVolumePrivate));

	object_class->finalize = brasero_volume_finalize;
}
