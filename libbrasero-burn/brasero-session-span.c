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

#include "brasero-drive.h"
#include "brasero-medium.h"

#include "burn-debug.h"
#include "brasero-session-helper.h"
#include "brasero-track.h"
#include "brasero-track-data.h"
#include "brasero-track-data-cfg.h"
#include "brasero-session-span.h"

typedef struct _BraseroSessionSpanPrivate BraseroSessionSpanPrivate;
struct _BraseroSessionSpanPrivate
{
	GSList * track_list;
	BraseroTrack * last_track;
};

#define BRASERO_SESSION_SPAN_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_SESSION_SPAN, BraseroSessionSpanPrivate))

G_DEFINE_TYPE (BraseroSessionSpan, brasero_session_span, BRASERO_TYPE_BURN_SESSION);

/**
 * brasero_session_span_get_max_space:
 * @session: a #BraseroSessionSpan
 *
 * Returns the maximum required space (in sectors) 
 * among all the possible spanned batches.
 * This means that when burningto a media
 * it will also be the minimum required
 * space to burn all the contents in several
 * batches.
 *
 * Return value: a #goffset.
 **/

goffset
brasero_session_span_get_max_space (BraseroSessionSpan *session)
{
	GSList *tracks;
	goffset max_sectors = 0;
	BraseroSessionSpanPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_SESSION_SPAN (session), 0);

	priv = BRASERO_SESSION_SPAN_PRIVATE (session);

	if (priv->last_track) {
		tracks = g_slist_find (priv->track_list, priv->last_track);

		if (!tracks->next)
			return 0;

		tracks = tracks->next;
	}
	else if (priv->track_list)
		tracks = priv->track_list;
	else
		tracks = brasero_burn_session_get_tracks (BRASERO_BURN_SESSION (session));

	for (; tracks; tracks = tracks->next) {
		BraseroTrack *track;
		goffset track_blocks = 0;

		track = tracks->data;

		if (BRASERO_IS_TRACK_DATA_CFG (track))
			return brasero_track_data_cfg_span_max_space (BRASERO_TRACK_DATA_CFG (track));

		/* This is the common case */
		brasero_track_get_size (BRASERO_TRACK (track),
					&track_blocks,
					NULL);

		max_sectors = MAX (max_sectors, track_blocks);
	}

	return max_sectors;
}

/**
 * brasero_session_span_again:
 * @session: a #BraseroSessionSpan
 *
 * Checks whether some data were not included during calls to brasero_session_span_next ().
 *
 * Return value: a #BraseroBurnResult. BRASERO_BURN_OK if there is not anymore data.
 * BRASERO_BURN_RETRY if the operation was successful and a new #BraseroTrackDataCfg was created.
 * BRASERO_BURN_ERR otherwise.
 **/

BraseroBurnResult
brasero_session_span_again (BraseroSessionSpan *session)
{
	GSList *tracks;
	BraseroTrack *track;
	BraseroSessionSpanPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_SESSION_SPAN (session), BRASERO_BURN_ERR);

	priv = BRASERO_SESSION_SPAN_PRIVATE (session);

	/* This is not an error */
	if (!priv->track_list)
		return BRASERO_BURN_OK;

	if (priv->last_track) {
		tracks = g_slist_find (priv->track_list, priv->last_track);
		if (!tracks->next) {
			priv->track_list = NULL;
			return BRASERO_BURN_OK;
		}

		return BRASERO_BURN_RETRY;
	}

	tracks = priv->track_list;
	track = tracks->data;

	if (BRASERO_IS_TRACK_DATA_CFG (track))
		return brasero_track_data_cfg_span_again (BRASERO_TRACK_DATA_CFG (track));

	return (tracks != NULL)? BRASERO_BURN_RETRY:BRASERO_BURN_OK;
}

/**
 * brasero_session_span_possible:
 * @session: a #BraseroSessionSpan
 *
 * Checks if a new #BraseroTrackData can be created from the files remaining in the tree 
 * after calls to brasero_session_span_next (). The maximum size of the data will be the one
 * of the medium inserted in the #BraseroDrive set for @session (see brasero_burn_session_set_burner ()).
 *
 * Return value: a #BraseroBurnResult. BRASERO_BURN_OK if there is not anymore data.
 * BRASERO_BURN_RETRY if the operation was successful and a new #BraseroTrackDataCfg was created.
 * BRASERO_BURN_ERR otherwise.
 **/

BraseroBurnResult
brasero_session_span_possible (BraseroSessionSpan *session)
{
	GSList *tracks;
	BraseroTrack *track;
	goffset max_sectors = 0;
	goffset track_blocks = 0;
	BraseroSessionSpanPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_SESSION_SPAN (session), BRASERO_BURN_ERR);

	priv = BRASERO_SESSION_SPAN_PRIVATE (session);

	max_sectors = brasero_burn_session_get_available_medium_space (BRASERO_BURN_SESSION (session));
	if (max_sectors <= 0)
		return BRASERO_BURN_ERR;

	if (!priv->track_list)
		tracks = brasero_burn_session_get_tracks (BRASERO_BURN_SESSION (session));
	else if (priv->last_track) {
		tracks = g_slist_find (priv->track_list, priv->last_track);
		if (!tracks->next) {
			priv->track_list = NULL;
			return BRASERO_BURN_OK;
		}
		tracks = tracks->next;
	}
	else
		tracks = priv->track_list;

	if (!tracks)
		return BRASERO_BURN_ERR;

	track = tracks->data;

	if (BRASERO_IS_TRACK_DATA_CFG (track))
		return brasero_track_data_cfg_span_possible (BRASERO_TRACK_DATA_CFG (track), max_sectors);

	/* This is the common case */
	brasero_track_get_size (BRASERO_TRACK (track),
				&track_blocks,
				NULL);

	if (track_blocks >= max_sectors)
		return BRASERO_BURN_ERR;

	return BRASERO_BURN_RETRY;
}

/**
 * brasero_session_span_start:
 * @session: a #BraseroSessionSpan
 *
 * Get the object ready for spanning a #BraseroBurnSession object. This function
 * must be called before brasero_session_span_next ().
 *
 * Return value: a #BraseroBurnResult. BRASERO_BURN_OK if successful.
 **/

BraseroBurnResult
brasero_session_span_start (BraseroSessionSpan *session)
{
	BraseroSessionSpanPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_SESSION_SPAN (session), BRASERO_BURN_ERR);

	priv = BRASERO_SESSION_SPAN_PRIVATE (session);

	priv->track_list = brasero_burn_session_get_tracks (BRASERO_BURN_SESSION (session));
	if (priv->last_track) {
		g_object_unref (priv->last_track);
		priv->last_track = NULL;
	}

	return BRASERO_BURN_OK;
}

/**
 * brasero_session_span_next:
 * @session: a #BraseroSessionSpan
 *
 * Sets the next batch of data to be burnt onto the medium inserted in the #BraseroDrive
 * set for @session (see brasero_burn_session_set_burner ()). Its free space or it capacity
 * will be used as the maximum amount of data to be burnt.
 *
 * Return value: a #BraseroBurnResult. BRASERO_BURN_OK if successful.
 **/

BraseroBurnResult
brasero_session_span_next (BraseroSessionSpan *session)
{
	GSList *tracks;
	gboolean pushed = FALSE;
	goffset max_sectors = 0;
	goffset total_sectors = 0;
	BraseroSessionSpanPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_SESSION_SPAN (session), BRASERO_BURN_ERR);

	priv = BRASERO_SESSION_SPAN_PRIVATE (session);

	g_return_val_if_fail (priv->track_list != NULL, BRASERO_BURN_ERR);

	max_sectors = brasero_burn_session_get_available_medium_space (BRASERO_BURN_SESSION (session));
	if (max_sectors <= 0)
		return BRASERO_BURN_ERR;

	/* NOTE: should we pop here? */
	if (priv->last_track) {
		tracks = g_slist_find (priv->track_list, priv->last_track);
		g_object_unref (priv->last_track);
		priv->last_track = NULL;

		if (!tracks->next) {
			priv->track_list = NULL;
			return BRASERO_BURN_OK;
		}
		tracks = tracks->next;
	}
	else
		tracks = priv->track_list;

	for (; tracks; tracks = tracks->next) {
		BraseroTrack *track;
		goffset track_blocks = 0;

		track = tracks->data;

		if (BRASERO_IS_TRACK_DATA_CFG (track)) {
			BraseroTrackData *new_track;
			BraseroBurnResult result;

			/* NOTE: the case where track_blocks < max_blocks will
			 * be handled by brasero_track_data_cfg_span () */

			/* This track type is the only one to be able to span itself */
			new_track = brasero_track_data_new ();
			result = brasero_track_data_cfg_span (BRASERO_TRACK_DATA_CFG (track),
							      max_sectors,
							      new_track);
			if (result != BRASERO_BURN_RETRY) {
				g_object_unref (new_track);
				return result;
			}

			pushed = TRUE;
			brasero_burn_session_push_tracks (BRASERO_BURN_SESSION (session));
			brasero_burn_session_add_track (BRASERO_BURN_SESSION (session),
							BRASERO_TRACK (new_track),
							NULL);
			break;
		}

		/* This is the common case */
		brasero_track_get_size (BRASERO_TRACK (track),
					&track_blocks,
					NULL);

		/* NOTE: keep the order of tracks */
		if (track_blocks + total_sectors >= max_sectors) {
			BRASERO_BURN_LOG ("Reached end of spanned size");
			break;
		}

		total_sectors += track_blocks;

		if (!pushed) {
			BRASERO_BURN_LOG ("Pushing tracks for media spanning");
			brasero_burn_session_push_tracks (BRASERO_BURN_SESSION (session));
			pushed = TRUE;
		}

		BRASERO_BURN_LOG ("Adding tracks");
		brasero_burn_session_add_track (BRASERO_BURN_SESSION (session), track, NULL);

		if (priv->last_track)
			g_object_unref (priv->last_track);

		priv->last_track = g_object_ref (track);
	}

	/* If we pushed anything it means we succeeded */
	return (pushed? BRASERO_BURN_RETRY:BRASERO_BURN_ERR);
}

/**
 * brasero_session_span_stop:
 * @session: a #BraseroSessionSpan
 *
 * Ends and cleans a spanning operation started with brasero_session_span_start ().
 *
 **/

void
brasero_session_span_stop (BraseroSessionSpan *session)
{
	BraseroSessionSpanPrivate *priv;

	g_return_if_fail (BRASERO_IS_SESSION_SPAN (session));

	priv = BRASERO_SESSION_SPAN_PRIVATE (session);

	if (priv->last_track) {
		g_object_unref (priv->last_track);
		priv->last_track = NULL;
	}
	else if (priv->track_list) {
		BraseroTrack *track;

		track = priv->track_list->data;
		if (BRASERO_IS_TRACK_DATA_CFG (track))
			brasero_track_data_cfg_span_stop (BRASERO_TRACK_DATA_CFG (track));
	}

	priv->track_list = NULL;
}

static void
brasero_session_span_init (BraseroSessionSpan *object)
{ }

static void
brasero_session_span_finalize (GObject *object)
{
	brasero_session_span_stop (BRASERO_SESSION_SPAN (object));
	G_OBJECT_CLASS (brasero_session_span_parent_class)->finalize (object);
}

static void
brasero_session_span_class_init (BraseroSessionSpanClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroSessionSpanPrivate));

	object_class->finalize = brasero_session_span_finalize;
}

/**
 * brasero_session_span_new:
 *
 * Creates a new #BraseroSessionSpan object.
 *
 * Return value: a #BraseroSessionSpan object
 **/

BraseroSessionSpan *
brasero_session_span_new (void)
{
	return g_object_new (BRASERO_TYPE_SESSION_SPAN, NULL);
}

