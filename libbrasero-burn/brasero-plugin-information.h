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
 
#ifndef _BURN_PLUGIN_REGISTRATION_H
#define _BURN_PLUGIN_REGISTRATION_H

#include <glib.h>
#include <glib-object.h>

#include "brasero-session.h"
#include "brasero-plugin.h"

G_BEGIN_DECLS

void
brasero_plugin_set_active (BraseroPlugin *plugin, gboolean active);

gboolean
brasero_plugin_get_active (BraseroPlugin *plugin,
                           gboolean ignore_errors);

const gchar *
brasero_plugin_get_name (BraseroPlugin *plugin);

const gchar *
brasero_plugin_get_display_name (BraseroPlugin *plugin);

const gchar *
brasero_plugin_get_author (BraseroPlugin *plugin);

guint
brasero_plugin_get_group (BraseroPlugin *plugin);

const gchar *
brasero_plugin_get_copyright (BraseroPlugin *plugin);

const gchar *
brasero_plugin_get_website (BraseroPlugin *plugin);

const gchar *
brasero_plugin_get_description (BraseroPlugin *plugin);

const gchar *
brasero_plugin_get_icon_name (BraseroPlugin *plugin);

typedef struct _BraseroPluginError BraseroPluginError;
struct _BraseroPluginError {
	BraseroPluginErrorType type;
	gchar *detail;
};

GSList *
brasero_plugin_get_errors (BraseroPlugin *plugin);

gchar *
brasero_plugin_get_error_string (BraseroPlugin *plugin);

gboolean
brasero_plugin_get_compulsory (BraseroPlugin *plugin);

guint
brasero_plugin_get_priority (BraseroPlugin *plugin);

/** 
 * This is to find out what are the capacities of a plugin 
 */

BraseroBurnResult
brasero_plugin_can_burn (BraseroPlugin *plugin);

BraseroBurnResult
brasero_plugin_can_image (BraseroPlugin *plugin);

BraseroBurnResult
brasero_plugin_can_convert (BraseroPlugin *plugin);


/**
 * Plugin configuration options
 */

BraseroPluginConfOption *
brasero_plugin_get_next_conf_option (BraseroPlugin *plugin,
				     BraseroPluginConfOption *current);

BraseroBurnResult
brasero_plugin_conf_option_get_info (BraseroPluginConfOption *option,
				     gchar **key,
				     gchar **description,
				     BraseroPluginConfOptionType *type);

GSList *
brasero_plugin_conf_option_bool_get_suboptions (BraseroPluginConfOption *option);

gint
brasero_plugin_conf_option_int_get_min (BraseroPluginConfOption *option);
gint
brasero_plugin_conf_option_int_get_max (BraseroPluginConfOption *option);


struct _BraseroPluginChoicePair {
	gchar *string;
	guint value;
};
typedef struct _BraseroPluginChoicePair BraseroPluginChoicePair;

GSList *
brasero_plugin_conf_option_choice_get (BraseroPluginConfOption *option);

G_END_DECLS

#endif
 
