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

#include <glib/gi18n-lib.h>

#include <gtk/gtk.h>

#include "brasero-src-selection.h"
#include "brasero-medium-selection.h"

#include "brasero-track.h"
#include "brasero-session.h"
#include "brasero-track-disc.h"

#include "brasero-drive.h"
#include "brasero-volume.h"

typedef struct _BraseroSrcSelectionPrivate BraseroSrcSelectionPrivate;
struct _BraseroSrcSelectionPrivate
{
	BraseroBurnSession *session;
	BraseroTrackDisc *track;
};

#define BRASERO_SRC_SELECTION_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_SRC_SELECTION, BraseroSrcSelectionPrivate))

enum {
	PROP_0,
	PROP_SESSION
};

G_DEFINE_TYPE (BraseroSrcSelection, brasero_src_selection, BRASERO_TYPE_MEDIUM_SELECTION);

static void
brasero_src_selection_medium_changed (BraseroMediumSelection *selection,
				      BraseroMedium *medium)
{
	BraseroSrcSelectionPrivate *priv;
	BraseroDrive *drive = NULL;

	priv = BRASERO_SRC_SELECTION_PRIVATE (selection);

	if (priv->session && priv->track) {
		drive = brasero_medium_get_drive (medium);
		brasero_track_disc_set_drive (priv->track, drive);
	}

	gtk_widget_set_sensitive (GTK_WIDGET (selection), drive != NULL);

	if (BRASERO_MEDIUM_SELECTION_CLASS (brasero_src_selection_parent_class)->medium_changed)
		BRASERO_MEDIUM_SELECTION_CLASS (brasero_src_selection_parent_class)->medium_changed (selection, medium);
}

GtkWidget *
brasero_src_selection_new (BraseroBurnSession *session)
{
	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (session), NULL);
	return GTK_WIDGET (g_object_new (BRASERO_TYPE_SRC_SELECTION,
					 "session", session,
					 NULL));
}

static void
brasero_src_selection_constructed (GObject *object)
{
	G_OBJECT_CLASS (brasero_src_selection_parent_class)->constructed (object);

	/* only show media with something to be read on them */
	brasero_medium_selection_show_media_type (BRASERO_MEDIUM_SELECTION (object),
						  BRASERO_MEDIA_TYPE_AUDIO|
						  BRASERO_MEDIA_TYPE_DATA);
}

static void
brasero_src_selection_init (BraseroSrcSelection *object)
{
}

static void
brasero_src_selection_finalize (GObject *object)
{
	BraseroSrcSelectionPrivate *priv;

	priv = BRASERO_SRC_SELECTION_PRIVATE (object);

	if (priv->session) {
		g_object_unref (priv->session);
		priv->session = NULL;
	}

	if (priv->track) {
		g_object_unref (priv->track);
		priv->track = NULL;
	}

	G_OBJECT_CLASS (brasero_src_selection_parent_class)->finalize (object);
}

static BraseroTrack *
_get_session_disc_track (BraseroBurnSession *session)
{
	BraseroTrack *track;
	GSList *tracks;
	guint num;

	tracks = brasero_burn_session_get_tracks (session);
	num = g_slist_length (tracks);

	if (num != 1)
		return NULL;

	track = tracks->data;
	if (BRASERO_IS_TRACK_DISC (track))
		return track;

	return NULL;
}

static void
brasero_src_selection_set_property (GObject *object,
				    guint property_id,
				    const GValue *value,
				    GParamSpec *pspec)
{
	BraseroSrcSelectionPrivate *priv;
	BraseroBurnSession *session;

	priv = BRASERO_SRC_SELECTION_PRIVATE (object);

	switch (property_id) {
	case PROP_SESSION:
	{
		BraseroMedium *medium;
		BraseroDrive *drive;
		BraseroTrack *track;

		session = g_value_get_object (value);

		priv->session = session;
		g_object_ref (session);

		if (priv->track)
			g_object_unref (priv->track);

		/* See if there was a track set; if so then use it */
		track = _get_session_disc_track (session);
		if (track) {
			priv->track = BRASERO_TRACK_DISC (track);
			g_object_ref (track);
		}
		else {
			priv->track = brasero_track_disc_new ();
			brasero_burn_session_add_track (priv->session,
							BRASERO_TRACK (priv->track),
							NULL);
		}

		drive = brasero_track_disc_get_drive (priv->track);
		medium = brasero_drive_get_medium (drive);
		if (!medium) {
			/* No medium set use set session medium source as the
			 * one currently active in the selection widget */
			medium = brasero_medium_selection_get_active (BRASERO_MEDIUM_SELECTION (object));
			brasero_src_selection_medium_changed (BRASERO_MEDIUM_SELECTION (object), medium);
		}
		else
			brasero_medium_selection_set_active (BRASERO_MEDIUM_SELECTION (object), medium);

		break;
	}

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
brasero_src_selection_get_property (GObject *object,
				    guint property_id,
				    GValue *value,
				    GParamSpec *pspec)
{
	BraseroSrcSelectionPrivate *priv;

	priv = BRASERO_SRC_SELECTION_PRIVATE (object);

	switch (property_id) {
	case PROP_SESSION:
		g_value_set_object (value, priv->session);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
brasero_src_selection_class_init (BraseroSrcSelectionClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	BraseroMediumSelectionClass *medium_selection_class = BRASERO_MEDIUM_SELECTION_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroSrcSelectionPrivate));

	object_class->finalize = brasero_src_selection_finalize;
	object_class->set_property = brasero_src_selection_set_property;
	object_class->get_property = brasero_src_selection_get_property;
	object_class->constructed = brasero_src_selection_constructed;

	medium_selection_class->medium_changed = brasero_src_selection_medium_changed;

	g_object_class_install_property (object_class,
					 PROP_SESSION,
					 g_param_spec_object ("session",
							      "The session to work with",
							      "The session to work with",
							      BRASERO_TYPE_BURN_SESSION,
							      G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY));
}
