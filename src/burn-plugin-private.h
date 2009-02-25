/***************************************************************************
 *            burn-plugin-private.h
 *
 *  Mon Feb 12 10:40:55 2007
 *  Copyright  2007  Rouquier Philippe
 *  <bonfire-app@wanadoo.fr>
 ****************************************************************************/

/*
 * Brasero is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * Brasero is distributed in the hope that it will be useful,
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

struct _BraseroPluginChoicePair {
	gchar *string;
	guint value;
};
typedef struct _BraseroPluginChoicePair BraseroPluginChoicePair;

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

gchar *
brasero_plugin_get_gconf_priority_key (BraseroPlugin *plugin);

const gchar *
brasero_plugin_get_error (BraseroPlugin *plugin);

gboolean
brasero_plugin_get_compulsory (BraseroPlugin *plugin);

guint
brasero_plugin_get_priority (BraseroPlugin *plugin);

gboolean
brasero_plugin_check_image_flags (BraseroPlugin *plugin,
				  BraseroMedia media,
				  BraseroBurnFlag current);
gboolean
brasero_plugin_check_blank_flags (BraseroPlugin *plugin,
				  BraseroMedia media,
				  BraseroBurnFlag current);
gboolean
brasero_plugin_check_record_flags (BraseroPlugin *plugin,
				   BraseroMedia media,
				   BraseroBurnFlag current);
gboolean
brasero_plugin_check_media_restrictions (BraseroPlugin *plugin,
					 BraseroMedia media);

gboolean
brasero_plugin_get_image_flags (BraseroPlugin *plugin,
			        BraseroMedia media,
				BraseroBurnFlag current,
			        BraseroBurnFlag *supported,
			        BraseroBurnFlag *compulsory);
gboolean
brasero_plugin_get_blank_flags (BraseroPlugin *plugin,
				BraseroMedia media,
				BraseroBurnFlag current,
				BraseroBurnFlag *supported,
				BraseroBurnFlag *compulsory);
gboolean
brasero_plugin_get_record_flags (BraseroPlugin *plugin,
				 BraseroMedia media,
				 BraseroBurnFlag current,
				 BraseroBurnFlag *supported,
				 BraseroBurnFlag *compulsory);

gboolean
brasero_plugin_get_process_flags (BraseroPlugin *plugin,
				  BraseroPluginProcessFlag *flags);


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

GSList *
brasero_plugin_conf_option_choice_get (BraseroPluginConfOption *option);

G_END_DECLS

#endif /* _BURN_PLUGIN_PRIVATE_H */

 
