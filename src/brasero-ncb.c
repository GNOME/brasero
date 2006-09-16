/***************************************************************************
 *            brasero-ncb.c
 *
 *  Sat Sep  9 11:00:42 2006
 *  Copyright  2006  philippe
 *  <philippe@algernon.localdomain>
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

#include <string.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <unistd.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <libgnomevfs/gnome-vfs-volume-monitor.h>
#include <libgnomevfs/gnome-vfs-volume.h>
#include <libgnomevfs/gnome-vfs-drive.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include "brasero-ncb.h"
#include "burn-basics.h"

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
	g_ptr_array_foreach (unmount_data->argv, (GFunc)g_free, NULL);
	g_ptr_array_free (unmount_data->argv, TRUE);

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
static gboolean
create_command (const gchar *device,
		GPtrArray *argv,
		gboolean mount)
{
	gchar *gnome_mount_path;
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
	}
	else if (!mount) {
		/* try to use traditional ways */
		str = g_find_program_in_path ("unmount");

		if (str == NULL) {
			g_ptr_array_free (argv, TRUE);
			return FALSE;
		}

		g_ptr_array_add (argv, str);
		str = g_strdup_printf ("%s", device);
		g_ptr_array_add (argv, str);
	}
	else {
		/* try to use traditional ways */
		str = g_find_program_in_path ("mount");

		if (str == NULL) {
			g_ptr_array_free (argv, TRUE);
			return FALSE;
		}

		g_ptr_array_add (argv, str);
		str = g_strdup_printf ("%s", device);
		g_ptr_array_add (argv, str);
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
	gchar        *envp [] = { 	"LC_ALL=C",
					NULL
	};

	data = arg;

	data->command_ok = TRUE;

	error = NULL;
	if (g_spawn_sync (NULL,
			  (char **) data->argv->pdata,
			  envp,
			  G_SPAWN_STDOUT_TO_DEV_NULL|G_SPAWN_STDERR_TO_DEV_NULL,
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
	data->timeout_tag = g_timeout_add (5 * 1000,
					   command_timeout,
					   data);

	g_thread_create (command_thread_start,
			 data,
			 FALSE,
			 NULL);

	g_main_loop_run (data->loop);

	g_main_loop_unref (data->loop);
	data->loop = NULL;

	command_ok = data->command_ok;

	if (data->error)
		g_propagate_error (error, data->error);

	/* Don't free data if mount operation still running. */
	if (!data->timeout)
		free_command_data (data);

	return command_ok;
}

/**
 * nautilus_burn_drive_unmount:
 * @drive: #NautilusBurnDrive
 *
 * Unmount the media in a #NautilusBurnDrive.
 *
 * Return value: %TRUE if the media was sucessfully unmounted, %FALSE otherwise.
 *
 * Since: 2.10
 **/
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

static gchar *
NCB_VOLUME_GET_MOUNT_POINT (GnomeVFSDrive *drive,
			    GError **error)
{
	gchar *mount_point = NULL;
	GList *iter, *volumes;
	gchar *local_path;

	/* get the uri for the mount point */
	volumes = gnome_vfs_drive_get_mounted_volumes (drive);
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
		if (mount_point)
			g_free (mount_point);

		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the disc mount point cannot be retrieved."));
		return NULL;
	}

	local_path = gnome_vfs_get_local_path_from_uri (mount_point);
	g_free (mount_point);

	return local_path;
}

gchar *
NCB_DRIVE_GET_MOUNT_POINT (NautilusBurnDrive *drive,
			   gboolean *mounted_by_us,
			   GError **error)
{
	GnomeVFSVolumeMonitor *monitor;
	gchar *mount_point;
	GList *drives;
	GList *iter;

	mount_point = NULL;

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
			if (!nautilus_burn_drive_is_mounted (drive)) {
				/* try to mount the disc */
				if (!NCB_DRIVE_MOUNT (drive, error))
					break;

				*mounted_by_us = TRUE;
			}
			else
				*mounted_by_us = FALSE;

			mount_point = NCB_VOLUME_GET_MOUNT_POINT (vfs_drive,
								  error);

			g_free (device_path);
			break;
		}

		g_free (device_path);
	}

	g_list_foreach (drives, (GFunc) gnome_vfs_drive_unref, NULL);
	g_list_free (drives);

	return mount_point;
}

NautilusBurnMediaType
NCB_DRIVE_GET_MEDIA (NautilusBurnDrive *drive)
{
	
	return NAUTILUS_BURN_MEDIA_TYPE_ERROR;
}
