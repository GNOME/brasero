/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-misc
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-misc is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-misc authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-misc. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-misc is distributed in the hope that it will be useful,
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

#ifndef _METADATA_H
#define _METADATA_H

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <gst/gst.h>

G_BEGIN_DECLS

typedef enum {
	BRASERO_METADATA_FLAG_NONE			= 0,
	BRASERO_METADATA_FLAG_SILENCES			= 1 << 1,
	BRASERO_METADATA_FLAG_MISSING			= 1 << 2,
	BRASERO_METADATA_FLAG_THUMBNAIL			= 1 << 3
} BraseroMetadataFlag;

#define BRASERO_TYPE_METADATA         (brasero_metadata_get_type ())
#define BRASERO_METADATA(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_METADATA, BraseroMetadata))
#define BRASERO_METADATA_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_METADATA, BraseroMetadataClass))
#define BRASERO_IS_METADATA(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_METADATA))
#define BRASERO_IS_METADATA_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_METADATA))
#define BRASERO_METADATA_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_METADATA, BraseroMetadataClass))

typedef struct {
	gint64 start;
	gint64 end;
} BraseroMetadataSilence;

typedef struct {
	gchar *uri;
	gchar *type;
	gchar *title;
	gchar *artist;
	gchar *album;
	gchar *genre;
	gchar *composer;
	gchar *musicbrainz_id;
	gchar *isrc;
	guint64 len;

	gint channels;
	gint rate;

	GSList *silences;

	GdkPixbuf *snapshot;

	guint is_seekable:1;
	guint has_audio:1;
	guint has_video:1;
	guint has_dts:1;
} BraseroMetadataInfo;

void
brasero_metadata_info_copy (BraseroMetadataInfo *dest,
			    BraseroMetadataInfo *src);
void
brasero_metadata_info_clear (BraseroMetadataInfo *info);
void
brasero_metadata_info_free (BraseroMetadataInfo *info);

typedef struct _BraseroMetadataClass BraseroMetadataClass;
typedef struct _BraseroMetadata BraseroMetadata;

struct _BraseroMetadataClass {
	GObjectClass parent_class;

	void		(*completed)	(BraseroMetadata *meta,
					 const GError *error);
	void		(*progress)	(BraseroMetadata *meta,
					 gdouble progress);

};

struct _BraseroMetadata {
	GObject parent;
};

GType brasero_metadata_get_type (void) G_GNUC_CONST;

BraseroMetadata *brasero_metadata_new (void);

gboolean
brasero_metadata_get_info_async (BraseroMetadata *metadata,
				 const gchar *uri,
				 BraseroMetadataFlag flags);

void
brasero_metadata_cancel (BraseroMetadata *metadata);

gboolean
brasero_metadata_set_uri (BraseroMetadata *metadata,
			  BraseroMetadataFlag flags,
			  const gchar *uri,
			  GError **error);

void
brasero_metadata_wait (BraseroMetadata *metadata,
		       GCancellable *cancel);
void
brasero_metadata_increase_listener_number (BraseroMetadata *metadata);

gboolean
brasero_metadata_decrease_listener_number (BraseroMetadata *metadata);

const gchar *
brasero_metadata_get_uri (BraseroMetadata *metadata);

BraseroMetadataFlag
brasero_metadata_get_flags (BraseroMetadata *metadata);

gboolean
brasero_metadata_get_result (BraseroMetadata *metadata,
			     BraseroMetadataInfo *info,
			     GError **error);

typedef int	(*BraseroMetadataGetXidCb)	(gpointer user_data);

void
brasero_metadata_set_get_xid_callback (BraseroMetadata *metadata,
                                       BraseroMetadataGetXidCb callback,
                                       gpointer user_data);
G_END_DECLS

#endif				/* METADATA_H */
