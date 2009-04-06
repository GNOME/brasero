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

#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtk.h>

#include <gconf/gconf-client.h>

#include "burn-basics.h"
#include "brasero-track.h"
#include "brasero-medium.h"
#include "brasero-session.h"
#include "burn-plugin-manager.h"
#include "brasero-drive.h"
#include "brasero-volume.h"
#include "brasero-medium-selection-priv.h"

#include "brasero-dest-selection.h"
#include "brasero-session-cfg.h"

typedef struct _BraseroDestSelectionPrivate BraseroDestSelectionPrivate;
struct _BraseroDestSelectionPrivate
{
	BraseroBurnSession *session;

	BraseroDrive *locked_drive;

	gulong valid_sig;
};

#define BRASERO_DEST_SELECTION_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_DEST_SELECTION, BraseroDestSelectionPrivate))

enum {
	PROP_0,
	PROP_SESSION
};

G_DEFINE_TYPE (BraseroDestSelection, brasero_dest_selection, BRASERO_TYPE_MEDIUM_SELECTION);


void
brasero_dest_selection_lock (BraseroDestSelection *self,
			     gboolean locked)
{
	BraseroDestSelectionPrivate *priv;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

	gtk_widget_set_sensitive (GTK_WIDGET (self), (locked != TRUE));

	gtk_widget_queue_draw (GTK_WIDGET (self));
	if (priv->locked_drive) {
		brasero_drive_unlock (priv->locked_drive);
		g_object_unref (priv->locked_drive);
	}

	if (locked) {
		BraseroMedium *medium;

		medium = brasero_medium_selection_get_active (BRASERO_MEDIUM_SELECTION (self));
		priv->locked_drive = brasero_medium_get_drive (medium);

		if (priv->locked_drive) {
			g_object_ref (priv->locked_drive);
			brasero_drive_lock (priv->locked_drive,
					    _("Ongoing burning process"),
					    NULL);
		}

		if (medium)
			g_object_unref (medium);
	}
}

static void
brasero_dest_selection_valid_session (BraseroSessionCfg *session,
				      BraseroDestSelection *self)
{
	BraseroDestSelectionPrivate *priv;
	BraseroMedium *medium;
	BraseroDrive *burner;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

	/* make sure the current displayed drive reflects that */
	burner = brasero_burn_session_get_burner (priv->session);
	medium = brasero_medium_selection_get_active (BRASERO_MEDIUM_SELECTION (self));
	if (burner != brasero_medium_get_drive (medium))
		brasero_medium_selection_set_active (BRASERO_MEDIUM_SELECTION (self), medium);

	if (medium)
		g_object_unref (medium);

	brasero_medium_selection_update_media_string (BRASERO_MEDIUM_SELECTION (self));
}

static void
brasero_dest_selection_medium_changed (BraseroMediumSelection *selection,
				       BraseroMedium *medium)
{
	BraseroDestSelectionPrivate *priv;

	priv = BRASERO_DEST_SELECTION_PRIVATE (selection);

	if (!priv->session)
		goto chain;

	if (!medium) {
	    	gtk_widget_set_sensitive (GTK_WIDGET (selection), FALSE);
		goto chain;
	}

	if (brasero_medium_get_drive (medium) == brasero_burn_session_get_burner (priv->session))
		goto chain;

	if (priv->locked_drive && priv->locked_drive != brasero_medium_get_drive (medium)) {
		brasero_medium_selection_set_active (selection, medium);
		goto chain;
	}

	brasero_burn_session_set_burner (priv->session, brasero_medium_get_drive (medium));
	gtk_widget_set_sensitive (GTK_WIDGET (selection), (priv->locked_drive == NULL));

chain:

	if (BRASERO_MEDIUM_SELECTION_CLASS (brasero_dest_selection_parent_class)->medium_changed)
		BRASERO_MEDIUM_SELECTION_CLASS (brasero_dest_selection_parent_class)->medium_changed (selection, medium);
}

static void
brasero_dest_selection_init (BraseroDestSelection *object)
{
	BraseroDestSelectionPrivate *priv;

	priv = BRASERO_DEST_SELECTION_PRIVATE (object);

	/* Only show media on which we can write and which are in a burner.
	 * There is one exception though, when we're copying media and when the
	 * burning device is the same as the dest device. */
	brasero_medium_selection_show_media_type (BRASERO_MEDIUM_SELECTION (object),
						  BRASERO_MEDIA_TYPE_WRITABLE);
}

static void
brasero_dest_selection_finalize (GObject *object)
{
	BraseroDestSelectionPrivate *priv;

	priv = BRASERO_DEST_SELECTION_PRIVATE (object);

	if (priv->valid_sig) {
		g_signal_handler_disconnect (priv->session,
					     priv->valid_sig);
		priv->valid_sig = 0;
	}

	if (priv->session) {
		g_object_unref (priv->session);
		priv->session = NULL;
	}

	if (priv->locked_drive) {
		brasero_drive_unlock (priv->locked_drive);
		g_object_unref (priv->locked_drive);
	}

	G_OBJECT_CLASS (brasero_dest_selection_parent_class)->finalize (object);
}

static gboolean
brasero_dest_selection_foreach_medium (BraseroMedium *medium,
				       gpointer callback_data)
{
	BraseroBurnSession *session;
	BraseroDrive *burner;

	session = callback_data;
	burner = brasero_burn_session_get_burner (session);

	if (!burner) {
		brasero_burn_session_set_burner (session, brasero_medium_get_drive (medium));
		return TRUE;
	}

	/* no need to deal with this case */
	if (brasero_drive_get_medium (burner) == medium)
		return TRUE;

	/* The rule is:
	 * - take the biggest
	 * - blank media are our favourite
	 * - try to avoid a medium that is already our source for copying */

	/* NOTE: we could check if medium is bigger */
	if (brasero_burn_session_get_dest_media (session) & BRASERO_MEDIUM_BLANK)
		return TRUE;

	if (brasero_medium_get_status (medium) & BRASERO_MEDIUM_BLANK) {
		brasero_burn_session_set_burner (session, brasero_medium_get_drive (medium));
		return TRUE;
	}
	if (brasero_burn_session_same_src_dest_drive (session)) {
		brasero_burn_session_set_burner (session, brasero_medium_get_drive (medium));
		return TRUE;
	}

	return TRUE;
}

void
brasero_dest_selection_choose_best (BraseroDestSelection *self)
{
	BraseroDestSelectionPrivate *priv;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);
	if (!(brasero_burn_session_get_flags (priv->session) & BRASERO_BURN_FLAG_MERGE)) {
		BraseroDrive *drive;

		/* Select the best fitting media */
		brasero_medium_selection_foreach (BRASERO_MEDIUM_SELECTION (self),
						  brasero_dest_selection_foreach_medium,
						  priv->session);

		drive = brasero_burn_session_get_burner (BRASERO_BURN_SESSION (priv->session));
		if (drive)
			brasero_medium_selection_set_active (BRASERO_MEDIUM_SELECTION (self),
							     brasero_drive_get_medium (drive));
	}
}

static void
brasero_dest_selection_set_property (GObject *object,
				     guint property_id,
				     const GValue *value,
				     GParamSpec *pspec)
{
	BraseroDestSelectionPrivate *priv;
	BraseroBurnSession *session;

	priv = BRASERO_DEST_SELECTION_PRIVATE (object);

	switch (property_id) {
	case PROP_SESSION: /* Readable and only writable at creation time */
		/* NOTE: no need to unref a potential previous session since
		 * it's only set at construct time */
		session = g_value_get_object (value);
		priv->session = session;
		g_object_ref (session);

		if (brasero_burn_session_get_flags (session) & BRASERO_BURN_FLAG_MERGE) {
			BraseroDrive *drive;

			drive = brasero_burn_session_get_burner (session);
			brasero_medium_selection_set_active (BRASERO_MEDIUM_SELECTION (object),
							     brasero_drive_get_medium (drive));
		}
		else {
			BraseroMedium *medium;

			medium = brasero_medium_selection_get_active (BRASERO_MEDIUM_SELECTION (object));
			if (medium) {
				brasero_burn_session_set_burner (session, brasero_medium_get_drive (medium));
				g_object_unref (medium);
			}
		}

		priv->valid_sig = g_signal_connect (session,
						    "is-valid",
						    G_CALLBACK (brasero_dest_selection_valid_session),
						    object);

		brasero_medium_selection_update_media_string (BRASERO_MEDIUM_SELECTION (object));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
brasero_dest_selection_get_property (GObject *object,
				     guint property_id,
				     GValue *value,
				     GParamSpec *pspec)
{
	BraseroDestSelectionPrivate *priv;

	priv = BRASERO_DEST_SELECTION_PRIVATE (object);

	switch (property_id) {
	case PROP_SESSION:
		g_object_ref (priv->session);
		g_value_set_object (value, priv->session);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static gchar *
brasero_dest_selection_get_output_path (BraseroDestSelection *self)
{
	gchar *path = NULL;
	BraseroImageFormat format;
	BraseroDestSelectionPrivate *priv;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

	format = brasero_burn_session_get_output_format (priv->session);
	switch (format) {
	case BRASERO_IMAGE_FORMAT_BIN:
		brasero_burn_session_get_output (priv->session,
						 &path,
						 NULL,
						 NULL);
		break;

	case BRASERO_IMAGE_FORMAT_CLONE:
		brasero_burn_session_get_output (priv->session,
						 NULL,
						 &path,
						 NULL);
		break;

	case BRASERO_IMAGE_FORMAT_CDRDAO:
		brasero_burn_session_get_output (priv->session,
						 NULL,
						 &path,
						 NULL);
		break;

	case BRASERO_IMAGE_FORMAT_CUE:
		brasero_burn_session_get_output (priv->session,
						 NULL,
						 &path,
						 NULL);
		break;

	default:
		break;
	}

	return path;
}

static gchar *
brasero_dest_selection_format_medium_string (BraseroMediumSelection *selection,
					     BraseroMedium *medium)
{
	gchar *label;
	gchar *medium_name;
	gchar *size_string;
	BraseroMedia media;
	gint64 size_bytes = 0;
	BraseroBurnFlag flags;
	BraseroTrackType *input = NULL;
	BraseroDestSelectionPrivate *priv;

	priv = BRASERO_DEST_SELECTION_PRIVATE (selection);

	medium_name = brasero_volume_get_name (BRASERO_VOLUME (medium));
	if (brasero_medium_get_status (medium) & BRASERO_MEDIUM_FILE) {
		gchar *path;

		/* get the set path for the image file */
		path = brasero_dest_selection_get_output_path (BRASERO_DEST_SELECTION (selection));
		if (!path)
			return medium_name;

		/* NOTE for translators: the first %s is medium_name ("File
		 * Image") and the second the path for the image file */
		label = g_strdup_printf (_("%s: \"%s\""),
					 medium_name,
					 path);
		g_free (medium_name);
		g_free (path);
		return label;
	}

	if (!priv->session) {
		g_free (medium_name);
		return NULL;
	}

	input = brasero_track_type_new ();
	brasero_burn_session_get_input_type (priv->session, input);
	if (brasero_track_type_get_has_medium (input)) {
		BraseroMedium *src_medium;

		src_medium = brasero_burn_session_get_src_medium (priv->session);
		if (src_medium == medium) {
			brasero_track_type_free (input);

			/* Translators: this string is only used when the user
			 * wants to copy a disc using the same destination and
			 * source drive. It tells him that brasero will use as
			 * destination disc a new one (once the source has been
			 * copied) which is to be inserted in the drive currently
			 * holding the source disc */
			label = g_strdup_printf (_("New disc in the burner holding source disc"));
			g_free (medium_name);
			return label;
		}
	}

	media = brasero_medium_get_status (medium);
	flags = brasero_burn_session_get_flags (priv->session);

	if ((media & BRASERO_MEDIUM_BLANK)
	|| ((flags & BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE)
	&&  brasero_burn_session_can_blank (priv->session) == BRASERO_BURN_OK)) {
		brasero_medium_get_capacity (medium,
					     &size_bytes,
					     NULL);
	}
	else if (flags & (BRASERO_BURN_FLAG_MERGE|BRASERO_BURN_FLAG_APPEND)) {
		brasero_medium_get_free_space (medium,
					       &size_bytes,
					       NULL);
	}
	else if (media & BRASERO_MEDIUM_CLOSED) {
		if (!brasero_burn_session_can_blank (priv->session) == BRASERO_BURN_OK) {
			brasero_track_type_free (input);

			/* NOTE for translators, the first %s is the medium name */
			label = g_strdup_printf (_("%s: no free space"), medium_name);
			g_free (medium_name);
			return label;
		}

		brasero_medium_get_capacity (medium,
					     &size_bytes,
					     NULL);
	}
	else {
		brasero_medium_get_capacity (medium,
					     &size_bytes,
					     NULL);
	}

	/* format the size */
	if (brasero_track_type_get_has_stream (input)
	|| (brasero_track_type_get_has_medium (input)
	&& (brasero_track_type_get_medium_type (input) & BRASERO_MEDIUM_HAS_AUDIO)))
		size_string = brasero_units_get_time_string (BRASERO_BYTES_TO_DURATION (size_bytes),
							     TRUE,
							     TRUE);
	else
		size_string = g_format_size_for_display (size_bytes);

	brasero_track_type_free (input);

	/* NOTE for translators: the first %s is the medium name, the second %s
	 * is its available free space. "Free" here is the free space available. */
	label = g_strdup_printf (_("%s: %s of free space"), medium_name, size_string);
	g_free (medium_name);
	g_free (size_string);

	return label;
}

static void
brasero_dest_selection_class_init (BraseroDestSelectionClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	BraseroMediumSelectionClass *medium_selection_class = BRASERO_MEDIUM_SELECTION_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroDestSelectionPrivate));

	object_class->finalize = brasero_dest_selection_finalize;
	object_class->set_property = brasero_dest_selection_set_property;
	object_class->get_property = brasero_dest_selection_get_property;

	medium_selection_class->format_medium_string = brasero_dest_selection_format_medium_string;
	medium_selection_class->medium_changed = brasero_dest_selection_medium_changed;
	g_object_class_install_property (object_class,
					 PROP_SESSION,
					 g_param_spec_object ("session",
							      "The session",
							      "The session to work with",
							      BRASERO_TYPE_BURN_SESSION,
							      G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY));
}

GtkWidget *
brasero_dest_selection_new (BraseroBurnSession *session)
{
	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (session), NULL);

	return g_object_new (BRASERO_TYPE_DEST_SELECTION,
			     "session", session,
			     NULL);
}
