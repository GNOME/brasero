/***************************************************************************
 *            burn-media-private.h
 *
 *  Wed Oct  8 16:42:17 2008
 *  Copyright  2008  ykw
 *  <ykw@localhost.localdomain>
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
 
#ifndef _BURN_MEDIA_PRIV_H_
#define _BURN_MEDIA_PRIV_H_

#include <glib.h>

#include "brasero-media.h"

G_BEGIN_DECLS

/**
 * These functions will be exposed when the burn backend will be split
 */

GSList *
brasero_media_get_all_list (BraseroMedia type);

BraseroMedia
brasero_media_capabilities (BraseroMedia media);


/**
 * For internal debugging purposes
 */

void
brasero_media_to_string (BraseroMedia media,
			 gchar *string);

#define BRASERO_MEDIA_LOG(format, ...)				\
	brasero_media_message (G_STRLOC,			\
			       format,				\
			       ##__VA_ARGS__);

void
brasero_media_message (const gchar *location,
		       const gchar *format,
		       ...);

G_END_DECLS

#endif /* _BURN_MEDIA_PRIV_H_ */

 
