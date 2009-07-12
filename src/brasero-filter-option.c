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
#include <glib/gstdio.h>

#include <gtk/gtk.h>

#include <gconf/gconf-client.h>

#include "brasero-misc.h"

#include "brasero-filter-option.h"
#include "brasero-data-vfs.h"

typedef struct _BraseroFilterOptionPrivate BraseroFilterOptionPrivate;
struct _BraseroFilterOptionPrivate
{
	GConfClient *client;
	guint broken_sym_notify;
	guint sym_notify;
	guint hidden_notify;
};

#define BRASERO_FILTER_OPTION_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_FILTER_OPTION, BraseroFilterOptionPrivate))

G_DEFINE_TYPE (BraseroFilterOption, brasero_filter_option, GTK_TYPE_VBOX);

static void
brasero_file_filtered_filter_hidden_cb (GtkToggleButton *button,
					BraseroFilterOption *self)
{
	BraseroFilterOptionPrivate *priv;

	priv = BRASERO_FILTER_OPTION_PRIVATE (self);
	gconf_client_set_bool (priv->client,
			       BRASERO_FILTER_HIDDEN_KEY,
			       gtk_toggle_button_get_active (button),
			       NULL);
}

static void
brasero_file_filtered_filter_broken_sym_cb (GtkToggleButton *button,
					    BraseroFilterOption *self)
{
	BraseroFilterOptionPrivate *priv;

	priv = BRASERO_FILTER_OPTION_PRIVATE (self);
	gconf_client_set_bool (priv->client,
			       BRASERO_FILTER_BROKEN_SYM_KEY,
			       gtk_toggle_button_get_active (button),
			       NULL);
}

static void
brasero_file_filtered_replace_sym_cb (GtkToggleButton *button,
				      BraseroFilterOption *self)
{
	BraseroFilterOptionPrivate *priv;

	priv = BRASERO_FILTER_OPTION_PRIVATE (self);
	gconf_client_set_bool (priv->client,
			       BRASERO_REPLACE_SYMLINK_KEY,
			       gtk_toggle_button_get_active (button),
			       NULL);
}

static void
brasero_file_filtered_gconf_notify_cb (GConfClient *client,
				       guint cnxn_id,
				       GConfEntry *entry,
				       gpointer user_data)
{
	GConfValue *value;
	GtkToggleButton *button = user_data;

	value = gconf_entry_get_value (entry);
	gtk_toggle_button_set_active (button, gconf_value_get_bool (value));
}

static void
brasero_filter_option_init (BraseroFilterOption *object)
{
	gchar *string;
	gboolean active;
	GtkWidget *frame;
	GError *error = NULL;
	GtkWidget *button_sym;
	GtkWidget *button_broken;
	GtkWidget *button_hidden;
	BraseroFilterOptionPrivate *priv;

	priv = BRASERO_FILTER_OPTION_PRIVATE (object);

	priv->client = gconf_client_get_default ();

	/* filter hidden files */
	active = gconf_client_get_bool (priv->client,
					BRASERO_FILTER_HIDDEN_KEY,
					NULL);

	button_hidden = gtk_check_button_new_with_mnemonic (_("Filter _hidden files"));
	gtk_widget_show (button_hidden);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_hidden), active);
	g_signal_connect (button_hidden,
			  "toggled",
			  G_CALLBACK (brasero_file_filtered_filter_hidden_cb),
			  object);

	priv->hidden_notify = gconf_client_notify_add (priv->client,
						       BRASERO_FILTER_HIDDEN_KEY,
						       brasero_file_filtered_gconf_notify_cb,
						       button_hidden, NULL, &error);
	if (error) {
		g_warning ("GConf : %s\n", error->message);
		g_error_free (error);
		error = NULL;
	}

	/* replace symlink */
	active = gconf_client_get_bool (priv->client,
					BRASERO_REPLACE_SYMLINK_KEY,
					NULL);

	button_sym = gtk_check_button_new_with_mnemonic (_("Re_place symbolic links"));
	gtk_widget_show (button_sym);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_sym), active);
	g_signal_connect (button_sym,
			  "toggled",
			  G_CALLBACK (brasero_file_filtered_replace_sym_cb),
			  object);

	priv->sym_notify = gconf_client_notify_add (priv->client,
						    BRASERO_REPLACE_SYMLINK_KEY,
						    brasero_file_filtered_gconf_notify_cb,
						    button_sym, NULL, &error);
	if (error) {
		g_warning ("GConf : %s\n", error->message);
		g_error_free (error);
		error = NULL;
	}

	/* filter broken symlink button */
	active = gconf_client_get_bool (priv->client,
					BRASERO_FILTER_BROKEN_SYM_KEY,
					NULL);

	button_broken = gtk_check_button_new_with_mnemonic (_("Filter _broken symbolic links"));
	gtk_widget_show (button_broken);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_broken), active);
	g_signal_connect (button_broken,
			  "toggled",
			  G_CALLBACK (brasero_file_filtered_filter_broken_sym_cb),
			  object);

	priv->broken_sym_notify = gconf_client_notify_add (priv->client,
							   BRASERO_FILTER_BROKEN_SYM_KEY,
							   brasero_file_filtered_gconf_notify_cb,
							   button_broken, NULL, &error);
	if (error) {
		g_warning ("GConf : %s\n", error->message);
		g_error_free (error);
		error = NULL;
	}

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

	if (priv->sym_notify) {
		gconf_client_notify_remove (priv->client, priv->sym_notify);
		priv->sym_notify = 0;
	}

	if (priv->hidden_notify) {
		gconf_client_notify_remove (priv->client,
					    priv->hidden_notify);
		priv->hidden_notify = 0;
	}

	if (priv->broken_sym_notify) {
		gconf_client_notify_remove (priv->client,
					    priv->broken_sym_notify);
		priv->broken_sym_notify = 0;
	}
	
	if (priv->client) {
		g_object_unref (priv->client);
		priv->client = NULL;
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
