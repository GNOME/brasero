/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2005-2008 <bonfire-app@wanadoo.fr>
 *
 * Brasero is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * Brasero is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
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

typedef struct _BraseroVolumePrivate BraseroVolumePrivate;
struct _BraseroVolumePrivate
{
	GCancellable *cancel;

	guint timeout_id;
	GMainLoop *loop;
	gboolean result;
	GError *error;
};

#define BRASERO_VOLUME_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_VOLUME, BraseroVolumePrivate))

G_DEFINE_TYPE (BraseroVolume, brasero_volume, BRASERO_TYPE_MEDIUM);

static GVolume *
brasero_volume_get_gvolume (BraseroVolume *self)
{
	const gchar *volume_path = NULL;
	BraseroVolumePrivate *priv;
	GVolumeMonitor *monitor;
	GVolume *volume = NULL;
	BraseroDrive *drive;
	GList *volumes;
	GList *iter;

	priv = BRASERO_VOLUME_PRIVATE (self);

	drive = brasero_medium_get_drive (BRASERO_MEDIUM (self));

#if defined(HAVE_STRUCT_USCSI_CMD)
	volume_path = brasero_drive_get_block_device (drive);
#else
	volume_path = brasero_drive_get_device (drive);
#endif

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
			volume = tmp;
			g_free (device_path);
			g_object_ref (volume);
			break;
		}

		g_free (device_path);
	}
	g_list_foreach (volumes, (GFunc) g_object_unref, NULL);
	g_list_free (volumes);

	if (!volume)
		BRASERO_MEDIA_LOG ("No volume found for medium");

	return volume;
}

gboolean
brasero_volume_is_mounted (BraseroVolume *self)
{
	GList *iter;
	GList *mounts;
	gboolean result;
	BraseroDrive *drive;
	GVolumeMonitor *monitor;
	const gchar *volume_path;
	BraseroVolumePrivate *priv;

	if (!self)
		return FALSE;

	priv = BRASERO_VOLUME_PRIVATE (self);

	drive = brasero_medium_get_drive (BRASERO_MEDIUM (self));

#if defined(HAVE_STRUCT_USCSI_CMD)
	volume_path = brasero_drive_get_block_device (drive);
#else
	volume_path = brasero_drive_get_device (drive);
#endif

	if (!volume_path)
		return FALSE;

	monitor = g_volume_monitor_get ();
	mounts = g_volume_monitor_get_mounts (monitor);
	g_object_unref (monitor);

	result = FALSE;
	for (iter = mounts; iter; iter = iter->next) {
		GMount *mount;
		GVolume *volume;
		gchar *device_path;

		mount = iter->data;
		volume = g_mount_get_volume (mount);
		if (!volume)
			continue;

		device_path = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
		if (!device_path)
			continue;		

		if (!strcmp (device_path, volume_path)) {
			result = TRUE;
			break;
		}
	}
	g_list_foreach (mounts, (GFunc) g_object_unref, NULL);
	g_list_free (mounts);

	return result;
}

gchar *
brasero_volume_get_mount_point (BraseroVolume *self,
				GError **error)
{
	BraseroVolumePrivate *priv;
	gchar *local_path = NULL;
	GVolume *volume;
	GMount *mount;
	GFile *root;

	priv = BRASERO_VOLUME_PRIVATE (self);

	volume = brasero_volume_get_gvolume (self);
	if (!volume)
		return NULL;

	/* get the uri for the mount point */
	mount = g_volume_get_mount (volume);
	g_object_unref (volume);
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
	}

	return local_path;
}

static void
brasero_volume_operation_end (BraseroVolume *self)
{
	BraseroVolumePrivate *priv;

	priv = BRASERO_VOLUME_PRIVATE (self);
	if (!priv->loop)
		return;

	if (!g_main_loop_is_running (priv->loop))
		return;

	g_main_loop_quit (priv->loop);	
}

static gboolean
brasero_volume_operation_timeout (gpointer data)
{
	BraseroVolume *self = BRASERO_VOLUME (data);
	BraseroVolumePrivate *priv;

	priv = BRASERO_VOLUME_PRIVATE (self);
	brasero_volume_operation_end (self);

	BRASERO_MEDIA_LOG ("Volume/Disc operation timed out");
	priv->timeout_id = 0;
	priv->result = FALSE;
	return FALSE;
}

static gboolean
brasero_volume_wait_for_operation_end (BraseroVolume *self,
				       GError **error)
{
	BraseroVolumePrivate *priv;

	priv = BRASERO_VOLUME_PRIVATE (self);

	/* put a timeout (30 sec) */
	priv->timeout_id = g_timeout_add_seconds (20,
						  brasero_volume_operation_timeout,
						  self);

	priv->loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (priv->loop);

	g_main_loop_unref (priv->loop);
	priv->loop = NULL;

	if (priv->timeout_id) {
		g_source_remove (priv->timeout_id);
		priv->timeout_id = 0;
	}

	if (priv->error) {
		if (error)
			g_propagate_error (error, priv->error);
		else
			g_error_free (priv->error);

		priv->error = NULL;
	}
	g_cancellable_reset (priv->cancel);

	return priv->result;
}

static void
brasero_volume_umounted_cb (GVolumeMonitor *monitor,
			    GMount *mount,
			    BraseroVolume *self)
{
	BraseroVolumePrivate *priv;
	GMount *vol_mount;
	GVolume *volume;

	priv = BRASERO_VOLUME_PRIVATE (self);

	volume = brasero_volume_get_gvolume (self);
	vol_mount = g_volume_get_mount (volume);
	g_object_unref (volume);

	/* If it's NULL then that means it was unmounted */
	if (!vol_mount) {
		brasero_volume_operation_end (self);
		return;
	}

	g_object_unref (vol_mount);

	/* Check it's the one we were looking for */
	if (vol_mount != mount)
		return;

	brasero_volume_operation_end (self);
}

static void
brasero_volume_umount_finish (GObject *source,
			      GAsyncResult *result,
			      gpointer user_data)
{
	BraseroVolume *self = BRASERO_VOLUME (user_data);
	BraseroVolumePrivate *priv;

	priv = BRASERO_VOLUME_PRIVATE (self);

	if (!priv->loop)
		return;

	priv->result = g_mount_unmount_finish (G_MOUNT (source),
					       result,
					       &priv->error);

	BRASERO_MEDIA_LOG ("Umount operation completed (result = %d)", priv->result);

	if (priv->error) {
		if (priv->error->code == G_IO_ERROR_FAILED_HANDLED) {
			/* means we shouldn't display any error message since 
			 * that was already done */
			g_error_free (priv->error);
			priv->error = NULL;
		}
		else if (priv->error->code == G_IO_ERROR_NOT_MOUNTED) {
			/* That can happen sometimes */
			g_error_free (priv->error);
			priv->error = NULL;
			priv->result = TRUE;
		}

		/* Since there was an error. The "unmounted" signal won't be 
		 * emitted by GVolumeMonitor and therefore we'd get stuck if
		 * we didn't get out of the loop. */
		brasero_volume_operation_end (self);
	}
	else if (!priv->result)
		brasero_volume_operation_end (self);
}

gboolean
brasero_volume_umount (BraseroVolume *self,
		       gboolean wait,
		       GError **error)
{
	GMount *mount;
	gboolean result;
	GVolume *volume;
	BraseroVolumePrivate *priv;

	if (!self)
		return TRUE;

	priv = BRASERO_VOLUME_PRIVATE (self);

	volume = brasero_volume_get_gvolume (self);
	if (!volume)
		return TRUE;

	if (!g_volume_can_mount (volume)) {
		/* if it can't be mounted then it's unmounted ... */
		g_object_unref (volume);
		return TRUE;
	}

	mount = g_volume_get_mount (volume);
	g_object_unref (volume);

	if (!mount)
		return TRUE;

	if (!g_mount_can_unmount (mount))
		return FALSE;

	if (wait) {
		gulong umount_sig;
		GVolumeMonitor *monitor;

		monitor = g_volume_monitor_get ();
		umount_sig = g_signal_connect_after (monitor,
						     "mount-removed",
						     G_CALLBACK (brasero_volume_umounted_cb),
						     self);

		g_mount_unmount (mount,
				 G_MOUNT_UNMOUNT_NONE,
				 priv->cancel,
				 brasero_volume_umount_finish,
				 self);
		result = brasero_volume_wait_for_operation_end (self, error);

		g_signal_handler_disconnect (monitor, umount_sig);
	}
	else {
		g_mount_unmount (mount,
				 G_MOUNT_UNMOUNT_NONE,
				 priv->cancel,
				 NULL,					/* callback */
				 self);
		result = TRUE;
	}
	g_object_unref (mount);

	return result;
}

static void
brasero_volume_mount_finish (GObject *source,
			     GAsyncResult *result,
			     gpointer user_data)
{
	BraseroVolume *self = BRASERO_VOLUME (user_data);
	BraseroVolumePrivate *priv;

	priv = BRASERO_VOLUME_PRIVATE (self);
	priv->result = g_volume_mount_finish (G_VOLUME (source),
					      result,
					      &priv->error);

	if (priv->error) {
		if (priv->error->code == G_IO_ERROR_FAILED_HANDLED) {
			/* means we shouldn't display any error message since 
			 * that was already done */
			g_error_free (priv->error);
			priv->error = NULL;
			priv->result = TRUE;
		}
		else if (priv->error->code == G_IO_ERROR_ALREADY_MOUNTED) {
			g_error_free (priv->error);
			priv->error = NULL;
			priv->result = TRUE;
		}
	}

	brasero_volume_operation_end (self);
}

gboolean
brasero_volume_mount (BraseroVolume *self,
		      gboolean wait,
		      GError **error)
{
	GMount *mount;
	gboolean result;
	GVolume *volume;
	BraseroVolumePrivate *priv;

	if (!self)
		return FALSE;

	priv = BRASERO_VOLUME_PRIVATE (self);

	volume = brasero_volume_get_gvolume (self);
	if (!volume)
		return FALSE;

	if (!g_volume_can_mount (volume)) {
		g_object_unref (volume);
		return FALSE;
	}

	mount = g_volume_get_mount (volume);
	if (mount) {
		g_object_unref (volume);
		g_object_unref (mount);
		return TRUE;
	}

	if (wait) {
		g_volume_mount (volume,
				G_MOUNT_MOUNT_NONE,
				NULL,					/* authentification */
				priv->cancel,
				brasero_volume_mount_finish,
				self);
		result = brasero_volume_wait_for_operation_end (self, error);
	}
	else {
		g_volume_mount (volume,
				G_MOUNT_MOUNT_NONE,
				NULL,					/* authentification */
				priv->cancel,
				NULL,
				self);
		result = TRUE;
	}
	g_object_unref (volume);

	return result;
}

static void
brasero_volume_ejected_cb (BraseroDrive *drive,
			   gpointer callback_data)
{
	BraseroVolume *self = BRASERO_VOLUME (callback_data);
	brasero_volume_operation_end (self);
}

static void
brasero_volume_eject_finish (GObject *source,
			     GAsyncResult *result,
			     gpointer user_data)
{
	BraseroVolume *self = BRASERO_VOLUME (user_data);
	BraseroVolumePrivate *priv;

	priv = BRASERO_VOLUME_PRIVATE (self);
	if (G_IS_DRIVE (source))
		priv->result = g_drive_eject_finish (G_DRIVE (source),
						     result,
						     &priv->error);
	else
		priv->result = g_volume_eject_finish (G_VOLUME (source),
						      result,
						      &priv->error);

	if (priv->error) {
		if (priv->error->code == G_IO_ERROR_FAILED_HANDLED) {
			/* means we shouldn't display any error message since 
			 * that was already done */
			g_error_free (priv->error);
			priv->error = NULL;
		}

		brasero_volume_operation_end (self);
	}
	else if (!priv->result)
		brasero_volume_operation_end (self);
}

static gboolean
brasero_volume_eject_gvolume (BraseroVolume *self,
			      gboolean wait,
			      GVolume *volume,
			      GError **error)
{
	BraseroVolumePrivate *priv;
	gboolean result;

	priv = BRASERO_VOLUME_PRIVATE (self);

	if (!g_volume_can_eject (volume))
		return FALSE;

	if (wait) {
		gulong eject_sig;
		BraseroDrive *drive;

		drive = brasero_medium_get_drive (BRASERO_MEDIUM (self));
		eject_sig = g_signal_connect (drive,
					      "medium-removed",
					      G_CALLBACK (brasero_volume_ejected_cb),
					      self);

		g_volume_eject (volume,
				G_MOUNT_UNMOUNT_NONE,
				priv->cancel,
				brasero_volume_eject_finish,
				self);

		g_object_ref (self);
		result = brasero_volume_wait_for_operation_end (self, error);
		g_object_unref (self);

		/* NOTE: from this point on self is no longer valid */

		g_signal_handler_disconnect (drive, eject_sig);
	}
	else {
		g_volume_eject (volume,
				G_MOUNT_UNMOUNT_NONE,
				priv->cancel,
				NULL,
				self);
		result = TRUE;
	}

	return result;
}

gboolean
brasero_volume_eject (BraseroVolume *self,
		      gboolean wait,
		      GError **error)
{
	GDrive *gdrive;
	GVolume *volume;
	gboolean result;
	BraseroDrive *drive;
	BraseroVolumePrivate *priv;

	BRASERO_MEDIA_LOG ("Ejecting");

	if (!self)
		return TRUE;

	priv = BRASERO_VOLUME_PRIVATE (self);

	drive = brasero_medium_get_drive (BRASERO_MEDIUM (self));
	gdrive = brasero_drive_get_gdrive (drive);
	if (!gdrive) {
		BRASERO_MEDIA_LOG ("No GDrive");
		goto last_resort;
	}

	if (!g_drive_can_eject (gdrive)) {
		BRASERO_MEDIA_LOG ("GDrive can't eject");
		goto last_resort;
	}

	if (wait) {
		gulong eject_sig;
		BraseroDrive *drive;

		drive = brasero_medium_get_drive (BRASERO_MEDIUM (self));
		eject_sig = g_signal_connect (drive,
					      "medium-removed",
					      G_CALLBACK (brasero_volume_ejected_cb),
					      self);

		g_drive_eject (gdrive,
			       G_MOUNT_UNMOUNT_NONE,
			       priv->cancel,
			       brasero_volume_eject_finish,
			       self);

		g_object_ref (self);
		result = brasero_volume_wait_for_operation_end (self, error);
		g_object_unref (self);

		/* NOTE: from this point on self is no longer valid */

		g_signal_handler_disconnect (drive, eject_sig);
	}
	else {
		g_drive_eject (gdrive,
			       G_MOUNT_UNMOUNT_NONE,
			       priv->cancel,
			       NULL,
			       self);
		result = TRUE;
	}

	g_object_unref (gdrive);
	return result;

last_resort:

	/* last resort */
	volume = brasero_volume_get_gvolume (self);
	result = brasero_volume_eject_gvolume (self, wait, volume, error);
	g_object_unref (volume);

	if (gdrive)
		g_object_unref (gdrive);

	return result;
}

gboolean
brasero_volume_can_eject (BraseroVolume *self)
{
	GDrive *gdrive;
	GVolume *volume;
	gboolean result;
	BraseroDrive *drive;
	BraseroVolumePrivate *priv;

	BRASERO_MEDIA_LOG ("Ejecting");

	if (!self)
		return TRUE;

	priv = BRASERO_VOLUME_PRIVATE (self);

	drive = brasero_medium_get_drive (BRASERO_MEDIUM (self));
	gdrive = brasero_drive_get_gdrive (drive);
	if (!gdrive) {
		BRASERO_MEDIA_LOG ("No GDrive");
		goto last_resort;
	}

	if (!g_drive_can_eject (gdrive)) {
		BRASERO_MEDIA_LOG ("GDrive can't eject");
		goto last_resort;
	}

	g_object_unref (gdrive);
	return TRUE;

last_resort:

	if (gdrive)
		g_object_unref (gdrive);

	/* last resort */
	volume = brasero_volume_get_gvolume (self);
	if (!volume)
		return FALSE;

	result = g_volume_can_eject (volume);
	g_object_unref (volume);

	return result;
}

void
brasero_volume_cancel_current_operation (BraseroVolume *self)
{
	BraseroVolumePrivate *priv;

	priv = BRASERO_VOLUME_PRIVATE (self);	

	priv->result = FALSE;

	g_cancellable_cancel (priv->cancel);
	if (priv->loop && g_main_loop_is_running (priv->loop))
		g_main_loop_quit (priv->loop);
}

GIcon *
brasero_volume_get_icon (BraseroVolume *self)
{
	GVolume *volume;
	GMount *mount;
	GIcon *icon;

	if (!self)
		return g_themed_icon_new_with_default_fallbacks ("drive-optical");

	if (brasero_medium_get_status (BRASERO_MEDIUM (self)) == BRASERO_MEDIUM_FILE)
		return g_themed_icon_new_with_default_fallbacks ("iso-image-new");

	volume = brasero_volume_get_gvolume (self);
	if (!volume)
		return g_themed_icon_new_with_default_fallbacks ("drive-optical");

	mount = g_volume_get_mount (volume);
	if (mount) {
		icon = g_mount_get_icon (mount);
		g_object_unref (mount);
	}
	else
		icon = g_volume_get_icon (volume);

	g_object_unref (volume);

	return icon;
}

gchar *
brasero_volume_get_name (BraseroVolume *self)
{
	BraseroVolumePrivate *priv;
	BraseroMedia media;
	const gchar *type;
	GVolume *volume;
	gchar *name;

	priv = BRASERO_VOLUME_PRIVATE (self);

	media = brasero_medium_get_status (BRASERO_MEDIUM (self));
	if (media & BRASERO_MEDIUM_FILE) {
		/* Translators: This is a fake drive, a file, and means that
		 * when we're writing, we're writing to a file and create an
		 * image on the hard drive. */
		return g_strdup (_("Image File"));
	}

	if (media & BRASERO_MEDIUM_HAS_AUDIO) {
		const gchar *audio_name;

		audio_name = brasero_medium_get_CD_TEXT_title (BRASERO_MEDIUM (self));
		if (audio_name)
			return g_strdup (audio_name);
	}

	volume = brasero_volume_get_gvolume (self);
	if (!volume)
		goto last_chance;

	name = g_volume_get_name (volume);
	g_object_unref (volume);

	if (name)
		return name;

last_chance:

	type = brasero_medium_get_type_string (BRASERO_MEDIUM (self));
	name = NULL;
	if (media & BRASERO_MEDIUM_BLANK) {
		/* NOTE for translators: the first %s is the disc type. */
		name = g_strdup_printf (_("Blank %s"), type);
	}
	else if (BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_HAS_AUDIO|BRASERO_MEDIUM_HAS_DATA)) {
		/* NOTE for translators: the first %s is the disc type. */
		name = g_strdup_printf (_("Audio and data %s"), type);
	}
	else if (media & BRASERO_MEDIUM_HAS_AUDIO) {
		/* NOTE for translators: the first %s is the disc type. */
		name = g_strdup_printf (_("Audio %s"), type);
	}
	else if (media & BRASERO_MEDIUM_HAS_DATA) {
		/* NOTE for translators: the first %s is the disc type. */
		name = g_strdup_printf (_("Data %s"), type);
	}
	else {
		/* NOTE for translators: the first %s is the disc type. */
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

	if (priv->cancel) {
		g_cancellable_cancel (priv->cancel);
		g_object_unref (priv->cancel);
		priv->cancel = NULL;
	}

	if (priv->timeout_id) {
		g_source_remove (priv->timeout_id);
		priv->timeout_id = 0;
	}

	if (priv->loop && g_main_loop_is_running (priv->loop))
		g_main_loop_quit (priv->loop);

	G_OBJECT_CLASS (brasero_volume_parent_class)->finalize (object);
}

static void
brasero_volume_class_init (BraseroVolumeClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroVolumePrivate));

	object_class->finalize = brasero_volume_finalize;
}

BraseroVolume *
brasero_volume_new (BraseroDrive *drive,
		    const gchar *udi)
{
	BraseroVolume *volume;

	g_return_val_if_fail (drive != NULL, NULL);
	volume = g_object_new (BRASERO_TYPE_VOLUME,
			       "drive", drive,
			       "udi", udi,
			       NULL);

	return volume;
}
