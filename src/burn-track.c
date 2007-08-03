/***************************************************************************
 *            burn-track.c
 *
 *  Thu Dec  7 09:50:38 2006
 *  Copyright  2006  algernon
 *  <algernon@localhost.localdomain>
 ****************************************************************************/

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include <glib.h>

#include "burn-track.h"
#include "burn-debug.h"
#include "burn-medium.h"
#include "brasero-ncb.h"
#include "burn-mkisofs-base.h"

struct _BraseroTrack {
	BraseroTrackType type;

	int ref;

	gint64 size;
	gint64 blocks;
	gint block_size;

	gchar *checksum;
	BraseroChecksumType checksum_type;
};

typedef struct {
	BraseroTrack track;

	GSList *grafts;			/* BraseroGraftPt */
	GSList *excluded;		/* list of uris (char*) that are to be always excluded */
} BraseroTrackData;

typedef struct {
	BraseroTrack track;

	NautilusBurnDrive *disc;
} BraseroTrackDisc;

typedef struct {
	BraseroTrack track;

	gchar *location;
	BraseroSongInfo *info;

	/* only used when it's a song */
	guint64 start;
	guint64 gap;
} BraseroTrackAudio;

typedef struct {
	BraseroTrack track;

	gchar *image;
	gchar *toc;
} BraseroTrackImage;

void
brasero_graft_point_free (BraseroGraftPt *graft)
{
	if (graft->uri)
		g_free (graft->uri);

	g_free (graft->path);

	g_slist_foreach (graft->excluded, (GFunc) g_free, NULL);
	g_slist_free (graft->excluded);

	g_free (graft);
}

BraseroGraftPt *
brasero_graft_point_copy (BraseroGraftPt *graft)
{
	BraseroGraftPt *newgraft;
	GSList *iter;
	gchar *uri;

	g_return_val_if_fail (graft != NULL, NULL);

	newgraft = g_new0 (BraseroGraftPt, 1);
	newgraft->path = g_strdup (graft->path);
	if (graft->uri)
		newgraft->uri = g_strdup (graft->uri);

	for (iter = graft->excluded; iter; iter = iter->next) {
		uri = iter->data;
		newgraft->excluded = g_slist_prepend (newgraft->excluded,
						      g_strdup (uri));
	}

	newgraft->excluded = g_slist_reverse (newgraft->excluded);
	return newgraft;
}

void
brasero_song_info_free (BraseroSongInfo *info)
{
	if (!info)
		return;

	g_free (info->title);
	g_free (info->artist);
	g_free (info->composer);
	g_free (info);
}

BraseroSongInfo *
brasero_song_info_copy (BraseroSongInfo *info)
{
	BraseroSongInfo *copy;

	copy = g_new0 (BraseroSongInfo, 1);

	copy->title = g_strdup (info->title);
	copy->artist = g_strdup (info->artist);
	copy->composer = g_strdup (info->composer);
	copy->isrc = info->isrc;

	return copy;
}

gboolean
brasero_track_type_equal (const BraseroTrackType *type_A,
			  const BraseroTrackType *type_B)
{
	if (type_A->type != type_B->type)
		return FALSE;

	switch (type_A->type) {
	case BRASERO_TRACK_TYPE_DATA:
		if (type_A->subtype.fs_type != type_B->subtype.fs_type)
			return FALSE;
		break;
	
	case BRASERO_TRACK_TYPE_DISC:
		if (type_B->subtype.media != type_A->subtype.media)
			return FALSE;
		break;
	
	case BRASERO_TRACK_TYPE_IMAGE:
		if (type_A->subtype.img_format != type_B->subtype.img_format)
			return FALSE;
		break;

	case BRASERO_TRACK_TYPE_AUDIO:
		if (type_A->subtype.audio_format != type_B->subtype.audio_format)
			return FALSE;
		break;

	default:
		break;
	}

	return TRUE;
}

gboolean
brasero_track_type_match (const BraseroTrackType *type_A,
			  const BraseroTrackType *type_B)
{
	if (type_A->type != type_B->type)
		return FALSE;

	switch (type_A->type) {
	case BRASERO_TRACK_TYPE_DATA:
		if (!(type_A->subtype.fs_type & type_B->subtype.fs_type))
			return FALSE;
		break;
	
	case BRASERO_TRACK_TYPE_DISC:
		if (!(type_A->subtype.media & type_B->subtype.media))
			return FALSE;
		break;
	
	case BRASERO_TRACK_TYPE_IMAGE:
		if (!(type_A->subtype.img_format & type_B->subtype.img_format))
			return FALSE;
		break;

	case BRASERO_TRACK_TYPE_AUDIO:
		break;

	default:
		break;
	}

	return TRUE;
}

static void
brasero_track_clean (BraseroTrack *track)
{
	g_return_if_fail (track != NULL);

	if (track->type.type == BRASERO_TRACK_TYPE_AUDIO) {
		BraseroTrackAudio *audio = (BraseroTrackAudio *) track;

		g_free (audio->location);
		brasero_song_info_free (audio->info);
	}
	else if (track->type.type == BRASERO_TRACK_TYPE_DATA) {
		GSList *iter;
		BraseroGraftPt *graft;
		BraseroTrackData *data = (BraseroTrackData *) track;

		for (iter = data->grafts; iter; iter = iter->next) {
			graft = iter->data;

			if (graft->uri)
				g_free (graft->uri);
			if (graft->path)
				g_free (graft->path);

			if (graft->excluded) {
				g_slist_foreach (graft->excluded, (GFunc) g_free, NULL);
				g_slist_free (graft->excluded);
			}

			g_free (graft);
		}
		g_slist_free (data->grafts);

		g_slist_foreach (data->excluded, (GFunc) g_free, NULL);
		g_slist_free (data->excluded);
	}
	else if (track->type.type == BRASERO_TRACK_TYPE_DISC) {
		BraseroTrackDisc *drive = (BraseroTrackDisc *) track;

		nautilus_burn_drive_unref (drive->disc);
	}
	else if (track->type.type == BRASERO_TRACK_TYPE_IMAGE) {
		BraseroTrackImage *image = (BraseroTrackImage *) track;

		g_free (image->image);
		g_free (image->toc);
	}

	g_free (track->checksum);
	memset (track, 0, sizeof (BraseroTrack));
}

void
brasero_track_unref (BraseroTrack *track)
{
	if (!track)
		return;

	track->ref--;

	if (track->ref >= 1)
		return;

	brasero_track_clean (track);
	g_free (track);
}

BraseroTrack *
brasero_track_new (BraseroTrackDataType type)
{
	BraseroTrack *track;

	switch (type) {
	case BRASERO_TRACK_TYPE_DATA:
		track = (BraseroTrack *) g_new0 (BraseroTrackAudio, 1);
		break;
	case BRASERO_TRACK_TYPE_DISC:
		track = (BraseroTrack *) g_new0 (BraseroTrackAudio, 1);
		break;
	case BRASERO_TRACK_TYPE_IMAGE:
		track = (BraseroTrack *) g_new0 (BraseroTrackAudio, 1);
		break;
	case BRASERO_TRACK_TYPE_AUDIO:
		track = (BraseroTrack *) g_new0 (BraseroTrackAudio, 1);
		break;
	default:
		return NULL;
	}

	track->ref = 1;
	track->type.type = type;

	return track;
}

void
brasero_track_ref (BraseroTrack *track)
{
	if (!track)
		return;

	track->ref ++;
}

BraseroTrackDataType
brasero_track_get_type (BraseroTrack *track,
			BraseroTrackType *type)
{
	if (!track)
		return BRASERO_TRACK_TYPE_NONE;

	if (!type)
		return track->type.type;

	memcpy (type, &track->type, sizeof (BraseroTrackType));
	if (track->type.type == BRASERO_TRACK_TYPE_DISC) {
		BraseroTrackDisc *disc;

		disc = (BraseroTrackDisc *) track;
		type->subtype.media = NCB_MEDIA_GET_STATUS (disc->disc);
	}

	return track->type.type;
}

static void
brasero_track_data_copy (BraseroTrackData *track, BraseroTrackData *copy)
{
	GSList *iter;

	for (iter = track->grafts; iter; iter = iter->next) {
		BraseroGraftPt *graft;

		graft = iter->data;
		graft = brasero_graft_point_copy (graft);
		copy->grafts = g_slist_prepend (copy->grafts, graft);
	}

	for (iter = track->excluded; iter; iter = iter->next) {
		gchar *excluded;

		excluded = iter->data;
		copy->excluded = g_slist_prepend (copy->excluded,
						  g_strdup (excluded));
	}
}

static void
brasero_track_disc_copy (BraseroTrackDisc *track, BraseroTrackDisc *copy)
{
	nautilus_burn_drive_ref (track->disc);
	copy->disc = track->disc;
}

static void
brasero_track_audio_copy (BraseroTrackAudio *track, BraseroTrackAudio *copy)
{
	copy->gap = track->gap;
	copy->start = track->start;
	copy->location = g_strdup (track->location);
	copy->info = brasero_song_info_copy (track->info);
}

static void
brasero_track_image_copy (BraseroTrackImage *track, BraseroTrackImage *copy)
{
	copy->toc = g_strdup (track->toc);
	copy->image = g_strdup (track->image);
}

BraseroTrack *
brasero_track_copy (BraseroTrack *track)
{
	BraseroTrackType type;
	BraseroTrack *copy;

	brasero_track_get_type (track, &type);
	copy = brasero_track_new (type.type);

	memcpy (copy, track, sizeof (BraseroTrack));
	if (copy->checksum)
		copy->checksum = g_strdup (copy->checksum);

	switch (type.type) {
	case BRASERO_TRACK_TYPE_DATA:
		brasero_track_data_copy ((BraseroTrackData *) track,
					 (BraseroTrackData *) copy);
		break;
	case BRASERO_TRACK_TYPE_DISC:
		brasero_track_disc_copy ((BraseroTrackDisc *) track,
					 (BraseroTrackDisc *) copy);
		break;
	case BRASERO_TRACK_TYPE_AUDIO:
		brasero_track_audio_copy ((BraseroTrackAudio *) track,
					  (BraseroTrackAudio *) copy);
		break;		
	case BRASERO_TRACK_TYPE_IMAGE:
		brasero_track_image_copy ((BraseroTrackImage *) track,
					  (BraseroTrackImage *) copy);
		break;
	default:
		return track;
	}

	return copy;
}

BraseroBurnResult
brasero_track_set_drive_source (BraseroTrack *track, NautilusBurnDrive *drive)
{
	BraseroTrackDisc *disc;

	if (track->type.type != BRASERO_TRACK_TYPE_DISC)
		return BRASERO_BURN_NOT_SUPPORTED;

	disc = (BraseroTrackDisc *) track;

	if (disc->disc)
		nautilus_burn_drive_unref (disc->disc);

	nautilus_burn_drive_ref (drive);
	disc->disc = drive;

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_track_set_audio_source (BraseroTrack *track,
				const gchar *uri,
				BraseroAudioFormat format)
{
	BraseroTrackAudio *audio;

	if (track->type.type != BRASERO_TRACK_TYPE_AUDIO)
		return BRASERO_BURN_NOT_SUPPORTED;

	audio = (BraseroTrackAudio *) track;

	if (audio->location)
		g_free (audio->location);

	if (format == BRASERO_AUDIO_FORMAT_NONE) {
		if (uri)
			BRASERO_BURN_LOG ("Setting a NONE audio format with a valid uri");

		track->type.subtype.audio_format = format;
		audio->location = NULL;
		return BRASERO_BURN_OK;
	}

	track->type.subtype.audio_format = format;
	audio->location = g_strdup (uri);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_track_set_audio_info (BraseroTrack *track,
			      BraseroSongInfo *info)
{
	BraseroTrackAudio *audio;

	if (track->type.type != BRASERO_TRACK_TYPE_AUDIO)
		return BRASERO_BURN_NOT_SUPPORTED;

	audio = (BraseroTrackAudio *) track;

	if (audio->info)
		brasero_song_info_free (info);

	audio->info = info;
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_track_set_audio_boundaries (BraseroTrack *track,
				    gint64 start,
				    gint64 gap)
{
	BraseroTrackAudio *audio;

	if (track->type.type != BRASERO_TRACK_TYPE_AUDIO)
		return BRASERO_BURN_NOT_SUPPORTED;

	audio = (BraseroTrackAudio *) track;

	audio->gap = gap;
	audio->start = start;
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_track_set_data_source (BraseroTrack *track,
			       GSList *grafts,
			       GSList *unreadable)
{
	BraseroTrackData *data;

	if (track->type.type != BRASERO_TRACK_TYPE_DATA)
		return BRASERO_BURN_NOT_SUPPORTED;

	data = (BraseroTrackData *) track;

	if (data->grafts) {
		g_slist_foreach (data->grafts, (GFunc) brasero_graft_point_free, NULL);
		g_slist_free (data->grafts);
	}

	if (data->excluded) {
		g_slist_foreach (data->excluded, (GFunc) g_free, NULL);
		g_slist_free (data->excluded);
	}

	data->grafts = grafts;
	data->excluded = unreadable;
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_track_set_data_fs (BraseroTrack *track,
			   BraseroImageFS fstype)
{
	if (track->type.type != BRASERO_TRACK_TYPE_DATA)
		return BRASERO_BURN_NOT_SUPPORTED;

	track->type.subtype.fs_type = fstype;

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_track_add_data_fs (BraseroTrack *track,
			   BraseroImageFS fstype)
{
	if (track->type.type != BRASERO_TRACK_TYPE_DATA)
		return BRASERO_BURN_NOT_SUPPORTED;

	fstype |= track->type.subtype.fs_type;
	track->type.subtype.fs_type = fstype;

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_track_unset_data_fs (BraseroTrack *track,
			     BraseroImageFS fstype)
{
	BraseroImageFS fstypes;

	if (track->type.type != BRASERO_TRACK_TYPE_DATA)
		return BRASERO_BURN_NOT_SUPPORTED;

	fstypes = track->type.subtype.fs_type;
	fstypes &= ~fstype;
	track->type.subtype.fs_type = fstypes;

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_track_set_image_source (BraseroTrack *track,
				const gchar *path,
				const gchar *toc,
				BraseroImageFormat format)
{
	BraseroTrackImage *image;

	if (track->type.type != BRASERO_TRACK_TYPE_IMAGE)
		return BRASERO_BURN_NOT_SUPPORTED;

	if (format == BRASERO_IMAGE_FORMAT_NONE)
		return BRASERO_BURN_NOT_SUPPORTED;

	track->type.subtype.img_format = format;

	image = (BraseroTrackImage *) track;

	if (image->image)
		g_free (image->image);

	if (image->toc)
		g_free (image->toc);

	image->image = g_strdup (path);
	image->toc = g_strdup (toc);
	return BRASERO_BURN_OK;
}

/**
 * Used to retrieve the data
 */

static gchar *
brasero_track_get_localpath (const gchar *uri)
{
	gchar *localpath;
	gchar *realuri;

	if (!uri)
		return NULL;

	if (uri [0] == '/')
		return g_strdup (uri);

	if (strncmp (uri, "file://", 7))
		return NULL;

	realuri = gnome_vfs_make_uri_from_input (uri);
	localpath = gnome_vfs_get_local_path_from_uri (realuri);
	g_free (realuri);

	return localpath;
}

gchar *
brasero_track_get_audio_source (BraseroTrack *track, gboolean local)
{
	BraseroTrackAudio *audio;

	if (track->type.type != BRASERO_TRACK_TYPE_AUDIO)
		return NULL;

	audio = (BraseroTrackAudio *) track;
	if (local)
		return brasero_track_get_localpath (audio->location);

	return g_strdup (audio->location);
}

gint64
brasero_track_get_audio_gap (BraseroTrack *track)
{
	BraseroTrackAudio *audio;

	if (track->type.type != BRASERO_TRACK_TYPE_AUDIO)
		return -1;

	audio = (BraseroTrackAudio *) track;
	return audio->gap;
}

gint64
brasero_track_get_audio_start (BraseroTrack *track)
{
	BraseroTrackAudio *audio;

	if (track->type.type != BRASERO_TRACK_TYPE_AUDIO)
		return -1;

	audio = (BraseroTrackAudio *) track;
	return audio->start;
}

BraseroSongInfo *
brasero_track_get_audio_info (BraseroTrack *track)
{
	BraseroTrackAudio *audio;

	if (track->type.type != BRASERO_TRACK_TYPE_AUDIO)
		return NULL;

	audio = (BraseroTrackAudio *) track;
	return brasero_song_info_copy (audio->info);
}

GSList *
brasero_track_get_data_grafts_source (BraseroTrack *track)
{
	BraseroTrackData *data;

	if (track->type.type != BRASERO_TRACK_TYPE_DATA)
		return NULL;

	data = (BraseroTrackData *) track;
	return data->grafts;
}

GSList *
brasero_track_get_data_excluded_source (BraseroTrack *track)
{
	BraseroTrackData *data;

	if (track->type.type != BRASERO_TRACK_TYPE_DATA)
		return NULL;

	data = (BraseroTrackData *) track;
	return data->excluded;
}

BraseroBurnResult
brasero_track_get_data_paths (BraseroTrack *track,
			      const gchar *grafts_path,
			      const gchar *excluded_path,
			      const gchar *emptydir,
			      GError **error)
{
	BraseroBurnResult result;
	BraseroTrackData *data;

	if (track->type.type != BRASERO_TRACK_TYPE_DATA)
		return BRASERO_BURN_ERR;

	data = (BraseroTrackData *) track;
	result = brasero_mkisofs_base_write_to_files (data->grafts,
						      data->excluded,
						      emptydir,
						      grafts_path,
						      excluded_path,
						      error);

	return result;
}

NautilusBurnDrive *
brasero_track_get_drive_source (BraseroTrack *track)
{
	BraseroTrackDisc *drive;

	if (track->type.type != BRASERO_TRACK_TYPE_DISC)
		return NULL;

	drive = (BraseroTrackDisc *) track;

	return drive->disc;
}

gchar *
brasero_track_get_image_source (BraseroTrack *track, gboolean uri)
{
	BraseroTrackImage *image;

	if (track->type.type != BRASERO_TRACK_TYPE_IMAGE)
		return NULL;

	image = (BraseroTrackImage *) track;

	if (uri)
		return g_strdup (image->image);
	else
		return brasero_track_get_localpath (image->image);
}

gchar *
brasero_track_get_toc_source (BraseroTrack *track, gboolean uri)
{
	BraseroTrackImage *image;

	if (track->type.type != BRASERO_TRACK_TYPE_IMAGE)
		return NULL;

	image = (BraseroTrackImage *) track;

	if (uri)
		return g_strdup (image->toc);
	else
		return brasero_track_get_localpath (image->toc);
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

	if (type == track->checksum_type
	&&  type == BRASERO_CHECKSUM_MD5
	&&  strcmp (checksum, track->checksum))
		result = BRASERO_BURN_ERR;

	if (track->checksum)
		g_free (track->checksum);

	track->checksum_type = type;
	track->checksum = g_strdup (checksum);
	return result;
}

const gchar *
brasero_track_get_checksum (BraseroTrack *track)
{
	return track->checksum;
}

BraseroChecksumType
brasero_track_get_checksum_type (BraseroTrack *track)
{
	return track->checksum_type;
}

void
brasero_track_set_estimated_size (BraseroTrack *track,
				  gint64 block_size,
				  gint64 blocks,
				  gint64 size)
{
	track->block_size = block_size;
	track->blocks = blocks;
	track->size = size;
}

BraseroBurnResult
brasero_track_get_estimated_size (BraseroTrack *track,
				  gint64 *block_size,
				  gint64 *blocks,
				  gint64 *size)
{
	if (track->block_size <= 0
	&&  track->blocks <= 0
	&&  track->size <= 0)
		return BRASERO_BURN_NOT_READY;

	if (block_size) {
		if (track->block_size <= 0)
			*block_size = track->size / track->blocks;
		else
			*block_size = track->block_size;
	}

	if (blocks) {
		if (track->blocks <= 0)
			*blocks = track->size / track->block_size;
		else
			*blocks = track->blocks;
	}

	if (size) {
		if (track->size <= 0)
			*size = track->blocks * track->block_size;
		else
			*size = track->size;
	}

	return BRASERO_BURN_OK;
}
