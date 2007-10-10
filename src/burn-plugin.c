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

#include "burn-basics.h"
#include "burn-debug.h"
#include "burn-plugin.h"
#include "burn-plugin-private.h"

struct _BraseroPluginFlags {
	BraseroMedia media;
	BraseroBurnFlag supported;
	BraseroBurnFlag compulsory;
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
	} specifics;
};

typedef struct _BraseroPluginPrivate BraseroPluginPrivate;
struct _BraseroPluginPrivate
{
	gboolean active;

	GSList *options;

	gchar *error;

	GType type;
	gchar *path;
	GModule *handle;

	gchar *name;
	gchar *author;
	gchar *description;
	gchar *copyright;
	gchar *website;

	guint priority;

	GSList *flags;
	GSList *blank_flags;

	BraseroPluginProcessFlag process_flags;
};

static const gchar *default_icon = "gtk-cdrom";

#define BRASERO_PLUGIN_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_PLUGIN, BraseroPluginPrivate))
G_DEFINE_TYPE (BraseroPlugin, brasero_plugin, G_TYPE_TYPE_MODULE);

enum
{
	PROP_0,
	PROP_PATH
};

enum
{
	LOADED_SIGNAL,
	ACTIVATED_SIGNAL,
	LAST_SIGNAL
};

static GTypeModuleClass* parent_class = NULL;
static guint plugin_signals [LAST_SIGNAL] = { 0 };

void
brasero_plugin_set_active (BraseroPlugin *self, gboolean active)
{
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (self);

	if (priv->type == G_TYPE_NONE)
		return;

	if (priv->active == active)
		return;

	priv->active = active;
	g_signal_emit (self,
		       plugin_signals [ACTIVATED_SIGNAL],
		       0,
		       active);
}

gboolean
brasero_plugin_get_active (BraseroPlugin *self)
{
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (self);

	if (priv->type == G_TYPE_NONE)
		return FALSE;

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
	if (priv->error) {
		g_free (priv->error);
		priv->error = NULL;
	}
}

/**
 * Plugin configure options
 */

static void
brasero_plugin_conf_option_free (BraseroPluginConfOption *option)
{
	if (option->type == BRASERO_PLUGIN_OPTION_BOOL)
		g_slist_free (option->specifics.suboptions);

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

/**
 * Used to set the caps of plugin
 */

void
brasero_plugin_define (BraseroPlugin *self,
		       const gchar *name,
		       const gchar *description,
		       const gchar *author,
		       guint priority)
{
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (self);

	brasero_plugin_cleanup_definition (self);

	priv->name = g_strdup (name);
	priv->author = g_strdup (author);
	priv->description = g_strdup (description);
	priv->priority = priority;
}

const gchar *
brasero_plugin_get_error (BraseroPlugin *self)
{
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (self);
	return priv->error;
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

	flags = brasero_plugin_get_flags (flags_list, media);
	if (flags) {
		flags->supported = supported;
		flags->compulsory = compulsory;
		return flags_list;
	};

	flags = g_new0 (BraseroPluginFlags, 1);
	flags->media = media;
	flags->supported = supported;
	flags->compulsory = compulsory;

	flags_list = g_slist_prepend (flags_list, flags);
	return flags_list;
}

void
brasero_plugin_set_flags (BraseroPlugin *self,
			  BraseroMedia media,
			  BraseroBurnFlag supported,
			  BraseroBurnFlag compulsory)
{
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (self);
	priv->flags = brasero_plugin_set_flags_real (priv->flags,
						     media,
						     supported,
						     compulsory);
}

gboolean
brasero_plugin_get_record_flags (BraseroPlugin *self,
				 BraseroMedia media,
				 BraseroBurnFlag *supported,
				 BraseroBurnFlag *compulsory)
{
	BraseroPluginFlags *flags;
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (self);
	flags = brasero_plugin_get_flags (priv->flags, media);
	if (!flags) {
		if (supported)
			*supported = BRASERO_BURN_FLAG_NONE;
		if (compulsory)
			*compulsory = BRASERO_BURN_FLAG_NONE;
		return FALSE;
	}

	if (supported)
		*supported = flags->supported & (BRASERO_BURN_FLAG_DUMMY|
						 BRASERO_BURN_FLAG_MULTI|
						 BRASERO_BURN_FLAG_DAO|
						 BRASERO_BURN_FLAG_BURNPROOF|
						 BRASERO_BURN_FLAG_OVERBURN|
						 BRASERO_BURN_FLAG_NOGRACE);
	if (compulsory)
		*compulsory = flags->compulsory & (BRASERO_BURN_FLAG_DUMMY|
						   BRASERO_BURN_FLAG_MULTI|
						   BRASERO_BURN_FLAG_DAO|
						   BRASERO_BURN_FLAG_BURNPROOF|
						   BRASERO_BURN_FLAG_OVERBURN|
						   BRASERO_BURN_FLAG_NOGRACE);
	return TRUE;
}

gboolean
brasero_plugin_get_image_flags (BraseroPlugin *self,
				BraseroMedia media,
				BraseroBurnFlag *supported,
				BraseroBurnFlag *compulsory)
{
	BraseroPluginFlags *flags;
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (self);
	flags = brasero_plugin_get_flags (priv->flags, media);

	if (!flags) {
		if (supported)
			*supported = BRASERO_BURN_FLAG_NONE;
		if (compulsory)
			*compulsory = BRASERO_BURN_FLAG_NONE;
		return FALSE;
	}

	if (supported)
		*supported = flags->supported & (BRASERO_BURN_FLAG_APPEND|
						 BRASERO_BURN_FLAG_MERGE);
	if (compulsory)
		*compulsory = flags->compulsory & (BRASERO_BURN_FLAG_APPEND|
						   BRASERO_BURN_FLAG_MERGE);
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
brasero_plugin_get_blank_flags (BraseroPlugin *self,
				BraseroMedia media,
			        BraseroBurnFlag *supported,
			        BraseroBurnFlag *compulsory)
{
	BraseroPluginFlags *flags;
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (self);
	flags = brasero_plugin_get_flags (priv->blank_flags, media);

	if (!flags) {
		if (supported)
			*supported = BRASERO_BURN_FLAG_NONE;
		if (compulsory)
			*compulsory = BRASERO_BURN_FLAG_NONE;
		return FALSE;
	}

	if (supported)
		*supported = flags->supported & (BRASERO_BURN_FLAG_NOGRACE|
						 BRASERO_BURN_FLAG_FAST_BLANK);
	if (compulsory)
		*compulsory = flags->compulsory & (BRASERO_BURN_FLAG_NOGRACE|
						   BRASERO_BURN_FLAG_FAST_BLANK);
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
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (plugin);
	return default_icon;
}

guint
brasero_plugin_get_priority (BraseroPlugin *self)
{
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (self);
	return priv->priority;
}

GType
brasero_plugin_get_gtype (BraseroPlugin *self)
{
	BraseroPluginPrivate *priv;

	priv = BRASERO_PLUGIN_PRIVATE (self);
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
	gchar *error = NULL;
	BraseroPluginPrivate *priv;
	BraseroPluginRegisterType register_func;

	priv = BRASERO_PLUGIN_PRIVATE (plugin);

	if (!priv->path)
		return FALSE;

	if (priv->handle)
		return TRUE;

	priv->handle = g_module_open (priv->path, G_MODULE_BIND_LAZY);
	if (!priv->handle) {
		priv->error = g_strdup (g_module_error ());
		return FALSE;
	}

	if (!g_module_symbol (priv->handle, "brasero_plugin_register", (gpointer) &register_func)) {
		BRASERO_BURN_LOG ("it doesn't appear to be a valid brasero plugin");
		brasero_plugin_unload (G_TYPE_MODULE (plugin));
		return FALSE;
	}

	priv->type = register_func (plugin, &error);
	if (error) {
		if (priv->error)
			g_free (priv->error);
		priv->error = error;
	}

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
brasero_plugin_init_real (BraseroPlugin *object)
{
	GModule *handle;
	BraseroPluginPrivate *priv;
	BraseroPluginRegisterType function;

	priv = BRASERO_PLUGIN_PRIVATE (object);

	g_type_module_set_name (G_TYPE_MODULE (object), priv->name);

	handle = g_module_open (priv->name, 0);
	if (!handle) {
		BRASERO_BURN_LOG ("Module can't be loaded: g_module_open failed");
		return;
	}

	if (!g_module_symbol (handle, "brasero_plugin_register", (gpointer) &function)) {
		g_module_close (handle);
		BRASERO_BURN_LOG ("Module can't be loaded: no register function");
		return;
	}

	priv->type = function (BRASERO_PLUGIN (object), &priv->error);
	if (priv->type == G_TYPE_NONE) {
		g_module_close (handle);
		BRASERO_BURN_LOG ("Module encountered an error while registering its capabilities:\n%s",
				  priv->error ? priv->error:"unknown error");
		return;
	}

	BRASERO_BURN_LOG ("Module %s successfully loaded", priv->name);
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
	g_free (priv->author);
	g_free (priv->description);

	g_slist_foreach (priv->flags, (GFunc) g_free, NULL);
	g_slist_free (priv->flags);

	g_slist_foreach (priv->blank_flags, (GFunc) g_free, NULL);
	g_slist_free (priv->blank_flags);

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

	parent_class = G_TYPE_MODULE_CLASS (g_type_class_peek_parent (klass));
	module_class->load = brasero_plugin_load;
	module_class->unload = brasero_plugin_unload;

	g_object_class_install_property (object_class,
	                                 PROP_PATH,
	                                 g_param_spec_string ("path",
	                                                      "Path",
	                                                      "Path for the module",
	                                                      NULL,
	                                                      G_PARAM_STATIC_NAME|G_PARAM_READABLE|G_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY));

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
