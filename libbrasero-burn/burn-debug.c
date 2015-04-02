/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-burn
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-burn is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-burn authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-burn. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-burn is distributed in the hope that it will be useful,
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
#include <stdio.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gmodule.h>

#include "brasero-media-private.h"

#include "burn-debug.h"
#include "brasero-track.h"
#include "brasero-media.h"

#include "brasero-burn-lib.h"

static gboolean debug = FALSE;

static const GOptionEntry options [] = {
	{ "brasero-burn-debug", 'g', 0, G_OPTION_ARG_NONE, &debug,
	  N_("Display debug statements on stdout for Brasero burn library"),
	  NULL },
	{ NULL }
};

void
brasero_burn_library_set_debug (gboolean value)
{
	debug = value;
}

/**
 * brasero_burn_library_get_option_group:
 *
 * Returns a GOptionGroup for the commandline arguments recognized by libbrasero-burn.
 * You should add this to your GOptionContext if your are using g_option_context_parse ()
 * to parse your commandline arguments.
 *
 * Return value: a #GOptionGroup *
 **/
GOptionGroup *
brasero_burn_library_get_option_group (void)
{
	GOptionGroup *group;

	group = g_option_group_new ("brasero-burn",
				    N_("Brasero media burning library"),
				    N_("Display options for Brasero-burn library"),
				    NULL,
				    NULL);
	g_option_group_add_entries (group, options);
	return group;
}

void
brasero_burn_debug_setup_module (GModule *handle)
{
	if (debug)
		g_module_make_resident (handle);
}

void
brasero_burn_debug_message (const gchar *location,
			    const gchar *format,
			    ...)
{
	va_list arg_list;

	if (!debug)
		return;

	printf ("BraseroBurn: (at %s) ", location);

	va_start (arg_list, format);
	vprintf (format, arg_list);
	va_end (arg_list);

	putchar ('\n');
}

void
brasero_burn_debug_messagev (const gchar *location,
			     const gchar *format,
			     va_list arg_list)
{
	if (!debug)
		return;

	printf ("BraseroBurn: (at %s) ", location);
	vprintf (format, arg_list);
	putchar ('\n');
}

static void
brasero_debug_burn_flags_to_string (gchar *buffer,
				    BraseroBurnFlag flags)
{	
	if (flags & BRASERO_BURN_FLAG_EJECT)
		strcat (buffer, "eject, ");
	if (flags & BRASERO_BURN_FLAG_NOGRACE)
		strcat (buffer, "no grace, ");
	if (flags & BRASERO_BURN_FLAG_DAO)
		strcat (buffer, "dao, ");
	if (flags & BRASERO_BURN_FLAG_RAW)
		strcat (buffer, "raw, ");
	if (flags & BRASERO_BURN_FLAG_OVERBURN)
		strcat (buffer, "overburn, ");
	if (flags & BRASERO_BURN_FLAG_BURNPROOF)
		strcat (buffer, "burnproof, ");
	if (flags & BRASERO_BURN_FLAG_NO_TMP_FILES)
		strcat (buffer, "no tmp file, ");
	if (flags & BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE)
		strcat (buffer, "blank before, ");
	if (flags & BRASERO_BURN_FLAG_APPEND)
		strcat (buffer, "append, ");
	if (flags & BRASERO_BURN_FLAG_MERGE)
		strcat (buffer, "merge, ");
	if (flags & BRASERO_BURN_FLAG_MULTI)
		strcat (buffer, "multi, ");
	if (flags & BRASERO_BURN_FLAG_DUMMY)
		strcat (buffer, "dummy, ");
	if (flags & BRASERO_BURN_FLAG_CHECK_SIZE)
		strcat (buffer, "check size, ");
	if (flags & BRASERO_BURN_FLAG_FAST_BLANK)
		strcat (buffer, "fast blank");	
}

void
brasero_burn_debug_flags_type_message (BraseroBurnFlag flags,
				       const gchar *location,
				       const gchar *format,
				       ...)
{
	gchar buffer [256] = {0};
	va_list arg_list;

	if (!debug)
		return;

	brasero_debug_burn_flags_to_string (buffer, flags);

	printf ("BraseroBurn: (at %s) ", location);

	va_start (arg_list, format);
	vprintf (format, arg_list);
	va_end (arg_list);

	putchar (' ');
	fputs (buffer, stdout);
	putchar ('\n');
}

static void
brasero_debug_image_format_to_string (gchar *buffer,
				      BraseroImageFormat format)
{
	if (format & BRASERO_IMAGE_FORMAT_BIN)
		strcat (buffer, "BIN ");
	if (format & BRASERO_IMAGE_FORMAT_CUE)
		strcat (buffer, "CUE ");
	if (format & BRASERO_IMAGE_FORMAT_CDRDAO)
		strcat (buffer, "CDRDAO ");
	if (format & BRASERO_IMAGE_FORMAT_CLONE)
		strcat (buffer, "CLONE ");
}

static void
brasero_debug_data_fs_to_string (gchar *buffer,
				 BraseroImageFS fs_type)
{
	if (fs_type & BRASERO_IMAGE_FS_ISO)
		strcat (buffer, "ISO ");
	if (fs_type & BRASERO_IMAGE_FS_UDF)
		strcat (buffer, "UDF ");
	if (fs_type & BRASERO_IMAGE_FS_SYMLINK)
		strcat (buffer, "SYMLINK ");
	if (fs_type & BRASERO_IMAGE_ISO_FS_LEVEL_3)
		strcat (buffer, "Level 3 ");
	if (fs_type & BRASERO_IMAGE_FS_JOLIET)
		strcat (buffer, "JOLIET ");
	if (fs_type & BRASERO_IMAGE_FS_VIDEO)
		strcat (buffer, "VIDEO ");
	if (fs_type & BRASERO_IMAGE_ISO_FS_DEEP_DIRECTORY)
		strcat (buffer, "DEEP ");
}

static void
brasero_debug_audio_format_to_string (gchar *buffer,
				      BraseroStreamFormat format)
{
	if (format & BRASERO_AUDIO_FORMAT_RAW)
		strcat (buffer, "RAW ");

	if (format & BRASERO_AUDIO_FORMAT_RAW_LITTLE_ENDIAN)
		strcat (buffer, "RAW (little endian)");

	if (format & BRASERO_AUDIO_FORMAT_UNDEFINED)
		strcat (buffer, "AUDIO UNDEFINED ");

	if (format & BRASERO_AUDIO_FORMAT_DTS)
		strcat (buffer, "DTS WAV ");

	if (format & BRASERO_AUDIO_FORMAT_MP2)
		strcat (buffer, "MP2 ");

	if (format & BRASERO_AUDIO_FORMAT_AC3)
		strcat (buffer, "AC3 ");

	if (format & BRASERO_AUDIO_FORMAT_44100)
		strcat (buffer, "44100 ");

	if (format & BRASERO_AUDIO_FORMAT_48000)
		strcat (buffer, "48000 ");

	if (format & BRASERO_VIDEO_FORMAT_UNDEFINED)
		strcat (buffer, "VIDEO UNDEFINED ");

	if (format & BRASERO_VIDEO_FORMAT_VCD)
		strcat (buffer, "VCD ");

	if (format & BRASERO_VIDEO_FORMAT_VCD)
		strcat (buffer, "Video DVD ");

	if (format & BRASERO_METADATA_INFO)
		strcat (buffer, "Metadata Information ");
}

void
brasero_burn_debug_track_type_struct_message (BraseroTrackType *type,
					      BraseroPluginIOFlag flags,
					      const gchar *location,
					      const gchar *format,
					      ...)
{
	gchar buffer [256];
	va_list arg_list;

	if (!debug)
		return;

	if (brasero_track_type_get_has_data (type)) {
		strcpy (buffer, "Data ");
		brasero_debug_data_fs_to_string (buffer, brasero_track_type_get_data_fs (type));
	}
	else if (brasero_track_type_get_has_medium (type)) {
		strcpy (buffer, "Disc ");
		brasero_media_to_string (brasero_track_type_get_medium_type (type), buffer);
	}
	else if (brasero_track_type_get_has_stream (type)) {
		strcpy (buffer, "Audio ");
		brasero_debug_audio_format_to_string (buffer, brasero_track_type_get_stream_format (type));

		if (flags != BRASERO_PLUGIN_IO_NONE) {
			strcat (buffer, "format accepts ");

			if (flags & BRASERO_PLUGIN_IO_ACCEPT_FILE)
				strcat (buffer, "files ");
			if (flags & BRASERO_PLUGIN_IO_ACCEPT_PIPE)
				strcat (buffer, "pipe ");
		}
	}
	else if (brasero_track_type_get_has_image (type)) {
		strcpy (buffer, "Image ");
		brasero_debug_image_format_to_string (buffer, brasero_track_type_get_image_format (type));

		if (flags != BRASERO_PLUGIN_IO_NONE) {
			strcat (buffer, "format accepts ");

			if (flags & BRASERO_PLUGIN_IO_ACCEPT_FILE)
				strcat (buffer, "files ");
			if (flags & BRASERO_PLUGIN_IO_ACCEPT_PIPE)
				strcat (buffer, "pipe ");
		}
	}
	else
		strcpy (buffer, "Undefined");

	printf ("BraseroBurn: (at %s) ", location);

	va_start (arg_list, format);
	vprintf (format, arg_list);
	va_end (arg_list);

	putchar (' ');
	fputs (buffer, stdout);
	putchar ('\n');
}

void
brasero_burn_debug_track_type_message (BraseroTrackDataType type,
				       guint subtype,
				       BraseroPluginIOFlag flags,
				       const gchar *location,
				       const gchar *format,
				       ...)
{
	gchar buffer [256];
	va_list arg_list;

	if (!debug)
		return;

	switch (type) {
	case BRASERO_TRACK_TYPE_DATA:
		strcpy (buffer, "Data ");
		brasero_debug_data_fs_to_string (buffer, subtype);
		break;
	case BRASERO_TRACK_TYPE_DISC:
		strcpy (buffer, "Disc ");
		brasero_media_to_string (subtype, buffer);
		break;
	case BRASERO_TRACK_TYPE_STREAM:
		strcpy (buffer, "Audio ");
		brasero_debug_audio_format_to_string (buffer, subtype);

		if (flags != BRASERO_PLUGIN_IO_NONE) {
			strcat (buffer, "format accepts ");

			if (flags & BRASERO_PLUGIN_IO_ACCEPT_FILE)
				strcat (buffer, "files ");
			if (flags & BRASERO_PLUGIN_IO_ACCEPT_PIPE)
				strcat (buffer, "pipe ");
		}
		break;
	case BRASERO_TRACK_TYPE_IMAGE:
		strcpy (buffer, "Image ");
		brasero_debug_image_format_to_string (buffer, subtype);

		if (flags != BRASERO_PLUGIN_IO_NONE) {
			strcat (buffer, "format accepts ");

			if (flags & BRASERO_PLUGIN_IO_ACCEPT_FILE)
				strcat (buffer, "files ");
			if (flags & BRASERO_PLUGIN_IO_ACCEPT_PIPE)
				strcat (buffer, "pipe ");
		}
		break;
	default:
		strcpy (buffer, "Undefined");
		break;
	}

	printf ("BraseroBurn: (at %s) ", location);

	va_start (arg_list, format);
	vprintf (format, arg_list);
	va_end (arg_list);

	putchar (' ');
	fputs (buffer, stdout);
	putchar ('\n');
}

