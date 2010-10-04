/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-misc
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-misc is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-misc authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-misc. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-misc is distributed in the hope that it will be useful,
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "brasero-tool-color-picker.h"

typedef struct _BraseroToolColorPickerPrivate BraseroToolColorPickerPrivate;
struct _BraseroToolColorPickerPrivate
{
	GtkWidget *dialog;

	GtkWidget *label;
	GtkWidget *icon;

	GdkColor color;
};

enum {
	COLOR_SET_SIGNAL,
	LAST_SIGNAL,
};
static guint tool_color_picker_signals[LAST_SIGNAL] = { 0 };


#define BRASERO_TOOL_COLOR_PICKER_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_TOOL_COLOR_PICKER, BraseroToolColorPickerPrivate))

G_DEFINE_TYPE (BraseroToolColorPicker, brasero_tool_color_picker, GTK_TYPE_TOOL_BUTTON);

void
brasero_tool_color_picker_set_text (BraseroToolColorPicker *self,
				    const gchar *text)
{
	BraseroToolColorPickerPrivate *priv;

	priv = BRASERO_TOOL_COLOR_PICKER_PRIVATE (self);
	gtk_label_set_text_with_mnemonic (GTK_LABEL (priv->label), text);
}

void
brasero_tool_color_picker_get_color (BraseroToolColorPicker *self,
				     GdkColor *color)
{
	BraseroToolColorPickerPrivate *priv;

	priv = BRASERO_TOOL_COLOR_PICKER_PRIVATE (self);
	color->red = priv->color.red;
	color->green = priv->color.green;
	color->blue = priv->color.blue;
}

void
brasero_tool_color_picker_set_color (BraseroToolColorPicker *self,
				     GdkColor *color)
{
	BraseroToolColorPickerPrivate *priv;

	priv = BRASERO_TOOL_COLOR_PICKER_PRIVATE (self);
	priv->color.red = color->red;
	priv->color.green = color->green;
	priv->color.blue = color->blue;

	gtk_widget_queue_draw (GTK_WIDGET (self));
}

static gboolean
brasero_tool_color_picker_draw (GtkWidget *widget,
				cairo_t *ctx,
				BraseroToolColorPicker *self)
{
	BraseroToolColorPickerPrivate *priv;
	GtkAllocation allocation;

	priv = BRASERO_TOOL_COLOR_PICKER_PRIVATE (self);

	gdk_cairo_set_source_color (ctx, &priv->color);
	gtk_widget_get_allocation (widget, &allocation);
	cairo_rectangle (ctx,
			 0,
			 0,
			 allocation.width,
			 allocation.height);
	cairo_fill (ctx);
	cairo_stroke (ctx);

	return TRUE;
}

static void
brasero_tool_color_picker_destroy (GtkWidget *widget,
                                   BraseroToolColorPicker *self)
{
	BraseroToolColorPickerPrivate *priv;

	priv = BRASERO_TOOL_COLOR_PICKER_PRIVATE (self);
	priv->dialog = NULL;
}

static void
brasero_tool_color_picker_response (GtkWidget *widget,
                                    GtkResponseType response,
                                    BraseroToolColorPicker *self)
{
	BraseroToolColorPickerPrivate *priv;
	GtkColorSelection *selection;

	priv = BRASERO_TOOL_COLOR_PICKER_PRIVATE (self);

	if (response == GTK_RESPONSE_OK) {
		selection = GTK_COLOR_SELECTION (gtk_color_selection_dialog_get_color_selection (GTK_COLOR_SELECTION_DIALOG (priv->dialog)));
		gtk_color_selection_get_current_color (selection, &priv->color);

		g_signal_emit (self,
			       tool_color_picker_signals[COLOR_SET_SIGNAL],
			       0);
	}

	gtk_widget_destroy (priv->dialog);
	priv->dialog = NULL;
}

static void
brasero_tool_color_picker_clicked (BraseroToolColorPicker *self,
				   gpointer NULL_data)
{
	GtkWidget *dialog;
	GtkWidget *toplevel;
	GtkColorSelection *selection;
	BraseroToolColorPickerPrivate *priv;

	priv = BRASERO_TOOL_COLOR_PICKER_PRIVATE (self);

	dialog = gtk_color_selection_dialog_new (_("Pick a Color"));
	selection = GTK_COLOR_SELECTION (gtk_color_selection_dialog_get_color_selection (GTK_COLOR_SELECTION_DIALOG (dialog)));
	gtk_color_selection_set_current_color (selection, &priv->color);

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
	if (toplevel && GTK_IS_WINDOW (toplevel)) {
		gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
		gtk_window_set_modal (GTK_WINDOW (dialog), gtk_window_get_modal (GTK_WINDOW (toplevel)));
	}

	g_signal_connect (GTK_COLOR_SELECTION_DIALOG (dialog),
			  "response",
			  G_CALLBACK (brasero_tool_color_picker_response),
			  self);
	g_signal_connect (dialog,
			  "destroy",
			  G_CALLBACK (brasero_tool_color_picker_destroy),
			  self);

	priv->dialog = dialog;
	gtk_widget_show (dialog);
	gtk_window_present (GTK_WINDOW (dialog));
}

static void
brasero_tool_color_picker_init (BraseroToolColorPicker *object)
{
	BraseroToolColorPickerPrivate *priv;

	priv = BRASERO_TOOL_COLOR_PICKER_PRIVATE (object);

	g_signal_connect (object,
			  "clicked",
			  G_CALLBACK (brasero_tool_color_picker_clicked),
			  NULL);

	priv->label = gtk_label_new_with_mnemonic ("");
	gtk_widget_show (priv->label);
	gtk_tool_button_set_label_widget (GTK_TOOL_BUTTON (object), priv->label);

	priv->icon = gtk_image_new ();
	gtk_widget_show (priv->icon);
	g_signal_connect (priv->icon,
			  "draw",
			  G_CALLBACK (brasero_tool_color_picker_draw),
			  object);

	/* This function expects a GtkMisc object!! */
	gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (object), priv->icon);
}

static void
brasero_tool_color_picker_finalize (GObject *object)
{
	G_OBJECT_CLASS (brasero_tool_color_picker_parent_class)->finalize (object);
}

static void
brasero_tool_color_picker_class_init (BraseroToolColorPickerClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroToolColorPickerPrivate));

	object_class->finalize = brasero_tool_color_picker_finalize;

	tool_color_picker_signals[COLOR_SET_SIGNAL] =
		g_signal_new ("color_set",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_FIRST,
		              0,
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0,
		              G_TYPE_NONE);
}

GtkWidget *
brasero_tool_color_picker_new (void)
{
	return g_object_new (BRASERO_TYPE_TOOL_COLOR_PICKER, NULL);
}
