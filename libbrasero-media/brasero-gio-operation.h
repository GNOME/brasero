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

#include <glib-object.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_GIO_OPERATION             (brasero_gio_operation_get_type ())
#define BRASERO_GIO_OPERATION(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_GIO_OPERATION, BraseroGioOperation))
#define BRASERO_GIO_OPERATION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_GIO_OPERATION, BraseroGioOperationClass))
#define BRASERO_IS_GIO_OPERATION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_GIO_OPERATION))
#define BRASERO_IS_GIO_OPERATION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_GIO_OPERATION))
#define BRASERO_GIO_OPERATION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_GIO_OPERATION, BraseroGioOperationClass))

typedef struct _BraseroGioOperationClass BraseroGioOperationClass;
typedef struct _BraseroGioOperation BraseroGioOperation;

struct _BraseroGioOperationClass
{
	GObjectClass parent_class;
};

struct _BraseroGioOperation
{
	GObject parent_instance;
};

GType brasero_gio_operation_get_type (void) G_GNUC_CONST;

BraseroGioOperation *
brasero_gio_operation_new (void);

void
brasero_gio_operation_cancel (BraseroGioOperation *operation);

G_END_DECLS

#endif /* _BRASERO_GIO_OPERATION_H_ */
