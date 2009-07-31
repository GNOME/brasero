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
		BraseroCaps *caps;
		GSList *links;

		caps = iter->data;
		if (caps->type.type != BRASERO_TRACK_TYPE_DISC)
			continue;

		if ((media & caps->type.subtype.media) != media)
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
				if (!brasero_plugin_get_active (plugin))
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

	if (supported)
		*supported = supported_flags;
	if (compulsory)
		*compulsory = compulsory_flags;

	return BRASERO_BURN_OK;
}

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
							    media,
							    session_flags,
							    supported,
							    compulsory);
	g_object_unref (self);

	return result;
}

static BraseroBurnResult
brasero_burn_caps_can_blank_real (BraseroBurnCaps *self,
				  BraseroMedia media,
				  BraseroBurnFlag flags)
{
	GSList *iter;

	BRASERO_BURN_LOG_DISC_TYPE (media, "Testing blanking caps for");
	if (media == BRASERO_MEDIUM_NONE) {
		BRASERO_BURN_LOG ("no media => no blanking possible");
		return BRASERO_BURN_NOT_SUPPORTED;
	}

	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		BraseroCaps *caps;
		GSList *links;

		caps = iter->data;
		if (caps->type.type != BRASERO_TRACK_TYPE_DISC)
			continue;

		if ((media & caps->type.subtype.media) != media)
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
				if (!brasero_plugin_get_active (plugin))
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
	result = brasero_burn_caps_can_blank_real (self, media, flags);
	g_object_unref (self);

	return result;
}

/**
 *
 */

static void
brasero_caps_link_get_record_flags (BraseroCapsLink *link,
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
		if (!brasero_plugin_get_active (plugin))
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
				  BraseroMedia media,
				  BraseroBurnFlag session_flags,
				  BraseroBurnFlag *supported)
{
	GSList *iter;

	/* Go through all plugins the get the supported/... data flags for link */
	for (iter = link->plugins; iter; iter = iter->next) {
		gboolean result;
		BraseroPlugin *plugin;
		BraseroBurnFlag plugin_supported;
		BraseroBurnFlag plugin_compulsory;

		plugin = iter->data;
		if (!brasero_plugin_get_active (plugin))
			continue;

		result = brasero_plugin_get_image_flags (plugin,
							 media,
							 session_flags,
							 &plugin_supported,
							 &plugin_compulsory);
		*supported |= plugin_supported;
	}
}

static gboolean
brasero_caps_link_check_data_flags (BraseroCapsLink *link,
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
		if (!brasero_plugin_get_active (plugin))
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
		if (!brasero_plugin_get_active (plugin))
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
					    BraseroMedia media)
{
	GSList *iter;

	/* Go through all plugins: at least one must support media */
	for (iter = link->plugins; iter; iter = iter->next) {
		gboolean result;
		BraseroPlugin *plugin;

		plugin = iter->data;
		if (!brasero_plugin_get_active (plugin))
			continue;

		result = brasero_plugin_check_media_restrictions (plugin, media);
		if (result)
			return TRUE;
	}

	return FALSE;
}

static gboolean
brasero_caps_find_link (BraseroCaps *caps,
			BraseroBurnFlag session_flags,
			gboolean use_flags,
			BraseroMedia media,
			BraseroTrackType *input,
			BraseroPluginIOFlag io_flags)
{
	GSList *iter;

	BRASERO_BURN_LOG_WITH_TYPE (&caps->type, BRASERO_PLUGIN_IO_NONE, "find_link");

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
		gboolean result;

		link = iter->data;

		if (!link->caps)
			continue;

		/* check that the link has some active plugin */
		if (!brasero_caps_link_active (link))
			continue;

		/* since this link contains recorders, check that at least one
		 * of them can handle the record flags */
		if (use_flags
		&&  caps->type.type == BRASERO_TRACK_TYPE_DISC
		&& !brasero_caps_link_check_record_flags (link, session_flags, media))
			continue;

		/* first see if that's the perfect fit:
		 * - it must have the same caps (type + subtype)
		 * - it must have the proper IO */
		if (link->caps->type.type == BRASERO_TRACK_TYPE_DATA) {
			if (use_flags
			&& !brasero_caps_link_check_data_flags (link, session_flags, media))
				continue;
		}
		else if (!brasero_caps_link_check_media_restrictions (link, media))
			continue;

		if ((link->caps->flags & BRASERO_PLUGIN_IO_ACCEPT_FILE)
		&&   brasero_caps_is_compatible_type (link->caps, input))
			return TRUE;

		/* we can't go further than a DISC type */
		if (link->caps->type.type == BRASERO_TRACK_TYPE_DISC)
			continue;

		if ((link->caps->flags & io_flags) == BRASERO_PLUGIN_IO_NONE)
			continue;

		/* try to see where the inputs of this caps leads to */
		result = brasero_caps_find_link (link->caps,
						 session_flags,
						 use_flags,
						 media,
						 input,
						 io_flags);
		if (result)
			return TRUE;
	}

	return FALSE;
}

static gboolean
brasero_caps_try_output (BraseroBurnCaps *self,
			 BraseroBurnFlag session_flags,
			 gboolean use_flags,
			 BraseroTrackType *output,
			 BraseroTrackType *input,
			 BraseroPluginIOFlag flags)
{
	BraseroCaps *caps;
	BraseroMedia media;

	/* here we search the start caps */
	caps = brasero_burn_caps_find_start_caps (self, output);
	if (!caps) {
		BRASERO_BURN_LOG ("No caps available");
		return FALSE;
	}

	if (output->type == BRASERO_TRACK_TYPE_DISC)
		media = output->subtype.media;
	else
		media = BRASERO_MEDIUM_FILE;

	return brasero_caps_find_link (caps,
				       session_flags,
				       use_flags,
				       media,
				       input,
				       flags);
}

static gboolean
brasero_caps_try_output_with_blanking (BraseroBurnCaps *self,
				       BraseroBurnSession *session,
				       BraseroTrackType *output,
				       BraseroTrackType *input,
				       BraseroPluginIOFlag io_flags,
				       gboolean use_flags)
{
	gboolean result;
	BraseroMedia media;
	BraseroCaps *last_caps;
	BraseroBurnFlag session_flags = BRASERO_BURN_FLAG_NONE;

	if (use_flags)
		session_flags = brasero_burn_session_get_flags (session);

	result = brasero_caps_try_output (self,
					  session_flags,
					  use_flags,
					  output,
					  input,
					  io_flags);
	if (result)
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
	if (output->type != BRASERO_TRACK_TYPE_DISC)
		return FALSE;

	/* output is a disc try with initial blanking */
	BRASERO_BURN_LOG ("Support for input/output failed.");

	/* apparently nothing can be done to reach our goal. Maybe that
	 * is because we first have to blank the disc. If so add a blank 
	 * task to the others as a first step */
	if ((use_flags && !(session_flags & BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE))
	||   brasero_burn_session_can_blank (session) != BRASERO_BURN_OK)
		return FALSE;

	BRASERO_BURN_LOG ("Trying with initial blanking");

	/* retry with the same disc type but blank this time */
	media = output->subtype.media;
	media &= ~(BRASERO_MEDIUM_CLOSED|
		   BRASERO_MEDIUM_APPENDABLE|
		   BRASERO_MEDIUM_UNFORMATTED|
		   BRASERO_MEDIUM_HAS_DATA|
		   BRASERO_MEDIUM_HAS_AUDIO);
	media |= BRASERO_MEDIUM_BLANK;
	output->subtype.media = media;

	last_caps = brasero_burn_caps_find_start_caps (self, output);
	if (!last_caps)
		return FALSE;

	return brasero_caps_find_link (last_caps,
				       session_flags,
				       use_flags,
				       media,
				       input,
				       io_flags);
}

BraseroBurnResult
brasero_burn_session_input_supported (BraseroBurnSession *session,
				      BraseroTrackType *input,
				      gboolean use_flags)
{
	gboolean result;
	BraseroBurnCaps *self;
	BraseroTrackType output;
	BraseroPluginIOFlag io_flags;

	if (use_flags) {
		result = brasero_check_flags_for_drive (brasero_burn_session_get_burner (session),
							brasero_burn_session_get_flags (session));

		if (!result)
			BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG_RES (session);
	}

	self = brasero_burn_caps_get_default ();
	if (!brasero_burn_session_is_dest_file (session)) {
		output.type = BRASERO_TRACK_TYPE_DISC;
		output.subtype.media = brasero_burn_session_get_dest_media (session);

		/* NOTE: for the special case where a disc could be rewritten
		 * and cannot be handled as such but needs prior blanking, we
		 * handle that situation in previous function.*/
	}
	else {
		output.type = BRASERO_TRACK_TYPE_IMAGE;
		output.subtype.img_format = brasero_burn_session_get_output_format (session);
	}

	if (BRASERO_BURN_SESSION_NO_TMP_FILE (session))
		io_flags = BRASERO_PLUGIN_IO_ACCEPT_PIPE;
	else
		io_flags = BRASERO_PLUGIN_IO_ACCEPT_FILE;

	BRASERO_BURN_LOG_TYPE (input, "Checking support for input");
	BRASERO_BURN_LOG_TYPE (&output, "and output");

	if (use_flags)
		BRASERO_BURN_LOG_FLAGS (brasero_burn_session_get_flags (session), "with flags");

	result = brasero_caps_try_output_with_blanking (self,
							session,
							&output,
							input,
							io_flags,
							use_flags);
	g_object_unref (self);

	if (!result) {
		BRASERO_BURN_LOG_TYPE (input, "Input not supported");
		return BRASERO_BURN_NOT_SUPPORTED;
	}

	return BRASERO_BURN_OK;
}

/**
 * brasero_burn_session_output_supported:
 * @session: a #BraseroBurnSession *
 * @output: a #BraseroTrackType *
 *
 * Make sure that the image type or medium type defined in @output can be
 * created/burnt given the parameters set in @session.
 *
 * Return value: BRASERO_BURN_OK if the medium type or the image type can be used as an output.
 **/
BraseroBurnResult
brasero_burn_session_output_supported (BraseroBurnSession *session,
				       BraseroTrackType *output)
{
	gboolean result;
	BraseroBurnCaps *self;
	BraseroTrackType input;
	BraseroPluginIOFlag io_flags;

	self = brasero_burn_caps_get_default ();

	/* Here, we can't check if the drive supports the flags since the output
	 * is hypothetical. There is no real medium. So forget the following :
	 * if (!brasero_burn_caps_flags_check_for_drive (session))
	 *	BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG_RES (session);
	 * The only thing we could do would be to check some known forbidden 
	 * flags for some media provided the output type is DISC. */

	/* Here flags don't matter as we don't record anything. Even the IOFlags
	 * since that can be checked later with brasero_burn_caps_get_flags. */
	if (BRASERO_BURN_SESSION_NO_TMP_FILE (session))
		io_flags = BRASERO_PLUGIN_IO_ACCEPT_PIPE;
	else
		io_flags = BRASERO_PLUGIN_IO_ACCEPT_FILE;

	brasero_burn_session_get_input_type (session, &input);
	BRASERO_BURN_LOG_TYPE (output, "Checking support for output");
	BRASERO_BURN_LOG_TYPE (&input, "and input");
	BRASERO_BURN_LOG_FLAGS (brasero_burn_session_get_flags (session), "with flags");
	
	result = brasero_caps_try_output_with_blanking (self,
							session,
							output,
							&input,
							io_flags,
							TRUE);

	g_object_unref (self);

	if (!result) {
		BRASERO_BURN_LOG_TYPE (output, "Output not supported");
		return BRASERO_BURN_NOT_SUPPORTED;
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
						      gboolean use_flags)
{
	GSList *iter;
	BraseroTrackType input;
	BraseroTrackType output;
	BraseroImageFormat format;
	BraseroBurnFlag session_flags;

	BRASERO_BURN_LOG ("Checking disc copy support with same source and destination");

	/* To determine if a CD/DVD can be copied using the same source/dest,
	 * we first determine if can be imaged and then if this image can be 
	 * burnt to whatever medium type. */
	memset (&input, 0, sizeof (BraseroTrackType));
	brasero_burn_session_get_input_type (session, &input);
	BRASERO_BURN_LOG_TYPE (&input, "input");

	if (use_flags) {
		/* NOTE: DAO can be a problem. So just in case remove it. It is
		 * not really useful in this context. What we want here is to
		 * know whether a medium can be used given the input; only 1
		 * flag is important here (MERGE) and can have consequences. */
		session_flags = brasero_burn_session_get_flags (session);
		session_flags &= ~BRASERO_BURN_FLAG_DAO;

		BRASERO_BURN_LOG_FLAGS (session_flags, "flags");
	}
	else
		session_flags = BRASERO_BURN_FLAG_NONE;

	/* Find one available output format */
	format = BRASERO_IMAGE_FORMAT_CDRDAO;
	output.type = BRASERO_TRACK_TYPE_IMAGE;

	for (; format > BRASERO_IMAGE_FORMAT_NONE; format >>= 1) {
		gboolean supported;

		output.subtype.img_format = format;

		BRASERO_BURN_LOG_TYPE (&output, "Testing temporary image format");
		supported = brasero_caps_try_output_with_blanking (self,
								   session,
								   &output,
								   &input,
								   BRASERO_PLUGIN_IO_ACCEPT_FILE,
								   use_flags);
		if (!supported)
			continue;

		/* This format can be used to create an image. Check if can be
		 * burnt now. Just find at least one medium. */
		for (iter = self->priv->caps_list; iter; iter = iter->next) {
			BraseroCaps *caps;
			gboolean result;

			caps = iter->data;

			if (caps->type.type != BRASERO_TRACK_TYPE_DISC)
				continue;

			result = brasero_caps_find_link (caps,
							 use_flags,
							 session_flags,
							 caps->type.subtype.media,
							 &output,
							 BRASERO_PLUGIN_IO_ACCEPT_FILE);

			BRASERO_BURN_LOG_DISC_TYPE (caps->type.subtype.media,
						    "Tested medium (%s)",
						    result ? "working":"not working");

			if (result)
				return BRASERO_BURN_OK;
		}
	}

	return BRASERO_BURN_NOT_SUPPORTED;
}

BraseroBurnResult
brasero_burn_session_can_burn (BraseroBurnSession *session,
			       gboolean use_flags)
{
	gboolean result;
	BraseroBurnCaps *self;
	BraseroTrackType input;
	BraseroTrackType output;
	BraseroPluginIOFlag io_flags;

	self = brasero_burn_caps_get_default ();

	/* Special case */
	if (brasero_burn_session_same_src_dest_drive (session)) {
		BraseroBurnResult res;

		res = brasero_burn_caps_is_session_supported_same_src_dest (self, session, use_flags);
		g_object_unref (self);
		return res;
	}

	if (use_flags) {
		result = brasero_check_flags_for_drive (brasero_burn_session_get_burner (session),
							brasero_burn_session_get_flags (session));

		if (!result) {
			g_object_unref (self);
			BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG_RES (session);
		}
	}

	/* Here flags don't matter as we don't record anything.
	 * Even the IOFlags since that can be checked later with
	 * brasero_burn_caps_get_flags. */
	if (BRASERO_BURN_SESSION_NO_TMP_FILE (session))
		io_flags = BRASERO_PLUGIN_IO_ACCEPT_PIPE;
	else
		io_flags = BRASERO_PLUGIN_IO_ACCEPT_FILE;

	brasero_burn_session_get_input_type (session, &input);

	if (!brasero_burn_session_is_dest_file (session)) {
		output.type = BRASERO_TRACK_TYPE_DISC;
		output.subtype.media = brasero_burn_session_get_dest_media (session);

		/* NOTE: for the special case where a disc could be rewritten
		 * and cannot be handled as such but needs prior blanking, we
		 * handle that situation in previous function.*/
	}
	else {
		output.type = BRASERO_TRACK_TYPE_IMAGE;
		output.subtype.img_format = brasero_burn_session_get_output_format (session);
	}

	BRASERO_BURN_LOG_TYPE (&output, "Checking support for session output");
	BRASERO_BURN_LOG_TYPE (&input, "and input");

	if (use_flags)
		BRASERO_BURN_LOG_FLAGS (brasero_burn_session_get_flags (session), "with flags");

	result = brasero_caps_try_output_with_blanking (self,
							session,
							&output,
							&input,
							io_flags,
							use_flags);

	g_object_unref (self);

	if (!result) {
		BRASERO_BURN_LOG_TYPE (&output, "Output not supported");
		return BRASERO_BURN_NOT_SUPPORTED;
	}

	return BRASERO_BURN_OK;
}

BraseroMedia
brasero_burn_session_get_required_media_type (BraseroBurnSession *session)
{
	BraseroMedia required_media = BRASERO_MEDIUM_NONE;
	BraseroBurnFlag session_flags;
	BraseroPluginIOFlag io_flags;
	BraseroTrackType input;
	BraseroBurnCaps *self;
	GSList *iter;

	if (brasero_burn_session_is_dest_file (session))
		return BRASERO_MEDIUM_FILE;

	self = brasero_burn_caps_get_default ();

	/* we try to determine here what type of medium is allowed to be burnt
	 * to whether a CD or a DVD. Appendable, blank are not properties being
	 * determined here. We just want it to be writable in a broad sense. */
	brasero_burn_session_get_input_type (session, &input);
	BRASERO_BURN_LOG_TYPE (&input, "Determining required media type for input");

	/* NOTE: BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE is a problem here since it
	 * is only used if needed. Likewise DAO can be a problem. So just in
	 * case remove them. They are not really useful in this context. What we
	 * want here is to know which media can be used given the input; only 1
	 * flag is important here (MERGE) and can have consequences. */
	session_flags = brasero_burn_session_get_flags (session);
	session_flags &= ~BRASERO_BURN_FLAG_DAO;

	BRASERO_BURN_LOG_FLAGS (session_flags, "and flags");

	if (BRASERO_BURN_SESSION_NO_TMP_FILE (session))
		io_flags = BRASERO_PLUGIN_IO_ACCEPT_PIPE;
	else
		io_flags = BRASERO_PLUGIN_IO_ACCEPT_FILE;

	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		BraseroCaps *caps;
		gboolean result;

		caps = iter->data;

		if (caps->type.type != BRASERO_TRACK_TYPE_DISC)
			continue;

		/* Put BRASERO_MEDIUM_NONE so we can always succeed */
		result = brasero_caps_find_link (caps,
						 session_flags,
						 TRUE,
						 BRASERO_MEDIUM_NONE,
						 &input,
						 io_flags);

		BRASERO_BURN_LOG_DISC_TYPE (caps->type.subtype.media,
					    "Tested (%s)",
					    result ? "working":"not working");

		if (!result)
			continue;

		/* This caps work, add its subtype */
		required_media |= caps->type.subtype.media;
	}

	/* filter as we are only interested in these */
	required_media &= BRASERO_MEDIUM_WRITABLE|
			  BRASERO_MEDIUM_CD|
			  BRASERO_MEDIUM_DVD;

	g_object_unref (self);
	return required_media;
}

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
	if (source.type == BRASERO_TRACK_TYPE_NONE) {
		g_object_unref (self);
		return BRASERO_IMAGE_FORMAT_NONE;
	}

	if (source.type == BRASERO_TRACK_TYPE_IMAGE) {
		g_object_unref (self);
		return source.subtype.img_format;
	}

	output.type = BRASERO_TRACK_TYPE_IMAGE;
	output.subtype.img_format = BRASERO_IMAGE_FORMAT_NONE;

	if (source.type == BRASERO_TRACK_TYPE_STREAM) {
		/* If that's AUDIO only without VIDEO then return */
		if (!(source.subtype.stream_format & (BRASERO_VIDEO_FORMAT_UNDEFINED|BRASERO_VIDEO_FORMAT_VCD|BRASERO_VIDEO_FORMAT_VIDEO_DVD))) {
			g_object_unref (self);
			return BRASERO_IMAGE_FORMAT_NONE;
		}

		/* Otherwise try all possible image types */
		output.subtype.img_format = BRASERO_IMAGE_FORMAT_CDRDAO;
		for (; output.subtype.img_format != BRASERO_IMAGE_FORMAT_NONE;
		       output.subtype.img_format >>= 1) {
		
			result = brasero_burn_session_output_supported (session,
									&output);
			if (result == BRASERO_BURN_OK) {
				g_object_unref (self);
				return output.subtype.img_format;
			}
		}

		g_object_unref (self);
		return BRASERO_IMAGE_FORMAT_NONE;
	}

	if (source.type == BRASERO_TRACK_TYPE_DATA
	|| (source.type == BRASERO_TRACK_TYPE_DISC
	&& (source.subtype.media & BRASERO_MEDIUM_DVD))) {
		output.subtype.img_format = BRASERO_IMAGE_FORMAT_BIN;
		result = brasero_burn_session_output_supported (session,
								&output);

		g_object_unref (self);

		if (result != BRASERO_BURN_OK)
			return BRASERO_IMAGE_FORMAT_NONE;

		return BRASERO_IMAGE_FORMAT_BIN;
	}

	/* for the input which are CDs there are lots of possible formats */
	output.subtype.img_format = BRASERO_IMAGE_FORMAT_CDRDAO;
	for (; output.subtype.img_format != BRASERO_IMAGE_FORMAT_NONE;
	       output.subtype.img_format >>= 1) {
	
		result = brasero_burn_session_output_supported (session,
								&output);
		if (result == BRASERO_BURN_OK) {
			g_object_unref (self);
			return output.subtype.img_format;
		}
	}

	g_object_unref (self);
	return BRASERO_IMAGE_FORMAT_NONE;
}

static BraseroPluginIOFlag
brasero_caps_get_flags (BraseroCaps *caps,
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
		if (!brasero_caps_link_active (link))
			continue;

		if (caps->type.type == BRASERO_TRACK_TYPE_DISC) {
			BraseroBurnFlag tmp;

			brasero_caps_link_get_record_flags (link,
							    media,
							    session_flags,
							    &rec_supported,
							    &rec_compulsory);

			/* see if that link can handle the record flags.
			 * NOTE: compulsory are not a failure in this case. */
			tmp = session_flags & BRASERO_PLUGIN_BURN_FLAG_MASK;
			if ((tmp & rec_supported) != tmp)
				continue;
		}

		if (link->caps->type.type == BRASERO_TRACK_TYPE_DATA) {
			BraseroBurnFlag tmp;

			brasero_caps_link_get_data_flags (link,
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
		else if (!brasero_caps_link_check_media_restrictions (link, media))
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
						   session_flags,
						   media,
						   input,
						   flags,
						   supported,
						   compulsory);
		if (io_flags == BRASERO_PLUGIN_IO_NONE)
			continue;

		retval |= (io_flags & flags);
		(*compulsory) &= rec_compulsory;
		(*supported) |= data_supported|rec_supported;
	}

	return retval;
}

static BraseroBurnFlag
brasero_medium_supported_flags (BraseroMedium *medium,
				BraseroBurnFlag flags)
{
	BraseroMedia media;

	media = brasero_medium_get_status (medium);

	/* This is always FALSE */
	if (media & BRASERO_MEDIUM_PLUS)
		flags &= ~BRASERO_BURN_FLAG_DUMMY;

	/* Simulation is only possible according to write modes. This mode is
	 * mostly used by cdrecord/wodim for CLONE images. */
	else if (media & BRASERO_MEDIUM_DVD) {
		if (!brasero_medium_can_use_dummy_for_sao (medium))
			flags &= ~BRASERO_BURN_FLAG_DUMMY;
	}
	else if (flags & BRASERO_BURN_FLAG_DAO) {
		if (!brasero_medium_can_use_dummy_for_sao (medium))
			flags &= ~BRASERO_BURN_FLAG_DUMMY;
	}
	else if (!brasero_medium_can_use_dummy_for_tao (medium))
		flags &= ~BRASERO_BURN_FLAG_DUMMY;

	if (!brasero_medium_can_use_burnfree (medium))
		flags &= ~BRASERO_BURN_FLAG_BURNPROOF;

	return flags;
}

static BraseroBurnFlag
brasero_burn_caps_flags_update_for_drive (BraseroBurnFlag flags,
					  BraseroBurnSession *session)
{
	BraseroDrive *drive;
	BraseroMedium *medium;

	drive = brasero_burn_session_get_burner (session);
	if (!drive)
		return flags;

	medium = brasero_drive_get_medium (drive);
	if (!medium)
		return TRUE;

	return brasero_medium_supported_flags (medium, flags);
}

static BraseroBurnResult
brasero_caps_get_flags_for_disc (BraseroBurnCaps *self,
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
	output.type = BRASERO_TRACK_TYPE_DISC;
	output.subtype.media = media;

	caps = brasero_burn_caps_find_start_caps (self, &output);
	if (!caps) {
		BRASERO_BURN_LOG_DISC_TYPE (media, "FLAGS: no caps could be found for");
		return BRASERO_BURN_NOT_SUPPORTED;
	}

	BRASERO_BURN_LOG_WITH_TYPE (&caps->type,
				    caps->flags,
				    "FLAGS: trying caps");

	io_flags = brasero_caps_get_flags (caps,
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

	/* RAW write mode should (must) only be used in this case */
	if ((supported_flags & BRASERO_BURN_FLAG_RAW)
	&&   input->type == BRASERO_TRACK_TYPE_IMAGE
	&&   input->subtype.img_format == BRASERO_IMAGE_FORMAT_CLONE) {
		supported_flags &= ~BRASERO_BURN_FLAG_DAO;
		compulsory_flags &= ~BRASERO_BURN_FLAG_DAO;
		compulsory_flags |= BRASERO_BURN_FLAG_RAW;
	}
	else
		supported_flags &= ~BRASERO_BURN_FLAG_RAW;

	if ((supported_flags & BRASERO_BURN_FLAG_DAO)
	&&   input->type == BRASERO_TRACK_TYPE_STREAM
	&&  (input->subtype.img_format & BRASERO_METADATA_INFO)) {
		/* In this case, DAO is compulsory if we want to write CD-TEXT */
		compulsory_flags |= BRASERO_BURN_FLAG_DAO;
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
					BraseroMedia media,
					BraseroBurnFlag session_flags,
					BraseroTrackType *input,
					BraseroBurnFlag *supported_flags,
					BraseroBurnFlag *compulsory_flags)
{
	BraseroBurnResult result;

	/* See if medium is supported out of the box */
	result = brasero_caps_get_flags_for_disc (self,
						  session_flags,
						  media,
						  input,
						  supported_flags,
						  compulsory_flags);

	/* see if we can add BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE. Add it when:
	 * - media can be blanked, it has audio or data and we're not merging
	 * - media is not formatted and it can be blanked/formatted */
	if (BRASERO_BURN_CAPS_SHOULD_BLANK (media, session_flags)
	&&  brasero_burn_caps_can_blank_real (self, media, session_flags) == BRASERO_BURN_OK)
		(*supported_flags) |= BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE;
	else if (session_flags & BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE)
		return BRASERO_BURN_NOT_SUPPORTED;

	if (((*supported_flags) & BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE) != 0) {
		BraseroBurnFlag blank_compulsory = BRASERO_BURN_FLAG_NONE;
		BraseroBurnFlag blank_supported = BRASERO_BURN_FLAG_NONE;

		/* If BLANK flag is supported then MERGE/APPEND can't be compulsory */
		(*compulsory_flags) &= ~(BRASERO_BURN_FLAG_MERGE|BRASERO_BURN_FLAG_APPEND);

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
		if (result != BRASERO_BURN_OK)
			(*compulsory_flags) |= BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE;

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
							  session_flags,
							  media,
							  input,
							  supported_flags,
							  compulsory_flags);

		/* if both attempts failed, drop it */
		if (result != BRASERO_BURN_OK
		&& (((*compulsory_flags) & BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE)))
			return result;

		/* need to add blanking flags */
		brasero_burn_caps_get_blanking_flags_real (self,
							   media,
							   session_flags,
							   &blank_supported,
							   &blank_compulsory);
		(*supported_flags) |= blank_supported;
		(*compulsory_flags) |= blank_compulsory;
	}
	else if (result != BRASERO_BURN_OK)
		return result;

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
brasero_burn_caps_get_flags_same_src_dest (BraseroBurnCaps *self,
					   BraseroBurnSession *session,
					   BraseroBurnFlag *supported_ret,
					   BraseroBurnFlag *compulsory_ret)
{
	GSList *iter;
	gboolean copy_supported;
	BraseroTrackType input;
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

	/* Check flags for all available format */
	format = BRASERO_IMAGE_FORMAT_CDRDAO;
	output.type = BRASERO_TRACK_TYPE_IMAGE;

	copy_supported = FALSE;
	for (; format > BRASERO_IMAGE_FORMAT_NONE; format >>= 1) {
		BraseroBurnResult result;
		gboolean format_supported;

		/* check this image type is possible given the current flags */
		if (format != BRASERO_IMAGE_FORMAT_CLONE
		&& (session_flags & BRASERO_BURN_FLAG_RAW))
			continue;

		output.subtype.img_format = format;

		/* NOTE: there is no need to get the flags here since there are
		 * no specific DISC => IMAGE flags. We just want to know if that
		 * is possible. */
		BRASERO_BURN_LOG_TYPE (&output, "Testing temporary image format");
		format_supported = brasero_caps_try_output_with_blanking (self,
									  session,
									  &output,
									  &input,
									  BRASERO_PLUGIN_IO_ACCEPT_FILE,
									  FALSE);
		if (!format_supported) {
			BRASERO_BURN_LOG_TYPE (&output, "Format not supported");
			continue;
		}

		/* This format can be used to create an image. Check if can be
		 * burnt now. Just find at least one medium. */
		format_supported = FALSE;
		for (iter = self->priv->caps_list; iter; iter = iter->next) {
			BraseroBurnFlag compulsory;
			BraseroBurnFlag supported;
			BraseroCaps *caps;

			caps = iter->data;
			if (caps->type.type != BRASERO_TRACK_TYPE_DISC)
				continue;

			/* Merge all available flags for each possible medium type */
			supported = BRASERO_BURN_FLAG_NONE;
			compulsory = BRASERO_BURN_FLAG_NONE;
			result = brasero_burn_caps_get_flags_for_medium (self,
									 caps->type.subtype.media,
									 session_flags,
									 &output,
									 &supported,
									 &compulsory);
			if (result != BRASERO_BURN_OK)
				continue;

			format_supported = TRUE;
			supported_final |= supported;
			compulsory_final &= compulsory;
		}

		BRASERO_BURN_LOG_TYPE (&output, "Format supported %i", format_supported);
		if (format_supported)
			copy_supported = TRUE;
	}

	if (!copy_supported)
		return BRASERO_BURN_NOT_SUPPORTED;

	*supported_ret |= supported_final;
	*compulsory_ret |= compulsory_final;
	
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_burn_session_get_burn_flags (BraseroBurnSession *session,
				     BraseroBurnFlag *supported,
				     BraseroBurnFlag *compulsory)
{
	gboolean res;
	BraseroMedia media;
	BraseroBurnCaps *self;
	BraseroTrackType input;
	BraseroBurnResult result;

	BraseroBurnFlag session_flags;
	/* FIXME: what's the meaning of NOGRACE when outputting ? */
	BraseroBurnFlag compulsory_flags = BRASERO_BURN_FLAG_NONE;
	BraseroBurnFlag supported_flags = BRASERO_BURN_FLAG_CHECK_SIZE|
					  BRASERO_BURN_FLAG_NOGRACE;

	self = brasero_burn_caps_get_default ();

	brasero_burn_session_get_input_type (session, &input);
	BRASERO_BURN_LOG_WITH_TYPE (&input,
				    BRASERO_PLUGIN_IO_NONE,
				    "FLAGS: searching available flags for input");

	if (brasero_burn_session_is_dest_file (session)) {
		BRASERO_BURN_LOG ("FLAGS: image required");

		/* In this case no APPEND/MERGE is possible */
		if (input.type == BRASERO_TRACK_TYPE_DISC)
			supported_flags |= BRASERO_BURN_FLAG_EJECT;

		*supported = supported_flags;
		*compulsory = compulsory_flags;

		BRASERO_BURN_LOG_FLAGS (supported_flags, "FLAGS: supported");
		BRASERO_BURN_LOG_FLAGS (compulsory_flags, "FLAGS: compulsory");

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
		g_object_unref (self);
		return BRASERO_BURN_ERR;
	}

	if ((session_flags & (BRASERO_BURN_FLAG_MERGE|BRASERO_BURN_FLAG_APPEND))
	&&  (session_flags & BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE)) {
		g_object_unref (self);
		return BRASERO_BURN_NOT_SUPPORTED;
	}
	
	/* Let's get flags for recording */
	media = brasero_burn_session_get_dest_media (session);
	result = brasero_burn_caps_get_flags_for_medium (self,
							 media,
							 session_flags,
							 &input,
							 &supported_flags,
							 &compulsory_flags);

	g_object_unref (self);

	if (result != BRASERO_BURN_OK)
		return result;

	supported_flags = brasero_burn_caps_flags_update_for_drive (supported_flags,
								    session);

	if (supported)
		*supported = supported_flags;

	if (compulsory)
		*compulsory = compulsory_flags;

	BRASERO_BURN_LOG_FLAGS (supported_flags, "FLAGS: supported");
	BRASERO_BURN_LOG_FLAGS (compulsory_flags, "FLAGS: compulsory");
	return BRASERO_BURN_OK;
}
