/***************************************************************************
 *            song-properties.h
 *
 *  lun avr 10 18:39:17 2006
 *  Copyright  2006  Rouquier Philippe
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

#ifndef SONG_PROPERTIES_H
#define SONG_PROPERTIES_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_SONG_PROPS         (brasero_song_props_get_type ())
#define BRASERO_SONG_PROPS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_SONG_PROPS, BraseroSongProps))
#define BRASERO_SONG_PROPS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_SONG_PROPS, BraseroSongPropsClass))
#define BRASERO_IS_SONG_PROPS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_SONG_PROPS))
#define BRASERO_IS_SONG_PROPS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_SONG_PROPS))
#define BRASERO_SONG_PROPS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_SONG_PROPS, BraseroSongPropsClass))

typedef struct BraseroSongPropsPrivate BraseroSongPropsPrivate;

typedef struct {
	GtkDialog parent;
	BraseroSongPropsPrivate *priv;
} BraseroSongProps;

typedef struct {
	GtkDialogClass parent_class;
} BraseroSongPropsClass;

GType brasero_song_props_get_type (void);
GtkWidget *brasero_song_props_new (void);

void
brasero_song_props_get_properties (BraseroSongProps *self,
				   gchar **artist,
				   gchar **title,
				   gchar **composer,
				   gchar **isrc,
				   gint64 *start,
				   gint64 *end,
				   gint64 *gap);
void
brasero_song_props_set_properties (BraseroSongProps *self,
				   gint track_num,
				   const gchar *artist,
				   const gchar *title,
				   const gchar *composer,
				   const gchar *isrc,
				   gint64 length,
				   gint64 start,
				   gint64 end,
				   gint64 gap);

G_END_DECLS

#endif /* SONG_PROPERTIES_H */
