/*
 * brasero
 * Copyright (C) Philippe Rouquier 2007 <bonfire-app@wanadoo.fr>
 * 
 * brasero is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * brasero is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with brasero.  If not, write to:
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
#include "burn-track.h"
#include "burn-plugin.h"
#include "burn-plugin-private.h"
#include "burn-plugin-manager.h"

static BraseroPluginManager *default_manager = NULL;

#define BRASERO_PLUGIN_MANAGER_NOT_SUPPORTED_LOG(caps, error)			\
{										\
	g_set_error (error,							\
		     BRASERO_BURN_ERROR,					\
		     BRASERO_BURN_ERROR_GENERAL,				\
		     _("unsupported operation (at %s)"),			\
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

#define BRASERO_PLUGIN_DIRECTORY	BRASERO_LIBDIR "/brasero/plugins"

static void
brasero_plugin_manager_set_plugins_state (BraseroPluginManager *self);

static GObjectClass* parent_class = NULL;

GSList *
brasero_plugin_manager_get_plugins_list (BraseroPluginManager *self)
{
	BraseroPluginManagerPrivate *priv;
	GSList *retval = NULL;
	GSList *iter;

	priv = BRASERO_PLUGIN_MANAGER_PRIVATE (self);

	/* filter those with G_TYPE_NONE */
	for (iter = priv->plugins; iter; iter = iter->next) {
		BraseroPlugin *plugin;

		plugin = iter->data;
		if (brasero_plugin_get_gtype (plugin) != G_TYPE_NONE)
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
			brasero_plugin_set_active (plugin, TRUE);
		}

		goto end;
	}

	for (iter = priv->plugins; iter; iter = iter->next) {
		GSList *node;
		gchar *real_name;
		BraseroPlugin *plugin;

		plugin = iter->data;
		brasero_plugin_get_info (plugin, &real_name, NULL, NULL);

		/* See if this plugin is in the names list. If not,
		 * de-activate it. */
		node = g_slist_find_custom (names,
					    real_name,
					    (GCompareFunc) strcmp);

		BRASERO_BURN_LOG ("Settings plugin %s %s",
				  real_name,
				  node != NULL? "active":"inactive");

		brasero_plugin_set_active (plugin, node != NULL);

		g_free (real_name);
	}

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
		BraseroPlugin *plugin;
		gpointer function;
		GModule *handle;
		gchar *path;

		/* the name must end with *.so */
		if (!g_str_has_suffix (name, G_MODULE_SUFFIX))
			continue;

		BRASERO_BURN_LOG ("found %s", name);

		path = g_module_build_path (BRASERO_PLUGIN_DIRECTORY, name);
		BRASERO_BURN_LOG ("loading %s", path);

		handle = g_module_open (path, 0);
		if (!handle) {
			BRASERO_BURN_LOG ("Module can't be loaded: g_module_open failed");
			continue;
		}

		if (!g_module_symbol (handle, "brasero_plugin_register", &function)) {
			BRASERO_BURN_LOG ("Module can't be loaded: no register function");
			continue;
		}

		g_module_close (handle);

		plugin = brasero_plugin_new (path);
		g_free (path);

		if (!plugin) {
			BRASERO_BURN_LOG ("Load failure");
			continue;
		}

		if (brasero_plugin_get_gtype (plugin) == G_TYPE_NONE) {
			BRASERO_BURN_LOG ("Load failure, no GType was returned %s",
					  brasero_plugin_get_error (plugin));
		}

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
}

BraseroPluginManager *
brasero_plugin_manager_get_default (void)
{
	if (!default_manager)
		default_manager = BRASERO_PLUGIN_MANAGER (g_object_new (BRASERO_TYPE_PLUGIN_MANAGER, NULL));

	return default_manager;
}
