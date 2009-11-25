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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>

#include "brasero-track-disc.h"

typedef struct _BraseroTrackDiscPrivate BraseroTrackDiscPrivate;
struct _BraseroTrackDiscPrivate
{
	BraseroDrive *drive;

	guint track_num;

	glong src_removed_sig;
	glong src_added_sig;
};

#define BRASERO_TRACK_DISC_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_TRACK_DISC, BraseroTrackDiscPrivate))

G_DEFINE_TYPE (BraseroTrackDisc, brasero_track_disc, BRASERO_TYPE_TRACK);

/**
 * brasero_track_disc_set_track_num:
 * @track: a #BraseroTrackDisc
 * @num: a #guint
 *
 * Sets a track number which can be used
 * to copy only one specific session on a multisession disc
 *
 * Return value: a #BraseroBurnResult.
 * BRASERO_BURN_OK if it was successful,
 * BRASERO_BURN_ERR otherwise.
 **/

BraseroBurnResult
brasero_track_disc_set_track_num (BraseroTrackDisc *track,
				  guint num)
{
	BraseroTrackDiscPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK_DISC (track), BRASERO_BURN_ERR);

	priv = BRASERO_TRACK_DISC_PRIVATE (track);
	priv->track_num = num;

	return BRASERO_BURN_OK;
}

/**
 * brasero_track_disc_get_track_num:
 * @track: a #BraseroTrackDisc
 *
 * Gets the track number which will be used
 * to copy only one specific session on a multisession disc
 *
 * Return value: a #guint. 0 if none is set, any other number otherwise.
 **/

guint
brasero_track_disc_get_track_num (BraseroTrackDisc *track)
{
	BraseroTrackDiscPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK_DISC (track), BRASERO_BURN_ERR);

	priv = BRASERO_TRACK_DISC_PRIVATE (track);
	return priv->track_num;
}

static void
brasero_track_disc_remove_drive (BraseroTrackDisc *track)
{
	BraseroTrackDiscPrivate *priv;

	priv = BRASERO_TRACK_DISC_PRIVATE (track);

	if (priv->src_added_sig) {
		g_signal_handler_disconnect (priv->drive, priv->src_added_sig);
		priv->src_added_sig = 0;
	}

	if (priv->src_removed_sig) {
		g_signal_handler_disconnect (priv->drive, priv->src_removed_sig);
		priv->src_removed_sig = 0;
	}

	if (priv->drive) {
		g_object_unref (priv->drive);
		priv->drive = NULL;
	}
}

static void
brasero_track_disc_medium_changed (BraseroDrive *drive,
				   BraseroMedium *medium,
				   BraseroTrack *track)
{
	brasero_track_changed (track);
}

/**
 * brasero_track_disc_set_drive:
 * @track: a #BraseroTrackDisc
 * @drive: a #BraseroDrive
 *
 * Sets @drive to be the #BraseroDrive that will be used
 * as the source when copying
 *
 * Return value: a #BraseroBurnResult.
 * BRASERO_BURN_OK if it was successful,
 * BRASERO_BURN_ERR otherwise.
 **/

BraseroBurnResult
brasero_track_disc_set_drive (BraseroTrackDisc *track,
			      BraseroDrive *drive)
{
	BraseroTrackDiscPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK_DISC (track), BRASERO_BURN_ERR);

	priv = BRASERO_TRACK_DISC_PRIVATE (track);

	brasero_track_disc_remove_drive (track);
	if (!drive) {
		brasero_track_changed (BRASERO_TRACK (track));
		return BRASERO_BURN_OK;
	}

	priv->drive = drive;
	g_object_ref (drive);

	priv->src_added_sig = g_signal_connect (drive,
						"medium-added",
						G_CALLBACK (brasero_track_disc_medium_changed),
						track);
	priv->src_removed_sig = g_signal_connect (drive,
						  "medium-removed",
						  G_CALLBACK (brasero_track_disc_medium_changed),
						  track);

	brasero_track_changed (BRASERO_TRACK (track));

	return BRASERO_BURN_OK;
}

/**
 * brasero_track_disc_get_drive:
 * @track: a #BraseroTrackDisc
 *
 * Gets the #BraseroDrive object that will be used as
 * the source when copying.
 *
 * Return value: a #BraseroDrive or NULL. Don't unref or free it.
 **/

BraseroDrive *
brasero_track_disc_get_drive (BraseroTrackDisc *track)
{
	BraseroTrackDiscPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK_DISC (track), NULL);

	priv = BRASERO_TRACK_DISC_PRIVATE (track);
	return priv->drive;
}

/**
 * brasero_track_disc_get_medium_type:
 * @track: a #BraseroTrackDisc
 *
 * Gets the #BraseroMedia for the medium that is
 * currently inserted into the drive assigned for @track
 * with brasero_track_disc_set_drive ().
 *
 * Return value: a #BraseroMedia.
 **/

BraseroMedia
brasero_track_disc_get_medium_type (BraseroTrackDisc *track)
{
	BraseroTrackDiscPrivate *priv;
	BraseroMedium *medium;

	g_return_val_if_fail (BRASERO_IS_TRACK_DISC (track), BRASERO_MEDIUM_NONE);

	priv = BRASERO_TRACK_DISC_PRIVATE (track);
	medium = brasero_drive_get_medium (priv->drive);
	if (!medium)
		return BRASERO_MEDIUM_NONE;

	return brasero_medium_get_status (medium);
}

static BraseroBurnResult
brasero_track_disc_get_size (BraseroTrack *track,
			     goffset *blocks,
			     goffset *block_size)
{
	BraseroMedium *medium;
	goffset medium_size = 0;
	goffset medium_blocks = 0;
	BraseroTrackDiscPrivate *priv;

	priv = BRASERO_TRACK_DISC_PRIVATE (track);
	medium = brasero_drive_get_medium (priv->drive);
	if (!medium)
		return BRASERO_BURN_NOT_READY;

	brasero_medium_get_data_size (medium, &medium_size, &medium_blocks);

	if (blocks)
		*blocks = medium_blocks;

	if (block_size)
		*block_size = medium_blocks? (medium_size / medium_blocks):0;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_track_disc_get_track_type (BraseroTrack *track,
				   BraseroTrackType *type)
{
	BraseroTrackDiscPrivate *priv;
	BraseroMedium *medium;

	priv = BRASERO_TRACK_DISC_PRIVATE (track);

	medium = brasero_drive_get_medium (priv->drive);

	brasero_track_type_set_has_medium (type);
	brasero_track_type_set_medium_type (type, brasero_medium_get_status (medium));

	return BRASERO_BURN_OK;
}

static void
brasero_track_disc_init (BraseroTrackDisc *object)
{ }

static void
brasero_track_disc_finalize (GObject *object)
{
	brasero_track_disc_remove_drive (BRASERO_TRACK_DISC (object));

	G_OBJECT_CLASS (brasero_track_disc_parent_class)->finalize (object);
}

static void
brasero_track_disc_class_init (BraseroTrackDiscClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	BraseroTrackClass* track_class = BRASERO_TRACK_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroTrackDiscPrivate));

	object_class->finalize = brasero_track_disc_finalize;

	track_class->get_size = brasero_track_disc_get_size;
	track_class->get_type = brasero_track_disc_get_track_type;
}

/**
 * brasero_track_disc_new:
 *
 * Creates a new #BraseroTrackDisc object.
 *
 * This type of tracks is used to copy media either
 * to a disc image file or to another medium.
 *
 * Return value: a #BraseroTrackDisc.
 **/

BraseroTrackDisc *
brasero_track_disc_new (void)
{
	return g_object_new (BRASERO_TYPE_TRACK_DISC, NULL);
}
