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

#include <gconf/gconf-client.h>

#include "burn-basics.h"
#include "burn-debug.h"
#include "burn-plugin-manager.h"
#include "brasero-session.h"
#include "burn-plugin-manager.h"
#include "burn-image-format.h"

#include "brasero-tags.h"
#include "brasero-track-image.h"
#include "brasero-session-cfg.h"
#include "brasero-burn-lib.h"
#include "brasero-session-helper.h"

typedef struct _BraseroSessionCfgPrivate BraseroSessionCfgPrivate;
struct _BraseroSessionCfgPrivate
{
	BraseroBurnFlag supported;
	BraseroBurnFlag compulsory;

	gchar *output;

	glong caps_sig;

	BraseroSessionError is_valid;

	guint CD_TEXT_modified:1;
	guint configuring:1;
	guint disabled:1;
};

#define BRASERO_SESSION_CFG_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_SESSION_CFG, BraseroSessionCfgPrivate))

enum
{
	IS_VALID_SIGNAL,
	LAST_SIGNAL
};


static guint session_cfg_signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (BraseroSessionCfg, brasero_session_cfg, BRASERO_TYPE_SESSION_SPAN);

#define BRASERO_DEST_SAVED_FLAGS		(BRASERO_DRIVE_PROPERTIES_FLAGS|BRASERO_BURN_FLAG_MULTI)
#define BRASERO_DRIVE_PROPERTIES_KEY		"/apps/brasero/drives"

/**
 * Get a key to save parameters through GConf
 */

static gchar *
brasero_session_cfg_get_gconf_key (BraseroSessionCfg *self,
				   const gchar *property)
{
	BraseroTrackType *type;
	BraseroMedium *medium;
	BraseroDrive *drive;
	gchar *display_name;
	gchar *key = NULL;
	gchar *disc_type;

	drive = brasero_burn_session_get_burner (BRASERO_BURN_SESSION (self));
	if (!drive)
		return NULL;

	medium = brasero_drive_get_medium (drive);
	if (brasero_medium_get_status (medium) == BRASERO_MEDIUM_NONE)
		return NULL;
	
	/* make sure display_name doesn't contain any forbidden characters */
	if (!brasero_drive_is_fake (drive)) {
		gchar *tmp;

		tmp = brasero_drive_get_display_name (drive);
		display_name = gconf_escape_key (tmp, -1);
		g_free (tmp);
	}
	else
		display_name = g_strdup ("File");

	display_name = display_name ? display_name : "";

	disc_type = gconf_escape_key (brasero_medium_get_type_string (medium), -1);
	if (!disc_type) {
		g_free (display_name);
		return NULL;
	}

	type = brasero_track_type_new ();
	brasero_burn_session_get_input_type (BRASERO_BURN_SESSION (self), type);
	if (brasero_track_type_get_has_medium (type))
		key = g_strdup_printf ("%s/%s/disc_%s/%s",
				       BRASERO_DRIVE_PROPERTIES_KEY,
				       display_name,
				       disc_type,
				       property);
	else if (brasero_track_type_get_has_data (type))
		key = g_strdup_printf ("%s/%s/data_%s/%s",
				       BRASERO_DRIVE_PROPERTIES_KEY,
				       display_name,
				       disc_type,
				       property);
	else if (brasero_track_type_get_has_image (type))
		key = g_strdup_printf ("%s/%s/image_%s/%s",
				       BRASERO_DRIVE_PROPERTIES_KEY,
				       display_name,
				       disc_type,
				       property);
	else if (brasero_track_type_get_has_stream (type))
		key = g_strdup_printf ("%s/%s/audio_%s/%s",
				       BRASERO_DRIVE_PROPERTIES_KEY,
				       display_name,
				       disc_type,
				       property);
	else
		key = g_strdup_printf ("%s/%s/none_%s/%s",
				       BRASERO_DRIVE_PROPERTIES_KEY,
				       display_name,
				       disc_type,
				       property);

	brasero_track_type_free (type);
	g_free (display_name);
	g_free (disc_type);
	return key;
}

BraseroSessionError
brasero_session_cfg_get_error (BraseroSessionCfg *self)
{
	BraseroSessionCfgPrivate *priv;

	priv = BRASERO_SESSION_CFG_PRIVATE (self);

	if (priv->is_valid == BRASERO_SESSION_VALID
	&&  priv->CD_TEXT_modified)
		return BRASERO_SESSION_NO_CD_TEXT;

	return priv->is_valid;
}

void
brasero_session_cfg_disable (BraseroSessionCfg *self)
{
	BraseroSessionCfgPrivate *priv;

	priv = BRASERO_SESSION_CFG_PRIVATE (self);
	priv->disabled = TRUE;
}

static void
brasero_session_cfg_save_drive_properties (BraseroSessionCfg *self)
{
	BraseroSessionCfgPrivate *priv;
	BraseroBurnFlag flags;
	GConfClient *client;
	const gchar *path;
	guint64 rate;
	gchar *key;

	priv = BRASERO_SESSION_CFG_PRIVATE (self);

	client = gconf_client_get_default ();

	rate = brasero_burn_session_get_rate (BRASERO_BURN_SESSION (self));
	key = brasero_session_cfg_get_gconf_key (self, "speed");
	if (!key) {
		g_object_unref (client);
		return;
	}

	gconf_client_set_int (client, key, rate / 1000, NULL);
	g_free (key);

	key = brasero_session_cfg_get_gconf_key (self, "flags");
	if (!key) {
		g_object_unref (client);
		return;
	}

	flags = gconf_client_get_int (client, key, NULL);
	flags &= ~BRASERO_DEST_SAVED_FLAGS;
	flags |= (brasero_burn_session_get_flags (BRASERO_BURN_SESSION (self)) & BRASERO_DEST_SAVED_FLAGS);
	gconf_client_set_int (client, key, flags, NULL);
	g_free (key);

	/* temporary directory */
	path = brasero_burn_session_get_tmpdir (BRASERO_BURN_SESSION (self));
	key = g_strdup_printf ("%s/tmpdir", BRASERO_DRIVE_PROPERTIES_KEY);
	gconf_client_set_string (client, key, path, NULL);
	g_free (key);

	g_object_unref (client);
}

static void
brasero_session_cfg_add_drive_properties_flags (BraseroSessionCfg *self,
						BraseroBurnFlag flags)
{
	BraseroMedia media;
	BraseroBurnFlag flag;
	BraseroBurnResult result;
	BraseroSessionCfgPrivate *priv;

	priv = BRASERO_SESSION_CFG_PRIVATE (self);

	media = brasero_burn_session_get_dest_media (BRASERO_BURN_SESSION (self));

	/* add flags then wipe out flags from session to check them one by one */
	flags |= brasero_burn_session_get_flags (BRASERO_BURN_SESSION (self));
	brasero_burn_session_remove_flag (BRASERO_BURN_SESSION (self), flags);

	priv->supported = BRASERO_BURN_FLAG_NONE;
	priv->compulsory = BRASERO_BURN_FLAG_NONE;
	result = brasero_burn_session_get_burn_flags (BRASERO_BURN_SESSION (self),
						      &priv->supported,
						      &priv->compulsory);
	if (result != BRASERO_BURN_OK) {
		brasero_burn_session_set_flags (BRASERO_BURN_SESSION (self), flags);
		return;
	}

	/* These are always supported and better be set. */
	brasero_burn_session_set_flags (BRASERO_BURN_SESSION (self),
					BRASERO_BURN_FLAG_DONT_OVERWRITE|
					BRASERO_BURN_FLAG_CHECK_SIZE|
					BRASERO_BURN_FLAG_NOGRACE);

	for (flag = BRASERO_BURN_FLAG_EJECT; flag < BRASERO_BURN_FLAG_LAST; flag <<= 1) {
		/* see if this flag was originally set */
		if (!(flags & flag))
			continue;

		/* Don't set write modes now in this case */
		if (brasero_burn_session_same_src_dest_drive (BRASERO_BURN_SESSION (self))
		&& (flag & (BRASERO_BURN_FLAG_DAO|BRASERO_BURN_FLAG_RAW)))
			continue;

		if (priv->compulsory
		&& (priv->compulsory & brasero_burn_session_get_flags (BRASERO_BURN_SESSION (self))) != priv->compulsory) {
			brasero_burn_session_add_flag (BRASERO_BURN_SESSION (self), priv->compulsory);

			priv->supported = BRASERO_BURN_FLAG_NONE;
			priv->compulsory = BRASERO_BURN_FLAG_NONE;
			brasero_burn_session_get_burn_flags (BRASERO_BURN_SESSION (self),
							     &priv->supported,
							     &priv->compulsory);
		}

		if (priv->supported & flag) {
			brasero_burn_session_add_flag (BRASERO_BURN_SESSION (self), flag);

			priv->supported = BRASERO_BURN_FLAG_NONE;
			priv->compulsory = BRASERO_BURN_FLAG_NONE;
			brasero_burn_session_get_burn_flags (BRASERO_BURN_SESSION (self),
							     &priv->supported,
							     &priv->compulsory);
		}
	}

	if (BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDRW_PLUS)
	||  BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDRW_RESTRICTED)
	||  BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDRW_PLUS_DL)) {
		/* This is a special case to favour libburn/growisofs
		 * wodim/cdrecord for these types of media. */
		if (priv->supported & BRASERO_BURN_FLAG_MULTI) {
			brasero_burn_session_add_flag (BRASERO_BURN_SESSION (self),
						       BRASERO_BURN_FLAG_MULTI);

			priv->supported = BRASERO_BURN_FLAG_NONE;
			priv->compulsory = BRASERO_BURN_FLAG_NONE;
			brasero_burn_session_get_burn_flags (BRASERO_BURN_SESSION (self),
							     &priv->supported,
							     &priv->compulsory);
		}
	}

	/* Always set this flag whenever possible */
	if (priv->supported & BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE) {
		brasero_burn_session_add_flag (BRASERO_BURN_SESSION (self),
					       BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE);

		if (priv->supported & BRASERO_BURN_FLAG_FAST_BLANK)
			brasero_burn_session_add_flag (BRASERO_BURN_SESSION (self),
						       BRASERO_BURN_FLAG_FAST_BLANK);

		priv->supported = BRASERO_BURN_FLAG_NONE;
		priv->compulsory = BRASERO_BURN_FLAG_NONE;
		brasero_burn_session_get_burn_flags (BRASERO_BURN_SESSION (self),
						     &priv->supported,
						     &priv->compulsory);
	}

	/* When copying with same drive don't set write mode, it'll be set later */
	if (!brasero_burn_session_same_src_dest_drive (BRASERO_BURN_SESSION (self))) {
		/* use DAO whenever it's possible */
		if (priv->supported & BRASERO_BURN_FLAG_DAO) {
			brasero_burn_session_add_flag (BRASERO_BURN_SESSION (self), BRASERO_BURN_FLAG_DAO);

			priv->supported = BRASERO_BURN_FLAG_NONE;
			priv->compulsory = BRASERO_BURN_FLAG_NONE;
			brasero_burn_session_get_burn_flags (BRASERO_BURN_SESSION (self),
							     &priv->supported,
							     &priv->compulsory);

			/* NOTE: after setting DAO, some flags may become
			 * compulsory like BLANK_BEFORE for CDRW with data */
		}
	}

	/* Always save flags */
	brasero_session_cfg_save_drive_properties (self);
}

static void
brasero_session_cfg_set_drive_properties (BraseroSessionCfg *self)
{
	BraseroSessionCfgPrivate *priv;
	BraseroTrackType *source;
	BraseroBurnFlag flags;
	BraseroMedium *medium;
	BraseroDrive *drive;
	GConfClient *client;
	GConfValue *value;
	guint64 rate;
	gchar *path;
	gchar *key;

	priv = BRASERO_SESSION_CFG_PRIVATE (self);

	/* The next two must work as they were checked earlier */
	source = brasero_track_type_new ();
	brasero_burn_session_get_input_type (BRASERO_BURN_SESSION (self), source);
	drive = brasero_burn_session_get_burner (BRASERO_BURN_SESSION (self));

	medium = brasero_drive_get_medium (drive);
	if (!medium || brasero_medium_get_status (medium) == BRASERO_MEDIUM_NONE)
		return;

	/* Update/set the rate */
	client = gconf_client_get_default ();

	key = brasero_session_cfg_get_gconf_key (self, "speed");
	value = gconf_client_get_without_default (client, key, NULL);
	g_free (key);

	if (value) {
		rate = gconf_value_get_int (value) * 1000;
		gconf_value_free (value);
	}
	else
		rate = brasero_medium_get_max_write_speed (medium);

	brasero_burn_session_set_rate (BRASERO_BURN_SESSION (self), rate);

	/* Set temporary directory
	 * NOTE: BraseroBurnSession can cope with NULL path */
	key = g_strdup_printf ("%s/tmpdir", BRASERO_DRIVE_PROPERTIES_KEY);
	path = gconf_client_get_string (client, key, NULL);
	g_free (key);

	brasero_burn_session_set_tmpdir (BRASERO_BURN_SESSION (self), path);
	g_free (path);

	/* Do the same with the flags.
	 * NOTE: we only save/load PROPERTIES_FLAGS */
	key = brasero_session_cfg_get_gconf_key (self, "flags");
	if (!key) {
		g_object_unref (client);
		return;
	}

	value = gconf_client_get_without_default (client, key, NULL);
	g_free (key);

	g_object_unref (client);

	if (brasero_burn_session_same_src_dest_drive (BRASERO_BURN_SESSION (self))) {
		/* Special case */
		if (value) {
			flags = gconf_value_get_int (value) & BRASERO_DEST_SAVED_FLAGS;
			gconf_value_free (value);
		}
		else
			flags = BRASERO_BURN_FLAG_EJECT|
				BRASERO_BURN_FLAG_BURNPROOF;

		flags |= BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE|
			 BRASERO_BURN_FLAG_FAST_BLANK;
	}
	else if (!value) {
		/* Set sound defaults. */
		flags = BRASERO_BURN_FLAG_EJECT|
			BRASERO_BURN_FLAG_BURNPROOF;

		if (brasero_track_type_get_has_data (source)
		||  brasero_track_type_get_has_medium (source)
		||  brasero_track_type_get_has_image (source))
			flags |= BRASERO_BURN_FLAG_NO_TMP_FILES;
	}
	else {
		/* set the saved flags */
		flags = gconf_value_get_int (value) & BRASERO_DEST_SAVED_FLAGS;
		gconf_value_free (value);
	}

	brasero_session_cfg_add_drive_properties_flags (self, flags);
}

static void
brasero_session_cfg_check_drive_settings (BraseroSessionCfg *self)
{
	BraseroSessionCfgPrivate *priv;
	BraseroBurnFlag flags;

	priv = BRASERO_SESSION_CFG_PRIVATE (self);

	/* Try to properly update the flags for the current drive */
	flags = brasero_burn_session_get_flags (BRASERO_BURN_SESSION (self));
	if (brasero_burn_session_same_src_dest_drive (BRASERO_BURN_SESSION (self))) {
		flags |= BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE|
			 BRASERO_BURN_FLAG_FAST_BLANK;
	}

	/* check each flag before re-adding it */
	brasero_session_cfg_add_drive_properties_flags (self, flags);
}

static BraseroSessionError
brasero_session_cfg_check_size (BraseroSessionCfg *self)
{
	BraseroSessionCfgPrivate *priv;
	BraseroBurnFlag flags;
	BraseroMedium *medium;
	BraseroDrive *burner;
	GValue *value = NULL;
	/* in sectors */
	gint64 session_size;
	gint64 max_sectors;
	gint64 disc_size;

	priv = BRASERO_SESSION_CFG_PRIVATE (self);

	burner = brasero_burn_session_get_burner (BRASERO_BURN_SESSION (self));
	if (!burner) {
		priv->is_valid = BRASERO_SESSION_NO_OUTPUT;
		return BRASERO_SESSION_NO_OUTPUT;
	}

	/* FIXME: here we could check the hard drive space */
	if (brasero_drive_is_fake (burner)) {
		priv->is_valid = BRASERO_SESSION_VALID;
		return BRASERO_SESSION_VALID;
	}

	medium = brasero_drive_get_medium (burner);
	if (!medium) {
		priv->is_valid = BRASERO_SESSION_NO_OUTPUT;
		return BRASERO_SESSION_NO_OUTPUT;
	}

	flags = brasero_burn_session_get_flags (BRASERO_BURN_SESSION (self));
	if (flags & (BRASERO_BURN_FLAG_MERGE|BRASERO_BURN_FLAG_APPEND))
		brasero_medium_get_free_space (medium, NULL, &disc_size);
	else if (brasero_burn_session_can_blank (BRASERO_BURN_SESSION (self)) == BRASERO_BURN_OK)
		brasero_medium_get_capacity (medium, NULL, &disc_size);
	else
		brasero_medium_get_free_space (medium, NULL, &disc_size);

	if (disc_size < 0)
		disc_size = 0;

	/* get input track size */
	session_size = 0;

	if (brasero_burn_session_tag_lookup (BRASERO_BURN_SESSION (self),
					     BRASERO_DATA_TRACK_SIZE_TAG,
					     &value) == BRASERO_BURN_OK) {
		session_size = g_value_get_int64 (value);
	}
	else if (brasero_burn_session_tag_lookup (BRASERO_BURN_SESSION (self),
						  BRASERO_STREAM_TRACK_SIZE_TAG,
						  &value) == BRASERO_BURN_OK) {
		session_size = g_value_get_int64 (value);
	}
	else
		brasero_burn_session_get_size (BRASERO_BURN_SESSION (self),
					       &session_size,
					       NULL);

	BRASERO_BURN_LOG ("Session size %lli/Disc size %lli",
			  session_size,
			  disc_size);

	if (session_size < disc_size) {
		priv->is_valid = BRASERO_SESSION_VALID;
		return BRASERO_SESSION_VALID;
	}

	/* FIXME: This is not good since with a DVD 3% of 4.3G may be too much
	 * with 3% we are slightly over the limit of the most overburnable discs
	 * but at least users can try to overburn as much as they can. */

	/* The idea would be to test write the disc with cdrecord from /dev/null
	 * until there is an error and see how much we were able to write. So,
	 * when we propose overburning to the user, we could ask if he wants
	 * us to determine how much data can be written to a particular disc
	 * provided he has chosen a real disc. */
	max_sectors = disc_size * 103 / 100;
	if (max_sectors < session_size) {
		priv->is_valid = BRASERO_SESSION_INSUFFICIENT_SPACE;
		return BRASERO_SESSION_INSUFFICIENT_SPACE;
	}

	if (!(flags & BRASERO_BURN_FLAG_OVERBURN)) {
		BraseroSessionCfgPrivate *priv;

		priv = BRASERO_SESSION_CFG_PRIVATE (self);

		if (!(priv->supported & BRASERO_BURN_FLAG_OVERBURN)) {
			priv->is_valid = BRASERO_SESSION_INSUFFICIENT_SPACE;
			return BRASERO_SESSION_INSUFFICIENT_SPACE;
		}

		priv->is_valid = BRASERO_SESSION_OVERBURN_NECESSARY;
		return BRASERO_SESSION_OVERBURN_NECESSARY;
	}

	priv->is_valid = BRASERO_SESSION_VALID;
	return BRASERO_SESSION_VALID;
}

static void
brasero_session_cfg_set_tracks_audio_format (BraseroBurnSession *session,
					     BraseroStreamFormat format)
{
	GSList *tracks;
	GSList *iter;

	tracks = brasero_burn_session_get_tracks (session);
	for (iter = tracks; iter; iter = iter->next) {
		BraseroTrack *track;

		track = iter->data;
		if (!BRASERO_IS_TRACK_STREAM (track))
			continue;

		brasero_track_stream_set_format (BRASERO_TRACK_STREAM (track), format);
	}
}

static void
brasero_session_cfg_update (BraseroSessionCfg *self,
			    gboolean update,
			    gboolean check)
{
	BraseroTrackType *source = NULL;
	BraseroSessionCfgPrivate *priv;
	BraseroBurnResult result;
	BraseroStatus *status;
	BraseroDrive *burner;

	priv = BRASERO_SESSION_CFG_PRIVATE (self);

	if (priv->configuring)
		return;

	/* Make sure the session is ready */
	status = brasero_status_new ();
	result = brasero_burn_session_get_status (BRASERO_BURN_SESSION (self), status);
	if (result == BRASERO_BURN_NOT_READY) {
		brasero_status_free (status);

		priv->is_valid = BRASERO_SESSION_NOT_READY;
		g_signal_emit (self,
			       session_cfg_signals [IS_VALID_SIGNAL],
			       0);
		return;

	}
	else if (result == BRASERO_BURN_ERR) {
		GError *error;

		error = brasero_status_get_error (status);
		if (error) {
			if (error->code == BRASERO_BURN_ERROR_EMPTY) {
				brasero_status_free (status);
				g_error_free (error);

				priv->is_valid = BRASERO_SESSION_EMPTY;
				g_signal_emit (self,
					       session_cfg_signals [IS_VALID_SIGNAL],
					       0);
				return;
			}

			g_error_free (error);
		}
	}
	brasero_status_free (status);

	/* Make sure there is a source */
	source = brasero_track_type_new ();
	brasero_burn_session_get_input_type (BRASERO_BURN_SESSION (self), source);

	if (brasero_track_type_is_empty (source)) {
		brasero_track_type_free (source);

		priv->is_valid = BRASERO_SESSION_NOT_SUPPORTED;
		g_signal_emit (self,
			       session_cfg_signals [IS_VALID_SIGNAL],
			       0);
		return;
	}

	if (brasero_track_type_get_has_medium (source)
	&&  brasero_track_type_get_medium_type (source) == BRASERO_MEDIUM_NONE) {
		brasero_track_type_free (source);

		priv->is_valid = BRASERO_SESSION_NO_INPUT_MEDIUM;
		g_signal_emit (self,
			       session_cfg_signals [IS_VALID_SIGNAL],
			       0);
		return;
	}

	if (brasero_track_type_get_has_image (source)
	&&  brasero_track_type_get_image_format (source) == BRASERO_IMAGE_FORMAT_NONE) {
		gchar *uri;
		GSList *tracks;

		brasero_track_type_free (source);

		tracks = brasero_burn_session_get_tracks (BRASERO_BURN_SESSION (self));

		/* It can be two cases:
		 * - no image set
		 * - image format cannot be detected */
		if (tracks) {
			BraseroTrack *track;

			track = tracks->data;
			uri = brasero_track_image_get_source (BRASERO_TRACK_IMAGE (track), TRUE);
			if (uri) {
				priv->is_valid = BRASERO_SESSION_UNKNOWN_IMAGE;
				g_free (uri);
			}
			else
				priv->is_valid = BRASERO_SESSION_NO_INPUT_IMAGE;
		}
		else
			priv->is_valid = BRASERO_SESSION_NO_INPUT_IMAGE;

		g_signal_emit (self,
			       session_cfg_signals [IS_VALID_SIGNAL],
			       0);
		return;
	}

	/* FIXME: another easy error to catch: AUDIO project with a DVD */

	/* make sure there is an output set */
	burner = brasero_burn_session_get_burner (BRASERO_BURN_SESSION (self));
	if (!burner) {
		brasero_track_type_free (source);

		priv->is_valid = BRASERO_SESSION_NO_OUTPUT;
		g_signal_emit (self,
			       session_cfg_signals [IS_VALID_SIGNAL],
			       0);
		return;
	}

	/* Check that current input and output work */
	if (priv->CD_TEXT_modified) {
		/* Try to redo what we undid (after all a new plugin could have
		 * been activated in the mean time ...) and see what happens */
		brasero_track_type_set_image_format (source,
						     BRASERO_METADATA_INFO|
						     brasero_track_type_get_image_format (source));
		result = brasero_burn_session_input_supported (BRASERO_BURN_SESSION (self),
							       source,
							       FALSE);
		if (result == BRASERO_BURN_OK) {
			priv->CD_TEXT_modified = FALSE;

			priv->configuring = TRUE;
			brasero_session_cfg_set_tracks_audio_format (BRASERO_BURN_SESSION (self),
								     brasero_track_type_get_stream_format (source));
			priv->configuring = FALSE;
		}
		else {
			/* No, nothing's changed */
			brasero_track_type_set_stream_format (source,
							      (~BRASERO_METADATA_INFO) &
							      brasero_track_type_get_stream_format (source));
			result = brasero_burn_session_input_supported (BRASERO_BURN_SESSION (self),
								       source,
								       FALSE);
		}
	}
	else {
		/* Don't use flags as they'll be adapted later. */
		result = brasero_burn_session_can_burn (BRASERO_BURN_SESSION (self),
							FALSE);
		if (result != BRASERO_BURN_OK
		&&  brasero_track_type_get_has_stream (source)
		&& (brasero_track_type_get_stream_format (source) & BRASERO_METADATA_INFO)) {
			/* Another special case in case some burning backends 
			 * don't support CD-TEXT for audio (libburn). If no
			 * other backend is available remove CD-TEXT option but
			 * tell user... */
			brasero_track_type_set_stream_format (source,
							      (~BRASERO_METADATA_INFO) &
							      brasero_track_type_get_stream_format (source));

			result = brasero_burn_session_input_supported (BRASERO_BURN_SESSION (self),
								       source,
								       FALSE);
			BRASERO_BURN_LOG ("Tested support without Metadata information (result %d)", result);
			if (result == BRASERO_BURN_OK) {
				priv->CD_TEXT_modified = TRUE;

				priv->configuring = TRUE;
				brasero_session_cfg_set_tracks_audio_format (BRASERO_BURN_SESSION (self),
									     brasero_track_type_get_has_stream (source));
				priv->configuring = FALSE;
			}
		}
	}

	if (result != BRASERO_BURN_OK) {
		if (brasero_track_type_get_has_medium (source)
		&& (brasero_track_type_get_medium_type (source) & BRASERO_MEDIUM_PROTECTED)
		&&  brasero_burn_library_input_supported (source) != BRASERO_BURN_OK) {
			/* This is a special case to display a helpful message */
			priv->is_valid = BRASERO_SESSION_DISC_PROTECTED;
			g_signal_emit (self,
				       session_cfg_signals [IS_VALID_SIGNAL],
				       0);
		}
		else {
			priv->is_valid = BRASERO_SESSION_NOT_SUPPORTED;
			g_signal_emit (self,
				       session_cfg_signals [IS_VALID_SIGNAL],
				       0);
		}

		brasero_track_type_free (source);
		return;
	}

	brasero_track_type_free (source);

	/* Configure flags */
	priv->configuring = TRUE;

	if (brasero_drive_is_fake (burner))
		/* Remove some impossible flags */
		brasero_burn_session_remove_flag (BRASERO_BURN_SESSION (self),
						  BRASERO_BURN_FLAG_DUMMY|
						  BRASERO_BURN_FLAG_NO_TMP_FILES);

	if (update)
		brasero_session_cfg_set_drive_properties (self);
	else if (check)
		brasero_session_cfg_check_drive_settings (self);

	priv->configuring = FALSE;

	/* Finally check size */
	if (brasero_burn_session_same_src_dest_drive (BRASERO_BURN_SESSION (self))) {
		priv->is_valid = BRASERO_SESSION_VALID;
		g_signal_emit (self,
			       session_cfg_signals [IS_VALID_SIGNAL],
			       0);
	}
	else {
		brasero_session_cfg_check_size (self);
		g_signal_emit (self,
			       session_cfg_signals [IS_VALID_SIGNAL],
			       0);
	}
}

static void
brasero_session_cfg_input_changed (BraseroBurnSession *session)
{
	BraseroSessionCfgPrivate *priv;

	priv = BRASERO_SESSION_CFG_PRIVATE (session);
	if (priv->disabled)
		return;

	/* when that happens it's mostly because a medium source changed, or
	 * a new image was set. 
	 * - reload saved flags
	 * - check if all flags are thereafter supported
	 * - check available formats for path
	 * - set one path
	 */
	brasero_session_cfg_update (BRASERO_SESSION_CFG (session),
				    TRUE,
				    FALSE);
}

static void
brasero_session_cfg_output_changed (BraseroBurnSession *session)
{
	BraseroSessionCfgPrivate *priv;

	priv = BRASERO_SESSION_CFG_PRIVATE (session);
	if (priv->disabled)
		return;

	/* In this case need to :
	 * - load flags 
	 * - check if all flags are thereafter supported
	 * - for images, set a path if it wasn't already set
	 */
	brasero_session_cfg_update (BRASERO_SESSION_CFG (session),
				    TRUE,
				    FALSE);
}

static void
brasero_session_cfg_caps_changed (BraseroPluginManager *manager,
				  BraseroSessionCfg *self)
{
	BraseroSessionCfgPrivate *priv;

	priv = BRASERO_SESSION_CFG_PRIVATE (self);
	if (priv->disabled)
		return;

	/* In this case we need to check if:
	 * - new flags are supported or not supported anymore
	 * - new image types as input/output are supported
	 * - if the current set of flags/input/output still works */
	brasero_session_cfg_update (self,
				    FALSE,
				    TRUE);
}

void
brasero_session_cfg_add_flags (BraseroSessionCfg *self,
			       BraseroBurnFlag flags)
{
	BraseroSessionCfgPrivate *priv;

	priv = BRASERO_SESSION_CFG_PRIVATE (self);

	if ((priv->supported & flags) != flags)
		return;

	if ((brasero_burn_session_get_flags (BRASERO_BURN_SESSION (self)) & flags) == flags)
		return;

	brasero_burn_session_add_flag (BRASERO_BURN_SESSION (self), flags);
	priv->supported = BRASERO_BURN_FLAG_NONE;
	priv->compulsory = BRASERO_BURN_FLAG_NONE;
	brasero_burn_session_get_burn_flags (BRASERO_BURN_SESSION (self),
					     &priv->supported,
					     &priv->compulsory);

	/* Always save flags */
	brasero_session_cfg_save_drive_properties (self);

	brasero_session_cfg_update (self,
				    FALSE,
				    FALSE);
}

void
brasero_session_cfg_remove_flags (BraseroSessionCfg *self,
				  BraseroBurnFlag flags)
{
	BraseroSessionCfgPrivate *priv;

	priv = BRASERO_SESSION_CFG_PRIVATE (self);

	brasero_burn_session_remove_flag (BRASERO_BURN_SESSION (self), flags);
	priv->supported = BRASERO_BURN_FLAG_NONE;
	priv->compulsory = BRASERO_BURN_FLAG_NONE;
	brasero_burn_session_get_burn_flags (BRASERO_BURN_SESSION (self),
					     &priv->supported,
					     &priv->compulsory);

	/* Always save flags */
	brasero_session_cfg_save_drive_properties (self);

	brasero_session_cfg_update (self,
				    FALSE,
				    FALSE);
}

gboolean
brasero_session_cfg_is_supported (BraseroSessionCfg *self,
				  BraseroBurnFlag flags)
{
	BraseroSessionCfgPrivate *priv;

	priv = BRASERO_SESSION_CFG_PRIVATE (self);
	return (priv->supported & flags) == flags;
}

gboolean
brasero_session_cfg_is_compulsory (BraseroSessionCfg *self,
				   BraseroBurnFlag flags)
{
	BraseroSessionCfgPrivate *priv;

	priv = BRASERO_SESSION_CFG_PRIVATE (self);
	return (priv->compulsory & flags) == flags;
}

static void
brasero_session_cfg_init (BraseroSessionCfg *object)
{
	BraseroSessionCfgPrivate *priv;
	BraseroPluginManager *manager;

	priv = BRASERO_SESSION_CFG_PRIVATE (object);

	manager = brasero_plugin_manager_get_default ();
	priv->caps_sig = g_signal_connect (manager,
					   "caps-changed",
					   G_CALLBACK (brasero_session_cfg_caps_changed),
					   object);
}

static void
brasero_session_cfg_finalize (GObject *object)
{
	BraseroSessionCfgPrivate *priv;

	priv = BRASERO_SESSION_CFG_PRIVATE (object);

	if (priv->caps_sig) {
		BraseroPluginManager *manager;

		manager = brasero_plugin_manager_get_default ();
		g_signal_handler_disconnect (manager, priv->caps_sig);
		priv->caps_sig = 0;
	}

	G_OBJECT_CLASS (brasero_session_cfg_parent_class)->finalize (object);
}

static void
brasero_session_cfg_class_init (BraseroSessionCfgClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	BraseroBurnSessionClass *session_class = BRASERO_BURN_SESSION_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroSessionCfgPrivate));

	object_class->finalize = brasero_session_cfg_finalize;

	session_class->input_changed = brasero_session_cfg_input_changed;
	session_class->output_changed = brasero_session_cfg_output_changed;

	session_cfg_signals[IS_VALID_SIGNAL] =
		g_signal_new ("is_valid",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_RUN_CLEANUP | G_SIGNAL_ACTION,
		              0,
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE,
			      0,
		              G_TYPE_NONE);
}

BraseroSessionCfg *
brasero_session_cfg_new (void)
{
	return g_object_new (BRASERO_TYPE_SESSION_CFG, NULL);
}
