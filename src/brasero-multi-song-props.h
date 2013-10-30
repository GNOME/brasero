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

#ifndef _BRASERO_MULTI_SONG_PROPS_H_
#define _BRASERO_MULTI_SONG_PROPS_H_

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

#include "brasero-rename.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_MULTI_SONG_PROPS             (brasero_multi_song_props_get_type ())
#define BRASERO_MULTI_SONG_PROPS(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_MULTI_SONG_PROPS, BraseroMultiSongProps))
#define BRASERO_MULTI_SONG_PROPS_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_MULTI_SONG_PROPS, BraseroMultiSongPropsClass))
#define BRASERO_IS_MULTI_SONG_PROPS(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_MULTI_SONG_PROPS))
#define BRASERO_IS_MULTI_SONG_PROPS_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_MULTI_SONG_PROPS))
#define BRASERO_MULTI_SONG_PROPS_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_MULTI_SONG_PROPS, BraseroMultiSongPropsClass))

typedef struct _BraseroMultiSongPropsClass BraseroMultiSongPropsClass;
typedef struct _BraseroMultiSongProps BraseroMultiSongProps;

struct _BraseroMultiSongPropsClass
{
	GtkDialogClass parent_class;
};

struct _BraseroMultiSongProps
{
	GtkDialog parent_instance;
};

GType brasero_multi_song_props_get_type (void) G_GNUC_CONST;

GtkWidget *
brasero_multi_song_props_new (void);

void
brasero_multi_song_props_set_show_gap (BraseroMultiSongProps *props,
				       gboolean show);

void
brasero_multi_song_props_set_rename_callback (BraseroMultiSongProps *props,
					      GtkTreeSelection *selection,
					      gint column_num,
					      BraseroRenameCallback callback);
void
brasero_multi_song_props_get_properties (BraseroMultiSongProps *props,
					 gchar **artist,
					 gchar **composer,
					 gchar **isrc,
					 gint64 *gap);

G_END_DECLS

#endif /* _BRASERO_MULTI_SONG_PROPS_H_ */
