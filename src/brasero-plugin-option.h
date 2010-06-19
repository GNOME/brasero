/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2007-2008 <bonfire-app@wanadoo.fr>
 * 
 *  Brasero is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#ifndef _BRASERO_PLUGIN_OPTION_H_
#define _BRASERO_PLUGIN_OPTION_H_

#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_PLUGIN_OPTION             (brasero_plugin_option_get_type ())
#define BRASERO_PLUGIN_OPTION(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_PLUGIN_OPTION, BraseroPluginOption))
#define BRASERO_PLUGIN_OPTION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_PLUGIN_OPTION, BraseroPluginOptionClass))
#define BRASERO_IS_PLUGIN_OPTION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_PLUGIN_OPTION))
#define BRASERO_IS_PLUGIN_OPTION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_PLUGIN_OPTION))
#define BRASERO_PLUGIN_OPTION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_PLUGIN_OPTION, BraseroPluginOptionClass))

typedef struct _BraseroPluginOptionClass BraseroPluginOptionClass;
typedef struct _BraseroPluginOption BraseroPluginOption;

struct _BraseroPluginOptionClass
{
	GtkDialogClass parent_class;
};

struct _BraseroPluginOption
{
	GtkDialog parent_instance;
};

GType brasero_plugin_option_get_type (void) G_GNUC_CONST;

GtkWidget *
brasero_plugin_option_new (void);

void
brasero_plugin_option_set_plugin (BraseroPluginOption *dialog,
				  BraseroPlugin *plugin);

G_END_DECLS

#endif /* _BRASERO_PLUGIN_OPTION_H_ */
