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
#include <glib/gi18n.h>

#include <gconf/gconf-client.h>

#include "burn-basics.h"
#include "burn-debug.h"
#include "brasero-track.h"
#include "brasero-plugin.h"
#include "brasero-plugin-information.h"
#include "burn-plugin-manager.h"

static BraseroPluginManager *default_manager = NULL;

#define BRASERO_PLUGIN_MANAGER_NOT_SUPPORTED_LOG(caps, error)			\
{										\
	BRASERO_BURN_LOG ("Unsupported operation");				\
	g_set_error (error,							\
		     BRASERO_BURN_ERROR,					\
		     BRASERO_BURN_ERROR_GENERAL,				\
		     _("An internal error occured"),				\
		     G_STRLOC);							\
	return BRASERO_BURN_NOT_SUPPORTED;					\
}

typedef struct _BraseroPluginManagerPrivate BraseroPluginManagerPrivate;
struct _BraseroPluginManagerPrivate {
	GSList *plugins;
	guint notification;
};

#define BRASERO_PLUGIN_MANAGER_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_PLUGIN_MANAGER, BraseroPluginManagerPrivate))

G_DEFINE_TYPE (BraseroPluginManager, brasero_plugin_manager, G_TYPE_OBJECT);

enum
{
	CAPS_CHANGED_SIGNAL,
	LAST_SIGNAL
};
static guint caps_signals [LAST_SIGNAL] = { 0 };

static void
brasero_plugin_manager_set_plugins_state (BraseroPluginManager *self);

static void
brasero_plugin_manager_plugin_state_changed (BraseroPlugin *plugin,
					     gboolean active,
					     BraseroPluginManager *self);

static GObjectClass* parent_class = NULL;

GSList *
brasero_plugin_manager_get_plugins_list (BraseroPluginManager *self)
{
	BraseroPluginManagerPrivate *priv;
	GSList *retval = NULL;
	GSList *iter;

	priv = BRASERO_PLUGIN_MANAGER_PRIVATE (self);

	for (iter = priv->plugins; iter; iter = iter->next) {
		BraseroPlugin *plugin;

		plugin = iter->data;
		retval = g_slist_prepend (retval, plugin);
	}

	return retval;
}

static void
brasero_plugin_manager_plugins_list_changed_cb (GConfClient *client,
						guint id,
						GConfEntry *entry,
						gpointer user_data)
{
	brasero_plugin_manager_set_plugins_state (BRASERO_PLUGIN_MANAGER (user_data));
}

static gint
brasero_plugin_strcmp (gconstpointer a, gconstpointer b)
{
	if (!a) {
		if (!b)
			return 0;

		return -1;
	}

	if (!b)
		return 1;

	return strcmp (a, b);
}

static void
brasero_plugin_manager_set_plugins_state (BraseroPluginManager *self)
{
	GSList *iter;
	GConfClient *client;
	GSList *names = NULL;
	GError *error = NULL;
	BraseroPluginManagerPrivate *priv;

	priv = BRASERO_PLUGIN_MANAGER_PRIVATE (self);

	/* get the list of user requested plugins. while at it we add a watch
	 * on the key so as to be warned whenever the user changes prefs. */
	client = gconf_client_get_default ();

	if (priv->notification) {
		gconf_client_notify_remove (client, priv->notification);
		priv->notification = 0;
	}

	BRASERO_BURN_LOG ("Getting list of plugins to be loaded");
	names = gconf_client_get_list (client,
				       BRASERO_PLUGIN_KEY,
				       GCONF_VALUE_STRING,
				       &error);
	
	if (error) {
		BRASERO_BURN_LOG ("Plugin list not set");

		/* couldn't get the key, maybe first launch so load everything
		 * in the plugin directory */
		g_error_free (error);
		error = NULL;
	}

	if (!names) {
		BRASERO_BURN_LOG ("Setting all plugins active");

		/* if names is NULL then accept all plugins */
		for (iter = priv->plugins; iter; iter = iter->next) {
			BraseroPlugin *plugin;

			plugin = iter->data;

			/* Skip plugins with a problem */
			if (brasero_plugin_get_gtype (plugin) == G_TYPE_NONE)
				continue;

			brasero_plugin_set_active (plugin, TRUE);
		}

		goto end;
	}

	for (iter = priv->plugins; iter; iter = iter->next) {
		GSList *node;
		BraseroPlugin *plugin;

		plugin = iter->data;

		/* Skip plugins with a problem */
		if (brasero_plugin_get_gtype (plugin) == G_TYPE_NONE)
			continue;

		if (brasero_plugin_get_compulsory (plugin)) {
			brasero_plugin_set_active (plugin, TRUE);
			BRASERO_BURN_LOG ("Setting plugin %s %s",
					  brasero_plugin_get_name (plugin),
					  brasero_plugin_get_active (plugin)? "active":"inactive");
			continue;
		}

		/* See if this plugin is in the names list. If not,
		 * de-activate it. */
		node = g_slist_find_custom (names,
					    brasero_plugin_get_name (plugin),
					    brasero_plugin_strcmp);

		/* we don't want to receive a signal from this plugin if its 
		 * active state changes */
		g_signal_handlers_block_matched (plugin,
						 G_SIGNAL_MATCH_FUNC,
						 0,
						 0,
						 0,
						 brasero_plugin_manager_plugin_state_changed,
						 NULL);

		brasero_plugin_set_active (plugin, node != NULL);

		g_signal_handlers_unblock_matched (plugin,
						   G_SIGNAL_MATCH_FUNC,
						   0,
						   0,
						   0,
						   brasero_plugin_manager_plugin_state_changed,
						   NULL);

		BRASERO_BURN_LOG ("Setting plugin %s %s",
				  brasero_plugin_get_name (plugin),
				  brasero_plugin_get_active (plugin)? "active":"inactive");
	}

	g_slist_foreach (names, (GFunc) g_free, NULL);
	g_slist_free (names);

end:

	BRASERO_BURN_LOG ("Watching GConf plugin key");
	priv->notification = gconf_client_notify_add (client,
						      BRASERO_PLUGIN_KEY,
						      brasero_plugin_manager_plugins_list_changed_cb,
						      self,
						      NULL,
						      NULL);
	g_object_unref (client);
}

static void
brasero_plugin_manager_plugin_state_changed (BraseroPlugin *plugin,
					     gboolean active,
					     BraseroPluginManager *self)
{
	BraseroPluginManagerPrivate *priv;
	GError *error = NULL;
	GConfClient *client;
	GSList *list = NULL;
	gboolean res;
	GSList *iter;

	priv = BRASERO_PLUGIN_MANAGER_PRIVATE (self);

	/* build a list of all active plugins */
	for (iter = priv->plugins; iter; iter = iter->next) {
		BraseroPlugin *plugin;
		const gchar *name;

		plugin = iter->data;

		if (brasero_plugin_get_gtype (plugin) == G_TYPE_NONE)
			continue;

		if (!brasero_plugin_get_active (plugin))
			continue;

		if (brasero_plugin_can_burn (plugin) == BRASERO_BURN_OK
		||  brasero_plugin_can_convert (plugin) == BRASERO_BURN_OK
		||  brasero_plugin_can_image (plugin) == BRASERO_BURN_OK)
			continue;

		name = brasero_plugin_get_name (plugin);
		if (name)
			list = g_slist_prepend (list, (gchar *) name);
	}

	client = gconf_client_get_default ();
	if (priv->notification) {
		gconf_client_notify_remove (client, priv->notification);
		priv->notification = 0;
	}

	if (list)
		res = gconf_client_set_list (client,
					     BRASERO_PLUGIN_KEY,
					     GCONF_VALUE_STRING,
					     list,
					     &error);
	else {
		gchar *none = "none";

		list = g_slist_prepend (list, none);
		res = gconf_client_set_list (client,
					     BRASERO_PLUGIN_KEY,
					     GCONF_VALUE_STRING,
					     list,
					     &error);
	}

	if (!res)
		BRASERO_BURN_LOG ("Error saving list of active plugins: %s",
				  error ? error->message:"no message");

	BRASERO_BURN_LOG ("Watching GConf plugin key");
	priv->notification = gconf_client_notify_add (client,
						      BRASERO_PLUGIN_KEY,
						      brasero_plugin_manager_plugins_list_changed_cb,
						      self,
						      NULL,
						      NULL);
	g_object_unref (client);
	g_slist_free (list);

	/* tell the rest of the world */
	g_signal_emit (self,
		       caps_signals [CAPS_CHANGED_SIGNAL],
		       0);
}

#if 0

/**
 * This function is only for debugging purpose. It allows to load plugins in a
 * particular order which is useful since sometimes it triggers some new bugs.
 */

static void
brasero_plugin_manager_init (BraseroPluginManager *self)
{
	guint i = 0;
	const gchar *name [] = {
				"libbrasero-transcode.so",
				"libbrasero-checksum.so",
				"libbrasero-dvdcss.so",
				"libbrasero-checksum-file.so",
				"libbrasero-local-track.so",
				"libbrasero-toc2cue.so",
				"libbrasero-wodim.so",
				"libbrasero-readom.so",
				"libbrasero-dvdrwformat.so",
				"libbrasero-genisoimage.so",
				"libbrasero-mkisofs.so",
				//"libbrasero-normalize.so",
				"libbrasero-cdrdao.so",
				//"libbrasero-readcd.so",
				//"libbrasero-cdrecord.so",
				"libbrasero-growisofs.so",
				//"libbrasero-libburn.so",
				//"libbrasero-libisofs.so",
				//"libbrasero-vcdimager.so",
				//"libbrasero-dvdauthor.so",
				//"libbrasero-vob.so"
				NULL};
	BraseroPluginManagerPrivate *priv;

	priv = BRASERO_PLUGIN_MANAGER_PRIVATE (self);

	/* open the plugin directory */
	BRASERO_BURN_LOG ("opening plugin directory %s", BRASERO_PLUGIN_DIRECTORY);

	/* load all plugins from directory */
	for (i = 0; name [i] != NULL; i++) {
		BraseroPluginRegisterType function;
		BraseroPlugin *plugin;
		GModule *handle;
		gchar *path;

		/* the name must end with *.so */
		if (!g_str_has_suffix (name [i], G_MODULE_SUFFIX))
			continue;

		path = g_module_build_path (BRASERO_PLUGIN_DIRECTORY, name [i]);
		BRASERO_BURN_LOG ("loading %s", path);

		handle = g_module_open (path, 0);
		if (!handle) {
			g_free (path);
			BRASERO_BURN_LOG ("Module can't be loaded: g_module_open failed (%s)",
					  g_module_error ());
			continue;
		}

		if (!g_module_symbol (handle, "brasero_plugin_register", (gpointer) &function)) {
			g_free (path);
			g_module_close (handle);
			BRASERO_BURN_LOG ("Module can't be loaded: no register function");
			continue;
		}

		/* now we can create the plugin */
		plugin = brasero_plugin_new (path);
		g_module_close (handle);
		g_free (path);

		if (!plugin) {
			BRASERO_BURN_LOG ("Load failure");
			continue;
		}

		if (brasero_plugin_get_gtype (plugin) == G_TYPE_NONE) {
			BRASERO_BURN_LOG ("Load failure, no GType was returned %s",
					  brasero_plugin_get_error (plugin));
		}
		else
			g_signal_connect (plugin,
					  "activated",
					  G_CALLBACK (brasero_plugin_manager_plugin_state_changed),
					  self);

		priv->plugins = g_slist_prepend (priv->plugins, plugin);
	}

	brasero_plugin_manager_set_plugins_state (self);
}

#endif

static void
brasero_plugin_manager_init (BraseroPluginManager *self)
{
	GDir *directory;
	const gchar *name;
	GError *error = NULL;
	BraseroPluginManagerPrivate *priv;

	priv = BRASERO_PLUGIN_MANAGER_PRIVATE (self);

	/* open the plugin directory */
	BRASERO_BURN_LOG ("opening plugin directory %s", BRASERO_PLUGIN_DIRECTORY);
	directory = g_dir_open (BRASERO_PLUGIN_DIRECTORY, 0, &error);
	if (!directory) {
		if (error) {
			BRASERO_BURN_LOG ("Error opening plugin directory %s", error->message);
			g_error_free (error);
			return;
		}
	}

	/* load all plugins from directory */
	while ((name = g_dir_read_name (directory))) {
		BraseroPluginRegisterType function;
		BraseroPlugin *plugin;
		GModule *handle;
		gchar *path;

		/* the name must end with *.so */
		if (!g_str_has_suffix (name, G_MODULE_SUFFIX))
			continue;

		path = g_module_build_path (BRASERO_PLUGIN_DIRECTORY, name);
		BRASERO_BURN_LOG ("loading %s", path);

		handle = g_module_open (path, 0);
		if (!handle) {
			g_free (path);
			BRASERO_BURN_LOG ("Module can't be loaded: g_module_open failed (%s)",
					  g_module_error ());
			continue;
		}

		if (!g_module_symbol (handle, "brasero_plugin_register", (gpointer) &function)) {
			g_free (path);
			g_module_close (handle);
			BRASERO_BURN_LOG ("Module can't be loaded: no register function");
			continue;
		}

		/* now we can create the plugin */
		plugin = brasero_plugin_new (path);
		g_module_close (handle);
		g_free (path);

		if (!plugin) {
			BRASERO_BURN_LOG ("Load failure");
			continue;
		}

		if (brasero_plugin_get_gtype (plugin) == G_TYPE_NONE) {
			BRASERO_BURN_LOG ("Load failure, no GType was returned %s",
					  brasero_plugin_get_error (plugin));
		}
		else
			g_signal_connect (plugin,
					  "activated",
					  G_CALLBACK (brasero_plugin_manager_plugin_state_changed),
					  self);

		g_assert (brasero_plugin_get_name (plugin));
		priv->plugins = g_slist_prepend (priv->plugins, plugin);
	}
	g_dir_close (directory);

	brasero_plugin_manager_set_plugins_state (self);
}

static void
brasero_plugin_manager_finalize (GObject *object)
{
	BraseroPluginManagerPrivate *priv;

	priv = BRASERO_PLUGIN_MANAGER_PRIVATE (object);

	if (priv->notification) {
		GConfClient *client;

		client = gconf_client_get_default ();
		gconf_client_notify_remove (client, priv->notification);
		priv->notification = 0;
	}

	if (priv->plugins) {
		g_slist_free (priv->plugins);
		priv->plugins = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
	default_manager = NULL;
}

static void
brasero_plugin_manager_class_init (BraseroPluginManagerClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	parent_class = G_OBJECT_CLASS (g_type_class_peek_parent (klass));

	g_type_class_add_private (klass, sizeof (BraseroPluginManagerPrivate));

	object_class->finalize = brasero_plugin_manager_finalize;

	caps_signals [CAPS_CHANGED_SIGNAL] =
		g_signal_new ("caps_changed",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
		              0,
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
}

BraseroPluginManager *
brasero_plugin_manager_get_default (void)
{
	if (!default_manager)
		default_manager = BRASERO_PLUGIN_MANAGER (g_object_new (BRASERO_TYPE_PLUGIN_MANAGER, NULL));

	return default_manager;
}
