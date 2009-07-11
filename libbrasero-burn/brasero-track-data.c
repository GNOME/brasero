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
brasero_track_data_set_source_real (BraseroTrackData *track,
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
brasero_track_data_set_source (BraseroTrackData *track,
			       GSList *grafts,
			       GSList *unreadable)
{
	BraseroTrackDataClass *klass;

	g_return_val_if_fail (BRASERO_IS_TRACK_DATA (track), BRASERO_BURN_ERR);

	klass = BRASERO_TRACK_DATA_GET_CLASS (track);
	return klass->set_source (track, grafts, unreadable);
}

static BraseroBurnResult
brasero_track_data_add_fs_real (BraseroTrackData *track,
				BraseroImageFS fstype)
{
	BraseroTrackDataPrivate *priv;

	priv = BRASERO_TRACK_DATA_PRIVATE (track);
	priv->fs_type |= fstype;
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_track_data_add_fs (BraseroTrackData *track,
			   BraseroImageFS fstype)
{
	BraseroTrackDataClass *klass;
	BraseroImageFS fs_before;
	BraseroBurnResult result;

	g_return_val_if_fail (BRASERO_IS_TRACK_DATA (track), BRASERO_BURN_NOT_SUPPORTED);

	fs_before = brasero_track_data_get_fs (track);
	klass = BRASERO_TRACK_DATA_GET_CLASS (track);
	if (!klass->add_fs)
		return BRASERO_BURN_NOT_SUPPORTED;

	result = klass->add_fs (track, fstype);
	if (result != BRASERO_BURN_OK)
		return result;

	if (fs_before != brasero_track_data_get_fs (track))
		brasero_track_changed (BRASERO_TRACK (track));

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_track_data_rm_fs_real (BraseroTrackData *track,
			       BraseroImageFS fstype)
{
	BraseroTrackDataPrivate *priv;

	priv = BRASERO_TRACK_DATA_PRIVATE (track);
	priv->fs_type &= ~(fstype);
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_track_data_rm_fs (BraseroTrackData *track,
			  BraseroImageFS fstype)
{
	BraseroTrackDataClass *klass;
	BraseroImageFS fs_before;
	BraseroBurnResult result;

	g_return_val_if_fail (BRASERO_IS_TRACK_DATA (track), BRASERO_BURN_NOT_SUPPORTED);

	fs_before = brasero_track_data_get_fs (track);
	klass = BRASERO_TRACK_DATA_GET_CLASS (track);
	if (!klass->rm_fs);
		return BRASERO_BURN_NOT_SUPPORTED;

	result = klass->rm_fs (track, fstype);
	if (result != BRASERO_BURN_OK)
		return result;

	if (fs_before != brasero_track_data_get_fs (track))
		brasero_track_changed (BRASERO_TRACK (track));

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_track_data_set_data_blocks (BraseroTrackData *track,
				    goffset blocks)
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

BraseroImageFS
brasero_track_data_get_fs (BraseroTrackData *track)
{
	BraseroTrackDataClass *klass;

	g_return_val_if_fail (BRASERO_IS_TRACK_DATA (track), BRASERO_IMAGE_FS_NONE);

	klass = BRASERO_TRACK_DATA_GET_CLASS (track);
	return klass->get_fs (track);
}

BraseroImageFS
brasero_track_data_get_fs_real (BraseroTrackData *track)
{
	BraseroTrackDataPrivate *priv;

	priv = BRASERO_TRACK_DATA_PRIVATE (track);
	return priv->fs_type;
}

GSList *
brasero_track_data_get_grafts (BraseroTrackData *track)
{
	BraseroTrackDataClass *klass;

	g_return_val_if_fail (BRASERO_IS_TRACK_DATA (track), NULL);

	klass = BRASERO_TRACK_DATA_GET_CLASS (track);
	return klass->get_grafts (track);
}

static GSList *
brasero_track_data_get_grafts_real (BraseroTrackData *track)
{
	BraseroTrackDataPrivate *priv;

	priv = BRASERO_TRACK_DATA_PRIVATE (track);
	return priv->grafts;
}

GSList *
brasero_track_data_get_excluded (BraseroTrackData *track,
				 gboolean copy)
{
	BraseroTrackDataClass *klass;
	GSList *retval = NULL;
	GSList *excluded;
	GSList *iter;

	g_return_val_if_fail (BRASERO_IS_TRACK_DATA (track), NULL);

	klass = BRASERO_TRACK_DATA_GET_CLASS (track);
	excluded = klass->get_excluded (track);
	if (!copy)
		return excluded;

	for (iter = excluded; iter; iter = iter->next) {
		gchar *uri;

		uri = iter->data;
		retval = g_slist_prepend (retval, g_strdup (uri));
	}

	return retval;
}

static GSList *
brasero_track_data_get_excluded_real (BraseroTrackData *track)
{
	BraseroTrackDataPrivate *priv;

	priv = BRASERO_TRACK_DATA_PRIVATE (track);
	return priv->excluded;
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
	GSList *grafts;
	GSList *excluded;
	BraseroBurnResult result;
	BraseroTrackDataClass *klass;

	g_return_val_if_fail (BRASERO_IS_TRACK_DATA (track), BRASERO_BURN_NOT_SUPPORTED);

	klass = BRASERO_TRACK_DATA_GET_CLASS (track);
	grafts = klass->get_grafts (track);
	excluded = klass->get_excluded (track);

	result = brasero_mkisofs_base_write_to_files (grafts,
						      excluded,
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
				 guint64 *file_num)
{
	BraseroTrackDataClass *klass;

	g_return_val_if_fail (BRASERO_IS_TRACK_DATA (track), 0);

	klass = BRASERO_TRACK_DATA_GET_CLASS (track);
	if (file_num)
		*file_num = klass->get_file_num (track);

	return BRASERO_BURN_OK;
}

static guint64
brasero_track_data_get_file_num_real (BraseroTrackData *track)
{
	BraseroTrackDataPrivate *priv;

	priv = BRASERO_TRACK_DATA_PRIVATE (track);
	return priv->file_num;
}

static BraseroBurnResult
brasero_track_data_get_size (BraseroTrack *track,
			     goffset *blocks,
			     goffset *block_size)
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
							       _("There are no files to write to disc")));
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
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroTrackClass *track_class = BRASERO_TRACK_CLASS (klass);
	BraseroTrackDataClass *track_data_class = BRASERO_TRACK_DATA_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroTrackDataPrivate));

	object_class->finalize = brasero_track_data_finalize;

	track_class->get_type = brasero_track_data_get_track_type;
	track_class->get_status = brasero_track_data_get_status;
	track_class->get_size = brasero_track_data_get_size;

	track_data_class->set_source = brasero_track_data_set_source_real;
	track_data_class->add_fs = brasero_track_data_add_fs_real;
	track_data_class->rm_fs = brasero_track_data_rm_fs_real;

	track_data_class->get_fs = brasero_track_data_get_fs_real;
	track_data_class->get_grafts = brasero_track_data_get_grafts_real;
	track_data_class->get_excluded = brasero_track_data_get_excluded_real;
	track_data_class->get_file_num = brasero_track_data_get_file_num_real;
}

BraseroTrackData *
brasero_track_data_new (void)
{
	return g_object_new (BRASERO_TYPE_TRACK_DATA, NULL);
}
