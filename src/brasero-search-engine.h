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

#ifndef _SEARCH_ENGINE_H
#define _SEARCH_ENGINE_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

enum {
	BRASERO_SEARCH_TREE_HIT_COL		= 0,
	BRASERO_SEARCH_TREE_NB_COL
};

typedef enum {
	BRASERO_SEARCH_SCOPE_ANY		= 0,
	BRASERO_SEARCH_SCOPE_VIDEO		= 1,
	BRASERO_SEARCH_SCOPE_MUSIC		= 1 << 1,
	BRASERO_SEARCH_SCOPE_PICTURES	= 1 << 2,
	BRASERO_SEARCH_SCOPE_DOCUMENTS	= 1 << 3,
} BraseroSearchScope;

#define BRASERO_TYPE_SEARCH_ENGINE         (brasero_search_engine_get_type ())
#define BRASERO_SEARCH_ENGINE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_SEARCH_ENGINE, BraseroSearchEngine))
#define BRASERO_IS_SEARCH_ENGINE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_SEARCH_ENGINE))
#define BRASERO_SEARCH_ENGINE_GET_IFACE(o) (G_TYPE_INSTANCE_GET_INTERFACE ((o), BRASERO_TYPE_SEARCH_ENGINE, BraseroSearchEngineIface))

typedef struct _BraseroSearchEngine        BraseroSearchEngine;
typedef struct _BraseroSearchEngineIface   BraseroSearchEngineIface;

struct _BraseroSearchEngineIface {
	GTypeInterface g_iface;

	/* <Signals> */
	void	(*search_error)				(BraseroSearchEngine *search,
							 GError *error);
	void	(*search_finished)			(BraseroSearchEngine *search);
	void	(*hit_removed)				(BraseroSearchEngine *search,
					                 gpointer hit);
	void	(*hit_added)				(BraseroSearchEngine *search,
						         gpointer hit);

	/* <Virtual functions> */
	gboolean	(*is_available)			(BraseroSearchEngine *search);
	gboolean	(*query_new)			(BraseroSearchEngine *search,
					                 const gchar *keywords);
	gboolean	(*query_set_scope)		(BraseroSearchEngine *search,
					                 BraseroSearchScope scope);
	gboolean	(*query_set_mime)		(BraseroSearchEngine *search,
					                 const gchar **mimes);
	gboolean	(*query_start)			(BraseroSearchEngine *search);

	gboolean	(*add_hits)			(BraseroSearchEngine *search,
					                 GtkTreeModel *model,
					                 gint range_start,
					                 gint range_end);

	gint		(*num_hits)			(BraseroSearchEngine *engine);

	const gchar*	(*uri_from_hit)			(BraseroSearchEngine *engine,
				                         gpointer hit);
	const gchar *	(*mime_from_hit)		(BraseroSearchEngine *engine,
				                	 gpointer hit);
	gint		(*score_from_hit)		(BraseroSearchEngine *engine,
							 gpointer hit);
};

GType brasero_search_engine_get_type (void);

BraseroSearchEngine *
brasero_search_engine_new_default (void);

gboolean
brasero_search_engine_is_available (BraseroSearchEngine *search);

gint
brasero_search_engine_num_hits (BraseroSearchEngine *search);

gboolean
brasero_search_engine_new_query (BraseroSearchEngine *search,
                                 const gchar *keywords);

gboolean
brasero_search_engine_set_query_scope (BraseroSearchEngine *search,
                                       BraseroSearchScope scope);

gboolean
brasero_search_engine_set_query_mime (BraseroSearchEngine *search,
                                      const gchar **mimes);

gboolean
brasero_search_engine_start_query (BraseroSearchEngine *search);

void
brasero_search_engine_query_finished (BraseroSearchEngine *search);

void
brasero_search_engine_query_error (BraseroSearchEngine *search,
                                   GError *error);

void
brasero_search_engine_hit_removed (BraseroSearchEngine *search,
                                   gpointer hit);
void
brasero_search_engine_hit_added (BraseroSearchEngine *search,
                                 gpointer hit);

const gchar *
brasero_search_engine_uri_from_hit (BraseroSearchEngine *search,
                                     gpointer hit);
const gchar *
brasero_search_engine_mime_from_hit (BraseroSearchEngine *search,
                                     gpointer hit);
gint
brasero_search_engine_score_from_hit (BraseroSearchEngine *search,
                                      gpointer hit);

gint
brasero_search_engine_add_hits (BraseroSearchEngine *search,
                                GtkTreeModel *model,
                                gint range_start,
                                gint range_end);

G_END_DECLS

#endif /* _SEARCH_ENGINE_H */
