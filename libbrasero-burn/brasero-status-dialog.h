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

#ifndef _BRASERO_STATUS_DIALOG_H_
#define _BRASERO_STATUS_DIALOG_H_

#include <glib-object.h>
#include <gtk/gtk.h>

#include "brasero-enums.h"
#include "brasero-session.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_STATUS_DIALOG             (brasero_status_dialog_get_type ())
#define BRASERO_STATUS_DIALOG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_STATUS_DIALOG, BraseroStatusDialog))
#define BRASERO_STATUS_DIALOG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_STATUS_DIALOG, BraseroStatusDialogClass))
#define BRASERO_IS_STATUS_DIALOG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_STATUS_DIALOG))
#define BRASERO_IS_STATUS_DIALOG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_STATUS_DIALOG))
#define BRASERO_STATUS_DIALOG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_STATUS_DIALOG, BraseroStatusDialogClass))

typedef struct _BraseroStatusDialogClass BraseroStatusDialogClass;
typedef struct _BraseroStatusDialog BraseroStatusDialog;

struct _BraseroStatusDialogClass
{
	GtkMessageDialogClass parent_class;
};

struct _BraseroStatusDialog
{
	GtkMessageDialog parent_instance;
};

GType brasero_status_dialog_get_type (void) G_GNUC_CONST;

GtkWidget *
brasero_status_dialog_new (BraseroBurnSession *session,
			   GtkWidget *parent);

G_END_DECLS

#endif /* _BRASERO_STATUS_DIALOG_H_ */
