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

#include "burn-basics.h"
#include "burn-plugin-manager.h"
#include "brasero-medium-selection-priv.h"
#include "brasero-session-helper.h"

#include "brasero-dest-selection.h"

#include "brasero-drive.h"
#include "brasero-medium.h"
#include "brasero-volume.h"

#include "brasero-burn-lib.h"
#include "brasero-tags.h"
#include "brasero-track.h"
#include "brasero-session.h"
#include "brasero-session-cfg.h"

typedef struct _BraseroDestSelectionPrivate BraseroDestSelectionPrivate;
struct _BraseroDestSelectionPrivate
{
	BraseroBurnSession *session;

	BraseroDrive *locked_drive;

	guint user_changed:1;
};

#define BRASERO_DEST_SELECTION_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_DEST_SELECTION, BraseroDestSelectionPrivate))

enum {
	PROP_0,
	PROP_SESSION
};

G_DEFINE_TYPE (BraseroDestSelection, brasero_dest_selection, BRASERO_TYPE_MEDIUM_SELECTION);

static void
brasero_dest_selection_lock (BraseroDestSelection *self,
			     gboolean locked)
{
	BraseroDestSelectionPrivate *priv;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

	if (locked == (priv->locked_drive != NULL))
		return;

	gtk_widget_set_sensitive (GTK_WIDGET (self), (locked != TRUE));
	gtk_widget_queue_draw (GTK_WIDGET (self));

	if (priv->locked_drive) {
		brasero_drive_unlock (priv->locked_drive);
		g_object_unref (priv->locked_drive);
		priv->locked_drive = NULL;
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
	brasero_medium_selection_update_media_string (BRASERO_MEDIUM_SELECTION (self));
}

static void
brasero_dest_selection_output_changed (BraseroSessionCfg *session,
				       BraseroMedium *former,
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
		brasero_medium_selection_set_active (BRASERO_MEDIUM_SELECTION (self),
						     brasero_drive_get_medium (burner));

	if (medium)
		g_object_unref (medium);
}

static void
brasero_dest_selection_flags_changed (BraseroBurnSession *session,
                                      GParamSpec *pspec,
				      BraseroDestSelection *self)
{
	BraseroDestSelectionPrivate *priv;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

	brasero_dest_selection_lock (self, (brasero_burn_session_get_flags (BRASERO_BURN_SESSION (priv->session)) & BRASERO_BURN_FLAG_MERGE) != 0);
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
brasero_dest_selection_user_change (BraseroDestSelection *selection,
                                    GParamSpec *pspec,
                                    gpointer NULL_data)
{
	gboolean shown = FALSE;
	BraseroDestSelectionPrivate *priv;

	/* we are only interested when the menu is shown */
	g_object_get (selection,
	              "popup-shown", &shown,
	              NULL);

	if (!shown)
		return;

	priv = BRASERO_DEST_SELECTION_PRIVATE (selection);
	priv->user_changed = TRUE;
}

static void
brasero_dest_selection_medium_removed (GtkTreeModel *model,
                                       GtkTreePath *path,
                                       gpointer user_data)
{
	BraseroDestSelectionPrivate *priv;

	priv = BRASERO_DEST_SELECTION_PRIVATE (user_data);
	if (priv->user_changed)
		return;

	if (gtk_combo_box_get_active (GTK_COMBO_BOX (user_data)) == -1)
		brasero_dest_selection_choose_best (BRASERO_DEST_SELECTION (user_data));
}

static void
brasero_dest_selection_medium_added (GtkTreeModel *model,
                                     GtkTreePath *path,
                                     GtkTreeIter *iter,
                                     gpointer user_data)
{
	BraseroDestSelectionPrivate *priv;

	priv = BRASERO_DEST_SELECTION_PRIVATE (user_data);
	if (priv->user_changed)
		return;

	brasero_dest_selection_choose_best (BRASERO_DEST_SELECTION (user_data));
}

static void
brasero_dest_selection_constructed (GObject *object)
{
	G_OBJECT_CLASS (brasero_dest_selection_parent_class)->constructed (object);

	/* Only show media on which we can write and which are in a burner.
	 * There is one exception though, when we're copying media and when the
	 * burning device is the same as the dest device. */
	brasero_medium_selection_show_media_type (BRASERO_MEDIUM_SELECTION (object),
						  BRASERO_MEDIA_TYPE_WRITABLE|
						  BRASERO_MEDIA_TYPE_FILE);
}

static void
brasero_dest_selection_init (BraseroDestSelection *object)
{
	GtkTreeModel *model;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (object));
	g_signal_connect (model,
	                  "row-inserted",
	                  G_CALLBACK (brasero_dest_selection_medium_added),
	                  object);
	g_signal_connect (model,
	                  "row-deleted",
	                  G_CALLBACK (brasero_dest_selection_medium_removed),
	                  object);

	/* This is to know when the user changed it on purpose */
	g_signal_connect (object,
	                  "notify::popup-shown",
	                  G_CALLBACK (brasero_dest_selection_user_change),
	                  NULL);
}

static void
brasero_dest_selection_clean (BraseroDestSelection *self)
{
	BraseroDestSelectionPrivate *priv;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

	if (priv->session) {
		g_signal_handlers_disconnect_by_func (priv->session,
						      brasero_dest_selection_valid_session,
						      self);
		g_signal_handlers_disconnect_by_func (priv->session,
						      brasero_dest_selection_output_changed,
						      self);
		g_signal_handlers_disconnect_by_func (priv->session,
						      brasero_dest_selection_flags_changed,
						      self);

		g_object_unref (priv->session);
		priv->session = NULL;
	}

	if (priv->locked_drive) {
		brasero_drive_unlock (priv->locked_drive);
		g_object_unref (priv->locked_drive);
		priv->locked_drive = NULL;
	}
}

static void
brasero_dest_selection_finalize (GObject *object)
{
	brasero_dest_selection_clean (BRASERO_DEST_SELECTION (object));
	G_OBJECT_CLASS (brasero_dest_selection_parent_class)->finalize (object);
}

static goffset
_get_medium_free_space (BraseroMedium *medium,
                        goffset session_blocks)
{
	BraseroMedia media;
	goffset blocks = 0;

	media = brasero_medium_get_status (medium);
	media = brasero_burn_library_get_media_capabilities (media);

	/* NOTE: we always try to blank a medium when we can */
	brasero_medium_get_free_space (medium,
				       NULL,
				       &blocks);

	if ((media & BRASERO_MEDIUM_REWRITABLE)
	&& blocks < session_blocks)
		brasero_medium_get_capacity (medium,
		                             NULL,
		                             &blocks);

	return blocks;
}

static gboolean
brasero_dest_selection_foreach_medium (BraseroMedium *medium,
				       gpointer callback_data)
{
	BraseroBurnSession *session;
	goffset session_blocks = 0;
	goffset burner_blocks = 0;
	goffset medium_blocks;
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
	 * - blank media are our favourite since it avoids hiding/blanking data
	 * - take the medium that is closest to the size we need to burn
	 * - try to avoid a medium that is already our source for copying */
	/* NOTE: we could check if medium is bigger */
	if ((brasero_burn_session_get_dest_media (session) & BRASERO_MEDIUM_BLANK)
	&&  (brasero_medium_get_status (medium) & BRASERO_MEDIUM_BLANK))
		goto choose_closest_size;

	if (brasero_burn_session_get_dest_media (session) & BRASERO_MEDIUM_BLANK)
		return TRUE;

	if (brasero_medium_get_status (medium) & BRASERO_MEDIUM_BLANK) {
		brasero_burn_session_set_burner (session, brasero_medium_get_drive (medium));
		return TRUE;
	}

	/* In case it is the same source/same destination, choose it this new
	 * medium except if the medium is a file. */
	if (brasero_burn_session_same_src_dest_drive (session)
	&& (brasero_medium_get_status (medium) & BRASERO_MEDIUM_FILE) == 0) {
		brasero_burn_session_set_burner (session, brasero_medium_get_drive (medium));
		return TRUE;
	}

	/* Any possible medium is better than file even if it means copying to
	 * the same drive with a new medium later. */
	if (brasero_drive_is_fake (burner)
	&& (brasero_medium_get_status (medium) & BRASERO_MEDIUM_FILE) == 0) {
		brasero_burn_session_set_burner (session, brasero_medium_get_drive (medium));
		return TRUE;
	}


choose_closest_size:

	brasero_burn_session_get_size (session, &session_blocks, NULL);
	medium_blocks = _get_medium_free_space (medium, session_blocks);

	if (medium_blocks - session_blocks <= 0)
		return TRUE;

	burner_blocks = _get_medium_free_space (brasero_drive_get_medium (burner), session_blocks);
	if (burner_blocks - session_blocks <= 0)
		brasero_burn_session_set_burner (session, brasero_medium_get_drive (medium));
	else if (burner_blocks - session_blocks > medium_blocks - session_blocks)
		brasero_burn_session_set_burner (session, brasero_medium_get_drive (medium));

	return TRUE;
}

void
brasero_dest_selection_choose_best (BraseroDestSelection *self)
{
	BraseroDestSelectionPrivate *priv;

	priv = BRASERO_DEST_SELECTION_PRIVATE (self);

	priv->user_changed = FALSE;
	if (!priv->session)
		return;

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

void
brasero_dest_selection_set_session (BraseroDestSelection *selection,
				    BraseroBurnSession *session)
{
	BraseroDestSelectionPrivate *priv;

	priv = BRASERO_DEST_SELECTION_PRIVATE (selection);

	if (priv->session)
		brasero_dest_selection_clean (selection);

	if (!session)
		return;

	priv->session = g_object_ref (session);
	if (brasero_burn_session_get_flags (session) & BRASERO_BURN_FLAG_MERGE) {
		BraseroDrive *drive;

		/* Prevent automatic resetting since a drive was set */
		priv->user_changed = TRUE;

		drive = brasero_burn_session_get_burner (session);
		brasero_medium_selection_set_active (BRASERO_MEDIUM_SELECTION (selection),
						     brasero_drive_get_medium (drive));
	}
	else {
		BraseroDrive *burner;

		/* Only try to set a better drive if there isn't one already set */
		burner = brasero_burn_session_get_burner (BRASERO_BURN_SESSION (priv->session));
		if (burner) {
			BraseroMedium *medium;

			/* Prevent automatic resetting since a drive was set */
			priv->user_changed = TRUE;

			medium = brasero_drive_get_medium (burner);
			brasero_medium_selection_set_active (BRASERO_MEDIUM_SELECTION (selection), medium);
		}
		else
			brasero_dest_selection_choose_best (BRASERO_DEST_SELECTION (selection));
	}

	g_signal_connect (session,
			  "is-valid",
			  G_CALLBACK (brasero_dest_selection_valid_session),
			  selection);
	g_signal_connect (session,
			  "output-changed",
			  G_CALLBACK (brasero_dest_selection_output_changed),
			  selection);
	g_signal_connect (session,
			  "notify::flags",
			  G_CALLBACK (brasero_dest_selection_flags_changed),
			  selection);

	brasero_medium_selection_update_media_string (BRASERO_MEDIUM_SELECTION (selection));
}

static void
brasero_dest_selection_set_property (GObject *object,
				     guint property_id,
				     const GValue *value,
				     GParamSpec *pspec)
{
	BraseroBurnSession *session;

	switch (property_id) {
	case PROP_SESSION: /* Readable and only writable at creation time */
		/* NOTE: no need to unref a potential previous session since
		 * it's only set at construct time */
		session = g_value_get_object (value);
		brasero_dest_selection_set_session (BRASERO_DEST_SELECTION (object), session);
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
						 NULL);
		break;

	case BRASERO_IMAGE_FORMAT_CLONE:
	case BRASERO_IMAGE_FORMAT_CDRDAO:
	case BRASERO_IMAGE_FORMAT_CUE:
		brasero_burn_session_get_output (priv->session,
						 NULL,
						 &path);
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
	guint used;
	gchar *label;
	goffset blocks = 0;
	gchar *medium_name;
	gchar *size_string;
	BraseroMedia media;
	BraseroBurnFlag flags;
	goffset size_bytes = 0;
	goffset data_blocks = 0;
	goffset session_bytes = 0;
	BraseroTrackType *input = NULL;
	BraseroDestSelectionPrivate *priv;

	priv = BRASERO_DEST_SELECTION_PRIVATE (selection);

	if (!priv->session)
		return NULL;

	medium_name = brasero_volume_get_name (BRASERO_VOLUME (medium));
	if (brasero_medium_get_status (medium) & BRASERO_MEDIUM_FILE) {
		gchar *path;

		input = brasero_track_type_new ();
		brasero_burn_session_get_input_type (priv->session, input);

		/* There should be a special name for image in video context */
		if (brasero_track_type_get_has_stream (input)
		&&  BRASERO_STREAM_FORMAT_HAS_VIDEO (brasero_track_type_get_stream_format (input))) {
			BraseroImageFormat format;

			format = brasero_burn_session_get_output_format (priv->session);
			if (format == BRASERO_IMAGE_FORMAT_CUE) {
				g_free (medium_name);
				if (brasero_burn_session_tag_lookup_int (priv->session, BRASERO_VCD_TYPE) == BRASERO_SVCD)
					medium_name = g_strdup (_("SVCD image"));
				else
					medium_name = g_strdup (_("VCD image"));
			}
			else if (format == BRASERO_IMAGE_FORMAT_BIN) {
				g_free (medium_name);
				medium_name = g_strdup (_("Video DVD image"));
			}
		}
		brasero_track_type_free (input);

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

		brasero_medium_selection_update_used_space (BRASERO_MEDIUM_SELECTION (selection),
							    medium,
							    0);
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
			label = g_strdup_printf (_("New disc in the burner holding the source disc"));
			g_free (medium_name);

			brasero_medium_selection_update_used_space (BRASERO_MEDIUM_SELECTION (selection),
								    medium,
								    0);
			return label;
		}
	}

	media = brasero_medium_get_status (medium);
	flags = brasero_burn_session_get_flags (priv->session);
	brasero_burn_session_get_size (priv->session,
				       &data_blocks,
				       &session_bytes);

	if (flags & (BRASERO_BURN_FLAG_MERGE|BRASERO_BURN_FLAG_APPEND))
		brasero_medium_get_free_space (medium, &size_bytes, &blocks);
	else if (brasero_burn_library_get_media_capabilities (media) & BRASERO_MEDIUM_REWRITABLE)
		brasero_medium_get_capacity (medium, &size_bytes, &blocks);
	else
		brasero_medium_get_free_space (medium, &size_bytes, &blocks);

	if (blocks) {
		used = data_blocks * 100 / blocks;
		if (data_blocks && !used)
			used = 1;

		used = MIN (100, used);
	}
	else
		used = 0;

	brasero_medium_selection_update_used_space (BRASERO_MEDIUM_SELECTION (selection),
						    medium,
						    used);
	blocks -= data_blocks;
	if (blocks <= 0) {
		brasero_track_type_free (input);

		/* NOTE for translators, the first %s is the medium name */
		label = g_strdup_printf (_("%s: not enough free space"), medium_name);
		g_free (medium_name);
		return label;
	}

	/* format the size */
	if (brasero_track_type_get_has_stream (input)
	&& BRASERO_STREAM_FORMAT_HAS_VIDEO (brasero_track_type_get_stream_format (input))) {
		guint64 free_time;

		/* This is an embarassing problem: this is an approximation
		 * based on the fact that 2 hours = 4.3GiB */
		free_time = size_bytes - session_bytes;
		free_time = free_time * 72000LL / 47LL;
		size_string = brasero_units_get_time_string (free_time,
							     TRUE,
							     TRUE);
	}
	else if (brasero_track_type_get_has_stream (input)
	|| (brasero_track_type_get_has_medium (input)
	&& (brasero_track_type_get_medium_type (input) & BRASERO_MEDIUM_HAS_AUDIO)))
		size_string = brasero_units_get_time_string (BRASERO_SECTORS_TO_DURATION (blocks),
							     TRUE,
							     TRUE);
	else
		size_string = g_format_size (size_bytes - session_bytes);

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
	object_class->constructed = brasero_dest_selection_constructed;

	medium_selection_class->format_medium_string = brasero_dest_selection_format_medium_string;
	medium_selection_class->medium_changed = brasero_dest_selection_medium_changed;
	g_object_class_install_property (object_class,
					 PROP_SESSION,
					 g_param_spec_object ("session",
							      "The session",
							      "The session to work with",
							      BRASERO_TYPE_BURN_SESSION,
							      G_PARAM_READWRITE));
}

GtkWidget *
brasero_dest_selection_new (BraseroBurnSession *session)
{
	return g_object_new (BRASERO_TYPE_DEST_SELECTION,
			     "session", session,
			     NULL);
}
