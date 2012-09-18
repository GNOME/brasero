/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2007-2008 <bonfire-app@wanadoo.fr>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef BUILD_PREVIEW

#include <string.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtk.h>

#include "brasero-player.h"
#include "brasero-preview.h"

typedef struct _BraseroPreviewPrivate BraseroPreviewPrivate;
struct _BraseroPreviewPrivate
{
	GtkWidget *player;

	guint set_uri_id;

	gchar *uri;
	gint64 start;
	gint64 end;

	guint is_enabled:1;
};

#define BRASERO_PREVIEW_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_PREVIEW, BraseroPreviewPrivate))

G_DEFINE_TYPE (BraseroPreview, brasero_preview, GTK_TYPE_ALIGNMENT);

static gboolean
brasero_preview_set_uri_delayed_cb (gpointer data)
{
	BraseroPreview *self = BRASERO_PREVIEW (data);
	BraseroPreviewPrivate *priv;

	priv = BRASERO_PREVIEW_PRIVATE (self);

	brasero_player_set_uri (BRASERO_PLAYER (priv->player), priv->uri);
	if (priv->end >= 0 && priv->start >= 0)
		brasero_player_set_boundaries (BRASERO_PLAYER (priv->player),
					       priv->start,
					       priv->end);

	priv->set_uri_id = 0;
	return FALSE;
}

static void
brasero_preview_source_selection_changed_cb (BraseroURIContainer *source,
					     BraseroPreview *self)
{
	BraseroPreviewPrivate *priv;
	gchar *uri;

	priv = BRASERO_PREVIEW_PRIVATE (self);

	/* make sure that we're supposed to activate preview */
	if (!priv->is_enabled)
		return;

	/* Should we always hide ? */
	uri = brasero_uri_container_get_selected_uri (source);
	if (!uri)
		gtk_widget_hide (priv->player);

	/* clean the potentially previous uri information */
	priv->end = -1;
	priv->start = -1;
	if (priv->uri)
		g_free (priv->uri);

	/* set the new one */
	priv->uri = uri;
	brasero_uri_container_get_boundaries (source, &priv->start, &priv->end);

	/* This delay is used in case the user searches the file he wants to display
	 * and goes through very quickly lots of other files before with arrows */
	if (!priv->set_uri_id)
		priv->set_uri_id = g_timeout_add (400,
						  brasero_preview_set_uri_delayed_cb,
						  self);
}

void
brasero_preview_add_source (BraseroPreview *self, BraseroURIContainer *source)
{
	g_signal_connect (source,
			  "uri-selected",
			  G_CALLBACK (brasero_preview_source_selection_changed_cb),
			  self);
}

/**
 * Hides preview until another uri is set and recognised
 */
void
brasero_preview_hide (BraseroPreview *self)
{
	BraseroPreviewPrivate *priv;

	priv = BRASERO_PREVIEW_PRIVATE (self);
	gtk_widget_hide (priv->player);
}

void
brasero_preview_set_enabled (BraseroPreview *self,
			     gboolean preview)
{
	BraseroPreviewPrivate *priv;

	priv = BRASERO_PREVIEW_PRIVATE (self);
	priv->is_enabled = preview;
}

static void
brasero_preview_player_error_cb (BraseroPlayer *player,
				 BraseroPreview *self)
{
	BraseroPreviewPrivate *priv;

	priv = BRASERO_PREVIEW_PRIVATE (self);
	gtk_widget_hide (priv->player);
}

static void
brasero_preview_player_ready_cb (BraseroPlayer *player,
				 BraseroPreview *self)
{
	BraseroPreviewPrivate *priv;

	priv = BRASERO_PREVIEW_PRIVATE (self);
	gtk_widget_show (priv->player);
}

static void
brasero_preview_init (BraseroPreview *object)
{
	BraseroPreviewPrivate *priv;

	priv = BRASERO_PREVIEW_PRIVATE (object);

	priv->player = brasero_player_new ();
	gtk_container_set_border_width (GTK_CONTAINER (priv->player), 4);

	gtk_container_add (GTK_CONTAINER (object), priv->player);
	g_signal_connect (priv->player,
			  "error",
			  G_CALLBACK (brasero_preview_player_error_cb),
			  object);
	g_signal_connect (priv->player,
			  "ready",
			  G_CALLBACK (brasero_preview_player_ready_cb),
			  object);
}

static void
brasero_preview_finalize (GObject *object)
{
	BraseroPreviewPrivate *priv;

	priv = BRASERO_PREVIEW_PRIVATE (object);

	if (priv->set_uri_id) {
		g_source_remove (priv->set_uri_id);
		priv->set_uri_id = 0;
	}

	if (priv->uri) {
		g_free (priv->uri);
		priv->uri = NULL;
	}

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

#endif /* BUILD_PREVIEW */
