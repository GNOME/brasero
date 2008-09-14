/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2005-2008 <bonfire-app@wanadoo.fr>
 * 
 * brasero is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * brasero is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtk.h>
 
#include "brasero-jacket-background.h"

typedef struct _BraseroJacketBackgroundPrivate BraseroJacketBackgroundPrivate;
struct _BraseroJacketBackgroundPrivate
{
	gchar *path;

	GtkWidget *image;
	GtkWidget *image_style;

	GtkWidget *color;
	GtkWidget *color2;
	GtkWidget *color_style;
};

#define BRASERO_JACKET_BACKGROUND_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_JACKET_BACKGROUND, BraseroJacketBackgroundPrivate))



G_DEFINE_TYPE (BraseroJacketBackground, brasero_jacket_background, GTK_TYPE_DIALOG);

BraseroJacketColorStyle
brasero_jacket_background_get_color_style (BraseroJacketBackground *self)
{
	BraseroJacketBackgroundPrivate *priv;

	priv = BRASERO_JACKET_BACKGROUND_PRIVATE (self);
	return gtk_combo_box_get_active (GTK_COMBO_BOX (priv->color_style));
}

BraseroJacketImageStyle
brasero_jacket_background_get_image_style (BraseroJacketBackground *self)
{
	BraseroJacketBackgroundPrivate *priv;

	priv = BRASERO_JACKET_BACKGROUND_PRIVATE (self);
	return gtk_combo_box_get_active (GTK_COMBO_BOX (priv->image_style));
}

gchar *
brasero_jacket_background_get_image_path (BraseroJacketBackground *self)
{
	BraseroJacketBackgroundPrivate *priv;

	priv = BRASERO_JACKET_BACKGROUND_PRIVATE (self);
	return gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (priv->image));
}

void
brasero_jacket_background_set_color_style (BraseroJacketBackground *self,
					   BraseroJacketColorStyle style)
{
	BraseroJacketBackgroundPrivate *priv;

	priv = BRASERO_JACKET_BACKGROUND_PRIVATE (self);
	gtk_combo_box_set_active (GTK_COMBO_BOX (priv->color_style), style);
}

void
brasero_jacket_background_get_color (BraseroJacketBackground *self,
				     GdkColor *color,
				     GdkColor *color2)
{
	BraseroJacketBackgroundPrivate *priv;

	priv = BRASERO_JACKET_BACKGROUND_PRIVATE (self);
	gtk_color_button_get_color (GTK_COLOR_BUTTON (priv->color), color);
	gtk_color_button_get_color (GTK_COLOR_BUTTON (priv->color2), color2);
}

void
brasero_jacket_background_set_image_style (BraseroJacketBackground *self,
					   BraseroJacketImageStyle style)
{
	BraseroJacketBackgroundPrivate *priv;

	priv = BRASERO_JACKET_BACKGROUND_PRIVATE (self);
	gtk_combo_box_set_active (GTK_COMBO_BOX (priv->image_style), style);
}

void
brasero_jacket_background_set_image_path (BraseroJacketBackground *self,
					  const gchar *path)
{
	BraseroJacketBackgroundPrivate *priv;

	if (!path)
		return;

	priv = BRASERO_JACKET_BACKGROUND_PRIVATE (self);
	gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (priv->image), path);
}

void
brasero_jacket_background_set_color (BraseroJacketBackground *self,
				     GdkColor *color,
				     GdkColor *color2)
{
	BraseroJacketBackgroundPrivate *priv;

	priv = BRASERO_JACKET_BACKGROUND_PRIVATE (self);
	gtk_color_button_set_color (GTK_COLOR_BUTTON (priv->color), color);
	gtk_color_button_set_color (GTK_COLOR_BUTTON (priv->color2), color2);
}

static void
brasero_jacket_background_color_type_changed_cb (GtkComboBox *combo,
						 BraseroJacketBackground *self)
{
	BraseroJacketBackgroundPrivate *priv;

	priv = BRASERO_JACKET_BACKGROUND_PRIVATE (self);

	if (gtk_combo_box_get_active (combo) == BRASERO_JACKET_COLOR_SOLID) {
		gtk_widget_hide (priv->color2);
		return;
	}

	gtk_widget_show (priv->color2);
}

static void
brasero_jacket_background_add_filters (BraseroJacketBackground *self)
{
	BraseroJacketBackgroundPrivate *priv;
	GtkFileFilter *filter;

	priv = BRASERO_JACKET_BACKGROUND_PRIVATE (self);

	filter = gtk_file_filter_new ();
	gtk_file_filter_add_pixbuf_formats (filter);
	gtk_file_filter_set_name (filter, _("Images"));
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (priv->image), filter);
}

static void
brasero_jacket_background_init (BraseroJacketBackground *object)
{
	BraseroJacketBackgroundPrivate *priv;
	GtkWidget *combo;
	GtkWidget *hbox2;
	GtkWidget *label;
	GtkWidget *vbox2;
	GtkWidget *vbox;
	GtkWidget *hbox;
	gchar *string;

	priv = BRASERO_JACKET_BACKGROUND_PRIVATE (object);

	vbox = gtk_vbox_new (FALSE, 12);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
	gtk_widget_show (vbox);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (object)->vbox), vbox, TRUE, TRUE, 0);

	string = g_strdup_printf ("<b>%s</b", _("_Color"));
	label = gtk_label_new_with_mnemonic (string);
	g_free (string);

	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

	label = gtk_label_new ("\t");
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

	vbox2 = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (hbox), vbox2, FALSE, TRUE, 0);

	hbox2 = gtk_hbox_new (FALSE, 12);
	gtk_widget_show (hbox2);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox2, FALSE, TRUE, 0);

	combo = gtk_combo_box_new_text ();
	priv->color_style = combo;
	gtk_widget_show (combo);
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Solid color"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Horizontal gradient"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Vertical gradient"));
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
	gtk_box_pack_start (GTK_BOX (hbox2), combo, FALSE, TRUE, 0);
	g_signal_connect (combo,
			  "changed",
			  G_CALLBACK (brasero_jacket_background_color_type_changed_cb),
			  object);

	priv->color = gtk_color_button_new ();
	gtk_widget_show (priv->color);
	gtk_box_pack_start (GTK_BOX (hbox2), priv->color, FALSE, TRUE, 0);

	priv->color2 = gtk_color_button_new ();
	gtk_box_pack_start (GTK_BOX (hbox2), priv->color2, FALSE, TRUE, 0);

	string = g_strdup_printf ("<b>%s</b>", _("_Image"));
	label = gtk_label_new_with_mnemonic (string);
	g_free (string);

	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

	label = gtk_label_new ("\t");
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	vbox2 = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, TRUE, 0);

	hbox2 = gtk_hbox_new (FALSE, 12);
	gtk_widget_show (hbox2);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox2, TRUE, TRUE, 0);

	label = gtk_label_new (_("Image path:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, TRUE, 0);

	priv->image = gtk_file_chooser_button_new (_("Choose an image"), GTK_FILE_CHOOSER_ACTION_OPEN);
	gtk_widget_show (priv->image);
	gtk_box_pack_start (GTK_BOX (hbox2), priv->image, TRUE, TRUE, 0);

	hbox2 = gtk_hbox_new (FALSE, 12);
	gtk_widget_show (hbox2);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox2, FALSE, TRUE, 0);

	label = gtk_label_new (_("Image style:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, TRUE, 0);

	combo = gtk_combo_box_new_text ();
	priv->image_style = combo;
	gtk_widget_show (combo);
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Centered"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Tiled"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Scaled"));
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
	gtk_box_pack_start (GTK_BOX (hbox2), combo, FALSE, TRUE, 0);

	gtk_dialog_add_button (GTK_DIALOG (object), 
			       GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);

	brasero_jacket_background_add_filters (object);
	gtk_window_set_default_size (GTK_WINDOW (object), 400, 240);
}

static void
brasero_jacket_background_finalize (GObject *object)
{
	G_OBJECT_CLASS (brasero_jacket_background_parent_class)->finalize (object);
}

static void
brasero_jacket_background_class_init (BraseroJacketBackgroundClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroJacketBackgroundPrivate));

	object_class->finalize = brasero_jacket_background_finalize;
}

GtkWidget *
brasero_jacket_background_new (void)
{
	return g_object_new (BRASERO_TYPE_JACKET_BACKGROUND,
			     "title", _("Background Properties"),
			     NULL);
}

