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

#include <sys/param.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>

#include "brasero-track-data.h"
#include "burn-mkisofs-base.h"
#include "burn-debug.h"

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

/**
 * brasero_graft_point_free:
 * @graft: a #BraseroGraftPt
 *
 * Frees @graft. Do not use @grafts afterwards.
 *
 **/

void
brasero_graft_point_free (BraseroGraftPt *graft)
{
	if (graft->uri)
		g_free (graft->uri);

	g_free (graft->path);
	g_free (graft);
}

/**
 * brasero_graft_point_copy:
 * @graft: a #BraseroGraftPt
 *
 * Copies @graft.
 *
 * Return value: a #BraseroGraftPt.
 **/

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

static BraseroBurnResult
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

/**
 * brasero_track_data_set_source:
 * @track: a #BraseroTrackData.
 * @grafts: (element-type BraseroBurn.GraftPt) (in) (transfer full): a #GSList of #BraseroGraftPt.
 * @unreadable: (element-type utf8) (allow-none) (in) (transfer full): a #GSList of URIS as strings or %NULL.
 *
 * Sets the lists of grafts points (@grafts) and excluded
 * URIs (@unreadable) to be used to create an image.
 *
 * Be careful @track takes ownership of @grafts and
 * @unreadable which must not be freed afterwards.
 *
 * Return value: a #BraseroBurnResult.
 * BRASERO_BURN_OK if it was successful,
 * BRASERO_BURN_ERR otherwise.
 **/

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

/**
 * brasero_track_data_add_fs:
 * @track: a #BraseroTrackData
 * @fstype: a #BraseroImageFS
 *
 * Adds one or more parameters determining the file system type
 * and various other options to create an image.
 *
 * Return value: a #BraseroBurnResult.
 * BRASERO_BURN_OK if it was successful,
 * BRASERO_BURN_ERR otherwise.
 **/

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

/**
 * brasero_track_data_rm_fs:
 * @track: a #BraseroTrackData
 * @fstype: a #BraseroImageFS
 *
 * Removes one or more parameters determining the file system type
 * and various other options to create an image.
 *
 * Return value: a #BraseroBurnResult.
 * BRASERO_BURN_OK if it was successful,
 * BRASERO_BURN_ERR otherwise.
 **/

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

/**
 * brasero_track_data_set_data_blocks:
 * @track: a #BraseroTrackData
 * @blocks: a #goffset
 *
 * Sets the size of the image to be created (in sectors of 2048 bytes).
 *
 * Return value: a #BraseroBurnResult.
 * BRASERO_BURN_OK if it was successful,
 * BRASERO_BURN_ERR otherwise.
 **/

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

/**
 * brasero_track_data_set_file_num:
 * @track: a #BraseroTrackData
 * @number: a #guint64
 *
 * Sets the number of files (not directories) in @track.
 *
 * Return value: a #BraseroBurnResult.
 * BRASERO_BURN_OK if it was successful,
 * BRASERO_BURN_ERR otherwise.
 **/

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

/**
 * brasero_track_data_get_fs:
 * @track: a #BraseroTrackData
 *
 * Returns the parameters determining the file system type
 * and various other options to create an image.
 *
 * Return value: a #BraseroImageFS.
 **/

BraseroImageFS
brasero_track_data_get_fs (BraseroTrackData *track)
{
	BraseroTrackDataClass *klass;

	g_return_val_if_fail (BRASERO_IS_TRACK_DATA (track), BRASERO_IMAGE_FS_NONE);

	klass = BRASERO_TRACK_DATA_GET_CLASS (track);
	return klass->get_fs (track);
}

static BraseroImageFS
brasero_track_data_get_fs_real (BraseroTrackData *track)
{
	BraseroTrackDataPrivate *priv;

	priv = BRASERO_TRACK_DATA_PRIVATE (track);
	return priv->fs_type;
}

static GHashTable *
brasero_track_data_mangle_joliet_name (GHashTable *joliet,
				       const gchar *path,
				       gchar *buffer)
{
	gboolean has_slash = FALSE;
	gint dot_pos = -1;
	gint dot_len = -1;
	gchar *name;
	gint width;
	gint start;
	gint num;
	gint end;

	/* NOTE: this wouldn't work on windows (not a big deal) */
	end = strlen (path);
	if (!end) {
		buffer [0] = '\0';
		return joliet;
	}

	memcpy (buffer, path, MIN (end, MAXPATHLEN));
	buffer [MIN (end, MAXPATHLEN)] = '\0';

	/* move back until we find a character different from G_DIR_SEPARATOR */
	end --;
	while (end >= 0 && G_IS_DIR_SEPARATOR (path [end])) {
		end --;
		has_slash = TRUE;
	}

	/* There are only slashes */
	if (end == -1)
		return joliet;

	start = end - 1;
	while (start >= 0 && !G_IS_DIR_SEPARATOR (path [start])) {
		/* Find the extension while at it */
		if (dot_pos <= 0 && path [start] == '.')
			dot_pos = start;

		start --;
	}

	if (end - start <= 64)
		return joliet;

	name = buffer + start + 1;
	if (dot_pos > 0)
		dot_len = end - dot_pos + 1;

	if (dot_len > 1 && dot_len < 5)
		memcpy (name + 64 - dot_len,
			path + dot_pos,
			dot_len);

	name [64] = '\0';

	if (!joliet) {
		joliet = g_hash_table_new_full (g_str_hash,
						g_str_equal,
						g_free,
						NULL);

		g_hash_table_insert (joliet, g_strdup (buffer), GINT_TO_POINTER (1));
		if (has_slash)
			strcat (buffer, G_DIR_SEPARATOR_S);

		BRASERO_BURN_LOG ("Mangled name to %s (truncated)", buffer);
		return joliet;
	}

	/* see if this path was already used */
	num = GPOINTER_TO_INT (g_hash_table_lookup (joliet, buffer));
	if (!num) {
		g_hash_table_insert (joliet, g_strdup (buffer), GINT_TO_POINTER (1));

		if (has_slash)
			strcat (buffer, G_DIR_SEPARATOR_S);

		BRASERO_BURN_LOG ("Mangled name to %s (truncated)", buffer);
		return joliet;
	}

	/* NOTE: g_hash_table_insert frees key_path */
	num ++;
	g_hash_table_insert (joliet, g_strdup (buffer), GINT_TO_POINTER (num));

	width = 1;
	while (num / (width * 10)) width ++;

	/* try to keep the extension */
	if (dot_len < 5 && dot_len > 1 )
		sprintf (name + (64 - width - dot_len),
			 "%i%s",
			 num,
			 path + dot_pos);
	else
		sprintf (name + (64 - width),
			 "%i",
			 num);

	if (has_slash)
		strcat (buffer, G_DIR_SEPARATOR_S);

	BRASERO_BURN_LOG ("Mangled name to %s", buffer);
	return joliet;
}

/**
 * brasero_track_data_get_grafts:
 * @track: a #BraseroTrackData
 *
 * Returns a list of #BraseroGraftPt.
 *
 * Do not free after usage as @track retains ownership.
 *
 * Return value: (transfer none) (element-type BraseroBurn.GraftPt) (allow-none): a #GSList of #BraseroGraftPt or %NULL if empty.
 **/

GSList *
brasero_track_data_get_grafts (BraseroTrackData *track)
{
	BraseroTrackDataClass *klass;
	GHashTable *mangle = NULL;
	BraseroImageFS image_fs;
	GSList *grafts;
	GSList *iter;

	g_return_val_if_fail (BRASERO_IS_TRACK_DATA (track), NULL);

	klass = BRASERO_TRACK_DATA_GET_CLASS (track);
	grafts = klass->get_grafts (track);

	image_fs = brasero_track_data_get_fs (track);
	if ((image_fs & BRASERO_IMAGE_FS_JOLIET) == 0)
		return grafts;

	for (iter = grafts; iter; iter = iter->next) {
		BraseroGraftPt *graft;
		gchar newpath [MAXPATHLEN];

		graft = iter->data;
		mangle = brasero_track_data_mangle_joliet_name (mangle,
								graft->path,
								newpath);

		g_free (graft->path);
		graft->path = g_strdup (newpath);
	}

	if (mangle)
		g_hash_table_destroy (mangle);

	return grafts;
}

static GSList *
brasero_track_data_get_grafts_real (BraseroTrackData *track)
{
	BraseroTrackDataPrivate *priv;

	priv = BRASERO_TRACK_DATA_PRIVATE (track);
	return priv->grafts;
}

/**
 * brasero_track_data_get_excluded_list:
 * @track: a #BraseroTrackData.
 *
 * Returns a list of URIs which must not be included in
 * the image to be created.
 * Do not free the list or any of the URIs after
 * usage as @track retains ownership.
 *
 * Return value: (transfer none) (element-type utf8) (allow-none): a #GSList of #gchar * or %NULL if no
 * URI should be excluded.
 **/

GSList *
brasero_track_data_get_excluded_list (BraseroTrackData *track)
{
	BraseroTrackDataClass *klass;

	g_return_val_if_fail (BRASERO_IS_TRACK_DATA (track), NULL);

	klass = BRASERO_TRACK_DATA_GET_CLASS (track);
	return klass->get_excluded (track);
}

static GSList *
brasero_track_data_get_excluded_real (BraseroTrackData *track)
{
	BraseroTrackDataPrivate *priv;

	priv = BRASERO_TRACK_DATA_PRIVATE (track);
	return priv->excluded;
}

/**
 * brasero_track_data_write_to_paths:
 * @track: a #BraseroTrackData.
 * @grafts_path: a #gchar.
 * @excluded_path: a #gchar.
 * @emptydir: a #gchar.
 * @videodir: (allow-none): a #gchar or %NULL.
 * @error: a #GError.
 *
 * Write to @grafts_path (a path to a file) the graft points,
 * and to @excluded_path (a path to a file) the list of paths to
 * be excluded; @emptydir is (path) is an empty
 * directory to be used for created directories;
 * @videodir (a path) is a directory to be used to build the
 * the video image.
 *
 * This is mostly for internal use by mkisofs and similar.
 *
 * This function takes care of file name mangling.
 *
 * Return value: a #BraseroBurnResult.
 **/

BraseroBurnResult
brasero_track_data_write_to_paths (BraseroTrackData *track,
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
						      brasero_track_data_get_fs (track),
						      emptydir,
						      videodir,
						      grafts_path,
						      excluded_path,
						      error);
	return result;
}

/**
 * brasero_track_data_get_file_num:
 * @track: a #BraseroTrackData.
 * @file_num: (allow-none) (out): a #guint64 or %NULL.
 *
 * Sets the number of files (not directories) in @file_num.
 *
 * Return value: a #BraseroBurnResult. %TRUE if @file_num
 * was set, %FALSE otherwise.
 **/

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

static BraseroBurnResult
brasero_track_data_get_track_type (BraseroTrack *track,
				   BraseroTrackType *type)
{
	BraseroTrackDataPrivate *priv;

	priv = BRASERO_TRACK_DATA_PRIVATE (track);

	brasero_track_type_set_has_data (type);
	brasero_track_type_set_data_fs (type, priv->fs_type);

	return BRASERO_BURN_OK;
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
	BraseroTrackDataPrivate *priv;

	priv = BRASERO_TRACK_DATA_PRIVATE (object);
	if (priv->grafts) {
		g_slist_foreach (priv->grafts, (GFunc) brasero_graft_point_free, NULL);
		g_slist_free (priv->grafts);
		priv->grafts = NULL;
	}

	if (priv->excluded) {
		g_slist_foreach (priv->excluded, (GFunc) g_free, NULL);
		g_slist_free (priv->excluded);
		priv->excluded = NULL;
	}

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

/**
 * brasero_track_data_new:
 *
 * Creates a new #BraseroTrackData.
 * 
 *This type of tracks is used to create a disc image
 * from or burn a selection of files.
 *
 * Return value: a #BraseroTrackData
 **/

BraseroTrackData *
brasero_track_data_new (void)
{
	return g_object_new (BRASERO_TYPE_TRACK_DATA, NULL);
}
