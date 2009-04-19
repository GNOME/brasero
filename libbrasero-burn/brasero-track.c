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

#include <string.h>

#include <glib.h>

#include "brasero-track.h"


typedef struct _BraseroTrackPrivate BraseroTrackPrivate;
struct _BraseroTrackPrivate
{
	GHashTable *tags;

	gchar *checksum;
	BraseroChecksumType checksum_type;
};

#define BRASERO_TRACK_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_TRACK, BraseroTrackPrivate))

enum
{
	CHANGED,

	LAST_SIGNAL
};


static guint track_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (BraseroTrack, brasero_track, G_TYPE_OBJECT);

BraseroTrackDataType
brasero_track_get_track_type (BraseroTrack *track,
			      BraseroTrackType *type)
{
	BraseroTrackClass *klass;

	g_return_val_if_fail (BRASERO_IS_TRACK (track), BRASERO_TRACK_TYPE_NONE);

	klass = BRASERO_TRACK_GET_CLASS (track);
	if (!klass->get_type)
		return BRASERO_TRACK_TYPE_NONE;

	return klass->get_type (track, type);
}

BraseroBurnResult
brasero_track_get_size (BraseroTrack *track,
			guint64 *blocks,
			guint64 *size)
{
	BraseroBurnResult res;
	BraseroTrackClass *klass;
	guint64 blocks_local = 0;
	guint block_size_local = 0;

	g_return_val_if_fail (BRASERO_IS_TRACK (track), BRASERO_TRACK_TYPE_NONE);

	klass = BRASERO_TRACK_GET_CLASS (track);
	if (!klass->get_size)
		return BRASERO_BURN_OK;

	res = klass->get_size (track, &blocks_local, &block_size_local);
	if (res != BRASERO_BURN_OK)
		return res;

	if (blocks)
		*blocks = blocks_local;

	if (size)
		*size = blocks_local * block_size_local;

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_track_get_status (BraseroTrack *track,
			  BraseroStatus *status)
{
	BraseroTrackClass *klass;

	g_return_val_if_fail (BRASERO_IS_TRACK (track), BRASERO_TRACK_TYPE_NONE);

	klass = BRASERO_TRACK_GET_CLASS (track);

	/* If this is not implement we consider that it means it is not needed
	 * and that the track doesn't perform on the side (threaded?) checks or
	 * information retrieval and that it's therefore always OK:
	 * - for example BraseroTrackDisc. */
	if (!klass->get_status) {
		if (status)
			brasero_status_set_completed (status);

		return BRASERO_BURN_OK;
	}

	return klass->get_status (track, status);
}

/**
 *
 */

BraseroBurnResult
brasero_track_set_checksum (BraseroTrack *track,
			    BraseroChecksumType type,
			    const gchar *checksum)
{
	BraseroBurnResult result = BRASERO_BURN_OK;
	BraseroTrackPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK (track), BRASERO_CHECKSUM_NONE);
	priv = BRASERO_TRACK_PRIVATE (track);

	if (type == priv->checksum_type
	&& (type == BRASERO_CHECKSUM_MD5 || type == BRASERO_CHECKSUM_SHA1 || type == BRASERO_CHECKSUM_SHA256)
	&&  checksum && strcmp (checksum, priv->checksum))
		result = BRASERO_BURN_ERR;

	if (priv->checksum)
		g_free (priv->checksum);

	priv->checksum_type = type;
	if (checksum)
		priv->checksum = g_strdup (checksum);
	else
		priv->checksum = NULL;

	return result;
}

const gchar *
brasero_track_get_checksum (BraseroTrack *track)
{
	BraseroTrackPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK (track), NULL);
	priv = BRASERO_TRACK_PRIVATE (track);

	return priv->checksum ? priv->checksum : "";
}

BraseroChecksumType
brasero_track_get_checksum_type (BraseroTrack *track)
{
	BraseroTrackPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK (track), BRASERO_CHECKSUM_NONE);
	priv = BRASERO_TRACK_PRIVATE (track);

	return priv->checksum_type;
}

/**
 * Can be used to set arbitrary data
 */
static void
brasero_track_tag_value_free (gpointer user_data)
{
	GValue *value = user_data;

	g_value_reset (value);
	g_free (value);
}

BraseroBurnResult
brasero_track_tag_add (BraseroTrack *track,
		       const gchar *tag,
		       GValue *value)
{
	BraseroTrackPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK (track), BRASERO_BURN_ERR);

	priv = BRASERO_TRACK_PRIVATE (track);

	if (!priv->tags)
		priv->tags = g_hash_table_new_full (g_str_hash,
						    g_str_equal,
						    g_free,
						    brasero_track_tag_value_free);
	g_hash_table_insert (priv->tags,
			     g_strdup (tag),
			     value);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_track_tag_add_int (BraseroTrack *track,
			   const gchar *tag,
			   int value_int)
{
	GValue *value;

	value = g_new0 (GValue, 1);
	g_value_init (value, G_TYPE_INT);
	g_value_set_int (value, value_int);

	return brasero_track_tag_add (track, tag, value);
}

BraseroBurnResult
brasero_track_tag_add_string (BraseroTrack *track,
			      const gchar *tag,
			      const gchar *string)
{
	GValue *value;

	value = g_new0 (GValue, 1);
	g_value_init (value, G_TYPE_STRING);
	g_value_set_string (value, string);

	return brasero_track_tag_add (track, tag, value);
}

BraseroBurnResult
brasero_track_tag_lookup (BraseroTrack *track,
			  const gchar *tag,
			  GValue **value)
{
	gpointer data;
	BraseroTrackPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TRACK (track), BRASERO_BURN_ERR);

	priv = BRASERO_TRACK_PRIVATE (track);

	if (!priv->tags)
		return BRASERO_BURN_ERR;

	data = g_hash_table_lookup (priv->tags, tag);
	if (!data)
		return BRASERO_BURN_ERR;

	if (value)
		*value = data;

	return BRASERO_BURN_OK;
}

int
brasero_track_tag_lookup_int (BraseroTrack *track,
			      const gchar *tag)
{
	GValue *value = NULL;
	BraseroBurnResult res;

	res = brasero_track_tag_lookup (track, tag, &value);
	if (res != BRASERO_BURN_OK)
		return 0;

	if (!value)
		return 0;

	if (!G_VALUE_HOLDS_INT (value))
		return 0;

	return g_value_get_int (value);
}

const gchar *
brasero_track_tag_lookup_string (BraseroTrack *track,
				 const gchar *tag)
{
	GValue *value = NULL;
	BraseroBurnResult res;

	res = brasero_track_tag_lookup (track, tag, &value);
	if (res != BRASERO_BURN_OK)
		return NULL;

	if (!value)
		return NULL;

	if (!G_VALUE_HOLDS_STRING (value))
		return NULL;

	return g_value_get_string (value);
}

void
brasero_track_tag_copy_missing (BraseroTrack *dest,
				BraseroTrack *src)
{
	BraseroTrackPrivate *priv;
	GHashTableIter iter;
	gpointer new_value;
	gpointer new_key;
	gpointer value;
	gpointer key;

	g_return_if_fail (BRASERO_IS_TRACK (dest));
	g_return_if_fail (BRASERO_IS_TRACK (src));

	priv = BRASERO_TRACK_PRIVATE (src);

	if (!priv->tags)
		return;

	g_hash_table_iter_init (&iter, priv->tags);

	priv = BRASERO_TRACK_PRIVATE (dest);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		if (g_hash_table_lookup (priv->tags, key))
			continue;

		new_value = g_new0 (GValue, 1);

		g_value_init (new_value, g_value_get_gtype (value));
		g_value_copy (new_value, value);

		new_key = g_strdup (key);

		g_hash_table_insert (priv->tags, new_key, new_value);
	}
}

void
brasero_track_changed (BraseroTrack *track)
{
	g_signal_emit (track,
		       track_signals [CHANGED],
		       0);
}


/**
 * GObject part
 */

static void
brasero_track_init (BraseroTrack *object)
{ }

static void
brasero_track_finalize (GObject *object)
{
	BraseroTrackPrivate *priv;

	priv = BRASERO_TRACK_PRIVATE (object);

	if (priv->tags) {
		g_hash_table_destroy (priv->tags);
		priv->tags = NULL;
	}

	if (priv->checksum) {
		g_free (priv->checksum);
		priv->checksum = NULL;
	}

	G_OBJECT_CLASS (brasero_track_parent_class)->finalize (object);
}

static void
brasero_track_class_init (BraseroTrackClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroTrackPrivate));

	object_class->finalize = brasero_track_finalize;

	track_signals[CHANGED] =
		g_signal_new ("changed",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
		              G_STRUCT_OFFSET (BraseroTrackClass, changed),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0,
		              G_TYPE_NONE);
}
