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

#include <gdk/gdkkeysyms.h>

#include <gtk/gtk.h>

#include "brasero-disc-message.h"

/**
 * This code was heavily inspired by gedit famous message area (gedit-message-area.c)
 */

typedef struct _BraseroDiscMessagePrivate BraseroDiscMessagePrivate;
struct _BraseroDiscMessagePrivate
{
	GtkSizeGroup *group;

	GtkWidget *progress;

	GtkWidget *expander;

	GtkWidget *primary;
	GtkWidget *secondary;

	GtkWidget *image;

	GtkWidget *main_box;
	GtkWidget *button_box;
	GtkWidget *text_box;

	guint context;

	guint id;
	guint timeout;

	guint changing_style:1;
	guint prevent_destruction:1;
};

#define BRASERO_DISC_MESSAGE_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_DISC_MESSAGE, BraseroDiscMessagePrivate))

enum
{
	RESPONSE,
	LAST_SIGNAL
};


static guint disc_message_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (BraseroDiscMessage, brasero_disc_message, GTK_TYPE_BIN);

#define RESPONSE_TYPE	"ResponseType"

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
	g_signal_emit (data,
		       disc_message_signals [RESPONSE],
		       0,
		       GTK_RESPONSE_DELETE_EVENT);
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

	priv->expander = gtk_expander_new_with_mnemonic (_("_Show errors"));
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

static void
brasero_disc_message_button_clicked_cb (GtkButton *button,
					BraseroDiscMessage *self)
{
	BraseroDiscMessagePrivate *priv;

	priv = BRASERO_DISC_MESSAGE_PRIVATE (self);

	priv->prevent_destruction = TRUE;
	g_signal_emit (self,
		       disc_message_signals [RESPONSE],
		       0,
		       GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), RESPONSE_TYPE)));
	priv->prevent_destruction = FALSE;

	gtk_widget_destroy (GTK_WIDGET (self));
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

GtkWidget *
brasero_disc_message_add_button (BraseroDiscMessage *self,
				 GtkSizeGroup *group,
				 const gchar *text,
				 const gchar *tooltip,
				 GtkResponseType response)
{
	GtkWidget *button;
	PangoLayout *layout;
	BraseroDiscMessagePrivate *priv;

	priv = BRASERO_DISC_MESSAGE_PRIVATE (self);

	button = gtk_button_new_from_stock (text);

	/* only add buttons to group if the text is not wrapped. Otherwise
	 * buttons would be too big. */
	layout = gtk_label_get_layout (GTK_LABEL (priv->primary));
	if (!pango_layout_is_wrapped (layout))
		gtk_size_group_add_widget (priv->group, button);

	gtk_widget_set_tooltip_text (button, tooltip);
	gtk_widget_show (button);
	g_signal_connect (button,
			  "clicked",
			  G_CALLBACK (brasero_disc_message_button_clicked_cb),
			  self);

	g_object_set_data (G_OBJECT (button), RESPONSE_TYPE, GINT_TO_POINTER (response));

	gtk_box_pack_start (GTK_BOX (priv->button_box),
			    button,
			    FALSE,
			    TRUE,
			    0);
	gtk_widget_queue_draw (GTK_WIDGET (self));
	return button;
}

void
brasero_disc_message_add_close_button (BraseroDiscMessage *self)
{
	GtkWidget *button;
	PangoLayout *layout;
	GtkWidget *alignment;
	BraseroDiscMessagePrivate *priv;

	priv = BRASERO_DISC_MESSAGE_PRIVATE (self);

	button = gtk_button_new ();

	/* only add buttons to group if the text is not wrapped. Otherwise
	 * buttons would be too big. */
	layout = gtk_label_get_layout (GTK_LABEL (priv->primary));
	if (pango_layout_is_wrapped (layout))
		gtk_size_group_add_widget (priv->group, button);

	alignment = gtk_alignment_new (1.0, 0.0, 0.0, 0.0);
	gtk_widget_show (alignment);
	gtk_container_add (GTK_CONTAINER (alignment), button);

	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	gtk_button_set_image (GTK_BUTTON (button),
			      gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_BUTTON));

	gtk_widget_set_tooltip_text (button, _("Close this notification window"));
	gtk_widget_show (button);
	g_signal_connect (button,
			  "clicked",
			  G_CALLBACK (brasero_disc_message_button_clicked_cb),
			  self);

	g_object_set_data (G_OBJECT (button),
			   RESPONSE_TYPE,
			   GINT_TO_POINTER (GTK_RESPONSE_CLOSE));

	gtk_box_pack_start (GTK_BOX (priv->main_box),
			    alignment,
			    FALSE,
			    TRUE,
			    0);
	gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
brasero_disc_message_remove_buttons (BraseroDiscMessage *self)
{
	BraseroDiscMessagePrivate *priv;

	priv = BRASERO_DISC_MESSAGE_PRIVATE (self);
	gtk_container_foreach (GTK_CONTAINER (priv->button_box),
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
brasero_disc_message_set_image (BraseroDiscMessage *self,
				const gchar *stock_id)
{
	BraseroDiscMessagePrivate *priv;

	priv = BRASERO_DISC_MESSAGE_PRIVATE (self);
	gtk_image_set_from_stock (GTK_IMAGE (priv->image),
				  stock_id,
				  GTK_ICON_SIZE_DIALOG);
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

/**
 * Two following functions are Cut and Pasted from gedit-message-area.c
 */
static void
style_set (GtkWidget        *widget,
	   GtkStyle         *prev_style,
	   BraseroDiscMessage *self)
{
	BraseroDiscMessagePrivate *priv;
	GtkWidget *window;
	GtkStyle *style;

	priv = BRASERO_DISC_MESSAGE_PRIVATE (self);

	if (priv->changing_style)
		return;

	window = gtk_window_new (GTK_WINDOW_POPUP);
	gtk_widget_set_name (window, "gtk-tooltip");
	gtk_widget_ensure_style (window);
	style = gtk_widget_get_style (window);

	priv->changing_style = TRUE;
	gtk_widget_set_style (GTK_WIDGET (self), style);
	priv->changing_style = FALSE;

	gtk_widget_destroy (window);

//	gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);

	gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
brasero_disc_message_init (BraseroDiscMessage *object)
{
	BraseroDiscMessagePrivate *priv;
	GtkWidget *main_box;

	priv = BRASERO_DISC_MESSAGE_PRIVATE (object);
	GTK_WIDGET_UNSET_FLAGS (GTK_WIDGET (object), GTK_NO_WINDOW);

	main_box = gtk_hbox_new (FALSE, 12);
	priv->main_box = main_box;
	gtk_widget_show (main_box);
	gtk_container_set_border_width (GTK_CONTAINER (main_box), 8);
	gtk_container_add (GTK_CONTAINER (object), main_box);

	/* Note that we connect to style-set on one of the internal
	 * widgets, not on the message area itself, since gtk does
	 * not deliver any further style-set signals for a widget on
	 * which the style has been forced with gtk_widget_set_style() */
	g_signal_connect (main_box,
			  "style-set",
			  G_CALLBACK (style_set),
			  object);

	priv->group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

	priv->image = gtk_image_new ();
	gtk_widget_show (priv->image);
	gtk_misc_set_alignment (GTK_MISC (priv->image), 0.5, 0.0);
	gtk_box_pack_start (GTK_BOX (main_box), priv->image, FALSE, FALSE, 0);

	priv->text_box = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (priv->text_box);
	gtk_box_pack_start (GTK_BOX (main_box), priv->text_box, TRUE, TRUE, 0);

	priv->primary = gtk_label_new (NULL);
	gtk_label_set_line_wrap_mode (GTK_LABEL (priv->primary), GTK_WRAP_WORD);
	gtk_label_set_line_wrap (GTK_LABEL (priv->primary), TRUE);
	gtk_size_group_add_widget (priv->group, priv->primary);
	gtk_misc_set_alignment (GTK_MISC (priv->primary), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (priv->text_box), priv->primary, TRUE, TRUE, 0);

	priv->button_box = gtk_vbox_new (FALSE, 8);
	gtk_widget_show (priv->button_box);
	gtk_box_pack_start (GTK_BOX (main_box),
			    priv->button_box,
			    FALSE,
			    FALSE,
			    0);
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

	g_object_unref (priv->group);
	priv->group = NULL;

	G_OBJECT_CLASS (brasero_disc_message_parent_class)->finalize (object);
}

static void
brasero_disc_message_size_request (GtkWidget *widget,
				   GtkRequisition *requisition)
{
	GtkBin *bin = GTK_BIN (widget);

	requisition->width = GTK_CONTAINER (widget)->border_width * 2;
	requisition->height = GTK_CONTAINER (widget)->border_width * 2;

	if (bin->child && GTK_WIDGET_VISIBLE (bin->child)) {
		GtkRequisition child_requisition;

		gtk_widget_size_request (bin->child, &child_requisition);

		requisition->width += child_requisition.width;
		requisition->height += child_requisition.height;
	}
}

static void
brasero_disc_message_size_allocate (GtkWidget *widget,
				    GtkAllocation *allocation)
{
	GtkBin *bin;
	GtkAllocation child_allocation;

	widget->allocation = *allocation;
	bin = GTK_BIN (widget);

	child_allocation.x = 0,
	child_allocation.y = 0;
	child_allocation.width = allocation->width;
	child_allocation.height = allocation->height;

	if (widget->window)
		gdk_window_move_resize (widget->window,
					allocation->x + GTK_CONTAINER (widget)->border_width,
					allocation->y + GTK_CONTAINER (widget)->border_width,
					child_allocation.width,
					child_allocation.height);

	if (bin->child)
		gtk_widget_size_allocate (bin->child, &child_allocation);
}

static void
brasero_disc_message_realize (GtkWidget *widget)
{
	GdkWindowAttr attributes;
	gint attributes_mask;

	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = widget->allocation.x;
	attributes.y = widget->allocation.y;
	attributes.width = widget->allocation.width;
	attributes.height = widget->allocation.height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual (widget);
	attributes.colormap = gtk_widget_get_colormap (widget);
	attributes.event_mask = gtk_widget_get_events (widget);
	attributes.event_mask |= GDK_EXPOSURE_MASK;
	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_COLORMAP;

	widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
					 &attributes,
					 attributes_mask);
	gdk_window_set_user_data (widget->window, widget);

	widget->style = gtk_style_attach (widget->style, widget->window);
    
	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
}

static gboolean
brasero_disc_message_expose_event (GtkWidget *widget,
				   GdkEventExpose *event)
{
	gtk_paint_flat_box (widget->style,
			    widget->window,
			    GTK_STATE_NORMAL,
			    GTK_SHADOW_OUT,
			    NULL,
			    widget,
			    "tooltip",
			    0,
			    0,
			    widget->allocation.width,
			    widget->allocation.height);

	GTK_WIDGET_CLASS (brasero_disc_message_parent_class)->expose_event (widget, event);

	return FALSE;
}

static void
brasero_disc_message_class_init (BraseroDiscMessageClass *klass)
{
	GtkWidgetClass* widget_class = GTK_WIDGET_CLASS (klass);
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	GtkBindingSet *binding_set;

	g_type_class_add_private (klass, sizeof (BraseroDiscMessagePrivate));

	object_class->finalize = brasero_disc_message_finalize;

	widget_class->expose_event = brasero_disc_message_expose_event;
	widget_class->realize = brasero_disc_message_realize;

	widget_class->size_request = brasero_disc_message_size_request;
	widget_class->size_allocate = brasero_disc_message_size_allocate;

	disc_message_signals[RESPONSE] =
		g_signal_new ("response",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_CLEANUP | G_SIGNAL_NO_RECURSE,
		              G_STRUCT_OFFSET (BraseroDiscMessageClass, response),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__INT,
		              G_TYPE_NONE, 1,
		              G_TYPE_INT);

	binding_set = gtk_binding_set_by_class (klass);
	gtk_binding_entry_add_signal (binding_set,
				      GDK_Escape,
				      0,
				      "response",
				      0,
				      G_TYPE_INT, GTK_RESPONSE_CLOSE);
}

GtkWidget *
brasero_disc_message_new (void)
{
	return g_object_new (BRASERO_TYPE_DISC_MESSAGE, NULL);
}
