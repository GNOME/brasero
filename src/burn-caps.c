/***************************************************************************
 *            burn-caps.c
 *
 *  mar avr 18 20:58:42 2006
 *  Copyright  2006  Rouquier Philippe
 *  brasero-app@wanadoo.fr
 ***************************************************************************/

/*
 *  Brasero is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Brasero is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to:
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

#include <gconf/gconf-client.h>

#include "burn-basics.h"
#include "burn-debug.h"
#include "burn-drive.h"
#include "burn-medium.h"
#include "burn-session.h"
#include "burn-plugin.h"
#include "burn-plugin-private.h"
#include "burn-task.h"
#include "burn-caps.h"

#define BRASERO_ENGINE_GROUP_KEY	"/apps/brasero/config/engine-group"

G_DEFINE_TYPE (BraseroBurnCaps, brasero_burn_caps, G_TYPE_OBJECT);

struct BraseroBurnCapsPrivate {
	GSList *caps_list;
	GSList *tests;

	GHashTable *groups;

	gchar *group_str;
	guint group_id;
};

struct _BraseroCaps {
	GSList *links;
	GSList *modifiers;
	BraseroTrackType type;
	BraseroPluginIOFlag flags;
};
typedef struct _BraseroCaps BraseroCaps;

struct _BraseroCapsLink {
	GSList *plugins;
	BraseroCaps *caps;
};
typedef struct _BraseroCapsLink BraseroCapsLink;

struct _BraseroCapsTest {
	GSList *links;
	BraseroChecksumType type;
};
typedef struct _BraseroCapsTest BraseroCapsTest;

#define SUBSTRACT(a, b)		((a) &= ~((b)&(a)))

static GObjectClass *parent_class = NULL;
static BraseroBurnCaps *default_caps = NULL;

#define BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG(session)				\
{										\
	brasero_burn_session_log (session, "Unsupported type of task operation"); \
	BRASERO_BURN_LOG ("Unsupported type of task operation");		\
	return NULL;								\
}

#define BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG_RES(session)			\
{										\
	brasero_burn_session_log (session, "Unsupported type of task operation"); \
	BRASERO_BURN_LOG ("Unsupported type of task operation");		\
	return BRASERO_BURN_NOT_SUPPORTED;					\
}

#define BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG_ERROR(session, error)		\
{										\
	if (error)								\
		g_set_error (error,						\
			    BRASERO_BURN_ERROR,					\
			    BRASERO_BURN_ERROR_GENERAL,				\
			    _("this operation is not supported")); \
	BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG (session);				\
}

static void
brasero_caps_link_free (BraseroCapsLink *link)
{
	g_slist_free (link->plugins);
	g_free (link);
}

static void
brasero_caps_free (BraseroCaps *caps)
{
	g_slist_foreach (caps->links, (GFunc) brasero_caps_link_free, NULL);
	g_slist_free (caps->links);
	g_free (caps);
}

static void
brasero_burn_caps_finalize (GObject *object)
{
	BraseroBurnCaps *cobj;

	cobj = BRASERO_BURNCAPS (object);
	
	default_caps = NULL;

	if (cobj->priv->groups) {
		g_hash_table_destroy (cobj->priv->groups);
		cobj->priv->groups = NULL;
	}

	g_slist_foreach (cobj->priv->caps_list, (GFunc) brasero_caps_free, NULL);
	g_slist_free (cobj->priv->caps_list);

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
	GConfClient *client;

	obj->priv = g_new0 (BraseroBurnCapsPrivate, 1);

	client = gconf_client_get_default ();
	obj->priv->group_str = gconf_client_get_string (client,
							BRASERO_ENGINE_GROUP_KEY,
							NULL);
	g_object_unref (client);
}

BraseroBurnCaps *
brasero_burn_caps_get_default ()
{
	if (!default_caps) 
		default_caps = BRASERO_BURNCAPS (g_object_new (BRASERO_TYPE_BURNCAPS, NULL));
	else
		g_object_ref (default_caps);

	return default_caps;
}

gint
brasero_burn_caps_register_plugin_group (BraseroBurnCaps *self,
					 const gchar *name)
{
	guint retval;

	if (!name)
		return 0;

	if (!self->priv->groups)
		self->priv->groups = g_hash_table_new_full (g_str_hash,
							    g_str_equal,
							    g_free,
							    NULL);

	retval = GPOINTER_TO_INT (g_hash_table_lookup (self->priv->groups, name));
	if (retval)
		return retval;

	g_hash_table_insert (self->priv->groups,
			     g_strdup (name),
			     GINT_TO_POINTER (g_hash_table_size (self->priv->groups) + 1));

	/* see if we have a group id now */
	if (!self->priv->group_id
	&&   self->priv->group_str
	&&  !strcmp (name, self->priv->group_str))
		self->priv->group_id = g_hash_table_size (self->priv->groups) + 1;

	return g_hash_table_size (self->priv->groups) + 1;
}

/* that function receives all errors returned by the object and 'learns' from 
 * these errors what are the safest defaults for a particular system. It should 
 * also offer fallbacks if an error occurs through a signal */
static BraseroBurnResult
brasero_burn_caps_job_error_cb (BraseroJob *job,
				BraseroBurnError error,
				BraseroBurnCaps *caps)
{
#if 0
	GError *error = NULL;
	GConfClient *client;

	/* This was originally to fix a bug in fedora 5 that prevents from
	 * sending SCSI commands as a normal user through cdrdao. There is a
	 * fallback fortunately with cdrecord and raw images but no on_the_fly
	 * burning.
	 * That could be used as a hook to know how a job runs and give a
	 * "penalty" to job types being too often faulty. There could also be
	 * a dialog to ask the user if he wants to use another backend.
	 */

	/* set it in GConf to remember that next time */
	client = gconf_client_get_default ();
	gconf_client_set_bool (client, GCONF_KEY_CDRDAO_DISABLED, TRUE, &error);
	if (error) {
		g_warning ("Can't write with GConf: %s", error->message);
		g_error_free (error);
	}
	g_object_unref (client);
#endif
	return BRASERO_BURN_ERR;
}

/**
 * returns the flags that must be used (compulsory),
 * and the flags that can be used (supported).
 */

void
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

		if ((caps->type.subtype.media & type->subtype.media) != type->subtype.media)
			return FALSE;
		break;
	
	case BRASERO_TRACK_TYPE_IMAGE:
		if ((caps->type.subtype.img_format & type->subtype.img_format) != type->subtype.img_format)
			return FALSE;
		break;

	case BRASERO_TRACK_TYPE_AUDIO:
		/* There is one small special case here with video. */
		if ((caps->type.subtype.audio_format & (BRASERO_VIDEO_FORMAT_UNDEFINED|
							BRASERO_VIDEO_FORMAT_VCD|
							BRASERO_VIDEO_FORMAT_VIDEO_DVD))
		&& !(type->subtype.audio_format & (BRASERO_VIDEO_FORMAT_UNDEFINED|
						   BRASERO_VIDEO_FORMAT_VCD|
						   BRASERO_VIDEO_FORMAT_VIDEO_DVD)))
			return FALSE;

		if ((caps->type.subtype.audio_format & type->subtype.audio_format) != type->subtype.audio_format)
			return FALSE;
		break;

	default:
		break;
	}

	return TRUE;
}

/**
 * Used to test what the library can do based on the medium type.
 * Returns BRASERO_MEDIUM_WRITABLE if the disc can be written
 * and / or BRASERO_MEDIUM_REWRITABLE if the disc can be erased.
 */

BraseroMedia
brasero_burn_caps_media_capabilities (BraseroBurnCaps *self,
				      BraseroMedia media)
{
	GSList *iter;
	GSList *links;
	BraseroMedia retval;
	BraseroCaps *caps = NULL;

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

	if (!caps)
		return BRASERO_MEDIUM_NONE;

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
			if (brasero_plugin_get_active (plugin)) {
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

	return retval;
}

static BraseroCaps *
brasero_caps_find_start_caps (BraseroTrackType *output)
{
	GSList *iter;
	BraseroBurnCaps *self;

	self = brasero_burn_caps_get_default ();
	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		BraseroCaps *caps;

		caps = iter->data;

		if (!brasero_caps_is_compatible_type (caps, output))
			continue;

		if (caps->type.type == BRASERO_TRACK_TYPE_DISC
		|| (caps->flags & BRASERO_PLUGIN_IO_ACCEPT_FILE))
			return caps;
	}

	return NULL;
}

BraseroBurnResult
brasero_burn_caps_get_blanking_flags (BraseroBurnCaps *caps,
				      BraseroBurnSession *session,
				      BraseroBurnFlag *supported,
				      BraseroBurnFlag *compulsory)
{
	GSList *iter;
	BraseroMedia media;
	gboolean supported_media;
	BraseroBurnFlag session_flags;
	BraseroBurnFlag supported_flags = BRASERO_BURN_FLAG_NONE;
	BraseroBurnFlag compulsory_flags = BRASERO_BURN_FLAG_ALL;

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

	supported_media = FALSE;
	session_flags = brasero_burn_session_get_flags (session);
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

BraseroTask *
brasero_burn_caps_new_blanking_task (BraseroBurnCaps *self,
				     BraseroBurnSession *session,
				     GError **error)
{
	GSList *iter;
	BraseroMedia media;
	BraseroBurnFlag flags;
	BraseroTask *task = NULL;

	media = brasero_burn_session_get_dest_media (session);
	flags = brasero_burn_session_get_flags (session);

	for (iter = self->priv->caps_list; iter; iter = iter->next) {
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
			BraseroPlugin *candidate;

			link = links->data;

			if (link->caps != NULL)
				continue;

			/* Go through all the plugins and find the best plugin
			 * for the task. It must :
			 * - be active
			 * - have the highest priority
			 * - accept the flags */
			candidate = NULL;
			for (plugins = link->plugins; plugins; plugins = plugins->next) {
				BraseroPlugin *plugin;

				plugin = plugins->data;

				if (!brasero_plugin_get_active (plugin))
					continue;

				if (!brasero_plugin_check_blank_flags (plugin, media, flags))
					continue;

				if (self->priv->group_id > 0 && candidate) {
					/* the candidate must be in the favourite group as much as possible */
					if (brasero_plugin_get_group (candidate) != self->priv->group_id) {
						if (brasero_plugin_get_group (plugin) == self->priv->group_id) {
							candidate = plugin;
							continue;
						}
					}
					else if (brasero_plugin_get_group (plugin) != self->priv->group_id)
						continue;
				}

				if (!candidate)
					candidate = plugin;
				else if (brasero_plugin_get_priority (plugin) >
					 brasero_plugin_get_priority (candidate))
					candidate = plugin;
			}

			if (candidate) {
				BraseroJob *job;
				GType type;

				type = brasero_plugin_get_gtype (candidate);
				job = BRASERO_JOB (g_object_new (type,
								 "output", NULL,
								 NULL));
				g_signal_connect (job,
						  "error",
						  G_CALLBACK (brasero_burn_caps_job_error_cb),
						  caps);

				task = BRASERO_TASK (g_object_new (BRASERO_TYPE_TASK,
								   "session", session,
								   "action", BRASERO_TASK_ACTION_ERASE,
								   NULL));
				brasero_task_add_item (task, BRASERO_TASK_ITEM (job));
				return task;
			}
		}
	}

	BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG_ERROR (session, error);
}

BraseroBurnResult
brasero_burn_caps_can_blank (BraseroBurnCaps *self,
			     BraseroBurnSession *session)
{
	GSList *iter;
	BraseroMedia media;
	BraseroBurnFlag flags;

	media = brasero_burn_session_get_dest_media (session);
	BRASERO_BURN_LOG_DISC_TYPE (media, "Testing blanking caps for");

	if (media == BRASERO_MEDIUM_NONE) {
		BRASERO_BURN_LOG ("no media => no blanking possible");
		return BRASERO_BURN_NOT_SUPPORTED;
	}

	flags = brasero_burn_session_get_flags (session);
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
brasero_caps_link_active (BraseroCapsLink *link)
{
	GSList *iter;

	/* See if link is active by going through all plugins. There must be at
	 * least one. */
	for (iter = link->plugins; iter; iter = iter->next) {
		BraseroPlugin *plugin;

		plugin = iter->data;
		if (brasero_plugin_get_active (plugin))
			return TRUE;
	}

	return FALSE;
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

	/* Go through all plugins: at least one must support record flags */
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

static BraseroPlugin *
brasero_caps_link_find_plugin (BraseroCapsLink *link,
			       gint group_id,
			       BraseroBurnFlag session_flags,
			       BraseroTrackType *output,
			       BraseroMedia media)
{
	GSList *iter;
	BraseroPlugin *candidate;

	/* Go through all plugins for a link and find the best one. It must:
	 * - be active
	 * - be part of the group (as much as possible)
	 * - have the highest priority
	 * - support the flags */
	candidate = NULL;
	for (iter = link->plugins; iter; iter = iter->next) {
		BraseroPlugin *plugin;

		plugin = iter->data;

		if (!brasero_plugin_get_active (plugin))
			continue;

		if (output->type == BRASERO_TRACK_TYPE_DISC) {
			gboolean result;

			result = brasero_plugin_check_record_flags (plugin,
								    media,
								    session_flags);
			if (!result)
				continue;
		}

		if (link->caps->type.type == BRASERO_TRACK_TYPE_DATA) {
			gboolean result;

			result = brasero_plugin_check_image_flags (plugin,
								   media,
								   session_flags);
			if (!result)
				continue;
		}
		else if (!brasero_plugin_check_media_restrictions (plugin, media))
			continue;

		if (group_id > 0 && candidate) {
			/* the candidate must be in the favourite group as much as possible */
			if (brasero_plugin_get_group (candidate) != group_id) {
				if (brasero_plugin_get_group (plugin) == group_id) {
					candidate = plugin;
					continue;
				}
			}
			else if (brasero_plugin_get_group (plugin) != group_id)
				continue;
		}

		if (!candidate)
			candidate = plugin;
		else if (brasero_plugin_get_priority (plugin) >
			 brasero_plugin_get_priority (candidate))
			candidate = plugin;
	}

	return candidate;
}

typedef struct _BraseroCapsLinkList BraseroCapsLinkList;
struct _BraseroCapsLinkList {
	BraseroCapsLinkList *next;
	BraseroCapsLink *link;
	BraseroPlugin *plugin;
};

static BraseroCapsLinkList *
brasero_caps_link_list_insert (BraseroCapsLinkList *list,
			       BraseroCapsLinkList *node,
			       gboolean fits)
{
	BraseroCapsLinkList *iter;

	if (!list)
		return node;

	if (brasero_plugin_get_priority (node->plugin) >
	    brasero_plugin_get_priority (list->plugin)) {
		node->next = list;
		return node;
	}

	if (brasero_plugin_get_priority (node->plugin) ==
	    brasero_plugin_get_priority (list->plugin)) {
		if (fits) {
			node->next = list;
			return node;
		}

		node->next = list->next;
		list->next = node;
		return list;
	}

	if (!list->next) {
		node->next = NULL;
		list->next = node;
		return list;
	}

	/* Need a node with at least the same priority. Stop if end is reached */
	iter = list;
	while (iter->next &&
	       brasero_plugin_get_priority (node->plugin) <
	       brasero_plugin_get_priority (iter->next->plugin))
		iter = iter->next;

	if (!iter->next) {
		/* reached the end of the list, put it at the end */
		iter->next = node;
		node->next = NULL;
	}
	else if (brasero_plugin_get_priority (node->plugin) <
		 brasero_plugin_get_priority (iter->next->plugin)) {
		/* Put it at the end of the list */
		node->next = NULL;
		iter->next->next = node;
	}
	else if (brasero_plugin_get_priority (node->plugin) >
		 brasero_plugin_get_priority (iter->next->plugin)) {
		/* insert it before iter->next */
		node->next = iter->next;
		iter->next = node;
	}
	else if (fits) {
		/* insert it before the link with the same priority */
		node->next = iter->next;
		iter->next = node;
	}
	else {
		/* insert it after the link with the same priority */
		node->next = iter->next->next;
		iter->next->next = node;
	}
	return list;
}

static GSList *
brasero_caps_find_best_link (BraseroCaps *caps,
			     gint group_id,
			     GSList *used_caps,
			     BraseroBurnFlag session_flags,
			     BraseroMedia media,
			     BraseroTrackType *input,
			     BraseroPluginIOFlag io_flags)
{
	GSList *iter;
	GSList *results = NULL;
	BraseroCapsLinkList *node = NULL;
	BraseroCapsLinkList *list = NULL;

	BRASERO_BURN_LOG_WITH_TYPE (&caps->type, BRASERO_PLUGIN_IO_NONE, "find_best_link");

	/* First, build a list of possible links and sort them out according to
	 * the priority based on the highest priority among their plugins. In 
	 * this case, we can't sort links beforehand since according to the
	 * flags, input, output in the session the plugins will or will not 
	 * be used. Moreover given the group_id thing the choice of plugin may
	 * depends. */

	/* This is done to address possible issues namely:
	 * - growisofs can handle DATA right from the start but has a lower
	 * priority than libburn. In this case growisofs would be used every 
	 * time for DATA despite its having a lower priority than libburn if we
	 * were looking for the best fit first
	 * - We don't want to follow the long path and have a useless (in this
	 * case) image converter plugin get included.
	 * ex: we would have: CDRDAO (input) toc2cue => (CUE) cdrdao => (DISC)
	 * instead of simply: CDRDAO (input) cdrdao => (DISC) */

	for (iter = caps->links; iter; iter = iter->next) {
		BraseroPlugin *plugin;
		BraseroCapsLink *link;
		gboolean fits;

		link = iter->data;

		/* skip blanking links */
		if (!link->caps) {
			BRASERO_BURN_LOG ("Blanking caps");
			continue;
		}

		/* the link should not link to an already used caps */
		if (g_slist_find (used_caps, link->caps)) {
			BRASERO_BURN_LOG ("Already used caps");
			continue;
		}

		/* see if that's a perfect fit;
		 * - it must have the same caps (type + subtype)
		 * - it must have the proper IO (file). */
		fits = (link->caps->flags & BRASERO_PLUGIN_IO_ACCEPT_FILE) &&
			brasero_caps_is_compatible_type (link->caps, input);

		if (!fits) {
			/* if it doesn't fit it must be at least connectable */
			if ((link->caps->flags & io_flags) == BRASERO_PLUGIN_IO_NONE) {
				BRASERO_BURN_LOG ("Not connectable");
				continue;
			}

			/* we can't go further than a DISC type, no need to keep it */
			if (link->caps->type.type == BRASERO_TRACK_TYPE_DISC) {
				BRASERO_BURN_LOG ("Can't go further than DISC caps");
				continue;
			}
		}

		/* See if this link can be used. For a link to be followed it 
		 * must:
		 * - have at least an active plugin
		 * - have at least a plugin accepting the record flags if caps type (output) 
		 *   is a disc (that means that the link is the recording part) 
		 * - have at least a plugin accepting the data flags if caps type (input)
		 *   is DATA. */
		plugin = brasero_caps_link_find_plugin (link,
							group_id,
							session_flags,
							&caps->type,
							media);
		if (!plugin) {
			BRASERO_BURN_LOG ("No plugin found");
			continue;
		}

		BRASERO_BURN_LOG ("Found candidate link");

		/* A plugin could be found which means that link can be used.
		 * Insert it in the list at the right place.
		 * The list is sorted according to priorities (starting with the
		 * highest). If 2 links have the same priority put first the one
		 * that has the correct input. */
		node = g_new0 (BraseroCapsLinkList, 1);
		node->plugin = plugin;
		node->link = link;

		list = brasero_caps_link_list_insert (list, node, fits);
	}

	if (!list) {
		BRASERO_BURN_LOG ("No links found");
		return NULL;
	}

	used_caps = g_slist_prepend (used_caps, caps);

	/* Then, go through this list (starting with highest priority links)
	 * The rule is we prefer the links with the highest priority; if two
	 * links have the same priority and one of them leads to a caps
	 * with the correct type then choose this one. */
	for (node = list; node; node = node->next) {
		guint search_group_id;

		/* see if that's a perfect fit; if so, then we're good. 
		 * - it must have the same caps (type + subtype)
		 * - it must have the proper IO (file) */
		if ((node->link->caps->flags & BRASERO_PLUGIN_IO_ACCEPT_FILE)
		&&   brasero_caps_is_compatible_type (node->link->caps, input)) {
			results = g_slist_prepend (NULL, node->link);
			break;
		}

		/* determine the group_id for the search */
		if (brasero_plugin_get_group (node->plugin) > 0 && group_id <= 0)
			search_group_id = brasero_plugin_get_group (node->plugin);
		else
			search_group_id = group_id;

		/* It's not a perfect fit. First see if a plugin with the same
		 * priority don't have the right input. Then see if we can reach
		 * the right input by going through all previous nodes */
		results = brasero_caps_find_best_link (node->link->caps,
						       search_group_id,
						       used_caps,
						       session_flags,
						       media,
						       input,
						       io_flags);
		if (results) {
			results = g_slist_prepend (results, node->link);
			break;
		}
	}

	/* clear up */
	used_caps = g_slist_remove (used_caps, caps);
	for (node = list; node; node = list) {
		list = node->next;
		g_free (node);
	}

	return results;
}

static GSList *
brasero_caps_add_processing_plugins_to_task (BraseroBurnSession *session,
					     BraseroTask *task,
					     BraseroCaps *caps,
					     BraseroTrackType *io_type,
					     BraseroPluginProcessFlag position)
{
	GSList *retval = NULL;
	GSList *iter;

	if (position == BRASERO_PLUGIN_RUN_NEVER
	||  caps->type.type == BRASERO_TRACK_TYPE_DISC)
		return NULL;

	BRASERO_BURN_LOG_WITH_TYPE (&caps->type,
				    caps->flags,
				    "Adding modifiers (position %i) (%i modifiers available) for",
				    position,
				    g_slist_length (caps->modifiers));

	/* Go through all plugins and add all possible modifiers. They must:
	 * - be active
	 * - accept the position flags
	 * => no need for modifiers to be sorted in list. */
	for (iter = caps->modifiers; iter; iter = iter->next) {
		BraseroPluginProcessFlag flags;
		BraseroPlugin *plugin;
		BraseroJob *job;
		GType type;

		plugin = iter->data;
		if (!brasero_plugin_get_active (plugin))
			continue;

		brasero_plugin_get_process_flags (plugin, &flags);
		if (!(flags & position))
			continue;

		type = brasero_plugin_get_gtype (plugin);
		job = BRASERO_JOB (g_object_new (type,
						 "output", io_type,
						 NULL));
		g_signal_connect (job,
				  "error",
				  G_CALLBACK (brasero_burn_caps_job_error_cb),
				  caps);

		if (!task
		||  !(caps->flags & BRASERO_PLUGIN_IO_ACCEPT_PIPE)
		||  !BRASERO_BURN_SESSION_NO_TMP_FILE (session)) {
			/* here the action taken is always to create an image */
			task = BRASERO_TASK (g_object_new (BRASERO_TYPE_TASK,
							   "session", session,
							   "action", BRASERO_BURN_ACTION_CREATING_IMAGE,
							   NULL));
			retval = g_slist_prepend (retval, task);
		}

		BRASERO_BURN_LOG ("%s (modifier) added to task", brasero_plugin_get_name (plugin));
		BRASERO_BURN_LOG_TYPE (io_type, "IO type");

		brasero_task_add_item (task, BRASERO_TASK_ITEM (job));
	}

	return retval;
}

static gboolean
brasero_burn_caps_flags_check_for_drive (BraseroBurnSession *session)
{
	BraseroDrive *drive;
	BraseroBurnFlag flags;

	drive = brasero_burn_session_get_burner (session);
	if (!drive)
		return TRUE;

	flags = brasero_burn_session_get_flags (session);
	if (!brasero_drive_has_safe_burn (drive)
	&&  !(flags & BRASERO_BURN_FLAG_BURNPROOF))
		return FALSE;

	return TRUE;
}

GSList *
brasero_burn_caps_new_task (BraseroBurnCaps *self,
			    BraseroBurnSession *session,
			    GError **error)
{
	BraseroPluginProcessFlag position;
	BraseroBurnFlag session_flags;
	BraseroTrackType plugin_input;
	BraseroTask *blanking = NULL;
	BraseroPluginIOFlag flags;
	BraseroTask *task = NULL;
	BraseroTrackType output;
	BraseroTrackType input;
	BraseroCaps *last_caps;
	GSList *retval = NULL;
	GSList *iter, *list;
	BraseroMedia media;
	gint group_id;

	/* determine the output and the flags for this task */
	if (brasero_burn_session_is_dest_file (session)) {
		media = BRASERO_MEDIUM_FILE;

		output.type = BRASERO_TRACK_TYPE_IMAGE;
		output.subtype.img_format = brasero_burn_session_get_output_format (session);
	}
	else {
		media = brasero_burn_session_get_dest_media (session);

		output.type = BRASERO_TRACK_TYPE_DISC;
		output.subtype.media = media;
	}

	if (BRASERO_BURN_SESSION_NO_TMP_FILE (session))
		flags = BRASERO_PLUGIN_IO_ACCEPT_PIPE;
	else
		flags = BRASERO_PLUGIN_IO_ACCEPT_FILE;

	BRASERO_BURN_LOG_WITH_TYPE (&output,
				    flags,
				    "Creating recording/imaging task");

	/* search the start caps and try to get a list of links */
	last_caps = brasero_caps_find_start_caps (&output);
	if (!last_caps)
		BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG_ERROR (session, error);

	brasero_burn_session_get_input_type (session, &input);
	BRASERO_BURN_LOG_WITH_TYPE (&input,
				    BRASERO_PLUGIN_IO_NONE,
				    "Input set =");

	session_flags = brasero_burn_session_get_flags (session);
	if (!brasero_burn_caps_flags_check_for_drive (session))
		BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG (session);

	/* Here remove BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE since we'll handle
	 * any possible need for blanking just afterwards if it doesn't work */
	session_flags &= ~(BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE|
			   BRASERO_BURN_FLAG_FAST_BLANK);

	list = brasero_caps_find_best_link (last_caps,
					    self->priv->group_id,
					    NULL,
					    session_flags,
					    media,
					    &input,
					    flags);
	if (!list) {
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
		if (output.type != BRASERO_TRACK_TYPE_DISC)
			BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG_ERROR (session, error);

		/* output is a disc try with initial blanking */
		BRASERO_BURN_LOG ("failed to create proper task. Trying with initial blanking");

		/* apparently nothing can be done to reach our goal. Maybe that
		 * is because we first have to blank the disc. If so add a blank 
		 * task to the others as a first step */
		session_flags = brasero_burn_session_get_flags (session);
		if (!(session_flags & BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE)
		||    brasero_burn_caps_can_blank (self, session) != BRASERO_BURN_OK)
			BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG_ERROR (session, error);

		/* retry with the same disc type but blank this time */
		media &= ~(BRASERO_MEDIUM_CLOSED|
			   BRASERO_MEDIUM_APPENDABLE|
	   		   BRASERO_MEDIUM_UNFORMATTED|
			   BRASERO_MEDIUM_HAS_DATA|
			   BRASERO_MEDIUM_HAS_AUDIO);
		media |= BRASERO_MEDIUM_BLANK;

		output.subtype.media = media;

		last_caps = brasero_caps_find_start_caps (&output);
		if (!last_caps)
			BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG_ERROR (session, error);

		/* if the flag BLANK_BEFORE_WRITE was set then remove it since
		 * we are actually blanking. Simply the record plugin won't have
		 * to do it. */
		session_flags &= ~BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE;
		list = brasero_caps_find_best_link (last_caps,
						    self->priv->group_id,
						    NULL,
						    session_flags,
						    media,
						    &input,
						    flags);
		if (!list)
			BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG_ERROR (session, error);

		BRASERO_BURN_LOG ("initial blank/erase task required")

		blanking = brasero_burn_caps_new_blanking_task (self, session, error);
		/* The problem here is that we shouldn't always prepend such a 
		 * task. For example when we copy a disc to another using the 
		 * same drive. In this case we should insert it before the last.
		 * Now, that always work so that's what we do in all cases. Once
		 * the whole list of tasks is created we insert this blanking
		 * task just before the last one. Another advantage is that the
		 * blanking of the disc is delayed as late as we can which means
		 * in case of error we keep it intact as late as we can. */
	}

	/* reverse the list of links to have them in the right order */
	list = g_slist_reverse (list);
	position = BRASERO_PLUGIN_RUN_FIRST;
	group_id = self->priv->group_id;

	brasero_burn_session_get_input_type (session, &plugin_input);
	for (iter = list; iter; iter = iter->next) {
		BraseroTrackType plugin_output;
		BraseroCapsLink *link;
		BraseroPlugin *plugin;
		BraseroJob *job;
		GSList *result;
		GType type;

		link = iter->data;

		if (last_caps->type.type == BRASERO_TRACK_TYPE_DISC && !iter->next) {
			/* if we are recording then the last caps is considered
			 * to be the one before the DISC caps since the latter
			 * can't have processing plugin */
			position |= BRASERO_PLUGIN_RUN_LAST;
		}

		/* determine the plugin output */
		if (iter->next) {
			BraseroCapsLink *next_link;

			next_link = iter->next->data;
			if (next_link == link) {
				/* That's a processing plugin so the output must
				 * be the exact same as the input, which is not
				 * necessarily the caps type referred to by the 
				 * link if the link is amongst the first. In
				 * that case that's the session input. */
				memcpy (&plugin_output,
					&plugin_input,
					sizeof (BraseroTrackType));
			}
			else {
				memcpy (&plugin_output,
					&next_link->caps->type,
					sizeof (BraseroTrackType));
			}
		}
		else
			memcpy (&plugin_output,
				&output,
				sizeof (BraseroTrackType));

		/* first see if there are track processing plugins */
		result = brasero_caps_add_processing_plugins_to_task (session,
								      task,
								      link->caps,
								      &plugin_input,
								      position);
		retval = g_slist_concat (retval, result);

		/* create job from the best plugin in link */
		plugin = brasero_caps_link_find_plugin (link,
							group_id,
							session_flags,
							&plugin_output,
							media);
		if (!plugin) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("internal error in plugin system"));
			g_slist_foreach (retval, (GFunc) g_object_unref, NULL);
			g_slist_free (retval);
			g_slist_free (list);
			return NULL;
		}

		/* This is meant to have plugins in the same group id as much as
		 * possible */
		if (brasero_plugin_get_group (plugin) > 0 && group_id <= 0)
			group_id = brasero_plugin_get_group (plugin);

		type = brasero_plugin_get_gtype (plugin);
		job = BRASERO_JOB (g_object_new (type,
						 "output", &plugin_output,
						 NULL));
		g_signal_connect (job,
				  "error",
				  G_CALLBACK (brasero_burn_caps_job_error_cb),
				  link);

		if (!task
		||  !(link->caps->flags & BRASERO_PLUGIN_IO_ACCEPT_PIPE)
		||  !BRASERO_BURN_SESSION_NO_TMP_FILE (session)) {
			/* only the last task will be doing the proper action
			 * all other are only steps to take to reach the final
			 * action */
			BRASERO_BURN_LOG ("New task");
			task = BRASERO_TASK (g_object_new (BRASERO_TYPE_TASK,
							   "session", session,
							   "action", BRASERO_TASK_ACTION_NORMAL,
							   NULL));
			retval = g_slist_append (retval, task);
		}

		brasero_task_add_item (task, BRASERO_TASK_ITEM (job));

		BRASERO_BURN_LOG ("%s added to task", brasero_plugin_get_name (plugin));
		BRASERO_BURN_LOG_TYPE (&plugin_input, "input");
		BRASERO_BURN_LOG_TYPE (&plugin_output, "output");

		position = BRASERO_PLUGIN_RUN_NEVER;

		/* the output of the plugin will become the input of the next */
		memcpy (&plugin_input, &plugin_output, sizeof (BraseroTrackType));
	}
	g_slist_free (list);

	if (last_caps->type.type != BRASERO_TRACK_TYPE_DISC) {
		GSList *result;

		/* imaging to a file so we never run the processing plugin on
		 * the fly in this case so as to allow the last plugin to output
		 * correctly to a file */
		/* NOTE: if it's not a disc we didn't modified the output
		 * subtype */
		result = brasero_caps_add_processing_plugins_to_task (session,
								      NULL,
								      last_caps,
								      &output,
								      BRASERO_PLUGIN_RUN_LAST);
		retval = g_slist_concat (retval, result);
	}
	else if (blanking) {
		retval = g_slist_insert_before (retval,
						g_slist_last (retval),
						blanking);
	}

	return retval;
}

BraseroTask *
brasero_burn_caps_new_checksuming_task (BraseroBurnCaps *self,
					BraseroBurnSession *session,
					GError **error)
{
	BraseroTrackType track_type;
	BraseroPlugin *candidate;
	BraseroCaps *last_caps;
	BraseroTrackType input;
	guint checksum_type;
	BraseroTrack *track;
	BraseroTask *task;
	BraseroJob *job;
	GSList *tracks;
	GSList *links;
	GSList *list;
	GSList *iter;

	brasero_burn_session_get_input_type (session, &input);
	BRASERO_BURN_LOG_WITH_TYPE (&input,
				    BRASERO_PLUGIN_IO_NONE,
				    "Creating checksuming task with input");

	/* first find a checksuming job that can output the type of required
	 * checksum. Then go through the caps to see if the input type can be
	 * found. */

	/* some checks */
	tracks = brasero_burn_session_get_tracks (session);
	if (g_slist_length (tracks) != 1) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("only one track at a time can be checked"));
		return NULL;
	}

	/* get the required checksum type */
	track = tracks->data;
	checksum_type = brasero_track_get_checksum_type (track);

	links = NULL;
	for (iter = self->priv->tests; iter; iter = iter->next) {
		BraseroCapsTest *test;

		test = iter->data;
		if (!test->links)
			continue;

		/* check this caps test supports the right checksum type */
		if (test->type & checksum_type) {
			links = test->links;
			break;
		}
	}

	if (!links) {
		/* we failed to find and create a proper task */
		BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG_ERROR (session, error);
	}

	list = NULL;
	last_caps = NULL;
	brasero_track_get_type (track, &track_type);
	for (iter = links; iter; iter = iter->next) {
		BraseroCapsLink *link;
		GSList *plugins;

		link = iter->data;

		/* NOTE: that shouldn't happen */
		if (!link->caps)
			continue;

		BRASERO_BURN_LOG_TYPE (&link->caps->type, "Trying link to");

		/* Make sure we have a candidate */
		candidate = NULL;
		for (plugins = link->plugins; plugins; plugins = plugins->next) {
			BraseroPlugin *plugin;

			plugin = plugins->data;
			if (!brasero_plugin_get_active (plugin))
				continue;

			/* note for checksuming task there is no group possible */
			if (!candidate)
				candidate = plugin;
			else if (brasero_plugin_get_priority (plugin) >
				 brasero_plugin_get_priority (candidate))
				candidate = plugin;
		}

		if (!candidate)
			continue;

		/* see if it can handle the input or if it can be linked to 
		 * another plugin that can */
		if (brasero_caps_is_compatible_type (link->caps, &input)) {
			/* this is the right caps */
			last_caps = link->caps;
			break;
		}

		/* don't go any further if that's a DISC type */
		if (link->caps->type.type == BRASERO_TRACK_TYPE_DISC)
			continue;

		/* the caps itself is not the right one so we try to 
		 * go through its links to find the right caps. */
		list = brasero_caps_find_best_link (link->caps,
						    self->priv->group_id,
						    NULL,
						    BRASERO_BURN_FLAG_NONE,
						    BRASERO_MEDIUM_NONE,
						    &input,
						    BRASERO_PLUGIN_IO_ACCEPT_PIPE);
		if (list) {
			last_caps = link->caps;
			break;
		}
	}

	if (!last_caps) {
		/* no link worked failure */
		BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG_ERROR (session, error);
	}

	/* we made it. Create task */
	task = BRASERO_TASK (g_object_new (BRASERO_TYPE_TASK,
					   "session", session,
					   "action", BRASERO_TASK_ACTION_CHECKSUM,
					   NULL));

	list = g_slist_reverse (list);
	for (iter = list; iter; iter = iter->next) {
		GType type;
		GSList *plugins;
		BraseroCapsLink *link;
		BraseroPlugin *candidate_plugin;
		BraseroTrackType *plugin_output;

		link = iter->data;

		/* determine the plugin output */
		if (iter->next) {
			BraseroCapsLink *next_link;

			next_link = iter->next->data;
			plugin_output = &next_link->caps->type;
		}
		else
			plugin_output = &last_caps->type;

		/* find the best plugin */
		candidate_plugin = NULL;
		for (plugins = link->plugins; plugins; plugins = plugins->next) {
			BraseroPlugin *plugin;

			plugin = plugins->data;

			if (!brasero_plugin_get_active (plugin))
				continue;

			if (!candidate_plugin)
				candidate_plugin = plugin;
			else if (brasero_plugin_get_priority (plugin) >
				 brasero_plugin_get_priority (candidate_plugin))
				candidate_plugin = plugin;
		}

		/* create the object */
		type = brasero_plugin_get_gtype (candidate_plugin);
		job = BRASERO_JOB (g_object_new (type,
						 "output", plugin_output,
						 NULL));
		g_signal_connect (job,
				  "error",
				  G_CALLBACK (brasero_burn_caps_job_error_cb),
				  link);

		brasero_task_add_item (task, BRASERO_TASK_ITEM (job));

		BRASERO_BURN_LOG ("%s added to task", brasero_plugin_get_name (candidate_plugin));
	}
	g_slist_free (list);

	/* Create the candidate */
	job = BRASERO_JOB (g_object_new (brasero_plugin_get_gtype (candidate),
					 "output", NULL,
					 NULL));
	g_signal_connect (job,
			  "error",
			  G_CALLBACK (brasero_burn_caps_job_error_cb),
			  self);
	brasero_task_add_item (task, BRASERO_TASK_ITEM (job));

	return task;
}

static gboolean
brasero_caps_find_link (BraseroCaps *caps,
			BraseroBurnFlag session_flags,
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
		if (caps->type.type == BRASERO_TRACK_TYPE_DISC
		&& !brasero_caps_link_check_record_flags (link, session_flags, media))
			continue;

		/* first see if that's the perfect fit:
		 * - it must have the same caps (type + subtype)
		 * - it must have the proper IO */
		if (link->caps->type.type == BRASERO_TRACK_TYPE_DATA) {
			if (!brasero_caps_link_check_data_flags (link, session_flags, media))
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
						 media,
						 input,
						 io_flags);
		if (result)
			return TRUE;
	}

	return FALSE;
}

static gboolean
brasero_caps_try_output (BraseroBurnFlag session_flags,
			 BraseroTrackType *output,
			 BraseroTrackType *input,
			 BraseroPluginIOFlag flags)
{
	BraseroCaps *caps;
	BraseroMedia media;

	/* here we search the start caps */
	caps = brasero_caps_find_start_caps (output);
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
				       media,
				       input,
				       flags);
}

static gboolean
brasero_caps_try_output_with_blanking (BraseroBurnCaps *self,
				       BraseroBurnSession *session,
				       BraseroTrackType *output,
				       BraseroTrackType *input,
				       BraseroPluginIOFlag io_flags)
{
	gboolean result;
	BraseroMedia media;
	BraseroCaps *last_caps;
	BraseroBurnFlag session_flags;

	session_flags = brasero_burn_session_get_flags (session);

	/* The case with prior blanking is checked later so no need for the next
	 * 2 flags */
	session_flags &= ~(BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE|
			   BRASERO_BURN_FLAG_FAST_BLANK);

	result = brasero_caps_try_output (session_flags,
					  output,
					  input,
					  io_flags);
	if (result)
		return result;

	session_flags = brasero_burn_session_get_flags (session);

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
	if (!(session_flags & BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE)
	||    brasero_burn_caps_can_blank (self, session) != BRASERO_BURN_OK)
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

	last_caps = brasero_caps_find_start_caps (output);
	if (!last_caps)
		return FALSE;

	/* if the flag BLANK_BEFORE_WRITE was set then remove it since
	 * we are actually blanking. Simply the record plugin won't have
	 * to do it. */
	session_flags &= ~BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE;
	return brasero_caps_find_link (last_caps,
				       session_flags,
				       media,
				       input,
				       io_flags);
}

BraseroBurnResult
brasero_burn_caps_is_input_supported (BraseroBurnCaps *self,
				      BraseroBurnSession *session,
				      BraseroTrackType *input)
{
	gboolean result;
	BraseroTrackType output;
	BraseroPluginIOFlag io_flags;

	if (!brasero_burn_caps_flags_check_for_drive (session))
		BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG_RES (session);

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
	BRASERO_BURN_LOG_FLAGS (brasero_burn_session_get_flags (session), "with flags");

	result = brasero_caps_try_output_with_blanking (self,
							session,
							&output,
							input,
							io_flags);
	if (!result) {
		BRASERO_BURN_LOG_TYPE (input, "Input not supported");
		return BRASERO_BURN_NOT_SUPPORTED;
	}

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_burn_caps_is_output_supported (BraseroBurnCaps *self,
				       BraseroBurnSession *session,
				       BraseroTrackType *output)
{
	gboolean result;
	BraseroTrackType input;
	BraseroPluginIOFlag io_flags;

	if (!brasero_burn_caps_flags_check_for_drive (session))
		BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG_RES (session);

	/* Here flags don't matter as we don't record anything.
	 * Even the IOFlags since that can be checked later with
	 * brasero_burn_caps_get_flags.
	 */
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
							io_flags);
	if (!result) {
		BRASERO_BURN_LOG_TYPE (output, "Output not supported");
		return BRASERO_BURN_NOT_SUPPORTED;
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_caps_is_session_supported_same_src_dest (BraseroBurnCaps *self,
						      BraseroBurnSession *session)
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

	/* NOTE: BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE is a problem here since it
	 * is only used if needed. Likewise DAO can be a problem. So just in
	 * case remove them. They are not really useful in this context. What we
	 * want here is to know whether a medium can be used given the input;
	 * only 1 flag is important here (MERGE) and can have consequences. */
	session_flags = brasero_burn_session_get_flags (session);
	session_flags &= ~(BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE|BRASERO_BURN_FLAG_DAO);

	BRASERO_BURN_LOG_FLAGS (session_flags, "flags");

	/* Find one available output format */
	format = BRASERO_IMAGE_FORMAT_CDRDAO;
	output.type = BRASERO_TRACK_TYPE_IMAGE;

	for (; format > BRASERO_IMAGE_FORMAT_NONE; format >>= 1) {
		BraseroBurnResult result;

		output.subtype.img_format = format;

		BRASERO_BURN_LOG_TYPE (&output, "Testing temporary image format");
		result = brasero_caps_try_output_with_blanking (self,
								session,
								&output,
								&input,
								BRASERO_PLUGIN_IO_ACCEPT_FILE);
		if (result != BRASERO_BURN_OK)
			continue;

		/* This format can be used to create an image. Check if can be
		 * burnt now. Just find at least one medium. */
		for (iter = self->priv->caps_list; iter; iter = iter->next) {
			BraseroCaps *caps;
			gboolean result;

			caps = iter->data;

			if (caps->type.type != BRASERO_TRACK_TYPE_DISC)
				continue;

			/* Put BRASERO_MEDIUM_NONE so we can always succeed */
			result = brasero_caps_find_link (caps,
							 session_flags,
							 BRASERO_MEDIUM_NONE,
							 &input,
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
brasero_burn_caps_is_session_supported (BraseroBurnCaps *self,
					BraseroBurnSession *session)
{
	gboolean result;
	BraseroTrackType input;
	BraseroTrackType output;
	BraseroPluginIOFlag io_flags;

	/* Special case */
	if (brasero_burn_session_same_src_dest_drive (session))
		return brasero_burn_caps_is_session_supported_same_src_dest (self, session);

	if (!brasero_burn_caps_flags_check_for_drive (session))
		BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG_RES (session);

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
	BRASERO_BURN_LOG_FLAGS (brasero_burn_session_get_flags (session), "with flags");

	result = brasero_caps_try_output_with_blanking (self,
							session,
							&output,
							&input,
							io_flags);
	if (!result) {
		BRASERO_BURN_LOG_TYPE (&output, "Output not supported");
		return BRASERO_BURN_NOT_SUPPORTED;
	}

	return BRASERO_BURN_OK;
}

BraseroMedia
brasero_burn_caps_get_required_media_type (BraseroBurnCaps *self,
					   BraseroBurnSession *session)
{
	BraseroMedia required_media = BRASERO_MEDIUM_NONE;
	BraseroBurnFlag session_flags;
	BraseroPluginIOFlag io_flags;
	BraseroTrackType input;
	GSList *iter;

	if (brasero_burn_session_is_dest_file (session))
		return BRASERO_MEDIUM_FILE;

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
	session_flags &= ~(BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE|BRASERO_BURN_FLAG_DAO);

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
			  BRASERO_MEDIUM_DVD|
			  BRASERO_MEDIUM_DVD_DL;

	return required_media;
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
brasero_burn_caps_flags_update_for_drive (BraseroBurnFlag flags,
					  BraseroBurnSession *session)
{
	BraseroDrive *drive;

	drive = brasero_burn_session_get_burner (session);
	if (!drive)
		return flags;

	if (!brasero_drive_has_safe_burn (drive))
		flags &= ~BRASERO_BURN_FLAG_BURNPROOF;

	return flags;
}

static BraseroBurnResult
brasero_caps_get_flags_for_disc (BraseroBurnFlag session_flags,
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

	caps = brasero_caps_find_start_caps (&output);
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

	if (io_flags & BRASERO_PLUGIN_IO_ACCEPT_PIPE) {
		supported_flags |= BRASERO_BURN_FLAG_NO_TMP_FILES;

		if ((session_flags & BRASERO_BURN_FLAG_NO_TMP_FILES)
		&&  (io_flags & BRASERO_PLUGIN_IO_ACCEPT_FILE) == 0)
			compulsory_flags |= BRASERO_BURN_FLAG_NO_TMP_FILES;
	}

	*supported |= supported_flags;
	*compulsory |= compulsory_flags;

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_burn_caps_get_flags (BraseroBurnCaps *self,
			     BraseroBurnSession *session,
			     BraseroBurnFlag *supported,
			     BraseroBurnFlag *compulsory)
{
	BraseroMedia media;
	BraseroTrackType input;
	BraseroBurnResult result;

	BraseroBurnFlag session_flags;
	/* FIXME: what's the meaning of NOGRACE when outputting ? */
	BraseroBurnFlag compulsory_flags = BRASERO_BURN_FLAG_NONE;
	BraseroBurnFlag supported_flags = BRASERO_BURN_FLAG_DONT_OVERWRITE|
					  BRASERO_BURN_FLAG_DONT_CLEAN_OUTPUT|
					  BRASERO_BURN_FLAG_CHECK_SIZE|
					  BRASERO_BURN_FLAG_NOGRACE;

	g_return_val_if_fail (BRASERO_IS_BURNCAPS (self), BRASERO_BURN_ERR);

	brasero_burn_session_get_input_type (session, &input);
	BRASERO_BURN_LOG_WITH_TYPE (&input,
				    BRASERO_PLUGIN_IO_NONE,
				    "FLAGS: searching available flags for input");

	if (brasero_burn_session_is_dest_file (session)) {
		BRASERO_BURN_LOG ("FLAGS: image required");

		/* In this case no APPEND/MERGE is possible */
		if (input.type == BRASERO_TRACK_TYPE_DISC)
			supported_flags |= BRASERO_BURN_FLAG_EJECT;

		/* FIXME: do the flag have the same meaning now with session
		 * making a clear distinction between tmp files and output */
		compulsory_flags |= BRASERO_BURN_FLAG_DONT_CLEAN_OUTPUT;

		*supported = supported_flags;
		*compulsory = compulsory_flags;

		BRASERO_BURN_LOG_FLAGS (supported_flags, "FLAGS: supported");
		BRASERO_BURN_LOG_FLAGS (compulsory_flags, "FLAGS: compulsory");
		return BRASERO_BURN_OK;
	}

	session_flags = brasero_burn_session_get_flags (session);
	BRASERO_BURN_LOG_FLAGS (session_flags, "FLAGS (session):");

	if (!brasero_burn_caps_flags_check_for_drive (session)) {
		BRASERO_BURN_LOG ("Session flags not supported");
		return BRASERO_BURN_ERR;
	}

	/* sanity check:
	 * - MERGE and BLANK are not possible together.
	 *   MERGE wins (always)
	 * - APPEND and MERGE are compatible. MERGE wins
	 * - APPEND and BLANK are incompatible and
	 *   They should both seldom be supported by a
	 *   backend, more probably each by a different
	 *   backend. */
	if ((session_flags & (BRASERO_BURN_FLAG_MERGE|BRASERO_BURN_FLAG_APPEND))
	&&  (session_flags & BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE))
		return BRASERO_BURN_NOT_SUPPORTED;
	
	/* Let's get flags for recording */
	supported_flags |= BRASERO_BURN_FLAG_EJECT;
	media = brasero_burn_session_get_dest_media (session);

	/* Here remove the BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE since that case
	 * will be handled later in case of failure */
	session_flags &= ~(BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE|
			   BRASERO_BURN_FLAG_FAST_BLANK);

	result = brasero_caps_get_flags_for_disc (session_flags,
						  media,
						  &input,
						  &supported_flags,
						  &compulsory_flags);
	session_flags = brasero_burn_session_get_flags (session);

	if (result == BRASERO_BURN_OK) {
		if (media & (BRASERO_MEDIUM_HAS_AUDIO|BRASERO_MEDIUM_HAS_DATA)) {
			gboolean operation;
	
			operation = (session_flags & (BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE|
						      BRASERO_BURN_FLAG_MERGE|
						      BRASERO_BURN_FLAG_APPEND)) != 0;
			if (!operation) {
				/* The backend supports natively the media but 
				 * no operation (append, merge, blank) was set.
				 */
				if (!(supported_flags & BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE)) {
					/* check if we can add the flag in case
					 * it isn't natively supported by a
					 * plugin. */
					if (brasero_burn_caps_can_blank (self, session) == BRASERO_BURN_OK)
						supported_flags |= BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE;
					else if (!(supported_flags & BRASERO_BURN_FLAG_MERGE))
						compulsory_flags |= BRASERO_BURN_FLAG_APPEND;
					else if (!(supported_flags & BRASERO_BURN_FLAG_APPEND))
						compulsory_flags |= BRASERO_BURN_FLAG_MERGE;
				}
				else {
					BraseroBurnFlag filter;

					filter = supported_flags & (BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE|
								    BRASERO_BURN_FLAG_MERGE|
								    BRASERO_BURN_FLAG_APPEND);

					/* if there is only one of the three
					 * operations supported then it must be
					 * compulsory. */
					if ((filter & BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE) == filter)
						compulsory_flags |= BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE;
					else if ((filter & BRASERO_BURN_FLAG_MERGE) == filter)
						compulsory_flags |= BRASERO_BURN_FLAG_MERGE;
					else if ((filter & BRASERO_BURN_FLAG_APPEND) == filter)
						compulsory_flags |= BRASERO_BURN_FLAG_APPEND;
				}
			}
			else {
				BraseroBurnFlag filter;

				/* There was an operation set for the media and
				 * the backend supports the specified operation
				 * (blanking/merging/appending) for the media.
				 * We're good.
				 * Nevertheless in case a backend supports all
				 * three operation we need to filter out the one
				 * (s) that were not specified
				 * NOTE: none of them should be compulsory 
				 * otherwise since they are exclusive then it 
				 * would mean the others are not possible
				 * What if many were specified? */

				/* Make sure blank flag is not added with merge
				 * choose merge if so. Merge and append are no
				 * problem */
				filter = session_flags & (BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE|
							  BRASERO_BURN_FLAG_MERGE|
							  BRASERO_BURN_FLAG_APPEND);

				supported_flags &= ~(BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE|
						     BRASERO_BURN_FLAG_MERGE|
						     BRASERO_BURN_FLAG_APPEND);
				supported_flags |= filter;
			}
		}
		else {
			/* make sure to filter MERGE/APPEND/BLANK from supported
			 * since there is no data on the medium */
			supported_flags &= ~(BRASERO_BURN_FLAG_MERGE|
					     BRASERO_BURN_FLAG_APPEND|
					     BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE);
		}
	}
	else {
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

		if (!(media & (BRASERO_MEDIUM_HAS_AUDIO|
			       BRASERO_MEDIUM_HAS_DATA|
			       BRASERO_MEDIUM_UNFORMATTED))) {
			/* media must have data/audio */
			return BRASERO_BURN_NOT_SUPPORTED;
		}

		if (session_flags & (BRASERO_BURN_FLAG_MERGE|
				     BRASERO_BURN_FLAG_APPEND)) {
			/* There is nothing we can do here */
			return BRASERO_BURN_NOT_SUPPORTED;
		}

		if (brasero_burn_caps_can_blank (self, session) != BRASERO_BURN_OK)
			return BRASERO_BURN_NOT_SUPPORTED;

		supported_flags |= BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE;
		compulsory_flags |= BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE;

		/* pretends it is blank and formatted to see if it would work.
		 * If it works then that means that the BLANK_BEFORE_WRITE flag
		 * is compulsory. */
		media &= ~(BRASERO_MEDIUM_CLOSED|
			   BRASERO_MEDIUM_APPENDABLE|
			   BRASERO_MEDIUM_UNFORMATTED|
			   BRASERO_MEDIUM_HAS_DATA|
			   BRASERO_MEDIUM_HAS_AUDIO);
		media |= BRASERO_MEDIUM_BLANK;
		session_flags &= ~BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE;

		result = brasero_caps_get_flags_for_disc (session_flags,
							  media,
							  &input,
							  &supported_flags,
							  &compulsory_flags);
		if (result != BRASERO_BURN_OK)
			return result;

		/* NOTE: the plugins can't have APPEND/MERGE for BLANK 
		 * media or it's an error so no need to filter them out */
	}

	if (supported_flags & BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE) {
		BraseroBurnFlag blank_compulsory = BRASERO_BURN_FLAG_NONE;
		BraseroBurnFlag blank_supported = BRASERO_BURN_FLAG_NONE;

		/* need to add blanking flags */
		brasero_burn_caps_get_blanking_flags (self,
						      session,
						      &blank_supported,
						      &blank_compulsory);
		supported_flags |= blank_supported;
		compulsory_flags |= blank_compulsory;
	}

	supported_flags = brasero_burn_caps_flags_update_for_drive (supported_flags, session);

	if (supported)
		*supported = supported_flags;

	if (compulsory)
		*compulsory = compulsory_flags;

	BRASERO_BURN_LOG_FLAGS (supported_flags, "FLAGS: supported");
	BRASERO_BURN_LOG_FLAGS (compulsory_flags, "FLAGS: compulsory");
	return BRASERO_BURN_OK;
}

/**
 * the following functions are used to register new caps
 */

static gint
brasero_burn_caps_sort (gconstpointer a, gconstpointer b)
{
	const BraseroCaps *caps_a = a;
	const BraseroCaps *caps_b = b;
	gint result;

	/* First put DISC (the most used caps) then IMAGE type; these two types
	 * are the ones that most often searched. At the end of the list we put
	 * DATA  and AUDIO.
	 * Another (sub)rule is that for DATA, DISC, AUDIO we put a caps that is
	 * encompassed by another before.
	 */

	result = caps_b->type.type - caps_a->type.type;
	if (result)
		return result;

	switch (caps_a->type.type) {
	case BRASERO_TRACK_TYPE_DISC:
		if (BRASERO_MEDIUM_TYPE (caps_a->type.subtype.media) !=
		    BRASERO_MEDIUM_TYPE (caps_b->type.subtype.media))
			return ((gint32) BRASERO_MEDIUM_TYPE (caps_a->type.subtype.media) -
			        (gint32) BRASERO_MEDIUM_TYPE (caps_b->type.subtype.media));

		if ((caps_a->type.subtype.media & (BRASERO_MEDIUM_DVD|BRASERO_MEDIUM_DVD_DL))
		&&  BRASERO_MEDIUM_SUBTYPE (caps_a->type.subtype.media) !=
		    BRASERO_MEDIUM_SUBTYPE (caps_b->type.subtype.media))			
			return ((gint32) BRASERO_MEDIUM_SUBTYPE (caps_a->type.subtype.media) -
			        (gint32) BRASERO_MEDIUM_SUBTYPE (caps_b->type.subtype.media));

		if (BRASERO_MEDIUM_ATTR (caps_a->type.subtype.media) !=
		    BRASERO_MEDIUM_ATTR (caps_b->type.subtype.media))
			return BRASERO_MEDIUM_ATTR (caps_a->type.subtype.media) -
			       BRASERO_MEDIUM_ATTR (caps_b->type.subtype.media);

		if (BRASERO_MEDIUM_STATUS (caps_a->type.subtype.media) !=
		    BRASERO_MEDIUM_STATUS (caps_b->type.subtype.media))
			return BRASERO_MEDIUM_STATUS (caps_a->type.subtype.media) -
			       BRASERO_MEDIUM_STATUS (caps_b->type.subtype.media);

		return (BRASERO_MEDIUM_INFO (caps_a->type.subtype.media) -
			BRASERO_MEDIUM_INFO (caps_b->type.subtype.media));

	case BRASERO_TRACK_TYPE_IMAGE:
		/* This way BIN subtype is always sorted at the end */
		return caps_a->type.subtype.img_format - caps_b->type.subtype.img_format;

	case BRASERO_TRACK_TYPE_AUDIO:
		if (caps_a->type.subtype.audio_format != caps_b->type.subtype.audio_format) {
			result = (caps_a->type.subtype.audio_format & caps_b->type.subtype.audio_format);
			if (result == caps_a->type.subtype.audio_format)
				return -1;
			else if (result == caps_b->type.subtype.audio_format)
				return 1;

			return  (gint32) caps_a->type.subtype.audio_format -
				(gint32) caps_b->type.subtype.audio_format;
		}
		break;

	case BRASERO_TRACK_TYPE_DATA:
		result = (caps_a->type.subtype.fs_type & caps_b->type.subtype.fs_type);
		if (result == caps_a->type.subtype.fs_type)
			return -1;
		else if (result == caps_b->type.subtype.fs_type)
			return 1;

		return (caps_a->type.subtype.fs_type - caps_b->type.subtype.fs_type);

	default:
		break;
	}

	return 0;
}

static BraseroCapsLink *
brasero_caps_link_copy (BraseroCapsLink *link)
{
	BraseroCapsLink *retval;

	retval = g_new0 (BraseroCapsLink, 1);
	retval->plugins = g_slist_copy (link->plugins);
	retval->caps = link->caps;

	return retval;
}

static void
brasero_caps_link_list_duplicate (BraseroCaps *dest, BraseroCaps *src)
{
	GSList *iter;

	for (iter = src->links; iter; iter = iter->next) {
		BraseroCapsLink *link;

		link = iter->data;
		dest->links = g_slist_prepend (dest->links, brasero_caps_link_copy (link));
	}
}

static BraseroCaps *
brasero_caps_copy (BraseroCaps *caps)
{
	BraseroCaps *retval;

	retval = g_new0 (BraseroCaps, 1);
	retval->flags = caps->flags;
	memcpy (&retval->type, &caps->type, sizeof (BraseroTrackType));
	retval->modifiers = g_slist_copy (caps->modifiers);

	return retval;
}

static void
brasero_caps_replicate_modifiers (BraseroCaps *dest, BraseroCaps *src)
{
	GSList *iter;

	for (iter = src->modifiers; iter; iter = iter->next) {
		BraseroPlugin *plugin;

		plugin = iter->data;

		if (g_slist_find (dest->modifiers, plugin))
			continue;

		dest->modifiers = g_slist_prepend (dest->modifiers, plugin);
	}
}

static void
brasero_caps_replicate_links (BraseroCaps *dest, BraseroCaps *src)
{
	BraseroBurnCaps *self;
	GSList *iter;

	self = brasero_burn_caps_get_default ();

	brasero_caps_link_list_duplicate (dest, src);

	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		BraseroCaps *iter_caps;
		GSList *links;

		iter_caps = iter->data;
		if (iter_caps == src)
			continue;

		for (links = iter_caps->links; links; links = links->next) {
			BraseroCapsLink *link;

			link = links->data;
			if (link->caps == src) {
				BraseroCapsLink *copy;

				copy = brasero_caps_link_copy (link);
				copy->caps = dest;
				iter_caps->links = g_slist_prepend (iter_caps->links, copy);
			}
		}
	}
}

static void
brasero_caps_replicate_tests (BraseroCaps *dest, BraseroCaps *src)
{
	BraseroBurnCaps *self;
	GSList *iter;

	self = brasero_burn_caps_get_default ();

	for (iter = self->priv->tests; iter; iter = iter->next) {
		BraseroCapsTest *test;
		GSList *links;

		test = iter->data;
		for (links = test->links; links; links = links->next) {
			BraseroCapsLink *link;

			link = links->data;
			if (link->caps == src) {
				BraseroCapsLink *copy;

				copy = brasero_caps_link_copy (link);
				copy->caps = dest;
				test->links = g_slist_prepend (test->links, copy);
			}
		}
	}
}

static BraseroCaps *
brasero_caps_copy_deep (BraseroCaps *caps)
{
	BraseroCaps *retval;
	BraseroBurnCaps *self;

	self = brasero_burn_caps_get_default ();

	retval = brasero_caps_copy (caps);
	brasero_caps_replicate_links (retval, caps);
	brasero_caps_replicate_tests (retval, caps);
	return retval;
}

static GSList *
brasero_caps_list_check_io (GSList *list, BraseroPluginIOFlag flags)
{
	GSList *iter;
	BraseroBurnCaps *self;

	self = brasero_burn_caps_get_default ();

	/* in this function we create the caps with the missing IO. All in the
	 * list have something in common with flags. */
	for (iter = list; iter; iter = iter->next) {
		BraseroCaps *caps;
		BraseroPluginIOFlag common;

		caps = iter->data;
		common = caps->flags & flags;
		if (common != caps->flags) {
			BraseroCaps *new_caps;

			/* (common == flags) && common != caps->flags
			 * caps->flags encompasses flags: Split the caps in two
			 * and only keep the interesting part */
			caps->flags &= ~common;

			/* this caps has changed and needs to be sorted again */
			self->priv->caps_list = g_slist_sort (self->priv->caps_list,
							      brasero_burn_caps_sort);

			new_caps = brasero_caps_copy_deep (caps);
			new_caps->flags = common;

			self->priv->caps_list = g_slist_insert_sorted (self->priv->caps_list,
								       new_caps,
								       brasero_burn_caps_sort);

			list = g_slist_prepend (list, new_caps);
		}
		else if (common != flags) {
			GSList *node, *next;
			BraseroPluginIOFlag complement = flags;

			complement &= ~common;
			for (node = list; node; node = next) {
				BraseroCaps *tmp;

				tmp = node->data;
				next = node->next;

				if (node == iter)
					continue;

				if (caps->type.type != tmp->type.type
				||  caps->type.subtype.media != tmp->type.subtype.media)
					continue;

				/* substract the flags and relocate them at the
				 * head of the list since we don't need to look
				 * them up again */
				complement &= ~(tmp->flags);
				list = g_slist_remove (list, tmp);
				list = g_slist_prepend (list, tmp);
			}

			if (complement != BRASERO_PLUGIN_IO_NONE) {
				BraseroCaps *new_caps;

				/* common == caps->flags && common != flags.
				 * Flags encompasses caps->flags. So we need to
				 * create a new caps for this type with the
				 * substraction of flags if the other part isn't
				 * in the list */
				new_caps = brasero_caps_copy (caps);
				new_caps->flags = flags & (~common);
				self->priv->caps_list = g_slist_insert_sorted (self->priv->caps_list,
									       new_caps,
									       brasero_burn_caps_sort);

				list = g_slist_prepend (list, new_caps);
			}
		}
	}

	return list;
}

GSList *
brasero_caps_image_new (BraseroPluginIOFlag flags,
			BraseroImageFormat format)
{
	BraseroImageFormat remaining_format;
	BraseroBurnCaps *self;
	GSList *retval = NULL;
	GSList *iter;

	BRASERO_BURN_LOG_WITH_FULL_TYPE (BRASERO_TRACK_TYPE_IMAGE,
					 format,
					 flags,
					 "New caps required");

	self = brasero_burn_caps_get_default ();

	remaining_format = format;

	/* We have to search all caps with or related to the format */
	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		BraseroCaps *caps;
		BraseroImageFormat common;
		BraseroPluginIOFlag common_io;

		caps = iter->data;
		if (caps->type.type != BRASERO_TRACK_TYPE_IMAGE)
			continue;

		common_io = caps->flags & flags;
		if (common_io == BRASERO_PLUGIN_IO_NONE)
			continue;

		common = (caps->type.subtype.img_format & format);
		if (common == BRASERO_IMAGE_FORMAT_NONE)
			continue;

		if (common != caps->type.subtype.img_format) {
			/* img_format encompasses format. Split it in two and
			 * keep caps with common format */
			SUBSTRACT (caps->type.subtype.img_format, common);
			self->priv->caps_list = g_slist_sort (self->priv->caps_list,
							      brasero_burn_caps_sort);

			caps = brasero_caps_copy_deep (caps);
			caps->type.subtype.img_format = common;

			self->priv->caps_list = g_slist_insert_sorted (self->priv->caps_list,
								       caps,
								       brasero_burn_caps_sort);
		}

		retval = g_slist_prepend (retval, caps);
		remaining_format &= ~common;
	}

	/* Now we make sure that all these new or already 
	 * existing caps have the proper IO Flags */
	retval = brasero_caps_list_check_io (retval, flags);

	if (remaining_format != BRASERO_IMAGE_FORMAT_NONE) {
		BraseroCaps *caps;

		caps = g_new0 (BraseroCaps, 1);
		caps->flags = flags;
		caps->type.subtype.img_format = remaining_format;
		caps->type.type = BRASERO_TRACK_TYPE_IMAGE;

		self->priv->caps_list = g_slist_insert_sorted (self->priv->caps_list,
							       caps,
							       brasero_burn_caps_sort);
		retval = g_slist_prepend (retval, caps);

		BRASERO_BURN_LOG_TYPE (&caps->type, "Created new caps");
	}

	return retval;
}

GSList *
brasero_caps_audio_new (BraseroPluginIOFlag flags,
			BraseroAudioFormat format)
{
	GSList *iter;
	GSList *retval = NULL;
	BraseroBurnCaps *self;
	GSList *encompassing = NULL;
	gboolean have_the_one = FALSE;

	BRASERO_BURN_LOG_WITH_FULL_TYPE (BRASERO_TRACK_TYPE_AUDIO,
					 format,
					 flags,
					 "New caps required");

	self = brasero_burn_caps_get_default ();

	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		BraseroCaps *caps;
		BraseroAudioFormat common;
		BraseroPluginIOFlag common_io;
		BraseroAudioFormat common_audio;
		BraseroAudioFormat common_video;

		caps = iter->data;

		if (caps->type.type != BRASERO_TRACK_TYPE_AUDIO)
			continue;

		common_io = (flags & caps->flags);
		if (common_io == BRASERO_PLUGIN_IO_NONE)
			continue;

		if (caps->type.subtype.audio_format == format) {
			/* that's the perfect fit */
			have_the_one = TRUE;
			retval = g_slist_prepend (retval, caps);
			continue;
		}

		/* Search caps strictly encompassed or encompassing our format
		 * NOTE: make sure that if there is a VIDEO stream in one of
		 * them, the other does have a VIDEO stream too. */
		common_audio = BRASERO_AUDIO_CAPS_AUDIO (caps->type.subtype.audio_format) & 
			       BRASERO_AUDIO_CAPS_AUDIO (format);
		if (common_audio == BRASERO_AUDIO_FORMAT_NONE)
			continue;

		common_video = BRASERO_AUDIO_CAPS_VIDEO (caps->type.subtype.audio_format) & 
			       BRASERO_AUDIO_CAPS_VIDEO (format);
		if (common_video == BRASERO_AUDIO_FORMAT_NONE)
			continue;

		common = common_audio|common_video;

		/* encompassed caps just add it to retval */
		if (caps->type.subtype.audio_format == common)
			retval = g_slist_prepend (retval, caps);

		/* encompassing caps keep it if we need to create perfect fit */
		if (format == common)
			encompassing = g_slist_prepend (encompassing, caps);
	}

	/* Now we make sure that all these new or already 
	 * existing caps have the proper IO Flags */
	retval = brasero_caps_list_check_io (retval, flags);

	if (!have_the_one) {
		BraseroCaps *caps;

		caps = g_new0 (BraseroCaps, 1);
		caps->flags = flags;
		caps->type.subtype.audio_format = format;
		caps->type.type = BRASERO_TRACK_TYPE_AUDIO;

		if (encompassing) {
			for (iter = encompassing; iter; iter = iter->next) {
				BraseroCaps *iter_caps;

				iter_caps = iter->data;
				brasero_caps_replicate_links (caps, iter_caps);
				brasero_caps_replicate_tests (caps, iter_caps);
				brasero_caps_replicate_modifiers (caps, iter_caps);
			}
		}

		self->priv->caps_list = g_slist_insert_sorted (self->priv->caps_list,
							       caps,
							       brasero_burn_caps_sort);
		retval = g_slist_prepend (retval, caps);

		BRASERO_BURN_LOG_TYPE (&caps->type, "Created new caps");
	}

	g_slist_free (encompassing);
	return retval;
}

GSList *
brasero_caps_data_new (BraseroImageFS fs_type)
{
	GSList *iter;
	GSList *retval = NULL;
	BraseroBurnCaps *self;
	GSList *encompassing = NULL;
	gboolean have_the_one = FALSE;

	BRASERO_BURN_LOG_WITH_FULL_TYPE (BRASERO_TRACK_TYPE_DATA,
					 fs_type,
					 BRASERO_PLUGIN_IO_NONE,
					 "New caps required");
	self = brasero_burn_caps_get_default ();

	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		BraseroCaps *caps;
		BraseroImageFS common;

		caps = iter->data;

		if (caps->type.type != BRASERO_TRACK_TYPE_DATA)
			continue;

		if (caps->type.subtype.fs_type == fs_type) {
			/* see if that's the perfect fit */
			have_the_one = TRUE;
			retval = g_slist_prepend (retval, caps);
			continue;
		}

		/* search caps strictly encompassing our format ... */
		common = caps->type.subtype.fs_type & fs_type;
		if (common == BRASERO_IMAGE_FS_NONE)
			continue;

		/* encompassed caps just add it to retval */
		if (caps->type.subtype.fs_type == common)
			retval = g_slist_prepend (retval, caps);

		/* encompassing caps keep it if we need to create perfect fit */
		if (fs_type == common)
			encompassing = g_slist_prepend (encompassing, caps);
	}

	if (!have_the_one) {
		BraseroCaps *caps;

		caps = g_new0 (BraseroCaps, 1);
		caps->flags = BRASERO_PLUGIN_IO_ACCEPT_FILE;
		caps->type.type = BRASERO_TRACK_TYPE_DATA;
		caps->type.subtype.fs_type = fs_type;

		if (encompassing) {
			for (iter = encompassing; iter; iter = iter->next) {
				BraseroCaps *iter_caps;

				iter_caps = iter->data;
				brasero_caps_replicate_links (caps, iter_caps);
				brasero_caps_replicate_tests (caps, iter_caps);
				brasero_caps_replicate_modifiers (caps, iter_caps);
			}
		}

		self->priv->caps_list = g_slist_insert_sorted (self->priv->caps_list,
							       caps,
							       brasero_burn_caps_sort);
		retval = g_slist_prepend (retval, caps);
	}

	g_slist_free (encompassing);
	return retval;
}

static GSList *
brasero_caps_disc_lookup_or_create (GSList *retval,
				    BraseroMedia media)
{
	GSList *iter;
	BraseroCaps *caps;

	if (!default_caps)
		brasero_burn_caps_get_default ();

	for (iter = default_caps->priv->caps_list; iter; iter = iter->next) {
		caps = iter->data;
		if (caps->type.subtype.media == media) {
			BRASERO_BURN_LOG_WITH_TYPE (&caps->type,
						    caps->flags,
						    "Retrieved");
			return g_slist_prepend (retval, caps);
		}
	}

	caps = g_new0 (BraseroCaps, 1);
	caps->flags = BRASERO_PLUGIN_IO_ACCEPT_FILE;
	caps->type.type = BRASERO_TRACK_TYPE_DISC;
	caps->type.subtype.media = media;

	BRASERO_BURN_LOG_WITH_TYPE (&caps->type,
				    caps->flags,
				    "Created");

	default_caps->priv->caps_list = g_slist_prepend (default_caps->priv->caps_list, caps);
	return g_slist_prepend (retval, caps);
}

static GSList *
brasero_caps_disc_new_status (GSList *retval,
			      BraseroMedia media,
			      BraseroMedia type)
{
	if ((type & BRASERO_MEDIUM_BLANK)
	&& !(media & BRASERO_MEDIUM_ROM)) {
		/* If media is blank there is no other possible property.
		 * BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDRW_RESTRICTED)
		 * condition is checked but in fact it's never valid since
		 * such a medium cannot exist if it hasn't been formatted before
		 * which is in contradiction with the fact is unformatted. */
		if (BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDRW_PLUS)
		||  BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDRW_RESTRICTED)
		||  BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDRW)
		||  BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDRW_PLUS_DL)) {
			/* This is only for above types */
			retval = brasero_caps_disc_lookup_or_create (retval,
								     media|
								     BRASERO_MEDIUM_BLANK);
			if (type & BRASERO_MEDIUM_UNFORMATTED)
				retval = brasero_caps_disc_lookup_or_create (retval,
									     media|
									     BRASERO_MEDIUM_BLANK|
									     BRASERO_MEDIUM_UNFORMATTED);
		}
		else
			retval = brasero_caps_disc_lookup_or_create (retval,
								     media|
								     BRASERO_MEDIUM_BLANK);
	}

	if (type & BRASERO_MEDIUM_CLOSED) {
		if (media & (BRASERO_MEDIUM_DVD|BRASERO_MEDIUM_DVD_DL))
			retval = brasero_caps_disc_lookup_or_create (retval,
								     media|
								     BRASERO_MEDIUM_CLOSED|
								     (type & BRASERO_MEDIUM_HAS_DATA)|
								     (type & BRASERO_MEDIUM_PROTECTED));
		else {
			if (type & BRASERO_MEDIUM_HAS_AUDIO)
				retval = brasero_caps_disc_lookup_or_create (retval,
									     media|
									     BRASERO_MEDIUM_CLOSED|
									     BRASERO_MEDIUM_HAS_AUDIO);
			if (type & BRASERO_MEDIUM_HAS_DATA)
				retval = brasero_caps_disc_lookup_or_create (retval,
									     media|
									     BRASERO_MEDIUM_CLOSED|
									     BRASERO_MEDIUM_HAS_DATA);
			if (BRASERO_MEDIUM_IS (type, BRASERO_MEDIUM_HAS_AUDIO|BRASERO_MEDIUM_HAS_DATA))
				retval = brasero_caps_disc_lookup_or_create (retval,
									     media|
									     BRASERO_MEDIUM_CLOSED|
									     BRASERO_MEDIUM_HAS_DATA|
									     BRASERO_MEDIUM_HAS_AUDIO);
		}
	}

	if ((type & BRASERO_MEDIUM_APPENDABLE)
	&& !(media & BRASERO_MEDIUM_ROM)
	&& !(media & BRASERO_MEDIUM_RESTRICTED)
	&& ! BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVD|BRASERO_MEDIUM_PLUS|BRASERO_MEDIUM_REWRITABLE)
	&& ! BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVD_DL|BRASERO_MEDIUM_PLUS|BRASERO_MEDIUM_REWRITABLE)) {
		if (media & BRASERO_MEDIUM_DVD)
			retval = brasero_caps_disc_lookup_or_create (retval,
								     media|
								     BRASERO_MEDIUM_APPENDABLE|
								     BRASERO_MEDIUM_HAS_DATA);
		else {
			if (type & BRASERO_MEDIUM_HAS_AUDIO)
				retval = brasero_caps_disc_lookup_or_create (retval,
									     media|
									     BRASERO_MEDIUM_APPENDABLE|
									     BRASERO_MEDIUM_HAS_AUDIO);
			if (type & BRASERO_MEDIUM_HAS_DATA)
				retval = brasero_caps_disc_lookup_or_create (retval,
									     media|
									     BRASERO_MEDIUM_APPENDABLE|
									     BRASERO_MEDIUM_HAS_DATA);
			if (BRASERO_MEDIUM_IS (type, BRASERO_MEDIUM_HAS_AUDIO|BRASERO_MEDIUM_HAS_DATA))
				retval = brasero_caps_disc_lookup_or_create (retval,
									     media|
									     BRASERO_MEDIUM_HAS_DATA|
									     BRASERO_MEDIUM_APPENDABLE|
									     BRASERO_MEDIUM_HAS_AUDIO);
		}
	}

	return retval;
}

static GSList *
brasero_caps_disc_new_attribute (GSList *retval,
				 BraseroMedia media,
				 BraseroMedia type)
{
	if (type & BRASERO_MEDIUM_REWRITABLE) {
		/* Always true for + media there are both single and dual layer */
		if (media & BRASERO_MEDIUM_PLUS)
			retval = brasero_caps_disc_new_status (retval,
							       media|BRASERO_MEDIUM_REWRITABLE,
							       type);
		/* There is no dual layer DVD-RW */
		else if (!(media & BRASERO_MEDIUM_DVD_DL))
			retval = brasero_caps_disc_new_status (retval,
							       media|BRASERO_MEDIUM_REWRITABLE,
							       type);
	}

	if ((type & BRASERO_MEDIUM_WRITABLE)
	&& !(media & BRASERO_MEDIUM_RESTRICTED))
		retval = brasero_caps_disc_new_status (retval,
						       media|BRASERO_MEDIUM_WRITABLE,
						       type);

	if (type & BRASERO_MEDIUM_ROM)
		retval = brasero_caps_disc_new_status (retval,
						       media|BRASERO_MEDIUM_ROM,
						       type);

	return retval;
}

static GSList *
brasero_caps_disc_new_subtype (GSList *retval,
			       BraseroMedia media,
			       BraseroMedia type)
{
	if (media & BRASERO_MEDIUM_BD) {
		if (type & BRASERO_MEDIUM_RANDOM)
			retval = brasero_caps_disc_new_attribute (retval,
								  media|BRASERO_MEDIUM_RANDOM,
								  type);
		if (type & BRASERO_MEDIUM_SRM)
			retval = brasero_caps_disc_new_attribute (retval,
								  media|BRASERO_MEDIUM_SRM,
								  type);
		if (type & BRASERO_MEDIUM_POW)
			retval = brasero_caps_disc_new_attribute (retval,
								  media|BRASERO_MEDIUM_POW,
								  type);
	}

	if (media & BRASERO_MEDIUM_DVD) {
		if (type & BRASERO_MEDIUM_SEQUENTIAL)
			retval = brasero_caps_disc_new_attribute (retval,
								  media|BRASERO_MEDIUM_SEQUENTIAL,
								  type);

		if (type & BRASERO_MEDIUM_RESTRICTED)
			retval = brasero_caps_disc_new_attribute (retval,
								  media|BRASERO_MEDIUM_RESTRICTED,
								  type);

		if (type & BRASERO_MEDIUM_PLUS)
			retval = brasero_caps_disc_new_attribute (retval,
								  media|BRASERO_MEDIUM_PLUS,
								  type);
		if (type & BRASERO_MEDIUM_ROM)
			retval = brasero_caps_disc_new_status (retval,
							       media|BRASERO_MEDIUM_ROM,
							       type);
	}

	if (media & BRASERO_MEDIUM_DVD_DL) {
		if (type & BRASERO_MEDIUM_SEQUENTIAL)
			retval = brasero_caps_disc_new_attribute (retval,
								  media|BRASERO_MEDIUM_SEQUENTIAL,
								  type);

		if (type & BRASERO_MEDIUM_JUMP)
			retval = brasero_caps_disc_new_attribute (retval,
								  media|BRASERO_MEDIUM_JUMP,
								  type);

		if (type & BRASERO_MEDIUM_PLUS)
			retval = brasero_caps_disc_new_attribute (retval,
								  media|BRASERO_MEDIUM_PLUS,
								  type);

		if (type & BRASERO_MEDIUM_ROM)
			retval = brasero_caps_disc_new_status (retval,
							       media|BRASERO_MEDIUM_ROM,
							       type);
	}

	return retval;
}

GSList *
brasero_caps_disc_new (BraseroMedia type)
{
	GSList *retval = NULL;

	if (type & BRASERO_MEDIUM_FILE)
		retval = brasero_caps_disc_lookup_or_create (retval, BRASERO_MEDIUM_FILE);					       

	if (type & BRASERO_MEDIUM_CD)
		retval = brasero_caps_disc_new_attribute (retval,
							  BRASERO_MEDIUM_CD,
							  type);

	if (type & BRASERO_MEDIUM_DVD)
		retval = brasero_caps_disc_new_subtype (retval,
							BRASERO_MEDIUM_DVD,
							type);

	if (type & BRASERO_MEDIUM_DVD_DL)
		retval = brasero_caps_disc_new_subtype (retval,
							BRASERO_MEDIUM_DVD_DL,
							type);

	if (type & BRASERO_MEDIUM_RAM)
		retval = brasero_caps_disc_new_attribute (retval,
							  BRASERO_MEDIUM_RAM,
							  type);

	if (type & BRASERO_MEDIUM_BD)
		retval = brasero_caps_disc_new_subtype (retval,
							BRASERO_MEDIUM_BD,
							type);

	return retval;
}

/**
 * these functions are to create links
 */

static BraseroCapsLink *
brasero_caps_find_link_for_input (BraseroCaps *caps,
				  BraseroCaps *input)
{
	GSList *links;

	for (links = caps->links; links; links = links->next) {
		BraseroCapsLink *link;

		link = links->data;
		if (link->caps == input)
			return link;
	}

	return NULL;
}

static void
brasero_caps_create_links (BraseroCaps *output,
	 		   GSList *inputs,
	 		   BraseroPlugin *plugin)
{
	for (; inputs; inputs = inputs->next) {
		BraseroCaps *input;
		BraseroCapsLink *link;

		input = inputs->data;

		if (output == input) {
			BRASERO_BURN_LOG ("Same input and output for link. Dropping");
			continue;
		}

		if (input->flags == output->flags
		&&  input->type.type == output->type.type  
		&&  input->type.subtype.media == output->type.subtype.media)
			BRASERO_BURN_LOG ("Recursive link");

		link = brasero_caps_find_link_for_input (output, input);

#if 0

		/* Mainly for extra debugging */
		BRASERO_BURN_LOG_TYPE (&output->type, "Linking");
		BRASERO_BURN_LOG_TYPE (&input->type, "to");
		BRASERO_BURN_LOG ("with %s", brasero_plugin_get_name (plugin));

#endif

		if (!link) {
			link = g_new0 (BraseroCapsLink, 1);
			link->caps = input;
			link->plugins = g_slist_prepend (NULL, plugin);

			output->links = g_slist_prepend (output->links, link);
		}
		else
			link->plugins = g_slist_prepend (link->plugins, plugin);
	}
}

void
brasero_plugin_link_caps (BraseroPlugin *plugin,
			  GSList *outputs,
			  GSList *inputs)
{
	/* we make sure the caps exists and if not we create them */
	for (; outputs; outputs = outputs->next) {
		BraseroCaps *output;

		output = outputs->data;
		brasero_caps_create_links (output, inputs, plugin);
	}
}

void
brasero_plugin_blank_caps (BraseroPlugin *plugin,
			   GSList *caps_list)
{
	for (; caps_list; caps_list = caps_list->next) {
		BraseroCaps *caps;
		BraseroCapsLink *link;

		caps = caps_list->data;

		if (caps->type.type != BRASERO_TRACK_TYPE_DISC)
			continue;
	
		BRASERO_BURN_LOG_WITH_TYPE (&caps->type,
					    caps->flags,
					    "Adding blank caps for");

		/* we need to find the link whose caps is NULL */
		link = brasero_caps_find_link_for_input (caps, NULL);
		if (!link) {
			link = g_new0 (BraseroCapsLink, 1);
			link->caps = NULL;
			link->plugins = g_slist_prepend (NULL, plugin);

			caps->links = g_slist_prepend (caps->links, link);
		}
		else
			link->plugins = g_slist_prepend (link->plugins, plugin);
	}
}

void
brasero_plugin_process_caps (BraseroPlugin *plugin,
			     GSList *caps_list)
{
	for (; caps_list; caps_list = caps_list->next) {
		BraseroCaps *caps;

		caps = caps_list->data;
		caps->modifiers = g_slist_prepend (caps->modifiers, plugin);
	}
}

void
brasero_plugin_check_caps (BraseroPlugin *plugin,
			   BraseroChecksumType type,
			   GSList *caps_list)
{
	BraseroCapsTest *test = NULL;
	BraseroBurnCaps *self;
	GSList *iter;

	/* Find the the BraseroCapsTest for this type; if none create it */
	self = brasero_burn_caps_get_default ();
	for (iter = self->priv->tests; iter; iter = iter->next) {
		BraseroCapsTest *tmp;

		tmp = iter->data;
		if (tmp->type == type) {
			test = tmp;
			break;
		}
	}

	if (!test) {
		test = g_new0 (BraseroCapsTest, 1);
		test->type = type;
		self->priv->tests = g_slist_prepend (self->priv->tests, test);
	}

	g_object_unref (self);

	for (; caps_list; caps_list = caps_list->next) {
		GSList *links;
		BraseroCaps *caps;
		BraseroCapsLink *link;

		caps = caps_list->data;

		/* try to find a link for the above caps, if none create one */
		link = NULL;
		for (links = test->links; links; links = links->next) {
			BraseroCapsLink *tmp;

			tmp = links->data;
			if (tmp->caps == caps) {
				link = tmp;
				break;
			}
		}

		if (!link) {
			link = g_new0 (BraseroCapsLink, 1);
			link->caps = caps;
			test->links = g_slist_prepend (test->links, link);
		}

		link->plugins = g_slist_prepend (link->plugins, plugin);
	}
}

/** 
 * This is to find out what are the capacities of a plugin 
 */

BraseroBurnResult
brasero_burn_caps_plugin_can_burn (BraseroBurnCaps *self,
				   BraseroPlugin *plugin)
{
	GSList *iter;

	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		BraseroCaps *caps;
		GSList *links;

		caps = iter->data;
		if (caps->type.type != BRASERO_TRACK_TYPE_DISC)
			continue;

		for (links = caps->links; links; links = links->next) {
			BraseroCapsLink *link;
			GSList *plugins;

			link = links->data;

			/* see if the plugin is in the link by going through the list */
			for (plugins = link->plugins; plugins; plugins = plugins->next) {
				BraseroPlugin *tmp;

				tmp = plugins->data;
				if (tmp == plugin)
					return BRASERO_BURN_OK;
			}
		}
	}

	return BRASERO_BURN_NOT_SUPPORTED;
}

BraseroBurnResult
brasero_burn_caps_plugin_can_image (BraseroBurnCaps *self,
				    BraseroPlugin *plugin)
{
	GSList *iter;

	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		BraseroTrackDataType destination;
		BraseroCaps *caps;
		GSList *links;

		caps = iter->data;
		if (caps->type.type != BRASERO_TRACK_TYPE_IMAGE
		&&  caps->type.type != BRASERO_TRACK_TYPE_AUDIO
		&&  caps->type.type != BRASERO_TRACK_TYPE_DATA)
			continue;

		destination = caps->type.type;
		for (links = caps->links; links; links = links->next) {
			BraseroCapsLink *link;
			GSList *plugins;

			link = links->data;
			if (!link->caps
			||   link->caps->type.type == destination)
				continue;

			/* see if the plugin is in the link by going through the list */
			for (plugins = link->plugins; plugins; plugins = plugins->next) {
				BraseroPlugin *tmp;

				tmp = plugins->data;
				if (tmp == plugin)
					return BRASERO_BURN_OK;
			}
		}
	}

	return BRASERO_BURN_NOT_SUPPORTED;
}

BraseroBurnResult
brasero_burn_caps_plugin_can_convert (BraseroBurnCaps *self,
				      BraseroPlugin *plugin)
{
	GSList *iter;

	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		BraseroTrackDataType destination;
		BraseroCaps *caps;
		GSList *links;

		caps = iter->data;
		if (caps->type.type != BRASERO_TRACK_TYPE_IMAGE
		&&  caps->type.type != BRASERO_TRACK_TYPE_AUDIO)
			continue;

		destination = caps->type.type;
		for (links = caps->links; links; links = links->next) {
			BraseroCapsLink *link;
			GSList *plugins;

			link = links->data;
			if (!link->caps
			||   link->caps->type.type != destination)
				continue;

			/* see if the plugin is in the link by going through the list */
			for (plugins = link->plugins; plugins; plugins = plugins->next) {
				BraseroPlugin *tmp;

				tmp = plugins->data;
				if (tmp == plugin)
					return BRASERO_BURN_OK;
			}
		}
	}

	return BRASERO_BURN_NOT_SUPPORTED;
}

gboolean
brasero_burn_caps_can_checksum (BraseroBurnCaps *self)
{
	GSList *iter;

	if (self->priv->tests == NULL)
		return FALSE;

	for (iter = self->priv->tests; iter; iter = iter->next) {
		BraseroCapsTest *tmp;
		GSList *links;

		tmp = iter->data;
		for (links = tmp->links; links; links = links->next) {
			BraseroCapsLink *link;

			link = links->data;
			if (brasero_caps_link_active (link))
				return TRUE;
		}
	}

	return FALSE;
}
