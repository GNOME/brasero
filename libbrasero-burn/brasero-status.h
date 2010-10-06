/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-burn
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
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

#ifndef _BRASERO_STATUS_H_
#define _BRASERO_STATUS_H_

#include <glib.h>
#include <glib-object.h>

#include <brasero-enums.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_STATUS             (brasero_status_get_type ())
#define BRASERO_STATUS(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_STATUS, BraseroStatus))
#define BRASERO_STATUS_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_STATUS, BraseroStatusClass))
#define BRASERO_IS_STATUS(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_STATUS))
#define BRASERO_IS_STATUS_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_STATUS))
#define BRASERO_STATUS_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_STATUS, BraseroStatusClass))

typedef struct _BraseroStatusClass BraseroStatusClass;
typedef struct _BraseroStatus BraseroStatus;

struct _BraseroStatusClass
{
	GObjectClass parent_class;
};

struct _BraseroStatus
{
	GObject parent_instance;
};

GType brasero_status_get_type (void) G_GNUC_CONST;

BraseroStatus *
brasero_status_new (void);

typedef enum {
	BRASERO_STATUS_OK			= 0,
	BRASERO_STATUS_ERROR,
	BRASERO_STATUS_QUESTION,
	BRASERO_STATUS_INFORMATION
} BraseroStatusType;

BraseroBurnResult
brasero_status_get_result (BraseroStatus *status);

gdouble
brasero_status_get_progress (BraseroStatus *status);

GError *
brasero_status_get_error (BraseroStatus *status);

gchar *
brasero_status_get_current_action (BraseroStatus *status);

void
brasero_status_set_completed (BraseroStatus *status);

void
brasero_status_set_not_ready (BraseroStatus *status,
			      gdouble progress,
			      const gchar *current_action);

void
brasero_status_set_running (BraseroStatus *status,
			    gdouble progress,
			    const gchar *current_action);

void
brasero_status_set_error (BraseroStatus *status,
			  GError *error);

G_END_DECLS

#endif
