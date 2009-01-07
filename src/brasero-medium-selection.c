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
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtk.h>

#include "brasero-medium-selection.h"
#include "brasero-utils.h"

#include "burn-medium.h"
#include "burn-volume-obj.h"
#include "burn-basics.h"
#include "burn-medium-monitor.h"

typedef struct _BraseroMediumSelectionPrivate BraseroMediumSelectionPrivate;
struct _BraseroMediumSelectionPrivate
{
	BraseroMediaType type;
	gulong added_sig;
	gulong removed_sig;
};

#define BRASERO_MEDIUM_SELECTION_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_MEDIUM_SELECTION, BraseroMediumSelectionPrivate))

typedef enum {
	ADDED_SIGNAL,
	REMOVED_SIGNAL,
	LAST_SIGNAL
} BraseroMediumSelectionSignalType;

static guint brasero_medium_selection_signals [LAST_SIGNAL] = { 0 };

enum {
	MEDIUM_COL,
	NAME_COL,
	ICON_COL,
	NUM_COL
};

G_DEFINE_TYPE (BraseroMediumSelection, brasero_medium_selection, GTK_TYPE_COMBO_BOX);

void
brasero_medium_selection_foreach (BraseroMediumSelection *selection,
				  BraseroMediumSelectionFunc function,
				  gpointer callback_data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (selection));

	if (!gtk_tree_model_get_iter_first (model, &iter))
		return;

	do {
		BraseroMedium *medium;

		medium = NULL;
		gtk_tree_model_get (model, &iter,
				    MEDIUM_COL, &medium,
				    -1);

		/* The following can happen when there isn't any medium */
		if (!medium)
			return;

		if (!function (medium, callback_data))
			break;

	} while (gtk_tree_model_iter_next (model, &iter));
}

static gchar *
brasero_medium_selection_get_medium_string (BraseroMediumSelection *self,
					    BraseroMedium *medium)
{
	gchar *label;
	gint64 size = 0;
	gchar *size_string;
	gchar *medium_name;
	BraseroMedia media;
	BraseroMediumSelectionClass *klass;

	klass = BRASERO_MEDIUM_SELECTION_GET_CLASS (self);
	if (klass->format_medium_string) {
		gchar *label;

		label = klass->format_medium_string (self, medium);
		if (label)
			return label;
	}

	medium_name = brasero_volume_get_name (BRASERO_VOLUME (medium));
	if (brasero_medium_get_status (medium) & BRASERO_MEDIUM_FILE)
		return medium_name;

	media = brasero_medium_get_status (medium);
	if (media & BRASERO_MEDIUM_BLANK) {
		/* NOTE for translators, the first %s is the medium name */
		label = g_strdup_printf (_("%s: empty"), medium_name);
		g_free (medium_name);

		return label;
	}

	brasero_medium_get_data_size (medium,
				      &size,
				      NULL);

	/* format the size */
	if (media & BRASERO_MEDIUM_HAS_DATA) {
		size_string = g_format_size_for_display (size);
		/* NOTE for translators: the first %s is the medium name, the
		 * second %s is the space (kio, gio) used by data on the disc.
		 */
		label = g_strdup_printf (_("%s: %s"),
					 medium_name,
					 size_string);
	}
	else {
		size_string = brasero_units_get_time_string_from_size (size,
								       TRUE,
								       TRUE);
		/* NOTE for translators: the first %s is the medium name, the
		 * second %s is the space (time) used by data on the disc.
		 * I really don't know if I should set this string as
		 * translatable. */
		label = g_strdup_printf (_("%s: %s"),
					 medium_name,
					 size_string);
	}

	g_free (medium_name);
	g_free (size_string);

	return label;
}

void
brasero_medium_selection_update_media_string (BraseroMediumSelection *self)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean valid;

	valid = FALSE;
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (self));
	if (!gtk_tree_model_get_iter_first (model, &iter))
		return;

	do {
		BraseroMedium *medium;
		gchar *label;

		medium = NULL;
		gtk_tree_model_get (model, &iter,
				    MEDIUM_COL, &medium,
				    -1);
		if (!medium)
			continue;

		label = brasero_medium_selection_get_medium_string (self, medium);
		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				    NAME_COL, label,
				    -1);

		g_object_unref (medium);
		g_free (label);
	} while (gtk_tree_model_iter_next (model, &iter));
}

static void
brasero_drive_selection_set_tooltip (BraseroMediumSelection *self)
{
	BraseroMediumSelectionPrivate *priv;
	BraseroMedium *medium;
	gchar *tooltip;

	priv = BRASERO_MEDIUM_SELECTION_PRIVATE (self);

	medium = brasero_medium_selection_get_active (self);
	if (medium) {
		tooltip = brasero_medium_get_tooltip (medium);
		g_object_unref (medium);
	}
	else
		tooltip = NULL;

	gtk_widget_set_tooltip_text (GTK_WIDGET (self), tooltip);
	g_free (tooltip);
}

static void
brasero_medium_selection_changed (GtkComboBox *box)
{
	brasero_drive_selection_set_tooltip (BRASERO_MEDIUM_SELECTION (box));
}

gboolean
brasero_medium_selection_set_active (BraseroMediumSelection *self,
				     BraseroMedium *medium)
{
	gboolean result = FALSE;
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (self));
	if (!gtk_tree_model_get_iter_first (model, &iter))
		return FALSE;

	do {
		BraseroMedium *iter_medium;

		gtk_tree_model_get (model, &iter,
				    MEDIUM_COL, &iter_medium,
				    -1);

		if (medium == iter_medium) {
			if (iter_medium)
				g_object_unref (iter_medium);

			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (self), &iter);
			result = TRUE;
			break;
		}

		g_object_unref (iter_medium);
	} while (gtk_tree_model_iter_next (model, &iter));

	return result;
}

BraseroMedium *
brasero_medium_selection_get_active (BraseroMediumSelection *self)
{
	BraseroMedium *medium;
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (self));
	if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (self), &iter))
		return NULL;

	gtk_tree_model_get (model, &iter,
			    MEDIUM_COL, &medium,
			    -1);
	return medium;
}

static void
brasero_medium_selection_update_no_disc_entry (BraseroMediumSelection *self,
					       GtkTreeModel *model,
					       GtkTreeIter *iter)
{
	BraseroMediumMonitor *monitor;
	GIcon *icon;

	monitor = brasero_medium_monitor_get_default ();
	if (brasero_medium_monitor_is_probing (monitor)) {
		icon = g_themed_icon_new_with_default_fallbacks ("image-loading");
		gtk_list_store_set (GTK_LIST_STORE (model), iter,
				    NAME_COL, _("Searching for available discs"),
				    ICON_COL, icon,
				    -1);
	}
	else {
		icon = g_themed_icon_new_with_default_fallbacks ("drive-optical");
		gtk_list_store_set (GTK_LIST_STORE (model), iter,
				    NAME_COL, _("No available disc"),
				    ICON_COL, icon,
				    -1);
	}

	g_object_unref (icon);
	g_object_unref (monitor);

	gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);
	gtk_combo_box_set_active_iter (GTK_COMBO_BOX (self), iter);
}

static void
brasero_medium_selection_add_no_disc_entry (BraseroMediumSelection *self)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	BraseroMediumSelectionPrivate *priv;

	priv = BRASERO_MEDIUM_SELECTION_PRIVATE (self);

	/* Nothing's available. Say it. Two cases here, either we're
	 * still probing drives or there isn't actually any available
	 * medium. */
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (self));
	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	brasero_medium_selection_update_no_disc_entry (self, model, &iter);
}

void
brasero_medium_selection_show_type (BraseroMediumSelection *self,
				    BraseroMediaType type)
{
	BraseroMediumSelectionPrivate *priv;
	BraseroMediumMonitor *monitor;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GSList *list;
	GSList *item;

	priv = BRASERO_MEDIUM_SELECTION_PRIVATE (self);

	priv->type = type;

	monitor = brasero_medium_monitor_get_default ();
	list = brasero_medium_monitor_get_media (monitor, type);
	g_object_unref (monitor);

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (self));
	if (gtk_tree_model_get_iter_first (model, &iter)) {
		/* First filter */
		do {
			GSList *node;
			BraseroMedium *medium;

			gtk_tree_model_get (model, &iter,
					    MEDIUM_COL, &medium,
					    -1);

			if (!medium) {
				/* That's the dummy line saying there isn't any
				 * available medium for whatever action it is */
				gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
				break;
			}

			node = g_slist_find (list, medium);
			g_object_unref (medium);

			if (!node) {
				if (gtk_list_store_remove (GTK_LIST_STORE (model), &iter)) {
					g_signal_emit (self,
						       brasero_medium_selection_signals [REMOVED_SIGNAL],
						       0);
					continue;
				}

				/* no more iter in the tree  get out */
				g_signal_emit (self,
					       brasero_medium_selection_signals [REMOVED_SIGNAL],
					       0);
				break;
			}

			g_object_unref (node->data);
			list = g_slist_delete_link (list, node);
		} while (gtk_tree_model_iter_next (model, &iter));
	}

	if (list) {
		/* add remaining media */
		for (item = list; item; item = item->next) {
			gchar *medium_name;
			GIcon *medium_icon;
			BraseroMedium *medium;

			medium = item->data;

			medium_name = brasero_medium_selection_get_medium_string (self, medium);
			medium_icon = brasero_volume_get_icon (BRASERO_VOLUME (medium));

			gtk_list_store_append (GTK_LIST_STORE (model), &iter);
			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					    MEDIUM_COL, medium,
					    NAME_COL, medium_name,
					    ICON_COL, medium_icon,
					    -1);
			g_free (medium_name);
			g_signal_emit (self,
				       brasero_medium_selection_signals [ADDED_SIGNAL],
				       0);
		}
		g_slist_foreach (list, (GFunc) g_object_unref, NULL);
		g_slist_free (list);
	}

	if (!gtk_tree_model_get_iter_first (model, &iter)) {
		brasero_medium_selection_add_no_disc_entry (self);
		return;
	}

	gtk_widget_set_sensitive (GTK_WIDGET (self), TRUE);
	if (gtk_combo_box_get_active (GTK_COMBO_BOX (self)) == -1)
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (self), &iter);
}

guint
brasero_medium_selection_get_drive_num (BraseroMediumSelection *self)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	int num = 0;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (self));
	if (!gtk_tree_model_get_iter_first (model, &iter))
		return 0;

	do {
		BraseroMedium *medium;

		medium = NULL;
		gtk_tree_model_get (model, &iter,
				    MEDIUM_COL, &medium,
				    -1);
		if (!medium)
			continue;

		g_object_unref (medium);
		num ++;

	} while (gtk_tree_model_iter_next (model, &iter));

	return num;
}

static void
brasero_medium_selection_medium_added_cb (BraseroMediumMonitor *monitor,
					  BraseroMedium *medium,
					  BraseroMediumSelection *self)
{
	BraseroMediumSelectionPrivate *priv;
	gboolean add = FALSE;
	GtkTreeModel *model;
	BraseroDrive *drive;
	gchar *medium_name;
	GIcon *medium_icon;
	GtkTreeIter iter;

	priv = BRASERO_MEDIUM_SELECTION_PRIVATE (self);

	drive = brasero_medium_get_drive (medium);
	if ((priv->type & BRASERO_MEDIA_TYPE_ANY_IN_BURNER)
	&&  (brasero_drive_can_write (drive)))
		add = TRUE;

	if ((priv->type & BRASERO_MEDIA_TYPE_READABLE)
	&&  (brasero_medium_get_status (medium) & (BRASERO_MEDIUM_HAS_AUDIO|BRASERO_MEDIUM_HAS_DATA)))
		add = TRUE;

	if (priv->type & BRASERO_MEDIA_TYPE_WRITABLE) {
		if (brasero_medium_can_be_written (medium))
			add = TRUE;
	}

	if (priv->type & BRASERO_MEDIA_TYPE_REWRITABLE) {
		if (brasero_medium_can_be_rewritten (medium))
			add = TRUE;
	}

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (self));

	if (!add) {
		/* Try to get the first iter (it shouldn't fail) */
		if (!gtk_tree_model_get_iter_first (model, &iter)) {
			brasero_medium_selection_add_no_disc_entry (self);
			return;
		}

		/* See if that's a real medium or not; if so, return. */
		medium = NULL;
		gtk_tree_model_get (model, &iter,
				    MEDIUM_COL, &medium,
				    -1);
		if (medium)
			return;

		brasero_medium_selection_update_no_disc_entry (self, model, &iter);
		return;
	}

	/* remove warning message */
	if (gtk_tree_model_get_iter_first (model, &iter)) {
		BraseroMedium *tmp;

		gtk_tree_model_get (model, &iter,
				    MEDIUM_COL, &tmp,
				    -1);
		if (!tmp)
			gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
		else
			g_object_unref (tmp);
	}

	medium_name = brasero_medium_selection_get_medium_string (self, medium);
	medium_icon = brasero_volume_get_icon (BRASERO_VOLUME (medium));
	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			    MEDIUM_COL, medium,
			    NAME_COL, medium_name,
			    ICON_COL, medium_icon,
			    -1);
	g_free (medium_name);

	gtk_widget_set_sensitive (GTK_WIDGET (self), TRUE);
	if (gtk_combo_box_get_active (GTK_COMBO_BOX (self)) == -1)
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (self), &iter);

	g_signal_emit (self,
		       brasero_medium_selection_signals [ADDED_SIGNAL],
		       0);
}

static void
brasero_medium_selection_medium_removed_cb (BraseroMediumMonitor *monitor,
					    BraseroMedium *medium,
					    BraseroMediumSelection *self)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (self));
	if (!gtk_tree_model_get_iter_first (model, &iter))
		return;

	do {
		BraseroMedium *iter_medium;

		gtk_tree_model_get (model, &iter,
				    MEDIUM_COL, &iter_medium,
				    -1);

		if (medium == iter_medium) {
			g_object_unref (iter_medium);
			gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
			g_signal_emit (self,
				       brasero_medium_selection_signals [REMOVED_SIGNAL],
				       0);
			break;
		}

		/* Could be NULL if a message "there is no medium ..." is on */
		if (iter_medium)
			g_object_unref (iter_medium);

	} while (gtk_tree_model_iter_next (model, &iter));

	if (!gtk_tree_model_get_iter_first (model, &iter)) {
		brasero_medium_selection_add_no_disc_entry (self);
		return;
	}

	if (gtk_combo_box_get_active (GTK_COMBO_BOX (self)) == -1)
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (self), &iter);
}

static void
brasero_medium_selection_init (BraseroMediumSelection *object)
{
	GtkListStore *model;
	GtkCellRenderer *renderer;
	BraseroMediumMonitor *monitor;
	BraseroMediumSelectionPrivate *priv;

	priv = BRASERO_MEDIUM_SELECTION_PRIVATE (object);

	monitor = brasero_medium_monitor_get_default ();
	priv->added_sig = g_signal_connect (monitor,
					    "medium-added",
					    G_CALLBACK (brasero_medium_selection_medium_added_cb),
					    object);
	priv->removed_sig = g_signal_connect (monitor,
					      "medium-removed",
					      G_CALLBACK (brasero_medium_selection_medium_removed_cb),
					      object);

	g_object_unref (monitor);

	/* get the list and fill the model */
	model = gtk_list_store_new (NUM_COL,
				    G_TYPE_OBJECT,
				    G_TYPE_STRING,
				    G_TYPE_ICON);

	gtk_combo_box_set_model (GTK_COMBO_BOX (object), GTK_TREE_MODEL (model));
	g_object_unref (model);

	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (renderer, "follow-state", TRUE, NULL);
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (object), renderer, FALSE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (object), renderer,
					"gicon", ICON_COL,
					NULL);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "xpad", 8, NULL);
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (object), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (object), renderer,
					"markup", NAME_COL,
					NULL);
}

static void
brasero_medium_selection_finalize (GObject *object)
{
	BraseroMediumSelectionPrivate *priv;
	BraseroMediumMonitor *monitor;

	priv = BRASERO_MEDIUM_SELECTION_PRIVATE (object);

	monitor = brasero_medium_monitor_get_default ();

	g_signal_handler_disconnect (monitor, priv->added_sig);
	g_signal_handler_disconnect (monitor, priv->removed_sig);
	priv->removed_sig = 0;
	priv->added_sig = 0;

	g_object_unref (monitor);

	G_OBJECT_CLASS (brasero_medium_selection_parent_class)->finalize (object);
}

static void
brasero_medium_selection_class_init (BraseroMediumSelectionClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	GtkComboBoxClass *combo_class = GTK_COMBO_BOX_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroMediumSelectionPrivate));

	object_class->finalize = brasero_medium_selection_finalize;

	combo_class->changed = brasero_medium_selection_changed;

	brasero_medium_selection_signals [ADDED_SIGNAL] =
	    g_signal_new ("medium_added",
			  BRASERO_TYPE_MEDIUM_SELECTION,
			  G_SIGNAL_RUN_FIRST|G_SIGNAL_ACTION|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL,
			  NULL,
			  g_cclosure_marshal_VOID__VOID,
			  G_TYPE_NONE,
			  0);
	brasero_medium_selection_signals [REMOVED_SIGNAL] =
	    g_signal_new ("medium_removed",
			  BRASERO_TYPE_MEDIUM_SELECTION,
			  G_SIGNAL_RUN_FIRST|G_SIGNAL_ACTION|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL,
			  NULL,
			  g_cclosure_marshal_VOID__VOID,
			  G_TYPE_NONE,
			  0);
}

GtkWidget *
brasero_medium_selection_new (void)
{
	return g_object_new (BRASERO_TYPE_MEDIUM_SELECTION, NULL);
}
