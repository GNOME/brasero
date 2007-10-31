/***************************************************************************
 *            burn-caps.c
 *
 *  mar avr 18 20:58:42 2006
 *  Copyright  2006  Rouquier Philippe
 *  brasero-app@wanadoo.fr
 ***************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n.h>

#include <gconf/gconf-client.h>

#include <nautilus-burn-drive.h>

#include "burn-basics.h"
#include "burn-debug.h"
#include "brasero-ncb.h"
#include "burn-medium.h"
#include "burn-session.h"
#include "burn-plugin.h"
#include "burn-plugin-private.h"
#include "burn-task.h"
#include "burn-caps.h"

#define BRASERO_ENGINE_GROUP_KEY	"/app/brasero/config/engine-group"

G_DEFINE_TYPE (BraseroBurnCaps, brasero_burn_caps, G_TYPE_OBJECT);

struct BraseroBurnCapsPrivate {
	GSList *caps_list;
	GHashTable *groups;

	gchar *group_str;
	guint group_id;
};

struct _BraseroCaps {
	GSList *links;
	GSList *tests;
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
	GSList *plugins;
	BraseroChecksumType type;
};
typedef struct _BraseroCapsTest BraseroCapsTest;

#define SUBSTRACT(a, b)		((a) &= ~((b)&(a)))

enum
{
	CAPS_CHANGED_SIGNAL,
	LAST_SIGNAL
};
static guint caps_signals [LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;
static BraseroBurnCaps *default_caps = NULL;

#define BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG(session)				\
{										\
	brasero_burn_session_log (session, "Unsupported type of task operation"); \
	BRASERO_BURN_LOG ("Unsupported type of task operation");		\
	return NULL;								\
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

	caps_signals [CAPS_CHANGED_SIGNAL] =
		g_signal_new ("caps_changed",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
		              G_STRUCT_OFFSET (BraseroBurnCapsClass, caps_changed),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE, 0);
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

static gboolean
brasero_caps_has_pointing_links (BraseroBurnCaps *self,
				 BraseroCaps *caps)
{
	GSList *iter;

	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		BraseroCaps *iter_caps;
		GSList *links;

		iter_caps = iter->data;
		if (iter_caps == caps)
			continue;

		for (links = iter_caps->links; links; links = links->next) {
			BraseroCapsLink *link;

			link = links->data;
			if (links->data == caps)
				return TRUE;
		}
	}

	return FALSE;
}

void
brasero_caps_unregister_plugin (BraseroPlugin *plugin)
{
	GSList *iter, *next;
	BraseroBurnCaps *self;

	self = brasero_burn_caps_get_default ();

	/* we search the plugin in all links and remove it from the list */
	for (iter = self->priv->caps_list; iter; iter = next) {
		GSList *links;
		BraseroCaps *caps;
		GSList *links_next;

		caps = iter->data;
		next = iter->next;

		caps->modifiers = g_slist_remove (caps->modifiers, plugin);
		for (links = caps->links; links; links = links_next) {
			BraseroCapsLink *link;

			link = links->data;
			links_next = links->next;

			link->plugins = g_slist_remove (link->plugins, plugin);
			if (link->plugins)
				continue;

			/* if the plugin was the last one in the link, remove the link */
			caps->links = g_slist_remove (caps->links, link);

			/* if the caps pointed by the link hasn't got any other link, remove the caps */
			if (link->caps
			&& !link->caps->links
			&& !brasero_caps_has_pointing_links (self, caps)) {
				self->priv->caps_list = g_slist_remove (self->priv->caps_list, caps);
				g_free (caps);
			}

			g_free (link);
		}

		/* if the caps hasn't got any other link nor modifier remove it */
		if (caps->links || caps->modifiers)
			continue;

		if (brasero_caps_has_pointing_links (self, caps))
			continue;

		self->priv->caps_list = g_slist_remove (self->priv->caps_list, caps);
		g_free (caps);
	}
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
	BraseroBurnFlag supported_flags = BRASERO_BURN_FLAG_NONE;
	BraseroBurnFlag compulsory_flags = BRASERO_BURN_FLAG_ALL;

	media = brasero_burn_session_get_dest_media (session);
	BRASERO_BURN_LOG_DISC_TYPE (media, "Testing blanking caps for");

	if (media == BRASERO_MEDIUM_NONE) {
		BRASERO_BURN_LOG ("Blanking not possible: no media");
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
				if (!brasero_plugin_get_blank_flags (plugin,
								     media,
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
	flags = brasero_burn_session_get_flags (session) & (BRASERO_BURN_FLAG_NOGRACE|
							    BRASERO_BURN_FLAG_FAST_BLANK);

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
				BraseroBurnFlag compulsory;
				BraseroBurnFlag supported;
				BraseroPlugin *plugin;

				plugin = plugins->data;

				if (!brasero_plugin_get_active (plugin))
					continue;

				if (!brasero_plugin_get_blank_flags (plugin,
								     media,
								     &supported,
								     &compulsory))
					continue;

				if ((flags & supported) != flags
				||  (flags & compulsory) != compulsory)
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

	flags = brasero_burn_session_get_flags (session) & (BRASERO_BURN_FLAG_NOGRACE|
							    BRASERO_BURN_FLAG_FAST_BLANK);

	BRASERO_BURN_LOG_DISC_TYPE (media, "checking blanking caps for");

	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		BraseroCaps *caps;
		GSList *links;

		caps = iter->data;
		if (caps->type.type != BRASERO_TRACK_TYPE_DISC)
			continue;

		if ((media & caps->type.subtype.media) != media)
			continue;

		BRASERO_BURN_LOG_WITH_TYPE (&caps->type,
					    BRASERO_PLUGIN_IO_NONE,
					    "Searching links for caps");

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
				BraseroBurnFlag compulsory;
				BraseroBurnFlag supported;
				BraseroPlugin *plugin;

				plugin = plugins->data;
				if (!brasero_plugin_get_active (plugin))
					continue;

				if (!brasero_plugin_get_blank_flags (plugin,
								     media,
								     &supported,
								     &compulsory))
					continue;

				if ((flags & supported) != flags
				||  (flags & compulsory) != compulsory)
					continue;

				return BRASERO_BURN_OK;
			}
		}
	}

	return BRASERO_BURN_NOT_SUPPORTED;
}

BraseroTask *
brasero_burn_caps_new_checksuming_task (BraseroBurnCaps *self,
					BraseroBurnSession *session,
					GError **error)
{
	BraseroTrackType input;
	guint checksum_type;
	BraseroTrack *track;
	BraseroTask *task;
	BraseroCaps *caps;
	BraseroJob *job;
	GSList *tracks;
	GSList *iter;

	brasero_burn_session_get_input_type (session, &input);
	BRASERO_BURN_LOG_WITH_TYPE (&input,
				    BRASERO_PLUGIN_IO_NONE,
				    "Creating checksuming task with input");

	caps = brasero_caps_find_start_caps (&input);
	if (!caps)
		BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG_ERROR (session, error);

	BRASERO_BURN_LOG_WITH_TYPE (&caps->type,
				    BRASERO_PLUGIN_IO_NONE,
				    "TESTING: found first caps (%i plugins for testing)",
				    g_slist_length (caps->tests));

	tracks = brasero_burn_session_get_tracks (session);
	if (g_slist_length (tracks) != 1) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("only one track at a time can be checked"));
		return NULL;
	}

	track = tracks->data;
	checksum_type = brasero_track_get_checksum_type (track);

	for (iter = caps->tests; iter; iter = iter->next) {
		BraseroPlugin *candidate = NULL;
		BraseroCapsTest *test;
		GSList *plugins;

		test = iter->data;

		/* check this caps test supports the right checksum type */
		if (!(test->type & checksum_type))
			continue;

		/* Go through all plugins and choose the plugin that:
		 * - have the highest priority
		 * - is active */
		for (plugins = test->plugins; plugins; plugins = plugins->next) {
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

		if (candidate) {
			job = BRASERO_JOB (g_object_new (brasero_plugin_get_gtype (candidate),
							 "output", NULL,
							 NULL));
			g_signal_connect (job,
					  "error",
					  G_CALLBACK (brasero_burn_caps_job_error_cb),
					  caps);

			task = BRASERO_TASK (g_object_new (BRASERO_TYPE_TASK,
							   "session", session,
							   "action", BRASERO_TASK_ACTION_CHECKSUM,
							   NULL));
			brasero_task_add_item (task, BRASERO_TASK_ITEM (job));

			return task;
		}
	}

	BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG_ERROR (session, error);
}

/**
 *
 */

static void
brasero_caps_link_get_record_flags (BraseroCapsLink *link,
				    BraseroMedia media,
				    BraseroBurnFlag *supported,
				    BraseroBurnFlag *compulsory_retval)
{
	GSList *iter;
	BraseroBurnFlag compulsory;

	compulsory = BRASERO_BURN_FLAG_ALL;

	/* Go through all plugins to get the supported/... record flags for link */
	for (iter = link->plugins; iter; iter = iter->next) {
		BraseroPlugin *plugin;
		BraseroBurnFlag plugin_supported;
		BraseroBurnFlag plugin_compulsory;

		plugin = iter->data;
		if (!brasero_plugin_get_active (plugin))
			continue;

		brasero_plugin_get_record_flags (plugin,
						 media,
						 &plugin_supported,
						 &plugin_compulsory);

		*supported |= plugin_supported;
		compulsory &= plugin_compulsory;
	}

	*compulsory_retval = compulsory;
}

static void
brasero_caps_link_get_data_flags (BraseroCapsLink *link,
				  BraseroMedia media,
				  BraseroBurnFlag *supported)
{
	GSList *iter;

	/* Go through all plugins the get the supported/... data flags for link */
	for (iter = link->plugins; iter; iter = iter->next) {
		BraseroPlugin *plugin;
		BraseroBurnFlag plugin_supported;
		BraseroBurnFlag plugin_compulsory;

		plugin = iter->data;
		if (!brasero_plugin_get_active (plugin))
			continue;

		brasero_plugin_get_image_flags (plugin,
					        media,
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

static BraseroBurnResult
brasero_caps_link_check_data_flags (BraseroCapsLink *link,
				    BraseroBurnFlag session_flags,
				    BraseroMedia media)
{
	BraseroBurnFlag flags;
	BraseroBurnFlag supported = BRASERO_BURN_FLAG_NONE;

	/* here we just make sure that at least one of the plugins in the link
	 * can comply with the flags (APPEND/MERGE) */
	flags = session_flags & (BRASERO_BURN_FLAG_APPEND|BRASERO_BURN_FLAG_MERGE);

	if ((flags & (BRASERO_BURN_FLAG_APPEND|BRASERO_BURN_FLAG_MERGE)) == 0)
		return TRUE;

	brasero_caps_link_get_data_flags (link, media, &supported);
	if ((flags & supported) != flags)
		return FALSE;

	return TRUE;
}

static gboolean
brasero_caps_link_check_record_flags (BraseroCapsLink *link,
				      BraseroBurnFlag session_flags,
				      BraseroMedia media)
{
	BraseroBurnFlag flags;
	BraseroBurnFlag supported = BRASERO_BURN_FLAG_NONE;
	BraseroBurnFlag compulsory = BRASERO_BURN_FLAG_NONE;

	flags = session_flags & BRASERO_PLUGIN_BURN_FLAG_MASK;

	brasero_caps_link_get_record_flags (link, media, &supported, &compulsory);

	if ((flags & supported) != flags
	||  (flags & compulsory) != compulsory)
		return FALSE;

	return TRUE;
}

static GSList *
brasero_caps_try_links (BraseroCaps *caps,
			BraseroBurnFlag session_flags,
			BraseroMedia media,
			BraseroTrackType *input,
			BraseroPluginIOFlag io_flags)
{
	GSList *iter;

	BRASERO_BURN_LOG_WITH_TYPE (&caps->type, BRASERO_PLUGIN_IO_NONE, "try_links");

	/* For a link to be followed it must first:
	 * - link to a caps with correct io flags
	 * - have at least a plugin accepting the record flags if caps type is
	 *   a disc (that means that the link is the recording part)
	 *
	 * and either:
	 * - link to a caps equal to the input
	 * - link to a caps (linking itself to another caps, ...) accepting the
	 *   input
	 */

	/* Don't check for the perfect fit first and then follow links; that's
	 * important since it gives priority to the right plugins. One example:
	 * growisofs can handle DATA right from the start but has a lower
	 * priority than libburn. In this case growisofs would be used every 
	 * time for DATA despite its having a lower priority than libburn if we
	 * were looking for the best fit first */
	for (iter = caps->links; iter; iter = iter->next) {
		BraseroCapsLink *link;
		gboolean is_type;
		GSList *results;

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
		if (link->caps->type.type == BRASERO_TRACK_TYPE_DATA
		&& !brasero_caps_link_check_data_flags (link, session_flags, media))
			continue;

		is_type = brasero_caps_is_compatible_type (link->caps, input);
		if ((link->caps->flags & BRASERO_PLUGIN_IO_ACCEPT_FILE) && is_type)
			return g_slist_prepend (NULL, link);

		/* we can't go further than a DISC type */
		if (link->caps->type.type == BRASERO_TRACK_TYPE_DISC)
			continue;

		if ((link->caps->flags & io_flags) == BRASERO_PLUGIN_IO_NONE)
			continue;

		/* try to see where the inputs of this caps leads to */
		results = brasero_caps_try_links (link->caps,
						  session_flags,
						  media,
						  input,
						  io_flags);
		if (results)
			return g_slist_prepend (results, link);
	}

	return NULL;
}

static BraseroPlugin *
brasero_caps_link_find_plugin (BraseroCapsLink *link,
			       gint group_id,
			       BraseroBurnSession *session,
			       BraseroTrackType *output,
			       BraseroMedia media)
{
	GSList *iter;
	BraseroPlugin *candidate;
	BraseroBurnFlag rec_flags;
	BraseroBurnFlag data_flags;

	rec_flags = brasero_burn_session_get_flags (session) &
		    (BRASERO_BURN_FLAG_DUMMY|
		     BRASERO_BURN_FLAG_MULTI|
		     BRASERO_BURN_FLAG_DAO|
		     BRASERO_BURN_FLAG_BURNPROOF|
		     BRASERO_BURN_FLAG_OVERBURN|
		     BRASERO_BURN_FLAG_NOGRACE);

	data_flags = brasero_burn_session_get_flags (session) &
		    (BRASERO_BURN_FLAG_APPEND|
		     BRASERO_BURN_FLAG_MERGE);

	/* Go through all plugins for a link and find the best one. It must:
	 * - be active
	 * - be part of the group (as much as possible)
	 * - have the highest priority
	 * - support the flags */
	candidate = NULL;
	for (iter = link->plugins; iter; iter = iter->next) {
		BraseroPlugin *plugin;
		BraseroBurnFlag supported;
		BraseroBurnFlag compulsory;

		plugin = iter->data;

		if (!brasero_plugin_get_active (plugin))
			continue;

		if (output->type == BRASERO_TRACK_TYPE_DISC) {
			brasero_plugin_get_record_flags (plugin,
							 media,
							 &supported,
							 &compulsory);
			if ((rec_flags & supported) != rec_flags
			||  (rec_flags & compulsory) != compulsory)
				continue;
		}

		if (link->caps->type.type == BRASERO_TRACK_TYPE_DATA) {
			brasero_plugin_get_image_flags (plugin,
						        media,
						        &supported,
						        &compulsory);
			if ((data_flags & supported) != data_flags
			||  (data_flags & compulsory) != compulsory)
				continue;
		}

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

static GSList *
brasero_caps_add_processing_plugins_to_task (BraseroBurnSession *session,
					     BraseroTask *task,
					     BraseroCaps *caps,
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
						 "output", &caps->type,
						 NULL));
		g_signal_connect (job,
				  "error",
				  G_CALLBACK (brasero_burn_caps_job_error_cb),
				  link);

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

		BRASERO_BURN_LOG ("%s (modifier) added to task",
				  brasero_plugin_get_name (plugin));

		brasero_task_add_item (task, BRASERO_TASK_ITEM (job));
	}

	return retval;
}

GSList *
brasero_burn_caps_new_task (BraseroBurnCaps *self,
			    BraseroBurnSession *session,
			    GError **error)
{
	BraseroPluginProcessFlag position;
	BraseroBurnFlag session_flags;
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
	list = brasero_caps_try_links (last_caps,
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
		if (!(session_flags & BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE)
		||    brasero_burn_caps_can_blank (self, session) != BRASERO_BURN_OK)
			BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG_ERROR (session, error);

		/* retry with the same disc type but blank this time */
		media &= ~(BRASERO_MEDIUM_CLOSED|
			   BRASERO_MEDIUM_APPENDABLE|
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
		list = brasero_caps_try_links (last_caps,
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
	for (iter = list; iter; iter = iter->next) {
		BraseroTrackType *plugin_output;
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
			plugin_output = &next_link->caps->type;
		}
		else
			plugin_output = &output;

		/* first see if there are track processing plugins */
		result = brasero_caps_add_processing_plugins_to_task (session,
								      task,
								      link->caps,
								      position);
		retval = g_slist_concat (retval, result);

		/* create job from the best plugin in link */
		plugin = brasero_caps_link_find_plugin (link,
							group_id,
							session,
							plugin_output,
							media);

		/* This is meant to have plugins in the same group id as much as
		 * possible */
		if (brasero_plugin_get_group (plugin) > 0 && group_id <= 0)
			group_id = brasero_plugin_get_group (plugin);

		type = brasero_plugin_get_gtype (plugin);
		job = BRASERO_JOB (g_object_new (type,
						 "output", plugin_output,
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

		position = BRASERO_PLUGIN_RUN_NEVER;
	}
	g_slist_free (list);

	if (last_caps->type.type != BRASERO_TRACK_TYPE_DISC) {
		GSList *result;

		/* imaging to a file so we never run the processing plugin on
		 * the fly in this case so as to allow the last plugin to output
		 * correctly to a file */
		result = brasero_caps_add_processing_plugins_to_task (session,
								      NULL,
								      last_caps,
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

static GSList *
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
		return NULL;
	}

	if (caps->type.type == BRASERO_TRACK_TYPE_DISC)
		media = caps->type.subtype.media;
	else
		media = BRASERO_MEDIUM_FILE;

	return brasero_caps_try_links (caps,
				       session_flags,
				       media,
				       input,
				       flags);
}

static GSList *
brasero_caps_try_output_with_blanking (BraseroBurnCaps *self,
				       BraseroBurnSession *session,
				       BraseroTrackType *output,
				       BraseroTrackType *input,
				       BraseroPluginIOFlag io_flags)
{
	GSList *list;
	BraseroMedia media;
	BraseroCaps *last_caps;
	BraseroBurnFlag session_flags;

	session_flags = brasero_burn_session_get_flags (session);
	list = brasero_caps_try_output (session_flags,
					output,
					input,
					io_flags);
	if (list)
		return list;

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
		return NULL;

	/* output is a disc try with initial blanking */
	BRASERO_BURN_LOG ("Support for input/output failed. Trying with initial blanking");

	/* apparently nothing can be done to reach our goal. Maybe that
	 * is because we first have to blank the disc. If so add a blank 
	 * task to the others as a first step */
	if (!(session_flags & BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE)
	||    brasero_burn_caps_can_blank (self, session) != BRASERO_BURN_OK)
		return NULL;

	/* retry with the same disc type but blank this time */
	media = output->subtype.media;
	media &= ~(BRASERO_MEDIUM_CLOSED|
		   BRASERO_MEDIUM_APPENDABLE|
		   BRASERO_MEDIUM_HAS_DATA|
		   BRASERO_MEDIUM_HAS_AUDIO);
	media |= BRASERO_MEDIUM_BLANK;
	output->subtype.media = media;

	last_caps = brasero_caps_find_start_caps (output);
	if (!last_caps)
		return NULL;

	/* if the flag BLANK_BEFORE_WRITE was set then remove it since
	 * we are actually blanking. Simply the record plugin won't have
	 * to do it. */
	session_flags &= ~BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE;
	list = brasero_caps_try_links (last_caps,
				       session_flags,
				       media,
				       input,
				       io_flags);
	return list;
}

static void
brasero_burn_caps_get_output (BraseroBurnCaps *self,
			      BraseroBurnSession *session,
			      BraseroTrackType *output)
{
	if (!brasero_burn_session_is_dest_file (session)) {
		BraseroBurnFlag flags;

		output->type = BRASERO_TRACK_TYPE_DISC;
		output->subtype.media = brasero_burn_session_get_dest_media (session);

		/* that's a special case: if a disc is rewritable but closed we
		 * may consider it to be blank on condition that we can blank it
		 * and that the flag blank before is set */
		flags = brasero_burn_session_get_flags (session);
		if (BRASERO_MEDIUM_IS (output->subtype.media, BRASERO_MEDIUM_CLOSED|BRASERO_MEDIUM_REWRITABLE)
		&&  brasero_burn_caps_can_blank (self, session) == BRASERO_BURN_OK
		&& (flags & BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE)) {
			output->subtype.media &= ~(BRASERO_MEDIUM_CLOSED|
						   BRASERO_MEDIUM_HAS_DATA|
						   BRASERO_MEDIUM_HAS_AUDIO);
			output->subtype.media |= BRASERO_MEDIUM_BLANK;
		}
	}
	else {
		output->type = BRASERO_TRACK_TYPE_IMAGE;
		output->subtype.img_format = brasero_burn_session_get_output_format (session);
	}
}

BraseroBurnResult
brasero_burn_caps_is_input_supported (BraseroBurnCaps *self,
				      BraseroBurnSession *session,
				      BraseroTrackType *input)
{
	GSList *list;
	BraseroTrackType output;

	BRASERO_BURN_LOG_WITH_TYPE (input,
				    BRASERO_PLUGIN_IO_NONE,
				    "Checking support for input");

	brasero_burn_caps_get_output (self,
				      session,
				      &output);

	list = brasero_caps_try_output_with_blanking (self,
						      session,
						      &output,
						      input,
						      BRASERO_PLUGIN_IO_ACCEPT_FILE);
	if (!list) {
		BRASERO_BURN_LOG_WITH_TYPE (input,
					    BRASERO_PLUGIN_IO_NONE,
					    "Input not supported");
		return BRASERO_BURN_NOT_SUPPORTED;
	}

	g_slist_free (list);
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_burn_caps_is_output_supported (BraseroBurnCaps *self,
				       BraseroBurnSession *session,
				       BraseroTrackType *output)
{
	GSList *list;
	BraseroTrackType input;
	BraseroPluginIOFlag io_flags;

	BRASERO_BURN_LOG_WITH_TYPE (output,
				    BRASERO_PLUGIN_IO_NONE,
				    "Checking support for output");

	/* Here flags don't matter as we don't record anything.
	 * Even the IOFlags since that can be checked later with
	 * brasero_burn_caps_get_flags.
	 */
	if (BRASERO_BURN_SESSION_NO_TMP_FILE (session))
		io_flags = BRASERO_PLUGIN_IO_ACCEPT_PIPE;
	else
		io_flags = BRASERO_PLUGIN_IO_ACCEPT_FILE;

	brasero_burn_session_get_input_type (session, &input);
	list = brasero_caps_try_output_with_blanking (self,
						      session,
						      output,
						      &input,
						      io_flags);
	if (!list) {
		BRASERO_BURN_LOG_WITH_TYPE (output,
					    BRASERO_PLUGIN_IO_NONE,
					    "Output not supported");
		return BRASERO_BURN_NOT_SUPPORTED;
	}

	g_slist_free (list);
	return BRASERO_BURN_OK;
}

BraseroMedia
brasero_burn_caps_get_required_media_type (BraseroBurnCaps *self,
					   BraseroBurnSession *session)
{
	BraseroBurnFlag session_flags;
	BraseroMedia required_media;
	BraseroPluginIOFlag io_flags;
	BraseroBurnFlag rec_flags;
	BraseroTrackType input;
	GSList *iter;

	if (brasero_burn_session_is_dest_file (session))
		return BRASERO_MEDIUM_FILE;

	rec_flags = brasero_burn_session_get_flags (session) & (BRASERO_BURN_FLAG_DUMMY|
								BRASERO_BURN_FLAG_MULTI|
								BRASERO_BURN_FLAG_DAO|
								BRASERO_BURN_FLAG_BURNPROOF|
								BRASERO_BURN_FLAG_OVERBURN|
								BRASERO_BURN_FLAG_NOGRACE);

	required_media = BRASERO_MEDIUM_WRITABLE;
	if (BRASERO_BURN_SESSION_APPEND (session))
		required_media |= BRASERO_MEDIUM_APPENDABLE;
	else
		required_media |= BRASERO_MEDIUM_BLANK;

	brasero_burn_session_get_input_type (session, &input);

	if (BRASERO_BURN_SESSION_NO_TMP_FILE (session))
		io_flags = BRASERO_PLUGIN_IO_ACCEPT_PIPE;
	else
		io_flags = BRASERO_PLUGIN_IO_ACCEPT_FILE;

	self = brasero_burn_caps_get_default ();
	session_flags = brasero_burn_session_get_flags (session);
	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		BraseroCaps *caps;
		GSList *list;

		caps = iter->data;

		if (caps->type.type != BRASERO_TRACK_TYPE_DISC)
			continue;

		list = brasero_caps_try_links (caps,
					       session_flags,
					       caps->type.subtype.media,
					       &input,
					       io_flags);
		if (!list)
			continue;

		/* This caps work, add its subtype */
		required_media |= caps->type.subtype.media;
		g_slist_free (list);
	}

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
							    caps->type.subtype.media,
							    &rec_supported,
							    &rec_compulsory);

			/* see if that link can handle the record flags */
			tmp = session_flags & (BRASERO_BURN_FLAG_DUMMY|
					       BRASERO_BURN_FLAG_MULTI|
					       BRASERO_BURN_FLAG_DAO|
					       BRASERO_BURN_FLAG_BURNPROOF|
					       BRASERO_BURN_FLAG_OVERBURN|
					       BRASERO_BURN_FLAG_NOGRACE);
			if ((tmp & rec_supported) != tmp
			||  (tmp & rec_compulsory) != rec_compulsory)
				continue;
		}

		if (link->caps->type.type == BRASERO_TRACK_TYPE_DATA) {
			BraseroBurnFlag tmp;

			brasero_caps_link_get_data_flags (link,
							  media,
						    	  &data_supported);

			/* see if that link can handle the data flags */
			tmp = session_flags & (BRASERO_BURN_FLAG_APPEND|
					       BRASERO_BURN_FLAG_MERGE);

			if ((tmp & data_supported) != tmp)
				continue;
		}

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

		BRASERO_BURN_LOG ("FLAGS: supported %i %i", supported_flags, compulsory_flags);
		return BRASERO_BURN_OK;
	}

	session_flags = brasero_burn_session_get_flags (session);
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

	result = brasero_caps_get_flags_for_disc (session_flags,
						  media,
						  &input,
						  &supported_flags,
						  &compulsory_flags);

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

		if (!(media & (BRASERO_MEDIUM_HAS_AUDIO|BRASERO_MEDIUM_HAS_DATA))) {
			/* media must have data/audio */
			return BRASERO_BURN_NOT_SUPPORTED;
		}

		if (session_flags & (BRASERO_BURN_FLAG_MERGE|BRASERO_BURN_FLAG_APPEND)) {
			/* There is nothing we can do here */
			return BRASERO_BURN_NOT_SUPPORTED;
		}

		if (brasero_burn_caps_can_blank (self, session) != BRASERO_BURN_OK)
			return BRASERO_BURN_NOT_SUPPORTED;

		supported_flags |= BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE;
		compulsory_flags |= BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE;

		/* pretends it is blank and see if it would work. If it works
		 * then that means that the BLANK_BEFORE_WRITE flag is
		 * compulsory. */
		media &= ~(BRASERO_MEDIUM_CLOSED|
			   BRASERO_MEDIUM_APPENDABLE|
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

	/* if it's an appendable disc and we're not going to blank it before
	 * writing then we can't have dao for next sessions */
	if (media & (BRASERO_MEDIUM_HAS_AUDIO|BRASERO_MEDIUM_HAS_DATA)
	&& (session_flags & BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE) == 0) {
		supported_flags &= ~BRASERO_BURN_FLAG_DAO;
		compulsory_flags &= ~BRASERO_BURN_FLAG_DAO;
	}

	/* if we want to leave the session open with DVD+/-R we can't use dao */
	if ((media & BRASERO_MEDIUM_DVD)
	&&  (session_flags & BRASERO_BURN_FLAG_MULTI)
	&&  (session_flags & BRASERO_BURN_FLAG_DAO)) {
		supported_flags &= ~BRASERO_BURN_FLAG_DAO;
		compulsory_flags &= ~BRASERO_BURN_FLAG_DAO;
	}

	*supported = supported_flags;
	*compulsory = compulsory_flags;

	BRASERO_BURN_LOG ("FLAGS: supported %i %i", supported_flags, compulsory_flags);

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

		if ((caps_a->type.subtype.media & BRASERO_MEDIUM_DVD)
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

static gint
brasero_caps_link_sort (gconstpointer a, gconstpointer b)
{
	const BraseroCapsLink *link_a = a;
	const BraseroCapsLink *link_b = b;

	/* special case for blanking caps links which dont have any caps */
	if (!link_a->caps)
		return 1;
	if (!link_b->caps)
		return -1;

	return brasero_burn_caps_sort (link_a->caps, link_b->caps);
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
		dest->links = g_slist_insert_sorted (dest->links,
						     brasero_caps_link_copy (link),
						     brasero_caps_link_sort);
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
	retval->tests = g_slist_copy (caps->tests);

	return retval;
}

static void
brasero_caps_add_test (BraseroCaps *caps,
		       BraseroChecksumType type,
		       BraseroPlugin *plugin)
{
	GSList *tests;

	for (tests = caps->tests; tests; tests = tests->next)  {
		BraseroCapsTest *test;
		BraseroChecksumType common;

		test = tests->data;

		common = (test->type & type);
		if (common == BRASERO_CHECKSUM_NONE)
			continue;

		if (g_slist_find (test->plugins, plugin)) {
			type &= ~common;
			continue;
		}

		if (common != test->type) {
			BraseroCapsTest *tmp;

			/* split it in two and keep the common part */
			test->type &= ~common;

			tmp = g_new0 (BraseroCapsTest, 1);
			tmp->plugins = g_slist_copy (test->plugins);
			caps->tests = g_slist_prepend (caps->tests, tmp);

			test = tmp;
			test->type = common;
		}

		type &= ~common;
		test->plugins = g_slist_prepend (test->plugins, plugin);
	}

	if (type != BRASERO_CHECKSUM_NONE) {
		BraseroCapsTest *test;

		test = g_new0 (BraseroCapsTest, 1);
		test->type = type;
		test->plugins = g_slist_prepend (NULL, plugin);
		caps->tests = g_slist_prepend (caps->tests, test);
	}
}

static void
brasero_caps_replicate_modifiers_tests (BraseroCaps *dest, BraseroCaps *src)
{
	GSList *iter;

	for (iter = src->modifiers; iter; iter = iter->next) {
		BraseroPlugin *plugin;

		plugin = iter->data;

		if (g_slist_find (dest->modifiers, plugin))
			continue;

		dest->modifiers = g_slist_prepend (dest->modifiers, plugin);
	}

	for (iter = src->tests; iter; iter = iter->next) {
		BraseroCapsTest *test;
		GSList *plugins;

		test = iter->data;
		for (plugins = test->plugins; plugins; plugins = plugins->next) {
			BraseroPlugin *plugin;

			plugin = plugins->data;
			brasero_caps_add_test (dest, test->type, plugin);
		}
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
				iter_caps->links = g_slist_insert_sorted (iter_caps->links,
									  copy,
									  brasero_caps_link_sort);
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
			 * caps->flags encompasses flags: Split the link in two
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

				/* common == caps->flags  && common != flags.
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
	BraseroBurnCaps *self;
	GSList *retval = NULL;
	GSList *iter;

	BRASERO_BURN_LOG_WITH_FULL_TYPE (BRASERO_TRACK_TYPE_IMAGE,
					 format,
					 flags,
					 "Creating new caps");

	self = brasero_burn_caps_get_default ();

	for (iter = self->priv->caps_list; iter && format != BRASERO_IMAGE_FORMAT_NONE; iter = iter->next) {
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
		else if (common == format) {
			/* format == caps->type.subtype.img_format */
			retval = g_slist_prepend (retval, caps);
			goto end;
		}

		retval = g_slist_prepend (retval, caps);
		format &= ~common;
	}

	/* Now we make sure that all these new or already 
	 * existing caps have the proper IO Flags */
	retval = brasero_caps_list_check_io (retval, flags);

	if (format != BRASERO_IMAGE_FORMAT_NONE){
		BraseroCaps *caps;

		caps = g_new0 (BraseroCaps, 1);
		caps->flags = flags;
		caps->type.subtype.img_format = format;
		caps->type.type = BRASERO_TRACK_TYPE_IMAGE;

		self->priv->caps_list = g_slist_insert_sorted (self->priv->caps_list,
							       caps,
							       brasero_burn_caps_sort);
		retval = g_slist_prepend (retval, caps);
	}

end:

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
					 "Creating new caps");

	self = brasero_burn_caps_get_default ();

	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		BraseroCaps *caps;
		BraseroAudioFormat common;
		BraseroPluginIOFlag common_io;

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

		/* search caps strictly encompassed or encompassing our format */
		common = caps->type.subtype.audio_format & format;
		if (common == BRASERO_AUDIO_FORMAT_NONE)
			continue;

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
				brasero_caps_replicate_modifiers_tests (caps, iter_caps);
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
					 "Creating new caps");
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
				brasero_caps_replicate_modifiers_tests (caps, iter_caps);
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

GSList *
brasero_caps_disc_new (BraseroMedia media)
{
	GSList *iter;
	BraseroBurnCaps *self;
	GSList *retval = NULL;
	BraseroCaps *caps = NULL;
	GSList *encompassing = NULL;
 
	BRASERO_BURN_LOG_DISC_TYPE (media, "Creating new caps");

	self = brasero_burn_caps_get_default ();

	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		BraseroMedia common;
		BraseroMedia media_less;
		BraseroMedia caps_media;
		BraseroMedia common_type;
		BraseroMedia common_attr;
		BraseroMedia common_info;
		BraseroMedia common_status;
		BraseroMedia common_subtype;

		caps = iter->data;

		media_less = BRASERO_MEDIUM_NONE;

		if (caps->type.type != BRASERO_TRACK_TYPE_DISC)
			continue;

		caps_media = caps->type.subtype.media;

		if (caps_media == media) {
			retval = g_slist_prepend (retval, caps);
			goto end;
		}

		/* the media and the caps have something in common if and only
		 * if type, status and attribute have all at least one thing in
		 * common (and subtype if type is DVD) */
		common = (caps_media & media);

		common_type = BRASERO_MEDIUM_TYPE (common);
		if (common_type == BRASERO_MEDIUM_NONE)
			continue;

		if (common_type & BRASERO_MEDIUM_DVD) {
			common_subtype = BRASERO_MEDIUM_SUBTYPE (common);
			if (common_subtype == BRASERO_MEDIUM_NONE)
				continue;
		}
		else
			common_subtype = BRASERO_MEDIUM_NONE;

		common_attr = BRASERO_MEDIUM_ATTR (common);
		if (common_attr == BRASERO_MEDIUM_NONE)
			continue;

		common_status = BRASERO_MEDIUM_STATUS (common);
		if (common_status == BRASERO_MEDIUM_NONE)
			continue;

		/* info flags are cumulative and not exclusive like above. i.e.
		 * you can have DATA + PROTECTED or DATA + AUDIO. Moreover they
		 * should only be found when CLOSED/APPENDABLE flags are set. */
		if (common_status & (BRASERO_MEDIUM_APPENDABLE|BRASERO_MEDIUM_CLOSED)) {
			common_info = BRASERO_MEDIUM_INFO (common);
			if (common_info == BRASERO_MEDIUM_NONE)
				continue;
		}
		else /* for blank disc */
			common_info = BRASERO_MEDIUM_NONE;

		if (BRASERO_MEDIUM_TYPE (caps_media) != common_type) {
			BraseroCaps *new_caps;

			/* common_type == media_type && common_type != caps_type
			 * so caps_type encompasses media_type: split and keep
			 * the part we are interested in */
			caps->type.subtype.media &= ~common_type;

			new_caps = brasero_caps_copy_deep (caps);
			new_caps->type.subtype.media &= ~BRASERO_MEDIUM_TYPE (caps_media);
			new_caps->type.subtype.media |= common_type;

			if (!(caps->type.subtype.media & BRASERO_MEDIUM_DVD)
			&&   (caps->type.subtype.media & BRASERO_MEDIUM_CD))
				caps->type.subtype.media &= ~(BRASERO_MEDIUM_DL|
							      BRASERO_MEDIUM_PLUS|
							      BRASERO_MEDIUM_SEQUENTIAL|
							      BRASERO_MEDIUM_RESTRICTED);

			if (!(new_caps->type.subtype.media & BRASERO_MEDIUM_DVD)
			&&   (new_caps->type.subtype.media & BRASERO_MEDIUM_CD))
				new_caps->type.subtype.media &= ~(BRASERO_MEDIUM_DL|
								  BRASERO_MEDIUM_PLUS|
								  BRASERO_MEDIUM_SEQUENTIAL|
								  BRASERO_MEDIUM_RESTRICTED);

			/* The order of the caps may have changed now */
			self->priv->caps_list = g_slist_sort (self->priv->caps_list,
							      brasero_burn_caps_sort);

			self->priv->caps_list = g_slist_insert_sorted (self->priv->caps_list,
								       new_caps,
								       brasero_burn_caps_sort);
			caps = new_caps;

			media_less = common_type;
			if (!(media & BRASERO_MEDIUM_DVD)
			&&   (media & BRASERO_MEDIUM_CD))
				media_less |= (BRASERO_MEDIUM_DL|
					       BRASERO_MEDIUM_PLUS|
					       BRASERO_MEDIUM_SEQUENTIAL|
					       BRASERO_MEDIUM_RESTRICTED);
		}
		else if (BRASERO_MEDIUM_TYPE (media) != common_type) {
			BraseroMedia first_half, second_half;

			/* common_type != media_type && common_type == caps_type
			 * so caps_type encompasses media_type.
			 * split the media in two and call ourselves for each */
			first_half = media & (~common_type);

			if (!(first_half & BRASERO_MEDIUM_DVD)
			&&   (first_half & BRASERO_MEDIUM_CD))
				first_half &= ~(BRASERO_MEDIUM_DL|
						BRASERO_MEDIUM_PLUS|
						BRASERO_MEDIUM_SEQUENTIAL|
						BRASERO_MEDIUM_RESTRICTED);

			retval = g_slist_concat (retval, brasero_caps_disc_new (first_half));

			second_half = media & (~BRASERO_MEDIUM_TYPE (media));
			second_half |= common_type;

			if (!(second_half & BRASERO_MEDIUM_DVD)
			&&   (second_half & BRASERO_MEDIUM_CD))
				second_half &= ~(BRASERO_MEDIUM_DL|
						 BRASERO_MEDIUM_PLUS|
						 BRASERO_MEDIUM_SEQUENTIAL|
						 BRASERO_MEDIUM_RESTRICTED);

			retval = g_slist_concat (retval, brasero_caps_disc_new (second_half));
			goto end;
		}

		if (common_subtype) {
			if (BRASERO_MEDIUM_SUBTYPE (caps_media) != common_subtype) {
				/* common_subtype == media_subtype &&
				 * common_subtype != caps_subtype
				 * so caps_subtype encompasses media_subtype: 
				 * split and keep the part we are interested in
				 */
				caps->type.subtype.media &= ~common_subtype;

				/* The order of the caps may have changed now */
				self->priv->caps_list = g_slist_sort (self->priv->caps_list,
								      brasero_burn_caps_sort);

				caps = brasero_caps_copy_deep (caps);
				caps->type.subtype.media &= ~BRASERO_MEDIUM_SUBTYPE (caps_media);
				caps->type.subtype.media |= common_subtype;

				self->priv->caps_list = g_slist_insert_sorted (self->priv->caps_list,
									       caps,
									       brasero_burn_caps_sort);

				media_less = common_subtype;
			}
			else if (BRASERO_MEDIUM_SUBTYPE (media) != common_subtype) {
				BraseroMedia first_half, second_half;

				/* common_subtype != media_subtype &&
				 * common_subtype == caps_subtype
				 * so caps_subtype encompasses media_subtype.
				 * split the media in two and call ourselves for
				 * each */
				first_half = (media|common_type) & (~common_subtype);

				retval = g_slist_concat (retval, brasero_caps_disc_new (first_half));

				second_half = (media|common_type) & (~BRASERO_MEDIUM_SUBTYPE (media));
				second_half |= common_subtype;

				retval = g_slist_concat (retval, brasero_caps_disc_new (second_half));
				goto end;
			}
		}

		if (BRASERO_MEDIUM_ATTR (caps_media) != common_attr) {
			BraseroCaps *new_caps;

			/* common_attr == media_attr && common_attr != caps_attr
			 * so caps_attr encompasses media_attr: split */
			caps->type.subtype.media &= ~common_attr;

			new_caps = brasero_caps_copy_deep (caps);
			new_caps->type.subtype.media &= ~BRASERO_MEDIUM_ATTR (caps_media);
			new_caps->type.subtype.media |= common_attr;

			if (!(caps->type.subtype.media & BRASERO_MEDIUM_WRITABLE)
			&&  !(caps->type.subtype.media & BRASERO_MEDIUM_REWRITABLE)
			&&   (caps->type.subtype.media & BRASERO_MEDIUM_ROM))
				caps->type.subtype.media &= ~(BRASERO_MEDIUM_APPENDABLE|BRASERO_MEDIUM_BLANK);

			if (!(new_caps->type.subtype.media & BRASERO_MEDIUM_WRITABLE)
			&&  !(new_caps->type.subtype.media & BRASERO_MEDIUM_REWRITABLE)
			&&   (new_caps->type.subtype.media & BRASERO_MEDIUM_ROM))
				new_caps->type.subtype.media &= ~(BRASERO_MEDIUM_APPENDABLE|BRASERO_MEDIUM_BLANK);

			/* The order of the caps may have changed now */
			self->priv->caps_list = g_slist_sort (self->priv->caps_list,
							      brasero_burn_caps_sort);

			self->priv->caps_list = g_slist_insert_sorted (self->priv->caps_list,
								       new_caps,
								       brasero_burn_caps_sort);
			caps = new_caps;

			media_less = common_attr;
			if (!(media & BRASERO_MEDIUM_WRITABLE)
			&&  !(media & BRASERO_MEDIUM_REWRITABLE)
			&&   (media & BRASERO_MEDIUM_ROM))
				media_less |= (BRASERO_MEDIUM_APPENDABLE|BRASERO_MEDIUM_BLANK);
		}
		else if (BRASERO_MEDIUM_ATTR (media) != common_attr) {
			BraseroMedia first_half, second_half;
			/* common_attr != media_attr && common_attr == caps_attr
			 * so media_attr encompasses caps_attr:
			 * split the media in two and call ourselves for each */
			first_half = (media|common_type|common_subtype) & (~common_attr);

			if (!(first_half & BRASERO_MEDIUM_WRITABLE)
			&&  !(first_half & BRASERO_MEDIUM_REWRITABLE)
			&&   (first_half & BRASERO_MEDIUM_ROM))
				first_half &= ~(BRASERO_MEDIUM_APPENDABLE|BRASERO_MEDIUM_BLANK);

			retval = g_slist_concat (retval, brasero_caps_disc_new (first_half));

			second_half = (media|common_type|common_subtype) & (~(BRASERO_MEDIUM_ATTR (media)));
			second_half |= common_attr;

			if (!(second_half & BRASERO_MEDIUM_WRITABLE)
			&&  !(second_half & BRASERO_MEDIUM_REWRITABLE)
			&&   (second_half & BRASERO_MEDIUM_ROM))
				second_half &= ~(BRASERO_MEDIUM_APPENDABLE|BRASERO_MEDIUM_BLANK);

			retval = g_slist_concat (retval, brasero_caps_disc_new (second_half));
			goto end;
		}

		if (BRASERO_MEDIUM_STATUS (caps_media) != common_status) {
			BraseroCaps *new_caps;

			/* common_status == media_status && common_status != caps_status
			 * so caps_status encompasses media_status: split */
			caps->type.subtype.media &= ~common_status;

			new_caps = brasero_caps_copy_deep (caps);
			new_caps->type.subtype.media &= ~BRASERO_MEDIUM_STATUS (caps_media);
			new_caps->type.subtype.media |= common_status;

			if (!(caps->type.subtype.media & BRASERO_MEDIUM_APPENDABLE)
			&&  !(caps->type.subtype.media & BRASERO_MEDIUM_CLOSED)
			&&   (caps->type.subtype.media & BRASERO_MEDIUM_BLANK))
				caps->type.subtype.media &= ~(BRASERO_MEDIUM_HAS_AUDIO|BRASERO_MEDIUM_HAS_DATA);

			if (!(new_caps->type.subtype.media & BRASERO_MEDIUM_APPENDABLE)
			&&  !(new_caps->type.subtype.media & BRASERO_MEDIUM_CLOSED)
			&&   (new_caps->type.subtype.media & BRASERO_MEDIUM_BLANK))
				new_caps->type.subtype.media &= ~(BRASERO_MEDIUM_HAS_AUDIO|BRASERO_MEDIUM_HAS_DATA);

			/* The order of the caps may have changed now */
			self->priv->caps_list = g_slist_sort (self->priv->caps_list,
							      brasero_burn_caps_sort);

			self->priv->caps_list = g_slist_insert_sorted (self->priv->caps_list,
								       new_caps,
								       brasero_burn_caps_sort);

			caps = new_caps;

			media_less = common_status;
			if (!(media & BRASERO_MEDIUM_APPENDABLE)
			&&  !(media & BRASERO_MEDIUM_CLOSED)
			&&   (media & BRASERO_MEDIUM_BLANK))
				media_less |= (BRASERO_MEDIUM_HAS_AUDIO|BRASERO_MEDIUM_HAS_DATA);
		}
		else if (BRASERO_MEDIUM_STATUS (media) != common_status) {
			BraseroMedia first_half, second_half;

			/* common_status != media_status && common_status == caps_status
			 * so caps_status encompasses media_status:
			 * split the media in two and call ourselves for each */
			first_half = (media|common_type|common_subtype|common_attr) & (~common_status);

			if (!(first_half & BRASERO_MEDIUM_APPENDABLE)
			&&  !(first_half & BRASERO_MEDIUM_CLOSED)
			&&   (first_half & BRASERO_MEDIUM_BLANK))
				first_half &= ~(BRASERO_MEDIUM_HAS_AUDIO|BRASERO_MEDIUM_HAS_DATA);

			retval = g_slist_concat (retval, brasero_caps_disc_new (first_half));

			second_half = (media|common_type|common_subtype|common_attr) & (~BRASERO_MEDIUM_STATUS (media));
			second_half |= common_status;

			if (!(second_half & BRASERO_MEDIUM_APPENDABLE)
			&&  !(second_half & BRASERO_MEDIUM_CLOSED)
			&&   (second_half & BRASERO_MEDIUM_BLANK))
				second_half &= ~(BRASERO_MEDIUM_HAS_AUDIO|BRASERO_MEDIUM_HAS_DATA);

			retval = g_slist_concat (retval, brasero_caps_disc_new (second_half));
			goto end;
		}

		media &= ~media_less;

		/* If we find a caps encompassing ours then duplicate it.
		 * If we find a caps encompassed by ours then add it to retval.
		 * The above rule also applies to perfect hit */

		/* for perfect hit and encompassed caps */
		if (common_info == BRASERO_MEDIUM_INFO (caps->type.subtype.media)) {
			retval = g_slist_prepend (retval, caps);

			if (common_info == BRASERO_MEDIUM_INFO (media))
				break;

			continue;
		}

		/* strictly encompassing caps: the problem is that there could
		 * many other strictly encompassing caps in the list and we need
		 * their links, caps point ... as wellif we need to create that
		 * perfect hit. So we add them to a list and they are stricly
		 * encompassing */
		if (common_info == BRASERO_MEDIUM_INFO (media))
			encompassing = g_slist_prepend (encompassing, caps);
	}

	if (BRASERO_MEDIUM_TYPE (media) == BRASERO_MEDIUM_NONE
	|| ((BRASERO_MEDIUM_TYPE (media) & BRASERO_MEDIUM_DVD) && !(BRASERO_MEDIUM_ATTR (media) & BRASERO_MEDIUM_ROM) && BRASERO_MEDIUM_SUBTYPE (media) == BRASERO_MEDIUM_NONE)
	||  BRASERO_MEDIUM_ATTR (media) == BRASERO_MEDIUM_NONE
	||  BRASERO_MEDIUM_STATUS (media) == BRASERO_MEDIUM_NONE)
		goto end;

	/* no perfect hit was found */
	caps = g_new0 (BraseroCaps, 1);
	caps->flags = BRASERO_PLUGIN_IO_ACCEPT_FILE;
	caps->type.type = BRASERO_TRACK_TYPE_DISC;
	caps->type.subtype.media = media;

	if (encompassing) {
		for (iter = encompassing; iter; iter = iter->next) {
			BraseroCaps *iter_caps;

			iter_caps = iter->data;
			brasero_caps_replicate_links (caps, iter_caps);
			brasero_caps_replicate_modifiers_tests (caps, iter_caps);
		}
	}
	
	self->priv->caps_list = g_slist_insert_sorted (self->priv->caps_list,
						       caps,
						       brasero_burn_caps_sort);

	retval = g_slist_prepend (retval, caps);

end:

	g_slist_free (encompassing);

	BRASERO_BURN_LOG ("Returning %i caps", g_slist_length (retval));
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

		if (!link) {
			link = g_new0 (BraseroCapsLink, 1);
			link->caps = input;
			link->plugins = g_slist_prepend (NULL, plugin);

			output->links = g_slist_insert_sorted (output->links,
							       link,
							       brasero_caps_link_sort);
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

			caps->links = g_slist_insert_sorted (caps->links,
							     link,
							     brasero_caps_link_sort);
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
	for (; caps_list; caps_list = caps_list->next) {
		BraseroCaps *caps;

		caps = caps_list->data;
		brasero_caps_add_test (caps, type, plugin);
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
		&&  caps->type.type != BRASERO_TRACK_TYPE_AUDIO)
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
