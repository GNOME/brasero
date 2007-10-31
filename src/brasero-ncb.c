/***************************************************************************
 *            brasero-ncb.c
 *
 *  Sat Sep  9 11:00:42 2006
 *  Copyright  2006  philippe
 *  <philippe@Rouquier Philippe.localdomain>
 ****************************************************************************/

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */

/* Some of the functions are cut and pasted from nautilus-burn-drive.c.
 * This is done to work around a bug in fedora that is not built with
 * gnome-mount support and therefore can't unmount the volumes mounted
 * by gnome-mount since it uses plain unmount command */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <libgnomevfs/gnome-vfs-volume-monitor.h>
#include <libgnomevfs/gnome-vfs-volume.h>
#include <libgnomevfs/gnome-vfs-drive.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include <nautilus-burn-drive-monitor.h>

#include "brasero-ncb.h"
#include "burn-basics.h"
#include "burn-volume.h"
#include "burn-medium.h"

#define BRASERO_MEDIUM_KEY "brasero-medium-key"

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
	const gchar *device;
	GPtrArray *argv;
	CommandData *data;
	gboolean command_ok;

	g_return_val_if_fail (drive != NULL, FALSE);

	/* fetches the device for the drive */
	device = NCB_DRIVE_GET_DEVICE (drive);
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
NCB_DRIVE_UNMOUNT (NautilusBurnDrive *drive,
		   GError **error)
{
	return launch_command (drive, FALSE, error);
}

gboolean
NCB_DRIVE_MOUNT (NautilusBurnDrive *drive,
		 GError **error)
{
	return launch_command (drive, TRUE, error);
}

static GnomeVFSDrive *
NCB_DRIVE_GET_VFS_DRIVE (NautilusBurnDrive *drive)
{
	GnomeVFSVolumeMonitor *monitor;
	GList *drives;
	GList *iter;

	monitor = gnome_vfs_get_volume_monitor ();
	drives = gnome_vfs_volume_monitor_get_connected_drives (monitor);
	for (iter = drives; iter; iter = iter->next) {
		GnomeVFSDeviceType device_type;
		GnomeVFSDrive *vfs_drive;
		gchar *device_path;

		vfs_drive = iter->data;

		device_type = gnome_vfs_drive_get_device_type (vfs_drive);
		if (device_type != GNOME_VFS_DEVICE_TYPE_AUDIO_CD
		&&  device_type != GNOME_VFS_DEVICE_TYPE_VIDEO_DVD
		&&  device_type != GNOME_VFS_DEVICE_TYPE_CDROM)
			continue;

		device_path = gnome_vfs_drive_get_device_path (vfs_drive);
		if (!strcmp (device_path, NCB_DRIVE_GET_DEVICE (drive))) {
			gnome_vfs_drive_ref (vfs_drive);

			g_list_foreach (drives, (GFunc) gnome_vfs_drive_unref, NULL);
			g_list_free (drives);
			g_free (device_path);
			return vfs_drive;
		}

		g_free (device_path);
	}

	g_list_foreach (drives, (GFunc) gnome_vfs_drive_unref, NULL);
	g_list_free (drives);

	return NULL;
}

gchar *
NCB_VOLUME_GET_MOUNT_POINT (NautilusBurnDrive *drive,
			    GError **error)
{
	GnomeVFSDrive *vfsdrive = NULL;
	gchar *mount_point = NULL;
	gchar *local_path = NULL;
	GList *iter, *volumes;

	/* get the uri for the mount point */
	vfsdrive = NCB_DRIVE_GET_VFS_DRIVE (drive);
	volumes = gnome_vfs_drive_get_mounted_volumes (vfsdrive);
	gnome_vfs_drive_unref (vfsdrive);

	for (iter = volumes; iter; iter = iter->next) {
		GnomeVFSVolume *volume;

		volume = iter->data;
		mount_point = gnome_vfs_volume_get_activation_uri (volume);
		if (mount_point)
			break;
	}
	gnome_vfs_drive_volume_list_free (volumes);

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
		local_path = gnome_vfs_get_local_path_from_uri (mount_point);
		g_free (mount_point);
	}

	return local_path;
}

gboolean
NCB_MEDIA_GET_LAST_DATA_TRACK_ADDRESS (NautilusBurnDrive *drive,
				       gint64 *byte,
				       gint64 *sector)
{
	BraseroMedium *medium;

	if (!drive) {
		if (byte)
			*byte = -1;
		if (sector)
			*sector = -1;
		return FALSE;
	}

	medium = g_object_get_data (G_OBJECT (drive), BRASERO_MEDIUM_KEY);
	if (!medium) {
		if (byte)
			*byte = -1;
		if (sector)
			*sector = -1;
		return FALSE;
	}

	return brasero_medium_get_last_data_track_address (medium,
							   byte,
							   sector);
}

gboolean
NCB_MEDIA_GET_LAST_DATA_TRACK_SPACE (NautilusBurnDrive *drive,
				     gint64 *size,
				     gint64 *blocks)
{
	BraseroMedium *medium;

	if (!drive) {
		if (size)
			*size = -1;
		if (blocks)
			*blocks = -1;
		return FALSE;
	}

	medium = g_object_get_data (G_OBJECT (drive), BRASERO_MEDIUM_KEY);
	if (!medium) {
		if (size)
			*size = -1;
		if (blocks)
			*blocks = -1;
		return FALSE;
	}

	return brasero_medium_get_last_data_track_space (medium, size, blocks);
}

gint64
NCB_MEDIA_GET_NEXT_WRITABLE_ADDRESS (NautilusBurnDrive *drive)
{
	BraseroMedium *medium;

	if (!drive)
		return -1;

	medium = g_object_get_data (G_OBJECT (drive), BRASERO_MEDIUM_KEY);
	if (!medium)
		return -1;

	return brasero_medium_get_next_writable_address (medium);
}

gint64
NCB_MEDIA_GET_MAX_WRITE_RATE (NautilusBurnDrive *drive)
{
	BraseroMedium *medium;

	if (!drive)
		return -1;

	medium = g_object_get_data (G_OBJECT (drive), BRASERO_MEDIUM_KEY);
	if (!medium)
		return -1;

	return brasero_medium_get_max_write_speed (medium);
}

BraseroMedia
NCB_MEDIA_GET_STATUS (NautilusBurnDrive *drive)
{
	BraseroMedium *medium;

	if (!drive)
		return BRASERO_MEDIUM_NONE;

	medium = g_object_get_data (G_OBJECT (drive), BRASERO_MEDIUM_KEY);
	if (!medium)
		return BRASERO_MEDIUM_NONE;

	return brasero_medium_get_status (medium);
}

void
NCB_MEDIA_GET_DATA_SIZE (NautilusBurnDrive *drive,
			 gint64 *size,
			 gint64 *blocks)
{
	BraseroMedium *medium;

	if (!drive)
		goto end;

	medium = g_object_get_data (G_OBJECT (drive), BRASERO_MEDIUM_KEY);
	if (!medium) 
		goto end;

	brasero_medium_get_data_size (medium, size, blocks);
	return;

end:
	if (size)
		*size = -1;
	if (blocks)
		*blocks = -1;
}

void
NCB_MEDIA_GET_CAPACITY (NautilusBurnDrive *drive,
			gint64 *size,
			gint64 *blocks)
{
	BraseroMedium *medium;

	if (!drive)
		goto end;

	medium = g_object_get_data (G_OBJECT (drive), BRASERO_MEDIUM_KEY);
	if (!medium)
		goto end;

	brasero_medium_get_capacity (medium, size, blocks);
	return;

end:
	if (size)
		*size = -1;
	if (blocks)
		*blocks = -1;
}

void
NCB_MEDIA_GET_FREE_SPACE (NautilusBurnDrive *drive,
			  gint64 *size,
			  gint64 *blocks)
{
	BraseroMedium *medium;

	if (!drive)
		goto end;

	medium = g_object_get_data (G_OBJECT (drive), BRASERO_MEDIUM_KEY);
	if (!medium)
		goto end;

	brasero_medium_get_free_space (medium, size, blocks);
	return;

end:
	if (size)
		*size = -1;
	if (blocks)
		*blocks = -1;
}

const gchar *
NCB_MEDIA_GET_TYPE_STRING (NautilusBurnDrive *drive)
{
	BraseroMedium *medium;

	if (!drive)
		return NULL;

	medium = g_object_get_data (G_OBJECT (drive), BRASERO_MEDIUM_KEY);
	if (!medium)
		return NULL;

	return brasero_medium_get_type_string (medium);
}

const gchar *
NCB_MEDIA_GET_ICON (NautilusBurnDrive *drive)
{
	BraseroMedium *medium;

	if (!drive)
		return NULL;

	medium = g_object_get_data (G_OBJECT (drive), BRASERO_MEDIUM_KEY);
	if (!medium)
		return NULL;

	return brasero_medium_get_icon (medium);
}

static void
brasero_ncb_inserted_medium_cb (NautilusBurnDriveMonitor *monitor,
				NautilusBurnDrive *drive,
				gpointer null_data)
{
	BraseroMedium *medium;

	medium = brasero_medium_new (drive);
	g_object_set_data (G_OBJECT (drive), BRASERO_MEDIUM_KEY, medium);
}

static void
brasero_ncb_removed_medium_cb (NautilusBurnDriveMonitor *monitor,
			       NautilusBurnDrive *drive,
			       gpointer null_data)
{
	BraseroMedium *medium;

	medium = g_object_get_data (G_OBJECT (drive), BRASERO_MEDIUM_KEY);
	g_object_set_data (G_OBJECT (drive), BRASERO_MEDIUM_KEY, NULL);

	if (!medium)
		return;
	g_object_unref (medium);
}

void
NCB_INIT (void)
{
	NautilusBurnDriveMonitor *monitor;
	GList *iter, *list;

	monitor = nautilus_burn_get_drive_monitor ();

	list = nautilus_burn_drive_monitor_get_drives (monitor);
	for (iter = list; iter; iter = iter->next) {
		BraseroMedium *medium;
		NautilusBurnDrive *drive;
		NautilusBurnMediaType medium_type;

		drive = iter->data;
		medium_type = nautilus_burn_drive_get_media_type (drive);
		if (medium_type < NAUTILUS_BURN_MEDIA_TYPE_CD)
			continue;

		medium = brasero_medium_new (drive);
		g_object_set_data (G_OBJECT (drive),
				   BRASERO_MEDIUM_KEY,
				   medium);
	}
	g_list_free (list);

	g_signal_connect (monitor,
			  "media-added",
			  G_CALLBACK (brasero_ncb_inserted_medium_cb),
			  NULL);
	g_signal_connect (monitor,
			  "media-removed",
			  G_CALLBACK (brasero_ncb_removed_medium_cb),
			  NULL);
}
