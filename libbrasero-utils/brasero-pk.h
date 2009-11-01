/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-burn
 * Copyright (C) Luis Medinas 2008 <lmedinas@gmail.com>
 * Copyright (C) Philippe Rouquier 2008 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-burn is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-burn authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-burn. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-burn is distributed in the hope that it will be useful,
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

#ifndef _BRASERO_PK_H_
#define _BRASERO_PK_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_PK             (brasero_pk_get_type ())
#define BRASERO_PK(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_PK, BraseroPK))
#define BRASERO_PK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_PK, BraseroPKClass))
#define BRASERO_IS_PK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_PK))
#define BRASERO_IS_PK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_PK))
#define BRASERO_PK_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_PK, BraseroPKClass))

typedef struct _BraseroPKClass BraseroPKClass;
typedef struct _BraseroPK BraseroPK;

struct _BraseroPKClass
{
	GObjectClass parent_class;
};

struct _BraseroPK
{
	GObject parent_instance;
};

GType brasero_pk_get_type (void) G_GNUC_CONST;

BraseroPK *brasero_pk_new (void);

gboolean
brasero_pk_install_gstreamer_plugin (BraseroPK *package,
                                     const gchar *element_name,
                                     int xid,
                                     GCancellable *cancel);
gboolean
brasero_pk_install_missing_app (BraseroPK *package,
                                const gchar *file_name,
                                int xid,
                                GCancellable *cancel);
gboolean
brasero_pk_install_missing_library (BraseroPK *package,
                                    const gchar *library_name,
                                    int xid,
                                    GCancellable *cancel);

G_END_DECLS

#endif /* _BRASERO_PK_H_ */
