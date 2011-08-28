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

#include "brasero-search-engine.h"

static void brasero_search_engine_base_init (gpointer g_class);

typedef enum {
	SEARCH_FINISHED,
	SEARCH_ERROR,
	HIT_REMOVED,
	HIT_ADDED,
	LAST_SIGNAL
} BraseroSearchEngineSignalType;

static guint brasero_search_engine_signals [LAST_SIGNAL] = { 0 };

gboolean
brasero_search_engine_is_available (BraseroSearchEngine *search)
{
	BraseroSearchEngineIface *iface;

	g_return_val_if_fail (BRASERO_IS_SEARCH_ENGINE (search), FALSE);

	iface = BRASERO_SEARCH_ENGINE_GET_IFACE (search);
	if (!iface->is_available)
		return FALSE;

	return (* iface->is_available) (search);
}

gboolean
brasero_search_engine_start_query (BraseroSearchEngine *search)
{
	BraseroSearchEngineIface *iface;

	g_return_val_if_fail (BRASERO_IS_SEARCH_ENGINE (search), FALSE);

	iface = BRASERO_SEARCH_ENGINE_GET_IFACE (search);
	if (!iface->query_start)
		return FALSE;

	return (* iface->query_start) (search);
}

gboolean
brasero_search_engine_new_query (BraseroSearchEngine *search,
                                 const gchar *keywords)
{
	BraseroSearchEngineIface *iface;

	g_return_val_if_fail (BRASERO_IS_SEARCH_ENGINE (search), FALSE);

	iface = BRASERO_SEARCH_ENGINE_GET_IFACE (search);
	if (!iface->query_new)
		return FALSE;

	return (* iface->query_new) (search, keywords);
}

gboolean
brasero_search_engine_set_query_scope (BraseroSearchEngine *search,
                                       BraseroSearchScope scope)
{
	BraseroSearchEngineIface *iface;

	g_return_val_if_fail (BRASERO_IS_SEARCH_ENGINE (search), FALSE);

	iface = BRASERO_SEARCH_ENGINE_GET_IFACE (search);
	if (!iface->query_set_scope)
		return FALSE;

	return (* iface->query_set_scope) (search, scope);
}

gboolean
brasero_search_engine_set_query_mime (BraseroSearchEngine *search,
                                      const gchar **mimes)
{
	BraseroSearchEngineIface *iface;

	g_return_val_if_fail (BRASERO_IS_SEARCH_ENGINE (search), FALSE);

	iface = BRASERO_SEARCH_ENGINE_GET_IFACE (search);
	if (!iface->query_set_mime)
		return FALSE;

	return (* iface->query_set_mime) (search, mimes);
}

gboolean
brasero_search_engine_add_hits (BraseroSearchEngine *search,
                                GtkTreeModel *model,
                                gint range_start,
                                gint range_end)
{
	BraseroSearchEngineIface *iface;

	g_return_val_if_fail (BRASERO_IS_SEARCH_ENGINE (search), FALSE);

	iface = BRASERO_SEARCH_ENGINE_GET_IFACE (search);
	if (!iface->add_hits)
		return FALSE;

	return iface->add_hits (search, model, range_start, range_end);
}

const gchar *
brasero_search_engine_uri_from_hit (BraseroSearchEngine *search,
                                     gpointer hit)
{
	BraseroSearchEngineIface *iface;

	g_return_val_if_fail (BRASERO_IS_SEARCH_ENGINE (search), NULL);

	if (!hit)
		return NULL;

	iface = BRASERO_SEARCH_ENGINE_GET_IFACE (search);
	if (!iface->uri_from_hit)
		return FALSE;

	return (* iface->uri_from_hit) (search, hit);
}

const gchar *
brasero_search_engine_mime_from_hit (BraseroSearchEngine *search,
                                     gpointer hit)
{
	BraseroSearchEngineIface *iface;

	g_return_val_if_fail (BRASERO_IS_SEARCH_ENGINE (search), NULL);

	if (!hit)
		return NULL;

	iface = BRASERO_SEARCH_ENGINE_GET_IFACE (search);
	if (!iface->mime_from_hit)
		return FALSE;

	return (* iface->mime_from_hit) (search, hit);
}

gint
brasero_search_engine_score_from_hit (BraseroSearchEngine *search,
                                      gpointer hit)
{
	BraseroSearchEngineIface *iface;

	g_return_val_if_fail (BRASERO_IS_SEARCH_ENGINE (search), 0);

	if (!hit)
		return 0;

	iface = BRASERO_SEARCH_ENGINE_GET_IFACE (search);
	if (!iface->score_from_hit)
		return FALSE;

	return (* iface->score_from_hit) (search, hit);
}

gint
brasero_search_engine_num_hits (BraseroSearchEngine *search)
{
	BraseroSearchEngineIface *iface;

	g_return_val_if_fail (BRASERO_IS_SEARCH_ENGINE (search), 0);

	iface = BRASERO_SEARCH_ENGINE_GET_IFACE (search);
	if (!iface->num_hits)
		return FALSE;

	return (* iface->num_hits) (search);
}

void
brasero_search_engine_query_finished (BraseroSearchEngine *search)
{
	g_signal_emit (search,
	               brasero_search_engine_signals [SEARCH_FINISHED],
	               0);
}

void
brasero_search_engine_hit_removed (BraseroSearchEngine *search,
                                   gpointer hit)
{
	g_signal_emit (search,
	               brasero_search_engine_signals [HIT_REMOVED],
	               0,
	               hit);
}

void
brasero_search_engine_hit_added (BraseroSearchEngine *search,
                                 gpointer hit)
{
	g_signal_emit (search,
	               brasero_search_engine_signals [HIT_ADDED],
	               0,
	               hit);
}

void
brasero_search_engine_query_error (BraseroSearchEngine *search,
                                   GError *error)
{
	g_signal_emit (search,
	               brasero_search_engine_signals [SEARCH_ERROR],
	               0,
	               error);
}

GType
brasero_search_engine_get_type()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroSearchEngineIface),
			brasero_search_engine_base_init,
			NULL,
			NULL,
			NULL,
			NULL,
			0,
			0,
			NULL
		};

		type = g_type_register_static (G_TYPE_INTERFACE, 
					       "BraseroSearchEngine",
					       &our_info,
					       0);

		g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
	}

	return type;
}

static void
brasero_search_engine_base_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (initialized)
		return;

	brasero_search_engine_signals [SEARCH_ERROR] =
	    g_signal_new ("search_error",
			  BRASERO_TYPE_SEARCH_ENGINE,
			  G_SIGNAL_RUN_LAST|G_SIGNAL_ACTION|G_SIGNAL_NO_RECURSE,
			  G_STRUCT_OFFSET (BraseroSearchEngineIface, search_error),
			  NULL,
			  NULL,
			  g_cclosure_marshal_VOID__POINTER,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_POINTER);
	brasero_search_engine_signals [SEARCH_FINISHED] =
	    g_signal_new ("search_finished",
			  BRASERO_TYPE_SEARCH_ENGINE,
			  G_SIGNAL_RUN_LAST|G_SIGNAL_ACTION|G_SIGNAL_NO_RECURSE,
			  G_STRUCT_OFFSET (BraseroSearchEngineIface, search_finished),
			  NULL,
			  NULL,
			  g_cclosure_marshal_VOID__VOID,
			  G_TYPE_NONE,
			  0);
	brasero_search_engine_signals [HIT_REMOVED] =
	    g_signal_new ("hit_removed",
			  BRASERO_TYPE_SEARCH_ENGINE,
			  G_SIGNAL_RUN_LAST|G_SIGNAL_ACTION|G_SIGNAL_NO_RECURSE,
			  G_STRUCT_OFFSET (BraseroSearchEngineIface, hit_removed),
			  NULL,
			  NULL,
			  g_cclosure_marshal_VOID__POINTER,
			  G_TYPE_NONE,
			  1,
	                  G_TYPE_POINTER);
	brasero_search_engine_signals [HIT_ADDED] =
	    g_signal_new ("hit_added",
			  BRASERO_TYPE_SEARCH_ENGINE,
			  G_SIGNAL_RUN_LAST|G_SIGNAL_ACTION|G_SIGNAL_NO_RECURSE,
			  G_STRUCT_OFFSET (BraseroSearchEngineIface, hit_added),
			  NULL,
			  NULL,
			  g_cclosure_marshal_VOID__POINTER,
			  G_TYPE_NONE,
			  1,
	                  G_TYPE_POINTER);
	initialized = TRUE;
}

#ifdef BUILD_SEARCH

#ifdef BUILD_TRACKER

#include "brasero-search-tracker.h"

BraseroSearchEngine *
brasero_search_engine_new_default (void)
{
	return g_object_new (BRASERO_TYPE_SEARCH_TRACKER, NULL);
}

#endif

#else

BraseroSearchEngine *
brasero_search_engine_new_default (void)
{
	return NULL;
}

#endif
