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

#include "burn-track.h"

G_BEGIN_DECLS

typedef enum {
	BRASERO_PROJECT_TYPE_INVALID,
	BRASERO_PROJECT_TYPE_COPY,
	BRASERO_PROJECT_TYPE_ISO,
	BRASERO_PROJECT_TYPE_AUDIO,
	BRASERO_PROJECT_TYPE_DATA,
	BRASERO_PROJECT_TYPE_VIDEO
} BraseroProjectType;

struct _BraseroDiscSong {
	gchar *uri;
	gint64 gap;
	gint64 start;
	gint64 end;

	BraseroSongInfo *info;
};
typedef struct _BraseroDiscSong BraseroDiscSong;

typedef struct {
	BraseroProjectType type;
	gchar *label;
	gchar *cover;

	union  {
		struct {
			GSList *grafts;
			GSList *excluded;
			GSList *restored;
		} data;

		GSList *tracks; /* BraseroDiscSong */
	} contents;
} BraseroDiscTrack;

void
brasero_track_clear (BraseroDiscTrack *track);
void
brasero_track_free (BraseroDiscTrack *track);

gboolean
brasero_project_open_project_xml (const gchar *uri,
				  BraseroDiscTrack **track,
				  gboolean warn_user);

gboolean
brasero_project_open_audio_playlist_project (const gchar *uri,
					     BraseroDiscTrack **track,
					     gboolean warn_user);

G_END_DECLS

#endif
