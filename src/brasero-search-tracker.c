/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Rouquier Philippe 2009 <bonfire-app@wanadoo.fr>
 * 
 * brasero is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * brasero is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>

#include <libtracker-sparql/tracker-sparql.h>
#include <gio/gio.h>

#include "brasero-search-tracker.h"
#include "brasero-search-engine.h"

typedef struct _BraseroSearchTrackerPrivate BraseroSearchTrackerPrivate;
struct _BraseroSearchTrackerPrivate
{
	TrackerSparqlConnection *connection;
	GCancellable *cancellable;
	GPtrArray *results;

	BraseroSearchScope scope;

	gchar **mimes;
	gchar *keywords;

	guint current_call_id;
};

#define BRASERO_SEARCH_TRACKER_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_SEARCH_TRACKER, BraseroSearchTrackerPrivate))

static void brasero_search_tracker_init_engine (BraseroSearchEngineIface *iface);

G_DEFINE_TYPE_WITH_CODE (BraseroSearchTracker,
			 brasero_search_tracker,
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (BRASERO_TYPE_SEARCH_ENGINE,
					        brasero_search_tracker_init_engine));

static gboolean
brasero_search_tracker_is_available (BraseroSearchEngine *engine)
{
	BraseroSearchTrackerPrivate *priv;

	priv = BRASERO_SEARCH_TRACKER_PRIVATE (engine);
	return (priv->connection != NULL);
}

static gint
brasero_search_tracker_num_hits (BraseroSearchEngine *engine)
{
	BraseroSearchTrackerPrivate *priv;

	priv = BRASERO_SEARCH_TRACKER_PRIVATE (engine);
	if (!priv->results)
		return 0;

	return priv->results->len;
}

static const gchar *
brasero_search_tracker_uri_from_hit (BraseroSearchEngine *engine,
                                     gpointer hit)
{
	gchar **tracker_hit;

	tracker_hit = hit;

	if (!tracker_hit)
		return NULL;

	if (g_strv_length (tracker_hit) >= 2)
		return tracker_hit [1];

	return NULL;
}

static const gchar *
brasero_search_tracker_mime_from_hit (BraseroSearchEngine *engine,
                                      gpointer hit)
{
	gchar **tracker_hit;

	tracker_hit = hit;

	if (!tracker_hit)
		return NULL;

	if (g_strv_length (tracker_hit) >= 3)
		return tracker_hit [2];

	return NULL;
}

static int
brasero_search_tracker_score_from_hit (BraseroSearchEngine *engine,
                                       gpointer hit)
{
	gchar **tracker_hit;

	tracker_hit = hit;

	if (!tracker_hit)
		return 0;

	if (g_strv_length (tracker_hit) >= 4)
		return (int) strtof (tracker_hit [3], NULL);

	return 0;
}

static void brasero_search_tracker_cursor_callback (GObject      *object,
						    GAsyncResult *result,
						    gpointer      user_data);

static void
brasero_search_tracker_cursor_next (BraseroSearchEngine *search,
				    TrackerSparqlCursor    *cursor)
{
	BraseroSearchTrackerPrivate *priv;
	priv = BRASERO_SEARCH_TRACKER_PRIVATE (search);
	
	tracker_sparql_cursor_next_async (cursor,
					  priv->cancellable,
					  brasero_search_tracker_cursor_callback,
					  search);
}

static void
brasero_search_tracker_cursor_callback (GObject      *object,
					GAsyncResult *result,
					gpointer      user_data)
{
	BraseroSearchEngine *search;
	GError *error = NULL;
	TrackerSparqlCursor *cursor;
	GList *hits;
	gboolean success;

	cursor = TRACKER_SPARQL_CURSOR (object);
	success = tracker_sparql_cursor_next_finish (cursor, result, &error);

	if (error) {
		brasero_search_engine_query_error (search, error);
		g_error_free (error);

		if (cursor) {
			g_object_unref (cursor);
		}

		return;
	}

	if (!success) {
		brasero_search_engine_query_finished (search);

		if (cursor) {
			g_object_unref (cursor);
		}

		return;
	}

	/* We iterate result by result, not n at a time. */
	hits = g_list_append (NULL, (gchar*) tracker_sparql_cursor_get_string (cursor, 0, NULL));
	brasero_search_engine_hit_added (search, hits);
	g_list_free (hits);

	/* Get next */
	brasero_search_tracker_cursor_next (search, cursor);
}

static void
brasero_search_tracker_reply (GObject      *object,
			      GAsyncResult *result,
			      gpointer user_data)
{
	BraseroSearchEngine *search = BRASERO_SEARCH_ENGINE (user_data);
	GError *error = NULL;
	
	TrackerSparqlCursor *cursor;
	GList *hits;
	gboolean success;

	cursor = TRACKER_SPARQL_CURSOR (object);
	success = tracker_sparql_cursor_next_finish (cursor, result, &error);

	if (cursor) {
		g_object_unref (cursor);
	}

	if (error) {
		brasero_search_engine_query_error (search, error);
		return;
	}

	if (!success) {
		brasero_search_engine_query_finished (search);
		
		if (cursor) {
			g_object_unref (cursor);
		}
		return;
    
	}

	hits = g_list_append (NULL, (gchar*) tracker_sparql_cursor_get_string (cursor, 0, NULL));
	brasero_search_engine_hit_added (search, result);
	g_list_free (hits);

	brasero_search_engine_query_finished (search);
}

static gboolean
brasero_search_tracker_query_start_real (BraseroSearchEngine *search,
					 gint index)
{
	BraseroSearchTrackerPrivate *priv;
	gboolean res = FALSE;
	GString *query = NULL;

	priv = BRASERO_SEARCH_TRACKER_PRIVATE (search);

	query = g_string_new ("SELECT ?file ?url ?mime "			/* Which variables should be returned */
			      "WHERE {"						/* Start defining the search and its scope */
			      "  ?file a nfo:FileDataObject . "			/* File must be a file (not a stream, ...) */
	                      "  ?file nie:url ?url . "				/* Get the url of the file */
	                      "  ?file nie:mimeType ?mime . "			/* Get its mime */
			      "  ?content nie:isStoredAs ?file . ");		/* Get the resource representing the content */

	if (priv->mimes) {
		int i;

		g_string_append (query, " FILTER ( ");
		for (i = 0; priv->mimes [i]; i ++) {				/* Filter files according to their mime type */
			if (i > 0)
				g_string_append (query, " || ");

			g_string_append_printf (query,
						"?mime = \"%s\"",
						priv->mimes [i]);
		}
		g_string_append (query, " ) ");
	}

	if (priv->scope) {
		gboolean param_added = FALSE;

		g_string_append (query,
				 "  ?content a ?type . "
				 "  FILTER ( ");

		if (priv->scope & BRASERO_SEARCH_SCOPE_MUSIC) {
			query = g_string_append (query, "?type = nmm:MusicPiece");
			param_added = TRUE;
		}

		if (priv->scope & BRASERO_SEARCH_SCOPE_VIDEO) {
			if (param_added)
				g_string_append (query, " || ");
			query = g_string_append (query, "?type = nfo:Video");

			param_added = TRUE;
		}
	
		if (priv->scope & BRASERO_SEARCH_SCOPE_PICTURES) {
			if (param_added)
				g_string_append (query, " || ");
			query = g_string_append (query, "?type = nfo:Image");

			param_added = TRUE;
		}

		if (priv->scope & BRASERO_SEARCH_SCOPE_DOCUMENTS) {
			if (param_added)
				g_string_append (query, " || ");
			query = g_string_append (query, "?type = nfo:Document");
		}

		g_string_append (query,
				 " ) ");
	}

	if (priv->keywords) {
		g_string_append_printf (query,
					"  ?file fts:match \"%s\" ",		/* File must match possible keywords */
					priv->keywords);

		g_string_append (query,
				 " } "
				 "ORDER BY ASC(fts:rank(?file))");
	} else {
		g_string_append (query,
				 "} ORDER BY DESC(?url) DESC(nfo:fileName(?file))");
	}

	tracker_sparql_connection_query_async (priv->connection,
					       query->str,
					       priv->cancellable,
					       brasero_search_tracker_reply,
					       search);
	g_string_free (query, TRUE);

	return res;
}

static gboolean
brasero_search_tracker_query_start (BraseroSearchEngine *search)
{
	return brasero_search_tracker_query_start_real (search, 0);
}

static gboolean
brasero_search_tracker_add_hit_to_tree (BraseroSearchEngine *search,
                                        GtkTreeModel *model,
                                        gint range_start,
                                        gint range_end)
{
	BraseroSearchTrackerPrivate *priv;
	gint i;

	priv = BRASERO_SEARCH_TRACKER_PRIVATE (search);

	if (!priv->results)
		return FALSE;

	for (i = range_start; g_ptr_array_index (priv->results, i) && i < range_end; i ++) {
		gchar **hit;
		GtkTreeIter row;

		hit = g_ptr_array_index (priv->results, i);
		gtk_list_store_insert_with_values (GTK_LIST_STORE (model), &row, -1,
		                                   BRASERO_SEARCH_TREE_HIT_COL, hit,
		                                   -1);
	}

	return TRUE;
}

static gboolean
brasero_search_tracker_query_set_scope (BraseroSearchEngine *search,
                                        BraseroSearchScope scope)
{
	BraseroSearchTrackerPrivate *priv;

	priv = BRASERO_SEARCH_TRACKER_PRIVATE (search);
	priv->scope = scope;
	return TRUE;
}

static gboolean
brasero_search_tracker_set_query_mime (BraseroSearchEngine *search,
				       const gchar **mimes)
{
	BraseroSearchTrackerPrivate *priv;

	priv = BRASERO_SEARCH_TRACKER_PRIVATE (search);

	if (priv->mimes) {
		g_strfreev (priv->mimes);
		priv->mimes = NULL;
	}

	priv->mimes = g_strdupv ((gchar **) mimes);
	return TRUE;
}

static void
brasero_search_tracker_clean (BraseroSearchTracker *search)
{
	BraseroSearchTrackerPrivate *priv;

	priv = BRASERO_SEARCH_TRACKER_PRIVATE (search);

	if (priv->current_call_id)
		g_cancellable_cancel (priv->cancellable);

	if (priv->results) {
		g_ptr_array_foreach (priv->results, (GFunc) g_strfreev, NULL);
		g_ptr_array_free (priv->results, TRUE);
		priv->results = NULL;
	}

	if (priv->keywords) {
		g_free (priv->keywords);
		priv->keywords = NULL;
	}

	if (priv->mimes) {
		g_strfreev (priv->mimes);
		priv->mimes = NULL;
	}
}

static gboolean
brasero_search_tracker_query_new (BraseroSearchEngine *search,
				  const gchar *keywords)
{
	BraseroSearchTrackerPrivate *priv;

	priv = BRASERO_SEARCH_TRACKER_PRIVATE (search);

	brasero_search_tracker_clean (BRASERO_SEARCH_TRACKER (search));
	priv->keywords = g_strdup (keywords);

	return TRUE;
}

static void
brasero_search_tracker_init_engine (BraseroSearchEngineIface *iface)
{
	iface->is_available = brasero_search_tracker_is_available;
	iface->query_new = brasero_search_tracker_query_new;
	iface->query_set_mime = brasero_search_tracker_set_query_mime;
	iface->query_set_scope = brasero_search_tracker_query_set_scope;
	iface->query_start = brasero_search_tracker_query_start;

	iface->uri_from_hit = brasero_search_tracker_uri_from_hit;
	iface->mime_from_hit = brasero_search_tracker_mime_from_hit;
	iface->score_from_hit = brasero_search_tracker_score_from_hit;

	iface->add_hits = brasero_search_tracker_add_hit_to_tree;
	iface->num_hits = brasero_search_tracker_num_hits;
}

static void
brasero_search_tracker_init (BraseroSearchTracker *object)
{
	BraseroSearchTrackerPrivate *priv;
	GError *error = NULL;

	priv = BRASERO_SEARCH_TRACKER_PRIVATE (object);
	priv->cancellable = g_cancellable_new ();
	priv->connection = tracker_sparql_connection_get (priv->cancellable, &error);

	if (error) {
		g_warning ("Could not establish a connection to Tracker: %s", error->message);
		g_error_free (error);
		g_object_unref (priv->cancellable);
		
		return;
	} else if (!priv->connection) {
		g_warning ("Could not establish a connection to Tracker, no TrackerSparqlConnection was returned");
		g_object_unref (priv->cancellable);
		
		return;
	}
}

static void
brasero_search_tracker_finalize (GObject *object)
{
	BraseroSearchTrackerPrivate *priv;

	priv = BRASERO_SEARCH_TRACKER_PRIVATE (object);

	brasero_search_tracker_clean (BRASERO_SEARCH_TRACKER (object));

	if (priv->connection) {
		g_object_unref (priv->connection);
		priv->connection = NULL;
	}

	G_OBJECT_CLASS (brasero_search_tracker_parent_class)->finalize (object);
}

static void
brasero_search_tracker_class_init (BraseroSearchTrackerClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);


	g_type_class_add_private (klass, sizeof (BraseroSearchTrackerPrivate));

	object_class->finalize = brasero_search_tracker_finalize;
}

