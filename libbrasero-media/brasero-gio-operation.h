/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Brasero
 * Copyright (C) Rouquier Philippe 2009 <bonfire-app@wanadoo.fr>
 * 
 * Brasero is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Brasero is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _BRASERO_GIO_OPERATION_H_
#define _BRASERO_GIO_OPERATION_H_

#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

gboolean
brasero_gio_operation_umount (GVolume *gvolume,
			      GCancellable *cancel,
			      gboolean wait,
			      GError **error);

gboolean
brasero_gio_operation_mount (GVolume *gvolume,
			     GtkWindow *parent_window,
			     GCancellable *cancel,
			     gboolean wait,
			     GError **error);

gboolean
brasero_gio_operation_eject_volume (GVolume *gvolume,
				    GCancellable *cancel,
				    gboolean wait,
				    GError **error);

gboolean
brasero_gio_operation_eject_drive (GDrive *gdrive,
				   GCancellable *cancel,
				   gboolean wait,
				   GError **error);

G_END_DECLS

#endif /* _BRASERO_GIO_OPERATION_H_ */
