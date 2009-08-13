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

#ifndef _BRASERO_TRACK_DATA_CFG_H_
#define _BRASERO_TRACK_DATA_CFG_H_

#include <glib-object.h>
#include <gtk/gtk.h>

#include <brasero-track-data.h>

G_BEGIN_DECLS

/**
 * GtkTreeModel Part
 */

/* This DND target when moving nodes inside ourselves */
#define BRASERO_DND_TARGET_DATA_TRACK_REFERENCE_LIST	"GTK_TREE_MODEL_ROW"

typedef enum {
	BRASERO_DATA_TREE_MODEL_NAME		= 0,
	BRASERO_DATA_TREE_MODEL_URI,
	BRASERO_DATA_TREE_MODEL_MIME_DESC,
	BRASERO_DATA_TREE_MODEL_MIME_ICON,
	BRASERO_DATA_TREE_MODEL_SIZE,
	BRASERO_DATA_TREE_MODEL_SHOW_PERCENT,
	BRASERO_DATA_TREE_MODEL_PERCENT,
	BRASERO_DATA_TREE_MODEL_STYLE,
	BRASERO_DATA_TREE_MODEL_COLOR,
	BRASERO_DATA_TREE_MODEL_EDITABLE,
	BRASERO_DATA_TREE_MODEL_IS_FILE,
	BRASERO_DATA_TREE_MODEL_IS_LOADING,
	BRASERO_DATA_TREE_MODEL_IS_IMPORTED,
	BRASERO_DATA_TREE_MODEL_COL_NUM
} BraseroTrackDataCfgColumn;


#define BRASERO_TYPE_TRACK_DATA_CFG             (brasero_track_data_cfg_get_type ())
#define BRASERO_TRACK_DATA_CFG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_TRACK_DATA_CFG, BraseroTrackDataCfg))
#define BRASERO_TRACK_DATA_CFG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_TRACK_DATA_CFG, BraseroTrackDataCfgClass))
#define BRASERO_IS_TRACK_DATA_CFG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_TRACK_DATA_CFG))
#define BRASERO_IS_TRACK_DATA_CFG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_TRACK_DATA_CFG))
#define BRASERO_TRACK_DATA_CFG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_TRACK_DATA_CFG, BraseroTrackDataCfgClass))

typedef struct _BraseroTrackDataCfgClass BraseroTrackDataCfgClass;
typedef struct _BraseroTrackDataCfg BraseroTrackDataCfg;

struct _BraseroTrackDataCfgClass
{
	BraseroTrackDataClass parent_class;
};

struct _BraseroTrackDataCfg
{
	BraseroTrackData parent_instance;
};

GType brasero_track_data_cfg_get_type (void) G_GNUC_CONST;

BraseroTrackDataCfg *
brasero_track_data_cfg_new (void);

gboolean
brasero_track_data_cfg_add (BraseroTrackDataCfg *track,
			    const gchar *uri,
			    GtkTreePath *parent);
GtkTreePath *
brasero_track_data_cfg_add_empty_directory (BraseroTrackDataCfg *track,
					    const gchar *name,
					    GtkTreePath *parent);

gboolean
brasero_track_data_cfg_remove (BraseroTrackDataCfg *track,
			       GtkTreePath *treepath);
gboolean
brasero_track_data_cfg_rename (BraseroTrackDataCfg *track,
			       const gchar *newname,
			       GtkTreePath *treepath);

gboolean
brasero_track_data_cfg_reset (BraseroTrackDataCfg *track);

gboolean
brasero_track_data_cfg_load_medium (BraseroTrackDataCfg *track,
				    BraseroMedium *medium,
				    GError **error);
void
brasero_track_data_cfg_unload_current_medium (BraseroTrackDataCfg *track);

BraseroMedium *
brasero_track_data_cfg_get_current_medium (BraseroTrackDataCfg *track);

GSList *
brasero_track_data_cfg_get_available_media (BraseroTrackDataCfg *track);

/**
 * For filtered URIs tree model
 */

void
brasero_track_data_cfg_dont_filter_uri (BraseroTrackDataCfg *track,
					const gchar *uri);

GSList *
brasero_track_data_cfg_get_restored_list (BraseroTrackDataCfg *track);

enum  {
	BRASERO_FILTERED_STOCK_ID_COL,
	BRASERO_FILTERED_URI_COL,
	BRASERO_FILTERED_STATUS_COL,
	BRASERO_FILTERED_FATAL_ERROR_COL,
	BRASERO_FILTERED_NB_COL,
};


void
brasero_track_data_cfg_restore (BraseroTrackDataCfg *track,
				GtkTreePath *treepath);

GtkTreeModel *
brasero_track_data_cfg_get_filtered_model (BraseroTrackDataCfg *track);


/**
 * Track Spanning
 */

BraseroBurnResult
brasero_track_data_cfg_span (BraseroTrackDataCfg *track,
			     goffset sectors,
			     BraseroTrackData *new_track);
BraseroBurnResult
brasero_track_data_cfg_span_again (BraseroTrackDataCfg *track);

BraseroBurnResult
brasero_track_data_cfg_span_possible (BraseroTrackDataCfg *track,
				      goffset sectors);

goffset
brasero_track_data_cfg_span_max_space (BraseroTrackDataCfg *track);

void
brasero_track_data_cfg_span_stop (BraseroTrackDataCfg *track);

/**
 * Icon
 */

GIcon *
brasero_track_data_cfg_get_icon (BraseroTrackDataCfg *track);

gchar *
brasero_track_data_cfg_get_icon_path (BraseroTrackDataCfg *track);

gboolean
brasero_track_data_cfg_set_icon (BraseroTrackDataCfg *track,
				 const gchar *icon_path,
				 GError **error);

G_END_DECLS

#endif /* _BRASERO_TRACK_DATA_CFG_H_ */
