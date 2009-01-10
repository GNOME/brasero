/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8 -*-
 *
 * brasero-drive-selection.c
 *
 * Copyright (C) 2002-2004 Bastien Nocera <hadess@hadess.net>
 * Copyright (C) 2005-2006 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2009      Philippe Rouquier <bonfire-app@wanadoo.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors: Bastien Nocera <hadess@hadess.net>
 *          William Jon McCann <mccann@jhu.edu>
 *
 */

#include "config.h"

#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "brasero-drive-selection.h"
#include "burn-medium-monitor.h"

/* Signals */
enum {
        DRIVE_CHANGED,
        LAST_SIGNAL
};

/* Arguments */
enum {
        PROP_0,
        PROP_DRIVE,
        PROP_DRIVE_TYPE,
        PROP_RECORDERS_ONLY,
};

enum {
        DISPLAY_NAME_COLUMN,
        DRIVE_COLUMN,
        N_COLUMNS
};

#define BRASERO_DRIVE_SELECTION_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_DRIVE_SELECTION, BraseroDriveSelectionPrivate))

typedef struct BraseroDriveSelectionPrivate BraseroDriveSelectionPrivate;

struct BraseroDriveSelectionPrivate {
        BraseroMediumMonitor *monitor;

        BraseroDrive        *selected_drive;
        BraseroDriveType     type;
};

#define BRASERO_DRIVE_SELECTION_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_DRIVE_SELECTION, BraseroDriveSelectionPrivate))

static void brasero_drive_selection_init         (BraseroDriveSelection *selection);

static void brasero_drive_selection_set_property (GObject      *object,
                                                  guint         property_id,
                                                  const GValue *value,
                                                  GParamSpec   *pspec);
static void brasero_drive_selection_get_property (GObject      *object,
                                                  guint         property_id,
                                                  GValue       *value,
                                                  GParamSpec   *pspec);

static void brasero_drive_selection_finalize     (GObject      *object);

static int brasero_drive_selection_table_signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (BraseroDriveSelection, brasero_drive_selection, GTK_TYPE_COMBO_BOX);

static void
brasero_drive_selection_class_init (BraseroDriveSelectionClass *klass)
{
        GObjectClass *object_class;
        GtkWidgetClass *widget_class;

        object_class = (GObjectClass *) klass;
        widget_class = (GtkWidgetClass *) klass;

        /* GObject */
        object_class->set_property = brasero_drive_selection_set_property;
        object_class->get_property = brasero_drive_selection_get_property;
        object_class->finalize = brasero_drive_selection_finalize;

        g_type_class_add_private (klass, sizeof (BraseroDriveSelectionPrivate));

        /* Properties */
        g_object_class_install_property (object_class,
                                         PROP_DRIVE,
                                         g_param_spec_object ("drive",
                                                              _("Drive"),
                                                              NULL,
                                                              BRASERO_TYPE_DRIVE,
                                                              G_PARAM_READWRITE));
        g_object_class_install_property (object_class, PROP_DRIVE_TYPE,
                                         g_param_spec_uint ("drive-type", NULL, NULL,
                                                            0, 255, BRASERO_DRIVE_TYPE_ALL_BUT_FILE,
                                                            G_PARAM_READWRITE));

        /* Signals */
        brasero_drive_selection_table_signals [DRIVE_CHANGED] =
                g_signal_new ("drive-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (BraseroDriveSelectionClass,
                                               drive_changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__OBJECT,
                              G_TYPE_NONE, 1,
                              BRASERO_TYPE_DRIVE);
}

static void
brasero_drive_selection_set_drive_internal (BraseroDriveSelection *selection,
                                            BraseroDrive          *drive)
{
        BraseroDriveSelectionPrivate *priv;

        priv = BRASERO_DRIVE_SELECTION_PRIVATE (selection);
        priv->selected_drive = g_object_ref (drive);

        g_signal_emit (G_OBJECT (selection),
                       brasero_drive_selection_table_signals [DRIVE_CHANGED],
                       0, drive);

        g_object_notify (G_OBJECT (selection), "drive");
}

static void
combo_changed (GtkComboBox                *combo,
               BraseroDriveSelection *selection)
{
        BraseroDrive      *drive;
        GtkTreeModel      *model;
        GtkTreeIter        iter;

        if (! gtk_combo_box_get_active_iter (GTK_COMBO_BOX (selection), &iter)) {
                return;
        }

        model = gtk_combo_box_get_model (GTK_COMBO_BOX (selection));
        gtk_tree_model_get (model, &iter, DRIVE_COLUMN, &drive, -1);

        if (drive == NULL) {
                return;
        }

        brasero_drive_selection_set_drive_internal (selection, drive);
}

static void
selection_update_sensitivity (BraseroDriveSelection *selection)
{
        GtkTreeModel *model;
        int           num_drives;

        model = gtk_combo_box_get_model (GTK_COMBO_BOX (selection));
        num_drives = gtk_tree_model_iter_n_children (model, NULL);

        gtk_widget_set_sensitive (GTK_WIDGET (selection), (num_drives > 0));
}

static gboolean
get_iter_for_drive (BraseroDriveSelection *selection,
                    BraseroDrive          *drive,
                    GtkTreeIter           *iter)
{
        GtkTreeModel      *model;
        gboolean           found;

        found = FALSE;

        model = gtk_combo_box_get_model (GTK_COMBO_BOX (selection));
        if (! gtk_tree_model_get_iter_first (model, iter)) {
                goto out;
        }

        do {
                BraseroDrive *drive2;

                gtk_tree_model_get (model, iter, DRIVE_COLUMN, &drive2, -1);

                if (drive == drive2) {
                        found = TRUE;
                        break;
                }

        } while (gtk_tree_model_iter_next (model, iter));
 out:
        return found;
}

static void
selection_append_drive (BraseroDriveSelection *selection,
                        BraseroDrive          *drive)
{
        char         *display_name;
        GtkTreeIter   iter;
        GtkTreeModel *model;

        display_name = brasero_drive_get_display_name (drive);

        model = gtk_combo_box_get_model (GTK_COMBO_BOX (selection));
        gtk_list_store_append (GTK_LIST_STORE (model), &iter);
        gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                            DISPLAY_NAME_COLUMN, display_name ? display_name : _("Unnamed CD/DVD Drive"),
                            DRIVE_COLUMN, drive,
                            -1);

        g_free (display_name);
}

static void
selection_remove_drive (BraseroDriveSelection *selection,
                        BraseroDrive          *drive)
{
        gboolean                      found;
        GtkTreeIter                   iter;
        GtkTreeModel                 *model;
        BraseroDriveSelectionPrivate *priv;

        priv = BRASERO_DRIVE_SELECTION_PRIVATE (selection);
        found = get_iter_for_drive (selection, drive, &iter);
        if (! found) {
                return;
        }

        model = gtk_combo_box_get_model (GTK_COMBO_BOX (selection));
        gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

        if (priv->selected_drive != NULL
        && (drive == priv->selected_drive)) {
                if (gtk_tree_model_get_iter_first (model, &iter)) {
                        gtk_combo_box_set_active_iter (GTK_COMBO_BOX (selection), &iter);
                }
        }
}

static void
populate_model (BraseroDriveSelection *selection,
                GtkListStore          *store)
{
        GSList                       *drives;
        BraseroDrive                 *drive;
        BraseroMediumMonitor         *monitor;
        BraseroDriveSelectionPrivate *priv;

        priv = BRASERO_DRIVE_SELECTION_PRIVATE (selection);
        monitor = brasero_medium_monitor_get_default ();
        drives = brasero_medium_monitor_get_drives (monitor, priv->type);
        while (drives != NULL) {
                drive = drives->data;

                selection_append_drive (selection, drive);

                if (drive != NULL) {
                        g_object_unref (drive);
                }
                drives = g_slist_delete_link (drives, drives);
        }

        gtk_combo_box_set_active (GTK_COMBO_BOX (selection), 0);
}

static void
drive_connected_cb (BraseroMediumMonitor   *monitor,
                    BraseroDrive          *drive,
                    BraseroDriveSelection *selection)
{
        selection_append_drive (selection, drive);

        selection_update_sensitivity (selection);
}

static void
drive_disconnected_cb (BraseroMediumMonitor   *monitor,
                       BraseroDrive          *drive,
                       BraseroDriveSelection *selection)
{
        selection_remove_drive (selection, drive);

        selection_update_sensitivity (selection);
}

static void
brasero_drive_selection_init (BraseroDriveSelection *selection)
{
        GtkCellRenderer              *cell;
        GtkListStore                 *store;
        BraseroDriveSelectionPrivate *priv;

        priv = BRASERO_DRIVE_SELECTION_PRIVATE (selection);

        priv->monitor = brasero_medium_monitor_get_default ();

        g_signal_connect (priv->monitor, "drive-added", G_CALLBACK (drive_connected_cb), selection);
        g_signal_connect (priv->monitor, "drive-removed", G_CALLBACK (drive_disconnected_cb), selection);

        store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, BRASERO_TYPE_DRIVE);
        gtk_combo_box_set_model (GTK_COMBO_BOX (selection),
                                 GTK_TREE_MODEL (store));

        cell = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (selection), cell, TRUE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (selection), cell,
                                        "text", DISPLAY_NAME_COLUMN,
                                        NULL);

        populate_model (selection, store);

        selection_update_sensitivity (selection);

        g_signal_connect (G_OBJECT (selection), "changed",
                          G_CALLBACK (combo_changed), selection);

}

static void
brasero_drive_selection_finalize (GObject *object)
{
        BraseroDriveSelection *selection = (BraseroDriveSelection *) object;
        BraseroDriveSelectionPrivate *priv;

        g_return_if_fail (selection != NULL);
        g_return_if_fail (BRASERO_IS_DRIVE_SELECTION (selection));

        priv = BRASERO_DRIVE_SELECTION_PRIVATE (selection);

        g_signal_handlers_disconnect_by_func (priv->monitor, G_CALLBACK (drive_connected_cb), selection);
        g_signal_handlers_disconnect_by_func (priv->monitor, G_CALLBACK (drive_disconnected_cb), selection);

        if (priv->selected_drive != NULL) {
                g_object_unref (priv->selected_drive);
        }

        if (G_OBJECT_CLASS (brasero_drive_selection_parent_class)->finalize != NULL) {
                (* G_OBJECT_CLASS (brasero_drive_selection_parent_class)->finalize) (object);
        }
}

/**
 * brasero_drive_selection_new:
 *
 * Create a new drive selector.
 *
 * Return value: Newly allocated #BraseroDriveSelection widget
 **/
GtkWidget *
brasero_drive_selection_new (void)
{
        GtkWidget *widget;

        widget = GTK_WIDGET
                (g_object_new (BRASERO_TYPE_DRIVE_SELECTION, NULL));

        return widget;
}

static void
repopulate_model (BraseroDriveSelection *selection)
{
        GtkTreeModel *model;

        /* block the combo changed signal handler until we're done */
        g_signal_handlers_block_by_func (G_OBJECT (selection),
                                         combo_changed, selection);

        model = gtk_combo_box_get_model (GTK_COMBO_BOX (selection));
        gtk_list_store_clear (GTK_LIST_STORE (model));
        populate_model (selection, GTK_LIST_STORE (model));

        g_signal_handlers_unblock_by_func (G_OBJECT (selection),
                                           combo_changed, selection);

        /* Force a signal out */
        combo_changed (GTK_COMBO_BOX (selection), (gpointer) selection);
}

void
brasero_drive_selection_show_type (BraseroDriveSelection          *selection,
                                   BraseroDriveType                type)
{
        BraseroDriveSelectionPrivate *priv;

        g_return_if_fail (selection != NULL);
        g_return_if_fail (BRASERO_IS_DRIVE_SELECTION (selection));

        priv = BRASERO_DRIVE_SELECTION_PRIVATE (selection);
        priv->type = type;

        repopulate_model (selection);
        selection_update_sensitivity (selection);
}

static void
brasero_drive_selection_set_property (GObject      *object,
                                      guint         property_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
        BraseroDriveSelection *selection;

        g_return_if_fail (BRASERO_IS_DRIVE_SELECTION (object));

        selection = BRASERO_DRIVE_SELECTION (object);

        switch (property_id) {
        case PROP_DRIVE:
                brasero_drive_selection_set_active (selection, g_value_get_object (value));
                break;
        case PROP_DRIVE_TYPE:
                brasero_drive_selection_show_type (selection, g_value_get_uint (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
}

static void
brasero_drive_selection_get_property (GObject    *object,
                                      guint       property_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
        BraseroDriveSelectionPrivate *priv;

        g_return_if_fail (BRASERO_IS_DRIVE_SELECTION (object));

        priv = BRASERO_DRIVE_SELECTION_PRIVATE (object);

        switch (property_id) {
        case PROP_DRIVE:
                g_value_set_object (value, priv->selected_drive);
                break;
        case PROP_DRIVE_TYPE:
                g_value_set_uint (value, priv->type);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
}

/**
 * brasero_drive_selection_set_active:
 * @selection: #BraseroDriveSelection
 * @drive: #BraseroDrive
 *
 * Set the current selected drive to that which corresponds to the
 * specified drive.
 *
 * Since: 2.14
 *
 **/
void
brasero_drive_selection_set_active (BraseroDriveSelection *selection,
                                          BraseroDrive          *drive)
{
        GtkTreeIter        iter;
        gboolean           found;

        g_return_if_fail (selection != NULL);
        g_return_if_fail (BRASERO_IS_DRIVE_SELECTION (selection));

        found = get_iter_for_drive (selection, drive, &iter);
        if (! found) {
                return;
        }

        gtk_combo_box_set_active_iter (GTK_COMBO_BOX (selection), &iter);
}

/**
 * brasero_drive_selection_get_active:
 * @selection: #BraseroDriveSelection
 *
 * Get the currently selected drive
 *
 * Return value: currently selected #BraseroDrive.  The drive must be
 * unreffed using nautilus_burn_drive_unref after use.
 *
 * Since: 2.14
 *
 **/
BraseroDrive *
brasero_drive_selection_get_active (BraseroDriveSelection *selection)
{
        BraseroDriveSelectionPrivate *priv;

        g_return_val_if_fail (selection != NULL, NULL);
        g_return_val_if_fail (BRASERO_IS_DRIVE_SELECTION (selection), NULL);

        priv = BRASERO_DRIVE_SELECTION_PRIVATE (selection);
        if (priv->selected_drive != NULL) {
                g_object_ref (priv->selected_drive);
        }

        return priv->selected_drive;
}

