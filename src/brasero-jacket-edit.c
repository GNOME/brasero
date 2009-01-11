/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Rouquier Philippe 2008 <bonfire-app@wanadoo.fr>
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

#include <gtk/gtk.h>

#include "brasero-jacket-edit.h"
#include "brasero-jacket-buffer.h"
#include "brasero-jacket-view.h"
#include "brasero-jacket-font.h"
#include "brasero-utils.h"
#include "brasero-tool-color-picker.h"

#include "burn-track.h"

typedef struct _BraseroJacketEditPrivate BraseroJacketEditPrivate;
struct _BraseroJacketEditPrivate
{
	GtkWidget *current_view;
	GtkWidget *front;
	GtkWidget *back;

	GtkWidget *fonts;
	GtkWidget *colours;

	GtkWidget *center;
	GtkWidget *right;
	GtkWidget *left;

	GtkWidget *underline;
	GtkWidget *italic;
	GtkWidget *bold;

	GtkWidget *background;
};

#define BRASERO_JACKET_EDIT_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_JACKET_EDIT, BraseroJacketEditPrivate))

enum {
	SNAP_PIX_COL,
	SNAP_TEXT_COL,
	SNAP_TOOLTIP_COL,
	SNAP_WIDGET_COL,
	SNAP_NUM_COL
};

G_DEFINE_TYPE (BraseroJacketEdit, brasero_jacket_edit, GTK_TYPE_VBOX);

static void
brasero_jacket_edit_print_page (GtkPrintOperation *operation,
				GtkPrintContext *context,
				gint page_num,
				BraseroJacketEdit *self)
{
	BraseroJacketEditPrivate *priv;
	guint y;

	priv = BRASERO_JACKET_EDIT_PRIVATE (self);

	y = brasero_jacket_view_print (BRASERO_JACKET_VIEW (priv->front), context, 0, 0);
	brasero_jacket_view_print (BRASERO_JACKET_VIEW (priv->back), context, 0, y + 20);
}

static void
brasero_jacket_edit_print_begin (GtkPrintOperation *operation,
				 GtkPrintContext *context,
				 BraseroJacketEdit *self)
{
	gtk_print_operation_set_n_pages (operation, 1);
}

static void
brasero_jacket_edit_print_pressed_cb (GtkButton *button,
				      BraseroJacketEdit *self)
{
	BraseroJacketEditPrivate *priv;
	GtkPrintOperation *print;	

	priv = BRASERO_JACKET_EDIT_PRIVATE (self);
	print = gtk_print_operation_new ();
	g_signal_connect (print,
			  "draw-page",
			  G_CALLBACK (brasero_jacket_edit_print_page),
			  self);
	g_signal_connect (print,
			  "begin-print",
			  G_CALLBACK (brasero_jacket_edit_print_begin),
			  self);
	gtk_print_operation_run (print,
				 GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
				 GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))),
				 NULL);
}

static void
brasero_jacket_edit_underline_pressed_cb (GtkToggleToolButton *button,
					  BraseroJacketEdit *self)
{
	BraseroJacketEditPrivate *priv;
	GtkTextBuffer *buffer;
	GtkTextIter start;
	GtkTextIter end;
	GtkTextTag *tag;

	priv = BRASERO_JACKET_EDIT_PRIVATE (self);

	if (!priv->current_view)
		return;

	buffer = brasero_jacket_view_get_active_buffer (BRASERO_JACKET_VIEW (priv->current_view));
	tag = gtk_text_buffer_create_tag (buffer, NULL,
					  "underline", gtk_toggle_tool_button_get_active (button) ? PANGO_UNDERLINE_SINGLE:PANGO_UNDERLINE_NONE,
					  NULL);

	if (gtk_text_buffer_get_has_selection (buffer)) {
		gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
		gtk_text_buffer_apply_tag (buffer, tag, &start, &end);
	}
	else
		brasero_jacket_buffer_add_default_tag (BRASERO_JACKET_BUFFER (buffer), tag);
}

static void
brasero_jacket_edit_italic_pressed_cb (GtkToggleToolButton *button,
				       BraseroJacketEdit *self)
{
	BraseroJacketEditPrivate *priv;
	GtkTextBuffer *buffer;
	GtkTextIter start;
	GtkTextIter end;
	GtkTextTag *tag;

	priv = BRASERO_JACKET_EDIT_PRIVATE (self);

	if (!priv->current_view)
		return;

	buffer = brasero_jacket_view_get_active_buffer (BRASERO_JACKET_VIEW (priv->current_view));
	tag = gtk_text_buffer_create_tag (buffer, NULL,
					  "style", gtk_toggle_tool_button_get_active (button) ? PANGO_STYLE_ITALIC:PANGO_STYLE_NORMAL,
					  NULL);

	if (gtk_text_buffer_get_has_selection (buffer)) {
		gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
		gtk_text_buffer_apply_tag (buffer, tag, &start, &end);
	}
	else
		brasero_jacket_buffer_add_default_tag (BRASERO_JACKET_BUFFER (buffer), tag);
}

static void
brasero_jacket_edit_bold_pressed_cb (GtkToggleToolButton *button,
				     BraseroJacketEdit *self)
{
	BraseroJacketEditPrivate *priv;
	GtkTextBuffer *buffer;
	GtkTextIter start;
	GtkTextIter end;
	GtkTextTag *tag;

	priv = BRASERO_JACKET_EDIT_PRIVATE (self);

	if (!priv->current_view)
		return;

	buffer = brasero_jacket_view_get_active_buffer (BRASERO_JACKET_VIEW (priv->current_view));
	tag = gtk_text_buffer_create_tag (buffer, NULL,
					  "weight", gtk_toggle_tool_button_get_active (button) ? PANGO_WEIGHT_BOLD:PANGO_WEIGHT_NORMAL,
					  NULL);

	if (gtk_text_buffer_get_has_selection (buffer)) {
		gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
		gtk_text_buffer_apply_tag (buffer, tag, &start, &end);
	}
	else
		brasero_jacket_buffer_add_default_tag (BRASERO_JACKET_BUFFER (buffer), tag);
}

static void
brasero_jacket_edit_center_pressed_cb (GtkToggleToolButton *button,
				       BraseroJacketEdit *self)
{
	BraseroJacketEditPrivate *priv;
	GtkTextBuffer *buffer;
	GtkTextIter start;
	GtkTextIter end;
	GtkTextTag *tag;

	priv = BRASERO_JACKET_EDIT_PRIVATE (self);

	if (!gtk_toggle_tool_button_get_active (button))
		return;

	if (!priv->current_view)
		return;

	buffer = brasero_jacket_view_get_active_buffer (BRASERO_JACKET_VIEW (priv->current_view));
	tag = gtk_text_buffer_create_tag (buffer, NULL,
					  "justification", GTK_JUSTIFY_CENTER,
					  NULL);

	if (!gtk_text_buffer_get_has_selection (buffer)) {
		GtkTextMark *mark;

		mark = gtk_text_buffer_get_insert (buffer);
		gtk_text_buffer_get_iter_at_mark (buffer, &start, mark);
		gtk_text_buffer_get_iter_at_mark (buffer, &end, mark);
		brasero_jacket_buffer_add_default_tag (BRASERO_JACKET_BUFFER (buffer), tag);
	}
	else
		gtk_text_buffer_get_selection_bounds (buffer, &start, &end);


	gtk_text_iter_set_line_index (&start, 0);
	gtk_text_iter_forward_to_line_end (&end);
	gtk_text_buffer_apply_tag (buffer, tag, &start, &end);
}

static void
brasero_jacket_edit_right_pressed_cb (GtkToggleToolButton *button,
				      BraseroJacketEdit *self)
{
	BraseroJacketEditPrivate *priv;
	GtkTextBuffer *buffer;
	GtkTextIter start;
	GtkTextIter end;
	GtkTextTag *tag;

	priv = BRASERO_JACKET_EDIT_PRIVATE (self);
	if (!priv->current_view)
		return;

	if (!gtk_toggle_tool_button_get_active (button))
		return;

	buffer = brasero_jacket_view_get_active_buffer (BRASERO_JACKET_VIEW (priv->current_view));
	tag = gtk_text_buffer_create_tag (buffer, NULL,
					  "justification", GTK_JUSTIFY_RIGHT,
					  NULL);

	if (!gtk_text_buffer_get_has_selection (buffer)) {
		GtkTextMark *mark;

		mark = gtk_text_buffer_get_insert (buffer);
		gtk_text_buffer_get_iter_at_mark (buffer, &start, mark);
		gtk_text_buffer_get_iter_at_mark (buffer, &end, mark);
		brasero_jacket_buffer_add_default_tag (BRASERO_JACKET_BUFFER (buffer), tag);
	}
	else
		gtk_text_buffer_get_selection_bounds (buffer, &start, &end);


	gtk_text_iter_set_line_index (&start, 0);
	gtk_text_iter_forward_to_line_end (&end);
	gtk_text_buffer_apply_tag (buffer, tag, &start, &end);
}

static void
brasero_jacket_edit_left_pressed_cb (GtkToggleToolButton *button,
				     BraseroJacketEdit *self)
{
	BraseroJacketEditPrivate *priv;
	GtkTextBuffer *buffer;
	GtkTextIter start;
	GtkTextIter end;
	GtkTextTag *tag;

	priv = BRASERO_JACKET_EDIT_PRIVATE (self);
	if (!priv->current_view)
		return;

	if (!gtk_toggle_tool_button_get_active (button))
		return;

	buffer = brasero_jacket_view_get_active_buffer (BRASERO_JACKET_VIEW (priv->current_view));
	tag = gtk_text_buffer_create_tag (buffer, NULL,
					  "justification", GTK_JUSTIFY_LEFT,
					  NULL);

	if (!gtk_text_buffer_get_has_selection (buffer)) {
		GtkTextMark *mark;

		mark = gtk_text_buffer_get_insert (buffer);
		gtk_text_buffer_get_iter_at_mark (buffer, &start, mark);
		gtk_text_buffer_get_iter_at_mark (buffer, &end, mark);
		brasero_jacket_buffer_add_default_tag (BRASERO_JACKET_BUFFER (buffer), tag);
	}
	else
		gtk_text_buffer_get_selection_bounds (buffer, &start, &end);

	gtk_text_iter_set_line_index (&start, 0);
	gtk_text_iter_forward_to_line_end (&end);
	gtk_text_buffer_apply_tag (buffer, tag, &start, &end);
}

static void
brasero_jacket_edit_colours_changed_cb (GtkColorButton *button,
					BraseroJacketEdit *self)
{
	BraseroJacketEditPrivate *priv;
	GtkTextBuffer *buffer;
	GtkTextIter start;
	GtkTextIter end;
	GdkColor color;
	GtkTextTag *tag;

	priv = BRASERO_JACKET_EDIT_PRIVATE (self);
	if (!priv->current_view)
		return;

	brasero_tool_color_picker_get_color (BRASERO_TOOL_COLOR_PICKER (button), &color);

	buffer = brasero_jacket_view_get_active_buffer (BRASERO_JACKET_VIEW (priv->current_view));
	tag = gtk_text_buffer_create_tag (buffer, NULL,
					  "foreground-gdk", &color,
					  NULL);
	if (!gtk_text_buffer_get_has_selection (buffer)) {
		brasero_jacket_buffer_add_default_tag (BRASERO_JACKET_BUFFER (buffer), tag);
		return;
	}

	gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
	gtk_text_buffer_apply_tag (buffer, tag, &start, &end);
}

static void
brasero_jacket_edit_font_changed_cb (BraseroJacketFont *button,
				     BraseroJacketEdit *self)
{
	BraseroJacketEditPrivate *priv;
	PangoFontDescription *desc;
	const gchar *font_name;
	GtkTextBuffer *buffer;
	GtkTextIter start;
	GtkTextIter end;
	GtkTextTag *tag;

	priv = BRASERO_JACKET_EDIT_PRIVATE (self);

	if (!priv->current_view)
		return;

	buffer = brasero_jacket_view_get_active_buffer (BRASERO_JACKET_VIEW (priv->current_view));
	if (!buffer)
		return;

	font_name = brasero_jacket_font_get_name (button);
	if (!font_name)
		return;

	desc = pango_font_description_from_string (font_name);
	tag = gtk_text_buffer_create_tag (buffer, NULL,
					  "font-desc", desc,
					  NULL);

	if (gtk_text_buffer_get_has_selection (buffer)) {
		gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
		gtk_text_buffer_apply_tag (buffer, tag, &start, &end);
	}
	else
		brasero_jacket_buffer_add_default_tag (BRASERO_JACKET_BUFFER (buffer), tag);
}

static void
brasero_jacket_edit_configure_background_pressed_cb (GtkToolButton *button,
						     BraseroJacketEdit *self)
{
	BraseroJacketEditPrivate *priv;

	priv = BRASERO_JACKET_EDIT_PRIVATE (self);
	if (priv->current_view)
		brasero_jacket_view_configure_background (BRASERO_JACKET_VIEW (priv->current_view));
}

static void
brasero_jacket_edit_update_button_state (BraseroJacketEdit *self)
{
	gint pos;
	GtkTextIter iter;
	gchar *font_name;
	GtkTextBuffer *buffer;
	GtkTextAttributes *attributes;
	BraseroJacketEditPrivate *priv;

	priv = BRASERO_JACKET_EDIT_PRIVATE (self);

	if (priv->current_view) {
		buffer = brasero_jacket_view_get_active_buffer (BRASERO_JACKET_VIEW (priv->current_view));
		gtk_widget_set_sensitive (priv->background, TRUE);
	}
	else {
		buffer = NULL;
		gtk_widget_set_sensitive (priv->background, FALSE);
	}

	if (!buffer)
		return;

	g_object_get (buffer,
		      "cursor-position", &pos,
		      NULL);

	if (pos)
		gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &iter, pos - 1);
	else
		gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &iter, pos);

	attributes = brasero_jacket_view_get_default_attributes (BRASERO_JACKET_VIEW (priv->current_view));
	gtk_text_iter_get_attributes (&iter, attributes);

	brasero_tool_color_picker_set_color (BRASERO_TOOL_COLOR_PICKER (priv->colours), &attributes->appearance.fg_color);
	
	font_name = pango_font_description_to_string (attributes->font);
	brasero_jacket_font_set_name (BRASERO_JACKET_FONT (priv->fonts), font_name);
	g_free (font_name);

	g_signal_handlers_block_by_func (priv->bold,
					 brasero_jacket_edit_bold_pressed_cb,
					 self);
	gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (priv->bold),
					  (pango_font_description_get_weight (attributes->font) != PANGO_WEIGHT_NORMAL));
	g_signal_handlers_unblock_by_func (priv->bold,
					   brasero_jacket_edit_bold_pressed_cb,
					   self);

	g_signal_handlers_block_by_func (priv->italic,
					 brasero_jacket_edit_italic_pressed_cb,
					 self);
	gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (priv->italic),
					  (pango_font_description_get_style (attributes->font) == PANGO_STYLE_ITALIC));
	g_signal_handlers_unblock_by_func (priv->italic,
					   brasero_jacket_edit_italic_pressed_cb,
					   self);

	g_signal_handlers_block_by_func (priv->underline,
					 brasero_jacket_edit_underline_pressed_cb,
					 self);
	gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (priv->underline),
					  (attributes->appearance.underline != PANGO_UNDERLINE_NONE));
	g_signal_handlers_unblock_by_func (priv->underline,
					   brasero_jacket_edit_underline_pressed_cb,
					   self);

	g_signal_handlers_block_by_func (priv->right,
					 brasero_jacket_edit_right_pressed_cb,
					 self);
	g_signal_handlers_block_by_func (priv->left,
					 brasero_jacket_edit_left_pressed_cb,
					 self);
	g_signal_handlers_block_by_func (priv->center,
					 brasero_jacket_edit_center_pressed_cb,
					 self);
	switch (attributes->justification) {
	case GTK_JUSTIFY_CENTER:
		gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (priv->center), TRUE);
		gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (priv->right), FALSE);
		gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (priv->left), FALSE);
		break;
	case GTK_JUSTIFY_LEFT:
		gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (priv->center), FALSE);
		gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (priv->right), FALSE);
		gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (priv->left), TRUE);
		break;
	case GTK_JUSTIFY_RIGHT:
		gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (priv->center), FALSE);
		gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (priv->right), TRUE);
		gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (priv->left), FALSE);
		break;
	default:
		gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (priv->center), FALSE);
		gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (priv->right), TRUE);
		gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (priv->left), FALSE);
		break;
	};
	g_signal_handlers_unblock_by_func (priv->right,
					   brasero_jacket_edit_right_pressed_cb,
					   self);
	g_signal_handlers_unblock_by_func (priv->left,
					   brasero_jacket_edit_left_pressed_cb,
					   self);
	g_signal_handlers_unblock_by_func (priv->center,
					   brasero_jacket_edit_center_pressed_cb,
					   self);

	gtk_text_attributes_unref (attributes);
}

static void
brasero_jacket_edit_tags_changed_cb (BraseroJacketView *view,
				     BraseroJacketEdit *self)
{
	BraseroJacketEditPrivate *priv;

	priv = BRASERO_JACKET_EDIT_PRIVATE (self);
	priv->current_view = GTK_WIDGET (view);
	brasero_jacket_edit_update_button_state (self);
}

static void
brasero_jacket_edit_init (BraseroJacketEdit *object)
{
	BraseroJacketEditPrivate *priv;
	GtkWidget *main_box;
	GtkWidget *toolbar;
	GtkWidget *scroll;
	GtkWidget *vbox;
	GtkWidget *item;
	GtkWidget *view;

	priv = BRASERO_JACKET_EDIT_PRIVATE (object);

	/* Toolbar */
	toolbar = gtk_toolbar_new ();
	gtk_widget_show (toolbar);
	gtk_box_pack_start (GTK_BOX (object), toolbar, FALSE, TRUE, 0);

	/* Items */
	item = GTK_WIDGET (gtk_tool_button_new_from_stock (GTK_STOCK_PRINT));
	gtk_widget_show (item);
	g_signal_connect (item,
			  "clicked",
			  G_CALLBACK (brasero_jacket_edit_print_pressed_cb),
			  object);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (item), 0);

	item = GTK_WIDGET (gtk_separator_tool_item_new ());
	gtk_widget_show (item);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (item), 0);

	item = GTK_WIDGET (gtk_tool_button_new (NULL, _("Bac_kground Properties")));
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item), "background");
	gtk_tool_button_set_use_underline (GTK_TOOL_BUTTON (item), TRUE);
	gtk_widget_show (item);
	gtk_widget_set_sensitive (item, FALSE);
	g_signal_connect (item,
			  "clicked",
			  G_CALLBACK (brasero_jacket_edit_configure_background_pressed_cb),
			  object);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (item), 0);
	priv->background = item;

	item = GTK_WIDGET (gtk_separator_tool_item_new ());
	gtk_widget_show (item);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (item), 0);

	item = GTK_WIDGET (gtk_radio_tool_button_new_from_stock (NULL, GTK_STOCK_JUSTIFY_RIGHT));
	gtk_widget_show (item);
	g_signal_connect (item,
			  "clicked",
			  G_CALLBACK (brasero_jacket_edit_right_pressed_cb),
			  object);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (item), 0);
	priv->right = item;

	item = GTK_WIDGET (gtk_radio_tool_button_new_from_stock (gtk_radio_tool_button_get_group (GTK_RADIO_TOOL_BUTTON (priv->right)), GTK_STOCK_JUSTIFY_CENTER));
	gtk_widget_show (item);
	g_signal_connect (item,
			  "clicked",
			  G_CALLBACK (brasero_jacket_edit_center_pressed_cb),
			  object);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (item), 0);
	priv->center = item;

	item = GTK_WIDGET (gtk_radio_tool_button_new_from_stock (gtk_radio_tool_button_get_group (GTK_RADIO_TOOL_BUTTON (priv->right)), GTK_STOCK_JUSTIFY_LEFT));
	gtk_widget_show (item);
	g_signal_connect (item,
			  "clicked",
			  G_CALLBACK (brasero_jacket_edit_left_pressed_cb),
			  object);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (item), 0);
	priv->left = item;

	item = GTK_WIDGET (gtk_separator_tool_item_new ());
	gtk_widget_show (item);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (item), 0);

	item = GTK_WIDGET (gtk_toggle_tool_button_new_from_stock (GTK_STOCK_UNDERLINE));
	gtk_widget_show (item);
	g_signal_connect (item,
			  "clicked",
			  G_CALLBACK (brasero_jacket_edit_underline_pressed_cb),
			  object);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (item), 0);
	priv->underline = item;

	item = GTK_WIDGET (gtk_toggle_tool_button_new_from_stock (GTK_STOCK_ITALIC));
	gtk_widget_show (item);
	g_signal_connect (item,
			  "clicked",
			  G_CALLBACK (brasero_jacket_edit_italic_pressed_cb),
			  object);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (item), 0);
	priv->italic = item;

	item = GTK_WIDGET (gtk_toggle_tool_button_new_from_stock (GTK_STOCK_BOLD));
	gtk_widget_show (item);
	g_signal_connect (item,
			  "clicked",
			  G_CALLBACK (brasero_jacket_edit_bold_pressed_cb),
			  object);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (item), 0);
	priv->bold = item;

	item = GTK_WIDGET (gtk_separator_tool_item_new ());
	gtk_widget_show (item);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (item), 0);

	priv->fonts = brasero_jacket_font_new ();
	gtk_widget_show (priv->fonts);
	g_signal_connect (priv->fonts,
			  "font-changed",
			  G_CALLBACK (brasero_jacket_edit_font_changed_cb),
			  object);
	g_signal_connect (priv->fonts,
			  "size-changed",
			  G_CALLBACK (brasero_jacket_edit_font_changed_cb),
			  object);

	item = GTK_WIDGET (gtk_tool_item_new ());
	gtk_widget_show (item);
	gtk_container_add (GTK_CONTAINER (item), priv->fonts);
	gtk_tool_item_set_expand (GTK_TOOL_ITEM (item), FALSE);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (item), 0);

	priv->colours = brasero_tool_color_picker_new ();
	brasero_tool_color_picker_set_text (BRASERO_TOOL_COLOR_PICKER (priv->colours),
					    _("_Text Color"));
	gtk_widget_show (priv->colours);
	g_signal_connect (priv->colours,
			  "color-set",
			  G_CALLBACK (brasero_jacket_edit_colours_changed_cb),
			  object);

	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (priv->colours), 1);

	/* contents */
	vbox = gtk_vbox_new (FALSE, 4);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
	gtk_widget_show (vbox);
	gtk_box_pack_start (GTK_BOX (object), vbox, TRUE, TRUE, 0);

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_widget_show (scroll);
	gtk_box_pack_start (GTK_BOX (vbox), scroll, TRUE, TRUE, 0);

	main_box = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (main_box);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scroll), main_box);

	view = brasero_jacket_view_new ();
	priv->front = view;
	gtk_widget_show (view);
	brasero_jacket_view_set_side (BRASERO_JACKET_VIEW (view), BRASERO_JACKET_FRONT);
	g_signal_connect (view,
			  "tags-changed",
			  G_CALLBACK (brasero_jacket_edit_tags_changed_cb),
			  object);

	gtk_box_pack_start (GTK_BOX (main_box), view, FALSE, FALSE, 0);

	view = brasero_jacket_view_new ();
	priv->back = view;
	gtk_widget_show (view);
	brasero_jacket_view_set_side (BRASERO_JACKET_VIEW (view), BRASERO_JACKET_BACK);

	g_signal_connect (view,
			  "tags-changed",
			  G_CALLBACK (brasero_jacket_edit_tags_changed_cb),
			  object);

	gtk_box_pack_start (GTK_BOX (main_box), view, FALSE, FALSE, 0);

	if (pango_font_description_get_set_fields (priv->front->style->font_desc) & PANGO_FONT_MASK_SIZE) {
		guint size;
		gchar string [8] = { 0, };

		size = pango_font_description_get_size (priv->front->style->font_desc);
		sprintf (string, "%i", size);
		brasero_jacket_font_set_name (BRASERO_JACKET_FONT (priv->fonts), "Sans 12");
	}

	gtk_widget_grab_focus (priv->front);
}

#define BRASERO_JACKET_EDIT_INSERT_TAGGED_TEXT(buffer_MACRO, text_MACRO, tag_MACRO, start_MACRO)	\
	gtk_text_buffer_insert_with_tags_by_name (buffer_MACRO, start_MACRO, text_MACRO, -1, tag_MACRO, NULL);

void
brasero_jacket_edit_set_audio_tracks (BraseroJacketEdit *self,
				      const gchar *label,
				      const gchar *cover,
				      GSList *tracks)
{
	BraseroJacketEditPrivate *priv;
	GtkTextBuffer *buffer;
	GtkTextIter start;
	GSList *iter;

	priv = BRASERO_JACKET_EDIT_PRIVATE (self);

	/* Set background for front cover */
	if (cover) {
		gchar *path;

		path = g_filename_from_uri (cover, NULL, NULL);
		if (!path)
			path = g_strdup (cover);

		brasero_jacket_view_set_image_style (BRASERO_JACKET_VIEW (priv->front), BRASERO_JACKET_IMAGE_STRETCH);
		brasero_jacket_view_set_image (BRASERO_JACKET_VIEW (priv->front), path);
		g_free (path);
	}
	else {
		/* Otherwise we create a very rough one */
		buffer = brasero_jacket_view_get_body_buffer (BRASERO_JACKET_VIEW (priv->front));

		/* create the tags */
		gtk_text_buffer_create_tag (buffer,
					    "Title",
					    "justification", GTK_JUSTIFY_CENTER,
					    "weight", PANGO_WEIGHT_BOLD,
					    "scale", PANGO_SCALE_X_LARGE,
					    "stretch", PANGO_STRETCH_ULTRA_EXPANDED,
					    NULL);

		gtk_text_buffer_get_start_iter (buffer, &start);

		if (label) {
			BRASERO_JACKET_EDIT_INSERT_TAGGED_TEXT (buffer, "\n\n\n\n", "Title", &start);
			BRASERO_JACKET_EDIT_INSERT_TAGGED_TEXT (buffer, label, "Title", &start);
		}
	}

	buffer = brasero_jacket_view_get_body_buffer (BRASERO_JACKET_VIEW (priv->back));

	/* create the tags */
	gtk_text_buffer_create_tag (buffer,
				    "Title",
				    "justification", GTK_JUSTIFY_CENTER,
				    "weight", PANGO_WEIGHT_BOLD,
				    "scale", PANGO_SCALE_LARGE,
				    "stretch", PANGO_STRETCH_ULTRA_EXPANDED,
				    NULL);
	gtk_text_buffer_create_tag (buffer,
				    "Subtitle",
				    "justification", GTK_JUSTIFY_LEFT,
				    "weight", PANGO_WEIGHT_NORMAL,
				    NULL);
	gtk_text_buffer_create_tag (buffer,
				    "Artist",
				    "weight", PANGO_WEIGHT_NORMAL,
				    "justification", GTK_JUSTIFY_LEFT,
				    "stretch", PANGO_STRETCH_NORMAL,
				    "style", PANGO_STYLE_ITALIC,
				    "scale", PANGO_SCALE_SMALL,
				    NULL);

	gtk_text_buffer_get_start_iter (buffer, &start);

	if (label) {
		BRASERO_JACKET_EDIT_INSERT_TAGGED_TEXT (buffer, label, "Title", &start);
		BRASERO_JACKET_EDIT_INSERT_TAGGED_TEXT (buffer, "\n\n", "Title", &start);
	}

	for (iter = tracks; iter; iter = iter->next) {
		gchar *num;
		gchar *time;
		BraseroTrack *track;
		BraseroSongInfo *info;

		track = iter->data;
		if (brasero_track_get_type (track, NULL) != BRASERO_TRACK_TYPE_AUDIO)
			continue;

		num = g_strdup_printf ("%i - ", g_slist_index (tracks, track) + 1);
		BRASERO_JACKET_EDIT_INSERT_TAGGED_TEXT (buffer, num, "Subtitle", &start);
		g_free (num);

		info = brasero_track_get_audio_info (track);

		if (info) {
			if (info->title) {
				BRASERO_JACKET_EDIT_INSERT_TAGGED_TEXT (buffer, info->title, "Subtitle", &start);
			}
			else {
				BRASERO_JACKET_EDIT_INSERT_TAGGED_TEXT (buffer, _("Unknown song"), "Subtitle", &start);
			}

			BRASERO_JACKET_EDIT_INSERT_TAGGED_TEXT (buffer, "\t\t", "Subtitle", &start);

			time = brasero_utils_get_time_string (brasero_track_get_audio_end (track) -
							      brasero_track_get_audio_start (track),
							      TRUE,
							      FALSE);
			BRASERO_JACKET_EDIT_INSERT_TAGGED_TEXT (buffer, time, "Subtitle", &start);
			g_free (time);

			BRASERO_JACKET_EDIT_INSERT_TAGGED_TEXT (buffer, "\n", "Subtitle", &start);

			if (info->artist) {
				/* Translators: "by" is followed by the name of an artist.
				 * This text is the one written on the cover of a disc.
				 * Before it there is the name of the song.
				 * I had to break it because it is in a GtkTextBuffer
				 * and every word has a different tag. */
				BRASERO_JACKET_EDIT_INSERT_TAGGED_TEXT (buffer, _("by"), "Artist", &start);
				BRASERO_JACKET_EDIT_INSERT_TAGGED_TEXT (buffer, " ", "Artist", &start);
				BRASERO_JACKET_EDIT_INSERT_TAGGED_TEXT (buffer, info->artist, "Artist", &start);
				BRASERO_JACKET_EDIT_INSERT_TAGGED_TEXT (buffer, " ", "Artist", &start);
			}

			if (info->composer)
				BRASERO_JACKET_EDIT_INSERT_TAGGED_TEXT (buffer, info->composer, "Subtitle", &start);

			BRASERO_JACKET_EDIT_INSERT_TAGGED_TEXT (buffer, "\n\n", "Subtitle", &start);
		}
	}

	/* side */
	buffer = brasero_jacket_view_get_side_buffer (BRASERO_JACKET_VIEW (priv->back));

	/* create the tags */
	gtk_text_buffer_create_tag (buffer,
				    "Title",
				    "justification", GTK_JUSTIFY_CENTER,
				    "weight", PANGO_WEIGHT_BOLD,
				    "stretch", PANGO_STRETCH_ULTRA_EXPANDED,
				    NULL);

	gtk_text_buffer_get_start_iter (buffer, &start);

	if (label) {
		BRASERO_JACKET_EDIT_INSERT_TAGGED_TEXT (buffer, label, "Title", &start);
	}
}

static void
brasero_jacket_edit_finalize (GObject *object)
{
	G_OBJECT_CLASS (brasero_jacket_edit_parent_class)->finalize (object);
}

static void
brasero_jacket_edit_class_init (BraseroJacketEditClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroJacketEditPrivate));

	object_class->finalize = brasero_jacket_edit_finalize;
}

GtkWidget *
brasero_jacket_edit_new (void)
{
	return g_object_new (BRASERO_TYPE_JACKET_EDIT, NULL);
}

GtkWidget *
brasero_jacket_edit_dialog_new (GtkWidget *toplevel,
				GtkWidget **dialog)
{
	GtkWidget *window;
	GtkWidget *contents;

	window = gtk_dialog_new_with_buttons (_("Cover Editor"),
					      GTK_WINDOW (toplevel),
					      GTK_DIALOG_MODAL|
					      GTK_DIALOG_DESTROY_WITH_PARENT|
					      GTK_DIALOG_NO_SEPARATOR,
					      GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
					      NULL);

	gtk_window_set_default_size (GTK_WINDOW (window), 680, 640);
	gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER_ON_PARENT);
	g_signal_connect (window,
			  "response",
			  G_CALLBACK (gtk_widget_destroy),
			  NULL);

	contents = brasero_jacket_edit_new ();
	gtk_widget_show (contents);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), contents, TRUE, TRUE, 0);
	gtk_widget_show (window);

	if (dialog)
		*dialog = window;

	return contents;
}

