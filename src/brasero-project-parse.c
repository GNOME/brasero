/***************************************************************************
 *            brasero-project-parse.c
 *
 *  dim nov 27 14:58:13 2008
 *  Copyright  2005-2008  Rouquier Philippe
 *  brasero-app@wanadoo.fr
 ***************************************************************************/

/*
 *  Brasero is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Brasero is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include <libxml/xmlerror.h>
#include <libxml/xmlwriter.h>
#include <libxml/parser.h>
#include <libxml/xmlstring.h>
#include <libxml/uri.h>

#ifdef BUILD_PLAYLIST
#include <totem-pl-parser.h>
#endif

#include "brasero-project-parse.h"
#include "brasero-app.h"

#include "brasero-track-stream.h"
#include "brasero-track-data.h"

void
brasero_stream_info_free (BraseroStreamInfo *info)
{
	if (!info)
		return;

	g_free (info->title);
	g_free (info->artist);
	g_free (info->composer);
	g_free (info);
}

BraseroStreamInfo *
brasero_stream_info_copy (BraseroStreamInfo *info)
{
	BraseroStreamInfo *copy;

	if (!info)
		return NULL;

	copy = g_new0 (BraseroStreamInfo, 1);

	copy->title = g_strdup (info->title);
	copy->artist = g_strdup (info->artist);
	copy->composer = g_strdup (info->composer);
	copy->isrc = info->isrc;

	return copy;
}

static void
brasero_track_clear_song (gpointer data)
{
	BraseroDiscSong *song;

	song = data;

	if (song->info)
		brasero_stream_info_free (song->info);

	g_free (song->uri);
	g_free (song);
}

void
brasero_track_clear (BraseroDiscTrack *track)
{
	if (!track)
		return;

	if (track->label) {
		g_free (track->label);
		track->label = NULL;
	}

	if (track->cover) {
		g_free (track->cover);
		track->cover = NULL;
	}

	if (track->type == BRASERO_PROJECT_TYPE_AUDIO) {
		g_slist_foreach (track->contents.tracks, (GFunc) brasero_track_clear_song, NULL);
		g_slist_free (track->contents.tracks);
	}
	else if (track->type == BRASERO_PROJECT_TYPE_DATA) {
		g_slist_foreach (track->contents.data.grafts, (GFunc) brasero_graft_point_free, NULL);
		g_slist_free (track->contents.data.grafts);
		g_slist_foreach (track->contents.data.restored, (GFunc) g_free, NULL);
		g_slist_free (track->contents.data.restored);
		g_slist_foreach (track->contents.data.excluded, (GFunc) g_free, NULL);
		g_slist_free (track->contents.data.excluded);
	}
}

void
brasero_track_free (BraseroDiscTrack *track)
{
	brasero_track_clear (track);
	g_free (track);
}

static void
brasero_project_invalid_project_dialog (const char *reason)
{
	brasero_app_alert (brasero_app_get_default (),
			   _("Error while loading the project"),
			   reason,
			   GTK_MESSAGE_ERROR);
}

static gboolean
_read_graft_point (xmlDocPtr project,
		   xmlNodePtr graft,
		   BraseroDiscTrack *track)
{
	BraseroGraftPt *retval;

	retval = g_new0 (BraseroGraftPt, 1);
	while (graft) {
		if (!xmlStrcmp (graft->name, (const xmlChar *) "uri")) {
			xmlChar *uri;

			if (retval->uri)
				goto error;

			uri = xmlNodeListGetString (project,
						    graft->xmlChildrenNode,
						    1);
			retval->uri = g_uri_unescape_string ((char *)uri, NULL);
			g_free (uri);
			if (!retval->uri)
				goto error;
		}
		else if (!xmlStrcmp (graft->name, (const xmlChar *) "path")) {
			if (retval->path)
				goto error;

			retval->path = (char *) xmlNodeListGetString (project,
								      graft->xmlChildrenNode,
								      1);
			if (!retval->path)
				goto error;
		}
		else if (!xmlStrcmp (graft->name, (const xmlChar *) "excluded")) {
			xmlChar *excluded;

			excluded = xmlNodeListGetString (project,
							 graft->xmlChildrenNode,
							 1);
			if (!excluded)
				goto error;

			track->contents.data.excluded = g_slist_prepend (track->contents.data.excluded,
									 xmlURIUnescapeString ((char*) excluded, 0, NULL));
			g_free (excluded);
		}
		else if (graft->type == XML_ELEMENT_NODE)
			goto error;

		graft = graft->next;
	}

	track->contents.data.grafts = g_slist_prepend (track->contents.data.grafts, retval);
	return TRUE;

error:
	brasero_graft_point_free (retval);
	return FALSE;
}

static BraseroDiscTrack *
_read_data_track (xmlDocPtr project,
		  xmlNodePtr item)
{
	BraseroDiscTrack *track;

	track = g_new0 (BraseroDiscTrack, 1);
	track->type = BRASERO_PROJECT_TYPE_DATA;

	while (item) {
		if (!xmlStrcmp (item->name, (const xmlChar *) "graft")) {
			if (!_read_graft_point (project, item->xmlChildrenNode, track))
				goto error;
		}
		else if (!xmlStrcmp (item->name, (const xmlChar *) "restored")) {
			xmlChar *restored;

			restored = xmlNodeListGetString (project,
							 item->xmlChildrenNode,
							 1);
			if (!restored)
				goto error;

			track->contents.data.restored = g_slist_prepend (track->contents.data.restored, restored);
		}
		else if (!xmlStrcmp (item->name, (const xmlChar *) "excluded")) {
			xmlChar *excluded;

			excluded = xmlNodeListGetString (project,
							 item->xmlChildrenNode,
							 1);
			if (!excluded)
				goto error;

			track->contents.data.excluded = g_slist_prepend (track->contents.data.excluded,
									 xmlURIUnescapeString ((char*) excluded, 0, NULL));
			g_free (excluded);
		}
		else if (item->type == XML_ELEMENT_NODE)
			goto error;

		item = item->next;
	}

	track->contents.data.excluded = g_slist_reverse (track->contents.data.excluded);
	track->contents.data.grafts = g_slist_reverse (track->contents.data.grafts);
	return track;

error:
	brasero_track_free (track);
	return NULL;
}

static BraseroDiscTrack *
_read_audio_track (xmlDocPtr project,
		   xmlNodePtr uris)
{
	BraseroDiscTrack *track;
	BraseroDiscSong *song;

	track = g_new0 (BraseroDiscTrack, 1);
	song = NULL;

	while (uris) {
		if (!xmlStrcmp (uris->name, (const xmlChar *) "uri")) {
			xmlChar *uri;

			uri = xmlNodeListGetString (project,
						    uris->xmlChildrenNode,
						    1);
			if (!uri)
				goto error;

			song = g_new0 (BraseroDiscSong, 1);
			song->uri = g_uri_unescape_string ((char *) uri, NULL);

			/* to know if this info was set or not */
			song->start = -1;
			song->end = -1;
			g_free (uri);
			track->contents.tracks = g_slist_prepend (track->contents.tracks, song);
		}
		else if (!xmlStrcmp (uris->name, (const xmlChar *) "silence")) {
			gchar *silence;

			if (!song)
				goto error;

			/* impossible to have two gaps in a row */
			if (song->gap)
				goto error;

			silence = (gchar *) xmlNodeListGetString (project,
								  uris->xmlChildrenNode,
								  1);
			if (!silence)
				goto error;

			song->gap = (gint64) g_ascii_strtoull (silence, NULL, 10);
			g_free (silence);
		}
		else if (!xmlStrcmp (uris->name, (const xmlChar *) "start")) {
			gchar *start;

			if (!song)
				goto error;

			start = (gchar *) xmlNodeListGetString (project,
								uris->xmlChildrenNode,
								1);
			if (!start)
				goto error;

			song->start = (gint64) g_ascii_strtoull (start, NULL, 10);
			g_free (start);
		}
		else if (!xmlStrcmp (uris->name, (const xmlChar *) "end")) {
			gchar *end;

			if (!song)
				goto error;

			end = (gchar *) xmlNodeListGetString (project,
							      uris->xmlChildrenNode,
							      1);
			if (!end)
				goto error;

			song->end = (gint64) g_ascii_strtoull (end, NULL, 10);
			g_free (end);
		}
		else if (!xmlStrcmp (uris->name, (const xmlChar *) "title")) {
			xmlChar *title;

			title = xmlNodeListGetString (project,
						      uris->xmlChildrenNode,
						      1);
			if (!title)
				goto error;

			if (!song->info)
				song->info = g_new0 (BraseroStreamInfo, 1);

			if (song->info->title)
				g_free (song->info->title);

			song->info->title = g_uri_unescape_string ((char *) title, NULL);
			g_free (title);
		}
		else if (!xmlStrcmp (uris->name, (const xmlChar *) "artist")) {
			xmlChar *artist;

			artist = xmlNodeListGetString (project,
						      uris->xmlChildrenNode,
						      1);
			if (!artist)
				goto error;

			if (!song->info)
				song->info = g_new0 (BraseroStreamInfo, 1);

			if (song->info->artist)
				g_free (song->info->artist);

			song->info->artist = g_uri_unescape_string ((char *) artist, NULL);
			g_free (artist);
		}
		else if (!xmlStrcmp (uris->name, (const xmlChar *) "composer")) {
			xmlChar *composer;

			composer = xmlNodeListGetString (project,
							 uris->xmlChildrenNode,
							 1);
			if (!composer)
				goto error;

			if (!song->info)
				song->info = g_new0 (BraseroStreamInfo, 1);

			if (song->info->composer)
				g_free (song->info->composer);

			song->info->composer = g_uri_unescape_string ((char *) composer, NULL);
			g_free (composer);
		}
		else if (!xmlStrcmp (uris->name, (const xmlChar *) "isrc")) {
			gchar *isrc;

			isrc = (gchar *) xmlNodeListGetString (project,
							       uris->xmlChildrenNode,
							       1);
			if (!isrc)
				goto error;

			if (!song->info)
				song->info = g_new0 (BraseroStreamInfo, 1);

			song->info->isrc = (gint) g_ascii_strtod (isrc, NULL);
			g_free (isrc);
		}
		else if (uris->type == XML_ELEMENT_NODE)
			goto error;

		uris = uris->next;
	}

	track->contents.tracks = g_slist_reverse (track->contents.tracks);
	return (BraseroDiscTrack*) track;

error:
	brasero_track_free ((BraseroDiscTrack *) track);
	return NULL;
}

static gboolean
_get_tracks (xmlDocPtr project,
	     xmlNodePtr track_node,
	     BraseroDiscTrack **track)
{
	BraseroDiscTrack *newtrack;

	track_node = track_node->xmlChildrenNode;

	newtrack = NULL;
	while (track_node) {
		if (!xmlStrcmp (track_node->name, (const xmlChar *) "audio")) {
			if (newtrack)
				goto error;

			newtrack = _read_audio_track (project,
						      track_node->xmlChildrenNode);
			if (!newtrack)
				goto error;

			newtrack->type = BRASERO_PROJECT_TYPE_AUDIO;
		}
		else if (!xmlStrcmp (track_node->name, (const xmlChar *) "data")) {
			if (newtrack)
				goto error;

			newtrack = _read_data_track (project,
						     track_node->xmlChildrenNode);

			if (!newtrack)
				goto error;
		}
		else if (!xmlStrcmp (track_node->name, (const xmlChar *) "video")) {
			if (newtrack)
				goto error;

			newtrack = _read_audio_track (project,
						      track_node->xmlChildrenNode);

			if (!newtrack)
				goto error;

			newtrack->type = BRASERO_PROJECT_TYPE_VIDEO;
		}
		else if (track_node->type == XML_ELEMENT_NODE)
			goto error;

		track_node = track_node->next;
	}

	if (!newtrack)
		goto error;

	*track = newtrack;
	return TRUE;

error :
	if (newtrack)
		brasero_track_free (newtrack);

	brasero_track_free (newtrack);
	return FALSE;
}

gboolean
brasero_project_open_project_xml (const gchar *uri,
				  BraseroDiscTrack **track,
				  gboolean warn_user)
{
	xmlNodePtr track_node = NULL;
	gchar *label = NULL;
	gchar *cover = NULL;
	xmlDocPtr project;
	xmlNodePtr item;
	gboolean retval;
	gchar *path;

	path = g_filename_from_uri (uri, NULL, NULL);
    	if (!path)
		return FALSE;

	/* start parsing xml doc */
	project = xmlParseFile (path);
    	g_free (path);

	if (!project) {
	    	if (warn_user)
			brasero_project_invalid_project_dialog (_("The project could not be opened."));

		return FALSE;
	}

	/* parses the "header" */
	item = xmlDocGetRootElement (project);
	if (!item) {
	    	if (warn_user)
			brasero_project_invalid_project_dialog (_("The file is empty."));

		xmlFreeDoc (project);
		return FALSE;
	}

	if (xmlStrcmp (item->name, (const xmlChar *) "braseroproject")
	||  item->next)
		goto error;

	item = item->children;
	while (item) {
		if (!xmlStrcmp (item->name, (const xmlChar *) "version")) {
			/* simply ignore it */
		}
		else if (!xmlStrcmp (item->name, (const xmlChar *) "label")) {
			label = (gchar *) xmlNodeListGetString (project,
								item->xmlChildrenNode,
								1);
			if (!(label))
				goto error;
		}
		else if (!xmlStrcmp (item->name, (const xmlChar *) "cover")) {
			xmlChar *escaped;

			escaped = xmlNodeListGetString (project,
							item->xmlChildrenNode,
							1);
			if (!escaped)
				goto error;

			cover = g_uri_unescape_string ((char *) escaped, NULL);
			g_free (escaped);
		}
		else if (!xmlStrcmp (item->name, (const xmlChar *) "track")) {
			if (track_node)
				goto error;

			track_node = item;
		}
		else if (item->type == XML_ELEMENT_NODE)
			goto error;

		item = item->next;
	}

	retval = _get_tracks (project, track_node, track);
	if (!retval)
		goto error;

	xmlFreeDoc (project);

	if (track && *track) {
		(*track)->label = label;
		(*track)->cover = cover;
	}

	return retval;

error:

	if (cover)
		g_free (cover);
	if (label)
		g_free (label);

	xmlFreeDoc (project);
    	if (warn_user)
		brasero_project_invalid_project_dialog (_("It does not seem to be a valid Brasero project."));

	return FALSE;
}

#ifdef BUILD_PLAYLIST

static void
brasero_project_playlist_playlist_started (TotemPlParser *parser,
					   const gchar *uri,
					   GHashTable *metadata,
					   gpointer user_data)
{
	gchar *string;
	gchar **retval = user_data;

	string = g_hash_table_lookup (metadata, TOTEM_PL_PARSER_FIELD_TITLE);
	if (string)
		*retval = g_strdup (string);
}

static void
brasero_project_playlist_entry_parsed (TotemPlParser *parser,
				       const gchar *uri,
				       GHashTable *metadata,
				       gpointer user_data)
{
	BraseroDiscTrack *track = user_data;
	BraseroDiscSong *song;

	song = g_new0 (BraseroDiscSong, 1);
	song->uri = g_strdup (uri);

	/* to know if this info was set or not */
	song->start = -1;
	song->end = -1;
	track->contents.tracks = g_slist_prepend (track->contents.tracks, song);
}

gboolean
brasero_project_open_audio_playlist_project (const gchar *uri,
					     BraseroDiscTrack **track,
					     gboolean warn_user)
{
	gchar *label = NULL;
	TotemPlParser *parser;
	TotemPlParserResult result;
	BraseroDiscTrack *new_track;

	new_track = g_new0 (BraseroDiscTrack, 1);
	new_track->type = BRASERO_PROJECT_TYPE_AUDIO;

	parser = totem_pl_parser_new ();
	g_object_set (parser,
		      "recurse", FALSE,
		      "disable-unsafe", TRUE,
		      NULL);

	g_signal_connect (parser,
			  "playlist-started",
			  G_CALLBACK (brasero_project_playlist_playlist_started),
			  &label);

	g_signal_connect (parser,
			  "entry-parsed",
			  G_CALLBACK (brasero_project_playlist_entry_parsed),
			  new_track);

	result = totem_pl_parser_parse (parser, uri, FALSE);
	if (result != TOTEM_PL_PARSER_RESULT_SUCCESS) {
		if (warn_user)
			brasero_project_invalid_project_dialog (_("It does not seem to be a valid Brasero project."));

		brasero_track_free (new_track);
	}
	else {
		if (new_track && label)
			new_track->label = label;

		*track = new_track;
	}

	g_object_unref (parser);

	return (result == TOTEM_PL_PARSER_RESULT_SUCCESS);
}

#endif
