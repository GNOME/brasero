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
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>

#include "brasero-track-data.h"
#include "burn-mkisofs-base.h"

typedef struct _BraseroTrackDataPrivate BraseroTrackDataPrivate;
struct _BraseroTrackDataPrivate
{
	BraseroImageFS fs_type;
	GSList *grafts;
	GSList *excluded;

	guint file_num;
	guint64 data_blocks;
};

#define BRASERO_TRACK_DATA_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_TRACK_DATA, BraseroTrackDataPrivate))

G_DEFINE_TYPE (BraseroTrackData, brasero_track_data, BRASERO_TYPE_TRACK);


void
brasero_graft_point_free (BraseroGraftPt *graft)
{
	if (graft->uri)
		g_free (graft->uri);

	g_free (graft->path);
	g_free (graft);
}

BraseroGraftPt *
brasero_graft_point_copy (BraseroGraftPt *graft)
{
	BraseroGraftPt *newgraft;

	g_return_val_if_fail (graft != NULL, NULL);

	newgraft = g_new0 (BraseroGraftPt, 1);
	newgraft->path = g_strdup (graft->path);
	if (graft->uri)
		newgraft->uri = g_strdup (graft->uri);

	return newgraft;
}

BraseroBurnResult
brasero_track_data_set_source (BraseroTrackData *track,
			       GSList *grafts,
			       GSList *unreadable)
{
	BraseroTrackDataPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK_DATA (track), BRASERO_BURN_NOT_SUPPORTED);

	priv = BRASERO_TRACK_DATA_PRIVATE (track);

	if (priv->grafts) {
		g_slist_foreach (priv->grafts, (GFunc) brasero_graft_point_free, NULL);
		g_slist_free (priv->grafts);
	}

	if (priv->excluded) {
		g_slist_foreach (priv->excluded, (GFunc) g_free, NULL);
		g_slist_free (priv->excluded);
	}

	priv->grafts = grafts;
	priv->excluded = unreadable;
	brasero_track_changed (BRASERO_TRACK (track));

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_track_data_add_fs (BraseroTrackData *track,
			   BraseroImageFS fstype)
{
	BraseroTrackDataPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK_DATA (track), BRASERO_BURN_NOT_SUPPORTED);

	priv = BRASERO_TRACK_DATA_PRIVATE (track);

	fstype |= priv->fs_type;
	if (fstype == priv->fs_type)
		return BRASERO_BURN_OK;

	priv->fs_type = fstype;
	brasero_track_changed (BRASERO_TRACK (track));

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_track_data_rm_fs (BraseroTrackData *track,
			  BraseroImageFS fstype)
{
	BraseroTrackDataPrivate *priv;
	BraseroImageFS new_fstype;

	g_return_val_if_fail (BRASERO_IS_TRACK_DATA (track), BRASERO_BURN_NOT_SUPPORTED);

	priv = BRASERO_TRACK_DATA_PRIVATE (track);

	new_fstype = priv->fs_type;
	new_fstype &= ~fstype;
	if (new_fstype == priv->fs_type)
		return BRASERO_BURN_OK;

	priv->fs_type = new_fstype;
	brasero_track_changed (BRASERO_TRACK (track));

	return BRASERO_BURN_OK;
}

BraseroImageFS
brasero_track_data_get_fs (BraseroTrackData *track)
{
	BraseroTrackDataPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK_DATA (track), BRASERO_IMAGE_FS_NONE);

	priv = BRASERO_TRACK_DATA_PRIVATE (track);
	return priv->fs_type;
}

BraseroBurnResult
brasero_track_data_set_data_blocks (BraseroTrackData *track,
				    guint64 blocks)
{
	BraseroTrackDataPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK_DATA (track), BRASERO_BURN_NOT_SUPPORTED);

	priv = BRASERO_TRACK_DATA_PRIVATE (track);
	priv->data_blocks = blocks;

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_track_data_set_file_num (BraseroTrackData *track,
				 guint64 number)
{
	BraseroTrackDataPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK_DATA (track), BRASERO_BURN_NOT_SUPPORTED);

	priv = BRASERO_TRACK_DATA_PRIVATE (track);

	priv->file_num = number;
	return BRASERO_BURN_OK;
}

GSList *
brasero_track_data_get_grafts (BraseroTrackData *track)
{
	BraseroTrackDataPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK_DATA (track), NULL);

	priv = BRASERO_TRACK_DATA_PRIVATE (track);

	return priv->grafts;
}

GSList *
brasero_track_data_get_excluded (BraseroTrackData *track,
				 gboolean copy)
{
	BraseroTrackDataPrivate *priv;
	GSList *retval = NULL;
	GSList *iter;

	g_return_val_if_fail (BRASERO_IS_TRACK_DATA (track), NULL);

	priv = BRASERO_TRACK_DATA_PRIVATE (track);

	if (!copy)
		return priv->excluded;

	for (iter = priv->excluded; iter; iter = iter->next) {
		gchar *uri;

		uri = iter->data;
		retval = g_slist_prepend (retval, g_strdup (uri));
	}

	return retval;
}

BraseroBurnResult
brasero_track_data_get_paths (BraseroTrackData *track,
			      gboolean use_joliet,
			      const gchar *grafts_path,
			      const gchar *excluded_path,
			      const gchar *emptydir,
			      const gchar *videodir,
			      GError **error)
{
	BraseroBurnResult result;
	BraseroTrackDataPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK_DATA (track), BRASERO_BURN_NOT_SUPPORTED);

	priv = BRASERO_TRACK_DATA_PRIVATE (track);
	result = brasero_mkisofs_base_write_to_files (priv->grafts,
						      priv->excluded,
						      use_joliet,
						      emptydir,
						      videodir,
						      grafts_path,
						      excluded_path,
						      error);
	return result;
}

BraseroBurnResult
brasero_track_data_get_file_num (BraseroTrackData *track,
				 guint64 *num_files)
{
	BraseroTrackDataPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK_DATA (track), BRASERO_BURN_NOT_SUPPORTED);

	priv = BRASERO_TRACK_DATA_PRIVATE (track);

	*num_files = priv->file_num;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_track_data_get_size (BraseroTrack *track,
			     guint64 *blocks,
			     guint *block_size)
{
	BraseroTrackDataPrivate *priv;

	priv = BRASERO_TRACK_DATA_PRIVATE (track);

	if (*block_size)
		*block_size = 2048;

	if (*blocks)
		*blocks = priv->data_blocks;

	return BRASERO_BURN_OK;
}

static BraseroTrackDataType
brasero_track_data_get_track_type (BraseroTrack *track,
				   BraseroTrackType *type)
{
	BraseroTrackDataPrivate *priv;

	priv = BRASERO_TRACK_DATA_PRIVATE (track);

	if (!type)
		return BRASERO_TRACK_TYPE_DATA;

	brasero_track_type_set_has_data (type);
	brasero_track_type_set_data_fs (type, priv->fs_type);

	return BRASERO_TRACK_TYPE_DATA;
}

static BraseroBurnResult
brasero_track_data_get_status (BraseroTrack *track,
			       BraseroStatus *status)
{
	BraseroTrackDataPrivate *priv;

	priv = BRASERO_TRACK_DATA_PRIVATE (track);

	if (!priv->grafts) {
		if (status)
			brasero_status_set_error (status,
						  g_error_new (BRASERO_BURN_ERROR,
							       BRASERO_BURN_ERROR_EMPTY,
							       _("The project is empty")));
		return BRASERO_BURN_ERR;
	}

	return BRASERO_BURN_OK;
}

static void
brasero_track_data_init (BraseroTrackData *object)
{ }

static void
brasero_track_data_finalize (GObject *object)
{
	G_OBJECT_CLASS (brasero_track_data_parent_class)->finalize (object);
}

static void
brasero_track_data_class_init (BraseroTrackDataClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	BraseroTrackClass* track_class = BRASERO_TRACK_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroTrackDataPrivate));

	object_class->finalize = brasero_track_data_finalize;

	track_class->get_type = brasero_track_data_get_track_type;
	track_class->get_status = brasero_track_data_get_status;
	track_class->get_size = brasero_track_data_get_size;
}

BraseroTrackData *
brasero_track_data_new (void)
{
	return g_object_new (BRASERO_TYPE_TRACK_DATA, NULL);
}
