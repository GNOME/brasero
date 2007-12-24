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

#ifndef _BURN_PLUGIN_H_
#define _BURN_PLUGIN_H_

#include <glib.h>
#include <glib-object.h>

#include "burn-medium.h"
#include "burn-track.h"

G_BEGIN_DECLS

#define BRASERO_PLUGIN_BURN_FLAG_MASK	(BRASERO_BURN_FLAG_DUMMY|		\
					 BRASERO_BURN_FLAG_MULTI|		\
					 BRASERO_BURN_FLAG_DAO|			\
					 BRASERO_BURN_FLAG_BURNPROOF|		\
					 BRASERO_BURN_FLAG_OVERBURN|		\
					 BRASERO_BURN_FLAG_NOGRACE|		\
					 BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE|	\
					 BRASERO_BURN_FLAG_APPEND|		\
					 BRASERO_BURN_FLAG_MERGE)

#define BRASERO_PLUGIN_IMAGE_FLAG_MASK	(BRASERO_BURN_FLAG_APPEND|		\
					 BRASERO_BURN_FLAG_MERGE)

#define BRASERO_PLUGIN_BLANK_FLAG_MASK	(BRASERO_BURN_FLAG_NOGRACE|		\
					 BRASERO_BURN_FLAG_FAST_BLANK)

#define BRASERO_TYPE_PLUGIN             (brasero_plugin_get_type ())
#define BRASERO_PLUGIN(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_PLUGIN, BraseroPlugin))
#define BRASERO_PLUGIN_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_PLUGIN, BraseroPluginClass))
#define BRASERO_IS_PLUGIN(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_PLUGIN))
#define BRASERO_IS_PLUGIN_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_PLUGIN))
#define BRASERO_PLUGIN_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_PLUGIN, BraseroPluginClass))

typedef struct _BraseroPluginClass BraseroPluginClass;
typedef struct _BraseroPlugin BraseroPlugin;

struct _BraseroPluginClass {
	GTypeModuleClass parent_class;

	/* Signals */
	void	(* loaded)	(BraseroPlugin *plugin);
	void	(* activated)	(BraseroPlugin *plugin);
};

struct _BraseroPlugin {
	GTypeModule parent_instance;
};

GType brasero_plugin_get_type (void) G_GNUC_CONST;

/**
 * These are the functions a plugin must implement
 */

GType brasero_plugin_register_caps (BraseroPlugin *plugin, gchar **error);

void
brasero_plugin_define (BraseroPlugin *plugin,
		       const gchar *name,
		       const gchar *description,
		       const gchar *author,
		       guint priority);
void
brasero_plugin_register_group (BraseroPlugin *plugin,
			       const gchar *name);

typedef enum {
	BRASERO_PLUGIN_IO_NONE			= 0,
	BRASERO_PLUGIN_IO_ACCEPT_PIPE		= 1,
	BRASERO_PLUGIN_IO_ACCEPT_FILE		= 1 << 1,
} BraseroPluginIOFlag;

typedef enum {
	BRASERO_PLUGIN_RUN_NEVER		= 0,
	BRASERO_PLUGIN_RUN_FIRST		= 1,
	BRASERO_PLUGIN_RUN_LAST			= 1 << 1,
} BraseroPluginProcessFlag;

GType
brasero_plugin_get_gtype (BraseroPlugin *plugin);

GSList *
brasero_caps_image_new (BraseroPluginIOFlag flags,
			BraseroImageFormat format);

GSList *
brasero_caps_audio_new (BraseroPluginIOFlag flags,
			BraseroAudioFormat format);

GSList *
brasero_caps_data_new (BraseroImageFS fs_type);

GSList *
brasero_caps_disc_new (BraseroMedia media);

GSList *
brasero_caps_checksum_new (BraseroChecksumType checksum);

void
brasero_plugin_link_caps (BraseroPlugin *plugin,
			  GSList *outputs,
			  GSList *inputs);

void
brasero_plugin_blank_caps (BraseroPlugin *plugin,
			   GSList *caps);

/**
 * This function is important since not only does it set the flags but it also 
 * tells brasero which types of media are supported. So even if a plugin doesn't
 * support any flag, use it to tell brasero which media are supported.
 * That's only needed if the plugin supports burn/blank operations.
 */
void
brasero_plugin_set_flags (BraseroPlugin *plugin,
			  BraseroMedia media,
			  BraseroBurnFlag supported,
			  BraseroBurnFlag compulsory);
void
brasero_plugin_set_blank_flags (BraseroPlugin *self,
				BraseroMedia media,
				BraseroBurnFlag supported,
				BraseroBurnFlag compulsory);

void
brasero_plugin_process_caps (BraseroPlugin *plugin,
			     GSList *caps);

void
brasero_plugin_set_process_flags (BraseroPlugin *plugin,
				  BraseroPluginProcessFlag flags);

void
brasero_plugin_check_caps (BraseroPlugin *plugin,
			   BraseroChecksumType type,
			   GSList *caps);

/**
 * Plugin configure options
 */

typedef struct _BraseroPluginConfOption BraseroPluginConfOption;

typedef enum {
	BRASERO_PLUGIN_OPTION_NONE	= 0,
	BRASERO_PLUGIN_OPTION_BOOL,
	BRASERO_PLUGIN_OPTION_INT,
	BRASERO_PLUGIN_OPTION_STRING,
} BraseroPluginConfOptionType;

BraseroPluginConfOption *
brasero_plugin_conf_option_new (const gchar *key,
				const gchar *description,
				BraseroPluginConfOptionType type);

BraseroBurnResult
brasero_plugin_add_conf_option (BraseroPlugin *plugin,
				BraseroPluginConfOption *option);

BraseroBurnResult
brasero_plugin_conf_option_bool_add_suboption (BraseroPluginConfOption *option,
					       BraseroPluginConfOption *suboption);

BraseroBurnResult
brasero_plugin_conf_option_int_set_range (BraseroPluginConfOption *option,
					  gint min,
					  gint max);

/**
 * Boiler plate for plugin definition to save the hassle of definition.
 * To be put at the beginning of the .c file.
 */
typedef GType	(* BraseroPluginRegisterType)	(BraseroPlugin *plugin, gchar **error);

#define BRASERO_PLUGIN_BOILERPLATE(PluginName, plugin_name, PARENT_NAME, ParentName) \
typedef struct {								\
	ParentName parent;							\
} PluginName;									\
										\
typedef struct {								\
	ParentName##Class parent_class;						\
} PluginName##Class;								\
										\
static GType plugin_name##_type = 0;						\
										\
static GType									\
plugin_name##_get_type (void)							\
{										\
	return plugin_name##_type;						\
}										\
										\
static void plugin_name##_class_init (PluginName##Class *klass);		\
static void plugin_name##_init (PluginName *sp);				\
static void plugin_name##_finalize (GObject *object);				\
static BraseroBurnResult plugin_name##_export_caps (BraseroPlugin *plugin, gchar **error);	\
										\
G_MODULE_EXPORT GType								\
brasero_plugin_register (BraseroPlugin *plugin, gchar **error)			\
{										\
	if (brasero_plugin_get_gtype (plugin) == G_TYPE_NONE) {			\
		BraseroBurnResult result;					\
		result = plugin_name##_export_caps (plugin, error);		\
		if (result != BRASERO_BURN_OK)					\
			return G_TYPE_NONE;					\
	}									\
	static const GTypeInfo our_info = {					\
		sizeof (PluginName##Class),					\
		NULL,								\
		NULL,								\
		(GClassInitFunc)plugin_name##_class_init,			\
		NULL,								\
		NULL,								\
		sizeof (PluginName),						\
		0,								\
		(GInstanceInitFunc)plugin_name##_init,				\
	};									\
	plugin_name##_type = g_type_module_register_type (G_TYPE_MODULE (plugin),		\
							  PARENT_NAME,		\
							  G_STRINGIFY (PluginName),		\
							  &our_info,		\
							  0);			\
	return plugin_name##_type;						\
}

G_END_DECLS

#endif /* _BURN_PLUGIN_H_ */
