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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 
/***************************************************************************
 *            burn-basics.c
 *
 *  Sat Feb 11 16:55:54 2006
 *  Copyright  2006  philippe
 *  <philippe@algernon.localdomain>
 ****************************************************************************/

#include <string.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <libgnomevfs/gnome-vfs.h>

#include "burn-basics.h"
#include "burn-common.h"

GQuark
brasero_burn_quark (void)
{
	static GQuark quark = 0;

	if (!quark)
		quark = g_quark_from_static_string ("BraseroBurnError");

	return quark;
}
 
const gchar *
brasero_burn_action_to_string (BraseroBurnAction action)
{
	gchar *strings [BRASERO_BURN_ACTION_LAST] = { 	"",
							N_("Getting size"),
							N_("Creating checksum"),
							N_("Creating image"),
							N_("Copying disc"),
							N_("Copying file"),
							N_("Analysing audio information"),
							N_("Transcoding song"),
							N_("Preparing to write"),
							N_("Writing leadin"),
							N_("Writing"),
							N_("Writing CD-TEXT information"),
							N_("Fixating"),
							N_("Writing leadout"),
							N_("Blanking"),
							N_("Success") };
	return _(strings [action]);
}

gchar *
brasero_track_source_get_image_localpath (BraseroTrackSource *track)
{
	gchar *localpath;
	gchar *uri;

	if (track->type != BRASERO_TRACK_SOURCE_IMAGE
	&& (track->format & BRASERO_IMAGE_FORMAT_ISO))
		return NULL;

	if (!track->contents.image.image)
		return NULL;

	if (track->contents.image.image [0] == '/')
		return g_strdup (track->contents.image.image);

	uri = gnome_vfs_make_uri_from_input (track->contents.image.image);
	localpath = gnome_vfs_get_local_path_from_uri (uri);
	g_free (uri);

	return localpath;
}

gchar *
brasero_track_source_get_raw_localpath (BraseroTrackSource *track)
{
	gchar *localpath;
	gchar *uri;

	/* NOTE: here cdrecord doesn't need *.toc image but the raw part */
	if (track->type != BRASERO_TRACK_SOURCE_IMAGE
	&& (track->format & BRASERO_IMAGE_FORMAT_CLONE))
		return NULL;

	if (!track->contents.image.image)
		return NULL;

	if (track->contents.image.image [0] == '/')
		return g_strdup (track->contents.image.image);

	uri = gnome_vfs_make_uri_from_input (track->contents.image.image);
	localpath = gnome_vfs_get_local_path_from_uri (uri);
	g_free (uri);

	return localpath;
}

gchar *
brasero_track_source_get_cue_localpath (BraseroTrackSource *track)
{
	gchar *localpath;
	gchar *uri;

	if (track->type != BRASERO_TRACK_SOURCE_IMAGE
	&& (track->format & BRASERO_IMAGE_FORMAT_CUE))
		return NULL;

	if (!track->contents.image.toc)
		return NULL;

	if (track->contents.image.toc [0] == '/')
		return g_strdup (track->contents.image.toc);

    	uri = gnome_vfs_make_uri_from_input (track->contents.image.toc);
	localpath = gnome_vfs_get_local_path_from_uri (uri);
	g_free (uri);

	return localpath;
}

void
brasero_track_source_free (BraseroTrackSource *track)
{
	g_return_if_fail (track != NULL);

	if (track->type == BRASERO_TRACK_SOURCE_SONG) {
		GSList *iter;
		BraseroSongFile *song;

		if (track->contents.songs.album)
			g_free (track->contents.songs.album);

		for (iter = track->contents.songs.files; iter; iter = iter->next) {
			song = iter->data;

			if (song->title)
				g_free (song->title);
			if (song->artist)
				g_free (song->artist);
			if (song->composer)
				g_free (song->composer);
			if (song->uri)
				g_free (song->uri);
		}

		g_slist_free (track->contents.songs.files);
	}
	else if (track->type == BRASERO_TRACK_SOURCE_DATA) {
		GSList *iter;
		BraseroGraftPt *graft;

		if (track->contents.data.label)
			g_free (track->contents.data.label);

		for (iter = track->contents.data.grafts; iter; iter = iter->next) {
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
		g_slist_free (track->contents.data.grafts);

		g_slist_foreach (track->contents.data.excluded, (GFunc) g_free, NULL);
		g_slist_free (track->contents.data.excluded);
	}
	else if (track->type == BRASERO_TRACK_SOURCE_GRAFTS) {
		if (track->contents.grafts.grafts_path) {
			g_free (track->contents.grafts.grafts_path);
		}

		if (track->contents.grafts.excluded_path) {
			g_free (track->contents.grafts.excluded_path);
		}

		if (track->contents.grafts.label)
			g_free (track->contents.grafts.label);
	}
	else if (track->type == BRASERO_TRACK_SOURCE_DISC)
		nautilus_burn_drive_unref (track->contents.drive.disc);
	else if (track->type == BRASERO_TRACK_SOURCE_INF) {
		if (track->contents.inf.album)
			g_free (track->contents.inf.album);

		g_slist_foreach (track->contents.inf.infos,
				 (GFunc) brasero_song_info_free,
				 NULL);

		g_slist_free (track->contents.inf.infos);
	}
	else if (track->type == BRASERO_TRACK_SOURCE_AUDIO) {
		GSList *iter;

		for (iter = track->contents.audio.files; iter; iter = iter->next)
			g_free (iter->data);

		g_slist_free (track->contents.audio.files);
	}
	else if (track->type == BRASERO_TRACK_SOURCE_IMAGE) {
		g_free (track->contents.image.image);
		g_free (track->contents.image.toc);
	}
	else if (track->type == BRASERO_TRACK_SOURCE_IMAGER)
		g_object_unref (track->contents.imager.obj);

	g_free (track);
}

BraseroTrackSource *
brasero_track_source_copy (const BraseroTrackSource *track)
{
	BraseroTrackSource *copy;

	g_return_val_if_fail (track != NULL, NULL);

	copy = g_new0 (BraseroTrackSource, 1);
	copy->type = track->type;
	copy->format = track->format;

	if (track->type == BRASERO_TRACK_SOURCE_SONG) {
		GSList *iter;
		BraseroSongFile *song;
		BraseroSongFile *song_copy;

		copy->contents.songs.album = g_strdup (track->contents.songs.album);
		for (iter = track->contents.songs.files; iter; iter = iter->next) {
			song = iter->data;

			song_copy = g_new0 (BraseroSongFile, 1);
			copy->contents.songs.files = g_slist_append (copy->contents.songs.files,
								      song_copy);
			if (song->title)
				song_copy->title = g_strdup (song->title);
			if (song->artist)
				song_copy->artist = g_strdup (song->artist);
			if (song->composer)
				song_copy->composer = g_strdup (song->composer);
			if (song->uri)
				song_copy->uri = g_strdup (song->uri);
			song_copy->isrc = song->isrc;
			song_copy->gap = song->gap;
		}
	}
	else if (track->type == BRASERO_TRACK_SOURCE_DATA) {
		GSList *iter;
		BraseroGraftPt *graft;

		if (track->contents.data.label)
			copy->contents.data.label = g_strdup (track->contents.data.label);

		for (iter = track->contents.data.grafts; iter; iter = iter->next) {
			BraseroGraftPt *graft_copy;
			GSList *excluded;

			graft = iter->data;
			graft_copy = g_new0 (BraseroGraftPt, 1);

			if (graft->uri)
				graft_copy->uri = g_strdup (graft->uri);
			if (graft->path)
				graft_copy->path = g_strdup (graft->path);

			for (excluded = graft->excluded; excluded; excluded = excluded->next)
				graft_copy->excluded = g_slist_append (graft_copy->excluded,
									g_strdup (excluded->data));

			copy->contents.data.grafts = g_slist_append (copy->contents.data.grafts, graft_copy);
		}

		/* copy the excluded files */
		for (iter = track->contents.data.excluded; iter; iter = iter->next) {
			gchar *uri;

			uri = iter->data;
			copy->contents.data.excluded = g_slist_append (copy->contents.data.excluded, g_strdup (uri));
		}
	}
	else if (track->type == BRASERO_TRACK_SOURCE_GRAFTS) {
		if (track->contents.grafts.grafts_path)
			copy->contents.grafts.grafts_path = g_strdup (track->contents.grafts.grafts_path);

		if (track->contents.grafts.excluded_path)
			copy->contents.grafts.excluded_path = g_strdup (track->contents.grafts.excluded_path);

		if (track->contents.grafts.label)
			copy->contents.grafts.label = g_strdup (track->contents.grafts.label);
	}
	else if (track->type == BRASERO_TRACK_SOURCE_DISC) {
		copy->contents.drive.disc = track->contents.drive.disc;
		nautilus_burn_drive_ref (track->contents.drive.disc);
	}
	else if (track->type == BRASERO_TRACK_SOURCE_INF) {
		GSList *iter;

		if (track->contents.inf.album)
			copy->contents.inf.album = g_strdup (track->contents.inf.album);

		for (iter = track->contents.inf.infos; iter; iter = iter->next) {
			BraseroSongInfo *info;

			info = iter->data;
			copy->contents.inf.infos = g_slist_append (copy->contents.inf.infos,
								   brasero_song_info_copy (info));
		}
	}
	else if (track->type == BRASERO_TRACK_SOURCE_AUDIO) {
		GSList *iter;

		for (iter = track->contents.audio.files; iter; iter = iter->next) {
			char *file;

			file = g_strdup (iter->data);
			copy->contents.audio.files = g_slist_append (copy->contents.audio.files, file);
		}
	}
	else if (track->type == BRASERO_TRACK_SOURCE_IMAGE) {
		copy->contents.image.image = g_strdup (track->contents.image.image);
		copy->contents.image.toc = g_strdup (track->contents.image.toc);
	}
	else if (track->type == BRASERO_TRACK_SOURCE_IMAGER) {
		copy->contents.imager.obj = track->contents.imager.obj;
		g_object_ref (copy->contents.imager.obj);
	}
	else if (track->type == BRASERO_TRACK_SOURCE_SUM)
		memcpy (copy, track, sizeof (BraseroTrackSource));
	else {
		g_free (copy);
		copy = NULL;
	}

	return copy;
}

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
brasero_graft_point_copy (BraseroGraftPt *graft) {
	BraseroGraftPt *newgraft;
	GSList *iter;
	char *uri;

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
	if (info->path)
		g_free (info->path);

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

	if (info->path)
		copy->path = g_strdup (info->path);

	copy->title = g_strdup (info->title);
	copy->artist = g_strdup (info->artist);
	copy->composer = g_strdup (info->composer);
	copy->isrc = info->isrc;
	copy->duration = info->duration;
	copy->sectors = info->sectors;

	return copy;
}
