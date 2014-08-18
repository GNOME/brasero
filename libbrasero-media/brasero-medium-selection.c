/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-media
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-media is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-media authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-media. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-media is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-media is distributed in the hope that it will be useful,
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
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtk.h>

#include "brasero-medium-selection.h"
#include "brasero-medium-selection-priv.h"
#include "brasero-medium.h"
#include "brasero-volume.h"
#include "brasero-medium-monitor.h"
#include "brasero-units.h"

typedef struct _BraseroMediumSelectionPrivate BraseroMediumSelectionPrivate;
struct _BraseroMediumSelectionPrivate
{
	BraseroMedium *active;

	BraseroMediaType type;
	gulong added_sig;
	gulong removed_sig;
};

#define BRASERO_MEDIUM_SELECTION_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_MEDIUM_SELECTION, BraseroMediumSelectionPrivate))

typedef enum {
	CHANGED_SIGNAL,
	LAST_SIGNAL
} BraseroMediumSelectionSignalType;

static guint brasero_medium_selection_signals [LAST_SIGNAL] = { 0 };

enum {
	PROP_0,
	PROP_MEDIUM,
	PROP_MEDIA_TYPE
};

enum {
	MEDIUM_COL,
	NAME_COL,
	ICON_COL,
	USED_COL,
	VISIBLE_PROGRESS_COL,
	VISIBLE_TEXT_COL,
	NUM_COL
};

/* GtkBuildable */
static GtkBuildableIface *parent_buildable_iface;

static void
brasero_medium_selection_buildable_init (GtkBuildableIface *iface)
{
	parent_buildable_iface = g_type_interface_peek_parent (iface);
} 

G_DEFINE_TYPE_WITH_CODE (BraseroMediumSelection,
                         brasero_medium_selection,
                         GTK_TYPE_COMBO_BOX,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, brasero_medium_selection_buildable_init));

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

		if (!function (medium, callback_data)) {
			g_object_unref (medium);
			break;
		}

		g_object_unref (medium);
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
		size_string = g_format_size (size);
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
brasero_medium_selection_update_used_space (BraseroMediumSelection *selector,
					    BraseroMedium *medium_arg,
					    guint used_space)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (selector));
	if (!gtk_tree_model_get_iter_first (model, &iter))
		return;

	do {
		BraseroMedium *medium;

		medium = NULL;
		gtk_tree_model_get (model, &iter,
				    MEDIUM_COL, &medium,
				    -1);

		if (medium == medium_arg) {
			g_object_unref (medium);

			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					    USED_COL, used_space,
					    VISIBLE_PROGRESS_COL, (gboolean) (used_space > 0),
					    VISIBLE_TEXT_COL, (gboolean) (used_space <= 0),
					    -1);
			break;
		}

		g_object_unref (medium);
	} while (gtk_tree_model_iter_next (model, &iter));
}

static void
brasero_medium_selection_set_show_used_space (BraseroMediumSelection *selector)
{
	GtkCellRenderer *renderer;

	gtk_cell_layout_clear (GTK_CELL_LAYOUT (selector));

	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (renderer, "follow-state", TRUE, NULL);
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (selector), renderer, FALSE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (selector), renderer,
					"gicon", ICON_COL,
					NULL);

	renderer = gtk_cell_renderer_progress_new ();
	g_object_set (renderer, "xpad", 4, "text-xalign", 0.0, NULL);
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (selector), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (selector), renderer,
					"text", NAME_COL,
					"value", USED_COL,
					"visible", VISIBLE_PROGRESS_COL,
					NULL);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "xpad", 4, NULL);
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (selector), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (selector), renderer,
					"markup", NAME_COL,
					"visible", VISIBLE_TEXT_COL,
					NULL);
}

void
brasero_medium_selection_update_media_string (BraseroMediumSelection *self)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

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
brasero_medium_selection_set_current_medium (BraseroMediumSelection *self,
					     GtkTreeIter *iter)
{
	BraseroMediumSelectionPrivate *priv;
	BraseroMedium *medium = NULL;
	GtkTreeModel *model;

	priv = BRASERO_MEDIUM_SELECTION_PRIVATE (self);

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (self));
	gtk_tree_model_get (model, iter,
			    MEDIUM_COL, &medium,
			    -1);
	if (medium)
		gtk_widget_set_sensitive (GTK_WIDGET (self), TRUE);
	else
		gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);

	if (priv->active == medium) {
		if (medium)
			g_object_unref (medium);

		return;
	}

	if (priv->active)
		g_object_unref (priv->active);

	/* NOTE: no need to ref priv->active as medium was 
	 * already reffed in gtk_tree_model_get () */
	priv->active = medium;
	g_signal_emit (self,
		       brasero_medium_selection_signals [CHANGED_SIGNAL],
		       0,
		       priv->active);
}

static void
brasero_drive_selection_set_tooltip (BraseroMediumSelection *self)
{
	BraseroMedium *medium;
	gchar *tooltip;

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
brasero_medium_selection_changed (GtkComboBox *combo)
{
	GtkTreeIter iter;

	if (!gtk_combo_box_get_active_iter (combo, &iter))
		return;

	brasero_medium_selection_set_current_medium (BRASERO_MEDIUM_SELECTION (combo), &iter);
	brasero_drive_selection_set_tooltip (BRASERO_MEDIUM_SELECTION (combo));
}

/**
 * brasero_medium_selection_set_active:
 * @selector: a #BraseroMediumSelection
 * @medium: a #BraseroMedium to set as the active one in the selector
 *
 * Sets the active medium. Emits the ::medium-changed signal.
 *
 * Return value: a #gboolean. TRUE if it succeeded, FALSE otherwise.
 **/
gboolean
brasero_medium_selection_set_active (BraseroMediumSelection *selector,
				     BraseroMedium *medium)
{
	BraseroMediumSelectionPrivate *priv;
	gboolean result = FALSE;
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_val_if_fail (selector != NULL, FALSE);
	g_return_val_if_fail (BRASERO_IS_MEDIUM_SELECTION (selector), FALSE);

	priv = BRASERO_MEDIUM_SELECTION_PRIVATE (selector);
	if (priv->active == medium)
		return TRUE;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (selector));
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

			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (selector), &iter);
			brasero_medium_selection_set_current_medium (selector, &iter);
			result = TRUE;
			break;
		}

		if (iter_medium)
			g_object_unref (iter_medium);
	} while (gtk_tree_model_iter_next (model, &iter));

	return result;
}

/**
 * brasero_medium_selection_get_active:
 * @selector: a #BraseroMediumSelection
 *
 * Gets the active medium.
 *
 * Return value: a #BraseroMedium or NULL. Unref when it is not needed anymore.
 **/
BraseroMedium *
brasero_medium_selection_get_active (BraseroMediumSelection *selector)
{
	BraseroMediumSelectionPrivate *priv;
	
	g_return_val_if_fail (selector != NULL, NULL);
	g_return_val_if_fail (BRASERO_IS_MEDIUM_SELECTION (selector), NULL);

	priv = BRASERO_MEDIUM_SELECTION_PRIVATE (selector);
	if (!priv->active)
		return NULL;

	return g_object_ref (priv->active);
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
				    VISIBLE_TEXT_COL, TRUE,
				    VISIBLE_PROGRESS_COL, FALSE,
				    -1);
	}
	else {
		icon = g_themed_icon_new_with_default_fallbacks ("drive-optical");
		gtk_list_store_set (GTK_LIST_STORE (model), iter,
				    NAME_COL, _("No disc available"),
				    ICON_COL, icon,
				    VISIBLE_TEXT_COL, TRUE,
				    VISIBLE_PROGRESS_COL, FALSE,
				    -1);
	}

	g_object_unref (icon);
	g_object_unref (monitor);

	gtk_combo_box_set_active_iter (GTK_COMBO_BOX (self), iter);
	brasero_medium_selection_set_current_medium (self, iter);
}

static void
brasero_medium_selection_add_no_disc_entry (BraseroMediumSelection *self)
{
	GtkTreeIter iter;
	GtkTreeModel *model;

	/* Nothing's available. Say it. Two cases here, either we're
	 * still probing drives or there isn't actually any available
	 * medium. */
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (self));
	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	brasero_medium_selection_update_no_disc_entry (self, model, &iter);
}

/**
 * brasero_medium_selection_show_media_type:
 * @selector: a #BraseroMediumSelection
 * @type: a #BraseroMediaType
 *
 * Filters and displays media corresponding to @type.
 **/
void
brasero_medium_selection_show_media_type (BraseroMediumSelection *selector,
					  BraseroMediaType type)
{
	BraseroMediumSelectionPrivate *priv;
	BraseroMediumMonitor *monitor;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GSList *list;
	GSList *item;

	g_return_if_fail (selector != NULL);
	g_return_if_fail (BRASERO_IS_MEDIUM_SELECTION (selector));

	priv = BRASERO_MEDIUM_SELECTION_PRIVATE (selector);

	priv->type = type;

	monitor = brasero_medium_monitor_get_default ();
	list = brasero_medium_monitor_get_media (monitor, type);
	g_object_unref (monitor);

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (selector));
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
				if (gtk_list_store_remove (GTK_LIST_STORE (model), &iter))
					continue;

				/* no more iter in the tree get out */
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

			gtk_list_store_insert_with_values (GTK_LIST_STORE (model), &iter,
			                                   -1,
			                                   MEDIUM_COL, medium,
			                                   -1);

			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (selector), &iter);

			medium_name = brasero_medium_selection_get_medium_string (selector, medium);
			medium_icon = brasero_volume_get_icon (BRASERO_VOLUME (medium));
			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					    NAME_COL, medium_name,
					    ICON_COL, medium_icon,
					    VISIBLE_TEXT_COL, TRUE,
					    VISIBLE_PROGRESS_COL, FALSE,
					    -1);
			g_free (medium_name);
			g_object_unref (medium_icon);
		}
		g_slist_foreach (list, (GFunc) g_object_unref, NULL);
		g_slist_free (list);
	}

	if (!gtk_tree_model_get_iter_first (model, &iter)) {
		brasero_medium_selection_add_no_disc_entry (selector);
		return;
	}

	gtk_widget_set_sensitive (GTK_WIDGET (selector), TRUE);
	if (gtk_combo_box_get_active (GTK_COMBO_BOX (selector)) == -1) {
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (selector), &iter);
		brasero_medium_selection_set_current_medium (selector, &iter);
	}
}

/**
 * brasero_medium_selection_get_media_num:
 * @selector: a #BraseroMediumSelection
 * @type: a #BraseroMediaType
 *
 * Returns the number of media being currently displayed and available.
 *
 * Return value: a #guint
 **/
guint
brasero_medium_selection_get_media_num (BraseroMediumSelection *selector)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	int num = 0;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (selector));
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

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (self));

	/* Make sure it's not already in our list */
	if (gtk_tree_model_get_iter_first (model, &iter)) {
		do {
			BraseroMedium *tmp;

			tmp = NULL;
			gtk_tree_model_get (model, &iter,
					    MEDIUM_COL, &tmp,
					    -1);
			if (tmp == medium)
				return;

		} while (gtk_tree_model_iter_next (model, &iter));
	}

	/* Make sure it does fit the types of media to display */
	drive = brasero_medium_get_drive (medium);
	if ((priv->type & BRASERO_MEDIA_TYPE_CD) == priv->type
	&& (brasero_medium_get_status (medium) & BRASERO_MEDIUM_CD))
		add = TRUE;

	if ((priv->type & BRASERO_MEDIA_TYPE_ANY_IN_BURNER)
	&&  (brasero_drive_can_write (drive))) {
		if ((priv->type & BRASERO_MEDIA_TYPE_CD)) {
			if (brasero_medium_get_status (medium) & BRASERO_MEDIUM_CD)
				add = TRUE;
		}
		else
			add = TRUE;
	}

	if ((priv->type & BRASERO_MEDIA_TYPE_AUDIO)
	&&  (brasero_medium_get_status (medium) & BRASERO_MEDIUM_HAS_AUDIO)) {
		if ((priv->type & BRASERO_MEDIA_TYPE_CD)) {
			if (brasero_medium_get_status (medium) & BRASERO_MEDIUM_CD)
				add = TRUE;
		}
		else
			add = TRUE;
	}

	if ((priv->type & BRASERO_MEDIA_TYPE_DATA)
	&&  (brasero_medium_get_status (medium) & BRASERO_MEDIUM_HAS_DATA)) {
		if ((priv->type & BRASERO_MEDIA_TYPE_CD)) {
			if (brasero_medium_get_status (medium) & BRASERO_MEDIUM_CD)
				add = TRUE;
		}
		else
			add = TRUE;
	}

	if (priv->type & BRASERO_MEDIA_TYPE_WRITABLE) {
		if (brasero_medium_can_be_written (medium)) {
			if ((priv->type & BRASERO_MEDIA_TYPE_CD)) {
				if (brasero_medium_get_status (medium) & BRASERO_MEDIUM_CD)
					add = TRUE;
			}
			else
				add = TRUE;
		}
	}

	if (priv->type & BRASERO_MEDIA_TYPE_REWRITABLE) {
		if (brasero_medium_can_be_rewritten (medium)) {
			if ((priv->type & BRASERO_MEDIA_TYPE_CD)) {
				if (brasero_medium_get_status (medium) & BRASERO_MEDIUM_CD)
					add = TRUE;
			}
			else
				add = TRUE;
		}
	}

	if (!add) {
		BraseroMedium *tmp;

		/* Try to get the first iter (it shouldn't fail) */
		if (!gtk_tree_model_get_iter_first (model, &iter)) {
			brasero_medium_selection_add_no_disc_entry (self);
			return;
		}

		/* See if that's a real medium or not; if so, return. */
		tmp = NULL;
		gtk_tree_model_get (model, &iter,
				    MEDIUM_COL, &tmp,
				    -1);
		if (tmp) {
			g_object_unref (tmp);
			return;
		}

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

	gtk_list_store_insert_with_values (GTK_LIST_STORE (model), &iter,
	                                   -1,
	                                   MEDIUM_COL, medium,
	                                   -1);

	medium_name = brasero_medium_selection_get_medium_string (self, medium);
	medium_icon = brasero_volume_get_icon (BRASERO_VOLUME (medium));
	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			    NAME_COL, medium_name,
			    ICON_COL, medium_icon,
			    VISIBLE_TEXT_COL, TRUE,
			    VISIBLE_PROGRESS_COL, FALSE,
			    -1);
	g_free (medium_name);
	g_object_unref (medium_icon);

	gtk_widget_set_sensitive (GTK_WIDGET (self), TRUE);
	if (gtk_combo_box_get_active (GTK_COMBO_BOX (self)) == -1) {
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (self), &iter);
		brasero_medium_selection_set_current_medium (self, &iter);
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
			g_object_unref (iter_medium);
			gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
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

	if (gtk_combo_box_get_active (GTK_COMBO_BOX (self)) == -1) {
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (self), &iter);
		brasero_medium_selection_set_current_medium (self, &iter);
	}
}

static void
brasero_medium_selection_constructed (GObject *object)
{
	G_OBJECT_CLASS (brasero_medium_selection_parent_class)->constructed (object);

	brasero_medium_selection_set_show_used_space (BRASERO_MEDIUM_SELECTION (object));
}

static void
brasero_medium_selection_init (BraseroMediumSelection *object)
{
	GtkListStore *model;
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
				    G_TYPE_ICON,
				    G_TYPE_UINT,
				    G_TYPE_BOOLEAN,
				    G_TYPE_BOOLEAN);

	gtk_combo_box_set_model (GTK_COMBO_BOX (object), GTK_TREE_MODEL (model));
	g_object_unref (model);
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
brasero_medium_selection_set_property (GObject *object,
				       guint prop_id,
				       const GValue *value,
				       GParamSpec *pspec)
{
	g_return_if_fail (BRASERO_IS_MEDIUM_SELECTION (object));

	switch (prop_id)
	{
	case PROP_MEDIA_TYPE:
		brasero_medium_selection_show_media_type (BRASERO_MEDIUM_SELECTION (object),
							  g_value_get_uint (value));
		break;
	case PROP_MEDIUM:
		brasero_medium_selection_set_active (BRASERO_MEDIUM_SELECTION (object),
						     BRASERO_MEDIUM (g_value_get_object (value)));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
brasero_medium_selection_get_property (GObject *object,
				       guint prop_id,
				       GValue *value,
				       GParamSpec *pspec)
{
	BraseroMediumSelectionPrivate *priv;

	g_return_if_fail (BRASERO_IS_MEDIUM_SELECTION (object));

	priv = BRASERO_MEDIUM_SELECTION_PRIVATE (object);

	switch (prop_id)
	{
	case PROP_MEDIA_TYPE:
		g_value_set_uint (value, priv->type);
		break;
	case PROP_MEDIUM:
		g_value_set_object (value, brasero_medium_selection_get_active (BRASERO_MEDIUM_SELECTION (object)));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
brasero_medium_selection_class_init (BraseroMediumSelectionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkComboBoxClass *combo_class = GTK_COMBO_BOX_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroMediumSelectionPrivate));

	object_class->constructed = brasero_medium_selection_constructed;
	object_class->finalize = brasero_medium_selection_finalize;
	object_class->set_property = brasero_medium_selection_set_property;
	object_class->get_property = brasero_medium_selection_get_property;

	combo_class->changed = brasero_medium_selection_changed;

	g_object_class_install_property (object_class, PROP_MEDIUM,
					 g_param_spec_object ("medium",
							      "Selected medium",
							      "The currently selected medium",
							      BRASERO_TYPE_MEDIUM, G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_MEDIA_TYPE,
					 g_param_spec_uint ("media-type",
							    "The type of media",
							    "The type of media displayed",
							    0, BRASERO_MEDIA_TYPE_ALL,
							    BRASERO_MEDIA_TYPE_NONE,
							    G_PARAM_READWRITE));
	/**
 	* BraseroMediumSelection::medium-changed:
 	* @monitor: the object which received the signal
  	* @medium: the new selected medium
	*
 	* This signal gets emitted when the selected medium has changed.
 	**/
	brasero_medium_selection_signals [CHANGED_SIGNAL] =
	    g_signal_new ("medium_changed",
			  BRASERO_TYPE_MEDIUM_SELECTION,
			  G_SIGNAL_RUN_FIRST|G_SIGNAL_ACTION|G_SIGNAL_NO_RECURSE,
			  G_STRUCT_OFFSET (BraseroMediumSelectionClass, medium_changed),
			  NULL,
			  NULL,
			  g_cclosure_marshal_VOID__OBJECT,
			  G_TYPE_NONE,
			  1,
			  BRASERO_TYPE_MEDIUM);
}

/**
 * brasero_medium_selection_new:
 *
 * Creates a new #BraseroMediumSelection object
 *
 * Return value: a #GtkWidget. Unref when it is not needed anymore.
 **/
GtkWidget *
brasero_medium_selection_new (void)
{
	return g_object_new (BRASERO_TYPE_MEDIUM_SELECTION, NULL);
}
