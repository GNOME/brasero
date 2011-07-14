/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2005-2008 <bonfire-app@wanadoo.fr>
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

#ifndef _BRASERO_FILTER_OPTION_H_
#define _BRASERO_FILTER_OPTION_H_

#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_FILTER_OPTION             (brasero_filter_option_get_type ())
#define BRASERO_FILTER_OPTION(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_FILTER_OPTION, BraseroFilterOption))
#define BRASERO_FILTER_OPTION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_FILTER_OPTION, BraseroFilterOptionClass))
#define BRASERO_IS_FILTER_OPTION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_FILTER_OPTION))
#define BRASERO_IS_FILTER_OPTION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_FILTER_OPTION))
#define BRASERO_FILTER_OPTION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_FILTER_OPTION, BraseroFilterOptionClass))

typedef struct _BraseroFilterOptionClass BraseroFilterOptionClass;
typedef struct _BraseroFilterOption BraseroFilterOption;

struct _BraseroFilterOptionClass
{
	GtkBoxClass parent_class;
};

struct _BraseroFilterOption
{
	GtkBox parent_instance;
};

GType brasero_filter_option_get_type (void) G_GNUC_CONST;

GtkWidget *
brasero_filter_option_new (void);

G_END_DECLS

#endif /* _BRASERO_FILTER_OPTION_H_ */
