/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Brasero
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Brasero is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Brasero authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Brasero. This permission is above and beyond the permissions granted
 * by the GPL license by which Brasero is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Brasero is distributed in the hope that it will be useful,
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
				return g_strdup_printf ("%lli h %02lli min %02lli",
							 hour,
							 minute,
							 second);
			else if (hour && minute)
				return g_strdup_printf ("%lli h %02lli",
							 hour,
							 minute);
			else
				return g_strdup_printf ("%lli h",hour);
		}
		else if (hour && minute && second)
			return g_strdup_printf ("%lli:%02lli:%02lli",
						 hour,
						 minute,
						 second);
		else if (hour && minute)
			return g_strdup_printf ("%lli:%02lli", hour, minute);
	}

	if (with_unit) {
		if (!second)
			return g_strdup_printf (_("%lli min"), minute);
		else
			return g_strdup_printf (_("%lli:%02lli min"), minute, second);
	}
	else
		return g_strdup_printf ("%lli:%02lli", minute, second);
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

