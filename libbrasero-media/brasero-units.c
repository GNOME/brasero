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
	gchar *second_str, *minute_str, *hour_str;
	gchar *time_str;

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

	minute_str = g_strdup_printf ("%02"G_GINT64_FORMAT, minute);
	second_str = g_strdup_printf ("%02"G_GINT64_FORMAT, second);

	if (hour) {
		hour_str = g_strdup_printf ("%"G_GINT64_FORMAT, hour);
		if (with_unit) {
			if (second)
				/* Translators: this is hour minute second like '2 h 14 min 25' */
				time_str = g_strdup_printf (_("%s h %s min %s"),
							    hour_str,
							    minute_str,
							    second_str);
			else if (minute)
				/* Translators: this is hour minute like '2 h 14' */
				time_str = g_strdup_printf (_("%s h %s"),
							    hour_str,
							    minute_str);
			else
				/* Translators: this is hour like '2 h' */
				time_str = g_strdup_printf (_("%s h"), hour_str);
		}
		else if (second)
			/* Translators: this is 'hour:minute:second' like '2:14:25' */
			time_str = g_strdup_printf (_("%s:%s:%s"),
						    hour_str,
						    minute_str,
						    second_str);
		else
			/* Translators: this is 'hour:minute' or 'minute:second' */
			time_str = g_strdup_printf (_("%s:%s"), hour_str, minute_str);

		g_free (hour_str);
	}
	else if (with_unit) {
		if (!second)
			/* Translators: %s is a duration expressed in minutes */
			time_str = g_strdup_printf (_("%s min"), minute_str);
		else
			/* Translators: the first %s is the number of minutes
			 * and the second one is the number of seconds.
			 * The whole string expresses a duration */
			time_str = g_strdup_printf (_("%s:%s min"), minute_str, second_str);
	}
	else
		time_str = g_strdup_printf (_("%s:%s"), minute_str, second_str);

	g_free (minute_str);
	g_free (second_str);
	return time_str;
}


gchar *
brasero_units_get_time_string_from_size (gint64 bytes,
					 gboolean with_unit,
					 gboolean round)
{
	guint64 time = 0;

	time = BRASERO_BYTES_TO_DURATION (bytes);
	return brasero_units_get_time_string (time, with_unit, round);
}

