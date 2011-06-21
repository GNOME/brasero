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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <gtk/gtk.h>

#include "brasero-plugin.h"
#include "brasero-plugin-information.h"
#include "brasero-plugin-option.h"

enum {
	STRING_COL,
	VALUE_COL,
	COL_NUM
};

typedef struct _BraseroPluginOptionPrivate BraseroPluginOptionPrivate;
struct _BraseroPluginOptionPrivate
{
	GtkWidget *title;
	GtkWidget *vbox;

	GSettings *settings;
};

#define BRASERO_PLUGIN_OPTION_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_PLUGIN_OPTION, BraseroPluginOptionPrivate))

G_DEFINE_TYPE (BraseroPluginOption, brasero_plugin_option, GTK_TYPE_DIALOG);
#define BRASERO_SCHEMA_CONFIG		"org.gnome.brasero.config"

static GtkWidget *
brasero_plugin_option_add_conf_widget (BraseroPluginOption *self,
				       BraseroPluginConfOption *option,
				       GtkBox *container)
{
	BraseroPluginOptionPrivate *priv;
	BraseroPluginConfOptionType type;
	GtkCellRenderer *renderer;
	GtkListStore *model;
	gchar *description;
	GSList *suboptions;
	GtkWidget *widget;
	GtkWidget *label;
	GtkTreeIter iter;
	GtkWidget *hbox;
	GtkWidget *box;
	gchar *key;

	priv = BRASERO_PLUGIN_OPTION_PRIVATE (self);

	brasero_plugin_conf_option_get_info (option,
					     &key,
					     &description,
					     &type);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_start (container, hbox, FALSE, FALSE, 0);

	label = gtk_label_new ("\t");
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	box = NULL;
	switch (type) {
	case BRASERO_PLUGIN_OPTION_BOOL:
		widget = gtk_check_button_new_with_label (description);
		g_settings_bind (priv->settings, key,
			         widget, "active",
			         G_SETTINGS_BIND_DEFAULT);

		gtk_widget_show (widget);

		suboptions = brasero_plugin_conf_option_bool_get_suboptions (option);
		if (suboptions) {
			box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
			gtk_widget_show (box);

			gtk_box_pack_start (GTK_BOX (box),
					    widget,
					    FALSE,
					    FALSE,
					    0);
			gtk_box_pack_start (GTK_BOX (hbox),
					    box,
					    FALSE,
					    FALSE,
					    0);

			for (; suboptions; suboptions = suboptions->next) {
				GtkWidget *child;
				BraseroPluginConfOption *suboption;

				suboption = suboptions->data;

				/* first create the slaves then set state */
				child = brasero_plugin_option_add_conf_widget (self, suboption, GTK_BOX (box));
				g_settings_bind (priv->settings, key,
				                 child, "sensitive",
				                 G_SETTINGS_BIND_DEFAULT);
			}
		}
		else
			gtk_box_pack_start (GTK_BOX (hbox),
					    widget,
					    FALSE,
					    FALSE,
					    0);
		break;

	case BRASERO_PLUGIN_OPTION_INT:
		box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

		label = gtk_label_new (description);
		gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
		gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

		widget = gtk_spin_button_new_with_range (brasero_plugin_conf_option_int_get_min (option),
		                                         brasero_plugin_conf_option_int_get_max (option),
		                                         1.0);
		gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);

		gtk_widget_show_all (box);
		gtk_box_pack_start (GTK_BOX (hbox),
				    box,
				    FALSE,
				    FALSE,
				    0);

		g_settings_bind (priv->settings, key,
			         widget, "value",
			         G_SETTINGS_BIND_DEFAULT);
		break;

	case BRASERO_PLUGIN_OPTION_STRING:
		box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

		label = gtk_label_new (description);
		gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
		gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

		widget = gtk_entry_new ();
		gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);

		gtk_widget_show_all (box);
		gtk_box_pack_start (GTK_BOX (hbox),
				    box,
				    FALSE,
				    FALSE,
				    0);

		g_settings_bind (priv->settings, key,
			         widget, "text",
			         G_SETTINGS_BIND_DEFAULT);
		break;

	case BRASERO_PLUGIN_OPTION_CHOICE:
		box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

		label = gtk_label_new (description);
		gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
		gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

		model = gtk_list_store_new (COL_NUM,
					    G_TYPE_STRING,
					    G_TYPE_INT);
		widget = gtk_combo_box_new_with_model (GTK_TREE_MODEL (model));
		g_object_unref (model);
		gtk_widget_show (widget);
		gtk_box_pack_start (GTK_BOX (box), widget, FALSE, TRUE, 0);

		renderer = gtk_cell_renderer_text_new ();
		gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (widget), renderer, TRUE);
		gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (widget), renderer,
						"text", STRING_COL,
						NULL);

		suboptions = brasero_plugin_conf_option_choice_get (option);
		for (; suboptions; suboptions = suboptions->next) {
			BraseroPluginChoicePair *pair;

			pair = suboptions->data;
			gtk_list_store_append (GTK_LIST_STORE (model), &iter);
			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					    STRING_COL, pair->string,
					    VALUE_COL, pair->value,
					    -1);
		}

		g_settings_bind (priv->settings, key,
			         widget, "active",
			         G_SETTINGS_BIND_DEFAULT);
			
		if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter)) {
			if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (model), &iter))
				gtk_combo_box_set_active_iter (GTK_COMBO_BOX (widget), &iter);
		}

		gtk_widget_show_all (box);
		gtk_box_pack_start (GTK_BOX (hbox),
				    box,
				    FALSE,
				    FALSE,
				    0);
		break;


	default:
		widget = NULL;
		break;
	}

	g_free (key);
	g_free (description);

	return widget;
}

void
brasero_plugin_option_set_plugin (BraseroPluginOption *self,
				  BraseroPlugin *plugin)
{
	BraseroPluginOptionPrivate *priv;
	BraseroPluginConfOption *option; 
	gchar *string;
	gchar *tmp;

	priv = BRASERO_PLUGIN_OPTION_PRIVATE (self);

	/* Use the translated name for the plugin. */
	tmp = g_strdup_printf (_("Options for plugin %s"), _(brasero_plugin_get_display_name (plugin)));
	string = g_strdup_printf ("<b>%s</b>", tmp);
	g_free (tmp);

	gtk_label_set_markup (GTK_LABEL (priv->title), string);
	g_free (string);

	option = brasero_plugin_get_next_conf_option (plugin, NULL);
	for (; option; option = brasero_plugin_get_next_conf_option (plugin, option))
		brasero_plugin_option_add_conf_widget (self,
						       option,
						       GTK_BOX (priv->vbox));
}

static void
brasero_plugin_option_init (BraseroPluginOption *object)
{
	BraseroPluginOptionPrivate *priv;
	GtkWidget *frame;

	priv = BRASERO_PLUGIN_OPTION_PRIVATE (object);

	frame = gtk_frame_new ("");
	gtk_container_set_border_width (GTK_CONTAINER (frame), 8);
	gtk_widget_show (frame);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);

	priv->title = gtk_frame_get_label_widget (GTK_FRAME (frame));
	gtk_label_set_use_markup (GTK_LABEL (priv->title), TRUE);

	priv->vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (priv->vbox);
	gtk_container_set_border_width (GTK_CONTAINER (priv->vbox), 8);
	gtk_container_add (GTK_CONTAINER (frame), priv->vbox);

	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (object))),
			    frame,
			    FALSE,
			    FALSE,
			    0);

	gtk_dialog_add_button (GTK_DIALOG (object),
			       GTK_STOCK_CLOSE, GTK_RESPONSE_OK);

	priv->settings = g_settings_new (BRASERO_SCHEMA_CONFIG);
}

static void
brasero_plugin_option_finalize (GObject *object)
{
	BraseroPluginOptionPrivate *priv;

	priv = BRASERO_PLUGIN_OPTION_PRIVATE (object);

	if (priv->settings) {
		g_object_unref (priv->settings);
		priv->settings = NULL;
	}

	G_OBJECT_CLASS (brasero_plugin_option_parent_class)->finalize (object);
}

static void
brasero_plugin_option_class_init (BraseroPluginOptionClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroPluginOptionPrivate));
	object_class->finalize = brasero_plugin_option_finalize;
}

GtkWidget *
brasero_plugin_option_new (void)
{
	return g_object_new (BRASERO_TYPE_PLUGIN_OPTION, NULL);
}
