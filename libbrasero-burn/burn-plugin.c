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
#include <gmodule.h>
#include <glib/gi18n-lib.h>

#include <gst/gst.h>

#include <stdio.h>

#include "brasero-media-private.h"

#include "brasero-media.h"

#include "burn-basics.h"
#include "burn-debug.h"
#include "brasero-plugin.h"
#include "brasero-plugin-private.h"
#include "brasero-plugin-information.h"
#include "brasero-plugin-registration.h"
#include "burn-caps.h"

#define BRASERO_SCHEMA_PLUGINS				"org.gnome.brasero.plugins"
#define BRASERO_PROPS_PRIORITY_KEY			"priority"

typedef struct _BraseroPluginFlagPair BraseroPluginFlagPair;

struct _BraseroPluginFlagPair {
	BraseroPluginFlagPair *next;
	BraseroBurnFlag supported;
	BraseroBurnFlag compulsory;
};

struct _BraseroPluginFlags {
	BraseroMedia media;
	BraseroPluginFlagPair *pairs;
};
typedef struct _BraseroPluginFlags BraseroPluginFlags;

struct _BraseroPluginConfOption {
	gchar *key;
	gchar *description;
	BraseroPluginConfOptionType type;

	union {
		struct {
			guint max;
			guint min;
		} range;

		GSList *suboptions;

		GSList *choices;
	} specifics;
};

typedef struct _BraseroPluginPrivate BraseroPluginPrivate;
struct _BraseroPluginPrivate
{
	GSettings *settings;

	gboolean active;
	guint group;

	GSList *options;

	GSList *errors;

	GType type;
	gchar *path;
	GModule *handle;

	gchar *name;
	gchar *display_name;
	gchar *author;
	gchar *description;
	gchar *copyright;
	gchar *website;

	guint notify_priority;
	guint priority_original;
	gint priority;

	GSList *flags;
	GSList *blank_flags;

	BraseroPluginProcessFlag process_flags;

	guint compulsory:1;
};

static const gchar *default_icon = "gtk-cdrom";

#define BRASERO_PLUGIN_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_PLUGIN, BraseroPluginPrivate))
G_DEFINE_TYPE (BraseroPlugin, brasero_plugin, G_TYPE_TYPE_MODULE);

enum
{
	PROP_0,
	PROP_PATH,
	PROP_PRIORITY
};

enum
{
	LOADED_SIGNAL,
	ACTIVATED_SIGNAL,
	LAST_SIGNAL
};

static GTypeModuleClass* parent_class = NULL;
static guint plugin_signals [LAST_SIGNAL] = { 0 };

static void
brasero_plugin_error_free (BraseroPluginError *error)
{
	g_free (error->detail);
	g_free (error);
}

void
brasero_plugin_add_error (BraseroPlugin *plugin,
                          BraseroPluginErrorType type,
                          const gchar *detail)
{
	BraseroPluginError *error;
	BraseroPluginPrivate *priv;

	g_return_if_fail (BRASERO_IS_PLUGIN (plugin));

	priv = BRASERO_PLUGIN_PRIVATE (plugin);

	error = g_new0 (BraseroPluginError, 1);
	error->detail = g_strdup (detail);
	error->type = type;

	priv->errors = g_slist_prepend (priv->errors, error);
}

void
brasero_plugin_test_gstreamer_plugin (BraseroPlugin *plugin,
                                      const gchar *name)
{
	GstElement *element;

	/* Let's see if we've got the plugins we need */
	element = gst_element_factory_make (name, NULL);
	if (!element)
		brasero_plugin_add_error (plugin,
		                          BRASERO_PLUGIN_ERROR_MISSING_GSTREAMER_PLUGIN,
		                          name);
	else
		gst_object_unref (element);
}

static gboolean
is_old_app_version (guint major,
		    guint minor,
		    guint sub,
		    gint version [3])
{
    /* Compare minor versions only when major versions are
       equal. Compare micro versions only when major and minor
       versions are equal. */
    if (major < version [0] ||
	(version [1] >= 0 && major == version [0] && minor < version [1]) ||
	(version [2] >= 0 && major == version [0] && minor == version [1] && sub < version [2]))
	    return TRUE;

    return FALSE;
}


void
brasero_plugin_test_app (BraseroPlugin *plugin,
                         const gchar *name,
                         const gchar *version_arg,
                         const gchar *version_format,
                         gint version [3])
{
	gchar *standard_output = NULL;
	gchar *standard_error = NULL;
	guint major, minor, sub;
	gchar *prog_path;
	GPtrArray *argv;
	gboolean res, old_app_version;
	int i;

	/* First see if this plugin can be used, i.e. if cdrecord is in
	 * the path */
	prog_path = g_find_program_in_path (name);
	if (!prog_path) {
		brasero_plugin_add_error (plugin,
		                          BRASERO_PLUGIN_ERROR_MISSING_APP,
		                          name);
		return;
	}

	if (!g_file_test (prog_path, G_FILE_TEST_IS_EXECUTABLE)) {
		g_free (prog_path);
		brasero_plugin_add_error (plugin,
		                          BRASERO_PLUGIN_ERROR_MISSING_APP,
		                          name);
		return;
	}

	/* make sure that's not a symlink pointing to something with another
	 * name like wodim.
	 * NOTE: we used to test the target and see if it had the same name as
	 * the symlink with GIO. The problem is, when the symlink pointed to
	 * another symlink, then GIO didn't follow that other symlink. And in
	 * the end it didn't work. So forbid all symlink. */
	if (g_file_test (prog_path, G_FILE_TEST_IS_SYMLINK)) {
		brasero_plugin_add_error (plugin,
		                          BRASERO_PLUGIN_ERROR_SYMBOLIC_LINK_APP,
		                          name);
		g_free (prog_path);
		return;
	}
	/* Make sure it's a regular file */
	else if (!g_file_test (prog_path, G_FILE_TEST_IS_REGULAR)) {
		brasero_plugin_add_error (plugin,
		                          BRASERO_PLUGIN_ERROR_MISSING_APP,
		                          name);
		g_free (prog_path);
		return;
	}

	if (!version_arg) {
		g_free (prog_path);
		return;
	}

	/* Check version */
	argv = g_ptr_array_new ();
	g_ptr_array_add (argv, prog_path);
	g_ptr_array_add (argv, (gchar *) version_arg);
	g_ptr_array_add (argv, NULL);

	res = g_spawn_sync (NULL,
	                    (gchar **) argv->pdata,
	                    NULL,
	                    0,
	                    NULL,
	                    NULL,
	                    &standard_output,
	                    &standard_error,
	                    NULL,
	                    NULL);

	g_ptr_array_free (argv, TRUE);
	g_free (prog_path);

	if (!res) {
		brasero_plugin_add_error (plugin,
		                          BRASERO_PLUGIN_ERROR_WRONG_APP_VERSION,
		                          name);
		return;
	}

	for (i = 0; i < 3 && version [i] >= 0; i++);

	if ((standard_output && sscanf (standard_output, version_format, &major, &minor, &sub) == i)
	||  (standard_error && sscanf (standard_error, version_format, &major, &minor, &sub) == i))
		old_app_version = is_old_app_version (major, minor, sub, version);
	else
		old_app_version = TRUE;

	if (old_app_version)
		brasero_plugin_add_error (plugin,
		                          BRASERO_PLUGIN_ERROR_WRONG_APP_VERSION,
		                          name);

	g_free (standard_output);
	g_free (standard_error);
}

void
brasero_plugin_set_compulsory (BraseroPlugin *self,
			       gboolean compulsory)
{
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (self);
	priv->compulsory = compulsory;
}

gboolean
brasero_plugin_get_compulsory (BraseroPlugin *self)
{
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (self);
	return priv->compulsory;
}

void
brasero_plugin_set_active (BraseroPlugin *self, gboolean active)
{
	BraseroPluginPrivate *priv;
	gboolean was_active;
	gboolean now_active;

	priv = BRASERO_PLUGIN_PRIVATE (self);

	was_active = brasero_plugin_get_active (self, FALSE);
	priv->active = active;

	now_active = brasero_plugin_get_active (self, FALSE);
	if (was_active == now_active)
		return;

	BRASERO_BURN_LOG ("Plugin %s is %s",
			  brasero_plugin_get_name (self),
			  now_active?"active":"inactive");

	g_signal_emit (self,
		       plugin_signals [ACTIVATED_SIGNAL],
		       0,
		       now_active);
}

gboolean
brasero_plugin_get_active (BraseroPlugin *plugin,
                           gboolean ignore_errors)
{
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (plugin);

	if (priv->type == G_TYPE_NONE)
		return FALSE;

	if (priv->priority < 0)
		return FALSE;

	if (priv->errors) {
		if (!ignore_errors)
			return FALSE;
	}

	return priv->active;
}

static void
brasero_plugin_cleanup_definition (BraseroPlugin *self)
{
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (self);

	g_free (priv->name);
	priv->name = NULL;
	g_free (priv->author);
	priv->author = NULL;
	g_free (priv->description);
	priv->description = NULL;
	g_free (priv->copyright);
	priv->copyright = NULL;
	g_free (priv->website);
	priv->website = NULL;
}

/**
 * Plugin configure options
 */

static void
brasero_plugin_conf_option_choice_pair_free (BraseroPluginChoicePair *pair)
{
	g_free (pair->string);
	g_free (pair);
}

static void
brasero_plugin_conf_option_free (BraseroPluginConfOption *option)
{
	if (option->type == BRASERO_PLUGIN_OPTION_BOOL)
		g_slist_free (option->specifics.suboptions);

	if (option->type == BRASERO_PLUGIN_OPTION_CHOICE) {
		g_slist_foreach (option->specifics.choices, (GFunc) brasero_plugin_conf_option_choice_pair_free, NULL);
		g_slist_free (option->specifics.choices);
	}

	g_free (option->key);
	g_free (option->description);

	g_free (option);
}

BraseroPluginConfOption *
brasero_plugin_get_next_conf_option (BraseroPlugin *self,
				     BraseroPluginConfOption *current)
{
	BraseroPluginPrivate *priv;
	GSList *node;

	priv = BRASERO_PLUGIN_PRIVATE (self);
	if (!priv->options)
		return NULL;

	if (!current)
		return priv->options->data;

	node = g_slist_find (priv->options, current);
	if (!node)
		return NULL;

	if (!node->next)
		return NULL;

	return node->next->data;
}

BraseroBurnResult
brasero_plugin_conf_option_get_info (BraseroPluginConfOption *option,
				     gchar **key,
				     gchar **description,
				     BraseroPluginConfOptionType *type)
{
	g_return_val_if_fail (option != NULL, BRASERO_BURN_ERR);

	if (key)
		*key = g_strdup (option->key);

	if (description)
		*description = g_strdup (option->description);

	if (type)
		*type = option->type;

	return BRASERO_BURN_OK;
}

BraseroPluginConfOption *
brasero_plugin_conf_option_new (const gchar *key,
				const gchar *description,
				BraseroPluginConfOptionType type)
{
	BraseroPluginConfOption *option;

	g_return_val_if_fail (key != NULL, NULL);
	g_return_val_if_fail (description != NULL, NULL);
	g_return_val_if_fail (type != BRASERO_PLUGIN_OPTION_NONE, NULL);

	option = g_new0 (BraseroPluginConfOption, 1);
	option->key = g_strdup (key);
	option->description = g_strdup (description);
	option->type = type;

	return option;
}

BraseroBurnResult
brasero_plugin_add_conf_option (BraseroPlugin *self,
				BraseroPluginConfOption *option)
{
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (self);
	priv->options = g_slist_append (priv->options, option);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_plugin_conf_option_bool_add_suboption (BraseroPluginConfOption *option,
					       BraseroPluginConfOption *suboption)
{
	if (option->type != BRASERO_PLUGIN_OPTION_BOOL)
		return BRASERO_BURN_ERR;

	option->specifics.suboptions = g_slist_prepend (option->specifics.suboptions,
						        suboption);
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_plugin_conf_option_int_set_range (BraseroPluginConfOption *option,
					  gint min,
					  gint max)
{
	if (option->type != BRASERO_PLUGIN_OPTION_INT)
		return BRASERO_BURN_ERR;

	option->specifics.range.max = max;
	option->specifics.range.min = min;
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_plugin_conf_option_choice_add (BraseroPluginConfOption *option,
				       const gchar *string,
				       gint value)
{
	BraseroPluginChoicePair *pair;

	if (option->type != BRASERO_PLUGIN_OPTION_CHOICE)
		return BRASERO_BURN_ERR;

	pair = g_new0 (BraseroPluginChoicePair, 1);
	pair->value = value;
	pair->string = g_strdup (string);
	option->specifics.choices = g_slist_append (option->specifics.choices, pair);

	return BRASERO_BURN_OK;
}

GSList *
brasero_plugin_conf_option_bool_get_suboptions (BraseroPluginConfOption *option)
{
	if (option->type != BRASERO_PLUGIN_OPTION_BOOL)
		return NULL;
	return option->specifics.suboptions;
}

gint
brasero_plugin_conf_option_int_get_max (BraseroPluginConfOption *option) 
{
	if (option->type != BRASERO_PLUGIN_OPTION_INT)
		return -1;
	return option->specifics.range.max;
}

gint
brasero_plugin_conf_option_int_get_min (BraseroPluginConfOption *option)
{
	if (option->type != BRASERO_PLUGIN_OPTION_INT)
		return -1;
	return option->specifics.range.min;
}

GSList *
brasero_plugin_conf_option_choice_get (BraseroPluginConfOption *option)
{
	if (option->type != BRASERO_PLUGIN_OPTION_CHOICE)
		return NULL;
	return option->specifics.choices;
}

/**
 * Used to set the caps of plugin
 */

void
brasero_plugin_define (BraseroPlugin *self,
		       const gchar *name,
                       const gchar *display_name,
                       const gchar *description,
		       const gchar *author,
		       guint priority)
{
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (self);

	brasero_plugin_cleanup_definition (self);

	priv->name = g_strdup (name);
	priv->display_name = g_strdup (display_name);
	priv->author = g_strdup (author);
	priv->description = g_strdup (description);
	priv->priority_original = priority;
}

void
brasero_plugin_set_group (BraseroPlugin *self,
			  gint group_id)
{
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (self);
	priv->group = group_id;
}

guint
brasero_plugin_get_group (BraseroPlugin *self)
{
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (self);
	return priv->group;
}

/**
 * brasero_plugin_get_errors:
 * @plugin: a #BraseroPlugin.
 *
 * This function returns a list of all errors that
 * prevents the plugin from working properly.
 *
 * Returns: a #GSList of #BraseroPluginError structures or %NULL.
 * It must not be freed.
 **/

GSList *
brasero_plugin_get_errors (BraseroPlugin *plugin)
{
	BraseroPluginPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_PLUGIN (plugin), NULL);
	priv = BRASERO_PLUGIN_PRIVATE (plugin);
	return priv->errors;
}

gchar *
brasero_plugin_get_error_string (BraseroPlugin *plugin)
{
	gchar *error_string = NULL;
	BraseroPluginPrivate *priv;
	GString *string;
	GSList *iter;

	g_return_val_if_fail (BRASERO_IS_PLUGIN (plugin), NULL);

	priv = BRASERO_PLUGIN_PRIVATE (plugin);

	string = g_string_new (NULL);
	for (iter = priv->errors; iter; iter = iter->next) {
		BraseroPluginError *error;

		error = iter->data;
		switch (error->type) {
			case BRASERO_PLUGIN_ERROR_MISSING_APP:
				g_string_append_c (string, '\n');
				g_string_append_printf (string, _("\"%s\" could not be found in the path"), error->detail);
				break;
			case BRASERO_PLUGIN_ERROR_MISSING_GSTREAMER_PLUGIN:
				g_string_append_c (string, '\n');
				g_string_append_printf (string, _("\"%s\" GStreamer plugin could not be found"), error->detail);
				break;
			case BRASERO_PLUGIN_ERROR_WRONG_APP_VERSION:
				g_string_append_c (string, '\n');
				g_string_append_printf (string, _("The version of \"%s\" is too old"), error->detail);
				break;
			case BRASERO_PLUGIN_ERROR_SYMBOLIC_LINK_APP:
				g_string_append_c (string, '\n');
				g_string_append_printf (string, _("\"%s\" is a symbolic link pointing to another program"), error->detail);
				break;
			case BRASERO_PLUGIN_ERROR_MISSING_LIBRARY:
				g_string_append_c (string, '\n');
				g_string_append_printf (string, _("\"%s\" could not be found"), error->detail);
				break;
			case BRASERO_PLUGIN_ERROR_LIBRARY_VERSION:
				g_string_append_c (string, '\n');
				g_string_append_printf (string, _("The version of \"%s\" is too old"), error->detail);
				break;
			case BRASERO_PLUGIN_ERROR_MODULE:
				g_string_append_c (string, '\n');
				g_string_append (string, error->detail);
				break;

			default:
				break;
		}
	}

	error_string = string->str;
	g_string_free (string, FALSE);
	return error_string;
}

static BraseroPluginFlags *
brasero_plugin_get_flags (GSList *flags,
			  BraseroMedia media)
{
	GSList *iter;

	for (iter = flags; iter; iter = iter->next) {
		BraseroPluginFlags *flags;

		flags = iter->data;
		if ((media & flags->media) == media)
			return flags;
	}

	return NULL;
}

static GSList *
brasero_plugin_set_flags_real (GSList *flags_list,
			       BraseroMedia media,
			       BraseroBurnFlag supported,
			       BraseroBurnFlag compulsory)
{
	BraseroPluginFlags *flags;
	BraseroPluginFlagPair *pair;

	flags = brasero_plugin_get_flags (flags_list, media);
	if (!flags) {
		flags = g_new0 (BraseroPluginFlags, 1);
		flags->media = media;
		flags_list = g_slist_prepend (flags_list, flags);
	}
	else for (pair = flags->pairs; pair; pair = pair->next) {
		/* have a look at the BraseroPluginFlagPair to see if there
		 * is an exactly similar pair of flags or at least which
		 * encompasses it to avoid redundancy. */
		if ((pair->supported & supported) == supported
		&&  (pair->compulsory & compulsory) == compulsory)
			return flags_list;
	}

	pair = g_new0 (BraseroPluginFlagPair, 1);
	pair->supported = supported;
	pair->compulsory = compulsory;

	pair->next = flags->pairs;
	flags->pairs = pair;

	return flags_list;
}

void
brasero_plugin_set_flags (BraseroPlugin *self,
			  BraseroMedia media,
			  BraseroBurnFlag supported,
			  BraseroBurnFlag compulsory)
{
	BraseroPluginPrivate *priv;
	GSList *list;
	GSList *iter;

	priv = BRASERO_PLUGIN_PRIVATE (self);

	list = brasero_media_get_all_list (media);
	for (iter = list; iter; iter = iter->next) {
		BraseroMedia medium;

		medium = GPOINTER_TO_INT (iter->data);
		priv->flags = brasero_plugin_set_flags_real (priv->flags,
							     medium,
							     supported,
							     compulsory);
	}
	g_slist_free (list);
}

static gboolean
brasero_plugin_get_all_flags (GSList *flags_list,
			      gboolean check_compulsory,
			      BraseroMedia media,
			      BraseroBurnFlag mask,
			      BraseroBurnFlag current,
			      BraseroBurnFlag *supported_retval,
			      BraseroBurnFlag *compulsory_retval)
{
	gboolean found;
	BraseroPluginFlags *flags;
	BraseroPluginFlagPair *iter;
	BraseroBurnFlag supported = BRASERO_BURN_FLAG_NONE;
	BraseroBurnFlag compulsory = (BRASERO_BURN_FLAG_ALL & mask);

	flags = brasero_plugin_get_flags (flags_list, media);
	if (!flags) {
		if (supported_retval)
			*supported_retval = BRASERO_BURN_FLAG_NONE;
		if (compulsory_retval)
			*compulsory_retval = BRASERO_BURN_FLAG_NONE;
		return FALSE;
	}

	/* Find all sets of flags that support the current flags */
	found = FALSE;
	for (iter = flags->pairs; iter; iter = iter->next) {
		BraseroBurnFlag compulsory_masked;

		if ((current & iter->supported) != current)
			continue;

		compulsory_masked = (iter->compulsory & mask);
		if (check_compulsory
		&& (current & compulsory_masked) != compulsory_masked)
			continue;

		supported |= iter->supported;
		compulsory &= compulsory_masked;
		found = TRUE;
	}

	if (!found) {
		if (supported_retval)
			*supported_retval = BRASERO_BURN_FLAG_NONE;
		if (compulsory_retval)
			*compulsory_retval = BRASERO_BURN_FLAG_NONE;
		return FALSE;
	}

	if (supported_retval)
		*supported_retval = supported;
	if (compulsory_retval)
		*compulsory_retval = compulsory;

	return TRUE;
}

gboolean
brasero_plugin_check_record_flags (BraseroPlugin *self,
				   BraseroMedia media,
				   BraseroBurnFlag current)
{
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (self);
	current &= BRASERO_PLUGIN_BURN_FLAG_MASK;

	return brasero_plugin_get_all_flags (priv->flags,
					     TRUE,
					     media,
					     BRASERO_PLUGIN_BURN_FLAG_MASK,
					     current,
					     NULL,
					     NULL);
}

gboolean
brasero_plugin_check_image_flags (BraseroPlugin *self,
				  BraseroMedia media,
				  BraseroBurnFlag current)
{
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (self);

	current &= BRASERO_PLUGIN_IMAGE_FLAG_MASK;

	/* If there is no flag that's no use checking anything. If there is no
	 * flag we don't care about the media and therefore it's always possible
	 * NOTE: that's no the case for other operation like burn/blank. */
	if (current == BRASERO_BURN_FLAG_NONE)
		return TRUE;

	return brasero_plugin_get_all_flags (priv->flags,
					     TRUE,
					     media,
					     BRASERO_PLUGIN_IMAGE_FLAG_MASK,
					     current,
					     NULL,
					     NULL);
}

gboolean
brasero_plugin_check_media_restrictions (BraseroPlugin *self,
					 BraseroMedia media)
{
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (self);

	/* no restrictions */
	if (!priv->flags)
		return TRUE;

	return (brasero_plugin_get_flags (priv->flags, media) != NULL);
}

gboolean
brasero_plugin_get_record_flags (BraseroPlugin *self,
				 BraseroMedia media,
				 BraseroBurnFlag current,
				 BraseroBurnFlag *supported,
				 BraseroBurnFlag *compulsory)
{
	BraseroPluginPrivate *priv;
	gboolean result;

	priv = BRASERO_PLUGIN_PRIVATE (self);
	current &= BRASERO_PLUGIN_BURN_FLAG_MASK;

	result = brasero_plugin_get_all_flags (priv->flags,
					       FALSE,
					       media,
					       BRASERO_PLUGIN_BURN_FLAG_MASK,
					       current,
					       supported,
					       compulsory);
	if (!result)
		return FALSE;

	if (supported)
		*supported &= BRASERO_PLUGIN_BURN_FLAG_MASK;
	if (compulsory)
		*compulsory &= BRASERO_PLUGIN_BURN_FLAG_MASK;

	return TRUE;
}

gboolean
brasero_plugin_get_image_flags (BraseroPlugin *self,
				BraseroMedia media,
				BraseroBurnFlag current,
				BraseroBurnFlag *supported,
				BraseroBurnFlag *compulsory)
{
	BraseroPluginPrivate *priv;
	gboolean result;

	priv = BRASERO_PLUGIN_PRIVATE (self);
	current &= BRASERO_PLUGIN_IMAGE_FLAG_MASK;

	result = brasero_plugin_get_all_flags (priv->flags,
					       FALSE,
					       media,
					       BRASERO_PLUGIN_IMAGE_FLAG_MASK,
					       current,
					       supported,
					       compulsory);
	if (!result)
		return FALSE;

	if (supported)
		*supported &= BRASERO_PLUGIN_IMAGE_FLAG_MASK;
	if (compulsory)
		*compulsory &= BRASERO_PLUGIN_IMAGE_FLAG_MASK;

	return TRUE;
}

void
brasero_plugin_set_blank_flags (BraseroPlugin *self,
				BraseroMedia media,
				BraseroBurnFlag supported,
				BraseroBurnFlag compulsory)
{
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (self);
	priv->blank_flags = brasero_plugin_set_flags_real (priv->blank_flags,
							   media,
							   supported,
							   compulsory);
}

gboolean
brasero_plugin_check_blank_flags (BraseroPlugin *self,
				  BraseroMedia media,
				  BraseroBurnFlag current)
{
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (self);
	current &= BRASERO_PLUGIN_BLANK_FLAG_MASK;

	return brasero_plugin_get_all_flags (priv->blank_flags,
					     TRUE,
					     media,
					     BRASERO_PLUGIN_BLANK_FLAG_MASK,
					     current,
					     NULL,
					     NULL);
}

gboolean
brasero_plugin_get_blank_flags (BraseroPlugin *self,
				BraseroMedia media,
				BraseroBurnFlag current,
			        BraseroBurnFlag *supported,
			        BraseroBurnFlag *compulsory)
{
	BraseroPluginPrivate *priv;
	gboolean result;

	priv = BRASERO_PLUGIN_PRIVATE (self);
	current &= BRASERO_PLUGIN_BLANK_FLAG_MASK;

	result = brasero_plugin_get_all_flags (priv->blank_flags,
					       FALSE,
					       media,
					       BRASERO_PLUGIN_BLANK_FLAG_MASK,
					       current,
					       supported,
					       compulsory);
	if (!result)
		return FALSE;

	if (supported)
		*supported &= BRASERO_PLUGIN_BLANK_FLAG_MASK;
	if (compulsory)
		*compulsory &= BRASERO_PLUGIN_BLANK_FLAG_MASK;

	return TRUE;
}

void
brasero_plugin_set_process_flags (BraseroPlugin *plugin,
				  BraseroPluginProcessFlag flags)
{
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (plugin);
	priv->process_flags = flags;
}

gboolean
brasero_plugin_get_process_flags (BraseroPlugin *plugin,
				  BraseroPluginProcessFlag *flags)
{
	BraseroPluginPrivate *priv;

	g_return_val_if_fail (flags != NULL, FALSE);

	priv = BRASERO_PLUGIN_PRIVATE (plugin);
	*flags = priv->process_flags;
	return TRUE;
}

const gchar *
brasero_plugin_get_name (BraseroPlugin *plugin)
{
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (plugin);
	return priv->name;
}

const gchar *
brasero_plugin_get_display_name (BraseroPlugin *plugin)
{
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (plugin);
	return priv->display_name ? priv->display_name:priv->name;

}

const gchar *
brasero_plugin_get_author (BraseroPlugin *plugin)
{
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (plugin);
	return priv->author;
}

const gchar *
brasero_plugin_get_copyright (BraseroPlugin *plugin)
{
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (plugin);
	return priv->copyright;
}

const gchar *
brasero_plugin_get_website (BraseroPlugin *plugin)
{
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (plugin);
	return priv->website;
}

const gchar *
brasero_plugin_get_description (BraseroPlugin *plugin)
{
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (plugin);
	return priv->description;
}

const gchar *
brasero_plugin_get_icon_name (BraseroPlugin *plugin)
{
	return default_icon;
}

guint
brasero_plugin_get_priority (BraseroPlugin *self)
{
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (self);

	if (priv->priority > 0)
		return priv->priority;

	return priv->priority_original;
}

GType
brasero_plugin_get_gtype (BraseroPlugin *self)
{
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (self);

	if (priv->errors)
		return G_TYPE_NONE;

	return priv->type;
}

/**
 * Function to initialize and load
 */

static void
brasero_plugin_unload (GTypeModule *module)
{
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (module);
	if (!priv->handle)
		return;

	g_module_close (priv->handle);
	priv->handle = NULL;
}

static gboolean
brasero_plugin_load_real (BraseroPlugin *plugin) 
{
	BraseroPluginPrivate *priv;
	BraseroPluginRegisterType register_func;

	priv = BRASERO_PLUGIN_PRIVATE (plugin);

	if (!priv->path)
		return FALSE;

	if (priv->handle)
		return TRUE;

	priv->handle = g_module_open (priv->path, G_MODULE_BIND_LAZY);
	if (!priv->handle) {
		brasero_plugin_add_error (plugin, BRASERO_PLUGIN_ERROR_MODULE, g_module_error ());
		BRASERO_BURN_LOG ("Module %s can't be loaded: g_module_open failed ()", priv->name);
		return FALSE;
	}

	if (!g_module_symbol (priv->handle, "brasero_plugin_register", (gpointer) &register_func)) {
		BRASERO_BURN_LOG ("it doesn't appear to be a valid brasero plugin");
		brasero_plugin_unload (G_TYPE_MODULE (plugin));
		return FALSE;
	}

	priv->type = register_func (plugin);
	brasero_burn_debug_setup_module (priv->handle);
	return TRUE;
}

static gboolean
brasero_plugin_load (GTypeModule *module) 
{
	if (!brasero_plugin_load_real (BRASERO_PLUGIN (module)))
		return FALSE;

	g_signal_emit (BRASERO_PLUGIN (module),
		       plugin_signals [LOADED_SIGNAL],
		       0);
	return TRUE;
}

static void
brasero_plugin_priority_changed (GSettings *settings,
                                 const gchar *key,
                                 BraseroPlugin *self)
{
	BraseroPluginPrivate *priv;
	gboolean is_active;

	priv = BRASERO_PLUGIN_PRIVATE (self);

	/* At the moment it can only be the priority key */
	priv->priority = g_settings_get_int (settings, BRASERO_PROPS_PRIORITY_KEY);

	is_active = brasero_plugin_get_active (self, FALSE);

	g_object_notify (G_OBJECT (self), "priority");
	if (is_active != brasero_plugin_get_active (self, FALSE))
		g_signal_emit (self,
			       plugin_signals [ACTIVATED_SIGNAL],
			       0,
			       is_active);
}

typedef void	(* BraseroPluginCheckConfig)	(BraseroPlugin *plugin);

/**
 * brasero_plugin_check_plugin_ready:
 * @plugin: a #BraseroPlugin.
 *
 * Ask a plugin to check whether it can operate.
 * brasero_plugin_can_operate () should be called
 * afterwards to know whether it can operate or not.
 *
 **/
void
brasero_plugin_check_plugin_ready (BraseroPlugin *plugin)
{
	GModule *handle;
	BraseroPluginPrivate *priv;
	BraseroPluginCheckConfig function = NULL;

	g_return_if_fail (BRASERO_IS_PLUGIN (plugin));
	priv = BRASERO_PLUGIN_PRIVATE (plugin);

	if (priv->errors) {
		g_slist_foreach (priv->errors, (GFunc) brasero_plugin_error_free, NULL);
		g_slist_free (priv->errors);
		priv->errors = NULL;
	}

	handle = g_module_open (priv->path, 0);
	if (!handle) {
		brasero_plugin_add_error (plugin, BRASERO_PLUGIN_ERROR_MODULE, g_module_error ());
		BRASERO_BURN_LOG ("Module %s can't be loaded: g_module_open failed ()", priv->name);
		return;
	}

	if (!g_module_symbol (handle, "brasero_plugin_check_config", (gpointer) &function)) {
		g_module_close (handle);
		BRASERO_BURN_LOG ("Module %s has no check config function", priv->name);
		return;
	}

	function (BRASERO_PLUGIN (plugin));
	g_module_close (handle);
}

static void
brasero_plugin_init_real (BraseroPlugin *object)
{
	GModule *handle;
	gchar *settings_path;
	BraseroPluginPrivate *priv;
	BraseroPluginRegisterType function = NULL;

	priv = BRASERO_PLUGIN_PRIVATE (object);

	g_type_module_set_name (G_TYPE_MODULE (object), priv->name);

	handle = g_module_open (priv->path, 0);
	if (!handle) {
		brasero_plugin_add_error (object, BRASERO_PLUGIN_ERROR_MODULE, g_module_error ());
		BRASERO_BURN_LOG ("Module %s (at %s) can't be loaded: g_module_open failed ()", priv->name, priv->path);
		return;
	}

	if (!g_module_symbol (handle, "brasero_plugin_register", (gpointer) &function)) {
		g_module_close (handle);
		BRASERO_BURN_LOG ("Module %s can't be loaded: no register function, priv->name", priv->name);
		return;
	}

	priv->type = function (object);
	if (priv->type == G_TYPE_NONE) {
		g_module_close (handle);
		BRASERO_BURN_LOG ("Module %s encountered an error while registering its capabilities", priv->name);
		return;
	}

	/* now see if we need to override the hardcoded priority of the plugin */
	settings_path = g_strconcat ("/org/gnome/brasero/plugins/",
	                             priv->name,
	                             G_DIR_SEPARATOR_S,
	                             NULL);
	priv->settings = g_settings_new_with_path (BRASERO_SCHEMA_PLUGINS,
	                                           settings_path);
	g_free (settings_path);

	priv->priority = g_settings_get_int (priv->settings, BRASERO_PROPS_PRIORITY_KEY);

	/* Make sure a key is created for each plugin */
	if (!priv->priority)
		g_settings_set_int (priv->settings, BRASERO_PROPS_PRIORITY_KEY, 0);

	g_signal_connect (priv->settings,
	                  "changed",
	                  G_CALLBACK (brasero_plugin_priority_changed),
	                  object);

	/* Check if it can operate */
	brasero_plugin_check_plugin_ready (object);

	g_module_close (handle);
}

static void
brasero_plugin_finalize (GObject *object)
{
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (object);

	if (priv->options) {
		g_slist_foreach (priv->options, (GFunc) brasero_plugin_conf_option_free, NULL);
		g_slist_free (priv->options);
		priv->options = NULL;
	}

	if (priv->handle) {
		brasero_plugin_unload (G_TYPE_MODULE (object));
		priv->handle = NULL;
	}

	if (priv->path) {
		g_free (priv->path);
		priv->path = NULL;
	}

	g_free (priv->name);
	g_free (priv->display_name);
	g_free (priv->author);
	g_free (priv->description);

	g_slist_foreach (priv->flags, (GFunc) g_free, NULL);
	g_slist_free (priv->flags);

	g_slist_foreach (priv->blank_flags, (GFunc) g_free, NULL);
	g_slist_free (priv->blank_flags);

	if (priv->settings) {
		g_object_unref (priv->settings);
		priv->settings = NULL;
	}

	if (priv->errors) {
		g_slist_foreach (priv->errors, (GFunc) brasero_plugin_error_free, NULL);
		g_slist_free (priv->errors);
		priv->errors = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_plugin_set_property (GObject *object,
			     guint prop_id,
			     const GValue *value,
			     GParamSpec *pspec)
{
	BraseroPlugin *self;
	BraseroPluginPrivate *priv;

	g_return_if_fail (BRASERO_IS_PLUGIN (object));

	self = BRASERO_PLUGIN (object);
	priv = BRASERO_PLUGIN_PRIVATE (self);

	switch (prop_id)
	{
	case PROP_PATH:
		/* NOTE: this property can only be set once */
		priv->path = g_strdup (g_value_get_string (value));
		brasero_plugin_init_real (self);
		break;
	case PROP_PRIORITY:
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
brasero_plugin_get_property (GObject *object,
			     guint prop_id,
			     GValue *value,
			     GParamSpec *pspec)
{
	BraseroPlugin *self;
	BraseroPluginPrivate *priv;

	g_return_if_fail (BRASERO_IS_PLUGIN (object));

	self = BRASERO_PLUGIN (object);
	priv = BRASERO_PLUGIN_PRIVATE (self);

	switch (prop_id)
	{
	case PROP_PATH:
		g_value_set_string (value, priv->path);
		break;
	case PROP_PRIORITY:
		g_value_set_int (value, priv->priority);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
brasero_plugin_init (BraseroPlugin *object)
{
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (object);
	priv->type = G_TYPE_NONE;
	priv->compulsory = TRUE;
}

static void
brasero_plugin_class_init (BraseroPluginClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	GTypeModuleClass *module_class = G_TYPE_MODULE_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroPluginPrivate));

	object_class->finalize = brasero_plugin_finalize;
	object_class->set_property = brasero_plugin_set_property;
	object_class->get_property = brasero_plugin_get_property;

	module_class->load = brasero_plugin_load;
	module_class->unload = brasero_plugin_unload;

	g_object_class_install_property (object_class,
	                                 PROP_PATH,
	                                 g_param_spec_string ("path",
	                                                      "Path",
	                                                      "Path for the module",
	                                                      NULL,
	                                                      G_PARAM_STATIC_NAME|G_PARAM_READABLE|G_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class,
	                                 PROP_PRIORITY,
	                                 g_param_spec_int ("priority",
							   "Priority",
							   "Priority of the module",
							   1,
							   G_MAXINT,
							   1,
							   G_PARAM_STATIC_NAME|G_PARAM_READABLE));

	plugin_signals [LOADED_SIGNAL] =
		g_signal_new ("loaded",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
		              G_STRUCT_OFFSET (BraseroPluginClass, loaded),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);

	plugin_signals [ACTIVATED_SIGNAL] =
		g_signal_new ("activated",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
		              G_STRUCT_OFFSET (BraseroPluginClass, activated),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__BOOLEAN,
		              G_TYPE_NONE, 1,
			      G_TYPE_BOOLEAN);
}

BraseroPlugin *
brasero_plugin_new (const gchar *path)
{
	if (!path)
		return NULL;

	return BRASERO_PLUGIN (g_object_new (BRASERO_TYPE_PLUGIN,
					     "path", path,
					     NULL));
}
