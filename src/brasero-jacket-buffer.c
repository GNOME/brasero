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

  g_return_if_fail (!dest->realized);

  for (; tags; tags = tags->next)
    {
      GtkTextTag *tag;
      GtkTextAttributes *vals;

      tag = tags->data;
      vals = tag->values;

      if (tag->bg_color_set)
        {
          dest->appearance.bg_color = vals->appearance.bg_color;

          dest->appearance.draw_bg = TRUE;
        }
      if (tag->fg_color_set)
        dest->appearance.fg_color = vals->appearance.fg_color;
      
      if (tag->pg_bg_color_set)
        {
          dest->pg_bg_color = gdk_color_copy (vals->pg_bg_color);
        }

      if (tag->bg_stipple_set)
        {
          g_object_ref (vals->appearance.bg_stipple);
          if (dest->appearance.bg_stipple)
            g_object_unref (dest->appearance.bg_stipple);
          dest->appearance.bg_stipple = vals->appearance.bg_stipple;

          dest->appearance.draw_bg = TRUE;
        }

      if (tag->fg_stipple_set)
        {
          g_object_ref (vals->appearance.fg_stipple);
          if (dest->appearance.fg_stipple)
            g_object_unref (dest->appearance.fg_stipple);
          dest->appearance.fg_stipple = vals->appearance.fg_stipple;
        }

      if (vals->font)
	{
	  if (dest->font)
	    pango_font_description_merge (dest->font, vals->font, TRUE);
	  else
	    dest->font = pango_font_description_copy (vals->font);
	}

      /* multiply all the scales together to get a composite */
      if (tag->scale_set)
        dest->font_scale *= vals->font_scale;
      
      if (tag->justification_set)
        dest->justification = vals->justification;

      if (vals->direction != GTK_TEXT_DIR_NONE)
        dest->direction = vals->direction;

      if (tag->left_margin_set) 
        {
          if (tag->accumulative_margin)
            left_margin_accumulative += vals->left_margin;
          else
            dest->left_margin = vals->left_margin;
        }

      if (tag->indent_set)
        dest->indent = vals->indent;

      if (tag->rise_set)
        dest->appearance.rise = vals->appearance.rise;

      if (tag->right_margin_set) 
        {
          if (tag->accumulative_margin)
            right_margin_accumulative += vals->right_margin;
          else
            dest->right_margin = vals->right_margin;
        }

      if (tag->pixels_above_lines_set)
        dest->pixels_above_lines = vals->pixels_above_lines;

      if (tag->pixels_below_lines_set)
        dest->pixels_below_lines = vals->pixels_below_lines;

      if (tag->pixels_inside_wrap_set)
        dest->pixels_inside_wrap = vals->pixels_inside_wrap;

      if (tag->tabs_set)
        {
          if (dest->tabs)
            pango_tab_array_free (dest->tabs);
          dest->tabs = pango_tab_array_copy (vals->tabs);
        }

      if (tag->wrap_mode_set)
        dest->wrap_mode = vals->wrap_mode;

      if (tag->underline_set)
        dest->appearance.underline = vals->appearance.underline;

      if (tag->strikethrough_set)
        dest->appearance.strikethrough = vals->appearance.strikethrough;

      if (tag->invisible_set)
        dest->invisible = vals->invisible;

      if (tag->editable_set)
        dest->editable = vals->editable;

      if (tag->bg_full_height_set)
        dest->bg_full_height = vals->bg_full_height;

      if (tag->language_set)
	dest->language = vals->language;
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
	priv->tags = NULL;
 
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

