/***************************************************************************
 *            burn-debug.h
 *
 *  Sat Apr 14 11:59:32 2007
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
 
#ifndef _BURN_DEBUG_H
#define _BURN_DEBUG_H

#include <glib.h>

G_BEGIN_DECLS

#define BRASERO_BURN_LOG_DOMAIN				"BraseroBurn"
#define BRASERO_BURN_LOG(format, ...)				\
		brasero_burn_debug_message (G_STRLOC,		\
					    format,		\
					    ##__VA_ARGS__);
#define BRASERO_BURN_LOGV(format)				\
	{							\
		va_list args_list;				\
		va_start (args_list, format);			\
		brasero_burn_debug_messagev (G_STRLOC,		\
					     format,		\
					     args_list);	\
		va_end (args_list);				\
	}

void
brasero_burn_set_debug (gboolean debug_value);

void
brasero_burn_debug_message (const gchar *location,
			    const gchar *format,
			    ...);

void
brasero_burn_debug_messagev (const gchar *location,
			     const gchar *format,
			     va_list args);

G_END_DECLS

#endif /* _BURN_DEBUG_H */

 
