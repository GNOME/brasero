/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * trunk
 * Copyright (C) Philippe Rouquier 2008 <bonfire-app@wanadoo.fr>
 * 
 * trunk is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * trunk is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with trunk.  If not, write to:
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

#include <nautilus-burn-drive-monitor.h>

#include "burn-basics.h"
#include "burn-medium.h"
#include "burn-drive.h"

typedef struct _BraseroDrivePrivate BraseroDrivePrivate;
struct _BraseroDrivePrivate
{
	BraseroMedium *medium;
	NautilusBurnDrive *ndrive;
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
	PROP_DRIVE
};

G_DEFINE_TYPE (BraseroDrive, brasero_drive, G_TYPE_OBJECT);

gboolean
brasero_drive_eject (BraseroDrive *drive)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (drive);
	return nautilus_burn_drive_eject (priv->ndrive);
}

typedef struct {
	gboolean    timeout;
	gboolean    command_ok;
	guint       timeout_tag;
	GMainLoop  *loop;
	GPtrArray  *argv;
	GError     *error;
} CommandData;

static void
free_command_data (CommandData *unmount_data)
{
	g_ptr_array_add (unmount_data->argv, NULL);
	g_strfreev ((gchar**) unmount_data->argv->pdata);
	g_ptr_array_free (unmount_data->argv, FALSE);

	g_free (unmount_data);
}

static gboolean
command_done (gpointer data)
{
	CommandData *unmount_data;
	unmount_data = data;

	if (unmount_data->timeout_tag != 0) {
		g_source_remove (unmount_data->timeout_tag);
	}

	if (unmount_data->loop != NULL &&
	    g_main_loop_is_running (unmount_data->loop)) {
		g_main_loop_quit (unmount_data->loop);
	}

	if (unmount_data->timeout) {
		/* We timed out, so unmount_data wasn't freed
		   at mainloop exit. */
		free_command_data (unmount_data);
	}

	return FALSE;
}

static gboolean
command_timeout (gpointer data)
{
	CommandData *unmount_data;
	unmount_data = data;

	/* We're sure, the callback hasn't been run, so just say
	   we were interrupted and return from the mainloop */

	unmount_data->command_ok = FALSE;
	unmount_data->timeout_tag = 0;
	unmount_data->timeout = TRUE;

	if (g_main_loop_is_running (unmount_data->loop)) {
		g_main_loop_quit (unmount_data->loop);
	}

	return FALSE;
}

/* Returns the full command */
static const gchar *locations [] = {
	"/bin",
	"/sbin",
	"/usr/sbin",
	NULL
};

static gchar *
try_hidden_locations (const gchar *name) {
	int i;

	for (i = 0; locations [i]; i++) {
		gchar *path;

		path = g_build_path (G_DIR_SEPARATOR_S,
				     locations [i],
				     name,
				     NULL);
		if (g_file_test (path, G_FILE_TEST_EXISTS))
			return path;

		g_free (path);
	}

	return NULL;
}

static gboolean
create_command (const gchar *device,
		GPtrArray *argv,
		gboolean mount)
{
	gchar *gnome_mount_path;
	gchar *pmount_path;
	gchar *str;

	/* try to see if gnome-mount is available */
	gnome_mount_path = g_find_program_in_path ("gnome-mount");
	if (gnome_mount_path) {
		g_ptr_array_add (argv, gnome_mount_path);
		str = g_strdup_printf ("--device=%s", device);
		g_ptr_array_add (argv, str);

		if (!mount) {
			str = g_strdup ("--unmount");
			g_ptr_array_add (argv, str);
		}

		str = g_strdup ("--no-ui");
		g_ptr_array_add (argv, str);

		str = g_strdup ("-t");
		g_ptr_array_add (argv, str);

		g_ptr_array_add (argv, NULL);
		return TRUE;
	}

	/* see if pmount or pumount are on the file system (used by ubuntu) */
	if (mount)
		pmount_path = g_find_program_in_path ("pmount");
	else
		pmount_path = g_find_program_in_path ("pumount");

	if (pmount_path) {
		g_ptr_array_add (argv, pmount_path);
		g_ptr_array_add (argv, g_strdup (device));
	}
	else if (!mount) {
		/* try to use traditional ways */
		str = g_find_program_in_path ("umount");

		if (!str)
			str = try_hidden_locations ("umount");

		if (!str) {
			g_ptr_array_add (argv, NULL);
			g_strfreev ((gchar**) argv->pdata);
			g_ptr_array_free (argv, FALSE);
			return FALSE;
		}

		g_ptr_array_add (argv, str);
		g_ptr_array_add (argv, g_strdup (device));
	}
	else {
		/* try to use traditional ways */
		str = g_find_program_in_path ("mount");

		if (!str)
			str = try_hidden_locations ("mount");

		if (!str) {
			g_ptr_array_add (argv, NULL);
			g_strfreev ((gchar**) argv->pdata);
			g_ptr_array_free (argv, FALSE);
			return FALSE;
		}

		g_ptr_array_add (argv, str);
		g_ptr_array_add (argv, g_strdup (device));
	}

	g_ptr_array_add (argv, NULL);

	return TRUE;
}

static void *
command_thread_start (void *arg)
{
	GError      *error;
	CommandData *data;
	gint         exit_status;

	data = arg;

	data->command_ok = TRUE;

	error = NULL;
	if (g_spawn_sync (NULL,
			  (char **) data->argv->pdata,
			  NULL,
			  0,
			  NULL, NULL,
			  NULL,
			  NULL,
			  &exit_status,
			  &error)) {
		if (exit_status == 0) {
			data->command_ok = TRUE;
		} else {
			data->command_ok = FALSE;
		}

		/* Delay a bit to make sure unmount finishes */
		sleep (2);
	} else {
		/* spawn failure */
		if (error)
			g_propagate_error (&data->error, error);

		data->command_ok = FALSE;
	}

	g_idle_add (command_done, data);

	g_thread_exit (NULL);

	return NULL;
}

static gboolean
launch_command (NautilusBurnDrive *drive,
		gboolean mount,
		GError **error)
{
	GPtrArray *argv;
	CommandData *data;
	gboolean command_ok;
	const gchar *device;

	g_return_val_if_fail (drive != NULL, FALSE);

	/* fetches the device for the drive */
	device = nautilus_burn_drive_get_device (drive);
	if (device == NULL)
		return FALSE;

	/* create the appropriate command */
	argv = g_ptr_array_new ();
	if (!create_command (device, argv, mount)) {
		g_set_error (error,
			     G_SPAWN_ERROR,
			     G_SPAWN_ERROR_NOENT,
			     _("(u)mount command could not be found in the path"));
		return FALSE;
	}

	command_ok = FALSE;

	data = g_new0 (CommandData, 1);
	data->loop = g_main_loop_new (NULL, FALSE);
	data->argv = argv;
	data->timeout_tag = g_timeout_add (5 * 1000, command_timeout, data);

	g_thread_create (command_thread_start,
			 data,
			 FALSE,
			 NULL);

	g_main_loop_run (data->loop);
	g_main_loop_unref (data->loop);
	data->loop = NULL;

	/* WORKAROUND: on my system (fedora 6/7) gnome-mount manages to unmount
	 * a volume but returns an error since it can't remove the mount point
	 * directory. So to avoid that (after all we don't care about this kind
	 * of error since in the end the volume gets unmounted) we only error
	 * out if the volume wasn't unmounted (which makes it our only criterium
	 * for success/failure) */
	if (mount != nautilus_burn_drive_is_mounted (drive)) {
		command_ok = FALSE;

		if (data->error)
			g_propagate_error (error, data->error);
		else
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("the drive could not be mounted"));
	}
	else
		command_ok = TRUE;

	/* Don't free data if mount operation still running. */
	if (!data->timeout)
		free_command_data (data);

	return command_ok;
}

gboolean
brasero_drive_mount (BraseroDrive *drive, GError **error)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (drive);

	return launch_command (priv->ndrive, TRUE, error);
}

gboolean
brasero_drive_unmount (BraseroDrive *drive, GError **error)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (drive);

	return launch_command (priv->ndrive, FALSE, error);
}

gboolean
brasero_drive_unmount_wait (BraseroDrive *self)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (self);

	return nautilus_burn_drive_unmount (priv->ndrive);
}

static GDrive *
brasero_drive_get_gdrive (BraseroDrive *drive)
{
	BraseroDrivePrivate *priv;
	GVolumeMonitor *monitor;
	GList *drives;
	GList *iter;

	priv = BRASERO_DRIVE_PRIVATE (drive);

	monitor = g_volume_monitor_get ();
	drives = g_volume_monitor_get_connected_drives (monitor);
	for (iter = drives; iter; iter = iter->next) {
		GDrive *vfs_drive;
		GList *vol_iter;
		GList *volumes;

		vfs_drive = iter->data;
		if (!g_drive_has_media (vfs_drive))
			continue;

		/* FIXME: try to see if we can get the identifier for drive */
		volumes = g_drive_get_volumes (vfs_drive);
		for (vol_iter = volumes; vol_iter; vol_iter = vol_iter->next) {
			GVolume *volume;
			gchar *device_path;

			volume = vol_iter->data;
			device_path = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
			if (!strcmp (device_path, nautilus_burn_drive_get_device (priv->ndrive))) {

				g_object_ref (vfs_drive);

				g_list_foreach (volumes, (GFunc) g_object_unref, NULL);
				g_list_free (volumes);

				g_list_foreach (drives, (GFunc) g_object_unref, NULL);
				g_list_free (drives);

				g_free (device_path);
				return vfs_drive;
			}
			g_free (device_path);
		}
		g_list_foreach (volumes, (GFunc) g_object_unref, NULL);
		g_list_free (volumes);
	}
	g_list_foreach (drives, (GFunc) g_object_unref, NULL);
	g_list_free (drives);

	return NULL;
}

gchar *
brasero_drive_get_mount_point (BraseroDrive *drive,
			       GError **error)
{
	gchar *mount_point = NULL;
	gchar *local_path = NULL;
	GDrive *vfsdrive = NULL;
	GList *iter, *volumes;

	/* get the uri for the mount point */
	vfsdrive = brasero_drive_get_gdrive (drive);
	volumes = g_drive_get_volumes (vfsdrive);
	g_object_unref (vfsdrive);

	for (iter = volumes; iter; iter = iter->next) {
		GVolume *volume;
		GMount *mount;
		GFile *root;

		volume = iter->data;

		mount = g_volume_get_mount (volume);
		if (!mount)
			continue;

		root = g_mount_get_root (mount);
		g_object_unref (mount);

		mount_point = g_file_get_uri (root);
		g_object_unref (root);

		if (mount_point)
			break;
	}
	g_list_foreach (volumes, (GFunc) g_object_unref, NULL);
	g_list_free (volumes);

	if (!mount_point || strncmp (mount_point, "file://", 7)) {
		/* mount point won't be usable */
		if (mount_point) {
			g_free (mount_point);
			mount_point = NULL;
		}

		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the disc mount point could not be retrieved."));
	}
	else {
		gchar *tmp;

		local_path = g_filename_from_uri (mount_point, NULL, NULL);
		tmp = local_path;
		local_path = g_strdup (local_path);
		g_free (tmp);
		
		g_free (mount_point);
	}

	return local_path;
}

gboolean
brasero_drive_is_mounted (BraseroDrive *self)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (self);
	return nautilus_burn_drive_is_mounted (priv->ndrive);
}

gboolean
brasero_drive_is_door_open (BraseroDrive *self)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (self);
	return nautilus_burn_drive_door_is_open (priv->ndrive);	
}

gboolean
brasero_drive_lock (BraseroDrive *self,
		    const gchar *reason,
		    gchar **reason_for_failure)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (self);
	return nautilus_burn_drive_lock (priv->ndrive, reason, reason_for_failure);
}

gboolean
brasero_drive_unlock (BraseroDrive *self)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (self);
	return nautilus_burn_drive_unlock (priv->ndrive);
}

gchar *
brasero_drive_get_display_name (BraseroDrive *self)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (self);
	return nautilus_burn_drive_get_name_for_display (priv->ndrive);
}

gchar *
brasero_drive_get_volume_label (BraseroDrive *self)
{
	BraseroDrivePrivate *priv;
	gchar *label;

	priv = BRASERO_DRIVE_PRIVATE (self);
	label = nautilus_burn_drive_get_media_label (priv->ndrive);
	if (label && label [0] == '\0') {
		g_free (label);
		return NULL;
	}

	return label;
}

const gchar *
brasero_drive_get_device (BraseroDrive *self)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (self);
	return nautilus_burn_drive_get_device (priv->ndrive);
}

BraseroMedium *
brasero_drive_get_medium (BraseroDrive *self)
{
	BraseroDrivePrivate *priv;

	if (!self)
		return NULL;

	priv = BRASERO_DRIVE_PRIVATE (self);
	return priv->medium;
}

NautilusBurnDrive *
brasero_drive_get_nautilus_drive (BraseroDrive *self)
{
	BraseroDrivePrivate *priv;

	if (!self)
		return NULL;

	priv = BRASERO_DRIVE_PRIVATE (self);
	return priv->ndrive;
}

void
brasero_drive_set_medium (BraseroDrive *self,
			  BraseroMedium *medium)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (self);

	if (priv->medium) {
		g_signal_emit (self,
			       drive_signals [MEDIUM_REMOVED],
			       0,
			       priv->medium);

		g_object_unref (priv->medium);
		priv->medium = NULL;
	}

	priv->medium = medium;

	if (medium) {
		g_object_ref (medium);
		g_signal_emit (self,
			       drive_signals [MEDIUM_INSERTED],
			       0,
			       priv->medium);
	}
}

gboolean
brasero_drive_can_write (BraseroDrive *self)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (self);
	return nautilus_burn_drive_can_write (priv->ndrive);
}

gboolean
brasero_drive_can_rewrite (BraseroDrive *self)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (self);
	return nautilus_burn_drive_can_rewrite (priv->ndrive);
}

gboolean
brasero_drive_is_fake (BraseroDrive *self)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (self);
	return (nautilus_burn_drive_get_drive_type (priv->ndrive) == NAUTILUS_BURN_DRIVE_TYPE_FILE);
}

static void
brasero_drive_init (BraseroDrive *object)
{ }

static void
brasero_drive_finalize (GObject *object)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (object);

	if (priv->ndrive) {
		g_object_unref (priv->ndrive);
		priv->ndrive = NULL;
	}

	if (priv->medium) {
		g_object_unref (priv->medium);
		priv->medium = NULL;
	}

	G_OBJECT_CLASS (brasero_drive_parent_class)->finalize (object);
}

static void
brasero_drive_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	BraseroDrivePrivate *priv;

	g_return_if_fail (BRASERO_IS_DRIVE (object));

	priv = BRASERO_DRIVE_PRIVATE (object);

	switch (prop_id)
	{
	case PROP_DRIVE:
		priv->ndrive = g_value_get_object (value);
		g_object_ref (priv->ndrive);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
brasero_drive_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	BraseroDrivePrivate *priv;

	g_return_if_fail (BRASERO_IS_DRIVE (object));

	priv = BRASERO_DRIVE_PRIVATE (object);

	switch (prop_id)
	{
	case PROP_DRIVE:
		g_object_ref (priv->ndrive);
		g_value_set_object (value, priv->ndrive);
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

	drive_signals[MEDIUM_INSERTED] =
		g_signal_new ("medium_added",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_ACTION,
		              0,
		              NULL, NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE, 1,
		              BRASERO_TYPE_MEDIUM);

	drive_signals[MEDIUM_REMOVED] =
		g_signal_new ("medium_removed",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_ACTION,
		              0,
		              NULL, NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE, 1,
		              BRASERO_TYPE_MEDIUM);

	g_object_class_install_property (object_class,
	                                 PROP_DRIVE,
	                                 g_param_spec_object ("drive",
	                                                      "drive",
	                                                      "drive in which medium is inserted",
	                                                      NAUTILUS_BURN_TYPE_DRIVE,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

BraseroDrive *
brasero_drive_new (NautilusBurnDrive *drive)
{
	return g_object_new (BRASERO_TYPE_DRIVE,
			     "drive", drive,
			     NULL);
}
