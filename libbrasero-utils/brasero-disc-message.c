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

#include <gdk/gdkkeysyms.h>

#include <gtk/gtk.h>

#include "brasero-disc-message.h"


typedef struct _BraseroDiscMessagePrivate BraseroDiscMessagePrivate;
struct _BraseroDiscMessagePrivate
{
	GtkWidget *progress;

	GtkWidget *expander;

	GtkWidget *primary;
	GtkWidget *secondary;

	GtkWidget *text_box;

	guint context;

	guint id;
	guint timeout;

	guint prevent_destruction:1;
};

#define BRASERO_DISC_MESSAGE_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_DISC_MESSAGE, BraseroDiscMessagePrivate))

G_DEFINE_TYPE (BraseroDiscMessage, brasero_disc_message, GTK_TYPE_INFO_BAR);

enum {
	TEXT_COL,
	NUM_COL
};

static gboolean
brasero_disc_message_timeout (gpointer data)
{
	BraseroDiscMessagePrivate *priv;

	priv = BRASERO_DISC_MESSAGE_PRIVATE (data);
	priv->timeout = 0;

	priv->prevent_destruction = TRUE;
	gtk_info_bar_response (data, GTK_RESPONSE_DELETE_EVENT);
	priv->prevent_destruction = FALSE;

	gtk_widget_destroy (GTK_WIDGET (data));
	return FALSE;
}

void
brasero_disc_message_set_timeout (BraseroDiscMessage *self,
				  guint mseconds)
{
	BraseroDiscMessagePrivate *priv;

	priv = BRASERO_DISC_MESSAGE_PRIVATE (self);

	if (priv->timeout) {
		g_source_remove (priv->timeout);
		priv->timeout = 0;
	}

	if (mseconds > 0)
		priv->timeout = g_timeout_add (mseconds,
					       brasero_disc_message_timeout,
					       self);
}

void
brasero_disc_message_set_context (BraseroDiscMessage *self,
				  guint context_id)
{
	BraseroDiscMessagePrivate *priv;

	priv = BRASERO_DISC_MESSAGE_PRIVATE (self);
	priv->context = context_id;
}

guint
brasero_disc_message_get_context (BraseroDiscMessage *self)
{
	BraseroDiscMessagePrivate *priv;

	priv = BRASERO_DISC_MESSAGE_PRIVATE (self);
	return priv->context;
}

static void
brasero_disc_message_expander_activated_cb (GtkExpander *expander,
					    BraseroDiscMessage *self)
{
	if (!gtk_expander_get_expanded (expander))
		gtk_expander_set_label (expander, _("_Hide changes"));
	else
		gtk_expander_set_label (expander, _("_Show changes"));
}

void
brasero_disc_message_add_errors (BraseroDiscMessage *self,
				 GSList *errors)
{
	BraseroDiscMessagePrivate *priv;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkListStore *model;
	GtkWidget *scroll;
	GtkWidget *tree;

	priv = BRASERO_DISC_MESSAGE_PRIVATE (self);

	if (priv->expander)
		gtk_widget_destroy (priv->expander);

	priv->expander = gtk_expander_new_with_mnemonic (_("_Show changes"));
	gtk_widget_show (priv->expander);
	gtk_box_pack_start (GTK_BOX (priv->text_box), priv->expander, FALSE, TRUE, 0);

	g_signal_connect (priv->expander,
			  "activate",
			  G_CALLBACK (brasero_disc_message_expander_activated_cb),
			  self);

	model = gtk_list_store_new (NUM_COL, G_TYPE_STRING);

	tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
	gtk_widget_show (tree);

	g_object_unref (G_OBJECT (model));

	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (tree), TRUE);
	gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree)),
				     GTK_SELECTION_NONE);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("error",
							   renderer,
							   "text", TEXT_COL,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree), FALSE);

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scroll);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll),
					     GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scroll), tree);
	gtk_container_add (GTK_CONTAINER (priv->expander), scroll);

	for (; errors; errors = errors->next) {
		GtkTreeIter iter;

		gtk_list_store_append (model, &iter);
		gtk_list_store_set (model, &iter,
				    TEXT_COL, errors->data,
				    -1);
	}
}

void
brasero_disc_message_remove_errors (BraseroDiscMessage *self)
{
	BraseroDiscMessagePrivate *priv;

	priv = BRASERO_DISC_MESSAGE_PRIVATE (self);

	gtk_widget_destroy (priv->expander);
	priv->expander = NULL;
}

void
brasero_disc_message_destroy (BraseroDiscMessage *self)
{
	BraseroDiscMessagePrivate *priv;

	priv = BRASERO_DISC_MESSAGE_PRIVATE (self);

	if (priv->prevent_destruction)
		return;

	gtk_widget_destroy (GTK_WIDGET (self));
}

void
brasero_disc_message_remove_buttons (BraseroDiscMessage *self)
{
	gtk_container_foreach (GTK_CONTAINER (gtk_info_bar_get_action_area (GTK_INFO_BAR (self))),
			       (GtkCallback) gtk_widget_destroy,
			       NULL);
}

static gboolean
brasero_disc_message_update_progress (gpointer data)
{
	BraseroDiscMessagePrivate *priv;

	priv = BRASERO_DISC_MESSAGE_PRIVATE (data);
	gtk_progress_bar_pulse (GTK_PROGRESS_BAR (priv->progress));
	return TRUE;
}

void
brasero_disc_message_set_progress_active (BraseroDiscMessage *self,
					  gboolean active)
{
	BraseroDiscMessagePrivate *priv;

	priv = BRASERO_DISC_MESSAGE_PRIVATE (self);

	if (!priv->progress) {
		priv->progress = gtk_progress_bar_new ();
		gtk_box_pack_start (GTK_BOX (priv->text_box), priv->progress, FALSE, TRUE, 0);
	}

	if (active) {
		gtk_widget_show (priv->progress);

		if (!priv->id)
			priv->id = g_timeout_add (150,
						  (GSourceFunc) brasero_disc_message_update_progress,
						  self);
	}
	else {
		gtk_widget_hide (priv->progress);
		if (priv->id) {
			g_source_remove (priv->id);
			priv->id = 0;
		}
	}
}

void
brasero_disc_message_set_progress (BraseroDiscMessage *self,
				   gdouble progress)
{
	BraseroDiscMessagePrivate *priv;

	priv = BRASERO_DISC_MESSAGE_PRIVATE (self);

	if (!priv->progress) {
		priv->progress = gtk_progress_bar_new ();
		gtk_box_pack_start (GTK_BOX (priv->text_box), priv->progress, FALSE, TRUE, 0);
	}

	gtk_widget_show (priv->progress);
	if (priv->id) {
		g_source_remove (priv->id);
		priv->id = 0;
	}

	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (priv->progress), progress);
}

void
brasero_disc_message_set_primary (BraseroDiscMessage *self,
				  const gchar *message)
{
	BraseroDiscMessagePrivate *priv;
	gchar *markup;

	priv = BRASERO_DISC_MESSAGE_PRIVATE (self);

	markup = g_strdup_printf ("<b>%s</b>", message);
	gtk_label_set_markup (GTK_LABEL (priv->primary), markup);
	g_free (markup);
	gtk_widget_show (priv->primary);
}

void
brasero_disc_message_set_secondary (BraseroDiscMessage *self,
				    const gchar *message)
{
	BraseroDiscMessagePrivate *priv;

	priv = BRASERO_DISC_MESSAGE_PRIVATE (self);

	if (!message) {
		if (priv->secondary) {
			gtk_widget_destroy (priv->secondary);
			priv->secondary = NULL;
		}
		return;
	}

	if (!priv->secondary) {
		priv->secondary = gtk_label_new (NULL);
		gtk_label_set_line_wrap_mode (GTK_LABEL (priv->secondary), GTK_WRAP_WORD);
		gtk_label_set_line_wrap (GTK_LABEL (priv->secondary), TRUE);
		gtk_misc_set_alignment (GTK_MISC (priv->secondary), 0.0, 0.5);
		gtk_box_pack_start (GTK_BOX (priv->text_box), priv->secondary, FALSE, TRUE, 0);
	}

	gtk_label_set_markup (GTK_LABEL (priv->secondary), message);
	gtk_widget_show (priv->secondary);
}

static void
brasero_disc_message_init (BraseroDiscMessage *object)
{
	BraseroDiscMessagePrivate *priv;
	GtkWidget *main_box;

	priv = BRASERO_DISC_MESSAGE_PRIVATE (object);

	main_box = gtk_info_bar_get_content_area (GTK_INFO_BAR (object));

	priv->text_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_show (priv->text_box);
	gtk_box_pack_start (GTK_BOX (main_box), priv->text_box, FALSE, FALSE, 0);

	priv->primary = gtk_label_new (NULL);
	gtk_widget_show (priv->primary);
	gtk_label_set_line_wrap_mode (GTK_LABEL (priv->primary), GTK_WRAP_WORD);
	gtk_label_set_line_wrap (GTK_LABEL (priv->primary), TRUE);
	gtk_misc_set_alignment (GTK_MISC (priv->primary), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (priv->text_box), priv->primary, FALSE, FALSE, 0);
}

static void
brasero_disc_message_finalize (GObject *object)
{
	BraseroDiscMessagePrivate *priv;

	priv = BRASERO_DISC_MESSAGE_PRIVATE (object);	
	if (priv->id) {
		g_source_remove (priv->id);
		priv->id = 0;
	}

	if (priv->timeout) {
		g_source_remove (priv->timeout);
		priv->timeout = 0;
	}

	G_OBJECT_CLASS (brasero_disc_message_parent_class)->finalize (object);
}

static void
brasero_disc_message_class_init (BraseroDiscMessageClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroDiscMessagePrivate));

	object_class->finalize = brasero_disc_message_finalize;
}

GtkWidget *
brasero_disc_message_new (void)
{
	return g_object_new (BRASERO_TYPE_DISC_MESSAGE, NULL);
}
