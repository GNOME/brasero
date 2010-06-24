/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2010 <bonfire-app@wanadoo.fr>
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

#ifndef _BRASERO_SONG_CONTROL_H_
#define _BRASERO_SONG_CONTROL_H_

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_SONG_CONTROL             (brasero_song_control_get_type ())
#define BRASERO_SONG_CONTROL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_SONG_CONTROL, BraseroSongControl))
#define BRASERO_SONG_CONTROL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_SONG_CONTROL, BraseroSongControlClass))
#define BRASERO_IS_SONG_CONTROL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_SONG_CONTROL))
#define BRASERO_IS_SONG_CONTROL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_SONG_CONTROL))
#define BRASERO_SONG_CONTROL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_SONG_CONTROL, BraseroSongControlClass))

typedef struct _BraseroSongControlClass BraseroSongControlClass;
typedef struct _BraseroSongControl BraseroSongControl;

struct _BraseroSongControlClass
{
	GtkAlignmentClass parent_class;
};

struct _BraseroSongControl
{
	GtkAlignment parent_instance;
};

GType brasero_song_control_get_type (void) G_GNUC_CONST;

GtkWidget *
brasero_song_control_new (void);

void
brasero_song_control_set_uri (BraseroSongControl *player,
                              const gchar *uri);

void
brasero_song_control_set_info (BraseroSongControl *player,
                               const gchar *title,
                               const gchar *artist);

void
brasero_song_control_set_boundaries (BraseroSongControl *player, 
                                     gsize start,
                                     gsize end);

gint64
brasero_song_control_get_pos (BraseroSongControl *control);

gint64
brasero_song_control_get_length (BraseroSongControl *control);

const gchar *
brasero_song_control_get_uri (BraseroSongControl *control);

G_END_DECLS

#endif /* _BRASERO_SONG_CONTROL_H_ */
