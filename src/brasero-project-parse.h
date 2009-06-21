/***************************************************************************
 *            disc.h
 *
 *  dim nov 27 14:58:13 2005
 *  Copyright  2005  Rouquier Philippe
 *  brasero-app@wanadoo.fr
 ***************************************************************************/

/*
 *  Brasero is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Brasero is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifndef _BRASERO_PROJECT_PARSE_H_
#define _BRASERO_PROJECT_PARSE_H_

#include <glib.h>

#include "brasero-track.h"
#include "brasero-session.h"

G_BEGIN_DECLS

typedef enum {
	BRASERO_PROJECT_SAVE_XML			= 0,
	BRASERO_PROJECT_SAVE_PLAIN			= 1,
	BRASERO_PROJECT_SAVE_PLAYLIST_PLS		= 2,
	BRASERO_PROJECT_SAVE_PLAYLIST_M3U		= 3,
	BRASERO_PROJECT_SAVE_PLAYLIST_XSPF		= 4,
	BRASERO_PROJECT_SAVE_PLAYLIST_IRIVER_PLA	= 5
} BraseroProjectSave;

typedef enum {
	BRASERO_PROJECT_TYPE_INVALID,
	BRASERO_PROJECT_TYPE_COPY,
	BRASERO_PROJECT_TYPE_ISO,
	BRASERO_PROJECT_TYPE_AUDIO,
	BRASERO_PROJECT_TYPE_DATA,
	BRASERO_PROJECT_TYPE_VIDEO
} BraseroProjectType;

gboolean
brasero_project_open_project_xml (const gchar *uri,
				  BraseroBurnSession *session,
				  gboolean warn_user);

gboolean
brasero_project_open_audio_playlist_project (const gchar *uri,
					     BraseroBurnSession *session,
					     gboolean warn_user);

gboolean 
brasero_project_save_project_xml (BraseroBurnSession *session,
				  const gchar *uri);

gboolean
brasero_project_save_audio_project_plain_text (BraseroBurnSession *session,
					       const gchar *uri);

gboolean
brasero_project_save_audio_project_playlist (BraseroBurnSession *session,
					     const gchar *uri,
					     BraseroProjectSave type);

G_END_DECLS

#endif
