/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2007 <bonfire-app@wanadoo.fr>
 * 
 * brasero is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
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

#ifdef BUILD_PREVIEW

#include <string.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtkframe.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtkcontainer.h>

#include "brasero-player.h"
#include "brasero-preview.h"

typedef struct _BraseroPreviewPrivate BraseroPreviewPrivate;
struct _BraseroPreviewPrivate
{
	GtkWidget *player;
	GtkWidget *frame;
};

#define BRASERO_PREVIEW_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_PREVIEW, BraseroPreviewPrivate))

G_DEFINE_TYPE (BraseroPreview, brasero_preview, GTK_TYPE_ALIGNMENT);

static void
brasero_preview_player_ready_cb (BraseroPlayer *player,
				 BraseroPreview *self)
{
	BraseroPreviewPrivate *priv;

	priv = BRASERO_PREVIEW_PRIVATE (self);
	gtk_widget_show (priv->frame);
}

static void
brasero_preview_source_selection_changed_cb (BraseroURIContainer *source,
					     BraseroPreview *self)
{
	gchar *uri;
	gint64 end;
	gint64 start;
	BraseroPreviewPrivate *priv;

	priv = BRASERO_PREVIEW_PRIVATE (self);

	/* hide while it's loading */
	gtk_widget_hide (priv->frame);

	uri = brasero_uri_container_get_selected_uri (source);
	brasero_player_set_uri (BRASERO_PLAYER (priv->player), uri);
	g_free (uri);

	if (brasero_uri_container_get_boundaries (source, &start, &end))
		brasero_player_set_boundaries (BRASERO_PLAYER (priv->player), start, end);
}

void
brasero_preview_add_source (BraseroPreview *self, BraseroURIContainer *source)
{
	g_signal_connect (source,
			  "uri-selected",
			  G_CALLBACK (brasero_preview_source_selection_changed_cb),
			  self);
}

static void
brasero_preview_init (BraseroPreview *object)
{
	BraseroPreviewPrivate *priv;

	priv = BRASERO_PREVIEW_PRIVATE (object);

	priv->frame = gtk_frame_new (_(" Preview "));
	gtk_widget_show (priv->frame);
	gtk_container_add (GTK_CONTAINER (object), priv->frame);

	priv->player = brasero_player_new ();
	gtk_container_set_border_width (GTK_CONTAINER (priv->player), 8);
	gtk_widget_show (priv->player);
	gtk_container_add (GTK_CONTAINER (priv->frame), priv->player);
	g_signal_connect (priv->player,
			  "ready",
			  G_CALLBACK (brasero_preview_player_ready_cb),
			  object);
}

static void
brasero_preview_finalize (GObject *object)
{
	G_OBJECT_CLASS (brasero_preview_parent_class)->finalize (object);
}

static void
brasero_preview_class_init (BraseroPreviewClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroPreviewPrivate));

	object_class->finalize = brasero_preview_finalize;
}

GtkWidget *
brasero_preview_new (void)
{
	return g_object_new (BRASERO_TYPE_PREVIEW, NULL);
}

#endif
