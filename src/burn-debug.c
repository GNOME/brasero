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

#include <glib.h>

#include "burn-debug.h"

static gboolean debug = FALSE;

void
brasero_burn_set_debug (gboolean debug_value)
{
	debug = debug_value;
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
