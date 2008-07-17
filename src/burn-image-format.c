/***************************************************************************
 *            burn-images-format.c
 *
 *  Mon Nov  5 16:01:44 2007
 *  Copyright  2007  Philippe Rouquier
 *  <bonfire-app@wanadoo.fr>
 ****************************************************************************/

/*
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

#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/param.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>

#include "burn-basics.h"
#include "burn-debug.h"
#include "burn-image-format.h"

static const gchar *
brasero_image_format_read_path (const gchar *ptr,
				gchar **path)
{
	const gchar *start, *end;

	/* make sure there is a white space */
	if (!isspace (*ptr))
		return NULL;

	/* jump over the white spaces */
	while (isspace (*ptr)) ptr ++;

	/* seek the first '"' if any */
	start = g_utf8_strchr (ptr, -1, '"');
	if (start) {
		start ++;

		/* seek the last '"' */
		end = g_utf8_strchr (start, -1, '"');
		if (!end)
			return NULL;

		ptr = end + 1;
	}
	else {
		/* there is no starting '"' seek last space */
		start = ptr;
		end = ptr;
		while (!isspace (*end)) end ++;

		ptr = end;
		if (isspace (*end))
			end --;
	}

	if (path)
		*path = g_strndup (start, end-start);

	return ptr;
}

static gchar *
brasero_image_format_get_cue_file_complement (const gchar *path)
{
	FILE *file;
	gchar *ptr;
	gchar *complement = NULL;
	/* a path can't be over MAXPATHLEN then buffer doesn't need to be over
	 * this value + 4 + white space + commas */
	gchar buffer [MAXPATHLEN+8+3];

	file = fopen (path, "r");
	if (!file) {
		if (g_str_has_suffix (path, ".cue"))
			return g_strdup_printf ("%.*sbin",
						strlen (path) - 3,
						path);

		return g_strdup_printf ("%s.bin", path);
	}

	while (fgets (buffer, sizeof (buffer), file)) {
		ptr = strstr (buffer, "FILE");
		if (ptr) {
			ptr += 4;
			if (brasero_image_format_read_path (ptr, &complement))
				break;
		}
	}
	fclose (file);

	/* check if the path is relative, if so then add the root path */
	if (complement && !g_path_is_absolute (complement)) {
		gchar *directory;
		gchar *tmp;

		directory = g_path_get_dirname (path);

		tmp = complement;
		complement = g_build_path (G_DIR_SEPARATOR_S,
					   directory,
					   complement,
					   NULL);
		g_free (tmp);
	}

	return complement;
}

static gchar *
brasero_image_format_get_toc_file_complement (const gchar *path)
{
	FILE *file;
	gchar *ptr;
	gchar *complement = NULL;
	/* a path can't be over MAXPATHLEN then buffer doesn't need to be over
	 * this value + keyword size + white space + commas */
	gchar buffer [MAXPATHLEN+8+3];

	/* NOTE: the problem here is that cdrdao files can have references to 
	 * multiple files. Which is great but not for us ... */
	file = fopen (path, "r");
	if (!file) {
		if (g_str_has_suffix (path, ".cue"))
			return g_strdup_printf ("%.*sbin",
						strlen (path) - 3,
						path);

		return g_strdup_printf ("%s.bin", path);
	}

	while (fgets (buffer, sizeof (buffer), file)) {
		ptr = strstr (buffer, "DATAFILE");
		if (ptr) {
			ptr += 8;
			if (brasero_image_format_read_path (ptr, &complement))
				break;
		}

		ptr = strstr (buffer, "FILE");
		if (ptr) {
			ptr += 4;
			if (brasero_image_format_read_path (ptr, &complement))
				break;
		}
	}
	fclose (file);

	/* check if the path is relative, if so then add the root path */
	if (complement && !g_path_is_absolute (complement)) {
		gchar *directory;
		gchar *tmp;

		directory = g_path_get_dirname (path);

		tmp = complement;
		complement = g_build_path (G_DIR_SEPARATOR_S,
					   directory,
					   complement,
					   NULL);
		g_free (tmp);
	}

	return complement;
}

/* FIXME this function is flawed at the moment. A cue file or toc file can 
 * hold different paths */
gchar *
brasero_image_format_get_complement (BraseroImageFormat format,
				     const gchar *path)
{
	gchar *retval = NULL;

	if (format == BRASERO_IMAGE_FORMAT_CLONE) {
		/* These are set rules no need to parse */
		if (g_str_has_suffix (path, ".toc"))
			retval = g_strdup_printf ("%.*sraw",
						  strlen (path) - 3,
						  path);
		else
			retval = g_strdup_printf ("%s.raw", path);
	}
	else if (format == BRASERO_IMAGE_FORMAT_CUE) {
		/* need to parse */
		retval = brasero_image_format_get_cue_file_complement (path);
	}
	else if (format == BRASERO_IMAGE_FORMAT_CDRDAO) {
		/* need to parse */
		retval = brasero_image_format_get_toc_file_complement (path);
	}
	else
		retval = NULL;

	return retval;
}

static gchar *
brasero_image_format_get_MSF_address (const gchar *ptr,
				      gint64 *block)
{
	gchar *next;
	gint64 address = 0;

	address = strtoll (ptr, &next, 10); 
	if (isspace (*next)) {
		*block = address;
		return next;
	}

	if (*next != ':')
		return NULL;

	next ++;
	ptr = next;
	address *= 60;
	address += strtoll (ptr, &next, 10);
	if (ptr == next)
		return NULL;

	if (*next != ':')
		return NULL;

	next ++;
	ptr = next;
	address *= 75;
	address += strtoll (ptr, &next, 10);
	if (ptr == next)
		return NULL;

	if (block)
		*block = address;

	return next;	
}

static gboolean
brasero_image_format_get_DATAFILE_info (const gchar *ptr,
					const gchar *parent,
					gint64 *size,
					GError **error)
{
	struct stat buffer;
	gchar *path;
	int res;

	/* get the path. NOTE: no need to check if it's relative since that's
	 * just to skip it. */
	ptr = brasero_image_format_read_path (ptr, &path);
	if (!ptr)
		return FALSE;

	/* skip white spaces */
	while (isspace (*ptr)) ptr++;

	if (ptr [0] == '\0'
	|| (ptr [0] == '/' && ptr [1] == '/'))
		goto stat_end;

	if (!brasero_image_format_get_MSF_address (ptr, size))
		return FALSE;

	return TRUE;

stat_end:

	/* check if the path is relative, if so then add the root path */
	if (path && !g_path_is_absolute (path)) {
		gchar *tmp;

		tmp = path;
		path = g_build_path (G_DIR_SEPARATOR_S,
				     parent,
				     path,
				     NULL);
		g_free (tmp);
	}

	/* if size is skipped then g_lstat () the file */
	res = g_lstat (path, &buffer);
	g_free (path);

	if (res == -1) {
		g_set_error (error,
			     BRASERO_BURN_ERR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("size can't be retrieved (%s)"),
			     strerror (errno));
		return FALSE;
	}

	if (size)
		*size = BRASERO_SIZE_TO_SECTORS (buffer.st_size, 2352);

	return TRUE;
}

static gboolean
brasero_image_format_get_FILE_info (const gchar *ptr,
				    const gchar *parent,
				    gint64 *size,
				    GError **error)
{
	struct stat buffer;
	gint64 start = 0;
	gchar *path;
	gchar *tmp;
	int res;

	/* get the path and skip it */
	ptr = brasero_image_format_read_path (ptr, &path);
	if (!ptr)
		return FALSE;

	/* skip white spaces */
	while (isspace (*ptr)) ptr++;

	/* skip a possible #.... (offset in bytes) */
	tmp = g_utf8_strchr (ptr, -1, '#');
	if (tmp) {
		tmp ++;
		while (isdigit (*tmp)) tmp ++;
		while (isspace (*tmp)) tmp++;
		ptr = tmp;
	}

	/* get the start */
	ptr = brasero_image_format_get_MSF_address (ptr, &start);
	if (!ptr) {
		g_free (path);
		return FALSE;
	}

	/* skip white spaces */
	while (isspace (*ptr)) ptr++;

	if (ptr [0] == '\0'
	|| (ptr [0] == '/' && ptr [1] == '/'))
		goto stat_end;

	/* get the size */
	if (!brasero_image_format_get_MSF_address (ptr, size)) {
		g_free (path);
		return FALSE;
	}

	return TRUE;

stat_end:

	/* check if the path is relative, if so then add the root path */
	if (path && !g_path_is_absolute (path)) {
		gchar *tmp;

		tmp = path;
		path = g_build_path (G_DIR_SEPARATOR_S,
				     parent,
				     path,
				     NULL);
		g_free (tmp);
	}

	/* if size is skipped then g_lstat () the file */
	res = g_lstat (path, &buffer);
	g_free (path);

	if (res == -1) {
		g_set_error (error,
			     BRASERO_BURN_ERR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("size can't be retrieved (%s)"),
			     strerror (errno));
		return FALSE;
	}

	if (size)
		*size = BRASERO_SIZE_TO_SECTORS (buffer.st_size, 2352) - start;

	return TRUE;
}

gboolean
brasero_image_format_get_cdrdao_size (gchar *path,
				      gint64 *sectors,
				      gint64 *size,
				      GError **error)
{
	FILE *file;
	gchar *parent;
	gint64 cue_size = 0;
	gchar buffer [MAXPATHLEN * 2];

	/* NOTE: the problem here is that cdrdao files can have references to 
	 * multiple files. Which is great but not for us ... */
	file = fopen (path, "r");
	if (!file) {
		g_set_error (error,
			     BRASERO_BURN_ERR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("size can't be retrieved (%s)"),
			     strerror (errno));
		return FALSE;
	}

	parent = g_path_get_dirname (path);
	while (fgets (buffer, sizeof (buffer), file)) {
		gchar *ptr;

		ptr = strstr (buffer, "DATAFILE");
		if (ptr) {
			gint64 size;

			ptr += 8;
			if (!brasero_image_format_get_DATAFILE_info (ptr, parent, &size, error))
				continue;

			cue_size += size;
			continue;
		}

		ptr = strstr (buffer, "FILE");
		if (ptr) {
			gint64 size;

			ptr += 4;
			/* first number is the position, the second the size,
			 * number after '#' is the offset (in bytes). */
			if (!brasero_image_format_get_FILE_info (ptr, parent, &size, error))
				continue;

			cue_size += size;
			continue;
		}

		ptr = strstr (buffer, "AUDIOFILE");
		if (ptr) {
			gint64 size;

			ptr += 4;
			/* first number is the position, the second the size,
			 * number after '#' is the offset (in bytes). */
			if (!brasero_image_format_get_FILE_info (ptr, parent, &size, error))
				continue;

			cue_size += size;
			continue;
		}

		ptr = strstr (buffer, "SILENCE");
		if (ptr) {
			gint64 size;

			ptr += 7;
			if (!isspace (*ptr))
				continue;

			if (!brasero_image_format_get_MSF_address (ptr, &size))
				continue;

			cue_size += size;
		}

		ptr = strstr (buffer, "PREGAP");
		if (ptr) {
			gint64 size;

			ptr += 6;
			if (!isspace (*ptr))
				continue;

			if (!brasero_image_format_get_MSF_address (ptr, &size))
				continue;

			cue_size += size;
		}

		ptr = strstr (buffer, "ZERO");
		if (ptr) {
			gint64 size;

			ptr += 4;
			if (!isspace (*ptr))
				continue;

			if (!brasero_image_format_get_MSF_address (ptr, &size))
				continue;

			cue_size += size;
		}
	}
	g_free (parent);

	fclose (file);

	if (sectors)
		*sectors = cue_size;

	if (size)
		*size = cue_size * 2352;

	return TRUE;
}

/**
 * .cue can use various data files but have to use them ALL. So we don't need
 * to care about a start/size address. We just go through the whole file and
 * stat every time we catch a FILE keyword.
 */

gboolean
brasero_image_format_get_cue_size (gchar *path,
				   gint64 *blocks,
				   gint64 *size,
				   GError **error)
{
	FILE *file;
	gint64 cue_size = 0;
	gchar buffer [MAXPATHLEN * 2];

	/* NOTE: the problem here is that cdrdao files can have references to 
	 * multiple files. Which is great but not for us ... */
	file = fopen (path, "r");
	if (!file) {
		g_set_error (error,
			     BRASERO_BURN_ERR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("size can't be retrieved (%s)"),
			     strerror (errno));
		return FALSE;
	}

	while (fgets (buffer, sizeof (buffer), file)) {
		const gchar *ptr;

		ptr = strstr (buffer, "FILE");
		if (ptr) {
			int res;
			gchar *file_path;
			struct stat buffer;

			ptr += 4;

			/* get the path */
			ptr = brasero_image_format_read_path (ptr, &file_path);
			if (!ptr)
				return FALSE;

			/* check if the path is relative, if so then add the root path */
			if (file_path && !g_path_is_absolute (file_path)) {
				gchar *directory;
				gchar *tmp;

				directory = g_path_get_dirname (path);

				tmp = file_path;
				file_path = g_build_path (G_DIR_SEPARATOR_S,
							  directory,
							  file_path,
							  NULL);
				g_free (tmp);
			}

			res = g_lstat (file_path, &buffer);
			if (res == -1) {
				g_set_error (error,
					     BRASERO_BURN_ERR,
					     BRASERO_BURN_ERROR_GENERAL,
					     _("size can't be retrieved for %s: %s"),
					     file_path,
					     strerror (errno));
				g_free (file_path);
				return FALSE;
			}

			g_free (file_path);
			cue_size += buffer.st_size;
			continue;
		}

		ptr = strstr (buffer, "PREGAP");
		if (ptr) {
			gint64 size;

			ptr += 6;
			if (!isspace (*ptr))
				continue;

			ptr ++;
			ptr = brasero_image_format_get_MSF_address (ptr, &size);
			if (!ptr)
				continue;

			cue_size += size * 2352;
			continue;
		}

		ptr = strstr (buffer, "POSTGAP");
		if (ptr) {
			gint64 size;

			ptr += 7;
			if (!isspace (*ptr))
				continue;

			ptr ++;
			ptr = brasero_image_format_get_MSF_address (ptr, &size);
			if (!ptr)
				continue;

			cue_size += size * 2352;
			continue;
		}
	}

	fclose (file);

	if (size)
		*size = cue_size;
	if (blocks)
		*blocks = BRASERO_SIZE_TO_SECTORS (cue_size, 2352);

	return TRUE;
}

BraseroImageFormat
brasero_image_format_identify_cuesheet (const gchar *path)
{
	FILE *file;
	BraseroImageFormat format;
	gchar buffer [MAXPATHLEN * 2];

	if (!path)
		return BRASERO_IMAGE_FORMAT_NONE;

	/* NOTE: the problem here is that cdrdao files can have references to 
	 * multiple files. Which is great but not for us ... */
	file = fopen (path, "r");
	if (!file)
		return BRASERO_IMAGE_FORMAT_NONE;

	format = BRASERO_IMAGE_FORMAT_NONE;
	while (fgets (buffer, sizeof (buffer), file)) {
		/* Keywords for cdrdao cuesheets */
		if (strstr (buffer, "CD_ROM_XA")
		||  strstr (buffer, "CD_ROM")
		||  strstr (buffer, "CD_DA")
		||  strstr (buffer, "CD_TEXT")) {
			format = BRASERO_IMAGE_FORMAT_CDRDAO;
			break;
		}
		else if (strstr (buffer, "TRACK")) {
			/* NOTE: there is also "AUDIO" but it's common to both */

			/* CDRDAO */
			if (strstr (buffer, "MODE1")
			||  strstr (buffer, "MODE1_RAW")
			||  strstr (buffer, "MODE2_FORM1")
			||  strstr (buffer, "MODE2_FORM2")
			||  strstr (buffer, "MODE_2_RAW")
			||  strstr (buffer, "MODE2_FORM_MIX")
			||  strstr (buffer, "MODE2")) {
				format = BRASERO_IMAGE_FORMAT_CDRDAO;
				break;
			}

			/* .CUE file */
			else if (strstr (buffer, "CDG")
			     ||  strstr (buffer, "MODE1/2048")
			     ||  strstr (buffer, "MODE1/2352")
			     ||  strstr (buffer, "MODE2/2336")
			     ||  strstr (buffer, "MODE2/2352")
			     ||  strstr (buffer, "CDI/2336")
			     ||  strstr (buffer, "CDI/2352")) {
				format = BRASERO_IMAGE_FORMAT_CUE;
				break;
			}
		}
		else if (strstr (buffer, "FILE")) {
			if (strstr (buffer, "MOTOROLA")
			||  strstr (buffer, "BINARY")
			||  strstr (buffer, "AIFF")
			||  strstr (buffer, "WAVE")
			||  strstr (buffer, "MP3")) {
				format = BRASERO_IMAGE_FORMAT_CUE;
				break;
			}
		}
	}
	fclose (file);

	BRASERO_BURN_LOG_WITH_FULL_TYPE (BRASERO_TRACK_TYPE_IMAGE,
					 format,
					 BRASERO_BURN_FLAG_NONE,
					 "Detected");
	return format;
}

gboolean
brasero_image_format_get_iso_size (gchar *path,
				   gint64 *blocks,
				   gint64 *size,
				   GError **error)
{
	struct stat buffer;
	int res;

	/* a simple stat () will do. That means of course that the image must be
	 * local. Now if local-track is enabled, it will always run first and we
	 * don't need that size before it starts. During the GET_SIZE phase of 
	 * the task it runs for, it can set the output size of the task by using
	 * gnome-vfs to retrieve it. That means this particular task will know
	 * the image size once it gets downloaded. local-task will also be able
	 * to report how much it downloads and therefore the task will be able
	 * to report its progress. Afterwards, no problem to get the image size
	 * since it'll be local and stat() will work.
	 * if local-track is not enabled we can't use non-local images anyway so
	 * there is no need to have a function set_size */
	res = g_lstat (path, &buffer);
	if (res == -1) {
		g_set_error (error,
			     BRASERO_BURN_ERR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("size can't be retrieved (%s)"),
			     strerror (errno));

		return FALSE;
	}

	if (size)
		*size = buffer.st_size;

	if (blocks)
		*blocks = (buffer.st_size / 2048) +
			  ((buffer.st_size % 2048) ? 1:0);

	return TRUE;
}

gboolean
brasero_image_format_get_clone_size (gchar *path,
				     gint64 *blocks,
				     gint64 *size,
				     GError **error)
{
	struct stat buffer;
	int res;

	/* a simple stat () will do. That means of course that the image must be
	 * local. Now if local-track is enabled, it will always run first and we
	 * don't need that size before it starts. During the GET_SIZE phase of 
	 * the task it runs for, it can set the output size of the task by using
	 * gnome-vfs to retrieve it. That means this particular task will know
	 * the image size once it gets downloaded. local-task will also be able
	 * to report how much it downloads and therefore the task will be able
	 * to report its progress. Afterwards, no problem to get the image size
	 * since it'll be local and stat() will work.
	 * if local-track is not enabled we can't use non-local images anyway so
	 * there is no need to have a function set_size */
	res = g_lstat (path, &buffer);
	if (res == -1) {
		g_set_error (error,
			     BRASERO_BURN_ERR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("size can't be retrieved (%s)"),
			     strerror (errno));

		return FALSE;
	}

	if (size)
		*size = buffer.st_size;

	if (blocks)
		*blocks = (buffer.st_size / 2448) +
			  ((buffer.st_size % 2448) ? 1:0);

	return TRUE;
}
