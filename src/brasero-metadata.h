/***************************************************************************
 *            metadata.h
 *
 *  jeu jui 28 12:49:41 2005
 *  Copyright  2005  Philippe Rouquier
 *  brasero-app@wanadoo.fr
 ***************************************************************************/

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

#ifndef METADATA_H
#define METADATA_H

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <gdk/gdkpixbuf.h>

#include <gst/gst.h>

G_BEGIN_DECLS

typedef enum {
	BRASERO_METADATA_FLAG_NONE			= 0,
	BRASERO_METADATA_FLAG_FAST			= 1,
	BRASERO_METADATA_FLAG_SILENCES			= 1 << 1,
	BRASERO_METADATA_FLAG_MISSING			= 1 << 2,
	BRASERO_METADATA_FLAG_SNAPHOT			= 1 << 3
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
	int isrc;
	guint64 len;

	GSList *silences;

	GdkPixbuf *snapshot;

	guint is_seekable:1;
	guint has_audio:1;
	guint has_video:1;
} BraseroMetadataInfo;

void
brasero_metadata_info_copy (BraseroMetadataInfo *dest,
			    BraseroMetadataInfo *src);
void
brasero_metadata_info_clear (BraseroMetadataInfo *info);
void
brasero_metadata_info_free (BraseroMetadataInfo *info);

typedef struct {
	GObject parent;
} BraseroMetadata;

typedef struct {
	GObjectClass parent_class;

	void		(*completed)	(BraseroMetadata *meta,
					 const GError *error);
	void		(*progress)	(BraseroMetadata *meta,
					 gdouble progress);

} BraseroMetadataClass;

GType brasero_metadata_get_type ();
BraseroMetadata *brasero_metadata_new (void);

void
brasero_metadata_cancel (BraseroMetadata *metadata);

gboolean
brasero_metadata_get_info_wait (BraseroMetadata *self,
				GCancellable *cancel,
				const gchar *uri,
				BraseroMetadataFlag flags,
				GError **error);

gboolean
brasero_metadata_get_info_sync (BraseroMetadata *meta,
				const gchar *uri,
				BraseroMetadataFlag flags,
				GError **error);

gboolean
brasero_metadata_get_info_async (BraseroMetadata *meta,
				 const gchar *uri,
				 BraseroMetadataFlag flags);

gboolean
brasero_metadata_set_info (BraseroMetadata *meta,
			   BraseroMetadataInfo *info);

#endif				/* METADATA_H */
