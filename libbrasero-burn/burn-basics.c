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
#include <string.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include "brasero-io.h"

#include "burn-basics.h"
#include "burn-debug.h"
#include "burn-caps.h"
#include "burn-plugin-manager.h"
#include "brasero-plugin-information.h"

#include "brasero-drive.h"
#include "brasero-medium-monitor.h"

#include "brasero-burn-lib.h"
#include "burn-caps.h"

static BraseroPluginManager *plugin_manager = NULL;
static BraseroMediumMonitor *medium_manager = NULL;
static BraseroBurnCaps *default_caps = NULL;


GQuark
brasero_burn_quark (void)
{
	static GQuark quark = 0;

	if (!quark)
		quark = g_quark_from_static_string ("BraseroBurnError");

	return quark;
}
 
const gchar *
brasero_burn_action_to_string (BraseroBurnAction action)
{
	gchar *strings [BRASERO_BURN_ACTION_LAST] = { 	"",
							N_("Getting size"),
							N_("Creating image"),
							N_("Writing"),
							N_("Blanking"),
							N_("Creating checksum"),
							N_("Copying disc"),
							N_("Copying file"),
							N_("Analysing audio files"),
							N_("Transcoding song"),
							N_("Preparing to write"),
							N_("Writing leadin"),
							N_("Writing CD-Text information"),
							N_("Finalizing"),
							N_("Writing leadout"),
						        N_("Starting to record"),
							N_("Success"),
							N_("Ejecting medium")};
	return _(strings [action]);
}

/**
 * utility functions
 */

gboolean
brasero_check_flags_for_drive (BraseroDrive *drive,
			       BraseroBurnFlag flags)
{
	BraseroMedia media;
	BraseroMedium *medium;

	if (!drive)
		return TRUE;

	if (brasero_drive_is_fake (drive))
		return TRUE;

	medium = brasero_drive_get_medium (drive);
	if (!medium)
		return TRUE;

	media = brasero_medium_get_status (medium);
	if (flags & BRASERO_BURN_FLAG_DUMMY) {
		/* This is always FALSE */
		if (media & BRASERO_MEDIUM_PLUS) {
			BRASERO_BURN_LOG ("Drive does not support BRASERO_MEDIUM_PLUS flag");
			return FALSE;
		}

		if (media & BRASERO_MEDIUM_DVD) {
			if (!brasero_medium_can_use_dummy_for_sao (medium)) {
				BRASERO_BURN_LOG ("Drive does not support using dummy for SAO");
				return FALSE;
			}
		}
		else if (flags & BRASERO_BURN_FLAG_DAO) {
			if (!brasero_medium_can_use_dummy_for_sao (medium)) {
				BRASERO_BURN_LOG ("Drive does not support using dummy for DAO");
				return FALSE;
			}
		}
		else if (!brasero_medium_can_use_dummy_for_tao (medium)) {
			BRASERO_BURN_LOG ("Drive does not support using dummy for TAO");
			return FALSE;
		}
	}

	if (flags & BRASERO_BURN_FLAG_BURNPROOF) {
		if (!brasero_medium_can_use_burnfree (medium)) {
			BRASERO_BURN_LOG ("Drive does not support burnproof/burnfree");
			return FALSE;
		}
	}

	return TRUE;
}

gchar *
brasero_string_get_localpath (const gchar *uri)
{
	gchar *localpath;
	gchar *realuri;
	GFile *file;

	if (!uri)
		return NULL;

	if (uri [0] == '/')
		return g_strdup (uri);

	if (strncmp (uri, "file://", 7))
		return NULL;

	file = g_file_new_for_commandline_arg (uri);
	realuri = g_file_get_uri (file);
	g_object_unref (file);

	localpath = g_filename_from_uri (realuri, NULL, NULL);
	g_free (realuri);

	return localpath;
}

gchar *
brasero_string_get_uri (const gchar *uri)
{
	gchar *uri_return;
	GFile *file;

	if (!uri)
		return NULL;

	if (uri [0] != '/')
		return g_strdup (uri);

	file = g_file_new_for_commandline_arg (uri);
	uri_return = g_file_get_uri (file);
	g_object_unref (file);

	return uri_return;
}

static void
brasero_caps_list_dump (void)
{
	GSList *iter;
	BraseroBurnCaps *self;

	self = brasero_burn_caps_get_default ();
	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		BraseroCaps *caps;

		caps = iter->data;
		BRASERO_BURN_LOG_WITH_TYPE (&caps->type,
					    caps->flags,
					    "Created %i links pointing to",
					    g_slist_length (caps->links));
	}

	g_object_unref (self);
}

/**
 * brasero_burn_library_start:
 * @argc: an #int.
 * @argv: a #char **.
 *
 * Starts the library. This function must be called
 * before using any of the functions.
 *
 * Rename to: init
 *
 * Returns: a #gboolean
 **/

gboolean
brasero_burn_library_start (int *argc,
                            char **argv [])
{
	BRASERO_BURN_LOG ("Initializing Brasero-burn %i.%i.%i",
			  BRASERO_MAJOR_VERSION,
			  BRASERO_MINOR_VERSION,
			  BRASERO_SUB);

#if defined(HAVE_STRUCT_USCSI_CMD)
	/* Work around: because on OpenSolaris brasero possibly be run
	 * as root for a user with 'Primary Administrator' profile,
	 * a root dbus session will be autospawned at that time.
	 * This fix is to work around
	 * http://bugzilla.gnome.org/show_bug.cgi?id=526454
	 */
	g_setenv ("DBUS_SESSION_BUS_ADDRESS", "autolaunch:", TRUE);
#endif

	/* ... and GStreamer) */
	if (!gst_init_check (argc, argv, NULL))
		return FALSE;

	/* This is for missing codec automatic install */
	gst_pb_utils_init ();

	/* initialize the media library */
	brasero_media_library_start ();

	/* initialize all device list */
	if (!medium_manager)
		medium_manager = brasero_medium_monitor_get_default ();

	/* initialize plugins */
	if (!default_caps)
		default_caps = BRASERO_BURNCAPS (g_object_new (BRASERO_TYPE_BURNCAPS, NULL));

	if (!plugin_manager)
		plugin_manager = brasero_plugin_manager_get_default ();

	brasero_caps_list_dump ();
	return TRUE;
}

BraseroBurnCaps *
brasero_burn_caps_get_default ()
{
	if (!default_caps)
		g_error ("You must call brasero_burn_library_start () before using API from libbrasero-burn");

	g_object_ref (default_caps);
	return default_caps;
}

/**
 * brasero_burn_library_get_plugins_list:
 * 
 * This function returns the list of plugins that 
 * are available to libbrasero-burn.
 *
 * Returns: (element-type GObject.Object) (transfer full):a #GSList that must be destroyed when not needed and each object unreffed.
 **/

GSList *
brasero_burn_library_get_plugins_list (void)
{
	plugin_manager = brasero_plugin_manager_get_default ();
	return brasero_plugin_manager_get_plugins_list (plugin_manager);
}

/**
 * brasero_burn_library_stop:
 *
 * Stop the library. Don't use any of the functions or
 * objects afterwards
 *
 * Rename to: deinit
 *
 **/
void
brasero_burn_library_stop (void)
{
	if (plugin_manager) {
		g_object_unref (plugin_manager);
		plugin_manager = NULL;
	}

	if (medium_manager) {
		g_object_unref (medium_manager);
		medium_manager = NULL;
	}

	if (default_caps) {
		g_object_unref (default_caps);
		default_caps = NULL;
	}

	/* Cleanup the io thing */
	brasero_io_shutdown ();
}

/**
 * brasero_burn_library_can_checksum:
 *
 * Checks whether the library can do any kind of
 * checksum at all.
 *
 * Returns: a #gboolean
 */

gboolean
brasero_burn_library_can_checksum (void)
{
	GSList *iter;
	BraseroBurnCaps *self;

	self = brasero_burn_caps_get_default ();

	if (self->priv->tests == NULL) {
		g_object_unref (self);
		return FALSE;
	}

	for (iter = self->priv->tests; iter; iter = iter->next) {
		BraseroCapsTest *tmp;
		GSList *links;

		tmp = iter->data;
		for (links = tmp->links; links; links = links->next) {
			BraseroCapsLink *link;

			link = links->data;
			if (brasero_caps_link_active (link, 0)) {
				g_object_unref (self);
				return TRUE;
			}
		}
	}

	g_object_unref (self);
	return FALSE;
}

/**
 * brasero_burn_library_input_supported:
 * @type: a #BraseroTrackType
 *
 * Checks whether @type can be used as input.
 *
 * Returns: a #BraseroBurnResult
 */

BraseroBurnResult
brasero_burn_library_input_supported (BraseroTrackType *type)
{
	GSList *iter;
	BraseroBurnCaps *self;

	g_return_val_if_fail (type != NULL, BRASERO_BURN_ERR);

	self = brasero_burn_caps_get_default ();

	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		BraseroCaps *caps;

		caps = iter->data;

		if (brasero_caps_is_compatible_type (caps, type)
		&&  brasero_burn_caps_is_input (self, caps)) {
			g_object_unref (self);
			return BRASERO_BURN_OK;
		}
	}

	g_object_unref (self);
	return BRASERO_BURN_ERR;
}

/**
 * brasero_burn_library_get_media_capabilities:
 * @media: a #BraseroMedia
 *
 * Used to test what the library can do based on the medium type.
 * Returns BRASERO_MEDIUM_WRITABLE if the disc can be written
 * and / or BRASERO_MEDIUM_REWRITABLE if the disc can be erased.
 *
 * Returns: a #BraseroMedia
 */

BraseroMedia
brasero_burn_library_get_media_capabilities (BraseroMedia media)
{
	GSList *iter;
	GSList *links;
	BraseroMedia retval;
	BraseroBurnCaps *self;
	BraseroCaps *caps = NULL;

	self = brasero_burn_caps_get_default ();

	retval = BRASERO_MEDIUM_NONE;
	BRASERO_BURN_LOG_DISC_TYPE (media, "checking media caps for");

	/* we're only interested in DISC caps. There should be only one caps fitting */
	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		caps = iter->data;
		if (caps->type.type != BRASERO_TRACK_TYPE_DISC)
			continue;

		if ((media & caps->type.subtype.media) == media)
			break;

		caps = NULL;
	}

	if (!caps) {
		g_object_unref (self);
		return BRASERO_MEDIUM_NONE;
	}

	/* check the links */
	for (links = caps->links; links; links = links->next) {
		GSList *plugins;
		gboolean active;
		BraseroCapsLink *link;

		link = links->data;

		/* this link must have at least one active plugin to be valid
		 * plugins are not sorted but in this case we don't need them
		 * to be. we just need one active if another is with a better
		 * priority all the better. */
		active = FALSE;
		for (plugins = link->plugins; plugins; plugins = plugins->next) {
			BraseroPlugin *plugin;

			plugin = plugins->data;
			/* Ignore plugin errors */
			if (brasero_plugin_get_active (plugin, TRUE)) {
				/* this link is valid */
				active = TRUE;
				break;
			}
		}

		if (!active)
			continue;

		if (!link->caps) {
			/* means that it can be blanked */
			retval |= BRASERO_MEDIUM_REWRITABLE;
			continue;
		}

		/* means it can be written. NOTE: if this disc has already some
		 * data on it, it even means it can be appended */
		retval |= BRASERO_MEDIUM_WRITABLE;
	}

	g_object_unref (self);
	return retval;
}

