/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2007-2008 <bonfire-app@wanadoo.fr>
 * 
 * brasero is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * brasero is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with brasero.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib/gi18n-lib.h>

#include "brasero-src-selection.h"
#include "brasero-drive-selection.h"
#include "burn-track.h"
#include "burn-session.h"
#include "burn-drive.h"

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

static BraseroDriveSelectionClass* parent_class = NULL;

G_DEFINE_TYPE (BraseroSrcSelection, brasero_src_selection, BRASERO_TYPE_DRIVE_SELECTION);

static void
brasero_src_selection_drive_changed (BraseroDriveSelection *selection,
				     BraseroDrive *drive)
{
	BraseroSrcSelectionPrivate *priv;

	priv = BRASERO_SRC_SELECTION_PRIVATE (selection);

	if (!priv->session)
		return;

	if (priv->track)
		brasero_track_unref (priv->track);

	priv->track = brasero_track_new (BRASERO_TRACK_TYPE_DISC);
	if (drive && brasero_drive_is_fake (drive))
		brasero_track_set_drive_source (priv->track, NULL);
	else
		brasero_track_set_drive_source (priv->track, drive);

	brasero_burn_session_add_track (priv->session, priv->track);
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
	brasero_drive_selection_set_tooltip (BRASERO_DRIVE_SELECTION (object),
					     _("Choose the disc to read from"));

	/* only show media with something to be read on them */
	brasero_drive_selection_set_type_shown (BRASERO_DRIVE_SELECTION (object),
						BRASERO_MEDIA_TYPE_READABLE);
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

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_src_selection_set_property (GObject *object,
				    guint property_id,
				    const GValue *value,
				    GParamSpec *pspec)
{
	BraseroSrcSelectionPrivate *priv;
	BraseroBurnSession *session;
	BraseroDrive *drive;

	priv = BRASERO_SRC_SELECTION_PRIVATE (object);

	switch (property_id) {
	case PROP_SESSION:
		session = g_value_get_object (value);

		priv->session = session;
		g_object_ref (session);

		if (priv->track)
			brasero_track_unref (priv->track);

		drive = brasero_drive_selection_get_drive (BRASERO_DRIVE_SELECTION (object));
		if (drive) {
			brasero_src_selection_drive_changed (BRASERO_DRIVE_SELECTION (object), drive);
			g_object_unref (drive);
		}

		break;

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
	BraseroDriveSelectionClass *select_class = BRASERO_DRIVE_SELECTION_CLASS (klass);

	parent_class = BRASERO_DRIVE_SELECTION_CLASS (g_type_class_peek_parent (klass));

	g_type_class_add_private (klass, sizeof (BraseroSrcSelectionPrivate));

	object_class->finalize = brasero_src_selection_finalize;
	object_class->set_property = brasero_src_selection_set_property;
	object_class->get_property = brasero_src_selection_get_property;

	select_class->drive_changed = brasero_src_selection_drive_changed;

	g_object_class_install_property (object_class,
					 PROP_SESSION,
					 g_param_spec_object ("session",
							      "The session to work with",
							      "The session to work with",
							      BRASERO_TYPE_BURN_SESSION,
							      G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY));
}
