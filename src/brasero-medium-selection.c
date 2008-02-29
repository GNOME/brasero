/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2008 <bonfire-app@wanadoo.fr>
 * 
 * brasero is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
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
#include "burn-medium.h"
#include "burn-basics.h"

typedef struct _BraseroMediumSelectionPrivate BraseroMediumSelectionPrivate;
struct _BraseroMediumSelectionPrivate
{
	BraseroMediaType type;
	gulong added_sig;
	gulong removed_sig;
};

#define BRASERO_MEDIUM_SELECTION_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_MEDIUM_SELECTION, BraseroMediumSelectionPrivate))

enum {
	MEDIUM_COL,
	NAME_COL,
	ICON_COL,
	NUM_COL
};

G_DEFINE_TYPE (BraseroMediumSelection, brasero_medium_selection, GTK_TYPE_COMBO_BOX);

enum {
	MEDIUM_CHANGED,
	LAST_SIGNAL
};
static gulong medium_selection_signals [LAST_SIGNAL];

static void
brasero_medium_selection_changed (GtkComboBox *box)
{
	GtkTreeIter iter;

	if (gtk_combo_box_get_active_iter (box, &iter))
		g_signal_emit (box,
			       medium_selection_signals [MEDIUM_CHANGED],
			       0);
}

void
brasero_medium_selection_set_active (BraseroMediumSelection *self,
				     BraseroMedium *medium)
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
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (self), &iter);
			g_signal_emit (self,
				       medium_selection_signals [MEDIUM_CHANGED],
				       0);
			break;
		}

	} while (gtk_tree_model_iter_next (model, &iter));
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

	if (!medium)
		return NULL;

	g_object_ref (medium);
	return medium;
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

			if (!g_slist_find (list, medium)) {
				if (gtk_list_store_remove (GTK_LIST_STORE (model), &iter))
					continue;

				break;
			}

			list = g_slist_remove (list, medium);
			g_object_unref (medium);
		} while (gtk_tree_model_iter_next (model, &iter));
	}

	if (list) {
		/* add remaining media */
		for (item = list; item; item = item->next) {
			gchar *medium_name;
			BraseroMedium *medium;
			const gchar *medium_icon;

			medium = item->data;

			medium_name = brasero_medium_get_label (medium, TRUE);
			medium_icon = brasero_medium_get_icon (medium);

			gtk_list_store_append (GTK_LIST_STORE (model), &iter);
			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					    MEDIUM_COL, medium,
					    NAME_COL, medium_name,
					    ICON_COL, medium_icon,
					    -1);
			g_free (medium_name);
		}
		g_slist_foreach (list, (GFunc) g_object_unref, NULL);
		g_slist_free (list);
	}

	if (!gtk_tree_model_get_iter_first (model, &iter)) {
		/* Nothing's available =(. Say it. */
		gtk_list_store_append (GTK_LIST_STORE (model), &iter);
		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				    NAME_COL, _("There is no available medium. Please insert one."),
				    -1),

		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (self), &iter);
		g_signal_emit (self,
			       medium_selection_signals [MEDIUM_CHANGED],
			       0);
		return;
	}

	if (gtk_combo_box_get_active (GTK_COMBO_BOX (self)) == -1) {
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (self), &iter);
		g_signal_emit (self,
			       medium_selection_signals [MEDIUM_CHANGED],
			       0);
	}
}

static void
brasero_medium_selection_medium_added_cb (BraseroMediumMonitor *monitor,
					  BraseroMedium *medium,
					  BraseroMediumSelection *self)
{
	BraseroMediumSelectionPrivate *priv;
	const gchar *medium_icon;
	gboolean add = FALSE;
	GtkTreeModel *model;
	gchar *medium_name;
	GtkTreeIter iter;

	priv = BRASERO_MEDIUM_SELECTION_PRIVATE (self);

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

	if (!add)
		return;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (self));

	/* remove warning message */
	if (gtk_tree_model_get_iter_first (model, &iter)) {
		BraseroMedium *tmp;

		gtk_tree_model_get (model, &iter,
				    MEDIUM_COL, &tmp,
				    -1);
		if (!medium)
			gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
	}

	medium_name = brasero_medium_get_label (medium, TRUE);
	medium_icon = brasero_medium_get_icon (medium);
	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			    MEDIUM_COL, medium,
			    NAME_COL, medium_name,
			    ICON_COL, medium_icon,
			    -1);
	g_free (medium_name);

	if (gtk_combo_box_get_active (GTK_COMBO_BOX (self)) == -1) {
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (self), &iter);
		g_signal_emit (self,
			       medium_selection_signals [MEDIUM_CHANGED],
			       0);
	}
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
			gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
			break;
		}
	} while (gtk_tree_model_iter_next (model, &iter));

	if (!gtk_tree_model_get_iter_first (model, &iter)) {
		/* Nothing's available any more =(. Say it. */
		gtk_list_store_append (GTK_LIST_STORE (model), &iter);
		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				    NAME_COL, _("There is no available medium. Please insert one."),
				    -1),

		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (self), &iter);
		g_signal_emit (self,
			       medium_selection_signals [MEDIUM_CHANGED],
			       0);
		return;
	}

	if (gtk_combo_box_get_active (GTK_COMBO_BOX (self)) == -1) {
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (self), &iter);
		g_signal_emit (self,
			       medium_selection_signals [MEDIUM_CHANGED],
			       0);
	}
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
				    G_TYPE_STRING);

	gtk_combo_box_set_model (GTK_COMBO_BOX (object), GTK_TREE_MODEL (model));

/*	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (renderer, "follow-state", TRUE, NULL);
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (object), renderer, FALSE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (object), renderer,
					"icon-name", ICON_COL,
					NULL);
*/
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
	priv->added_sig = 0;
	priv->removed_sig = 0;

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

	medium_selection_signals [MEDIUM_CHANGED] =
	    g_signal_new ("medium_changed",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_FIRST,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__VOID,
			  G_TYPE_NONE,
			  0,
			  G_TYPE_NONE);
}

GtkWidget *
brasero_medium_selection_new (void)
{
	return g_object_new (BRASERO_TYPE_MEDIUM_SELECTION, NULL);
}
