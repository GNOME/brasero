/***************************************************************************
*            search-entry.h
*
*  jeu mai 19 20:06:55 2005
*  Copyright  2005  Philippe Rouquier
*  brasero-app@wanadoo.fr
****************************************************************************/

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

#ifdef BUILD_SEARCH

#ifndef SEARCH_ENTRY_H
#define SEARCH_ENTRY_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "brasero-layout.h"
#include "brasero-search-engine.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_SEARCH_ENTRY         (brasero_search_entry_get_type ())
#define BRASERO_SEARCH_ENTRY(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_SEARCH_ENTRY, BraseroSearchEntry))
#define BRASERO_SEARCH_ENTRY_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_SEARCH_ENTRY, BraseroSearchEntryClass))
#define BRASERO_IS_SEARCH_ENTRY(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_SEARCH_ENTRY))
#define BRASERO_IS_SEARCH_ENTRY_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_SEARCH_ENTRY))
#define BRASERO_SEARCH_ENTRY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_SEARCH_ENTRY, BraseroSearchEntryClass))

typedef struct BraseroSearchEntryPrivate BraseroSearchEntryPrivate;

typedef struct {
	GtkTable parent;
	BraseroSearchEntryPrivate *priv;
} BraseroSearchEntry;

typedef struct {
	GtkTableClass parent_class;

	void	(*activated)	(BraseroSearchEntry *entry);

} BraseroSearchEntryClass;

GType brasero_search_entry_get_type (void);
GtkWidget *brasero_search_entry_new (void);

gboolean
brasero_search_entry_set_query (BraseroSearchEntry *entry,
                                BraseroSearchEngine *search);

void
brasero_search_entry_set_context (BraseroSearchEntry *entry,
				  BraseroLayoutType type);

#endif				/* SEARCH_ENTRY_H */

#endif
