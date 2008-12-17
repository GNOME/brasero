/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
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
 * Authors: William Jon McCann <mccann@jhu.edu>
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "nautilus-burn-bar.h"

static void nautilus_disc_burn_bar_finalize   (GObject *object);

#define NAUTILUS_DISC_BURN_BAR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NAUTILUS_TYPE_DISC_BURN_BAR, NautilusDiscBurnBarPrivate))

struct NautilusDiscBurnBarPrivate
{
        GtkWidget   *button;
};

enum {
       ACTIVATE,
       LAST_SIGNAL
};

static guint           signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (NautilusDiscBurnBar, nautilus_disc_burn_bar, GTK_TYPE_HBOX)

GtkWidget *
nautilus_disc_burn_bar_get_button (NautilusDiscBurnBar *bar)
{
        GtkWidget *button;

        g_return_val_if_fail (bar != NULL, NULL);

        button = bar->priv->button;

        return button;
}

static void
nautilus_disc_burn_bar_set_property (GObject            *object,
                                guint               prop_id,
                                const GValue       *value,
                                GParamSpec         *pspec)
{
        NautilusDiscBurnBar *self;

        self = NAUTILUS_DISC_BURN_BAR (object);

        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
nautilus_disc_burn_bar_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
        NautilusDiscBurnBar *self;

        self = NAUTILUS_DISC_BURN_BAR (object);

        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
nautilus_disc_burn_bar_class_init (NautilusDiscBurnBarClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize     = nautilus_disc_burn_bar_finalize;
        object_class->get_property = nautilus_disc_burn_bar_get_property;
        object_class->set_property = nautilus_disc_burn_bar_set_property;

        g_type_class_add_private (klass, sizeof (NautilusDiscBurnBarPrivate));

        signals [ACTIVATE] = g_signal_new ("activate",
                                           G_TYPE_FROM_CLASS (klass),
                                           G_SIGNAL_RUN_LAST,
                                           G_STRUCT_OFFSET (NautilusDiscBurnBarClass, activate),
                                           NULL, NULL,
                                           g_cclosure_marshal_VOID__VOID,
                                           G_TYPE_NONE, 0);

}

static void
button_clicked_cb (GtkWidget       *button,
                   NautilusDiscBurnBar *bar)
{
        g_signal_emit (bar, signals [ACTIVATE], 0);
}

static void
nautilus_disc_burn_bar_init (NautilusDiscBurnBar *bar)
{
        GtkWidget   *label;
        GtkWidget   *hbox;

        bar->priv = NAUTILUS_DISC_BURN_BAR_GET_PRIVATE (bar);

        hbox = GTK_WIDGET (bar);

        label = gtk_label_new (_("CD/DVD Creator Folder"));
        gtk_widget_show (label);
        gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

        bar->priv->button = gtk_button_new_with_label (_("Write to Disc"));
        gtk_widget_show (bar->priv->button);
        gtk_box_pack_end (GTK_BOX (hbox), bar->priv->button, FALSE, FALSE, 0);

        g_signal_connect (bar->priv->button, "clicked",
                          G_CALLBACK (button_clicked_cb),
                          bar);

        gtk_widget_set_tooltip_text (bar->priv->button, _("Write contents to a CD or DVD disc"));
}

static void
nautilus_disc_burn_bar_finalize (GObject *object)
{
        NautilusDiscBurnBar *bar;

        g_return_if_fail (object != NULL);
        g_return_if_fail (NAUTILUS_IS_DISC_BURN_BAR (object));

        bar = NAUTILUS_DISC_BURN_BAR (object);

        g_return_if_fail (bar->priv != NULL);

        G_OBJECT_CLASS (nautilus_disc_burn_bar_parent_class)->finalize (object);
}

GtkWidget *
nautilus_disc_burn_bar_new (void)
{
        GObject *result;

        result = g_object_new (NAUTILUS_TYPE_DISC_BURN_BAR,
                               NULL);

        return GTK_WIDGET (result);
}
