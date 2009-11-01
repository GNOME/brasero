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

#ifndef _BURN_PLUGIN_H_
#define _BURN_PLUGIN_H_

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>

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
	void	(* activated)	(BraseroPlugin *plugin,
			                  gboolean active);
};

struct _BraseroPlugin {
	GTypeModule parent_instance;
};

GType brasero_plugin_get_type (void) G_GNUC_CONST;

GType
brasero_plugin_get_gtype (BraseroPlugin *self);

/**
 * Plugin configure options
 */

typedef struct _BraseroPluginConfOption BraseroPluginConfOption;
typedef enum {
	BRASERO_PLUGIN_OPTION_NONE	= 0,
	BRASERO_PLUGIN_OPTION_BOOL,
	BRASERO_PLUGIN_OPTION_INT,
	BRASERO_PLUGIN_OPTION_STRING,
	BRASERO_PLUGIN_OPTION_CHOICE
} BraseroPluginConfOptionType;

typedef enum {
	BRASERO_PLUGIN_RUN_NEVER		= 0,

	/* pre-process initial track */
	BRASERO_PLUGIN_RUN_PREPROCESSING	= 1,

	/* run before final image/disc is created */
	BRASERO_PLUGIN_RUN_BEFORE_TARGET	= 1 << 1,

	/* run after final image/disc is created: post-processing */
	BRASERO_PLUGIN_RUN_AFTER_TARGET		= 1 << 2,
} BraseroPluginProcessFlag;

G_END_DECLS

#endif /* _BURN_PLUGIN_H_ */
