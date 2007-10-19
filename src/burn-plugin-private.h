/***************************************************************************
 *            burn-plugin-private.h
 *
 *  Mon Feb 12 10:40:55 2007
 *  Copyright  2007  algernon
 *  <algernon@localhost.localdomain>
 ****************************************************************************/

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */
 
#ifndef _BURN_PLUGIN_PRIVATE_H
#define _BURN_PLUGIN_PRIVATE_H

#include <glib.h>
#include <glib-object.h>

#include "burn-basics.h"
#include "burn-session.h"
#include "burn-plugin.h"

G_BEGIN_DECLS

#define BRASERO_PLUGIN_PRIORITY_KEY			"/apps/brasero/config"

BraseroPlugin *
brasero_plugin_new (const gchar *path);

void
brasero_plugin_set_active (BraseroPlugin *plugin, gboolean active);

gboolean
brasero_plugin_get_active (BraseroPlugin *plugin);

const gchar *
brasero_plugin_get_name (BraseroPlugin *plugin);

const gchar *
brasero_plugin_get_author (BraseroPlugin *plugin);

const gchar *
brasero_plugin_get_copyright (BraseroPlugin *plugin);

const gchar *
brasero_plugin_get_website (BraseroPlugin *plugin);

const gchar *
brasero_plugin_get_description (BraseroPlugin *plugin);

const gchar *
brasero_plugin_get_icon_name (BraseroPlugin *plugin);

const gchar *
brasero_plugin_get_error (BraseroPlugin *self);

guint
brasero_plugin_get_priority (BraseroPlugin *plugin);

gboolean
brasero_plugin_get_image_flags (BraseroPlugin *plugin,
			        BraseroMedia media,
			        BraseroBurnFlag *supported,
			        BraseroBurnFlag *compulsory);
gboolean
brasero_plugin_get_blank_flags (BraseroPlugin *plugin,
				BraseroMedia media,
				BraseroBurnFlag *supported,
				BraseroBurnFlag *compulsory);
gboolean
brasero_plugin_get_record_flags (BraseroPlugin *plugin,
				 BraseroMedia media,
				 BraseroBurnFlag *supported,
				 BraseroBurnFlag *compulsory);
gboolean
brasero_plugin_get_process_flags (BraseroPlugin *plugin,
				  BraseroPluginProcessFlag *flags);

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

G_END_DECLS

#endif /* _BURN_PLUGIN_PRIVATE_H */

 
