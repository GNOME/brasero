/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2008 <bonfire-app@wanadoo.fr>
 * 
 * brasero is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
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

#include "burn-basics.h"
#include "burn-volume-obj.h"

typedef struct _BraseroVolumePrivate BraseroVolumePrivate;
struct _BraseroVolumePrivate
{
	GCancellable *cancel;

	GMainLoop *loop;
	gboolean result;
	GError *error;
};

#define BRASERO_VOLUME_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_VOLUME, BraseroVolumePrivate))

enum
{
	MOUNTED,
	UNMOUNTED,

	LAST_SIGNAL
};


static guint volume_signals[LAST_SIGNAL] = { 0 };

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
	volume_path = brasero_drive_get_device (drive);

	/* NOTE: medium-monitor already holds a reference for GVolumeMonitor */
	monitor = g_volume_monitor_get ();
	volumes = g_volume_monitor_get_volumes (monitor);
	g_object_unref (monitor);

	for (iter = volumes; iter; iter = iter->next) {
		gchar *device_path;
		GVolume *tmp;

		tmp = iter->data;
		device_path = g_volume_get_identifier (tmp, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);

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
		g_warning ("No volume found for medium");

	return volume;
}

gboolean
brasero_volume_is_mounted (BraseroVolume *self)
{
	GMount *mount;
	GVolume *volume;
	BraseroVolumePrivate *priv;

	if (!self)
		return FALSE;

	priv = BRASERO_VOLUME_PRIVATE (self);

	volume = brasero_volume_get_gvolume (self);
	if (!g_volume_can_mount (volume)) {
		/* if it can't be mounted then it's unmounted ... */
		g_object_unref (volume);
		return FALSE;
	}

	mount = g_volume_get_mount (volume);
	g_object_unref (volume);
	if (!mount)
		return FALSE;

	g_object_unref (mount);
	return TRUE;
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

	/* get the uri for the mount point */
	mount = g_volume_get_mount (volume);
	g_object_unref (volume);
	if (!mount)
		return NULL;

	root = g_mount_get_root (mount);
	g_object_unref (mount);

	if (!root) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the disc mount point could not be retrieved."));
	}
	else {
		local_path = g_file_get_path (root);
		g_object_unref (root);
	}

	return local_path;
}

static gboolean
brasero_volume_wait_for_operation_end (BraseroVolume *self,
				       GError **error)
{
	BraseroVolumePrivate *priv;

	priv = BRASERO_VOLUME_PRIVATE (self);

	/* FIXME! that's where we should put a timeout (30 sec ?) */
	priv->loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (priv->loop);

	g_main_loop_unref (priv->loop);
	priv->loop = NULL;

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
	}

	if (priv->result)
		g_signal_emit (self,
			       volume_signals[UNMOUNTED],
			       0);
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
		umount_sig = g_signal_connect (monitor,
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

	if (priv->result)
		g_signal_emit (self,
			       volume_signals[MOUNTED],
			       0);
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
	priv->result = g_drive_eject_finish (G_DRIVE (source),
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

gboolean
brasero_volume_eject (BraseroVolume *self,
		      gboolean wait,
		      GError **error)
{
	GDrive *gdrive;
	gboolean result;
	GVolume *volume;
	BraseroVolumePrivate *priv;

	if (!self)
		return TRUE;

	priv = BRASERO_VOLUME_PRIVATE (self);

	volume = brasero_volume_get_gvolume (self);
	gdrive = g_volume_get_drive (volume);
	g_object_unref (volume);

	if (!g_drive_can_eject (gdrive)) {
		g_object_unref (gdrive);
		return FALSE;
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

gchar *
brasero_volume_get_name (BraseroVolume *self)
{
	BraseroVolumePrivate *priv;
	BraseroMedia media;
	GVolume *volume;
	gchar *name;

	priv = BRASERO_VOLUME_PRIVATE (self);

	media = brasero_medium_get_status (BRASERO_MEDIUM (self));
	if (media & BRASERO_MEDIUM_FILE) {
		/* FIXME: here let's read the image label ?*/
		return NULL;
	}

	volume = brasero_volume_get_gvolume (self);
	name = g_volume_get_name (volume);
	g_object_unref (volume);

	return name;
}

gchar *
brasero_volume_get_display_label (BraseroVolume *self,
				  gboolean with_markup)
{
	BraseroVolumePrivate *priv;
	BraseroDrive *drive;
	BraseroMedia media;
	const gchar *type;
	GVolume *volume;
	gchar *label;
	gchar *name;

	priv = BRASERO_VOLUME_PRIVATE (self);

	media = brasero_medium_get_status (BRASERO_MEDIUM (self));
	if (media & BRASERO_MEDIUM_FILE) {
		/* Translators: This is a fake drive, a file, and means that
		 * when we're writing, we're writing to a file and create an
		 * image on the hard drive. */
		label = g_strdup (_("Image File"));
		if (!with_markup)
			return label;

		name = label;
		label = g_strdup_printf ("<b>%s</b>", label);
		g_free (name);

		return label;
	}

	type = brasero_medium_get_type_string (BRASERO_MEDIUM (self));

	volume = brasero_volume_get_gvolume (self);
	name = g_volume_get_name (volume);
	g_object_unref (volume);

	if (name && name [0] != '\0') {
		if (with_markup)
			/* NOTE for translators: the first %s is the disc type and the
			 * second %s the label of the already existing session on this disc. */
			label = g_strdup_printf (_("<b>Data %s</b>: \"%s\""),
						 type,
						 name);
		else
			label = g_strdup_printf (_("Data %s: \"%s\""),
						 type,
						 name);

		g_free (name);
		return label;
	}
	g_free (name);

	drive = brasero_medium_get_drive (BRASERO_MEDIUM (self));
	name = brasero_drive_get_display_name (drive);

	if (media & BRASERO_MEDIUM_BLANK) {
		if (with_markup)
			/* NOTE for translators: the first %s is the disc type and the
			 * second %s the name of the drive this disc is in. */
			label = g_strdup_printf (_("<b>Blank %s</b> in %s"),
						 type,
						 name);
		else
			label = g_strdup_printf (_("Blank %s in %s"),
						 type,
						 name);
	}
	else if (BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_HAS_AUDIO|BRASERO_MEDIUM_HAS_DATA)) {
		if (with_markup)
			/* NOTE for translators: the first %s is the disc type and the
			 * second %s the name of the drive this disc is in. */
			label = g_strdup_printf (_("<b>Audio and data %s</b> in %s"),
						 type,
						 name);
		else
			label = g_strdup_printf (_("Audio and data %s in %s"),
						 type,
						 name);
	}
	else if (media & BRASERO_MEDIUM_HAS_AUDIO) {
		if (with_markup)
			/* NOTE for translators: the first %s is the disc type and the
			 * second %s the name of the drive this disc is in. */
			label = g_strdup_printf (_("<b>Audio %s</b> in %s"),
						 type,
						 name);
		else
			label = g_strdup_printf (_("Audio %s in %s"),
						 type,
						 name);
	}
	else if (media & BRASERO_MEDIUM_HAS_DATA) {
		if (with_markup)
			/* NOTE for translators: the first %s is the disc type and the
		 	* second %s the name of the drive this disc is in. */
			label = g_strdup_printf (_("<b>Data %s</b> in %s"),
						 type,
						 name);
		else
			label = g_strdup_printf (_("Data %s in %s"),
						 type,
						 name);
	}
	else {
		if (with_markup)
			/* NOTE for translators: the first %s is the disc type and the
		 	* second %s the name of the drive this disc is in. */
			label = g_strdup_printf (_("<b>%s</b> in %s"),
						 type,
						 name);
		else
			label = g_strdup_printf (_("%s in %s"),
						 type,
						 name);
	}

	g_free (name);
	return label;
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

	volume_signals[MOUNTED] =
		g_signal_new ("mounted",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_ACTION,
		              0,
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0,
		              G_TYPE_NONE);

	volume_signals[UNMOUNTED] =
		g_signal_new ("unmounted",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_ACTION,
		              0,
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0,
		              G_TYPE_NONE);
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
