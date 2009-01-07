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
 
#ifndef _BRASERO_UNITS_H
#define BRASERO_UNITS_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * Used to convert between known units
 **/

#define BRASERO_DURATION_TO_BYTES(duration)					\
	((gint64) (duration) * 75 * 2352 / 1000000000 +				\
	(((gint64) ((duration) * 75 * 2352) % 1000000000) ? 1:0))
#define BRASERO_DURATION_TO_SECTORS(duration)					\
	((gint64) (duration) * 75 / 1000000000 +				\
	(((gint64) ((duration) * 75) % 1000000000) ? 1:0))
#define BRASERO_SIZE_TO_SECTORS(size, secsize)					\
	(((size) / (secsize)) + (((size) % (secsize)) ? 1:0))
#define BRASERO_BYTES_TO_DURATION(bytes)					\
	(guint64) ((guint64) ((guint64) (bytes) * 1000000000) / (guint64) (2352 * 75) + 				\
	(guint64) (((guint64) ((guint64) (bytes) * 1000000000) % (guint64) (2352 * 75)) ? 1:0))

/**
 * Used to get string
 */

gchar *
brasero_units_get_time_string (guint64 time,
			       gboolean with_unit,
			       gboolean round);

gchar *
brasero_units_get_time_string_from_size (gint64 size,
					 gboolean with_unit,
					 gboolean round);

G_END_DECLS

#endif /* BRASERO_UNITS_H */

 
