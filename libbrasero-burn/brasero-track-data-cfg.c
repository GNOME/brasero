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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtk.h>

#include "brasero-units.h"

#include "brasero-track-data-cfg.h"

#include "brasero-data-project.h"
#include "brasero-data-tree-model.h"

typedef struct _BraseroTrackDataCfgPrivate BraseroTrackDataCfgPrivate;
struct _BraseroTrackDataCfgPrivate
{
	BraseroDataTreeModel *tree;
};

#define BRASERO_TRACK_DATA_CFG_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_TRACK_DATA_CFG, BraseroTrackDataCfgPrivate))


G_DEFINE_TYPE (BraseroTrackDataCfg, brasero_track_data_cfg, BRASERO_TYPE_TRACK_DATA);

gboolean
brasero_track_data_cfg_add (BraseroTrackDataCfg *track,
			    const gchar *uri,
			    GtkTreePath *parent)
{
	BraseroTrackDataCfgPrivate *priv;
	BraseroFileNode *parent_node;

	g_return_val_if_fail (BRASERO_TRACK_DATA_CFG (track), FALSE);
	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);
	parent_node = brasero_data_tree_model_path_to_node (BRASERO_DATA_TREE_MODEL (priv->tree), parent);
	return (brasero_data_project_add_loading_node (BRASERO_DATA_PROJECT (priv->tree), uri, parent_node) != NULL);
}

gboolean
brasero_track_data_cfg_add_empty_directory (BraseroTrackDataCfg *track,
					    const gchar *name,
					    GtkTreePath *parent)
{
	BraseroTrackDataCfgPrivate *priv;
	BraseroFileNode *parent_node;

	g_return_val_if_fail (BRASERO_TRACK_DATA_CFG (track), FALSE);
	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);
	parent_node = brasero_data_tree_model_path_to_node (BRASERO_DATA_TREE_MODEL (priv->tree), parent);
	return (brasero_data_project_add_empty_directory (BRASERO_DATA_PROJECT (priv->tree), name, parent_node) != NULL);
}

void
brasero_track_data_cfg_remove (BraseroTrackDataCfg *track,
			       GtkTreePath *treepath)
{
	BraseroTrackDataCfgPrivate *priv;
	BraseroFileNode *node;

	g_return_if_fail (BRASERO_TRACK_DATA_CFG (track));
	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);
	node = brasero_data_tree_model_path_to_node (BRASERO_DATA_TREE_MODEL (priv->tree), treepath);
	return brasero_data_project_remove_node (BRASERO_DATA_PROJECT (priv->tree), node);
}

gboolean
brasero_track_data_cfg_rename (BraseroTrackDataCfg *track,
			       const gchar *newname,
			       GtkTreePath *treepath)
{
	BraseroTrackDataCfgPrivate *priv;
	BraseroFileNode *node;

	g_return_val_if_fail (BRASERO_TRACK_DATA_CFG (track), FALSE);
	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);
	node = brasero_data_tree_model_path_to_node (BRASERO_DATA_TREE_MODEL (priv->tree), treepath);
	return brasero_data_project_rename_node (BRASERO_DATA_PROJECT (priv->tree),
						 node,
						 newname);
}

void
brasero_track_data_cfg_reset (BraseroTrackDataCfg *track)
{
	BraseroTrackDataCfgPrivate *priv;

	g_return_if_fail (BRASERO_TRACK_DATA_CFG (track));
	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);
	brasero_data_project_reset (BRASERO_DATA_PROJECT (priv->tree));
}

GSList *
brasero_track_data_cfg_get_restored_uri (BraseroTrackDataCfg *track)
{
	BraseroTrackDataCfgPrivate *priv;
	GSList *list = NULL;

	g_return_val_if_fail (BRASERO_TRACK_DATA_CFG (track), NULL);
	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);
	brasero_data_vfs_get_restored (BRASERO_DATA_VFS (priv->tree), &list);
	return list;
}

void
brasero_track_data_cfg_exclude_uri (BraseroTrackDataCfg *track,
				    const gchar *uri)
{
	BraseroTrackDataCfgPrivate *priv;

	g_return_if_fail (BRASERO_TRACK_DATA_CFG (track));
	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);
	brasero_data_vfs_remove_restored (BRASERO_DATA_VFS (priv->tree), uri);
	brasero_data_project_exclude_uri (BRASERO_DATA_PROJECT (priv->tree), uri);
}

void
brasero_track_data_cfg_restore_uri (BraseroTrackDataCfg *track,
				    const gchar *uri)
{
	BraseroTrackDataCfgPrivate *priv;

	g_return_if_fail (BRASERO_TRACK_DATA_CFG (track));
	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);
	brasero_data_vfs_add_restored (BRASERO_DATA_VFS (priv->tree), uri);
	brasero_data_project_restore_uri (BRASERO_DATA_PROJECT (priv->tree), uri);
}

gboolean
brasero_track_data_cfg_load_medium (BraseroTrackDataCfg *track,
				    BraseroMedium *medium,
				    GError **error)
{
	BraseroTrackDataCfgPrivate *priv;

	g_return_val_if_fail (BRASERO_TRACK_DATA_CFG (track), FALSE);
	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);
	return brasero_data_session_add_last (BRASERO_DATA_SESSION (track),
					      medium,
					      error);
}

void
brasero_track_data_cfg_unload_current_medium (BraseroTrackDataCfg *track)
{
	BraseroTrackDataCfgPrivate *priv;

	g_return_if_fail (BRASERO_TRACK_DATA_CFG (track));
	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);
	brasero_data_session_remove_last (BRASERO_DATA_SESSION (track));
}

BraseroMedium *
brasero_track_data_cfg_get_current_medium (BraseroTrackDataCfg *track)
{
	BraseroTrackDataCfgPrivate *priv;

	g_return_val_if_fail (BRASERO_TRACK_DATA_CFG (track), NULL);
	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);
	return brasero_data_session_get_loaded_medium (BRASERO_DATA_SESSION (track));
}

GSList *
brasero_track_data_cfg_get_available_media (BraseroTrackDataCfg *track)
{
	BraseroTrackDataCfgPrivate *priv;

	g_return_val_if_fail (BRASERO_TRACK_DATA_CFG (track), NULL);
	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);
	return brasero_data_session_get_available_media (BRASERO_DATA_SESSION (track));
}

GtkTreeModel *
brasero_track_data_cfg_get_tree_model (BraseroTrackDataCfg *track)
{
	BraseroTrackDataCfgPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK_DATA_CFG (track), NULL);

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);
	return GTK_TREE_MODEL (priv->tree);
}

static BraseroImageFS
brasero_track_data_cfg_get_fs (BraseroTrackData *track)
{
	BraseroTrackDataCfgPrivate *priv;
	BraseroFileTreeStats *stats;
	BraseroImageFS fs_type;
	BraseroFileNode *root;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);

	root = brasero_data_project_get_root (BRASERO_DATA_PROJECT (priv->tree));
	stats = BRASERO_FILE_NODE_STATS (root);

	fs_type = BRASERO_IMAGE_FS_ISO;
	if (brasero_data_project_has_symlinks (BRASERO_DATA_PROJECT (priv->tree)))
		fs_type |= BRASERO_IMAGE_FS_SYMLINK;
	else {
		/* These two are incompatible with symlinks */
		if (brasero_data_project_is_joliet_compliant (BRASERO_DATA_PROJECT (priv->tree)))
			fs_type |= BRASERO_IMAGE_FS_JOLIET;

		if (brasero_data_project_is_video_project (BRASERO_DATA_PROJECT (priv->tree)))
			fs_type |= BRASERO_IMAGE_FS_VIDEO;
	}

	if (stats->num_2GiB != 0) {
		fs_type |= BRASERO_IMAGE_ISO_FS_LEVEL_3;
		if (!(fs_type & BRASERO_IMAGE_FS_SYMLINK))
			fs_type |= BRASERO_IMAGE_FS_UDF;
	}

	if (stats->num_deep != 0)
		fs_type |= BRASERO_IMAGE_ISO_FS_DEEP_DIRECTORY;

	return fs_type;
}

static GSList *
brasero_track_data_cfg_get_grafts (BraseroTrackData *track)
{
	BraseroTrackDataCfgPrivate *priv;
	BraseroImageFS fs_type;
	GSList *grafts = NULL;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);

	/* append a slash for mkisofs */
	brasero_data_project_get_contents (BRASERO_DATA_PROJECT (priv->tree),
					   &grafts,
					   NULL,
					   (fs_type & BRASERO_IMAGE_FS_JOLIET) != 0,
					   TRUE);
	return grafts;
}

static GSList *
brasero_track_data_cfg_get_excluded (BraseroTrackData *track)
{
	BraseroTrackDataCfgPrivate *priv;
	GSList *unreadable = NULL;
	BraseroImageFS fs_type;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);

	/* append a slash for mkisofs */
	brasero_data_project_get_contents (BRASERO_DATA_PROJECT (priv->tree),
					   NULL,
					   &unreadable,
					   (fs_type & BRASERO_IMAGE_FS_JOLIET) != 0,
					   TRUE);
	return unreadable;
}

static guint64
brasero_track_data_cfg_get_file_num (BraseroTrackData *track)
{
	BraseroTrackDataCfgPrivate *priv;
	BraseroFileTreeStats *stats;
	BraseroFileNode *root;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);

	root = brasero_data_project_get_root (BRASERO_DATA_PROJECT (priv->tree));
	stats = BRASERO_FILE_NODE_STATS (root);

	return stats->children;
}

static BraseroTrackDataType
brasero_track_data_cfg_get_track_type (BraseroTrack *track,
				       BraseroTrackType *type)
{
	BraseroTrackDataCfgPrivate *priv;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);

	if (!type)
		return BRASERO_TRACK_TYPE_DATA;

	brasero_track_type_set_has_data (type);
	brasero_track_type_set_data_fs (type, brasero_track_data_cfg_get_fs (BRASERO_TRACK_DATA (track)));

	return BRASERO_TRACK_TYPE_DATA;
}

static BraseroBurnResult
brasero_track_data_cfg_get_status (BraseroTrack *track,
				   BraseroStatus *status)
{
	BraseroTrackDataCfgPrivate *priv;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);

	/* This one goes before the next since a node may be loading but not
	 * yet in the project and therefore project will look empty */
	if (brasero_data_vfs_is_active (BRASERO_DATA_VFS (priv->tree))) {
		if (status)
			brasero_status_set_not_ready (status,
						      -1,
						      g_strdup (_("Analysing files")));

		return BRASERO_BURN_NOT_READY;
	}

	if (brasero_data_project_is_empty (BRASERO_DATA_PROJECT (priv->tree)))
		return BRASERO_BURN_ERR;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_track_data_cfg_get_size (BraseroTrack *track,
				 goffset *blocks,
				 goffset *block_size)
{
	BraseroTrackDataCfgPrivate *priv;
	goffset bytes = 0;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (track);

	bytes = brasero_data_project_get_size (BRASERO_DATA_PROJECT (priv->tree));
	if (blocks)
		*blocks = BRASERO_BYTES_TO_SECTORS (bytes, 2048);

	if (block_size)
		*block_size = 2048;

	return BRASERO_BURN_OK;
}

static void
brasero_track_data_cfg_init (BraseroTrackDataCfg *object)
{
	BraseroTrackDataCfgPrivate *priv;

	priv = BRASERO_TRACK_DATA_CFG_PRIVATE (object);
	priv->tree = brasero_data_tree_model_new ();
}

static void
brasero_track_data_cfg_finalize (GObject *object)
{
	G_OBJECT_CLASS (brasero_track_data_cfg_parent_class)->finalize (object);
}

static void
brasero_track_data_cfg_class_init (BraseroTrackDataCfgClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroTrackClass *track_class = BRASERO_TRACK_CLASS (klass);
	BraseroTrackDataClass *parent_class = BRASERO_TRACK_DATA_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroTrackDataCfgPrivate));

	object_class->finalize = brasero_track_data_cfg_finalize;

	track_class->get_size = brasero_track_data_cfg_get_size;
	track_class->get_type = brasero_track_data_cfg_get_track_type;
	track_class->get_status = brasero_track_data_cfg_get_status;

	parent_class->get_fs = brasero_track_data_cfg_get_fs;
	parent_class->get_grafts = brasero_track_data_cfg_get_grafts;
	parent_class->get_excluded = brasero_track_data_cfg_get_excluded;
	parent_class->get_file_num = brasero_track_data_cfg_get_file_num;
}

