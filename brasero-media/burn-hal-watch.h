/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Rouquier Philippe 2008 <bonfire-app@wanadoo.fr>
 * 
 * brasero is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * brasero is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _BRASERO_HAL_WATCH_H_
#define _BRASERO_HAL_WATCH_H_

#include <glib-object.h>

#include <libhal.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_HAL_WATCH             (brasero_hal_watch_get_type ())
#define BRASERO_HAL_WATCH(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_HAL_WATCH, BraseroHALWatch))
#define BRASERO_HAL_WATCH_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_HAL_WATCH, BraseroHALWatchClass))
#define BRASERO_IS_HAL_WATCH(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_HAL_WATCH))
#define BRASERO_IS_HAL_WATCH_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_HAL_WATCH))
#define BRASERO_HAL_WATCH_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_HAL_WATCH, BraseroHALWatchClass))

typedef struct _BraseroHALWatchClass BraseroHALWatchClass;
typedef struct _BraseroHALWatch BraseroHALWatch;

struct _BraseroHALWatchClass
{
	GObjectClass parent_class;
};

struct _BraseroHALWatch
{
	GObject parent_instance;
};

GType brasero_hal_watch_get_type (void) G_GNUC_CONST;

BraseroHALWatch *
brasero_hal_watch_get_default (void);

void
brasero_hal_watch_destroy (void);

LibHalContext *
brasero_hal_watch_get_ctx (BraseroHALWatch *watch);

G_END_DECLS

#endif /* _BRASERO_HAL_WATCH_H_ */
