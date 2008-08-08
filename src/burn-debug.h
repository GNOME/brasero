/***************************************************************************
 *            burn-debug.h
 *
 *  Sat Apr 14 11:59:32 2007
 *  Copyright  2007  Rouquier Philippe
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
 
#ifndef _BURN_DEBUG_H
#define _BURN_DEBUG_H

#include <glib.h>
#include <gmodule.h>

#include "burn-medium.h"
#include "burn-track.h"
#include "burn-plugin.h"

G_BEGIN_DECLS

#define BRASERO_BURN_LOG_DOMAIN					"BraseroBurn"

#define BRASERO_BURN_LOG(format, ...)				\
		brasero_burn_debug_message (G_STRLOC,		\
					    format,		\
					    ##__VA_ARGS__);

#define BRASERO_BURN_LOGV(format, args_list)			\
		brasero_burn_debug_messagev (G_STRLOC,		\
					     format,		\
					     args_list);

#define BRASERO_BURN_LOG_DISC_TYPE(media_MACRO, format, ...)	\
		BRASERO_BURN_LOG_WITH_FULL_TYPE (BRASERO_TRACK_TYPE_DISC,	\
						 media_MACRO,	\
						 BRASERO_PLUGIN_IO_NONE,	\
						 format,			\
						 ##__VA_ARGS__);

#define BRASERO_BURN_LOG_FLAGS(flags_MACRO, format, ...)			\
		brasero_burn_debug_flags_type_message (flags_MACRO,		\
						       G_STRLOC,		\
						       format,			\
						       ##__VA_ARGS__);

#define BRASERO_BURN_LOG_TYPE(type_MACRO, format, ...)				\
		BRASERO_BURN_LOG_WITH_TYPE (type_MACRO,				\
					    BRASERO_PLUGIN_IO_NONE,		\
					    format,				\
					    ##__VA_ARGS__);
#define BRASERO_BURN_LOG_WITH_TYPE(type_MACRO, flags_MACRO, format, ...)	\
		BRASERO_BURN_LOG_WITH_FULL_TYPE ((type_MACRO)->type,		\
						 (type_MACRO)->subtype.media,	\
						 flags_MACRO,			\
						 format,			\
						 ##__VA_ARGS__);

#define BRASERO_BURN_LOG_WITH_FULL_TYPE(type_MACRO, subtype_MACRO, flags_MACRO, format, ...)	\
		brasero_burn_debug_track_type_message ((type_MACRO),				\
						       (subtype_MACRO),				\
						       (flags_MACRO),				\
						       G_STRLOC,				\
						       format,					\
						       ##__VA_ARGS__);
void
brasero_burn_set_debug (gboolean debug_value);

void
brasero_burn_debug_setup_module (GModule *handle);

void
brasero_burn_debug_track_type_message (BraseroTrackDataType type,
				       guint subtype,
				       BraseroPluginIOFlag flags,
				       const gchar *location,
				       const gchar *format,
				       ...);
void
brasero_burn_debug_flags_type_message (BraseroBurnFlag flags,
				       const gchar *location,
				       const gchar *format,
				       ...);
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

 
