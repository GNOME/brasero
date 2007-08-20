/***************************************************************************
*            search.h
*
*  dim mai 22 11:20:54 2005
*  Copyright  2005  Philippe Rouquier
*  brasero-app@wanadoo.fr
****************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef BUILD_SEARCH

#ifndef SEARCH_H
#define SEARCH_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtkvbox.h>

G_BEGIN_DECLS
#define BRASERO_TYPE_SEARCH         (brasero_search_get_type ())
#define BRASERO_SEARCH(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_SEARCH, BraseroSearch))
#define BRASERO_SEARCH_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_SEARCH, BraseroSearchClass))
#define BRASERO_IS_SEARCH(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_SEARCH))
#define BRASERO_IS_SEARCH_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_SEARCH))
#define BRASERO_SEARCH_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_SEARCH, BraseroSearchClass))
typedef struct BraseroSearchPrivate BraseroSearchPrivate;

typedef struct {
	GtkVBox parent;
	BraseroSearchPrivate *priv;
} BraseroSearch;

typedef struct {
	GtkVBoxClass parent_class;;
} BraseroSearchClass;

GType brasero_search_get_type ();
GtkWidget *brasero_search_new ();

#endif				/* SEARCH_H */

#endif
