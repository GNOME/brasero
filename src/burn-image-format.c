/***************************************************************************
 *            burn-images-format.c
 *
 *  Mon Nov  5 16:01:44 2007
 *  Copyright  2007  Philippe Rouquier
 *  <bonfire-app@wanadoo.fr>
 ****************************************************************************/

/*
 * Libbrasero-media is free software; you can redistribute it and/or modify
fy
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
						(int) strlen (path) - 3,
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
						(int) strlen (path) - 3,
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
		/* These are set rules no need to parse:
		 * the toc file has to end with .toc suffix */
		if (g_str_has_suffix (path, ".toc"))
			retval = g_strndup (path, strlen (path) - 4);
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
					GFile *parent,
					gint64 *size_file,
					GError **error)
{
	gchar *path = NULL;
	GFileInfo *info;
	GFile *file;

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

	if (!brasero_image_format_get_MSF_address (ptr, size_file)) {
		g_free (path);
		return FALSE;
	}

	g_free (path);
	return TRUE;

stat_end:

	/* check if the path is relative, if so then add the root path */
	if (path && !g_path_is_absolute (path))
		file = g_file_resolve_relative_path (parent, path);
	else if (path) {
		gchar *img_uri;
		gchar *scheme;

		scheme = g_file_get_uri_scheme (parent);
		img_uri = g_strconcat (scheme, "://", path, NULL);
		g_free (scheme);

		file = g_file_new_for_commandline_arg (img_uri);
		g_free (img_uri);
	}

	g_free (path);

	/* NOTE: follow symlink if any */
	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_STANDARD_SIZE,
				  G_FILE_QUERY_INFO_NONE,
				  NULL,
				  error);
	g_object_unref (file);
	if (!info)
		return FALSE;

	if (size_file)
		*size_file = BRASERO_BYTES_TO_SECTORS (g_file_info_get_size (info), 2352);

	g_object_unref (info);

	return TRUE;
}

static gboolean
brasero_image_format_get_FILE_info (const gchar *ptr,
				    GFile *parent,
				    gint64 *size_img,
				    GError **error)
{
	gchar *path = NULL;
	gint64 start = 0;
	GFileInfo *info;
	GFile *file;
	gchar *tmp;

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
	if (!brasero_image_format_get_MSF_address (ptr, size_img)) {
		g_free (path);
		return FALSE;
	}

	g_free (path);
	return TRUE;

stat_end:

	/* check if the path is relative, if so then add the root path */
	if (path && !g_path_is_absolute (path))
		file = g_file_resolve_relative_path (parent, path);
	else if (path) {
		gchar *img_uri;
		gchar *scheme;

		scheme = g_file_get_uri_scheme (parent);
		img_uri = g_strconcat (scheme, "://", path, NULL);
		g_free (scheme);

		file = g_file_new_for_commandline_arg (img_uri);
		g_free (img_uri);
	}

	g_free (path);

	/* NOTE: follow symlink if any */
	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_STANDARD_SIZE,
				  G_FILE_QUERY_INFO_NONE,
				  NULL,
				  error);
	g_object_unref (file);
	if (!info)
		return FALSE;

	if (size_img)
		*size_img = BRASERO_BYTES_TO_SECTORS (g_file_info_get_size (info), 2352) - start;

	g_object_unref (info);

	return TRUE;
}

gboolean
brasero_image_format_get_cdrdao_size (gchar *uri,
				      gint64 *sectors,
				      gint64 *size_img,
				      GError **error)
{
	GFile *file;
	gchar *line;
	GFile *parent;
	gint64 cue_size = 0;
	GFileInputStream *input;
	GDataInputStream *stream;

	file = g_file_new_for_uri (uri);
	input = g_file_read (file, NULL, error);

	if (!input) {
		g_object_unref (file);
		return FALSE;
	}

	stream = g_data_input_stream_new (G_INPUT_STREAM (input));
	g_object_unref (input);

	parent = g_file_get_parent (file);
	while ((line = g_data_input_stream_read_line (stream, NULL, NULL, error))) {
		gchar *ptr;

		if ((ptr = strstr (line, "DATAFILE"))) {
			gint64 size_file;

			ptr += 8;
			if (brasero_image_format_get_DATAFILE_info (ptr, parent, &size_file, error))
				cue_size += size_file;
		}
		else if ((ptr = strstr (line, "FILE"))) {
			gint64 size_file;

			ptr += 4;
			/* first number is the position, the second the size,
			 * number after '#' is the offset (in bytes). */
			if (brasero_image_format_get_FILE_info (ptr, parent, &size_file, error))
				cue_size += size_file;
		}
		else if ((ptr = strstr (line, "AUDIOFILE"))) {
			gint64 size_file;

			ptr += 4;
			/* first number is the position, the second the size,
			 * number after '#' is the offset (in bytes). */
			if (brasero_image_format_get_FILE_info (ptr, parent, &size_file, error))
				cue_size += size_file;
		}
		else if ((ptr = strstr (line, "SILENCE"))) {
			gint64 size_silence;

			ptr += 7;
			if (isspace (*ptr)
			&&  brasero_image_format_get_MSF_address (ptr, &size_silence))
				cue_size += size_silence;
		}
		else if ((ptr = strstr (line, "PREGAP"))) {
			gint64 size_pregap;

			ptr += 6;
			if (isspace (*ptr)
			&&  brasero_image_format_get_MSF_address (ptr, &size_pregap))
				cue_size += size_pregap;
		}
		else if ((ptr = strstr (line, "ZERO"))) {
			gint64 size_zero;

			ptr += 4;
			if (isspace (*ptr)
			&&  brasero_image_format_get_MSF_address (ptr, &size_zero))
				cue_size += size_zero;
		}

		g_free (line);
	}
	g_object_unref (parent);

	g_object_unref (stream);
	g_object_unref (file);

	if (sectors)
		*sectors = cue_size;

	if (size_img)
		*size_img = cue_size * 2352;

	return TRUE;
}

/**
 * .cue can use various data files but have to use them ALL. So we don't need
 * to care about a start/size address. We just go through the whole file and
 * stat every time we catch a FILE keyword.
 */

gboolean
brasero_image_format_get_cue_size (gchar *uri,
				   gint64 *blocks,
				   gint64 *size_img,
				   GError **error)
{
	GFile *file;
	gchar *line;
	gint64 cue_size = 0;
	GFileInputStream *input;
	GDataInputStream *stream;

	file = g_file_new_for_uri (uri);
	input = g_file_read (file, NULL, error);

	if (!input) {
		g_object_unref (file);
		return FALSE;
	}

	stream = g_data_input_stream_new (G_INPUT_STREAM (input));
	g_object_unref (input);

	while ((line = g_data_input_stream_read_line (stream, NULL, NULL, error))) {
		const gchar *ptr;

		if ((ptr = strstr (line, "FILE"))) {
			GFileInfo *info;
			GFile *file_img;
			gchar *file_path;

			ptr += 4;

			/* get the path */
			ptr = brasero_image_format_read_path (ptr, &file_path);
			if (!ptr) {
				g_object_unref (stream);
				g_object_unref (file);
				g_free (line);
				return FALSE;
			}

			/* check if the path is relative, if so then add the root path */
			if (file_path && !g_path_is_absolute (file_path)) {
				GFile *parent;

				parent = g_file_get_parent (file);
				file_img = g_file_resolve_relative_path (parent, file_path);
				g_object_unref (parent);
			}
			else if (file_path) {
				gchar *img_uri;
				gchar *scheme;

				scheme = g_file_get_uri_scheme (file);
				img_uri = g_strconcat (scheme, "://", file_path, NULL);
				g_free (scheme);

				file_img = g_file_new_for_commandline_arg (img_uri);
				g_free (img_uri);
			}

			g_free (file_path);

			/* NOTE: follow symlink if any */
			info = g_file_query_info (file_img,
						  G_FILE_ATTRIBUTE_STANDARD_SIZE,
						  G_FILE_QUERY_INFO_NONE,
						  NULL,
						  error);
			g_object_unref (file_img);

			if (!info) {
				g_free (line);
				g_object_unref (file);
				g_object_unref (stream);
				return FALSE;
			}

			cue_size += g_file_info_get_size (info);
			g_object_unref (info);
		}
		else if ((ptr = strstr (line, "PREGAP"))) {
			ptr += 6;
			if (isspace (*ptr)) {
				gint64 size_pregap;

				ptr ++;
				ptr = brasero_image_format_get_MSF_address (ptr, &size_pregap);
				if (ptr)
					cue_size += size_pregap * 2352;
			}
		}
		else if ((ptr = strstr (line, "POSTGAP"))) {
			ptr += 7;
			if (isspace (*ptr)) {
				gint64 size_postgap;

				ptr ++;
				ptr = brasero_image_format_get_MSF_address (ptr, &size_postgap);
				if (ptr)
					cue_size += size_postgap * 2352;
			}
		}

		g_free (line);
	}

	g_object_unref (stream);
	g_object_unref (file);

	if (size_img)
		*size_img = cue_size;
	if (blocks)
		*blocks = BRASERO_BYTES_TO_SECTORS (cue_size, 2352);

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
brasero_image_format_get_iso_size (gchar *uri,
				   gint64 *blocks,
				   gint64 *size_img,
				   GError **error)
{
	GFileInfo *info;
	GFile *file;

	if (!uri)
		return FALSE;

	/* NOTE: follow symlink if any */
	file = g_file_new_for_uri (uri);
	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_STANDARD_SIZE,
				  G_FILE_QUERY_INFO_NONE,
				  NULL,
				  error);
	g_object_unref (file);
	if (!info)
		return FALSE;

	if (size_img)
		*size_img = g_file_info_get_size (info);

	if (blocks)
		*blocks = BRASERO_BYTES_TO_SECTORS (g_file_info_get_size (info), 2048);

	g_object_unref (info);
	return TRUE;
}

gboolean
brasero_image_format_get_clone_size (gchar *uri,
				     gint64 *blocks,
				     gint64 *size_img,
				     GError **error)
{
	GFileInfo *info;
	GFile *file;

	if (!uri)
		return FALSE;

	/* NOTE: follow symlink if any */
	file = g_file_new_for_uri (uri);
	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_STANDARD_SIZE,
				  G_FILE_QUERY_INFO_NONE,
				  NULL,
				  error);
	g_object_unref (file);

	if (!info)
		return FALSE;

	if (size_img)
		*size_img = g_file_info_get_size (info);

	if (blocks)
		*blocks = BRASERO_BYTES_TO_SECTORS (g_file_info_get_size (info), 2448);

	g_object_unref (info);

	return TRUE;
}

gchar *
brasero_image_format_get_default_path (BraseroImageFormat format)
{
	const gchar *suffixes [] = {".iso",
				    ".toc",
				    ".cue",
				    ".toc",
				    NULL };
	const gchar *suffix = NULL;
	gchar *path;
	gint i = 0;

	if (format & BRASERO_IMAGE_FORMAT_BIN)
		suffix = suffixes [0];
	else if (format & BRASERO_IMAGE_FORMAT_CLONE)
		suffix = suffixes [1];
	else if (format & BRASERO_IMAGE_FORMAT_CUE)
		suffix = suffixes [2];
	else if (format & BRASERO_IMAGE_FORMAT_CDRDAO)
		suffix = suffixes [3];

	path = g_strdup_printf ("%s/brasero%s",
				g_get_home_dir (),
				suffix);

	while (g_file_test (path, G_FILE_TEST_EXISTS)) {
		g_free (path);

		path = g_strdup_printf ("%s/brasero-%i%s",
					g_get_home_dir (),
					i,
					suffix);
		i ++;
	}

	return path;
}

gchar *
brasero_image_format_fix_path_extension (BraseroImageFormat format,
					 gboolean check_existence,
					 gchar *path)
{
	gchar *dot;
	guint i = 0;
	gchar *retval = NULL;
	const gchar *suffix = NULL;;
	const gchar *suffixes [] = {".iso",
				    ".toc",
				    ".cue",
				    ".toc",
				    NULL };

	/* search the last dot to check extension */
	dot = g_utf8_strrchr (path, -1, '.');
	if (dot && strlen (dot) < 5 && strlen (dot) > 1) {
		if (format & BRASERO_IMAGE_FORMAT_BIN
		&&  strcmp (suffixes [0], dot))
			*dot = '\0';
		else if (format & BRASERO_IMAGE_FORMAT_CLONE
		     &&  strcmp (suffixes [1], dot))
			*dot = '\0';
		else if (format & BRASERO_IMAGE_FORMAT_CUE
		     &&  strcmp (suffixes [2], dot))
			*dot = '\0';
		else if (format & BRASERO_IMAGE_FORMAT_CDRDAO
		     &&  strcmp (suffixes [3], dot))
			*dot = '\0';
		else
			return g_strdup (path);
	}

	/* determine the proper suffix */
	if (format & BRASERO_IMAGE_FORMAT_BIN)
		suffix = suffixes [0];
	else if (format & BRASERO_IMAGE_FORMAT_CLONE)
		suffix = suffixes [1];
	else if (format & BRASERO_IMAGE_FORMAT_CUE)
		suffix = suffixes [2];
	else if (format & BRASERO_IMAGE_FORMAT_CDRDAO)
		suffix = suffixes [3];
	else
		return g_strdup (path);

	/* make sure the file doesn't exist */
	retval = g_strdup_printf ("%s%s", path, suffix);
	if (!check_existence)
		return retval;

	while (g_file_test (retval, G_FILE_TEST_EXISTS)) {
		g_free (retval);
		retval = g_strdup_printf ("%s-%i%s", path, i, suffix);
		i ++;
	}

	return retval;
}
