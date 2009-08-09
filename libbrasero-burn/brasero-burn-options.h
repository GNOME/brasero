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

#ifndef _BRASERO_BURN_OPTIONS_H_
#define _BRASERO_BURN_OPTIONS_H_

#include <glib-object.h>

#include <gtk/gtk.h>

#include <brasero-medium-monitor.h>

#include <brasero-session-cfg.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_BURN_OPTIONS             (brasero_burn_options_get_type ())
#define BRASERO_BURN_OPTIONS(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_BURN_OPTIONS, BraseroBurnOptions))
#define BRASERO_BURN_OPTIONS_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_BURN_OPTIONS, BraseroBurnOptionsClass))
#define BRASERO_IS_BURN_OPTIONS(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_BURN_OPTIONS))
#define BRASERO_IS_BURN_OPTIONS_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_BURN_OPTIONS))
#define BRASERO_BURN_OPTIONS_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_BURN_OPTIONS, BraseroBurnOptionsClass))

typedef struct _BraseroBurnOptionsClass BraseroBurnOptionsClass;
typedef struct _BraseroBurnOptions BraseroBurnOptions;

struct _BraseroBurnOptionsClass
{
	GtkDialogClass parent_class;
};

struct _BraseroBurnOptions
{
	GtkDialog parent_instance;
};

GType brasero_burn_options_get_type (void) G_GNUC_CONST;

GtkWidget *
brasero_burn_options_new (BraseroSessionCfg *session);

void
brasero_burn_options_add_options (BraseroBurnOptions *dialog,
				  GtkWidget *options);

G_END_DECLS

#endif /* _BRASERO_BURN_OPTIONS_H_ */
