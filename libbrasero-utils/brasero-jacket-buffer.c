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

#include <gtk/gtk.h>
 
#include "brasero-jacket-buffer.h"

typedef struct _BraseroJacketBufferPrivate BraseroJacketBufferPrivate;
struct _BraseroJacketBufferPrivate
{
	GSList *tags;

	guint pos;

	gchar *default_text;

	guint inserting_text:1;
	guint empty:1;
};

#define BRASERO_JACKET_BUFFER_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_JACKET_BUFFER, BraseroJacketBufferPrivate))

#define BRASERO_JACKET_BUFFER_TAG	"jacket-buffer-tag"

G_DEFINE_TYPE (BraseroJacketBuffer, brasero_jacket_buffer, GTK_TYPE_TEXT_BUFFER);

gchar *
brasero_jacket_buffer_get_text (BraseroJacketBuffer *self,
				GtkTextIter *start,
				GtkTextIter *end,
				gboolean invisible_chars,
				gboolean get_default_text)
{
	BraseroJacketBufferPrivate *priv;

	priv = BRASERO_JACKET_BUFFER_PRIVATE (self);
	if (priv->empty && !get_default_text)
		return NULL;

	return gtk_text_buffer_get_text (GTK_TEXT_BUFFER (self), start, end, invisible_chars);
}

/* As the name suggests it is copied from GTK 2.14.3
 * It was changed to use GSList * as arguments */
static void
_gtk_text_attributes_fill_from_tags (GtkTextAttributes *dest,
                                     GSList	       *tags)
{
	guint left_margin_accumulative = 0;
	guint right_margin_accumulative = 0;

	for (; tags; tags = tags->next) {
		GtkTextTag *tag;
		gboolean accumulative_margin;
		gboolean background_set;
		gboolean fg_color_set;
		gboolean pg_bg_color_set;
		gboolean scale_set;
		gboolean left_margin_set;
		gboolean justification_set;
		gboolean indent_set;
		gboolean rise_set;
		gboolean right_margin_set;
		gboolean pixels_above_lines_set;
		gboolean pixels_below_lines_set;
		gboolean tabs_set;
		gboolean wrap_mode_set;
		gboolean pixels_inside_wrap_set;
		gboolean underline_set;
		gboolean strikethrough_set;
		gboolean invisible_set;
		gboolean editable_set;
		gboolean bg_full_height_set;
		gboolean language_set;
		GtkTextDirection direction;
		PangoFontDescription *font_desc;

		tag = tags->data;

		g_object_get (tag,
		              "accumulative-margin", &accumulative_margin,
		              "background-set", &background_set,
		              "foreground-set", &fg_color_set,
		              "paragraph-background-set", &pg_bg_color_set,
		              "scale-set", &scale_set,
		              "left-margin-set", &left_margin_set,
		              "justification-set", &justification_set,
		              "indent-set", &indent_set,
		              "rise-set", &rise_set,
		              "right-margin-set", &right_margin_set,
		              "pixels-above-lines-set", &pixels_above_lines_set,
		              "pixels-below-lines-set", &pixels_below_lines_set,
		              "tabs-set", &tabs_set,
		              "wrap-mode-set", &wrap_mode_set,
		              "pixels-inside-wrap-set", &pixels_inside_wrap_set,
		              "underline-set", &underline_set,
		              "strikethrough-set", &strikethrough_set,
		              "invisible-set", &invisible_set,
		              "editable-set", &editable_set,
		              "background-full-height-set", &bg_full_height_set,
		              "language-set", &language_set,
		              "direction", &direction,
		              "font-desc", &font_desc,
		              NULL);

		if (dest->appearance.draw_bg) {
			 GdkColor *color = NULL;

			 g_object_get (tag, "background-gdk", &color, NULL);
			 dest->appearance.bg_color = *color;
			 gdk_color_free (color);

			 dest->appearance.draw_bg = TRUE;
		 }

		if (fg_color_set) {
			GdkColor *color = NULL;

			g_object_get (tag, "foreground-gdk", &color, NULL);
			dest->appearance.fg_color = *color;
			gdk_color_free (color);
		}

		if (pg_bg_color_set) {
			if (dest->pg_bg_color)
				gdk_color_free (dest->pg_bg_color);

			g_object_get (tag, "paragraph-background-gdk", &dest->pg_bg_color, NULL);
		}

		if (font_desc) {
			if (dest->font) {
				pango_font_description_merge (dest->font, font_desc, TRUE);
				pango_font_description_free (font_desc);
			}
			else
				dest->font = font_desc;
		}

		/* multiply all the scales together to get a composite */
		if (scale_set) {
			gdouble font_scale;
			g_object_get (tag, "font-scale", &font_scale, NULL);
			dest->font_scale *= font_scale;
		}

		if (justification_set)
			g_object_get (tag, "justification", &dest->justification, NULL);

		if (direction != GTK_TEXT_DIR_NONE)
			dest->direction = direction;

		if (left_margin_set) {
			gint left_margin;

			g_object_get (tag, "left-margin", &left_margin, NULL);
			if (accumulative_margin)
				left_margin_accumulative += left_margin;
			else
				dest->left_margin = left_margin;
		}

		if (indent_set)
			g_object_get (tag, "indent", &dest->indent, NULL);

		if (rise_set)
			g_object_get (tag, "rise", &dest->appearance.rise, NULL);

		if (right_margin_set) {
			gint right_margin;

			g_object_get (tag, "right-margin", &right_margin, NULL);

			if (accumulative_margin)
				right_margin_accumulative += right_margin;
			else
				dest->right_margin = right_margin;
		}

		if (pixels_above_lines_set)
			g_object_get (tag, "pixels-above-lines", &dest->pixels_above_lines, NULL);

		if (pixels_below_lines_set)
			g_object_get (tag, "pixels-below-lines", &dest->pixels_below_lines, NULL);

		if (pixels_inside_wrap_set)
			g_object_get (tag, "pixels-inside-wrap", &dest->pixels_inside_wrap, NULL);

		if (tabs_set) {
			if (dest->tabs)
				pango_tab_array_free (dest->tabs);
			g_object_get (tag, "pixels-inside-wrap", &dest->tabs, NULL);
		}

		if (wrap_mode_set)
			g_object_get (tag, "wrap-mode", &dest->wrap_mode, NULL);

		if (underline_set) {
			gint underline;

			g_object_get (tag, "underline", &underline, NULL);
			dest->appearance.underline = underline;
		}

		if (strikethrough_set) {
			gint strikethrough;

			g_object_get (tag, "strikethrough", &strikethrough, NULL);
			dest->appearance.strikethrough = strikethrough;
		}

		if (invisible_set) {
			gint invisible;

			g_object_get (tag, "invisible", &invisible, NULL);
			dest->invisible = invisible;
		}

		if (editable_set) {
			gint editable;

			g_object_get (tag, "editable", &editable, NULL);
			dest->editable = editable;
		}

		if (bg_full_height_set) {
			gint bg_full_height;

			g_object_get (tag, "background-full-height", &bg_full_height, NULL);
			dest->bg_full_height = bg_full_height;
		}

		if (language_set) {
			g_free (dest->language);
			g_object_get (tag, "language", &dest->language, NULL);
		}
    }

	dest->left_margin += left_margin_accumulative;
	dest->right_margin += right_margin_accumulative;
}

void
brasero_jacket_buffer_add_default_tag (BraseroJacketBuffer *self,
				       GtkTextTag *tag)
{
	BraseroJacketBufferPrivate *priv;

	priv = BRASERO_JACKET_BUFFER_PRIVATE (self);

	g_object_ref (tag);
	priv->tags = g_slist_append (priv->tags, tag);
}

void
brasero_jacket_buffer_get_attributes (BraseroJacketBuffer *self,
				      GtkTextAttributes *attributes)
{
	BraseroJacketBufferPrivate *priv;

	priv = BRASERO_JACKET_BUFFER_PRIVATE (self);

	/* Now also merge changes that are 'on hold', that is non applied tags */
	if (!priv->tags)
		return;

	_gtk_text_attributes_fill_from_tags (attributes, priv->tags);
}

static void
brasero_jacket_buffer_cursor_position_changed_cb (GObject *buffer,
						  GParamSpec *spec,
						  gpointer NULL_data)
{
	BraseroJacketBufferPrivate *priv;
	GtkTextIter iter;
	guint pos;

	priv = BRASERO_JACKET_BUFFER_PRIVATE (buffer);

	if (priv->inserting_text)
		return;

	g_object_get (buffer,
		      "cursor-position", &pos,
		      NULL);

	if (priv->pos == pos)
		return;

	if (pos)
		gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &iter, pos - 1);
	else
		gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &iter, pos);

	g_slist_foreach (priv->tags, (GFunc) g_object_unref, NULL);
	g_slist_free (priv->tags);
 
	priv->tags = gtk_text_iter_get_tags (&iter);
	g_slist_foreach (priv->tags, (GFunc) g_object_ref, NULL);
}

static void
brasero_jacket_buffer_insert_text (GtkTextBuffer *buffer,
				   GtkTextIter *location,
				   const gchar *text,
				   gint length)
{
	GtkTextIter end;
	GSList *tag_iter;
	GtkTextIter start;
	guint start_offset;
	BraseroJacketBufferPrivate *priv;

	priv = BRASERO_JACKET_BUFFER_PRIVATE (buffer);

	start_offset = gtk_text_iter_get_offset (location);
	priv->inserting_text = TRUE;

	brasero_jacket_buffer_show_default_text (BRASERO_JACKET_BUFFER (buffer), FALSE);

	/* revalidate iter in case above function caused invalidation */
	gtk_text_buffer_get_iter_at_offset (buffer, location, start_offset);

	GTK_TEXT_BUFFER_CLASS (brasero_jacket_buffer_parent_class)->insert_text (buffer, location, text, length);

	priv->inserting_text = FALSE;
	gtk_text_buffer_get_iter_at_offset (buffer, &start, start_offset);
	end = *location;

	/* apply tags */
	for (tag_iter = priv->tags; tag_iter; tag_iter = tag_iter->next) {
		GtkTextTag *tag;

		tag = tag_iter->data;
		gtk_text_buffer_apply_tag (buffer, tag,
					   &start,
					   &end);
	}
}

void
brasero_jacket_buffer_show_default_text (BraseroJacketBuffer *self,
					 gboolean show)
{
	BraseroJacketBufferPrivate *priv;
	GtkTextIter start, end;

	priv = BRASERO_JACKET_BUFFER_PRIVATE (self);

	if (show) {
		if (gtk_text_buffer_get_char_count (GTK_TEXT_BUFFER (self)))
			return;

		gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (self), &start);
		GTK_TEXT_BUFFER_CLASS (brasero_jacket_buffer_parent_class)->insert_text (GTK_TEXT_BUFFER (self), &start, priv->default_text, -1);

		gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (self), &start);
		gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (self), &end);
		gtk_text_buffer_apply_tag_by_name (GTK_TEXT_BUFFER (self),
						   BRASERO_JACKET_BUFFER_TAG,
						   &start, &end);
		priv->empty = 1;
	}
	else if (priv->empty) {
		gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (self), &start);
		gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (self), &end);
		gtk_text_buffer_delete (GTK_TEXT_BUFFER (self), &start, &end);
		gtk_text_buffer_remove_all_tags (GTK_TEXT_BUFFER (self), &start, &end);

		priv->empty = 0;
	}
}

void
brasero_jacket_buffer_set_default_text (BraseroJacketBuffer *self,
					const gchar *default_text)
{
	BraseroJacketBufferPrivate *priv;
	GtkTextIter start, end;
	GtkTextTagTable *table;

	priv = BRASERO_JACKET_BUFFER_PRIVATE (self);

	table = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (self));
	if (!gtk_text_tag_table_lookup (table, BRASERO_JACKET_BUFFER_TAG))
		gtk_text_buffer_create_tag (GTK_TEXT_BUFFER (self),
					    BRASERO_JACKET_BUFFER_TAG,
					    "foreground", "grey",
					    "justification", GTK_JUSTIFY_CENTER,
					    "stretch", PANGO_STRETCH_EXPANDED,
					    NULL);

	if (priv->default_text) {
		g_free (priv->default_text);
		priv->default_text = NULL;
	}

	if (!default_text)
		return;

	priv->default_text = g_strdup (default_text);

	if (gtk_text_buffer_get_char_count (GTK_TEXT_BUFFER (self)))
		return;

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (self), &start);
	GTK_TEXT_BUFFER_CLASS (brasero_jacket_buffer_parent_class)->insert_text (GTK_TEXT_BUFFER (self), &start, default_text, -1);

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (self), &start);
	gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (self), &end);
	gtk_text_buffer_apply_tag_by_name (GTK_TEXT_BUFFER (self),
					   BRASERO_JACKET_BUFFER_TAG,
					   &start, &end);
}

static void
brasero_jacket_buffer_init (BraseroJacketBuffer *object)
{
	BraseroJacketBufferPrivate *priv;

	priv = BRASERO_JACKET_BUFFER_PRIVATE (object);

	priv->empty = 1;
	g_signal_connect (object,
			  "notify::cursor-position",
			  G_CALLBACK (brasero_jacket_buffer_cursor_position_changed_cb),
			  NULL);
}

static void
brasero_jacket_buffer_finalize (GObject *object)
{
	BraseroJacketBufferPrivate *priv;

	priv = BRASERO_JACKET_BUFFER_PRIVATE (object);

	if (priv->default_text) {
		g_free (priv->default_text);
		priv->default_text = NULL;
	}

	if (priv->tags) {
		g_slist_foreach (priv->tags, (GFunc) g_object_unref, NULL);
		g_slist_free (priv->tags);
	}

	G_OBJECT_CLASS (brasero_jacket_buffer_parent_class)->finalize (object);
}

static void
brasero_jacket_buffer_class_init (BraseroJacketBufferClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	GtkTextBufferClass* parent_class = GTK_TEXT_BUFFER_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroJacketBufferPrivate));

	object_class->finalize = brasero_jacket_buffer_finalize;

	parent_class->insert_text = brasero_jacket_buffer_insert_text;
}

BraseroJacketBuffer *
brasero_jacket_buffer_new (void)
{
	return g_object_new (BRASERO_TYPE_JACKET_BUFFER, NULL);
}

