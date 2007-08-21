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

GType	brasero_plugin_register		(BraseroPlugin *plugin, gchar **error);
void	brasero_plugin_cleanup		(BraseroPlugin *plugin);

void
brasero_plugin_define (BraseroPlugin *plugin,
		       const gchar *name,
		       const gchar *description,
		       const gchar *author,
		       guint priority);

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
 * Boiler plate for plugin definition to save the hassle of definition.
 * To be put at the beginning of the .c file.
 */

#define BRASERO_PLUGIN_BOILERPLATE(TN, t_n, TP, t_p)				\
typedef struct {								\
	t_p parent;								\
} TN;										\
typedef struct {								\
	t_p##Class parent_class;						\
} TN##Class;									\
static void t_n##_class_init (TN##Class *klass);				\
static void t_n##_init (TN *sp);						\
static void t_n##_finalize (GObject *object);					\
static GType									\
t_n##_get_type (BraseroPlugin *plugin)						\
{										\
	static GType type = 0;							\
	if(type == 0) {								\
		static const GTypeInfo our_info = {				\
			sizeof (TN##Class),					\
			NULL,							\
			NULL,							\
			(GClassInitFunc)t_n##_class_init,			\
			NULL,							\
			NULL,							\
			sizeof (TN),						\
			0,							\
			(GInstanceInitFunc)t_n##_init,				\
		};								\
		type = g_type_module_register_type (G_TYPE_MODULE (plugin),	\
						    TP,				\
						    G_STRINGIFY (TN),		\
						    &our_info,			\
						    0);				\
	}									\
	return type;								\
}

G_END_DECLS

#endif /* _BURN_PLUGIN_H_ */
