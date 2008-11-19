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

#ifndef _BRASERO_TOOL_COLOR_PICKER_H_
#define _BRASERO_TOOL_COLOR_PICKER_H_

#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_TOOL_COLOR_PICKER             (brasero_tool_color_picker_get_type ())
#define BRASERO_TOOL_COLOR_PICKER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_TOOL_COLOR_PICKER, BraseroToolColorPicker))
#define BRASERO_TOOL_COLOR_PICKER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_TOOL_COLOR_PICKER, BraseroToolColorPickerClass))
#define BRASERO_IS_TOOL_COLOR_PICKER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_TOOL_COLOR_PICKER))
#define BRASERO_IS_TOOL_COLOR_PICKER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_TOOL_COLOR_PICKER))
#define BRASERO_TOOL_COLOR_PICKER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_TOOL_COLOR_PICKER, BraseroToolColorPickerClass))

typedef struct _BraseroToolColorPickerClass BraseroToolColorPickerClass;
typedef struct _BraseroToolColorPicker BraseroToolColorPicker;

struct _BraseroToolColorPickerClass
{
	GtkToolButtonClass parent_class;
};

struct _BraseroToolColorPicker
{
	GtkToolButton parent_instance;
};

GType brasero_tool_color_picker_get_type (void) G_GNUC_CONST;

GtkWidget *
brasero_tool_color_picker_new (void);

void
brasero_tool_color_picker_set_text (BraseroToolColorPicker *picker,
				    const gchar *text);
void
brasero_tool_color_picker_set_color (BraseroToolColorPicker *picker,
				     GdkColor *color);
void
brasero_tool_color_picker_get_color (BraseroToolColorPicker *picker,
				     GdkColor *color);

G_END_DECLS

#endif /* _BRASERO_TOOL_COLOR_PICKER_H_ */
