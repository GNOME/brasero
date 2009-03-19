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
#include "brasero-utils.h"

#include "brasero-track.h"
#include "brasero-session.h"
#include "brasero-drive.h"
#include "brasero-volume.h"

typedef struct _BraseroSrcSelectionPrivate BraseroSrcSelectionPrivate;
struct _BraseroSrcSelectionPrivate
{
	BraseroBurnSession *session;
	BraseroTrack *track;
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

	if (!priv->session)
		goto chain;

	drive = brasero_medium_get_drive (medium);

	/* NOTE: don't check for drive == NULL to set the session input type */
	if (priv->track
	&&  drive == brasero_burn_session_get_src_drive (priv->session))
		goto chain;

	if (priv->track)
		brasero_track_unref (priv->track);

	priv->track = brasero_track_new (BRASERO_TRACK_TYPE_DISC);
	if (!drive || brasero_drive_is_fake (drive))
		brasero_track_set_drive_source (priv->track, NULL);
	else
		brasero_track_set_drive_source (priv->track, drive);

	brasero_burn_session_add_track (priv->session, priv->track);

chain:

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
brasero_src_selection_init (BraseroSrcSelection *object)
{
	/* only show media with something to be read on them */
	brasero_medium_selection_show_media_type (BRASERO_MEDIUM_SELECTION (object),
						  BRASERO_MEDIA_TYPE_AUDIO|
						  BRASERO_MEDIA_TYPE_DATA);
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
		brasero_track_unref (priv->track);
		priv->track = NULL;
	}

	G_OBJECT_CLASS (brasero_src_selection_parent_class)->finalize (object);
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

		session = g_value_get_object (value);

		priv->session = session;
		g_object_ref (session);

		if (priv->track)
			brasero_track_unref (priv->track);

		medium = brasero_burn_session_get_src_medium (session);
		if (!medium) {
			/* No medium set use set session medium source as the
			 * one currently active in the selection widget */
			medium = brasero_medium_selection_get_active (BRASERO_MEDIUM_SELECTION (object));
			brasero_src_selection_medium_changed (BRASERO_MEDIUM_SELECTION (object), medium);
		}
		else	/* Use the one set in the session */
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

	medium_selection_class->medium_changed = brasero_src_selection_medium_changed;

	g_object_class_install_property (object_class,
					 PROP_SESSION,
					 g_param_spec_object ("session",
							      "The session to work with",
							      "The session to work with",
							      BRASERO_TYPE_BURN_SESSION,
							      G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY));
}
