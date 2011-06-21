/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2007-2008 <bonfire-app@wanadoo.fr>
 * 
 *  Brasero is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 * brasero is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with brasero.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifndef _BRASERO_TIME_BUTTON_H_
#define _BRASERO_TIME_BUTTON_H_

#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_TIME_BUTTON             (brasero_time_button_get_type ())
#define BRASERO_TIME_BUTTON(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_TIME_BUTTON, BraseroTimeButton))
#define BRASERO_TIME_BUTTON_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_TIME_BUTTON, BraseroTimeButtonClass))
#define BRASERO_IS_TIME_BUTTON(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_TIME_BUTTON))
#define BRASERO_IS_TIME_BUTTON_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_TIME_BUTTON))
#define BRASERO_TIME_BUTTON_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_TIME_BUTTON, BraseroTimeButtonClass))

typedef struct _BraseroTimeButtonClass BraseroTimeButtonClass;
typedef struct _BraseroTimeButton BraseroTimeButton;

struct _BraseroTimeButtonClass
{
	GtkBoxClass parent_class;

	void		(*value_changed)	(BraseroTimeButton *self);
};

struct _BraseroTimeButton
{
	GtkBox parent_instance;
};

GType brasero_time_button_get_type (void) G_GNUC_CONST;

GtkWidget *
brasero_time_button_new (void);

gint64
brasero_time_button_get_value (BraseroTimeButton *time);

void
brasero_time_button_set_value (BraseroTimeButton *time,
			       gint64 value);
void
brasero_time_button_set_max (BraseroTimeButton *time,
			     gint64 max);

void
brasero_time_button_set_show_frames (BraseroTimeButton *time,
				     gboolean show);

G_END_DECLS

#endif /* _BRASERO_TIME_BUTTON_H_ */
