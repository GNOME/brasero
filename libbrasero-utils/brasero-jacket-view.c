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

#include <cairo.h>

#include "brasero-jacket-view.h"
#include "brasero-jacket-buffer.h"

#include "brasero-misc.h"

typedef struct _BraseroJacketViewPrivate BraseroJacketViewPrivate;
struct _BraseroJacketViewPrivate
{
	BraseroJacketSide side;

	GtkWidget *edit;
	GtkWidget *sides;

	GdkColor b_color;
	GdkColor b_color2;
	BraseroJacketColorStyle color_style;

	cairo_pattern_t *pattern;

	GdkPixbuf *image;
	GdkPixbuf *scaled;

	gchar *image_path;
	BraseroJacketImageStyle image_style;
};

#define BRASERO_JACKET_VIEW_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_JACKET_VIEW, BraseroJacketViewPrivate))

enum
{
	PRINTED,
	TAGS_CHANGED,
	LAST_SIGNAL
};


static guint jacket_view_signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (BraseroJacketView, brasero_jacket_view, GTK_TYPE_CONTAINER);

#define BRASERO_JACKET_VIEW_MARGIN		20


static GSList *
brasero_jacket_view_tag_begins (GtkTextIter *iter,
				GtkTextAttributes *attributes)
{
	PangoAttribute *attr;
	GSList *open_attr = NULL;

	attr = pango_attr_foreground_new (attributes->appearance.fg_color.red,
					  attributes->appearance.fg_color.green,
					  attributes->appearance.fg_color.blue);
	attr->start_index = gtk_text_iter_get_visible_line_index (iter);
	open_attr = g_slist_prepend (open_attr, attr);

	attr = pango_attr_font_desc_new (attributes->font);
	attr->start_index = gtk_text_iter_get_visible_line_index (iter);
	open_attr = g_slist_prepend (open_attr, attr);

	attr = pango_attr_underline_new (attributes->appearance.underline);
	attr->start_index = gtk_text_iter_get_visible_line_index (iter);
	open_attr = g_slist_prepend (open_attr, attr);

	return open_attr;
}

static void
brasero_jacket_view_tag_ends (GtkTextIter *iter,
			      PangoAttrList *attributes,
			      GSList *open_attr)
{
	GSList *list;

	for (list = open_attr; list; list = list->next) {
		PangoAttribute *attr;

		attr = list->data;
		attr->end_index = gtk_text_iter_get_visible_line_index (iter);
		pango_attr_list_insert (attributes, attr);
	}
}

static void
brasero_jacket_view_set_line_attributes (GtkTextView *view,
					 PangoLayout *layout,
					 guint line_num)
{
	PangoAttrList *attributes = NULL;
	GtkTextAttributes *text_attr;
	GSList *open_attr = NULL;
	PangoAlignment alignment;
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	GtkTextIter end;

	attributes = pango_attr_list_new ();

	buffer = gtk_text_view_get_buffer (view);
	gtk_text_buffer_get_iter_at_line (buffer, &iter, line_num);

	text_attr = gtk_text_view_get_default_attributes (view);
	gtk_text_iter_get_attributes (&iter, text_attr);

	switch (text_attr->justification) {
	case GTK_JUSTIFY_CENTER:
		alignment = PANGO_ALIGN_CENTER;
		break;
	case GTK_JUSTIFY_LEFT:
		alignment = PANGO_ALIGN_LEFT;
		break;
	case GTK_JUSTIFY_RIGHT:
		alignment = PANGO_ALIGN_RIGHT;
		break;
	default:
		alignment = PANGO_ALIGN_LEFT;
		break;
	};

	open_attr = brasero_jacket_view_tag_begins (&iter, text_attr);
	gtk_text_attributes_unref (text_attr);

	while (gtk_text_iter_forward_to_tag_toggle (&iter, NULL) &&
	       gtk_text_iter_get_line (&iter) == line_num &&
	      !gtk_text_iter_is_end (&iter)) {

		brasero_jacket_view_tag_ends (&iter, attributes, open_attr);
		g_slist_free (open_attr);

		text_attr = gtk_text_view_get_default_attributes (view);
		gtk_text_iter_get_attributes (&iter, text_attr);

		switch (text_attr->justification) {
		case GTK_JUSTIFY_CENTER:
			alignment = PANGO_ALIGN_CENTER;
			break;
		case GTK_JUSTIFY_LEFT:
			alignment = PANGO_ALIGN_LEFT;
			break;
		case GTK_JUSTIFY_RIGHT:
			alignment = PANGO_ALIGN_RIGHT;
			break;
		default:
			alignment = PANGO_ALIGN_LEFT;
			break;
		};
		open_attr = brasero_jacket_view_tag_begins (&iter, text_attr);
		gtk_text_attributes_unref (text_attr);
	}

	/* Safer to do this in case one tag finishes on next line */
	gtk_text_buffer_get_iter_at_line (buffer, &end, line_num);
	gtk_text_iter_forward_to_line_end (&end);

	/* go through all still opened attributes */
	brasero_jacket_view_tag_ends (&end, attributes, open_attr);
	g_slist_free (open_attr);

	pango_layout_set_attributes (layout, attributes);
	pango_attr_list_unref (attributes);

	pango_layout_set_alignment (layout, alignment);
}

static void
brasero_jacket_view_render_side_text (BraseroJacketView *self,
				      cairo_t *ctx,
				      PangoLayout *layout,
				      gdouble resolution,
				      guint x,
				      guint y)
{
	guint y_left;
	guint y_right;
	guint width;
	guint x_left;
	guint x_right;
	guint line_num;
	guint line_max;
	GtkTextBuffer *buffer;
	BraseroJacketViewPrivate *priv;

	priv = BRASERO_JACKET_VIEW_PRIVATE (self);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->sides));
	line_max = gtk_text_buffer_get_line_count (buffer);

	width = resolution * COVER_HEIGHT_SIDE_INCH;
	x_left = x;
	y_left = y + COVER_HEIGHT_SIDE_INCH * resolution;

	x_right = x + COVER_WIDTH_BACK_INCH * resolution;
	y_right = y;

	for (line_num = 0; line_num < line_max; line_num ++) {
		gchar *text;
		PangoRectangle rect;
		GtkTextIter start, end;

		cairo_set_source_rgb (ctx, 0.0, 0.0, 0.0);

		gtk_text_buffer_get_iter_at_line (buffer, &start, line_num);
		gtk_text_buffer_get_iter_at_line (buffer, &end, line_num);
		gtk_text_iter_forward_to_line_end (&end);

		text = brasero_jacket_buffer_get_text (BRASERO_JACKET_BUFFER (buffer), &start, &end, FALSE, FALSE);
		if (text && text [0] != '\0' && text [0] != '\n') {
			pango_layout_set_text (layout, text, -1);
			g_free (text);
		}
		else
			pango_layout_set_text (layout, " ", -1);

		pango_layout_set_width (layout, width * PANGO_SCALE);
		pango_layout_set_wrap (layout, PANGO_WRAP_CHAR);
		brasero_jacket_view_set_line_attributes (GTK_TEXT_VIEW (priv->sides), layout, line_num);

		pango_layout_get_pixel_extents (layout, NULL, &rect);

		cairo_save (ctx);

		cairo_move_to (ctx, x_left, y_left);
		pango_cairo_update_layout (ctx, layout);
		cairo_rotate (ctx, - G_PI_2);
		pango_cairo_show_layout (ctx, layout);

		cairo_restore (ctx);

		cairo_save (ctx);

		cairo_move_to (ctx, x_right, y_right);
		pango_cairo_update_layout (ctx, layout);
		cairo_rotate (ctx, G_PI_2);
		pango_cairo_show_layout (ctx, layout);

		cairo_restore (ctx);

		x_right -= rect.height;
		x_left += rect.height;
	}
}

static void
brasero_jacket_view_render (BraseroJacketView *self,
			    cairo_t *ctx,
			    PangoLayout *layout,
			    gdouble resolution_x,
			    gdouble resolution_y,
			    guint x,
			    guint y,
			    GdkRectangle *area,
			    gboolean render_if_empty)
{
	BraseroJacketViewPrivate *priv;
	gint height, width;

	priv = BRASERO_JACKET_VIEW_PRIVATE (self);

	if (priv->side == BRASERO_JACKET_BACK) {
		width = COVER_WIDTH_BACK_INCH * resolution_x;
		height = COVER_HEIGHT_BACK_INCH * resolution_y;
	}
	else {
		width = COVER_WIDTH_FRONT_INCH * resolution_x;
		height = COVER_HEIGHT_FRONT_INCH * resolution_y;
	}

	/* set clip */
	cairo_reset_clip (ctx);
	cairo_rectangle (ctx, area->x, area->y, area->width, area->height);
	cairo_clip (ctx);

	/* draw white surroundings */
	cairo_set_source_rgb (ctx, 1.0, 1.0, 1.0);
	cairo_paint (ctx);

	/* draw background */
	cairo_rectangle (ctx, x, y, width, height);
	cairo_clip (ctx);

	if (priv->pattern) {
		cairo_set_source (ctx, priv->pattern);
		cairo_paint (ctx);
	}

	if (priv->scaled) {
		if (priv->image_style == BRASERO_JACKET_IMAGE_CENTER)
			gdk_cairo_set_source_pixbuf (ctx,
						     priv->scaled,
						     x + (width - gdk_pixbuf_get_width (priv->scaled))/ 2.0,
						     y + (height - gdk_pixbuf_get_height (priv->scaled)) / 2.0);
		else
			gdk_cairo_set_source_pixbuf (ctx, priv->scaled, x, y);

		if (priv->image_style == BRASERO_JACKET_IMAGE_TILE) {
			cairo_pattern_t *pattern;

			pattern = cairo_get_source (ctx);
			cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);
		}

		cairo_paint (ctx);
	}

	if (priv->side == BRASERO_JACKET_BACK) {
		cairo_save (ctx);

		/* Draw the rectangle */
		cairo_set_antialias (ctx, CAIRO_ANTIALIAS_DEFAULT);
		cairo_set_source_rgb (ctx, 0.5, 0.5, 0.5);
		cairo_set_line_width (ctx, 0.5);
		cairo_set_line_cap (ctx, CAIRO_LINE_CAP_ROUND);

		cairo_move_to (ctx,
			       x + COVER_WIDTH_SIDE_INCH * resolution_x,
			       y);
		cairo_line_to (ctx,
			       x + COVER_WIDTH_SIDE_INCH * resolution_x,
			       y + (COVER_HEIGHT_SIDE_INCH * resolution_y));

		cairo_move_to (ctx,
			       x + (COVER_WIDTH_BACK_INCH - COVER_WIDTH_SIDE_INCH) * resolution_x,
			       y);
		cairo_line_to (ctx,
			       x + (COVER_WIDTH_BACK_INCH - COVER_WIDTH_SIDE_INCH) * resolution_x,
			       y + (COVER_HEIGHT_SIDE_INCH * resolution_y));

		cairo_stroke (ctx);

		cairo_restore (ctx);
		cairo_save (ctx);

		/* also render text in the sides */
		brasero_jacket_view_render_side_text (self,
						      ctx,
						      layout,
						      resolution_y,
						      x,
						      y);

		cairo_restore (ctx);
	}

	/* Draw the rectangle */
	cairo_set_source_rgb (ctx, 0.5, 0.5, 0.5);
	cairo_set_line_width (ctx, 0.5);
	cairo_set_line_cap (ctx, CAIRO_LINE_CAP_ROUND);

	cairo_rectangle (ctx,
			 x,
			 y,
			 width,
			 height);
	cairo_stroke (ctx);
}

static void
brasero_jacket_view_render_body (BraseroJacketView *self,
				 cairo_t *ctx,
				 gdouble resolution_x,
				 gdouble resolution_y,
				 guint x,
				 guint y,
				 gboolean render_if_empty)
{
	guint width;
	gint line_max;
	gint line_num = 0;
	PangoLayout *layout;
	GtkTextBuffer *buffer;
	BraseroJacketViewPrivate *priv;

	priv = BRASERO_JACKET_VIEW_PRIVATE (self);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->edit));
	line_max = gtk_text_buffer_get_line_count (buffer);

	if (priv->side == BRASERO_JACKET_BACK)
		width = ((COVER_WIDTH_BACK_INCH - COVER_WIDTH_SIDE_INCH * 2) * resolution_x - COVER_TEXT_MARGIN * resolution_x * 2) * PANGO_SCALE;
	else
		width = (COVER_WIDTH_FRONT_INCH * resolution_x - COVER_TEXT_MARGIN * resolution_x * 2) * PANGO_SCALE;

	for (line_num = 0; line_num < line_max; line_num ++) {
		gchar *text;
		PangoRectangle rect;
		PangoContext *context;
		GtkTextIter start, end;

		cairo_set_source_rgb (ctx, 0.0, 0.0, 0.0);
		layout = pango_cairo_create_layout (ctx);

		context = pango_layout_get_context (layout);
		pango_cairo_context_set_resolution (context, resolution_x);

		gtk_text_buffer_get_iter_at_line (buffer, &start, line_num);
		gtk_text_buffer_get_iter_at_line (buffer, &end, line_num);
		gtk_text_iter_forward_to_line_end (&end);

		text = brasero_jacket_buffer_get_text (BRASERO_JACKET_BUFFER (buffer), &start, &end, FALSE, render_if_empty);
		if (text && text [0] != '\0' && text [0] != '\n') {
			pango_layout_set_text (layout, text, -1);
			g_free (text);
		}
		else
			pango_layout_set_text (layout, " ", -1);

		pango_layout_set_width (layout, width);
		pango_layout_set_wrap (layout, PANGO_WRAP_CHAR);
		brasero_jacket_view_set_line_attributes (GTK_TEXT_VIEW (priv->edit), layout, line_num);

		if (priv->side == BRASERO_JACKET_BACK)
			cairo_move_to (ctx,
				       x + COVER_WIDTH_SIDE_INCH * resolution_x + COVER_TEXT_MARGIN * resolution_x,
				       y + COVER_TEXT_MARGIN * resolution_y);
		else
			cairo_move_to (ctx,
				       x + COVER_TEXT_MARGIN * resolution_x,
				       y + COVER_TEXT_MARGIN * resolution_y);
		pango_cairo_show_layout (ctx, layout);

		pango_layout_get_pixel_extents (layout, NULL, &rect);
		y += rect.height;

		g_object_unref (layout);
	}
}

guint
brasero_jacket_view_print (BraseroJacketView *self,
			   GtkPrintContext *context,
			   guint x,
			   guint y)
{
	cairo_t *ctx;
	GdkRectangle rect;
	PangoLayout *layout;
	gdouble resolution_x;
	gdouble resolution_y;
	BraseroJacketViewPrivate *priv;

	priv = BRASERO_JACKET_VIEW_PRIVATE (self);

	ctx = gtk_print_context_get_cairo_context (context);

	/* set clip */
	resolution_x = gtk_print_context_get_dpi_x (context);
	resolution_y = gtk_print_context_get_dpi_y (context);
	rect.x = x;
	rect.y = y;

	if (priv->side == BRASERO_JACKET_BACK) {
		rect.width = resolution_x * COVER_WIDTH_BACK_INCH;
		rect.height = resolution_y * COVER_HEIGHT_BACK_INCH;
	}
	else {
		rect.width = resolution_x * COVER_WIDTH_FRONT_INCH;
		rect.height = resolution_y * COVER_HEIGHT_FRONT_INCH;
	}

	layout = gtk_print_context_create_pango_layout (context);
	brasero_jacket_view_render (self,
				    ctx,
				    layout,
				    resolution_x,
				    resolution_y,
				    x,
				    y,
				    &rect,
				    FALSE);

	/* Now let's render the text in main buffer */
	brasero_jacket_view_render_body (self,
					 ctx,
					 resolution_x,
					 resolution_y,
					 x,
					 y,
					 FALSE);

	g_object_unref (layout);

	return rect.height;
}

cairo_surface_t *
brasero_jacket_view_snapshot (BraseroJacketView *self)
{
	BraseroJacketViewPrivate *priv;
	cairo_surface_t *surface;
	PangoLayout *layout;
	GtkWidget *toplevel;
	gdouble resolution;
	GdkRectangle area;
	cairo_t *ctx;
	guint height;
	guint width;

	priv = BRASERO_JACKET_VIEW_PRIVATE (self);

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
	if (!GTK_IS_WINDOW (toplevel))
		return NULL;

	resolution = gdk_screen_get_resolution (gtk_window_get_screen (GTK_WINDOW (toplevel)));

	layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), NULL);

	if (priv->side == BRASERO_JACKET_BACK) {
		width = resolution * COVER_WIDTH_BACK_INCH + 1;
		height = resolution * COVER_HEIGHT_BACK_INCH + 1;
	}
	else {
		width = resolution * COVER_WIDTH_FRONT_INCH + 1;
		height = resolution * COVER_HEIGHT_FRONT_INCH + 1;
	}

	surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
					      width,
					      height);
	ctx = cairo_create (surface);

	area = GTK_WIDGET (self)->allocation;
	area.x = 0;
	area.y = 0;
	brasero_jacket_view_render (self,
				    ctx,
				    layout,
				    resolution,
				    resolution,
				    0,
				    0,
				    &area,
				    FALSE);

	/* Now let's render the text in main buffer */
	brasero_jacket_view_render_body (self,
					 ctx,
					 resolution,
					 resolution,
					 0,
					 0,
					 FALSE);

	g_object_unref (layout);
	cairo_destroy (ctx);
	return surface;
}

static void
brasero_jacket_view_cursor_position_changed_cb (GObject *buffer,
						GParamSpec *spec,
						BraseroJacketView *self)
{
	BraseroJacketViewPrivate *priv;

	priv = BRASERO_JACKET_VIEW_PRIVATE (self);
	g_signal_emit (self,
		       jacket_view_signals [TAGS_CHANGED],
		       0);
}

static void
brasero_jacket_view_apply_tag (GtkTextBuffer *buffer,
			       GtkTextTag *tag,
			       GtkTextIter *start,
			       GtkTextIter *end,
			       BraseroJacketView *self)
{
	g_signal_emit (self,
		       jacket_view_signals [TAGS_CHANGED],
		       0);
	gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
brasero_jacket_view_side_buffer_changed (GtkTextBuffer *buffer,
					 BraseroJacketView *self)
{
	gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
brasero_jacket_view_focus_in_cb (GtkWidget *view,
				 GdkEventFocus *event,
				 BraseroJacketView *self)
{
	GtkTextView *text_view = GTK_TEXT_VIEW (view);
	GtkTextBuffer *buffer;

	if (text_view->editable) {
		text_view->need_im_reset = TRUE;
		gtk_im_context_focus_in (text_view->im_context);
	}

	buffer = gtk_text_view_get_buffer (text_view);
	brasero_jacket_buffer_show_default_text (BRASERO_JACKET_BUFFER (buffer), FALSE);

	g_signal_emit (self,
		       jacket_view_signals [TAGS_CHANGED],
		       0);
}

static void
brasero_jacket_view_focus_out_cb (GtkWidget *view,
				  GdkEventFocus *event,
				  BraseroJacketView *self)
{
	GtkTextView *text_view = GTK_TEXT_VIEW (view);
	GtkTextBuffer *buffer;

	if (text_view->editable) {
		text_view->need_im_reset = TRUE;
		gtk_im_context_focus_out (text_view->im_context);
	}

	buffer = gtk_text_view_get_buffer (text_view);
	brasero_jacket_buffer_show_default_text (BRASERO_JACKET_BUFFER (buffer), TRUE);

	g_signal_emit (self,
		       jacket_view_signals [TAGS_CHANGED],
		       0);
}

static void
brasero_jacket_view_scrolled_cb (GtkAdjustment *adj,
				 GtkTextView *view)
{
	gint trailing;
	GtkTextIter end;
	GtkTextIter start;
	GdkRectangle rect;
	GtkTextBuffer *buffer;

	if (gtk_adjustment_get_value (adj) == 0.0)
		return;

	g_signal_stop_emission_by_name (adj, "value-changed");

	buffer = gtk_text_view_get_buffer (view);

	gtk_text_buffer_get_end_iter (buffer, &end);

	gtk_text_view_get_visible_rect (view, &rect);
	gtk_text_view_get_iter_at_position (view, &start, &trailing, rect.x + rect.width, rect.y + rect.height - gtk_adjustment_get_value (adj));
	gtk_text_buffer_delete (buffer, &start, &end);

	gtk_adjustment_set_value (adj, 0.0);
}

void
brasero_jacket_view_configure_background (BraseroJacketView *self)
{
	BraseroJacketImageStyle image_style;
	BraseroJacketColorStyle color_style;
	BraseroJacketViewPrivate *priv;
	GtkWidget *dialog;
	GdkColor color2;
	GdkColor color;
	gchar *path;

	priv = BRASERO_JACKET_VIEW_PRIVATE (self);

	dialog = brasero_jacket_background_new ();

	brasero_jacket_background_set_image_path (BRASERO_JACKET_BACKGROUND (dialog), priv->image_path);
	brasero_jacket_background_set_image_style (BRASERO_JACKET_BACKGROUND (dialog), priv->image_style);
	brasero_jacket_background_set_color (BRASERO_JACKET_BACKGROUND (dialog),
					     &priv->b_color,
					     &priv->b_color2);
	brasero_jacket_background_set_color_style (BRASERO_JACKET_BACKGROUND (dialog), priv->color_style);

	gtk_dialog_run (GTK_DIALOG (dialog));

	image_style = brasero_jacket_background_get_image_style (BRASERO_JACKET_BACKGROUND (dialog));
	path = brasero_jacket_background_get_image_path (BRASERO_JACKET_BACKGROUND (dialog));
	brasero_jacket_view_set_image_style (self, image_style);
	brasero_jacket_view_set_image (self, path);
	g_free (path);

	brasero_jacket_background_get_color (BRASERO_JACKET_BACKGROUND (dialog), &color, &color2);
	brasero_jacket_view_set_color_background (self, &color, &color2);

	color_style = brasero_jacket_background_get_color_style (BRASERO_JACKET_BACKGROUND (dialog));
	brasero_jacket_view_set_color_style (self, color_style);

	gtk_widget_destroy (dialog);
}

static void
brasero_jacket_view_change_image_activated_cb (GtkMenuItem *item,
					       BraseroJacketView *self)
{
	brasero_jacket_view_configure_background (self);
}

static void
brasero_jacket_view_populate_popup_cb (GtkTextView *view,
				       GtkMenu *menu,
				       BraseroJacketView *self)
{
	GtkWidget *item;

	item = gtk_separator_menu_item_new ();
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	item = gtk_menu_item_new_with_mnemonic (_("Set Bac_kground Properties"));
	gtk_widget_show (item);
	g_signal_connect (item,
			  "activate",
			  G_CALLBACK (brasero_jacket_view_change_image_activated_cb),
			  self);

	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
}

void
brasero_jacket_view_set_side (BraseroJacketView *self,
			      BraseroJacketSide side)
{
	BraseroJacketViewPrivate *priv;
	GtkTextBuffer *buffer;

	priv = BRASERO_JACKET_VIEW_PRIVATE (self);

	priv->side = side;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->edit));

	if (priv->side == BRASERO_JACKET_BACK) {
		GtkTextBuffer *sides_buffer;
		GtkObject *vadj;
		GtkObject *hadj;

		sides_buffer = GTK_TEXT_BUFFER (brasero_jacket_buffer_new ());
		g_signal_connect (sides_buffer,
				  "changed",
				  G_CALLBACK (brasero_jacket_view_side_buffer_changed),
				  self);
		g_signal_connect (sides_buffer,
				  "notify::cursor-position",
				  G_CALLBACK (brasero_jacket_view_cursor_position_changed_cb),
				  self);
		g_signal_connect_after (sides_buffer,
					"apply-tag",
					G_CALLBACK (brasero_jacket_view_apply_tag),
					self);
		brasero_jacket_buffer_set_default_text (BRASERO_JACKET_BUFFER (sides_buffer), _("SIDES"));

		priv->sides = gtk_text_view_new_with_buffer (sides_buffer);
		gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (priv->sides), GTK_WRAP_CHAR);
		gtk_widget_set_parent (priv->sides, GTK_WIDGET (self));
		gtk_widget_show (priv->sides);

		g_signal_connect (priv->sides,
				  "focus-in-event",
				  G_CALLBACK (brasero_jacket_view_focus_in_cb),
				  self);
		g_signal_connect_after (priv->sides,
					"focus-out-event",
					G_CALLBACK (brasero_jacket_view_focus_out_cb),
					self);

		brasero_jacket_buffer_set_default_text (BRASERO_JACKET_BUFFER (buffer), _("BACK COVER"));

		hadj = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
		vadj = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

		g_signal_connect (hadj,
				  "value-changed",
				  G_CALLBACK (brasero_jacket_view_scrolled_cb),
				  priv->sides);
		g_signal_connect (vadj,
				  "value-changed",
				  G_CALLBACK (brasero_jacket_view_scrolled_cb),
				  priv->sides);

		gtk_widget_set_scroll_adjustments (priv->sides, GTK_ADJUSTMENT (hadj), GTK_ADJUSTMENT (vadj));
	}
	else
		brasero_jacket_buffer_set_default_text (BRASERO_JACKET_BUFFER (buffer), _("FRONT COVER"));
}

static void
brasero_jacket_view_update_edit_image (BraseroJacketView *self)
{
	cairo_t *ctx;
	guint resolution;
	GdkWindow *window;
	GdkPixmap *pixmap;
	GtkWidget *toplevel;
	guint width, height, x, y;
	BraseroJacketViewPrivate *priv;

	priv = BRASERO_JACKET_VIEW_PRIVATE (self);

	if (!priv->pattern && !priv->scaled)
		return;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
	if (!GTK_IS_WINDOW (toplevel))
		return;

	resolution = gdk_screen_get_resolution (gtk_window_get_screen (GTK_WINDOW (toplevel)));
	window = gtk_text_view_get_window (GTK_TEXT_VIEW (priv->edit), GTK_TEXT_WINDOW_TEXT);

	if (!window)
		return;

	x = COVER_TEXT_MARGIN * resolution;
	y = COVER_TEXT_MARGIN * resolution;
	width = priv->edit->allocation.width;
	height = priv->edit->allocation.height;

	if (priv->side == BRASERO_JACKET_BACK)
		x += COVER_WIDTH_SIDE_INCH * resolution;

	pixmap = gdk_pixmap_new (GDK_DRAWABLE (window),
				 width,
				 height,
				 -1);

	ctx = gdk_cairo_create (GDK_DRAWABLE (pixmap));

	if (priv->pattern) {
		cairo_rectangle (ctx,
				 0,
				 0,
				 width,
				 height);
		cairo_clip (ctx);

		cairo_set_source (ctx, priv->pattern);
		cairo_paint (ctx);
	}
	else {
		GdkGC *gc;

		gc = gdk_gc_new (GDK_DRAWABLE (pixmap));
		gdk_gc_set_fill (gc, GDK_SOLID);
		gdk_gc_set_rgb_fg_color (gc, &priv->edit->style->bg [0]);
		gdk_gc_set_rgb_bg_color (gc, &priv->edit->style->bg [0]);
		gdk_draw_rectangle (GDK_DRAWABLE (pixmap),
				    gc,
				    TRUE,
				    0,
				    0,
				    width,
				    height);
		g_object_unref (gc);
	}

	if (priv->scaled) {
		if (priv->image_style == BRASERO_JACKET_IMAGE_CENTER) {
			if (width < gdk_pixbuf_get_width (priv->scaled))
				gdk_draw_pixbuf (GDK_DRAWABLE (pixmap),
						 NULL,
						 priv->scaled,
						(gdk_pixbuf_get_width (priv->scaled) - width) / 2,
						(gdk_pixbuf_get_height (priv->scaled) - height) / 2,
						 0, 0,
						 width,
						 height,
						 GDK_RGB_DITHER_NORMAL,
						 -1,
						 -1);
			else
				gdk_draw_pixbuf (GDK_DRAWABLE (pixmap),
						 NULL,
						 priv->scaled,
						 0, 0,
						 (width - gdk_pixbuf_get_width (priv->scaled)) / 2,
						 (height - gdk_pixbuf_get_height (priv->scaled)) / 2,
						 -1,
						 -1,
						 GDK_RGB_DITHER_NORMAL,
						 -1,
						 -1);
		}
		else if (priv->image_style == BRASERO_JACKET_IMAGE_TILE) {
			cairo_pattern_t *pattern;

			gdk_cairo_set_source_pixbuf (ctx, priv->scaled, -x, -y);
			pattern = cairo_get_source (ctx);
			cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);
			cairo_paint (ctx);
		}
		else
			gdk_draw_pixbuf (GDK_DRAWABLE (pixmap),
					 NULL,
					 priv->scaled,
					 x,
					 y,
					 0, 0,
					 width,
					 height,
					 GDK_RGB_DITHER_NORMAL,
					 -1,
					 -1);
	}

	cairo_destroy (ctx);

	gdk_window_set_back_pixmap (window, pixmap, FALSE);
	g_object_unref (pixmap);
}

static void
brasero_jacket_view_update_image (BraseroJacketView *self)
{
	BraseroJacketViewPrivate *priv;

	priv = BRASERO_JACKET_VIEW_PRIVATE (self);

	if (!priv->image)
		return;

	if (priv->image_style == BRASERO_JACKET_IMAGE_CENTER) {
		g_object_ref (priv->image);
		priv->scaled = priv->image;
	}
	else if (priv->image_style == BRASERO_JACKET_IMAGE_TILE) {
		g_object_ref (priv->image);
		priv->scaled = priv->image;		
	}
	else if (priv->image_style == BRASERO_JACKET_IMAGE_STRETCH) {
		guint width;
		guint height;
		guint resolution;
		GtkWidget *toplevel;

		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
		if (!GTK_IS_WINDOW (toplevel))
			return;

		resolution = gdk_screen_get_resolution (gtk_window_get_screen (GTK_WINDOW (toplevel)));

		if (priv->side == BRASERO_JACKET_BACK) {
			height = resolution * COVER_HEIGHT_BACK_INCH;
			width = resolution * COVER_WIDTH_BACK_INCH;
		}
		else {
			height = resolution * COVER_HEIGHT_FRONT_INCH;
			width = resolution * COVER_WIDTH_FRONT_INCH;
		}

		priv->scaled = gdk_pixbuf_scale_simple (priv->image,
							width,
							height,
							GDK_INTERP_HYPER);
	}

	brasero_jacket_view_update_edit_image (self);
	gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
brasero_jacket_view_set_image_style (BraseroJacketView *self,
				     BraseroJacketImageStyle style)
{
	BraseroJacketViewPrivate *priv;

	priv = BRASERO_JACKET_VIEW_PRIVATE (self);

	if (priv->scaled) {
		g_object_unref (priv->scaled);
		priv->scaled = NULL;
	}

	priv->image_style = style;
	brasero_jacket_view_update_image (self);
}

static void
brasero_jacket_view_update_color (BraseroJacketView *self)
{
	guint resolution;
	GtkWidget *toplevel;
	guint width, height;
	cairo_pattern_t *pattern;
	BraseroJacketViewPrivate *priv;

	priv = BRASERO_JACKET_VIEW_PRIVATE (self);

	if (priv->pattern) {
		cairo_pattern_destroy (priv->pattern);
		priv->pattern = NULL;
	}

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
	if (!GTK_IS_WINDOW (toplevel))
		return;

	resolution = gdk_screen_get_resolution (gtk_window_get_screen (GTK_WINDOW (toplevel)));
	if (priv->side == BRASERO_JACKET_BACK) {
		height = resolution * COVER_HEIGHT_BACK_INCH;
		width = resolution * COVER_WIDTH_BACK_INCH;
	}
	else {
		height = resolution * COVER_HEIGHT_FRONT_INCH;
		width = resolution * COVER_WIDTH_FRONT_INCH;
	}

	if (priv->color_style == BRASERO_JACKET_COLOR_SOLID) {
		pattern = cairo_pattern_create_rgb (priv->b_color.red/G_MAXINT16,
						    priv->b_color.green/G_MAXINT16,
						    priv->b_color.blue/G_MAXINT16);
	}
	else {
		if (priv->color_style == BRASERO_JACKET_COLOR_HGRADIENT)
			pattern = cairo_pattern_create_linear (0.0,
							       0.0,
							       width,
							       0.0);
		else /* if (priv->color_style == BRASERO_JACKET_COLOR_VGRADIENT) */
			pattern = cairo_pattern_create_linear (0.0,
							       0.0,
							       0.0,
							       height);

		cairo_pattern_add_color_stop_rgb (pattern,
						  0.0,
						  priv->b_color.red/G_MAXINT16,
						  priv->b_color.green/G_MAXINT16,
						  priv->b_color.blue/G_MAXINT16);

		cairo_pattern_add_color_stop_rgb (pattern,
						  1.0,
						  priv->b_color2.red/G_MAXINT16,
						  priv->b_color2.green/G_MAXINT16,
						  priv->b_color2.blue/G_MAXINT16);
	}

	priv->pattern = pattern;

	brasero_jacket_view_update_edit_image (self);
	gtk_widget_queue_draw (GTK_WIDGET (self));
}

const gchar *
brasero_jacket_view_get_image (BraseroJacketView *self)
{
	BraseroJacketViewPrivate *priv;

	priv = BRASERO_JACKET_VIEW_PRIVATE (self);
	return priv->image_path;
}

const gchar *
brasero_jacket_view_set_image (BraseroJacketView *self,
			       const gchar *path)
{
	BraseroJacketViewPrivate *priv;
	GdkPixbuf *image = NULL;
	GError *error = NULL;

	priv = BRASERO_JACKET_VIEW_PRIVATE (self);

	if (!path)
		return priv->image_path;

	image = gdk_pixbuf_new_from_file (path, &error);
	if (error) {
		brasero_utils_message_dialog (gtk_widget_get_toplevel (GTK_WIDGET (self)),
					      /* Translators: This is an image,
					       * a picture, not a "Disc Image" */
					      _("The image could not be loaded."),
					      error->message,
					      GTK_MESSAGE_ERROR);
		g_error_free (error);
		return priv->image_path;
	}

	if (priv->image_path) {
		g_free (priv->image_path);
		priv->image_path = NULL;
	}

	if (priv->scaled) {
		g_object_unref (priv->scaled);
		priv->scaled = NULL;
	}

	if (priv->image) {
		g_object_unref (priv->image);
		priv->image = NULL;
	}

	priv->image_path = g_strdup (path);
	priv->image = image;

	brasero_jacket_view_update_image (self);
	return priv->image_path;
}

void
brasero_jacket_view_set_color_background (BraseroJacketView *self,
					  GdkColor *color,
					  GdkColor *color2)
{
	BraseroJacketViewPrivate *priv;

	priv = BRASERO_JACKET_VIEW_PRIVATE (self);
	priv->b_color = *color;
	priv->b_color2 = *color2;
	brasero_jacket_view_update_color (self);
}

void
brasero_jacket_view_set_color_style (BraseroJacketView *self,
				     BraseroJacketColorStyle style)
{
	BraseroJacketViewPrivate *priv;

	priv = BRASERO_JACKET_VIEW_PRIVATE (self);
	priv->color_style = style;
	brasero_jacket_view_update_color (self);
}

GtkTextAttributes *
brasero_jacket_view_get_attributes (BraseroJacketView *self,
				    GtkTextIter *iter)
{
	BraseroJacketViewPrivate *priv;
	GtkTextAttributes *attributes;
	GtkTextBuffer *buffer;

	priv = BRASERO_JACKET_VIEW_PRIVATE (self);

	attributes = gtk_text_view_get_default_attributes (GTK_TEXT_VIEW (priv->edit));

	if (iter)
		gtk_text_iter_get_attributes (iter, attributes);

	/* Now also merge changes that are 'on hold', that is non applied tags */
	buffer = brasero_jacket_view_get_active_buffer (self);
	if (!buffer)
		return attributes;

	brasero_jacket_buffer_get_attributes (BRASERO_JACKET_BUFFER (buffer), attributes);
	return attributes;
}

GtkTextBuffer *
brasero_jacket_view_get_active_buffer (BraseroJacketView *self)
{
	BraseroJacketViewPrivate *priv;
	GtkWidget *current;

	priv = BRASERO_JACKET_VIEW_PRIVATE (self);

	if (priv->sides && gtk_widget_is_focus (priv->sides))
		current = priv->sides;
	else if (gtk_widget_is_focus (priv->edit))
		current = priv->edit;
	else
		return NULL;

	return gtk_text_view_get_buffer (GTK_TEXT_VIEW (current));
}

GtkTextBuffer *
brasero_jacket_view_get_body_buffer (BraseroJacketView *self)
{
	BraseroJacketViewPrivate *priv;

	priv = BRASERO_JACKET_VIEW_PRIVATE (self);

	return gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->edit));	
}

GtkTextBuffer *
brasero_jacket_view_get_side_buffer (BraseroJacketView *self)
{
	BraseroJacketViewPrivate *priv;

	priv = BRASERO_JACKET_VIEW_PRIVATE (self);

	if (!priv->sides)
		return NULL;

	return gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->sides));
}

static gboolean
brasero_jacket_view_expose (GtkWidget *widget,
			    GdkEventExpose *event)
{
	guint x;
	guint y;
	cairo_t *ctx;
	gdouble resolution;
	GtkWidget *toplevel;
	PangoLayout *layout;
	BraseroJacketViewPrivate *priv;

	priv = BRASERO_JACKET_VIEW_PRIVATE (widget);

	ctx = gdk_cairo_create (GDK_DRAWABLE (widget->window));

	toplevel = gtk_widget_get_toplevel (widget);
	if (!GTK_IS_WINDOW (toplevel))
		return FALSE;

	resolution = gdk_screen_get_resolution (gtk_window_get_screen (GTK_WINDOW (toplevel)));
	layout = gtk_widget_create_pango_layout (widget, NULL);
	if (priv->side == BRASERO_JACKET_BACK) {
		x = (widget->allocation.width - resolution * COVER_WIDTH_BACK_INCH) / 2;
		y = (widget->allocation.height - resolution * COVER_HEIGHT_BACK_INCH) - BRASERO_JACKET_VIEW_MARGIN;

		brasero_jacket_view_render (BRASERO_JACKET_VIEW (widget),
					    ctx,
					    layout,
					    resolution,
					    resolution,
					    x,
					    y,
					    &event->area,
					    TRUE);

		/* rectangle for side text */

		/* set clip */
		cairo_reset_clip (ctx);
		cairo_rectangle (ctx, event->area.x, event->area.y, event->area.width, event->area.height);
		cairo_clip (ctx);

		cairo_move_to (ctx, 0., 0.);

		cairo_set_antialias (ctx, CAIRO_ANTIALIAS_DEFAULT);
		cairo_set_source_rgb (ctx, 0.5, 0.5, 0.5);
		cairo_set_line_width (ctx, 0.5);
		cairo_set_line_cap (ctx, CAIRO_LINE_CAP_ROUND);

		cairo_rectangle (ctx,
				 priv->sides->allocation.x - 1,
				 priv->sides->allocation.y - 1,
				 priv->sides->allocation.width + 2,
				 priv->sides->allocation.height + 2);
		cairo_stroke (ctx);

		gtk_container_propagate_expose (GTK_CONTAINER (widget),
						priv->sides,
						event);
	}
	else {
		x = (widget->allocation.width - resolution * COVER_WIDTH_FRONT_INCH) / 2;
		y = (widget->allocation.height - resolution * COVER_HEIGHT_FRONT_INCH) / 2;

		brasero_jacket_view_render (BRASERO_JACKET_VIEW (widget),
					    ctx,
					    layout,
					    resolution,
					    resolution,
					    x,
					    y,
					    &event->area,
					    TRUE);
	}

	gtk_container_propagate_expose (GTK_CONTAINER (widget),
					priv->edit,
					event);

	g_object_unref (layout);
	cairo_destroy (ctx);
	return FALSE;
}

static void
brasero_jacket_view_realize (GtkWidget *widget)
{
	BraseroJacketViewPrivate *priv;
	GdkWindowAttr attributes;
	gint attributes_mask;

	priv = BRASERO_JACKET_VIEW_PRIVATE (widget);

	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = widget->allocation.x;
	attributes.y = widget->allocation.y;
	attributes.width = widget->allocation.width;
	attributes.height = widget->allocation.height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual (widget);
	attributes.colormap = gtk_widget_get_colormap (widget);
	attributes.event_mask = gtk_widget_get_events (widget);
	attributes.event_mask |= GDK_EXPOSURE_MASK|GDK_BUTTON_PRESS_MASK|GDK_LEAVE_NOTIFY_MASK;
	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_COLORMAP;

	widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
					 &attributes,
					 attributes_mask);
	gdk_window_set_user_data (widget->window, widget);

	widget->style = gtk_style_attach (widget->style, widget->window);
	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
}

static void
brasero_jacket_view_map (GtkWidget *widget)
{
	g_return_if_fail (widget != NULL);
	gdk_window_show (widget->window);

	GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);

	if (GTK_WIDGET_CLASS (brasero_jacket_view_parent_class)->map)
		GTK_WIDGET_CLASS (brasero_jacket_view_parent_class)->map (widget);
}

static void
brasero_jacket_view_unmap (GtkWidget *widget)
{
	g_return_if_fail (widget != NULL);
	gdk_window_hide (widget->window);

	GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);

	if (GTK_WIDGET_CLASS (brasero_jacket_view_parent_class)->unmap)
		GTK_WIDGET_CLASS (brasero_jacket_view_parent_class)->unmap (widget);
}

static void
brasero_jacket_view_size_request (GtkWidget *widget,
				  GtkRequisition *request)
{
	BraseroJacketViewPrivate *priv;
	GtkWidget *toplevel;
	gdouble resolution;

	priv = BRASERO_JACKET_VIEW_PRIVATE (widget);

	if (!widget->parent)
		return;

	toplevel = gtk_widget_get_toplevel (widget);
	if (!GTK_IS_WINDOW (toplevel))
		return;

	resolution = gdk_screen_get_resolution (gtk_window_get_screen (GTK_WINDOW (toplevel)));

	if (priv->side == BRASERO_JACKET_FRONT) {
		request->width = COVER_WIDTH_FRONT_INCH * resolution + BRASERO_JACKET_VIEW_MARGIN * 2;
		request->height = COVER_HEIGHT_FRONT_INCH * resolution + BRASERO_JACKET_VIEW_MARGIN * 2;
	}
	else if (priv->side == BRASERO_JACKET_BACK) {
		request->width = COVER_WIDTH_BACK_INCH * resolution +
				 BRASERO_JACKET_VIEW_MARGIN * 2;
		request->height = COVER_HEIGHT_BACK_INCH * resolution +
				  COVER_WIDTH_SIDE_INCH * resolution +
				  BRASERO_JACKET_VIEW_MARGIN * 3;
	}
}

static void
brasero_jacket_view_size_allocate (GtkWidget *widget,
				   GtkAllocation *allocation)
{
	BraseroJacketViewPrivate *priv;
	GtkAllocation view_alloc;
	GtkWidget *toplevel;
	gint resolution;

	toplevel = gtk_widget_get_toplevel (widget);
	if (!GTK_IS_WINDOW (toplevel))
		return;

	resolution = gdk_screen_get_resolution (gtk_window_get_screen (GTK_WINDOW (toplevel)));
	priv = BRASERO_JACKET_VIEW_PRIVATE (widget);

	if (priv->image && priv->image_style == BRASERO_JACKET_IMAGE_STRETCH) {
		if (priv->scaled) {
			g_object_unref (priv->scaled);
			priv->scaled = NULL;
		}

		/* scale pixbuf */
		brasero_jacket_view_update_image (BRASERO_JACKET_VIEW (widget));
	}

	view_alloc.x = BRASERO_JACKET_VIEW_MARGIN + COVER_TEXT_MARGIN * resolution;
	view_alloc.y = BRASERO_JACKET_VIEW_MARGIN + COVER_TEXT_MARGIN * resolution;

	if (priv->side == BRASERO_JACKET_BACK) {
		view_alloc.x = (allocation->width - COVER_HEIGHT_SIDE_INCH * resolution) / 2;
		view_alloc.y = BRASERO_JACKET_VIEW_MARGIN;
		view_alloc.width = COVER_HEIGHT_SIDE_INCH * resolution;
		view_alloc.height = COVER_WIDTH_SIDE_INCH * resolution;

		gtk_widget_size_allocate (priv->sides, &view_alloc);

		view_alloc.x = (allocation->width - COVER_WIDTH_BACK_INCH * resolution) / 2 +
			       (COVER_TEXT_MARGIN + COVER_WIDTH_SIDE_INCH) * resolution;

		view_alloc.y = (allocation->height - resolution * COVER_HEIGHT_BACK_INCH) -
				BRASERO_JACKET_VIEW_MARGIN +
				COVER_TEXT_MARGIN * resolution;

		view_alloc.width = COVER_WIDTH_BACK_INCH * resolution -
				   COVER_TEXT_MARGIN * resolution * 2 - 
				   COVER_WIDTH_SIDE_INCH * resolution * 2;
		view_alloc.height = COVER_HEIGHT_BACK_INCH * resolution -
				    COVER_TEXT_MARGIN * resolution * 2;
	}
	else {
		view_alloc.x = (allocation->width - COVER_WIDTH_FRONT_INCH * resolution) / 2 +
				COVER_TEXT_MARGIN * resolution;
		view_alloc.y = (allocation->height - resolution * COVER_HEIGHT_FRONT_INCH) / 2 +
				COVER_TEXT_MARGIN * resolution;

		view_alloc.width = COVER_WIDTH_FRONT_INCH * resolution -
				   COVER_TEXT_MARGIN * resolution * 2;
		view_alloc.height = COVER_HEIGHT_FRONT_INCH * resolution -
				    COVER_TEXT_MARGIN * resolution * 2;
	}

	brasero_jacket_view_update_edit_image (BRASERO_JACKET_VIEW (widget));
	gtk_widget_size_allocate (priv->edit, &view_alloc);

	widget->allocation = *allocation;
	if (GTK_WIDGET_REALIZED (widget) && !GTK_WIDGET_NO_WINDOW (widget)) {
		gdk_window_move_resize (widget->window,
					allocation->x,
					allocation->y,
					allocation->width,
					allocation->height);
	}
}

static void
brasero_jacket_view_container_forall (GtkContainer *container,
				      gboolean include_internals,
				      GtkCallback callback,
				      gpointer callback_data)
{
	BraseroJacketViewPrivate *priv;

	priv = BRASERO_JACKET_VIEW_PRIVATE (container);
	if (priv->edit)
		callback (priv->edit, callback_data);
	if (priv->sides)
		callback (priv->sides, callback_data);
}

static void
brasero_jacket_view_container_remove (GtkContainer *container,
				      GtkWidget *widget)
{
	BraseroJacketViewPrivate *priv;

	priv = BRASERO_JACKET_VIEW_PRIVATE (container);
	if (priv->edit == widget)
		priv->edit = NULL;

	if (priv->sides == widget)
		priv->sides = NULL;
}

static void
brasero_jacket_view_init (BraseroJacketView *object)
{
	BraseroJacketViewPrivate *priv;
	GtkTextBuffer *buffer;
	GtkObject *vadj;
	GtkObject *hadj;

	priv = BRASERO_JACKET_VIEW_PRIVATE (object);

	buffer = GTK_TEXT_BUFFER (brasero_jacket_buffer_new ());
	g_signal_connect (buffer,
			  "notify::cursor-position",
			  G_CALLBACK (brasero_jacket_view_cursor_position_changed_cb),
			  object);
	g_signal_connect_after (buffer,
				"apply-tag",
				G_CALLBACK (brasero_jacket_view_apply_tag),
				object);

	priv->edit = gtk_text_view_new_with_buffer (buffer);
	g_object_unref (buffer);

	priv->b_color = priv->edit->style->bg [0];
	priv->color_style = BRASERO_JACKET_COLOR_SOLID;

	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (priv->edit), GTK_WRAP_CHAR);
	gtk_widget_set_parent (priv->edit, GTK_WIDGET (object));
	gtk_widget_show (priv->edit);

	g_signal_connect (priv->edit,
			  "focus-in-event",
			  G_CALLBACK (brasero_jacket_view_focus_in_cb),
			  object);
	g_signal_connect_after (priv->edit,
				"focus-out-event",
				G_CALLBACK (brasero_jacket_view_focus_out_cb),
				object);
	g_signal_connect_after (priv->edit,
				"populate-popup",
				G_CALLBACK (brasero_jacket_view_populate_popup_cb),
				object);
	hadj = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
	vadj = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

	g_signal_connect (hadj,
			  "value-changed",
			  G_CALLBACK (brasero_jacket_view_scrolled_cb),
			  priv->edit);
	g_signal_connect (vadj,
			  "value-changed",
			  G_CALLBACK (brasero_jacket_view_scrolled_cb),
			  priv->edit);

	gtk_container_set_focus_child (GTK_CONTAINER (object), priv->edit);
	gtk_widget_set_scroll_adjustments (priv->edit,
					   GTK_ADJUSTMENT (hadj),
					   GTK_ADJUSTMENT (vadj));
}

static void
brasero_jacket_view_finalize (GObject *object)
{
	BraseroJacketViewPrivate *priv;

	priv = BRASERO_JACKET_VIEW_PRIVATE (object);
	if (priv->image) {
		g_object_unref (priv->image);
		priv->image = NULL;
	}

	if (priv->scaled) {
		g_object_unref (priv->scaled);
		priv->scaled = NULL;
	}

	if (priv->pattern) {
		cairo_pattern_destroy (priv->pattern);
		priv->pattern = NULL;
	}

	if (priv->image_path) {
		g_free (priv->image_path);
		priv->image_path = NULL;
	}

	G_OBJECT_CLASS (brasero_jacket_view_parent_class)->finalize (object);
}

static void
brasero_jacket_view_class_init (BraseroJacketViewClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass* widget_class = GTK_WIDGET_CLASS (klass);
	GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroJacketViewPrivate));

	object_class->finalize = brasero_jacket_view_finalize;

	widget_class->expose_event = brasero_jacket_view_expose;
	widget_class->map = brasero_jacket_view_map;
	widget_class->unmap = brasero_jacket_view_unmap;
	widget_class->realize = brasero_jacket_view_realize;
	widget_class->size_allocate = brasero_jacket_view_size_allocate;
	widget_class->size_request = brasero_jacket_view_size_request;

	container_class->forall = brasero_jacket_view_container_forall;
	container_class->remove = brasero_jacket_view_container_remove;

	jacket_view_signals[PRINTED] =
		g_signal_new ("printed",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION | G_SIGNAL_NO_HOOKS,
		              0,
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0,
		              G_TYPE_NONE);
	jacket_view_signals[TAGS_CHANGED] =
		g_signal_new ("tags_changed",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION | G_SIGNAL_NO_HOOKS,
		              0,
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0,
		              G_TYPE_NONE);
}

GtkWidget *
brasero_jacket_view_new (void)
{
	return g_object_new (BRASERO_TYPE_JACKET_VIEW, NULL);
}
