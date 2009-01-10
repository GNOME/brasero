/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2008 <bonfire-app@wanadoo.fr>
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

#ifndef _BRASERO_BURN_OPTIONS_H_
#define _BRASERO_BURN_OPTIONS_H_

#include <glib-object.h>

#include <gtk/gtk.h>

#include "brasero-medium-monitor.h"

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

BraseroBurnSession *
brasero_burn_options_get_session (BraseroBurnOptions *self);

void
brasero_burn_options_add_source (BraseroBurnOptions *self,
				 const gchar *title,
				 ...);

void
brasero_burn_options_add_options (BraseroBurnOptions *self,
				  GtkWidget *options);

GtkWidget *
brasero_burn_options_add_burn_button (BraseroBurnOptions *self,
				      const gchar *text,
				      const gchar *icon);
void
brasero_burn_options_lock_selection (BraseroBurnOptions *self);

void
brasero_burn_options_set_type_shown (BraseroBurnOptions *self,
				     BraseroMediaType type);

G_END_DECLS

#endif /* _BRASERO_BURN_OPTIONS_H_ */
