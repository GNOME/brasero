/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2009 <bonfire-app@wanadoo.fr>
 * 
 * brasero is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * brasero is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _BRASERO_SETTING_H_
#define _BRASERO_SETTING_H_

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
	BRASERO_SETTING_VALUE_NONE,

	/** gint value **/
	BRASERO_SETTING_WIN_WIDTH,
	BRASERO_SETTING_WIN_HEIGHT,
	BRASERO_SETTING_STOCK_FILE_CHOOSER_PERCENT,
	BRASERO_SETTING_BRASERO_FILE_CHOOSER_PERCENT,
	BRASERO_SETTING_PLAYER_VOLUME,
	BRASERO_SETTING_DISPLAY_PROPORTION,
	BRASERO_SETTING_DISPLAY_LAYOUT,
	BRASERO_SETTING_DATA_DISC_COLUMN,
	BRASERO_SETTING_DATA_DISC_COLUMN_ORDER,
	BRASERO_SETTING_IMAGE_SIZE_WIDTH,
	BRASERO_SETTING_IMAGE_SIZE_HEIGHT,
	BRASERO_SETTING_VIDEO_SIZE_HEIGHT,
	BRASERO_SETTING_VIDEO_SIZE_WIDTH,

	/** gboolean **/
	BRASERO_SETTING_WIN_MAXIMIZED,
	BRASERO_SETTING_SHOW_SIDEPANE,
	BRASERO_SETTING_SHOW_PREVIEW,

	/** gchar * **/
	BRASERO_SETTING_DISPLAY_LAYOUT_AUDIO,
	BRASERO_SETTING_DISPLAY_LAYOUT_DATA,
	BRASERO_SETTING_DISPLAY_LAYOUT_VIDEO,

	/** gchar ** **/
	BRASERO_SETTING_SEARCH_ENTRY_HISTORY,

} BraseroSettingValue;

#define BRASERO_TYPE_SETTING             (brasero_setting_get_type ())
#define BRASERO_SETTING(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_SETTING, BraseroSetting))
#define BRASERO_SETTING_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_SETTING, BraseroSettingClass))
#define BRASERO_IS_SETTING(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_SETTING))
#define BRASERO_IS_SETTING_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_SETTING))
#define BRASERO_SETTING_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_SETTING, BraseroSettingClass))

typedef struct _BraseroSettingClass BraseroSettingClass;
typedef struct _BraseroSetting BraseroSetting;

struct _BraseroSettingClass
{
	GObjectClass parent_class;

	/* Signals */
	void(* value_changed) (BraseroSetting *self, gint value);
};

struct _BraseroSetting
{
	GObject parent_instance;
};

GType brasero_setting_get_type (void) G_GNUC_CONST;

BraseroSetting *
brasero_setting_get_default (void);

gboolean
brasero_setting_get_value (BraseroSetting *setting,
                           BraseroSettingValue setting_value,
                           gpointer *value);

gboolean
brasero_setting_set_value (BraseroSetting *setting,
                           BraseroSettingValue setting_value,
                           gconstpointer value);

gboolean
brasero_setting_load (BraseroSetting *setting);

gboolean
brasero_setting_save (BraseroSetting *setting);

G_END_DECLS

#endif /* _BRASERO_SETTING_H_ */
