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
#include <glib/gi18n.h>

#include "brasero-media-private.h"

#include "burn-basics.h"
#include "burn-debug.h"
#include "brasero-drive.h"
#include "brasero-medium.h"
#include "brasero-session.h"
#include "brasero-plugin.h"
#include "brasero-plugin-information.h"
#include "burn-task.h"
#include "burn-caps.h"
#include "brasero-track-type-private.h"

#define BRASERO_ENGINE_GROUP_KEY	"engine-group"
#define BRASERO_SCHEMA_CONFIG		"org.gnome.brasero.config"

G_DEFINE_TYPE (BraseroBurnCaps, brasero_burn_caps, G_TYPE_OBJECT);

static GObjectClass *parent_class = NULL;


static void
brasero_caps_link_free (BraseroCapsLink *link)
{
	g_slist_free (link->plugins);
	g_free (link);
}

BraseroBurnResult
brasero_caps_link_check_recorder_flags_for_input (BraseroCapsLink *link,
                                                  BraseroBurnFlag session_flags)
{
	if (brasero_track_type_get_has_image (&link->caps->type)) {
		BraseroImageFormat format;

		format = brasero_track_type_get_image_format (&link->caps->type);
		if (format == BRASERO_IMAGE_FORMAT_CUE
		||  format == BRASERO_IMAGE_FORMAT_CDRDAO) {
			if ((session_flags & BRASERO_BURN_FLAG_DAO) == 0)
				return BRASERO_BURN_NOT_SUPPORTED;
		}
		else if (format == BRASERO_IMAGE_FORMAT_CLONE) {
			/* RAW write mode should (must) only be used in this case */
			if ((session_flags & BRASERO_BURN_FLAG_RAW) == 0)
				return BRASERO_BURN_NOT_SUPPORTED;
		}
	}

	return BRASERO_BURN_OK;
}

gboolean
brasero_caps_link_active (BraseroCapsLink *link,
                          gboolean ignore_plugin_errors)
{
	GSList *iter;

	/* See if link is active by going through all plugins. There must be at
	 * least one. */
	for (iter = link->plugins; iter; iter = iter->next) {
		BraseroPlugin *plugin;

		plugin = iter->data;
		if (brasero_plugin_get_active (plugin, ignore_plugin_errors))
			return TRUE;
	}

	return FALSE;
}

BraseroPlugin *
brasero_caps_link_need_download (BraseroCapsLink *link)
{
	GSList *iter;
	BraseroPlugin *plugin_ret = NULL;

	/* See if for link to be active, we need to 
	 * download additional apps/libs/.... */
	for (iter = link->plugins; iter; iter = iter->next) {
		BraseroPlugin *plugin;

		plugin = iter->data;

		/* If a plugin can be used without any
		 * error then that means that the link
		 * can be followed without additional
		 * download. */
		if (brasero_plugin_get_active (plugin, FALSE))
			return NULL;

		if (brasero_plugin_get_active (plugin, TRUE)) {
			if (!plugin_ret)
				plugin_ret = plugin;
			else if (brasero_plugin_get_priority (plugin) > brasero_plugin_get_priority (plugin_ret))
				plugin_ret = plugin;
		}
	}

	return plugin_ret;
}

static void
brasero_caps_test_free (BraseroCapsTest *caps)
{
	g_slist_foreach (caps->links, (GFunc) brasero_caps_link_free, NULL);
	g_slist_free (caps->links);
	g_free (caps);
}

static void
brasero_caps_free (BraseroCaps *caps)
{
	g_slist_free (caps->modifiers);
	g_slist_foreach (caps->links, (GFunc) brasero_caps_link_free, NULL);
	g_slist_free (caps->links);
	g_free (caps);
}

static gboolean
brasero_caps_has_active_input (BraseroCaps *caps,
			       BraseroCaps *input)
{
	GSList *links;

	for (links = caps->links; links; links = links->next) {
		BraseroCapsLink *link;

		link = links->data;
		if (link->caps != input)
			continue;

		/* Ignore plugin errors */
		if (brasero_caps_link_active (link, TRUE))
			return TRUE;
	}

	return FALSE;
}

gboolean
brasero_burn_caps_is_input (BraseroBurnCaps *self,
			    BraseroCaps *input)
{
	GSList *iter;

	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		BraseroCaps *tmp;

		tmp = iter->data;
		if (tmp == input)
			continue;

		if (brasero_caps_has_active_input (tmp, input))
			return TRUE;
	}

	return FALSE;
}

gboolean
brasero_caps_is_compatible_type (const BraseroCaps *caps,
				 const BraseroTrackType *type)
{
	if (caps->type.type != type->type)
		return FALSE;

	switch (type->type) {
	case BRASERO_TRACK_TYPE_DATA:
		if ((caps->type.subtype.fs_type & type->subtype.fs_type) != type->subtype.fs_type)
			return FALSE;
		break;
	
	case BRASERO_TRACK_TYPE_DISC:
		if (type->subtype.media == BRASERO_MEDIUM_NONE)
			return FALSE;

		/* Reminder: we now create every possible types */
		if (caps->type.subtype.media != type->subtype.media)
			return FALSE;
		break;
	
	case BRASERO_TRACK_TYPE_IMAGE:
		if (type->subtype.img_format == BRASERO_IMAGE_FORMAT_NONE)
			return FALSE;

		if ((caps->type.subtype.img_format & type->subtype.img_format) != type->subtype.img_format)
			return FALSE;
		break;

	case BRASERO_TRACK_TYPE_STREAM:
		/* There is one small special case here with video. */
		if ((caps->type.subtype.stream_format & (BRASERO_VIDEO_FORMAT_UNDEFINED|
							 BRASERO_VIDEO_FORMAT_VCD|
							 BRASERO_VIDEO_FORMAT_VIDEO_DVD))
		&& !(type->subtype.stream_format & (BRASERO_VIDEO_FORMAT_UNDEFINED|
						   BRASERO_VIDEO_FORMAT_VCD|
						   BRASERO_VIDEO_FORMAT_VIDEO_DVD)))
			return FALSE;

		if ((caps->type.subtype.stream_format & type->subtype.stream_format) != type->subtype.stream_format)
			return FALSE;
		break;

	default:
		break;
	}

	return TRUE;
}

BraseroCaps *
brasero_burn_caps_find_start_caps (BraseroBurnCaps *self,
				   BraseroTrackType *output)
{
	GSList *iter;

	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		BraseroCaps *caps;

		caps = iter->data;

		if (!brasero_caps_is_compatible_type (caps, output))
			continue;

		if (brasero_track_type_get_has_medium (&caps->type)
		|| (caps->flags & BRASERO_PLUGIN_IO_ACCEPT_FILE))
			return caps;
	}

	return NULL;
}

static void
brasero_burn_caps_finalize (GObject *object)
{
	BraseroBurnCaps *cobj;

	cobj = BRASERO_BURNCAPS (object);
	
	if (cobj->priv->groups) {
		g_hash_table_destroy (cobj->priv->groups);
		cobj->priv->groups = NULL;
	}

	g_slist_foreach (cobj->priv->caps_list, (GFunc) brasero_caps_free, NULL);
	g_slist_free (cobj->priv->caps_list);

	if (cobj->priv->group_str) {
		g_free (cobj->priv->group_str);
		cobj->priv->group_str = NULL;
	}

	if (cobj->priv->tests) {
		g_slist_foreach (cobj->priv->tests, (GFunc) brasero_caps_test_free, NULL);
		g_slist_free (cobj->priv->tests);
		cobj->priv->tests = NULL;
	}

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_burn_caps_class_init (BraseroBurnCapsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_burn_caps_finalize;
}

static void
brasero_burn_caps_init (BraseroBurnCaps *obj)
{
	GSettings *settings;

	obj->priv = g_new0 (BraseroBurnCapsPrivate, 1);

	settings = g_settings_new (BRASERO_SCHEMA_CONFIG);
	obj->priv->group_str = g_settings_get_string (settings, BRASERO_ENGINE_GROUP_KEY);
	g_object_unref (settings);
}
