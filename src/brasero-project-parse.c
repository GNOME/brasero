/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

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

#include "brasero-units.h"
#include "brasero-track-stream-cfg.h"
#include "brasero-track-data-cfg.h"
#include "brasero-session.h"
#include "brasero-tags.h"

#define BRASERO_PROJECT_VERSION "0.2"

static void
brasero_project_invalid_project_dialog (const char *reason)
{
	brasero_app_alert (brasero_app_get_default (),
			   _("Error while loading the project."),
			   reason,
			   GTK_MESSAGE_ERROR);
}

static GSList *
_read_graft_point (xmlDocPtr project,
		   xmlNodePtr graft,
		   GSList *grafts)
{
	BraseroGraftPt *retval;

	retval = g_new0 (BraseroGraftPt, 1);
        grafts = g_slist_prepend (grafts, retval);
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
		else if (graft->type == XML_ELEMENT_NODE)
			goto error;

		graft = graft->next;
	}

	return grafts;

error:

        g_slist_foreach (grafts, (GFunc) brasero_graft_point_free, NULL);
        g_slist_free (grafts);

	return NULL;
}

static BraseroTrack *
_read_data_track (xmlDocPtr project,
		  xmlNodePtr item)
{
	BraseroTrackDataCfg *track;
        GSList *grafts= NULL;
        GSList *excluded = NULL;

	track = brasero_track_data_cfg_new ();

	while (item) {
		if (!xmlStrcmp (item->name, (const xmlChar *) "graft")) {
			if (!(grafts = _read_graft_point (project, item->xmlChildrenNode, grafts)))
				goto error;
		}
		else if (!xmlStrcmp (item->name, (const xmlChar *) "icon")) {
			xmlChar *icon_path;

			icon_path = xmlNodeListGetString (project,
							  item->xmlChildrenNode,
							  1);
			if (!icon_path)
				goto error;

			brasero_track_data_cfg_set_icon (track, (gchar *) icon_path, NULL);
                        g_free (icon_path);
		}
		else if (!xmlStrcmp (item->name, (const xmlChar *) "restored")) {
			xmlChar *restored;

			restored = xmlNodeListGetString (project,
							 item->xmlChildrenNode,
							 1);
			if (!restored)
				goto error;

                        brasero_track_data_cfg_dont_filter_uri (track, (gchar *) restored);
                        g_free (restored);
		}
		else if (!xmlStrcmp (item->name, (const xmlChar *) "excluded")) {
			xmlChar *excluded_uri;

			excluded_uri = xmlNodeListGetString (project,
							     item->xmlChildrenNode,
							     1);
			if (!excluded_uri)
				goto error;

			excluded = g_slist_prepend (excluded, xmlURIUnescapeString ((char*) excluded_uri, 0, NULL));
			g_free (excluded_uri);
		}
		else if (item->type == XML_ELEMENT_NODE)
			goto error;

		item = item->next;
	}

        grafts = g_slist_reverse (grafts);
        excluded = g_slist_reverse (excluded);
        brasero_track_data_set_source (BRASERO_TRACK_DATA (track),
                                                           grafts,
                                                           excluded);
	return BRASERO_TRACK (track);

error:

        g_slist_foreach (grafts, (GFunc) brasero_graft_point_free, NULL);
        g_slist_free (grafts);

        g_slist_foreach (excluded, (GFunc) g_free, NULL);
        g_slist_free (excluded);

	g_object_unref (track);

	return NULL;
}

static BraseroTrack *
_read_audio_track (xmlDocPtr project,
		   xmlNodePtr uris,
                   gboolean is_video)
{
	BraseroTrackStreamCfg *track;

	track = brasero_track_stream_cfg_new ();

	while (uris) {
		if (!xmlStrcmp (uris->name, (const xmlChar *) "uri")) {
			xmlChar *uri;
                        gchar *unescaped_uri;

			uri = xmlNodeListGetString (project,
						    uris->xmlChildrenNode,
						    1);
			if (!uri)
				goto error;

                        unescaped_uri = g_uri_unescape_string ((char *) uri, NULL);
                        g_free (uri);

			/* Note: this must come before brasero_track_stream_set_boundaries ()
			 * or we will reset the end point to 0 */
			brasero_track_stream_set_source (BRASERO_TRACK_STREAM (track), unescaped_uri);

			/* For the moment pretend it is a video file. Since it is BraseroTrackStreamCfg, that
			 * will be set properly afterwards. */
			if (is_video)
				brasero_track_stream_set_format (BRASERO_TRACK_STREAM (track),
				                                 BRASERO_VIDEO_FORMAT_UNDEFINED);

                        g_free (unescaped_uri);
		}
		else if (!xmlStrcmp (uris->name, (const xmlChar *) "silence")) {
			gchar *silence;

			/* impossible to have two gaps in a row */
			if (brasero_track_stream_get_gap (BRASERO_TRACK_STREAM (track)) > 0)
				goto error;

			silence = (gchar *) xmlNodeListGetString (project,
								  uris->xmlChildrenNode,
								  1);
			if (!silence)
				goto error;

                        brasero_track_stream_set_boundaries (BRASERO_TRACK_STREAM (track),
                                                             -1,
                                                             -1,
                                                             g_ascii_strtoull (silence, NULL, 10));
			g_free (silence);
		}
		else if (!xmlStrcmp (uris->name, (const xmlChar *) "start")) {
			gchar *start;

			start = (gchar *) xmlNodeListGetString (project,
								uris->xmlChildrenNode,
								1);
			if (!start)
				goto error;

                        brasero_track_stream_set_boundaries (BRASERO_TRACK_STREAM (track),
                                                             g_ascii_strtoull (start, NULL, 10),
                                                             -1,
                                                             -1);
			g_free (start);
		}
		else if (!xmlStrcmp (uris->name, (const xmlChar *) "end")) {
			gchar *end;

			end = (gchar *) xmlNodeListGetString (project,
							      uris->xmlChildrenNode,
							      1);
			if (!end)
				goto error;

                        brasero_track_stream_set_boundaries (BRASERO_TRACK_STREAM (track),
                                                             -1,
                                                             g_ascii_strtoull (end, NULL, 10),
                                                             -1);
			g_free (end);
		}
		else if (!xmlStrcmp (uris->name, (const xmlChar *) "title")) {
			xmlChar *title;
			gchar *unescaped_title;

			title = xmlNodeListGetString (project,
						      uris->xmlChildrenNode,
						      1);
			if (!title)
				goto error;

                        unescaped_title = g_uri_unescape_string ((char *) title, NULL);
                        g_free (title);

                        brasero_track_tag_add_string (BRASERO_TRACK (track),
                                                      BRASERO_TRACK_STREAM_TITLE_TAG,
                                                      unescaped_title);
        		g_free (unescaped_title);
		}
		else if (!xmlStrcmp (uris->name, (const xmlChar *) "artist")) {
			xmlChar *artist;
                        gchar *unescaped_artist;

			artist = xmlNodeListGetString (project,
						      uris->xmlChildrenNode,
						      1);
			if (!artist)
				goto error;

			unescaped_artist = g_uri_unescape_string ((char *) artist, NULL);
			g_free (artist);

                        brasero_track_tag_add_string (BRASERO_TRACK (track),
                                                      BRASERO_TRACK_STREAM_ARTIST_TAG,
                                                      unescaped_artist);
        		g_free (unescaped_artist);
		}
		else if (!xmlStrcmp (uris->name, (const xmlChar *) "composer")) {
			xmlChar *composer;
                        gchar *unescaped_composer;

			composer = xmlNodeListGetString (project,
							 uris->xmlChildrenNode,
							 1);
			if (!composer)
				goto error;

			unescaped_composer = g_uri_unescape_string ((char *) composer, NULL);
			g_free (composer);

                        brasero_track_tag_add_string (BRASERO_TRACK (track),
                                                      BRASERO_TRACK_STREAM_COMPOSER_TAG,
                                                      unescaped_composer);
        		g_free (unescaped_composer);
		}
		else if (!xmlStrcmp (uris->name, (const xmlChar *) "isrc")) {
			xmlChar *isrc;
                        gchar *unescaped_isrc;

			isrc = xmlNodeListGetString (project,
						     uris->xmlChildrenNode,
						     1);
			if (!isrc)
				goto error;

			unescaped_isrc = g_uri_unescape_string ((char *) isrc, NULL);
			g_free (isrc);

                        brasero_track_tag_add_string (BRASERO_TRACK (track),
                                                      BRASERO_TRACK_STREAM_ISRC_TAG,
                                                      unescaped_isrc);
        		g_free (unescaped_isrc);
		}
		else if (uris->type == XML_ELEMENT_NODE)
			goto error;

		uris = uris->next;
	}

	return BRASERO_TRACK (track);

error:

	g_object_unref (track);

	return NULL;
}

static gboolean
_get_tracks (xmlDocPtr project,
	     xmlNodePtr track_node,
	     BraseroBurnSession *session)
{
	GSList *tracks = NULL;
	GSList *iter;

	track_node = track_node->xmlChildrenNode;

	while (track_node) {
		BraseroTrack *newtrack;

		if (!xmlStrcmp (track_node->name, (const xmlChar *) "audio")) {
			newtrack = _read_audio_track (project, track_node->xmlChildrenNode, FALSE);
			if (!newtrack)
				goto error;

			tracks = g_slist_append (tracks, newtrack);
		}
		else if (!xmlStrcmp (track_node->name, (const xmlChar *) "data")) {
			newtrack = _read_data_track (project, track_node->xmlChildrenNode);

			if (!newtrack)
				goto error;

			tracks = g_slist_append (tracks, newtrack);
		}
		else if (!xmlStrcmp (track_node->name, (const xmlChar *) "video")) {
			newtrack = _read_audio_track (project, track_node->xmlChildrenNode, TRUE);

			if (!newtrack)
				goto error;

			tracks = g_slist_append (tracks, newtrack);
		}
		else if (track_node->type == XML_ELEMENT_NODE)
			goto error;

		track_node = track_node->next;
	}

	if (!tracks)
		goto error;

	for (iter = tracks; iter; iter = iter->next) {
		BraseroTrack *newtrack;

		newtrack = iter->data;
		brasero_burn_session_add_track (session, newtrack, NULL);
		g_object_unref (newtrack);
	}

	g_slist_free (tracks);

	return TRUE;

error :

	if (tracks) {
		g_slist_foreach (tracks, (GFunc) g_object_unref, NULL);
		g_slist_free (tracks);
	}

	return FALSE;
}

gboolean
brasero_project_open_project_xml (const gchar *uri,
				  BraseroBurnSession *session,
				  gboolean warn_user)
{
	xmlNodePtr track_node = NULL;
	gchar *label = NULL;
	gchar *cover = NULL;
	xmlDocPtr project;
	xmlNodePtr item;
	gboolean retval;
	GFile *file;
	gchar *path;

	file = g_file_new_for_commandline_arg (uri);
	path = g_file_get_path (file);
	g_object_unref (file);
	if (!path)
		return FALSE;

	/* start parsing xml doc */
	project = xmlParseFile (path);
    	g_free (path);

	if (!project) {
	    	if (warn_user)
			brasero_project_invalid_project_dialog (_("The project could not be opened"));

		return FALSE;
	}

	/* parses the "header" */
	item = xmlDocGetRootElement (project);
	if (!item) {
	    	if (warn_user)
			brasero_project_invalid_project_dialog (_("The file is empty"));

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

	retval = _get_tracks (project, track_node, session);
	if (!retval)
		goto error;

	xmlFreeDoc (project);

        brasero_burn_session_set_label (session, label);
        g_free (label);

        if (cover) {
                GValue *value;

                value = g_new0 (GValue, 1);
                g_value_init (value, G_TYPE_STRING);
                g_value_set_string (value, cover);
                brasero_burn_session_tag_add (session,
                                               BRASERO_COVER_URI,
                                               value);

                g_free (cover);
        }

        return retval;

error:

	if (cover)
		g_free (cover);
	if (label)
		g_free (label);

	xmlFreeDoc (project);
    	if (warn_user)
		brasero_project_invalid_project_dialog (_("It does not seem to be a valid Brasero project"));

	return FALSE;
}

#ifdef BUILD_PLAYLIST

static void
brasero_project_playlist_playlist_started (TotemPlParser *parser,
					   const gchar *uri,
					   GHashTable *metadata,
					   gpointer user_data)
{
        BraseroBurnSession *session = user_data;

        brasero_burn_session_set_label (session, g_hash_table_lookup (metadata, TOTEM_PL_PARSER_FIELD_TITLE));
}

static void
brasero_project_playlist_entry_parsed (TotemPlParser *parser,
				       const gchar *uri,
				       GHashTable *metadata,
				       gpointer user_data)
{
	BraseroBurnSession *session = user_data;
        BraseroTrackStreamCfg *track;

        track = brasero_track_stream_cfg_new ();
        brasero_track_stream_set_source (BRASERO_TRACK_STREAM (track), uri);
        brasero_burn_session_add_track (session, BRASERO_TRACK (track), NULL);
}

gboolean
brasero_project_open_audio_playlist_project (const gchar *uri,
					     BraseroBurnSession *session,
					     gboolean warn_user)
{
	TotemPlParser *parser;
	TotemPlParserResult result;
	GFile *file;
	char *_uri;

	file = g_file_new_for_commandline_arg (uri);
	_uri = g_file_get_uri (file);
	g_object_unref (file);

	parser = totem_pl_parser_new ();
	g_object_set (parser,
		      "recurse", FALSE,
		      "disable-unsafe", TRUE,
		      NULL);

	g_signal_connect (parser,
			  "playlist-started",
			  G_CALLBACK (brasero_project_playlist_playlist_started),
			  session);

	g_signal_connect (parser,
			  "entry-parsed",
			  G_CALLBACK (brasero_project_playlist_entry_parsed),
			  session);

	result = totem_pl_parser_parse (parser, _uri, FALSE);
	if (result != TOTEM_PL_PARSER_RESULT_SUCCESS) {
		if (warn_user)
			brasero_project_invalid_project_dialog (_("It does not seem to be a valid Brasero project"));
	}

	g_free (_uri);
	g_object_unref (parser);

	return (result == TOTEM_PL_PARSER_RESULT_SUCCESS);
}

#endif

/**
 * Project saving
 */

static gboolean
_save_audio_track_xml (xmlTextWriter *project,
		       BraseroTrackStream *track)
{
	xmlChar *escaped;
	gchar *start;
	gint success;
	gchar *uri;
	gchar *end;

	uri = brasero_track_stream_get_source (track, TRUE);
	escaped = (unsigned char *) g_uri_escape_string (uri, NULL, FALSE);
	g_free (uri);

	success = xmlTextWriterWriteElement (project,
					    (xmlChar *) "uri",
					     escaped);
	g_free (escaped);

	if (success == -1)
		return FALSE;

	if (brasero_track_stream_get_gap (track) > 0) {
		gchar *silence;

		silence = g_strdup_printf ("%"G_GINT64_FORMAT, brasero_track_stream_get_gap (track));
		success = xmlTextWriterWriteElement (project,
						     (xmlChar *) "silence",
						     (xmlChar *) silence);

		g_free (silence);
		if (success == -1)
			return FALSE;
	}

	if (brasero_track_stream_get_end (track) > 0) {
		/* start of the song */
		start = g_strdup_printf ("%"G_GINT64_FORMAT, brasero_track_stream_get_start (track));
		success = xmlTextWriterWriteElement (project,
						     (xmlChar *) "start",
						     (xmlChar *) start);

		g_free (start);
		if (success == -1)
			return FALSE;

		/* end of the song */
		end = g_strdup_printf ("%"G_GINT64_FORMAT, brasero_track_stream_get_end (track));
		success = xmlTextWriterWriteElement (project,
						     (xmlChar *) "end",
						     (xmlChar *) end);

		g_free (end);
		if (success == -1)
			return FALSE;
	}

	if (brasero_track_tag_lookup_string (BRASERO_TRACK (track), BRASERO_TRACK_STREAM_TITLE_TAG)) {
		escaped = (unsigned char *) g_uri_escape_string (brasero_track_tag_lookup_string (BRASERO_TRACK (track), BRASERO_TRACK_STREAM_TITLE_TAG), NULL, FALSE);
		success = xmlTextWriterWriteElement (project,
						    (xmlChar *) "title",
						     escaped);
		g_free (escaped);

		if (success == -1)
			return FALSE;
	}

	if (brasero_track_tag_lookup_string (BRASERO_TRACK (track), BRASERO_TRACK_STREAM_ARTIST_TAG)) {
		escaped = (unsigned char *) g_uri_escape_string (brasero_track_tag_lookup_string (BRASERO_TRACK (track), BRASERO_TRACK_STREAM_ARTIST_TAG), NULL, FALSE);
		success = xmlTextWriterWriteElement (project,
						    (xmlChar *) "artist",
						     escaped);
		g_free (escaped);

		if (success == -1)
			return FALSE;
	}

	if (brasero_track_tag_lookup_string (BRASERO_TRACK (track), BRASERO_TRACK_STREAM_COMPOSER_TAG)) {
		escaped = (unsigned char *) g_uri_escape_string (brasero_track_tag_lookup_string (BRASERO_TRACK (track), BRASERO_TRACK_STREAM_COMPOSER_TAG), NULL, FALSE);
		success = xmlTextWriterWriteElement (project,
						    (xmlChar *) "composer",
						     escaped);
		g_free (escaped);
		if (success == -1)
			return FALSE;
	}

	if (brasero_track_tag_lookup_string (BRASERO_TRACK (track), BRASERO_TRACK_STREAM_ISRC_TAG)) {
		escaped = (unsigned char *) g_uri_escape_string (brasero_track_tag_lookup_string (BRASERO_TRACK (track), BRASERO_TRACK_STREAM_ISRC_TAG), NULL, FALSE);
		success = xmlTextWriterWriteElement (project,
						     (xmlChar *) "isrc",
						     escaped);

		g_free (escaped);
		if (success == -1)
			return FALSE;
	}

	return TRUE;
}

static gboolean
_save_data_track_xml (xmlTextWriter *project,
		      BraseroBurnSession *session)
{
	gchar *uri;
	gint success;
	GSList *iter;
	GSList *tracks;
	GSList *grafts;
	gchar *filename;
	BraseroTrackDataCfg *track;

	tracks = brasero_burn_session_get_tracks (session);
	track = BRASERO_TRACK_DATA_CFG (tracks->data);

	filename = brasero_track_data_cfg_get_icon_path (track);
	if (filename) {
		/* Write the icon if any */
		success = xmlTextWriterWriteElement (project, (xmlChar *) "icon", (xmlChar *) filename);
		g_free (filename);
		if (success < 0)
			return FALSE;
	}

	grafts = brasero_track_data_get_grafts (BRASERO_TRACK_DATA (track));
	for (; grafts; grafts = grafts->next) {
		BraseroGraftPt *graft;

		graft = grafts->data;

		success = xmlTextWriterStartElement (project, (xmlChar *) "graft");
		if (success < 0)
			return FALSE;

		success = xmlTextWriterWriteElement (project, (xmlChar *) "path", (xmlChar *) graft->path);
		if (success < 0)
			return FALSE;

		if (graft->uri) {
			xmlChar *escaped;

			escaped = (unsigned char *) g_uri_escape_string (graft->uri, NULL, FALSE);
			success = xmlTextWriterWriteElement (project, (xmlChar *) "uri", escaped);
			g_free (escaped);
			if (success < 0)
				return FALSE;
		}

		success = xmlTextWriterEndElement (project); /* graft */
		if (success < 0)
			return FALSE;
	}

	/* save excluded uris */
	iter = brasero_track_data_get_excluded_list (BRASERO_TRACK_DATA (track));
	for (; iter; iter = iter->next) {
		xmlChar *escaped;

		escaped = xmlURIEscapeStr ((xmlChar *) iter->data, NULL);
		success = xmlTextWriterWriteElement (project, (xmlChar *) "excluded", (xmlChar *) escaped);
		g_free (escaped);
		if (success < 0)
			return FALSE;
	}

	/* save restored uris */
	iter = brasero_track_data_cfg_get_restored_list (track);
	for (; iter; iter = iter->next) {
		uri = iter->data;
		success = xmlTextWriterWriteElement (project, (xmlChar *) "restored", (xmlChar *) uri);
		if (success < 0)
			return FALSE;
	}

	/* NOTE: we don't write symlinks and unreadable they are useless */
	return TRUE;
}

gboolean 
brasero_project_save_project_xml (BraseroBurnSession *session,
				  const gchar *uri)
{
	BraseroTrackType *track_type = NULL;
	xmlTextWriter *project;
	gboolean retval;
	GSList *tracks;
	GValue *value;
	gint success;
	gchar *path;

	path = g_filename_from_uri (uri, NULL, NULL);
	if (!path)
		return FALSE;

	project = xmlNewTextWriterFilename (path, 0);
	if (!project) {
		g_free (path);
		return FALSE;
	}

	xmlTextWriterSetIndent (project, 1);
	xmlTextWriterSetIndentString (project, (xmlChar *) "\t");

	success = xmlTextWriterStartDocument (project,
					      NULL,
					      "UTF-8",
					      NULL);
	if (success < 0)
		goto error;

	success = xmlTextWriterStartElement (project, (xmlChar *) "braseroproject");
	if (success < 0)
		goto error;

	/* write the name of the version */
	success = xmlTextWriterWriteElement (project,
					     (xmlChar *) "version",
					     (xmlChar *) BRASERO_PROJECT_VERSION);
	if (success < 0)
		goto error;

	if (brasero_burn_session_get_label (session)) {
		success = xmlTextWriterWriteElement (project,
						     (xmlChar *) "label",
						     (xmlChar *) brasero_burn_session_get_label (session));

		if (success < 0)
			goto error;
	}

	value = NULL;
	brasero_burn_session_tag_lookup (session,
					 BRASERO_COVER_URI,
					 &value);
	if (value) {
		gchar *escaped;

		escaped = g_uri_escape_string (g_value_get_string (value), NULL, FALSE);
		success = xmlTextWriterWriteElement (project,
						     (xmlChar *) "cover",
						     (xmlChar *) escaped);
		g_free (escaped);

		if (success < 0)
			goto error;
	}

	success = xmlTextWriterStartElement (project, (xmlChar *) "track");
	if (success < 0)
		goto error;

	track_type = brasero_track_type_new ();
	tracks = brasero_burn_session_get_tracks (session);

	for (; tracks; tracks = tracks->next) {
		BraseroTrack *track;

		track = tracks->data;

		brasero_track_get_track_type (track, track_type);
		if (brasero_track_type_get_has_stream (track_type)) {
			if (BRASERO_STREAM_FORMAT_HAS_VIDEO (brasero_track_type_get_stream_format (track_type)))
				success = xmlTextWriterStartElement (project, (xmlChar *) "video");
			else
				success = xmlTextWriterStartElement (project, (xmlChar *) "audio");

			if (success < 0)
				goto error;

			retval = _save_audio_track_xml (project, BRASERO_TRACK_STREAM (track));
			if (!retval)
				goto error;

			success = xmlTextWriterEndElement (project); /* audio/video */
			if (success < 0)
				goto error;
		}
		else if (brasero_track_type_get_has_data (track_type)) {
			success = xmlTextWriterStartElement (project, (xmlChar *) "data");
			if (success < 0)
				goto error;

			retval = _save_data_track_xml (project, session);
			if (!retval)
				goto error;

			success = xmlTextWriterEndElement (project); /* data */
			if (success < 0)
				goto error;
		}
		else
			retval = FALSE;
	}

	success = xmlTextWriterEndElement (project); /* track */
	if (success < 0)
		goto error;

	brasero_track_type_free (track_type);

	success = xmlTextWriterEndElement (project); /* braseroproject */
	if (success < 0)
		goto error;

	xmlTextWriterEndDocument (project);
	xmlFreeTextWriter (project);
	g_free (path);
	return TRUE;

error:

	if (track_type)
		brasero_track_type_free (track_type);

	xmlTextWriterEndDocument (project);
	xmlFreeTextWriter (project);

	g_remove (path);
	g_free (path);

	return FALSE;
}

gboolean
brasero_project_save_audio_project_plain_text (BraseroBurnSession *session,
					       const gchar *uri)
{
	const gchar *title;
	guint written;
	GSList *iter;
	gchar *path;
	FILE *file;

    	path = g_filename_from_uri (uri, NULL, NULL);
    	if (!path)
		return FALSE;

	file = fopen (path, "w+");
	g_free (path);
	if (!file)
		return FALSE;

	/* write title */
	title = brasero_burn_session_get_label (session);
	written = fwrite (title, strlen (title), 1, file);
	if (written != 1)
		goto error;

	written = fwrite ("\n", 1, 1, file);
	if (written != 1)
		goto error;

	iter = brasero_burn_session_get_tracks (session);
	for (; iter; iter = iter->next) {
		BraseroTrackStream *track;
		const gchar *text;
		gchar *time;
		guint64 len;
		gchar *uri;

		track = iter->data;

		text = brasero_track_tag_lookup_string (BRASERO_TRACK (track), BRASERO_TRACK_STREAM_TITLE_TAG);
		written = fwrite (title, 1, strlen (title), file);
		if (written != strlen (title))
			goto error;

		len = 0;
		brasero_track_stream_get_length (track, &len);
		time = brasero_units_get_time_string (len, TRUE, FALSE);
		if (time) {
			written = fwrite ("\t", 1, 1, file);
			if (written != 1)
				goto error;

			written = fwrite (time, 1, strlen (time), file);
			if (written != strlen (time)) {
				g_free (time);
				goto error;
			}
			g_free (time);
		}

		text = brasero_track_tag_lookup_string (BRASERO_TRACK (track), BRASERO_TRACK_STREAM_ARTIST_TAG);
		if (text) {
			gchar *string;

			written = fwrite ("\t", 1, 1, file);
			if (written != 1)
				goto error;

			/* Translators: %s is an artist */
			string = g_strdup_printf (" by %s", text);
			written = fwrite (string, 1, strlen (string), file);
			if (written != strlen (string)) {
				g_free (string);
				goto error;
			}
			g_free (string);
		}

		written = fwrite ("\n(", 1, 2, file);
		if (written != 2)
			goto error;

		uri = brasero_track_stream_get_source (track, TRUE);
		written = fwrite (uri, 1, strlen (uri), file);
		if (written != strlen (uri)) {
			g_free (uri);
			goto error;
		}

		g_free (uri);

		written = fwrite (")", 1, 1, file);
		if (written != 1)
			goto error;

		written = fwrite ("\n\n", 1, 2, file);
		if (written != 2)
			goto error;
	}

	fclose (file);
	return TRUE;
	
error:

	fclose (file);

	return FALSE;
}

#ifdef BUILD_PLAYLIST

gboolean
brasero_project_save_audio_project_playlist (BraseroBurnSession *session,
					     const gchar *uri,
					     BraseroProjectSave type)
{
	TotemPlParserType pl_type;
	TotemPlParser *parser;
	TotemPlPlaylist *playlist;
	TotemPlPlaylistIter pl_iter;
	gboolean result;
	GFile *file;
	GSList *iter;

	file = g_file_new_for_uri (uri);
	parser = totem_pl_parser_new ();
	playlist = totem_pl_playlist_new ();

	/* populate playlist */
	iter = brasero_burn_session_get_tracks (session);
	for (; iter; iter = iter->next) {
		BraseroTrackStream *track;
		const gchar *title;
		gchar *uri;

		track = iter->data;

		uri = brasero_track_stream_get_source (track, TRUE);
		title = brasero_track_tag_lookup_string (BRASERO_TRACK (track), BRASERO_TRACK_STREAM_TITLE_TAG);

		totem_pl_playlist_append (playlist, &pl_iter);
		totem_pl_playlist_set (playlist, &pl_iter,
				       TOTEM_PL_PARSER_FIELD_URI, uri,
				       TOTEM_PL_PARSER_FIELD_TITLE, title,
				       NULL);
		g_free (uri);
	}

	switch (type) {
		case BRASERO_PROJECT_SAVE_PLAYLIST_M3U:
			pl_type = TOTEM_PL_PARSER_M3U;
			break;
		case BRASERO_PROJECT_SAVE_PLAYLIST_XSPF:
			pl_type = TOTEM_PL_PARSER_XSPF;
			break;
		case BRASERO_PROJECT_SAVE_PLAYLIST_IRIVER_PLA:
			pl_type = TOTEM_PL_PARSER_IRIVER_PLA;
			break;

		case BRASERO_PROJECT_SAVE_PLAYLIST_PLS:
		default:
			pl_type = TOTEM_PL_PARSER_PLS;
			break;
	}

	result = totem_pl_parser_save (parser, playlist, file,
				       brasero_burn_session_get_label (session),
				       pl_type, NULL);

	g_object_unref (playlist);
	g_object_unref (parser);
	g_object_unref (file);

	return result;
}

#endif
