/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Brasero
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 * 
 *  Brasero is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 * brasero is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with brasero.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include <gio/gio.h>

#include <gtk/gtk.h>

#include <beagle/beagle.h>

#include "brasero-search-beagle.h"
#include "brasero-search-engine.h"


typedef struct _BraseroSearchBeaglePrivate BraseroSearchBeaglePrivate;
struct _BraseroSearchBeaglePrivate
{
	BeagleClient *client;
	BeagleQuery *query;

	GSList * hits;
};

#define BRASERO_SEARCH_BEAGLE_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_SEARCH_BEAGLE, BraseroSearchBeaglePrivate))

static void brasero_search_beagle_init_engine (BraseroSearchEngineIface *iface);

G_DEFINE_TYPE_WITH_CODE (BraseroSearchBeagle,
			 brasero_search_beagle,
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (BRASERO_TYPE_SEARCH_ENGINE,
					        brasero_search_beagle_init_engine));

static gboolean
brasero_search_beagle_is_available (BraseroSearchEngine *engine)
{
	BraseroSearchBeaglePrivate *priv;

	priv = BRASERO_SEARCH_BEAGLE_PRIVATE (engine);
	if (priv->client)
		return TRUE;

	priv->client = beagle_client_new (NULL);
	return (priv->client != NULL);
}

static gint
brasero_search_beagle_num_hits (BraseroSearchEngine *engine)
{
	BraseroSearchBeaglePrivate *priv;

	priv = BRASERO_SEARCH_BEAGLE_PRIVATE (engine);
	return g_slist_length (priv->hits);
}

static const gchar *
brasero_search_beagle_uri_from_hit (BraseroSearchEngine *engine,
                                     gpointer hit)
{
	return beagle_hit_get_uri (BEAGLE_HIT (hit));
}

static gchar *
brasero_search_beagle_name_from_hit (BraseroSearchEngine *engine,
                                     gpointer hit)
{
	gchar *name;
	const gchar *uri;
	gchar *unescaped_uri;

	uri = beagle_hit_get_uri (BEAGLE_HIT (hit));

	/* beagle return badly formed uri not
	 * encoded in UTF-8 locale charset so we
	 * check them just in case */
	unescaped_uri = g_uri_unescape_string (uri, NULL);
	if (!g_utf8_validate (unescaped_uri, -1, NULL)) {
		g_free (unescaped_uri);
		return NULL;
	}

	name = g_path_get_basename (unescaped_uri);
	g_free (unescaped_uri);

	return name;
}

static const gchar *
brasero_search_beagle_mime_from_hit (BraseroSearchEngine *engine,
                                     gpointer hit)
{
	return beagle_hit_get_mime_type (BEAGLE_HIT (hit));
}

static const gchar *
brasero_search_beagle_description_from_hit (BraseroSearchEngine *engine,
                                            gpointer hit)
{
	const gchar *mime;

	mime = beagle_hit_get_mime_type (BEAGLE_HIT (hit));
	if (!mime)
		return NULL;

	return g_content_type_get_description (mime);
}

static int
brasero_search_beagle_score_from_hit (BraseroSearchEngine *engine,
                                      gpointer hit)
{
	return (int) (beagle_hit_get_score (BEAGLE_HIT (hit)) * 100);
}

static GIcon *
brasero_search_beagle_icon_from_hit (BraseroSearchEngine *engine,
                                     gpointer hit)
{
	const gchar *mime;

	mime = beagle_hit_get_mime_type (BEAGLE_HIT (hit));
	if (!mime)
		return NULL;

	if (!strcmp (mime, "inode/directory"))
		mime = "x-directory/normal";

	return g_content_type_get_icon (mime);
}

static gboolean
brasero_search_beagle_add_hit_to_tree (BraseroSearchEngine *search,
                                       GtkTreeModel *model,
                                       gint range_start,
                                       gint range_end)
{
	BraseroSearchBeaglePrivate *priv;
	GSList *iter;
	gint num;

	priv = BRASERO_SEARCH_BEAGLE_PRIVATE (search);

	num = 0;
	iter = g_slist_nth (priv->hits, range_start);

	for (; iter && num < range_end - range_start; iter = iter->next, num ++) {
		BeagleHit *hit;
		GtkTreeIter row;

		hit = iter->data;

		gtk_list_store_insert_with_values (GTK_LIST_STORE (model), &row, -1,
		                                   BRASERO_SEARCH_TREE_HIT_COL, hit,
		                                   -1);
	}

	return TRUE;
}

static void
brasero_search_beagle_hit_added_cb (BeagleQuery *query,
				    BeagleHitsAddedResponse *response,
				    BraseroSearchBeagle *search)
{
	GSList *list;
	GSList *iter;
	BraseroSearchBeaglePrivate *priv;

	priv = BRASERO_SEARCH_BEAGLE_PRIVATE (search);

	/* NOTE : list must not be modified nor freed */
	list = beagle_hits_added_response_get_hits (response);
	list = g_slist_copy (list);

	if (priv->hits)
		priv->hits = g_slist_concat (priv->hits, list);
	else
		priv->hits = list;

	for (iter = list; iter; iter = iter->next) {
		BeagleHit *hit;

		hit = iter->data;
		beagle_hit_ref (hit);

		brasero_search_engine_hit_added (BRASERO_SEARCH_ENGINE (search), hit);
	}
}

static void
brasero_search_beagle_hit_substracted_cb (BeagleQuery *query,
					  BeagleHitsSubtractedResponse *response,
					  BraseroSearchBeagle *search)
{
	GSList *list, *iter;
	BraseroSearchBeaglePrivate *priv;

	priv = BRASERO_SEARCH_BEAGLE_PRIVATE (search);

	list = beagle_hits_subtracted_response_get_uris (response);
	for (iter = list; iter; iter = iter->next) {
		GSList *next, *hit_iter;
		const gchar *removed_uri;

		removed_uri = iter->data;

		/* see if it isn't in the hits that are still waiting */
		for (hit_iter = priv->hits; hit_iter; hit_iter = next) {
			BeagleHit *hit;
			const char *hit_uri;
	
			next = hit_iter->next;
			hit = hit_iter->data;

			hit_uri = beagle_hit_get_uri (hit);
			if (!strcmp (hit_uri, removed_uri)) {
				priv->hits = g_slist_remove (priv->hits, hit);
				brasero_search_engine_hit_removed (BRASERO_SEARCH_ENGINE (search), hit);
				beagle_hit_unref (hit);
			}
		}
	}
}

static void
brasero_search_beagle_finished_cb (BeagleQuery *query,
				   BeagleFinishedResponse *response,
				   BraseroSearchBeagle *search)
{
	brasero_search_engine_query_finished (BRASERO_SEARCH_ENGINE (search));
}

static void
brasero_search_beagle_error_cb (BeagleRequest *request,
				GError *error,
				BraseroSearchEngine *search)
{
	brasero_search_engine_query_error (BRASERO_SEARCH_ENGINE (search), error);
}

static gboolean
brasero_search_beagle_query_start (BraseroSearchEngine *search)
{
	BraseroSearchBeaglePrivate *priv;
	GError *error = NULL;

	priv = BRASERO_SEARCH_BEAGLE_PRIVATE (search);

	/* search itself */
	if (!priv->query) {
		g_warning ("No query");
		return FALSE;
	}

	beagle_query_set_max_hits (priv->query, 10000);
	g_signal_connect (G_OBJECT (priv->query), "hits-added",
			  G_CALLBACK (brasero_search_beagle_hit_added_cb),
			  search);
	g_signal_connect (G_OBJECT (priv->query), "hits-subtracted",
			  G_CALLBACK
			  (brasero_search_beagle_hit_substracted_cb),
			  search);
	g_signal_connect (G_OBJECT (priv->query), "finished",
			  G_CALLBACK (brasero_search_beagle_finished_cb),
			  search);
	g_signal_connect (G_OBJECT (priv->query), "error",
			  G_CALLBACK (brasero_search_beagle_error_cb),
			  search);

	beagle_client_send_request_async (priv->client,
					  BEAGLE_REQUEST (priv->query),
					  &error);
	if (error) {
		brasero_search_engine_query_error (BRASERO_SEARCH_ENGINE (search), error);
		g_error_free (error);
		return FALSE;
	}

	return TRUE;
}

static void
brasero_search_beagle_clean (BraseroSearchBeagle *beagle)
{
	BraseroSearchBeaglePrivate *priv;

	priv = BRASERO_SEARCH_BEAGLE_PRIVATE (beagle);

	if (priv->query) {
		g_object_unref (priv->query);
		priv->query = NULL;
	}

	if (priv->hits) {
		g_slist_foreach (priv->hits, (GFunc) beagle_hit_unref, NULL);
		g_slist_free (priv->hits);
		priv->hits = NULL;
	}
}

static gboolean
brasero_search_beagle_query_new (BraseroSearchEngine *search,
                                  const gchar *keywords)
{
	BraseroSearchBeaglePrivate *priv;
	BeagleQueryPartHuman *text;

	priv = BRASERO_SEARCH_BEAGLE_PRIVATE (search);

	brasero_search_beagle_clean (BRASERO_SEARCH_BEAGLE (search));
	priv->query = beagle_query_new ();

	if (keywords) {
		BeagleQueryPartHuman *text;

		text = beagle_query_part_human_new ();
		beagle_query_part_human_set_string (text, keywords);
		beagle_query_part_set_logic (BEAGLE_QUERY_PART (text),
					     BEAGLE_QUERY_PART_LOGIC_REQUIRED);

		beagle_query_add_part (priv->query, BEAGLE_QUERY_PART (text));
	}

	text = beagle_query_part_human_new ();
	beagle_query_part_human_set_string (text, "type:File");
	beagle_query_add_part (priv->query, BEAGLE_QUERY_PART (text));

	return TRUE;
}

static gboolean
brasero_search_beagle_query_set_scope (BraseroSearchEngine *search,
                                       BraseroSearchScope scope)
{
	BeagleQueryPartOr *or_part = NULL;
	BraseroSearchBeaglePrivate *priv;

	priv = BRASERO_SEARCH_BEAGLE_PRIVATE (search);

	if (!priv->query)
		return FALSE;

	if (scope & BRASERO_SEARCH_SCOPE_DOCUMENTS) {
		BeagleQueryPartProperty *filetype;

		if (!or_part)
			or_part = beagle_query_part_or_new ();

		filetype = beagle_query_part_property_new ();
		beagle_query_part_property_set_property_type (filetype, BEAGLE_PROPERTY_TYPE_KEYWORD);
		beagle_query_part_property_set_key (filetype, "beagle:FileType");
		beagle_query_part_property_set_value (filetype, "document");
		beagle_query_part_or_add_subpart (or_part, BEAGLE_QUERY_PART (filetype));
	}

	if (scope & BRASERO_SEARCH_SCOPE_PICTURES) {
		BeagleQueryPartProperty *filetype;

		if (!or_part)
			or_part = beagle_query_part_or_new ();

		filetype = beagle_query_part_property_new ();
		beagle_query_part_property_set_property_type (filetype, BEAGLE_PROPERTY_TYPE_KEYWORD);
		beagle_query_part_property_set_key (filetype, "beagle:FileType");
		beagle_query_part_property_set_value (filetype, "image");
		beagle_query_part_or_add_subpart (or_part, BEAGLE_QUERY_PART (filetype));
	}

	if (scope & BRASERO_SEARCH_SCOPE_MUSIC) {
		BeagleQueryPartProperty *filetype;

		if (!or_part)
			or_part = beagle_query_part_or_new ();

		filetype = beagle_query_part_property_new ();
		beagle_query_part_property_set_property_type (filetype, BEAGLE_PROPERTY_TYPE_KEYWORD);
		beagle_query_part_property_set_key (filetype, "beagle:FileType");
		beagle_query_part_property_set_value (filetype, "audio");
		beagle_query_part_or_add_subpart (or_part, BEAGLE_QUERY_PART (filetype));
	}

	if (scope & BRASERO_SEARCH_SCOPE_VIDEO) {
		BeagleQueryPartProperty *filetype;

		if (!or_part)
			or_part = beagle_query_part_or_new ();

		filetype = beagle_query_part_property_new ();
		beagle_query_part_property_set_property_type (filetype, BEAGLE_PROPERTY_TYPE_KEYWORD);
		beagle_query_part_property_set_key (filetype, "beagle:FileType");
		beagle_query_part_property_set_value (filetype, "video");
		beagle_query_part_or_add_subpart (or_part, BEAGLE_QUERY_PART (filetype));
	}

	if (!or_part)
		return FALSE;

	beagle_query_add_part (priv->query, BEAGLE_QUERY_PART (or_part));
	return TRUE;
}

static gboolean
brasero_search_beagle_set_query_mime (BraseroSearchEngine *search,
                                      const gchar **mimes)
{
	int i;
	BeagleQueryPartOr *or_part;
	BraseroSearchBeaglePrivate *priv;

	priv = BRASERO_SEARCH_BEAGLE_PRIVATE (search);

	if (!priv->query)
		return FALSE;

	or_part = beagle_query_part_or_new ();
	for (i = 0; mimes [i]; i ++) {
		BeagleQueryPartProperty *filetype;

		filetype = beagle_query_part_property_new ();
		beagle_query_part_property_set_property_type (filetype, BEAGLE_PROPERTY_TYPE_KEYWORD);
		beagle_query_part_property_set_key (filetype, "beagle:MimeType");
		beagle_query_part_property_set_value (filetype, mimes [i]);
		beagle_query_part_or_add_subpart (or_part, BEAGLE_QUERY_PART (filetype));
	}

	beagle_query_add_part (priv->query, BEAGLE_QUERY_PART (or_part));

	return TRUE;
}

static void
brasero_search_beagle_init_engine (BraseroSearchEngineIface *iface)
{
	iface->is_available = brasero_search_beagle_is_available;
	iface->uri_from_hit = brasero_search_beagle_uri_from_hit;
	iface->name_from_hit = brasero_search_beagle_name_from_hit;
	iface->icon_from_hit = brasero_search_beagle_icon_from_hit;
	iface->score_from_hit = brasero_search_beagle_score_from_hit;
	iface->mime_from_hit = brasero_search_beagle_mime_from_hit;
	iface->description_from_hit = brasero_search_beagle_description_from_hit;
	iface->query_new = brasero_search_beagle_query_new;
	iface->query_set_scope = brasero_search_beagle_query_set_scope;
	iface->query_set_mime = brasero_search_beagle_set_query_mime;
	iface->query_start = brasero_search_beagle_query_start;
	iface->add_hits = brasero_search_beagle_add_hit_to_tree;
	iface->num_hits = brasero_search_beagle_num_hits;
}

static void
brasero_search_beagle_init (BraseroSearchBeagle *object)
{
	BraseroSearchBeaglePrivate *priv;

	priv = BRASERO_SEARCH_BEAGLE_PRIVATE (object);

	priv->client = beagle_client_new (NULL);
}

static void
brasero_search_beagle_finalize (GObject *object)
{
	BraseroSearchBeaglePrivate *priv;

	priv = BRASERO_SEARCH_BEAGLE_PRIVATE (object);

	if (priv->client) {
		g_object_unref (priv->client);
		priv->client = NULL;
	}

	brasero_search_beagle_clean (BRASERO_SEARCH_BEAGLE (object));
	G_OBJECT_CLASS (brasero_search_beagle_parent_class)->finalize (object);
}

static void
brasero_search_beagle_class_init (BraseroSearchBeagleClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroSearchBeaglePrivate));

	object_class->finalize = brasero_search_beagle_finalize;
}

