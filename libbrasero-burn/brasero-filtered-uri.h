/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-burn
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-burn is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-burn authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-burn. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-burn is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifndef _BRASERO_FILTERED_URI_H_
#define _BRASERO_FILTERED_URI_H_

#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum {
	BRASERO_FILTER_NONE			= 0,
	BRASERO_FILTER_HIDDEN			= 1,
	BRASERO_FILTER_UNREADABLE,
	BRASERO_FILTER_BROKEN_SYM,
	BRASERO_FILTER_RECURSIVE_SYM,
	BRASERO_FILTER_UNKNOWN,
} BraseroFilterStatus;

#define BRASERO_TYPE_FILTERED_URI             (brasero_filtered_uri_get_type ())
#define BRASERO_FILTERED_URI(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_FILTERED_URI, BraseroFilteredUri))
#define BRASERO_FILTERED_URI_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_FILTERED_URI, BraseroFilteredUriClass))
#define BRASERO_IS_FILTERED_URI(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_FILTERED_URI))
#define BRASERO_IS_FILTERED_URI_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_FILTERED_URI))
#define BRASERO_FILTERED_URI_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_FILTERED_URI, BraseroFilteredUriClass))

typedef struct _BraseroFilteredUriClass BraseroFilteredUriClass;
typedef struct _BraseroFilteredUri BraseroFilteredUri;

struct _BraseroFilteredUriClass
{
	GtkListStoreClass parent_class;
};

struct _BraseroFilteredUri
{
	GtkListStore parent_instance;
};

GType brasero_filtered_uri_get_type (void) G_GNUC_CONST;

BraseroFilteredUri *
brasero_filtered_uri_new (void);

gchar *
brasero_filtered_uri_restore (BraseroFilteredUri *filtered,
			      GtkTreePath *treepath);

BraseroFilterStatus
brasero_filtered_uri_lookup_restored (BraseroFilteredUri *filtered,
				      const gchar *uri);

GSList *
brasero_filtered_uri_get_restored_list (BraseroFilteredUri *filtered);

void
brasero_filtered_uri_filter (BraseroFilteredUri *filtered,
			     const gchar *uri,
			     BraseroFilterStatus status);
void
brasero_filtered_uri_dont_filter (BraseroFilteredUri *filtered,
				  const gchar *uri);

void
brasero_filtered_uri_clear (BraseroFilteredUri *filtered);

void
brasero_filtered_uri_remove_with_children (BraseroFilteredUri *filtered,
					   const gchar *uri);

G_END_DECLS

#endif /* _BRASERO_FILTERED_URI_H_ */
