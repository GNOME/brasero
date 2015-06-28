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
#include <sys/resource.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include "brasero-volume.h"

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

	/* Do some caching to improve performances */
	BraseroImageFormat output_format;
	gchar *output;

	BraseroTrackType *source;
	goffset disc_size;
	goffset session_blocks;
	goffset session_size;

	BraseroSessionError is_valid;

	guint CD_TEXT_modified:1;
	guint configuring:1;
	guint disabled:1;

	guint output_msdos:1;
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
 * @session: a #BraseroSessionCfg
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
		if (!dot || strcmp (suffixes [0], dot)) {
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
		&& (!dot || strcmp (suffixes [1], dot))) {
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
		     && (!dot || strcmp (suffixes [2], dot))) {
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
		     && (!dot || strcmp (suffixes [3], dot))) {
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
	BraseroSessionCfgPrivate *priv;

	klass = BRASERO_BURN_SESSION_CLASS (brasero_session_cfg_parent_class);
	priv = BRASERO_SESSION_CFG_PRIVATE (session);

	result = klass->get_output_path (session,
					 image,
					 toc);
	if (result == BRASERO_BURN_OK)
		return result;

	if (priv->output_format == BRASERO_IMAGE_FORMAT_NONE)
		return BRASERO_BURN_ERR;

	/* Note: path and format are determined earlier in fact, in the function
	 * that check the free space on the hard drive. */
	path = g_strdup (priv->output);
	format = priv->output_format;

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
		g_free (priv->output);
		priv->output = NULL;
		return BRASERO_BURN_ERR;
	}

	return BRASERO_BURN_OK;
}

static BraseroImageFormat
brasero_session_cfg_get_output_format (BraseroBurnSession *session)
{
	BraseroBurnSessionClass *klass;
	BraseroSessionCfgPrivate *priv;
	BraseroImageFormat format;

	klass = BRASERO_BURN_SESSION_CLASS (brasero_session_cfg_parent_class);
	format = klass->get_output_format (session);

	if (format != BRASERO_IMAGE_FORMAT_NONE)
		return format;

	priv = BRASERO_SESSION_CFG_PRIVATE (session);

	if (priv->output_format)
		return priv->output_format;

	/* Cache the path for later use */
	priv->output_format = brasero_burn_session_get_default_output_format (session);
	return priv->output_format;
}

/**
 * brasero_session_cfg_get_error:
 * @session: a #BraseroSessionCfg
 *
 * This function returns the current status and if
 * autoconfiguration is/was successful.
 *
 * Return value: a #BraseroSessionError.
 **/

BraseroSessionError
brasero_session_cfg_get_error (BraseroSessionCfg *session)
{
	BraseroSessionCfgPrivate *priv;

	priv = BRASERO_SESSION_CFG_PRIVATE (session);

	if (priv->is_valid == BRASERO_SESSION_VALID
	&&  priv->CD_TEXT_modified)
		return BRASERO_SESSION_NO_CD_TEXT;

	return priv->is_valid;
}

/**
 * brasero_session_cfg_disable:
 * @session: a #BraseroSessionCfg
 *
 * This function disables autoconfiguration
 *
 **/

void
brasero_session_cfg_disable (BraseroSessionCfg *session)
{
	BraseroSessionCfgPrivate *priv;

	priv = BRASERO_SESSION_CFG_PRIVATE (session);
	priv->disabled = TRUE;
}

/**
 * brasero_session_cfg_enable:
 * @session: a #BraseroSessionCfg
 *
 * This function (re)-enables autoconfiguration
 *
 **/

void
brasero_session_cfg_enable (BraseroSessionCfg *session)
{
	BraseroSessionCfgPrivate *priv;

	priv = BRASERO_SESSION_CFG_PRIVATE (session);
	priv->disabled = FALSE;
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

#if 0
	
	/* NOTE: Stop setting DAO here except if it
	 * is declared compulsory (but that's handled
	 * somewhere else) or if it was explicity set.
	 * If we set it just  by  default when it's
	 * supported but not compulsory, MULTI
	 * becomes not supported anymore.
	 * For data the only way it'd be really useful
	 * is if we wanted to fit a selection on the disc.
	 * The problem here is that we don't know
	 * what the size of the final image is going
	 * to be.
	 * For images there are cases when after 
	 * writing an image the user may want to
	 * add some more data to it later. As in the
	 * case of data the only way this flag would
	 * be useful would be to help fit the image
	 * on the disc. But I doubt it would really
	 * help a lot.
	 * For audio we need it to write things like
	 * CD-TEXT but in this case the backend
	 * will return it as compulsory. */

	/* Another case when this flag is needed is
	 * for DVD-RW quick blanked as they only
	 * support DAO. But there again this should
	 * be covered by the backend that should
	 * return DAO as compulsory. */

	/* Leave the code as a reminder. */
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
			 * compulsory like MULTI. */
		}
	}
#endif
}

static void
brasero_session_cfg_set_drive_properties_flags (BraseroSessionCfg *self,
						BraseroBurnFlag flags)
{
	BraseroDrive *drive;
	BraseroBurnFlag flag;
	BraseroMedium *medium;
	BraseroBurnResult result;
	BraseroBurnFlag original_flags;
	BraseroSessionCfgPrivate *priv;

	priv = BRASERO_SESSION_CFG_PRIVATE (self);

	original_flags = brasero_burn_session_get_flags (BRASERO_BURN_SESSION (self));

	/* If the session is invalid no need to check the flags: just add them.
	 * The correct flags will be re-computed anyway when the session becomes
	 * valid again. */
	if (priv->is_valid != BRASERO_SESSION_VALID) {
		BRASERO_BURN_LOG ("Session currently not ready for flag computation: adding flags (will update later)");
		brasero_burn_session_set_flags (BRASERO_BURN_SESSION (self), flags);
		return;
	}

	BRASERO_BURN_LOG ("Resetting all flags");
	BRASERO_BURN_LOG_FLAGS (original_flags, "Current are");
	BRASERO_BURN_LOG_FLAGS (flags, "New should be");

	drive = brasero_burn_session_get_burner (BRASERO_BURN_SESSION (self));
	if (!drive) {
		BRASERO_BURN_LOG ("No drive");
		return;
	}

	medium = brasero_drive_get_medium (drive);
	if (!medium) {
		BRASERO_BURN_LOG ("No medium");
		return;
	}

	/* This prevents signals to be emitted while (re-) adding them one by one */
	g_object_freeze_notify (G_OBJECT (self));

	brasero_burn_session_set_flags (BRASERO_BURN_SESSION (self), BRASERO_BURN_FLAG_NONE);

	priv->supported = BRASERO_BURN_FLAG_NONE;
	priv->compulsory = BRASERO_BURN_FLAG_NONE;
	result = brasero_burn_session_get_burn_flags (BRASERO_BURN_SESSION (self),
						      &priv->supported,
						      &priv->compulsory);

	if (result != BRASERO_BURN_OK) {
		brasero_burn_session_set_flags (BRASERO_BURN_SESSION (self), original_flags | flags);
		g_object_thaw_notify (G_OBJECT (self));
		return;
	}

	for (flag = BRASERO_BURN_FLAG_EJECT; flag < BRASERO_BURN_FLAG_LAST; flag <<= 1) {
		/* see if this flag was originally set */
		if ((flags & flag) == 0)
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

	if (original_flags & BRASERO_BURN_FLAG_DAO
	&&  priv->supported & BRASERO_BURN_FLAG_DAO) {
		/* Only set DAO if it was explicitely requested */
		brasero_burn_session_add_flag (BRASERO_BURN_SESSION (self), BRASERO_BURN_FLAG_DAO);

		priv->supported = BRASERO_BURN_FLAG_NONE;
		priv->compulsory = BRASERO_BURN_FLAG_NONE;
		brasero_burn_session_get_burn_flags (BRASERO_BURN_SESSION (self),
						     &priv->supported,
						     &priv->compulsory);

		/* NOTE: after setting DAO, some flags may become
		 * compulsory like MULTI. */
	}

	brasero_session_cfg_set_drive_properties_default_flags (self);

	/* These are always supported and better be set. */
	brasero_burn_session_add_flag (BRASERO_BURN_SESSION (self),
	                               BRASERO_BURN_FLAG_CHECK_SIZE|
	                               BRASERO_BURN_FLAG_NOGRACE);

	/* This one is only supported when we are
	 * burning to a disc or copying a disc but it
	 * would better be set. */
	if (priv->supported & BRASERO_BURN_FLAG_EJECT)
		brasero_burn_session_add_flag (BRASERO_BURN_SESSION (self),
		                               BRASERO_BURN_FLAG_EJECT);

	/* allow notify::flags signal again */
	g_object_thaw_notify (G_OBJECT (self));
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
brasero_session_cfg_check_drive_settings (BraseroSessionCfg *self)
{
	BraseroBurnFlag flags;

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
brasero_session_cfg_check_volume_size (BraseroSessionCfg *self)
{
	struct rlimit limit;
	BraseroSessionCfgPrivate *priv;

	priv = BRASERO_SESSION_CFG_PRIVATE (self);
	if (!priv->disc_size) {
		GFileInfo *info;
		gchar *directory;
		GFile *file = NULL;
		const gchar *filesystem;

		/* Cache the path for later use */
		if (priv->output_format == BRASERO_IMAGE_FORMAT_NONE)
			priv->output_format = brasero_burn_session_get_output_format (BRASERO_BURN_SESSION (self));

		if (!priv->output) {
			gchar *name = NULL;

			/* If we try to copy a volume get (and use) its name */
			if (brasero_track_type_get_has_medium (priv->source)) {
				BraseroMedium *medium;

				medium = brasero_burn_session_get_src_medium (BRASERO_BURN_SESSION (self));
				if (medium)
					name = brasero_volume_get_name (BRASERO_VOLUME (medium));
			}

			priv->output = brasero_image_format_get_default_path (priv->output_format, name);
			g_free (name);
		}

		directory = g_path_get_dirname (priv->output);
		file = g_file_new_for_path (directory);
		g_free (directory);

		if (file == NULL)
			goto error;

		/* Check permissions first */
		info = g_file_query_info (file,
					  G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE,
					  G_FILE_QUERY_INFO_NONE,
					  NULL,
					  NULL);
		if (!info) {
			g_object_unref (file);
			goto error;
		}

		if (!g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE)) {
			g_object_unref (info);
			g_object_unref (file);
			goto error;
		}
		g_object_unref (info);

		/* Now size left */
		info = g_file_query_filesystem_info (file,
						     G_FILE_ATTRIBUTE_FILESYSTEM_FREE ","
						     G_FILE_ATTRIBUTE_FILESYSTEM_TYPE,
						     NULL,
						     NULL);
		g_object_unref (file);

		if (!info)
			goto error;

		/* Now check the filesystem type: the problem here is that some
		 * filesystems have a maximum file size limit of 4 GiB and more than
		 * often we need a temporary file size of 4 GiB or more. */
		filesystem = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_FILESYSTEM_TYPE);
		if (!g_strcmp0 (filesystem, "msdos"))
			priv->output_msdos = TRUE;
		else
			priv->output_msdos = FALSE;

		priv->disc_size = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE);
		g_object_unref (info);
	}

	BRASERO_BURN_LOG ("Session size %" G_GOFFSET_FORMAT "/Hard drive size %" G_GOFFSET_FORMAT,
			  priv->session_size,
			  priv->disc_size);

	if (priv->output_msdos && priv->session_size >= 2147483648ULL)
		goto error;

	if (priv->session_size > priv->disc_size)
		goto error;

	/* Last but not least, use getrlimit () to check that we are allowed to
	 * write a file of such length and that quotas won't get in our way */
	if (getrlimit (RLIMIT_FSIZE, &limit))
		goto error;

	if (limit.rlim_cur < priv->session_size)
		goto error;

	priv->is_valid = BRASERO_SESSION_VALID;
	return BRASERO_SESSION_VALID;

error:

	priv->is_valid = BRASERO_SESSION_INSUFFICIENT_SPACE;
	return BRASERO_SESSION_INSUFFICIENT_SPACE;
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
	goffset max_sectors;

	priv = BRASERO_SESSION_CFG_PRIVATE (self);

	/* Get the session size if need be */
	if (!priv->session_blocks) {
		if (brasero_burn_session_tag_lookup (BRASERO_BURN_SESSION (self),
						     BRASERO_DATA_TRACK_SIZE_TAG,
						     &value) == BRASERO_BURN_OK) {
			priv->session_blocks = g_value_get_int64 (value);
			priv->session_size = priv->session_blocks * 2048;
		}
		else if (brasero_burn_session_tag_lookup (BRASERO_BURN_SESSION (self),
							  BRASERO_STREAM_TRACK_SIZE_TAG,
							  &value) == BRASERO_BURN_OK) {
			priv->session_blocks = g_value_get_int64 (value);
			priv->session_size = priv->session_blocks * 2352;
		}
		else
			brasero_burn_session_get_size (BRASERO_BURN_SESSION (self),
						       &priv->session_blocks,
						       &priv->session_size);
	}

	/* Get the disc and its size if need be */
	burner = brasero_burn_session_get_burner (BRASERO_BURN_SESSION (self));
	if (!burner) {
		priv->is_valid = BRASERO_SESSION_NO_OUTPUT;
		return BRASERO_SESSION_NO_OUTPUT;
	}

	if (brasero_drive_is_fake (burner))
		return brasero_session_cfg_check_volume_size (self);

	medium = brasero_drive_get_medium (burner);
	if (!medium) {
		priv->is_valid = BRASERO_SESSION_NO_OUTPUT;
		return BRASERO_SESSION_NO_OUTPUT;
	}

	/* Get both sizes if need be */
	if (!priv->disc_size) {
		priv->disc_size = brasero_burn_session_get_available_medium_space (BRASERO_BURN_SESSION (self));
		if (priv->disc_size < 0)
			priv->disc_size = 0;
	}

	BRASERO_BURN_LOG ("Session size %" G_GOFFSET_FORMAT "/Disc size %" G_GOFFSET_FORMAT,
			  priv->session_blocks,
			  priv->disc_size);

	if (priv->session_blocks < priv->disc_size) {
		priv->is_valid = BRASERO_SESSION_VALID;
		return BRASERO_SESSION_VALID;
	}

	/* Overburn is only for CDs */
	if ((brasero_medium_get_status (medium) & BRASERO_MEDIUM_CD) == 0) {
		priv->is_valid = BRASERO_SESSION_INSUFFICIENT_SPACE;
		return BRASERO_SESSION_INSUFFICIENT_SPACE;
	}

	/* The idea would be to test write the disc with cdrecord from /dev/null
	 * until there is an error and see how much we were able to write. So,
	 * when we propose overburning to the user, we could ask if he wants
	 * us to determine how much data can be written to a particular disc
	 * provided he has chosen a real disc. */
	max_sectors = priv->disc_size * 103 / 100;
	if (max_sectors < priv->session_blocks) {
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

static gboolean
brasero_session_cfg_can_update (BraseroSessionCfg *self)
{
	BraseroSessionCfgPrivate *priv;
	BraseroBurnResult result;
	BraseroStatus *status;

	priv = BRASERO_SESSION_CFG_PRIVATE (self);

	if (priv->disabled)
		return FALSE;

	if (priv->configuring)
		return FALSE;

	/* Make sure the session is ready */
	status = brasero_status_new ();
	result = brasero_burn_session_get_status (BRASERO_BURN_SESSION (self), status);
	if (result == BRASERO_BURN_NOT_READY || result == BRASERO_BURN_RUNNING) {
		g_object_unref (status);

		priv->is_valid = BRASERO_SESSION_NOT_READY;
		g_signal_emit (self,
			       session_cfg_signals [IS_VALID_SIGNAL],
			       0);
		return FALSE;
	}

	if (result == BRASERO_BURN_ERR) {
		GError *error;

		error = brasero_status_get_error (status);
		if (error) {
			if (error->code == BRASERO_BURN_ERROR_EMPTY) {
				g_object_unref (status);
				g_error_free (error);

				priv->is_valid = BRASERO_SESSION_EMPTY;
				g_signal_emit (self,
					       session_cfg_signals [IS_VALID_SIGNAL],
					       0);
				return FALSE;
			}

			g_error_free (error);
		}
	}
	g_object_unref (status);
	return TRUE;
}

static void
brasero_session_cfg_update (BraseroSessionCfg *self)
{
	BraseroSessionCfgPrivate *priv;
	BraseroBurnResult result;
	BraseroDrive *burner;

	priv = BRASERO_SESSION_CFG_PRIVATE (self);

	/* Make sure there is a source */
	if (priv->source) {
		brasero_track_type_free (priv->source);
		priv->source = NULL;
	}

	priv->source = brasero_track_type_new ();
	brasero_burn_session_get_input_type (BRASERO_BURN_SESSION (self), priv->source);

	if (brasero_track_type_is_empty (priv->source)) {
		priv->is_valid = BRASERO_SESSION_EMPTY;
		g_signal_emit (self,
			       session_cfg_signals [IS_VALID_SIGNAL],
			       0);
		return;
	}

	/* it can be empty with just an empty track */
	if (brasero_track_type_get_has_medium (priv->source)
	&&  brasero_track_type_get_medium_type (priv->source) == BRASERO_MEDIUM_NONE) {
		priv->is_valid = BRASERO_SESSION_NO_INPUT_MEDIUM;
		g_signal_emit (self,
			       session_cfg_signals [IS_VALID_SIGNAL],
			       0);
		return;
	}

	if (brasero_track_type_get_has_image (priv->source)
	&&  brasero_track_type_get_image_format (priv->source) == BRASERO_IMAGE_FORMAT_NONE) {
		gchar *uri;
		GSList *tracks;

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

	/* make sure there is an output set */
	burner = brasero_burn_session_get_burner (BRASERO_BURN_SESSION (self));
	if (!burner) {
		priv->is_valid = BRASERO_SESSION_NO_OUTPUT;
		g_signal_emit (self,
			       session_cfg_signals [IS_VALID_SIGNAL],
			       0);
		return;
	}

	/* In case the output was an image remove the path cache. It will be
	 * re-computed on demand. */
	if (priv->output) {
		g_free (priv->output);
		priv->output = NULL;
	}

	if (priv->output_format)
		priv->output_format = BRASERO_IMAGE_FORMAT_NONE;

	/* Check that current input and output work */
	if (brasero_track_type_get_has_stream (priv->source)) {
		if (priv->CD_TEXT_modified) {
			/* Try to redo what we undid (after all a new plugin
			 * could have been activated in the mean time ...) and
			 * see what happens */
			brasero_track_type_set_stream_format (priv->source,
							      BRASERO_METADATA_INFO|
							      brasero_track_type_get_stream_format (priv->source));
			result = brasero_burn_session_input_supported (BRASERO_BURN_SESSION (self), priv->source, FALSE);
			if (result == BRASERO_BURN_OK) {
				priv->CD_TEXT_modified = FALSE;

				priv->configuring = TRUE;
				brasero_session_cfg_set_tracks_audio_format (BRASERO_BURN_SESSION (self),
									     brasero_track_type_get_stream_format (priv->source));
				priv->configuring = FALSE;
			}
			else {
				/* No, nothing's changed */
				brasero_track_type_set_stream_format (priv->source,
								      (~BRASERO_METADATA_INFO) &
								      brasero_track_type_get_stream_format (priv->source));
				result = brasero_burn_session_input_supported (BRASERO_BURN_SESSION (self), priv->source, FALSE);
			}
		}
		else {
			result = brasero_burn_session_can_burn (BRASERO_BURN_SESSION (self), FALSE);

			if (result != BRASERO_BURN_OK
			&& (brasero_track_type_get_stream_format (priv->source) & BRASERO_METADATA_INFO)) {
				/* Another special case in case some burning backends 
				 * don't support CD-TEXT for audio (libburn). If no
				 * other backend is available remove CD-TEXT option but
				 * tell user... */
				brasero_track_type_set_stream_format (priv->source,
								      (~BRASERO_METADATA_INFO) &
								      brasero_track_type_get_stream_format (priv->source));

				result = brasero_burn_session_input_supported (BRASERO_BURN_SESSION (self), priv->source, FALSE);

				BRASERO_BURN_LOG ("Tested support without Metadata information (result %d)", result);
				if (result == BRASERO_BURN_OK) {
					priv->CD_TEXT_modified = TRUE;

					priv->configuring = TRUE;
					brasero_session_cfg_set_tracks_audio_format (BRASERO_BURN_SESSION (self),
										     brasero_track_type_get_has_stream (priv->source));
					priv->configuring = FALSE;
				}
			}
		}
	}
	else if (brasero_track_type_get_has_medium (priv->source)
	&&  (brasero_track_type_get_medium_type (priv->source) & BRASERO_MEDIUM_HAS_AUDIO)) {
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

			/* NOTE: this is the same as brasero_burn_session_supported () */
			result = brasero_burn_session_get_tmp_image_type_same_src_dest (BRASERO_BURN_SESSION (self), tmp_type);
			if (result == BRASERO_BURN_OK) {
				if (brasero_track_type_get_has_image (tmp_type)) {
					format = brasero_track_type_get_image_format (tmp_type);
					priv->CD_TEXT_modified = (format & (BRASERO_IMAGE_FORMAT_CDRDAO|BRASERO_IMAGE_FORMAT_CUE)) == 0;
				}
				else if (brasero_track_type_get_has_stream (tmp_type)) {
					/* FIXME: for the moment
					 * we consider that this
					 * type will always allow
					 * to copy CD-TEXT */
					priv->CD_TEXT_modified = FALSE;
				}
				else
					priv->CD_TEXT_modified = TRUE;
			}
			else
				priv->CD_TEXT_modified = TRUE;

			brasero_track_type_free (tmp_type);

			BRASERO_BURN_LOG ("Temporary image type %i", format);
		}
		else {
			result = brasero_burn_session_can_burn (BRASERO_BURN_SESSION (self), FALSE);
			format = brasero_burn_session_get_output_format (BRASERO_BURN_SESSION (self));
			priv->CD_TEXT_modified = (format & (BRASERO_IMAGE_FORMAT_CDRDAO|BRASERO_IMAGE_FORMAT_CUE)) == 0;
		}
	}
	else {
		/* Don't use flags as they'll be adapted later. */
		priv->CD_TEXT_modified = FALSE;
		result = brasero_burn_session_can_burn (BRASERO_BURN_SESSION (self), FALSE);
	}

	if (result != BRASERO_BURN_OK) {
		if (brasero_track_type_get_has_medium (priv->source)
		&& (brasero_track_type_get_medium_type (priv->source) & BRASERO_MEDIUM_PROTECTED)
		&&  brasero_burn_library_input_supported (priv->source) != BRASERO_BURN_OK) {
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

		return;
	}

	/* Special case for video projects */
	if (brasero_track_type_get_has_stream (priv->source)
	&&  BRASERO_STREAM_FORMAT_HAS_VIDEO (brasero_track_type_get_stream_format (priv->source))) {
		/* Only set if it was not already set */
		if (brasero_burn_session_tag_lookup (BRASERO_BURN_SESSION (self), BRASERO_VCD_TYPE, NULL) != BRASERO_BURN_OK)
			brasero_burn_session_tag_add_int (BRASERO_BURN_SESSION (self),
							  BRASERO_VCD_TYPE,
							  BRASERO_SVCD);
	}

	/* Configure flags */
	priv->configuring = TRUE;

	if (brasero_drive_is_fake (burner))
		/* Remove some impossible flags */
		brasero_burn_session_remove_flag (BRASERO_BURN_SESSION (self),
						  BRASERO_BURN_FLAG_DUMMY|
						  BRASERO_BURN_FLAG_NO_TMP_FILES);

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
	BraseroBurnFlag session_flags;
	BraseroSessionCfgPrivate *priv;

	priv = BRASERO_SESSION_CFG_PRIVATE (session);
	if (priv->disabled)
		return;
	
	priv->session_blocks = 0;
	priv->session_size = 0;

	session_flags = brasero_burn_session_get_flags (BRASERO_BURN_SESSION (session));
	if (is_loaded) {
		/* Set the correct medium and add the flag */
		brasero_burn_session_set_burner (BRASERO_BURN_SESSION (session),
						 brasero_medium_get_drive (medium));

		if ((session_flags & BRASERO_BURN_FLAG_MERGE) == 0)
			brasero_session_cfg_add_drive_properties_flags (session, BRASERO_BURN_FLAG_MERGE);
	}
	else if ((session_flags & BRASERO_BURN_FLAG_MERGE) != 0)
		brasero_session_cfg_rm_drive_properties_flags (session, BRASERO_BURN_FLAG_MERGE);
}

static void
brasero_session_cfg_track_added (BraseroBurnSession *session,
				 BraseroTrack *track)
{
	BraseroSessionCfgPrivate *priv;

	if (!brasero_session_cfg_can_update (BRASERO_SESSION_CFG (session)))
		return;

	priv = BRASERO_SESSION_CFG_PRIVATE (session);
	priv->session_blocks = 0;
	priv->session_size = 0;

	if (BRASERO_IS_TRACK_DATA_CFG (track))
		g_signal_connect (track,
				  "session-loaded",
				  G_CALLBACK (brasero_session_cfg_session_loaded),
				  session);

	/* A track was added: 
	 * - check if all flags are supported
	 * - check available formats for path
	 * - set one path */
	brasero_session_cfg_update (BRASERO_SESSION_CFG (session));
	brasero_session_cfg_check_drive_settings (BRASERO_SESSION_CFG (session));
}

static void
brasero_session_cfg_track_removed (BraseroBurnSession *session,
				   BraseroTrack *track,
				   guint former_position)
{
	BraseroSessionCfgPrivate *priv;

	if (!brasero_session_cfg_can_update (BRASERO_SESSION_CFG (session)))
		return;

	priv = BRASERO_SESSION_CFG_PRIVATE (session);
	priv->session_blocks = 0;
	priv->session_size = 0;

	/* Just in case */
	g_signal_handlers_disconnect_by_func (track,
					      brasero_session_cfg_session_loaded,
					      session);

	/* If there were several tracks and at least one remained there is no
	 * use checking flags since the source type has not changed anyway.
	 * If there is no more track, there is no use checking flags anyway. */
	brasero_session_cfg_update (BRASERO_SESSION_CFG (session));
}

static void
brasero_session_cfg_track_changed (BraseroBurnSession *session,
				   BraseroTrack *track)
{
	BraseroSessionCfgPrivate *priv;
	BraseroTrackType *current;

	if (!brasero_session_cfg_can_update (BRASERO_SESSION_CFG (session)))
		return;

	priv = BRASERO_SESSION_CFG_PRIVATE (session);
	priv->session_blocks = 0;
	priv->session_size = 0;

	current = brasero_track_type_new ();
	brasero_burn_session_get_input_type (session, current);
	if (brasero_track_type_equal (current, priv->source)) {
		/* This is a shortcut if the source type has not changed */
		brasero_track_type_free (current);
		brasero_session_cfg_check_size (BRASERO_SESSION_CFG (session));
		g_signal_emit (session,
			       session_cfg_signals [IS_VALID_SIGNAL],
			       0);
 		return;
	}
	brasero_track_type_free (current);

	/* when that happens it's mostly because a medium source changed, or
	 * a new image was set. 
	 * - check if all flags are supported
	 * - check available formats for path
	 * - set one path if need be */
	brasero_session_cfg_update (BRASERO_SESSION_CFG (session));
	brasero_session_cfg_check_drive_settings (BRASERO_SESSION_CFG (session));
}

static void
brasero_session_cfg_output_changed (BraseroBurnSession *session,
				    BraseroMedium *former)
{
	BraseroSessionCfgPrivate *priv;

	if (!brasero_session_cfg_can_update (BRASERO_SESSION_CFG (session)))
		return;

	priv = BRASERO_SESSION_CFG_PRIVATE (session);
	priv->disc_size = 0;

	/* Case for video project */
	if (priv->source
	&&  brasero_track_type_get_has_stream (priv->source)
	&&  BRASERO_STREAM_FORMAT_HAS_VIDEO (brasero_track_type_get_stream_format (priv->source))) {
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

	/* In this case need to :
	 * - check if all flags are supported
	 * - for images, set a path if it wasn't already set */
	brasero_session_cfg_update (BRASERO_SESSION_CFG (session));
	brasero_session_cfg_check_drive_settings (BRASERO_SESSION_CFG (session));
}

static void
brasero_session_cfg_caps_changed (BraseroPluginManager *manager,
				  BraseroSessionCfg *self)
{
	BraseroSessionCfgPrivate *priv;

	if (!brasero_session_cfg_can_update (self))
		return;
 
	priv = BRASERO_SESSION_CFG_PRIVATE (self);
	priv->disc_size = 0;
	priv->session_blocks = 0;
	priv->session_size = 0;

	/* In this case we need to check if:
	 * - flags are supported or not supported anymore
	 * - image types as input/output are supported
	 * - if the current set of input/output still works */
	brasero_session_cfg_update (self);
	brasero_session_cfg_check_drive_settings (self);
}

/**
 * brasero_session_cfg_add_flags:
 * @session: a #BraseroSessionCfg
 * @flags: a #BraseroBurnFlag
 *
 * Adds all flags from @flags that are supported.
 *
 **/

void
brasero_session_cfg_add_flags (BraseroSessionCfg *session,
			       BraseroBurnFlag flags)
{
	BraseroSessionCfgPrivate *priv;

	priv = BRASERO_SESSION_CFG_PRIVATE (session);

	if ((priv->supported & flags) != flags)
		return;

	if ((brasero_burn_session_get_flags (BRASERO_BURN_SESSION (session)) & flags) == flags)
		return;

	brasero_session_cfg_add_drive_properties_flags (session, flags);

	if (brasero_session_cfg_can_update (session))
		brasero_session_cfg_update (session);
}

/**
 * brasero_session_cfg_remove_flags:
 * @session: a #BraseroSessionCfg
 * @flags: a #BraseroBurnFlag
 *
 * Removes all flags that are not compulsory.
 *
 **/

void
brasero_session_cfg_remove_flags (BraseroSessionCfg *session,
				  BraseroBurnFlag flags)
{
	brasero_burn_session_remove_flag (BRASERO_BURN_SESSION (session), flags);

	/* For this case reset all flags as some flags could
	 * be made available after the removal of one flag
	 * Example: After the removal of MULTI, FAST_BLANK
	 * becomes available again for DVDRW sequential */
	brasero_session_cfg_set_drive_properties_default_flags (session);

	if (brasero_session_cfg_can_update (session))
		brasero_session_cfg_update (session);
}

/**
 * brasero_session_cfg_is_supported:
 * @session: a #BraseroSessionCfg
 * @flag: a #BraseroBurnFlag
 *
 * Checks whether a particular flag is supported.
 *
 * Return value: a #gboolean. TRUE if it is supported;
 * FALSE otherwise.
 **/

gboolean
brasero_session_cfg_is_supported (BraseroSessionCfg *session,
				  BraseroBurnFlag flag)
{
	BraseroSessionCfgPrivate *priv;

	priv = BRASERO_SESSION_CFG_PRIVATE (session);
	return (priv->supported & flag) == flag;
}

/**
 * brasero_session_cfg_is_compulsory:
 * @session: a #BraseroSessionCfg
 * @flag: a #BraseroBurnFlag
 *
 * Checks whether a particular flag is compulsory.
 *
 * Return value: a #gboolean. TRUE if it is compulsory;
 * FALSE otherwise.
 **/

gboolean
brasero_session_cfg_is_compulsory (BraseroSessionCfg *session,
				   BraseroBurnFlag flag)
{
	BraseroSessionCfgPrivate *priv;

	priv = BRASERO_SESSION_CFG_PRIVATE (session);
	return (priv->compulsory & flag) == flag;
}

static void
brasero_session_cfg_init (BraseroSessionCfg *object)
{
	BraseroSessionCfgPrivate *priv;
	BraseroPluginManager *manager;

	priv = BRASERO_SESSION_CFG_PRIVATE (object);

	priv->is_valid = BRASERO_SESSION_EMPTY;
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
	GSList *tracks;

	priv = BRASERO_SESSION_CFG_PRIVATE (object);

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

	if (priv->source) {
		brasero_track_type_free (priv->source);
		priv->source = NULL;
	}

	if (priv->output) {
		g_free (priv->output);
		priv->output = NULL;
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

	session_class->get_output_path = brasero_session_cfg_get_output_path;
	session_class->get_output_format = brasero_session_cfg_get_output_format;
	session_class->set_output_image = brasero_session_cfg_set_output_image;

	session_class->track_added = brasero_session_cfg_track_added;
	session_class->track_removed = brasero_session_cfg_track_removed;
	session_class->track_changed = brasero_session_cfg_track_changed;
	session_class->output_changed = brasero_session_cfg_output_changed;
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
