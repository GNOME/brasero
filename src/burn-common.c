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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 
/***************************************************************************
 *            burn-common.c
 *
 *  Tue Feb 14 15:43:28 2006
 *  Copyright  2006  philippe
 *  <brasero-app@wanadoo.fr>
 ****************************************************************************/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <errno.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <fcntl.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <libgnomevfs/gnome-vfs.h>
#include <nautilus-burn-drive.h>

#include "burn-basics.h"
#include "burn-common.h"
#include "utils.h"

long
brasero_burn_common_compute_time_remaining (gint64 bytes, double bytes_per_sec)
{
	long secs;

	if (bytes_per_sec <= 1)
		return -1;

	secs = bytes / bytes_per_sec;

	return secs;	
}

gboolean
brasero_burn_common_rm (const char *uri)
{
	GnomeVFSDirectoryHandle *handle;
	GnomeVFSFileInfo *info;
	char *file_uri, *name;

	/* NOTE : we don't follow uris as certain files could be linked */
	if (gnome_vfs_directory_open (&handle, uri, GNOME_VFS_FILE_INFO_DEFAULT) != GNOME_VFS_OK) {
		gnome_vfs_unlink (uri);
		return FALSE;
	}

	info = gnome_vfs_file_info_new ();
	while (gnome_vfs_directory_read_next (handle, info) == GNOME_VFS_OK) {
		if (*info->name == '.'
		&& (info->name[1] == 0
		|| (info->name[1] == '.' && info->name[2] == 0)))
			continue;

		name = gnome_vfs_escape_host_and_path_string (info->name);
		file_uri = g_strconcat (uri, "/", name, NULL);
		g_free (name);

		if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
			brasero_burn_common_rm (file_uri);
		else if (gnome_vfs_unlink (file_uri) != GNOME_VFS_OK)
			g_warning ("Cannot remove file %s\n", file_uri);

		g_free (file_uri);
		gnome_vfs_file_info_clear (info);
	}
	gnome_vfs_file_info_unref (info);

	gnome_vfs_directory_close (handle);
	if (gnome_vfs_remove_directory (uri) != GNOME_VFS_OK) {
		g_warning ("Cannot remove directory %s\n", uri);
		return FALSE;
	}

	return TRUE;
}

BraseroBurnResult
brasero_burn_common_check_output (char **output,
				  gboolean overwrite,
				  char **toc,
				  GError **error)
{
	if (!output)
		goto toc;

	/* takes care of the output file */
	if (!*output) {
		int fd;
		char *tmp;

		tmp = g_strdup_printf ("%s/"BRASERO_BURN_TMP_FILE_NAME, g_get_tmp_dir ());
		fd = g_mkstemp (tmp);
		if (fd == -1) {
			g_free (tmp);
			g_set_error (error, 
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("a temporary file can't be created: %s"),
				     strerror (errno));
			return BRASERO_BURN_ERR;
		}

		close (fd);
		g_remove (tmp);
		*output = tmp;
	}
	else if (g_file_test (*output, G_FILE_TEST_EXISTS)) {
		if (!overwrite) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("%s already exists"),
				     *output);
			return BRASERO_BURN_ERR;
		}
		else if (!g_remove (*output)
		      &&  !brasero_burn_common_rm (*output)) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("%s can't be removed"),
				     *output);
			return BRASERO_BURN_ERR;
		}
	}

toc:
	if (!toc)
		return BRASERO_BURN_OK;

	if ((*toc) == NULL)
		*toc = g_strdup_printf ("%s.toc", *output);

	if (g_file_test (*toc, G_FILE_TEST_EXISTS)) {
		if (!overwrite) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("%s already exists"),
				     *toc);
			return BRASERO_BURN_ERR;
		}
		else if (!g_remove (*toc)
		      &&  !brasero_burn_common_rm (*toc)) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("%s can't be removed"),
				     *toc);
			return BRASERO_BURN_ERR;
		}			
	}

	return BRASERO_BURN_OK;
}

gdouble
brasero_burn_common_get_average (GSList **values, gdouble value)
{
	const unsigned int scale = 10000;
	unsigned int num = 0;
	gdouble average;
	gint32 int_value;
	GSList *l;

	if (value * scale < G_MAXINT)
		int_value = (gint32) ceil (scale * value);
	else if (value / scale < G_MAXINT)
		int_value = (gint32) ceil (-1.0 * value / scale);
	else
		return value;
		
	*values = g_slist_prepend (*values, GINT_TO_POINTER (int_value));

	average = 0;
	for (l = *values; l; l = l->next) {
		gdouble r = (gdouble) GPOINTER_TO_INT (l->data);

		if (r < 0)
			r *= scale * -1.0;
		else
			r /= scale;

		average += r;
		num++;
		if (num == MAX_VALUE_AVERAGE && l->next)
			l = g_slist_delete_link (l, l->next);
	}

	average /= num;
	return average;
}

static gpointer
_eject_async (gpointer data)
{
	NautilusBurnDrive *drive = NAUTILUS_BURN_DRIVE (data);

	nautilus_burn_drive_eject (drive);
	nautilus_burn_drive_unref (drive);

	return NULL;
}

void
brasero_burn_common_eject_async (NautilusBurnDrive *drive)
{
	GError *error = NULL;

	nautilus_burn_drive_ref (drive);
	g_thread_create (_eject_async, drive, FALSE, &error);
	if (error) {
		g_warning ("Could not create thread %s\n", error->message);
		g_error_free (error);

		nautilus_burn_drive_unref (drive);
		nautilus_burn_drive_eject (drive);
	}
}

BraseroBurnResult
brasero_burn_common_check_local_file (const gchar *uri, GError **error)
{
	gboolean unreadable;
	GnomeVFSResult res;
	GnomeVFSFileInfo *info;

	/* since file is local no need to look it up asynchronously */
	info = gnome_vfs_file_info_new ();

	res = gnome_vfs_get_file_info (uri,
				       info,
				       GNOME_VFS_FILE_INFO_GET_ACCESS_RIGHTS);

	if (res != GNOME_VFS_OK) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     gnome_vfs_result_to_string (res));

		gnome_vfs_file_info_unref (info);
		return BRASERO_BURN_ERR;
	}

	/* the files must be either directories, regular files or symlinks
	 * but no dev file, sockets or whatever else.
	 * NOTE: we allow symlinks since ncb does too but when the grafts come
	 * from brasero there can't be symlinks since they are replaced by their
	 * targets. */
	if (info->type != GNOME_VFS_FILE_TYPE_DIRECTORY
	&&  info->type != GNOME_VFS_FILE_TYPE_REGULAR
	&&  info->type != GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_INVALID_FILE,
			     _("One file is not a directory or a regular file"));
		return BRASERO_BURN_ERR;
	}

	/* check if the file is readable */
	unreadable = FALSE;
	if (info->uid == getuid ()) {
		if (!(info->permissions & GNOME_VFS_PERM_USER_READ))
			unreadable = TRUE;
	}
	else if (brasero_utils_is_gid_in_groups (info->gid)) {
		if (!(info->permissions & GNOME_VFS_PERM_GROUP_READ))
			unreadable = TRUE;
	}
	else if (!(info->permissions & GNOME_VFS_PERM_OTHER_READ)) {
		unreadable = TRUE;
	}

	gnome_vfs_file_info_unref (info);

	if (unreadable) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_INVALID_FILE,
			     _("the file can't be read"));
	      return BRASERO_BURN_ERR;
	}

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_burn_common_create_tmp_directory (gchar **directory_path,
					  gboolean overwrite,
					  GError **error)
{
	gchar *tmpdir;

	if (!directory_path)
		return BRASERO_BURN_ERR;

	tmpdir = *directory_path;

	if (tmpdir && g_file_test (tmpdir, G_FILE_TEST_EXISTS)) {
		if (!overwrite) {
			gchar *name;

		    	name = g_path_get_basename (tmpdir);
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("%s already exists"),
				     name);
			g_free (name);
			return BRASERO_BURN_ERR;
		}

		if (!g_file_test (tmpdir, G_FILE_TEST_IS_DIR)) {
			g_remove (tmpdir);

			if (g_file_test (tmpdir, G_FILE_TEST_EXISTS)) {
				gchar *name;

				name = g_path_get_basename (tmpdir);
				g_set_error (error,
					     BRASERO_BURN_ERROR,
					     BRASERO_BURN_ERROR_GENERAL,
					     _("%s can't be removed and is not a directory (%s)"),
					     name,
					     strerror (errno));
				g_free (name);
				return BRASERO_BURN_ERR;
			}

			if (g_mkdir_with_parents (tmpdir, 700) == -1) {
				gchar *name;
	
				name = g_path_get_basename (tmpdir);
				g_set_error (error,
					     BRASERO_BURN_ERROR,
					     BRASERO_BURN_ERROR_GENERAL,
					     _("%s can't be created (%s)"),
					     name,
					     strerror (errno));
				g_free (name);
				return BRASERO_BURN_ERR;
			}
		}
	}
	else if (tmpdir) {
		if (g_mkdir_with_parents (tmpdir, 700) == -1) {
			gchar *name;

			name = g_path_get_basename (tmpdir);
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("directory \"%s\" can't be created (%s)"),
				     name,
				     strerror (errno));
			g_free (name);
			return BRASERO_BURN_ERR;
		}
	}
	else {
		gchar *path;

		/* create a working directory in tmp */
		path = g_build_path (G_DIR_SEPARATOR_S,
				     g_get_tmp_dir (),
				     BRASERO_BURN_TMP_FILE_NAME,
				     NULL);

		tmpdir = mkdtemp (path);

		if (tmpdir == NULL) {
			g_free (path);
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("a temporary directory could not be created (%s)"),
				     strerror (errno));
			return BRASERO_BURN_ERR;
		}

		*directory_path = tmpdir;
	}

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_common_create_pipe (int fd [2], GError **error)
{
	long flags = 0;

	/* now we generate the data, piping it to cdrecord presumably */
	if (pipe (fd)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the pipe couldn't be created (%s)"),
			     strerror (errno));
		return BRASERO_BURN_ERR;
	}

	if (fcntl (fd [0], F_GETFL, &flags) != -1) {
		flags |= O_NONBLOCK;
		if (fcntl (fd [0], F_SETFL, flags) == -1) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("couldn't set non blocking mode"));
			return BRASERO_BURN_ERR;
		}
	}
	else {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("couldn't get pipe flags"));
		return BRASERO_BURN_ERR;
	}

	flags = 0;
	if (fcntl (fd [1], F_GETFL, &flags) != -1) {
		flags |= O_NONBLOCK;
		if (fcntl (fd [1], F_SETFL, flags) == -1) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("couldn't set non blocking mode"));
			return BRASERO_BURN_ERR;
		}
	}
	else {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("couldn't get pipe flags"));
		return BRASERO_BURN_ERR;
	}

	return BRASERO_BURN_OK;
}
