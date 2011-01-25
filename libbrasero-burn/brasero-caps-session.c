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
#include <glib/gi18n.h>

#include "burn-caps.h"
#include "burn-debug.h"
#include "brasero-plugin.h"
#include "brasero-plugin-private.h"
#include "brasero-plugin-information.h"
#include "brasero-session-helper.h"

#define BRASERO_BURN_CAPS_SHOULD_BLANK(media_MACRO, flags_MACRO)		\
	((media_MACRO & BRASERO_MEDIUM_UNFORMATTED) ||				\
	((media_MACRO & (BRASERO_MEDIUM_HAS_AUDIO|BRASERO_MEDIUM_HAS_DATA)) &&	\
	 (flags_MACRO & (BRASERO_BURN_FLAG_MERGE|BRASERO_BURN_FLAG_APPEND)) == FALSE))

#define BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG_RES(session)			\
{										\
	brasero_burn_session_log (session, "Unsupported type of task operation"); \
	BRASERO_BURN_LOG ("Unsupported type of task operation");		\
	return BRASERO_BURN_NOT_SUPPORTED;					\
}

static BraseroBurnResult
brasero_burn_caps_get_blanking_flags_real (BraseroBurnCaps *caps,
                                           gboolean ignore_errors,
					   BraseroMedia media,
					   BraseroBurnFlag session_flags,
					   BraseroBurnFlag *supported,
					   BraseroBurnFlag *compulsory)
{
	GSList *iter;
	gboolean supported_media;
	BraseroBurnFlag supported_flags = BRASERO_BURN_FLAG_NONE;
	BraseroBurnFlag compulsory_flags = BRASERO_BURN_FLAG_ALL;

	BRASERO_BURN_LOG_DISC_TYPE (media, "Getting blanking flags for");
	if (media == BRASERO_MEDIUM_NONE) {
		BRASERO_BURN_LOG ("Blanking not possible: no media");
		if (supported)
			*supported = BRASERO_BURN_FLAG_NONE;
		if (compulsory)
			*compulsory = BRASERO_BURN_FLAG_NONE;
		return BRASERO_BURN_NOT_SUPPORTED;
	}

	supported_media = FALSE;
	for (iter = caps->priv->caps_list; iter; iter = iter->next) {
		BraseroMedia caps_media;
		BraseroCaps *caps;
		GSList *links;

		caps = iter->data;
		if (!brasero_track_type_get_has_medium (&caps->type))
			continue;

		caps_media = brasero_track_type_get_medium_type (&caps->type);
		if ((media & caps_media) != media)
			continue;

		for (links = caps->links; links; links = links->next) {
			GSList *plugins;
			BraseroCapsLink *link;

			link = links->data;

			if (link->caps != NULL)
				continue;

			supported_media = TRUE;
			/* don't need the plugins to be sorted since we go
			 * through all the plugin list to get all blanking flags
			 * available. */
			for (plugins = link->plugins; plugins; plugins = plugins->next) {
				BraseroPlugin *plugin;
				BraseroBurnFlag supported_plugin;
				BraseroBurnFlag compulsory_plugin;

				plugin = plugins->data;
				if (!brasero_plugin_get_active (plugin, ignore_errors))
					continue;

				if (!brasero_plugin_get_blank_flags (plugin,
								     media,
								     session_flags,
								     &supported_plugin,
								     &compulsory_plugin))
					continue;

				supported_flags |= supported_plugin;
				compulsory_flags &= compulsory_plugin;
			}
		}
	}

	if (!supported_media) {
		BRASERO_BURN_LOG ("media blanking not supported");
		return BRASERO_BURN_NOT_SUPPORTED;
	}

	/* This is a special case that is in MMC specs:
	 * DVD-RW sequential must be fully blanked
	 * if we really want multisession support. */
	if (BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDRW)
	&& (session_flags & BRASERO_BURN_FLAG_MULTI)) {
		if (compulsory_flags & BRASERO_BURN_FLAG_FAST_BLANK) {
			BRASERO_BURN_LOG ("fast media blanking only supported but multisession required for DVDRW");
			return BRASERO_BURN_NOT_SUPPORTED;
		}

		supported_flags &= ~BRASERO_BURN_FLAG_FAST_BLANK;

		BRASERO_BURN_LOG ("removed fast blank for a DVDRW with multisession");
	}

	if (supported)
		*supported = supported_flags;
	if (compulsory)
		*compulsory = compulsory_flags;

	return BRASERO_BURN_OK;
}

/**
 * brasero_burn_session_get_blank_flags:
 * @session: a #BraseroBurnSession
 * @supported: a #BraseroBurnFlag
 * @compulsory: a #BraseroBurnFlag
 *
 * Given the various parameters stored in @session,
 * stores in @supported and @compulsory, the flags
 * that can be used (@supported) and must be used
 * (@compulsory) when blanking the medium in the
 * #BraseroDrive (set with brasero_burn_session_set_burner ()).
 *
 * Return value: a #BraseroBurnResult.
 * BRASERO_BURN_OK if the retrieval was successful.
 * BRASERO_BURN_ERR otherwise.
 **/

BraseroBurnResult
brasero_burn_session_get_blank_flags (BraseroBurnSession *session,
				      BraseroBurnFlag *supported,
				      BraseroBurnFlag *compulsory)
{
	BraseroMedia media;
	BraseroBurnCaps *self;
	BraseroBurnResult result;
	BraseroBurnFlag session_flags;

	media = brasero_burn_session_get_dest_media (session);
	BRASERO_BURN_LOG_DISC_TYPE (media, "Getting blanking flags for");

	if (media == BRASERO_MEDIUM_NONE) {
		BRASERO_BURN_LOG ("Blanking not possible: no media");
		if (supported)
			*supported = BRASERO_BURN_FLAG_NONE;
		if (compulsory)
			*compulsory = BRASERO_BURN_FLAG_NONE;
		return BRASERO_BURN_NOT_SUPPORTED;
	}

	session_flags = brasero_burn_session_get_flags (session);

	self = brasero_burn_caps_get_default ();
	result = brasero_burn_caps_get_blanking_flags_real (self,
	                                                    brasero_burn_session_get_strict_support (session) == FALSE,
							    media,
							    session_flags,
							    supported,
							    compulsory);
	g_object_unref (self);

	return result;
}

static BraseroBurnResult
brasero_burn_caps_can_blank_real (BraseroBurnCaps *self,
                                  gboolean ignore_plugin_errors,
                                  BraseroMedia media,
				  BraseroBurnFlag flags)
{
	GSList *iter;

	BRASERO_BURN_LOG_DISC_TYPE (media, "Testing blanking caps for");
	if (media == BRASERO_MEDIUM_NONE) {
		BRASERO_BURN_LOG ("no media => no blanking possible");
		return BRASERO_BURN_NOT_SUPPORTED;
	}

	/* This is a special case from MMC: DVD-RW sequential
	 * can only be multisession is they were fully blanked
	 * so if there are the two flags, abort. */
	if (BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDRW)
	&&  (flags & BRASERO_BURN_FLAG_MULTI)
	&&  (flags & BRASERO_BURN_FLAG_FAST_BLANK)) {
		BRASERO_BURN_LOG ("fast media blanking only supported but multisession required for DVD-RW");
		return BRASERO_BURN_NOT_SUPPORTED;
	}

	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		BraseroCaps *caps;
		GSList *links;

		caps = iter->data;
		if (caps->type.type != BRASERO_TRACK_TYPE_DISC)
			continue;

		if ((media & brasero_track_type_get_medium_type (&caps->type)) != media)
			continue;

		BRASERO_BURN_LOG_TYPE (&caps->type, "Searching links for caps");

		for (links = caps->links; links; links = links->next) {
			GSList *plugins;
			BraseroCapsLink *link;

			link = links->data;

			if (link->caps != NULL)
				continue;

			BRASERO_BURN_LOG ("Searching plugins");

			/* Go through all plugins for the link and stop if we 
			 * find at least one active plugin that accepts the
			 * flags. No need for plugins to be sorted */
			for (plugins = link->plugins; plugins; plugins = plugins->next) {
				BraseroPlugin *plugin;

				plugin = plugins->data;
				if (!brasero_plugin_get_active (plugin, ignore_plugin_errors))
					continue;

				if (brasero_plugin_check_blank_flags (plugin, media, flags)) {
					BRASERO_BURN_LOG_DISC_TYPE (media, "Can blank");
					return BRASERO_BURN_OK;
				}
			}
		}
	}

	BRASERO_BURN_LOG_DISC_TYPE (media, "No blanking capabilities for");

	return BRASERO_BURN_NOT_SUPPORTED;
}

/**
 * brasero_burn_session_can_blank:
 * @session: a #BraseroBurnSession
 *
 * Given the various parameters stored in @session, this
 * function checks whether the medium in the
 * #BraseroDrive (set with brasero_burn_session_set_burner ())
 * can be blanked.
 *
 * Return value: a #BraseroBurnResult.
 * BRASERO_BURN_OK if it is possible.
 * BRASERO_BURN_ERR otherwise.
 **/

BraseroBurnResult
brasero_burn_session_can_blank (BraseroBurnSession *session)
{
	BraseroMedia media;
	BraseroBurnFlag flags;
	BraseroBurnCaps *self;
	BraseroBurnResult result;

	self = brasero_burn_caps_get_default ();

	media = brasero_burn_session_get_dest_media (session);
	BRASERO_BURN_LOG_DISC_TYPE (media, "Testing blanking caps for");

	if (media == BRASERO_MEDIUM_NONE) {
		BRASERO_BURN_LOG ("no media => no blanking possible");
		g_object_unref (self);
		return BRASERO_BURN_NOT_SUPPORTED;
	}

	flags = brasero_burn_session_get_flags (session);
	result = brasero_burn_caps_can_blank_real (self,
	                                           brasero_burn_session_get_strict_support (session) == FALSE,
	                                           media,
	                                           flags);
	g_object_unref (self);

	return result;
}

static void
brasero_caps_link_get_record_flags (BraseroCapsLink *link,
                                    gboolean ignore_plugin_errors,
				    BraseroMedia media,
				    BraseroBurnFlag session_flags,
				    BraseroBurnFlag *supported,
				    BraseroBurnFlag *compulsory_retval)
{
	GSList *iter;
	BraseroBurnFlag compulsory;

	compulsory = BRASERO_BURN_FLAG_ALL;

	/* Go through all plugins to get the supported/... record flags for link */
	for (iter = link->plugins; iter; iter = iter->next) {
		gboolean result;
		BraseroPlugin *plugin;
		BraseroBurnFlag plugin_supported;
		BraseroBurnFlag plugin_compulsory;

		plugin = iter->data;
		if (!brasero_plugin_get_active (plugin, ignore_plugin_errors))
			continue;

		result = brasero_plugin_get_record_flags (plugin,
							  media,
							  session_flags,
							  &plugin_supported,
							  &plugin_compulsory);
		if (!result)
			continue;

		*supported |= plugin_supported;
		compulsory &= plugin_compulsory;
	}

	*compulsory_retval = compulsory;
}

static void
brasero_caps_link_get_data_flags (BraseroCapsLink *link,
                                  gboolean ignore_plugin_errors,
				  BraseroMedia media,
				  BraseroBurnFlag session_flags,
				  BraseroBurnFlag *supported)
{
	GSList *iter;

	/* Go through all plugins the get the supported/... data flags for link */
	for (iter = link->plugins; iter; iter = iter->next) {
		BraseroPlugin *plugin;
		BraseroBurnFlag plugin_supported;
		BraseroBurnFlag plugin_compulsory;

		plugin = iter->data;
		if (!brasero_plugin_get_active (plugin, ignore_plugin_errors))
			continue;

		brasero_plugin_get_image_flags (plugin,
		                                media,
		                                session_flags,
		                                &plugin_supported,
		                                &plugin_compulsory);
		*supported |= plugin_supported;
	}
}

static gboolean
brasero_caps_link_check_data_flags (BraseroCapsLink *link,
                                    gboolean ignore_plugin_errors,
				    BraseroBurnFlag session_flags,
				    BraseroMedia media)
{
	GSList *iter;
	BraseroBurnFlag flags;

	/* here we just make sure that at least one of the plugins in the link
	 * can comply with the flags (APPEND/MERGE) */
	flags = session_flags & (BRASERO_BURN_FLAG_APPEND|BRASERO_BURN_FLAG_MERGE);

	/* If there are no image flags forget it */
	if (flags == BRASERO_BURN_FLAG_NONE)
		return TRUE;

	/* Go through all plugins; at least one must support image flags */
	for (iter = link->plugins; iter; iter = iter->next) {
		gboolean result;
		BraseroPlugin *plugin;

		plugin = iter->data;
		if (!brasero_plugin_get_active (plugin, ignore_plugin_errors))
			continue;

		result = brasero_plugin_check_image_flags (plugin,
							   media,
							   session_flags);
		if (result)
			return TRUE;
	}

	return FALSE;
}

static gboolean
brasero_caps_link_check_record_flags (BraseroCapsLink *link,
                                      gboolean ignore_plugin_errors,
				      BraseroBurnFlag session_flags,
				      BraseroMedia media)
{
	GSList *iter;
	BraseroBurnFlag flags;

	flags = session_flags & BRASERO_PLUGIN_BURN_FLAG_MASK;

	/* If there are no record flags forget it */
	if (flags == BRASERO_BURN_FLAG_NONE)
		return TRUE;
	
	/* Go through all plugins: at least one must support record flags */
	for (iter = link->plugins; iter; iter = iter->next) {
		gboolean result;
		BraseroPlugin *plugin;

		plugin = iter->data;
		if (!brasero_plugin_get_active (plugin, ignore_plugin_errors))
			continue;

		result = brasero_plugin_check_record_flags (plugin,
							    media,
							    session_flags);
		if (result)
			return TRUE;
	}

	return FALSE;
}

static gboolean
brasero_caps_link_check_media_restrictions (BraseroCapsLink *link,
                                            gboolean ignore_plugin_errors,
					    BraseroMedia media)
{
	GSList *iter;

	/* Go through all plugins: at least one must support media */
	for (iter = link->plugins; iter; iter = iter->next) {
		gboolean result;
		BraseroPlugin *plugin;

		plugin = iter->data;
		if (!brasero_plugin_get_active (plugin, ignore_plugin_errors))
			continue;

		result = brasero_plugin_check_media_restrictions (plugin, media);
		if (result)
			return TRUE;
	}

	return FALSE;
}

static BraseroBurnResult
brasero_caps_report_plugin_error (BraseroPlugin *plugin,
                                  BraseroForeachPluginErrorCb callback,
                                  gpointer user_data)
{
	GSList *errors;

	errors = brasero_plugin_get_errors (plugin);
	if (!errors)
		return BRASERO_BURN_ERR;

	do {
		BraseroPluginError *error;
		BraseroBurnResult result;

		error = errors->data;
		result = callback (error->type, error->detail, user_data);
		if (result == BRASERO_BURN_RETRY) {
			/* Something has been done
			 * to fix the error like an install
			 * so reload the errors */
			brasero_plugin_check_plugin_ready (plugin);
			errors = brasero_plugin_get_errors (plugin);
			continue;
		}

		if (result != BRASERO_BURN_OK)
			return result;

		errors = errors->next;
	} while (errors);

	return BRASERO_BURN_OK;
}

struct _BraseroFindLinkCtx {
	BraseroMedia media;
	BraseroTrackType *input;
	BraseroPluginIOFlag io_flags;
	BraseroBurnFlag session_flags;

	BraseroForeachPluginErrorCb callback;
	gpointer user_data;

	guint ignore_plugin_errors:1;
	guint check_session_flags:1;
};
typedef struct _BraseroFindLinkCtx BraseroFindLinkCtx;

static void
brasero_caps_find_link_set_ctx (BraseroBurnSession *session,
                                BraseroFindLinkCtx *ctx,
                                BraseroTrackType *input)
{
	ctx->input = input;

	if (ctx->check_session_flags) {
		ctx->session_flags = brasero_burn_session_get_flags (session);

		if (BRASERO_BURN_SESSION_NO_TMP_FILE (session))
			ctx->io_flags = BRASERO_PLUGIN_IO_ACCEPT_PIPE;
		else
			ctx->io_flags = BRASERO_PLUGIN_IO_ACCEPT_FILE;
	}
	else
		ctx->io_flags = BRASERO_PLUGIN_IO_ACCEPT_FILE|
					BRASERO_PLUGIN_IO_ACCEPT_PIPE;

	if (!ctx->callback)
		ctx->ignore_plugin_errors = (brasero_burn_session_get_strict_support (session) == FALSE);
	else
		ctx->ignore_plugin_errors = TRUE;
}

static BraseroBurnResult
brasero_caps_find_link (BraseroCaps *caps,
                        BraseroFindLinkCtx *ctx)
{
	GSList *iter;

	BRASERO_BURN_LOG_WITH_TYPE (&caps->type, BRASERO_PLUGIN_IO_NONE, "Found link (with %i links):", g_slist_length (caps->links));

	/* Here we only make sure we have at least one link working. For a link
	 * to be followed it must first:
	 * - link to a caps with correct io flags
	 * - have at least a plugin accepting the record flags if caps type is
	 *   a disc (that means that the link is the recording part)
	 *
	 * and either:
	 * - link to a caps equal to the input
	 * - link to a caps (linking itself to another caps, ...) accepting the
	 *   input
	 */

	for (iter = caps->links; iter; iter = iter->next) {
		BraseroCapsLink *link;
		BraseroBurnResult result;

		link = iter->data;

		if (!link->caps)
			continue;
		
		/* check that the link has some active plugin */
		if (!brasero_caps_link_active (link, ctx->ignore_plugin_errors))
			continue;

		/* since this link contains recorders, check that at least one
		 * of them can handle the record flags */
		if (ctx->check_session_flags && brasero_track_type_get_has_medium (&caps->type)) {
			if (!brasero_caps_link_check_record_flags (link, ctx->ignore_plugin_errors, ctx->session_flags, ctx->media))
				continue;

			if (brasero_caps_link_check_recorder_flags_for_input (link, ctx->session_flags) != BRASERO_BURN_OK)
				continue;
		}

		/* first see if that's the perfect fit:
		 * - it must have the same caps (type + subtype)
		 * - it must have the proper IO */
		if (brasero_track_type_get_has_data (&link->caps->type)) {
			if (ctx->check_session_flags
			&& !brasero_caps_link_check_data_flags (link, ctx->ignore_plugin_errors, ctx->session_flags, ctx->media))
				continue;
		}
		else if (!brasero_caps_link_check_media_restrictions (link, ctx->ignore_plugin_errors, ctx->media))
			continue;

		if ((link->caps->flags & BRASERO_PLUGIN_IO_ACCEPT_FILE)
		&&   brasero_caps_is_compatible_type (link->caps, ctx->input)) {
			if (ctx->callback) {
				BraseroPlugin *plugin;

				/* If we are supposed to report/signal that the plugin
				 * could be used but only if some more elements are 
				 * installed */
				plugin = brasero_caps_link_need_download (link);
				if (plugin)
					return brasero_caps_report_plugin_error (plugin, ctx->callback, ctx->user_data);
			}
			return BRASERO_BURN_OK;
		}

		/* we can't go further than a DISC type */
		if (brasero_track_type_get_has_medium (&link->caps->type))
			continue;

		if ((link->caps->flags & ctx->io_flags) == BRASERO_PLUGIN_IO_NONE)
			continue;

		/* try to see where the inputs of this caps leads to */
		result = brasero_caps_find_link (link->caps, ctx);
		if (result == BRASERO_BURN_CANCEL)
			return result;

		if (result == BRASERO_BURN_OK) {
			if (ctx->callback) {
				BraseroPlugin *plugin;

				/* If we are supposed to report/signal that the plugin
				 * could be used but only if some more elements are 
				 * installed */
				plugin = brasero_caps_link_need_download (link);
				if (plugin)
					return brasero_caps_report_plugin_error (plugin, ctx->callback, ctx->user_data);
			}
			return BRASERO_BURN_OK;
		}
	}

	return BRASERO_BURN_NOT_SUPPORTED;
}

static BraseroBurnResult
brasero_caps_try_output (BraseroBurnCaps *self,
                         BraseroFindLinkCtx *ctx,
                         BraseroTrackType *output)
{
	BraseroCaps *caps;

	/* here we search the start caps */
	caps = brasero_burn_caps_find_start_caps (self, output);
	if (!caps) {
		BRASERO_BURN_LOG ("No caps available");
		return BRASERO_BURN_NOT_SUPPORTED;
	}

	/* Here flags don't matter as we don't record anything.
	 * Even the IOFlags since that can be checked later with
	 * brasero_burn_caps_get_flags. */
	if (brasero_track_type_get_has_medium (output))
		ctx->media = brasero_track_type_get_medium_type (output);
	else
		ctx->media = BRASERO_MEDIUM_FILE;

	return brasero_caps_find_link (caps, ctx);
}

static BraseroBurnResult
brasero_caps_try_output_with_blanking (BraseroBurnCaps *self,
                                       BraseroBurnSession *session,
                                       BraseroFindLinkCtx *ctx,
                                       BraseroTrackType *output)
{
	gboolean result;
	BraseroCaps *last_caps;

	result = brasero_caps_try_output (self, ctx, output);
	if (result == BRASERO_BURN_OK
	||  result == BRASERO_BURN_CANCEL)
		return result;

	/* we reached this point in two cases:
	 * - if the disc cannot be handled
	 * - if some flags are not handled
	 * It is helpful only if:
	 * - the disc was closed and no plugin can handle this type of 
	 * disc once closed (CD-R(W))
	 * - there was the flag BLANK_BEFORE_WRITE set and no plugin can
	 * handle this flag (means that the plugin should erase and
	 * then write on its own. Basically that works only with
	 * overwrite formatted discs, DVD+RW, ...) */
	if (!brasero_track_type_get_has_medium (output))
		return BRASERO_BURN_NOT_SUPPORTED;

	/* output is a disc try with initial blanking */
	BRASERO_BURN_LOG ("Support for input/output failed.");

	/* apparently nothing can be done to reach our goal. Maybe that
	 * is because we first have to blank the disc. If so add a blank 
	 * task to the others as a first step */
	if ((ctx->check_session_flags
	&& !(ctx->session_flags & BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE))
	||   brasero_burn_session_can_blank (session) != BRASERO_BURN_OK)
		return BRASERO_BURN_NOT_SUPPORTED;

	BRASERO_BURN_LOG ("Trying with initial blanking");

	/* retry with the same disc type but blank this time */
	ctx->media = brasero_track_type_get_medium_type (output);
	ctx->media &= ~(BRASERO_MEDIUM_CLOSED|
	                BRASERO_MEDIUM_APPENDABLE|
	                BRASERO_MEDIUM_UNFORMATTED|
	                BRASERO_MEDIUM_HAS_DATA|
	                BRASERO_MEDIUM_HAS_AUDIO);
	ctx->media |= BRASERO_MEDIUM_BLANK;
	brasero_track_type_set_medium_type (output, ctx->media);

	last_caps = brasero_burn_caps_find_start_caps (self, output);
	if (!last_caps)
		return BRASERO_BURN_NOT_SUPPORTED;

	return brasero_caps_find_link (last_caps, ctx);
}

/**
 * brasero_burn_session_input_supported:
 * @session: a #BraseroBurnSession
 * @input: a #BraseroTrackType
 * @check_flags: a #gboolean
 *
 * Given the various parameters stored in @session, this
 * function checks whether a session with the data type
 * @type could be burnt to the medium in the #BraseroDrive (set 
 * through brasero_burn_session_set_burner ()).
 * If @check_flags is %TRUE, then flags are taken into account
 * and are not if it is %FALSE.
 *
 * Return value: a #BraseroBurnResult.
 * BRASERO_BURN_OK if it is possible.
 * BRASERO_BURN_ERR otherwise.
 **/

BraseroBurnResult
brasero_burn_session_input_supported (BraseroBurnSession *session,
				      BraseroTrackType *input,
                                      gboolean check_flags)
{
	BraseroBurnCaps *self;
	BraseroBurnResult result;
	BraseroTrackType output;
	BraseroFindLinkCtx ctx = { 0, NULL, 0, };

	result = brasero_burn_session_get_output_type (session, &output);
	if (result != BRASERO_BURN_OK)
		return result;

	BRASERO_BURN_LOG_TYPE (input, "Checking support for input");
	BRASERO_BURN_LOG_TYPE (&output, "and output");

	ctx.check_session_flags = check_flags;
	brasero_caps_find_link_set_ctx (session, &ctx, input);

	if (check_flags) {
		result = brasero_check_flags_for_drive (brasero_burn_session_get_burner (session),
							ctx.session_flags);

		if (!result)
			BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG_RES (session);

		BRASERO_BURN_LOG_FLAGS (ctx.session_flags, "with flags");
	}

	self = brasero_burn_caps_get_default ();
	result = brasero_caps_try_output_with_blanking (self,
							session,
	                                                &ctx,
							&output);
	g_object_unref (self);

	if (result != BRASERO_BURN_OK) {
		BRASERO_BURN_LOG_TYPE (input, "Input not supported");
		return result;
	}

	return BRASERO_BURN_OK;
}

/**
 * brasero_burn_session_output_supported:
 * @session: a #BraseroBurnSession *
 * @output: a #BraseroTrackType *
 *
 * Make sure that the image type or medium type defined in @output can be
 * created/burnt given the parameters and the current data set in @session.
 *
 * Return value: BRASERO_BURN_OK if the medium type or the image type can be used as an output.
 **/
BraseroBurnResult
brasero_burn_session_output_supported (BraseroBurnSession *session,
				       BraseroTrackType *output)
{
	BraseroBurnCaps *self;
	BraseroTrackType input;
	BraseroBurnResult result;
	BraseroFindLinkCtx ctx = { 0, NULL, 0, };

	/* Here, we can't check if the drive supports the flags since the output
	 * is hypothetical. There is no real medium. So forget the following :
	 * if (!brasero_burn_caps_flags_check_for_drive (session))
	 *	BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG_RES (session);
	 * The only thing we could do would be to check some known forbidden 
	 * flags for some media provided the output type is DISC. */

	brasero_burn_session_get_input_type (session, &input);
	brasero_caps_find_link_set_ctx (session, &ctx, &input);

	BRASERO_BURN_LOG_TYPE (output, "Checking support for output");
	BRASERO_BURN_LOG_TYPE (&input, "and input");
	BRASERO_BURN_LOG_FLAGS (brasero_burn_session_get_flags (session), "with flags");
	
	self = brasero_burn_caps_get_default ();
	result = brasero_caps_try_output_with_blanking (self,
							session,
	                                                &ctx,
							output);
	g_object_unref (self);

	if (result != BRASERO_BURN_OK) {
		BRASERO_BURN_LOG_TYPE (output, "Output not supported");
		return result;
	}

	return BRASERO_BURN_OK;
}

/**
 * This is only to be used in case one wants to copy using the same drive.
 * It determines the possible middle image type.
 */

static BraseroBurnResult
brasero_burn_caps_is_session_supported_same_src_dest (BraseroBurnCaps *self,
						      BraseroBurnSession *session,
                                                      BraseroFindLinkCtx *ctx,
                                                      BraseroTrackType *tmp_type)
{
	GSList *iter;
	BraseroDrive *burner;
	BraseroTrackType input;
	BraseroBurnResult result;
	BraseroTrackType output;
	BraseroImageFormat format;

	BRASERO_BURN_LOG ("Checking disc copy support with same source and destination");

	/* To determine if a CD/DVD can be copied using the same source/dest,
	 * we first determine if can be imaged and then if this image can be 
	 * burnt to whatever medium type. */
	brasero_caps_find_link_set_ctx (session, ctx, &input);
	ctx->io_flags = BRASERO_PLUGIN_IO_ACCEPT_FILE;

	memset (&input, 0, sizeof (BraseroTrackType));
	brasero_burn_session_get_input_type (session, &input);
	BRASERO_BURN_LOG_TYPE (&input, "input");

	if (ctx->check_session_flags) {
		/* NOTE: DAO can be a problem. So just in case remove it. It is
		 * not really useful in this context. What we want here is to
		 * know whether a medium can be used given the input; only 1
		 * flag is important here (MERGE) and can have consequences. */
		ctx->session_flags &= ~BRASERO_BURN_FLAG_DAO;
		BRASERO_BURN_LOG_FLAGS (ctx->session_flags, "flags");
	}

	burner = brasero_burn_session_get_burner (session);

	/* First see if it works with a stream type */
	brasero_track_type_set_has_stream (&output);

	/* FIXME! */
	brasero_track_type_set_stream_format (&output,
	                                      BRASERO_AUDIO_FORMAT_RAW|
	                                      BRASERO_METADATA_INFO);

	BRASERO_BURN_LOG_TYPE (&output, "Testing stream type");
	result = brasero_caps_try_output (self, ctx, &output);
	if (result == BRASERO_BURN_CANCEL)
		return result;

	if (result == BRASERO_BURN_OK) {
		BRASERO_BURN_LOG ("Stream type seems to be supported as output");

		/* This format can be used to create an image. Check if can be
		 * burnt now. Just find at least one medium. */
		for (iter = self->priv->caps_list; iter; iter = iter->next) {
			BraseroBurnResult result;
			BraseroMedia media;
			BraseroCaps *caps;

			caps = iter->data;

			if (!brasero_track_type_get_has_medium (&caps->type))
				continue;

			media = brasero_track_type_get_medium_type (&caps->type);
			/* Audio is only supported by CDs */
			if ((media & BRASERO_MEDIUM_CD) == 0)
				continue;

			/* This type of disc cannot be burnt; skip them */
			if (media & BRASERO_MEDIUM_ROM)
				continue;

			/* Make sure this is supported by the drive */
			if (!brasero_drive_can_write_media (burner, media))
				continue;

			ctx->media = media;
			result = brasero_caps_find_link (caps, ctx);
			BRASERO_BURN_LOG_DISC_TYPE (media,
						    "Tested medium (%s)",
						    result == BRASERO_BURN_OK ? "working":"not working");

			if (result == BRASERO_BURN_OK) {
				if (tmp_type) {
					brasero_track_type_set_has_stream (tmp_type);
					brasero_track_type_set_stream_format (tmp_type, brasero_track_type_get_stream_format (&output));
				}
					
				return BRASERO_BURN_OK;
			}

			if (result == BRASERO_BURN_CANCEL)
				return result;
		}
	}
	else
		BRASERO_BURN_LOG ("Stream format not supported as output");

	/* Find one available output format */
	format = BRASERO_IMAGE_FORMAT_CDRDAO;
	brasero_track_type_set_has_image (&output);

	for (; format > BRASERO_IMAGE_FORMAT_NONE; format >>= 1) {
		brasero_track_type_set_image_format (&output, format);

		BRASERO_BURN_LOG_TYPE (&output, "Testing temporary image format");

		/* Don't need to try blanking here (saves
		 * a few lines of debug) since that is an 
		 * image */
		result = brasero_caps_try_output (self, ctx, &output);
		if (result == BRASERO_BURN_CANCEL)
			return result;

		if (result != BRASERO_BURN_OK)
			continue;

		/* This format can be used to create an image. Check if can be
		 * burnt now. Just find at least one medium. */
		for (iter = self->priv->caps_list; iter; iter = iter->next) {
			BraseroBurnResult result;
			BraseroMedia media;
			BraseroCaps *caps;

			caps = iter->data;

			if (!brasero_track_type_get_has_medium (&caps->type))
				continue;

			media = brasero_track_type_get_medium_type (&caps->type);

			/* This type of disc cannot be burnt; skip them */
			if (media & BRASERO_MEDIUM_ROM)
				continue;

			/* These three types only work with CDs. Skip the rest. */
			if ((format == BRASERO_IMAGE_FORMAT_CDRDAO
			||   format == BRASERO_IMAGE_FORMAT_CLONE
			||   format == BRASERO_IMAGE_FORMAT_CUE)
			&& (media & BRASERO_MEDIUM_CD) == 0)
				continue;

			/* Make sure this is supported by the drive */
			if (!brasero_drive_can_write_media (burner, media))
				continue;

			ctx->media = media;
			result = brasero_caps_find_link (caps, ctx);
			BRASERO_BURN_LOG_DISC_TYPE (media,
						    "Tested medium (%s)",
						    result == BRASERO_BURN_OK ? "working":"not working");

			if (result == BRASERO_BURN_OK) {
				if (tmp_type) {
					brasero_track_type_set_has_image (tmp_type);
					brasero_track_type_set_image_format (tmp_type, brasero_track_type_get_image_format (&output));
				}
					
				return BRASERO_BURN_OK;
			}

			if (result == BRASERO_BURN_CANCEL)
				return result;
		}
	}

	return BRASERO_BURN_NOT_SUPPORTED;
}

BraseroBurnResult
brasero_burn_session_get_tmp_image_type_same_src_dest (BraseroBurnSession *session,
                                                       BraseroTrackType *image_type)
{
	BraseroBurnCaps *self;
	BraseroBurnResult result;
	BraseroFindLinkCtx ctx = { 0, NULL, 0, };

	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (session), BRASERO_BURN_ERR);

	self = brasero_burn_caps_get_default ();
	result = brasero_burn_caps_is_session_supported_same_src_dest (self,
	                                                               session,
	                                                               &ctx,
	                                                               image_type);
	g_object_unref (self);
	return result;
}

static BraseroBurnResult
brasero_burn_session_supported (BraseroBurnSession *session,
                                BraseroFindLinkCtx *ctx)
{
	gboolean result;
	BraseroBurnCaps *self;
	BraseroTrackType input;
	BraseroTrackType output;

	/* Special case */
	if (brasero_burn_session_same_src_dest_drive (session)) {
		BraseroBurnResult res;

		self = brasero_burn_caps_get_default ();
		res = brasero_burn_caps_is_session_supported_same_src_dest (self, session, ctx, NULL);
		g_object_unref (self);

		return res;
	}

	result = brasero_burn_session_get_output_type (session, &output);
	if (result != BRASERO_BURN_OK)
		BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG_RES (session);

	brasero_burn_session_get_input_type (session, &input);
	brasero_caps_find_link_set_ctx (session, ctx, &input);

	BRASERO_BURN_LOG_TYPE (&output, "Checking support for session. Ouput is ");
	BRASERO_BURN_LOG_TYPE (&input, "and input is");

	if (ctx->check_session_flags) {
		result = brasero_check_flags_for_drive (brasero_burn_session_get_burner (session), ctx->session_flags);
		if (!result)
			BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG_RES (session);

		BRASERO_BURN_LOG_FLAGS (ctx->session_flags, "with flags");
	}

	self = brasero_burn_caps_get_default ();
	result = brasero_caps_try_output_with_blanking (self,
							session,
	                                                ctx,
							&output);
	g_object_unref (self);

	if (result != BRASERO_BURN_OK) {
		BRASERO_BURN_LOG_TYPE (&output, "Session not supported");
		return result;
	}

	BRASERO_BURN_LOG_TYPE (&output, "Session supported");
	return BRASERO_BURN_OK;
}

/**
 * brasero_burn_session_can_burn:
 * @session: a #BraseroBurnSession
 * @check_flags: a #gboolean
 *
 * Given the various parameters stored in @session, this
 * function checks whether the data contained in @session
 * can be burnt to the medium in the #BraseroDrive (set 
 * through brasero_burn_session_set_burner ()).
 * If @check_flags determine the behavior of this function.
 *
 * Return value: a #BraseroBurnResult.
 * BRASERO_BURN_OK if it is possible.
 * BRASERO_BURN_ERR otherwise.
 **/

BraseroBurnResult
brasero_burn_session_can_burn (BraseroBurnSession *session,
			       gboolean check_flags)
{
	BraseroFindLinkCtx ctx = { 0, NULL, 0, };

	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (session), BRASERO_BURN_ERR);

	ctx.check_session_flags = check_flags;

	return brasero_burn_session_supported (session, &ctx);
}

/**
 * brasero_session_foreach_plugin_error:
 * @session: a #BraseroBurnSession.
 * @callback: a #BraseroSessionPluginErrorCb.
 * @user_data: a #gpointer. The data passed to @callback.
 *
 * Call @callback for each error in plugins.
 *
 * Return value: a #BraseroBurnResult.
 * BRASERO_BURN_OK if it is possible.
 * BRASERO_BURN_ERR otherwise.
 **/

BraseroBurnResult
brasero_session_foreach_plugin_error (BraseroBurnSession *session,
                                      BraseroForeachPluginErrorCb callback,
                                      gpointer user_data)
{
	BraseroFindLinkCtx ctx = { 0, NULL, 0, };

	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (session), BRASERO_BURN_ERR);

	ctx.callback = callback;
	ctx.user_data = user_data;
	
	return brasero_burn_session_supported (session, &ctx);
}

/**
 * brasero_burn_session_get_required_media_type:
 * @session: a #BraseroBurnSession
 *
 * Return the medium types that could be used to burn
 * @session.
 *
 * Return value: a #BraseroMedia
 **/

BraseroMedia
brasero_burn_session_get_required_media_type (BraseroBurnSession *session)
{
	BraseroMedia required_media = BRASERO_MEDIUM_NONE;
	BraseroFindLinkCtx ctx = { 0, NULL, 0, };
	BraseroTrackType input;
	BraseroBurnCaps *self;
	GSList *iter;

	if (brasero_burn_session_is_dest_file (session))
		return BRASERO_MEDIUM_FILE;

	/* we try to determine here what type of medium is allowed to be burnt
	 * to whether a CD or a DVD. Appendable, blank are not properties being
	 * determined here. We just want it to be writable in a broad sense. */
	ctx.check_session_flags = TRUE;
	brasero_burn_session_get_input_type (session, &input);
	brasero_caps_find_link_set_ctx (session, &ctx, &input);
	BRASERO_BURN_LOG_TYPE (&input, "Determining required media type for input");

	/* NOTE: BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE is a problem here since it
	 * is only used if needed. Likewise DAO can be a problem. So just in
	 * case remove them. They are not really useful in this context. What we
	 * want here is to know which media can be used given the input; only 1
	 * flag is important here (MERGE) and can have consequences. */
	ctx.session_flags &= ~BRASERO_BURN_FLAG_DAO;
	BRASERO_BURN_LOG_FLAGS (ctx.session_flags, "and flags");

	self = brasero_burn_caps_get_default ();
	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		BraseroCaps *caps;
		gboolean result;

		caps = iter->data;

		if (!brasero_track_type_get_has_medium (&caps->type))
			continue;

		/* Put BRASERO_MEDIUM_NONE so we can always succeed */
		result = brasero_caps_find_link (caps, &ctx);
		BRASERO_BURN_LOG_DISC_TYPE (caps->type.subtype.media,
					    "Tested (%s)",
					    result == BRASERO_BURN_OK ? "working":"not working");

		if (result == BRASERO_BURN_CANCEL) {
			g_object_unref (self);
			return result;
		}

		if (result != BRASERO_BURN_OK)
			continue;

		/* This caps work, add its subtype */
		required_media |= brasero_track_type_get_medium_type (&caps->type);
	}

	/* filter as we are only interested in these */
	required_media &= BRASERO_MEDIUM_WRITABLE|
			  BRASERO_MEDIUM_CD|
			  BRASERO_MEDIUM_DVD;

	g_object_unref (self);
	return required_media;
}

/**
 * brasero_burn_session_get_possible_output_formats:
 * @session: a #BraseroBurnSession
 * @formats: a #BraseroImageFormat
 *
 * Returns the disc image types that could be set to create
 * an image given the current state of @session.
 *
 * Return value: a #guint. The number of formats available.
 **/

guint
brasero_burn_session_get_possible_output_formats (BraseroBurnSession *session,
						  BraseroImageFormat *formats)
{
	guint num = 0;
	BraseroImageFormat format;
	BraseroTrackType *output = NULL;

	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (session), 0);

	/* see how many output format are available */
	format = BRASERO_IMAGE_FORMAT_CDRDAO;
	(*formats) = BRASERO_IMAGE_FORMAT_NONE;

	output = brasero_track_type_new ();
	brasero_track_type_set_has_image (output);

	for (; format > BRASERO_IMAGE_FORMAT_NONE; format >>= 1) {
		BraseroBurnResult result;

		brasero_track_type_set_image_format (output, format);
		result = brasero_burn_session_output_supported (session, output);
		if (result == BRASERO_BURN_OK) {
			(*formats) |= format;
			num ++;
		}
	}

	brasero_track_type_free (output);

	return num;
}

/**
 * brasero_burn_session_get_default_output_format:
 * @session: a #BraseroBurnSession
 *
 * Returns the default disc image type that should be set to create
 * an image given the current state of @session.
 *
 * Return value: a #BraseroImageFormat
 **/

BraseroImageFormat
brasero_burn_session_get_default_output_format (BraseroBurnSession *session)
{
	BraseroBurnCaps *self;
	BraseroBurnResult result;
	BraseroTrackType source = { BRASERO_TRACK_TYPE_NONE, { 0, }};
	BraseroTrackType output = { BRASERO_TRACK_TYPE_NONE, { 0, }};

	self = brasero_burn_caps_get_default ();

	if (!brasero_burn_session_is_dest_file (session)) {
		g_object_unref (self);
		return BRASERO_IMAGE_FORMAT_NONE;
	}

	brasero_burn_session_get_input_type (session, &source);
	if (brasero_track_type_is_empty (&source)) {
		g_object_unref (self);
		return BRASERO_IMAGE_FORMAT_NONE;
	}

	if (brasero_track_type_get_has_image (&source)) {
		g_object_unref (self);
		return brasero_track_type_get_image_format (&source);
	}

	brasero_track_type_set_has_image (&output);
	brasero_track_type_set_image_format (&output, BRASERO_IMAGE_FORMAT_NONE);

	if (brasero_track_type_get_has_stream (&source)) {
		/* Otherwise try all possible image types */
		output.subtype.img_format = BRASERO_IMAGE_FORMAT_CDRDAO;
		for (; output.subtype.img_format != BRASERO_IMAGE_FORMAT_NONE;
		       output.subtype.img_format >>= 1) {
		
			result = brasero_burn_session_output_supported (session,
									&output);
			if (result == BRASERO_BURN_OK) {
				g_object_unref (self);
				return brasero_track_type_get_image_format (&output);
			}
		}

		g_object_unref (self);
		return BRASERO_IMAGE_FORMAT_NONE;
	}

	if (brasero_track_type_get_has_data (&source)
	|| (brasero_track_type_get_has_medium (&source)
	&& (brasero_track_type_get_medium_type (&source) & BRASERO_MEDIUM_DVD))) {
		brasero_track_type_set_image_format (&output, BRASERO_IMAGE_FORMAT_BIN);
		result = brasero_burn_session_output_supported (session, &output);
		g_object_unref (self);

		if (result != BRASERO_BURN_OK)
			return BRASERO_IMAGE_FORMAT_NONE;

		return BRASERO_IMAGE_FORMAT_BIN;
	}

	/* for the input which are CDs there are lots of possible formats */
	output.subtype.img_format = BRASERO_IMAGE_FORMAT_CDRDAO;
	for (; output.subtype.img_format != BRASERO_IMAGE_FORMAT_NONE;
	       output.subtype.img_format >>= 1) {
	
		result = brasero_burn_session_output_supported (session, &output);
		if (result == BRASERO_BURN_OK) {
			g_object_unref (self);
			return brasero_track_type_get_image_format (&output);
		}
	}

	g_object_unref (self);
	return BRASERO_IMAGE_FORMAT_NONE;
}

static BraseroBurnResult
brasero_caps_set_flags_from_recorder_input (BraseroTrackType *input,
                                            BraseroBurnFlag *supported,
                                            BraseroBurnFlag *compulsory)
{
	if (brasero_track_type_get_has_image (input)) {
		BraseroImageFormat format;

		format = brasero_track_type_get_image_format (input);
		if (format == BRASERO_IMAGE_FORMAT_CUE
		||  format == BRASERO_IMAGE_FORMAT_CDRDAO) {
			if ((*supported) & BRASERO_BURN_FLAG_DAO)
				(*compulsory) |= BRASERO_BURN_FLAG_DAO;
			else
				return BRASERO_BURN_NOT_SUPPORTED;
		}
		else if (format == BRASERO_IMAGE_FORMAT_CLONE) {
			/* RAW write mode should (must) only be used in this case */
			if ((*supported) & BRASERO_BURN_FLAG_RAW) {
				(*supported) &= ~BRASERO_BURN_FLAG_DAO;
				(*compulsory) &= ~BRASERO_BURN_FLAG_DAO;
				(*compulsory) |= BRASERO_BURN_FLAG_RAW;
			}
			else
				return BRASERO_BURN_NOT_SUPPORTED;
		}
		else
			(*supported) &= ~BRASERO_BURN_FLAG_RAW;
	}
	
	return BRASERO_BURN_OK;
}

static BraseroPluginIOFlag
brasero_caps_get_flags (BraseroCaps *caps,
                        gboolean ignore_plugin_errors,
			BraseroBurnFlag session_flags,
			BraseroMedia media,
			BraseroTrackType *input,
			BraseroPluginIOFlag flags,
			BraseroBurnFlag *supported,
			BraseroBurnFlag *compulsory)
{
	GSList *iter;
	BraseroPluginIOFlag retval = BRASERO_PLUGIN_IO_NONE;

	/* First we must know if this link leads somewhere. It must 
	 * accept the already existing flags. If it does, see if it 
	 * accepts the input and if not, if one of its ancestors does */
	for (iter = caps->links; iter; iter = iter->next) {
		BraseroBurnFlag data_supported = BRASERO_BURN_FLAG_NONE;
		BraseroBurnFlag rec_compulsory = BRASERO_BURN_FLAG_ALL;
		BraseroBurnFlag rec_supported = BRASERO_BURN_FLAG_NONE;
		BraseroPluginIOFlag io_flags;
		BraseroCapsLink *link;

		link = iter->data;

		if (!link->caps)
			continue;

		/* check that the link has some active plugin */
		if (!brasero_caps_link_active (link, ignore_plugin_errors))
			continue;

		if (brasero_track_type_get_has_medium (&caps->type)) {
			BraseroBurnFlag tmp;
			BraseroBurnResult result;

			brasero_caps_link_get_record_flags (link,
			                                    ignore_plugin_errors,
							    media,
							    session_flags,
							    &rec_supported,
							    &rec_compulsory);

			/* see if that link can handle the record flags.
			 * NOTE: compulsory are not a failure in this case. */
			tmp = session_flags & BRASERO_PLUGIN_BURN_FLAG_MASK;
			if ((tmp & rec_supported) != tmp)
				continue;

			/* This is the recording plugin, check its input as
			 * some flags depend on it. */
			result = brasero_caps_set_flags_from_recorder_input (&link->caps->type,
			                                                     &rec_supported,
			                                                     &rec_compulsory);
			if (result != BRASERO_BURN_OK)
				continue;
		}

		if (brasero_track_type_get_has_data (&link->caps->type)) {
			BraseroBurnFlag tmp;

			brasero_caps_link_get_data_flags (link,
			                                  ignore_plugin_errors,
							  media,
							  session_flags,
						    	  &data_supported);

			/* see if that link can handle the data flags. 
			 * NOTE: compulsory are not a failure in this case. */
			tmp = session_flags & (BRASERO_BURN_FLAG_APPEND|
					       BRASERO_BURN_FLAG_MERGE);

			if ((tmp & data_supported) != tmp)
				continue;
		}
		else if (!brasero_caps_link_check_media_restrictions (link, ignore_plugin_errors, media))
			continue;

		/* see if that's the perfect fit */
		if ((link->caps->flags & BRASERO_PLUGIN_IO_ACCEPT_FILE)
		&&   brasero_caps_is_compatible_type (link->caps, input)) {
			/* special case for input that handle output/input */
			if (caps->type.type == BRASERO_TRACK_TYPE_DISC)
				retval |= BRASERO_PLUGIN_IO_ACCEPT_PIPE;
			else
				retval |= caps->flags;

			(*compulsory) &= rec_compulsory;
			(*supported) |= data_supported|rec_supported;
			continue;
		}

		if ((link->caps->flags & flags) == BRASERO_PLUGIN_IO_NONE)
			continue;

		/* we can't go further than a DISC type */
		if (link->caps->type.type == BRASERO_TRACK_TYPE_DISC)
			continue;

		/* try to see where the inputs of this caps leads to */
		io_flags = brasero_caps_get_flags (link->caps,
		                                   ignore_plugin_errors,
						   session_flags,
						   media,
						   input,
						   flags,
						   supported,
						   compulsory);
		if (io_flags == BRASERO_PLUGIN_IO_NONE)
			continue;

		(*compulsory) &= rec_compulsory;
		retval |= (io_flags & flags);
		(*supported) |= data_supported|rec_supported;
	}

	return retval;
}

static void
brasero_medium_supported_flags (BraseroMedium *medium,
				BraseroBurnFlag *supported_flags,
                                BraseroBurnFlag *compulsory_flags)
{
	BraseroMedia media;

	media = brasero_medium_get_status (medium);

	/* This is always FALSE */
	if (media & BRASERO_MEDIUM_PLUS)
		(*supported_flags) &= ~BRASERO_BURN_FLAG_DUMMY;

	/* Simulation is only possible according to write modes. This mode is
	 * mostly used by cdrecord/wodim for CLONE images. */
	else if (media & BRASERO_MEDIUM_DVD) {
		if (!brasero_medium_can_use_dummy_for_sao (medium))
			(*supported_flags) &= ~BRASERO_BURN_FLAG_DUMMY;
	}
	else if ((*supported_flags) & BRASERO_BURN_FLAG_DAO) {
		if (!brasero_medium_can_use_dummy_for_sao (medium))
			(*supported_flags) &= ~BRASERO_BURN_FLAG_DUMMY;
	}
	else if (!brasero_medium_can_use_dummy_for_tao (medium))
		(*supported_flags) &= ~BRASERO_BURN_FLAG_DUMMY;

	/* The following is only true if we won't _have_ to blank
	 * the disc since a CLOSED disc is not able for tao/sao.
	 * so if BLANK_BEFORE_RIGHT is TRUE then we leave 
	 * the benefit of the doubt, but flags should be rechecked
	 * after the drive was blanked. */
	if (((*compulsory_flags) & BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE) == 0
	&&  !BRASERO_MEDIUM_RANDOM_WRITABLE (media)
	&&  !brasero_medium_can_use_tao (medium)) {
		(*supported_flags) &= ~BRASERO_BURN_FLAG_MULTI;

		if (brasero_medium_can_use_sao (medium))
			(*compulsory_flags) |= BRASERO_BURN_FLAG_DAO;
		else
			(*supported_flags) &= ~BRASERO_BURN_FLAG_DAO;
	}

	if (!brasero_medium_can_use_burnfree (medium))
		(*supported_flags) &= ~BRASERO_BURN_FLAG_BURNPROOF;
}

static void
brasero_burn_caps_flags_update_for_drive (BraseroBurnSession *session,
                                          BraseroBurnFlag *supported_flags,
                                          BraseroBurnFlag *compulsory_flags)
{
	BraseroDrive *drive;
	BraseroMedium *medium;

	drive = brasero_burn_session_get_burner (session);
	if (!drive)
		return;

	medium = brasero_drive_get_medium (drive);
	if (!medium)
		return;

	brasero_medium_supported_flags (medium,
	                                supported_flags,
	                                compulsory_flags);
}

static BraseroBurnResult
brasero_caps_get_flags_for_disc (BraseroBurnCaps *self,
                                 gboolean ignore_plugin_errors,
				 BraseroBurnFlag session_flags,
				 BraseroMedia media,
				 BraseroTrackType *input,
				 BraseroBurnFlag *supported,
				 BraseroBurnFlag *compulsory)
{
	BraseroBurnFlag supported_flags = BRASERO_BURN_FLAG_NONE;
	BraseroBurnFlag compulsory_flags = BRASERO_BURN_FLAG_ALL;
	BraseroPluginIOFlag io_flags;
	BraseroTrackType output;
	BraseroCaps *caps;

	/* create the output to find first caps */
	brasero_track_type_set_has_medium (&output);
	brasero_track_type_set_medium_type (&output, media);

	caps = brasero_burn_caps_find_start_caps (self, &output);
	if (!caps) {
		BRASERO_BURN_LOG_DISC_TYPE (media, "FLAGS: no caps could be found for");
		return BRASERO_BURN_NOT_SUPPORTED;
	}

	BRASERO_BURN_LOG_WITH_TYPE (&caps->type,
				    caps->flags,
				    "FLAGS: trying caps");

	io_flags = brasero_caps_get_flags (caps,
	                                   ignore_plugin_errors,
					   session_flags,
					   media,
					   input,
					   BRASERO_PLUGIN_IO_ACCEPT_FILE|
					   BRASERO_PLUGIN_IO_ACCEPT_PIPE,
					   &supported_flags,
					   &compulsory_flags);

	if (io_flags == BRASERO_PLUGIN_IO_NONE) {
		BRASERO_BURN_LOG ("FLAGS: not supported");
		return BRASERO_BURN_NOT_SUPPORTED;
	}

	/* NOTE: DO NOT TEST the input image here. What should be tested is the
	 * type of the input right before the burner plugin. See:
	 * brasero_burn_caps_set_flags_from_recorder_input())
	 * For example in the following situation: AUDIO => CUE => BURNER the
	 * DAO flag would not be set otherwise. */

	if (brasero_track_type_get_has_stream (input)) {
		BraseroStreamFormat format;

		format = brasero_track_type_get_stream_format (input);
		if (format & BRASERO_METADATA_INFO) {
			/* In this case, DAO is compulsory if we want to write CD-TEXT */
			if (supported_flags & BRASERO_BURN_FLAG_DAO)
				compulsory_flags |= BRASERO_BURN_FLAG_DAO;
			else
				return BRASERO_BURN_NOT_SUPPORTED;
		}
	}

	if (compulsory_flags & BRASERO_BURN_FLAG_DAO) {
		/* unlikely */
		compulsory_flags &= ~BRASERO_BURN_FLAG_RAW;
		supported_flags &= ~BRASERO_BURN_FLAG_RAW;
	}
	
	if (io_flags & BRASERO_PLUGIN_IO_ACCEPT_PIPE) {
		supported_flags |= BRASERO_BURN_FLAG_NO_TMP_FILES;

		if ((io_flags & BRASERO_PLUGIN_IO_ACCEPT_FILE) == 0)
			compulsory_flags |= BRASERO_BURN_FLAG_NO_TMP_FILES;
	}

	*supported |= supported_flags;
	*compulsory |= compulsory_flags;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_caps_get_flags_for_medium (BraseroBurnCaps *self,
                                        BraseroBurnSession *session,
					BraseroMedia media,
					BraseroBurnFlag session_flags,
					BraseroTrackType *input,
					BraseroBurnFlag *supported_flags,
					BraseroBurnFlag *compulsory_flags)
{
	BraseroBurnResult result;
	gboolean can_blank = FALSE;

	/* See if medium is supported out of the box */
	result = brasero_caps_get_flags_for_disc (self,
	                                          brasero_burn_session_get_strict_support (session) == FALSE,
						  session_flags,
						  media,
						  input,
						  supported_flags,
						  compulsory_flags);

	/* see if we can add BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE. Add it when:
	 * - media can be blanked, it has audio or data and we're not merging
	 * - media is not formatted and it can be blanked/formatted */
	if (brasero_burn_caps_can_blank_real (self, brasero_burn_session_get_strict_support (session) == FALSE, media, session_flags) == BRASERO_BURN_OK)
		can_blank = TRUE;
	else if (session_flags & BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE)
		return BRASERO_BURN_NOT_SUPPORTED;

	if (can_blank) {
		gboolean first_success;
		BraseroBurnFlag blank_compulsory = BRASERO_BURN_FLAG_NONE;
		BraseroBurnFlag blank_supported = BRASERO_BURN_FLAG_NONE;

		/* we reached this point in two cases:
		 * - if the disc cannot be handled
		 * - if some flags are not handled
		 * It is helpful only if:
		 * - the disc was closed and no plugin can handle this type of 
		 * disc once closed (CD-R(W))
		 * - there was the flag BLANK_BEFORE_WRITE set and no plugin can
		 * handle this flag (means that the plugin should erase and
		 * then write on its own. Basically that works only with
		 * overwrite formatted discs, DVD+RW, ...) */

		/* What's above is not entirely true. In fact we always need to
		 * check even if we first succeeded. There are some cases like
		 * CDRW where it's useful.
		 * Ex: a CDRW with data appendable can be either appended (then
		 * no DAO possible) or blanked and written (DAO possible). */

		/* result here is the result of the first operation, so if it
		 * failed, BLANK before becomes compulsory. */
		first_success = (result == BRASERO_BURN_OK);

		/* pretends it is blank and formatted to see if it would work.
		 * If it works then that means that the BLANK_BEFORE_WRITE flag
		 * is compulsory. */
		media &= ~(BRASERO_MEDIUM_CLOSED|
			   BRASERO_MEDIUM_APPENDABLE|
			   BRASERO_MEDIUM_UNFORMATTED|
			   BRASERO_MEDIUM_HAS_DATA|
			   BRASERO_MEDIUM_HAS_AUDIO);
		media |= BRASERO_MEDIUM_BLANK;
		result = brasero_caps_get_flags_for_disc (self,
		                                          brasero_burn_session_get_strict_support (session) == FALSE,
							  session_flags,
							  media,
							  input,
							  supported_flags,
							  compulsory_flags);

		/* if both attempts failed, drop it */
		if (result != BRASERO_BURN_OK) {
			/* See if we entirely failed */
			if (!first_success)
				return result;

			/* we tried with a blank medium but did not 
			 * succeed. So that means the flags BLANK.
			 * is not supported */
		}
		else {
			(*supported_flags) |= BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE;

			if (!first_success)
				(*compulsory_flags) |= BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE;

			/* If BLANK flag is supported then MERGE/APPEND can't be compulsory */
			(*compulsory_flags) &= ~(BRASERO_BURN_FLAG_MERGE|BRASERO_BURN_FLAG_APPEND);

			/* need to add blanking flags */
			brasero_burn_caps_get_blanking_flags_real (self,
			                                           brasero_burn_session_get_strict_support (session) == FALSE,
								   media,
								   session_flags,
								   &blank_supported,
								   &blank_compulsory);
			(*supported_flags) |= blank_supported;
			(*compulsory_flags) |= blank_compulsory;
		}
		
	}
	else if (result != BRASERO_BURN_OK)
		return result;

	/* These are a special case for DVDRW sequential */
	if (BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDRW)) {
		/* That's a way to give priority to MULTI over FAST
		 * and leave the possibility to always use MULTI. */
		if (session_flags & BRASERO_BURN_FLAG_MULTI)
			(*supported_flags) &= ~BRASERO_BURN_FLAG_FAST_BLANK;
		else if ((session_flags & BRASERO_BURN_FLAG_FAST_BLANK)
		         &&  (session_flags & BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE)) {
			/* We should be able to handle this case differently but unfortunately
			 * there are buggy firmwares that won't report properly the supported
			 * mode writes */
			if (!((*supported_flags) & BRASERO_BURN_FLAG_DAO))
					 return BRASERO_BURN_NOT_SUPPORTED;

			(*compulsory_flags) |= BRASERO_BURN_FLAG_DAO;
		}
	}

	if (session_flags & BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE) {
		/* make sure we remove MERGE/APPEND from supported and
		 * compulsory since that's not possible anymore */
		(*supported_flags) &= ~(BRASERO_BURN_FLAG_MERGE|BRASERO_BURN_FLAG_APPEND);
		(*compulsory_flags) &= ~(BRASERO_BURN_FLAG_MERGE|BRASERO_BURN_FLAG_APPEND);
	}

	/* FIXME! we should restart the whole process if
	 * ((session_flags & compulsory_flags) != compulsory_flags) since that
	 * means that some supported files could be excluded but were not */

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_caps_get_flags_same_src_dest_for_types (BraseroBurnCaps *self,
                                                     BraseroBurnSession *session,
                                                     BraseroTrackType *input,
                                                     BraseroTrackType *output,
                                                     BraseroBurnFlag *supported_ret,
                                                     BraseroBurnFlag *compulsory_ret)
{
	GSList *iter;
	gboolean type_supported;
	BraseroBurnResult result;
	BraseroBurnFlag session_flags;
	BraseroFindLinkCtx ctx = { 0, NULL, 0, };
	BraseroBurnFlag supported_final = BRASERO_BURN_FLAG_NONE;
	BraseroBurnFlag compulsory_final = BRASERO_BURN_FLAG_ALL;

	/* NOTE: there is no need to get the flags here since there are
	 * no specific DISC => IMAGE flags. We just want to know if that
	 * is possible. */
	BRASERO_BURN_LOG_TYPE (output, "Testing temporary image format");

	brasero_caps_find_link_set_ctx (session, &ctx, input);
	ctx.io_flags = BRASERO_PLUGIN_IO_ACCEPT_FILE;

	/* Here there is no need to try blanking as there
	 * is no disc (saves a few debug lines) */
	result = brasero_caps_try_output (self, &ctx, output);
	if (result != BRASERO_BURN_OK) {
		BRASERO_BURN_LOG_TYPE (output, "Format not supported");
		return result;
	}

	session_flags = brasero_burn_session_get_flags (session);

	/* This format can be used to create an image. Check if can be
	 * burnt now. Just find at least one medium. */
	type_supported = FALSE;
	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		BraseroBurnFlag compulsory;
		BraseroBurnFlag supported;
		BraseroBurnResult result;
		BraseroMedia media;
		BraseroCaps *caps;

		caps = iter->data;
		if (!brasero_track_type_get_has_medium (&caps->type))
			continue;

		media = brasero_track_type_get_medium_type (&caps->type);

		/* This type of disc cannot be burnt; skip them */
		if (media & BRASERO_MEDIUM_ROM)
			continue;

		if ((media & BRASERO_MEDIUM_CD) == 0) {
			if (brasero_track_type_get_has_image (output)) {
				BraseroImageFormat format;

				format = brasero_track_type_get_image_format (output);
				/* These three types only work with CDs. */
				if (format == BRASERO_IMAGE_FORMAT_CDRDAO
				||   format == BRASERO_IMAGE_FORMAT_CLONE
				||   format == BRASERO_IMAGE_FORMAT_CUE)
					continue;
			}
			else if (brasero_track_type_get_has_stream (output))
				continue;
		}

		/* Merge all available flags for each possible medium type */
		supported = BRASERO_BURN_FLAG_NONE;
		compulsory = BRASERO_BURN_FLAG_NONE;

		result = brasero_caps_get_flags_for_disc (self,
		                                          brasero_burn_session_get_strict_support (session) == FALSE,
		                                          session_flags,
		                                          media,
							  output,
							  &supported,
							  &compulsory);

		if (result != BRASERO_BURN_OK)
			continue;

		type_supported = TRUE;
		supported_final |= supported;
		compulsory_final &= compulsory;
	}

	BRASERO_BURN_LOG_TYPE (output, "Format supported %i", type_supported);
	if (!type_supported)
		return BRASERO_BURN_NOT_SUPPORTED;

	*supported_ret = supported_final;
	*compulsory_ret = compulsory_final;
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_caps_get_flags_same_src_dest (BraseroBurnCaps *self,
					   BraseroBurnSession *session,
					   BraseroBurnFlag *supported_ret,
					   BraseroBurnFlag *compulsory_ret)
{
	BraseroTrackType input;
	BraseroBurnResult result;
	gboolean copy_supported;
	BraseroTrackType output;
	BraseroImageFormat format;
	BraseroBurnFlag session_flags;
	BraseroBurnFlag supported_final = BRASERO_BURN_FLAG_NONE;
	BraseroBurnFlag compulsory_final = BRASERO_BURN_FLAG_ALL;

	BRASERO_BURN_LOG ("Retrieving disc copy flags with same source and destination");

	/* To determine if a CD/DVD can be copied using the same source/dest,
	 * we first determine if can be imaged and then what are the flags when
	 * we can burn it to a particular medium type. */
	memset (&input, 0, sizeof (BraseroTrackType));
	brasero_burn_session_get_input_type (session, &input);
	BRASERO_BURN_LOG_TYPE (&input, "input");

	session_flags = brasero_burn_session_get_flags (session);
	BRASERO_BURN_LOG_FLAGS (session_flags, "(FLAGS) Session flags");

	/* Check the current flags are possible */
	if (session_flags & (BRASERO_BURN_FLAG_MERGE|BRASERO_BURN_FLAG_NO_TMP_FILES))
		return BRASERO_BURN_NOT_SUPPORTED;

	/* Check for stream type */
	brasero_track_type_set_has_stream (&output);
	/* FIXME! */
	brasero_track_type_set_stream_format (&output,
	                                      BRASERO_AUDIO_FORMAT_RAW|
	                                      BRASERO_METADATA_INFO);

	result = brasero_burn_caps_get_flags_same_src_dest_for_types (self,
	                                                              session,
	                                                              &input,
	                                                              &output,
	                                                              &supported_final,
	                                                              &compulsory_final);
	if (result == BRASERO_BURN_CANCEL)
		return result;

	copy_supported = (result == BRASERO_BURN_OK);

	/* Check flags for all available format */
	format = BRASERO_IMAGE_FORMAT_CDRDAO;
	brasero_track_type_set_has_image (&output);
	for (; format > BRASERO_IMAGE_FORMAT_NONE; format >>= 1) {
		BraseroBurnFlag supported;
		BraseroBurnFlag compulsory;

		/* check if this image type is possible given the current flags */
		if (format != BRASERO_IMAGE_FORMAT_CLONE
		&& (session_flags & BRASERO_BURN_FLAG_RAW))
			continue;

		brasero_track_type_set_image_format (&output, format);

		supported = BRASERO_BURN_FLAG_NONE;
		compulsory = BRASERO_BURN_FLAG_NONE;
		result = brasero_burn_caps_get_flags_same_src_dest_for_types (self,
		                                                              session,
		                                                              &input,
		                                                              &output,
		                                                              &supported,
		                                                              &compulsory);
		if (result == BRASERO_BURN_CANCEL)
			return result;

		if (result != BRASERO_BURN_OK)
			continue;

		copy_supported = TRUE;
		supported_final |= supported;
		compulsory_final &= compulsory;
	}

	if (!copy_supported)
		return BRASERO_BURN_NOT_SUPPORTED;

	*supported_ret |= supported_final;
	*compulsory_ret |= compulsory_final;

	/* we also add these two flags as being supported
	 * since they could be useful and can't be tested
	 * until the disc is inserted which it is not at the
	 * moment */
	(*supported_ret) |= (BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE|
			     BRASERO_BURN_FLAG_FAST_BLANK);

	if (brasero_track_type_get_medium_type (&input) & BRASERO_MEDIUM_HAS_AUDIO) {
		/* This is a special case for audio discs.
		 * Since they may contain CD-TEXT and
		 * since CD-TEXT can only be written with
		 * DAO then we must make sure the user
		 * can't use MULTI since then DAO is
		 * impossible. */
		(*compulsory_ret) |= BRASERO_BURN_FLAG_DAO;

		/* This is just to make sure */
		(*supported_ret) &= (~BRASERO_BURN_FLAG_MULTI);
		(*compulsory_ret) &= (~BRASERO_BURN_FLAG_MULTI);
	}

	return BRASERO_BURN_OK;
}

/**
 * This is meant to use as internal API
 */
BraseroBurnResult
brasero_caps_session_get_image_flags (BraseroTrackType *input,
                                     BraseroTrackType *output,
                                     BraseroBurnFlag *supported,
                                     BraseroBurnFlag *compulsory)
{
	BraseroBurnFlag compulsory_flags = BRASERO_BURN_FLAG_NONE;
	BraseroBurnFlag supported_flags = BRASERO_BURN_FLAG_CHECK_SIZE|BRASERO_BURN_FLAG_NOGRACE;

	BRASERO_BURN_LOG ("FLAGS: image required");

	/* In this case no APPEND/MERGE is possible */
	if (brasero_track_type_get_has_medium (input))
		supported_flags |= BRASERO_BURN_FLAG_EJECT;

	*supported = supported_flags;
	*compulsory = compulsory_flags;

	BRASERO_BURN_LOG_FLAGS (supported_flags, "FLAGS: supported");
	BRASERO_BURN_LOG_FLAGS (compulsory_flags, "FLAGS: compulsory");

	return BRASERO_BURN_OK;
}

/**
 * brasero_burn_session_get_burn_flags:
 * @session: a #BraseroBurnSession
 * @supported: a #BraseroBurnFlag or NULL
 * @compulsory: a #BraseroBurnFlag or NULL
 *
 * Given the various parameters stored in @session, this function
 * stores:
 * - the flags that can be used (@supported)
 * - the flags that must be used (@compulsory)
 * when writing @session to a disc.
 *
 * Return value: a #BraseroBurnResult.
 * BRASERO_BURN_OK if the retrieval was successful.
 * BRASERO_BURN_ERR otherwise.
 **/

BraseroBurnResult
brasero_burn_session_get_burn_flags (BraseroBurnSession *session,
				     BraseroBurnFlag *supported,
				     BraseroBurnFlag *compulsory)
{
	gboolean res;
	BraseroMedia media;
	BraseroBurnCaps *self;
	BraseroTrackType *input;
	BraseroBurnResult result;

	BraseroBurnFlag session_flags;
	/* FIXME: what's the meaning of NOGRACE when outputting ? */
	BraseroBurnFlag compulsory_flags = BRASERO_BURN_FLAG_NONE;
	BraseroBurnFlag supported_flags = BRASERO_BURN_FLAG_CHECK_SIZE|
					  BRASERO_BURN_FLAG_NOGRACE;

	self = brasero_burn_caps_get_default ();

	input = brasero_track_type_new ();
	brasero_burn_session_get_input_type (session, input);
	BRASERO_BURN_LOG_WITH_TYPE (input,
				    BRASERO_PLUGIN_IO_NONE,
				    "FLAGS: searching available flags for input");

	if (brasero_burn_session_is_dest_file (session)) {
		BraseroTrackType *output;

		BRASERO_BURN_LOG ("FLAGS: image required");

		output = brasero_track_type_new ();
		brasero_burn_session_get_output_type (session, output);
		brasero_caps_session_get_image_flags (input, output, supported, compulsory);
		brasero_track_type_free (output);

		brasero_track_type_free (input);
		g_object_unref (self);
		return BRASERO_BURN_OK;
	}

	supported_flags |= BRASERO_BURN_FLAG_EJECT;

	/* special case */
	if (brasero_burn_session_same_src_dest_drive (session)) {
		BRASERO_BURN_LOG ("Same source and destination");
		result = brasero_burn_caps_get_flags_same_src_dest (self,
								    session,
								    &supported_flags,
								    &compulsory_flags);

		/* These flags are of course never possible */
		supported_flags &= ~(BRASERO_BURN_FLAG_NO_TMP_FILES|
				     BRASERO_BURN_FLAG_MERGE);
		compulsory_flags &= ~(BRASERO_BURN_FLAG_NO_TMP_FILES|
				      BRASERO_BURN_FLAG_MERGE);

		if (result == BRASERO_BURN_OK) {
			BRASERO_BURN_LOG_FLAGS (supported_flags, "FLAGS: supported");
			BRASERO_BURN_LOG_FLAGS (compulsory_flags, "FLAGS: compulsory");

			*supported = supported_flags;
			*compulsory = compulsory_flags;
		}
		else
			BRASERO_BURN_LOG ("No available flags for copy");

		brasero_track_type_free (input);
		g_object_unref (self);
		return result;
	}

	session_flags = brasero_burn_session_get_flags (session);
	BRASERO_BURN_LOG_FLAGS (session_flags, "FLAGS (session):");

	/* sanity check:
	 * - drive must support flags
	 * - MERGE and BLANK are not possible together.
	 * - APPEND and MERGE are compatible. MERGE wins
	 * - APPEND and BLANK are incompatible */
	res = brasero_check_flags_for_drive (brasero_burn_session_get_burner (session), session_flags);
	if (!res) {
		BRASERO_BURN_LOG ("Session flags not supported by drive");
		brasero_track_type_free (input);
		g_object_unref (self);
		return BRASERO_BURN_ERR;
	}

	if ((session_flags & (BRASERO_BURN_FLAG_MERGE|BRASERO_BURN_FLAG_APPEND))
	&&  (session_flags & BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE)) {
		brasero_track_type_free (input);
		g_object_unref (self);
		return BRASERO_BURN_NOT_SUPPORTED;
	}
	
	/* Let's get flags for recording */
	media = brasero_burn_session_get_dest_media (session);
	result = brasero_burn_caps_get_flags_for_medium (self,
	                                                 session,
							 media,
							 session_flags,
							 input,
							 &supported_flags,
							 &compulsory_flags);

	brasero_track_type_free (input);
	g_object_unref (self);

	if (result != BRASERO_BURN_OK)
		return result;

	brasero_burn_caps_flags_update_for_drive (session,
	                                          &supported_flags,
	                                          &compulsory_flags);

	if (supported)
		*supported = supported_flags;

	if (compulsory)
		*compulsory = compulsory_flags;

	BRASERO_BURN_LOG_FLAGS (supported_flags, "FLAGS: supported");
	BRASERO_BURN_LOG_FLAGS (compulsory_flags, "FLAGS: compulsory");
	return BRASERO_BURN_OK;
}
