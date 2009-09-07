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
#include "libbrasero-marshal.h"

#include "brasero-tags.h"
#include "brasero-track-image.h"
#include "brasero-track-data-cfg.h"
#include "brasero-session-cfg.h"
#include "brasero-burn-lib.h"
#include "brasero-session-helper.h"

/**
 * SECTION:brasero-session-cfg
 * @short_description: Configure automatically a #BraseroBurnSession object
 * @see_also: #BraseroBurn, #BraseroBurnSession
 * @include: brasero-session-cfg.h
 *
 * This object configures automatically a session reacting to any change
 * made to the various parameters.
 **/

typedef struct _BraseroSessionCfgPrivate BraseroSessionCfgPrivate;
struct _BraseroSessionCfgPrivate
{
	BraseroBurnFlag supported;
	BraseroBurnFlag compulsory;

	gchar *output;

	BraseroSessionError is_valid;

	guint CD_TEXT_modified:1;
	guint configuring:1;
	guint disabled:1;

	guint inhibit_flag_sig:1;
};

#define BRASERO_SESSION_CFG_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_SESSION_CFG, BraseroSessionCfgPrivate))

enum
{
	IS_VALID_SIGNAL,
	WRONG_EXTENSION_SIGNAL,
	LAST_SIGNAL
};


static guint session_cfg_signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (BraseroSessionCfg, brasero_session_cfg, BRASERO_TYPE_SESSION_SPAN);

#define BRASERO_DEST_SAVED_FLAGS		(BRASERO_DRIVE_PROPERTIES_FLAGS|BRASERO_BURN_FLAG_MULTI)
#define BRASERO_DRIVE_PROPERTIES_KEY		"/apps/brasero/drives"

/**
 * This is to handle tags (and more particularly video ones)
 */

static void
brasero_session_cfg_tag_changed (BraseroBurnSession *session,
                                 const gchar *tag)
{
	if (!strcmp (tag, BRASERO_VCD_TYPE)) {
		int svcd_type;

		svcd_type = brasero_burn_session_tag_lookup_int (session, BRASERO_VCD_TYPE);
		if (svcd_type != BRASERO_SVCD)
			brasero_burn_session_tag_add_int (session,
			                                  BRASERO_VIDEO_OUTPUT_ASPECT,
			                                  BRASERO_VIDEO_ASPECT_4_3);
	}
}

/**
 * brasero_session_cfg_has_default_output_path:
 * @cfg: a #BraseroSessionCfg
 *
 * This function returns whether the path returned
 * by brasero_burn_session_get_output () is an 
 * automatically created one.
 *
 * Return value: a #gboolean. TRUE if the path(s)
 * creation is handled by @session, FALSE if it was
 * set.
 **/

gboolean
brasero_session_cfg_has_default_output_path (BraseroSessionCfg *session)
{
	BraseroBurnSessionClass *klass;
	BraseroBurnResult result;

	klass = BRASERO_BURN_SESSION_CLASS (brasero_session_cfg_parent_class);
	result = klass->get_output_path (BRASERO_BURN_SESSION (session), NULL, NULL);
	return (result == BRASERO_BURN_OK);
}

static gboolean
brasero_session_cfg_wrong_extension_signal (BraseroSessionCfg *session)
{
	GValue instance_and_params [1];
	GValue return_value;

	instance_and_params [0].g_type = 0;
	g_value_init (instance_and_params, G_TYPE_FROM_INSTANCE (session));
	g_value_set_instance (instance_and_params, session);
	
	return_value.g_type = 0;
	g_value_init (&return_value, G_TYPE_BOOLEAN);
	g_value_set_boolean (&return_value, FALSE);

	g_signal_emitv (instance_and_params,
			session_cfg_signals [WRONG_EXTENSION_SIGNAL],
			0,
			&return_value);

	g_value_unset (instance_and_params);

	return g_value_get_boolean (&return_value);
}

static BraseroBurnResult
brasero_session_cfg_set_output_image (BraseroBurnSession *session,
				      BraseroImageFormat format,
				      const gchar *image,
				      const gchar *toc)
{
	gchar *dot;
	gchar *set_toc = NULL;
	gchar * set_image = NULL;
	BraseroBurnResult result;
	BraseroBurnSessionClass *klass;
	const gchar *suffixes [] = {".iso",
				    ".toc",
				    ".cue",
				    ".toc",
				    NULL };

	/* Make sure something actually changed */
	klass = BRASERO_BURN_SESSION_CLASS (brasero_session_cfg_parent_class);
	klass->get_output_path (BRASERO_BURN_SESSION (session),
	                        &set_image,
	                        &set_toc);

	if (!set_image && !set_toc) {
		/* see if image and toc set paths differ */
		brasero_burn_session_get_output (BRASERO_BURN_SESSION (session),
		                                 &set_image,
		                                 &set_toc);
		if (set_image && image && !strcmp (set_image, image)) {
			/* It's the same default path so no 
			 * need to carry on and actually set
			 * the path of image. */
			image = NULL;
		}

		if (set_toc && toc && !strcmp (set_toc, toc)) {
			/* It's the same default path so no 
			 * need to carry on and actually set
			 * the path of image. */
			toc = NULL;
		}
	}

	if (set_image)
		g_free (set_image);

	if (set_toc)
		g_free (set_toc);

	/* First set all information */
	result = klass->set_output_image (session,
					  format,
					  image,
					  toc);

	if (!image && !toc)
		return result;

	if (format == BRASERO_IMAGE_FORMAT_NONE)
		format = brasero_burn_session_get_output_format (session);

	if (format == BRASERO_IMAGE_FORMAT_NONE)
		return result;

	if (format & BRASERO_IMAGE_FORMAT_BIN) {
		dot = g_utf8_strrchr (image, -1, '.');
		if (strcmp (suffixes [0], dot)) {
			gboolean res;

			res = brasero_session_cfg_wrong_extension_signal (BRASERO_SESSION_CFG (session));
			if (res) {
				gchar *fixed_path;

				fixed_path = brasero_image_format_fix_path_extension (format, FALSE, image);
				/* NOTE: call ourselves with the fixed path as this way,
				 * in case the path is the same as the default one after
				 * fixing the extension we'll keep on using default path */
				result = brasero_burn_session_set_image_output_full (session,
				                                                     format,
				                                                     fixed_path,
				                                                     toc);
				g_free (fixed_path);
			}
		}
	}
	else {
		dot = g_utf8_strrchr (toc, -1, '.');

		if (format & BRASERO_IMAGE_FORMAT_CLONE
		&& strcmp (suffixes [1], dot)) {
			gboolean res;

			res = brasero_session_cfg_wrong_extension_signal (BRASERO_SESSION_CFG (session));
			if (res) {
				gchar *fixed_path;

				fixed_path = brasero_image_format_fix_path_extension (format, FALSE, toc);
				result = brasero_burn_session_set_image_output_full (session,
				                                                     format,
				                                                     image,
				                                                     fixed_path);
				g_free (fixed_path);
			}
		}
		else if (format & BRASERO_IMAGE_FORMAT_CUE
		     && strcmp (suffixes [2], dot)) {
			gboolean res;

			res = brasero_session_cfg_wrong_extension_signal (BRASERO_SESSION_CFG (session));
			if (res) {
				gchar *fixed_path;

				fixed_path = brasero_image_format_fix_path_extension (format, FALSE, toc);
				result = brasero_burn_session_set_image_output_full (session,
				                                                     format,
				                                                     image,
				                                                     fixed_path);
				g_free (fixed_path);
			}
		}
		else if (format & BRASERO_IMAGE_FORMAT_CDRDAO
		     && strcmp (suffixes [3], dot)) {
			gboolean res;

			res = brasero_session_cfg_wrong_extension_signal (BRASERO_SESSION_CFG (session));
			if (res) {
				gchar *fixed_path;

				fixed_path = brasero_image_format_fix_path_extension (format, FALSE, toc);
				result = brasero_burn_session_set_image_output_full (session,
				                                                     format,
				                                                     image,
				                                                     fixed_path);
				g_free (fixed_path);
			}
		}
	}

	return result;
}

static BraseroBurnResult
brasero_session_cfg_get_output_path (BraseroBurnSession *session,
				     gchar **image,
				     gchar **toc)
{
	gchar *path = NULL;
	BraseroBurnResult result;
	BraseroImageFormat format;
	BraseroBurnSessionClass *klass;

	klass = BRASERO_BURN_SESSION_CLASS (brasero_session_cfg_parent_class);

	result = klass->get_output_path (session,
					 image,
					 toc);
	if (result == BRASERO_BURN_OK)
		return result;

	format = brasero_burn_session_get_output_format (session);
	path = brasero_image_format_get_default_path (format);

	switch (format) {
	case BRASERO_IMAGE_FORMAT_BIN:
		if (image)
			*image = path;
		break;

	case BRASERO_IMAGE_FORMAT_CDRDAO:
	case BRASERO_IMAGE_FORMAT_CLONE:
	case BRASERO_IMAGE_FORMAT_CUE:
		if (toc)
			*toc = path;

		if (image)
			*image = brasero_image_format_get_complement (format, path);
		break;

	default:
		g_free (path);
		return BRASERO_BURN_ERR;
	}

	return BRASERO_BURN_OK;
}

static BraseroImageFormat
brasero_session_cfg_get_output_format (BraseroBurnSession *session)
{
	BraseroBurnSessionClass *klass;
	BraseroImageFormat format;

	klass = BRASERO_BURN_SESSION_CLASS (brasero_session_cfg_parent_class);
	format = klass->get_output_format (session);

	if (format == BRASERO_IMAGE_FORMAT_NONE)
		format = brasero_burn_session_get_default_output_format (session);

	return format;
}

/**
 * Get a key to save parameters through GConf
 */

static gchar *
brasero_session_cfg_get_gconf_key (BraseroSessionCfg *self,
				   BraseroMedium *medium,
				   const gchar *property)
{
	BraseroTrackType *type;
	BraseroDrive *drive;
	gchar *display_name;
	gchar *key = NULL;
	gchar *disc_type;

	if (brasero_medium_get_status (medium) == BRASERO_MEDIUM_NONE)
		return NULL;

	drive = brasero_medium_get_drive (medium);

	/* make sure display_name doesn't contain any forbidden characters */
	if (!brasero_drive_is_fake (drive)) {
		gchar *tmp;

		tmp = brasero_drive_get_display_name (drive);
		display_name = gconf_escape_key (tmp, -1);
		g_free (tmp);
	}
	else
		display_name = g_strdup ("File");

	display_name = display_name ? display_name : g_strdup ("");

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

/**
 * brasero_session_cfg_get_error:
 * @cfg: a #BraseroSessionCfg
 *
 * This function returns the current status and if
 * autoconfiguration is/was successful.
 *
 * Return value: a #BraseroSessionError.
 **/

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

/**
 * brasero_session_cfg_disable:
 * @cfg: a #BraseroSessionCfg
 *
 * This function disables autoconfiguration
 *
 **/

void
brasero_session_cfg_disable (BraseroSessionCfg *self)
{
	BraseroSessionCfgPrivate *priv;

	priv = BRASERO_SESSION_CFG_PRIVATE (self);
	priv->disabled = TRUE;
}

/**
 * brasero_session_cfg_enable:
 * @cfg: a #BraseroSessionCfg
 *
 * This function (re)-enables autoconfiguration
 *
 **/

void
brasero_session_cfg_enable (BraseroSessionCfg *self)
{
	BraseroSessionCfgPrivate *priv;

	priv = BRASERO_SESSION_CFG_PRIVATE (self);
	priv->disabled = FALSE;
}

static void
brasero_session_cfg_save_drive_flags (BraseroSessionCfg *self,
				      BraseroMedium *medium)
{
	BraseroSessionCfgPrivate *priv;
	BraseroBurnFlag flags;
	GConfClient *client;
	gchar *key;

	priv = BRASERO_SESSION_CFG_PRIVATE (self);

	client = gconf_client_get_default ();
	key = brasero_session_cfg_get_gconf_key (self, medium, "flags");
	if (!key) {
		g_object_unref (client);
		return;
	}

	flags = gconf_client_get_int (client, key, NULL);
	flags &= ~BRASERO_DEST_SAVED_FLAGS;
	flags |= (brasero_burn_session_get_flags (BRASERO_BURN_SESSION (self)) & BRASERO_DEST_SAVED_FLAGS);
	gconf_client_set_int (client, key, flags, NULL);
	g_free (key);
}

static void
brasero_session_cfg_save_drive_properties (BraseroSessionCfg *self,
					   BraseroMedium *medium)
{
	BraseroSessionCfgPrivate *priv;
	GConfClient *client;
	const gchar *path;
	guint64 rate;
	gchar *key;

	priv = BRASERO_SESSION_CFG_PRIVATE (self);

	brasero_session_cfg_save_drive_flags (self, medium);

	client = gconf_client_get_default ();

	rate = brasero_burn_session_get_rate (BRASERO_BURN_SESSION (self));
	key = brasero_session_cfg_get_gconf_key (self, medium, "speed");
	if (!key) {
		g_object_unref (client);
		return;
	}

	gconf_client_set_int (client, key, rate / 1000, NULL);
	g_free (key);

	/* temporary directory */
	path = brasero_burn_session_get_tmpdir (BRASERO_BURN_SESSION (self));
	key = g_strdup_printf ("%s/tmpdir", BRASERO_DRIVE_PROPERTIES_KEY);
	gconf_client_set_string (client, key, path, NULL);
	g_free (key);

	g_object_unref (client);
}

static void
brasero_session_cfg_set_drive_properties_default_flags (BraseroSessionCfg *self)
{
	BraseroMedia media;
	BraseroSessionCfgPrivate *priv;

	priv = BRASERO_SESSION_CFG_PRIVATE (self);

	media = brasero_burn_session_get_dest_media (BRASERO_BURN_SESSION (self));

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

		if (priv->supported & BRASERO_BURN_FLAG_FAST_BLANK
		&& (media & BRASERO_MEDIUM_UNFORMATTED) == 0)
			brasero_burn_session_add_flag (BRASERO_BURN_SESSION (self),
						       BRASERO_BURN_FLAG_FAST_BLANK);

		priv->supported = BRASERO_BURN_FLAG_NONE;
		priv->compulsory = BRASERO_BURN_FLAG_NONE;
		brasero_burn_session_get_burn_flags (BRASERO_BURN_SESSION (self),
						     &priv->supported,
						     &priv->compulsory);
	}

	/* When copying with same drive don't set write mode, it'll be set later */
	if (!brasero_burn_session_same_src_dest_drive (BRASERO_BURN_SESSION (self))
	&&  !(media & BRASERO_MEDIUM_DVD)) {
		/* use DAO whenever it's possible except for DVDs otherwise
		 * wodime which claims to support it will be used by default
		 * instead of say growisofs. */
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
}

static void
brasero_session_cfg_set_drive_properties_flags (BraseroSessionCfg *self,
						BraseroBurnFlag flags)
{
	BraseroDrive *drive;
	BraseroMedia media;
	BraseroBurnFlag flag;
	BraseroMedium *medium;
	BraseroBurnResult result;
	BraseroBurnFlag original_flags;
	BraseroSessionCfgPrivate *priv;

	priv = BRASERO_SESSION_CFG_PRIVATE (self);

	original_flags = brasero_burn_session_get_flags (BRASERO_BURN_SESSION (self));
	BRASERO_BURN_LOG ("Resetting all flags");
	BRASERO_BURN_LOG_FLAGS (original_flags, "Current are");
	BRASERO_BURN_LOG_FLAGS (flags, "New should be");

	drive = brasero_burn_session_get_burner (BRASERO_BURN_SESSION (self));
	if (!drive)
		return;

	medium = brasero_drive_get_medium (drive);
	if (!medium)
		return;

	media = brasero_medium_get_status (medium);

	/* This prevents signals to be emitted while (re-) adding them one by one */
	priv->inhibit_flag_sig = TRUE;

	brasero_burn_session_set_flags (BRASERO_BURN_SESSION (self), BRASERO_BURN_FLAG_NONE);

	priv->supported = BRASERO_BURN_FLAG_NONE;
	priv->compulsory = BRASERO_BURN_FLAG_NONE;
	result = brasero_burn_session_get_burn_flags (BRASERO_BURN_SESSION (self),
						      &priv->supported,
						      &priv->compulsory);

	if (result != BRASERO_BURN_OK) {
		brasero_burn_session_set_flags (BRASERO_BURN_SESSION (self), original_flags | flags);
		priv->inhibit_flag_sig = FALSE;
		return;
	}

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

	brasero_session_cfg_set_drive_properties_default_flags (self);

	/* allow flag changed signal again */
	priv->inhibit_flag_sig = FALSE;

	/* These are always supported and better be set.
	 * Set them now to trigger the "flags-changed" signal */
	brasero_burn_session_add_flag (BRASERO_BURN_SESSION (self),
	                               BRASERO_BURN_FLAG_CHECK_SIZE|
	                               BRASERO_BURN_FLAG_NOGRACE);

	/* Always save flags */
	brasero_session_cfg_save_drive_flags (self, medium);
}

static void
brasero_session_cfg_add_drive_properties_flags (BraseroSessionCfg *self,
						BraseroBurnFlag flags)
{
	/* add flags then wipe out flags from session to check them one by one */
	flags |= brasero_burn_session_get_flags (BRASERO_BURN_SESSION (self));
	brasero_session_cfg_set_drive_properties_flags (self, flags);
}

static void
brasero_session_cfg_rm_drive_properties_flags (BraseroSessionCfg *self,
					       BraseroBurnFlag flags)
{
	/* add flags then wipe out flags from session to check them one by one */
	flags = brasero_burn_session_get_flags (BRASERO_BURN_SESSION (self)) & (~flags);
	brasero_session_cfg_set_drive_properties_flags (self, flags);
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
	drive = brasero_burn_session_get_burner (BRASERO_BURN_SESSION (self));
	medium = brasero_drive_get_medium (drive);
	if (!medium || brasero_medium_get_status (medium) == BRASERO_MEDIUM_NONE)
		return;

	/* Update/set the rate */
	client = gconf_client_get_default ();

	key = brasero_session_cfg_get_gconf_key (self, medium, "speed");
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
	key = brasero_session_cfg_get_gconf_key (self, medium, "flags");
	if (!key) {
		g_object_unref (client);
		return;
	}

	value = gconf_client_get_without_default (client, key, NULL);
	g_free (key);

	g_object_unref (client);

	source = brasero_track_type_new ();
	brasero_burn_session_get_input_type (BRASERO_BURN_SESSION (self), source);
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
	brasero_track_type_free (source);

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
	goffset session_size;
	goffset max_sectors;
	goffset disc_size;

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

	disc_size = brasero_burn_session_get_available_medium_space (BRASERO_BURN_SESSION (self));
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

	flags = brasero_burn_session_get_flags (BRASERO_BURN_SESSION (self));
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

	if (result == BRASERO_BURN_ERR) {
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

		priv->is_valid = BRASERO_SESSION_EMPTY;
		g_signal_emit (self,
			       session_cfg_signals [IS_VALID_SIGNAL],
			       0);
		return;
	}

	/* it can be empty with just an empty track */
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
	if (brasero_track_type_get_has_stream (source)) {
		if (priv->CD_TEXT_modified) {
			/* Try to redo what we undid (after all a new plugin could have
			 * been activated in the mean time ...) and see what happens */
			brasero_track_type_set_stream_format (source,
							      BRASERO_METADATA_INFO|
							      brasero_track_type_get_stream_format (source));
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
			result = brasero_burn_session_can_burn (BRASERO_BURN_SESSION (self), FALSE);

			if (result != BRASERO_BURN_OK
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
	}
	else if (brasero_track_type_get_has_medium (source)
	&&  (brasero_track_type_get_medium_type (source) & BRASERO_MEDIUM_HAS_AUDIO)) {
		BraseroImageFormat format = BRASERO_IMAGE_FORMAT_NONE;

		/* If we copy an audio disc check the image
		 * type we're writing to as it may mean that
		 * CD-TEXT cannot be copied.
		 * Make sure that the writing backend
		 * supports writing CD-TEXT?
		 * no, if a backend reports it supports an
		 * image type it should be able to burn all
		 * its data. */
		if (!brasero_burn_session_is_dest_file (BRASERO_BURN_SESSION (self))) {
			BraseroTrackType *tmp_type;

			tmp_type = brasero_track_type_new ();

			/* NOTE: this is the same as brasero_burn_session_can_burn () */
			result = brasero_burn_session_get_tmp_image_type_same_src_dest (BRASERO_BURN_SESSION (self), tmp_type);
			if (result == BRASERO_BURN_OK)
				format = brasero_track_type_get_image_format (tmp_type);
			else
				format = BRASERO_IMAGE_FORMAT_NONE;

			brasero_track_type_free (tmp_type);

			BRASERO_BURN_LOG ("Temporary image type %i", format);
		}
		else {
			result = brasero_burn_session_can_burn (BRASERO_BURN_SESSION (self),
			                                        FALSE);
			format = brasero_burn_session_get_output_format (BRASERO_BURN_SESSION (self));
		}

		priv->CD_TEXT_modified = FALSE;
		if (!(format & (BRASERO_IMAGE_FORMAT_CDRDAO|BRASERO_IMAGE_FORMAT_CUE)))
			priv->CD_TEXT_modified = TRUE;
	}
	else {
		/* Don't use flags as they'll be adapted later. */
		priv->CD_TEXT_modified = FALSE;
		result = brasero_burn_session_can_burn (BRASERO_BURN_SESSION (self),
							FALSE);
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

	/* Special case for video projects */
	if (brasero_track_type_get_has_stream (source)
	&& BRASERO_STREAM_FORMAT_HAS_VIDEO (brasero_track_type_get_stream_format (source))) {
		/* Only set if it was not already set */
		if (brasero_burn_session_tag_lookup (BRASERO_BURN_SESSION (self), BRASERO_VCD_TYPE, NULL) != BRASERO_BURN_OK)
			brasero_burn_session_tag_add_int (BRASERO_BURN_SESSION (self),
							  BRASERO_VCD_TYPE,
							  BRASERO_SVCD);
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
brasero_session_cfg_session_loaded (BraseroTrackDataCfg *track,
				    BraseroMedium *medium,
				    gboolean is_loaded,
				    BraseroSessionCfg *session)
{
	if (is_loaded) {
		/* Set the correct medium and add the flag */
		brasero_burn_session_set_burner (BRASERO_BURN_SESSION (session),
						 brasero_medium_get_drive (medium));

		brasero_session_cfg_add_drive_properties_flags (session, BRASERO_BURN_FLAG_MERGE);
	}
	else
		brasero_session_cfg_rm_drive_properties_flags (session, BRASERO_BURN_FLAG_MERGE);
}

static void
brasero_session_cfg_track_added (BraseroBurnSession *session,
				 BraseroTrack *track)
{
	BraseroSessionCfgPrivate *priv;

	priv = BRASERO_SESSION_CFG_PRIVATE (session);
	if (priv->disabled)
		return;

	if (BRASERO_IS_TRACK_DATA_CFG (track))
		g_signal_connect (track,
				  "session-loaded",
				  G_CALLBACK (brasero_session_cfg_session_loaded),
				  session);

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
brasero_session_cfg_track_removed (BraseroBurnSession *session,
				   BraseroTrack *track,
				   guint former_position)
{
	BraseroSessionCfgPrivate *priv;

	priv = BRASERO_SESSION_CFG_PRIVATE (session);
	if (priv->disabled)
		return;

	/* Just in case */
	g_signal_handlers_disconnect_by_func (track,
					      brasero_session_cfg_session_loaded,
					      session);

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
brasero_session_cfg_track_changed (BraseroBurnSession *session,
				   BraseroTrack *track)
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
brasero_session_cfg_output_changed (BraseroBurnSession *session,
				    BraseroMedium *former)
{
	BraseroSessionCfgPrivate *priv;
	BraseroTrackType *type;

	priv = BRASERO_SESSION_CFG_PRIVATE (session);
	if (priv->disabled)
		return;

	/* Case for video project */
	type = brasero_track_type_new ();
	brasero_burn_session_get_input_type (session, type);

	if (brasero_track_type_get_has_stream (type)
	&&  BRASERO_STREAM_FORMAT_HAS_VIDEO (brasero_track_type_get_stream_format (type))) {
		BraseroMedia media;

		media = brasero_burn_session_get_dest_media (session);
		if (media & BRASERO_MEDIUM_DVD)
			brasero_burn_session_tag_add_int (session,
			                                  BRASERO_DVD_STREAM_FORMAT,
			                                  BRASERO_AUDIO_FORMAT_AC3);
		else if (media & BRASERO_MEDIUM_CD)
			brasero_burn_session_tag_add_int (session,
							  BRASERO_DVD_STREAM_FORMAT,
							  BRASERO_AUDIO_FORMAT_MP2);
		else {
			BraseroImageFormat format;

			format = brasero_burn_session_get_output_format (session);
			if (format == BRASERO_IMAGE_FORMAT_CUE)
				brasero_burn_session_tag_add_int (session,
								  BRASERO_DVD_STREAM_FORMAT,
								  BRASERO_AUDIO_FORMAT_MP2);
			else
				brasero_burn_session_tag_add_int (session,
								  BRASERO_DVD_STREAM_FORMAT,
								  BRASERO_AUDIO_FORMAT_AC3);
		}
	}

	brasero_track_type_free (type);

	brasero_session_cfg_save_drive_properties (BRASERO_SESSION_CFG (session),
						   former);

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

static void
brasero_session_cfg_flags_changed (BraseroBurnSession *session) 
{
	BraseroSessionCfgPrivate *priv;

	priv = BRASERO_SESSION_CFG_PRIVATE (session);
	if (priv->disabled)
		return;

	/* when we update the flags we don't want a
	 * whole series of "flags-changed" emitted.
	 * so make sure there is just one at the end */
	if (priv->inhibit_flag_sig)
		g_signal_stop_emission_by_name (session, "flags-changed");
}

/**
 * brasero_session_cfg_add_flags:
 * @cfg: a #BraseroSessionCfg
 * @flags: a #BraseroBurnFlag
 *
 * Adds all flags from @flags that are not supported.
 *
 **/

void
brasero_session_cfg_add_flags (BraseroSessionCfg *self,
			       BraseroBurnFlag flags)
{
	BraseroSessionCfgPrivate *priv;
	BraseroDrive *drive;

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
	drive = brasero_burn_session_get_burner (BRASERO_BURN_SESSION (self));
	if (drive && brasero_drive_get_medium (drive))
		brasero_session_cfg_save_drive_flags (self, brasero_drive_get_medium (drive));

	brasero_session_cfg_update (self,
				    FALSE,
				    FALSE);
}

/**
 * brasero_session_cfg_remove_flags:
 * @cfg: a #BraseroSessionCfg
 * @flags: a #BraseroBurnFlag
 *
 * Removes all flags that are not compulsory.
 *
 **/

void
brasero_session_cfg_remove_flags (BraseroSessionCfg *self,
				  BraseroBurnFlag flags)
{
	BraseroSessionCfgPrivate *priv;
	BraseroDrive *drive;

	priv = BRASERO_SESSION_CFG_PRIVATE (self);

	brasero_burn_session_remove_flag (BRASERO_BURN_SESSION (self), flags);

	/* For this case reset all flags as some flags could
	 * be made available after the removal of one flag
	 * Example: After the removal of MULTI, FAST_BLANK
	 * becomes available again for DVDRW sequential */
	brasero_session_cfg_set_drive_properties_default_flags (self);

	/* Always save flags */
	drive = brasero_burn_session_get_burner (BRASERO_BURN_SESSION (self));
	if (drive && brasero_drive_get_medium (drive))
		brasero_session_cfg_save_drive_flags (self, brasero_drive_get_medium (drive));

	brasero_session_cfg_update (self,
				    FALSE,
				    FALSE);
}

/**
 * brasero_session_cfg_is_supported:
 * @cfg: a #BraseroSessionCfg
 * @flag: a #BraseroBurnFlag
 *
 * Checks whether a particular flag is supported.
 *
 * Return value: a #gboolean. TRUE if it is supported;
 * FALSE otherwise.
 **/

gboolean
brasero_session_cfg_is_supported (BraseroSessionCfg *self,
				  BraseroBurnFlag flag)
{
	BraseroSessionCfgPrivate *priv;

	priv = BRASERO_SESSION_CFG_PRIVATE (self);
	return (priv->supported & flag) == flag;
}

/**
 * brasero_session_cfg_is_compulsory:
 * @cfg: a #BraseroSessionCfg
 * @flag: a #BraseroBurnFlag
 *
 * Checks whether a particular flag is compulsory.
 *
 * Return value: a #gboolean. TRUE if it is compulsory;
 * FALSE otherwise.
 **/

gboolean
brasero_session_cfg_is_compulsory (BraseroSessionCfg *self,
				   BraseroBurnFlag flag)
{
	BraseroSessionCfgPrivate *priv;

	priv = BRASERO_SESSION_CFG_PRIVATE (self);
	return (priv->compulsory & flag) == flag;
}

static void
brasero_session_cfg_init (BraseroSessionCfg *object)
{
	BraseroSessionCfgPrivate *priv;
	BraseroPluginManager *manager;

	priv = BRASERO_SESSION_CFG_PRIVATE (object);

	manager = brasero_plugin_manager_get_default ();
	g_signal_connect (manager,
	                  "caps-changed",
	                  G_CALLBACK (brasero_session_cfg_caps_changed),
	                  object);
}

static void
brasero_session_cfg_finalize (GObject *object)
{
	BraseroPluginManager *manager;
	BraseroSessionCfgPrivate *priv;
	BraseroDrive *drive;
	GSList *tracks;

	priv = BRASERO_SESSION_CFG_PRIVATE (object);

	drive = brasero_burn_session_get_burner (BRASERO_BURN_SESSION (object));
	if (drive && brasero_drive_get_medium (drive))
		brasero_session_cfg_save_drive_properties (BRASERO_SESSION_CFG (object),
							   brasero_drive_get_medium (drive));

	tracks = brasero_burn_session_get_tracks (BRASERO_BURN_SESSION (object));
	for (; tracks; tracks = tracks->next) {
		BraseroTrack *track;

		track = tracks->data;
		g_signal_handlers_disconnect_by_func (track,
						      brasero_session_cfg_session_loaded,
						      object);
	}

	manager = brasero_plugin_manager_get_default ();
	g_signal_handlers_disconnect_by_func (manager,
	                                      brasero_session_cfg_caps_changed,
	                                      object);

	G_OBJECT_CLASS (brasero_session_cfg_parent_class)->finalize (object);
}

static void
brasero_session_cfg_class_init (BraseroSessionCfgClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	BraseroBurnSessionClass *session_class = BRASERO_BURN_SESSION_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroSessionCfgPrivate));

	object_class->finalize = brasero_session_cfg_finalize;

	session_class->get_output_path = brasero_session_cfg_get_output_path;
	session_class->get_output_format = brasero_session_cfg_get_output_format;
	session_class->set_output_image = brasero_session_cfg_set_output_image;

	session_class->track_added = brasero_session_cfg_track_added;
	session_class->track_removed = brasero_session_cfg_track_removed;
	session_class->track_changed = brasero_session_cfg_track_changed;
	session_class->output_changed = brasero_session_cfg_output_changed;
	session_class->flags_changed = brasero_session_cfg_flags_changed;
	session_class->tag_changed = brasero_session_cfg_tag_changed;

	session_cfg_signals [WRONG_EXTENSION_SIGNAL] =
		g_signal_new ("wrong_extension",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_RUN_CLEANUP | G_SIGNAL_ACTION,
		              0,
		              NULL, NULL,
		              brasero_marshal_BOOLEAN__VOID,
		              G_TYPE_BOOLEAN,
			      0,
		              G_TYPE_NONE);
	session_cfg_signals [IS_VALID_SIGNAL] =
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

/**
 * brasero_session_cfg_new:
 *
 *  Creates a new #BraseroSessionCfg object.
 *
 * Return value: a #BraseroSessionCfg object.
 **/

BraseroSessionCfg *
brasero_session_cfg_new (void)
{
	return g_object_new (BRASERO_TYPE_SESSION_CFG, NULL);
}
