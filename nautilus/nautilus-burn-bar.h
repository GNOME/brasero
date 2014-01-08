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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors: William Jon McCann <mccann@jhu.edu>
 *
 */

#ifndef __NAUTILUS_BURN_BAR_H
#define __NAUTILUS_BURN_BAR_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_DISC_BURN_BAR         (nautilus_disc_burn_bar_get_type ())
#define NAUTILUS_DISC_BURN_BAR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NAUTILUS_TYPE_DISC_BURN_BAR, NautilusDiscBurnBar))
#define NAUTILUS_DISC_BURN_BAR_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NAUTILUS_TYPE_DISC_BURN_BAR, NautilusDiscBurnBarClass))
#define NAUTILUS_IS_DISC_BURN_BAR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NAUTILUS_TYPE_DISC_BURN_BAR))
#define NAUTILUS_IS_DISC_BURN_BAR_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NAUTILUS_TYPE_DISC_BURN_BAR))
#define NAUTILUS_DISC_BURN_BAR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NAUTILUS_TYPE_DISC_BURN_BAR, NautilusDiscBurnBarClass))

typedef struct NautilusDiscBurnBarPrivate NautilusDiscBurnBarPrivate;

typedef struct
{
        GtkBox                     box;
        NautilusDiscBurnBarPrivate *priv;
} NautilusDiscBurnBar;

typedef struct
{
        GtkBoxClass          parent_class;

	void (* title_changed) (NautilusDiscBurnBar *bar);
	void (* icon_changed)  (NautilusDiscBurnBar *bar);
	void (* activate)      (NautilusDiscBurnBar *bar);

} NautilusDiscBurnBarClass;

GType       nautilus_disc_burn_bar_get_type          (void);
GtkWidget  *nautilus_disc_burn_bar_new               (void);

GtkWidget  *nautilus_disc_burn_bar_get_button        (NautilusDiscBurnBar *bar);

const gchar *
nautilus_disc_burn_bar_get_icon (NautilusDiscBurnBar *bar);

void
nautilus_disc_burn_bar_set_icon (NautilusDiscBurnBar *bar,
                                 const gchar *icon_path);

void
nautilus_disc_burn_bar_set_title (NautilusDiscBurnBar *bar,
                                  const gchar *title);

const gchar *
nautilus_disc_burn_bar_get_title (NautilusDiscBurnBar *bar);

G_END_DECLS

#endif /* __GS_BURN_BAR_H */
