/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-media
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-media is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-media authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-media. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-media is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-media is distributed in the hope that it will be useful,
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

#include <glib.h>
#include <glib/gi18n-lib.h>

#include "brasero-units.h"

gchar *
brasero_units_get_time_string (guint64 time,
			       gboolean with_unit,
			       gboolean round)
{
	gint64 second, minute, hour;

	time /= 1000000000;
	hour = time / 3600;
	time = time % 3600;
	minute = time / 60;

	if (round) {
		if ((time % 60) > 30)
			minute ++;

		second = 0;
	}
	else
		second = time % 60;

	if (hour) {
		if (with_unit) {
			if (hour && minute && second)
				/* FIXME: mark these strings for translation? */
				return g_strdup_printf ("%"G_GINT64_FORMAT" h %02"G_GINT64_FORMAT" min %02"G_GINT64_FORMAT,
							 hour,
							 minute,
							 second);
			else if (hour && minute)
				return g_strdup_printf ("%" G_GINT64_FORMAT " h %02"G_GINT64_FORMAT,
							 hour,
							 minute);
			else
				return g_strdup_printf ("%"G_GINT64_FORMAT " h", hour);
		}
		else if (hour && minute && second)
			return g_strdup_printf ("%"G_GINT64_FORMAT":%02"G_GINT64_FORMAT":%02"G_GINT64_FORMAT,
						 hour,
						 minute,
						 second);
		else if (hour && minute)
			return g_strdup_printf ("%"G_GINT64_FORMAT":%02"G_GINT64_FORMAT, hour, minute);
	}

	if (with_unit) {
		if (!second)
			/* Translators: %lli is a duration expressed in minutes
			 * hence the "min" as unit. */
			return g_strdup_printf (_("%"G_GINT64_FORMAT" min"), minute);
		else
			/* Translators: the first %lli is the number of minutes
			 * and the second one is the number of seconds.
			 * The whole string expresses a duration */
			return g_strdup_printf (_("%"G_GINT64_FORMAT":%02"G_GINT64_FORMAT" min"), minute, second);
	}
	else
		return g_strdup_printf ("%"G_GINT64_FORMAT":%02"G_GINT64_FORMAT, minute, second);
}


gchar *
brasero_units_get_time_string_from_size (gint64 size,
					 gboolean with_unit,
					 gboolean round)
{
	guint64 time = 0;

	time = BRASERO_BYTES_TO_DURATION (size);
	return brasero_units_get_time_string (time, with_unit, round);
}

