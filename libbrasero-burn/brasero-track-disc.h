/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-burn
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-burn is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-burn authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-burn. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-burn is distributed in the hope that it will be useful,
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

#ifndef _BRASERO_TRACK_DISC_H_
#define _BRASERO_TRACK_DISC_H_

#include <glib-object.h>

#include <brasero-drive.h>

#include <brasero-track.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_TRACK_DISC             (brasero_track_disc_get_type ())
#define BRASERO_TRACK_DISC(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_TRACK_DISC, BraseroTrackDisc))
#define BRASERO_TRACK_DISC_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_TRACK_DISC, BraseroTrackDiscClass))
#define BRASERO_IS_TRACK_DISC(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_TRACK_DISC))
#define BRASERO_IS_TRACK_DISC_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_TRACK_DISC))
#define BRASERO_TRACK_DISC_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_TRACK_DISC, BraseroTrackDiscClass))

typedef struct _BraseroTrackDiscClass BraseroTrackDiscClass;
typedef struct _BraseroTrackDisc BraseroTrackDisc;

struct _BraseroTrackDiscClass
{
	BraseroTrackClass parent_class;
};

struct _BraseroTrackDisc
{
	BraseroTrack parent_instance;
};

GType brasero_track_disc_get_type (void) G_GNUC_CONST;

BraseroTrackDisc *
brasero_track_disc_new (void);

BraseroBurnResult
brasero_track_disc_set_drive (BraseroTrackDisc *track,
			      BraseroDrive *drive);

BraseroDrive *
brasero_track_disc_get_drive (BraseroTrackDisc *track);

BraseroBurnResult
brasero_track_disc_set_track_num (BraseroTrackDisc *track,
				  guint num);

guint
brasero_track_disc_get_track_num (BraseroTrackDisc *track);

BraseroMedia
brasero_track_disc_get_medium_type (BraseroTrackDisc *track);

G_END_DECLS

#endif /* _BRASERO_TRACK_DISC_H_ */
