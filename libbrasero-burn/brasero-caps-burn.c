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

#include "brasero-caps-burn.h"
#include "burn-caps.h"
#include "burn-debug.h"
#include "burn-plugin-private.h"
#include "burn-task.h"

/**
 * This macro is used to determine whether or not blanking could change anything
 * for the medium so that we can write to it.
 */
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
			     BRASERO_BURN_ERROR,				\
			     BRASERO_BURN_ERROR_GENERAL,			\
			     _("An internal error occured"));	 		\
	BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG (session);				\
}

/* That function receives all errors returned by the object and 'learns' from 
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

static gboolean
brasero_burn_caps_sort_modifiers (gconstpointer a,
				  gconstpointer b)
{
	BraseroPlugin *plug_a = BRASERO_PLUGIN (a);
	BraseroPlugin *plug_b = BRASERO_PLUGIN (b);

	return brasero_plugin_get_priority (plug_a) -
	       brasero_plugin_get_priority (plug_b);
}

static GSList *
brasero_caps_add_processing_plugins_to_task (BraseroBurnSession *session,
					     BraseroTask *task,
					     BraseroCaps *caps,
					     BraseroTrackType *io_type,
					     BraseroPluginProcessFlag position)
{
	GSList *retval = NULL;
	GSList *modifiers;
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
	 * - accept the position flags */
	modifiers = g_slist_copy (caps->modifiers);
	modifiers = g_slist_sort (modifiers, brasero_burn_caps_sort_modifiers);

	for (iter = modifiers; iter; iter = iter->next) {
		BraseroPluginProcessFlag flags;
		BraseroPlugin *plugin;
		BraseroJob *job;
		GType type;

		plugin = iter->data;
		if (!brasero_plugin_get_active (plugin))
			continue;

		brasero_plugin_get_process_flags (plugin, &flags);
		if ((flags & position) != position)
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

		BRASERO_BURN_LOG ("%s (modifier) added to task",
				  brasero_plugin_get_name (plugin));

		BRASERO_BURN_LOG_TYPE (io_type, "IO type");

		brasero_task_add_item (task, BRASERO_TASK_ITEM (job));
	}
	g_slist_free (modifiers);

	return retval;
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
	gboolean res;

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
	last_caps = brasero_burn_caps_find_start_caps (self, &output);
	if (!last_caps)
		BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG_ERROR (session, error);

	brasero_burn_session_get_input_type (session, &input);
	BRASERO_BURN_LOG_WITH_TYPE (&input,
				    BRASERO_PLUGIN_IO_NONE,
				    "Input set =");

	session_flags = brasero_burn_session_get_flags (session);
	res = brasero_check_flags_for_drive (brasero_burn_session_get_burner (session), session_flags);
	if (!res)
		BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG (session);

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
		if (!(session_flags & BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE)
		||    brasero_burn_session_can_blank (session) != BRASERO_BURN_OK)
			BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG_ERROR (session, error);

		/* retry with the same disc type but blank this time */
		media &= ~(BRASERO_MEDIUM_CLOSED|
			   BRASERO_MEDIUM_APPENDABLE|
	   		   BRASERO_MEDIUM_UNFORMATTED|
			   BRASERO_MEDIUM_HAS_DATA|
			   BRASERO_MEDIUM_HAS_AUDIO);
		media |= BRASERO_MEDIUM_BLANK;

		output.subtype.media = media;

		last_caps = brasero_burn_caps_find_start_caps (self, &output);
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
	position = BRASERO_PLUGIN_RUN_PREPROCESSING;
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
				     _("An internal error occured"));
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

		position = BRASERO_PLUGIN_RUN_BEFORE_TARGET;

		/* the output of the plugin will become the input of the next */
		memcpy (&plugin_input, &plugin_output, sizeof (BraseroTrackType));
	}
	g_slist_free (list);

	/* add the post processing plugins */
	list = brasero_caps_add_processing_plugins_to_task (session,
							    NULL,
							    last_caps,
							    &output,
							    BRASERO_PLUGIN_RUN_AFTER_TARGET);
	retval = g_slist_concat (retval, list);

	if (last_caps->type.type == BRASERO_TRACK_TYPE_DISC && blanking) {
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
			     _("Only one track at a time can be checked"));
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
	brasero_track_get_track_type (track, &track_type);
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
