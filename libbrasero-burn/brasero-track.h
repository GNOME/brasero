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

#ifndef _BURN_TRACK_H
#define _BURN_TRACK_H

#include <glib.h>
#include <glib-object.h>

#include <brasero-drive.h>
#include <brasero-medium.h>

#include <brasero-enums.h>
#include <brasero-error.h>
#include <brasero-status.h>

#include <brasero-track-type.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_TRACK             (brasero_track_get_type ())
#define BRASERO_TRACK(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_TRACK, BraseroTrack))
#define BRASERO_TRACK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_TRACK, BraseroTrackClass))
#define BRASERO_IS_TRACK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_TRACK))
#define BRASERO_IS_TRACK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_TRACK))
#define BRASERO_TRACK_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_TRACK, BraseroTrackClass))

typedef struct _BraseroTrackClass BraseroTrackClass;
typedef struct _BraseroTrack BraseroTrack;

struct _BraseroTrackClass
{
	GObjectClass parent_class;

	/* Virtual functions */
	BraseroBurnResult	(* get_status)		(BraseroTrack *track,
							 BraseroStatus *status);

	BraseroBurnResult	(* get_size)		(BraseroTrack *track,
							 goffset *blocks,
							 goffset *block_size);

	BraseroBurnResult	(* get_type)		(BraseroTrack *track,
							 BraseroTrackType *type);

	/* Signals */
	void			(* changed)		(BraseroTrack *track);
};

struct _BraseroTrack
{
	GObject parent_instance;
};

GType brasero_track_get_type (void) G_GNUC_CONST;

void
brasero_track_changed (BraseroTrack *track);



BraseroBurnResult
brasero_track_get_size (BraseroTrack *track,
			goffset *blocks,
			goffset *bytes);

BraseroBurnResult
brasero_track_get_track_type (BraseroTrack *track,
			      BraseroTrackType *type);

BraseroBurnResult
brasero_track_get_status (BraseroTrack *track,
			  BraseroStatus *status);


/** 
 * Checksums
 */

typedef enum {
	BRASERO_CHECKSUM_NONE			= 0,
	BRASERO_CHECKSUM_DETECT			= 1,		/* means the plugin handles detection of checksum type */
	BRASERO_CHECKSUM_MD5			= 1 << 1,
	BRASERO_CHECKSUM_MD5_FILE		= 1 << 2,
	BRASERO_CHECKSUM_SHA1			= 1 << 3,
	BRASERO_CHECKSUM_SHA1_FILE		= 1 << 4,
	BRASERO_CHECKSUM_SHA256			= 1 << 5,
	BRASERO_CHECKSUM_SHA256_FILE		= 1 << 6,
} BraseroChecksumType;

BraseroBurnResult
brasero_track_set_checksum (BraseroTrack *track,
			    BraseroChecksumType type,
			    const gchar *checksum);

const gchar *
brasero_track_get_checksum (BraseroTrack *track);

BraseroChecksumType
brasero_track_get_checksum_type (BraseroTrack *track);

BraseroBurnResult
brasero_track_tag_add (BraseroTrack *track,
		       const gchar *tag,
		       GValue *value);

BraseroBurnResult
brasero_track_tag_lookup (BraseroTrack *track,
			  const gchar *tag,
			  GValue **value);

void
brasero_track_tag_copy_missing (BraseroTrack *dest,
				BraseroTrack *src);

/**
 * Convenience functions for tags
 */

BraseroBurnResult
brasero_track_tag_add_string (BraseroTrack *track,
			      const gchar *tag,
			      const gchar *string);

const gchar *
brasero_track_tag_lookup_string (BraseroTrack *track,
				 const gchar *tag);

BraseroBurnResult
brasero_track_tag_add_int (BraseroTrack *track,
			   const gchar *tag,
			   int value);

int
brasero_track_tag_lookup_int (BraseroTrack *track,
			      const gchar *tag);

G_END_DECLS

#endif /* _BURN_TRACK_H */

 
