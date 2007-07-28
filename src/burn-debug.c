/***************************************************************************
 *            burn-debug.c
 *
 *  Sat Apr 14 10:53:08 2007
 *  Copyright  2007  algernon
 *  <algernon@localhost.localdomain>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include <glib.h>
#include <gmodule.h>

#include "burn-debug.h"
#include "burn-track.h"

static gboolean debug = FALSE;

void
brasero_burn_set_debug (gboolean debug_value)
{
	debug = debug_value;
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
	gchar *format_real;

	if (!debug)
		return;

	format_real = g_strdup_printf ("At %s: %s",
				       location,
				       format);

	va_start (arg_list, format);
	g_logv (BRASERO_BURN_LOG_DOMAIN,
		G_LOG_LEVEL_DEBUG,
		format_real,
		arg_list);
	va_end (arg_list);

	g_free (format_real);
}

void
brasero_burn_debug_messagev (const gchar *location,
			     const gchar *format,
			     va_list arg_list)
{
	gchar *format_real;

	if (!debug)
		return;

	format_real = g_strdup_printf ("At %s: %s",
				       location,
				       format);

	g_logv (BRASERO_BURN_LOG_DOMAIN,
		G_LOG_LEVEL_DEBUG,
		format_real,
		arg_list);

	g_free (format_real);
}

static void
brasero_debug_medium_info_to_string (gchar *buffer,
				     BraseroMedia media)
{
	if (media & BRASERO_MEDIUM_FILE)
		strcat (buffer, "file ");

	if (media & BRASERO_MEDIUM_CD)
		strcat (buffer, "CD ");

	if (media & BRASERO_MEDIUM_DVD)
		strcat (buffer, "DVD ");

	if (media & BRASERO_MEDIUM_RAM)
		strcat (buffer, "RAM");

	if (media & BRASERO_MEDIUM_BD)
		strcat (buffer, "BD ");

	/* DVD subtypes */
	if (media & BRASERO_MEDIUM_PLUS)
		strcat (buffer, "+ ");

	if (media & BRASERO_MEDIUM_SEQUENTIAL)
		strcat (buffer, "- (sequential) ");

	if (media & BRASERO_MEDIUM_RESTRICTED)
		strcat (buffer, "- (restricted) ");

	if (media & BRASERO_MEDIUM_DL)
		strcat (buffer, "DL ");

	/* discs attributes */
	if (media & BRASERO_MEDIUM_REWRITABLE)
		strcat (buffer, "RW ");

	if (media & BRASERO_MEDIUM_WRITABLE)
		strcat (buffer, "W ");

	if (media & BRASERO_MEDIUM_ROM)
		strcat (buffer, "ROM ");

	/* status of the disc */
	if (media & BRASERO_MEDIUM_CLOSED)
		strcat (buffer, "closed ");

	if (media & BRASERO_MEDIUM_BLANK)
		strcat (buffer, "blank ");

	if (media & BRASERO_MEDIUM_APPENDABLE)
		strcat (buffer, "appendable ");

	if (media & BRASERO_MEDIUM_PROTECTED)
		strcat (buffer, "protected ");

	if (media & BRASERO_MEDIUM_HAS_DATA)
		strcat (buffer, "with data ");

	if (media & BRASERO_MEDIUM_HAS_AUDIO)
		strcat (buffer, "with audio ");
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
	if (fs_type & BRASERO_IMAGE_FS_JOLIET)
		strcat (buffer, "JOLIET ");
	if (fs_type & BRASERO_IMAGE_FS_VIDEO)
		strcat (buffer, "VIDEO ");
}

static void
brasero_debug_audio_format_to_string (gchar *buffer,
				      BraseroAudioFormat format)
{
	if (format & BRASERO_AUDIO_FORMAT_RAW)
		strcat (buffer, "RAW ");

	if (format & BRASERO_AUDIO_FORMAT_UNDEFINED)
		strcat (buffer, "UNDEFINED ");

	if (format & BRASERO_AUDIO_FORMAT_4_CHANNEL)
		strcat (buffer, "4 CHANNELS ");
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
	gchar *format_real;
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
		brasero_debug_medium_info_to_string (buffer, subtype);
		break;
	case BRASERO_TRACK_TYPE_AUDIO:
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

	format_real = g_strdup_printf ("At %s: %s %s",
				       location,
				       format,
				       buffer);

	va_start (arg_list, format);
	g_logv (BRASERO_BURN_LOG_DOMAIN,
		G_LOG_LEVEL_DEBUG,
		format_real,
		arg_list);
	va_end (arg_list);

	g_free (format_real);
}
