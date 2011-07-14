/*
 * brasero-plugin-manager.h
 * This file is part of brasero
 *
 * Copyright (C) 2007 Philippe Rouquier
 *
 * Based on brasero code (brasero/brasero-plugin-manager.c) by: 
 * 	- Paolo Maggi <paolo@gnome.org>
 *
 * Libbrasero-media is free software; you can redistribute it and/or modify
fy
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Brasero is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifndef __BRASERO_PLUGIN_MANAGER_UI_H__
#define __BRASERO_PLUGIN_MANAGER_UI_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

/*
 * Type checking and casting macros
 */
#define BRASERO_TYPE_PLUGIN_MANAGER_UI              (brasero_plugin_manager_ui_get_type())
#define BRASERO_PLUGIN_MANAGER_UI(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), BRASERO_TYPE_PLUGIN_MANAGER_UI, BraseroPluginManagerUI))
#define BRASERO_PLUGIN_MANAGER_UI_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), BRASERO_TYPE_PLUGIN_MANAGER_UI, BraseroPluginManagerUIClass))
#define BRASERO_IS_PLUGIN_MANAGER_UI(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), BRASERO_TYPE_PLUGIN_MANAGER_UI))
#define BRASERO_IS_PLUGIN_MANAGER_UI_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_PLUGIN_MANAGER_UI))
#define BRASERO_PLUGIN_MANAGER_UI_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), BRASERO_TYPE_PLUGIN_MANAGER_UI, BraseroPluginManagerUIClass))

/* Private structure type */
typedef struct _BraseroPluginManagerUIPrivate BraseroPluginManagerUIPrivate;

/*
 * Main object structure
 */
typedef struct _BraseroPluginManagerUI BraseroPluginManagerUI;

struct _BraseroPluginManagerUI 
{
	GtkBox vbox;
};

/*
 * Class definition
 */
typedef struct _BraseroPluginManagerUIClass BraseroPluginManagerUIClass;

struct _BraseroPluginManagerUIClass 
{
	GtkBoxClass parent_class;
};

/*
 * Public methods
 */
GType		 brasero_plugin_manager_ui_get_type		(void) G_GNUC_CONST;

GtkWidget	*brasero_plugin_manager_ui_new		(void);
   
G_END_DECLS

#endif  /* __BRASERO_PLUGIN_MANAGER_UI_H__  */
