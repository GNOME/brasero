/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2005-2008 <bonfire-app@wanadoo.fr>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gio/gio.h>

#include <gtk/gtk.h>

#include "brasero-misc.h"

#include "brasero-filter-option.h"
#include "brasero-data-vfs.h"

typedef struct _BraseroFilterOptionPrivate BraseroFilterOptionPrivate;
struct _BraseroFilterOptionPrivate
{
	GSettings *settings;
};

#define BRASERO_FILTER_OPTION_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_FILTER_OPTION, BraseroFilterOptionPrivate))

G_DEFINE_TYPE (BraseroFilterOption, brasero_filter_option, GTK_TYPE_BOX);

static void
brasero_filter_option_init (BraseroFilterOption *object)
{
	gchar *string;
	GtkWidget *frame;
	GtkWidget *button_sym;
	GtkWidget *button_broken;
	GtkWidget *button_hidden;
	BraseroFilterOptionPrivate *priv;

	priv = BRASERO_FILTER_OPTION_PRIVATE (object);

	priv->settings = g_settings_new (BRASERO_SCHEMA_FILTER);

	gtk_orientable_set_orientation (GTK_ORIENTABLE (object), GTK_ORIENTATION_VERTICAL);

	/* filter hidden files */
	button_hidden = gtk_check_button_new_with_mnemonic (_("Filter _hidden files"));
	g_settings_bind (priv->settings, BRASERO_PROPS_FILTER_HIDDEN,
	                 button_hidden, "active",
	                 G_SETTINGS_BIND_DEFAULT);
	gtk_widget_show (button_hidden);

	/* replace symlink */
	button_sym = gtk_check_button_new_with_mnemonic (_("Re_place symbolic links"));
	g_settings_bind (priv->settings, BRASERO_PROPS_FILTER_REPLACE_SYMLINK,
	                 button_sym, "active",
	                 G_SETTINGS_BIND_DEFAULT);
	gtk_widget_show (button_sym);

	/* filter broken symlink button */
	button_broken = gtk_check_button_new_with_mnemonic (_("Filter _broken symbolic links"));
	g_settings_bind (priv->settings, BRASERO_PROPS_FILTER_BROKEN,
	                 button_broken, "active",
	                 G_SETTINGS_BIND_DEFAULT);
	gtk_widget_show (button_broken);

	string = g_strdup_printf ("<b>%s</b>", _("Filtering options"));
	frame = brasero_utils_pack_properties (string,
					       button_sym,
					       button_broken,
					       button_hidden,
					       NULL);
	g_free (string);

	gtk_box_pack_start (GTK_BOX (object),
			    frame,
			    FALSE,
			    FALSE,
			    0);
}

static void
brasero_filter_option_finalize (GObject *object)
{
	BraseroFilterOptionPrivate *priv;

	priv = BRASERO_FILTER_OPTION_PRIVATE (object);

	if (priv->settings) {
		g_object_unref (priv->settings);
		priv->settings = NULL;
	}

	G_OBJECT_CLASS (brasero_filter_option_parent_class)->finalize (object);
}

static void
brasero_filter_option_class_init (BraseroFilterOptionClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroFilterOptionPrivate));

	object_class->finalize = brasero_filter_option_finalize;
}

GtkWidget *
brasero_filter_option_new (void)
{
	return g_object_new (BRASERO_TYPE_FILTER_OPTION, NULL);
}
