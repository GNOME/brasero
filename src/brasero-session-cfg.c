/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2008 <bonfire-app@wanadoo.fr>
 * 
 * brasero is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * brasero is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
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
#include "burn-session.h"
#include "burn-caps.h"
#include "burn-plugin-manager.h"
#include "burn-image-format.h"

#include "brasero-session-cfg.h"

typedef struct _BraseroSessionCfgPrivate BraseroSessionCfgPrivate;
struct _BraseroSessionCfgPrivate
{
	BraseroBurnCaps *caps;

	BraseroBurnFlag supported;
	BraseroBurnFlag compulsory;

	gchar *output;

	glong caps_sig;

	BraseroSessionError is_valid;

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

G_DEFINE_TYPE (BraseroSessionCfg, brasero_session_cfg, BRASERO_TYPE_BURN_SESSION);

#define BRASERO_DEST_SAVED_FLAGS	(BRASERO_DRIVE_PROPERTIES_FLAGS|BRASERO_BURN_FLAG_MULTI)

BraseroSessionError
brasero_session_cfg_get_error (BraseroSessionCfg *self)
{
	BraseroSessionCfgPrivate *priv;

	priv = BRASERO_SESSION_CFG_PRIVATE (self);
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
	key = brasero_burn_session_get_config_key (BRASERO_BURN_SESSION (self), "speed");
	if (!key) {
		g_object_unref (client);
		return;
	}

	gconf_client_set_int (client, key, rate / 1024, NULL);
	g_free (key);

	key = brasero_burn_session_get_config_key (BRASERO_BURN_SESSION (self), "flags");
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
	BraseroBurnFlag flag;
	BraseroBurnResult result;
	BraseroSessionCfgPrivate *priv;

	priv = BRASERO_SESSION_CFG_PRIVATE (self);

	priv->supported = BRASERO_BURN_FLAG_NONE;
	priv->compulsory = BRASERO_BURN_FLAG_NONE;

	/* add flags then wipe out flags from session to check them one by one */
	flags |= brasero_burn_session_get_flags (BRASERO_BURN_SESSION (self));
	brasero_burn_session_remove_flag (BRASERO_BURN_SESSION (self), flags);

	result = brasero_burn_caps_get_flags (priv->caps,
					      BRASERO_BURN_SESSION (self),
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
			brasero_burn_caps_get_flags (priv->caps,
						     BRASERO_BURN_SESSION (self),
						     &priv->supported,
						     &priv->compulsory);
		}

		if (priv->supported & flag) {
			brasero_burn_session_add_flag (BRASERO_BURN_SESSION (self), flag);

			priv->supported = BRASERO_BURN_FLAG_NONE;
			priv->compulsory = BRASERO_BURN_FLAG_NONE;
			brasero_burn_caps_get_flags (priv->caps,
						     BRASERO_BURN_SESSION (self),
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
		brasero_burn_caps_get_flags (priv->caps,
					     BRASERO_BURN_SESSION (self),
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
			brasero_burn_caps_get_flags (priv->caps,
						     BRASERO_BURN_SESSION (self),
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
	BraseroTrackType source = { 0, };
	BraseroSessionCfgPrivate *priv;
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
	brasero_burn_session_get_input_type (BRASERO_BURN_SESSION (self), &source);
	drive = brasero_burn_session_get_burner (BRASERO_BURN_SESSION (self));

	medium = brasero_drive_get_medium (drive);
	if (!medium || brasero_medium_get_status (medium) == BRASERO_MEDIUM_NONE)
		return;

	/* Update/set the rate */
	client = gconf_client_get_default ();

	key = brasero_burn_session_get_config_key (BRASERO_BURN_SESSION (self), "speed");
	value = gconf_client_get_without_default (client, key, NULL);
	g_free (key);

	if (value) {
		rate = gconf_value_get_int (value) * 1024;
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
	key = brasero_burn_session_get_config_key (BRASERO_BURN_SESSION (self), "flags");
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
		/* Set sound defaults */
		flags = BRASERO_BURN_FLAG_EJECT|
			BRASERO_BURN_FLAG_BURNPROOF;

		if (source.type == BRASERO_TRACK_TYPE_DATA
		||  source.type == BRASERO_TRACK_TYPE_DISC
		||  source.type == BRASERO_TRACK_TYPE_IMAGE)
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
	GSList *iter;

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
	else
		brasero_medium_get_capacity (medium, NULL, &disc_size);

	if (disc_size < 0)
		disc_size = 0;

	/* get input track size */
	iter = brasero_burn_session_get_tracks (BRASERO_BURN_SESSION (self));
	session_size = 0;

	if (brasero_burn_session_tag_lookup (BRASERO_BURN_SESSION (self),
					     BRASERO_DATA_TRACK_SIZE_TAG,
					     &value) == BRASERO_BURN_OK) {
		session_size = g_value_get_int64 (value);
	}
	else if (brasero_burn_session_tag_lookup (BRASERO_BURN_SESSION (self),
						  BRASERO_AUDIO_TRACK_SIZE_TAG,
						  &value) == BRASERO_BURN_OK) {
		session_size = g_value_get_int64 (value);
	}
	else for (; iter; iter = iter->next) {
		BraseroTrackDataType type;
		BraseroTrack *track;
		gint64 sectors;

		track = iter->data;
		sectors = 0;

		type = brasero_track_get_type (track, NULL);
		if (type == BRASERO_TRACK_TYPE_DISC)
			brasero_track_get_disc_data_size (track, &sectors, NULL);
		else if (type == BRASERO_TRACK_TYPE_IMAGE)
			brasero_track_get_image_size (track, NULL, &sectors, NULL, NULL);
		else if (type == BRASERO_TRACK_TYPE_AUDIO) {
			gint64 len = 0;

			brasero_track_get_audio_length (track, &len);
			sectors = BRASERO_DURATION_TO_SECTORS (len);
		}

		session_size += sectors;
	}

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
brasero_session_cfg_update (BraseroSessionCfg *self,
			    gboolean update,
			    gboolean check)
{
	BraseroSessionCfgPrivate *priv;
	BraseroTrackType source = { 0, };
	BraseroBurnResult result;
	BraseroDrive *burner;

	priv = BRASERO_SESSION_CFG_PRIVATE (self);

	if (priv->configuring)
		return;

	/* make sure there is a source */
	brasero_burn_session_get_input_type (BRASERO_BURN_SESSION (self), &source);
	if (source.type == BRASERO_TRACK_TYPE_NONE) {
		priv->is_valid = BRASERO_SESSION_NOT_SUPPORTED;
		g_signal_emit (self,
			       session_cfg_signals [IS_VALID_SIGNAL],
			       0);
		return;
	}

	if (source.type == BRASERO_TRACK_TYPE_DISC
	&&  source.subtype.media == BRASERO_MEDIUM_NONE) {
		priv->is_valid = BRASERO_SESSION_NO_INPUT_MEDIUM;
		g_signal_emit (self,
			       session_cfg_signals [IS_VALID_SIGNAL],
			       0);
		return;
	}

	if (source.type == BRASERO_TRACK_TYPE_IMAGE
	&&  source.subtype.img_format == BRASERO_IMAGE_FORMAT_NONE) {
		priv->is_valid = BRASERO_SESSION_NO_INPUT_IMAGE;
		g_signal_emit (self,
			       session_cfg_signals [IS_VALID_SIGNAL],
			       0);
		return;
	}

	/* make sure there is an output set */
	burner = brasero_burn_session_get_burner (BRASERO_BURN_SESSION (self));
	if (!burner) {
		priv->is_valid = BRASERO_SESSION_NO_OUTPUT;
		g_signal_emit (self,
			       session_cfg_signals [IS_VALID_SIGNAL],
			       0);
		return;
	}

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

	result = brasero_burn_caps_is_session_supported (priv->caps, BRASERO_BURN_SESSION (self));

	if (result != BRASERO_BURN_OK) {
		/* This is a special case */
		if (source.type == BRASERO_TRACK_TYPE_DISC
		&& (source.subtype.media & BRASERO_MEDIUM_PROTECTED)
		&&  brasero_burn_caps_has_capability (priv->caps, &source) != BRASERO_BURN_OK) {
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
		return;
	}

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
	brasero_burn_caps_get_flags (priv->caps,
				     BRASERO_BURN_SESSION (self),
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
	brasero_burn_caps_get_flags (priv->caps,
				     BRASERO_BURN_SESSION (self),
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

	priv->caps = brasero_burn_caps_get_default ();

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

	if (priv->caps) {
		g_object_unref (priv->caps);
		priv->caps = NULL;
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
		              G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_RUN_CLEANUP | G_SIGNAL_ACTION,
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
